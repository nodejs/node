// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/logging/counters.h"

#include "src/base/atomic-utils.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/time.h"
#include "src/builtins/builtins-definitions.h"
#include "src/execution/isolate.h"
#include "src/logging/log-inl.h"
#include "src/logging/log.h"

namespace v8 {
namespace internal {

StatsTable::StatsTable(Counters* counters)
    : lookup_function_(nullptr),
      create_histogram_function_(nullptr),
      add_histogram_sample_function_(nullptr) {}

void StatsTable::SetCounterFunction(CounterLookupCallback f) {
  lookup_function_ = f;
}

int* StatsCounterBase::FindLocationInStatsTable() const {
  return counters_->FindLocation(name_);
}

StatsCounterThreadSafe::StatsCounterThreadSafe(Counters* counters,
                                               const char* name)
    : StatsCounterBase(counters, name) {}

void StatsCounterThreadSafe::Set(int Value) {
  if (ptr_) {
    base::MutexGuard Guard(&mutex_);
    SetLoc(ptr_, Value);
  }
}

void StatsCounterThreadSafe::Increment() {
  if (ptr_) {
    base::MutexGuard Guard(&mutex_);
    IncrementLoc(ptr_);
  }
}

void StatsCounterThreadSafe::Increment(int value) {
  if (ptr_) {
    base::MutexGuard Guard(&mutex_);
    IncrementLoc(ptr_, value);
  }
}

void StatsCounterThreadSafe::Decrement() {
  if (ptr_) {
    base::MutexGuard Guard(&mutex_);
    DecrementLoc(ptr_);
  }
}

void StatsCounterThreadSafe::Decrement(int value) {
  if (ptr_) {
    base::MutexGuard Guard(&mutex_);
    DecrementLoc(ptr_, value);
  }
}

void Histogram::AddSample(int sample) {
  if (Enabled()) {
    counters_->AddHistogramSample(histogram_, sample);
  }
}

void* Histogram::CreateHistogram() const {
  return counters_->CreateHistogram(name_, min_, max_, num_buckets_);
}

void TimedHistogram::AddTimedSample(base::TimeDelta sample) {
  if (Enabled()) {
    int64_t sample_int = resolution_ == HistogramTimerResolution::MICROSECOND
                             ? sample.InMicroseconds()
                             : sample.InMilliseconds();
    AddSample(static_cast<int>(sample_int));
  }
}

void TimedHistogram::Start(base::ElapsedTimer* timer, Isolate* isolate) {
  if (Enabled()) timer->Start();
  if (isolate) Logger::CallEventLogger(isolate, name(), Logger::START, true);
}

void TimedHistogram::Stop(base::ElapsedTimer* timer, Isolate* isolate) {
  if (Enabled()) {
    base::TimeDelta delta = timer->Elapsed();
    timer->Stop();
    AddTimedSample(delta);
  }
  if (isolate != nullptr) {
    Logger::CallEventLogger(isolate, name(), Logger::END, true);
  }
}

void TimedHistogram::RecordAbandon(base::ElapsedTimer* timer,
                                   Isolate* isolate) {
  if (Enabled()) {
    DCHECK(timer->IsStarted());
    timer->Stop();
    int64_t sample = resolution_ == HistogramTimerResolution::MICROSECOND
                         ? base::TimeDelta::Max().InMicroseconds()
                         : base::TimeDelta::Max().InMilliseconds();
    AddSample(static_cast<int>(sample));
  }
  if (isolate != nullptr) {
    Logger::CallEventLogger(isolate, name(), Logger::END, true);
  }
}

Counters::Counters(Isolate* isolate)
    :
#define SC(name, caption) name##_(this, "c:" #caption),
      STATS_COUNTER_TS_LIST(SC)
#undef SC
#ifdef V8_RUNTIME_CALL_STATS
          runtime_call_stats_(RuntimeCallStats::kMainIsolateThread),
      worker_thread_runtime_call_stats_(),
#endif
      isolate_(isolate),
      stats_table_(this) {
  static const struct {
    Histogram Counters::*member;
    const char* caption;
    int min;
    int max;
    int num_buckets;
  } kHistograms[] = {
#define HR(name, caption, min, max, num_buckets) \
  {&Counters::name##_, #caption, min, max, num_buckets},
      HISTOGRAM_RANGE_LIST(HR)
#undef HR
  };
  for (const auto& histogram : kHistograms) {
    this->*histogram.member =
        Histogram(histogram.caption, histogram.min, histogram.max,
                  histogram.num_buckets, this);
  }

  const int DefaultTimedHistogramNumBuckets = 50;

  static const struct {
    HistogramTimer Counters::*member;
    const char* caption;
    int max;
    HistogramTimerResolution res;
  } kHistogramTimers[] = {
#define HT(name, caption, max, res) \
  {&Counters::name##_, #caption, max, HistogramTimerResolution::res},
      HISTOGRAM_TIMER_LIST(HT)
#undef HT
  };
  for (const auto& timer : kHistogramTimers) {
    this->*timer.member = HistogramTimer(timer.caption, 0, timer.max, timer.res,
                                         DefaultTimedHistogramNumBuckets, this);
  }

  static const struct {
    TimedHistogram Counters::*member;
    const char* caption;
    int max;
    HistogramTimerResolution res;
  } kTimedHistograms[] = {
#define HT(name, caption, max, res) \
  {&Counters::name##_, #caption, max, HistogramTimerResolution::res},
      TIMED_HISTOGRAM_LIST(HT)
#undef HT
  };
  for (const auto& timer : kTimedHistograms) {
    this->*timer.member = TimedHistogram(timer.caption, 0, timer.max, timer.res,
                                         DefaultTimedHistogramNumBuckets, this);
  }

  static const struct {
    AggregatableHistogramTimer Counters::*member;
    const char* caption;
  } kAggregatableHistogramTimers[] = {
#define AHT(name, caption) {&Counters::name##_, #caption},
      AGGREGATABLE_HISTOGRAM_TIMER_LIST(AHT)
#undef AHT
  };
  for (const auto& aht : kAggregatableHistogramTimers) {
    this->*aht.member = AggregatableHistogramTimer(
        aht.caption, 0, 10000000, DefaultTimedHistogramNumBuckets, this);
  }

  static const struct {
    Histogram Counters::*member;
    const char* caption;
  } kHistogramPercentages[] = {
#define HP(name, caption) {&Counters::name##_, #caption},
      HISTOGRAM_PERCENTAGE_LIST(HP)
#undef HP
  };
  for (const auto& percentage : kHistogramPercentages) {
    this->*percentage.member = Histogram(percentage.caption, 0, 101, 100, this);
  }

  // Exponential histogram assigns bucket limits to points
  // p[1], p[2], ... p[n] such that p[i+1] / p[i] = constant.
  // The constant factor is equal to the n-th root of (high / low),
  // where the n is the number of buckets, the low is the lower limit,
  // the high is the upper limit.
  // For n = 50, low = 1000, high = 500000: the factor = 1.13.
  static const struct {
    Histogram Counters::*member;
    const char* caption;
  } kLegacyMemoryHistograms[] = {
#define HM(name, caption) {&Counters::name##_, #caption},
      HISTOGRAM_LEGACY_MEMORY_LIST(HM)
#undef HM
  };
  for (const auto& histogram : kLegacyMemoryHistograms) {
    this->*histogram.member =
        Histogram(histogram.caption, 1000, 500000, 50, this);
  }

  // clang-format off
  static const struct {
    StatsCounter Counters::*member;
    const char* caption;
  } kStatsCounters[] = {
#define SC(name, caption) {&Counters::name##_, "c:" #caption},
  STATS_COUNTER_LIST_1(SC)
  STATS_COUNTER_LIST_2(SC)
  STATS_COUNTER_NATIVE_CODE_LIST(SC)
#undef SC
#define SC(name)                                             \
  {&Counters::count_of_##name##_, "c:" "V8.CountOf_" #name}, \
  {&Counters::size_of_##name##_, "c:" "V8.SizeOf_" #name},
      INSTANCE_TYPE_LIST(SC)
#undef SC
#define SC(name)                            \
  {&Counters::count_of_CODE_TYPE_##name##_, \
    "c:" "V8.CountOf_CODE_TYPE-" #name},     \
  {&Counters::size_of_CODE_TYPE_##name##_,  \
    "c:" "V8.SizeOf_CODE_TYPE-" #name},
      CODE_KIND_LIST(SC)
#undef SC
#define SC(name)                              \
  {&Counters::count_of_FIXED_ARRAY_##name##_, \
    "c:" "V8.CountOf_FIXED_ARRAY-" #name},     \
  {&Counters::size_of_FIXED_ARRAY_##name##_,  \
    "c:" "V8.SizeOf_FIXED_ARRAY-" #name},
      FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(SC)
#undef SC
  };
  // clang-format on
  for (const auto& counter : kStatsCounters) {
    this->*counter.member = StatsCounter(this, counter.caption);
  }
}

void Counters::ResetCounterFunction(CounterLookupCallback f) {
  stats_table_.SetCounterFunction(f);

#define SC(name, caption) name##_.Reset();
  STATS_COUNTER_LIST_1(SC)
  STATS_COUNTER_LIST_2(SC)
  STATS_COUNTER_TS_LIST(SC)
  STATS_COUNTER_NATIVE_CODE_LIST(SC)
#undef SC

#define SC(name)              \
  count_of_##name##_.Reset(); \
  size_of_##name##_.Reset();
  INSTANCE_TYPE_LIST(SC)
#undef SC

#define SC(name)                        \
  count_of_CODE_TYPE_##name##_.Reset(); \
  size_of_CODE_TYPE_##name##_.Reset();
  CODE_KIND_LIST(SC)
#undef SC

#define SC(name)                          \
  count_of_FIXED_ARRAY_##name##_.Reset(); \
  size_of_FIXED_ARRAY_##name##_.Reset();
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(SC)
#undef SC
}

void Counters::ResetCreateHistogramFunction(CreateHistogramCallback f) {
  stats_table_.SetCreateHistogramFunction(f);

#define HR(name, caption, min, max, num_buckets) name##_.Reset();
  HISTOGRAM_RANGE_LIST(HR)
#undef HR

#define HT(name, caption, max, res) name##_.Reset();
  HISTOGRAM_TIMER_LIST(HT)
#undef HT

#define HT(name, caption, max, res) name##_.Reset();
  TIMED_HISTOGRAM_LIST(HT)
#undef HT

#define AHT(name, caption) name##_.Reset();
  AGGREGATABLE_HISTOGRAM_TIMER_LIST(AHT)
#undef AHT

#define HP(name, caption) name##_.Reset();
  HISTOGRAM_PERCENTAGE_LIST(HP)
#undef HP

#define HM(name, caption) name##_.Reset();
  HISTOGRAM_LEGACY_MEMORY_LIST(HM)
#undef HM
}

}  // namespace internal
}  // namespace v8
