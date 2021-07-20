// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_COUNTERS_H_
#define V8_LOGGING_COUNTERS_H_

#include <memory>

#include "include/v8.h"
#include "src/base/atomic-utils.h"
#include "src/base/optional.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/time.h"
#include "src/common/globals.h"
#include "src/logging/counters-definitions.h"
#include "src/logging/runtime-call-stats.h"
#include "src/objects/code-kind.h"
#include "src/objects/fixed-array.h"
#include "src/objects/objects.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

// StatsCounters is an interface for plugging into external
// counters for monitoring.  Counters can be looked up and
// manipulated by name.

class Counters;
class Isolate;

class StatsTable {
 public:
  StatsTable(const StatsTable&) = delete;
  StatsTable& operator=(const StatsTable&) = delete;

  // Register an application-defined function for recording
  // subsequent counter statistics.
  void SetCounterFunction(CounterLookupCallback f);

  // Register an application-defined function to create histograms for
  // recording subsequent histogram samples.
  void SetCreateHistogramFunction(CreateHistogramCallback f) {
    create_histogram_function_ = f;
  }

  // Register an application-defined function to add a sample
  // to a histogram created with CreateHistogram function.
  void SetAddHistogramSampleFunction(AddHistogramSampleCallback f) {
    add_histogram_sample_function_ = f;
  }

  bool HasCounterFunction() const { return lookup_function_ != nullptr; }

  // Lookup the location of a counter by name.  If the lookup
  // is successful, returns a non-nullptr pointer for writing the
  // value of the counter.  Each thread calling this function
  // may receive a different location to store it's counter.
  // The return value must not be cached and re-used across
  // threads, although a single thread is free to cache it.
  int* FindLocation(const char* name) {
    if (!lookup_function_) return nullptr;
    return lookup_function_(name);
  }

  // Create a histogram by name. If the create is successful,
  // returns a non-nullptr pointer for use with AddHistogramSample
  // function. min and max define the expected minimum and maximum
  // sample values. buckets is the maximum number of buckets
  // that the samples will be grouped into.
  void* CreateHistogram(const char* name, int min, int max, size_t buckets) {
    if (!create_histogram_function_) return nullptr;
    return create_histogram_function_(name, min, max, buckets);
  }

  // Add a sample to a histogram created with the CreateHistogram
  // function.
  void AddHistogramSample(void* histogram, int sample) {
    if (!add_histogram_sample_function_) return;
    return add_histogram_sample_function_(histogram, sample);
  }

 private:
  friend class Counters;

  explicit StatsTable(Counters* counters);

  CounterLookupCallback lookup_function_;
  CreateHistogramCallback create_histogram_function_;
  AddHistogramSampleCallback add_histogram_sample_function_;
};

// Base class for stats counters.
class StatsCounterBase {
 protected:
  Counters* counters_;
  const char* name_;
  int* ptr_;

  StatsCounterBase() = default;
  StatsCounterBase(Counters* counters, const char* name)
      : counters_(counters), name_(name), ptr_(nullptr) {}

  void SetLoc(int* loc, int value) { *loc = value; }
  void IncrementLoc(int* loc) { (*loc)++; }
  void IncrementLoc(int* loc, int value) { (*loc) += value; }
  void DecrementLoc(int* loc) { (*loc)--; }
  void DecrementLoc(int* loc, int value) { (*loc) -= value; }

  V8_EXPORT_PRIVATE int* FindLocationInStatsTable() const;
};

// StatsCounters are dynamically created values which can be tracked in
// the StatsTable.  They are designed to be lightweight to create and
// easy to use.
//
// Internally, a counter represents a value in a row of a StatsTable.
// The row has a 32bit value for each process/thread in the table and also
// a name (stored in the table metadata).  Since the storage location can be
// thread-specific, this class cannot be shared across threads. Note: This
// class is not thread safe.
class StatsCounter : public StatsCounterBase {
 public:
  // Sets the counter to a specific value.
  void Set(int value) {
    if (int* loc = GetPtr()) SetLoc(loc, value);
  }

  // Increments the counter.
  void Increment() {
    if (int* loc = GetPtr()) IncrementLoc(loc);
  }

  void Increment(int value) {
    if (int* loc = GetPtr()) IncrementLoc(loc, value);
  }

  // Decrements the counter.
  void Decrement() {
    if (int* loc = GetPtr()) DecrementLoc(loc);
  }

  void Decrement(int value) {
    if (int* loc = GetPtr()) DecrementLoc(loc, value);
  }

  // Is this counter enabled?
  // Returns false if table is full.
  bool Enabled() { return GetPtr() != nullptr; }

  // Get the internal pointer to the counter. This is used
  // by the code generator to emit code that manipulates a
  // given counter without calling the runtime system.
  int* GetInternalPointer() {
    int* loc = GetPtr();
    DCHECK_NOT_NULL(loc);
    return loc;
  }

 private:
  friend class Counters;

  StatsCounter() = default;
  StatsCounter(Counters* counters, const char* name)
      : StatsCounterBase(counters, name), lookup_done_(false) {}

  // Reset the cached internal pointer.
  void Reset() { lookup_done_ = false; }

  // Returns the cached address of this counter location.
  int* GetPtr() {
    if (lookup_done_) return ptr_;
    lookup_done_ = true;
    ptr_ = FindLocationInStatsTable();
    return ptr_;
  }

  bool lookup_done_;
};

// Thread safe version of StatsCounter.
class V8_EXPORT_PRIVATE StatsCounterThreadSafe : public StatsCounterBase {
 public:
  void Set(int Value);
  void Increment();
  void Increment(int value);
  void Decrement();
  void Decrement(int value);
  bool Enabled() { return ptr_ != nullptr; }
  int* GetInternalPointer() {
    DCHECK_NOT_NULL(ptr_);
    return ptr_;
  }

 private:
  friend class Counters;

  StatsCounterThreadSafe(Counters* counters, const char* name);
  void Reset() { ptr_ = FindLocationInStatsTable(); }

  base::Mutex mutex_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StatsCounterThreadSafe);
};

// A Histogram represents a dynamically created histogram in the
// StatsTable.  Note: This class is thread safe.
class Histogram {
 public:
  // Add a single sample to this histogram.
  void AddSample(int sample);

  // Returns true if this histogram is enabled.
  bool Enabled() { return histogram_ != nullptr; }

  const char* name() { return name_; }

  int min() const { return min_; }
  int max() const { return max_; }
  int num_buckets() const { return num_buckets_; }

  // Asserts that |expected_counters| are the same as the Counters this
  // Histogram reports to.
  void AssertReportsToCounters(Counters* expected_counters) {
    DCHECK_EQ(counters_, expected_counters);
  }

 protected:
  Histogram() = default;
  Histogram(const char* name, int min, int max, int num_buckets,
            Counters* counters)
      : name_(name),
        min_(min),
        max_(max),
        num_buckets_(num_buckets),
        histogram_(nullptr),
        counters_(counters) {
    DCHECK(counters_);
  }

  Counters* counters() const { return counters_; }

  // Reset the cached internal pointer.
  void Reset() { histogram_ = CreateHistogram(); }

 private:
  friend class Counters;

  void* CreateHistogram() const;

  const char* name_;
  int min_;
  int max_;
  int num_buckets_;
  void* histogram_;
  Counters* counters_;
};

enum class HistogramTimerResolution { MILLISECOND, MICROSECOND };

// A thread safe histogram timer. It also allows distributions of
// nested timed results.
class TimedHistogram : public Histogram {
 public:
  // Start the timer. Log if isolate non-null.
  V8_EXPORT_PRIVATE void Start(base::ElapsedTimer* timer, Isolate* isolate);

  // Stop the timer and record the results. Log if isolate non-null.
  V8_EXPORT_PRIVATE void Stop(base::ElapsedTimer* timer, Isolate* isolate);

  // Records a TimeDelta::Max() result. Useful to record percentage of tasks
  // that never got to run in a given scenario. Log if isolate non-null.
  void RecordAbandon(base::ElapsedTimer* timer, Isolate* isolate);

  // Add a single sample to this histogram.
  void AddTimedSample(base::TimeDelta sample);

 protected:
  friend class Counters;
  HistogramTimerResolution resolution_;

  TimedHistogram() = default;
  TimedHistogram(const char* name, int min, int max,
                 HistogramTimerResolution resolution, int num_buckets,
                 Counters* counters)
      : Histogram(name, min, max, num_buckets, counters),
        resolution_(resolution) {}
  void AddTimeSample();
};

// Helper class for scoping a TimedHistogram.
class V8_NODISCARD TimedHistogramScope {
 public:
  explicit TimedHistogramScope(TimedHistogram* histogram,
                               Isolate* isolate = nullptr)
      : histogram_(histogram), isolate_(isolate) {
    histogram_->Start(&timer_, isolate);
  }

  ~TimedHistogramScope() { histogram_->Stop(&timer_, isolate_); }

 private:
  base::ElapsedTimer timer_;
  TimedHistogram* histogram_;
  Isolate* isolate_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TimedHistogramScope);
};

enum class OptionalTimedHistogramScopeMode { TAKE_TIME, DONT_TAKE_TIME };

// Helper class for scoping a TimedHistogram.
// It will not take time for mode = DONT_TAKE_TIME.
class V8_NODISCARD OptionalTimedHistogramScope {
 public:
  OptionalTimedHistogramScope(TimedHistogram* histogram, Isolate* isolate,
                              OptionalTimedHistogramScopeMode mode)
      : histogram_(histogram), isolate_(isolate), mode_(mode) {
    if (mode == OptionalTimedHistogramScopeMode::TAKE_TIME) {
      histogram_->Start(&timer_, isolate);
    }
  }

  ~OptionalTimedHistogramScope() {
    if (mode_ == OptionalTimedHistogramScopeMode::TAKE_TIME) {
      histogram_->Stop(&timer_, isolate_);
    }
  }

 private:
  base::ElapsedTimer timer_;
  TimedHistogram* const histogram_;
  Isolate* const isolate_;
  const OptionalTimedHistogramScopeMode mode_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(OptionalTimedHistogramScope);
};

// Helper class for recording a TimedHistogram asynchronously with manual
// controls (it will not generate a report if destroyed without explicitly
// triggering a report). |async_counters| should be a shared_ptr to
// |histogram->counters()|, making it is safe to report to an
// AsyncTimedHistogram after the associated isolate has been destroyed.
// AsyncTimedHistogram can be moved/copied to avoid computing Now() multiple
// times when the times of multiple tasks are identical; each copy will generate
// its own report.
class AsyncTimedHistogram {
 public:
  explicit AsyncTimedHistogram(TimedHistogram* histogram,
                               std::shared_ptr<Counters> async_counters)
      : histogram_(histogram), async_counters_(std::move(async_counters)) {
    histogram_->AssertReportsToCounters(async_counters_.get());
    histogram_->Start(&timer_, nullptr);
  }

  // Records the time elapsed to |histogram_| and stops |timer_|.
  void RecordDone() { histogram_->Stop(&timer_, nullptr); }

  // Records TimeDelta::Max() to |histogram_| and stops |timer_|.
  void RecordAbandon() { histogram_->RecordAbandon(&timer_, nullptr); }

 private:
  base::ElapsedTimer timer_;
  TimedHistogram* histogram_;
  std::shared_ptr<Counters> async_counters_;
};

// Helper class for scoping a TimedHistogram, where the histogram is selected at
// stop time rather than start time.
// TODO(leszeks): This is heavily reliant on TimedHistogram::Start() doing
// nothing but starting the timer, and TimedHistogram::Stop() logging the sample
// correctly even if Start() was not called. This happens to be true iff Stop()
// is passed a null isolate, but that's an implementation detail of
// TimedHistogram, and we shouldn't rely on it.
class V8_NODISCARD LazyTimedHistogramScope {
 public:
  LazyTimedHistogramScope() : histogram_(nullptr) { timer_.Start(); }
  ~LazyTimedHistogramScope() {
    // We should set the histogram before this scope exits.
    DCHECK_NOT_NULL(histogram_);
    histogram_->Stop(&timer_, nullptr);
  }

  void set_histogram(TimedHistogram* histogram) { histogram_ = histogram; }

 private:
  base::ElapsedTimer timer_;
  TimedHistogram* histogram_;
};

// A HistogramTimer allows distributions of non-nested timed results
// to be created. WARNING: This class is not thread safe and can only
// be run on the foreground thread.
class HistogramTimer : public TimedHistogram {
 public:
  // Note: public for testing purposes only.
  HistogramTimer(const char* name, int min, int max,
                 HistogramTimerResolution resolution, int num_buckets,
                 Counters* counters)
      : TimedHistogram(name, min, max, resolution, num_buckets, counters) {}

  inline void Start();
  inline void Stop();

  // Returns true if the timer is running.
  bool Running() { return Enabled() && timer_.IsStarted(); }

  // TODO(bmeurer): Remove this when HistogramTimerScope is fixed.
#ifdef DEBUG
  base::ElapsedTimer* timer() { return &timer_; }
#endif

 private:
  friend class Counters;

  base::ElapsedTimer timer_;

  HistogramTimer() = default;
};

// Helper class for scoping a HistogramTimer.
// TODO(bmeurer): The ifdeffery is an ugly hack around the fact that the
// Parser is currently reentrant (when it throws an error, we call back
// into JavaScript and all bets are off), but ElapsedTimer is not
// reentry-safe. Fix this properly and remove |allow_nesting|.
class V8_NODISCARD HistogramTimerScope {
 public:
  explicit HistogramTimerScope(HistogramTimer* timer,
                               bool allow_nesting = false)
#ifdef DEBUG
      : timer_(timer), skipped_timer_start_(false) {
    if (timer_->timer()->IsStarted() && allow_nesting) {
      skipped_timer_start_ = true;
    } else {
      timer_->Start();
    }
  }
#else
      : timer_(timer) {
    timer_->Start();
  }
#endif
  ~HistogramTimerScope() {
#ifdef DEBUG
    if (!skipped_timer_start_) {
      timer_->Stop();
    }
#else
    timer_->Stop();
#endif
  }

 private:
  HistogramTimer* timer_;
#ifdef DEBUG
  bool skipped_timer_start_;
#endif
};

// A histogram timer that can aggregate events within a larger scope.
//
// Intended use of this timer is to have an outer (aggregating) and an inner
// (to be aggregated) scope, where the inner scope measure the time of events,
// and all those inner scope measurements will be summed up by the outer scope.
// An example use might be to aggregate the time spent in lazy compilation
// while running a script.
//
// Helpers:
// - AggregatingHistogramTimerScope, the "outer" scope within which
//     times will be summed up.
// - AggregatedHistogramTimerScope, the "inner" scope which defines the
//     events to be timed.
class AggregatableHistogramTimer : public Histogram {
 public:
  // Start/stop the "outer" scope.
  void Start() { time_ = base::TimeDelta(); }
  void Stop() {
    if (time_ != base::TimeDelta()) {
      // Only add non-zero samples, since zero samples represent situations
      // where there were no aggregated samples added.
      AddSample(static_cast<int>(time_.InMicroseconds()));
    }
  }

  // Add a time value ("inner" scope).
  void Add(base::TimeDelta other) { time_ += other; }

 private:
  friend class Counters;

  AggregatableHistogramTimer() = default;
  AggregatableHistogramTimer(const char* name, int min, int max,
                             int num_buckets, Counters* counters)
      : Histogram(name, min, max, num_buckets, counters) {}

  base::TimeDelta time_;
};

// A helper class for use with AggregatableHistogramTimer. This is the
// // outer-most timer scope used with an AggregatableHistogramTimer. It will
// // aggregate the information from the inner AggregatedHistogramTimerScope.
class V8_NODISCARD AggregatingHistogramTimerScope {
 public:
  explicit AggregatingHistogramTimerScope(AggregatableHistogramTimer* histogram)
      : histogram_(histogram) {
    histogram_->Start();
  }
  ~AggregatingHistogramTimerScope() { histogram_->Stop(); }

 private:
  AggregatableHistogramTimer* histogram_;
};

// A helper class for use with AggregatableHistogramTimer, the "inner" scope
// // which defines the events to be timed.
class V8_NODISCARD AggregatedHistogramTimerScope {
 public:
  explicit AggregatedHistogramTimerScope(AggregatableHistogramTimer* histogram)
      : histogram_(histogram) {
    timer_.Start();
  }
  ~AggregatedHistogramTimerScope() { histogram_->Add(timer_.Elapsed()); }

 private:
  base::ElapsedTimer timer_;
  AggregatableHistogramTimer* histogram_;
};

// AggretatedMemoryHistogram collects (time, value) sample pairs and turns
// them into time-uniform samples for the backing historgram, such that the
// backing histogram receives one sample every T ms, where the T is controlled
// by the FLAG_histogram_interval.
//
// More formally: let F be a real-valued function that maps time to sample
// values. We define F as a linear interpolation between adjacent samples. For
// each time interval [x; x + T) the backing histogram gets one sample value
// that is the average of F(t) in the interval.
template <typename Histogram>
class AggregatedMemoryHistogram {
 public:
  // Note: public for testing purposes only.
  explicit AggregatedMemoryHistogram(Histogram* backing_histogram)
      : AggregatedMemoryHistogram() {
    backing_histogram_ = backing_histogram;
  }

  // Invariants that hold before and after AddSample if
  // is_initialized_ is true:
  //
  // 1) For we processed samples that came in before start_ms_ and sent the
  // corresponding aggregated samples to backing histogram.
  // 2) (last_ms_, last_value_) is the last received sample.
  // 3) last_ms_ < start_ms_ + FLAG_histogram_interval.
  // 4) aggregate_value_ is the average of the function that is constructed by
  // linearly interpolating samples received between start_ms_ and last_ms_.
  void AddSample(double current_ms, double current_value);

 private:
  friend class Counters;

  AggregatedMemoryHistogram()
      : is_initialized_(false),
        start_ms_(0.0),
        last_ms_(0.0),
        aggregate_value_(0.0),
        last_value_(0.0),
        backing_histogram_(nullptr) {}
  double Aggregate(double current_ms, double current_value);

  bool is_initialized_;
  double start_ms_;
  double last_ms_;
  double aggregate_value_;
  double last_value_;
  Histogram* backing_histogram_;
};

template <typename Histogram>
void AggregatedMemoryHistogram<Histogram>::AddSample(double current_ms,
                                                     double current_value) {
  if (!is_initialized_) {
    aggregate_value_ = current_value;
    start_ms_ = current_ms;
    last_value_ = current_value;
    last_ms_ = current_ms;
    is_initialized_ = true;
  } else {
    const double kEpsilon = 1e-6;
    const int kMaxSamples = 1000;
    if (current_ms < last_ms_ + kEpsilon) {
      // Two samples have the same time, remember the last one.
      last_value_ = current_value;
    } else {
      double sample_interval_ms = FLAG_histogram_interval;
      double end_ms = start_ms_ + sample_interval_ms;
      if (end_ms <= current_ms + kEpsilon) {
        // Linearly interpolate between the last_ms_ and the current_ms.
        double slope = (current_value - last_value_) / (current_ms - last_ms_);
        int i;
        // Send aggregated samples to the backing histogram from the start_ms
        // to the current_ms.
        for (i = 0; i < kMaxSamples && end_ms <= current_ms + kEpsilon; i++) {
          double end_value = last_value_ + (end_ms - last_ms_) * slope;
          double sample_value;
          if (i == 0) {
            // Take aggregate_value_ into account.
            sample_value = Aggregate(end_ms, end_value);
          } else {
            // There is no aggregate_value_ for i > 0.
            sample_value = (last_value_ + end_value) / 2;
          }
          backing_histogram_->AddSample(static_cast<int>(sample_value + 0.5));
          last_value_ = end_value;
          last_ms_ = end_ms;
          end_ms += sample_interval_ms;
        }
        if (i == kMaxSamples) {
          // We hit the sample limit, ignore the remaining samples.
          aggregate_value_ = current_value;
          start_ms_ = current_ms;
        } else {
          aggregate_value_ = last_value_;
          start_ms_ = last_ms_;
        }
      }
      aggregate_value_ = current_ms > start_ms_ + kEpsilon
                             ? Aggregate(current_ms, current_value)
                             : aggregate_value_;
      last_value_ = current_value;
      last_ms_ = current_ms;
    }
  }
}

template <typename Histogram>
double AggregatedMemoryHistogram<Histogram>::Aggregate(double current_ms,
                                                       double current_value) {
  double interval_ms = current_ms - start_ms_;
  double value = (current_value + last_value_) / 2;
  // The aggregate_value_ is the average for [start_ms_; last_ms_].
  // The value is the average for [last_ms_; current_ms].
  // Return the weighted average of the aggregate_value_ and the value.
  return aggregate_value_ * ((last_ms_ - start_ms_) / interval_ms) +
         value * ((current_ms - last_ms_) / interval_ms);
}

// This file contains all the v8 counters that are in use.
class Counters : public std::enable_shared_from_this<Counters> {
 public:
  explicit Counters(Isolate* isolate);

  // Register an application-defined function for recording
  // subsequent counter statistics. Note: Must be called on the main
  // thread.
  void ResetCounterFunction(CounterLookupCallback f);

  // Register an application-defined function to create histograms for
  // recording subsequent histogram samples. Note: Must be called on
  // the main thread.
  void ResetCreateHistogramFunction(CreateHistogramCallback f);

  // Register an application-defined function to add a sample
  // to a histogram. Will be used in all subsequent sample additions.
  // Note: Must be called on the main thread.
  void SetAddHistogramSampleFunction(AddHistogramSampleCallback f) {
    stats_table_.SetAddHistogramSampleFunction(f);
  }

#define HR(name, caption, min, max, num_buckets) \
  Histogram* name() { return &name##_; }
  HISTOGRAM_RANGE_LIST(HR)
#undef HR

#define HT(name, caption, max, res) \
  HistogramTimer* name() { return &name##_; }
  HISTOGRAM_TIMER_LIST(HT)
#undef HT

#define HT(name, caption, max, res) \
  TimedHistogram* name() { return &name##_; }
  TIMED_HISTOGRAM_LIST(HT)
#undef HT

#define AHT(name, caption) \
  AggregatableHistogramTimer* name() { return &name##_; }
  AGGREGATABLE_HISTOGRAM_TIMER_LIST(AHT)
#undef AHT

#define HP(name, caption) \
  Histogram* name() { return &name##_; }
  HISTOGRAM_PERCENTAGE_LIST(HP)
#undef HP

#define HM(name, caption) \
  Histogram* name() { return &name##_; }
  HISTOGRAM_LEGACY_MEMORY_LIST(HM)
#undef HM

#define SC(name, caption) \
  StatsCounter* name() { return &name##_; }
  STATS_COUNTER_LIST_1(SC)
  STATS_COUNTER_LIST_2(SC)
  STATS_COUNTER_NATIVE_CODE_LIST(SC)
#undef SC

#define SC(name, caption) \
  StatsCounterThreadSafe* name() { return &name##_; }
  STATS_COUNTER_TS_LIST(SC)
#undef SC

  // clang-format off
  enum Id {
#define RATE_ID(name, caption, max, res) k_##name,
    HISTOGRAM_TIMER_LIST(RATE_ID)
    TIMED_HISTOGRAM_LIST(RATE_ID)
#undef RATE_ID
#define AGGREGATABLE_ID(name, caption) k_##name,
    AGGREGATABLE_HISTOGRAM_TIMER_LIST(AGGREGATABLE_ID)
#undef AGGREGATABLE_ID
#define PERCENTAGE_ID(name, caption) k_##name,
    HISTOGRAM_PERCENTAGE_LIST(PERCENTAGE_ID)
#undef PERCENTAGE_ID
#define MEMORY_ID(name, caption) k_##name,
    HISTOGRAM_LEGACY_MEMORY_LIST(MEMORY_ID)
#undef MEMORY_ID
#define COUNTER_ID(name, caption) k_##name,
    STATS_COUNTER_LIST_1(COUNTER_ID)
    STATS_COUNTER_LIST_2(COUNTER_ID)
    STATS_COUNTER_TS_LIST(COUNTER_ID)
    STATS_COUNTER_NATIVE_CODE_LIST(COUNTER_ID)
#undef COUNTER_ID
#define COUNTER_ID(name) kCountOf##name, kSizeOf##name,
    INSTANCE_TYPE_LIST(COUNTER_ID)
#undef COUNTER_ID
#define COUNTER_ID(name) kCountOfCODE_TYPE_##name, \
    kSizeOfCODE_TYPE_##name,
    CODE_KIND_LIST(COUNTER_ID)
#undef COUNTER_ID
#define COUNTER_ID(name) kCountOfFIXED_ARRAY__##name, \
    kSizeOfFIXED_ARRAY__##name,
    FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(COUNTER_ID)
#undef COUNTER_ID
    stats_counter_count
  };
  // clang-format on

#ifdef V8_RUNTIME_CALL_STATS
  RuntimeCallStats* runtime_call_stats() { return &runtime_call_stats_; }

  WorkerThreadRuntimeCallStats* worker_thread_runtime_call_stats() {
    return &worker_thread_runtime_call_stats_;
  }
#else   // V8_RUNTIME_CALL_STATS
  RuntimeCallStats* runtime_call_stats() { return nullptr; }

  WorkerThreadRuntimeCallStats* worker_thread_runtime_call_stats() {
    return nullptr;
  }
#endif  // V8_RUNTIME_CALL_STATS

 private:
  friend class StatsTable;
  friend class StatsCounterBase;
  friend class Histogram;
  friend class HistogramTimer;

  int* FindLocation(const char* name) {
    return stats_table_.FindLocation(name);
  }

  void* CreateHistogram(const char* name, int min, int max, size_t buckets) {
    return stats_table_.CreateHistogram(name, min, max, buckets);
  }

  void AddHistogramSample(void* histogram, int sample) {
    stats_table_.AddHistogramSample(histogram, sample);
  }

  Isolate* isolate() { return isolate_; }

#define HR(name, caption, min, max, num_buckets) Histogram name##_;
  HISTOGRAM_RANGE_LIST(HR)
#undef HR

#define HT(name, caption, max, res) HistogramTimer name##_;
  HISTOGRAM_TIMER_LIST(HT)
#undef HT

#define HT(name, caption, max, res) TimedHistogram name##_;
  TIMED_HISTOGRAM_LIST(HT)
#undef HT

#define AHT(name, caption) AggregatableHistogramTimer name##_;
  AGGREGATABLE_HISTOGRAM_TIMER_LIST(AHT)
#undef AHT

#define HP(name, caption) Histogram name##_;
  HISTOGRAM_PERCENTAGE_LIST(HP)
#undef HP

#define HM(name, caption) Histogram name##_;
  HISTOGRAM_LEGACY_MEMORY_LIST(HM)
#undef HM

#define SC(name, caption) StatsCounter name##_;
  STATS_COUNTER_LIST_1(SC)
  STATS_COUNTER_LIST_2(SC)
  STATS_COUNTER_NATIVE_CODE_LIST(SC)
#undef SC

#define SC(name, caption) StatsCounterThreadSafe name##_;
  STATS_COUNTER_TS_LIST(SC)
#undef SC

#define SC(name)                  \
  StatsCounter size_of_##name##_; \
  StatsCounter count_of_##name##_;
  INSTANCE_TYPE_LIST(SC)
#undef SC

#define SC(name)                            \
  StatsCounter size_of_CODE_TYPE_##name##_; \
  StatsCounter count_of_CODE_TYPE_##name##_;
  CODE_KIND_LIST(SC)
#undef SC

#define SC(name)                              \
  StatsCounter size_of_FIXED_ARRAY_##name##_; \
  StatsCounter count_of_FIXED_ARRAY_##name##_;
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(SC)
#undef SC

#ifdef V8_RUNTIME_CALL_STATS
  RuntimeCallStats runtime_call_stats_;
  WorkerThreadRuntimeCallStats worker_thread_runtime_call_stats_;
#endif
  Isolate* isolate_;
  StatsTable stats_table_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Counters);
};

void HistogramTimer::Start() {
  TimedHistogram::Start(&timer_, counters()->isolate());
}

void HistogramTimer::Stop() {
  TimedHistogram::Stop(&timer_, counters()->isolate());
}

}  // namespace internal
}  // namespace v8

#endif  // V8_LOGGING_COUNTERS_H_
