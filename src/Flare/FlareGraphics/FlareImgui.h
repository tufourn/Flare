#pragma once

#include <volk.h>

namespace Flare {
struct GpuDevice;

struct FlareImgui {
  void init(GpuDevice *gpuDevice);
  void shutdown();

  void newFrame();
  void draw(VkCommandBuffer cmd, VkImageView target);

  GpuDevice *gpu;
  VkDescriptorPool descriptorPool;
};
} // namespace Flare
