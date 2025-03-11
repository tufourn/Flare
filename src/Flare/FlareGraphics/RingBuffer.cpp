#include "RingBuffer.h"

#include "GpuDevice.h"

namespace Flare {

void RingBuffer::init(GpuDevice *gpu, uint32_t bufferCount) {
  gpuDevice = gpu;
  ringSize = bufferCount;
  bufferRing.resize(bufferCount);
}
void RingBuffer::init(GpuDevice *gpu, uint32_t bufferCount,
                      const BufferCI &ci) {
  init(gpu, bufferCount);

  for (size_t i = 0; i < bufferCount; i++) {
    bufferRing[i] = gpuDevice->createBuffer(ci);
  }
}

void RingBuffer::shutdown() {
  for (const auto &handle : bufferRing) {
    if (handle.isValid()) {
      gpuDevice->destroyBuffer(handle);
    }
  }
}

void RingBuffer::moveToNextBuffer() {
  ringIndex++;
  if (ringIndex >= ringSize) {
    ringIndex = 0;
  }
}

void RingBuffer::createBuffer(BufferCI &ci) {
  if (bufferRing[ringIndex].isValid()) {
    gpuDevice->destroyBuffer(bufferRing[ringIndex]);
  }
  bufferRing[ringIndex] = gpuDevice->createBuffer(ci);
}

Handle<Buffer> RingBuffer::buffer() { return bufferRing[ringIndex]; }

Handle<Buffer> RingBuffer::buffer(uint32_t index) { return bufferRing[index]; }
} // namespace Flare