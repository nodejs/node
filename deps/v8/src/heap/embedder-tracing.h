// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_EMBEDDER_TRACING_H_
#define V8_HEAP_EMBEDDER_TRACING_H_

#include <atomic>

#include "include/v8-cppgc.h"
#include "include/v8-embedder-heap.h"
#include "include/v8-traced-handle.h"
#include "src/common/allow-deprecated.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/heap/cppgc-js/cpp-heap.h"

namespace v8 {
namespace internal {

class Heap;
class JSObject;

class V8_EXPORT_PRIVATE DefaultEmbedderRootsHandler final
    : public EmbedderRootsHandler {
 public:
  bool IsRoot(const v8::TracedReference<v8::Value>& handle) final;

  void ResetRoot(const v8::TracedReference<v8::Value>& handle) final;

  void SetTracer(EmbedderHeapTracer* tracer) { tracer_ = tracer; }

 private:
  EmbedderHeapTracer* tracer_ = nullptr;
};

class V8_EXPORT_PRIVATE LocalEmbedderHeapTracer final {
 public:
  using WrapperInfo = std::pair<void*, void*>;
  using WrapperCache = std::vector<WrapperInfo>;

  // WrapperInfo is passed over the API. Use VerboseWrapperInfo to access pair
  // internals in a named way. See ProcessingScope::TracePossibleJSWrapper()
  // below on how a V8 object is parsed to gather the information.
  struct VerboseWrapperInfo {
    constexpr explicit VerboseWrapperInfo(const WrapperInfo& raw_info)
        : raw_info(raw_info) {}

    // Information describing the type pointed to via instance().
    void* type_info() const { return raw_info.first; }
    // Direct pointer to an instance described by type_info().
    void* instance() const { return raw_info.second; }
    // Returns whether the info is empty and thus does not keep a C++ object
    // alive.
    bool is_empty() const { return !type_info() || !instance(); }

    const WrapperInfo& raw_info;
  };

  class V8_EXPORT_PRIVATE V8_NODISCARD ProcessingScope {
   public:
    explicit ProcessingScope(LocalEmbedderHeapTracer* tracer);
    ~ProcessingScope();

    void TracePossibleWrapper(JSObject js_object);

    void AddWrapperInfoForTesting(WrapperInfo info);

   private:
    static constexpr size_t kWrapperCacheSize = 1000;

    void FlushWrapperCacheIfFull();

    LocalEmbedderHeapTracer* const tracer_;
    const WrapperDescriptor wrapper_descriptor_;
    WrapperCache wrapper_cache_;
  };

  static V8_INLINE bool ExtractWrappableInfo(Isolate*, JSObject,
                                             const WrapperDescriptor&,
                                             WrapperInfo*);
  static V8_INLINE bool ExtractWrappableInfo(
      Isolate*, const WrapperDescriptor&, const EmbedderDataSlot& type_slot,
      const EmbedderDataSlot& instance_slot, WrapperInfo*);

  explicit LocalEmbedderHeapTracer(Isolate* isolate) : isolate_(isolate) {}

  ~LocalEmbedderHeapTracer() {
    if (remote_tracer_) remote_tracer_->isolate_ = nullptr;
    // CppHeap is not detached from Isolate here. Detaching is done explciitly
    // on Isolate/Heap/CppHeap destruction.
  }

  bool InUse() const { return cpp_heap_ || (remote_tracer_ != nullptr); }
  // This method doesn't take CppHeap into account.
  EmbedderHeapTracer* remote_tracer() const {
    DCHECK_NULL(cpp_heap_);
    return remote_tracer_;
  }

  void SetRemoteTracer(EmbedderHeapTracer* tracer);
  void SetCppHeap(CppHeap* cpp_heap);
  void PrepareForTrace(EmbedderHeapTracer::TraceFlags flags);
  void TracePrologue(EmbedderHeapTracer::TraceFlags flags);
  void TraceEpilogue();
  void EnterFinalPause();
  bool Trace(double deadline);
  bool IsRemoteTracingDone();

  bool ShouldFinalizeIncrementalMarking() {
    return !FLAG_incremental_marking_wrappers || !InUse() ||
           (IsRemoteTracingDone() && embedder_worklist_empty_);
  }

  void SetEmbedderWorklistEmpty(bool is_empty) {
    embedder_worklist_empty_ = is_empty;
  }

  void IncreaseAllocatedSize(size_t bytes) {
    remote_stats_.used_size.fetch_add(bytes, std::memory_order_relaxed);
    remote_stats_.allocated_size += bytes;
    if (remote_stats_.allocated_size >
        remote_stats_.allocated_size_limit_for_check) {
      StartIncrementalMarkingIfNeeded();
      remote_stats_.allocated_size_limit_for_check =
          remote_stats_.allocated_size + kEmbedderAllocatedThreshold;
    }
  }

  void DecreaseAllocatedSize(size_t bytes) {
    DCHECK_GE(remote_stats_.used_size.load(std::memory_order_relaxed), bytes);
    remote_stats_.used_size.fetch_sub(bytes, std::memory_order_relaxed);
  }

  void StartIncrementalMarkingIfNeeded();

  size_t used_size() const {
    return remote_stats_.used_size.load(std::memory_order_relaxed);
  }
  size_t allocated_size() const { return remote_stats_.allocated_size; }

  WrapperInfo ExtractWrapperInfo(Isolate* isolate, JSObject js_object);

  void SetWrapperDescriptor(const WrapperDescriptor& wrapper_descriptor) {
    DCHECK_NULL(cpp_heap_);
    wrapper_descriptor_ = wrapper_descriptor;
  }

  void UpdateRemoteStats(size_t, double);

  DefaultEmbedderRootsHandler& default_embedder_roots_handler() {
    return default_embedder_roots_handler_;
  }

  void NotifyEmptyEmbedderStack();

  EmbedderHeapTracer::EmbedderStackState embedder_stack_state() const {
    return embedder_stack_state_;
  }

  void EmbedderWriteBarrier(Heap*, JSObject);

 private:
  static constexpr size_t kEmbedderAllocatedThreshold = 128 * KB;

  static constexpr WrapperDescriptor::InternalFieldIndex
      kDefaultWrapperTypeEmbedderIndex = 0;
  static constexpr WrapperDescriptor::InternalFieldIndex
      kDefaultWrapperInstanceEmbedderIndex = 1;

  static constexpr WrapperDescriptor GetDefaultWrapperDescriptor() {
    // The default descriptor assumes the indices that known embedders use.
    return WrapperDescriptor(kDefaultWrapperTypeEmbedderIndex,
                             kDefaultWrapperInstanceEmbedderIndex,
                             WrapperDescriptor::kUnknownEmbedderId);
  }

  CppHeap* cpp_heap() {
    DCHECK_NOT_NULL(cpp_heap_);
    DCHECK_NULL(remote_tracer_);
    DCHECK_IMPLIES(isolate_, cpp_heap_ == isolate_->heap()->cpp_heap());
    return cpp_heap_;
  }

  WrapperDescriptor wrapper_descriptor() {
    if (cpp_heap_)
      return cpp_heap()->wrapper_descriptor();
    else
      return wrapper_descriptor_;
  }

  Isolate* const isolate_;
  EmbedderHeapTracer* remote_tracer_ = nullptr;
  CppHeap* cpp_heap_ = nullptr;
  DefaultEmbedderRootsHandler default_embedder_roots_handler_;

  EmbedderHeapTracer::EmbedderStackState embedder_stack_state_ =
      EmbedderHeapTracer::EmbedderStackState::kMayContainHeapPointers;
  // Indicates whether the embedder worklist was observed empty on the main
  // thread. This is opportunistic as concurrent marking tasks may hold local
  // segments of potential embedder fields to move to the main thread.
  bool embedder_worklist_empty_ = false;

  struct RemoteStatistics {
    // Used size of objects in bytes reported by the embedder. Updated via
    // TraceSummary at the end of tracing and incrementally when the GC is not
    // in progress.
    std::atomic<size_t> used_size{0};
    // Totally bytes allocated by the embedder. Monotonically
    // increasing value. Used to approximate allocation rate.
    size_t allocated_size = 0;
    // Limit for |allocated_size| in bytes to avoid checking for starting a GC
    // on each increment.
    size_t allocated_size_limit_for_check = 0;
  } remote_stats_;

  // Default descriptor only used when the embedder is using EmbedderHeapTracer.
  // The value is overriden by CppHeap with values that the embedder provided
  // upon initialization.
  WrapperDescriptor wrapper_descriptor_ = GetDefaultWrapperDescriptor();

  friend class EmbedderStackStateScope;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EMBEDDER_TRACING_H_
