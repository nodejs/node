// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/cppgc/allocation.h"
#include "include/cppgc/explicit-management.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/heap-consistency.h"
#include "include/cppgc/internal/api-constants.h"
#include "include/cppgc/persistent.h"
#include "include/cppgc/platform.h"
#include "include/cppgc/testing.h"
#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-cppgc.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-traced-handle.h"
#include "src/api/api-inl.h"
#include "src/base/platform/time.h"
#include "src/common/globals.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/sweeper.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/heap/cppgc-js/unified-heap-utils.h"
#include "test/unittests/heap/heap-utils.h"

namespace v8 {
namespace internal {

namespace {

class Wrappable final : public cppgc::GarbageCollected<Wrappable> {
 public:
  static size_t destructor_callcount;

  ~Wrappable() { destructor_callcount++; }

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(wrapper_); }

  void SetWrapper(v8::Isolate* isolate, v8::Local<v8::Object> wrapper) {
    wrapper_.Reset(isolate, wrapper);
  }

  TracedReference<v8::Object>& wrapper() { return wrapper_; }

 private:
  TracedReference<v8::Object> wrapper_;
};

size_t Wrappable::destructor_callcount = 0;

using UnifiedHeapDetachedTest = TestWithHeapInternals;

}  // namespace

TEST_F(UnifiedHeapTest, OnlyGC) { CollectGarbageWithEmbedderStack(); }

TEST_F(UnifiedHeapTest, FindingV8ToBlinkReference) {
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  uint16_t wrappable_type = WrapperHelper::kTracedEmbedderId;
  auto* wrappable_object =
      cppgc::MakeGarbageCollected<Wrappable>(allocation_handle());
  v8::Local<v8::Object> api_object =
      WrapperHelper::CreateWrapper(context, &wrappable_type, wrappable_object);
  Wrappable::destructor_callcount = 0;
  EXPECT_FALSE(api_object.IsEmpty());
  EXPECT_EQ(0u, Wrappable::destructor_callcount);
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(0u, Wrappable::destructor_callcount);
  WrapperHelper::ResetWrappableConnection(api_object);
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(1u, Wrappable::destructor_callcount);
}

TEST_F(UnifiedHeapTest, WriteBarrierV8ToCppReference) {
  if (!v8_flags.incremental_marking) return;
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  void* wrappable = cppgc::MakeGarbageCollected<Wrappable>(allocation_handle());
  v8::Local<v8::Object> api_object =
      WrapperHelper::CreateWrapper(context, nullptr, nullptr);
  Wrappable::destructor_callcount = 0;
  WrapperHelper::ResetWrappableConnection(api_object);
  SimulateIncrementalMarking();
  uint16_t type_info = WrapperHelper::kTracedEmbedderId;
  WrapperHelper::SetWrappableConnection(api_object, &type_info, wrappable);
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(0u, Wrappable::destructor_callcount);
}

#if DEBUG
namespace {
class Unreferenced : public cppgc::GarbageCollected<Unreferenced> {
 public:
  void Trace(cppgc::Visitor*) const {}
};
}  // namespace

TEST_F(UnifiedHeapTest, FreeUnreferencedDuringNoGcScope) {
  v8::HandleScope handle_scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  auto* unreferenced = cppgc::MakeGarbageCollected<Unreferenced>(
      allocation_handle(),
      cppgc::AdditionalBytes(cppgc::internal::api_constants::kMB));
  // Force safepoint to force flushing of cached allocated/freed sizes in cppgc.
  cpp_heap().stats_collector()->NotifySafePointForTesting();
  {
    cppgc::subtle::NoGarbageCollectionScope no_gc_scope(cpp_heap());
    cppgc::subtle::FreeUnreferencedObject(cpp_heap(), *unreferenced);
    // Force safepoint to make sure allocated size decrease due to freeing
    // unreferenced object is reported to CppHeap. Due to
    // NoGarbageCollectionScope, CppHeap will cache the reported decrease and
    // won't report it further.
    cpp_heap().stats_collector()->NotifySafePointForTesting();
  }
  // Running a GC resets the allocated size counters to the current marked bytes
  // counter.
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  // If CppHeap didn't clear it's cached values when the counters were reset,
  // the next safepoint will try to decrease the cached value from the last
  // marked bytes (which is smaller than the cached value) and crash.
  cppgc::MakeGarbageCollected<Unreferenced>(allocation_handle());
  cpp_heap().stats_collector()->NotifySafePointForTesting();
}
#endif  // DEBUG

TEST_F(UnifiedHeapTest, TracedReferenceRetainsFromStack) {
  v8::HandleScope handle_scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  TracedReference<v8::Object> holder;
  {
    v8::HandleScope inner_handle_scope(v8_isolate());
    auto local = v8::Object::New(v8_isolate());
    EXPECT_TRUE(local->IsObject());
    holder.Reset(v8_isolate(), local);
  }
  CollectGarbageWithEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  auto local = holder.Get(v8_isolate());
  EXPECT_TRUE(local->IsObject());
}

TEST_F(UnifiedHeapDetachedTest, AllocationBeforeConfigureHeap) {
  auto heap = v8::CppHeap::Create(
      V8::GetCurrentPlatform(),
      CppHeapCreateParams{{}, WrapperHelper::DefaultWrapperDescriptor()});
  auto* object =
      cppgc::MakeGarbageCollected<Wrappable>(heap->GetAllocationHandle());
  cppgc::WeakPersistent<Wrappable> weak_holder{object};

  auto& js_heap = *isolate()->heap();
  js_heap.AttachCppHeap(heap.get());
  auto& cpp_heap = *CppHeap::From(isolate()->heap()->cpp_heap());
  {
    CollectGarbage(OLD_SPACE);
    cpp_heap.AsBase().sweeper().FinishIfRunning();
    EXPECT_TRUE(weak_holder);
  }
  USE(object);
  {
    EmbedderStackStateScope stack_scope(
        &js_heap, EmbedderStackStateScope::kExplicitInvocation,
        StackState::kNoHeapPointers);
    CollectGarbage(OLD_SPACE);
    cpp_heap.AsBase().sweeper().FinishIfRunning();
    EXPECT_FALSE(weak_holder);
  }
}

TEST_F(UnifiedHeapDetachedTest, StandAloneCppGC) {
  // Test ensures that stand-alone C++ GC are possible when using CppHeap. This
  // works even in the presence of wrappables using TracedReference as long
  // as the reference is empty.
  auto heap = v8::CppHeap::Create(
      V8::GetCurrentPlatform(),
      CppHeapCreateParams{{}, WrapperHelper::DefaultWrapperDescriptor()});
  auto* object =
      cppgc::MakeGarbageCollected<Wrappable>(heap->GetAllocationHandle());
  cppgc::WeakPersistent<Wrappable> weak_holder{object};

  heap->EnableDetachedGarbageCollectionsForTesting();
  {
    heap->CollectGarbageForTesting(
        cppgc::EmbedderStackState::kMayContainHeapPointers);
    EXPECT_TRUE(weak_holder);
  }
  USE(object);
  {
    heap->CollectGarbageForTesting(cppgc::EmbedderStackState::kNoHeapPointers);
    EXPECT_FALSE(weak_holder);
  }
}

TEST_F(UnifiedHeapDetachedTest, StandaloneTestingHeap) {
  // Perform garbage collection through the StandaloneTestingHeap API.
  auto cpp_heap = v8::CppHeap::Create(
      V8::GetCurrentPlatform(),
      CppHeapCreateParams{{}, WrapperHelper::DefaultWrapperDescriptor()});
  cpp_heap->EnableDetachedGarbageCollectionsForTesting();
  cppgc::testing::StandaloneTestingHeap heap(cpp_heap->GetHeapHandle());
  heap.StartGarbageCollection();
  heap.PerformMarkingStep(cppgc::EmbedderStackState::kNoHeapPointers);
  heap.FinalizeGarbageCollection(cppgc::EmbedderStackState::kNoHeapPointers);
}

}  // namespace internal
}  // namespace v8

namespace cppgc {

class CustomSpaceForTest : public CustomSpace<CustomSpaceForTest> {
 public:
  static constexpr size_t kSpaceIndex = 0;
};

constexpr size_t CustomSpaceForTest::kSpaceIndex;

}  // namespace cppgc

namespace v8 {
namespace internal {

namespace {

class StatisticsReceiver final : public CustomSpaceStatisticsReceiver {
 public:
  static size_t num_calls_;

  StatisticsReceiver(cppgc::CustomSpaceIndex space_index, size_t bytes)
      : expected_space_index_(space_index), expected_bytes_(bytes) {}

  void AllocatedBytes(cppgc::CustomSpaceIndex space_index, size_t bytes) final {
    EXPECT_EQ(expected_space_index_.value, space_index.value);
    EXPECT_EQ(expected_bytes_, bytes);
    ++num_calls_;
  }

 private:
  const cppgc::CustomSpaceIndex expected_space_index_;
  const size_t expected_bytes_;
};

size_t StatisticsReceiver::num_calls_ = 0u;

class GCed final : public cppgc::GarbageCollected<GCed> {
 public:
  ~GCed() {
    // Force a finalizer to guarantee sweeping can't finish without the main
    // thread.
    USE(data_);
  }
  static size_t GetAllocatedSize() {
    return sizeof(GCed) + sizeof(cppgc::internal::HeapObjectHeader);
  }
  void Trace(cppgc::Visitor*) const {}

 private:
  char data_[KB];
};

}  // namespace
}  // namespace internal
}  // namespace v8

namespace cppgc {
template <>
struct SpaceTrait<v8::internal::GCed> {
  using Space = CustomSpaceForTest;
};

}  // namespace cppgc

namespace v8 {
namespace internal {

namespace {

class UnifiedHeapWithCustomSpaceTest : public UnifiedHeapTest {
 public:
  static std::vector<std::unique_ptr<cppgc::CustomSpaceBase>>
  GetCustomSpaces() {
    std::vector<std::unique_ptr<cppgc::CustomSpaceBase>> custom_spaces;
    custom_spaces.emplace_back(std::make_unique<cppgc::CustomSpaceForTest>());
    return custom_spaces;
  }
  UnifiedHeapWithCustomSpaceTest() : UnifiedHeapTest(GetCustomSpaces()) {}
};

}  // namespace

TEST_F(UnifiedHeapWithCustomSpaceTest, CollectCustomSpaceStatisticsAtLastGC) {
  // TPH does not support kIncrementalAndConcurrent yet.
  if (v8_flags.enable_third_party_heap) return;
  StatisticsReceiver::num_calls_ = 0;
  // Initial state.
  cpp_heap().CollectCustomSpaceStatisticsAtLastGC(
      {cppgc::CustomSpaceForTest::kSpaceIndex},
      std::make_unique<StatisticsReceiver>(
          cppgc::CustomSpaceForTest::kSpaceIndex, 0u));
  EXPECT_EQ(1u, StatisticsReceiver::num_calls_);
  // State unpdated only after GC.
  cppgc::Persistent<GCed> live_obj =
      cppgc::MakeGarbageCollected<GCed>(allocation_handle());
  cppgc::MakeGarbageCollected<GCed>(allocation_handle());
  cpp_heap().CollectCustomSpaceStatisticsAtLastGC(
      {cppgc::CustomSpaceForTest::kSpaceIndex},
      std::make_unique<StatisticsReceiver>(
          cppgc::CustomSpaceForTest::kSpaceIndex, 0u));
  EXPECT_EQ(2u, StatisticsReceiver::num_calls_);
  // Check state after GC.
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  cpp_heap().CollectCustomSpaceStatisticsAtLastGC(
      {cppgc::CustomSpaceForTest::kSpaceIndex},
      std::make_unique<StatisticsReceiver>(
          cppgc::CustomSpaceForTest::kSpaceIndex, GCed::GetAllocatedSize()));
  EXPECT_EQ(3u, StatisticsReceiver::num_calls_);
  // State callback delayed during sweeping.
  cppgc::Persistent<GCed> another_live_obj =
      cppgc::MakeGarbageCollected<GCed>(allocation_handle());
  while (v8::platform::PumpMessageLoop(
      V8::GetCurrentPlatform(), v8_isolate(),
      v8::platform::MessageLoopBehavior::kDoNotWait)) {
    // Empty the message loop to avoid finalizing garbage collections through
    // unrelated tasks.
  }
  CollectGarbageWithoutEmbedderStack(
      cppgc::Heap::SweepingType::kIncrementalAndConcurrent);
  DCHECK(cpp_heap().sweeper().IsSweepingInProgress());
  cpp_heap().CollectCustomSpaceStatisticsAtLastGC(
      {cppgc::CustomSpaceForTest::kSpaceIndex},
      std::make_unique<StatisticsReceiver>(
          cppgc::CustomSpaceForTest::kSpaceIndex,
          2 * GCed::GetAllocatedSize()));
  while (v8::platform::PumpMessageLoop(
      V8::GetCurrentPlatform(), v8_isolate(),
      v8::platform::MessageLoopBehavior::kWaitForWork)) {
    if (3 < StatisticsReceiver::num_calls_) {
      EXPECT_FALSE(cpp_heap().sweeper().IsSweepingInProgress());
      break;
    }
  }
  EXPECT_EQ(4u, StatisticsReceiver::num_calls_);
}

namespace {

class InConstructionObjectReferringToGlobalHandle final
    : public cppgc::GarbageCollected<
          InConstructionObjectReferringToGlobalHandle> {
 public:
  InConstructionObjectReferringToGlobalHandle(Heap* heap,
                                              v8::Local<v8::Object> wrapper)
      : wrapper_(reinterpret_cast<v8::Isolate*>(heap->isolate()), wrapper) {
    heap->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kTesting);
    heap->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kTesting);
  }

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(wrapper_); }

  TracedReference<v8::Object>& GetWrapper() { return wrapper_; }

 private:
  TracedReference<v8::Object> wrapper_;
};

}  // namespace

TEST_F(UnifiedHeapTest, InConstructionObjectReferringToGlobalHandle) {
  v8::HandleScope handle_scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope inner_handle_scope(v8_isolate());
    auto local = v8::Object::New(v8_isolate());
    auto* cpp_obj = cppgc::MakeGarbageCollected<
        InConstructionObjectReferringToGlobalHandle>(
        allocation_handle(),
        reinterpret_cast<i::Isolate*>(v8_isolate())->heap(), local);
    CHECK_NE(kGlobalHandleZapValue,
             *reinterpret_cast<Address*>(*cpp_obj->GetWrapper()));
  }
}

}  // namespace internal
}  // namespace v8
