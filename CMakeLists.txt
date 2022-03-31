cmake_minimum_required(VERSION 3.16)
project(vulkan_engine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_package(Vulkan REQUIRED)

add_subdirectory(thirdparty)
add_subdirectory(src)
