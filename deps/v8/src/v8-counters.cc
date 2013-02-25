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

#include "v8-counters.h"

namespace v8 {
namespace internal {

Counters::Counters() {
#define HT(name, caption) \
    HistogramTimer name = { {#caption, 0, 10000, 50, NULL, false}, 0, 0 }; \
    name##_ = name;
    HISTOGRAM_TIMER_LIST(HT)
#undef HT

#define HP(name, caption) \
    Histogram name = { #caption, 0, 101, 100, NULL, false }; \
    name##_ = name;
    HISTOGRAM_PERCENTAGE_LIST(HP)
#undef HP

#define HM(name, caption) \
    Histogram name = { #caption, 1000, 500000, 50, NULL, false }; \
    name##_ = name;
    HISTOGRAM_MEMORY_LIST(HM)
#undef HM

#define SC(name, caption) \
    StatsCounter name = { "c:" #caption, NULL, false };\
    name##_ = name;

    STATS_COUNTER_LIST_1(SC)
    STATS_COUNTER_LIST_2(SC)
#undef SC

#define SC(name) \
    StatsCounter count_of_##name = { "c:" "V8.CountOf_" #name, NULL, false };\
    count_of_##name##_ = count_of_##name; \
    StatsCounter size_of_##name = { "c:" "V8.SizeOf_" #name, NULL, false };\
    size_of_##name##_ = size_of_##name;
    INSTANCE_TYPE_LIST(SC)
#undef SC

#define SC(name) \
    StatsCounter count_of_CODE_TYPE_##name = { \
      "c:" "V8.CountOf_CODE_TYPE-" #name, NULL, false }; \
    count_of_CODE_TYPE_##name##_ = count_of_CODE_TYPE_##name; \
    StatsCounter size_of_CODE_TYPE_##name = { \
      "c:" "V8.SizeOf_CODE_TYPE-" #name, NULL, false }; \
    size_of_CODE_TYPE_##name##_ = size_of_CODE_TYPE_##name;
    CODE_KIND_LIST(SC)
#undef SC

#define SC(name) \
    StatsCounter count_of_FIXED_ARRAY_##name = { \
      "c:" "V8.CountOf_FIXED_ARRAY-" #name, NULL, false }; \
    count_of_FIXED_ARRAY_##name##_ = count_of_FIXED_ARRAY_##name; \
    StatsCounter size_of_FIXED_ARRAY_##name = { \
      "c:" "V8.SizeOf_FIXED_ARRAY-" #name, NULL, false }; \
    size_of_FIXED_ARRAY_##name##_ = size_of_FIXED_ARRAY_##name;
    FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(SC)
#undef SC

  StatsCounter state_counters[] = {
#define COUNTER_NAME(name) \
    { "c:V8.State" #name, NULL, false },
    STATE_TAG_LIST(COUNTER_NAME)
#undef COUNTER_NAME
  };

  for (int i = 0; i < kSlidingStateWindowCounterCount; ++i) {
    state_counters_[i] = state_counters[i];
  }
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
