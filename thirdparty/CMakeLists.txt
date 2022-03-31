find_package(Vulkan REQUIRED)

add_library(vk-bootstrap STATIC)
add_library(tinyobjloader STATIC)
add_library(vma INTERFACE)
add_library(glm INTERFACE)

target_sources(vk-bootstrap PRIVATE vk-bootstrap/VkBootstrap.h vk-bootstrap/VkBootstrap.cpp)

target_include_directories(vk-bootstrap PUBLIC vk-bootstrap)
target_link_libraries(vk-bootstrap PUBLIC Vulkan::Vulkan)

target_include_directories(vma INTERFACE vma)
target_include_directories(glm INTERFACE glm)

target_sources(tinyobjloader PRIVATE tinyobjloader/tiny_obj_loader.h tinyobjloader/tiny_obj_loader.cc)

target_include_directories(tinyobjloader PUBLIC tinyobjloader)
