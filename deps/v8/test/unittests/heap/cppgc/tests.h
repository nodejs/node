// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_HEAP_CPPGC_TESTS_H_
#define V8_UNITTESTS_HEAP_CPPGC_TESTS_H_

#include "include/cppgc/heap.h"
#include "include/cppgc/platform.h"
#include "src/base/page-allocator.h"
#include "src/heap/cppgc/heap.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {
namespace testing {

class TestWithPlatform : public ::testing::Test {
 protected:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

 private:
  static std::unique_ptr<cppgc::PageAllocator> page_allocator_;
};

class TestWithHeap : public TestWithPlatform {
 protected:
  TestWithHeap();

  void PreciseGC() {
    heap_->ForceGarbageCollectionSlow(
        "TestWithHeap", "Testing", Heap::GCConfig::StackState::kNoHeapPointers);
  }

  cppgc::Heap* GetHeap() const { return heap_.get(); }

 private:
  std::unique_ptr<cppgc::Heap> heap_;
};

// Restrictive test fixture that supports allocation but will make sure no
// garbage collection is triggered. This is useful for writing idiomatic
// tests where object are allocated on the managed heap while still avoiding
// far reaching test consquences of full garbage collection calls.
class TestSupportingAllocationOnly : public TestWithHeap {
 protected:
  TestSupportingAllocationOnly();

 private:
  Heap::NoGCScope no_gc_scope_;
};

}  // namespace testing
}  // namespace internal
}  // namespace cppgc

#endif  // V8_UNITTESTS_HEAP_CPPGC_TESTS_H_
