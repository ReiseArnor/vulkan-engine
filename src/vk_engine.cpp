#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_mesh.h"
#include "vk_pipeline.h"
#include "vk_types.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <type_traits>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>

#include <VkBootstrap.h>

#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/trigonometric.hpp>

constexpr bool bUseValidationLayers = true;

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      std::cout << "Detected Vulkan error: " << err << "\n";                   \
      abort();                                                                 \
    }                                                                          \
  } while (0)

void heapify_materials(std::vector<RenderObject> &renderables,
                       const std::size_t size, const std::size_t index)
{
  auto largest = index;
  auto l = 2 * index + 1;
  auto r = 2 * index + 2;

  if (l < size && renderables.at(l).material > renderables.at(largest).material)
    largest = l;

  if (r < size && renderables.at(r).material > renderables.at(largest).material)
    largest = r;

  if (largest != index) {
    std::swap(renderables.at(index), renderables.at(largest));
    heapify_materials(renderables, size, largest);
  }
}

void heapify_meshes(std::vector<RenderObject> &renderables,
                    const std::size_t size, const std::size_t index)
{
  auto largest = index;
  auto l = 2 * index + 1;
  auto r = 2 * index + 2;

  if (l < size && renderables.at(l).mesh > renderables.at(largest).mesh)
    largest = l;

  if (r < size && renderables.at(r).mesh > renderables.at(largest).mesh)
    largest = r;

  if (largest != index) {
    if (renderables.at(index).material == renderables.at(largest).material)
      std::swap(renderables.at(index), renderables.at(largest));
    heapify_meshes(renderables, size, largest);
  }
}


void sort_renderables(std::vector<RenderObject> &renderables)
{
  const auto size = renderables.size();

  // sort by material
  for (int index = static_cast<int>(size - 1); index >= 0; index--)
    heapify_materials(renderables, size, static_cast<std::size_t>(index));

  for (int index = static_cast<int>(size - 1); index >= 0; index--) {
    std::swap(renderables.at(0),
              renderables.at(static_cast<std::size_t>(index)));
    heapify_materials(renderables, static_cast<std::size_t>(index), 0);
  }

  // sort by mesh taking into account the material
  // not sure yet if it's worth to do this
  for (int index = static_cast<int>(size - 1); index >= 0; index--)
    heapify_meshes(renderables, size, static_cast<std::size_t>(index));

  for (int index = static_cast<int>(size - 1); index >= 0; index--) {
    std::swap(renderables.at(0),
              renderables.at(static_cast<std::size_t>(index)));
    heapify_meshes(renderables, static_cast<std::size_t>(index), 0);
  }
}


void VulkanEngine::init()
{
  SDL_Init(SDL_INIT_VIDEO);
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

  _window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED, _windowExtent.width,
                             _windowExtent.height, window_flags);

  init_vulkan();
  init_swapchain();
  init_commands();
  init_default_renderpass();
  init_framebuffers();
  init_sync_structures();
  init_pipelines();

  load_meshes();

  init_scene();

  sort_renderables(_renderables);

  _is_initialized = true;
}


void VulkanEngine::cleanup()
{
  if (_is_initialized) {
    // make sure the GPU has stopped doing its things
    vkDeviceWaitIdle(_device);

    _main_deletion_queue.flush();

    vkDestroySurfaceKHR(_instance, _surface, nullptr);

    vkDestroyDevice(_device, nullptr);
    vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
    vkDestroyInstance(_instance, nullptr);

    SDL_DestroyWindow(_window);
  }
}


void VulkanEngine::draw()
{
  // wait until GPU has finished rendering the last frame. Timeout of 1 second
  VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._render_fence,
                           VK_TRUE, 1000000000));
  VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._render_fence));

  // request image from the swapchain, one second timeout
  uint32_t swapchain_image_index;
  VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000,
                                 get_current_frame()._present_semaphore,
                                 nullptr, &swapchain_image_index));

  VK_CHECK(vkResetCommandBuffer(get_current_frame()._main_command_buffer, 0));

  // naming it cmd for shorter writing
  auto cmd = get_current_frame()._main_command_buffer;

  VkCommandBufferBeginInfo cmd_begin_info = {};
  cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cmd_begin_info.pNext = nullptr;

  cmd_begin_info.pInheritanceInfo = nullptr;
  cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

  VkClearValue clear_value;
  float flash = abs(sin(static_cast<float>(_frame_number) / 120.f));
  clear_value.color = {{0.0f, 0.0f, flash, 1.0f}};

  // clear depth at 1
  VkClearValue depth_clear;
  depth_clear.depthStencil.depth = 1.f;

  // start the main renderpass
  VkRenderPassBeginInfo rp_info = vkinit::render_pass_begin_info(
      _render_pass, _windowExtent, _framebuffers[swapchain_image_index]);

  // connect clear values
  rp_info.clearValueCount = 2;

  VkClearValue clear_values[] = {clear_value, depth_clear};

  rp_info.pClearValues = &clear_values[0];

  vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

  draw_objects(cmd, _renderables.data(), static_cast<int>(_renderables.size()));

  vkCmdEndRenderPass(cmd);
  VK_CHECK(vkEndCommandBuffer(cmd));

  // prepare the submission to the queue
  // we want to wait on the _present_semaphore, as that semaphore is signaled
  // when the swapchain is ready we will signal the _render_semaphore, to signal
  // that rendering has finished

  VkSubmitInfo submit = {};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.pNext = nullptr;

  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  submit.pWaitDstStageMask = &wait_stage;

  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &get_current_frame()._present_semaphore;

  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &get_current_frame()._render_semaphore;

  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;

  // submit command buffer to the queue and execute it
  // _render_fence will now block until the graphics commands finish execution
  VK_CHECK(vkQueueSubmit(_graphics_queue, 1, &submit,
                         get_current_frame()._render_fence));

  // this will put the image we just rendered into the visible window
  // we wannt to wait on the _render_semaphore for that
  // as its necessary that drawing commands have finished before the image is
  // displayed to the user
  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = nullptr;

  present_info.pSwapchains = &_swapchain;
  present_info.swapchainCount = 1;

  present_info.pWaitSemaphores = &get_current_frame()._render_semaphore;
  present_info.waitSemaphoreCount = 1;

  present_info.pImageIndices = &swapchain_image_index;

  VK_CHECK(vkQueuePresentKHR(_graphics_queue, &present_info));

  // increase the number of frames drawn
  ++_frame_number;
}


void VulkanEngine::run()
{
  SDL_Event e;
  bool quit = false;

  while (!quit) {
    while (SDL_PollEvent(&e) != 0) {
      switch (e.type) {
      case SDL_QUIT:
        quit = true;
        break;
      case SDL_KEYDOWN:
        switch (e.key.keysym.sym) {
        case SDLK_LEFT:
          move_camera(Move::LEFT);
          std::cout << "left key\n";
          break;
        case SDLK_RIGHT:
          move_camera(Move::RIGHT);
          std::cout << "right key\n";
          break;
        case SDLK_UP:
          move_camera(Move::UP);
          std::cout << "up key\n";
          break;
        case SDLK_DOWN:
          move_camera(Move::DOWN);
          std::cout << "down key\n";
          break;
        case SDLK_SPACE:
          ++_selected_shader;
          if (_selected_shader > 1)
            _selected_shader = 0;
          break;
        }
        break;
      case SDL_MOUSEWHEEL:
        std::cout << "mouse wheel\n";
        break;
      }
    }
    draw();
  }
}


void VulkanEngine::init_vulkan()
{
  vkb::InstanceBuilder builder;

  // make the Vulkan instance, with vasic debug features
  auto inst_ret = builder.set_app_name("Example Vulkan App")
                      .request_validation_layers(bUseValidationLayers)
                      .require_api_version(1, 1, 0)
                      .use_default_debug_messenger()
                      .build();

  vkb::Instance vkb_inst = inst_ret.value();

  // store the instance
  _instance = vkb_inst.instance;
  // store the debug messenger
  _debug_messenger = vkb_inst.debug_messenger;

  // get the surface of the window we opened with SDL
  SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

  // use vkbootstrap to select a GPU
  // we want a GPT that can write to he SDL surface and supports Vulkan 1.1
  vkb::PhysicalDeviceSelector selector{vkb_inst};
  vkb::PhysicalDevice physical_device =
      selector.set_minimum_version(1, 1).set_surface(_surface).select().value();

  // create the final Vulkan device
  vkb::DeviceBuilder device_builder{physical_device};

  vkb::Device vkb_device = device_builder.build().value();

  // get the VkDevice handle used in the rest of a Vulkan App
  _device = vkb_device.device;
  _chosen_gpu = physical_device.physical_device;

  // get Graphics queue

  _graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
  _graphics_queue_family =
      vkb_device.get_queue_index(vkb::QueueType::graphics).value();

  // initialize the memory allocator
  VmaAllocatorCreateInfo allocator_info = {};
  allocator_info.physicalDevice = _chosen_gpu;
  allocator_info.device = _device;
  allocator_info.instance = _instance;

  vmaCreateAllocator(&allocator_info, &_allocator);
}


void VulkanEngine::init_swapchain()
{
  vkb::SwapchainBuilder swapchain_builder{_chosen_gpu, _device, _surface};

  vkb::Swapchain vkb_swapchain =
      swapchain_builder.use_default_format_selection()
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(_windowExtent.width, _windowExtent.height)
          .build()
          .value();

  _swapchain = vkb_swapchain.swapchain;
  _swapchain_images = vkb_swapchain.get_images().value();
  _swapchain_image_views = vkb_swapchain.get_image_views().value();
  _swapchain_image_format = vkb_swapchain.image_format;

  _main_deletion_queue.push_function(
      [this]() { vkDestroySwapchainKHR(_device, _swapchain, nullptr); });

  // depth image size will match the window
  VkExtent3D depth_image_extent = {_windowExtent.width, _windowExtent.height,
                                   1};

  // hardcoding the depth format to 32 bit float
  _depth_format = VK_FORMAT_D32_SFLOAT;

  // the depth image will be an image with the format we selected and depth
  // attachment usage flag
  auto dimg_info = vkinit::image_create_info(
      _depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      depth_image_extent);

  // for the depth image, we want to allocate it from GPU local memory
  VmaAllocationCreateInfo dimg_alloc_info = {};
  dimg_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  dimg_alloc_info.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // allocate and create the image
  vmaCreateImage(_allocator, &dimg_info, &dimg_alloc_info, &_depth_image._image,
                 &_depth_image._allocation, nullptr);

  // build an image-view for the depth image to use for rendering
  auto dview_info = vkinit::image_view_create_info(
      _depth_format, _depth_image._image, VK_IMAGE_ASPECT_DEPTH_BIT);

  VK_CHECK(
      vkCreateImageView(_device, &dview_info, nullptr, &_depth_image_view));

  _main_deletion_queue.push_function([this]() {
    vkDestroyImageView(_device, _depth_image_view, nullptr);
    vmaDestroyImage(_allocator, _depth_image._image, _depth_image._allocation);
  });
}


void VulkanEngine::init_commands()
{
  // create a command pool for commands submitted to the graphics queue
  // we also want the pool to allow for resetting of individual command buffers
  auto command_pool_info = vkinit::command_pool_create_info(
      _graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  for (unsigned int index = 0; index < FRAME_OVERLAP; index++) {
    VK_CHECK(vkCreateCommandPool(_device, &command_pool_info, nullptr,
                                 &_frames[index]._command_pool));

    // allocate the default command buffer that we will use for rendering
    auto cmd_alloc_info =
        vkinit::command_buffer_allocate_info(_frames[index]._command_pool);

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmd_alloc_info,
                                      &_frames[index]._main_command_buffer));

    _main_deletion_queue.push_function([=]() {
      vkDestroyCommandPool(_device, _frames[index]._command_pool, nullptr);
    });
  }
}


void VulkanEngine::init_default_renderpass()
{
  // renderpass will user this color attachment
  VkAttachmentDescription color_attachment = {};
  // will have the format needed by the swapchain
  color_attachment.format = _swapchain_image_format;
  // 1 sample, we wont be doing MSAA
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  // we Clear when this attachment is loaded
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  // we keep the attachment store when the renderpass ends
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  // we dont care about stencil
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  // we dont know or care about the starting layout of the attachment
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  // after the renderpass ends, the image has to be on a layout ready for
  // display
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref = {};
  // attachment will index into the pAttachments array in the parent renderpass
  // itself
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // Depth attachment
  VkAttachmentDescription depth_attachment = {};
  depth_attachment.flags = 0;
  depth_attachment.format = _depth_format;
  depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment_ref = {};
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // we are going create 1 subpass, which is the minimum you can do
  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;

  VkAttachmentDescription attachments[2] = {color_attachment, depth_attachment};

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

  render_pass_info.attachmentCount = 2;
  render_pass_info.pAttachments = &attachments[0];
  // connect the subpass to the info
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;

  VkSubpassDependency dependency = {};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkSubpassDependency depth_dependency = {};
  depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  depth_dependency.dstSubpass = 0;
  depth_dependency.srcStageMask =
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR;
  depth_dependency.srcAccessMask = 0;
  depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkSubpassDependency dependencies[2] = {dependency, depth_dependency};

  render_pass_info.dependencyCount = 2;
  render_pass_info.pDependencies = &dependencies[0];

  VK_CHECK(
      vkCreateRenderPass(_device, &render_pass_info, nullptr, &_render_pass));

  _main_deletion_queue.push_function(
      [this]() { vkDestroyRenderPass(_device, _render_pass, nullptr); });
}


void VulkanEngine::init_framebuffers()
{
  // create the framebuffers for the swapchain images, This will connect the
  // renderpass to the images for rendering
  VkFramebufferCreateInfo fb_info = {};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.pNext = nullptr;

  fb_info.renderPass = _render_pass;
  fb_info.attachmentCount = 1;
  fb_info.width = _windowExtent.width;
  fb_info.height = _windowExtent.height;
  fb_info.layers = 1;

  // grab how many images we have in the swapchain
  const uint32_t swapchain_image_count = _swapchain_images.size();
  _framebuffers = std::vector<VkFramebuffer>(swapchain_image_count);

  // create framebuffers for each of the swapchain image views
  for (auto i = 0; i < swapchain_image_count; i++) {
    VkImageView attachments[2];
    attachments[0] = _swapchain_image_views[i];
    attachments[1] = _depth_image_view;

    fb_info.pAttachments = attachments;
    fb_info.attachmentCount = 2;

    VK_CHECK(
        vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));

    _main_deletion_queue.push_function([this, i = i]() {
      vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
      vkDestroyImageView(_device, _swapchain_image_views[i], nullptr);
    });
  }
}


void VulkanEngine::init_sync_structures()
{

  // we want to create the fence with the create signaled flag, so we can wait
  // on it before using it on a GPU command (for the first frame)
  auto fence_info = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

  // for the semaphores we dont need any flags
  auto semaphore_info = vkinit::semaphore_create_info();

  for (unsigned int index = 0; index < FRAME_OVERLAP; index++) {
    VK_CHECK(vkCreateFence(_device, &fence_info, nullptr,
                           &_frames[index]._render_fence));

    _main_deletion_queue.push_function([=]() {
      vkDestroyFence(_device, _frames[index]._render_fence, nullptr);
    });


    VK_CHECK(vkCreateSemaphore(_device, &semaphore_info, nullptr,
                               &_frames[index]._present_semaphore));
    VK_CHECK(vkCreateSemaphore(_device, &semaphore_info, nullptr,
                               &_frames[index]._render_semaphore));

    _main_deletion_queue.push_function([=]() {
      vkDestroySemaphore(_device, _frames[index]._present_semaphore, nullptr);
      vkDestroySemaphore(_device, _frames[index]._render_semaphore, nullptr);
    });
  }
}


bool VulkanEngine::load_shader_module(const char *file_path,
                                      VkShaderModule *out_shader_module)
{
  std::ifstream file(file_path, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    return false;
  }

  auto file_size = static_cast<size_t>(file.tellg());

  std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

  file.seekg(0);
  file.read(reinterpret_cast<char *>(buffer.data()), file_size);
  file.close();

  VkShaderModuleCreateInfo shader_info = {};
  shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_info.pNext = nullptr;

  // codeSize has to be in bytes, so multiply the ints in the buffer by size of
  // int to know the real size of the buffer
  shader_info.codeSize = buffer.size() * sizeof(uint32_t);
  shader_info.pCode = buffer.data();

  // check that the creation goes well
  VkShaderModule shader_module;
  if (vkCreateShaderModule(_device, &shader_info, nullptr, &shader_module) !=
      VK_SUCCESS) {
    return false;
  }
  *out_shader_module = shader_module;
  return true;
}


void VulkanEngine::init_pipelines()
{
  VkShaderModule triangle_frag_shader;
  if (!load_shader_module("../shaders/colored_triangle.frag.spv",
                          &triangle_frag_shader)) {
    std::cout << "Error when building the triangle fragment shader module\n";
  }
  else {
    std::cout << "Triangle fragment shader successfully loaded\n";
  }

  VkShaderModule triangle_vert_shader;
  if (!load_shader_module("../shaders/colored_triangle.vert.spv",
                          &triangle_vert_shader)) {
    std::cout << "Error when building the triangle vertex shader module\n";
  }
  else {
    std::cout << "Triangle vertex shader successfully loaded\n";
  }

  VkShaderModule red_triangle_vert_shader;
  if (!load_shader_module("../shaders/triangle.vert.spv",
                          &red_triangle_vert_shader)) {
    std::cout << "Error when building the red triangle vertex shader module\n";
  }
  else {
    std::cout << "Red triangle vertex shader successfully loaded\n";
  }

  VkShaderModule red_triangle_frag_shader;
  if (!load_shader_module("../shaders/triangle.frag.spv",
                          &red_triangle_frag_shader)) {
    std::cout
        << "Error when building the red triangle fragment shader module\n";
  }
  else {
    std::cout << "Red triangle fragment shader successfully loaded\n";
  }

  // build the pipeline layout that controls the I/O of the shader
  // we are not using descriptor sets of other systems yet, so no need to use
  // anything other than empty default
  auto pipeline_layout_info = vkinit::pipeline_layout_create_info();
  VkPushConstantRange triangle_push_constant;
  // starts at 0
  triangle_push_constant.offset = 0;
  triangle_push_constant.size = sizeof(MeshPushConstants);
  triangle_push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  pipeline_layout_info.pPushConstantRanges = &triangle_push_constant;
  pipeline_layout_info.pushConstantRangeCount = 1;

  VkPipelineLayout triangle_pipeline_layout;

  VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr,
                                  &triangle_pipeline_layout));

  PipelineBuilder pipeline_builder;

  pipeline_builder._depth_stencil = vkinit::depth_stencil_create_info(
      true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
  pipeline_builder._shader_stages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                triangle_vert_shader));

  pipeline_builder._shader_stages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                triangle_frag_shader));

  pipeline_builder._vertex_input_info =
      vkinit::vertex_input_state_create_info();

  auto vertex_description = Vertex::get_vertex_description();

  // connect the pipeline builder vertex input info to the one we get from
  // Vertex
  pipeline_builder._vertex_input_info.pVertexAttributeDescriptions =
      vertex_description.attributes.data();
  pipeline_builder._vertex_input_info.vertexAttributeDescriptionCount =
      vertex_description.attributes.size();

  pipeline_builder._vertex_input_info.pVertexBindingDescriptions =
      vertex_description.bindings.data();
  pipeline_builder._vertex_input_info.vertexBindingDescriptionCount =
      vertex_description.bindings.size();


  pipeline_builder._input_assembly =
      vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  // build viewport and scissors from the swapchain extents
  pipeline_builder._viewport.x = 0.0f;
  pipeline_builder._viewport.y = 0.0f;
  pipeline_builder._viewport.width = static_cast<float>(_windowExtent.width);
  pipeline_builder._viewport.height = static_cast<float>(_windowExtent.height);
  pipeline_builder._viewport.minDepth = 0.0f;
  pipeline_builder._viewport.maxDepth = 1.0f;

  pipeline_builder._scissor.offset = {0, 0};
  pipeline_builder._scissor.extent = _windowExtent;

  // fill the triangles
  pipeline_builder._rasterizer =
      vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

  // no multisampling
  pipeline_builder._multisampling = vkinit::multisampling_state_create_info();

  // a single blend attachment with no blending and writing to RGBA
  pipeline_builder._color_blend_attachment =
      vkinit::color_blend_attachment_state();

  pipeline_builder._pipeline_layout = triangle_pipeline_layout;

  auto triangle_pipeline =
      pipeline_builder.build_pipeline(_device, _render_pass);

  create_material(triangle_pipeline, triangle_pipeline_layout, "tri_mat");

  // clear the shader stages for the builder
  pipeline_builder._shader_stages.clear();

  // add the other shaders
  pipeline_builder._shader_stages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                red_triangle_vert_shader));

  pipeline_builder._shader_stages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                red_triangle_frag_shader));

  auto red_triangle_pipeline =
      pipeline_builder.build_pipeline(_device, _render_pass);

  create_material(red_triangle_pipeline, triangle_pipeline_layout,
                  "red_tri_mat");

  // build the mesh pipeline
  pipeline_builder._shader_stages.clear();

  VkShaderModule mesh_vert_shader;
  if (!load_shader_module("../shaders/tri_mesh.vert.spv", &mesh_vert_shader)) {
    std::cout << "Error when building the mesh triangle vertex shader module\n";
  }
  else {
    std::cout << "Mesh triangle vertex shader successfully loaded\n";
  }

  pipeline_builder._shader_stages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                                                mesh_vert_shader));

  pipeline_builder._shader_stages.push_back(
      vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                                                triangle_frag_shader));

  auto mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();

  VkPushConstantRange push_constant;
  // starts at 0
  push_constant.offset = 0;
  push_constant.size = sizeof(MeshPushConstants);
  push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
  mesh_pipeline_layout_info.pushConstantRangeCount = 1;

  VkPipelineLayout mesh_pipeline_layout;
  VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr,
                                  &mesh_pipeline_layout));

  pipeline_builder._pipeline_layout = mesh_pipeline_layout;

  auto mesh_pipeline = pipeline_builder.build_pipeline(_device, _render_pass);

  create_material(mesh_pipeline, mesh_pipeline_layout, "defaultmesh");

  // destroy all shader modules, outside of the queue
  vkDestroyShaderModule(_device, mesh_vert_shader, nullptr);
  vkDestroyShaderModule(_device, red_triangle_vert_shader, nullptr);
  vkDestroyShaderModule(_device, red_triangle_frag_shader, nullptr);
  vkDestroyShaderModule(_device, triangle_vert_shader, nullptr);
  vkDestroyShaderModule(_device, triangle_frag_shader, nullptr);

  _main_deletion_queue.push_function([=, this]() {
    vkDestroyPipeline(_device, triangle_pipeline, nullptr);
    vkDestroyPipeline(_device, red_triangle_pipeline, nullptr);
    vkDestroyPipeline(_device, mesh_pipeline, nullptr);

    vkDestroyPipelineLayout(_device, triangle_pipeline_layout, nullptr);
    vkDestroyPipelineLayout(_device, mesh_pipeline_layout, nullptr);
  });
}


void VulkanEngine::load_meshes()
{
  Mesh triangle_mesh;
  triangle_mesh._vertices.resize(3);

  triangle_mesh._vertices[0].position = {1.f, 1.f, 0.f};
  triangle_mesh._vertices[1].position = {-1.f, 1.f, 0.f};
  triangle_mesh._vertices[2].position = {0.f, -1.f, 0.f};

  triangle_mesh._vertices[0].color = {0.f, 1.f, 0.f};
  triangle_mesh._vertices[1].color = {0.f, 1.f, 0.f};
  triangle_mesh._vertices[2].color = {0.f, 1.f, 0.f};

  Mesh monkey_mesh;
  monkey_mesh.load_from_obj("../assets/monkey_smooth.obj");

  Mesh structure_mesh;
  structure_mesh.load_from_obj("../assets/structure.obj");

  Mesh fence_mesh;
  fence_mesh.load_from_obj("../assets/fence.obj");

  Mesh roof_mesh;
  roof_mesh.load_from_obj("../assets/roof.obj");

  upload_mesh(triangle_mesh);
  upload_mesh(monkey_mesh);
  upload_mesh(structure_mesh);
  upload_mesh(fence_mesh);
  upload_mesh(roof_mesh);

  _meshes["monkey"] = monkey_mesh;
  _meshes["triangle"] = triangle_mesh;
  _meshes["structure"] = structure_mesh;
  _meshes["fence"] = fence_mesh;
  _meshes["roof"] = roof_mesh;
}


void VulkanEngine::upload_mesh(Mesh &mesh)
{
  // allocate vertex buffer
  VkBufferCreateInfo buffer_info = {};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.pNext = nullptr;
  buffer_info.size = mesh._vertices.size() * sizeof(Vertex);
  buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  // let the VMA library know that this data should be writeable by CPU, but
  // also readable by GPU
  VmaAllocationCreateInfo vma_alloc_info = {};
  vma_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

  // allocate the buffer
  VK_CHECK(vmaCreateBuffer(_allocator, &buffer_info, &vma_alloc_info,
                           &mesh._vertexBuffer._buffer,
                           &mesh._vertexBuffer._allocation, nullptr));

  _main_deletion_queue.push_function([this, mesh = mesh]() {
    vmaDestroyBuffer(_allocator, mesh._vertexBuffer._buffer,
                     mesh._vertexBuffer._allocation);
  });

  // copy vertex data
  void *data;
  vmaMapMemory(_allocator, mesh._vertexBuffer._allocation, &data);

  memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(Vertex));

  vmaUnmapMemory(_allocator, mesh._vertexBuffer._allocation);
}


Material *VulkanEngine::create_material(VkPipeline pipeline,
                                        VkPipelineLayout layout,
                                        const std::string &name)
{
  Material mat;
  mat.pipeline = pipeline;
  mat.pipeline_layout = layout;
  _materials[name] = mat;
  return &_materials[name];
}


Material *VulkanEngine::get_material(const std::string &name)
{
  auto it = _materials.find(name);
  if (it == _materials.end())
    return nullptr;
  else
    return &(*it).second;
}


Mesh *VulkanEngine::get_mesh(const std::string &name)
{
  auto it = _meshes.find(name);
  if (it == _meshes.end())
    return nullptr;
  else
    return &(*it).second;
}


void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject *first,
                                int count)
{
  // make a model view matrix for rendering the objects camera view
  glm::mat4 view = glm::translate(glm::mat4(1.f), _cam_pos);
  // camera projection
  glm::mat4 projection =
      glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.f);
  projection[1][1] *= -1;

  Mesh *last_mesh = nullptr;
  Material *last_material = nullptr;
  for (int i = 0; i < count; i++) {
    RenderObject &object = first[i];

    // only bind the pipeline if it doesnt match with the already bound one
    if (object.material != last_material) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        object.material->pipeline);
      last_material = object.material;
    }

    glm::mat4 model = object.transform_matrix;
    // final render matrix, that we are calculation on the cpu
    glm::mat4 mesh_matrix = projection * view * model;

    MeshPushConstants constants;
    constants.render_matrix = mesh_matrix;

    // upload the mesh to the GPU via push constants
    vkCmdPushConstants(cmd, object.material->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants),
                       &constants);

    // only bind the mesh if its a different one from last bind
    if (object.mesh != last_mesh) {
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer,
                             &offset);
      last_mesh = object.mesh;
    }

    // we can now draw
    vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, 0);
  }
}


void VulkanEngine::init_scene()
{
  RenderObject monkey;
  monkey.mesh = get_mesh("monkey");
  monkey.material = get_material("defaultmesh");
  monkey.transform_matrix = glm::mat4{1.f};

  _renderables.push_back(monkey);

  for (int x = -20; x <= 20; x++) {
    for (int y = -20; y <= 20; y++) {
      RenderObject tri;

      if (y % 4 == 0)
        tri.mesh = get_mesh("triangle");
      else if (y % 4 == 1)
        tri.mesh = get_mesh("structure");
      else if (y % 4 == 2)
        tri.mesh = get_mesh("fence");
      else
        tri.mesh = get_mesh("roof");

      if (abs(y % 3) == 0)
        tri.material = get_material("defaultmesh");
      else if (abs(y % 3) == 1)
        tri.material = get_material("red_tri_mat");
      else
        tri.material = get_material("tri_mat");

      glm::mat4 translation =
          glm::translate(glm::mat4{1.0}, glm::vec3(x, 0, y));
      glm::mat4 scale = glm::scale(glm::mat4{1.0}, glm::vec3(0.2, 0.2, 0.2));
      tri.transform_matrix = translation * scale;

      _renderables.push_back(tri);
    }
  }
}


void VulkanEngine::move_camera(const Move direction)
{
  switch (direction) {
  case Move::UP:
    _cam_pos.y -= 0.1f;
    break;
  case Move::DOWN:
    _cam_pos.y += 0.1f;
    break;
  case Move::LEFT:
    _cam_pos.x += 0.1f;
    break;
  case Move::RIGHT:
    _cam_pos.x -= 0.1f;
    break;
  }
}

FrameData &VulkanEngine::get_current_frame()
{
  return _frames[_frame_number % FRAME_OVERLAP];
}
