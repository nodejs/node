// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/persistent.h"
#include "include/cppgc/platform.h"
#include "include/cppgc/testing.h"
#include "include/libplatform/libplatform.h"
#include "include/v8-cppgc.h"
#include "include/v8.h"
#include "src/api/api-inl.h"
#include "src/base/platform/time.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/sweeper.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/heap/unified-heap-utils.h"

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
  v8::Local<v8::Object> api_object = WrapperHelper::CreateWrapper(
      context, &wrappable_type,
      cppgc::MakeGarbageCollected<Wrappable>(allocation_handle()));
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
  if (!FLAG_incremental_marking) return;
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  void* wrappable = cppgc::MakeGarbageCollected<Wrappable>(allocation_handle());
  v8::Local<v8::Object> api_object =
      WrapperHelper::CreateWrapper(context, nullptr, nullptr);
  Wrappable::destructor_callcount = 0;
  WrapperHelper::ResetWrappableConnection(api_object);
  SimulateIncrementalMarking();
  {
    // The following snippet shows the embedder code for implementing a GC-safe
    // setter for JS to C++ references.
    WrapperHelper::SetWrappableConnection(api_object, wrappable, wrappable);
    JSHeapConsistency::WriteBarrierParams params;
    auto barrier_type = JSHeapConsistency::GetWriteBarrierType(
        api_object, 1, wrappable, params,
        [this]() -> cppgc::HeapHandle& { return cpp_heap().GetHeapHandle(); });
    EXPECT_EQ(JSHeapConsistency::WriteBarrierType::kMarking, barrier_type);
    JSHeapConsistency::DijkstraMarkingBarrier(
        params, cpp_heap().GetHeapHandle(), wrappable);
  }
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(0u, Wrappable::destructor_callcount);
}

TEST_F(UnifiedHeapTest, WriteBarrierCppToV8Reference) {
  if (!FLAG_incremental_marking) return;
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  cppgc::Persistent<Wrappable> wrappable =
      cppgc::MakeGarbageCollected<Wrappable>(allocation_handle());
  Wrappable::destructor_callcount = 0;
  SimulateIncrementalMarking();
  // Pick a sentinel to compare against.
  void* kMagicAddress = &Wrappable::destructor_callcount;
  {
    // The following snippet shows the embedder code for implementing a GC-safe
    // setter for C++ to JS references.
    v8::HandleScope nested_scope(v8_isolate());
    v8::Local<v8::Object> api_object =
        WrapperHelper::CreateWrapper(context, nullptr, nullptr);
    // Setting only one field to avoid treating this as wrappable backref, see
    // `LocalEmbedderHeapTracer::ExtractWrapperInfo`.
    api_object->SetAlignedPointerInInternalField(1, kMagicAddress);
    wrappable->SetWrapper(v8_isolate(), api_object);
    JSHeapConsistency::WriteBarrierParams params;
    auto barrier_type = JSHeapConsistency::GetWriteBarrierType(
        wrappable->wrapper(), params,
        [this]() -> cppgc::HeapHandle& { return cpp_heap().GetHeapHandle(); });
    EXPECT_EQ(JSHeapConsistency::WriteBarrierType::kMarking, barrier_type);
    JSHeapConsistency::DijkstraMarkingBarrier(
        params, cpp_heap().GetHeapHandle(), wrappable->wrapper());
  }
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(0u, Wrappable::destructor_callcount);
  EXPECT_EQ(kMagicAddress,
            wrappable->wrapper()->GetAlignedPointerFromInternalField(1));
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
    js_heap.SetEmbedderStackStateForNextFinalization(
        EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers);
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

}  // namespace internal
}  // namespace v8
