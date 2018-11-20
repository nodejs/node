// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/time.h"

#if V8_OS_MACOSX
#include <mach/mach_time.h>
#endif
#if V8_OS_POSIX
#include <sys/time.h>
#endif

#if V8_OS_WIN
#include "src/base/win32-headers.h"
#endif

#include "src/base/platform/elapsed-timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

TEST(TimeDelta, FromAndIn) {
  EXPECT_EQ(TimeDelta::FromDays(2), TimeDelta::FromHours(48));
  EXPECT_EQ(TimeDelta::FromHours(3), TimeDelta::FromMinutes(180));
  EXPECT_EQ(TimeDelta::FromMinutes(2), TimeDelta::FromSeconds(120));
  EXPECT_EQ(TimeDelta::FromSeconds(2), TimeDelta::FromMilliseconds(2000));
  EXPECT_EQ(TimeDelta::FromMilliseconds(2), TimeDelta::FromMicroseconds(2000));
  EXPECT_EQ(static_cast<int>(13), TimeDelta::FromDays(13).InDays());
  EXPECT_EQ(static_cast<int>(13), TimeDelta::FromHours(13).InHours());
  EXPECT_EQ(static_cast<int>(13), TimeDelta::FromMinutes(13).InMinutes());
  EXPECT_EQ(static_cast<int64_t>(13), TimeDelta::FromSeconds(13).InSeconds());
  EXPECT_DOUBLE_EQ(13.0, TimeDelta::FromSeconds(13).InSecondsF());
  EXPECT_EQ(static_cast<int64_t>(13),
            TimeDelta::FromMilliseconds(13).InMilliseconds());
  EXPECT_DOUBLE_EQ(13.0, TimeDelta::FromMilliseconds(13).InMillisecondsF());
  EXPECT_EQ(static_cast<int64_t>(13),
            TimeDelta::FromMicroseconds(13).InMicroseconds());
}


#if V8_OS_MACOSX
TEST(TimeDelta, MachTimespec) {
  TimeDelta null = TimeDelta();
  EXPECT_EQ(null, TimeDelta::FromMachTimespec(null.ToMachTimespec()));
  TimeDelta delta1 = TimeDelta::FromMilliseconds(42);
  EXPECT_EQ(delta1, TimeDelta::FromMachTimespec(delta1.ToMachTimespec()));
  TimeDelta delta2 = TimeDelta::FromDays(42);
  EXPECT_EQ(delta2, TimeDelta::FromMachTimespec(delta2.ToMachTimespec()));
}
#endif


TEST(Time, JsTime) {
  Time t = Time::FromJsTime(700000.3);
  EXPECT_DOUBLE_EQ(700000.3, t.ToJsTime());
}


#if V8_OS_POSIX
TEST(Time, Timespec) {
  Time null;
  EXPECT_TRUE(null.IsNull());
  EXPECT_EQ(null, Time::FromTimespec(null.ToTimespec()));
  Time now = Time::Now();
  EXPECT_EQ(now, Time::FromTimespec(now.ToTimespec()));
  Time now_sys = Time::NowFromSystemTime();
  EXPECT_EQ(now_sys, Time::FromTimespec(now_sys.ToTimespec()));
  Time unix_epoch = Time::UnixEpoch();
  EXPECT_EQ(unix_epoch, Time::FromTimespec(unix_epoch.ToTimespec()));
  Time max = Time::Max();
  EXPECT_TRUE(max.IsMax());
  EXPECT_EQ(max, Time::FromTimespec(max.ToTimespec()));
}


TEST(Time, Timeval) {
  Time null;
  EXPECT_TRUE(null.IsNull());
  EXPECT_EQ(null, Time::FromTimeval(null.ToTimeval()));
  Time now = Time::Now();
  EXPECT_EQ(now, Time::FromTimeval(now.ToTimeval()));
  Time now_sys = Time::NowFromSystemTime();
  EXPECT_EQ(now_sys, Time::FromTimeval(now_sys.ToTimeval()));
  Time unix_epoch = Time::UnixEpoch();
  EXPECT_EQ(unix_epoch, Time::FromTimeval(unix_epoch.ToTimeval()));
  Time max = Time::Max();
  EXPECT_TRUE(max.IsMax());
  EXPECT_EQ(max, Time::FromTimeval(max.ToTimeval()));
}
#endif


#if V8_OS_WIN
TEST(Time, Filetime) {
  Time null;
  EXPECT_TRUE(null.IsNull());
  EXPECT_EQ(null, Time::FromFiletime(null.ToFiletime()));
  Time now = Time::Now();
  EXPECT_EQ(now, Time::FromFiletime(now.ToFiletime()));
  Time now_sys = Time::NowFromSystemTime();
  EXPECT_EQ(now_sys, Time::FromFiletime(now_sys.ToFiletime()));
  Time unix_epoch = Time::UnixEpoch();
  EXPECT_EQ(unix_epoch, Time::FromFiletime(unix_epoch.ToFiletime()));
  Time max = Time::Max();
  EXPECT_TRUE(max.IsMax());
  EXPECT_EQ(max, Time::FromFiletime(max.ToFiletime()));
}
#endif


namespace {

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
    EXPECT_NE(static_cast<int64_t>(0), delta.InMicroseconds());
  } while (delta > target_granularity && !timer.HasExpired(kExpirationTimeout));
  EXPECT_LE(delta, target_granularity);
}

}  // namespace


TEST(Time, NowResolution) {
  // We assume that Time::Now() has at least 16ms resolution.
  static const TimeDelta kTargetGranularity = TimeDelta::FromMilliseconds(16);
  ResolutionTest<Time>(&Time::Now, kTargetGranularity);
}


TEST(TimeTicks, NowResolution) {
  // We assume that TimeTicks::Now() has at least 16ms resolution.
  static const TimeDelta kTargetGranularity = TimeDelta::FromMilliseconds(16);
  ResolutionTest<TimeTicks>(&TimeTicks::Now, kTargetGranularity);
}


TEST(TimeTicks, HighResolutionNowResolution) {
  if (!TimeTicks::IsHighResolutionClockWorking()) return;

  // We assume that TimeTicks::HighResolutionNow() has sub-ms resolution.
  static const TimeDelta kTargetGranularity = TimeDelta::FromMilliseconds(1);
  ResolutionTest<TimeTicks>(&TimeTicks::HighResolutionNow, kTargetGranularity);
}


TEST(TimeTicks, IsMonotonic) {
  TimeTicks previous_normal_ticks;
  TimeTicks previous_highres_ticks;
  ElapsedTimer timer;
  timer.Start();
  while (!timer.HasExpired(TimeDelta::FromMilliseconds(100))) {
    TimeTicks normal_ticks = TimeTicks::Now();
    TimeTicks highres_ticks = TimeTicks::HighResolutionNow();
    EXPECT_GE(normal_ticks, previous_normal_ticks);
    EXPECT_GE((normal_ticks - previous_normal_ticks).InMicroseconds(), 0);
    EXPECT_GE(highres_ticks, previous_highres_ticks);
    EXPECT_GE((highres_ticks - previous_highres_ticks).InMicroseconds(), 0);
    previous_normal_ticks = normal_ticks;
    previous_highres_ticks = highres_ticks;
  }
}

}  // namespace base
}  // namespace v8
