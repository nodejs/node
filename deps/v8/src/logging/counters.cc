// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/logging/counters.h"

#include <iomanip>

#include "src/base/platform/platform.h"
#include "src/builtins/builtins-definitions.h"
#include "src/execution/isolate.h"
#include "src/logging/counters-inl.h"
#include "src/logging/log-inl.h"
#include "src/logging/log.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

std::atomic_uint TracingFlags::runtime_stats{0};
std::atomic_uint TracingFlags::gc{0};
std::atomic_uint TracingFlags::gc_stats{0};
std::atomic_uint TracingFlags::ic_stats{0};

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

void TimedHistogram::Start(base::ElapsedTimer* timer, Isolate* isolate) {
  if (Enabled()) timer->Start();
  if (isolate) Logger::CallEventLogger(isolate, name(), Logger::START, true);
}

void TimedHistogram::Stop(base::ElapsedTimer* timer, Isolate* isolate) {
  if (Enabled()) {
    int64_t sample = resolution_ == HistogramTimerResolution::MICROSECOND
                         ? timer->Elapsed().InMicroseconds()
                         : timer->Elapsed().InMilliseconds();
    timer->Stop();
    AddSample(static_cast<int>(sample));
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
    : isolate_(isolate),
      stats_table_(this),
// clang format off
#define SC(name, caption) name##_(this, "c:" #caption),
      STATS_COUNTER_TS_LIST(SC)
#undef SC
      // clang format on
      runtime_call_stats_(RuntimeCallStats::kMainIsolateThread),
      worker_thread_runtime_call_stats_() {
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

base::TimeTicks (*RuntimeCallTimer::Now)() =
    &base::TimeTicks::HighResolutionNow;

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
    if (counter->count() == 0) return;
    entries.push_back(
        Entry(counter->name(), counter->time(), counter->count()));
    total_time += counter->time();
    total_call_count += counter->count();
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

    V8_NOINLINE void Print(std::ostream& os) {
      os.precision(2);
      os << std::fixed << std::setprecision(2);
      os << std::setw(50) << name_;
      os << std::setw(10) << static_cast<double>(time_) / 1000 << "ms ";
      os << std::setw(6) << time_percent_ << "%";
      os << std::setw(10) << count_ << " ";
      os << std::setw(6) << count_percent_ << "%";
      os << std::endl;
    }

    V8_NOINLINE void SetTotal(base::TimeDelta total_time,
                              uint64_t total_count) {
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
  count_ = 0;
  time_ = 0;
}

void RuntimeCallCounter::Dump(v8::tracing::TracedValue* value) {
  value->BeginArray(name_);
  value->AppendDouble(count_);
  value->AppendDouble(time_);
  value->EndArray();
}

void RuntimeCallCounter::Add(RuntimeCallCounter* other) {
  count_ += other->count();
  time_ += other->time().InMicroseconds();
}

void RuntimeCallTimer::Snapshot() {
  base::TimeTicks now = Now();
  // Pause only / topmost timer in the timer stack.
  Pause(now);
  // Commit all the timer's elapsed time to the counters.
  RuntimeCallTimer* timer = this;
  while (timer != nullptr) {
    timer->CommitTimeToCounter();
    timer = timer->parent();
  }
  Resume(now);
}

RuntimeCallStats::RuntimeCallStats(ThreadType thread_type)
    : in_use_(false), thread_type_(thread_type) {
  static const char* const kNames[] = {
#define CALL_BUILTIN_COUNTER(name) "GC_" #name,
      FOR_EACH_GC_COUNTER(CALL_BUILTIN_COUNTER)  //
#undef CALL_BUILTIN_COUNTER
#define CALL_RUNTIME_COUNTER(name) #name,
      FOR_EACH_MANUAL_COUNTER(CALL_RUNTIME_COUNTER)  //
#undef CALL_RUNTIME_COUNTER
#define CALL_RUNTIME_COUNTER(name, nargs, ressize) #name,
      FOR_EACH_INTRINSIC(CALL_RUNTIME_COUNTER)  //
#undef CALL_RUNTIME_COUNTER
#define CALL_BUILTIN_COUNTER(name) #name,
      BUILTIN_LIST_C(CALL_BUILTIN_COUNTER)  //
#undef CALL_BUILTIN_COUNTER
#define CALL_BUILTIN_COUNTER(name) "API_" #name,
      FOR_EACH_API_COUNTER(CALL_BUILTIN_COUNTER)  //
#undef CALL_BUILTIN_COUNTER
#define CALL_BUILTIN_COUNTER(name) #name,
      FOR_EACH_HANDLER_COUNTER(CALL_BUILTIN_COUNTER)  //
#undef CALL_BUILTIN_COUNTER
#define THREAD_SPECIFIC_COUNTER(name) #name,
      FOR_EACH_THREAD_SPECIFIC_COUNTER(THREAD_SPECIFIC_COUNTER)  //
#undef THREAD_SPECIFIC_COUNTER
  };
  for (int i = 0; i < kNumberOfCounters; i++) {
    this->counters_[i] = RuntimeCallCounter(kNames[i]);
  }
}

namespace {
constexpr RuntimeCallCounterId FirstCounter(RuntimeCallCounterId first, ...) {
  return first;
}

#define THREAD_SPECIFIC_COUNTER(name) k##name,
constexpr RuntimeCallCounterId kFirstThreadVariantCounter =
    FirstCounter(FOR_EACH_THREAD_SPECIFIC_COUNTER(THREAD_SPECIFIC_COUNTER) 0);
#undef THREAD_SPECIFIC_COUNTER

#define THREAD_SPECIFIC_COUNTER(name) +1
constexpr int kThreadVariantCounterCount =
    0 FOR_EACH_THREAD_SPECIFIC_COUNTER(THREAD_SPECIFIC_COUNTER);
#undef THREAD_SPECIFIC_COUNTER

constexpr auto kLastThreadVariantCounter = static_cast<RuntimeCallCounterId>(
    kFirstThreadVariantCounter + kThreadVariantCounterCount - 1);
}  // namespace

bool RuntimeCallStats::HasThreadSpecificCounterVariants(
    RuntimeCallCounterId id) {
  // Check that it's in the range of the thread-specific variant counters and
  // also that it's one of the background counters.
  return id >= kFirstThreadVariantCounter && id <= kLastThreadVariantCounter;
}

bool RuntimeCallStats::IsBackgroundThreadSpecificVariant(
    RuntimeCallCounterId id) {
  return HasThreadSpecificCounterVariants(id) &&
         (id - kFirstThreadVariantCounter) % 2 == 1;
}

void RuntimeCallStats::Enter(RuntimeCallTimer* timer,
                             RuntimeCallCounterId counter_id) {
  DCHECK(IsCalledOnTheSameThread());
  RuntimeCallCounter* counter = GetCounter(counter_id);
  DCHECK_NOT_NULL(counter->name());
  timer->Start(counter, current_timer());
  current_timer_.SetValue(timer);
  current_counter_.SetValue(counter);
}

void RuntimeCallStats::Leave(RuntimeCallTimer* timer) {
  DCHECK(IsCalledOnTheSameThread());
  RuntimeCallTimer* stack_top = current_timer();
  if (stack_top == nullptr) return;  // Missing timer is a result of Reset().
  CHECK(stack_top == timer);
  current_timer_.SetValue(timer->Stop());
  RuntimeCallTimer* cur_timer = current_timer();
  current_counter_.SetValue(cur_timer ? cur_timer->counter() : nullptr);
}

void RuntimeCallStats::Add(RuntimeCallStats* other) {
  for (int i = 0; i < kNumberOfCounters; i++) {
    GetCounter(i)->Add(other->GetCounter(i));
  }
}

// static
void RuntimeCallStats::CorrectCurrentCounterId(RuntimeCallCounterId counter_id,
                                               CounterMode mode) {
  DCHECK(IsCalledOnTheSameThread());
  if (mode == RuntimeCallStats::CounterMode::kThreadSpecific) {
    counter_id = CounterIdForThread(counter_id);
  }
  DCHECK(IsCounterAppropriateForThread(counter_id));

  RuntimeCallTimer* timer = current_timer();
  if (timer == nullptr) return;
  RuntimeCallCounter* counter = GetCounter(counter_id);
  timer->set_counter(counter);
  current_counter_.SetValue(counter);
}

bool RuntimeCallStats::IsCalledOnTheSameThread() {
  if (thread_id_.IsValid()) return thread_id_ == ThreadId::Current();
  thread_id_ = ThreadId::Current();
  return true;
}

void RuntimeCallStats::Print() {
  StdoutStream os;
  Print(os);
}

void RuntimeCallStats::Print(std::ostream& os) {
  RuntimeCallStatEntries entries;
  if (current_timer_.Value() != nullptr) {
    current_timer_.Value()->Snapshot();
  }
  for (int i = 0; i < kNumberOfCounters; i++) {
    entries.Add(GetCounter(i));
  }
  entries.Print(os);
}

void RuntimeCallStats::Reset() {
  if (V8_LIKELY(!TracingFlags::is_runtime_stats_enabled())) return;

  // In tracing, we only what to trace the time spent on top level trace events,
  // if runtime counter stack is not empty, we should clear the whole runtime
  // counter stack, and then reset counters so that we can dump counters into
  // top level trace events accurately.
  while (current_timer_.Value()) {
    current_timer_.SetValue(current_timer_.Value()->Stop());
  }

  for (int i = 0; i < kNumberOfCounters; i++) {
    GetCounter(i)->Reset();
  }

  in_use_ = true;
}

void RuntimeCallStats::Dump(v8::tracing::TracedValue* value) {
  for (int i = 0; i < kNumberOfCounters; i++) {
    if (GetCounter(i)->count() > 0) GetCounter(i)->Dump(value);
  }
  in_use_ = false;
}

WorkerThreadRuntimeCallStats::WorkerThreadRuntimeCallStats()
    : isolate_thread_id_(ThreadId::Current()) {}

WorkerThreadRuntimeCallStats::~WorkerThreadRuntimeCallStats() {
  if (tls_key_) base::Thread::DeleteThreadLocalKey(*tls_key_);
}

base::Thread::LocalStorageKey WorkerThreadRuntimeCallStats::GetKey() {
  DCHECK(TracingFlags::is_runtime_stats_enabled());
  if (!tls_key_) tls_key_ = base::Thread::CreateThreadLocalKey();
  return *tls_key_;
}

RuntimeCallStats* WorkerThreadRuntimeCallStats::NewTable() {
  DCHECK(TracingFlags::is_runtime_stats_enabled());
  // Never create a new worker table on the isolate's main thread.
  DCHECK_NE(ThreadId::Current(), isolate_thread_id_);
  std::unique_ptr<RuntimeCallStats> new_table =
      std::make_unique<RuntimeCallStats>(RuntimeCallStats::kWorkerThread);
  RuntimeCallStats* result = new_table.get();

  base::MutexGuard lock(&mutex_);
  tables_.push_back(std::move(new_table));
  return result;
}

void WorkerThreadRuntimeCallStats::AddToMainTable(
    RuntimeCallStats* main_call_stats) {
  base::MutexGuard lock(&mutex_);
  for (auto& worker_stats : tables_) {
    DCHECK_NE(main_call_stats, worker_stats.get());
    main_call_stats->Add(worker_stats.get());
    worker_stats->Reset();
  }
}

WorkerThreadRuntimeCallStatsScope::WorkerThreadRuntimeCallStatsScope(
    WorkerThreadRuntimeCallStats* worker_stats)
    : table_(nullptr) {
  if (V8_LIKELY(!TracingFlags::is_runtime_stats_enabled())) return;

  table_ = reinterpret_cast<RuntimeCallStats*>(
      base::Thread::GetThreadLocal(worker_stats->GetKey()));
  if (table_ == nullptr) {
    table_ = worker_stats->NewTable();
    base::Thread::SetThreadLocal(worker_stats->GetKey(), table_);
  }

  if ((TracingFlags::runtime_stats.load(std::memory_order_relaxed) &
       v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING)) {
    table_->Reset();
  }
}

WorkerThreadRuntimeCallStatsScope::~WorkerThreadRuntimeCallStatsScope() {
  if (V8_LIKELY(table_ == nullptr)) return;

  if ((TracingFlags::runtime_stats.load(std::memory_order_relaxed) &
       v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING)) {
    auto value = v8::tracing::TracedValue::Create();
    table_->Dump(value.get());
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats"),
                         "V8.RuntimeStats", TRACE_EVENT_SCOPE_THREAD,
                         "runtime-call-stats", std::move(value));
  }
}

}  // namespace internal
}  // namespace v8
