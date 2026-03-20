#include "BufferPool.h"

BufferPool::BufferPool() {}

BufferPool::~BufferPool()
{
    // 清理 Buffer 对象池
    TaggedBufferPtr head = bufferFreeStack_.load(std::memory_order_relaxed);
    Buffer *b = head.ptr;
    while (b)
    {
        Buffer *next = b->nextBuffer_.load(std::memory_order_relaxed);
        delete b;
        b = next;
    }

    // 清理 Chunk 内存池
    for (int i = 0; i < 11; ++i)
    {
        TaggedChunkPtr chead = buckets_[i].freeStack_.load(std::memory_order_relaxed);
        Chunk *c = chead.ptr;
        while (c)
        {
            Chunk *next = c->next_.load(std::memory_order_relaxed);
            delete c;
            c = next;
        }
    }
}

Buffer *BufferPool::BorrowBuffer()
{
    TaggedBufferPtr oldHead = bufferFreeStack_.load(std::memory_order_acquire);
    TaggedBufferPtr newHead;

    while (oldHead.ptr)
    {
        newHead.ptr = oldHead.ptr->nextBuffer_.load(std::memory_order_relaxed);
        newHead.tag = oldHead.tag + 1; // 弹出也增加 tag 以确保安全

        if (bufferFreeStack_.compare_exchange_weak(oldHead, newHead,
                                                   std::memory_order_release,
                                                   std::memory_order_acquire))
        {
            return oldHead.ptr;
        }
    }
    return new Buffer(this);
}

void BufferPool::ReleaseBuffer(Buffer *buf)
{
    if (!buf)
        return;
    buf->Recycle();

    TaggedBufferPtr oldHead = bufferFreeStack_.load(std::memory_order_acquire);
    TaggedBufferPtr newHead;
    newHead.ptr = buf;

    do
    {
        buf->nextBuffer_.store(oldHead.ptr, std::memory_order_relaxed);
        newHead.tag = oldHead.tag + 1;
    } while (!bufferFreeStack_.compare_exchange_weak(oldHead, newHead,
                                                     std::memory_order_release,
                                                     std::memory_order_acquire));
}

Chunk *BufferPool::GetChunk(size_t size)
{
    int idx = GetBucketIndex(size);
    if (idx == -1)
        return new Chunk(size);

    auto &bucket = buckets_[idx];
    TaggedChunkPtr oldHead = bucket.freeStack_.load(std::memory_order_acquire);
    TaggedChunkPtr newHead;

    while (oldHead.ptr)
    {
        newHead.ptr = oldHead.ptr->next_.load(std::memory_order_relaxed);
        newHead.tag = oldHead.tag + 1;

        if (bucket.freeStack_.compare_exchange_weak(oldHead, newHead,
                                                    std::memory_order_release,
                                                    std::memory_order_acquire))
        {
            bucket.count_.fetch_sub(1, std::memory_order_relaxed);
            oldHead.ptr->Reset();
            return oldHead.ptr;
        }
    }

    bucket.capacityLimit_.fetch_add(8, std::memory_order_relaxed);
    return new Chunk(1 << (idx + 10));
}

void BufferPool::ReturnChunk(Chunk *chunk)
{
    if (!chunk)
        return;
    int idx = GetBucketIndex(chunk->capacity_);
    if (idx == -1)
    {
        delete chunk;
        return;
    }

    auto &bucket = buckets_[idx];
    if (bucket.count_.load(std::memory_order_relaxed) >= bucket.capacityLimit_.load())
    {
        delete chunk;
        return;
    }

    TaggedChunkPtr oldHead = bucket.freeStack_.load(std::memory_order_acquire);
    TaggedChunkPtr newHead;
    newHead.ptr = chunk;

    do
    {
        chunk->next_.store(oldHead.ptr, std::memory_order_relaxed);
        newHead.tag = oldHead.tag + 1;
    } while (!bucket.freeStack_.compare_exchange_weak(oldHead, newHead,
                                                      std::memory_order_release,
                                                      std::memory_order_acquire));
    bucket.count_.fetch_add(1, std::memory_order_relaxed);
}

int BufferPool::GetBucketIndex(size_t size)
{
    if (size == 0)
        return 0;
    if (size <= 1024)
        return 0;
    unsigned int val = static_cast<unsigned int>(size - 1);
    int index = (31 - __builtin_clz(val)) - 10 + 1;
    return (index >= 0 && index < 11) ? index : -1;
}
