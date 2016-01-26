#ifndef TOOLS_JS2C_CACHE_H_
#define TOOLS_JS2C_CACHE_H_

#include "v8.h"

#include <stdlib.h>
#include <string.h>

namespace js2c_cache {

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    return data == nullptr ? data : memset(data, 0, length);
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) { free(data); }
};

}  // namespace js2c_cache

#endif  // TOOLS_JS2C_CACHE_H_
