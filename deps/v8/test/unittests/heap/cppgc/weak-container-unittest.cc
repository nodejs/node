// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>

#include "include/cppgc/allocation.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/marker.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/stats-collector.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {
class WeakContainerTest : public testing::TestWithHeap {
 public:
  void StartMarking() {
    CHECK_EQ(0u,
             Heap::From(GetHeap())->AsBase().stats_collector()->marked_bytes());
    MarkingConfig config = {CollectionType::kMajor, StackState::kNoHeapPointers,
                            MarkingConfig::MarkingType::kIncremental};
    GetMarkerRef() = std::make_unique<Marker>(
        Heap::From(GetHeap())->AsBase(), GetPlatformHandle().get(), config);
    GetMarkerRef()->StartMarking();
  }

  void FinishMarking(StackState stack_state) {
    GetMarkerRef()->FinishMarking(stack_state);
    marked_bytes_ =
        Heap::From(GetHeap())->AsBase().stats_collector()->marked_bytes();
    GetMarkerRef().reset();
    Heap::From(GetHeap())->stats_collector()->NotifySweepingCompleted(
        GCConfig::SweepingType::kAtomic);
  }

  size_t GetMarkedBytes() const { return marked_bytes_; }

 private:
  size_t marked_bytes_ = 0;
};

template <typename T>
constexpr size_t SizeOf() {
  return RoundUp<kAllocationGranularity>(sizeof(T) + sizeof(HeapObjectHeader));
}

class TraceableGCed : public GarbageCollected<TraceableGCed> {
 public:
  void Trace(cppgc::Visitor*) const {
    reinterpret_cast<std::atomic<size_t>*>(&n_trace_calls)
        ->fetch_add(1, std::memory_order_relaxed);
  }
  mutable size_t n_trace_calls = 0;
};

class NonTraceableGCed : public GarbageCollected<NonTraceableGCed> {
 public:
  void Trace(cppgc::Visitor*) const { n_trace_calls++; }
  mutable size_t n_trace_calls = 0;
};

void EmptyWeakCallback(const LivenessBroker&, const void*) {}

}  // namespace

}  // namespace internal

template <>
struct TraceTrait<internal::TraceableGCed>
    : public internal::TraceTraitBase<internal::TraceableGCed> {
  static TraceDescriptor GetWeakTraceDescriptor(const void* self) {
    return {self, Trace};
  }
};

template <>
struct TraceTrait<internal::NonTraceableGCed>
    : public internal::TraceTraitBase<internal::NonTraceableGCed> {
  static TraceDescriptor GetWeakTraceDescriptor(const void* self) {
    return {self, nullptr};
  }
};

namespace internal {

TEST_F(WeakContainerTest, TraceableGCedTraced) {
  TraceableGCed* obj =
      MakeGarbageCollected<TraceableGCed>(GetAllocationHandle());
  obj->n_trace_calls = 0u;
  StartMarking();
  GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback, nullptr);
  FinishMarking(StackState::kNoHeapPointers);
  EXPECT_NE(0u, obj->n_trace_calls);
  EXPECT_EQ(SizeOf<TraceableGCed>(), GetMarkedBytes());
}

TEST_F(WeakContainerTest, NonTraceableGCedNotTraced) {
  NonTraceableGCed* obj =
      MakeGarbageCollected<NonTraceableGCed>(GetAllocationHandle());
  obj->n_trace_calls = 0u;
  StartMarking();
  GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback, nullptr);
  FinishMarking(StackState::kNoHeapPointers);
  EXPECT_EQ(0u, obj->n_trace_calls);
  EXPECT_EQ(SizeOf<NonTraceableGCed>(), GetMarkedBytes());
}

TEST_F(WeakContainerTest, NonTraceableGCedNotTracedConservatively) {
  NonTraceableGCed* obj =
      MakeGarbageCollected<NonTraceableGCed>(GetAllocationHandle());
  obj->n_trace_calls = 0u;
  StartMarking();
  GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback, nullptr);
  FinishMarking(StackState::kMayContainHeapPointers);
  EXPECT_NE(0u, obj->n_trace_calls);
  EXPECT_EQ(SizeOf<NonTraceableGCed>(), GetMarkedBytes());
}

TEST_F(WeakContainerTest, PreciseGCTracesWeakContainerWhenTraced) {
  TraceableGCed* obj =
      MakeGarbageCollected<TraceableGCed>(GetAllocationHandle());
  obj->n_trace_calls = 0u;
  StartMarking();
  GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback, nullptr);
  FinishMarking(StackState::kNoHeapPointers);
  EXPECT_EQ(1u, obj->n_trace_calls);
  EXPECT_EQ(SizeOf<TraceableGCed>(), GetMarkedBytes());
}

TEST_F(WeakContainerTest, ConservativeGCTracesWeakContainer) {
  TraceableGCed* obj =
      MakeGarbageCollected<TraceableGCed>(GetAllocationHandle());
  obj->n_trace_calls = 0u;
  StartMarking();
  GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback, nullptr);
  FinishMarking(StackState::kMayContainHeapPointers);
  EXPECT_EQ(2u, obj->n_trace_calls);
  EXPECT_EQ(SizeOf<TraceableGCed>(), GetMarkedBytes());
}

TEST_F(WeakContainerTest, ConservativeGCTracesWeakContainerOnce) {
  NonTraceableGCed* obj =
      MakeGarbageCollected<NonTraceableGCed>(GetAllocationHandle());
  NonTraceableGCed* copy_obj = obj;
  USE(copy_obj);
  NonTraceableGCed* another_copy_obj = obj;
  USE(another_copy_obj);
  obj->n_trace_calls = 0u;
  StartMarking();
  GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback, nullptr);
  FinishMarking(StackState::kMayContainHeapPointers);
  EXPECT_EQ(1u, obj->n_trace_calls);
  EXPECT_EQ(SizeOf<NonTraceableGCed>(), GetMarkedBytes());
}

namespace {

struct WeakCallback {
  static void callback(const LivenessBroker&, const void* data) {
    n_callback_called++;
    obj = data;
  }
  static size_t n_callback_called;
  static const void* obj;
};
size_t WeakCallback::n_callback_called = 0u;
const void* WeakCallback::obj = nullptr;

}  // namespace

TEST_F(WeakContainerTest, WeakContainerWeakCallbackCalled) {
  TraceableGCed* obj =
      MakeGarbageCollected<TraceableGCed>(GetAllocationHandle());
  WeakCallback::n_callback_called = 0u;
  WeakCallback::obj = nullptr;
  StartMarking();
  GetMarkerRef()->Visitor().TraceWeakContainer(obj, WeakCallback::callback,
                                               obj);
  FinishMarking(StackState::kMayContainHeapPointers);
  EXPECT_NE(0u, WeakCallback::n_callback_called);
  EXPECT_EQ(SizeOf<TraceableGCed>(), GetMarkedBytes());
  EXPECT_EQ(obj, WeakCallback::obj);
}

}  // namespace internal
}  // namespace cppgc
