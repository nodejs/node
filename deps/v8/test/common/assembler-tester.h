// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_COMMON_ASSEMBLER_TESTER_H_
#define V8_TEST_COMMON_ASSEMBLER_TESTER_H_

#include "src/assembler.h"

namespace v8 {
namespace internal {

static inline uint8_t* AllocateAssemblerBuffer(
    size_t* allocated,
    size_t requested = v8::internal::AssemblerBase::kMinimalBufferSize,
    void* address = nullptr) {
  size_t page_size = v8::internal::AllocatePageSize();
  size_t alloc_size = RoundUp(requested, page_size);
  void* result = v8::internal::AllocatePages(
      GetPlatformPageAllocator(), address, alloc_size, page_size,
      v8::PageAllocator::kReadWriteExecute);
  CHECK(result);
  *allocated = alloc_size;
  return static_cast<uint8_t*>(result);
}

static inline void MakeAssemblerBufferExecutable(uint8_t* buffer,
                                                 size_t allocated) {
  // Flush the instruction cache as part of making the buffer executable.
  // Note: we do this before setting permissions to ReadExecute because on
  // some older Arm64 kernels there is a bug which causes an access error on
  // cache flush instructions to trigger access error on non-writable memory.
  // See https://bugs.chromium.org/p/v8/issues/detail?id=8157
  Assembler::FlushICache(buffer, allocated);

  bool result =
      v8::internal::SetPermissions(GetPlatformPageAllocator(), buffer,
                                   allocated, v8::PageAllocator::kReadExecute);
  CHECK(result);
}

static inline void MakeAssemblerBufferWritable(uint8_t* buffer,
                                               size_t allocated) {
  bool result =
      v8::internal::SetPermissions(GetPlatformPageAllocator(), buffer,
                                   allocated, v8::PageAllocator::kReadWrite);
  CHECK(result);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TEST_COMMON_ASSEMBLER_TESTER_H_
