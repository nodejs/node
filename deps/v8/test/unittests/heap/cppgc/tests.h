// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_HEAP_CPPGC_TESTS_H_
#define V8_UNITTESTS_HEAP_CPPGC_TESTS_H_

#include "include/cppgc/heap.h"
#include "include/cppgc/platform.h"
#include "src/heap/cppgc/heap.h"
#include "test/unittests/heap/cppgc/test-platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {
namespace testing {

class TestWithPlatform : public ::testing::Test {
 protected:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

  TestPlatform& GetPlatform() const { return *platform_; }

  std::shared_ptr<TestPlatform> GetPlatformHandle() const { return platform_; }

 protected:
  static std::shared_ptr<TestPlatform> platform_;
};

class TestWithHeap : public TestWithPlatform {
 protected:
  TestWithHeap();

  void PreciseGC() {
    heap_->ForceGarbageCollectionSlow(
        ::testing::UnitTest::GetInstance()->current_test_info()->name(),
        "Testing", cppgc::Heap::StackState::kNoHeapPointers);
  }

  cppgc::Heap* GetHeap() const { return heap_.get(); }

  cppgc::AllocationHandle& GetAllocationHandle() const {
    return allocation_handle_;
  }

  std::unique_ptr<MarkerBase>& GetMarkerRef() {
    return Heap::From(GetHeap())->marker_;
  }

  void ResetLinearAllocationBuffers();

 private:
  std::unique_ptr<cppgc::Heap> heap_;
  cppgc::AllocationHandle& allocation_handle_;
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
