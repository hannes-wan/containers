#ifndef COMM_BUFFER_BUFFER_POOL_H_
#define COMM_BUFFER_BUFFER_POOL_H_

#include <atomic>
#include <cstdint>
#include "Buffer.h"

class BufferPool
{
public:
    BufferPool();
    ~BufferPool();

    Buffer *BorrowBuffer();
    void ReleaseBuffer(Buffer *buf);

    Chunk *GetChunk(size_t size);
    void ReturnChunk(Chunk *chunk);

private:
    int GetBucketIndex(size_t size);

    // ABA 保护：带标签的指针
    struct alignas(16) TaggedChunkPtr
    {
        Chunk *ptr;
        uintptr_t tag;
    };

    struct alignas(16) TaggedBufferPtr
    {
        Buffer *ptr;
        uintptr_t tag;
    };

    struct Bucket
    {
        std::atomic<TaggedChunkPtr> freeStack_{{nullptr, 0}};
        std::atomic<size_t> count_{0};
        std::atomic<size_t> capacityLimit_{0};
    };

    Bucket buckets_[11];
    std::atomic<TaggedBufferPtr> bufferFreeStack_{{nullptr, 0}};
};

#endif
