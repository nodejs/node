// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/persistent.h"

#include <vector>

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/member.h"
#include "include/cppgc/type-traits.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/visitor.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

struct GCed : GarbageCollected<GCed> {
  static size_t trace_call_count;
  virtual void Trace(cppgc::Visitor*) const { ++trace_call_count; }
};
size_t GCed::trace_call_count = 0;

struct DerivedGCed : GCed {
  void Trace(cppgc::Visitor* v) const override { GCed::Trace(v); }
};

template <template <typename> class PersistentType>
PersistentRegion& GetRegion(cppgc::Heap* heap) {
  auto* heap_impl = internal::Heap::From(heap);
  return IsWeak<PersistentType<GCed>>::value
             ? heap_impl->GetWeakPersistentRegion()
             : heap_impl->GetStrongPersistentRegion();
}

template <typename T>
using LocalizedPersistent =
    internal::BasicPersistent<T, internal::StrongPersistentPolicy,
                              internal::KeepLocationPolicy,
                              internal::DefaultCheckingPolicy>;

class RootVisitor final : public VisitorBase {
 public:
  RootVisitor() = default;

  const auto& WeakCallbacks() const { return weak_callbacks_; }

  void ProcessWeakCallbacks() {
    const auto info = LivenessBrokerFactory::Create();
    for (const auto& cb : weak_callbacks_) {
      cb.first(info, cb.second);
    }
    weak_callbacks_.clear();
  }

 protected:
  void VisitRoot(const void* t, TraceDescriptor desc) final {
    desc.callback(this, desc.base_object_payload);
  }
  void VisitWeakRoot(const void*, TraceDescriptor, WeakCallback callback,
                     const void* object) final {
    weak_callbacks_.emplace_back(callback, object);
  }

 private:
  std::vector<std::pair<WeakCallback, const void*>> weak_callbacks_;
};
class PersistentTest : public testing::TestSupportingAllocationOnly {};

}  // namespace

template <template <typename> class PersistentType>
void NullStateCtor(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<Persistent>(heap).NodesInUse());
  {
    PersistentType<GCed> empty;
    EXPECT_EQ(nullptr, empty.Get());
    EXPECT_EQ(nullptr, empty.Release());
    EXPECT_EQ(0u, GetRegion<Persistent>(heap).NodesInUse());
  }
  {
    PersistentType<GCed> empty = nullptr;
    EXPECT_EQ(nullptr, empty.Get());
    EXPECT_EQ(nullptr, empty.Release());
    EXPECT_EQ(0u, GetRegion<Persistent>(heap).NodesInUse());
  }
  {
    PersistentType<GCed> empty = kSentinelPointer;
    EXPECT_EQ(kSentinelPointer, empty);
    EXPECT_EQ(kSentinelPointer, empty.Release());
    EXPECT_EQ(0u, GetRegion<Persistent>(heap).NodesInUse());
  }
  {
    // Runtime null must not allocated associated node.
    PersistentType<GCed> empty = static_cast<GCed*>(0);
    EXPECT_EQ(nullptr, empty.Get());
    EXPECT_EQ(nullptr, empty.Release());
    EXPECT_EQ(0u, GetRegion<Persistent>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, NullStateCtor) {
  auto* heap = GetHeap();
  NullStateCtor<Persistent>(heap);
  NullStateCtor<WeakPersistent>(heap);
}

template <template <typename> class PersistentType>
void RawCtor(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  GCed* gced = MakeGarbageCollected<GCed>(heap);
  {
    PersistentType<GCed> p = gced;
    EXPECT_EQ(gced, p.Get());
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p = *gced;
    EXPECT_EQ(gced, p.Get());
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, RawCtor) {
  auto* heap = GetHeap();
  RawCtor<Persistent>(heap);
  RawCtor<WeakPersistent>(heap);
}

template <template <typename> class PersistentType>
void CopyCtor(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p1 = MakeGarbageCollected<GCed>(heap);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    PersistentType<GCed> p2 = p1;
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(p1.Get(), p2.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p1;
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
    PersistentType<GCed> p2 = p1;
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(nullptr, p1.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<DerivedGCed> p1 = MakeGarbageCollected<DerivedGCed>(heap);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    PersistentType<GCed> p2 = p1;
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(p1.Get(), p2.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    static constexpr size_t kSlots = 512u;
    const PersistentType<GCed> prototype = MakeGarbageCollected<GCed>(heap);
    std::vector<PersistentType<GCed>> vector;
    vector.reserve(kSlots);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    for (size_t i = 0; i < kSlots; ++i) {
      vector.emplace_back(prototype);
      EXPECT_EQ(i + 2, GetRegion<PersistentType>(heap).NodesInUse());
    }
    vector.clear();
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, CopyCtor) {
  auto* heap = GetHeap();
  CopyCtor<Persistent>(heap);
  CopyCtor<WeakPersistent>(heap);
}

template <template <typename> class PersistentType>
void MoveCtor(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    GCed* gced = MakeGarbageCollected<GCed>(heap);
    PersistentType<GCed> p1 = gced;
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    PersistentType<GCed> p2 = std::move(p1);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(gced, p2.Get());
    // Moved-from-object is in the valid specified (nullptr) state.
    EXPECT_EQ(nullptr, p1.Get());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<DerivedGCed> p1 = MakeGarbageCollected<DerivedGCed>(heap);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    // Move ctor is not heterogeneous - fall back to copy ctor.
    PersistentType<GCed> p2 = std::move(p1);
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(p1.Get(), p2.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p1;
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
    PersistentType<GCed> p2 = std::move(p1);
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(nullptr, p1.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, MoveCtor) {
  auto* heap = GetHeap();
  MoveCtor<Persistent>(heap);
  MoveCtor<WeakPersistent>(heap);
}

template <template <typename> class PersistentType,
          template <typename> class MemberType>
void MemberCtor(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    MemberType<GCed> m = MakeGarbageCollected<GCed>(heap);
    PersistentType<GCed> p = m;
    EXPECT_EQ(m.Get(), p.Get());
    EXPECT_EQ(m, p);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, MemberCtor) {
  auto* heap = GetHeap();
  MemberCtor<Persistent, Member>(heap);
  MemberCtor<Persistent, WeakMember>(heap);
  MemberCtor<Persistent, UntracedMember>(heap);
  MemberCtor<WeakPersistent, Member>(heap);
  MemberCtor<WeakPersistent, WeakMember>(heap);
  MemberCtor<WeakPersistent, UntracedMember>(heap);
}

template <template <typename> class PersistentType>
void NullStateAssignemnt(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p = MakeGarbageCollected<GCed>(heap);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p = nullptr;
    EXPECT_EQ(nullptr, p.Get());
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  {
    PersistentType<GCed> p = MakeGarbageCollected<GCed>(heap);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p = kSentinelPointer;
    EXPECT_EQ(kSentinelPointer, p);
    EXPECT_EQ(kSentinelPointer, p.Get());
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  {
    PersistentType<GCed> p = MakeGarbageCollected<GCed>(heap);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p = static_cast<GCed*>(0);
    EXPECT_EQ(nullptr, p.Get());
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  }
}

TEST_F(PersistentTest, NullStateAssignemnt) {
  auto* heap = GetHeap();
  NullStateAssignemnt<Persistent>(heap);
  NullStateAssignemnt<WeakPersistent>(heap);
}

template <template <typename> class PersistentType>
void RawAssignment(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  GCed* gced = MakeGarbageCollected<GCed>(heap);
  {
    PersistentType<GCed> p;
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
    p = gced;
    EXPECT_EQ(gced, p.Get());
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p;
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
    p = *gced;
    EXPECT_EQ(gced, p.Get());
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, RawAssignment) {
  auto* heap = GetHeap();
  RawAssignment<Persistent>(heap);
  RawAssignment<WeakPersistent>(heap);
}

template <template <typename> class PersistentType>
void CopyAssignment(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p1 = MakeGarbageCollected<GCed>(heap);
    PersistentType<GCed> p2;
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p2 = p1;
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(p1.Get(), p2.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p1 = MakeGarbageCollected<GCed>(heap);
    PersistentType<GCed> p2 = MakeGarbageCollected<GCed>(heap);
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    p2 = p1;
    // The old node from p2 must be dropped.
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(p1.Get(), p2.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<DerivedGCed> p1 = MakeGarbageCollected<DerivedGCed>(heap);
    PersistentType<GCed> p2;
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p2 = p1;
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(p1.Get(), p2.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    static constexpr size_t kSlots = 512u;
    const PersistentType<GCed> prototype = MakeGarbageCollected<GCed>(heap);
    std::vector<PersistentType<GCed>> vector(kSlots);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    size_t i = 0;
    for (auto& p : vector) {
      p = prototype;
      EXPECT_EQ(i + 2, GetRegion<PersistentType>(heap).NodesInUse());
      ++i;
    }
    vector.clear();
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, CopyAssignment) {
  auto* heap = GetHeap();
  CopyAssignment<Persistent>(heap);
  CopyAssignment<WeakPersistent>(heap);
}

template <template <typename> class PersistentType>
void MoveAssignment(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    GCed* gced = MakeGarbageCollected<GCed>(heap);
    PersistentType<GCed> p1 = gced;
    PersistentType<GCed> p2;
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p2 = std::move(p1);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(gced, p2.Get());
    // Moved-from-object is in the valid specified (nullptr) state.
    EXPECT_EQ(nullptr, p1.Get());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p1;
    PersistentType<GCed> p2 = MakeGarbageCollected<GCed>(heap);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p2 = std::move(p1);
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
    // Moved-from-object is in the valid specified (nullptr) state.
    EXPECT_EQ(nullptr, p2.Get());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    GCed* gced = MakeGarbageCollected<GCed>(heap);
    PersistentType<GCed> p1 = gced;
    PersistentType<GCed> p2 = MakeGarbageCollected<GCed>(heap);
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    p2 = std::move(p1);
    // The old node from p2 must be dropped.
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(gced, p2.Get());
    // Moved-from-object is in the valid specified (nullptr) state.
    EXPECT_EQ(nullptr, p1.Get());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<DerivedGCed> p1 = MakeGarbageCollected<DerivedGCed>(heap);
    PersistentType<GCed> p2;
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    // Move ctor is not heterogeneous - fall back to copy assignment.
    p2 = std::move(p1);
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(p1.Get(), p2.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, MoveAssignment) {
  auto* heap = GetHeap();
  MoveAssignment<Persistent>(heap);
  MoveAssignment<WeakPersistent>(heap);
}

template <template <typename> class PersistentType,
          template <typename> class MemberType>
void MemberAssignment(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    MemberType<GCed> m = MakeGarbageCollected<GCed>(heap);
    PersistentType<GCed> p;
    p = m;
    EXPECT_EQ(m.Get(), p.Get());
    EXPECT_EQ(m, p);
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, MemberAssignment) {
  auto* heap = GetHeap();
  MemberAssignment<Persistent, Member>(heap);
  MemberAssignment<Persistent, WeakMember>(heap);
  MemberAssignment<Persistent, UntracedMember>(heap);
  MemberAssignment<WeakPersistent, Member>(heap);
  MemberAssignment<WeakPersistent, WeakMember>(heap);
  MemberAssignment<WeakPersistent, UntracedMember>(heap);
}

template <template <typename> class PersistentType>
void ClearTest(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  PersistentType<GCed> p = MakeGarbageCollected<GCed>(heap);
  EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
  EXPECT_NE(nullptr, p.Get());
  p.Clear();
  EXPECT_EQ(nullptr, p.Get());
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, Clear) {
  auto* heap = GetHeap();
  ClearTest<Persistent>(heap);
  ClearTest<WeakPersistent>(heap);
}

template <template <typename> class PersistentType>
void ReleaseTest(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  GCed* gced = MakeGarbageCollected<GCed>(heap);
  PersistentType<GCed> p = gced;
  EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
  EXPECT_NE(nullptr, p.Get());
  GCed* raw = p.Release();
  EXPECT_EQ(gced, raw);
  EXPECT_EQ(nullptr, p.Get());
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, Release) {
  auto* heap = GetHeap();
  ReleaseTest<Persistent>(heap);
  ReleaseTest<WeakPersistent>(heap);
}

template <template <typename> class PersistentType1,
          template <typename> class PersistentType2>
void HeterogeneousConversion(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType1>(heap).NodesInUse());
  EXPECT_EQ(0u, GetRegion<PersistentType2>(heap).NodesInUse());
  {
    PersistentType1<GCed> persistent1 = MakeGarbageCollected<GCed>(heap);
    PersistentType2<GCed> persistent2 = persistent1;
    EXPECT_EQ(persistent1.Get(), persistent2.Get());
    EXPECT_EQ(1u, GetRegion<PersistentType1>(heap).NodesInUse());
    EXPECT_EQ(1u, GetRegion<PersistentType2>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType1>(heap).NodesInUse());
  EXPECT_EQ(0u, GetRegion<PersistentType2>(heap).NodesInUse());
  {
    PersistentType1<DerivedGCed> persistent1 =
        MakeGarbageCollected<DerivedGCed>(heap);
    PersistentType2<GCed> persistent2 = persistent1;
    EXPECT_EQ(persistent1.Get(), persistent2.Get());
    EXPECT_EQ(1u, GetRegion<PersistentType1>(heap).NodesInUse());
    EXPECT_EQ(1u, GetRegion<PersistentType2>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType1>(heap).NodesInUse());
  EXPECT_EQ(0u, GetRegion<PersistentType2>(heap).NodesInUse());
  {
    PersistentType1<GCed> persistent1 = MakeGarbageCollected<GCed>(heap);
    PersistentType2<GCed> persistent2;
    persistent2 = persistent1;
    EXPECT_EQ(persistent1.Get(), persistent2.Get());
    EXPECT_EQ(1u, GetRegion<PersistentType1>(heap).NodesInUse());
    EXPECT_EQ(1u, GetRegion<PersistentType2>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType1>(heap).NodesInUse());
  EXPECT_EQ(0u, GetRegion<PersistentType2>(heap).NodesInUse());
  {
    PersistentType1<DerivedGCed> persistent1 =
        MakeGarbageCollected<DerivedGCed>(heap);
    PersistentType2<GCed> persistent2;
    persistent2 = persistent1;
    EXPECT_EQ(persistent1.Get(), persistent2.Get());
    EXPECT_EQ(1u, GetRegion<PersistentType1>(heap).NodesInUse());
    EXPECT_EQ(1u, GetRegion<PersistentType2>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType1>(heap).NodesInUse());
  EXPECT_EQ(0u, GetRegion<PersistentType2>(heap).NodesInUse());
}

TEST_F(PersistentTest, HeterogeneousConversion) {
  auto* heap = GetHeap();
  HeterogeneousConversion<Persistent, WeakPersistent>(heap);
  HeterogeneousConversion<WeakPersistent, Persistent>(heap);
}

TEST_F(PersistentTest, TraceStrong) {
  auto* heap = GetHeap();
  static constexpr size_t kItems = 512;
  std::vector<Persistent<GCed>> vec(kItems);
  for (auto& p : vec) {
    p = MakeGarbageCollected<GCed>(heap);
  }
  {
    GCed::trace_call_count = 0;
    RootVisitor v;
    GetRegion<Persistent>(heap).Trace(&v);
    EXPECT_EQ(kItems, GCed::trace_call_count);
    EXPECT_EQ(kItems, GetRegion<Persistent>(heap).NodesInUse());
  }
  {
    GCed::trace_call_count = 0;
    vec[0].Clear();
    vec[kItems / 2].Clear();
    vec[kItems / 4].Clear();
    vec[kItems - 1].Clear();
    RootVisitor v;
    GetRegion<Persistent>(heap).Trace(&v);
    EXPECT_EQ(kItems - 4, GCed::trace_call_count);
    EXPECT_EQ(kItems - 4, GetRegion<Persistent>(heap).NodesInUse());
  }
  {
    GCed::trace_call_count = 0;
    vec.clear();
    RootVisitor v;
    GetRegion<Persistent>(heap).Trace(&v);
    EXPECT_EQ(0u, GCed::trace_call_count);
    EXPECT_EQ(0u, GetRegion<Persistent>(heap).NodesInUse());
  }
}

TEST_F(PersistentTest, TraceWeak) {
  auto* heap = GetHeap();
  static constexpr size_t kItems = 512;
  std::vector<WeakPersistent<GCed>> vec(kItems);
  for (auto& p : vec) {
    p = MakeGarbageCollected<GCed>(heap);
  }
  GCed::trace_call_count = 0;
  RootVisitor v;
  GetRegion<WeakPersistent>(heap).Trace(&v);
  const auto& callbacks = v.WeakCallbacks();
  EXPECT_EQ(kItems, callbacks.size());
  EXPECT_EQ(kItems, GetRegion<WeakPersistent>(heap).NodesInUse());

  v.ProcessWeakCallbacks();
  for (const auto& p : vec) {
    EXPECT_EQ(nullptr, p.Get());
  }
  EXPECT_EQ(0u, GetRegion<WeakPersistent>(heap).NodesInUse());
}

#if CPPGC_SUPPORTS_SOURCE_LOCATION
TEST_F(PersistentTest, LocalizedPersistent) {
  GCed* gced = MakeGarbageCollected<GCed>(GetHeap());
  {
    const auto expected_loc = SourceLocation::Current();
    LocalizedPersistent<GCed> p = gced;
    const auto actual_loc = p.Location();
    EXPECT_STREQ(expected_loc.Function(), actual_loc.Function());
    EXPECT_STREQ(expected_loc.FileName(), actual_loc.FileName());
    EXPECT_EQ(expected_loc.Line() + 1, actual_loc.Line());
  }
  {
    // Copy ctor doesn't copy source location.
    LocalizedPersistent<GCed> p1 = gced;
    LocalizedPersistent<GCed> p2 = p1;
    EXPECT_STREQ(p1.Location().Function(), p2.Location().Function());
    EXPECT_STREQ(p1.Location().FileName(), p2.Location().FileName());
    EXPECT_EQ(p1.Location().Line() + 1, p2.Location().Line());
  }
  {
    // Copy assignment doesn't copy source location.
    LocalizedPersistent<GCed> p1 = gced;
    LocalizedPersistent<GCed> p2;
    p2 = p1;
    EXPECT_STREQ(p1.Location().Function(), p2.Location().Function());
    EXPECT_STREQ(p1.Location().FileName(), p2.Location().FileName());
    EXPECT_EQ(p1.Location().Line() + 1, p2.Location().Line());
  }
  {
    // Clearing doesn't clear source location.
    LocalizedPersistent<GCed> p1 = gced;
    LocalizedPersistent<GCed> p2 = gced;
    p2.Clear();
    EXPECT_STREQ(p1.Location().Function(), p2.Location().Function());
    EXPECT_STREQ(p1.Location().FileName(), p2.Location().FileName());
    EXPECT_EQ(p1.Location().Line() + 1, p2.Location().Line());
  }
  {
    LocalizedPersistent<GCed> p1 = gced;
    const auto expected_loc = p1.Location();
    LocalizedPersistent<GCed> p2 = std::move(p1);
    EXPECT_STREQ(expected_loc.Function(), p2.Location().Function());
    EXPECT_STREQ(expected_loc.FileName(), p2.Location().FileName());
    EXPECT_EQ(expected_loc.Line(), p2.Location().Line());
  }
  {
    LocalizedPersistent<GCed> p1 = gced;
    const auto expected_loc = p1.Location();
    LocalizedPersistent<GCed> p2;
    p2 = std::move(p1);
    EXPECT_STREQ(expected_loc.Function(), p2.Location().Function());
    EXPECT_STREQ(expected_loc.FileName(), p2.Location().FileName());
    EXPECT_EQ(expected_loc.Line(), p2.Location().Line());
  }
}
#endif

}  // namespace internal
}  // namespace cppgc
