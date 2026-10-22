// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEAP_HEAP_TESTER_H_
#define HEAP_HEAP_TESTER_H_

#include "src/heap/allocation-result.h"
#include "src/objects/objects.h"

// Tests that should have access to private methods of {v8::internal::Heap}.
// Those tests need to be defined using HEAP_TEST(Name) { ... }.
#define HEAP_TEST_METHODS(V)                                \
  V(CompactionFullAbortedPage)                              \
  V(CompactionPartiallyAbortedPage)                         \
  V(CompactionPartiallyAbortedPageIntraAbortedPointers)     \
  V(CompactionPartiallyAbortedPageWithRememberedSetEntries) \
  V(StressHandles)

#define HEAP_TEST(Name)                                                   \
  CcTest register_test_##Name(v8::internal::heap::HeapTester::Test##Name, \
                              __FILE__, #Name, true, true);               \
  void v8::internal::heap::HeapTester::Test##Name()

namespace v8 {
namespace internal {
namespace heap {

class HeapTester {
 public:
#define DECLARE_STATIC(Name) static void Test##Name();

  HEAP_TEST_METHODS(DECLARE_STATIC)
#undef HEAP_TEST_METHODS

  // test-alloc.cc
  static DirectHandle<Object> TestAllocateAfterFailures();

  // test-api.cc
  static void ResetWeakHandle(bool global_gc);
};

}  // namespace heap
}  // namespace internal
}  // namespace v8

#endif  // HEAP_HEAP_TESTER_H_
