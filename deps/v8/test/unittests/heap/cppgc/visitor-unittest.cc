// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/visitor.h"

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/trace-trait.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/liveness-broker.h"
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

class DispatchingVisitor final : public VisitorBase {
 public:
  DispatchingVisitor(const void* object, const void* payload)
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
  Member<GCed> ref = MakeGarbageCollected<GCed>(GetAllocationHandle());
  DispatchingVisitor visitor(ref, ref);
  EXPECT_EQ(0u, GCed::trace_callcount);
  visitor.Trace(ref);
  EXPECT_EQ(1u, GCed::trace_callcount);
}

TEST_F(VisitorTest, DispatchTraceGCedMixin) {
  auto* gced_mixin_app =
      MakeGarbageCollected<GCedMixinApplication>(GetAllocationHandle());
  auto* gced_mixin = static_cast<GCedMixin*>(gced_mixin_app);
  // Ensure that we indeed test dispatching an inner object.
  EXPECT_NE(static_cast<void*>(gced_mixin_app), static_cast<void*>(gced_mixin));
  Member<GCedMixin> ref = gced_mixin;
  DispatchingVisitor visitor(gced_mixin, gced_mixin_app);
  EXPECT_EQ(0u, GCed::trace_callcount);
  visitor.Trace(ref);
  EXPECT_EQ(1u, GCed::trace_callcount);
}

TEST_F(VisitorTest, DispatchTraceWeakGCed) {
  WeakMember<GCed> ref = MakeGarbageCollected<GCed>(GetAllocationHandle());
  DispatchingVisitor visitor(ref, ref);
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
  DispatchingVisitor visitor(gced_mixin, gced_mixin_app);
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
  Composite() { callback_callcount = 0; }
  void Trace(Visitor* visitor) const { callback_callcount++; }
};

size_t Composite::callback_callcount;

class GCedWithComposite final : public GarbageCollected<GCedWithComposite> {
 public:
  void Trace(Visitor* visitor) const { visitor->Trace(composite); }

  Composite composite;
};

}  // namespace

TEST_F(VisitorTest, DispatchToCompositeObject) {
  Member<GCedWithComposite> ref =
      MakeGarbageCollected<GCedWithComposite>(GetAllocationHandle());
  DispatchingVisitor visitor(ref, ref);
  EXPECT_EQ(0u, Composite::callback_callcount);
  visitor.Trace(ref);
  EXPECT_EQ(1u, Composite::callback_callcount);
}

}  // namespace internal
}  // namespace cppgc
