// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_COUNTERS_H_
#define V8_COUNTERS_H_

#include "../include/v8.h"
#include "allocation.h"

namespace v8 {
namespace internal {

// StatsCounters is an interface for plugging into external
// counters for monitoring.  Counters can be looked up and
// manipulated by name.

class StatsTable {
 public:
  // Register an application-defined function where
  // counters can be looked up.
  void SetCounterFunction(CounterLookupCallback f) {
    lookup_function_ = f;
  }

  // Register an application-defined function to create
  // a histogram for passing to the AddHistogramSample function
  void SetCreateHistogramFunction(CreateHistogramCallback f) {
    create_histogram_function_ = f;
  }

  // Register an application-defined function to add a sample
  // to a histogram created with CreateHistogram function
  void SetAddHistogramSampleFunction(AddHistogramSampleCallback f) {
    add_histogram_sample_function_ = f;
  }

  bool HasCounterFunction() const {
    return lookup_function_ != NULL;
  }

  // Lookup the location of a counter by name.  If the lookup
  // is successful, returns a non-NULL pointer for writing the
  // value of the counter.  Each thread calling this function
  // may receive a different location to store it's counter.
  // The return value must not be cached and re-used across
  // threads, although a single thread is free to cache it.
  int* FindLocation(const char* name) {
    if (!lookup_function_) return NULL;
    return lookup_function_(name);
  }

  // Create a histogram by name. If the create is successful,
  // returns a non-NULL pointer for use with AddHistogramSample
  // function. min and max define the expected minimum and maximum
  // sample values. buckets is the maximum number of buckets
  // that the samples will be grouped into.
  void* CreateHistogram(const char* name,
                        int min,
                        int max,
                        size_t buckets) {
    if (!create_histogram_function_) return NULL;
    return create_histogram_function_(name, min, max, buckets);
  }

  // Add a sample to a histogram created with the CreateHistogram
  // function.
  void AddHistogramSample(void* histogram, int sample) {
    if (!add_histogram_sample_function_) return;
    return add_histogram_sample_function_(histogram, sample);
  }

 private:
  StatsTable();

  CounterLookupCallback lookup_function_;
  CreateHistogramCallback create_histogram_function_;
  AddHistogramSampleCallback add_histogram_sample_function_;

  friend class Isolate;

  DISALLOW_COPY_AND_ASSIGN(StatsTable);
};

// StatsCounters are dynamically created values which can be tracked in
// the StatsTable.  They are designed to be lightweight to create and
// easy to use.
//
// Internally, a counter represents a value in a row of a StatsTable.
// The row has a 32bit value for each process/thread in the table and also
// a name (stored in the table metadata).  Since the storage location can be
// thread-specific, this class cannot be shared across threads.
class StatsCounter {
 public:
  StatsCounter() { }
  explicit StatsCounter(Isolate* isolate, const char* name)
      : isolate_(isolate), name_(name), ptr_(NULL), lookup_done_(false) { }

  // Sets the counter to a specific value.
  void Set(int value) {
    int* loc = GetPtr();
    if (loc) *loc = value;
  }

  // Increments the counter.
  void Increment() {
    int* loc = GetPtr();
    if (loc) (*loc)++;
  }

  void Increment(int value) {
    int* loc = GetPtr();
    if (loc)
      (*loc) += value;
  }

  // Decrements the counter.
  void Decrement() {
    int* loc = GetPtr();
    if (loc) (*loc)--;
  }

  void Decrement(int value) {
    int* loc = GetPtr();
    if (loc) (*loc) -= value;
  }

  // Is this counter enabled?
  // Returns false if table is full.
  bool Enabled() {
    return GetPtr() != NULL;
  }

  // Get the internal pointer to the counter. This is used
  // by the code generator to emit code that manipulates a
  // given counter without calling the runtime system.
  int* GetInternalPointer() {
    int* loc = GetPtr();
    ASSERT(loc != NULL);
    return loc;
  }

 protected:
  // Returns the cached address of this counter location.
  int* GetPtr() {
    if (lookup_done_) return ptr_;
    lookup_done_ = true;
    ptr_ = FindLocationInStatsTable();
    return ptr_;
  }

 private:
  int* FindLocationInStatsTable() const;

  Isolate* isolate_;
  const char* name_;
  int* ptr_;
  bool lookup_done_;
};

// A Histogram represents a dynamically created histogram in the StatsTable.
// It will be registered with the histogram system on first use.
class Histogram {
 public:
  Histogram() { }
  Histogram(const char* name,
            int min,
            int max,
            int num_buckets,
            Isolate* isolate)
      : name_(name),
        min_(min),
        max_(max),
        num_buckets_(num_buckets),
        histogram_(NULL),
        lookup_done_(false),
        isolate_(isolate) { }

  // Add a single sample to this histogram.
  void AddSample(int sample);

  // Returns true if this histogram is enabled.
  bool Enabled() {
    return GetHistogram() != NULL;
  }

  // Reset the cached internal pointer.
  void Reset() {
    lookup_done_ = false;
  }

 protected:
  // Returns the handle to the histogram.
  void* GetHistogram() {
    if (!lookup_done_) {
      lookup_done_ = true;
      histogram_ = CreateHistogram();
    }
    return histogram_;
  }

  const char* name() { return name_; }
  Isolate* isolate() const { return isolate_; }

 private:
  void* CreateHistogram() const;

  const char* name_;
  int min_;
  int max_;
  int num_buckets_;
  void* histogram_;
  bool lookup_done_;
  Isolate* isolate_;
};

// A HistogramTimer allows distributions of results to be created.
class HistogramTimer : public Histogram {
 public:
  HistogramTimer() { }
  HistogramTimer(const char* name,
                 int min,
                 int max,
                 int num_buckets,
                 Isolate* isolate)
      : Histogram(name, min, max, num_buckets, isolate) {}

  // Start the timer.
  void Start();

  // Stop the timer and record the results.
  void Stop();

  // Returns true if the timer is running.
  bool Running() {
    return Enabled() && timer_.IsStarted();
  }

 private:
  ElapsedTimer timer_;
};

// Helper class for scoping a HistogramTimer.
class HistogramTimerScope BASE_EMBEDDED {
 public:
  explicit HistogramTimerScope(HistogramTimer* timer) :
  timer_(timer) {
    timer_->Start();
  }
  ~HistogramTimerScope() {
    timer_->Stop();
  }
 private:
  HistogramTimer* timer_;
};


} }  // namespace v8::internal

#endif  // V8_COUNTERS_H_
