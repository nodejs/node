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

TEST_F(TestingTest, OverrideEmbeddertackStateScope) {
  {
    auto* gced = MakeGarbageCollected<GCed>(GetHeap()->GetAllocationHandle());
    WeakPersistent<GCed> weak{gced};
    internal::Heap::From(GetHeap())->CollectGarbage(
        Heap::Config::PreciseAtomicConfig());
    EXPECT_FALSE(weak);
  }
  {
    auto* gced = MakeGarbageCollected<GCed>(GetHeap()->GetAllocationHandle());
    WeakPersistent<GCed> weak{gced};
    cppgc::testing::OverrideEmbedderStackStateScope override_stack(
        GetHeap()->GetHeapHandle(),
        EmbedderStackState::kMayContainHeapPointers);
    internal::Heap::From(GetHeap())->CollectGarbage(
        Heap::Config::PreciseAtomicConfig());
    EXPECT_TRUE(weak);
  }
  {
    auto* gced = MakeGarbageCollected<GCed>(GetHeap()->GetAllocationHandle());
    WeakPersistent<GCed> weak{gced};
    cppgc::testing::OverrideEmbedderStackStateScope override_stack(
        GetHeap()->GetHeapHandle(), EmbedderStackState::kNoHeapPointers);
    internal::Heap::From(GetHeap())->CollectGarbage(
        Heap::Config::ConservativeAtomicConfig());
    EXPECT_FALSE(weak);
  }
}

}  // namespace internal
}  // namespace cppgc
