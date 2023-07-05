// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_FLUSH_INSTRUCTION_CACHE_H_
#define V8_CODEGEN_FLUSH_INSTRUCTION_CACHE_H_

#include "include/v8-internal.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

V8_EXPORT_PRIVATE void FlushInstructionCache(void* start, size_t size);
V8_EXPORT_PRIVATE V8_INLINE void FlushInstructionCache(Address start,
                                                       size_t size) {
  return FlushInstructionCache(reinterpret_cast<void*>(start), size);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_FLUSH_INSTRUCTION_CACHE_H_
