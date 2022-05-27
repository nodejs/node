// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_CODE_MEMORY_ACCESS_INL_H_
#define V8_COMMON_CODE_MEMORY_ACCESS_INL_H_

#include "src/common/code-memory-access.h"
#include "src/logging/log.h"

namespace v8 {
namespace internal {

RwxMemoryWriteScope::RwxMemoryWriteScope(const char* comment) { SetWritable(); }

RwxMemoryWriteScope::~RwxMemoryWriteScope() { SetExecutable(); }

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT

// Ignoring this warning is considered better than relying on
// __builtin_available.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
// static
void RwxMemoryWriteScope::SetWritable() {
  if (code_space_write_nesting_level_ == 0) {
    pthread_jit_write_protect_np(0);
  }
  code_space_write_nesting_level_++;
}

// static
void RwxMemoryWriteScope::SetExecutable() {
  code_space_write_nesting_level_--;
  if (code_space_write_nesting_level_ == 0) {
    pthread_jit_write_protect_np(1);
  }
}
#pragma clang diagnostic pop

#else  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT

void RwxMemoryWriteScope::SetWritable() {}

void RwxMemoryWriteScope::SetExecutable() {}

#endif  // V8_HAS_PTHREAD_JIT_WRITE_PROTECT

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_CODE_MEMORY_ACCESS_INL_H_
