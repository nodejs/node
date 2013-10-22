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
