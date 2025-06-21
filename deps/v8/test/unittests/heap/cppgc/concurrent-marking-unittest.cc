// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"
#include "include/cppgc/default-platform.h"
#include "include/cppgc/member.h"
#include "include/cppgc/persistent.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/marker.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/stats-collector.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class ConcurrentMarkingTest : public testing::TestWithHeap {
 public:
#if defined(THREAD_SANITIZER)
  // Use more iteration on tsan builds to expose data races.
  static constexpr int kNumStep = 1000;
#else
  static constexpr int kNumStep = 10;
#endif  // defined(THREAD_SANITIZER)

  void StartConcurrentGC() {
    Heap* heap = Heap::From(GetHeap());
    heap->DisableHeapGrowingForTesting();
    heap->StartIncrementalGarbageCollection(
        GCConfig::PreciseConcurrentConfig());
    heap->marker()->SetMainThreadMarkingDisabledForTesting(true);
  }

  bool SingleStep(StackState stack_state) {
    MarkerBase* marker = Heap::From(GetHeap())->marker();
    DCHECK(marker);
    return marker->IncrementalMarkingStepForTesting(stack_state);
  }

  void FinishGC() {
    Heap* heap = Heap::From(GetHeap());
    heap->marker()->SetMainThreadMarkingDisabledForTesting(false);
    heap->FinalizeIncrementalGarbageCollectionIfRunning(
        GCConfig::PreciseConcurrentConfig());
  }
};

template <typename T>
struct GCedHolder : public GarbageCollected<GCedHolder<T>> {
  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(object); }
  Member<T> object;
};

class GCed : public GarbageCollected<GCed> {
 public:
  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(child_); }

  Member<GCed> child_;
};

class GCedWithCallback : public GarbageCollected<GCedWithCallback> {
 public:
  template <typename Callback>
  explicit GCedWithCallback(Callback callback) {
    callback(this);
  }

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(child_); }

  Member<GCedWithCallback> child_;
};

class Mixin : public GarbageCollectedMixin {
 public:
  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(child_); }

  Member<Mixin> child_;
};

class GCedWithMixin : public GarbageCollected<GCedWithMixin>, public Mixin {
 public:
  void Trace(cppgc::Visitor* visitor) const { Mixin::Trace(visitor); }
};

}  // namespace

// The following tests below check for data races during concurrent marking.

TEST_F(ConcurrentMarkingTest, MarkingObjects) {
  StartConcurrentGC();
  Persistent<GCedHolder<GCed>> root =
      MakeGarbageCollected<GCedHolder<GCed>>(GetAllocationHandle());
  Member<GCed>* last_object = &root->object;
  for (int i = 0; i < kNumStep; ++i) {
    for (int j = 0; j < kNumStep; ++j) {
      *last_object = MakeGarbageCollected<GCed>(GetAllocationHandle());
      last_object = &(*last_object)->child_;
    }
    // Use SingleStep to re-post concurrent jobs.
    SingleStep(StackState::kNoHeapPointers);
  }
  FinishGC();
}

TEST_F(ConcurrentMarkingTest, MarkingInConstructionObjects) {
  StartConcurrentGC();
  Persistent<GCedHolder<GCedWithCallback>> root =
      MakeGarbageCollected<GCedHolder<GCedWithCallback>>(GetAllocationHandle());
  Member<GCedWithCallback>* last_object = &root->object;
  for (int i = 0; i < kNumStep; ++i) {
    for (int j = 0; j < kNumStep; ++j) {
      MakeGarbageCollected<GCedWithCallback>(
          GetAllocationHandle(), [&last_object](GCedWithCallback* obj) {
            *last_object = obj;
            last_object = &(*last_object)->child_;
          });
    }
    // Use SingleStep to re-post concurrent jobs.
    SingleStep(StackState::kNoHeapPointers);
  }
  FinishGC();
}

TEST_F(ConcurrentMarkingTest, MarkingMixinObjects) {
  StartConcurrentGC();
  Persistent<GCedHolder<Mixin>> root =
      MakeGarbageCollected<GCedHolder<Mixin>>(GetAllocationHandle());
  Member<Mixin>* last_object = &root->object;
  for (int i = 0; i < kNumStep; ++i) {
    for (int j = 0; j < kNumStep; ++j) {
      *last_object = MakeGarbageCollected<GCedWithMixin>(GetAllocationHandle());
      last_object = &(*last_object)->child_;
    }
    // Use SingleStep to re-post concurrent jobs.
    SingleStep(StackState::kNoHeapPointers);
  }
  FinishGC();
}

namespace {

struct ConcurrentlyTraceable : public GarbageCollected<ConcurrentlyTraceable> {
  static size_t trace_counter;
  void Trace(Visitor*) const { ++trace_counter; }
};
size_t ConcurrentlyTraceable::trace_counter = 0;

struct NotConcurrentlyTraceable
    : public GarbageCollected<NotConcurrentlyTraceable> {
  static size_t trace_counter;
  void Trace(Visitor* visitor) const {
    if (visitor->DeferTraceToMutatorThreadIfConcurrent(
            this,
            [](Visitor*, const void*) {
              ++NotConcurrentlyTraceable::trace_counter;
            },
            sizeof(NotConcurrentlyTraceable)))
      return;
    ++trace_counter;
  }
};
size_t NotConcurrentlyTraceable::trace_counter = 0;

}  // namespace

TEST_F(ConcurrentMarkingTest, ConcurrentlyTraceableObjectIsTracedConcurrently) {
  Persistent<GCedHolder<ConcurrentlyTraceable>> root =
      MakeGarbageCollected<GCedHolder<ConcurrentlyTraceable>>(
          GetAllocationHandle());
  root->object =
      MakeGarbageCollected<ConcurrentlyTraceable>(GetAllocationHandle());
  EXPECT_EQ(0u, ConcurrentlyTraceable::trace_counter);
  StartConcurrentGC();
  GetMarkerRef()->WaitForConcurrentMarkingForTesting();
  EXPECT_NE(0u, ConcurrentlyTraceable::trace_counter);
  FinishGC();
}

TEST_F(ConcurrentMarkingTest,
       NotConcurrentlyTraceableObjectIsNotTracedConcurrently) {
  Persistent<GCedHolder<NotConcurrentlyTraceable>> root =
      MakeGarbageCollected<GCedHolder<NotConcurrentlyTraceable>>(
          GetAllocationHandle());
  root->object =
      MakeGarbageCollected<NotConcurrentlyTraceable>(GetAllocationHandle());
  EXPECT_EQ(0u, NotConcurrentlyTraceable::trace_counter);
  StartConcurrentGC();
  GetMarkerRef()->WaitForConcurrentMarkingForTesting();
  EXPECT_EQ(0u, NotConcurrentlyTraceable::trace_counter);
  FinishGC();
}

}  // namespace internal
}  // namespace cppgc
