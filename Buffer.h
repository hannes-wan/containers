#ifndef COMM_BUFFER_BUFFER_H_
#define COMM_BUFFER_BUFFER_H_

#include <atomic>
#include <algorithm>
#include <cstring>
#include "Chunk.h"

class BufferPool;

class Buffer
{
    friend class BufferPool;

public:
    explicit Buffer(BufferPool *pool);
    ~Buffer();

    void Append(const void *src, size_t len);

    template <typename F>
    void Peek(F &&processor) const
    {
        Chunk *curr = head_.load(std::memory_order_acquire);
        while (curr)
        {
            size_t readable = curr->committedPos_.load(std::memory_order_acquire);
            if (readable > 0)
                processor(curr->data_, readable);
            curr = curr->next_.load(std::memory_order_acquire);
        }
    }

    size_t TotalSize() const;
    void Recycle();

private:
    BufferPool *pool_;
    alignas(64) std::atomic<Chunk *> head_;
    alignas(64) std::atomic<Chunk *> tail_;
    alignas(64) std::atomic<Buffer *> nextBuffer_{nullptr};
};

#endif
