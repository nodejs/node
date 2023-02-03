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

namespace {
std::atomic<int> unused_counter_dump{0};
}

bool StatsCounter::Enabled() { return GetPtr() != &unused_counter_dump; }

std::atomic<int>* StatsCounter::SetupPtrFromStatsTable() {
  // {Init} must have been called.
  DCHECK_NOT_NULL(counters_);
  DCHECK_NOT_NULL(name_);
  int* location = counters_->FindLocation(name_);
  std::atomic<int>* ptr =
      location ? base::AsAtomicPtr(location) : &unused_counter_dump;
#ifdef DEBUG
  std::atomic<int>* old_ptr = ptr_.exchange(ptr, std::memory_order_release);
  DCHECK_IMPLIES(old_ptr, old_ptr == ptr);
#else
  ptr_.store(ptr, std::memory_order_release);
#endif
  return ptr;
}

void Histogram::AddSample(int sample) {
  if (Enabled()) {
    counters_->AddHistogramSample(histogram_, sample);
  }
}

void* Histogram::CreateHistogram() const {
  return counters_->CreateHistogram(name_, min_, max_, num_buckets_);
}

void TimedHistogram::Stop(base::ElapsedTimer* timer) {
  DCHECK(Enabled());
  AddTimedSample(timer->Elapsed());
  timer->Stop();
}

void TimedHistogram::AddTimedSample(base::TimeDelta sample) {
  if (Enabled()) {
    int64_t sample_int = resolution_ == TimedHistogramResolution::MICROSECOND
                             ? sample.InMicroseconds()
                             : sample.InMilliseconds();
    AddSample(static_cast<int>(sample_int));
  }
}

void TimedHistogram::RecordAbandon(base::ElapsedTimer* timer,
                                   Isolate* isolate) {
  if (Enabled()) {
    DCHECK(timer->IsStarted());
    timer->Stop();
    int64_t sample = resolution_ == TimedHistogramResolution::MICROSECOND
                         ? base::TimeDelta::Max().InMicroseconds()
                         : base::TimeDelta::Max().InMilliseconds();
    AddSample(static_cast<int>(sample));
  }
  if (isolate != nullptr) {
    V8FileLogger::CallEventLogger(isolate, name(), v8::LogEventStatus::kEnd,
                                  true);
  }
}

#ifdef DEBUG
bool TimedHistogram::ToggleRunningState(bool expect_to_run) const {
  static thread_local base::LazyInstance<
      std::unordered_map<const TimedHistogram*, bool>>::type active_timer =
      LAZY_INSTANCE_INITIALIZER;
  bool is_running = (*active_timer.Pointer())[this];
  DCHECK_NE(is_running, expect_to_run);
  (*active_timer.Pointer())[this] = !is_running;
  return true;
}
#endif

Counters::Counters(Isolate* isolate)
    :
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
    (this->*histogram.member)
        .Initialize(histogram.caption, histogram.min, histogram.max,
                    histogram.num_buckets, this);
  }

  const int DefaultTimedHistogramNumBuckets = 50;

  static const struct {
    NestedTimedHistogram Counters::*member;
    const char* caption;
    int max;
    TimedHistogramResolution res;
  } kNestedTimedHistograms[] = {
#define HT(name, caption, max, res) \
  {&Counters::name##_, #caption, max, TimedHistogramResolution::res},
      NESTED_TIMED_HISTOGRAM_LIST(HT) NESTED_TIMED_HISTOGRAM_LIST_SLOW(HT)
#undef HT
  };
  for (const auto& timer : kNestedTimedHistograms) {
    (this->*timer.member)
        .Initialize(timer.caption, 0, timer.max, timer.res,
                    DefaultTimedHistogramNumBuckets, this);
  }

  static const struct {
    TimedHistogram Counters::*member;
    const char* caption;
    int max;
    TimedHistogramResolution res;
  } kTimedHistograms[] = {
#define HT(name, caption, max, res) \
  {&Counters::name##_, #caption, max, TimedHistogramResolution::res},
      TIMED_HISTOGRAM_LIST(HT)
#undef HT
  };
  for (const auto& timer : kTimedHistograms) {
    (this->*timer.member)
        .Initialize(timer.caption, 0, timer.max, timer.res,
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
    (this->*aht.member)
        .Initialize(aht.caption, 0, 10000000, DefaultTimedHistogramNumBuckets,
                    this);
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
    (this->*percentage.member)
        .Initialize(percentage.caption, 0, 101, 100, this);
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
    (this->*histogram.member)
        .Initialize(histogram.caption, 1000, 500000, 50, this);
  }

  static constexpr struct {
    StatsCounter Counters::*member;
    const char* caption;
  } kStatsCounters[] = {
#define SC(name, caption) {&Counters::name##_, "c:" caption},
#define BARE_SC(name, caption) SC(name, #caption)
#define COUNT_AND_SIZE_SC(name)            \
  SC(count_of_##name, "V8.CountOf_" #name) \
  SC(size_of_##name, "V8.SizeOf_" #name)
#define CODE_KIND_SC(name) COUNT_AND_SIZE_SC(CODE_TYPE_##name)
#define FIXED_ARRAY_INSTANCE_TYPE_SC(name) COUNT_AND_SIZE_SC(FIXED_ARRAY_##name)

      // clang-format off
  STATS_COUNTER_LIST_1(BARE_SC)
  STATS_COUNTER_LIST_2(BARE_SC)
  STATS_COUNTER_NATIVE_CODE_LIST(BARE_SC)
  INSTANCE_TYPE_LIST(COUNT_AND_SIZE_SC)
  CODE_KIND_LIST(CODE_KIND_SC)
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(FIXED_ARRAY_INSTANCE_TYPE_SC)
  // clang-format on

#undef FIXED_ARRAY_INSTANCE_TYPE_SC
#undef CODE_KIND_SC
#undef COUNT_AND_SIZE_SC
#undef BARE_SC
#undef SC
  };
  for (const auto& counter : kStatsCounters) {
    (this->*counter.member).Init(this, counter.caption);
  }
}

void Counters::ResetCounterFunction(CounterLookupCallback f) {
  stats_table_.SetCounterFunction(f);

#define SC(name, caption) name##_.Reset();
  STATS_COUNTER_LIST_1(SC)
  STATS_COUNTER_LIST_2(SC)
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
  NESTED_TIMED_HISTOGRAM_LIST(HT)
#undef HT

#define HT(name, caption, max, res) name##_.Reset();
  NESTED_TIMED_HISTOGRAM_LIST_SLOW(HT)
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
