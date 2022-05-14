#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;

layout (location = 0) out vec3 outColor;

layout( push_constant ) uniform constants {
    vec4 data;
    mat4 render_matrix;
} PushConstants;

layout(set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
} camera_data;

struct ObjectData{
    mat4 model;
};

// all object matrices
layout(std140, set = 1, binding = 0) readonly buffer ObjectBuffer{
    ObjectData objects[];
} object_buffer;

void main() {
    mat4 model_matrix = object_buffer.objects[gl_BaseInstance].model;
    mat4 transform_matrix = (camera_data.viewproj * model_matrix);
    gl_Position = transform_matrix * vec4(vPosition, 1.f);
    outColor = vColor;
}
