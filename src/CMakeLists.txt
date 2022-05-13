set(CPP_SOURCE main.cpp vk_engine.cpp vk_initializers.cpp vk_pipeline.cpp vk_mesh.cpp)
set(CPP_HEADERS vk_engine.h vk_initializers.h vk_init.h vk_types.h vk_mesh.h)

if(MSVC)
    set(CPP_FLAGS /W4 /permissive-)
else()
    set(CPP_FLAGS -Wall -Wextra -Wconversion -Wpedantic)
    set(CPP_LINKING_OPTS -fno-omit-frame-pointer -fsanitize=undefined,address)
endif()

add_executable(${PROJECT_NAME} ${CPP_SOURCE})
target_compile_options(${PROJECT_NAME} PRIVATE ${CPP_FLAGS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan SDL2 vk-bootstrap vma tinyobjloader)
target_link_options(${PROJECT_NAME} PRIVATE ${CPP_LINKING_OPTS})
