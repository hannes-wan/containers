#include "Buffer.h"
#include "BufferPool.h"
#include <immintrin.h>

Buffer::Buffer(BufferPool *pool) : pool_(pool), head_(nullptr), tail_(nullptr) {}

Buffer::~Buffer()
{
    Recycle();
}

void Buffer::Append(const void *src, size_t len)
{
    if (len == 0)
        return;
    const char *p = static_cast<const char *>(src);
    size_t remaining = len;

    while (remaining > 0)
    {
        Chunk *curr_tail = tail_.load(std::memory_order_acquire);

        if (!curr_tail)
        {
            curr_tail = pool_->GetChunk(1024);
            head_.store(curr_tail, std::memory_order_release);
            tail_.store(curr_tail, std::memory_order_release);
        }

        size_t pos = curr_tail->writePos_.fetch_add(remaining, std::memory_order_relaxed);

        if (pos < curr_tail->capacity_)
        {
            size_t can_write = std::min(remaining, curr_tail->capacity_ - pos);
            std::memcpy(curr_tail->data_ + pos, p, can_write);

            while (curr_tail->committedPos_.load(std::memory_order_acquire) != pos)
            {
                _mm_pause();
            }
            curr_tail->committedPos_.store(pos + can_write, std::memory_order_release);

            p += can_write;
            remaining -= can_write;
        }

        if (remaining > 0)
        {
            size_t next_cap = std::max(static_cast<size_t>(1024), curr_tail->capacity_ * 2);
            Chunk *next_node = pool_->GetChunk(next_cap);
            Chunk *expected = nullptr;
            if (curr_tail->next_.compare_exchange_strong(expected, next_node, std::memory_order_release))
            {
                tail_.store(next_node, std::memory_order_release);
            }
            else
            {
                pool_->ReturnChunk(next_node);
            }
        }
    }
}

size_t Buffer::TotalSize() const
{
    size_t total = 0;
    Chunk *curr = head_.load(std::memory_order_acquire);
    while (curr)
    {
        total += curr->committedPos_.load(std::memory_order_acquire);
        curr = curr->next_.load(std::memory_order_acquire);
    }
    return total;
}

void Buffer::Recycle()
{
    Chunk *curr = head_.exchange(nullptr, std::memory_order_acquire);
    tail_.store(nullptr, std::memory_order_release);
    while (curr)
    {
        Chunk *next = curr->next_.load(std::memory_order_relaxed);
        pool_->ReturnChunk(curr);
        curr = next;
    }
}
