// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/counters.h"

#include <iomanip>

#include "src/base/platform/platform.h"
#include "src/isolate.h"
#include "src/log-inl.h"
#include "src/log.h"

namespace v8 {
namespace internal {

StatsTable::StatsTable()
    : lookup_function_(NULL),
      create_histogram_function_(NULL),
      add_histogram_sample_function_(NULL) {}


int* StatsCounter::FindLocationInStatsTable() const {
  return isolate_->stats_table()->FindLocation(name_);
}


void Histogram::AddSample(int sample) {
  if (Enabled()) {
    isolate()->stats_table()->AddHistogramSample(histogram_, sample);
  }
}

void* Histogram::CreateHistogram() const {
  return isolate()->stats_table()->
      CreateHistogram(name_, min_, max_, num_buckets_);
}


// Start the timer.
void HistogramTimer::Start() {
  if (Enabled()) {
    timer_.Start();
  }
  Logger::CallEventLogger(isolate(), name(), Logger::START, true);
}


// Stop the timer and record the results.
void HistogramTimer::Stop() {
  if (Enabled()) {
    int64_t sample = resolution_ == MICROSECOND
                         ? timer_.Elapsed().InMicroseconds()
                         : timer_.Elapsed().InMilliseconds();
    // Compute the delta between start and stop, in microseconds.
    AddSample(static_cast<int>(sample));
    timer_.Stop();
  }
  Logger::CallEventLogger(isolate(), name(), Logger::END, true);
}


Counters::Counters(Isolate* isolate) {
#define HR(name, caption, min, max, num_buckets) \
  name##_ = Histogram(#caption, min, max, num_buckets, isolate);
  HISTOGRAM_RANGE_LIST(HR)
#undef HR

#define HT(name, caption, max, res) \
  name##_ = HistogramTimer(#caption, 0, max, HistogramTimer::res, 50, isolate);
    HISTOGRAM_TIMER_LIST(HT)
#undef HT

#define AHT(name, caption) \
  name##_ = AggregatableHistogramTimer(#caption, 0, 10000000, 50, isolate);
    AGGREGATABLE_HISTOGRAM_TIMER_LIST(AHT)
#undef AHT

#define HP(name, caption) \
    name##_ = Histogram(#caption, 0, 101, 100, isolate);
    HISTOGRAM_PERCENTAGE_LIST(HP)
#undef HP


// Exponential histogram assigns bucket limits to points
// p[1], p[2], ... p[n] such that p[i+1] / p[i] = constant.
// The constant factor is equal to the n-th root of (high / low),
// where the n is the number of buckets, the low is the lower limit,
// the high is the upper limit.
// For n = 50, low = 1000, high = 500000: the factor = 1.13.
#define HM(name, caption) \
    name##_ = Histogram(#caption, 1000, 500000, 50, isolate);
  HISTOGRAM_LEGACY_MEMORY_LIST(HM)
#undef HM
// For n = 100, low = 4000, high = 2000000: the factor = 1.06.
#define HM(name, caption) \
  name##_ = Histogram(#caption, 4000, 2000000, 100, isolate);
  HISTOGRAM_MEMORY_LIST(HM)
#undef HM

#define HM(name, caption) \
  aggregated_##name##_ = AggregatedMemoryHistogram<Histogram>(&name##_);
    HISTOGRAM_MEMORY_LIST(HM)
#undef HM

#define SC(name, caption) \
    name##_ = StatsCounter(isolate, "c:" #caption);

    STATS_COUNTER_LIST_1(SC)
    STATS_COUNTER_LIST_2(SC)
#undef SC

#define SC(name) \
    count_of_##name##_ = StatsCounter(isolate, "c:" "V8.CountOf_" #name); \
    size_of_##name##_ = StatsCounter(isolate, "c:" "V8.SizeOf_" #name);
    INSTANCE_TYPE_LIST(SC)
#undef SC

#define SC(name) \
    count_of_CODE_TYPE_##name##_ = \
        StatsCounter(isolate, "c:" "V8.CountOf_CODE_TYPE-" #name); \
    size_of_CODE_TYPE_##name##_ = \
        StatsCounter(isolate, "c:" "V8.SizeOf_CODE_TYPE-" #name);
    CODE_KIND_LIST(SC)
#undef SC

#define SC(name) \
    count_of_FIXED_ARRAY_##name##_ = \
        StatsCounter(isolate, "c:" "V8.CountOf_FIXED_ARRAY-" #name); \
    size_of_FIXED_ARRAY_##name##_ = \
        StatsCounter(isolate, "c:" "V8.SizeOf_FIXED_ARRAY-" #name);
    FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(SC)
#undef SC

#define SC(name) \
    count_of_CODE_AGE_##name##_ = \
        StatsCounter(isolate, "c:" "V8.CountOf_CODE_AGE-" #name); \
    size_of_CODE_AGE_##name##_ = \
        StatsCounter(isolate, "c:" "V8.SizeOf_CODE_AGE-" #name);
    CODE_AGE_LIST_COMPLETE(SC)
#undef SC
}


void Counters::ResetCounters() {
#define SC(name, caption) name##_.Reset();
  STATS_COUNTER_LIST_1(SC)
  STATS_COUNTER_LIST_2(SC)
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

#define SC(name)                       \
  count_of_CODE_AGE_##name##_.Reset(); \
  size_of_CODE_AGE_##name##_.Reset();
  CODE_AGE_LIST_COMPLETE(SC)
#undef SC
}


void Counters::ResetHistograms() {
#define HR(name, caption, min, max, num_buckets) name##_.Reset();
  HISTOGRAM_RANGE_LIST(HR)
#undef HR

#define HT(name, caption, max, res) name##_.Reset();
    HISTOGRAM_TIMER_LIST(HT)
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

class RuntimeCallStatEntries {
 public:
  void Print(std::ostream& os) {
    if (total_call_count == 0) return;
    std::sort(entries.rbegin(), entries.rend());
    os << std::setw(50) << "Runtime Function/C++ Builtin" << std::setw(12)
       << "Time" << std::setw(18) << "Count" << std::endl
       << std::string(88, '=') << std::endl;
    for (Entry& entry : entries) {
      entry.SetTotal(total_time, total_call_count);
      entry.Print(os);
    }
    os << std::string(88, '-') << std::endl;
    Entry("Total", total_time, total_call_count).Print(os);
  }

  // By default, the compiler will usually inline this, which results in a large
  // binary size increase: std::vector::push_back expands to a large amount of
  // instructions, and this function is invoked repeatedly by macros.
  V8_NOINLINE void Add(RuntimeCallCounter* counter) {
    if (counter->count == 0) return;
    entries.push_back(Entry(counter->name, counter->time, counter->count));
    total_time += counter->time;
    total_call_count += counter->count;
  }

 private:
  class Entry {
   public:
    Entry(const char* name, base::TimeDelta time, uint64_t count)
        : name_(name),
          time_(time.InMicroseconds()),
          count_(count),
          time_percent_(100),
          count_percent_(100) {}

    bool operator<(const Entry& other) const {
      if (time_ < other.time_) return true;
      if (time_ > other.time_) return false;
      return count_ < other.count_;
    }

    void Print(std::ostream& os) {
      os.precision(2);
      os << std::fixed << std::setprecision(2);
      os << std::setw(50) << name_;
      os << std::setw(10) << static_cast<double>(time_) / 1000 << "ms ";
      os << std::setw(6) << time_percent_ << "%";
      os << std::setw(10) << count_ << " ";
      os << std::setw(6) << count_percent_ << "%";
      os << std::endl;
    }

    void SetTotal(base::TimeDelta total_time, uint64_t total_count) {
      if (total_time.InMicroseconds() == 0) {
        time_percent_ = 0;
      } else {
        time_percent_ = 100.0 * time_ / total_time.InMicroseconds();
      }
      count_percent_ = 100.0 * count_ / total_count;
    }

   private:
    const char* name_;
    int64_t time_;
    uint64_t count_;
    double time_percent_;
    double count_percent_;
  };

  uint64_t total_call_count = 0;
  base::TimeDelta total_time;
  std::vector<Entry> entries;
};

void RuntimeCallCounter::Reset() {
  count = 0;
  time = base::TimeDelta();
}

void RuntimeCallCounter::Dump(std::stringstream& out) {
  out << "\"" << name << "\":[" << count << "," << time.InMicroseconds()
      << "],";
}

// static
void RuntimeCallStats::Enter(RuntimeCallStats* stats, RuntimeCallTimer* timer,
                             CounterId counter_id) {
  RuntimeCallCounter* counter = &(stats->*counter_id);
  timer->Start(counter, stats->current_timer_);
  stats->current_timer_ = timer;
}

// static
void RuntimeCallStats::Leave(RuntimeCallStats* stats, RuntimeCallTimer* timer) {
  if (stats->current_timer_ == timer) {
    stats->current_timer_ = timer->Stop();
  } else {
    // Must be a Threading cctest. Walk the chain of Timers to find the
    // buried one that's leaving. We don't care about keeping nested timings
    // accurate, just avoid crashing by keeping the chain intact.
    RuntimeCallTimer* next = stats->current_timer_;
    while (next->parent_ != timer) next = next->parent_;
    next->parent_ = timer->Stop();
  }
}

// static
void RuntimeCallStats::CorrectCurrentCounterId(RuntimeCallStats* stats,
                                               CounterId counter_id) {
  DCHECK_NOT_NULL(stats->current_timer_);
  RuntimeCallCounter* counter = &(stats->*counter_id);
  stats->current_timer_->counter_ = counter;
}

void RuntimeCallStats::Print(std::ostream& os) {
  RuntimeCallStatEntries entries;

#define PRINT_COUNTER(name) entries.Add(&this->name);
  FOR_EACH_MANUAL_COUNTER(PRINT_COUNTER)
#undef PRINT_COUNTER

#define PRINT_COUNTER(name, nargs, ressize) entries.Add(&this->Runtime_##name);
  FOR_EACH_INTRINSIC(PRINT_COUNTER)
#undef PRINT_COUNTER

#define PRINT_COUNTER(name) entries.Add(&this->Builtin_##name);
  BUILTIN_LIST_C(PRINT_COUNTER)
#undef PRINT_COUNTER

#define PRINT_COUNTER(name) entries.Add(&this->API_##name);
  FOR_EACH_API_COUNTER(PRINT_COUNTER)
#undef PRINT_COUNTER

#define PRINT_COUNTER(name) entries.Add(&this->Handler_##name);
  FOR_EACH_HANDLER_COUNTER(PRINT_COUNTER)
#undef PRINT_COUNTER

  entries.Print(os);
}

void RuntimeCallStats::Reset() {
  if (!FLAG_runtime_call_stats &&
      !TRACE_EVENT_RUNTIME_CALL_STATS_TRACING_ENABLED())
    return;
#define RESET_COUNTER(name) this->name.Reset();
  FOR_EACH_MANUAL_COUNTER(RESET_COUNTER)
#undef RESET_COUNTER

#define RESET_COUNTER(name, nargs, result_size) this->Runtime_##name.Reset();
  FOR_EACH_INTRINSIC(RESET_COUNTER)
#undef RESET_COUNTER

#define RESET_COUNTER(name) this->Builtin_##name.Reset();
  BUILTIN_LIST_C(RESET_COUNTER)
#undef RESET_COUNTER

#define RESET_COUNTER(name) this->API_##name.Reset();
  FOR_EACH_API_COUNTER(RESET_COUNTER)
#undef RESET_COUNTER

#define RESET_COUNTER(name) this->Handler_##name.Reset();
  FOR_EACH_HANDLER_COUNTER(RESET_COUNTER)
#undef RESET_COUNTER

  in_use_ = true;
}

std::string RuntimeCallStats::Dump() {
  buffer_.str(std::string());
  buffer_.clear();
  buffer_ << "{";
#define DUMP_COUNTER(name) \
  if (this->name.count > 0) this->name.Dump(buffer_);
  FOR_EACH_MANUAL_COUNTER(DUMP_COUNTER)
#undef DUMP_COUNTER

#define DUMP_COUNTER(name, nargs, result_size) \
  if (this->Runtime_##name.count > 0) this->Runtime_##name.Dump(buffer_);
  FOR_EACH_INTRINSIC(DUMP_COUNTER)
#undef DUMP_COUNTER

#define DUMP_COUNTER(name) \
  if (this->Builtin_##name.count > 0) this->Builtin_##name.Dump(buffer_);
  BUILTIN_LIST_C(DUMP_COUNTER)
#undef DUMP_COUNTER

#define DUMP_COUNTER(name) \
  if (this->API_##name.count > 0) this->API_##name.Dump(buffer_);
  FOR_EACH_API_COUNTER(DUMP_COUNTER)
#undef DUMP_COUNTER

#define DUMP_COUNTER(name) \
  if (this->Handler_##name.count > 0) this->Handler_##name.Dump(buffer_);
  FOR_EACH_HANDLER_COUNTER(DUMP_COUNTER)
#undef DUMP_COUNTER
  buffer_ << "\"END\":[]}";
  in_use_ = false;
  return buffer_.str();
}

}  // namespace internal
}  // namespace v8
