// Copyright 2013 the V8 project authors. All rights reserved.
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

#include <cstdlib>

#include "v8.h"

#include "cctest.h"
#if V8_OS_WIN
#include "win32-headers.h"
#endif

using namespace v8::internal;


TEST(TimeDeltaFromAndIn) {
  CHECK(TimeDelta::FromDays(2) == TimeDelta::FromHours(48));
  CHECK(TimeDelta::FromHours(3) == TimeDelta::FromMinutes(180));
  CHECK(TimeDelta::FromMinutes(2) == TimeDelta::FromSeconds(120));
  CHECK(TimeDelta::FromSeconds(2) == TimeDelta::FromMilliseconds(2000));
  CHECK(TimeDelta::FromMilliseconds(2) == TimeDelta::FromMicroseconds(2000));
  CHECK_EQ(static_cast<int>(13), TimeDelta::FromDays(13).InDays());
  CHECK_EQ(static_cast<int>(13), TimeDelta::FromHours(13).InHours());
  CHECK_EQ(static_cast<int>(13), TimeDelta::FromMinutes(13).InMinutes());
  CHECK_EQ(static_cast<int64_t>(13), TimeDelta::FromSeconds(13).InSeconds());
  CHECK_EQ(13.0, TimeDelta::FromSeconds(13).InSecondsF());
  CHECK_EQ(static_cast<int64_t>(13),
           TimeDelta::FromMilliseconds(13).InMilliseconds());
  CHECK_EQ(13.0, TimeDelta::FromMilliseconds(13).InMillisecondsF());
  CHECK_EQ(static_cast<int64_t>(13),
           TimeDelta::FromMicroseconds(13).InMicroseconds());
}


#if V8_OS_MACOSX
TEST(TimeDeltaFromMachTimespec) {
  TimeDelta null = TimeDelta();
  CHECK(null == TimeDelta::FromMachTimespec(null.ToMachTimespec()));
  TimeDelta delta1 = TimeDelta::FromMilliseconds(42);
  CHECK(delta1 == TimeDelta::FromMachTimespec(delta1.ToMachTimespec()));
  TimeDelta delta2 = TimeDelta::FromDays(42);
  CHECK(delta2 == TimeDelta::FromMachTimespec(delta2.ToMachTimespec()));
}
#endif


TEST(TimeJsTime) {
  Time t = Time::FromJsTime(700000.3);
  CHECK_EQ(700000.3, t.ToJsTime());
}


#if V8_OS_POSIX
TEST(TimeFromTimespec) {
  Time null;
  CHECK(null.IsNull());
  CHECK(null == Time::FromTimespec(null.ToTimespec()));
  Time now = Time::Now();
  CHECK(now == Time::FromTimespec(now.ToTimespec()));
  Time now_sys = Time::NowFromSystemTime();
  CHECK(now_sys == Time::FromTimespec(now_sys.ToTimespec()));
  Time unix_epoch = Time::UnixEpoch();
  CHECK(unix_epoch == Time::FromTimespec(unix_epoch.ToTimespec()));
  Time max = Time::Max();
  CHECK(max.IsMax());
  CHECK(max == Time::FromTimespec(max.ToTimespec()));
}


TEST(TimeFromTimeval) {
  Time null;
  CHECK(null.IsNull());
  CHECK(null == Time::FromTimeval(null.ToTimeval()));
  Time now = Time::Now();
  CHECK(now == Time::FromTimeval(now.ToTimeval()));
  Time now_sys = Time::NowFromSystemTime();
  CHECK(now_sys == Time::FromTimeval(now_sys.ToTimeval()));
  Time unix_epoch = Time::UnixEpoch();
  CHECK(unix_epoch == Time::FromTimeval(unix_epoch.ToTimeval()));
  Time max = Time::Max();
  CHECK(max.IsMax());
  CHECK(max == Time::FromTimeval(max.ToTimeval()));
}
#endif


#if V8_OS_WIN
TEST(TimeFromFiletime) {
  Time null;
  CHECK(null.IsNull());
  CHECK(null == Time::FromFiletime(null.ToFiletime()));
  Time now = Time::Now();
  CHECK(now == Time::FromFiletime(now.ToFiletime()));
  Time now_sys = Time::NowFromSystemTime();
  CHECK(now_sys == Time::FromFiletime(now_sys.ToFiletime()));
  Time unix_epoch = Time::UnixEpoch();
  CHECK(unix_epoch == Time::FromFiletime(unix_epoch.ToFiletime()));
  Time max = Time::Max();
  CHECK(max.IsMax());
  CHECK(max == Time::FromFiletime(max.ToFiletime()));
}
#endif


TEST(TimeTicksIsMonotonic) {
  TimeTicks previous_normal_ticks;
  TimeTicks previous_highres_ticks;
  ElapsedTimer timer;
  timer.Start();
  while (!timer.HasExpired(TimeDelta::FromMilliseconds(100))) {
    TimeTicks normal_ticks = TimeTicks::Now();
    TimeTicks highres_ticks = TimeTicks::HighResolutionNow();
    CHECK_GE(normal_ticks, previous_normal_ticks);
    CHECK_GE((normal_ticks - previous_normal_ticks).InMicroseconds(), 0);
    CHECK_GE(highres_ticks, previous_highres_ticks);
    CHECK_GE((highres_ticks - previous_highres_ticks).InMicroseconds(), 0);
    previous_normal_ticks = normal_ticks;
    previous_highres_ticks = highres_ticks;
  }
}


template <typename T>
static void ResolutionTest(T (*Now)(), TimeDelta target_granularity) {
  // We're trying to measure that intervals increment in a VERY small amount
  // of time -- according to the specified target granularity. Unfortunately,
  // if we happen to have a context switch in the middle of our test, the
  // context switch could easily exceed our limit. So, we iterate on this
  // several times. As long as we're able to detect the fine-granularity
  // timers at least once, then the test has succeeded.
  static const TimeDelta kExpirationTimeout = TimeDelta::FromSeconds(1);
  ElapsedTimer timer;
  timer.Start();
  TimeDelta delta;
  do {
    T start = Now();
    T now = start;
    // Loop until we can detect that the clock has changed. Non-HighRes timers
    // will increment in chunks, i.e. 15ms. By spinning until we see a clock
    // change, we detect the minimum time between measurements.
    do {
      now = Now();
      delta = now - start;
    } while (now <= start);
    CHECK_NE(static_cast<int64_t>(0), delta.InMicroseconds());
  } while (delta > target_granularity && !timer.HasExpired(kExpirationTimeout));
  CHECK_LE(delta, target_granularity);
}


TEST(TimeNowResolution) {
  // We assume that Time::Now() has at least 16ms resolution.
  static const TimeDelta kTargetGranularity = TimeDelta::FromMilliseconds(16);
  ResolutionTest<Time>(&Time::Now, kTargetGranularity);
}


TEST(TimeTicksNowResolution) {
  // We assume that TimeTicks::Now() has at least 16ms resolution.
  static const TimeDelta kTargetGranularity = TimeDelta::FromMilliseconds(16);
  ResolutionTest<TimeTicks>(&TimeTicks::Now, kTargetGranularity);
}


TEST(TimeTicksHighResolutionNowResolution) {
  if (!TimeTicks::IsHighResolutionClockWorking()) return;

  // We assume that TimeTicks::HighResolutionNow() has sub-ms resolution.
  static const TimeDelta kTargetGranularity = TimeDelta::FromMilliseconds(1);
  ResolutionTest<TimeTicks>(&TimeTicks::HighResolutionNow, kTargetGranularity);
}
