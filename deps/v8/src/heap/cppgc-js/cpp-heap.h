// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_CPP_HEAP_H_
#define V8_HEAP_CPPGC_JS_CPP_HEAP_H_

#if CPPGC_IS_STANDALONE
static_assert(
    false, "V8 targets can not be built with cppgc_is_standalone set to true.");
#endif

#include "include/v8-cppgc.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/stats-collector.h"
#include "src/logging/metrics.h"

namespace v8 {

class Isolate;

namespace internal {

// A C++ heap implementation used with V8 to implement unified heap.
class V8_EXPORT_PRIVATE CppHeap final
    : public cppgc::internal::HeapBase,
      public v8::CppHeap,
      public v8::EmbedderHeapTracer,
      public cppgc::internal::StatsCollector::AllocationObserver {
 public:
  class MetricRecorderAdapter final : public cppgc::internal::MetricRecorder {
   public:
    static constexpr int kMaxBatchedEvents = 16;

    explicit MetricRecorderAdapter(CppHeap& cpp_heap) : cpp_heap_(cpp_heap) {}

    void AddMainThreadEvent(const FullCycle& cppgc_event) final;
    void AddMainThreadEvent(const MainThreadIncrementalMark& cppgc_event) final;
    void AddMainThreadEvent(
        const MainThreadIncrementalSweep& cppgc_event) final;

    void FlushBatchedIncrementalEvents();

    // The following 3 methods are only used for reporting nested cpp events
    // through V8. Standalone events are reported directly.
    bool MetricsReportPending() const;

    const base::Optional<cppgc::internal::MetricRecorder::FullCycle>
    ExtractLastFullGcEvent();
    const base::Optional<
        cppgc::internal::MetricRecorder::MainThreadIncrementalMark>
    ExtractLastIncrementalMarkEvent();

   private:
    Isolate* GetIsolate() const;

    v8::metrics::Recorder::ContextId GetContextId() const;

    CppHeap& cpp_heap_;
    v8::metrics::GarbageCollectionFullMainThreadBatchedIncrementalMark
        incremental_mark_batched_events_;
    v8::metrics::GarbageCollectionFullMainThreadBatchedIncrementalSweep
        incremental_sweep_batched_events_;
    base::Optional<cppgc::internal::MetricRecorder::FullCycle>
        last_full_gc_event_;
    base::Optional<cppgc::internal::MetricRecorder::MainThreadIncrementalMark>
        last_incremental_mark_event_;
  };

  static CppHeap* From(v8::CppHeap* heap) {
    return static_cast<CppHeap*>(heap);
  }
  static const CppHeap* From(const v8::CppHeap* heap) {
    return static_cast<const CppHeap*>(heap);
  }

  CppHeap(
      v8::Platform* platform,
      const std::vector<std::unique_ptr<cppgc::CustomSpaceBase>>& custom_spaces,
      const v8::WrapperDescriptor& wrapper_descriptor);
  ~CppHeap() final;

  CppHeap(const CppHeap&) = delete;
  CppHeap& operator=(const CppHeap&) = delete;

  HeapBase& AsBase() { return *this; }
  const HeapBase& AsBase() const { return *this; }

  void AttachIsolate(Isolate* isolate);
  void DetachIsolate();

  void Terminate();

  void EnableDetachedGarbageCollectionsForTesting();

  void CollectGarbageForTesting(
      cppgc::internal::GarbageCollector::Config::StackState);

  void CollectCustomSpaceStatisticsAtLastGC(
      std::vector<cppgc::CustomSpaceIndex>,
      std::unique_ptr<CustomSpaceStatisticsReceiver>);

  // v8::EmbedderHeapTracer interface.
  void RegisterV8References(
      const std::vector<std::pair<void*, void*> >& embedder_fields) final;
  void TracePrologue(TraceFlags flags) final;
  bool AdvanceTracing(double deadline_in_ms) final;
  bool IsTracingDone() final;
  void TraceEpilogue(TraceSummary* trace_summary) final;
  void EnterFinalPause(EmbedderStackState stack_state) final;

  // StatsCollector::AllocationObserver interface.
  void AllocatedObjectSizeIncreased(size_t) final;
  void AllocatedObjectSizeDecreased(size_t) final;
  void ResetAllocatedObjectSize(size_t) final {}

  MetricRecorderAdapter* GetMetricRecorder() const;

 private:
  void FinalizeIncrementalGarbageCollectionIfNeeded(
      cppgc::Heap::StackState) final {
    // For unified heap, CppHeap shouldn't finalize independently (i.e.
    // finalization is not needed) thus this method is left empty.
  }

  void ReportBufferedAllocationSizeIfPossible();

  void StartIncrementalGarbageCollectionForTesting() final;
  void FinalizeIncrementalGarbageCollectionForTesting(EmbedderStackState) final;

  Isolate* isolate_ = nullptr;
  bool marking_done_ = false;
  TraceFlags current_flags_ = TraceFlags::kNoFlags;

  // Buffered allocated bytes. Reporting allocated bytes to V8 can trigger a GC
  // atomic pause. Allocated bytes are buffer in case this is temporarily
  // prohibited.
  int64_t buffered_allocated_bytes_ = 0;

  v8::WrapperDescriptor wrapper_descriptor_;

  bool in_detached_testing_mode_ = false;
  bool force_incremental_marking_for_testing_ = false;

  bool is_in_v8_marking_step_ = false;

  friend class MetricRecorderAdapter;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_CPP_HEAP_H_
