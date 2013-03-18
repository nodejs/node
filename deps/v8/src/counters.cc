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
  return Isolate::Current()->stats_table()->FindLocation(name_);
}


// Start the timer.
void StatsCounterTimer::Start() {
  if (!counter_.Enabled())
    return;
  stop_time_ = 0;
  start_time_ = OS::Ticks();
}

// Stop the timer and record the results.
void StatsCounterTimer::Stop() {
  if (!counter_.Enabled())
    return;
  stop_time_ = OS::Ticks();

  // Compute the delta between start and stop, in milliseconds.
  int milliseconds = static_cast<int>(stop_time_ - start_time_) / 1000;
  counter_.Increment(milliseconds);
}

void Histogram::AddSample(int sample) {
  if (Enabled()) {
    Isolate::Current()->stats_table()->AddHistogramSample(histogram_, sample);
  }
}

void* Histogram::CreateHistogram() const {
  return Isolate::Current()->stats_table()->
      CreateHistogram(name_, min_, max_, num_buckets_);
}

// Start the timer.
void HistogramTimer::Start() {
  if (histogram_.Enabled()) {
    stop_time_ = 0;
    start_time_ = OS::Ticks();
  }
  if (FLAG_log_internal_timer_events) {
    LOG(Isolate::Current(), TimerEvent(Logger::START, histogram_.name_));
  }
}

// Stop the timer and record the results.
void HistogramTimer::Stop() {
  if (histogram_.Enabled()) {
    stop_time_ = OS::Ticks();
    // Compute the delta between start and stop, in milliseconds.
    int milliseconds = static_cast<int>(stop_time_ - start_time_) / 1000;
    histogram_.AddSample(milliseconds);
  }
  if (FLAG_log_internal_timer_events) {
    LOG(Isolate::Current(), TimerEvent(Logger::END, histogram_.name_));
  }
}

} }  // namespace v8::internal
