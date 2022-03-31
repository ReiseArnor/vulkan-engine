#include "vk_pipeline.h"
#include "vk_initializers.h"
#include <iostream>
#include <vulkan/vulkan_core.h>

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass)
{
  // make viewport state from our stored viewport and scissor
  // at the moment we wont support multiple viewports or scissors
  VkPipelineViewportStateCreateInfo viewport_state = {};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.pNext = nullptr;

  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &_viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &_scissor;

  // setup dummy color blending
  // the blending is just "no blend", but we do write to the color attachment
  VkPipelineColorBlendStateCreateInfo color_blending = {};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.pNext = nullptr;

  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &_color_blend_attachment;

  // build the actual pipeline
  VkGraphicsPipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.pNext = nullptr;

  pipeline_info.stageCount = _shader_stages.size();
  pipeline_info.pStages = _shader_stages.data();
  pipeline_info.pVertexInputState = &_vertex_input_info;
  pipeline_info.pInputAssemblyState = &_input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &_rasterizer;
  pipeline_info.pMultisampleState = &_multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDepthStencilState = &_depth_stencil;
  pipeline_info.layout = _pipeline_layout;
  pipeline_info.renderPass = pass;
  pipeline_info.subpass = 0;
  pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

  VkPipeline new_pipeline;
  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info,
                                nullptr, &new_pipeline) != VK_SUCCESS) {
    std::cout << "failed to create pipeline\n";
    return VK_NULL_HANDLE;
  }
  else {
    return new_pipeline;
  }
}
