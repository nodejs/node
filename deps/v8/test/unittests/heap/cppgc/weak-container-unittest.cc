// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"
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
  using Config = Marker::MarkingConfig;

  void StartMarking() {
    Config config = {Config::CollectionType::kMajor,
                     Config::StackState::kNoHeapPointers,
                     Config::MarkingType::kIncremental};
    GetMarkerRef() = MarkerFactory::CreateAndStartMarking<Marker>(
        Heap::From(GetHeap())->AsBase(), GetPlatformHandle().get(), config);
  }

  void FinishMarking(Config::StackState stack_state) {
    GetMarkerRef()->FinishMarking(stack_state);
    GetMarkerRef().reset();
    Heap::From(GetHeap())->stats_collector()->NotifySweepingCompleted();
  }
};

class TraceableGCed : public GarbageCollected<TraceableGCed> {
 public:
  void Trace(cppgc::Visitor*) const { n_trace_calls++; }
  static size_t n_trace_calls;
};
size_t TraceableGCed::n_trace_calls = 0u;

class NonTraceableGCed : public GarbageCollected<NonTraceableGCed> {
 public:
  void Trace(cppgc::Visitor*) const { n_trace_calls++; }
  static size_t n_trace_calls;
};
size_t NonTraceableGCed::n_trace_calls = 0u;

void EmptyWeakCallback(const LivenessBroker&, const void*) {}

template <typename T>
V8_NOINLINE T access(volatile const T& t) {
  return t;
}

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
  TraceableGCed::n_trace_calls = 0u;
  StartMarking();
  GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback, nullptr);
  FinishMarking(Config::StackState::kNoHeapPointers);
  EXPECT_NE(0u, TraceableGCed::n_trace_calls);
  access(obj);
}

TEST_F(WeakContainerTest, NonTraceableGCedNotTraced) {
  NonTraceableGCed* obj =
      MakeGarbageCollected<NonTraceableGCed>(GetAllocationHandle());
  NonTraceableGCed::n_trace_calls = 0u;
  StartMarking();
  GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback, nullptr);
  FinishMarking(Config::StackState::kNoHeapPointers);
  EXPECT_EQ(0u, NonTraceableGCed::n_trace_calls);
  access(obj);
}

TEST_F(WeakContainerTest, NonTraceableGCedNotTracedConservatively) {
  NonTraceableGCed* obj =
      MakeGarbageCollected<NonTraceableGCed>(GetAllocationHandle());
  NonTraceableGCed::n_trace_calls = 0u;
  StartMarking();
  GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback, nullptr);
  FinishMarking(Config::StackState::kMayContainHeapPointers);
  EXPECT_NE(0u, NonTraceableGCed::n_trace_calls);
  access(obj);
}

TEST_F(WeakContainerTest, ConservativeGCTracesWeakContainer) {
  size_t trace_count_without_conservative;
  {
    TraceableGCed* obj =
        MakeGarbageCollected<TraceableGCed>(GetAllocationHandle());
    TraceableGCed::n_trace_calls = 0u;
    StartMarking();
    GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback,
                                                 nullptr);
    FinishMarking(Config::StackState::kNoHeapPointers);
    trace_count_without_conservative = TraceableGCed::n_trace_calls;
    access(obj);
  }
  {
    TraceableGCed* obj =
        MakeGarbageCollected<TraceableGCed>(GetAllocationHandle());
    TraceableGCed::n_trace_calls = 0u;
    StartMarking();
    GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback,
                                                 nullptr);
    FinishMarking(Config::StackState::kMayContainHeapPointers);
    EXPECT_LT(trace_count_without_conservative, TraceableGCed::n_trace_calls);
    access(obj);
  }
}

TEST_F(WeakContainerTest, ConservativeGCTracesWeakContainerOnce) {
  NonTraceableGCed* obj =
      MakeGarbageCollected<NonTraceableGCed>(GetAllocationHandle());
  NonTraceableGCed* copy_obj = obj;
  USE(copy_obj);
  NonTraceableGCed* another_copy_obj = obj;
  USE(another_copy_obj);
  NonTraceableGCed::n_trace_calls = 0u;
  StartMarking();
  GetMarkerRef()->Visitor().TraceWeakContainer(obj, EmptyWeakCallback, nullptr);
  FinishMarking(Config::StackState::kMayContainHeapPointers);
  EXPECT_EQ(1u, NonTraceableGCed::n_trace_calls);
  access(obj);
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
  FinishMarking(Config::StackState::kMayContainHeapPointers);
  EXPECT_NE(0u, WeakCallback::n_callback_called);
  EXPECT_EQ(obj, WeakCallback::obj);
}

}  // namespace internal
}  // namespace cppgc
