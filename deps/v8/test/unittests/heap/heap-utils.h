// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_HEAP_HEAP_UTILS_H_
#define V8_UNITTESTS_HEAP_HEAP_UTILS_H_

#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class HeapInternalsBase {
 protected:
  void SimulateIncrementalMarking(Heap* heap, bool force_completion);
  void SimulateFullSpace(
      v8::internal::NewSpace* space,
      std::vector<Handle<FixedArray>>* out_handles = nullptr);
  void SimulateFullSpace(v8::internal::PagedSpace* space);
  void FillCurrentPage(v8::internal::NewSpace* space,
                       std::vector<Handle<FixedArray>>* out_handles = nullptr);
};

template <typename TMixin>
class WithHeapInternals : public TMixin, HeapInternalsBase {
 public:
  WithHeapInternals() = default;
  WithHeapInternals(const WithHeapInternals&) = delete;
  WithHeapInternals& operator=(const WithHeapInternals&) = delete;

  void CollectGarbage(i::AllocationSpace space) {
    heap()->CollectGarbage(space, i::GarbageCollectionReason::kTesting);
  }

  void FullGC() {
    heap()->CollectGarbage(OLD_SPACE, i::GarbageCollectionReason::kTesting);
  }

  void YoungGC() {
    heap()->CollectGarbage(NEW_SPACE, i::GarbageCollectionReason::kTesting);
  }

  void CollectAllAvailableGarbage() {
    heap()->CollectAllAvailableGarbage(i::GarbageCollectionReason::kTesting);
  }

  Heap* heap() const { return this->i_isolate()->heap(); }

  void SimulateIncrementalMarking(bool force_completion = true) {
    return HeapInternalsBase::SimulateIncrementalMarking(heap(),
                                                         force_completion);
  }

  void SimulateFullSpace(
      v8::internal::NewSpace* space,
      std::vector<Handle<FixedArray>>* out_handles = nullptr) {
    return HeapInternalsBase::SimulateFullSpace(space, out_handles);
  }
  void SimulateFullSpace(v8::internal::PagedSpace* space) {
    return HeapInternalsBase::SimulateFullSpace(space);
  }

  void GrowNewSpace() {
    SafepointScope scope(heap());
    if (!heap()->new_space()->IsAtMaximumCapacity()) {
      heap()->new_space()->Grow();
    }
  }

  void SealCurrentObjects() {
    // If you see this check failing, disable the flag at the start of your
    // test: v8_flags.stress_concurrent_allocation = false; Background thread
    // allocating concurrently interferes with this function.
    CHECK(!v8_flags.stress_concurrent_allocation);
    FullGC();
    FullGC();
    heap()->mark_compact_collector()->EnsureSweepingCompleted(
        MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
    heap()->old_space()->FreeLinearAllocationArea();
    for (Page* page : *heap()->old_space()) {
      page->MarkNeverAllocateForTesting();
    }
  }

  void GcAndSweep(i::AllocationSpace space) {
    heap()->CollectGarbage(space, GarbageCollectionReason::kTesting);
    if (heap()->mark_compact_collector()->sweeping_in_progress()) {
      SafepointScope scope(heap());
      heap()->mark_compact_collector()->EnsureSweepingCompleted(
          MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
    }
  }
};

START_ALLOW_USE_DEPRECATED()

class V8_NODISCARD TemporaryEmbedderHeapTracerScope {
 public:
  TemporaryEmbedderHeapTracerScope(v8::Isolate* isolate,
                                   v8::EmbedderHeapTracer* tracer)
      : isolate_(isolate) {
    isolate_->SetEmbedderHeapTracer(tracer);
  }

  ~TemporaryEmbedderHeapTracerScope() {
    isolate_->SetEmbedderHeapTracer(nullptr);
  }

 private:
  v8::Isolate* const isolate_;
};

END_ALLOW_USE_DEPRECATED()

using TestWithHeapInternals =                  //
    WithHeapInternals<                         //
        WithInternalIsolateMixin<              //
            WithIsolateScopeMixin<             //
                WithIsolateMixin<              //
                    WithDefaultPlatformMixin<  //
                        ::testing::Test>>>>>;

using TestWithHeapInternalsAndContext =  //
    WithContextMixin<                    //
        TestWithHeapInternals>;

inline void CollectGarbage(i::AllocationSpace space, v8::Isolate* isolate) {
  reinterpret_cast<i::Isolate*>(isolate)->heap()->CollectGarbage(
      space, i::GarbageCollectionReason::kTesting);
}

inline void FullGC(v8::Isolate* isolate) {
  reinterpret_cast<i::Isolate*>(isolate)->heap()->CollectAllGarbage(
      i::Heap::kNoGCFlags, i::GarbageCollectionReason::kTesting);
}

inline void YoungGC(v8::Isolate* isolate) {
  reinterpret_cast<i::Isolate*>(isolate)->heap()->CollectGarbage(
      i::NEW_SPACE, i::GarbageCollectionReason::kTesting);
}

template <typename GlobalOrPersistent>
bool InYoungGeneration(v8::Isolate* isolate, const GlobalOrPersistent& global) {
  CHECK(!v8_flags.single_generation);
  v8::HandleScope scope(isolate);
  auto tmp = global.Get(isolate);
  return i::Heap::InYoungGeneration(*v8::Utils::OpenHandle(*tmp));
}

bool IsNewObjectInCorrectGeneration(HeapObject object);

template <typename GlobalOrPersistent>
bool IsNewObjectInCorrectGeneration(v8::Isolate* isolate,
                                    const GlobalOrPersistent& global) {
  v8::HandleScope scope(isolate);
  auto tmp = global.Get(isolate);
  return IsNewObjectInCorrectGeneration(*v8::Utils::OpenHandle(*tmp));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_HEAP_HEAP_UTILS_H_
