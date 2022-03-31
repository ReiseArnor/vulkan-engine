#include "vk_mesh.h"

#include <cstddef>

#include <iostream>
#include <string>
#include <tiny_obj_loader.h>
#include <vector>
#include <vulkan/vulkan_core.h>

VertexInputDescription Vertex::get_vertex_description() {
  VertexInputDescription description;

  // we will have just 1 vertex buffer binding, with a per-vertex rate
  VkVertexInputBindingDescription main_binding = {};
  main_binding.binding = 0;
  main_binding.stride = sizeof(Vertex);
  main_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  description.bindings.push_back(main_binding);

  // Position will be store at Location 0
  VkVertexInputAttributeDescription position_attr = {};
  position_attr.binding = 0;
  position_attr.location = 0;
  position_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
  position_attr.offset = offsetof(Vertex, position);

  // Normal will be store at Location 1
  VkVertexInputAttributeDescription normal_attr = {};
  normal_attr.binding = 0;
  normal_attr.location = 1;
  normal_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
  normal_attr.offset = offsetof(Vertex, normal);

  // Color will be store at Location 2
  VkVertexInputAttributeDescription color_attr = {};
  color_attr.binding = 0;
  color_attr.location = 2;
  color_attr.format = VK_FORMAT_R32G32B32_SFLOAT;
  color_attr.offset = offsetof(Vertex, color);

  description.attributes.push_back(position_attr);
  description.attributes.push_back(normal_attr);
  description.attributes.push_back(color_attr);
  return description;
}

bool Mesh::load_from_obj(const char *filename) {
  // attrib will contain the vertex arrays of the file
  tinyobj::attrib_t attrib;
  // shapes contains the info for each separate object in the file
  std::vector<tinyobj::shape_t> shapes;
  // materials contains the information about the material of each shape, but we
  // wont use it
  std::vector<tinyobj::material_t> materials;

  // error and warning output from the load function
  std::string warn;
  std::string err;

  // load the OBJ file
  tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename,
                   nullptr);
  if (!warn.empty()) {
    std::cout << "WARN: " << warn << "\n";
  }

  if (!err.empty()) {
    std::cerr << err << "\n";
    return false;
  }

  // loop over shapes
  for (auto shape : shapes) {
    // loop over faces(polygon)
    size_t index_offset = 0;
    for (auto face : shape.mesh.num_face_vertices) {

      // hardcode loading to triangles
      int fv = 3;

      // loop over vertices in the face
      for (size_t v = 0; v < fv; v++) {
        // access to vertex
        tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

        // vertex position
        tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
        tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
        tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
        // vertex normal
        tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
        tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
        tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

        // copy it into our vertex
        Vertex new_vert;
        new_vert.position.x = vx;
        new_vert.position.y = vy;
        new_vert.position.z = vz;

        new_vert.normal.x = nx;
        new_vert.normal.y = ny;
        new_vert.normal.z = nz;

        // we are setting the vertex color as the vertex nomal. This is just for
        // display purposes
        new_vert.color = new_vert.normal;

        _vertices.push_back(new_vert);
      }
      index_offset += fv;
    }
  }

  return true;
}
