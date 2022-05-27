// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/code-memory-access-inl.h"

namespace v8 {
namespace internal {

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT

thread_local int RwxMemoryWriteScope::code_space_write_nesting_level_ = 0;

RwxMemoryWriteScopeForTesting::RwxMemoryWriteScopeForTesting() {
  RwxMemoryWriteScope::SetWritable();
}

RwxMemoryWriteScopeForTesting::~RwxMemoryWriteScopeForTesting() {
  RwxMemoryWriteScope::SetExecutable();
}

#endif  // V8_HAS_PTHREAD_JIT_WRITE_PROTECT

}  // namespace internal
}  // namespace v8
