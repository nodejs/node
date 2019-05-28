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
class JSObject;

class V8_EXPORT_PRIVATE LocalEmbedderHeapTracer final {
 public:
  using WrapperInfo = std::pair<void*, void*>;
  using WrapperCache = std::vector<WrapperInfo>;

  class V8_EXPORT_PRIVATE ProcessingScope {
   public:
    explicit ProcessingScope(LocalEmbedderHeapTracer* tracer);
    ~ProcessingScope();

    void TracePossibleWrapper(JSObject js_object);

    void AddWrapperInfoForTesting(WrapperInfo info);

   private:
    static constexpr size_t kWrapperCacheSize = 1000;

    void FlushWrapperCacheIfFull();

    LocalEmbedderHeapTracer* const tracer_;
    WrapperCache wrapper_cache_;
  };

  explicit LocalEmbedderHeapTracer(Isolate* isolate) : isolate_(isolate) {}

  ~LocalEmbedderHeapTracer() {
    if (remote_tracer_) remote_tracer_->isolate_ = nullptr;
  }

  bool InUse() const { return remote_tracer_ != nullptr; }
  EmbedderHeapTracer* remote_tracer() const { return remote_tracer_; }

  void SetRemoteTracer(EmbedderHeapTracer* tracer);
  void TracePrologue();
  void TraceEpilogue();
  void EnterFinalPause();
  bool Trace(double deadline);
  bool IsRemoteTracingDone();

  bool IsRootForNonTracingGC(const v8::TracedGlobal<v8::Value>& handle) {
    return !InUse() || remote_tracer_->IsRootForNonTracingGC(handle);
  }

  void NotifyV8MarkingWorklistWasEmpty() {
    num_v8_marking_worklist_was_empty_++;
  }

  bool ShouldFinalizeIncrementalMarking() {
    static const size_t kMaxIncrementalFixpointRounds = 3;
    return !FLAG_incremental_marking_wrappers || !InUse() ||
           (IsRemoteTracingDone() && embedder_worklist_empty_) ||
           num_v8_marking_worklist_was_empty_ > kMaxIncrementalFixpointRounds;
  }

  void SetEmbedderStackStateForNextFinalization(
      EmbedderHeapTracer::EmbedderStackState stack_state);

  void SetEmbedderWorklistEmpty(bool is_empty) {
    embedder_worklist_empty_ = is_empty;
  }

 private:
  Isolate* const isolate_;
  EmbedderHeapTracer* remote_tracer_ = nullptr;

  size_t num_v8_marking_worklist_was_empty_ = 0;
  EmbedderHeapTracer::EmbedderStackState embedder_stack_state_ =
      EmbedderHeapTracer::kUnknown;
  // Indicates whether the embedder worklist was observed empty on the main
  // thread. This is opportunistic as concurrent marking tasks may hold local
  // segments of potential embedder fields to move to the main thread.
  bool embedder_worklist_empty_ = false;

  friend class EmbedderStackStateScope;
};

class V8_EXPORT_PRIVATE EmbedderStackStateScope final {
 public:
  EmbedderStackStateScope(LocalEmbedderHeapTracer* local_tracer,
                          EmbedderHeapTracer::EmbedderStackState stack_state)
      : local_tracer_(local_tracer),
        old_stack_state_(local_tracer_->embedder_stack_state_) {
    local_tracer_->embedder_stack_state_ = stack_state;
  }

  ~EmbedderStackStateScope() {
    local_tracer_->embedder_stack_state_ = old_stack_state_;
  }

 private:
  LocalEmbedderHeapTracer* const local_tracer_;
  const EmbedderHeapTracer::EmbedderStackState old_stack_state_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EMBEDDER_TRACING_H_
