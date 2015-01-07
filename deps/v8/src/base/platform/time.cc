// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/time.h"

#if V8_OS_POSIX
#include <fcntl.h>  // for O_RDONLY
#include <sys/time.h>
#include <unistd.h>
#endif
#if V8_OS_MACOSX
#include <mach/mach_time.h>
#endif

#include <string.h>

#if V8_OS_WIN
#include "src/base/lazy-instance.h"
#include "src/base/win32-headers.h"
#endif
#include "src/base/cpu.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

TimeDelta TimeDelta::FromDays(int days) {
  return TimeDelta(days * Time::kMicrosecondsPerDay);
}


TimeDelta TimeDelta::FromHours(int hours) {
  return TimeDelta(hours * Time::kMicrosecondsPerHour);
}


TimeDelta TimeDelta::FromMinutes(int minutes) {
  return TimeDelta(minutes * Time::kMicrosecondsPerMinute);
}


TimeDelta TimeDelta::FromSeconds(int64_t seconds) {
  return TimeDelta(seconds * Time::kMicrosecondsPerSecond);
}


TimeDelta TimeDelta::FromMilliseconds(int64_t milliseconds) {
  return TimeDelta(milliseconds * Time::kMicrosecondsPerMillisecond);
}


TimeDelta TimeDelta::FromNanoseconds(int64_t nanoseconds) {
  return TimeDelta(nanoseconds / Time::kNanosecondsPerMicrosecond);
}


int TimeDelta::InDays() const {
  return static_cast<int>(delta_ / Time::kMicrosecondsPerDay);
}


int TimeDelta::InHours() const {
  return static_cast<int>(delta_ / Time::kMicrosecondsPerHour);
}


int TimeDelta::InMinutes() const {
  return static_cast<int>(delta_ / Time::kMicrosecondsPerMinute);
}


double TimeDelta::InSecondsF() const {
  return static_cast<double>(delta_) / Time::kMicrosecondsPerSecond;
}


int64_t TimeDelta::InSeconds() const {
  return delta_ / Time::kMicrosecondsPerSecond;
}


double TimeDelta::InMillisecondsF() const {
  return static_cast<double>(delta_) / Time::kMicrosecondsPerMillisecond;
}


int64_t TimeDelta::InMilliseconds() const {
  return delta_ / Time::kMicrosecondsPerMillisecond;
}


int64_t TimeDelta::InNanoseconds() const {
  return delta_ * Time::kNanosecondsPerMicrosecond;
}


#if V8_OS_MACOSX

TimeDelta TimeDelta::FromMachTimespec(struct mach_timespec ts) {
  DCHECK_GE(ts.tv_nsec, 0);
  DCHECK_LT(ts.tv_nsec,
            static_cast<long>(Time::kNanosecondsPerSecond));  // NOLINT
  return TimeDelta(ts.tv_sec * Time::kMicrosecondsPerSecond +
                   ts.tv_nsec / Time::kNanosecondsPerMicrosecond);
}


struct mach_timespec TimeDelta::ToMachTimespec() const {
  struct mach_timespec ts;
  DCHECK(delta_ >= 0);
  ts.tv_sec = delta_ / Time::kMicrosecondsPerSecond;
  ts.tv_nsec = (delta_ % Time::kMicrosecondsPerSecond) *
      Time::kNanosecondsPerMicrosecond;
  return ts;
}

#endif  // V8_OS_MACOSX


#if V8_OS_POSIX

TimeDelta TimeDelta::FromTimespec(struct timespec ts) {
  DCHECK_GE(ts.tv_nsec, 0);
  DCHECK_LT(ts.tv_nsec,
            static_cast<long>(Time::kNanosecondsPerSecond));  // NOLINT
  return TimeDelta(ts.tv_sec * Time::kMicrosecondsPerSecond +
                   ts.tv_nsec / Time::kNanosecondsPerMicrosecond);
}


struct timespec TimeDelta::ToTimespec() const {
  struct timespec ts;
  ts.tv_sec = delta_ / Time::kMicrosecondsPerSecond;
  ts.tv_nsec = (delta_ % Time::kMicrosecondsPerSecond) *
      Time::kNanosecondsPerMicrosecond;
  return ts;
}

#endif  // V8_OS_POSIX


#if V8_OS_WIN

// We implement time using the high-resolution timers so that we can get
// timeouts which are smaller than 10-15ms. To avoid any drift, we
// periodically resync the internal clock to the system clock.
class Clock FINAL {
 public:
  Clock() : initial_ticks_(GetSystemTicks()), initial_time_(GetSystemTime()) {}

  Time Now() {
    // Time between resampling the un-granular clock for this API (1 minute).
    const TimeDelta kMaxElapsedTime = TimeDelta::FromMinutes(1);

    LockGuard<Mutex> lock_guard(&mutex_);

    // Determine current time and ticks.
    TimeTicks ticks = GetSystemTicks();
    Time time = GetSystemTime();

    // Check if we need to synchronize with the system clock due to a backwards
    // time change or the amount of time elapsed.
    TimeDelta elapsed = ticks - initial_ticks_;
    if (time < initial_time_ || elapsed > kMaxElapsedTime) {
      initial_ticks_ = ticks;
      initial_time_ = time;
      return time;
    }

    return initial_time_ + elapsed;
  }

  Time NowFromSystemTime() {
    LockGuard<Mutex> lock_guard(&mutex_);
    initial_ticks_ = GetSystemTicks();
    initial_time_ = GetSystemTime();
    return initial_time_;
  }

 private:
  static TimeTicks GetSystemTicks() {
    return TimeTicks::Now();
  }

  static Time GetSystemTime() {
    FILETIME ft;
    ::GetSystemTimeAsFileTime(&ft);
    return Time::FromFiletime(ft);
  }

  TimeTicks initial_ticks_;
  Time initial_time_;
  Mutex mutex_;
};


static LazyStaticInstance<Clock, DefaultConstructTrait<Clock>,
                          ThreadSafeInitOnceTrait>::type clock =
    LAZY_STATIC_INSTANCE_INITIALIZER;


Time Time::Now() {
  return clock.Pointer()->Now();
}


Time Time::NowFromSystemTime() {
  return clock.Pointer()->NowFromSystemTime();
}


// Time between windows epoch and standard epoch.
static const int64_t kTimeToEpochInMicroseconds = V8_INT64_C(11644473600000000);


Time Time::FromFiletime(FILETIME ft) {
  if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0) {
    return Time();
  }
  if (ft.dwLowDateTime == std::numeric_limits<DWORD>::max() &&
      ft.dwHighDateTime == std::numeric_limits<DWORD>::max()) {
    return Max();
  }
  int64_t us = (static_cast<uint64_t>(ft.dwLowDateTime) +
                (static_cast<uint64_t>(ft.dwHighDateTime) << 32)) / 10;
  return Time(us - kTimeToEpochInMicroseconds);
}


FILETIME Time::ToFiletime() const {
  DCHECK(us_ >= 0);
  FILETIME ft;
  if (IsNull()) {
    ft.dwLowDateTime = 0;
    ft.dwHighDateTime = 0;
    return ft;
  }
  if (IsMax()) {
    ft.dwLowDateTime = std::numeric_limits<DWORD>::max();
    ft.dwHighDateTime = std::numeric_limits<DWORD>::max();
    return ft;
  }
  uint64_t us = static_cast<uint64_t>(us_ + kTimeToEpochInMicroseconds) * 10;
  ft.dwLowDateTime = static_cast<DWORD>(us);
  ft.dwHighDateTime = static_cast<DWORD>(us >> 32);
  return ft;
}

#elif V8_OS_POSIX

Time Time::Now() {
  struct timeval tv;
  int result = gettimeofday(&tv, NULL);
  DCHECK_EQ(0, result);
  USE(result);
  return FromTimeval(tv);
}


Time Time::NowFromSystemTime() {
  return Now();
}


Time Time::FromTimespec(struct timespec ts) {
  DCHECK(ts.tv_nsec >= 0);
  DCHECK(ts.tv_nsec < static_cast<long>(kNanosecondsPerSecond));  // NOLINT
  if (ts.tv_nsec == 0 && ts.tv_sec == 0) {
    return Time();
  }
  if (ts.tv_nsec == static_cast<long>(kNanosecondsPerSecond - 1) &&  // NOLINT
      ts.tv_sec == std::numeric_limits<time_t>::max()) {
    return Max();
  }
  return Time(ts.tv_sec * kMicrosecondsPerSecond +
              ts.tv_nsec / kNanosecondsPerMicrosecond);
}


struct timespec Time::ToTimespec() const {
  struct timespec ts;
  if (IsNull()) {
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    return ts;
  }
  if (IsMax()) {
    ts.tv_sec = std::numeric_limits<time_t>::max();
    ts.tv_nsec = static_cast<long>(kNanosecondsPerSecond - 1);  // NOLINT
    return ts;
  }
  ts.tv_sec = us_ / kMicrosecondsPerSecond;
  ts.tv_nsec = (us_ % kMicrosecondsPerSecond) * kNanosecondsPerMicrosecond;
  return ts;
}


Time Time::FromTimeval(struct timeval tv) {
  DCHECK(tv.tv_usec >= 0);
  DCHECK(tv.tv_usec < static_cast<suseconds_t>(kMicrosecondsPerSecond));
  if (tv.tv_usec == 0 && tv.tv_sec == 0) {
    return Time();
  }
  if (tv.tv_usec == static_cast<suseconds_t>(kMicrosecondsPerSecond - 1) &&
      tv.tv_sec == std::numeric_limits<time_t>::max()) {
    return Max();
  }
  return Time(tv.tv_sec * kMicrosecondsPerSecond + tv.tv_usec);
}


struct timeval Time::ToTimeval() const {
  struct timeval tv;
  if (IsNull()) {
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    return tv;
  }
  if (IsMax()) {
    tv.tv_sec = std::numeric_limits<time_t>::max();
    tv.tv_usec = static_cast<suseconds_t>(kMicrosecondsPerSecond - 1);
    return tv;
  }
  tv.tv_sec = us_ / kMicrosecondsPerSecond;
  tv.tv_usec = us_ % kMicrosecondsPerSecond;
  return tv;
}

#endif  // V8_OS_WIN


Time Time::FromJsTime(double ms_since_epoch) {
  // The epoch is a valid time, so this constructor doesn't interpret
  // 0 as the null time.
  if (ms_since_epoch == std::numeric_limits<double>::max()) {
    return Max();
  }
  return Time(
      static_cast<int64_t>(ms_since_epoch * kMicrosecondsPerMillisecond));
}


double Time::ToJsTime() const {
  if (IsNull()) {
    // Preserve 0 so the invalid result doesn't depend on the platform.
    return 0;
  }
  if (IsMax()) {
    // Preserve max without offset to prevent overflow.
    return std::numeric_limits<double>::max();
  }
  return static_cast<double>(us_) / kMicrosecondsPerMillisecond;
}


#if V8_OS_WIN

class TickClock {
 public:
  virtual ~TickClock() {}
  virtual int64_t Now() = 0;
  virtual bool IsHighResolution() = 0;
};


// Overview of time counters:
// (1) CPU cycle counter. (Retrieved via RDTSC)
// The CPU counter provides the highest resolution time stamp and is the least
// expensive to retrieve. However, the CPU counter is unreliable and should not
// be used in production. Its biggest issue is that it is per processor and it
// is not synchronized between processors. Also, on some computers, the counters
// will change frequency due to thermal and power changes, and stop in some
// states.
//
// (2) QueryPerformanceCounter (QPC). The QPC counter provides a high-
// resolution (100 nanoseconds) time stamp but is comparatively more expensive
// to retrieve. What QueryPerformanceCounter actually does is up to the HAL.
// (with some help from ACPI).
// According to http://blogs.msdn.com/oldnewthing/archive/2005/09/02/459952.aspx
// in the worst case, it gets the counter from the rollover interrupt on the
// programmable interrupt timer. In best cases, the HAL may conclude that the
// RDTSC counter runs at a constant frequency, then it uses that instead. On
// multiprocessor machines, it will try to verify the values returned from
// RDTSC on each processor are consistent with each other, and apply a handful
// of workarounds for known buggy hardware. In other words, QPC is supposed to
// give consistent result on a multiprocessor computer, but it is unreliable in
// reality due to bugs in BIOS or HAL on some, especially old computers.
// With recent updates on HAL and newer BIOS, QPC is getting more reliable but
// it should be used with caution.
//
// (3) System time. The system time provides a low-resolution (typically 10ms
// to 55 milliseconds) time stamp but is comparatively less expensive to
// retrieve and more reliable.
class HighResolutionTickClock FINAL : public TickClock {
 public:
  explicit HighResolutionTickClock(int64_t ticks_per_second)
      : ticks_per_second_(ticks_per_second) {
    DCHECK_LT(0, ticks_per_second);
  }
  virtual ~HighResolutionTickClock() {}

  int64_t Now() OVERRIDE {
    LARGE_INTEGER now;
    BOOL result = QueryPerformanceCounter(&now);
    DCHECK(result);
    USE(result);

    // Intentionally calculate microseconds in a round about manner to avoid
    // overflow and precision issues. Think twice before simplifying!
    int64_t whole_seconds = now.QuadPart / ticks_per_second_;
    int64_t leftover_ticks = now.QuadPart % ticks_per_second_;
    int64_t ticks = (whole_seconds * Time::kMicrosecondsPerSecond) +
        ((leftover_ticks * Time::kMicrosecondsPerSecond) / ticks_per_second_);

    // Make sure we never return 0 here, so that TimeTicks::HighResolutionNow()
    // will never return 0.
    return ticks + 1;
  }

  bool IsHighResolution() OVERRIDE { return true; }

 private:
  int64_t ticks_per_second_;
};


class RolloverProtectedTickClock FINAL : public TickClock {
 public:
  // We initialize rollover_ms_ to 1 to ensure that we will never
  // return 0 from TimeTicks::HighResolutionNow() and TimeTicks::Now() below.
  RolloverProtectedTickClock() : last_seen_now_(0), rollover_ms_(1) {}
  virtual ~RolloverProtectedTickClock() {}

  int64_t Now() OVERRIDE {
    LockGuard<Mutex> lock_guard(&mutex_);
    // We use timeGetTime() to implement TimeTicks::Now(), which rolls over
    // every ~49.7 days. We try to track rollover ourselves, which works if
    // TimeTicks::Now() is called at least every 49 days.
    // Note that we do not use GetTickCount() here, since timeGetTime() gives
    // more predictable delta values, as described here:
    // http://blogs.msdn.com/b/larryosterman/archive/2009/09/02/what-s-the-difference-between-gettickcount-and-timegettime.aspx
    // timeGetTime() provides 1ms granularity when combined with
    // timeBeginPeriod(). If the host application for V8 wants fast timers, it
    // can use timeBeginPeriod() to increase the resolution.
    DWORD now = timeGetTime();
    if (now < last_seen_now_) {
      rollover_ms_ += V8_INT64_C(0x100000000);  // ~49.7 days.
    }
    last_seen_now_ = now;
    return (now + rollover_ms_) * Time::kMicrosecondsPerMillisecond;
  }

  bool IsHighResolution() OVERRIDE { return false; }

 private:
  Mutex mutex_;
  DWORD last_seen_now_;
  int64_t rollover_ms_;
};


static LazyStaticInstance<RolloverProtectedTickClock,
                          DefaultConstructTrait<RolloverProtectedTickClock>,
                          ThreadSafeInitOnceTrait>::type tick_clock =
    LAZY_STATIC_INSTANCE_INITIALIZER;


struct CreateHighResTickClockTrait {
  static TickClock* Create() {
    // Check if the installed hardware supports a high-resolution performance
    // counter, and if not fallback to the low-resolution tick clock.
    LARGE_INTEGER ticks_per_second;
    if (!QueryPerformanceFrequency(&ticks_per_second)) {
      return tick_clock.Pointer();
    }

    // On Athlon X2 CPUs (e.g. model 15) the QueryPerformanceCounter
    // is unreliable, fallback to the low-resolution tick clock.
    CPU cpu;
    if (strcmp(cpu.vendor(), "AuthenticAMD") == 0 && cpu.family() == 15) {
      return tick_clock.Pointer();
    }

    return new HighResolutionTickClock(ticks_per_second.QuadPart);
  }
};


static LazyDynamicInstance<TickClock, CreateHighResTickClockTrait,
                           ThreadSafeInitOnceTrait>::type high_res_tick_clock =
    LAZY_DYNAMIC_INSTANCE_INITIALIZER;


TimeTicks TimeTicks::Now() {
  // Make sure we never return 0 here.
  TimeTicks ticks(tick_clock.Pointer()->Now());
  DCHECK(!ticks.IsNull());
  return ticks;
}


TimeTicks TimeTicks::HighResolutionNow() {
  // Make sure we never return 0 here.
  TimeTicks ticks(high_res_tick_clock.Pointer()->Now());
  DCHECK(!ticks.IsNull());
  return ticks;
}


// static
bool TimeTicks::IsHighResolutionClockWorking() {
  return high_res_tick_clock.Pointer()->IsHighResolution();
}


// static
TimeTicks TimeTicks::KernelTimestampNow() { return TimeTicks(0); }


// static
bool TimeTicks::KernelTimestampAvailable() { return false; }

#else  // V8_OS_WIN

TimeTicks TimeTicks::Now() {
  return HighResolutionNow();
}


TimeTicks TimeTicks::HighResolutionNow() {
  int64_t ticks;
#if V8_OS_MACOSX
  static struct mach_timebase_info info;
  if (info.denom == 0) {
    kern_return_t result = mach_timebase_info(&info);
    DCHECK_EQ(KERN_SUCCESS, result);
    USE(result);
  }
  ticks = (mach_absolute_time() / Time::kNanosecondsPerMicrosecond *
           info.numer / info.denom);
#elif V8_OS_SOLARIS
  ticks = (gethrtime() / Time::kNanosecondsPerMicrosecond);
#elif V8_LIBRT_NOT_AVAILABLE
  // TODO(bmeurer): This is a temporary hack to support cross-compiling
  // Chrome for Android in AOSP. Remove this once AOSP is fixed, also
  // cleanup the tools/gyp/v8.gyp file.
  struct timeval tv;
  int result = gettimeofday(&tv, NULL);
  DCHECK_EQ(0, result);
  USE(result);
  ticks = (tv.tv_sec * Time::kMicrosecondsPerSecond + tv.tv_usec);
#elif V8_OS_POSIX
  struct timespec ts;
  int result = clock_gettime(CLOCK_MONOTONIC, &ts);
  DCHECK_EQ(0, result);
  USE(result);
  ticks = (ts.tv_sec * Time::kMicrosecondsPerSecond +
           ts.tv_nsec / Time::kNanosecondsPerMicrosecond);
#endif  // V8_OS_MACOSX
  // Make sure we never return 0 here.
  return TimeTicks(ticks + 1);
}


// static
bool TimeTicks::IsHighResolutionClockWorking() {
  return true;
}


#if V8_OS_LINUX && !V8_LIBRT_NOT_AVAILABLE

class KernelTimestampClock {
 public:
  KernelTimestampClock() : clock_fd_(-1), clock_id_(kClockInvalid) {
    clock_fd_ = open(kTraceClockDevice, O_RDONLY);
    if (clock_fd_ == -1) {
      return;
    }
    clock_id_ = get_clockid(clock_fd_);
  }

  virtual ~KernelTimestampClock() {
    if (clock_fd_ != -1) {
      close(clock_fd_);
    }
  }

  int64_t Now() {
    if (clock_id_ == kClockInvalid) {
      return 0;
    }

    struct timespec ts;

    clock_gettime(clock_id_, &ts);
    return ((int64_t)ts.tv_sec * kNsecPerSec) + ts.tv_nsec;
  }

  bool Available() { return clock_id_ != kClockInvalid; }

 private:
  static const clockid_t kClockInvalid = -1;
  static const char kTraceClockDevice[];
  static const uint64_t kNsecPerSec = 1000000000;

  int clock_fd_;
  clockid_t clock_id_;

  static int get_clockid(int fd) { return ((~(clockid_t)(fd) << 3) | 3); }
};


// Timestamp module name
const char KernelTimestampClock::kTraceClockDevice[] = "/dev/trace_clock";

#else

class KernelTimestampClock {
 public:
  KernelTimestampClock() {}

  int64_t Now() { return 0; }
  bool Available() { return false; }
};

#endif  // V8_OS_LINUX && !V8_LIBRT_NOT_AVAILABLE

static LazyStaticInstance<KernelTimestampClock,
                          DefaultConstructTrait<KernelTimestampClock>,
                          ThreadSafeInitOnceTrait>::type kernel_tick_clock =
    LAZY_STATIC_INSTANCE_INITIALIZER;


// static
TimeTicks TimeTicks::KernelTimestampNow() {
  return TimeTicks(kernel_tick_clock.Pointer()->Now());
}


// static
bool TimeTicks::KernelTimestampAvailable() {
  return kernel_tick_clock.Pointer()->Available();
}

#endif  // V8_OS_WIN

} }  // namespace v8::base
