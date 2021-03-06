// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-verifier.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/member.h"
#include "include/cppgc/persistent.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class MarkingVerifierTest : public testing::TestWithHeap {
 public:
  using StackState = Heap::Config::StackState;

  void VerifyMarking(HeapBase& heap, StackState stack_state) {
    Heap::From(GetHeap())->object_allocator().ResetLinearAllocationBuffers();
    MarkingVerifier verifier(heap);
    verifier.Run(stack_state);
  }
};

class GCed : public GarbageCollected<GCed> {
 public:
  void SetChild(GCed* child) { child_ = child; }
  void SetWeakChild(GCed* child) { weak_child_ = child; }
  GCed* child() const { return child_.Get(); }
  GCed* weak_child() const { return weak_child_.Get(); }
  void Trace(cppgc::Visitor* visitor) const {
    visitor->Trace(child_);
    visitor->Trace(weak_child_);
  }

 private:
  Member<GCed> child_;
  WeakMember<GCed> weak_child_;
};

template <typename T>
V8_NOINLINE T access(volatile const T& t) {
  return t;
}

}  // namespace

// Following tests should not crash.

TEST_F(MarkingVerifierTest, DoesntDieOnMarkedOnStackReference) {
  GCed* object = MakeGarbageCollected<GCed>(GetAllocationHandle());
  HeapObjectHeader::FromPayload(object).TryMarkAtomic();
  VerifyMarking(Heap::From(GetHeap())->AsBase(),
                StackState::kMayContainHeapPointers);
  access(object);
}

TEST_F(MarkingVerifierTest, DoesntDieOnMarkedMember) {
  Persistent<GCed> parent = MakeGarbageCollected<GCed>(GetAllocationHandle());
  HeapObjectHeader::FromPayload(parent.Get()).TryMarkAtomic();
  parent->SetChild(MakeGarbageCollected<GCed>(GetAllocationHandle()));
  HeapObjectHeader::FromPayload(parent->child()).TryMarkAtomic();
  VerifyMarking(Heap::From(GetHeap())->AsBase(), StackState::kNoHeapPointers);
}

TEST_F(MarkingVerifierTest, DoesntDieOnMarkedWeakMember) {
  Persistent<GCed> parent = MakeGarbageCollected<GCed>(GetAllocationHandle());
  HeapObjectHeader::FromPayload(parent.Get()).TryMarkAtomic();
  parent->SetWeakChild(MakeGarbageCollected<GCed>(GetAllocationHandle()));
  HeapObjectHeader::FromPayload(parent->weak_child()).TryMarkAtomic();
  VerifyMarking(Heap::From(GetHeap())->AsBase(), StackState::kNoHeapPointers);
}

namespace {

class GCedWithCallback : public GarbageCollected<GCedWithCallback> {
 public:
  template <typename Callback>
  explicit GCedWithCallback(Callback callback) {
    callback(this);
  }
  void Trace(cppgc::Visitor* visitor) const {}
};

}  // namespace

TEST_F(MarkingVerifierTest, DoesntDieOnInConstructionOnObject) {
  MakeGarbageCollected<GCedWithCallback>(
      GetAllocationHandle(), [this](GCedWithCallback* obj) {
        HeapObjectHeader::FromPayload(obj).TryMarkAtomic();
        VerifyMarking(Heap::From(GetHeap())->AsBase(),
                      StackState::kMayContainHeapPointers);
      });
}

namespace {
class GCedWithCallbackAndChild final
    : public GarbageCollected<GCedWithCallbackAndChild> {
 public:
  template <typename Callback>
  GCedWithCallbackAndChild(GCed* gced, Callback callback) : child_(gced) {
    callback(this);
  }
  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(child_); }

 private:
  Member<GCed> child_;
};

template <typename T>
struct Holder : public GarbageCollected<Holder<T>> {
 public:
  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(object); }
  Member<T> object = nullptr;
};
}  // namespace

TEST_F(MarkingVerifierTest, DoesntDieOnInConstructionObjectWithWriteBarrier) {
  // Regression test:  https://crbug.com/v8/10989.
  // GCedWithCallbackAndChild is marked by write barrier and then discarded by
  // FlushNotFullyConstructedObjects because it is already marked.
  Persistent<Holder<GCedWithCallbackAndChild>> persistent =
      MakeGarbageCollected<Holder<GCedWithCallbackAndChild>>(
          GetAllocationHandle());
  GarbageCollector::Config config =
      GarbageCollector::Config::PreciseIncrementalConfig();
  Heap::From(GetHeap())->StartIncrementalGarbageCollection(config);
  MakeGarbageCollected<GCedWithCallbackAndChild>(
      GetAllocationHandle(), MakeGarbageCollected<GCed>(GetAllocationHandle()),
      [&persistent](GCedWithCallbackAndChild* obj) {
        persistent->object = obj;
      });
  GetMarkerRef()->IncrementalMarkingStepForTesting(
      GarbageCollector::Config::StackState::kNoHeapPointers);
  Heap::From(GetHeap())->FinalizeIncrementalGarbageCollectionIfRunning(config);
}

// Death tests.

namespace {

class MarkingVerifierDeathTest : public MarkingVerifierTest {};

}  // namespace

TEST_F(MarkingVerifierDeathTest, DieOnUnmarkedOnStackReference) {
  GCed* object = MakeGarbageCollected<GCed>(GetAllocationHandle());
  EXPECT_DEATH_IF_SUPPORTED(VerifyMarking(Heap::From(GetHeap())->AsBase(),
                                          StackState::kMayContainHeapPointers),
                            "");
  access(object);
}

TEST_F(MarkingVerifierDeathTest, DieOnUnmarkedMember) {
  Persistent<GCed> parent = MakeGarbageCollected<GCed>(GetAllocationHandle());
  HeapObjectHeader::FromPayload(parent.Get()).TryMarkAtomic();
  parent->SetChild(MakeGarbageCollected<GCed>(GetAllocationHandle()));
  EXPECT_DEATH_IF_SUPPORTED(VerifyMarking(Heap::From(GetHeap())->AsBase(),
                                          StackState::kNoHeapPointers),
                            "");
}

TEST_F(MarkingVerifierDeathTest, DieOnUnmarkedWeakMember) {
  Persistent<GCed> parent = MakeGarbageCollected<GCed>(GetAllocationHandle());
  HeapObjectHeader::FromPayload(parent.Get()).TryMarkAtomic();
  parent->SetWeakChild(MakeGarbageCollected<GCed>(GetAllocationHandle()));
  EXPECT_DEATH_IF_SUPPORTED(VerifyMarking(Heap::From(GetHeap())->AsBase(),
                                          StackState::kNoHeapPointers),
                            "");
}

}  // namespace internal
}  // namespace cppgc
