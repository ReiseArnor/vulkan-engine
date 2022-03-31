#include "vk_initializers.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>

VkCommandPoolCreateInfo
vkinit::command_pool_create_info(uint32_t queue_family_index,
                                 VkCommandPoolCreateFlags flags /*= 0*/)
{
  VkCommandPoolCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  info.pNext = nullptr;

  info.queueFamilyIndex = queue_family_index;
  info.flags = flags;
  return info;
}

VkCommandBufferAllocateInfo vkinit::command_buffer_allocate_info(
    VkCommandPool pool, uint32_t count /*= 1*/,
    VkCommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/)
{
  VkCommandBufferAllocateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.pNext = nullptr;

  info.commandPool = pool;
  info.commandBufferCount = count;
  info.level = level;
  return info;
}

VkPipelineShaderStageCreateInfo
vkinit::pipeline_shader_stage_create_info(VkShaderStageFlagBits stage,
                                          VkShaderModule shader_module)
{
  VkPipelineShaderStageCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.pNext = nullptr;

  info.stage = stage;
  info.module = shader_module;
  // the entry point of the shader
  info.pName = "main";
  return info;
}

VkPipelineVertexInputStateCreateInfo vkinit::vertex_input_state_create_info()
{
  VkPipelineVertexInputStateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  info.pNext = nullptr;

  // no vertex bindings or attributes
  info.vertexBindingDescriptionCount = 0;
  info.vertexAttributeDescriptionCount = 0;

  info.pVertexBindingDescriptions = nullptr;
  info.pVertexAttributeDescriptions = nullptr;

  info.flags = 0;
  return info;
}

VkPipelineInputAssemblyStateCreateInfo
vkinit::input_assembly_create_info(VkPrimitiveTopology topology)
{
  VkPipelineInputAssemblyStateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  info.pNext = nullptr;

  info.topology = topology;
  // we are not going to use primitive restart
  info.primitiveRestartEnable = VK_FALSE;
  return info;
}

VkPipelineRasterizationStateCreateInfo
vkinit::rasterization_state_create_info(VkPolygonMode polygon_mode)
{
  VkPipelineRasterizationStateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  info.pNext = nullptr;

  info.depthClampEnable = VK_FALSE;
  // discard all primitives before the rasterization stage if enabled which we
  // dont want
  info.rasterizerDiscardEnable = VK_FALSE;

  info.polygonMode = polygon_mode;
  info.lineWidth = 1.0f;
  // no backface cull
  info.cullMode = VK_CULL_MODE_NONE;
  info.frontFace = VK_FRONT_FACE_CLOCKWISE;
  // no depth bias
  info.depthBiasEnable = VK_FALSE;
  info.depthBiasConstantFactor = 0.f;
  info.depthBiasClamp = 0.f;
  info.depthBiasSlopeFactor = 0.f;
  return info;
}

VkPipelineMultisampleStateCreateInfo vkinit::multisampling_state_create_info()
{
  VkPipelineMultisampleStateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  info.pNext = nullptr;

  info.sampleShadingEnable = VK_FALSE;
  // multisampling defaulted to no multisampling (1 sample per pixel)
  info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  info.minSampleShading = 1.0f;
  info.pSampleMask = nullptr;
  info.alphaToCoverageEnable = VK_FALSE;
  info.alphaToOneEnable = VK_FALSE;
  return info;
}

VkPipelineColorBlendAttachmentState vkinit::color_blend_attachment_state()
{
  VkPipelineColorBlendAttachmentState color_blend_attachment = {};
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_FALSE;
  return color_blend_attachment;
}

VkPipelineLayoutCreateInfo vkinit::pipeline_layout_create_info()
{
  VkPipelineLayoutCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.pNext = nullptr;

  // empty defaults;
  info.flags = 0;
  info.setLayoutCount = 0;
  info.pSetLayouts = nullptr;
  info.pushConstantRangeCount = 0;
  info.pPushConstantRanges = nullptr;
  return info;
}

VkFenceCreateInfo vkinit::fence_create_info(VkFenceCreateFlags flags /*=0 */)
{
  VkFenceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = flags;
  return info;
}

VkSemaphoreCreateInfo
vkinit::semaphore_create_info(VkSemaphoreCreateFlags flags /*=0 */)
{
  VkSemaphoreCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = flags;
  return info;
}

VkImageCreateInfo vkinit::image_create_info(VkFormat format,
                                            VkImageUsageFlags usage_flags,
                                            VkExtent3D extent)
{
  VkImageCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext = nullptr;

  info.imageType = VK_IMAGE_TYPE_2D;

  info.format = format;
  info.extent = extent;

  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usage_flags;
  return info;
}

VkImageViewCreateInfo
vkinit::image_view_create_info(VkFormat format, VkImage image,
                               VkImageAspectFlags aspect_flags)
{
  // build a image-view for the depth image to use for rendering
  VkImageViewCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.pNext = nullptr;

  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.image = image;
  info.format = format;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount = 1;
  info.subresourceRange.aspectMask = aspect_flags;
  return info;
}

VkPipelineDepthStencilStateCreateInfo
vkinit::depth_stencil_create_info(bool depth_test, bool depth_write,
                                  VkCompareOp compare_op)
{
  VkPipelineDepthStencilStateCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  info.pNext = nullptr;

  info.depthTestEnable = depth_test ? VK_TRUE : VK_FALSE;
  info.depthWriteEnable = depth_write ? VK_TRUE : VK_FALSE;
  info.depthCompareOp = depth_test ? compare_op : VK_COMPARE_OP_ALWAYS;
  info.minDepthBounds = 0.0f; // optional
  info.maxDepthBounds = 1.0f; // optional
  info.stencilTestEnable = VK_FALSE;
  return info;
}

VkRenderPassBeginInfo vkinit::render_pass_begin_info(VkRenderPass render_pass,
                                                     VkExtent2D extent,
                                                     VkFramebuffer framebuffer)
{
  VkRenderPassBeginInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  info.pNext = nullptr;

  info.renderPass = render_pass;
  info.renderArea.offset.x = 0;
  info.renderArea.offset.y = 0;
  info.renderArea.extent = extent;
  info.framebuffer = framebuffer;
  return info;
}
