// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_GC_TRACER_H_
#define V8_HEAP_GC_TRACER_H_

#include "src/base/compiler-specific.h"
#include "src/base/platform/platform.h"
#include "src/base/ring-buffer.h"
#include "src/common/globals.h"
#include "src/heap/heap.h"
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

#define TRACE_GC(tracer, scope_id)                             \
  GCTracer::Scope::ScopeId gc_tracer_scope_id(scope_id);       \
  GCTracer::Scope gc_tracer_scope(tracer, gc_tracer_scope_id); \
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),             \
               GCTracer::Scope::Name(gc_tracer_scope_id))

#define TRACE_BACKGROUND_GC(tracer, scope_id)                   \
  GCTracer::BackgroundScope background_scope(tracer, scope_id); \
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"),              \
               GCTracer::BackgroundScope::Name(scope_id))

// GCTracer collects and prints ONE line after each garbage collector
// invocation IFF --trace_gc is used.
class V8_EXPORT_PRIVATE GCTracer {
 public:
  struct IncrementalMarkingInfos {
    IncrementalMarkingInfos() : duration(0), longest_step(0), steps(0) {}

    void Update(double duration) {
      steps++;
      this->duration += duration;
      if (duration > longest_step) {
        longest_step = duration;
      }
    }

    void ResetCurrentCycle() {
      duration = 0;
      longest_step = 0;
      steps = 0;
    }

    double duration;
    double longest_step;
    int steps;
  };

  class Scope {
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
      FIRST_GENERAL_BACKGROUND_SCOPE = BACKGROUND_ARRAY_BUFFER_FREE,
      LAST_GENERAL_BACKGROUND_SCOPE = BACKGROUND_UNMAPPER,
      FIRST_MC_BACKGROUND_SCOPE = MC_BACKGROUND_EVACUATE_COPY,
      LAST_MC_BACKGROUND_SCOPE = MC_BACKGROUND_SWEEPING,
      FIRST_TOP_MC_SCOPE = MC_CLEAR,
      LAST_TOP_MC_SCOPE = MC_SWEEP,
      FIRST_MINOR_GC_BACKGROUND_SCOPE = MINOR_MC_BACKGROUND_EVACUATE_COPY,
      LAST_MINOR_GC_BACKGROUND_SCOPE = SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL
    };

    Scope(GCTracer* tracer, ScopeId scope);
    ~Scope();
    static const char* Name(ScopeId id);

   private:
    GCTracer* tracer_;
    ScopeId scope_;
    double start_time_;
    RuntimeCallTimer timer_;
    RuntimeCallStats* runtime_stats_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Scope);
  };

  class V8_EXPORT_PRIVATE BackgroundScope {
   public:
    enum ScopeId {
#define DEFINE_SCOPE(scope) scope,
      TRACER_BACKGROUND_SCOPES(DEFINE_SCOPE)
#undef DEFINE_SCOPE
          NUMBER_OF_SCOPES,
      FIRST_GENERAL_BACKGROUND_SCOPE = BACKGROUND_ARRAY_BUFFER_FREE,
      LAST_GENERAL_BACKGROUND_SCOPE = BACKGROUND_UNMAPPER,
      FIRST_MC_BACKGROUND_SCOPE = MC_BACKGROUND_EVACUATE_COPY,
      LAST_MC_BACKGROUND_SCOPE = MC_BACKGROUND_SWEEPING,
      FIRST_MINOR_GC_BACKGROUND_SCOPE = MINOR_MC_BACKGROUND_EVACUATE_COPY,
      LAST_MINOR_GC_BACKGROUND_SCOPE = SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL
    };
    BackgroundScope(GCTracer* tracer, ScopeId scope);
    ~BackgroundScope();

    static const char* Name(ScopeId id);

   private:
    GCTracer* tracer_;
    ScopeId scope_;
    double start_time_;
    RuntimeCallTimer timer_;
    RuntimeCallCounter counter_;
    bool runtime_stats_enabled_;
    DISALLOW_COPY_AND_ASSIGN(BackgroundScope);
  };

  class Event {
   public:
    enum Type {
      SCAVENGER = 0,
      MARK_COMPACTOR = 1,
      INCREMENTAL_MARK_COMPACTOR = 2,
      MINOR_MARK_COMPACTOR = 3,
      START = 4
    };

    Event(Type type, GarbageCollectionReason gc_reason,
          const char* collector_reason);

    // Returns a string describing the event type.
    const char* TypeName(bool short_name) const;

    // Type of event
    Type type;

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

    // Duration of incremental marking steps for INCREMENTAL_MARK_COMPACTOR.
    double incremental_marking_duration;

    // Amounts of time spent in different scopes during GC.
    double scopes[Scope::NUMBER_OF_SCOPES];

    // Holds details for incremental marking scopes.
    IncrementalMarkingInfos
        incremental_marking_scopes[Scope::NUMBER_OF_INCREMENTAL_SCOPES];
  };

  static const int kThroughputTimeFrameMs = 5000;
  static constexpr double kConservativeSpeedInBytesPerMillisecond = 128 * KB;

  static double CombineSpeedsInBytesPerMillisecond(double default_speed,
                                                   double optional_speed);

  static RuntimeCallCounterId RCSCounterFromScope(Scope::ScopeId id);

  explicit GCTracer(Heap* heap);

  // Start collecting data.
  void Start(GarbageCollector collector, GarbageCollectionReason gc_reason,
             const char* collector_reason);

  // Stop collecting data and print results.
  void Stop(GarbageCollector collector);

  void NotifyYoungGenerationHandling(
      YoungGenerationHandling young_generation_handling);

  // Sample and accumulate bytes allocated since the last GC.
  void SampleAllocation(double current_ms, size_t new_space_counter_bytes,
                        size_t old_generation_counter_bytes,
                        size_t embedder_counter_bytes);

  // Log the accumulated new space allocation bytes.
  void AddAllocation(double current_ms);

  void AddContextDisposalTime(double time);

  void AddCompactionEvent(double duration, size_t live_bytes_compacted);

  void AddSurvivalRatio(double survival_ratio);

  // Log an incremental marking step.
  void AddIncrementalMarkingStep(double duration, size_t bytes);

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
  // last time_ms milliseconds. Reported through v8::EmbedderHeapTracer.
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
  // kThroughputTimeFrameMs seconds. Reported through v8::EmbedderHeapTracer.
  // Returns 0 if no allocation events have been recorded.
  double CurrentEmbedderAllocationThroughputInBytesPerMillisecond() const;

  // Computes the context disposal rate in milliseconds. It takes the time
  // frame of the first recorded context disposal to the current time and
  // divides it by the number of recorded events.
  // Returns 0 if no events have been recorded.
  double ContextDisposalRateInMilliseconds() const;

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

  V8_INLINE void AddScopeSample(Scope::ScopeId scope, double duration) {
    DCHECK(scope < Scope::NUMBER_OF_SCOPES);
    if (scope >= Scope::FIRST_INCREMENTAL_SCOPE &&
        scope <= Scope::LAST_INCREMENTAL_SCOPE) {
      incremental_marking_scopes_[scope - Scope::FIRST_INCREMENTAL_SCOPE]
          .Update(duration);
    } else {
      current_.scopes[scope] += duration;
    }
  }

  void AddBackgroundScopeSample(BackgroundScope::ScopeId scope, double duration,
                                RuntimeCallCounter* runtime_call_counter);

  void RecordGCPhasesHistograms(TimedHistogram* gc_timer);

  void RecordEmbedderSpeed(size_t bytes, double duration);

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
  FRIEND_TEST(GCTracerTest, RecordGCSumHistograms);
  FRIEND_TEST(GCTracerTest, RecordMarkCompactHistograms);
  FRIEND_TEST(GCTracerTest, RecordScavengerHistograms);

  struct BackgroundCounter {
    double total_duration_ms;
    RuntimeCallCounter runtime_call_counter;
  };

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

  // Overall time spent in mark compact within a given GC cycle. Exact
  // accounting of events within a GC is not necessary which is why the
  // recording takes place at the end of the atomic pause.
  void RecordGCSumCounters(double atomic_pause_duration);

  // Print one detailed trace line in name=value format.
  // TODO(ernstm): Move to Heap.
  void PrintNVP() const;

  // Print one trace line.
  // TODO(ernstm): Move to Heap.
  void Print() const;

  // Prints a line and also adds it to the heap's ring buffer so that
  // it can be included in later crash dumps.
  void PRINTF_FORMAT(2, 3) Output(const char* format, ...) const;

  double TotalExternalTime() const {
    return current_.scopes[Scope::HEAP_EXTERNAL_WEAK_GLOBAL_HANDLES] +
           current_.scopes[Scope::HEAP_EXTERNAL_EPILOGUE] +
           current_.scopes[Scope::HEAP_EXTERNAL_PROLOGUE] +
           current_.scopes[Scope::MC_INCREMENTAL_EXTERNAL_EPILOGUE] +
           current_.scopes[Scope::MC_INCREMENTAL_EXTERNAL_PROLOGUE];
  }

  void FetchBackgroundCounters(int first_global_scope, int last_global_scope,
                               int first_background_scope,
                               int last_background_scope);
  void FetchBackgroundMinorGCCounters();
  void FetchBackgroundMarkCompactCounters();
  void FetchBackgroundGeneralCounters();

  // Pointer to the heap that owns this tracer.
  Heap* heap_;

  // Current tracer event. Populated during Start/Stop cycle. Valid after Stop()
  // has returned.
  Event current_;

  // Previous tracer event.
  Event previous_;

  // Size of incremental marking steps (in bytes) accumulated since the end of
  // the last mark compact GC.
  size_t incremental_marking_bytes_;

  // Duration of incremental marking steps since the end of the last mark-
  // compact event.
  double incremental_marking_duration_;

  double incremental_marking_start_time_;

  double recorded_incremental_marking_speed_;

  double recorded_embedder_speed_ = 0.0;

  // Incremental scopes carry more information than just the duration. The infos
  // here are merged back upon starting/stopping the GC tracer.
  IncrementalMarkingInfos
      incremental_marking_scopes_[Scope::NUMBER_OF_INCREMENTAL_SCOPES];


  // Timestamp and allocation counter at the last sampled allocation event.
  double allocation_time_ms_;
  size_t new_space_allocation_counter_bytes_;
  size_t old_generation_allocation_counter_bytes_;
  size_t embedder_allocation_counter_bytes_;

  // Accumulated duration and allocated bytes since the last GC.
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
  base::RingBuffer<double> recorded_context_disposal_times_;
  base::RingBuffer<double> recorded_survival_ratios_;

  base::Mutex background_counter_mutex_;
  BackgroundCounter background_counter_[BackgroundScope::NUMBER_OF_SCOPES];

  DISALLOW_COPY_AND_ASSIGN(GCTracer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_GC_TRACER_H_
