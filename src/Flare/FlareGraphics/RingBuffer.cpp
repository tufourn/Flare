#include "RingBuffer.h"

#include "GpuDevice.h"

namespace Flare {

    void RingBuffer::init(GpuDevice *gpu, uint32_t bufferCount, const BufferCI &ci, RingBuffer::Type type) {
        gpuDevice = gpu;
        ringSize = bufferCount;
        bufferRing.resize(bufferCount);
        bufferType = type;

        for (size_t i = 0; i < bufferCount; i++) {
            switch (bufferType) {
                case Type::eStorage:
                    bufferRing[i] = gpuDevice->createBuffer(ci);
                    break;
                case Type::eUniform:
                    bufferRing[i] = gpuDevice->createUniform(ci);
                    break;
            }
        }
    }

    void RingBuffer::shutdown() {
        for (const auto &handle: bufferRing) {
            switch (bufferType) {
                case Type::eStorage:
                    gpuDevice->destroyBuffer(handle);
                    break;
                case Type::eUniform:
                    gpuDevice->destroyUniform(handle);
                    break;
            }
        }
    }

    void RingBuffer::moveToNextBuffer() {
        ringIndex++;
        if (ringIndex >= ringSize) {
            ringIndex = 0;
        }
    }

    Handle<Buffer> RingBuffer::buffer() {
        return bufferRing[ringIndex];
    }

    Handle<Buffer> RingBuffer::buffer(uint32_t index) {
        return bufferRing[index];
    }
}