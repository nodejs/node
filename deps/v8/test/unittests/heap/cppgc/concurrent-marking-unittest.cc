// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"
#include "include/cppgc/default-platform.h"
#include "include/cppgc/heap-consistency.h"
#include "include/cppgc/liveness-broker.h"
#include "include/cppgc/member.h"
#include "include/cppgc/persistent.h"
#include "include/cppgc/testing.h"
#include "include/cppgc/trace-trait.h"
#include "src/heap/cppgc-internal/globals.h"
#include "src/heap/cppgc-internal/marker.h"
#include "src/heap/cppgc-internal/marking-visitor.h"
#include "src/heap/cppgc-internal/stats-collector.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(CPPGC_IS_STANDALONE)
#include "src/common/synchronization-point-support.h"
#endif  // !defined(CPPGC_IS_STANDALONE)

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
            sizeof(NotConcurrentlyTraceable))) {
      return;
    }
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

#if !defined(CPPGC_IS_STANDALONE)

namespace {

class WeakTableBacking : public GarbageCollected<WeakTableBacking> {
 public:
  explicit WeakTableBacking(GCed* value) : value_(value) {}
  void Trace(cppgc::Visitor* visitor) const { visitor->TraceStrongly(value_); }

  CPPGC_PLUGIN_IGNORE("Traced strongly through TraceStrongly")
  WeakMember<GCed> value_;
};

class WeakTable {
  CPPGC_DISALLOW_NEW();

 public:
  static constexpr std::string_view kSyncPoint =
      "cppgc.ConcurrentMarkingTest.TraceWeakTable";

  WeakTable() = default;
  explicit WeakTable(AllocationHandle& handle)
      : backing_(MakeGarbageCollected<WeakTableBacking>(
            handle, MakeGarbageCollected<GCed>(handle))) {}

  void Trace(cppgc::Visitor* visitor) const {
    const WeakTableBacking* backing = backing_.Get();
    SYNCHRONIZATION_POINT(kSyncPoint);
    visitor->TraceWeakContainer(backing, WeakCallback, this);
  }

  void Swap(WeakTable& other) {
    backing_.Swap(other.backing_);
    WriteBarrier(backing_);
    WriteBarrier(other.backing_);
  }

 private:
  static void WeakCallback(const LivenessBroker& broker, const void* data) {
    const WeakTable* self = static_cast<const WeakTable*>(data);
    if (self->backing_ && !broker.IsHeapObjectAlive(self->backing_->value_)) {
      self->backing_->value_.Clear();
    }
  }

  static void WriteBarrier(Member<WeakTableBacking>& slot) {
    WeakTableBacking* value = slot.Get();
    if (!value) return;
    subtle::HeapConsistency::WriteBarrierParams params;
    const auto barrier_type =
        subtle::HeapConsistency::GetWriteBarrierType(slot, params);
    DCHECK(barrier_type ==
               subtle::HeapConsistency::WriteBarrierType::kMarking ||
           barrier_type == subtle::HeapConsistency::WriteBarrierType::kNone);
    if (barrier_type == subtle::HeapConsistency::WriteBarrierType::kMarking) {
      subtle::HeapConsistency::DijkstraWriteBarrier(params, value);
    }
    if (subtle::HeapConsistency::GetWriteBarrierType(value, params) ==
        subtle::HeapConsistency::WriteBarrierType::kMarking) {
      subtle::HeapConsistency::SteeleWriteBarrier(params, value);
    }
  }

  CPPGC_PLUGIN_IGNORE("Traced through TraceWeakContainer")
  Member<WeakTableBacking> backing_;
};

struct WeakTableHolder : public GarbageCollected<WeakTableHolder> {
  explicit WeakTableHolder(AllocationHandle& handle) : table(handle) {}
  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(table); }

  WeakTable table;
};

}  // namespace

}  // namespace internal

template <>
struct TraceTrait<internal::WeakTableBacking>
    : public internal::TraceTraitBase<internal::WeakTableBacking> {
  static TraceDescriptor GetWeakTraceDescriptor(const void* self) {
    return {self, nullptr};
  }
};

namespace internal {

TEST_F(ConcurrentMarkingTest, SwapWeakTableWithStackDuringConcurrentMarking) {
  // Regression test: https://crbug.com/575143870.
  static constexpr v8::base::TimeDelta kTimeout =
      v8::base::TimeDelta::FromSeconds(5);

  auto* sync_points = v8::internal::SynchronizationPointSupport::Get();
  sync_points->RequestBlockAt(std::string(WeakTable::kSyncPoint), kTimeout);

  Persistent<WeakTableHolder> holder = MakeGarbageCollected<WeakTableHolder>(
      GetAllocationHandle(), GetAllocationHandle());
  WeakTable stack_table;

  cppgc::testing::StandaloneTestingHeap testing_heap(GetHeapHandle());
  testing_heap.StartGarbageCollection();

  ASSERT_TRUE(sync_points->WaitUntilBlocked(WeakTable::kSyncPoint, kTimeout));

  holder->table.Swap(stack_table);

  ASSERT_TRUE(sync_points->Resume(WeakTable::kSyncPoint));

  testing_heap.FinalizeGarbageCollection(EmbedderStackState::kNoHeapPointers);

  ConservativeGC();
}

#endif  // !defined(CPPGC_IS_STANDALONE)

}  // namespace internal
}  // namespace cppgc
