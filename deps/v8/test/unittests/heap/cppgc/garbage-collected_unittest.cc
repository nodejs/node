// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/garbage-collected.h"

#include "include/cppgc/allocation.h"
#include "src/heap/cppgc/heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class GCed : public GarbageCollected<GCed> {};
class NotGCed {};
class Mixin : public GarbageCollectedMixin {};
class GCedWithMixin : public GarbageCollected<GCedWithMixin>, public Mixin {
  USING_GARBAGE_COLLECTED_MIXIN();
};
class OtherMixin : public GarbageCollectedMixin {};
class MergedMixins : public Mixin, public OtherMixin {
  MERGE_GARBAGE_COLLECTED_MIXINS();

 public:
  void Trace(cppgc::Visitor* visitor) const override {
    Mixin::Trace(visitor);
    OtherMixin::Trace(visitor);
  }
};
class GCWithMergedMixins : public GCed, public MergedMixins {
  USING_GARBAGE_COLLECTED_MIXIN();

 public:
  void Trace(cppgc::Visitor* visitor) const override {
    MergedMixins::Trace(visitor);
  }
};

class GarbageCollectedTestWithHeap
    : public testing::TestSupportingAllocationOnly {};

}  // namespace

TEST(GarbageCollectedTest, GarbageCollectedTrait) {
  STATIC_ASSERT(!IsGarbageCollectedType<int>::value);
  STATIC_ASSERT(!IsGarbageCollectedType<NotGCed>::value);
  STATIC_ASSERT(IsGarbageCollectedType<GCed>::value);
  STATIC_ASSERT(IsGarbageCollectedType<Mixin>::value);
  STATIC_ASSERT(IsGarbageCollectedType<GCedWithMixin>::value);
  STATIC_ASSERT(IsGarbageCollectedType<MergedMixins>::value);
  STATIC_ASSERT(IsGarbageCollectedType<GCWithMergedMixins>::value);
}

TEST(GarbageCollectedTest, GarbageCollectedMixinTrait) {
  STATIC_ASSERT(!IsGarbageCollectedMixinType<int>::value);
  STATIC_ASSERT(!IsGarbageCollectedMixinType<GCed>::value);
  STATIC_ASSERT(!IsGarbageCollectedMixinType<NotGCed>::value);
  STATIC_ASSERT(IsGarbageCollectedMixinType<Mixin>::value);
  STATIC_ASSERT(IsGarbageCollectedMixinType<GCedWithMixin>::value);
  STATIC_ASSERT(IsGarbageCollectedMixinType<MergedMixins>::value);
  STATIC_ASSERT(IsGarbageCollectedMixinType<GCWithMergedMixins>::value);
}

TEST_F(GarbageCollectedTestWithHeap, GetObjectStartReturnsCorrentAddress) {
  GCed* gced = MakeGarbageCollected<GCed>(GetHeap());
  GCedWithMixin* gced_with_mixin =
      MakeGarbageCollected<GCedWithMixin>(GetHeap());
  EXPECT_EQ(gced_with_mixin, static_cast<Mixin*>(gced_with_mixin)
                                 ->GetTraceDescriptor()
                                 .base_object_payload);
  EXPECT_NE(gced, static_cast<Mixin*>(gced_with_mixin)
                      ->GetTraceDescriptor()
                      .base_object_payload);
}

}  // namespace internal
}  // namespace cppgc
