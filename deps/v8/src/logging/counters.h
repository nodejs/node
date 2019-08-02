// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_COUNTERS_H_
#define V8_LOGGING_COUNTERS_H_

#include "include/v8.h"
#include "src/base/atomic-utils.h"
#include "src/base/optional.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/time.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/init/heap-symbols.h"
#include "src/logging/counters-definitions.h"
#include "src/objects/objects.h"
#include "src/runtime/runtime.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/traced-value.h"
#include "src/tracing/tracing-category-observer.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

// This struct contains a set of flags that can be modified from multiple
// threads at runtime unlike the normal FLAG_-like flags which are not modified
// after V8 instance is initialized.

struct TracingFlags {
  static V8_EXPORT_PRIVATE std::atomic_uint runtime_stats;
  static V8_EXPORT_PRIVATE std::atomic_uint gc;
  static V8_EXPORT_PRIVATE std::atomic_uint gc_stats;
  static V8_EXPORT_PRIVATE std::atomic_uint ic_stats;

  static bool is_runtime_stats_enabled() {
    return runtime_stats.load(std::memory_order_relaxed) != 0;
  }

  static bool is_gc_enabled() {
    return gc.load(std::memory_order_relaxed) != 0;
  }

  static bool is_gc_stats_enabled() {
    return gc_stats.load(std::memory_order_relaxed) != 0;
  }

  static bool is_ic_stats_enabled() {
    return ic_stats.load(std::memory_order_relaxed) != 0;
  }
};

// StatsCounters is an interface for plugging into external
// counters for monitoring.  Counters can be looked up and
// manipulated by name.

class Counters;

class StatsTable {
 public:
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

  DISALLOW_COPY_AND_ASSIGN(StatsTable);
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
class TimedHistogramScope {
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
class OptionalTimedHistogramScope {
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
class LazyTimedHistogramScope {
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
class HistogramTimerScope {
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
class AggregatingHistogramTimerScope {
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
class AggregatedHistogramTimerScope {
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

class RuntimeCallCounter final {
 public:
  RuntimeCallCounter() : RuntimeCallCounter(nullptr) {}
  explicit RuntimeCallCounter(const char* name)
      : name_(name), count_(0), time_(0) {}
  V8_NOINLINE void Reset();
  V8_NOINLINE void Dump(v8::tracing::TracedValue* value);
  void Add(RuntimeCallCounter* other);

  const char* name() const { return name_; }
  int64_t count() const { return count_; }
  base::TimeDelta time() const {
    return base::TimeDelta::FromMicroseconds(time_);
  }
  void Increment() { count_++; }
  void Add(base::TimeDelta delta) { time_ += delta.InMicroseconds(); }

 private:
  friend class RuntimeCallStats;

  const char* name_;
  int64_t count_;
  // Stored as int64_t so that its initialization can be deferred.
  int64_t time_;
};

// RuntimeCallTimer is used to keep track of the stack of currently active
// timers used for properly measuring the own time of a RuntimeCallCounter.
class RuntimeCallTimer final {
 public:
  RuntimeCallCounter* counter() { return counter_; }
  void set_counter(RuntimeCallCounter* counter) { counter_ = counter; }
  RuntimeCallTimer* parent() const { return parent_.Value(); }
  void set_parent(RuntimeCallTimer* timer) { parent_.SetValue(timer); }
  const char* name() const { return counter_->name(); }

  inline bool IsStarted();

  inline void Start(RuntimeCallCounter* counter, RuntimeCallTimer* parent);
  void Snapshot();
  inline RuntimeCallTimer* Stop();

  // Make the time source configurable for testing purposes.
  V8_EXPORT_PRIVATE static base::TimeTicks (*Now)();

 private:
  inline void Pause(base::TimeTicks now);
  inline void Resume(base::TimeTicks now);
  inline void CommitTimeToCounter();

  RuntimeCallCounter* counter_ = nullptr;
  base::AtomicValue<RuntimeCallTimer*> parent_;
  base::TimeTicks start_ticks_;
  base::TimeDelta elapsed_;
};

#define FOR_EACH_GC_COUNTER(V) \
  TRACER_SCOPES(V)             \
  TRACER_BACKGROUND_SCOPES(V)

#define FOR_EACH_API_COUNTER(V)                            \
  V(ArrayBuffer_Cast)                                      \
  V(ArrayBuffer_Detach)                                    \
  V(ArrayBuffer_New)                                       \
  V(Array_CloneElementAt)                                  \
  V(Array_New)                                             \
  V(BigInt64Array_New)                                     \
  V(BigInt_NewFromWords)                                   \
  V(BigIntObject_BigIntValue)                              \
  V(BigIntObject_New)                                      \
  V(BigUint64Array_New)                                    \
  V(BooleanObject_BooleanValue)                            \
  V(BooleanObject_New)                                     \
  V(Context_New)                                           \
  V(Context_NewRemoteContext)                              \
  V(DataView_New)                                          \
  V(Date_New)                                              \
  V(Date_NumberValue)                                      \
  V(Debug_Call)                                            \
  V(debug_GetPrivateFields)                                \
  V(Error_New)                                             \
  V(External_New)                                          \
  V(Float32Array_New)                                      \
  V(Float64Array_New)                                      \
  V(Function_Call)                                         \
  V(Function_New)                                          \
  V(Function_NewInstance)                                  \
  V(FunctionTemplate_GetFunction)                          \
  V(FunctionTemplate_New)                                  \
  V(FunctionTemplate_NewRemoteInstance)                    \
  V(FunctionTemplate_NewWithCache)                         \
  V(FunctionTemplate_NewWithFastHandler)                   \
  V(Int16Array_New)                                        \
  V(Int32Array_New)                                        \
  V(Int8Array_New)                                         \
  V(Isolate_DateTimeConfigurationChangeNotification)       \
  V(Isolate_LocaleConfigurationChangeNotification)         \
  V(JSON_Parse)                                            \
  V(JSON_Stringify)                                        \
  V(Map_AsArray)                                           \
  V(Map_Clear)                                             \
  V(Map_Delete)                                            \
  V(Map_Get)                                               \
  V(Map_Has)                                               \
  V(Map_New)                                               \
  V(Map_Set)                                               \
  V(Message_GetEndColumn)                                  \
  V(Message_GetLineNumber)                                 \
  V(Message_GetSourceLine)                                 \
  V(Message_GetStartColumn)                                \
  V(Module_Evaluate)                                       \
  V(Module_InstantiateModule)                              \
  V(NumberObject_New)                                      \
  V(NumberObject_NumberValue)                              \
  V(Object_CallAsConstructor)                              \
  V(Object_CallAsFunction)                                 \
  V(Object_CreateDataProperty)                             \
  V(Object_DefineOwnProperty)                              \
  V(Object_DefineProperty)                                 \
  V(Object_Delete)                                         \
  V(Object_DeleteProperty)                                 \
  V(Object_ForceSet)                                       \
  V(Object_Get)                                            \
  V(Object_GetOwnPropertyDescriptor)                       \
  V(Object_GetOwnPropertyNames)                            \
  V(Object_GetPropertyAttributes)                          \
  V(Object_GetPropertyNames)                               \
  V(Object_GetRealNamedProperty)                           \
  V(Object_GetRealNamedPropertyAttributes)                 \
  V(Object_GetRealNamedPropertyAttributesInPrototypeChain) \
  V(Object_GetRealNamedPropertyInPrototypeChain)           \
  V(Object_Has)                                            \
  V(Object_HasOwnProperty)                                 \
  V(Object_HasRealIndexedProperty)                         \
  V(Object_HasRealNamedCallbackProperty)                   \
  V(Object_HasRealNamedProperty)                           \
  V(Object_New)                                            \
  V(Object_ObjectProtoToString)                            \
  V(Object_Set)                                            \
  V(Object_SetAccessor)                                    \
  V(Object_SetIntegrityLevel)                              \
  V(Object_SetPrivate)                                     \
  V(Object_SetPrototype)                                   \
  V(ObjectTemplate_New)                                    \
  V(ObjectTemplate_NewInstance)                            \
  V(Object_ToArrayIndex)                                   \
  V(Object_ToBigInt)                                       \
  V(Object_ToDetailString)                                 \
  V(Object_ToInt32)                                        \
  V(Object_ToInteger)                                      \
  V(Object_ToNumber)                                       \
  V(Object_ToObject)                                       \
  V(Object_ToString)                                       \
  V(Object_ToUint32)                                       \
  V(Persistent_New)                                        \
  V(Private_New)                                           \
  V(Promise_Catch)                                         \
  V(Promise_Chain)                                         \
  V(Promise_HasRejectHandler)                              \
  V(Promise_Resolver_New)                                  \
  V(Promise_Resolver_Reject)                               \
  V(Promise_Resolver_Resolve)                              \
  V(Promise_Result)                                        \
  V(Promise_Status)                                        \
  V(Promise_Then)                                          \
  V(Proxy_New)                                             \
  V(RangeError_New)                                        \
  V(ReferenceError_New)                                    \
  V(RegExp_New)                                            \
  V(ScriptCompiler_Compile)                                \
  V(ScriptCompiler_CompileFunctionInContext)               \
  V(ScriptCompiler_CompileUnbound)                         \
  V(Script_Run)                                            \
  V(Set_Add)                                               \
  V(Set_AsArray)                                           \
  V(Set_Clear)                                             \
  V(Set_Delete)                                            \
  V(Set_Has)                                               \
  V(Set_New)                                               \
  V(SharedArrayBuffer_New)                                 \
  V(String_Concat)                                         \
  V(String_NewExternalOneByte)                             \
  V(String_NewExternalTwoByte)                             \
  V(String_NewFromOneByte)                                 \
  V(String_NewFromTwoByte)                                 \
  V(String_NewFromUtf8)                                    \
  V(StringObject_New)                                      \
  V(StringObject_StringValue)                              \
  V(String_Write)                                          \
  V(String_WriteUtf8)                                      \
  V(Symbol_New)                                            \
  V(SymbolObject_New)                                      \
  V(SymbolObject_SymbolValue)                              \
  V(SyntaxError_New)                                       \
  V(TracedGlobal_New)                                      \
  V(TryCatch_StackTrace)                                   \
  V(TypeError_New)                                         \
  V(Uint16Array_New)                                       \
  V(Uint32Array_New)                                       \
  V(Uint8Array_New)                                        \
  V(Uint8ClampedArray_New)                                 \
  V(UnboundScript_GetId)                                   \
  V(UnboundScript_GetLineNumber)                           \
  V(UnboundScript_GetName)                                 \
  V(UnboundScript_GetSourceMappingURL)                     \
  V(UnboundScript_GetSourceURL)                            \
  V(ValueDeserializer_ReadHeader)                          \
  V(ValueDeserializer_ReadValue)                           \
  V(ValueSerializer_WriteValue)                            \
  V(Value_InstanceOf)                                      \
  V(Value_Int32Value)                                      \
  V(Value_IntegerValue)                                    \
  V(Value_NumberValue)                                     \
  V(Value_TypeOf)                                          \
  V(Value_Uint32Value)                                     \
  V(WeakMap_Get)                                           \
  V(WeakMap_New)                                           \
  V(WeakMap_Set)

#define FOR_EACH_MANUAL_COUNTER(V)             \
  V(AccessorGetterCallback)                    \
  V(AccessorSetterCallback)                    \
  V(ArrayLengthGetter)                         \
  V(ArrayLengthSetter)                         \
  V(BoundFunctionLengthGetter)                 \
  V(BoundFunctionNameGetter)                   \
  V(CompileAnalyse)                            \
  V(CompileBackgroundAnalyse)                  \
  V(CompileBackgroundCompileTask)              \
  V(CompileBackgroundEval)                     \
  V(CompileBackgroundFunction)                 \
  V(CompileBackgroundIgnition)                 \
  V(CompileBackgroundRewriteReturnResult)      \
  V(CompileBackgroundScopeAnalysis)            \
  V(CompileBackgroundScript)                   \
  V(CompileCollectSourcePositions)             \
  V(CompileDeserialize)                        \
  V(CompileEnqueueOnDispatcher)                \
  V(CompileEval)                               \
  V(CompileFinalizeBackgroundCompileTask)      \
  V(CompileFinishNowOnDispatcher)              \
  V(CompileFunction)                           \
  V(CompileGetFromOptimizedCodeMap)            \
  V(CompileIgnition)                           \
  V(CompileIgnitionFinalization)               \
  V(CompileRewriteReturnResult)                \
  V(CompileScopeAnalysis)                      \
  V(CompileScript)                             \
  V(CompileSerialize)                          \
  V(CompileWaitForDispatcher)                  \
  V(DeoptimizeCode)                            \
  V(DeserializeContext)                        \
  V(DeserializeIsolate)                        \
  V(FunctionCallback)                          \
  V(FunctionLengthGetter)                      \
  V(FunctionPrototypeGetter)                   \
  V(FunctionPrototypeSetter)                   \
  V(GC_Custom_AllAvailableGarbage)             \
  V(GC_Custom_IncrementalMarkingObserver)      \
  V(GC_Custom_SlowAllocateRaw)                 \
  V(GCEpilogueCallback)                        \
  V(GCPrologueCallback)                        \
  V(Genesis)                                   \
  V(GetMoreDataCallback)                       \
  V(IndexedDefinerCallback)                    \
  V(IndexedDeleterCallback)                    \
  V(IndexedDescriptorCallback)                 \
  V(IndexedEnumeratorCallback)                 \
  V(IndexedGetterCallback)                     \
  V(IndexedQueryCallback)                      \
  V(IndexedSetterCallback)                     \
  V(Invoke)                                    \
  V(InvokeApiFunction)                         \
  V(InvokeApiInterruptCallbacks)               \
  V(InvokeFunctionCallback)                    \
  V(JS_Execution)                              \
  V(Map_SetPrototype)                          \
  V(Map_TransitionToAccessorProperty)          \
  V(Map_TransitionToDataProperty)              \
  V(MessageListenerCallback)                   \
  V(NamedDefinerCallback)                      \
  V(NamedDeleterCallback)                      \
  V(NamedDescriptorCallback)                   \
  V(NamedEnumeratorCallback)                   \
  V(NamedGetterCallback)                       \
  V(NamedQueryCallback)                        \
  V(NamedSetterCallback)                       \
  V(Object_DeleteProperty)                     \
  V(ObjectVerify)                              \
  V(OptimizeCode)                              \
  V(ParseArrowFunctionLiteral)                 \
  V(ParseBackgroundArrowFunctionLiteral)       \
  V(ParseBackgroundFunctionLiteral)            \
  V(ParseBackgroundProgram)                    \
  V(ParseEval)                                 \
  V(ParseFunction)                             \
  V(ParseFunctionLiteral)                      \
  V(ParseProgram)                              \
  V(PreParseArrowFunctionLiteral)              \
  V(PreParseBackgroundArrowFunctionLiteral)    \
  V(PreParseBackgroundWithVariableResolution)  \
  V(PreParseWithVariableResolution)            \
  V(PropertyCallback)                          \
  V(PrototypeMap_TransitionToAccessorProperty) \
  V(PrototypeMap_TransitionToDataProperty)     \
  V(PrototypeObject_DeleteProperty)            \
  V(RecompileConcurrent)                       \
  V(RecompileSynchronous)                      \
  V(ReconfigureToDataProperty)                 \
  V(StringLengthGetter)                        \
  V(TestCounter1)                              \
  V(TestCounter2)                              \
  V(TestCounter3)

#define FOR_EACH_HANDLER_COUNTER(V)               \
  V(KeyedLoadIC_KeyedLoadSloppyArgumentsStub)     \
  V(KeyedLoadIC_LoadElementDH)                    \
  V(KeyedLoadIC_LoadIndexedInterceptorStub)       \
  V(KeyedLoadIC_LoadIndexedStringDH)              \
  V(KeyedLoadIC_SlowStub)                         \
  V(KeyedStoreIC_ElementsTransitionAndStoreStub)  \
  V(KeyedStoreIC_KeyedStoreSloppyArgumentsStub)   \
  V(KeyedStoreIC_SlowStub)                        \
  V(KeyedStoreIC_StoreElementStub)                \
  V(KeyedStoreIC_StoreFastElementStub)            \
  V(LoadGlobalIC_LoadScriptContextField)          \
  V(LoadGlobalIC_SlowStub)                        \
  V(LoadIC_FunctionPrototypeStub)                 \
  V(LoadIC_HandlerCacheHit_Accessor)              \
  V(LoadIC_LoadAccessorDH)                        \
  V(LoadIC_LoadAccessorFromPrototypeDH)           \
  V(LoadIC_LoadApiGetterFromPrototypeDH)          \
  V(LoadIC_LoadCallback)                          \
  V(LoadIC_LoadConstantDH)                        \
  V(LoadIC_LoadConstantFromPrototypeDH)           \
  V(LoadIC_LoadFieldDH)                           \
  V(LoadIC_LoadFieldFromPrototypeDH)              \
  V(LoadIC_LoadGlobalDH)                          \
  V(LoadIC_LoadGlobalFromPrototypeDH)             \
  V(LoadIC_LoadIntegerIndexedExoticDH)            \
  V(LoadIC_LoadInterceptorDH)                     \
  V(LoadIC_LoadInterceptorFromPrototypeDH)        \
  V(LoadIC_LoadNativeDataPropertyDH)              \
  V(LoadIC_LoadNativeDataPropertyFromPrototypeDH) \
  V(LoadIC_LoadNonexistentDH)                     \
  V(LoadIC_LoadNonMaskingInterceptorDH)           \
  V(LoadIC_LoadNormalDH)                          \
  V(LoadIC_LoadNormalFromPrototypeDH)             \
  V(LoadIC_NonReceiver)                           \
  V(LoadIC_Premonomorphic)                        \
  V(LoadIC_SlowStub)                              \
  V(LoadIC_StringLength)                          \
  V(LoadIC_StringWrapperLength)                   \
  V(StoreGlobalIC_SlowStub)                       \
  V(StoreGlobalIC_StoreScriptContextField)        \
  V(StoreGlobalIC_Premonomorphic)                 \
  V(StoreIC_HandlerCacheHit_Accessor)             \
  V(StoreIC_NonReceiver)                          \
  V(StoreIC_Premonomorphic)                       \
  V(StoreIC_SlowStub)                             \
  V(StoreIC_StoreAccessorDH)                      \
  V(StoreIC_StoreAccessorOnPrototypeDH)           \
  V(StoreIC_StoreApiSetterOnPrototypeDH)          \
  V(StoreIC_StoreFieldDH)                         \
  V(StoreIC_StoreGlobalDH)                        \
  V(StoreIC_StoreGlobalTransitionDH)              \
  V(StoreIC_StoreInterceptorStub)                 \
  V(StoreIC_StoreNativeDataPropertyDH)            \
  V(StoreIC_StoreNativeDataPropertyOnPrototypeDH) \
  V(StoreIC_StoreNormalDH)                        \
  V(StoreIC_StoreTransitionDH)                    \
  V(StoreInArrayLiteralIC_SlowStub)

enum RuntimeCallCounterId {
#define CALL_RUNTIME_COUNTER(name) kGC_##name,
  FOR_EACH_GC_COUNTER(CALL_RUNTIME_COUNTER)
#undef CALL_RUNTIME_COUNTER
#define CALL_RUNTIME_COUNTER(name) k##name,
      FOR_EACH_MANUAL_COUNTER(CALL_RUNTIME_COUNTER)
#undef CALL_RUNTIME_COUNTER
#define CALL_RUNTIME_COUNTER(name, nargs, ressize) kRuntime_##name,
          FOR_EACH_INTRINSIC(CALL_RUNTIME_COUNTER)
#undef CALL_RUNTIME_COUNTER
#define CALL_BUILTIN_COUNTER(name) kBuiltin_##name,
              BUILTIN_LIST_C(CALL_BUILTIN_COUNTER)
#undef CALL_BUILTIN_COUNTER
#define CALL_BUILTIN_COUNTER(name) kAPI_##name,
                  FOR_EACH_API_COUNTER(CALL_BUILTIN_COUNTER)
#undef CALL_BUILTIN_COUNTER
#define CALL_BUILTIN_COUNTER(name) kHandler_##name,
                      FOR_EACH_HANDLER_COUNTER(CALL_BUILTIN_COUNTER)
#undef CALL_BUILTIN_COUNTER
                          kNumberOfCounters
};

class RuntimeCallStats final {
 public:
  V8_EXPORT_PRIVATE RuntimeCallStats();

  // Starting measuring the time for a function. This will establish the
  // connection to the parent counter for properly calculating the own times.
  V8_EXPORT_PRIVATE void Enter(RuntimeCallTimer* timer,
                               RuntimeCallCounterId counter_id);

  // Leave a scope for a measured runtime function. This will properly add
  // the time delta to the current_counter and subtract the delta from its
  // parent.
  V8_EXPORT_PRIVATE void Leave(RuntimeCallTimer* timer);

  // Set counter id for the innermost measurement. It can be used to refine
  // event kind when a runtime entry counter is too generic.
  V8_EXPORT_PRIVATE void CorrectCurrentCounterId(
      RuntimeCallCounterId counter_id);

  V8_EXPORT_PRIVATE void Reset();
  // Add all entries from another stats object.
  void Add(RuntimeCallStats* other);
  V8_EXPORT_PRIVATE void Print(std::ostream& os);
  V8_EXPORT_PRIVATE void Print();
  V8_NOINLINE void Dump(v8::tracing::TracedValue* value);

  ThreadId thread_id() const { return thread_id_; }
  RuntimeCallTimer* current_timer() { return current_timer_.Value(); }
  RuntimeCallCounter* current_counter() { return current_counter_.Value(); }
  bool InUse() { return in_use_; }
  bool IsCalledOnTheSameThread();

  static const int kNumberOfCounters =
      static_cast<int>(RuntimeCallCounterId::kNumberOfCounters);
  RuntimeCallCounter* GetCounter(RuntimeCallCounterId counter_id) {
    return &counters_[static_cast<int>(counter_id)];
  }
  RuntimeCallCounter* GetCounter(int counter_id) {
    return &counters_[counter_id];
  }

 private:
  // Top of a stack of active timers.
  base::AtomicValue<RuntimeCallTimer*> current_timer_;
  // Active counter object associated with current timer.
  base::AtomicValue<RuntimeCallCounter*> current_counter_;
  // Used to track nested tracing scopes.
  bool in_use_;
  ThreadId thread_id_;
  RuntimeCallCounter counters_[kNumberOfCounters];
};

class WorkerThreadRuntimeCallStats final {
 public:
  WorkerThreadRuntimeCallStats();
  ~WorkerThreadRuntimeCallStats();

  // Returns the TLS key associated with this WorkerThreadRuntimeCallStats.
  base::Thread::LocalStorageKey GetKey();

  // Returns a new worker thread runtime call stats table managed by this
  // WorkerThreadRuntimeCallStats.
  RuntimeCallStats* NewTable();

  // Adds the counters from the worker thread tables to |main_call_stats|.
  void AddToMainTable(RuntimeCallStats* main_call_stats);

 private:
  base::Mutex mutex_;
  std::vector<std::unique_ptr<RuntimeCallStats>> tables_;
  base::Optional<base::Thread::LocalStorageKey> tls_key_;
};

// Creating a WorkerThreadRuntimeCallStatsScope will provide a thread-local
// runtime call stats table, and will dump the table to an immediate trace event
// when it is destroyed.
class WorkerThreadRuntimeCallStatsScope final {
 public:
  WorkerThreadRuntimeCallStatsScope(
      WorkerThreadRuntimeCallStats* off_thread_stats);
  ~WorkerThreadRuntimeCallStatsScope();

  RuntimeCallStats* Get() const { return table_; }

 private:
  RuntimeCallStats* table_;
};

#define CHANGE_CURRENT_RUNTIME_COUNTER(runtime_call_stats, counter_id) \
  do {                                                                 \
    if (V8_UNLIKELY(TracingFlags::is_runtime_stats_enabled()) &&       \
        runtime_call_stats) {                                          \
      runtime_call_stats->CorrectCurrentCounterId(counter_id);         \
    }                                                                  \
  } while (false)

#define TRACE_HANDLER_STATS(isolate, counter_name) \
  CHANGE_CURRENT_RUNTIME_COUNTER(                  \
      isolate->counters()->runtime_call_stats(),   \
      RuntimeCallCounterId::kHandler_##counter_name)

// A RuntimeCallTimerScopes wraps around a RuntimeCallTimer to measure the
// the time of C++ scope.
class RuntimeCallTimerScope {
 public:
  inline RuntimeCallTimerScope(Isolate* isolate,
                               RuntimeCallCounterId counter_id);
  // This constructor is here just to avoid calling GetIsolate() when the
  // stats are disabled and the isolate is not directly available.
  inline RuntimeCallTimerScope(Isolate* isolate, HeapObject heap_object,
                               RuntimeCallCounterId counter_id);
  inline RuntimeCallTimerScope(RuntimeCallStats* stats,
                               RuntimeCallCounterId counter_id) {
    if (V8_LIKELY(!TracingFlags::is_runtime_stats_enabled() ||
                  stats == nullptr))
      return;
    stats_ = stats;
    stats_->Enter(&timer_, counter_id);
  }

  inline ~RuntimeCallTimerScope() {
    if (V8_UNLIKELY(stats_ != nullptr)) {
      stats_->Leave(&timer_);
    }
  }

 private:
  RuntimeCallStats* stats_ = nullptr;
  RuntimeCallTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(RuntimeCallTimerScope);
};

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

  RuntimeCallStats* runtime_call_stats() { return &runtime_call_stats_; }

  WorkerThreadRuntimeCallStats* worker_thread_runtime_call_stats() {
    return &worker_thread_runtime_call_stats_;
  }

 private:
  friend class StatsTable;
  friend class StatsCounterBase;
  friend class Histogram;
  friend class HistogramTimer;

  Isolate* isolate_;
  StatsTable stats_table_;

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

  RuntimeCallStats runtime_call_stats_;
  WorkerThreadRuntimeCallStats worker_thread_runtime_call_stats_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Counters);
};

void HistogramTimer::Start() {
  TimedHistogram::Start(&timer_, counters()->isolate());
}

void HistogramTimer::Stop() {
  TimedHistogram::Stop(&timer_, counters()->isolate());
}

RuntimeCallTimerScope::RuntimeCallTimerScope(Isolate* isolate,
                                             RuntimeCallCounterId counter_id) {
  if (V8_LIKELY(!TracingFlags::is_runtime_stats_enabled())) return;
  stats_ = isolate->counters()->runtime_call_stats();
  stats_->Enter(&timer_, counter_id);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_LOGGING_COUNTERS_H_
