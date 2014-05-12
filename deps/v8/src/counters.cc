// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8.h"

#include "counters.h"
#include "isolate.h"
#include "platform.h"

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
  isolate()->event_logger()(name(), Logger::START);
}


// Stop the timer and record the results.
void HistogramTimer::Stop() {
  if (Enabled()) {
    // Compute the delta between start and stop, in milliseconds.
    AddSample(static_cast<int>(timer_.Elapsed().InMilliseconds()));
    timer_.Stop();
  }
  isolate()->event_logger()(name(), Logger::END);
}


Counters::Counters(Isolate* isolate) {
#define HT(name, caption) \
    name##_ = HistogramTimer(#caption, 0, 10000, 50, isolate);
    HISTOGRAM_TIMER_LIST(HT)
#undef HT

#define HP(name, caption) \
    name##_ = Histogram(#caption, 0, 101, 100, isolate);
    HISTOGRAM_PERCENTAGE_LIST(HP)
#undef HP

#define HM(name, caption) \
    name##_ = Histogram(#caption, 1000, 500000, 50, isolate);
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


void Counters::ResetHistograms() {
#define HT(name, caption) name##_.Reset();
    HISTOGRAM_TIMER_LIST(HT)
#undef HT

#define HP(name, caption) name##_.Reset();
    HISTOGRAM_PERCENTAGE_LIST(HP)
#undef HP

#define HM(name, caption) name##_.Reset();
    HISTOGRAM_MEMORY_LIST(HM)
#undef HM
}

} }  // namespace v8::internal
