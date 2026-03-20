#include "Chunk.h"

Chunk::Chunk(size_t cap)
    : data_(static_cast<char *>(std::malloc(cap))),
      capacity_(cap),
      writePos_(0),
      committedPos_(0),
      next_(nullptr) {}

Chunk::~Chunk()
{
    std::free(data_);
}

void Chunk::Reset()
{
    writePos_.store(0, std::memory_order_relaxed);
    committedPos_.store(0, std::memory_order_relaxed);
    next_.store(nullptr, std::memory_order_relaxed);
}
