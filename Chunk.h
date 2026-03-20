#ifndef COMM_BUFFER_CHUNK_H_
#define COMM_BUFFER_CHUNK_H_

#include <atomic>
#include <cstdlib>

struct alignas(64) Chunk
{
    char *data_;
    size_t capacity_;
    std::atomic<size_t> writePos_;
    std::atomic<size_t> committedPos_;
    std::atomic<Chunk *> next_;

    explicit Chunk(size_t cap);
    ~Chunk();

    void Reset();

    Chunk(const Chunk &) = delete;
    Chunk &operator=(const Chunk &) = delete;
};

#endif
