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

#if defined(THREAD_SANITIZER)

namespace {

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

template <typename T>
class GCedHolder : public GarbageCollected<GCedHolder<T>> {
 public:
  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(object_); }

  Member<T> object_;
};

class ConcurrentMarkingTest : public testing::TestWithHeap {
 public:
  using Config = Heap::Config;
  static constexpr Config ConcurrentPreciseConfig = {
      Config::CollectionType::kMajor, Config::StackState::kNoHeapPointers,
      Config::MarkingType::kIncrementalAndConcurrent,
      Config::SweepingType::kIncrementalAndConcurrent};

  void StartConcurrentGC() {
    Heap* heap = Heap::From(GetHeap());
    heap->DisableHeapGrowingForTesting();
    heap->StartIncrementalGarbageCollection(ConcurrentPreciseConfig);
    heap->marker()->DisableIncrementalMarkingForTesting();
  }

  bool SingleStep(Config::StackState stack_state) {
    MarkerBase* marker = Heap::From(GetHeap())->marker();
    DCHECK(marker);
    return marker->IncrementalMarkingStepForTesting(stack_state);
  }

  void FinishSteps(Config::StackState stack_state) {
    while (!SingleStep(stack_state)) {
    }
  }

  void FinishGC() {
    Heap::From(GetHeap())->FinalizeIncrementalGarbageCollectionIfRunning(
        ConcurrentPreciseConfig);
  }
};

// static
constexpr ConcurrentMarkingTest::Config
    ConcurrentMarkingTest::ConcurrentPreciseConfig;

}  // namespace

// The following tests below check for data races during concurrent marking.

TEST_F(ConcurrentMarkingTest, MarkingObjects) {
  static constexpr int kNumStep = 1000;
  StartConcurrentGC();
  Persistent<GCedHolder<GCed>> root =
      MakeGarbageCollected<GCedHolder<GCed>>(GetAllocationHandle());
  Member<GCed>* last_object = &root->object_;
  for (int i = 0; i < kNumStep; ++i) {
    for (int j = 0; j < kNumStep; ++j) {
      *last_object = MakeGarbageCollected<GCed>(GetAllocationHandle());
      last_object = &(*last_object)->child_;
    }
    // Use SignleStep to re-post concurrent jobs.
    SingleStep(Config::StackState::kNoHeapPointers);
  }
  FinishGC();
}

TEST_F(ConcurrentMarkingTest, MarkingInConstructionObjects) {
  static constexpr int kNumStep = 1000;
  StartConcurrentGC();
  Persistent<GCedHolder<GCedWithCallback>> root =
      MakeGarbageCollected<GCedHolder<GCedWithCallback>>(GetAllocationHandle());
  Member<GCedWithCallback>* last_object = &root->object_;
  for (int i = 0; i < kNumStep; ++i) {
    for (int j = 0; j < kNumStep; ++j) {
      MakeGarbageCollected<GCedWithCallback>(
          GetAllocationHandle(), [&last_object](GCedWithCallback* obj) {
            *last_object = obj;
            last_object = &(*last_object)->child_;
          });
    }
    // Use SignleStep to re-post concurrent jobs.
    SingleStep(Config::StackState::kNoHeapPointers);
  }
  FinishGC();
}

TEST_F(ConcurrentMarkingTest, MarkingMixinObjects) {
  static constexpr int kNumStep = 1000;
  StartConcurrentGC();
  Persistent<GCedHolder<Mixin>> root =
      MakeGarbageCollected<GCedHolder<Mixin>>(GetAllocationHandle());
  Member<Mixin>* last_object = &root->object_;
  for (int i = 0; i < kNumStep; ++i) {
    for (int j = 0; j < kNumStep; ++j) {
      *last_object = MakeGarbageCollected<GCedWithMixin>(GetAllocationHandle());
      last_object = &(*last_object)->child_;
    }
    // Use SignleStep to re-post concurrent jobs.
    SingleStep(Config::StackState::kNoHeapPointers);
  }
  FinishGC();
}

#endif  // defined(THREAD_SANITIZER)

}  // namespace internal
}  // namespace cppgc
