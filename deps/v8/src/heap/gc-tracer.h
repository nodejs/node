// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_GC_TRACER_H_
#define V8_HEAP_GC_TRACER_H_

#include "include/v8-metrics.h"
#include "src/base/compiler-specific.h"
#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/base/ring-buffer.h"
#include "src/common/globals.h"
#include "src/init/heap-symbols.h"
#include "src/logging/counters.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {
namespace internal {

using BytesAndDuration = std::pair<uint64_t, double>;

inline BytesAndDuration MakeBytesAndDuration(uint64_t bytes, double duration) {
  return std::make_pair(bytes, duration);
}

enum ScavengeSpeedMode { kForAllObjects, kForSurvivedObjects };

#define TRACE_GC_CATEGORIES \
  "devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.gc")

// Sweeping for full GC may be interleaved with sweeping for minor
// gc. The below scopes should use TRACE_GC_EPOCH to associate them
// with the right cycle.
#define TRACE_GC(tracer, scope_id)                                    \
  DCHECK_NE(GCTracer::Scope::MC_SWEEP, scope_id);                     \
  DCHECK_NE(GCTracer::Scope::MC_BACKGROUND_SWEEPING, scope_id);       \
  GCTracer::Scope UNIQUE_IDENTIFIER(gc_tracer_scope)(                 \
      tracer, GCTracer::Scope::ScopeId(scope_id), ThreadKind::kMain); \
  TRACE_EVENT0(TRACE_GC_CATEGORIES,                                   \
               GCTracer::Scope::Name(GCTracer::Scope::ScopeId(scope_id)))

#define TRACE_GC1(tracer, scope_id, thread_kind)                \
  GCTracer::Scope UNIQUE_IDENTIFIER(gc_tracer_scope)(           \
      tracer, GCTracer::Scope::ScopeId(scope_id), thread_kind); \
  TRACE_EVENT0(TRACE_GC_CATEGORIES,                             \
               GCTracer::Scope::Name(GCTracer::Scope::ScopeId(scope_id)))

#define TRACE_GC_EPOCH(tracer, scope_id, thread_kind)                     \
  GCTracer::Scope UNIQUE_IDENTIFIER(gc_tracer_scope)(                     \
      tracer, GCTracer::Scope::ScopeId(scope_id), thread_kind);           \
  TRACE_EVENT1(TRACE_GC_CATEGORIES,                                       \
               GCTracer::Scope::Name(GCTracer::Scope::ScopeId(scope_id)), \
               "epoch", tracer->CurrentEpoch(scope_id))

using CollectionEpoch = uint32_t;

// GCTracer collects and prints ONE line after each garbage collector
// invocation IFF --trace_gc is used.
class V8_EXPORT_PRIVATE GCTracer {
 public:
  GCTracer(const GCTracer&) = delete;
  GCTracer& operator=(const GCTracer&) = delete;

  struct IncrementalMarkingInfos {
    V8_INLINE IncrementalMarkingInfos();
    V8_INLINE void Update(double delta);
    V8_INLINE void ResetCurrentCycle();

    double duration;      // in ms
    double longest_step;  // in ms
    int steps;
  };

  class V8_EXPORT_PRIVATE V8_NODISCARD Scope {
   public:
    enum ScopeId {
#define DEFINE_SCOPE(scope) scope,
      TRACER_SCOPES(DEFINE_SCOPE) TRACER_BACKGROUND_SCOPES(DEFINE_SCOPE)
#undef DEFINE_SCOPE
          NUMBER_OF_SCOPES,

      FIRST_INCREMENTAL_SCOPE = MC_INCREMENTAL,
      LAST_INCREMENTAL_SCOPE = MC_INCREMENTAL_SWEEPING,
      FIRST_SCOPE = MC_INCREMENTAL,
      NUMBER_OF_INCREMENTAL_SCOPES =
          LAST_INCREMENTAL_SCOPE - FIRST_INCREMENTAL_SCOPE + 1,
      FIRST_GENERAL_BACKGROUND_SCOPE = BACKGROUND_YOUNG_ARRAY_BUFFER_SWEEP,
      LAST_GENERAL_BACKGROUND_SCOPE = BACKGROUND_SAFEPOINT,
      FIRST_MC_BACKGROUND_SCOPE = MC_BACKGROUND_EVACUATE_COPY,
      LAST_MC_BACKGROUND_SCOPE = MC_BACKGROUND_SWEEPING,
      FIRST_TOP_MC_SCOPE = MC_CLEAR,
      LAST_TOP_MC_SCOPE = MC_SWEEP,
      FIRST_MINOR_GC_BACKGROUND_SCOPE = MINOR_MC_BACKGROUND_EVACUATE_COPY,
      LAST_MINOR_GC_BACKGROUND_SCOPE = SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL,
      FIRST_BACKGROUND_SCOPE = FIRST_GENERAL_BACKGROUND_SCOPE,
      LAST_BACKGROUND_SCOPE = LAST_MINOR_GC_BACKGROUND_SCOPE
    };

    V8_INLINE Scope(GCTracer* tracer, ScopeId scope, ThreadKind thread_kind);
    V8_INLINE ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    static constexpr const char* Name(ScopeId id);
    static constexpr bool NeedsYoungEpoch(ScopeId id);
    static constexpr int IncrementalOffset(ScopeId id);

   private:
    GCTracer* const tracer_;
    const ScopeId scope_;
    const ThreadKind thread_kind_;
    const double start_time_;
#ifdef V8_RUNTIME_CALL_STATS
    RuntimeCallTimer timer_;
    RuntimeCallStats* runtime_stats_ = nullptr;
    base::Optional<WorkerThreadRuntimeCallStatsScope> runtime_call_stats_scope_;
#endif  // defined(V8_RUNTIME_CALL_STATS)
  };

  class Event {
   public:
    enum Type {
      SCAVENGER = 0,
      MARK_COMPACTOR = 1,
      INCREMENTAL_MARK_COMPACTOR = 2,
      MINOR_MARK_COMPACTOR = 3,
      INCREMENTAL_MINOR_MARK_COMPACTOR = 4,
      START = 5,
    };

    // Returns true if the event corresponds to a young generation GC.
    V8_INLINE static constexpr bool IsYoungGenerationEvent(Type type);

    // The state diagram for a GC cycle:
    //   (NOT_RUNNING) -----(StartCycle)----->
    //   MARKING       --(StartAtomicPause)-->
    //   ATOMIC        ---(StopAtomicPause)-->
    //   SWEEPING      ------(StopCycle)-----> NOT_RUNNING
    enum class State { NOT_RUNNING, MARKING, ATOMIC, SWEEPING };

    Event(Type type, State state, GarbageCollectionReason gc_reason,
          const char* collector_reason);

    // Returns a string describing the event type.
    const char* TypeName(bool short_name) const;

    // Type of the event.
    Type type;

    // State of the cycle corresponding to the event.
    State state;

    GarbageCollectionReason gc_reason;
    const char* collector_reason;

    // Timestamp set in the constructor.
    double start_time;

    // Timestamp set in the destructor.
    double end_time;

    // Memory reduction flag set.
    bool reduce_memory;

    // Size of objects in heap set in constructor.
    size_t start_object_size;

    // Size of objects in heap set in destructor.
    size_t end_object_size;

    // Size of memory allocated from OS set in constructor.
    size_t start_memory_size;

    // Size of memory allocated from OS set in destructor.
    size_t end_memory_size;

    // Total amount of space either wasted or contained in one of free lists
    // before the current GC.
    size_t start_holes_size;

    // Total amount of space either wasted or contained in one of free lists
    // after the current GC.
    size_t end_holes_size;

    // Size of young objects in constructor.
    size_t young_object_size;

    // Size of survived young objects in destructor.
    size_t survived_young_object_size;

    // Bytes marked incrementally for INCREMENTAL_MARK_COMPACTOR
    size_t incremental_marking_bytes;

    // Duration (in ms) of incremental marking steps for
    // INCREMENTAL_MARK_COMPACTOR.
    double incremental_marking_duration;

    // Amounts of time (in ms) spent in different scopes during GC.
    double scopes[Scope::NUMBER_OF_SCOPES];

    // Holds details for incremental marking scopes.
    IncrementalMarkingInfos
        incremental_scopes[Scope::NUMBER_OF_INCREMENTAL_SCOPES];
  };

  class RecordGCPhasesInfo {
   public:
    RecordGCPhasesInfo(Heap* heap, GarbageCollector collector);

    enum class Mode { None, Scavenger, Finalize };

    Mode mode() const { return mode_; }
    const char* trace_event_name() const { return trace_event_name_; }

    // The timer used for a given GC type:
    // - GCScavenger: young generation GC
    // - GCCompactor: full GC
    // - GCFinalizeMC: finalization of incremental full GC
    // - GCFinalizeMCReduceMemory: finalization of incremental full GC with
    //   memory reduction.
    TimedHistogram* type_timer() const { return type_timer_; }
    TimedHistogram* type_priority_timer() const { return type_priority_timer_; }

   private:
    Mode mode_;
    const char* trace_event_name_;
    TimedHistogram* type_timer_;
    TimedHistogram* type_priority_timer_;
  };

  static const int kThroughputTimeFrameMs = 5000;
  static constexpr double kConservativeSpeedInBytesPerMillisecond = 128 * KB;

  static double CombineSpeedsInBytesPerMillisecond(double default_speed,
                                                   double optional_speed);

#ifdef V8_RUNTIME_CALL_STATS
  V8_INLINE static RuntimeCallCounterId RCSCounterFromScope(Scope::ScopeId id);
#endif  // defined(V8_RUNTIME_CALL_STATS)

  explicit GCTracer(Heap* heap);

  V8_INLINE CollectionEpoch CurrentEpoch(Scope::ScopeId id) const;

  // Start and stop an observable pause.
  void StartObservablePause();
  void StopObservablePause();

  // Update the current event if it precedes the start of the observable pause.
  void UpdateCurrentEvent(GarbageCollectionReason gc_reason,
                          const char* collector_reason);

  void UpdateStatistics(GarbageCollector collector);
  void FinalizeCurrentEvent();

  enum class MarkingType { kAtomic, kIncremental };

  // Start and stop a GC cycle (collecting data and reporting results).
  void StartCycle(GarbageCollector collector, GarbageCollectionReason gc_reason,
                  const char* collector_reason, MarkingType marking);
  void StopYoungCycleIfNeeded();
  void StopFullCycleIfNeeded();

  // Start and stop a cycle's atomic pause.
  void StartAtomicPause();
  void StopAtomicPause();

  void StartInSafepoint();
  void StopInSafepoint();

  void NotifyFullSweepingCompleted();
  void NotifyYoungSweepingCompleted();

  void NotifyFullCppGCCompleted();
  void NotifyYoungCppGCRunning();
  void NotifyYoungCppGCCompleted();

#ifdef DEBUG
  V8_INLINE bool IsInObservablePause() const;
  V8_INLINE bool IsInAtomicPause() const;

  // Checks if the current event is consistent with a collector.
  V8_INLINE bool IsConsistentWithCollector(GarbageCollector collector) const;

  // Checks if the current event corresponds to a full GC cycle whose sweeping
  // has not finalized yet.
  V8_INLINE bool IsSweepingInProgress() const;
#endif

  // Sample and accumulate bytes allocated since the last GC.
  void SampleAllocation(double current_ms, size_t new_space_counter_bytes,
                        size_t old_generation_counter_bytes,
                        size_t embedder_counter_bytes);

  // Log the accumulated new space allocation bytes.
  void AddAllocation(double current_ms);

  void AddCompactionEvent(double duration, size_t live_bytes_compacted);

  void AddSurvivalRatio(double survival_ratio);

  // Log an incremental marking step.
  void AddIncrementalMarkingStep(double duration, size_t bytes);

  // Log an incremental marking step.
  void AddIncrementalSweepingStep(double duration);

  // Compute the average incremental marking speed in bytes/millisecond.
  // Returns a conservative value if no events have been recorded.
  double IncrementalMarkingSpeedInBytesPerMillisecond() const;

  // Compute the average embedder speed in bytes/millisecond.
  // Returns a conservative value if no events have been recorded.
  double EmbedderSpeedInBytesPerMillisecond() const;

  // Compute the average scavenge speed in bytes/millisecond.
  // Returns 0 if no events have been recorded.
  double ScavengeSpeedInBytesPerMillisecond(
      ScavengeSpeedMode mode = kForAllObjects) const;

  // Compute the average compaction speed in bytes/millisecond.
  // Returns 0 if not enough events have been recorded.
  double CompactionSpeedInBytesPerMillisecond() const;

  // Compute the average mark-sweep speed in bytes/millisecond.
  // Returns 0 if no events have been recorded.
  double MarkCompactSpeedInBytesPerMillisecond() const;

  // Compute the average incremental mark-sweep finalize speed in
  // bytes/millisecond.
  // Returns 0 if no events have been recorded.
  double FinalIncrementalMarkCompactSpeedInBytesPerMillisecond() const;

  // Compute the overall mark compact speed including incremental steps
  // and the final mark-compact step.
  double CombinedMarkCompactSpeedInBytesPerMillisecond();

  // Allocation throughput in the new space in bytes/millisecond.
  // Returns 0 if no allocation events have been recorded.
  double NewSpaceAllocationThroughputInBytesPerMillisecond(
      double time_ms = 0) const;

  // Allocation throughput in the old generation in bytes/millisecond in the
  // last time_ms milliseconds.
  // Returns 0 if no allocation events have been recorded.
  double OldGenerationAllocationThroughputInBytesPerMillisecond(
      double time_ms = 0) const;

  // Allocation throughput in the embedder in bytes/millisecond in the
  // last time_ms milliseconds.
  // Returns 0 if no allocation events have been recorded.
  double EmbedderAllocationThroughputInBytesPerMillisecond(
      double time_ms = 0) const;

  // Allocation throughput in heap in bytes/millisecond in the last time_ms
  // milliseconds.
  // Returns 0 if no allocation events have been recorded.
  double AllocationThroughputInBytesPerMillisecond(double time_ms) const;

  // Allocation throughput in heap in bytes/milliseconds in the last
  // kThroughputTimeFrameMs seconds.
  // Returns 0 if no allocation events have been recorded.
  double CurrentAllocationThroughputInBytesPerMillisecond() const;

  // Allocation throughput in old generation in bytes/milliseconds in the last
  // kThroughputTimeFrameMs seconds.
  // Returns 0 if no allocation events have been recorded.
  double CurrentOldGenerationAllocationThroughputInBytesPerMillisecond() const;

  // Allocation throughput in the embedder in bytes/milliseconds in the last
  // kThroughputTimeFrameMs seconds.
  // Returns 0 if no allocation events have been recorded.
  double CurrentEmbedderAllocationThroughputInBytesPerMillisecond() const;

  // Computes the average survival ratio based on the last recorded survival
  // events.
  // Returns 0 if no events have been recorded.
  double AverageSurvivalRatio() const;

  // Returns true if at least one survival event was recorded.
  bool SurvivalEventsRecorded() const;

  // Discard all recorded survival events.
  void ResetSurvivalEvents();

  void NotifyIncrementalMarkingStart();

  // Returns average mutator utilization with respect to mark-compact
  // garbage collections. This ignores scavenger.
  double AverageMarkCompactMutatorUtilization() const;
  double CurrentMarkCompactMutatorUtilization() const;

  V8_INLINE void AddScopeSample(Scope::ScopeId id, double duration);

  void RecordGCPhasesHistograms(RecordGCPhasesInfo::Mode mode);

  void RecordEmbedderSpeed(size_t bytes, double duration);

  // Returns the average time between scheduling and invocation of an
  // incremental marking task.
  double AverageTimeToIncrementalMarkingTask() const;
  void RecordTimeToIncrementalMarkingTask(double time_to_task);

#ifdef V8_RUNTIME_CALL_STATS
  V8_INLINE WorkerThreadRuntimeCallStats* worker_thread_runtime_call_stats();
#endif  // defined(V8_RUNTIME_CALL_STATS)

  bool IsCurrentGCDueToAllocationFailure() const {
    return current_.gc_reason == GarbageCollectionReason::kAllocationFailure;
  }

  GarbageCollector GetCurrentCollector() const;

 private:
  FRIEND_TEST(GCTracer, AverageSpeed);
  FRIEND_TEST(GCTracerTest, AllocationThroughput);
  FRIEND_TEST(GCTracerTest, BackgroundScavengerScope);
  FRIEND_TEST(GCTracerTest, BackgroundMinorMCScope);
  FRIEND_TEST(GCTracerTest, BackgroundMajorMCScope);
  FRIEND_TEST(GCTracerTest, EmbedderAllocationThroughput);
  FRIEND_TEST(GCTracerTest, MultithreadedBackgroundScope);
  FRIEND_TEST(GCTracerTest, NewSpaceAllocationThroughput);
  FRIEND_TEST(GCTracerTest, PerGenerationAllocationThroughput);
  FRIEND_TEST(GCTracerTest, PerGenerationAllocationThroughputWithProvidedTime);
  FRIEND_TEST(GCTracerTest, RegularScope);
  FRIEND_TEST(GCTracerTest, IncrementalMarkingDetails);
  FRIEND_TEST(GCTracerTest, IncrementalScope);
  FRIEND_TEST(GCTracerTest, IncrementalMarkingSpeed);
  FRIEND_TEST(GCTracerTest, MutatorUtilization);
  FRIEND_TEST(GCTracerTest, RecordMarkCompactHistograms);
  FRIEND_TEST(GCTracerTest, RecordScavengerHistograms);

  struct BackgroundCounter {
    double total_duration_ms;
  };

  void StopCycle(GarbageCollector collector);

  // Statistics for incremental and background scopes are kept out of the
  // current event and only copied there by FinalizeCurrentEvent, at StopCycle.
  // This method can be used to access scopes correctly, before this happens.
  // Note: when accessing a background scope via this method, the caller is
  // responsible for avoiding data races, e.g., by acquiring
  // background_counter_mutex_.
  V8_INLINE constexpr double current_scope(Scope::ScopeId id) const;

  V8_INLINE constexpr const IncrementalMarkingInfos& incremental_scope(
      Scope::ScopeId id) const;

  // Returns the average speed of the events in the buffer.
  // If the buffer is empty, the result is 0.
  // Otherwise, the result is between 1 byte/ms and 1 GB/ms.
  static double AverageSpeed(const base::RingBuffer<BytesAndDuration>& buffer);
  static double AverageSpeed(const base::RingBuffer<BytesAndDuration>& buffer,
                             const BytesAndDuration& initial, double time_ms);

  void ResetForTesting();
  void ResetIncrementalMarkingCounters();
  void RecordIncrementalMarkingSpeed(size_t bytes, double duration);
  void RecordMutatorUtilization(double mark_compactor_end_time,
                                double mark_compactor_duration);

  // Update counters for an entire full GC cycle. Exact accounting of events
  // within a GC is not necessary which is why the recording takes place at the
  // end of the atomic pause.
  void RecordGCSumCounters();

  V8_INLINE double MonotonicallyIncreasingTimeInMs();

  // Print one detailed trace line in name=value format.
  // TODO(ernstm): Move to Heap.
  void PrintNVP() const;

  // Print one trace line.
  // TODO(ernstm): Move to Heap.
  void Print() const;

  // Prints a line and also adds it to the heap's ring buffer so that
  // it can be included in later crash dumps.
  void PRINTF_FORMAT(2, 3) Output(const char* format, ...) const;

  void FetchBackgroundCounters(int first_scope, int last_scope);
  void FetchBackgroundMinorGCCounters();
  void FetchBackgroundMarkCompactCounters();
  void FetchBackgroundGeneralCounters();

  void ReportFullCycleToRecorder();
  void ReportIncrementalMarkingStepToRecorder(double v8_duration);
  void ReportIncrementalSweepingStepToRecorder(double v8_duration);
  void ReportYoungCycleToRecorder();

  // Pointer to the heap that owns this tracer.
  Heap* heap_;

  // Current tracer event. Populated during Start/Stop cycle. Valid after Stop()
  // has returned.
  Event current_;

  // Previous tracer event.
  Event previous_;

  // The starting time of the observable pause or 0.0 if we're not inside it.
  double start_of_observable_pause_ = 0.0;

  // We need two epochs, since there can be scavenges during incremental
  // marking.
  CollectionEpoch epoch_young_ = 0;
  CollectionEpoch epoch_full_ = 0;

  // Size of incremental marking steps (in bytes) accumulated since the end of
  // the last mark compact GC.
  size_t incremental_marking_bytes_;

  // Duration (in ms) of incremental marking steps since the end of the last
  // mark-compact event.
  double incremental_marking_duration_;

  double incremental_marking_start_time_;

  double recorded_incremental_marking_speed_;

  double average_time_to_incremental_marking_task_ = 0.0;

  double recorded_embedder_speed_ = 0.0;

  // Incremental scopes carry more information than just the duration. The infos
  // here are merged back upon starting/stopping the GC tracer.
  IncrementalMarkingInfos
      incremental_scopes_[Scope::NUMBER_OF_INCREMENTAL_SCOPES];

  // Timestamp and allocation counter at the last sampled allocation event.
  double allocation_time_ms_;
  size_t new_space_allocation_counter_bytes_;
  size_t old_generation_allocation_counter_bytes_;
  size_t embedder_allocation_counter_bytes_;

  // Accumulated duration (in ms) and allocated bytes since the last GC.
  double allocation_duration_since_gc_;
  size_t new_space_allocation_in_bytes_since_gc_;
  size_t old_generation_allocation_in_bytes_since_gc_;
  size_t embedder_allocation_in_bytes_since_gc_;

  double combined_mark_compact_speed_cache_;

  // Counts how many tracers were started without stopping.
  int start_counter_;

  // Used for computing average mutator utilization.
  double average_mutator_duration_;
  double average_mark_compact_duration_;
  double current_mark_compact_mutator_utilization_;
  double previous_mark_compact_end_time_;

  base::RingBuffer<BytesAndDuration> recorded_minor_gcs_total_;
  base::RingBuffer<BytesAndDuration> recorded_minor_gcs_survived_;
  base::RingBuffer<BytesAndDuration> recorded_compactions_;
  base::RingBuffer<BytesAndDuration> recorded_incremental_mark_compacts_;
  base::RingBuffer<BytesAndDuration> recorded_mark_compacts_;
  base::RingBuffer<BytesAndDuration> recorded_new_generation_allocations_;
  base::RingBuffer<BytesAndDuration> recorded_old_generation_allocations_;
  base::RingBuffer<BytesAndDuration> recorded_embedder_generation_allocations_;
  base::RingBuffer<double> recorded_survival_ratios_;

  // A full GC cycle stops only when both v8 and cppgc (if available) GCs have
  // finished sweeping.
  bool notified_full_sweeping_completed_ = false;
  bool notified_full_cppgc_completed_ = false;

  bool notified_young_sweeping_completed_ = false;
  // Similar to full GCs, a young GC cycle stops only when both v8 and cppgc GCs
  // have finished sweeping.
  bool notified_young_cppgc_completed_ = false;
  // Keep track whether the young cppgc GC was scheduled (as opposed to full
  // cycles, for young cycles cppgc is not always scheduled).
  bool notified_young_cppgc_running_ = false;

  // When a full GC cycle is interrupted by a young generation GC cycle, the
  // |previous_| event is used as temporary storage for the |current_| event
  // that corresponded to the full GC cycle, and this field is set to true.
  bool young_gc_while_full_gc_ = false;

  v8::metrics::GarbageCollectionFullMainThreadBatchedIncrementalMark
      incremental_mark_batched_events_;
  v8::metrics::GarbageCollectionFullMainThreadBatchedIncrementalSweep
      incremental_sweep_batched_events_;

  mutable base::Mutex background_counter_mutex_;
  BackgroundCounter background_counter_[Scope::NUMBER_OF_SCOPES];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_GC_TRACER_H_
