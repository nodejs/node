// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_GC_TRACER_H_
#define V8_HEAP_GC_TRACER_H_

#include "src/base/platform/platform.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// A simple ring buffer class with maximum size known at compile time.
// The class only implements the functionality required in GCTracer.
template <typename T, size_t MAX_SIZE>
class RingBuffer {
 public:
  class const_iterator {
   public:
    const_iterator() : index_(0), elements_(NULL) {}

    const_iterator(size_t index, const T* elements)
        : index_(index), elements_(elements) {}

    bool operator==(const const_iterator& rhs) const {
      return elements_ == rhs.elements_ && index_ == rhs.index_;
    }

    bool operator!=(const const_iterator& rhs) const {
      return elements_ != rhs.elements_ || index_ != rhs.index_;
    }

    operator const T*() const { return elements_ + index_; }

    const T* operator->() const { return elements_ + index_; }

    const T& operator*() const { return elements_[index_]; }

    const_iterator& operator++() {
      index_ = (index_ + 1) % (MAX_SIZE + 1);
      return *this;
    }

    const_iterator& operator--() {
      index_ = (index_ + MAX_SIZE) % (MAX_SIZE + 1);
      return *this;
    }

   private:
    size_t index_;
    const T* elements_;
  };

  RingBuffer() : begin_(0), end_(0) {}

  bool empty() const { return begin_ == end_; }
  size_t size() const {
    return (end_ - begin_ + MAX_SIZE + 1) % (MAX_SIZE + 1);
  }
  const_iterator begin() const { return const_iterator(begin_, elements_); }
  const_iterator end() const { return const_iterator(end_, elements_); }
  const_iterator back() const { return --end(); }
  void push_back(const T& element) {
    elements_[end_] = element;
    end_ = (end_ + 1) % (MAX_SIZE + 1);
    if (end_ == begin_) begin_ = (begin_ + 1) % (MAX_SIZE + 1);
  }
  void push_front(const T& element) {
    begin_ = (begin_ + MAX_SIZE) % (MAX_SIZE + 1);
    if (begin_ == end_) end_ = (end_ + MAX_SIZE) % (MAX_SIZE + 1);
    elements_[begin_] = element;
  }

  void reset() {
    begin_ = 0;
    end_ = 0;
  }

 private:
  T elements_[MAX_SIZE + 1];
  size_t begin_;
  size_t end_;

  DISALLOW_COPY_AND_ASSIGN(RingBuffer);
};


enum ScavengeSpeedMode { kForAllObjects, kForSurvivedObjects };


// GCTracer collects and prints ONE line after each garbage collector
// invocation IFF --trace_gc is used.
// TODO(ernstm): Unit tests.
class GCTracer {
 public:
  class Scope {
   public:
    enum ScopeId {
      EXTERNAL,
      MC_CLEAR,
      MC_CLEAR_CODE_FLUSH,
      MC_CLEAR_DEPENDENT_CODE,
      MC_CLEAR_GLOBAL_HANDLES,
      MC_CLEAR_MAPS,
      MC_CLEAR_SLOTS_BUFFER,
      MC_CLEAR_STORE_BUFFER,
      MC_CLEAR_STRING_TABLE,
      MC_CLEAR_WEAK_CELLS,
      MC_CLEAR_WEAK_COLLECTIONS,
      MC_CLEAR_WEAK_LISTS,
      MC_EVACUATE,
      MC_EVACUATE_CANDIDATES,
      MC_EVACUATE_CLEAN_UP,
      MC_EVACUATE_NEW_SPACE,
      MC_EVACUATE_UPDATE_POINTERS,
      MC_EVACUATE_UPDATE_POINTERS_BETWEEN_EVACUATED,
      MC_EVACUATE_UPDATE_POINTERS_TO_EVACUATED,
      MC_EVACUATE_UPDATE_POINTERS_TO_NEW,
      MC_EVACUATE_UPDATE_POINTERS_WEAK,
      MC_FINISH,
      MC_INCREMENTAL_FINALIZE,
      MC_MARK,
      MC_MARK_FINISH_INCREMENTAL,
      MC_MARK_PREPARE_CODE_FLUSH,
      MC_MARK_ROOTS,
      MC_MARK_WEAK_CLOSURE,
      MC_SWEEP,
      MC_SWEEP_CODE,
      MC_SWEEP_MAP,
      MC_SWEEP_OLD,
      SCAVENGER_CODE_FLUSH_CANDIDATES,
      SCAVENGER_OBJECT_GROUPS,
      SCAVENGER_OLD_TO_NEW_POINTERS,
      SCAVENGER_ROOTS,
      SCAVENGER_SCAVENGE,
      SCAVENGER_SEMISPACE,
      SCAVENGER_WEAK,
      NUMBER_OF_SCOPES
    };

    Scope(GCTracer* tracer, ScopeId scope);
    ~Scope();

   private:
    GCTracer* tracer_;
    ScopeId scope_;
    double start_time_;

    DISALLOW_COPY_AND_ASSIGN(Scope);
  };


  class AllocationEvent {
   public:
    // Default constructor leaves the event uninitialized.
    AllocationEvent() {}

    AllocationEvent(double duration, size_t allocation_in_bytes);

    // Time spent in the mutator during the end of the last sample to the
    // beginning of the next sample.
    double duration_;

    // Memory allocated in the new space during the end of the last sample
    // to the beginning of the next sample
    size_t allocation_in_bytes_;
  };


  class CompactionEvent {
   public:
    CompactionEvent() : duration(0), live_bytes_compacted(0) {}

    CompactionEvent(double duration, intptr_t live_bytes_compacted)
        : duration(duration), live_bytes_compacted(live_bytes_compacted) {}

    double duration;
    intptr_t live_bytes_compacted;
  };


  class ContextDisposalEvent {
   public:
    // Default constructor leaves the event uninitialized.
    ContextDisposalEvent() {}

    explicit ContextDisposalEvent(double time);

    // Time when context disposal event happened.
    double time_;
  };


  class SurvivalEvent {
   public:
    // Default constructor leaves the event uninitialized.
    SurvivalEvent() {}

    explicit SurvivalEvent(double survival_ratio);

    double promotion_ratio_;
  };


  class Event {
   public:
    enum Type {
      SCAVENGER = 0,
      MARK_COMPACTOR = 1,
      INCREMENTAL_MARK_COMPACTOR = 2,
      START = 3
    };

    // Default constructor leaves the event uninitialized.
    Event() {}

    Event(Type type, const char* gc_reason, const char* collector_reason);

    // Returns a string describing the event type.
    const char* TypeName(bool short_name) const;

    // Type of event
    Type type;

    const char* gc_reason;
    const char* collector_reason;

    // Timestamp set in the constructor.
    double start_time;

    // Timestamp set in the destructor.
    double end_time;

    // Memory reduction flag set.
    bool reduce_memory;

    // Size of objects in heap set in constructor.
    intptr_t start_object_size;

    // Size of objects in heap set in destructor.
    intptr_t end_object_size;

    // Size of memory allocated from OS set in constructor.
    intptr_t start_memory_size;

    // Size of memory allocated from OS set in destructor.
    intptr_t end_memory_size;

    // Total amount of space either wasted or contained in one of free lists
    // before the current GC.
    intptr_t start_holes_size;

    // Total amount of space either wasted or contained in one of free lists
    // after the current GC.
    intptr_t end_holes_size;

    // Size of new space objects in constructor.
    intptr_t new_space_object_size;
    // Size of survived new space objects in desctructor.
    intptr_t survived_new_space_object_size;

    // Number of incremental marking steps since creation of tracer.
    // (value at start of event)
    int cumulative_incremental_marking_steps;

    // Incremental marking steps since
    // - last event for SCAVENGER events
    // - last INCREMENTAL_MARK_COMPACTOR event for INCREMENTAL_MARK_COMPACTOR
    // events
    int incremental_marking_steps;

    // Bytes marked since creation of tracer (value at start of event).
    intptr_t cumulative_incremental_marking_bytes;

    // Bytes marked since
    // - last event for SCAVENGER events
    // - last INCREMENTAL_MARK_COMPACTOR event for INCREMENTAL_MARK_COMPACTOR
    // events
    intptr_t incremental_marking_bytes;

    // Cumulative duration of incremental marking steps since creation of
    // tracer. (value at start of event)
    double cumulative_incremental_marking_duration;

    // Duration of incremental marking steps since
    // - last event for SCAVENGER events
    // - last INCREMENTAL_MARK_COMPACTOR event for INCREMENTAL_MARK_COMPACTOR
    // events
    double incremental_marking_duration;

    // Cumulative pure duration of incremental marking steps since creation of
    // tracer. (value at start of event)
    double cumulative_pure_incremental_marking_duration;

    // Duration of pure incremental marking steps since
    // - last event for SCAVENGER events
    // - last INCREMENTAL_MARK_COMPACTOR event for INCREMENTAL_MARK_COMPACTOR
    // events
    double pure_incremental_marking_duration;

    // Longest incremental marking step since start of marking.
    // (value at start of event)
    double longest_incremental_marking_step;

    // Amounts of time spent in different scopes during GC.
    double scopes[Scope::NUMBER_OF_SCOPES];
  };

  static const size_t kRingBufferMaxSize = 10;

  typedef RingBuffer<Event, kRingBufferMaxSize> EventBuffer;

  typedef RingBuffer<AllocationEvent, kRingBufferMaxSize> AllocationEventBuffer;

  typedef RingBuffer<ContextDisposalEvent, kRingBufferMaxSize>
      ContextDisposalEventBuffer;

  typedef RingBuffer<CompactionEvent, kRingBufferMaxSize> CompactionEventBuffer;

  typedef RingBuffer<SurvivalEvent, kRingBufferMaxSize> SurvivalEventBuffer;

  static const int kThroughputTimeFrameMs = 5000;

  explicit GCTracer(Heap* heap);

  // Start collecting data.
  void Start(GarbageCollector collector, const char* gc_reason,
             const char* collector_reason);

  // Stop collecting data and print results.
  void Stop(GarbageCollector collector);

  // Sample and accumulate bytes allocated since the last GC.
  void SampleAllocation(double current_ms, size_t new_space_counter_bytes,
                        size_t old_generation_counter_bytes);

  // Log the accumulated new space allocation bytes.
  void AddAllocation(double current_ms);

  void AddContextDisposalTime(double time);

  void AddCompactionEvent(double duration, intptr_t live_bytes_compacted);

  void AddSurvivalRatio(double survival_ratio);

  // Log an incremental marking step.
  void AddIncrementalMarkingStep(double duration, intptr_t bytes);

  void AddIncrementalMarkingFinalizationStep(double duration);

  // Log time spent in marking.
  void AddMarkingTime(double duration) {
    cumulative_marking_duration_ += duration;
  }

  // Time spent in marking.
  double cumulative_marking_duration() const {
    return cumulative_marking_duration_;
  }

  // Log time spent in sweeping on main thread.
  void AddSweepingTime(double duration) {
    cumulative_sweeping_duration_ += duration;
  }

  // Time spent in sweeping on main thread.
  double cumulative_sweeping_duration() const {
    return cumulative_sweeping_duration_;
  }

  // Compute the mean duration of the last scavenger events. Returns 0 if no
  // events have been recorded.
  double MeanScavengerDuration() const {
    return MeanDuration(scavenger_events_);
  }

  // Compute the max duration of the last scavenger events. Returns 0 if no
  // events have been recorded.
  double MaxScavengerDuration() const { return MaxDuration(scavenger_events_); }

  // Compute the mean duration of the last mark compactor events. Returns 0 if
  // no events have been recorded.
  double MeanMarkCompactorDuration() const {
    return MeanDuration(mark_compactor_events_);
  }

  // Compute the max duration of the last mark compactor events. Return 0 if no
  // events have been recorded.
  double MaxMarkCompactorDuration() const {
    return MaxDuration(mark_compactor_events_);
  }

  // Compute the mean duration of the last incremental mark compactor
  // events. Returns 0 if no events have been recorded.
  double MeanIncrementalMarkCompactorDuration() const {
    return MeanDuration(incremental_mark_compactor_events_);
  }

  // Compute the mean step duration of the last incremental marking round.
  // Returns 0 if no incremental marking round has been completed.
  double MeanIncrementalMarkingDuration() const;

  // Compute the max step duration of the last incremental marking round.
  // Returns 0 if no incremental marking round has been completed.
  double MaxIncrementalMarkingDuration() const;

  // Compute the average incremental marking speed in bytes/millisecond.
  // Returns 0 if no events have been recorded.
  intptr_t IncrementalMarkingSpeedInBytesPerMillisecond() const;

  // Compute the average scavenge speed in bytes/millisecond.
  // Returns 0 if no events have been recorded.
  intptr_t ScavengeSpeedInBytesPerMillisecond(
      ScavengeSpeedMode mode = kForAllObjects) const;

  // Compute the average compaction speed in bytes/millisecond.
  // Returns 0 if not enough events have been recorded.
  intptr_t CompactionSpeedInBytesPerMillisecond() const;

  // Compute the average mark-sweep speed in bytes/millisecond.
  // Returns 0 if no events have been recorded.
  intptr_t MarkCompactSpeedInBytesPerMillisecond() const;

  // Compute the average incremental mark-sweep finalize speed in
  // bytes/millisecond.
  // Returns 0 if no events have been recorded.
  intptr_t FinalIncrementalMarkCompactSpeedInBytesPerMillisecond() const;

  // Compute the overall mark compact speed including incremental steps
  // and the final mark-compact step.
  double CombinedMarkCompactSpeedInBytesPerMillisecond();

  // Allocation throughput in the new space in bytes/millisecond.
  // Returns 0 if no allocation events have been recorded.
  size_t NewSpaceAllocationThroughputInBytesPerMillisecond(
      double time_ms = 0) const;

  // Allocation throughput in the old generation in bytes/millisecond in the
  // last time_ms milliseconds.
  // Returns 0 if no allocation events have been recorded.
  size_t OldGenerationAllocationThroughputInBytesPerMillisecond(
      double time_ms = 0) const;

  // Allocation throughput in heap in bytes/millisecond in the last time_ms
  // milliseconds.
  // Returns 0 if no allocation events have been recorded.
  size_t AllocationThroughputInBytesPerMillisecond(double time_ms) const;

  // Allocation throughput in heap in bytes/milliseconds in the last
  // kThroughputTimeFrameMs seconds.
  // Returns 0 if no allocation events have been recorded.
  size_t CurrentAllocationThroughputInBytesPerMillisecond() const;

  // Allocation throughput in old generation in bytes/milliseconds in the last
  // kThroughputTimeFrameMs seconds.
  // Returns 0 if no allocation events have been recorded.
  size_t CurrentOldGenerationAllocationThroughputInBytesPerMillisecond() const;

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

 private:
  // Print one detailed trace line in name=value format.
  // TODO(ernstm): Move to Heap.
  void PrintNVP() const;

  // Print one trace line.
  // TODO(ernstm): Move to Heap.
  void Print() const;

  // Prints a line and also adds it to the heap's ring buffer so that
  // it can be included in later crash dumps.
  void Output(const char* format, ...) const;

  // Compute the mean duration of the events in the given ring buffer.
  double MeanDuration(const EventBuffer& events) const;

  // Compute the max duration of the events in the given ring buffer.
  double MaxDuration(const EventBuffer& events) const;

  void ClearMarkCompactStatistics() {
    cumulative_incremental_marking_steps_ = 0;
    cumulative_incremental_marking_bytes_ = 0;
    cumulative_incremental_marking_duration_ = 0;
    cumulative_pure_incremental_marking_duration_ = 0;
    longest_incremental_marking_step_ = 0;
    cumulative_incremental_marking_finalization_steps_ = 0;
    cumulative_incremental_marking_finalization_duration_ = 0;
    longest_incremental_marking_finalization_step_ = 0;
    cumulative_marking_duration_ = 0;
    cumulative_sweeping_duration_ = 0;
  }

  // Pointer to the heap that owns this tracer.
  Heap* heap_;

  // Current tracer event. Populated during Start/Stop cycle. Valid after Stop()
  // has returned.
  Event current_;

  // Previous tracer event.
  Event previous_;

  // Previous INCREMENTAL_MARK_COMPACTOR event.
  Event previous_incremental_mark_compactor_event_;

  // RingBuffers for SCAVENGER events.
  EventBuffer scavenger_events_;

  // RingBuffers for MARK_COMPACTOR events.
  EventBuffer mark_compactor_events_;

  // RingBuffers for INCREMENTAL_MARK_COMPACTOR events.
  EventBuffer incremental_mark_compactor_events_;

  // RingBuffer for allocation events.
  AllocationEventBuffer new_space_allocation_events_;
  AllocationEventBuffer old_generation_allocation_events_;

  // RingBuffer for context disposal events.
  ContextDisposalEventBuffer context_disposal_events_;

  // RingBuffer for compaction events.
  CompactionEventBuffer compaction_events_;

  // RingBuffer for survival events.
  SurvivalEventBuffer survival_events_;

  // Cumulative number of incremental marking steps since creation of tracer.
  int cumulative_incremental_marking_steps_;

  // Cumulative size of incremental marking steps (in bytes) since creation of
  // tracer.
  intptr_t cumulative_incremental_marking_bytes_;

  // Cumulative duration of incremental marking steps since creation of tracer.
  double cumulative_incremental_marking_duration_;

  // Cumulative duration of pure incremental marking steps since creation of
  // tracer.
  double cumulative_pure_incremental_marking_duration_;

  // Longest incremental marking step since start of marking.
  double longest_incremental_marking_step_;

  // Cumulative number of incremental marking finalization steps since creation
  // of tracer.
  int cumulative_incremental_marking_finalization_steps_;

  // Cumulative duration of incremental marking finalization steps since
  // creation of tracer.
  double cumulative_incremental_marking_finalization_duration_;

  // Longest incremental marking finalization step since start of marking.
  double longest_incremental_marking_finalization_step_;

  // Total marking time.
  // This timer is precise when run with --print-cumulative-gc-stat
  double cumulative_marking_duration_;

  // Total sweeping time on the main thread.
  // This timer is precise when run with --print-cumulative-gc-stat
  // TODO(hpayer): Account for sweeping time on sweeper threads. Add a
  // different field for that.
  // TODO(hpayer): This timer right now just holds the sweeping time
  // of the initial atomic sweeping pause. Make sure that it accumulates
  // all sweeping operations performed on the main thread.
  double cumulative_sweeping_duration_;

  // Timestamp and allocation counter at the last sampled allocation event.
  double allocation_time_ms_;
  size_t new_space_allocation_counter_bytes_;
  size_t old_generation_allocation_counter_bytes_;

  // Accumulated duration and allocated bytes since the last GC.
  double allocation_duration_since_gc_;
  size_t new_space_allocation_in_bytes_since_gc_;
  size_t old_generation_allocation_in_bytes_since_gc_;

  double combined_mark_compact_speed_cache_;

  // Counts how many tracers were started without stopping.
  int start_counter_;

  DISALLOW_COPY_AND_ASSIGN(GCTracer);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_GC_TRACER_H_
