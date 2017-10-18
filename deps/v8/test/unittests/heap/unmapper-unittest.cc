// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef __linux__
#include <sys/mman.h>
#undef MAP_TYPE
#endif  // __linux__

#include "src/heap/heap-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/isolate.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class SequentialUnmapperTest : public TestWithIsolate {
 public:
  SequentialUnmapperTest() = default;
  ~SequentialUnmapperTest() override = default;

  static void SetUpTestCase() {
    old_flag_ = i::FLAG_concurrent_sweeping;
    i::FLAG_concurrent_sweeping = false;
    TestWithIsolate::SetUpTestCase();
  }

  static void TearDownTestCase() {
    TestWithIsolate::TearDownTestCase();
    i::FLAG_concurrent_sweeping = old_flag_;
  }

  Heap* heap() { return isolate()->heap(); }
  MemoryAllocator* allocator() { return heap()->memory_allocator(); }
  MemoryAllocator::Unmapper* unmapper() { return allocator()->unmapper(); }

 private:
  static bool old_flag_;

  DISALLOW_COPY_AND_ASSIGN(SequentialUnmapperTest);
};

bool SequentialUnmapperTest::old_flag_;

#ifdef __linux__

// See v8:5945.
TEST_F(SequentialUnmapperTest, UnmapOnTeardownAfterAlreadyFreeingPooled) {
  Page* page =
      allocator()->AllocatePage(MemoryAllocator::PageAreaSize(OLD_SPACE),
                                static_cast<PagedSpace*>(heap()->old_space()),
                                Executability::NOT_EXECUTABLE);
  EXPECT_NE(nullptr, page);
  const int page_size = getpagesize();
  void* start_address = static_cast<void*>(page->address());
  EXPECT_EQ(0, msync(start_address, page_size, MS_SYNC));
  allocator()->Free<MemoryAllocator::kPooledAndQueue>(page);
  EXPECT_EQ(0, msync(start_address, page_size, MS_SYNC));
  unmapper()->FreeQueuedChunks();
  EXPECT_EQ(0, msync(start_address, page_size, MS_SYNC));
  unmapper()->TearDown();
  EXPECT_EQ(-1, msync(start_address, page_size, MS_SYNC));
}

// See v8:5945.
TEST_F(SequentialUnmapperTest, UnmapOnTeardown) {
  Page* page =
      allocator()->AllocatePage(MemoryAllocator::PageAreaSize(OLD_SPACE),
                                static_cast<PagedSpace*>(heap()->old_space()),
                                Executability::NOT_EXECUTABLE);
  EXPECT_NE(nullptr, page);
  const int page_size = getpagesize();
  void* start_address = static_cast<void*>(page->address());
  EXPECT_EQ(0, msync(start_address, page_size, MS_SYNC));
  allocator()->Free<MemoryAllocator::kPooledAndQueue>(page);
  EXPECT_EQ(0, msync(start_address, page_size, MS_SYNC));
  unmapper()->TearDown();
  EXPECT_EQ(-1, msync(start_address, page_size, MS_SYNC));
}

#endif  // __linux__

}  // namespace internal
}  // namespace v8
