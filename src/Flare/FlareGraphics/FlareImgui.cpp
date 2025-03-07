#include "FlareImgui.h"
#include "GpuDevice.h"
#include "VkHelper.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace Flare {
void FlareImgui::init(GpuDevice *gpuDevice) {
  gpu = gpuDevice;

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
  };
  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 0;
  for (VkDescriptorPoolSize &pool_size : pool_sizes)
    pool_info.maxSets += pool_size.descriptorCount;
  pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  if (vkCreateDescriptorPool(gpu->device, &pool_info, nullptr,
                             &descriptorPool) != VK_SUCCESS) {
    spdlog::error("Imgui: failed to create descriptor pool");
  }

  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForVulkan(gpu->glfwWindow, true);

  VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

  ImGui_ImplVulkan_InitInfo initInfo = {
      .Instance = gpu->instance,
      .PhysicalDevice = gpu->physicalDevice,
      .Device = gpu->device,
      .QueueFamily = gpu->mainFamily,
      .Queue = gpu->mainQueue,
      .DescriptorPool = descriptorPool,
      .MinImageCount = static_cast<uint32_t>(gpu->swapchainImages.size()),
      .ImageCount = static_cast<uint32_t>(gpu->swapchainImages.size()),
      .UseDynamicRendering = true,
      .PipelineRenderingCreateInfo = {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
          .colorAttachmentCount = 1,
          .pColorAttachmentFormats = &format,
      }};
  ImGui_ImplVulkan_Init(&initInfo);

  ImGui_ImplVulkan_CreateFontsTexture();
}

void FlareImgui::shutdown() {
  vkDeviceWaitIdle(gpu->device);

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  vkDestroyDescriptorPool(gpu->device, descriptorPool, nullptr);
}

void FlareImgui::newFrame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void FlareImgui::draw(VkCommandBuffer cmd, VkImageView target) {
  VkRenderingAttachmentInfo colorAttachment = VkHelper::colorAttachment(
      target, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE);
  VkRenderingInfo renderInfo =
      VkHelper::renderingInfo(gpu->swapchainExtent, 1, &colorAttachment);

  vkCmdBeginRendering(cmd, &renderInfo);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
  vkCmdEndRendering(cmd);
}
} // namespace Flare