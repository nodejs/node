// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_METRICS_H_
#define V8_METRICS_H_

#include "v8.h"  // NOLINT(build/include_directory)

namespace v8 {
namespace metrics {

struct GarbageCollectionPhases {
  int64_t compact_wall_clock_duration_in_us = -1;
  int64_t mark_wall_clock_duration_in_us = -1;
  int64_t sweep_wall_clock_duration_in_us = -1;
  int64_t weak_wall_clock_duration_in_us = -1;
};

struct GarbageCollectionSizes {
  int64_t bytes_before = -1;
  int64_t bytes_after = -1;
  int64_t bytes_freed = -1;
};

struct GarbageCollectionFullCycle {
  GarbageCollectionPhases total;
  GarbageCollectionPhases total_cpp;
  GarbageCollectionPhases main_thread;
  GarbageCollectionPhases main_thread_cpp;
  GarbageCollectionPhases main_thread_atomic;
  GarbageCollectionPhases main_thread_atomic_cpp;
  GarbageCollectionPhases main_thread_incremental;
  GarbageCollectionPhases main_thread_incremental_cpp;
  GarbageCollectionSizes objects;
  GarbageCollectionSizes objects_cpp;
  GarbageCollectionSizes memory;
  GarbageCollectionSizes memory_cpp;
  double collection_rate_in_percent;
  double collection_rate_cpp_in_percent;
  double efficiency_in_bytes_per_us;
  double efficiency_cpp_in_bytes_per_us;
  double main_thread_efficiency_in_bytes_per_us;
  double main_thread_efficiency_cpp_in_bytes_per_us;
};

struct GarbageCollectionFullMainThreadIncrementalMark {
  int64_t wall_clock_duration_in_us = -1;
  int64_t cpp_wall_clock_duration_in_us = -1;
};

struct GarbageCollectionFullMainThreadIncrementalSweep {
  int64_t wall_clock_duration_in_us = -1;
  int64_t cpp_wall_clock_duration_in_us = -1;
};

struct GarbageCollectionYoungCycle {
  int64_t total_wall_clock_duration_in_us = -1;
  int64_t main_thread_wall_clock_duration_in_us = -1;
  double collection_rate_in_percent;
  double efficiency_in_bytes_per_us;
  double main_thread_efficiency_in_bytes_per_us;
};

struct WasmModuleDecoded {
  bool async = false;
  bool streamed = false;
  bool success = false;
  size_t module_size_in_bytes = 0;
  size_t function_count = 0;
  int64_t wall_clock_duration_in_us = -1;
};

struct WasmModuleCompiled {
  bool async = false;
  bool streamed = false;
  bool cached = false;
  bool deserialized = false;
  bool lazy = false;
  bool success = false;
  size_t code_size_in_bytes = 0;
  size_t liftoff_bailout_count = 0;
  int64_t wall_clock_duration_in_us = -1;
};

struct WasmModuleInstantiated {
  bool async = false;
  bool success = false;
  size_t imported_function_count = 0;
  int64_t wall_clock_duration_in_us = -1;
};

struct WasmModuleTieredUp {
  bool lazy = false;
  size_t code_size_in_bytes = 0;
  int64_t wall_clock_duration_in_us = -1;
};

struct WasmModulesPerIsolate {
  size_t count = 0;
};

#define V8_MAIN_THREAD_METRICS_EVENTS(V)             \
  V(GarbageCollectionFullCycle)                      \
  V(GarbageCollectionFullMainThreadIncrementalMark)  \
  V(GarbageCollectionFullMainThreadIncrementalSweep) \
  V(GarbageCollectionYoungCycle)                     \
  V(WasmModuleDecoded)                               \
  V(WasmModuleCompiled)                              \
  V(WasmModuleInstantiated)                          \
  V(WasmModuleTieredUp)

#define V8_THREAD_SAFE_METRICS_EVENTS(V) V(WasmModulesPerIsolate)

/**
 * This class serves as a base class for recording event-based metrics in V8.
 * There a two kinds of metrics, those which are expected to be thread-safe and
 * whose implementation is required to fulfill this requirement and those whose
 * implementation does not have that requirement and only needs to be
 * executable on the main thread. If such an event is triggered from a
 * background thread, it will be delayed and executed by the foreground task
 * runner.
 *
 * The thread-safe events are listed in the V8_THREAD_SAFE_METRICS_EVENTS
 * macro above while the main thread event are listed in
 * V8_MAIN_THREAD_METRICS_EVENTS above. For the former, a virtual method
 * AddMainThreadEvent(const E& event, v8::Context::Token token) will be
 * generated and for the latter AddThreadSafeEvent(const E& event).
 *
 * Thread-safe events are not allowed to access the context and therefore do
 * not carry a context ID with them. These IDs can be generated using
 * Recorder::GetContextId() and the ID will be valid throughout the lifetime
 * of the isolate. It is not guaranteed that the ID will still resolve to
 * a valid context using Recorder::GetContext() at the time the metric is
 * recorded. In this case, an empty handle will be returned.
 *
 * The embedder is expected to call v8::Isolate::SetMetricsRecorder()
 * providing its implementation and have the virtual methods overwritten
 * for the events it cares about.
 */
class V8_EXPORT Recorder {
 public:
  // A unique identifier for a context in this Isolate.
  // It is guaranteed to not be reused throughout the lifetime of the Isolate.
  class ContextId {
   public:
    ContextId() : id_(kEmptyId) {}

    bool IsEmpty() const { return id_ == kEmptyId; }
    static const ContextId Empty() { return ContextId{kEmptyId}; }

    bool operator==(const ContextId& other) const { return id_ == other.id_; }
    bool operator!=(const ContextId& other) const { return id_ != other.id_; }

   private:
    friend class ::v8::Context;
    friend class ::v8::internal::Isolate;

    explicit ContextId(uintptr_t id) : id_(id) {}

    static constexpr uintptr_t kEmptyId = 0;
    uintptr_t id_;
  };

  virtual ~Recorder() = default;

#define ADD_MAIN_THREAD_EVENT(E) \
  virtual void AddMainThreadEvent(const E& event, ContextId context_id) {}
  V8_MAIN_THREAD_METRICS_EVENTS(ADD_MAIN_THREAD_EVENT)
#undef ADD_MAIN_THREAD_EVENT

#define ADD_THREAD_SAFE_EVENT(E) \
  virtual void AddThreadSafeEvent(const E& event) {}
  V8_THREAD_SAFE_METRICS_EVENTS(ADD_THREAD_SAFE_EVENT)
#undef ADD_THREAD_SAFE_EVENT

  virtual void NotifyIsolateDisposal() {}

  // Return the context with the given id or an empty handle if the context
  // was already garbage collected.
  static MaybeLocal<Context> GetContext(Isolate* isolate, ContextId id);
  // Return the unique id corresponding to the given context.
  static ContextId GetContextId(Local<Context> context);
};

}  // namespace metrics
}  // namespace v8

#endif  // V8_METRICS_H_
