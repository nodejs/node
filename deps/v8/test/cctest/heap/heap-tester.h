// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEAP_HEAP_TESTER_H_
#define HEAP_HEAP_TESTER_H_

#include "src/handles.h"
#include "src/heap/spaces.h"

// Tests that should have access to private methods of {v8::internal::Heap}.
// Those tests need to be defined using HEAP_TEST(Name) { ... }.
#define HEAP_TEST_METHODS(V)                              \
  V(CompactionFullAbortedPage)                            \
  V(CompactionPartiallyAbortedPage)                       \
  V(CompactionPartiallyAbortedPageIntraAbortedPointers)   \
  V(CompactionPartiallyAbortedPageWithStoreBufferEntries) \
  V(CompactionSpaceDivideMultiplePages)                   \
  V(CompactionSpaceDivideSinglePage)                      \
  V(GCFlags)                                              \
  V(MarkCompactCollector)                                 \
  V(NoPromotion)                                          \
  V(NumberStringCacheSize)                                \
  V(ObjectGroups)                                         \
  V(Promotion)                                            \
  V(Regression39128)                                      \
  V(ResetWeakHandle)                                      \
  V(StressHandles)                                        \
  V(TestMemoryReducerSampleJsCalls)                       \
  V(TestSizeOfObjects)                                    \
  V(WriteBarriersInCopyJSObject)


#define HEAP_TEST(Name)                                                       \
  CcTest register_test_##Name(v8::internal::HeapTester::Test##Name, __FILE__, \
                              #Name, NULL, true, true);                       \
  void v8::internal::HeapTester::Test##Name()


#define THREADED_HEAP_TEST(Name)                                             \
  RegisterThreadedTest register_##Name(v8::internal::HeapTester::Test##Name, \
                                       #Name);                               \
  /* */ HEAP_TEST(Name)


namespace v8 {
namespace internal {

class HeapTester {
 public:
#define DECLARE_STATIC(Name) static void Test##Name();

  HEAP_TEST_METHODS(DECLARE_STATIC)
#undef HEAP_TEST_METHODS

  /* test-alloc.cc */
  static AllocationResult AllocateAfterFailures();
  static Handle<Object> TestAllocateAfterFailures();

  /* test-api.cc */
  static void ResetWeakHandle(bool global_gc);

  /* test-spaces.cc */
  static CompactionSpaceCollection** InitializeCompactionSpaces(Heap* heap,
                                                                int num_spaces);
  static void DestroyCompactionSpaces(CompactionSpaceCollection** spaces,
                                      int num_spaces);
  static void MergeCompactionSpaces(PagedSpace* space,
                                    CompactionSpaceCollection** spaces,
                                    int num_spaces);
  static void AllocateInCompactionSpaces(CompactionSpaceCollection** spaces,
                                         AllocationSpace id, int num_spaces,
                                         int num_objects, int object_size);
  static void CompactionStats(CompactionSpaceCollection** spaces,
                              AllocationSpace id, int num_spaces,
                              intptr_t* capacity, intptr_t* size);
  static void TestCompactionSpaceDivide(int num_additional_objects,
                                        int object_size,
                                        int num_compaction_spaces,
                                        int additional_capacity_in_bytes);
};

}  // namespace internal
}  // namespace v8

#endif  // HEAP_HEAP_TESTER_H_
