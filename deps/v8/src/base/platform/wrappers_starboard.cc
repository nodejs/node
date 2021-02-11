// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard/memory.h"

#include "src/base/platform/wrappers.h"

namespace v8 {
namespace base {

void* Malloc(size_t size) { return SbMemoryAlloc(size); }

void* Realloc(void* memory, size_t size) {
  return SbMemoryReallocate(memory, size);
}

void Free(void* memory) { return SbMemoryDeallocate(memory); }

void* Calloc(size_t count, size_t size) { return SbMemoryCalloc(count, size); }

void* Memcpy(void* dest, const void* source, size_t count) {
  return SbMemoryCopy(dest, source, count);
}

FILE* Fopen(const char* filename, const char* mode) { return NULL; }

int Fclose(FILE* stream) { return -1; }

}  // namespace base
}  // namespace v8
