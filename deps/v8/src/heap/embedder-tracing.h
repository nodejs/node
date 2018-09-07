// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_EMBEDDER_TRACING_H_
#define V8_HEAP_EMBEDDER_TRACING_H_

#include "include/v8.h"
#include "src/flags.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Heap;

class V8_EXPORT_PRIVATE LocalEmbedderHeapTracer final {
 public:
  typedef std::pair<void*, void*> WrapperInfo;

  explicit LocalEmbedderHeapTracer(Isolate* isolate)
      : isolate_(isolate),
        remote_tracer_(nullptr),
        num_v8_marking_worklist_was_empty_(0) {}

  ~LocalEmbedderHeapTracer() {
    if (remote_tracer_) remote_tracer_->isolate_ = nullptr;
  }

  void SetRemoteTracer(EmbedderHeapTracer* tracer) {
    if (remote_tracer_) remote_tracer_->isolate_ = nullptr;

    remote_tracer_ = tracer;
    if (remote_tracer_)
      remote_tracer_->isolate_ = reinterpret_cast<v8::Isolate*>(isolate_);
  }

  bool InUse() { return remote_tracer_ != nullptr; }

  void TracePrologue();
  void TraceEpilogue();
  void AbortTracing();
  void EnterFinalPause();
  bool Trace(double deadline,
             EmbedderHeapTracer::AdvanceTracingActions actions);
  bool IsRemoteTracingDone();

  size_t NumberOfCachedWrappersToTrace() {
    return cached_wrappers_to_trace_.size();
  }
  void AddWrapperToTrace(WrapperInfo entry) {
    cached_wrappers_to_trace_.push_back(entry);
  }
  void ClearCachedWrappersToTrace() { cached_wrappers_to_trace_.clear(); }
  void RegisterWrappersWithRemoteTracer();

  // In order to avoid running out of memory we force tracing wrappers if there
  // are too many of them.
  bool RequiresImmediateWrapperProcessing();

  void NotifyV8MarkingWorklistWasEmpty() {
    num_v8_marking_worklist_was_empty_++;
  }
  bool ShouldFinalizeIncrementalMarking() {
    static const size_t kMaxIncrementalFixpointRounds = 3;
    return !FLAG_incremental_marking_wrappers || !InUse() ||
           IsRemoteTracingDone() ||
           num_v8_marking_worklist_was_empty_ > kMaxIncrementalFixpointRounds;
  }

 private:
  typedef std::vector<WrapperInfo> WrapperCache;

  Isolate* const isolate_;
  EmbedderHeapTracer* remote_tracer_;
  WrapperCache cached_wrappers_to_trace_;
  size_t num_v8_marking_worklist_was_empty_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EMBEDDER_TRACING_H_
