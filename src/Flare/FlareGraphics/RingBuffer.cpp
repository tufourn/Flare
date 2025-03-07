#include "RingBuffer.h"

#include "GpuDevice.h"

namespace Flare {

void RingBuffer::init(GpuDevice *gpu, uint32_t bufferCount,
                      const BufferCI &ci) {
  gpuDevice = gpu;
  ringSize = bufferCount;
  bufferRing.resize(bufferCount);

  for (size_t i = 0; i < bufferCount; i++) {
    bufferRing[i] = gpuDevice->createBuffer(ci);
  }
}

void RingBuffer::shutdown() {
  for (const auto &handle : bufferRing) {
    gpuDevice->destroyBuffer(handle);
  }
}

void RingBuffer::moveToNextBuffer() {
  ringIndex++;
  if (ringIndex >= ringSize) {
    ringIndex = 0;
  }
}

Handle<Buffer> RingBuffer::buffer() { return bufferRing[ringIndex]; }

Handle<Buffer> RingBuffer::buffer(uint32_t index) { return bufferRing[index]; }
} // namespace Flare