// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/v8.h"

#include "src/heap/concurrent-marking.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact.h"
#include "src/heap/worklist.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

void PublishSegment(ConcurrentMarking::MarkingWorklist* worklist,
                    HeapObject* object) {
  for (size_t i = 0; i <= ConcurrentMarking::MarkingWorklist::kSegmentCapacity;
       i++) {
    worklist->Push(0, object);
  }
  CHECK(worklist->Pop(0, &object));
}

TEST(ConcurrentMarking) {
  if (!i::FLAG_concurrent_marking) return;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  ConcurrentMarking::MarkingWorklist shared, bailout;
  WeakObjects weak_objects;
  ConcurrentMarking* concurrent_marking =
      new ConcurrentMarking(heap, &shared, &bailout, &weak_objects);
  PublishSegment(&shared, heap->undefined_value());
  concurrent_marking->ScheduleTasks();
  concurrent_marking->EnsureCompleted();
  delete concurrent_marking;
}

TEST(ConcurrentMarkingReschedule) {
  if (!i::FLAG_concurrent_marking) return;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  ConcurrentMarking::MarkingWorklist shared, bailout;
  WeakObjects weak_objects;
  ConcurrentMarking* concurrent_marking =
      new ConcurrentMarking(heap, &shared, &bailout, &weak_objects);
  PublishSegment(&shared, heap->undefined_value());
  concurrent_marking->ScheduleTasks();
  concurrent_marking->EnsureCompleted();
  PublishSegment(&shared, heap->undefined_value());
  concurrent_marking->RescheduleTasksIfNeeded();
  concurrent_marking->EnsureCompleted();
  delete concurrent_marking;
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
