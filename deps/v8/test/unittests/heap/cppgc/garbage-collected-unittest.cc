// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/garbage-collected.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/type-traits.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class GCed : public GarbageCollected<GCed> {
 public:
  void Trace(Visitor*) const {}
};
class NotGCed {};
class Mixin : public GarbageCollectedMixin {};
class GCedWithMixin : public GarbageCollected<GCedWithMixin>, public Mixin {};
class OtherMixin : public GarbageCollectedMixin {};
class MergedMixins : public Mixin, public OtherMixin {
 public:
  void Trace(cppgc::Visitor* visitor) const override {
    Mixin::Trace(visitor);
    OtherMixin::Trace(visitor);
  }
};
class GCWithMergedMixins : public GCed, public MergedMixins {
 public:
  void Trace(cppgc::Visitor* visitor) const override {
    MergedMixins::Trace(visitor);
  }
};

class GarbageCollectedTestWithHeap
    : public testing::TestSupportingAllocationOnly {};

}  // namespace

TEST(GarbageCollectedTest, GarbageCollectedTrait) {
  STATIC_ASSERT(!IsGarbageCollectedTypeV<int>);
  STATIC_ASSERT(!IsGarbageCollectedTypeV<NotGCed>);
  STATIC_ASSERT(IsGarbageCollectedTypeV<GCed>);
  STATIC_ASSERT(!IsGarbageCollectedTypeV<Mixin>);
  STATIC_ASSERT(IsGarbageCollectedTypeV<GCedWithMixin>);
  STATIC_ASSERT(!IsGarbageCollectedTypeV<MergedMixins>);
  STATIC_ASSERT(IsGarbageCollectedTypeV<GCWithMergedMixins>);
}

TEST(GarbageCollectedTest, GarbageCollectedMixinTrait) {
  STATIC_ASSERT(!IsGarbageCollectedMixinTypeV<int>);
  STATIC_ASSERT(!IsGarbageCollectedMixinTypeV<GCed>);
  STATIC_ASSERT(!IsGarbageCollectedMixinTypeV<NotGCed>);
  STATIC_ASSERT(IsGarbageCollectedMixinTypeV<Mixin>);
  STATIC_ASSERT(!IsGarbageCollectedMixinTypeV<GCedWithMixin>);
  STATIC_ASSERT(IsGarbageCollectedMixinTypeV<MergedMixins>);
  STATIC_ASSERT(!IsGarbageCollectedMixinTypeV<GCWithMergedMixins>);
}

TEST(GarbageCollectedTest, GarbageCollectedOrMixinTrait) {
  STATIC_ASSERT(!IsGarbageCollectedOrMixinTypeV<int>);
  STATIC_ASSERT(IsGarbageCollectedOrMixinTypeV<GCed>);
  STATIC_ASSERT(!IsGarbageCollectedOrMixinTypeV<NotGCed>);
  STATIC_ASSERT(IsGarbageCollectedOrMixinTypeV<Mixin>);
  STATIC_ASSERT(IsGarbageCollectedOrMixinTypeV<GCedWithMixin>);
  STATIC_ASSERT(IsGarbageCollectedOrMixinTypeV<MergedMixins>);
  STATIC_ASSERT(IsGarbageCollectedOrMixinTypeV<GCWithMergedMixins>);
}

TEST(GarbageCollectedTest, GarbageCollectedWithMixinTrait) {
  STATIC_ASSERT(!IsGarbageCollectedWithMixinTypeV<int>);
  STATIC_ASSERT(!IsGarbageCollectedWithMixinTypeV<GCed>);
  STATIC_ASSERT(!IsGarbageCollectedWithMixinTypeV<NotGCed>);
  STATIC_ASSERT(!IsGarbageCollectedWithMixinTypeV<Mixin>);
  STATIC_ASSERT(IsGarbageCollectedWithMixinTypeV<GCedWithMixin>);
  STATIC_ASSERT(!IsGarbageCollectedWithMixinTypeV<MergedMixins>);
  STATIC_ASSERT(IsGarbageCollectedWithMixinTypeV<GCWithMergedMixins>);
}

TEST_F(GarbageCollectedTestWithHeap, GetObjectStartReturnsCurrentAddress) {
  GCed* gced = MakeGarbageCollected<GCed>(GetAllocationHandle());
  GCedWithMixin* gced_with_mixin =
      MakeGarbageCollected<GCedWithMixin>(GetAllocationHandle());
  const void* base_object_payload = TraceTrait<Mixin>::GetTraceDescriptor(
                                        static_cast<Mixin*>(gced_with_mixin))
                                        .base_object_payload;
  EXPECT_EQ(gced_with_mixin, base_object_payload);
  EXPECT_NE(gced, base_object_payload);
}

namespace {

class GCedWithPostConstructionCallback final : public GCed {
 public:
  static size_t cb_callcount;
  GCedWithPostConstructionCallback() { cb_callcount = 0; }
};
size_t GCedWithPostConstructionCallback::cb_callcount;

class MixinWithPostConstructionCallback {
 public:
  static size_t cb_callcount;
  MixinWithPostConstructionCallback() { cb_callcount = 0; }
  using MarkerForMixinWithPostConstructionCallback = int;
};
size_t MixinWithPostConstructionCallback::cb_callcount;

class GCedWithMixinWithPostConstructionCallback final
    : public GCed,
      public MixinWithPostConstructionCallback {};

}  // namespace
}  // namespace internal

template <>
struct PostConstructionCallbackTrait<
    internal::GCedWithPostConstructionCallback> {
  static void Call(internal::GCedWithPostConstructionCallback* object) {
    EXPECT_FALSE(
        internal::HeapObjectHeader::FromPayload(object).IsInConstruction());
    internal::GCedWithPostConstructionCallback::cb_callcount++;
  }
};

template <typename T>
struct PostConstructionCallbackTrait<
    T,
    internal::void_t<typename T::MarkerForMixinWithPostConstructionCallback>> {
  // The parameter could just be T*.
  static void Call(
      internal::GCedWithMixinWithPostConstructionCallback* object) {
    EXPECT_FALSE(
        internal::HeapObjectHeader::FromPayload(object).IsInConstruction());
    internal::GCedWithMixinWithPostConstructionCallback::cb_callcount++;
  }
};

namespace internal {

TEST_F(GarbageCollectedTestWithHeap, PostConstructionCallback) {
  EXPECT_EQ(0u, GCedWithPostConstructionCallback::cb_callcount);
  MakeGarbageCollected<GCedWithPostConstructionCallback>(GetAllocationHandle());
  EXPECT_EQ(1u, GCedWithPostConstructionCallback::cb_callcount);
}

TEST_F(GarbageCollectedTestWithHeap, PostConstructionCallbackForMixin) {
  EXPECT_EQ(0u, MixinWithPostConstructionCallback::cb_callcount);
  MakeGarbageCollected<GCedWithMixinWithPostConstructionCallback>(
      GetAllocationHandle());
  EXPECT_EQ(1u, MixinWithPostConstructionCallback::cb_callcount);
}

}  // namespace internal
}  // namespace cppgc
