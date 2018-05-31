// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COUNTERS_H_
#define V8_COUNTERS_H_

#include "include/v8.h"
#include "src/allocation.h"
#include "src/base/atomic-utils.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/time.h"
#include "src/globals.h"
#include "src/heap-symbols.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/runtime/runtime.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/traced-value.h"
#include "src/tracing/tracing-category-observer.h"

namespace v8 {
namespace internal {

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
  void* CreateHistogram(const char* name,
                        int min,
                        int max,
                        size_t buckets) {
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

  StatsCounterBase() {}
  StatsCounterBase(Counters* counters, const char* name)
      : counters_(counters), name_(name), ptr_(nullptr) {}

  void SetLoc(int* loc, int value) { *loc = value; }
  void IncrementLoc(int* loc) { (*loc)++; }
  void IncrementLoc(int* loc, int value) { (*loc) += value; }
  void DecrementLoc(int* loc) { (*loc)--; }
  void DecrementLoc(int* loc, int value) { (*loc) -= value; }

  int* FindLocationInStatsTable() const;
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

  StatsCounter() {}
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
class StatsCounterThreadSafe : public StatsCounterBase {
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
  Histogram() {}
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
  void Start(base::ElapsedTimer* timer, Isolate* isolate);

  // Stop the timer and record the results. Log if isolate non-null.
  void Stop(base::ElapsedTimer* timer, Isolate* isolate);

  // Records a TimeDelta::Max() result. Useful to record percentage of tasks
  // that never got to run in a given scenario. Log if isolate non-null.
  void RecordAbandon(base::ElapsedTimer* timer, Isolate* isolate);

 protected:
  friend class Counters;
  HistogramTimerResolution resolution_;

  TimedHistogram() {}
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

  ~AsyncTimedHistogram() = default;

  AsyncTimedHistogram(const AsyncTimedHistogram& other) = default;
  AsyncTimedHistogram& operator=(const AsyncTimedHistogram& other) = default;
  AsyncTimedHistogram(AsyncTimedHistogram&& other) = default;
  AsyncTimedHistogram& operator=(AsyncTimedHistogram&& other) = default;

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
  bool Running() {
    return Enabled() && timer_.IsStarted();
  }

  // TODO(bmeurer): Remove this when HistogramTimerScope is fixed.
#ifdef DEBUG
  base::ElapsedTimer* timer() { return &timer_; }
#endif

 private:
  friend class Counters;

  base::ElapsedTimer timer_;

  HistogramTimer() {}
};

// Helper class for scoping a HistogramTimer.
// TODO(bmeurer): The ifdeffery is an ugly hack around the fact that the
// Parser is currently reentrant (when it throws an error, we call back
// into JavaScript and all bets are off), but ElapsedTimer is not
// reentry-safe. Fix this properly and remove |allow_nesting|.
class HistogramTimerScope BASE_EMBEDDED {
 public:
  explicit HistogramTimerScope(HistogramTimer* timer,
                               bool allow_nesting = false)
#ifdef DEBUG
      : timer_(timer),
        skipped_timer_start_(false) {
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

  AggregatableHistogramTimer() {}
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
  V(ArrayBuffer_Neuter)                                    \
  V(ArrayBuffer_New)                                       \
  V(Array_CloneElementAt)                                  \
  V(Array_New)                                             \
  V(BigInt64Array_New)                                     \
  V(BigUint64Array_New)                                    \
  V(BigIntObject_New)                                      \
  V(BigIntObject_BigIntValue)                              \
  V(BooleanObject_BooleanValue)                            \
  V(BooleanObject_New)                                     \
  V(Context_New)                                           \
  V(Context_NewRemoteContext)                              \
  V(DataView_New)                                          \
  V(Date_DateTimeConfigurationChangeNotification)          \
  V(Date_New)                                              \
  V(Date_NumberValue)                                      \
  V(Debug_Call)                                            \
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
  V(Promise_Resolver_Resolve)                              \
  V(Promise_Resolver_Reject)                               \
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
  V(Value_InstanceOf)                                      \
  V(Value_IntegerValue)                                    \
  V(Value_Int32Value)                                      \
  V(Value_NumberValue)                                     \
  V(Value_TypeOf)                                          \
  V(Value_Uint32Value)                                     \
  V(ValueDeserializer_ReadHeader)                          \
  V(ValueDeserializer_ReadValue)                           \
  V(ValueSerializer_WriteValue)

#define FOR_EACH_MANUAL_COUNTER(V)             \
  V(AccessorGetterCallback)                    \
  V(AccessorSetterCallback)                    \
  V(ArrayLengthGetter)                         \
  V(ArrayLengthSetter)                         \
  V(BoundFunctionNameGetter)                   \
  V(BoundFunctionLengthGetter)                 \
  V(CompileBackgroundAnalyse)                  \
  V(CompileBackgroundEval)                     \
  V(CompileBackgroundIgnition)                 \
  V(CompileBackgroundScript)                   \
  V(CompileBackgroundRewriteReturnResult)      \
  V(CompileBackgroundScopeAnalysis)            \
  V(CompileDeserialize)                        \
  V(CompileEval)                               \
  V(CompileAnalyse)                            \
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
  V(FunctionCallback)                          \
  V(FunctionPrototypeGetter)                   \
  V(FunctionPrototypeSetter)                   \
  V(FunctionLengthGetter)                      \
  V(GC_Custom_AllAvailableGarbage)             \
  V(GC_Custom_IncrementalMarkingObserver)      \
  V(GC_Custom_SlowAllocateRaw)                 \
  V(GCEpilogueCallback)                        \
  V(GCPrologueCallback)                        \
  V(GetMoreDataCallback)                       \
  V(NamedDefinerCallback)                      \
  V(NamedDeleterCallback)                      \
  V(NamedDescriptorCallback)                   \
  V(NamedQueryCallback)                        \
  V(NamedSetterCallback)                       \
  V(NamedGetterCallback)                       \
  V(NamedEnumeratorCallback)                   \
  V(IndexedDefinerCallback)                    \
  V(IndexedDeleterCallback)                    \
  V(IndexedDescriptorCallback)                 \
  V(IndexedGetterCallback)                     \
  V(IndexedQueryCallback)                      \
  V(IndexedSetterCallback)                     \
  V(IndexedEnumeratorCallback)                 \
  V(InvokeApiInterruptCallbacks)               \
  V(InvokeFunctionCallback)                    \
  V(JS_Execution)                              \
  V(Map_SetPrototype)                          \
  V(Map_TransitionToAccessorProperty)          \
  V(Map_TransitionToDataProperty)              \
  V(Object_DeleteProperty)                     \
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
  V(PreParseBackgroundNoVariableResolution)    \
  V(PreParseBackgroundWithVariableResolution)  \
  V(PreParseNoVariableResolution)              \
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
  V(KeyedLoadIC_LoadIndexedInterceptorStub)       \
  V(KeyedLoadIC_KeyedLoadSloppyArgumentsStub)     \
  V(KeyedLoadIC_LoadElementDH)                    \
  V(KeyedLoadIC_LoadIndexedStringDH)              \
  V(KeyedLoadIC_SlowStub)                         \
  V(KeyedStoreIC_ElementsTransitionAndStoreStub)  \
  V(KeyedStoreIC_KeyedStoreSloppyArgumentsStub)   \
  V(KeyedStoreIC_SlowStub)                        \
  V(KeyedStoreIC_StoreFastElementStub)            \
  V(KeyedStoreIC_StoreElementStub)                \
  V(StoreInArrayLiteralIC_SlowStub)               \
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
  V(LoadIC_LoadNonMaskingInterceptorDH)           \
  V(LoadIC_LoadInterceptorFromPrototypeDH)        \
  V(LoadIC_LoadNativeDataPropertyDH)              \
  V(LoadIC_LoadNativeDataPropertyFromPrototypeDH) \
  V(LoadIC_LoadNonexistentDH)                     \
  V(LoadIC_LoadNormalDH)                          \
  V(LoadIC_LoadNormalFromPrototypeDH)             \
  V(LoadIC_NonReceiver)                           \
  V(LoadIC_Premonomorphic)                        \
  V(LoadIC_SlowStub)                              \
  V(LoadIC_StringLength)                          \
  V(LoadIC_StringWrapperLength)                   \
  V(StoreGlobalIC_StoreScriptContextField)        \
  V(StoreGlobalIC_SlowStub)                       \
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
  V(StoreIC_StoreTransitionDH)

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

class RuntimeCallStats final : public ZoneObject {
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

#define CHANGE_CURRENT_RUNTIME_COUNTER(runtime_call_stats, counter_id) \
  do {                                                                 \
    if (V8_UNLIKELY(FLAG_runtime_stats) && runtime_call_stats) {       \
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
  inline RuntimeCallTimerScope(HeapObject* heap_object,
                               RuntimeCallCounterId counter_id);
  inline RuntimeCallTimerScope(RuntimeCallStats* stats,
                               RuntimeCallCounterId counter_id) {
    if (V8_LIKELY(!FLAG_runtime_stats || stats == nullptr)) return;
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

#define HISTOGRAM_RANGE_LIST(HR)                                               \
  /* Generic range histograms: HR(name, caption, min, max, num_buckets) */     \
  HR(background_marking, V8.GCBackgroundMarking, 0, 10000, 101)                \
  HR(background_scavenger, V8.GCBackgroundScavenger, 0, 10000, 101)            \
  HR(background_sweeping, V8.GCBackgroundSweeping, 0, 10000, 101)              \
  HR(detached_context_age_in_gc, V8.DetachedContextAgeInGC, 0, 20, 21)         \
  HR(code_cache_reject_reason, V8.CodeCacheRejectReason, 1, 6, 6)              \
  HR(errors_thrown_per_context, V8.ErrorsThrownPerContext, 0, 200, 20)         \
  HR(debug_feature_usage, V8.DebugFeatureUsage, 1, 7, 7)                       \
  HR(incremental_marking_reason, V8.GCIncrementalMarkingReason, 0, 21, 22)     \
  HR(incremental_marking_sum, V8.GCIncrementalMarkingSum, 0, 10000, 101)       \
  HR(mark_compact_reason, V8.GCMarkCompactReason, 0, 21, 22)                   \
  HR(scavenge_reason, V8.GCScavengeReason, 0, 21, 22)                          \
  HR(young_generation_handling, V8.GCYoungGenerationHandling, 0, 2, 3)         \
  /* Asm/Wasm. */                                                              \
  HR(wasm_functions_per_asm_module, V8.WasmFunctionsPerModule.asm, 1, 100000,  \
     51)                                                                       \
  HR(wasm_functions_per_wasm_module, V8.WasmFunctionsPerModule.wasm, 1,        \
     100000, 51)                                                               \
  HR(array_buffer_big_allocations, V8.ArrayBufferLargeAllocations, 0, 4096,    \
     13)                                                                       \
  HR(array_buffer_new_size_failures, V8.ArrayBufferNewSizeFailures, 0, 4096,   \
     13)                                                                       \
  HR(shared_array_allocations, V8.SharedArrayAllocationSizes, 0, 4096, 13)     \
  HR(wasm_asm_function_size_bytes, V8.WasmFunctionSizeBytes.asm, 1, GB, 51)    \
  HR(wasm_wasm_function_size_bytes, V8.WasmFunctionSizeBytes.wasm, 1, GB, 51)  \
  HR(wasm_asm_module_size_bytes, V8.WasmModuleSizeBytes.asm, 1, GB, 51)        \
  HR(wasm_wasm_module_size_bytes, V8.WasmModuleSizeBytes.wasm, 1, GB, 51)      \
  HR(wasm_asm_min_mem_pages_count, V8.WasmMinMemPagesCount.asm, 1, 2 << 16,    \
     51)                                                                       \
  HR(wasm_wasm_min_mem_pages_count, V8.WasmMinMemPagesCount.wasm, 1, 2 << 16,  \
     51)                                                                       \
  HR(wasm_wasm_max_mem_pages_count, V8.WasmMaxMemPagesCount.wasm, 1, 2 << 16,  \
     51)                                                                       \
  HR(wasm_decode_asm_module_peak_memory_bytes,                                 \
     V8.WasmDecodeModulePeakMemoryBytes.asm, 1, GB, 51)                        \
  HR(wasm_decode_wasm_module_peak_memory_bytes,                                \
     V8.WasmDecodeModulePeakMemoryBytes.wasm, 1, GB, 51)                       \
  HR(asm_wasm_translation_peak_memory_bytes,                                   \
     V8.AsmWasmTranslationPeakMemoryBytes, 1, GB, 51)                          \
  HR(wasm_compile_function_peak_memory_bytes,                                  \
     V8.WasmCompileFunctionPeakMemoryBytes, 1, GB, 51)                         \
  HR(asm_module_size_bytes, V8.AsmModuleSizeBytes, 1, GB, 51)                  \
  HR(asm_wasm_translation_throughput, V8.AsmWasmTranslationThroughput, 1, 100, \
     20)                                                                       \
  HR(wasm_lazy_compilation_throughput, V8.WasmLazyCompilationThroughput, 1,    \
     10000, 50)                                                                \
  HR(compile_script_cache_behaviour, V8.CompileScript.CacheBehaviour, 0, 19, 20)

#define HISTOGRAM_TIMER_LIST(HT)                                               \
  /* Garbage collection timers. */                                             \
  HT(gc_compactor, V8.GCCompactor, 10000, MILLISECOND)                         \
  HT(gc_compactor_background, V8.GCCompactorBackground, 10000, MILLISECOND)    \
  HT(gc_compactor_foreground, V8.GCCompactorForeground, 10000, MILLISECOND)    \
  HT(gc_finalize, V8.GCFinalizeMC, 10000, MILLISECOND)                         \
  HT(gc_finalize_background, V8.GCFinalizeMCBackground, 10000, MILLISECOND)    \
  HT(gc_finalize_foreground, V8.GCFinalizeMCForeground, 10000, MILLISECOND)    \
  HT(gc_finalize_reduce_memory, V8.GCFinalizeMCReduceMemory, 10000,            \
     MILLISECOND)                                                              \
  HT(gc_finalize_reduce_memory_background,                                     \
     V8.GCFinalizeMCReduceMemoryBackground, 10000, MILLISECOND)                \
  HT(gc_finalize_reduce_memory_foreground,                                     \
     V8.GCFinalizeMCReduceMemoryForeground, 10000, MILLISECOND)                \
  HT(gc_scavenger, V8.GCScavenger, 10000, MILLISECOND)                         \
  HT(gc_scavenger_background, V8.GCScavengerBackground, 10000, MILLISECOND)    \
  HT(gc_scavenger_foreground, V8.GCScavengerForeground, 10000, MILLISECOND)    \
  HT(gc_context, V8.GCContext, 10000,                                          \
     MILLISECOND) /* GC context cleanup time */                                \
  HT(gc_idle_notification, V8.GCIdleNotification, 10000, MILLISECOND)          \
  HT(gc_incremental_marking, V8.GCIncrementalMarking, 10000, MILLISECOND)      \
  HT(gc_incremental_marking_start, V8.GCIncrementalMarkingStart, 10000,        \
     MILLISECOND)                                                              \
  HT(gc_incremental_marking_finalize, V8.GCIncrementalMarkingFinalize, 10000,  \
     MILLISECOND)                                                              \
  HT(gc_low_memory_notification, V8.GCLowMemoryNotification, 10000,            \
     MILLISECOND)                                                              \
  /* Compilation times. */                                                     \
  HT(compile, V8.CompileMicroSeconds, 1000000, MICROSECOND)                    \
  HT(compile_eval, V8.CompileEvalMicroSeconds, 1000000, MICROSECOND)           \
  /* Serialization as part of compilation (code caching) */                    \
  HT(compile_serialize, V8.CompileSerializeMicroSeconds, 100000, MICROSECOND)  \
  HT(compile_deserialize, V8.CompileDeserializeMicroSeconds, 1000000,          \
     MICROSECOND)                                                              \
  /* Total compilation time incl. caching/parsing */                           \
  HT(compile_script, V8.CompileScriptMicroSeconds, 1000000, MICROSECOND)       \
  /* Total JavaScript execution time (including callbacks and runtime calls */ \
  HT(execute, V8.Execute, 1000000, MICROSECOND)                                \
  /* Asm/Wasm */                                                               \
  HT(asm_wasm_translation_time, V8.AsmWasmTranslationMicroSeconds, 1000000,    \
     MICROSECOND)                                                              \
  HT(wasm_lazy_compilation_time, V8.WasmLazyCompilationMicroSeconds, 1000000,  \
     MICROSECOND)                                                              \
  HT(wasm_execution_time, V8.WasmExecutionTimeMicroSeconds, 10000000,          \
     MICROSECOND)

#define TIMED_HISTOGRAM_LIST(HT)                                               \
  HT(wasm_decode_asm_module_time, V8.WasmDecodeModuleMicroSeconds.asm,         \
     1000000, MICROSECOND)                                                     \
  HT(wasm_decode_wasm_module_time, V8.WasmDecodeModuleMicroSeconds.wasm,       \
     1000000, MICROSECOND)                                                     \
  HT(wasm_decode_asm_function_time, V8.WasmDecodeFunctionMicroSeconds.asm,     \
     1000000, MICROSECOND)                                                     \
  HT(wasm_decode_wasm_function_time, V8.WasmDecodeFunctionMicroSeconds.wasm,   \
     1000000, MICROSECOND)                                                     \
  HT(wasm_compile_asm_module_time, V8.WasmCompileModuleMicroSeconds.asm,       \
     10000000, MICROSECOND)                                                    \
  HT(wasm_compile_wasm_module_time, V8.WasmCompileModuleMicroSeconds.wasm,     \
     10000000, MICROSECOND)                                                    \
  HT(wasm_compile_asm_function_time, V8.WasmCompileFunctionMicroSeconds.asm,   \
     1000000, MICROSECOND)                                                     \
  HT(wasm_compile_wasm_function_time, V8.WasmCompileFunctionMicroSeconds.wasm, \
     1000000, MICROSECOND)                                                     \
  HT(liftoff_compile_time, V8.LiftoffCompileMicroSeconds, 10000000,            \
     MICROSECOND)                                                              \
  HT(wasm_instantiate_wasm_module_time,                                        \
     V8.WasmInstantiateModuleMicroSeconds.wasm, 10000000, MICROSECOND)         \
  HT(wasm_instantiate_asm_module_time,                                         \
     V8.WasmInstantiateModuleMicroSeconds.asm, 10000000, MICROSECOND)          \
  /* Total compilation time incl. caching/parsing for various cache states. */ \
  HT(compile_script_with_produce_cache,                                        \
     V8.CompileScriptMicroSeconds.ProduceCache, 1000000, MICROSECOND)          \
  HT(compile_script_with_isolate_cache_hit,                                    \
     V8.CompileScriptMicroSeconds.IsolateCacheHit, 1000000, MICROSECOND)       \
  HT(compile_script_with_consume_cache,                                        \
     V8.CompileScriptMicroSeconds.ConsumeCache, 1000000, MICROSECOND)          \
  HT(compile_script_consume_failed,                                            \
     V8.CompileScriptMicroSeconds.ConsumeCache.Failed, 1000000, MICROSECOND)   \
  HT(compile_script_no_cache_other,                                            \
     V8.CompileScriptMicroSeconds.NoCache.Other, 1000000, MICROSECOND)         \
  HT(compile_script_no_cache_because_inline_script,                            \
     V8.CompileScriptMicroSeconds.NoCache.InlineScript, 1000000, MICROSECOND)  \
  HT(compile_script_no_cache_because_script_too_small,                         \
     V8.CompileScriptMicroSeconds.NoCache.ScriptTooSmall, 1000000,             \
     MICROSECOND)                                                              \
  HT(compile_script_no_cache_because_cache_too_cold,                           \
     V8.CompileScriptMicroSeconds.NoCache.CacheTooCold, 1000000, MICROSECOND)  \
  HT(compile_script_on_background,                                             \
     V8.CompileScriptMicroSeconds.BackgroundThread, 1000000, MICROSECOND)      \
  HT(gc_parallel_task_latency, V8.GC.ParallelTaskLatencyMicroSeconds, 1000000, \
     MICROSECOND)

#define AGGREGATABLE_HISTOGRAM_TIMER_LIST(AHT) \
  AHT(compile_lazy, V8.CompileLazyMicroSeconds)

#define HISTOGRAM_PERCENTAGE_LIST(HP)                                          \
  /* Heap fragmentation. */                                                    \
  HP(external_fragmentation_total, V8.MemoryExternalFragmentationTotal)        \
  HP(external_fragmentation_old_space, V8.MemoryExternalFragmentationOldSpace) \
  HP(external_fragmentation_code_space,                                        \
     V8.MemoryExternalFragmentationCodeSpace)                                  \
  HP(external_fragmentation_map_space, V8.MemoryExternalFragmentationMapSpace) \
  HP(external_fragmentation_lo_space, V8.MemoryExternalFragmentationLoSpace)

// Note: These use Histogram with options (min=1000, max=500000, buckets=50).
#define HISTOGRAM_LEGACY_MEMORY_LIST(HM)                                      \
  HM(heap_sample_total_committed, V8.MemoryHeapSampleTotalCommitted)          \
  HM(heap_sample_total_used, V8.MemoryHeapSampleTotalUsed)                    \
  HM(heap_sample_map_space_committed, V8.MemoryHeapSampleMapSpaceCommitted)   \
  HM(heap_sample_code_space_committed, V8.MemoryHeapSampleCodeSpaceCommitted) \
  HM(heap_sample_maximum_committed, V8.MemoryHeapSampleMaximumCommitted)

// Note: These define both Histogram and AggregatedMemoryHistogram<Histogram>
// histograms with options (min=4000, max=2000000, buckets=100).
#define HISTOGRAM_MEMORY_LIST(HM)                   \
  HM(memory_heap_committed, V8.MemoryHeapCommitted) \
  HM(memory_heap_used, V8.MemoryHeapUsed)

// WARNING: STATS_COUNTER_LIST_* is a very large macro that is causing MSVC
// Intellisense to crash.  It was broken into two macros (each of length 40
// lines) rather than one macro (of length about 80 lines) to work around
// this problem.  Please avoid using recursive macros of this length when
// possible.
#define STATS_COUNTER_LIST_1(SC)                                    \
  /* Global Handle Count*/                                          \
  SC(global_handles, V8.GlobalHandles)                              \
  /* OS Memory allocated */                                         \
  SC(memory_allocated, V8.OsMemoryAllocated)                        \
  SC(maps_normalized, V8.MapsNormalized)                            \
  SC(maps_created, V8.MapsCreated)                                  \
  SC(elements_transitions, V8.ObjectElementsTransitions)            \
  SC(props_to_dictionary, V8.ObjectPropertiesToDictionary)          \
  SC(elements_to_dictionary, V8.ObjectElementsToDictionary)         \
  SC(alive_after_last_gc, V8.AliveAfterLastGC)                      \
  SC(objs_since_last_young, V8.ObjsSinceLastYoung)                  \
  SC(objs_since_last_full, V8.ObjsSinceLastFull)                    \
  SC(string_table_capacity, V8.StringTableCapacity)                 \
  SC(number_of_symbols, V8.NumberOfSymbols)                         \
  SC(script_wrappers, V8.ScriptWrappers)                            \
  SC(inlined_copied_elements, V8.InlinedCopiedElements)             \
  SC(arguments_adaptors, V8.ArgumentsAdaptors)                      \
  SC(compilation_cache_hits, V8.CompilationCacheHits)               \
  SC(compilation_cache_misses, V8.CompilationCacheMisses)           \
  /* Amount of evaled source code. */                               \
  SC(total_eval_size, V8.TotalEvalSize)                             \
  /* Amount of loaded source code. */                               \
  SC(total_load_size, V8.TotalLoadSize)                             \
  /* Amount of parsed source code. */                               \
  SC(total_parse_size, V8.TotalParseSize)                           \
  /* Amount of source code skipped over using preparsing. */        \
  SC(total_preparse_skipped, V8.TotalPreparseSkipped)               \
  /* Amount of compiled source code. */                             \
  SC(total_compile_size, V8.TotalCompileSize)                       \
  /* Amount of source code compiled with the full codegen. */       \
  SC(total_full_codegen_source_size, V8.TotalFullCodegenSourceSize) \
  /* Number of contexts created from scratch. */                    \
  SC(contexts_created_from_scratch, V8.ContextsCreatedFromScratch)  \
  /* Number of contexts created by partial snapshot. */             \
  SC(contexts_created_by_snapshot, V8.ContextsCreatedBySnapshot)    \
  /* Number of code objects found from pc. */                       \
  SC(pc_to_code, V8.PcToCode)                                       \
  SC(pc_to_code_cached, V8.PcToCodeCached)                          \
  /* The store-buffer implementation of the write barrier. */       \
  SC(store_buffer_overflows, V8.StoreBufferOverflows)

#define STATS_COUNTER_LIST_2(SC)                                               \
  /* Number of code stubs. */                                                  \
  SC(code_stubs, V8.CodeStubs)                                                 \
  /* Amount of stub code. */                                                   \
  SC(total_stubs_code_size, V8.TotalStubsCodeSize)                             \
  /* Amount of (JS) compiled code. */                                          \
  SC(total_compiled_code_size, V8.TotalCompiledCodeSize)                       \
  SC(gc_compactor_caused_by_request, V8.GCCompactorCausedByRequest)            \
  SC(gc_compactor_caused_by_promoted_data, V8.GCCompactorCausedByPromotedData) \
  SC(gc_compactor_caused_by_oldspace_exhaustion,                               \
     V8.GCCompactorCausedByOldspaceExhaustion)                                 \
  SC(gc_last_resort_from_js, V8.GCLastResortFromJS)                            \
  SC(gc_last_resort_from_handles, V8.GCLastResortFromHandles)                  \
  SC(ic_keyed_load_generic_smi, V8.ICKeyedLoadGenericSmi)                      \
  SC(ic_keyed_load_generic_symbol, V8.ICKeyedLoadGenericSymbol)                \
  SC(ic_keyed_load_generic_slow, V8.ICKeyedLoadGenericSlow)                    \
  SC(ic_named_load_global_stub, V8.ICNamedLoadGlobalStub)                      \
  SC(ic_store_normal_miss, V8.ICStoreNormalMiss)                               \
  SC(ic_store_normal_hit, V8.ICStoreNormalHit)                                 \
  SC(ic_binary_op_miss, V8.ICBinaryOpMiss)                                     \
  SC(ic_compare_miss, V8.ICCompareMiss)                                        \
  SC(ic_call_miss, V8.ICCallMiss)                                              \
  SC(ic_keyed_call_miss, V8.ICKeyedCallMiss)                                   \
  SC(ic_store_miss, V8.ICStoreMiss)                                            \
  SC(ic_keyed_store_miss, V8.ICKeyedStoreMiss)                                 \
  SC(cow_arrays_converted, V8.COWArraysConverted)                              \
  SC(constructed_objects, V8.ConstructedObjects)                               \
  SC(constructed_objects_runtime, V8.ConstructedObjectsRuntime)                \
  SC(megamorphic_stub_cache_probes, V8.MegamorphicStubCacheProbes)             \
  SC(megamorphic_stub_cache_misses, V8.MegamorphicStubCacheMisses)             \
  SC(megamorphic_stub_cache_updates, V8.MegamorphicStubCacheUpdates)           \
  SC(enum_cache_hits, V8.EnumCacheHits)                                        \
  SC(enum_cache_misses, V8.EnumCacheMisses)                                    \
  SC(fast_new_closure_total, V8.FastNewClosureTotal)                           \
  SC(string_add_runtime, V8.StringAddRuntime)                                  \
  SC(string_add_native, V8.StringAddNative)                                    \
  SC(string_add_runtime_ext_to_one_byte, V8.StringAddRuntimeExtToOneByte)      \
  SC(sub_string_runtime, V8.SubStringRuntime)                                  \
  SC(sub_string_native, V8.SubStringNative)                                    \
  SC(regexp_entry_runtime, V8.RegExpEntryRuntime)                              \
  SC(regexp_entry_native, V8.RegExpEntryNative)                                \
  SC(number_to_string_native, V8.NumberToStringNative)                         \
  SC(number_to_string_runtime, V8.NumberToStringRuntime)                       \
  SC(math_exp_runtime, V8.MathExpRuntime)                                      \
  SC(math_log_runtime, V8.MathLogRuntime)                                      \
  SC(math_pow_runtime, V8.MathPowRuntime)                                      \
  SC(stack_interrupts, V8.StackInterrupts)                                     \
  SC(runtime_profiler_ticks, V8.RuntimeProfilerTicks)                          \
  SC(runtime_calls, V8.RuntimeCalls)                                           \
  SC(bounds_checks_eliminated, V8.BoundsChecksEliminated)                      \
  SC(bounds_checks_hoisted, V8.BoundsChecksHoisted)                            \
  SC(soft_deopts_requested, V8.SoftDeoptsRequested)                            \
  SC(soft_deopts_inserted, V8.SoftDeoptsInserted)                              \
  SC(soft_deopts_executed, V8.SoftDeoptsExecuted)                              \
  /* Number of write barriers in generated code. */                            \
  SC(write_barriers_dynamic, V8.WriteBarriersDynamic)                          \
  SC(write_barriers_static, V8.WriteBarriersStatic)                            \
  SC(new_space_bytes_available, V8.MemoryNewSpaceBytesAvailable)               \
  SC(new_space_bytes_committed, V8.MemoryNewSpaceBytesCommitted)               \
  SC(new_space_bytes_used, V8.MemoryNewSpaceBytesUsed)                         \
  SC(old_space_bytes_available, V8.MemoryOldSpaceBytesAvailable)               \
  SC(old_space_bytes_committed, V8.MemoryOldSpaceBytesCommitted)               \
  SC(old_space_bytes_used, V8.MemoryOldSpaceBytesUsed)                         \
  SC(code_space_bytes_available, V8.MemoryCodeSpaceBytesAvailable)             \
  SC(code_space_bytes_committed, V8.MemoryCodeSpaceBytesCommitted)             \
  SC(code_space_bytes_used, V8.MemoryCodeSpaceBytesUsed)                       \
  SC(map_space_bytes_available, V8.MemoryMapSpaceBytesAvailable)               \
  SC(map_space_bytes_committed, V8.MemoryMapSpaceBytesCommitted)               \
  SC(map_space_bytes_used, V8.MemoryMapSpaceBytesUsed)                         \
  SC(lo_space_bytes_available, V8.MemoryLoSpaceBytesAvailable)                 \
  SC(lo_space_bytes_committed, V8.MemoryLoSpaceBytesCommitted)                 \
  SC(lo_space_bytes_used, V8.MemoryLoSpaceBytesUsed)                           \
  /* Total code size (including metadata) of baseline code or bytecode. */     \
  SC(total_baseline_code_size, V8.TotalBaselineCodeSize)                       \
  /* Total count of functions compiled using the baseline compiler. */         \
  SC(total_baseline_compile_count, V8.TotalBaselineCompileCount)

#define STATS_COUNTER_TS_LIST(SC)                                    \
  SC(wasm_generated_code_size, V8.WasmGeneratedCodeBytes)            \
  SC(wasm_reloc_size, V8.WasmRelocBytes)                             \
  SC(wasm_lazily_compiled_functions, V8.WasmLazilyCompiledFunctions) \
  SC(liftoff_compiled_functions, V8.LiftoffCompiledFunctions)        \
  SC(liftoff_unsupported_functions, V8.LiftoffUnsupportedFunctions)

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
  HISTOGRAM_MEMORY_LIST(HM)
#undef HM

#define HM(name, caption)                                     \
  AggregatedMemoryHistogram<Histogram>* aggregated_##name() { \
    return &aggregated_##name##_;                             \
  }
  HISTOGRAM_MEMORY_LIST(HM)
#undef HM

#define SC(name, caption) \
  StatsCounter* name() { return &name##_; }
  STATS_COUNTER_LIST_1(SC)
  STATS_COUNTER_LIST_2(SC)
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
    HISTOGRAM_MEMORY_LIST(MEMORY_ID)
#undef MEMORY_ID
#define COUNTER_ID(name, caption) k_##name,
    STATS_COUNTER_LIST_1(COUNTER_ID)
    STATS_COUNTER_LIST_2(COUNTER_ID)
    STATS_COUNTER_TS_LIST(COUNTER_ID)
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

#define AHT(name, caption) \
  AggregatableHistogramTimer name##_;
  AGGREGATABLE_HISTOGRAM_TIMER_LIST(AHT)
#undef AHT

#define HP(name, caption) \
  Histogram name##_;
  HISTOGRAM_PERCENTAGE_LIST(HP)
#undef HP

#define HM(name, caption) \
  Histogram name##_;
  HISTOGRAM_LEGACY_MEMORY_LIST(HM)
  HISTOGRAM_MEMORY_LIST(HM)
#undef HM

#define HM(name, caption) \
  AggregatedMemoryHistogram<Histogram> aggregated_##name##_;
  HISTOGRAM_MEMORY_LIST(HM)
#undef HM

#define SC(name, caption) \
  StatsCounter name##_;
  STATS_COUNTER_LIST_1(SC)
  STATS_COUNTER_LIST_2(SC)
#undef SC

#define SC(name, caption) StatsCounterThreadSafe name##_;
  STATS_COUNTER_TS_LIST(SC)
#undef SC

#define SC(name) \
  StatsCounter size_of_##name##_; \
  StatsCounter count_of_##name##_;
  INSTANCE_TYPE_LIST(SC)
#undef SC

#define SC(name) \
  StatsCounter size_of_CODE_TYPE_##name##_; \
  StatsCounter count_of_CODE_TYPE_##name##_;
  CODE_KIND_LIST(SC)
#undef SC

#define SC(name) \
  StatsCounter size_of_FIXED_ARRAY_##name##_; \
  StatsCounter count_of_FIXED_ARRAY_##name##_;
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(SC)
#undef SC

  RuntimeCallStats runtime_call_stats_;

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
  if (V8_LIKELY(!FLAG_runtime_stats)) return;
  stats_ = isolate->counters()->runtime_call_stats();
  stats_->Enter(&timer_, counter_id);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COUNTERS_H_
