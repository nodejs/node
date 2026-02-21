// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/testing.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/persistent.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {
class TestingTest : public testing::TestWithHeap {};

class GCed : public GarbageCollected<GCed> {
 public:
  void Trace(Visitor*) const {}
};
}  // namespace

TEST_F(TestingTest,
       OverrideEmbeddertackStateScopeDoesNotOverrideExplicitCalls) {
  {
    auto* gced = MakeGarbageCollected<GCed>(GetHeap()->GetAllocationHandle());
    WeakPersistent<GCed> weak{gced};
    internal::Heap::From(GetHeap())->CollectGarbage(
        GCConfig::PreciseAtomicConfig());
    EXPECT_FALSE(weak);
  }
  {
    auto* gced = MakeGarbageCollected<GCed>(GetHeap()->GetAllocationHandle());
    WeakPersistent<GCed> weak{gced};
    cppgc::testing::OverrideEmbedderStackStateScope override_stack(
        GetHeap()->GetHeapHandle(),
        EmbedderStackState::kMayContainHeapPointers);
    internal::Heap::From(GetHeap())->CollectGarbage(
        GCConfig::PreciseAtomicConfig());
    EXPECT_FALSE(weak);
  }
  {
    auto* gced = MakeGarbageCollected<GCed>(GetHeap()->GetAllocationHandle());
    WeakPersistent<GCed> weak{gced};
    cppgc::testing::OverrideEmbedderStackStateScope override_stack(
        GetHeap()->GetHeapHandle(), EmbedderStackState::kNoHeapPointers);
    internal::Heap::From(GetHeap())->CollectGarbage(
        GCConfig::ConservativeAtomicConfig());
    EXPECT_TRUE(weak);
  }
}

TEST_F(TestingTest, StandaloneTestingHeap) {
  // Perform garbage collection through the StandaloneTestingHeap API.
  cppgc::testing::StandaloneTestingHeap heap(GetHeap()->GetHeapHandle());
  heap.StartGarbageCollection();
  heap.PerformMarkingStep(EmbedderStackState::kNoHeapPointers);
  heap.FinalizeGarbageCollection(EmbedderStackState::kNoHeapPointers);
}

}  // namespace internal
}  // namespace cppgc
