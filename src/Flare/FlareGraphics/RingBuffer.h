#include "GpuResources.h"

namespace Flare {
    struct GpuDevice;

    struct RingBuffer {
        enum class Type {
            eUniform,
            eStorage,
        };

        void init(GpuDevice *gpu, uint32_t bufferCount, const BufferCI &ci, Type type);

        void shutdown();

        void moveToNextBuffer();

        Handle<Buffer> buffer();
        Handle<Buffer> buffer(uint32_t index);

        uint32_t ringSize;
        GpuDevice* gpuDevice;
        uint32_t ringIndex = 0;
        std::vector<Handle<Buffer>> bufferRing;
        Type bufferType;
    };
}