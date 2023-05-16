// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/object-size-trait.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "src/heap/cppgc/heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class ObjectSizeTraitTest : public testing::TestWithHeap {};

class GCed : public GarbageCollected<GCed> {
 public:
  void Trace(Visitor*) const {}
};

class NotGCed {};
class Mixin : public GarbageCollectedMixin {};
class UnmanagedMixinWithDouble {
 protected:
  virtual void ForceVTable() {}
};
class GCedWithMixin : public GarbageCollected<GCedWithMixin>,
                      public UnmanagedMixinWithDouble,
                      public Mixin {};

}  // namespace

TEST_F(ObjectSizeTraitTest, GarbageCollected) {
  auto* obj = cppgc::MakeGarbageCollected<GCed>(GetAllocationHandle());
  EXPECT_GE(subtle::ObjectSizeTrait<GCed>::GetSize(*obj), sizeof(GCed));
}

TEST_F(ObjectSizeTraitTest, GarbageCollectedMixin) {
  auto* obj = cppgc::MakeGarbageCollected<GCedWithMixin>(GetAllocationHandle());
  Mixin& mixin = static_cast<Mixin&>(*obj);
  EXPECT_NE(static_cast<void*>(&mixin), obj);
  EXPECT_GE(subtle::ObjectSizeTrait<Mixin>::GetSize(mixin),
            sizeof(GCedWithMixin));
}

}  // namespace internal
}  // namespace cppgc
