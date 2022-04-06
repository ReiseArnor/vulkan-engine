#pragma once
#include "vk_mesh.h"
#include "vk_types.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan_core.h>

#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <vk_mem_alloc.h>

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;

  void push_function(std::function<void()> &&function)
  {
    deletors.push_back(function);
  }

  void flush()
  {
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
      (*it)();
    }

    deletors.clear();
  }
};

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 render_matrix;
};

// note that we store the VkPipeline and layout by value, not pointer.
// They are 64 bit handles to internal driver structures anyway so storing
// pointers to them isnt very useful

struct Material {
  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;
};

struct RenderObject {
  Mesh *mesh;
  Material *material;

  glm::mat4 transform_matrix;
};

enum class Move { UP, DOWN, LEFT, RIGHT };

class VulkanEngine {
public:
  bool _is_initialized{false};
  int _frame_number{0};

  VkExtent2D _windowExtent{1700, 900};

  struct SDL_Window *_window{nullptr};

  void init();
  void cleanup();
  void draw();
  void run();

  VkInstance _instance;
  VkDebugUtilsMessengerEXT _debug_messenger;
  VkPhysicalDevice _chosen_gpu;
  VkDevice _device;
  VkSurfaceKHR _surface;

  VkSwapchainKHR _swapchain;
  VkFormat _swapchain_image_format;
  std::vector<VkImage> _swapchain_images;
  std::vector<VkImageView> _swapchain_image_views;

  VkQueue _graphics_queue;
  uint32_t _graphics_queue_family;

  VkCommandPool _command_pool;
  VkCommandBuffer _main_command_buffer;

  VkRenderPass _render_pass;
  std::vector<VkFramebuffer> _framebuffers;

  VkSemaphore _present_semaphore, _render_semaphore;
  VkFence _render_fence;

  bool load_shader_module(const char *file_path,
                          VkShaderModule *out_shader_module);

  VkPipelineLayout _triangle_pipeline_layout;

  VkPipeline _triangle_pipeline;
  VkPipeline _red_triangle_pipeline;

  int _selected_shader{0};

  DeletionQueue _main_deletion_queue;

  VmaAllocator _allocator;

  VkPipeline _mesh_pipeline;
  Mesh _triangle_mesh;
  VkPipelineLayout _mesh_pipeline_layout;

  Mesh _monkey_mesh;

  VkImageView _depth_image_view;
  AllocateImage _depth_image;
  VkFormat _depth_format;

  // default array of renderable objects
  std::vector<RenderObject> _renderables;

  std::unordered_map<std::string, Material> _materials;
  std::unordered_map<std::string, Mesh> _meshes;

  Material *create_material(VkPipeline pipeline, VkPipelineLayout layout,
                            const std::string &name);

  // return nullptr if it cant be found
  Material *get_material(const std::string &name);
  Mesh *get_mesh(const std::string &name);

  void draw_objects(VkCommandBuffer cmd, RenderObject *first, int count);

  glm::vec3 _cam_pos = {0.f, -6.f, -10.f};
  void move_camera(const Move direction);

private:
  void init_vulkan();
  void init_swapchain();
  void init_commands();
  void init_default_renderpass();
  void init_framebuffers();
  void init_sync_structures();
  void init_pipelines();
  void init_scene();

  void load_meshes();
  void upload_mesh(Mesh &mesh);
};
