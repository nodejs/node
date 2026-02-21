// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/visitor.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/member.h"
#include "include/cppgc/trace-trait.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/liveness-broker.h"
#include "src/heap/cppgc/object-allocator.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class TraceTraitTest : public testing::TestSupportingAllocationOnly {};
class VisitorTest : public testing::TestSupportingAllocationOnly {};

class GCed : public GarbageCollected<GCed> {
 public:
  static size_t trace_callcount;

  GCed() { trace_callcount = 0; }

  virtual void Trace(cppgc::Visitor* visitor) const { trace_callcount++; }
};
size_t GCed::trace_callcount;

class GCedMixin : public GarbageCollectedMixin {
 public:
  static size_t trace_callcount;

  GCedMixin() { trace_callcount = 0; }

  virtual void Trace(cppgc::Visitor* visitor) const { trace_callcount++; }
};
size_t GCedMixin::trace_callcount;

class OtherPayload {
 public:
  virtual void* GetDummy() const { return nullptr; }
};

class GCedMixinApplication : public GCed,
                             public OtherPayload,
                             public GCedMixin {
 public:
  void Trace(cppgc::Visitor* visitor) const override {
    GCed::Trace(visitor);
    GCedMixin::Trace(visitor);
  }
};

}  // namespace

TEST_F(TraceTraitTest, GetObjectStartGCed) {
  auto* gced = MakeGarbageCollected<GCed>(GetAllocationHandle());
  EXPECT_EQ(gced,
            TraceTrait<GCed>::GetTraceDescriptor(gced).base_object_payload);
}

TEST_F(TraceTraitTest, GetObjectStartGCedMixin) {
  auto* gced_mixin_app =
      MakeGarbageCollected<GCedMixinApplication>(GetAllocationHandle());
  auto* gced_mixin = static_cast<GCedMixin*>(gced_mixin_app);
  EXPECT_EQ(gced_mixin_app,
            TraceTrait<GCedMixin>::GetTraceDescriptor(gced_mixin)
                .base_object_payload);
}

TEST_F(TraceTraitTest, TraceGCed) {
  auto* gced = MakeGarbageCollected<GCed>(GetAllocationHandle());
  EXPECT_EQ(0u, GCed::trace_callcount);
  TraceTrait<GCed>::Trace(nullptr, gced);
  EXPECT_EQ(1u, GCed::trace_callcount);
}

TEST_F(TraceTraitTest, TraceGCedMixin) {
  auto* gced_mixin_app =
      MakeGarbageCollected<GCedMixinApplication>(GetAllocationHandle());
  auto* gced_mixin = static_cast<GCedMixin*>(gced_mixin_app);
  EXPECT_EQ(0u, GCed::trace_callcount);
  TraceTrait<GCedMixin>::Trace(nullptr, gced_mixin);
  EXPECT_EQ(1u, GCed::trace_callcount);
}

TEST_F(TraceTraitTest, TraceGCedThroughTraceDescriptor) {
  auto* gced = MakeGarbageCollected<GCed>(GetAllocationHandle());
  EXPECT_EQ(0u, GCed::trace_callcount);
  TraceDescriptor desc = TraceTrait<GCed>::GetTraceDescriptor(gced);
  desc.callback(nullptr, desc.base_object_payload);
  EXPECT_EQ(1u, GCed::trace_callcount);
}

TEST_F(TraceTraitTest, TraceGCedMixinThroughTraceDescriptor) {
  auto* gced_mixin_app =
      MakeGarbageCollected<GCedMixinApplication>(GetAllocationHandle());
  auto* gced_mixin = static_cast<GCedMixin*>(gced_mixin_app);
  EXPECT_EQ(0u, GCed::trace_callcount);
  TraceDescriptor desc = TraceTrait<GCedMixin>::GetTraceDescriptor(gced_mixin);
  desc.callback(nullptr, desc.base_object_payload);
  EXPECT_EQ(1u, GCed::trace_callcount);
}

namespace {
class MixinInstanceWithoutTrace
    : public GarbageCollected<MixinInstanceWithoutTrace>,
      public GCedMixin {};
}  // namespace

TEST_F(TraceTraitTest, MixinInstanceWithoutTrace) {
  // Verify that a mixin instance without any traceable
  // references inherits the mixin's trace implementation.
  auto* mixin_without_trace =
      MakeGarbageCollected<MixinInstanceWithoutTrace>(GetAllocationHandle());
  auto* mixin = static_cast<GCedMixin*>(mixin_without_trace);
  EXPECT_EQ(0u, GCedMixin::trace_callcount);
  TraceDescriptor mixin_without_trace_desc =
      TraceTrait<MixinInstanceWithoutTrace>::GetTraceDescriptor(
          mixin_without_trace);
  TraceDescriptor mixin_desc = TraceTrait<GCedMixin>::GetTraceDescriptor(mixin);
  EXPECT_EQ(mixin_without_trace_desc.callback, mixin_desc.callback);
  EXPECT_EQ(mixin_without_trace_desc.base_object_payload,
            mixin_desc.base_object_payload);
  TraceDescriptor desc =
      TraceTrait<MixinInstanceWithoutTrace>::GetTraceDescriptor(
          mixin_without_trace);
  desc.callback(nullptr, desc.base_object_payload);
  EXPECT_EQ(1u, GCedMixin::trace_callcount);
}

namespace {

class DispatchingVisitor : public VisitorBase {
 public:
  ~DispatchingVisitor() override = default;

  template <typename T>
  void TraceForTesting(T* t) {
    TraceRawForTesting(this, t);
  }

 protected:
  void Visit(const void* t, TraceDescriptor desc) override {
    desc.callback(this, desc.base_object_payload);
  }
};

class CheckingVisitor final : public DispatchingVisitor {
 public:
  explicit CheckingVisitor(const void* object)
      : object_(object), payload_(object) {}
  CheckingVisitor(const void* object, const void* payload)
      : object_(object), payload_(payload) {}

 protected:
  void Visit(const void* t, TraceDescriptor desc) final {
    EXPECT_EQ(object_, t);
    EXPECT_EQ(payload_, desc.base_object_payload);
    desc.callback(this, desc.base_object_payload);
  }

  void VisitWeak(const void* t, TraceDescriptor desc, WeakCallback callback,
                 const void* weak_member) final {
    EXPECT_EQ(object_, t);
    EXPECT_EQ(payload_, desc.base_object_payload);
    LivenessBroker broker = LivenessBrokerFactory::Create();
    callback(broker, weak_member);
  }

 private:
  const void* object_;
  const void* payload_;
};

}  // namespace

TEST_F(VisitorTest, DispatchTraceGCed) {
  auto* gced = MakeGarbageCollected<GCed>(GetAllocationHandle());
  CheckingVisitor visitor(gced);
  EXPECT_EQ(0u, GCed::trace_callcount);
  visitor.TraceForTesting(gced);
  EXPECT_EQ(1u, GCed::trace_callcount);
}

TEST_F(VisitorTest, DispatchTraceGCedMixin) {
  auto* gced_mixin_app =
      MakeGarbageCollected<GCedMixinApplication>(GetAllocationHandle());
  auto* gced_mixin = static_cast<GCedMixin*>(gced_mixin_app);
  // Ensure that we indeed test dispatching an inner object.
  EXPECT_NE(static_cast<void*>(gced_mixin_app), static_cast<void*>(gced_mixin));
  CheckingVisitor visitor(gced_mixin, gced_mixin_app);
  EXPECT_EQ(0u, GCed::trace_callcount);
  visitor.TraceForTesting(gced_mixin);
  EXPECT_EQ(1u, GCed::trace_callcount);
}

TEST_F(VisitorTest, DispatchTraceWeakGCed) {
  WeakMember<GCed> ref = MakeGarbageCollected<GCed>(GetAllocationHandle());
  CheckingVisitor visitor(ref, ref);
  visitor.Trace(ref);
  // No marking, so reference should be cleared.
  EXPECT_EQ(nullptr, ref.Get());
}

TEST_F(VisitorTest, DispatchTraceWeakGCedMixin) {
  auto* gced_mixin_app =
      MakeGarbageCollected<GCedMixinApplication>(GetAllocationHandle());
  auto* gced_mixin = static_cast<GCedMixin*>(gced_mixin_app);
  // Ensure that we indeed test dispatching an inner object.
  EXPECT_NE(static_cast<void*>(gced_mixin_app), static_cast<void*>(gced_mixin));
  WeakMember<GCedMixin> ref = gced_mixin;
  CheckingVisitor visitor(gced_mixin, gced_mixin_app);
  visitor.Trace(ref);
  // No marking, so reference should be cleared.
  EXPECT_EQ(nullptr, ref.Get());
}

namespace {

class WeakCallbackVisitor final : public VisitorBase {
 public:
  void RegisterWeakCallback(WeakCallback callback, const void* param) final {
    LivenessBroker broker = LivenessBrokerFactory::Create();
    callback(broker, param);
  }
};

struct WeakCallbackDispatcher {
  static size_t callback_callcount;
  static const void* callback_param;

  static void Setup(const void* expected_param) {
    callback_callcount = 0;
    callback_param = expected_param;
  }

  static void Call(const LivenessBroker& broker, const void* param) {
    EXPECT_EQ(callback_param, param);
    callback_callcount++;
  }
};

size_t WeakCallbackDispatcher::callback_callcount;
const void* WeakCallbackDispatcher::callback_param;

class GCedWithCustomWeakCallback final
    : public GarbageCollected<GCedWithCustomWeakCallback> {
 public:
  void CustomWeakCallbackMethod(const LivenessBroker& broker) {
    WeakCallbackDispatcher::Call(broker, this);
  }

  void Trace(cppgc::Visitor* visitor) const {
    visitor->RegisterWeakCallbackMethod<
        GCedWithCustomWeakCallback,
        &GCedWithCustomWeakCallback::CustomWeakCallbackMethod>(this);
  }
};

}  // namespace

TEST_F(VisitorTest, DispatchRegisterWeakCallback) {
  WeakCallbackVisitor visitor;
  WeakCallbackDispatcher::Setup(&visitor);
  EXPECT_EQ(0u, WeakCallbackDispatcher::callback_callcount);
  visitor.RegisterWeakCallback(WeakCallbackDispatcher::Call, &visitor);
  EXPECT_EQ(1u, WeakCallbackDispatcher::callback_callcount);
}

TEST_F(VisitorTest, DispatchRegisterWeakCallbackMethod) {
  WeakCallbackVisitor visitor;
  auto* gced =
      MakeGarbageCollected<GCedWithCustomWeakCallback>(GetAllocationHandle());
  WeakCallbackDispatcher::Setup(gced);
  EXPECT_EQ(0u, WeakCallbackDispatcher::callback_callcount);
  gced->Trace(&visitor);
  EXPECT_EQ(1u, WeakCallbackDispatcher::callback_callcount);
}

namespace {

class Composite final {
 public:
  static size_t callback_callcount;
  static constexpr size_t kExpectedTraceCount = 1;
  static size_t TraceCount() { return callback_callcount; }

  Composite() { callback_callcount = 0; }
  void Trace(Visitor* visitor) const { callback_callcount++; }
};

size_t Composite::callback_callcount;

class GCedWithComposite final : public GarbageCollected<GCedWithComposite> {
 public:
  static constexpr size_t kExpectedTraceCount = Composite::kExpectedTraceCount;
  static size_t TraceCount() { return Composite::TraceCount(); }

  void Trace(Visitor* visitor) const { visitor->Trace(composite); }

  Composite composite;
};

class VirtualBase {
 public:
  virtual ~VirtualBase() = default;
  virtual size_t GetCallbackCount() const = 0;
};

class CompositeWithVtable : public VirtualBase {
 public:
  static size_t callback_callcount;
  static constexpr size_t kExpectedTraceCount = 1;
  static size_t TraceCount() { return callback_callcount; }

  CompositeWithVtable() { callback_callcount = 0; }
  ~CompositeWithVtable() override = default;
  void Trace(Visitor* visitor) const { callback_callcount++; }
  size_t GetCallbackCount() const override { return callback_callcount; }
};

size_t CompositeWithVtable::callback_callcount;

class GCedWithCompositeWithVtable final
    : public GarbageCollected<GCedWithCompositeWithVtable> {
 public:
  static constexpr size_t kExpectedTraceCount = 1;
  static size_t TraceCount() { return CompositeWithVtable::callback_callcount; }

  void Trace(Visitor* visitor) const { visitor->Trace(composite); }

  CompositeWithVtable composite;
};

}  // namespace

TEST_F(VisitorTest, DispatchToCompositeObject) {
  auto* gced = MakeGarbageCollected<GCedWithComposite>(GetAllocationHandle());
  CheckingVisitor visitor(gced);
  EXPECT_EQ(0u, GCedWithComposite::TraceCount());
  visitor.TraceForTesting(gced);
  EXPECT_EQ(GCedWithComposite::kExpectedTraceCount,
            GCedWithComposite::TraceCount());
}

TEST_F(VisitorTest, DispatchToCompositeObjectWithVtable) {
  auto* gced =
      MakeGarbageCollected<GCedWithCompositeWithVtable>(GetAllocationHandle());
  CheckingVisitor visitor(gced);
  EXPECT_EQ(0u, GCedWithCompositeWithVtable::TraceCount());
  visitor.TraceForTesting(gced);
  EXPECT_EQ(GCedWithCompositeWithVtable::kExpectedTraceCount,
            GCedWithCompositeWithVtable::TraceCount());
}

namespace {

// Fibonacci hashing. See boost::hash_combine.
inline void hash_combine(std::size_t& seed) {}

template <typename T, typename... Rest>
void hash_combine(size_t& seed, const T& v, Rest... rest) {
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  hash_combine(seed, rest...);
}

class HashingVisitor final : public DispatchingVisitor {
 public:
  size_t hash() const { return hash_; }

 protected:
  void Visit(const void* t, TraceDescriptor desc) final {
    hash_combine(hash_, desc.base_object_payload);
    desc.callback(this, desc.base_object_payload);
  }

 private:
  size_t hash_ = 0;
};

template <template <class> class MemberType, typename GCType>
class GCedWithMultipleMember final
    : public GarbageCollected<GCedWithMultipleMember<MemberType, GCType>> {
 public:
  static constexpr size_t kNumElements = 17;
  static constexpr size_t kExpectedTraceCount = kNumElements;
  static size_t TraceCount() { return GCType::TraceCount(); }

  void Trace(Visitor* visitor) const {
    visitor->TraceMultiple(fields, kNumElements);
  }

  MemberType<GCType> fields[kNumElements];
};

template <class GCType>
void DispatchMultipleMemberTest(AllocationHandle& handle) {
  size_t hash = 0;
  auto* holder = MakeGarbageCollected<GCType>(handle);
  hash_combine(hash, holder);
  for (auto i = 0u; i < GCType::kNumElements; ++i) {
    holder->fields[i] = MakeGarbageCollected<GCedWithComposite>(handle);
    hash_combine(hash, holder->fields[i].Get());
  }
  HashingVisitor visitor;
  EXPECT_EQ(0u, GCType::TraceCount());
  visitor.TraceForTesting(holder);
  EXPECT_EQ(GCType::kExpectedTraceCount, GCType::TraceCount());
  EXPECT_NE(0u, hash);
  EXPECT_EQ(hash, visitor.hash());
}

}  // namespace

TEST_F(VisitorTest, DispatchToMultipleMember) {
  using GCType = GCedWithMultipleMember<Member, GCedWithComposite>;
  DispatchMultipleMemberTest<GCType>(GetAllocationHandle());
}

TEST_F(VisitorTest, DispatchToMultipleUncompressedMember) {
  using GCType =
      GCedWithMultipleMember<subtle::UncompressedMember, GCedWithComposite>;
  DispatchMultipleMemberTest<GCType>(GetAllocationHandle());
}

namespace {

class GCedWithMultipleComposite final
    : public GarbageCollected<GCedWithMultipleComposite> {
 public:
  static constexpr size_t kNumElements = 17;
  static constexpr size_t kExpectedTraceCount =
      kNumElements * Composite::kExpectedTraceCount;
  static size_t TraceCount() { return Composite::TraceCount(); }

  void Trace(Visitor* visitor) const {
    visitor->TraceMultiple(fields, kNumElements);
  }

  Composite fields[kNumElements];
};

class GCedWithMultipleCompositeUninitializedVtable final
    : public GarbageCollected<GCedWithMultipleCompositeUninitializedVtable> {
 public:
  static constexpr size_t kNumElements = 17;
  static constexpr size_t kExpectedTraceCount =
      kNumElements * CompositeWithVtable::kExpectedTraceCount;
  static size_t TraceCount() { return CompositeWithVtable::TraceCount(); }

  explicit GCedWithMultipleCompositeUninitializedVtable(
      size_t initialized_fields) {
    // Clear some vtable pointers. Such objects should not be traced.
    memset(static_cast<void*>(&fields[initialized_fields]), 0,
           sizeof(CompositeWithVtable) * (kNumElements - initialized_fields));
  }

  void Trace(Visitor* visitor) const {
    visitor->TraceMultiple(fields, kNumElements);
  }

  CompositeWithVtable fields[kNumElements];
};

}  // namespace

TEST_F(VisitorTest, DispatchToMultipleCompositeObjects) {
  auto* holder =
      MakeGarbageCollected<GCedWithMultipleComposite>(GetAllocationHandle());
  DispatchingVisitor visitor;
  EXPECT_EQ(0u, GCedWithMultipleComposite::TraceCount());
  visitor.TraceForTesting(holder);
  EXPECT_EQ(GCedWithMultipleComposite::kExpectedTraceCount,
            GCedWithMultipleComposite::TraceCount());
}

TEST_F(VisitorTest, DispatchMultipleInlinedObjectsWithClearedVtable) {
  auto* holder =
      MakeGarbageCollected<GCedWithMultipleCompositeUninitializedVtable>(
          GetAllocationHandle(), GCedWithMultipleComposite::kNumElements / 2);
  DispatchingVisitor visitor;
  EXPECT_EQ(0u, GCedWithMultipleCompositeUninitializedVtable::TraceCount());
  visitor.TraceForTesting(holder);
  EXPECT_EQ(
      GCedWithMultipleCompositeUninitializedVtable::kExpectedTraceCount / 2,
      GCedWithMultipleCompositeUninitializedVtable::TraceCount());
}

}  // namespace internal
}  // namespace cppgc
