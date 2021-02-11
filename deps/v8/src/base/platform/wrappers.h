// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PLATFORM_WRAPPERS_H_
#define V8_BASE_PLATFORM_WRAPPERS_H_

#include <stddef.h>
#include <stdio.h>

namespace v8 {
namespace base {

void* Malloc(size_t size);

void* Realloc(void* memory, size_t size);

void Free(void* memory);

void* Calloc(size_t count, size_t size);

void* Memcpy(void* dest, const void* source, size_t count);

FILE* Fopen(const char* filename, const char* mode);

int Fclose(FILE* stream);

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_PLATFORM_WRAPPERS_H_
