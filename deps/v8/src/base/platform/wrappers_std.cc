// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/base/platform/wrappers.h"

namespace v8 {
namespace base {

void* Malloc(size_t size) { return malloc(size); }

void* Realloc(void* memory, size_t size) { return realloc(memory, size); }

void Free(void* memory) { return free(memory); }

void* Calloc(size_t count, size_t size) { return calloc(count, size); }

void* Memcpy(void* dest, const void* source, size_t count) {
  return memcpy(dest, source, count);
}

FILE* Fopen(const char* filename, const char* mode) {
  return fopen(filename, mode);
}

int Fclose(FILE* stream) { return fclose(stream); }

}  // namespace base
}  // namespace v8
