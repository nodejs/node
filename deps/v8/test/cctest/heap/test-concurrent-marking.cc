// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/v8.h"

#include "src/heap/concurrent-marking.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
TEST(ConcurrentMarking) {
  if (!i::FLAG_concurrent_marking) return;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  ConcurrentMarkingDeque deque(heap);
  deque.Push(heap->undefined_value());
  ConcurrentMarking* concurrent_marking = new ConcurrentMarking(heap, &deque);
  concurrent_marking->StartTask();
  concurrent_marking->WaitForTaskToComplete();
  delete concurrent_marking;
}

}  // namespace internal
}  // namespace v8
