// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_COMMON_ASSEMBLER_TESTER_H_
#define V8_TEST_COMMON_ASSEMBLER_TESTER_H_

#include "src/codegen/assembler.h"
#include "src/codegen/code-desc.h"

namespace v8 {
namespace internal {

class TestingAssemblerBuffer : public AssemblerBuffer {
 public:
  TestingAssemblerBuffer(size_t requested, void* address) {
    size_t page_size = v8::internal::AllocatePageSize();
    size_t alloc_size = RoundUp(requested, page_size);
    CHECK_GE(kMaxInt, alloc_size);
    size_ = static_cast<int>(alloc_size);
    buffer_ = static_cast<byte*>(AllocatePages(GetPlatformPageAllocator(),
                                               address, alloc_size, page_size,
                                               v8::PageAllocator::kReadWrite));
    CHECK_NOT_NULL(buffer_);
  }

  ~TestingAssemblerBuffer() {
    CHECK(FreePages(GetPlatformPageAllocator(), buffer_, size_));
  }

  byte* start() const override { return buffer_; }

  int size() const override { return size_; }

  std::unique_ptr<AssemblerBuffer> Grow(int new_size) override {
    FATAL("Cannot grow TestingAssemblerBuffer");
  }

  std::unique_ptr<AssemblerBuffer> CreateView() const {
    return ExternalAssemblerBuffer(buffer_, size_);
  }

  void MakeExecutable() {
    // Flush the instruction cache as part of making the buffer executable.
    // Note: we do this before setting permissions to ReadExecute because on
    // some older ARM kernels there is a bug which causes an access error on
    // cache flush instructions to trigger access error on non-writable memory.
    // See https://bugs.chromium.org/p/v8/issues/detail?id=8157
    FlushInstructionCache(buffer_, size_);

    bool result = SetPermissions(GetPlatformPageAllocator(), buffer_, size_,
                                 v8::PageAllocator::kReadExecute);
    CHECK(result);
  }

  void MakeWritable() {
    bool result = SetPermissions(GetPlatformPageAllocator(), buffer_, size_,
                                 v8::PageAllocator::kReadWrite);
    CHECK(result);
  }

  // TODO(wasm): Only needed for the "test-jump-table-assembler.cc" tests.
  void MakeWritableAndExecutable() {
    bool result = SetPermissions(GetPlatformPageAllocator(), buffer_, size_,
                                 v8::PageAllocator::kReadWriteExecute);
    CHECK(result);
  }

 private:
  byte* buffer_;
  int size_;
};

static inline std::unique_ptr<TestingAssemblerBuffer> AllocateAssemblerBuffer(
    size_t requested = v8::internal::AssemblerBase::kMinimalBufferSize,
    void* address = nullptr) {
  return base::make_unique<TestingAssemblerBuffer>(requested, address);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TEST_COMMON_ASSEMBLER_TESTER_H_
