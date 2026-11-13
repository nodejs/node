// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "include/v8-embedder-heap.h"
#include "include/v8-function.h"
#include "include/v8-traced-handle.h"
#include "src/handles/handles.h"
#include "src/handles/traced-handles.h"
#include "test/unittests/heap/cppgc-js/unified-heap-utils.h"
#include "test/unittests/heap/heap-utils.h"

namespace v8::internal {

namespace {

using EmbedderRootsHandlerTest = TestWithHeapInternalsAndContext;

class V8_NODISCARD TemporaryEmbedderRootsHandleScope final {
 public:
  TemporaryEmbedderRootsHandleScope(v8::Isolate* isolate,
                                    v8::EmbedderRootsHandler* handler)
      : isolate_(isolate) {
    isolate_->SetEmbedderRootsHandler(handler);
  }

  ~TemporaryEmbedderRootsHandleScope() {
    isolate_->SetEmbedderRootsHandler(nullptr);
  }

 private:
  v8::Isolate* const isolate_;
};

// EmbedderRootsHandler that can optimize Scavenger handling when used with
// TracedReference.
class ClearingEmbedderRootsHandler final : public v8::EmbedderRootsHandler {
 public:
  explicit ClearingEmbedderRootsHandler(v8::Isolate* isolate)
      : EmbedderRootsHandler(), isolate_(isolate) {}

  void ResetRoot(const v8::TracedReference<v8::Value>& handle) final {
    // Convention for test: Objects that are optimized have use a back pointer
    // in the wrappable field.
    BasicTracedReference<v8::Value>* original_handle =
        reinterpret_cast<BasicTracedReference<v8::Value>*>(
            v8::Object::Unwrap<kTagForTesting>(isolate_,
                                               handle.As<v8::Object>()));
    original_handle->Reset();
  }

 private:
  v8::Isolate* const isolate_;
};

void ConstructNonDroppableJSObject(v8::Isolate* isolate,
                                   v8::Local<v8::Context> context,
                                   v8::TracedReference<v8::Object>* handle) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> object(v8::Object::New(isolate));
  EXPECT_FALSE(object.IsEmpty());
  *handle = v8::TracedReference<v8::Object>(isolate, object);
  EXPECT_FALSE(handle->IsEmpty());
}

void ConstructNonDroppableJSApiObject(v8::Isolate* isolate,
                                      v8::Local<v8::Context> context,
                                      v8::TracedReference<v8::Object>* handle) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> object = WrapperHelper::CreateWrapper(context, nullptr);
  EXPECT_FALSE(object.IsEmpty());
  *handle = v8::TracedReference<v8::Object>(isolate, object);
  EXPECT_FALSE(handle->IsEmpty());
}

void ConstructDroppableJSApiObject(v8::Isolate* isolate,
                                   v8::Local<v8::Context> context,
                                   v8::TracedReference<v8::Object>* handle) {
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> object = WrapperHelper::CreateWrapper(context, handle);
  EXPECT_FALSE(object.IsEmpty());
  *handle = v8::TracedReference<v8::Object>(
      isolate, object, typename v8::TracedReference<v8::Object>::IsDroppable{});
  EXPECT_FALSE(handle->IsEmpty());
}

}  // namespace

namespace {

enum class SurvivalMode { kSurvives, kDies };

template <typename ModifierFunction, typename ConstructTracedReferenceFunction,
          typename GCFunction>
void TracedReferenceTest(v8::Isolate* isolate,
                         ConstructTracedReferenceFunction construct_function,
                         ModifierFunction modifier_function,
                         GCFunction gc_function, SurvivalMode survives) {
  auto i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ManualGCScope manual_gc_scope(i_isolate);
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(
      i_isolate->heap());
  v8::HandleScope scope(isolate);
  auto* traced_handles = i_isolate->traced_handles();
  const size_t initial_count = traced_handles->used_node_count();
  // Store v8::TracedReference on the stack here on purpose. On Android storing
  // it on the heap is problematic. This is because the native memory allocator
  // on Android sets the top-byte of allocations for verification. However, in
  // same tests we store the address of the v8::TracedReference in the
  // CppHeapPointerTable to simulate a cppgc wrapper object. The table expectes
  // the hightest 16-bit to be 0 for all entries.
  v8::TracedReference<v8::Object> handle;
  construct_function(isolate, isolate->GetCurrentContext(), &handle);
  ASSERT_TRUE(IsNewObjectInCorrectGeneration(isolate, handle));
  modifier_function(handle);
  const size_t after_modification_count = traced_handles->used_node_count();
  gc_function();
  // Cannot check the handle as it is not explicitly cleared by the GC. Instead
  // check the handles count.
  CHECK_IMPLIES(survives == SurvivalMode::kSurvives,
                after_modification_count == traced_handles->used_node_count());
  CHECK_IMPLIES(survives == SurvivalMode::kDies,
                initial_count == traced_handles->used_node_count());
}

}  // namespace

TEST_F(EmbedderRootsHandlerTest,
       FullGC_UnreachableTracedReferenceToNonDroppableDies) {
  if (v8_flags.stress_incremental_marking) {
    GTEST_SKIP() << "When stressing incremental marking, a write barrier may "
                    "keep the object alive.";
  }

  ClearingEmbedderRootsHandler handler(v8_isolate());
  TemporaryEmbedderRootsHandleScope roots_handler_scope(v8_isolate(), &handler);
  TracedReferenceTest(
      v8_isolate(), ConstructNonDroppableJSObject,
      [](const TracedReference<v8::Object>&) {}, [this]() { InvokeMajorGC(); },
      SurvivalMode::kDies);
}

TEST_F(EmbedderRootsHandlerTest,
       FullGC_UnreachableTracedReferenceToNonDroppableDies2) {
  ManualGCScope manual_gcs(i_isolate());
  ClearingEmbedderRootsHandler handler(v8_isolate());
  TemporaryEmbedderRootsHandleScope roots_handler_scope(v8_isolate(), &handler);
  // The TracedReference itself will die as it's not found by the full GC. The
  // pointee will be kept alive through other means.
  v8::Global<v8::Object> strong_global;
  TracedReferenceTest(
      v8_isolate(), ConstructNonDroppableJSObject,
      [this, &strong_global](const TracedReference<v8::Object>& handle) {
        v8::HandleScope scope(v8_isolate());
        strong_global =
            v8::Global<v8::Object>(v8_isolate(), handle.Get(v8_isolate()));
      },
      [this, &strong_global]() {
        InvokeMajorGC();
        strong_global.Reset();
      },
      SurvivalMode::kDies);
}

TEST_F(EmbedderRootsHandlerTest,
       YoungGC_UnreachableTracedReferenceToNonDroppableSurvives) {
  if (v8_flags.single_generation) GTEST_SKIP();

  ManualGCScope manual_gc(i_isolate());
  ClearingEmbedderRootsHandler handler(v8_isolate());
  TemporaryEmbedderRootsHandleScope roots_handler_scope(v8_isolate(), &handler);
  TracedReferenceTest(
      v8_isolate(), ConstructNonDroppableJSObject,
      [](const TracedReference<v8::Object>&) {}, [this]() { InvokeMinorGC(); },
      SurvivalMode::kSurvives);
}

TEST_F(EmbedderRootsHandlerTest,
       YoungGC_UnreachableTracedReferenceToNonDroppableAPIObjectSurvives) {
  if (v8_flags.single_generation) GTEST_SKIP();

  ManualGCScope manual_gc(i_isolate());
  ClearingEmbedderRootsHandler handler(v8_isolate());
  TemporaryEmbedderRootsHandleScope roots_handler_scope(v8_isolate(), &handler);
  TracedReferenceTest(
      v8_isolate(), ConstructNonDroppableJSApiObject,
      [](const TracedReference<v8::Object>&) {}, [this]() { InvokeMinorGC(); },
      SurvivalMode::kSurvives);
}

TEST_F(EmbedderRootsHandlerTest,
       YoungGC_UnreachableTracedReferenceToDroppableDies) {
  if (v8_flags.single_generation || !v8_flags.reclaim_unmodified_wrappers) {
    GTEST_SKIP();
  }

  ManualGCScope manual_gc(i_isolate());
  ClearingEmbedderRootsHandler handler(v8_isolate());
  TemporaryEmbedderRootsHandleScope roots_handler_scope(v8_isolate(), &handler);
  TracedReferenceTest(
      v8_isolate(), ConstructDroppableJSApiObject,
      [](TracedReference<v8::Object>& handle) {}, [this]() { InvokeMinorGC(); },
      SurvivalMode::kDies);
}

namespace {

// Simulates Blink's ScriptWrappable + DOMDataStore multi-world wrapper storage:
// - `inline_wrappers_` represents main-world wrappers stored inline in
//   `ScriptWrappable::wrapper_`.
// - `secondary_wrappers_` represents isolated-world wrappers stored in a
//   per-world `DOMDataStore::wrapper_map_`.
// Both the main-world and isolated-world JS wrapper objects for item `i`
// unwrap via `v8::Object::Unwrap` to the same `&inline_wrappers_[i]` address.
class SharedWrappableEmbedderRootsHandler final
    : public v8::EmbedderRootsHandler {
 public:
  SharedWrappableEmbedderRootsHandler(
      v8::Isolate* isolate, v8::TracedReference<v8::Object>* inline_wrappers,
      v8::TracedReference<v8::Object>* secondary_wrappers)
      : EmbedderRootsHandler(),
        isolate_(isolate),
        inline_wrappers_(inline_wrappers),
        secondary_wrappers_(secondary_wrappers) {}

  void ResetRoot(const v8::TracedReference<v8::Value>& handle) final {
    if (TryResetRoot(handle)) {
      return;
    }
    const v8::TracedReference<v8::Object>& traced = handle.As<v8::Object>();
    auto* inline_wrapper = reinterpret_cast<v8::TracedReference<v8::Object>*>(
        v8::Object::Unwrap<kTagForTesting>(isolate_, traced));
    const size_t index = static_cast<size_t>(inline_wrapper - inline_wrappers_);
    if (secondary_wrappers_[index] == traced) {
      secondary_wrappers_[index].Reset();
    }
  }

  bool TryResetRoot(const v8::TracedReference<v8::Value>& handle) final {
    const v8::TracedReference<v8::Object>& traced = handle.As<v8::Object>();
    auto* inline_wrapper = reinterpret_cast<v8::TracedReference<v8::Object>*>(
        v8::Object::Unwrap<kTagForTesting>(isolate_, traced));
    if (*inline_wrapper == traced) {
      inline_wrapper->Reset();
      return true;
    }
    return false;
  }

 private:
  v8::Isolate* const isolate_;
  v8::TracedReference<v8::Object>* const inline_wrappers_;
  v8::TracedReference<v8::Object>* const secondary_wrappers_;
};

}  // namespace

TEST_F(EmbedderRootsHandlerTest,
       YoungGC_ParallelResetRootSharedWrappableAcrossWorlds) {
  if (v8_flags.single_generation || !v8_flags.reclaim_unmodified_wrappers ||
      !v8_flags.parallel_reclaim_unmodified_wrappers) {
    GTEST_SKIP();
  }

  ManualGCScope manual_gc(i_isolate());
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(
      i_isolate()->heap());
  GrowNewSpaceToMaximumCapacity();

  // ParallelWeakHandlesProcessor spawns up to kMaxParallelTasks = 3 tasks at
  // kBlocksPerTask = 8 granularity. Allocate pairs of droppable handles in
  // block-sized batches (256) so that each item's inline_wrapper and
  // secondary_wrapper reside in adjacent TracedNodeBlocks and are processed
  // concurrently by different threads during minor GC.
  constexpr size_t kBatchSize = 256;
  constexpr size_t kNumWrappables = kBatchSize * 12;
  std::array<v8::TracedReference<v8::Object>, kNumWrappables> inline_wrappers;
  std::array<v8::TracedReference<v8::Object>, kNumWrappables>
      secondary_wrappers;

  SharedWrappableEmbedderRootsHandler handler(
      v8_isolate(), inline_wrappers.data(), secondary_wrappers.data());
  TemporaryEmbedderRootsHandleScope roots_handler_scope(v8_isolate(), &handler);

  v8::HandleScope outer_scope(v8_isolate());
  v8::Local<v8::Context> context = v8_isolate()->GetCurrentContext();
  v8::Local<v8::Function> ctor = v8::FunctionTemplate::New(v8_isolate())
                                     ->GetFunction(context)
                                     .ToLocalChecked();

  auto* traced_handles = i_isolate()->traced_handles();
  const size_t initial_count = traced_handles->used_node_count();

  for (int round = 0; round < 10; ++round) {
    for (size_t base = 0; base < kNumWrappables; base += kBatchSize) {
      v8::HandleScope scope(v8_isolate());
      for (size_t i = base; i < base + kBatchSize; ++i) {
        v8::Local<v8::Object> obj = ctor->NewInstance(context).ToLocalChecked();
        WrapperHelper::SetWrappableConnection(v8_isolate(), obj,
                                              &inline_wrappers[i]);
        inline_wrappers[i].Reset(
            v8_isolate(), obj,
            typename v8::TracedReference<v8::Object>::IsDroppable{});
      }
      for (size_t i = base; i < base + kBatchSize; ++i) {
        v8::Local<v8::Object> obj = ctor->NewInstance(context).ToLocalChecked();
        WrapperHelper::SetWrappableConnection(v8_isolate(), obj,
                                              &inline_wrappers[i]);
        secondary_wrappers[i].Reset(
            v8_isolate(), obj,
            typename v8::TracedReference<v8::Object>::IsDroppable{});
      }
    }

    InvokeMinorGC();
    CHECK_EQ(initial_count, traced_handles->used_node_count());
  }
}

}  // namespace v8::internal
