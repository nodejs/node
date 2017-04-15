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

  LocalEmbedderHeapTracer()
      : remote_tracer_(nullptr), num_v8_marking_deque_was_empty_(0) {}

  void SetRemoteTracer(EmbedderHeapTracer* tracer) { remote_tracer_ = tracer; }
  bool InUse() { return remote_tracer_ != nullptr; }

  void TracePrologue();
  void TraceEpilogue();
  void AbortTracing();
  void EnterFinalPause();
  bool Trace(double deadline,
             EmbedderHeapTracer::AdvanceTracingActions actions);

  size_t NumberOfWrappersToTrace();
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

  void NotifyV8MarkingDequeWasEmpty() { num_v8_marking_deque_was_empty_++; }
  bool ShouldFinalizeIncrementalMarking() {
    static const size_t kMaxIncrementalFixpointRounds = 3;
    return !FLAG_incremental_marking_wrappers || !InUse() ||
           NumberOfWrappersToTrace() == 0 ||
           num_v8_marking_deque_was_empty_ > kMaxIncrementalFixpointRounds;
  }

 private:
  typedef std::vector<WrapperInfo> WrapperCache;

  EmbedderHeapTracer* remote_tracer_;
  WrapperCache cached_wrappers_to_trace_;
  size_t num_v8_marking_deque_was_empty_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EMBEDDER_TRACING_H_
