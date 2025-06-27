// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "include/cppgc/allocation.h"
#include "include/cppgc/cross-thread-persistent.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/internal/persistent-node.h"
#include "include/cppgc/internal/pointer-policies.h"
#include "include/cppgc/member.h"
#include "include/cppgc/persistent.h"
#include "include/cppgc/source-location.h"
#include "include/cppgc/type-traits.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/liveness-broker.h"
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
struct PersistentRegionTrait;

template <>
struct PersistentRegionTrait<Persistent> {
  static PersistentRegion& Get(cppgc::Heap* heap) {
    return internal::Heap::From(heap)->GetStrongPersistentRegion();
  }
};

template <>
struct PersistentRegionTrait<WeakPersistent> {
  static PersistentRegion& Get(cppgc::Heap* heap) {
    return internal::Heap::From(heap)->GetWeakPersistentRegion();
  }
};

template <>
struct PersistentRegionTrait<subtle::CrossThreadPersistent> {
  static CrossThreadPersistentRegion& Get(cppgc::Heap* heap) {
    return internal::Heap::From(heap)->GetStrongCrossThreadPersistentRegion();
  }
};

template <>
struct PersistentRegionTrait<subtle::WeakCrossThreadPersistent> {
  static CrossThreadPersistentRegion& Get(cppgc::Heap* heap) {
    return internal::Heap::From(heap)->GetWeakCrossThreadPersistentRegion();
  }
};

template <template <typename> class PersistentType>
auto& GetRegion(cppgc::Heap* heap) {
  return PersistentRegionTrait<PersistentType>::Get(heap);
}

template <typename T>
using LocalizedPersistent =
    internal::BasicPersistent<T, internal::StrongPersistentPolicy,
                              internal::KeepLocationPolicy,
                              internal::DefaultPersistentCheckingPolicy>;

template <typename T>
using LocalizedCrossThreadPersistent = internal::BasicCrossThreadPersistent<
    T, internal::StrongCrossThreadPersistentPolicy,
    internal::KeepLocationPolicy, internal::DisabledCheckingPolicy>;

class TestRootVisitor final : public RootVisitorBase {
 public:
  TestRootVisitor() = default;

  const auto& WeakCallbacks() const { return weak_callbacks_; }

  void ProcessWeakCallbacks() {
    const auto info = LivenessBrokerFactory::Create();
    for (const auto& cb : weak_callbacks_) {
      cb.first(info, cb.second);
    }
    weak_callbacks_.clear();
  }

 protected:
  void VisitRoot(const void* t, TraceDescriptor desc,
                 const SourceLocation&) final {
    desc.callback(nullptr, desc.base_object_payload);
  }
  void VisitWeakRoot(const void*, TraceDescriptor, WeakCallback callback,
                     const void* object, const SourceLocation&) final {
    weak_callbacks_.emplace_back(callback, object);
  }

 private:
  std::vector<std::pair<WeakCallback, const void*>> weak_callbacks_;
};

class PersistentTest : public testing::TestWithHeap {};
class PersistentDeathTest : public testing::TestWithHeap {};

}  // namespace

template <template <typename> class PersistentType>
void NullStateCtor(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> empty;
    EXPECT_EQ(nullptr, empty.Get());
    EXPECT_EQ(nullptr, empty.Release());
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  {
    PersistentType<GCed> empty = nullptr;
    EXPECT_EQ(nullptr, empty.Get());
    EXPECT_EQ(nullptr, empty.Release());
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  {
    PersistentType<GCed> empty = kSentinelPointer;
    EXPECT_EQ(kSentinelPointer, empty);
    EXPECT_EQ(kSentinelPointer, empty.Release());
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  {
    // Runtime null must not allocated associated node.
    PersistentType<GCed> empty = static_cast<GCed*>(nullptr);
    EXPECT_EQ(nullptr, empty.Get());
    EXPECT_EQ(nullptr, empty.Release());
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, NullStateCtor) {
  auto* heap = GetHeap();
  NullStateCtor<Persistent>(heap);
  NullStateCtor<WeakPersistent>(heap);
  NullStateCtor<subtle::CrossThreadPersistent>(heap);
  NullStateCtor<subtle::WeakCrossThreadPersistent>(heap);
}

template <template <typename> class PersistentType>
void RawCtor(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  GCed* gced = MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
  {
    PersistentType<const GCed> p = gced;
    EXPECT_EQ(gced, p.Get());
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
}

TEST_F(PersistentTest, RawCtor) {
  auto* heap = GetHeap();
  RawCtor<Persistent>(heap);
  RawCtor<WeakPersistent>(heap);
  RawCtor<subtle::CrossThreadPersistent>(heap);
  RawCtor<subtle::WeakCrossThreadPersistent>(heap);
}

template <template <typename> class PersistentType>
void CopyCtor(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p1 =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
    PersistentType<DerivedGCed> p1 =
        MakeGarbageCollected<DerivedGCed>(heap->GetAllocationHandle());
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    PersistentType<GCed> p2 = p1;
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(p1.Get(), p2.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    static constexpr size_t kSlots = 512u;
    const PersistentType<GCed> prototype =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
  CopyCtor<subtle::CrossThreadPersistent>(heap);
  CopyCtor<subtle::WeakCrossThreadPersistent>(heap);
}

template <template <typename> class PersistentType>
void MoveCtor(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    GCed* gced = MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
    PersistentType<DerivedGCed> p1 =
        MakeGarbageCollected<DerivedGCed>(heap->GetAllocationHandle());
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
  MoveCtor<subtle::CrossThreadPersistent>(heap);
  MoveCtor<subtle::WeakCrossThreadPersistent>(heap);
}

template <template <typename> class PersistentType,
          template <typename> class MemberType>
void MemberCtor(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    MemberType<GCed> m =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
  MemberCtor<subtle::CrossThreadPersistent, Member>(heap);
  MemberCtor<subtle::CrossThreadPersistent, WeakMember>(heap);
  MemberCtor<subtle::CrossThreadPersistent, UntracedMember>(heap);
  MemberCtor<subtle::WeakCrossThreadPersistent, Member>(heap);
  MemberCtor<subtle::WeakCrossThreadPersistent, WeakMember>(heap);
  MemberCtor<subtle::WeakCrossThreadPersistent, UntracedMember>(heap);
}

template <template <typename> class PersistentType>
void NullStateAssignment(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p = nullptr;
    EXPECT_EQ(nullptr, p.Get());
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  {
    PersistentType<GCed> p =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p = kSentinelPointer;
    EXPECT_EQ(kSentinelPointer, p);
    EXPECT_EQ(kSentinelPointer, p.Get());
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  }
  {
    PersistentType<GCed> p =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p = static_cast<GCed*>(nullptr);
    EXPECT_EQ(nullptr, p.Get());
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  }
}

TEST_F(PersistentTest, NullStateAssignment) {
  auto* heap = GetHeap();
  NullStateAssignment<Persistent>(heap);
  NullStateAssignment<WeakPersistent>(heap);
  NullStateAssignment<subtle::CrossThreadPersistent>(heap);
  NullStateAssignment<subtle::WeakCrossThreadPersistent>(heap);
}

template <template <typename> class PersistentType>
void RawAssignment(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  GCed* gced = MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
  RawAssignment<subtle::CrossThreadPersistent>(heap);
  RawAssignment<subtle::WeakCrossThreadPersistent>(heap);
}

template <template <typename> class PersistentType>
void CopyAssignment(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p1 =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    PersistentType<GCed> p2;
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p2 = p1;
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(p1.Get(), p2.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<GCed> p1 =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    PersistentType<GCed> p2 =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    p2 = p1;
    // The old node from p2 must be dropped.
    EXPECT_EQ(2u, GetRegion<PersistentType>(heap).NodesInUse());
    EXPECT_EQ(p1.Get(), p2.Get());
    EXPECT_EQ(p1, p2);
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    PersistentType<DerivedGCed> p1 =
        MakeGarbageCollected<DerivedGCed>(heap->GetAllocationHandle());
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
    const PersistentType<GCed> prototype =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
  CopyAssignment<subtle::CrossThreadPersistent>(heap);
  CopyAssignment<subtle::WeakCrossThreadPersistent>(heap);
}

template <template <typename> class PersistentType>
void MoveAssignment(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    GCed* gced = MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
    PersistentType<GCed> p2 =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    EXPECT_EQ(1u, GetRegion<PersistentType>(heap).NodesInUse());
    p2 = std::move(p1);
    EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
    // Moved-from-object is in the valid specified (nullptr) state.
    EXPECT_EQ(nullptr, p2.Get());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    GCed* gced = MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    PersistentType<GCed> p1 = gced;
    PersistentType<GCed> p2 =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
    PersistentType<DerivedGCed> p1 =
        MakeGarbageCollected<DerivedGCed>(heap->GetAllocationHandle());
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
  MoveAssignment<subtle::CrossThreadPersistent>(heap);
  MoveAssignment<subtle::WeakCrossThreadPersistent>(heap);
}

template <template <typename> class PersistentType,
          template <typename> class MemberType>
void MemberAssignment(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  {
    MemberType<GCed> m =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
  MemberAssignment<subtle::CrossThreadPersistent, Member>(heap);
  MemberAssignment<subtle::CrossThreadPersistent, WeakMember>(heap);
  MemberAssignment<subtle::CrossThreadPersistent, UntracedMember>(heap);
  MemberAssignment<subtle::WeakCrossThreadPersistent, Member>(heap);
  MemberAssignment<subtle::WeakCrossThreadPersistent, WeakMember>(heap);
  MemberAssignment<subtle::WeakCrossThreadPersistent, UntracedMember>(heap);
}

template <template <typename> class PersistentType>
void ClearTest(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  PersistentType<GCed> p =
      MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
  ClearTest<subtle::CrossThreadPersistent>(heap);
  ClearTest<subtle::WeakCrossThreadPersistent>(heap);
}

template <template <typename> class PersistentType>
void ReleaseTest(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType>(heap).NodesInUse());
  GCed* gced = MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
  ReleaseTest<subtle::CrossThreadPersistent>(heap);
  ReleaseTest<subtle::WeakCrossThreadPersistent>(heap);
}

template <template <typename> class PersistentType1,
          template <typename> class PersistentType2>
void HeterogeneousConversion(cppgc::Heap* heap) {
  EXPECT_EQ(0u, GetRegion<PersistentType1>(heap).NodesInUse());
  EXPECT_EQ(0u, GetRegion<PersistentType2>(heap).NodesInUse());
  {
    PersistentType1<GCed> persistent1 =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    PersistentType2<GCed> persistent2 = persistent1;
    EXPECT_EQ(persistent1.Get(), persistent2.Get());
    EXPECT_EQ(1u, GetRegion<PersistentType1>(heap).NodesInUse());
    EXPECT_EQ(1u, GetRegion<PersistentType2>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType1>(heap).NodesInUse());
  EXPECT_EQ(0u, GetRegion<PersistentType2>(heap).NodesInUse());
  {
    PersistentType1<DerivedGCed> persistent1 =
        MakeGarbageCollected<DerivedGCed>(heap->GetAllocationHandle());
    PersistentType2<GCed> persistent2 = persistent1;
    EXPECT_EQ(persistent1.Get(), persistent2.Get());
    EXPECT_EQ(1u, GetRegion<PersistentType1>(heap).NodesInUse());
    EXPECT_EQ(1u, GetRegion<PersistentType2>(heap).NodesInUse());
  }
  EXPECT_EQ(0u, GetRegion<PersistentType1>(heap).NodesInUse());
  EXPECT_EQ(0u, GetRegion<PersistentType2>(heap).NodesInUse());
  {
    PersistentType1<GCed> persistent1 =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
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
        MakeGarbageCollected<DerivedGCed>(heap->GetAllocationHandle());
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

namespace {

class Parent : public GarbageCollected<Parent> {
 public:
  virtual void Trace(Visitor*) const {}
  void ParentFoo() { /* Dummy method to trigger vtable check on UBSan. */
  }
};
class Child : public Parent {
 public:
  void ChildFoo() { /* Dummy method to trigger vtable check on UBSan. */
  }
};

template <template <typename> class PersistentType>
void ImplicitUpcast(cppgc::Heap* heap) {
  PersistentType<Child> child;
  PersistentType<Parent> parent = child;
}

template <template <typename> class PersistentType>
void ExplicitDowncast(cppgc::Heap* heap) {
  PersistentType<Parent> parent{
      MakeGarbageCollected<Child>(heap->GetAllocationHandle())};
  PersistentType<Child> child = parent.template To<Child>();
  child->ChildFoo();
}

}  // namespace

TEST_F(PersistentTest, ImplicitUpcast) {
  auto* heap = GetHeap();
  ImplicitUpcast<Persistent>(heap);
  ImplicitUpcast<WeakPersistent>(heap);
  ImplicitUpcast<subtle::CrossThreadPersistent>(heap);
  ImplicitUpcast<subtle::WeakCrossThreadPersistent>(heap);
}

TEST_F(PersistentTest, ExplicitDowncast) {
  auto* heap = GetHeap();
  ExplicitDowncast<Persistent>(heap);
  ExplicitDowncast<WeakPersistent>(heap);
  ExplicitDowncast<subtle::CrossThreadPersistent>(heap);
  ExplicitDowncast<subtle::WeakCrossThreadPersistent>(heap);
}

namespace {
template <template <typename> class PersistentType1,
          template <typename> class PersistentType2>
void EqualityTest(cppgc::Heap* heap) {
  {
    GCed* gced = MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    PersistentType1<GCed> persistent1 = gced;
    PersistentType2<GCed> persistent2 = gced;
    EXPECT_TRUE(persistent1 == persistent2);
    EXPECT_FALSE(persistent1 != persistent2);
    persistent2 = persistent1;
    EXPECT_TRUE(persistent1 == persistent2);
    EXPECT_FALSE(persistent1 != persistent2);
  }
  {
    PersistentType1<GCed> persistent1 =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    PersistentType2<GCed> persistent2 =
        MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
    EXPECT_TRUE(persistent1 != persistent2);
    EXPECT_FALSE(persistent1 == persistent2);
  }
}
}  // namespace

TEST_F(PersistentTest, EqualityTest) {
  cppgc::Heap* heap = GetHeap();
  EqualityTest<Persistent, Persistent>(heap);
  EqualityTest<Persistent, WeakPersistent>(heap);
  EqualityTest<Persistent, subtle::CrossThreadPersistent>(heap);
  EqualityTest<Persistent, subtle::WeakCrossThreadPersistent>(heap);
  EqualityTest<WeakPersistent, Persistent>(heap);
  EqualityTest<WeakPersistent, WeakPersistent>(heap);
  EqualityTest<WeakPersistent, subtle::CrossThreadPersistent>(heap);
  EqualityTest<WeakPersistent, subtle::WeakCrossThreadPersistent>(heap);
  EqualityTest<subtle::CrossThreadPersistent, Persistent>(heap);
  EqualityTest<subtle::CrossThreadPersistent, WeakPersistent>(heap);
  EqualityTest<subtle::CrossThreadPersistent, subtle::CrossThreadPersistent>(
      heap);
  EqualityTest<subtle::CrossThreadPersistent,
               subtle::WeakCrossThreadPersistent>(heap);
  EqualityTest<subtle::WeakCrossThreadPersistent, Persistent>(heap);
  EqualityTest<subtle::WeakCrossThreadPersistent, WeakPersistent>(heap);
  EqualityTest<subtle::WeakCrossThreadPersistent,
               subtle::CrossThreadPersistent>(heap);
  EqualityTest<subtle::WeakCrossThreadPersistent,
               subtle::WeakCrossThreadPersistent>(heap);
}

TEST_F(PersistentTest, TraceStrong) {
  auto* heap = GetHeap();
  static constexpr size_t kItems = 512;
  std::vector<Persistent<GCed>> vec(kItems);
  for (auto& p : vec) {
    p = MakeGarbageCollected<GCed>(GetAllocationHandle());
  }
  {
    GCed::trace_call_count = 0;
    TestRootVisitor v;
    GetRegion<Persistent>(heap).Iterate(v);
    EXPECT_EQ(kItems, GCed::trace_call_count);
    EXPECT_EQ(kItems, GetRegion<Persistent>(heap).NodesInUse());
  }
  {
    GCed::trace_call_count = 0;
    vec[0].Clear();
    vec[kItems / 2].Clear();
    vec[kItems / 4].Clear();
    vec[kItems - 1].Clear();
    TestRootVisitor v;
    GetRegion<Persistent>(heap).Iterate(v);
    EXPECT_EQ(kItems - 4, GCed::trace_call_count);
    EXPECT_EQ(kItems - 4, GetRegion<Persistent>(heap).NodesInUse());
  }
  {
    GCed::trace_call_count = 0;
    vec.clear();
    TestRootVisitor v;
    GetRegion<Persistent>(heap).Iterate(v);
    EXPECT_EQ(0u, GCed::trace_call_count);
    EXPECT_EQ(0u, GetRegion<Persistent>(heap).NodesInUse());
  }
}

TEST_F(PersistentTest, TraceWeak) {
  auto* heap = GetHeap();
  static constexpr size_t kItems = 512;
  std::vector<WeakPersistent<GCed>> vec(kItems);
  for (auto& p : vec) {
    p = MakeGarbageCollected<GCed>(GetAllocationHandle());
  }
  GCed::trace_call_count = 0;
  TestRootVisitor v;
  GetRegion<WeakPersistent>(heap).Iterate(v);
  const auto& callbacks = v.WeakCallbacks();
  EXPECT_EQ(kItems, callbacks.size());
  EXPECT_EQ(kItems, GetRegion<WeakPersistent>(heap).NodesInUse());

  v.ProcessWeakCallbacks();
  for (const auto& p : vec) {
    EXPECT_EQ(nullptr, p.Get());
  }
  EXPECT_EQ(0u, GetRegion<WeakPersistent>(heap).NodesInUse());
}

TEST_F(PersistentTest, ClearOnHeapDestruction) {
  Persistent<GCed> persistent;
  WeakPersistent<GCed> weak_persistent;

  // Create another heap that can be destroyed during the test.
  auto heap = Heap::Create(GetPlatformHandle());
  persistent = MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
  weak_persistent = MakeGarbageCollected<GCed>(heap->GetAllocationHandle());
  const Persistent<GCed> persistent_sentinel(kSentinelPointer);
  const WeakPersistent<GCed> weak_persistent_sentinel(kSentinelPointer);
  const subtle::CrossThreadPersistent<GCed> cross_thread_persistent_sentinel(
      kSentinelPointer);
  const subtle::WeakCrossThreadPersistent<GCed>
      cross_thread_weak_persistent_sentinel(kSentinelPointer);
  heap.reset();

  EXPECT_EQ(nullptr, persistent);
  EXPECT_EQ(nullptr, weak_persistent);
  // Sentinel values survive as they do not represent actual heap objects.
  EXPECT_EQ(kSentinelPointer, persistent_sentinel);
  EXPECT_EQ(kSentinelPointer, weak_persistent_sentinel);
}

#if V8_SUPPORTS_SOURCE_LOCATION
TEST_F(PersistentTest, LocalizedPersistent) {
  GCed* gced = MakeGarbageCollected<GCed>(GetAllocationHandle());
  {
    const auto expected_loc = SourceLocation::Current();
    LocalizedPersistent<GCed> p = gced;
    const auto actual_loc = p.Location();
    EXPECT_STREQ(expected_loc.Function(), actual_loc.Function());
    EXPECT_STREQ(expected_loc.FileName(), actual_loc.FileName());
    EXPECT_EQ(expected_loc.Line() + 1, actual_loc.Line());
  }
  {
    const auto expected_loc = SourceLocation::Current();
    LocalizedCrossThreadPersistent<GCed> p = gced;
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
    // Copy ctor doesn't copy source location.
    LocalizedCrossThreadPersistent<GCed> p1 = gced;
    LocalizedCrossThreadPersistent<GCed> p2 = p1;
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
    // Copy assignment doesn't copy source location.
    LocalizedCrossThreadPersistent<GCed> p1 = gced;
    LocalizedCrossThreadPersistent<GCed> p2;
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
    // Clearing doesn't clear source location.
    LocalizedCrossThreadPersistent<GCed> p1 = gced;
    LocalizedCrossThreadPersistent<GCed> p2 = gced;
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
    LocalizedCrossThreadPersistent<GCed> p1 = gced;
    const auto expected_loc = p1.Location();
    LocalizedCrossThreadPersistent<GCed> p2 = std::move(p1);
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
  {
    LocalizedCrossThreadPersistent<GCed> p1 = gced;
    const auto expected_loc = p1.Location();
    LocalizedCrossThreadPersistent<GCed> p2;
    p2 = std::move(p1);
    EXPECT_STREQ(expected_loc.Function(), p2.Location().Function());
    EXPECT_STREQ(expected_loc.FileName(), p2.Location().FileName());
    EXPECT_EQ(expected_loc.Line(), p2.Location().Line());
  }
}

#endif

namespace {

class ExpectingLocationVisitor final : public RootVisitorBase {
 public:
  explicit ExpectingLocationVisitor(const SourceLocation& expected_location)
      : expected_loc_(expected_location) {}

 protected:
  void VisitRoot(const void* t, TraceDescriptor desc,
                 const SourceLocation& loc) final {
    EXPECT_STREQ(expected_loc_.Function(), loc.Function());
    EXPECT_STREQ(expected_loc_.FileName(), loc.FileName());
    EXPECT_EQ(expected_loc_.Line(), loc.Line());
  }

 private:
  const SourceLocation& expected_loc_;
};

}  // namespace

TEST_F(PersistentTest, PersistentTraceLocation) {
  GCed* gced = MakeGarbageCollected<GCed>(GetAllocationHandle());
  {
#if V8_SUPPORTS_SOURCE_LOCATION
    // Baseline for creating expected location which has a different line
    // number.
    const auto loc = SourceLocation::Current();
    const auto expected_loc =
        SourceLocation::Current(loc.Function(), loc.FileName(), loc.Line() + 6);
#else   // !V8_SUPPORTS_SOURCE_LOCATION
    const SourceLocation expected_loc;
#endif  // !V8_SUPPORTS_SOURCE_LOCATION
    LocalizedPersistent<GCed> p = gced;
    ExpectingLocationVisitor visitor(expected_loc);
    visitor.Trace(p);
  }
}

namespace {
class IncompleteType;
}  // namespace

TEST_F(PersistentTest, EmptyPersistentConstructDestructWithoutCompleteType) {
  // Test ensures that empty constructor and destructor compile without having
  // a complete type available.
  Persistent<IncompleteType> p1;
  WeakPersistent<IncompleteType> p2;
  subtle::CrossThreadPersistent<IncompleteType> p3;
  subtle::WeakCrossThreadPersistent<IncompleteType> p4;
}

TEST_F(PersistentTest, Lock) {
  subtle::WeakCrossThreadPersistent<GCed> weak;
  auto strong = weak.Lock();
}

namespace {

class TraceCounter final : public GarbageCollected<TraceCounter> {
 public:
  void Trace(cppgc::Visitor* visitor) const {
    trace_calls_++;
  }

  size_t trace_calls() const { return trace_calls_; }

 private:
  mutable size_t trace_calls_ = 0;
};

class DestructionCounter final : public GarbageCollected<DestructionCounter> {
 public:
  static size_t destructor_calls_;

  ~DestructionCounter() { destructor_calls_++; }

  void Trace(cppgc::Visitor*) const {}
};
size_t DestructionCounter::destructor_calls_;

}  // namespace

TEST_F(PersistentTest, PersistentRetainsObject) {
  Persistent<TraceCounter> trace_counter =
      MakeGarbageCollected<TraceCounter>(GetAllocationHandle());
  WeakPersistent<TraceCounter> weak_trace_counter(trace_counter.Get());
  EXPECT_EQ(0u, trace_counter->trace_calls());
  PreciseGC();
  size_t saved_trace_count = trace_counter->trace_calls();
  EXPECT_LT(0u, saved_trace_count);
  PreciseGC();
  EXPECT_LT(saved_trace_count, trace_counter->trace_calls());
  EXPECT_TRUE(weak_trace_counter);
}

TEST_F(PersistentTest, WeakPersistentDoesNotRetainObject) {
  WeakPersistent<TraceCounter> weak_trace_counter =
      MakeGarbageCollected<TraceCounter>(GetAllocationHandle());
  PreciseGC();
  EXPECT_FALSE(weak_trace_counter);
}

TEST_F(PersistentTest, ObjectReclaimedAfterClearedPersistent) {
  WeakPersistent<DestructionCounter> weak_finalized;
  {
    DestructionCounter::destructor_calls_ = 0;
    Persistent<DestructionCounter> finalized =
        MakeGarbageCollected<DestructionCounter>(GetAllocationHandle());
    weak_finalized = finalized.Get();
    EXPECT_EQ(0u, DestructionCounter::destructor_calls_);
    PreciseGC();
    EXPECT_EQ(0u, DestructionCounter::destructor_calls_);
    USE(finalized);
    EXPECT_TRUE(weak_finalized);
  }
  PreciseGC();
  EXPECT_EQ(1u, DestructionCounter::destructor_calls_);
  EXPECT_FALSE(weak_finalized);
}

namespace {

class PersistentAccessOnBackgroundThread : public v8::base::Thread {
 public:
  explicit PersistentAccessOnBackgroundThread(GCed* raw_gced)
      : v8::base::Thread(v8::base::Thread::Options(
            "PersistentAccessOnBackgroundThread", 2 * kMB)),
        raw_gced_(raw_gced) {}

  void Run() override {
    EXPECT_DEATH_IF_SUPPORTED(
        Persistent<GCed> gced(static_cast<GCed*>(raw_gced_)), "");
  }

 private:
  void* raw_gced_;
};

}  // namespace

TEST_F(PersistentDeathTest, CheckCreationThread) {
#ifdef DEBUG
  // In DEBUG mode, every Persistent creation should check whether the handle
  // is created on the right thread. In release mode, this check is only
  // performed on slow path allocations.
  Persistent<GCed> first_persistent_triggers_slow_path(
      MakeGarbageCollected<GCed>(GetAllocationHandle()));
#endif  // DEBUG
  PersistentAccessOnBackgroundThread thread(
      MakeGarbageCollected<GCed>(GetAllocationHandle()));
  CHECK(thread.StartSynchronously());
  thread.Join();
}

}  // namespace internal
}  // namespace cppgc
