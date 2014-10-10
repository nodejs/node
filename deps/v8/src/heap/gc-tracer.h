// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_GC_TRACER_H_
#define V8_HEAP_GC_TRACER_H_

#include "src/base/platform/platform.h"

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

 private:
  T elements_[MAX_SIZE + 1];
  size_t begin_;
  size_t end_;

  DISALLOW_COPY_AND_ASSIGN(RingBuffer);
};


// GCTracer collects and prints ONE line after each garbage collector
// invocation IFF --trace_gc is used.
// TODO(ernstm): Unit tests.
class GCTracer {
 public:
  class Scope {
   public:
    enum ScopeId {
      EXTERNAL,
      MC_MARK,
      MC_SWEEP,
      MC_SWEEP_NEWSPACE,
      MC_SWEEP_OLDSPACE,
      MC_SWEEP_CODE,
      MC_SWEEP_CELL,
      MC_SWEEP_MAP,
      MC_EVACUATE_PAGES,
      MC_UPDATE_NEW_TO_NEW_POINTERS,
      MC_UPDATE_ROOT_TO_NEW_POINTERS,
      MC_UPDATE_OLD_TO_NEW_POINTERS,
      MC_UPDATE_POINTERS_TO_EVACUATED,
      MC_UPDATE_POINTERS_BETWEEN_EVACUATED,
      MC_UPDATE_MISC_POINTERS,
      MC_WEAKCOLLECTION_PROCESS,
      MC_WEAKCOLLECTION_CLEAR,
      MC_WEAKCOLLECTION_ABORT,
      MC_FLUSH_CODE,
      NUMBER_OF_SCOPES
    };

    Scope(GCTracer* tracer, ScopeId scope) : tracer_(tracer), scope_(scope) {
      start_time_ = base::OS::TimeCurrentMillis();
    }

    ~Scope() {
      DCHECK(scope_ < NUMBER_OF_SCOPES);  // scope_ is unsigned.
      tracer_->current_.scopes[scope_] +=
          base::OS::TimeCurrentMillis() - start_time_;
    }

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

    AllocationEvent(double duration, intptr_t allocation_in_bytes);

    // Time spent in the mutator during the end of the last garbage collection
    // to the beginning of the next garbage collection.
    double duration_;

    // Memory allocated in the new space during the end of the last garbage
    // collection to the beginning of the next garbage collection.
    intptr_t allocation_in_bytes_;
  };

  class Event {
   public:
    enum Type { SCAVENGER = 0, MARK_COMPACTOR = 1, START = 2 };

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

    // Number of incremental marking steps since creation of tracer.
    // (value at start of event)
    int cumulative_incremental_marking_steps;

    // Incremental marking steps since
    // - last event for SCAVENGER events
    // - last MARK_COMPACTOR event for MARK_COMPACTOR events
    int incremental_marking_steps;

    // Bytes marked since creation of tracer (value at start of event).
    intptr_t cumulative_incremental_marking_bytes;

    // Bytes marked since
    // - last event for SCAVENGER events
    // - last MARK_COMPACTOR event for MARK_COMPACTOR events
    intptr_t incremental_marking_bytes;

    // Cumulative duration of incremental marking steps since creation of
    // tracer. (value at start of event)
    double cumulative_incremental_marking_duration;

    // Duration of incremental marking steps since
    // - last event for SCAVENGER events
    // - last MARK_COMPACTOR event for MARK_COMPACTOR events
    double incremental_marking_duration;

    // Cumulative pure duration of incremental marking steps since creation of
    // tracer. (value at start of event)
    double cumulative_pure_incremental_marking_duration;

    // Duration of pure incremental marking steps since
    // - last event for SCAVENGER events
    // - last MARK_COMPACTOR event for MARK_COMPACTOR events
    double pure_incremental_marking_duration;

    // Longest incremental marking step since start of marking.
    // (value at start of event)
    double longest_incremental_marking_step;

    // Amounts of time spent in different scopes during GC.
    double scopes[Scope::NUMBER_OF_SCOPES];
  };

  static const int kRingBufferMaxSize = 10;

  typedef RingBuffer<Event, kRingBufferMaxSize> EventBuffer;

  typedef RingBuffer<AllocationEvent, kRingBufferMaxSize> AllocationEventBuffer;

  explicit GCTracer(Heap* heap);

  // Start collecting data.
  void Start(GarbageCollector collector, const char* gc_reason,
             const char* collector_reason);

  // Stop collecting data and print results.
  void Stop();

  // Log an allocation throughput event.
  void AddNewSpaceAllocationTime(double duration, intptr_t allocation_in_bytes);

  // Log an incremental marking step.
  void AddIncrementalMarkingStep(double duration, intptr_t bytes);

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
  intptr_t ScavengeSpeedInBytesPerMillisecond() const;

  // Compute the max mark-sweep speed in bytes/millisecond.
  // Returns 0 if no events have been recorded.
  intptr_t MarkCompactSpeedInBytesPerMillisecond() const;

  // Allocation throughput in the new space in bytes/millisecond.
  // Returns 0 if no events have been recorded.
  intptr_t NewSpaceAllocationThroughputInBytesPerMillisecond() const;

 private:
  // Print one detailed trace line in name=value format.
  // TODO(ernstm): Move to Heap.
  void PrintNVP() const;

  // Print one trace line.
  // TODO(ernstm): Move to Heap.
  void Print() const;

  // Compute the mean duration of the events in the given ring buffer.
  double MeanDuration(const EventBuffer& events) const;

  // Compute the max duration of the events in the given ring buffer.
  double MaxDuration(const EventBuffer& events) const;

  // Pointer to the heap that owns this tracer.
  Heap* heap_;

  // Current tracer event. Populated during Start/Stop cycle. Valid after Stop()
  // has returned.
  Event current_;

  // Previous tracer event.
  Event previous_;

  // Previous MARK_COMPACTOR event.
  Event previous_mark_compactor_event_;

  // RingBuffers for SCAVENGER events.
  EventBuffer scavenger_events_;

  // RingBuffers for MARK_COMPACTOR events.
  EventBuffer mark_compactor_events_;

  // RingBuffer for allocation events.
  AllocationEventBuffer allocation_events_;

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

  // Holds the new space top pointer recorded at the end of the last garbage
  // collection.
  intptr_t new_space_top_after_gc_;

  DISALLOW_COPY_AND_ASSIGN(GCTracer);
};
}
}  // namespace v8::internal

#endif  // V8_HEAP_GC_TRACER_H_
