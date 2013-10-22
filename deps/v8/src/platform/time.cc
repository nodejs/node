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

#include "platform/time.h"

#if V8_OS_POSIX
#include <sys/time.h>
#endif
#if V8_OS_MACOSX
#include <mach/mach_time.h>
#endif

#include <cstring>

#include "checks.h"
#include "cpu.h"
#include "platform.h"
#if V8_OS_WIN
#include "win32-headers.h"
#endif

#if V8_OS_WIN
// Prototype for GetTickCount64() procedure.
extern "C" {
typedef ULONGLONG (WINAPI *GETTICKCOUNT64PROC)(void);
}
#endif

namespace v8 {
namespace internal {

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
  ASSERT_GE(ts.tv_nsec, 0);
  ASSERT_LT(ts.tv_nsec,
            static_cast<long>(Time::kNanosecondsPerSecond));  // NOLINT
  return TimeDelta(ts.tv_sec * Time::kMicrosecondsPerSecond +
                   ts.tv_nsec / Time::kNanosecondsPerMicrosecond);
}


struct mach_timespec TimeDelta::ToMachTimespec() const {
  struct mach_timespec ts;
  ASSERT(delta_ >= 0);
  ts.tv_sec = delta_ / Time::kMicrosecondsPerSecond;
  ts.tv_nsec = (delta_ % Time::kMicrosecondsPerSecond) *
      Time::kNanosecondsPerMicrosecond;
  return ts;
}

#endif  // V8_OS_MACOSX


#if V8_OS_POSIX

TimeDelta TimeDelta::FromTimespec(struct timespec ts) {
  ASSERT_GE(ts.tv_nsec, 0);
  ASSERT_LT(ts.tv_nsec,
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
class Clock V8_FINAL {
 public:
  Clock() : initial_time_(CurrentWallclockTime()),
            initial_ticks_(TimeTicks::Now()) {}

  Time Now() {
    // This must be executed under lock.
    LockGuard<Mutex> lock_guard(&mutex_);

    // Calculate the time elapsed since we started our timer.
    TimeDelta elapsed = TimeTicks::Now() - initial_ticks_;

    // Check if we don't need to synchronize with the wallclock yet.
    if (elapsed.InMicroseconds() <= kMaxMicrosecondsToAvoidDrift) {
      return initial_time_ + elapsed;
    }

    // Resynchronize with the wallclock.
    initial_ticks_ = TimeTicks::Now();
    initial_time_ = CurrentWallclockTime();
    return initial_time_;
  }

  Time NowFromSystemTime() {
    // This must be executed under lock.
    LockGuard<Mutex> lock_guard(&mutex_);

    // Resynchronize with the wallclock.
    initial_ticks_ = TimeTicks::Now();
    initial_time_ = CurrentWallclockTime();
    return initial_time_;
  }

 private:
  // Time between resampling the un-granular clock for this API (1 minute).
  static const int64_t kMaxMicrosecondsToAvoidDrift =
      Time::kMicrosecondsPerMinute;

  static Time CurrentWallclockTime() {
    FILETIME ft;
    ::GetSystemTimeAsFileTime(&ft);
    return Time::FromFiletime(ft);
  }

  TimeTicks initial_ticks_;
  Time initial_time_;
  Mutex mutex_;
};


static LazyDynamicInstance<Clock,
    DefaultCreateTrait<Clock>,
    ThreadSafeInitOnceTrait>::type clock = LAZY_DYNAMIC_INSTANCE_INITIALIZER;


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
  ASSERT(us_ >= 0);
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
  ASSERT_EQ(0, result);
  USE(result);
  return FromTimeval(tv);
}


Time Time::NowFromSystemTime() {
  return Now();
}


Time Time::FromTimespec(struct timespec ts) {
  ASSERT(ts.tv_nsec >= 0);
  ASSERT(ts.tv_nsec < static_cast<long>(kNanosecondsPerSecond));  // NOLINT
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
  ASSERT(tv.tv_usec >= 0);
  ASSERT(tv.tv_usec < static_cast<suseconds_t>(kMicrosecondsPerSecond));
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
class HighResolutionTickClock V8_FINAL : public TickClock {
 public:
  explicit HighResolutionTickClock(int64_t ticks_per_second)
      : ticks_per_second_(ticks_per_second) {
    ASSERT_LT(0, ticks_per_second);
  }
  virtual ~HighResolutionTickClock() {}

  virtual int64_t Now() V8_OVERRIDE {
    LARGE_INTEGER now;
    BOOL result = QueryPerformanceCounter(&now);
    ASSERT(result);
    USE(result);

    // Intentionally calculate microseconds in a round about manner to avoid
    // overflow and precision issues. Think twice before simplifying!
    int64_t whole_seconds = now.QuadPart / ticks_per_second_;
    int64_t leftover_ticks = now.QuadPart % ticks_per_second_;
    int64_t ticks = (whole_seconds * Time::kMicrosecondsPerSecond) +
        ((leftover_ticks * Time::kMicrosecondsPerSecond) / ticks_per_second_);

    // Make sure we never return 0 here, so that TimeTicks::HighResNow()
    // will never return 0.
    return ticks + 1;
  }

 private:
  int64_t ticks_per_second_;
};


// The GetTickCount64() API is what we actually want for the regular tick
// clock, but this is only available starting with Windows Vista.
class WindowsVistaTickClock V8_FINAL : public TickClock {
 public:
  explicit WindowsVistaTickClock(GETTICKCOUNT64PROC func) : func_(func) {
    ASSERT(func_ != NULL);
  }
  virtual ~WindowsVistaTickClock() {}

  virtual int64_t Now() V8_OVERRIDE {
    // Query the current ticks (in ms).
    ULONGLONG tick_count_ms = (*func_)();

    // Convert to microseconds (make sure to never return 0 here).
    return (tick_count_ms * Time::kMicrosecondsPerMillisecond) + 1;
  }

 private:
  GETTICKCOUNT64PROC func_;
};


class RolloverProtectedTickClock V8_FINAL : public TickClock {
 public:
  // We initialize rollover_ms_ to 1 to ensure that we will never
  // return 0 from TimeTicks::HighResNow() and TimeTicks::Now() below.
  RolloverProtectedTickClock() : last_seen_now_(0), rollover_ms_(1) {}
  virtual ~RolloverProtectedTickClock() {}

  virtual int64_t Now() V8_OVERRIDE {
    LockGuard<Mutex> lock_guard(&mutex_);
    // We use timeGetTime() to implement TimeTicks::Now(), which rolls over
    // every ~49.7 days. We try to track rollover ourselves, which works if
    // TimeTicks::Now() is called at least every 49 days.
    // Note that we do not use GetTickCount() here, since timeGetTime() gives
    // more predictable delta values, as described here:
    // http://blogs.msdn.com/b/larryosterman/archive/2009/09/02/what-s-the-difference-between-gettickcount-and-timegettime.aspx
    DWORD now = timeGetTime();
    if (now < last_seen_now_) {
      rollover_ms_ += V8_INT64_C(0x100000000);  // ~49.7 days.
    }
    last_seen_now_ = now;
    return (now + rollover_ms_) * Time::kMicrosecondsPerMillisecond;
  }

 private:
  Mutex mutex_;
  DWORD last_seen_now_;
  int64_t rollover_ms_;
};


struct CreateTickClockTrait {
  static TickClock* Create() {
    // Try to load GetTickCount64() from kernel32.dll (available since Vista).
    HMODULE kernel32 = ::GetModuleHandleA("kernel32.dll");
    ASSERT(kernel32 != NULL);
    FARPROC proc = ::GetProcAddress(kernel32, "GetTickCount64");
    if (proc != NULL) {
      return new WindowsVistaTickClock(
          reinterpret_cast<GETTICKCOUNT64PROC>(proc));
    }

    // Fallback to the rollover protected tick clock.
    return new RolloverProtectedTickClock;
  }
};


static LazyDynamicInstance<TickClock,
    CreateTickClockTrait,
    ThreadSafeInitOnceTrait>::type tick_clock =
        LAZY_DYNAMIC_INSTANCE_INITIALIZER;


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


static LazyDynamicInstance<TickClock,
    CreateHighResTickClockTrait,
    ThreadSafeInitOnceTrait>::type high_res_tick_clock =
        LAZY_DYNAMIC_INSTANCE_INITIALIZER;


TimeTicks TimeTicks::Now() {
  // Make sure we never return 0 here.
  TimeTicks ticks(tick_clock.Pointer()->Now());
  ASSERT(!ticks.IsNull());
  return ticks;
}


TimeTicks TimeTicks::HighResNow() {
  // Make sure we never return 0 here.
  TimeTicks ticks(high_res_tick_clock.Pointer()->Now());
  ASSERT(!ticks.IsNull());
  return ticks;
}

#else  // V8_OS_WIN

TimeTicks TimeTicks::Now() {
  return HighResNow();
}


TimeTicks TimeTicks::HighResNow() {
  int64_t ticks;
#if V8_OS_MACOSX
  static struct mach_timebase_info info;
  if (info.denom == 0) {
    kern_return_t result = mach_timebase_info(&info);
    ASSERT_EQ(KERN_SUCCESS, result);
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
  ASSERT_EQ(0, result);
  USE(result);
  ticks = (tv.tv_sec * Time::kMicrosecondsPerSecond + tv.tv_usec);
#elif V8_OS_POSIX
  struct timespec ts;
  int result = clock_gettime(CLOCK_MONOTONIC, &ts);
  ASSERT_EQ(0, result);
  USE(result);
  ticks = (ts.tv_sec * Time::kMicrosecondsPerSecond +
           ts.tv_nsec / Time::kNanosecondsPerMicrosecond);
#endif  // V8_OS_MACOSX
  // Make sure we never return 0 here.
  return TimeTicks(ticks + 1);
}

#endif  // V8_OS_WIN

} }  // namespace v8::internal
