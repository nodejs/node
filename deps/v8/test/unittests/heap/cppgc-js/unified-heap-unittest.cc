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
#include "include/cppgc/testing.h"
#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-cppgc.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-traced-handle.h"
#include "src/api/api-inl.h"
#include "src/common/globals.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/sweeper.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/heap/cppgc-js/unified-heap-utils.h"
#include "test/unittests/heap/heap-utils.h"

namespace v8::internal {

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

TEST_F(UnifiedHeapTest, FindingV8ToCppReference) {
  v8::HandleScope scope(v8_isolate());
  uint16_t wrappable_type = WrapperHelper::kTracedEmbedderId;
  auto* wrappable_object =
      cppgc::MakeGarbageCollected<Wrappable>(allocation_handle());
  v8::Local<v8::Object> api_object = WrapperHelper::CreateWrapper(
      v8_isolate()->GetCurrentContext(), &wrappable_type, wrappable_object);
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
  void* wrappable = cppgc::MakeGarbageCollected<Wrappable>(allocation_handle());
  v8::Local<v8::Object> api_object = WrapperHelper::CreateWrapper(
      v8_isolate()->GetCurrentContext(), nullptr, nullptr);
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
    InvokeMajorGC();
    cpp_heap.AsBase().sweeper().FinishIfRunning();
    EXPECT_TRUE(weak_holder);
  }
  USE(object);
  {
    EmbedderStackStateScope stack_scope(
        &js_heap, EmbedderStackStateScope::kExplicitInvocation,
        StackState::kNoHeapPointers);
    InvokeMajorGC();
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

}  // namespace v8::internal

namespace cppgc {

class CustomSpaceForTest : public CustomSpace<CustomSpaceForTest> {
 public:
  static constexpr size_t kSpaceIndex = 0;
};

constexpr size_t CustomSpaceForTest::kSpaceIndex;

}  // namespace cppgc

namespace v8::internal {

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
}  // namespace v8::internal

namespace cppgc {
template <>
struct SpaceTrait<v8::internal::GCed> {
  using Space = CustomSpaceForTest;
};

}  // namespace cppgc

namespace v8::internal {

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
  {
    v8::HandleScope inner_handle_scope(v8_isolate());
    auto local = v8::Object::New(v8_isolate());
    auto* cpp_obj = cppgc::MakeGarbageCollected<
        InConstructionObjectReferringToGlobalHandle>(
        allocation_handle(),
        reinterpret_cast<i::Isolate*>(v8_isolate())->heap(), local);
    CHECK_NE(kGlobalHandleZapValue,
             ValueHelper::ValueAsAddress(
                 ValueHelper::HandleAsValue(cpp_obj->GetWrapper())));
  }
}

namespace {

class ResetReferenceInDestructorObject final
    : public cppgc::GarbageCollected<ResetReferenceInDestructorObject> {
 public:
  ResetReferenceInDestructorObject(Heap* heap, v8::Local<v8::Object> wrapper)
      : wrapper_(reinterpret_cast<v8::Isolate*>(heap->isolate()), wrapper) {}
  ~ResetReferenceInDestructorObject() { wrapper_.Reset(); }

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(wrapper_); }

 private:
  TracedReference<v8::Object> wrapper_;
};

}  // namespace

TEST_F(UnifiedHeapTest, ResetReferenceInDestructor) {
  v8::HandleScope handle_scope(v8_isolate());
  {
    v8::HandleScope inner_handle_scope(v8_isolate());
    auto local = v8::Object::New(v8_isolate());
    cppgc::MakeGarbageCollected<ResetReferenceInDestructorObject>(
        allocation_handle(),
        reinterpret_cast<i::Isolate*>(v8_isolate())->heap(), local);
  }
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
}

TEST_F(UnifiedHeapTest, OnStackReferencesAreTemporary) {
  ManualGCScope manual_gc(i_isolate());
  v8::Global<v8::Object> observer;
  {
    v8::TracedReference<v8::Value> stack_ref;
    v8::HandleScope scope(v8_isolate());
    v8::Local<v8::Object> api_object = WrapperHelper::CreateWrapper(
        v8_isolate()->GetCurrentContext(), nullptr, nullptr);
    stack_ref.Reset(v8_isolate(), api_object);
    observer.Reset(v8_isolate(), api_object);
    observer.SetWeak();
  }
  EXPECT_FALSE(observer.IsEmpty());
  {
    // Conservative scanning may find stale pointers to on-stack handles.
    // Disable scanning, assuming the slots are overwritten.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        reinterpret_cast<Isolate*>(v8_isolate())->heap());
    CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  }
  EXPECT_TRUE(observer.IsEmpty());
}

TEST_F(UnifiedHeapTest, TracedReferenceOnStack) {
  ManualGCScope manual_gc(i_isolate());
  v8::Global<v8::Object> observer;
  v8::TracedReference<v8::Value> stack_ref;
  {
    v8::HandleScope scope(v8_isolate());
    v8::Local<v8::Object> object = WrapperHelper::CreateWrapper(
        v8_isolate()->GetCurrentContext(), nullptr, nullptr);
    stack_ref.Reset(v8_isolate(), object);
    observer.Reset(v8_isolate(), object);
    observer.SetWeak();
  }
  EXPECT_FALSE(observer.IsEmpty());
  InvokeMajorGC();
  EXPECT_FALSE(observer.IsEmpty());
}

namespace {

enum class Operation {
  kCopy,
  kMove,
};

template <typename T>
V8_NOINLINE void PerformOperation(Operation op, T* target, T* source) {
  switch (op) {
    case Operation::kMove:
      *target = std::move(*source);
      break;
    case Operation::kCopy:
      *target = *source;
      source->Reset();
      break;
  }
}

enum class TargetHandling {
  kNonInitialized,
  kInitializedYoungGen,
  kInitializedOldGen
};

class GCedWithHeapRef final : public cppgc::GarbageCollected<GCedWithHeapRef> {
 public:
  v8::TracedReference<v8::Value> heap_handle;

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(heap_handle); }
};

V8_NOINLINE void StackToHeapTest(v8::Isolate* v8_isolate, Operation op,
                                 TargetHandling target_handling) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  v8::Global<v8::Object> observer;
  v8::TracedReference<v8::Value> stack_handle;
  v8::CppHeap* cpp_heap = v8_isolate->GetCppHeap();
  cppgc::Persistent<GCedWithHeapRef> cpp_heap_obj =
      cppgc::MakeGarbageCollected<GCedWithHeapRef>(
          cpp_heap->GetAllocationHandle());
  if (target_handling != TargetHandling::kNonInitialized) {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> to_object = WrapperHelper::CreateWrapper(
        v8_isolate->GetCurrentContext(), nullptr, nullptr);
    EXPECT_TRUE(
        IsNewObjectInCorrectGeneration(*v8::Utils::OpenHandle(*to_object)));
    if (!v8_flags.single_generation &&
        target_handling == TargetHandling::kInitializedOldGen) {
      InvokeMajorGC(i_isolate);
      EXPECT_FALSE(
          i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    }
    cpp_heap_obj->heap_handle.Reset(v8_isolate, to_object);
  }
  {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> object = WrapperHelper::CreateWrapper(
        v8_isolate->GetCurrentContext(), nullptr, nullptr);
    stack_handle.Reset(v8_isolate, object);
    observer.Reset(v8_isolate, object);
    observer.SetWeak();
  }
  EXPECT_FALSE(observer.IsEmpty());
  InvokeMajorGC(i_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  PerformOperation(op, &cpp_heap_obj->heap_handle, &stack_handle);
  InvokeMajorGC(i_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  cpp_heap_obj.Clear();
  {
    // Conservative scanning may find stale pointers to on-stack handles.
    // Disable scanning, assuming the slots are overwritten.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        i_isolate->heap());
    InvokeMajorGC(i_isolate);
  }
  ASSERT_TRUE(observer.IsEmpty());
}

V8_NOINLINE void HeapToStackTest(v8::Isolate* v8_isolate, Operation op,
                                 TargetHandling target_handling) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  v8::Global<v8::Object> observer;
  v8::TracedReference<v8::Value> stack_handle;
  v8::CppHeap* cpp_heap = v8_isolate->GetCppHeap();
  cppgc::Persistent<GCedWithHeapRef> cpp_heap_obj =
      cppgc::MakeGarbageCollected<GCedWithHeapRef>(
          cpp_heap->GetAllocationHandle());
  if (target_handling != TargetHandling::kNonInitialized) {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> to_object = WrapperHelper::CreateWrapper(
        v8_isolate->GetCurrentContext(), nullptr, nullptr);
    EXPECT_TRUE(
        IsNewObjectInCorrectGeneration(*v8::Utils::OpenHandle(*to_object)));
    if (!v8_flags.single_generation &&
        target_handling == TargetHandling::kInitializedOldGen) {
      InvokeMajorGC(i_isolate);
      EXPECT_FALSE(
          i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    }
    stack_handle.Reset(v8_isolate, to_object);
  }
  {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> object = WrapperHelper::CreateWrapper(
        v8_isolate->GetCurrentContext(), nullptr, nullptr);
    cpp_heap_obj->heap_handle.Reset(v8_isolate, object);
    observer.Reset(v8_isolate, object);
    observer.SetWeak();
  }
  EXPECT_FALSE(observer.IsEmpty());
  InvokeMajorGC(i_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  PerformOperation(op, &stack_handle, &cpp_heap_obj->heap_handle);
  InvokeMajorGC(i_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  stack_handle.Reset();
  {
    // Conservative scanning may find stale pointers to on-stack handles.
    // Disable scanning, assuming the slots are overwritten.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        i_isolate->heap());
    InvokeMajorGC(i_isolate);
  }
  EXPECT_TRUE(observer.IsEmpty());
}

V8_NOINLINE void StackToStackTest(v8::Isolate* v8_isolate, Operation op,
                                  TargetHandling target_handling) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  v8::Global<v8::Object> observer;
  v8::TracedReference<v8::Value> stack_handle1;
  v8::TracedReference<v8::Value> stack_handle2;
  if (target_handling != TargetHandling::kNonInitialized) {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> to_object = WrapperHelper::CreateWrapper(
        v8_isolate->GetCurrentContext(), nullptr, nullptr);
    EXPECT_TRUE(
        IsNewObjectInCorrectGeneration(*v8::Utils::OpenHandle(*to_object)));
    if (!v8_flags.single_generation &&
        target_handling == TargetHandling::kInitializedOldGen) {
      InvokeMajorGC(i_isolate);
      EXPECT_FALSE(
          i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*to_object)));
    }
    stack_handle2.Reset(v8_isolate, to_object);
  }
  {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Object> object = WrapperHelper::CreateWrapper(
        v8_isolate->GetCurrentContext(), nullptr, nullptr);
    stack_handle1.Reset(v8_isolate, object);
    observer.Reset(v8_isolate, object);
    observer.SetWeak();
  }
  EXPECT_FALSE(observer.IsEmpty());
  InvokeMajorGC(i_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  PerformOperation(op, &stack_handle2, &stack_handle1);
  InvokeMajorGC(i_isolate);
  EXPECT_FALSE(observer.IsEmpty());
  stack_handle2.Reset();
  {
    // Conservative scanning may find stale pointers to on-stack handles.
    // Disable scanning, assuming the slots are overwritten.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        i_isolate->heap());
    InvokeMajorGC(i_isolate);
  }
  EXPECT_TRUE(observer.IsEmpty());
}

}  // namespace

TEST_F(UnifiedHeapTest, TracedReferenceMove) {
  ManualGCScope manual_gc(i_isolate());
  StackToHeapTest(v8_isolate(), Operation::kMove,
                  TargetHandling::kNonInitialized);
  StackToHeapTest(v8_isolate(), Operation::kMove,
                  TargetHandling::kInitializedYoungGen);
  StackToHeapTest(v8_isolate(), Operation::kMove,
                  TargetHandling::kInitializedOldGen);
  HeapToStackTest(v8_isolate(), Operation::kMove,
                  TargetHandling::kNonInitialized);
  HeapToStackTest(v8_isolate(), Operation::kMove,
                  TargetHandling::kInitializedYoungGen);
  HeapToStackTest(v8_isolate(), Operation::kMove,
                  TargetHandling::kInitializedOldGen);
  StackToStackTest(v8_isolate(), Operation::kMove,
                   TargetHandling::kNonInitialized);
  StackToStackTest(v8_isolate(), Operation::kMove,
                   TargetHandling::kInitializedYoungGen);
  StackToStackTest(v8_isolate(), Operation::kMove,
                   TargetHandling::kInitializedOldGen);
}

TEST_F(UnifiedHeapTest, TracedReferenceCopy) {
  ManualGCScope manual_gc(i_isolate());
  StackToHeapTest(v8_isolate(), Operation::kCopy,
                  TargetHandling::kNonInitialized);
  StackToHeapTest(v8_isolate(), Operation::kCopy,
                  TargetHandling::kInitializedYoungGen);
  StackToHeapTest(v8_isolate(), Operation::kCopy,
                  TargetHandling::kInitializedOldGen);
  HeapToStackTest(v8_isolate(), Operation::kCopy,
                  TargetHandling::kNonInitialized);
  HeapToStackTest(v8_isolate(), Operation::kCopy,
                  TargetHandling::kInitializedYoungGen);
  HeapToStackTest(v8_isolate(), Operation::kCopy,
                  TargetHandling::kInitializedOldGen);
  StackToStackTest(v8_isolate(), Operation::kCopy,
                   TargetHandling::kNonInitialized);
  StackToStackTest(v8_isolate(), Operation::kCopy,
                   TargetHandling::kInitializedYoungGen);
  StackToStackTest(v8_isolate(), Operation::kCopy,
                   TargetHandling::kInitializedOldGen);
}

TEST_F(UnifiedHeapTest, TracingInEphemerons) {
  // Tests that wrappers that are part of ephemerons are traced.
  ManualGCScope manual_gc(i_isolate());
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);

  uint16_t wrappable_type = WrapperHelper::kTracedEmbedderId;
  Wrappable::destructor_callcount = 0;

  v8::Local<v8::Object> key =
      v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
  Handle<JSWeakMap> weak_map = i_isolate()->factory()->NewJSWeakMap();
  {
    v8::HandleScope inner_scope(v8_isolate());
    // C++ object that should be traced through ephemeron value.
    auto* wrappable_object =
        cppgc::MakeGarbageCollected<Wrappable>(allocation_handle());
    v8::Local<v8::Object> value = WrapperHelper::CreateWrapper(
        v8_isolate()->GetCurrentContext(), &wrappable_type, wrappable_object);
    EXPECT_FALSE(value.IsEmpty());
    Handle<JSObject> js_key =
        handle(JSObject::cast(*v8::Utils::OpenHandle(*key)), i_isolate());
    Handle<JSReceiver> js_value = v8::Utils::OpenHandle(*value);
    int32_t hash = js_key->GetOrCreateHash(i_isolate()).value();
    JSWeakCollection::Set(weak_map, js_key, js_value, hash);
  }
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(Wrappable::destructor_callcount, 0u);
}

TEST_F(UnifiedHeapTest, TracedReferenceHandlesDoNotLeak) {
  // TracedReference handles are not cleared by the destructor of the embedder
  // object. To avoid leaks we need to mark these handles during GC.
  // This test checks that unmarked handles do not leak.
  ManualGCScope manual_gc(i_isolate());
  v8::HandleScope scope(v8_isolate());
  v8::TracedReference<v8::Value> ref;
  ref.Reset(v8_isolate(), v8::Undefined(v8_isolate()));
  auto* traced_handles = i_isolate()->traced_handles();
  const size_t initial_count = traced_handles->used_node_count();
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  const size_t final_count = traced_handles->used_node_count();
  EXPECT_EQ(initial_count, final_count + 1);
}

namespace {
class Wrappable2 final : public cppgc::GarbageCollected<Wrappable2> {
 public:
  static size_t destructor_call_count;
  void Trace(cppgc::Visitor* visitor) const {}
  ~Wrappable2() { destructor_call_count++; }
};

size_t Wrappable2::destructor_call_count = 0;
}  // namespace

TEST_F(UnifiedHeapTest, WrapperDescriptorGetter) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope scope(isolate);
  auto* wrappable_object =
      cppgc::MakeGarbageCollected<Wrappable2>(allocation_handle());
  v8::WrapperDescriptor descriptor =
      isolate->GetCppHeap()->wrapper_descriptor();
  v8::Local<v8::ObjectTemplate> tmpl = v8::ObjectTemplate::New(isolate);
  int size = std::max(descriptor.wrappable_type_index,
                      descriptor.wrappable_instance_index) +
             1;
  tmpl->SetInternalFieldCount(size);
  v8::Local<v8::Object> api_object =
      tmpl->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
  api_object->SetAlignedPointerInInternalField(
      descriptor.wrappable_type_index,
      &descriptor.embedder_id_for_garbage_collected);
  api_object->SetAlignedPointerInInternalField(
      descriptor.wrappable_instance_index, wrappable_object);

  Wrappable2::destructor_call_count = 0;
  EXPECT_EQ(0u, Wrappable2::destructor_call_count);
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(0u, Wrappable2::destructor_call_count);
  api_object->SetAlignedPointerInInternalField(
      descriptor.wrappable_instance_index, nullptr);
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(1u, Wrappable2::destructor_call_count);
}

}  // namespace v8::internal
