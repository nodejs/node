// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/prefinalizer.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "src/heap/cppgc/heap-object-header-inl.h"
#include "src/heap/cppgc/heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class PrefinalizerTest : public testing::TestWithHeap {};

class GCed : public GarbageCollected<GCed> {
  CPPGC_USING_PRE_FINALIZER(GCed, PreFinalizer);

 public:
  void Trace(Visitor*) const {}
  void PreFinalizer() { ++prefinalizer_callcount; }

  static size_t prefinalizer_callcount;
};
size_t GCed::prefinalizer_callcount = 0;

}  // namespace

TEST_F(PrefinalizerTest, PrefinalizerCalledOnDeadObject) {
  GCed::prefinalizer_callcount = 0;
  auto* object = MakeGarbageCollected<GCed>(GetHeap());
  USE(object);
  EXPECT_EQ(0u, GCed::prefinalizer_callcount);
  PreciseGC();
  EXPECT_EQ(1u, GCed::prefinalizer_callcount);
  PreciseGC();
  EXPECT_EQ(1u, GCed::prefinalizer_callcount);
}

TEST_F(PrefinalizerTest, PrefinalizerNotCalledOnLiveObject) {
  GCed::prefinalizer_callcount = 0;
  auto* object = MakeGarbageCollected<GCed>(GetHeap());
  HeapObjectHeader::FromPayload(object).TryMarkAtomic();
  EXPECT_EQ(0u, GCed::prefinalizer_callcount);
  PreciseGC();
  EXPECT_EQ(0u, GCed::prefinalizer_callcount);
  PreciseGC();
  EXPECT_EQ(1u, GCed::prefinalizer_callcount);
}

namespace {

class Mixin : public GarbageCollectedMixin {
  CPPGC_USING_PRE_FINALIZER(Mixin, PreFinalizer);

 public:
  void PreFinalizer() { ++prefinalizer_callcount; }

  static size_t prefinalizer_callcount;
};
size_t Mixin::prefinalizer_callcount = 0;

class GCedWithMixin : public GarbageCollected<GCedWithMixin>, public Mixin {
  USING_GARBAGE_COLLECTED_MIXIN();
};

}  // namespace

TEST_F(PrefinalizerTest, PrefinalizerCalledOnDeadMixinObject) {
  Mixin::prefinalizer_callcount = 0;
  auto* object = MakeGarbageCollected<GCedWithMixin>(GetHeap());
  USE(object);
  EXPECT_EQ(0u, Mixin::prefinalizer_callcount);
  PreciseGC();
  EXPECT_EQ(1u, Mixin::prefinalizer_callcount);
  PreciseGC();
  EXPECT_EQ(1u, Mixin::prefinalizer_callcount);
}

TEST_F(PrefinalizerTest, PrefinalizerNotCalledOnLiveMixinObject) {
  Mixin::prefinalizer_callcount = 0;
  auto* object = MakeGarbageCollected<GCedWithMixin>(GetHeap());
  HeapObjectHeader::FromPayload(object).TryMarkAtomic();
  EXPECT_EQ(0u, Mixin::prefinalizer_callcount);
  PreciseGC();
  EXPECT_EQ(0u, Mixin::prefinalizer_callcount);
  PreciseGC();
  EXPECT_EQ(1u, Mixin::prefinalizer_callcount);
}

namespace {

class BaseMixin : public GarbageCollectedMixin {
  CPPGC_USING_PRE_FINALIZER(BaseMixin, PreFinalizer);

 public:
  void PreFinalizer();

  static size_t prefinalizer_callcount;
};
size_t BaseMixin::prefinalizer_callcount = 0;

class InheritingMixin : public BaseMixin {
  CPPGC_USING_PRE_FINALIZER(InheritingMixin, PreFinalizer);

 public:
  void PreFinalizer();

  static size_t prefinalizer_callcount;
};
size_t InheritingMixin::prefinalizer_callcount = 0;

class GCedWithMixins : public GarbageCollected<GCedWithMixins>,
                       public InheritingMixin {
  USING_GARBAGE_COLLECTED_MIXIN();
  CPPGC_USING_PRE_FINALIZER(GCedWithMixins, PreFinalizer);

 public:
  void PreFinalizer();

  static size_t prefinalizer_callcount;
};
size_t GCedWithMixins::prefinalizer_callcount = 0;

void BaseMixin::PreFinalizer() {
  EXPECT_EQ(1u, GCedWithMixins::prefinalizer_callcount);
  EXPECT_EQ(1u, InheritingMixin::prefinalizer_callcount);
  EXPECT_EQ(0u, BaseMixin::prefinalizer_callcount);
  ++BaseMixin::prefinalizer_callcount;
}

void InheritingMixin::PreFinalizer() {
  EXPECT_EQ(1u, GCedWithMixins::prefinalizer_callcount);
  EXPECT_EQ(0u, InheritingMixin::prefinalizer_callcount);
  EXPECT_EQ(0u, BaseMixin::prefinalizer_callcount);
  InheritingMixin::prefinalizer_callcount = true;
}

void GCedWithMixins::PreFinalizer() {
  EXPECT_EQ(0u, GCedWithMixins::prefinalizer_callcount);
  EXPECT_EQ(0u, InheritingMixin::prefinalizer_callcount);
  EXPECT_EQ(0u, BaseMixin::prefinalizer_callcount);
  GCedWithMixins::prefinalizer_callcount = true;
}
}  // namespace

TEST_F(PrefinalizerTest, PrefinalizerInvocationPreservesOrder) {
  BaseMixin::prefinalizer_callcount = 0;
  InheritingMixin::prefinalizer_callcount = 0;
  GCedWithMixins::prefinalizer_callcount = 0;
  auto* object = MakeGarbageCollected<GCedWithMixins>(GetHeap());
  USE(object);
  EXPECT_EQ(0u, GCedWithMixins::prefinalizer_callcount);
  EXPECT_EQ(0u, InheritingMixin::prefinalizer_callcount);
  EXPECT_EQ(0u, BaseMixin::prefinalizer_callcount);
  PreciseGC();
  EXPECT_EQ(1u, GCedWithMixins::prefinalizer_callcount);
  EXPECT_EQ(1u, InheritingMixin::prefinalizer_callcount);
  EXPECT_EQ(1u, BaseMixin::prefinalizer_callcount);
  PreciseGC();
  EXPECT_EQ(1u, GCedWithMixins::prefinalizer_callcount);
  EXPECT_EQ(1u, InheritingMixin::prefinalizer_callcount);
  EXPECT_EQ(1u, BaseMixin::prefinalizer_callcount);
}

namespace {

class AllocatingPrefinalizer : public GarbageCollected<AllocatingPrefinalizer> {
  CPPGC_USING_PRE_FINALIZER(AllocatingPrefinalizer, PreFinalizer);

 public:
  explicit AllocatingPrefinalizer(cppgc::Heap* heap) : heap_(heap) {}
  void Trace(Visitor*) const {}
  void PreFinalizer() { MakeGarbageCollected<GCed>(heap_); }

 private:
  cppgc::Heap* heap_;
};

}  // namespace

#ifdef DEBUG

TEST_F(PrefinalizerTest, PrefinalizerFailsOnAllcoation) {
  auto* object =
      MakeGarbageCollected<AllocatingPrefinalizer>(GetHeap(), GetHeap());
  USE(object);
  EXPECT_DEATH_IF_SUPPORTED(PreciseGC(), "");
}

#endif  // DEBUG

}  // namespace internal
}  // namespace cppgc
