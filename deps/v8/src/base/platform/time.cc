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
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pthread.h>
#endif

#include <cstring>
#include <ostream>

#if V8_OS_WIN
#include "src/base/atomicops.h"
#include "src/base/lazy-instance.h"
#include "src/base/win32-headers.h"
#endif
#include "src/base/cpu.h"
#include "src/base/logging.h"
#include "src/base/platform/platform.h"

namespace {

#if V8_OS_MACOSX
int64_t ComputeThreadTicks() {
  mach_msg_type_number_t thread_info_count = THREAD_BASIC_INFO_COUNT;
  thread_basic_info_data_t thread_info_data;
  kern_return_t kr = thread_info(
      pthread_mach_thread_np(pthread_self()),
      THREAD_BASIC_INFO,
      reinterpret_cast<thread_info_t>(&thread_info_data),
      &thread_info_count);
  CHECK_EQ(kr, KERN_SUCCESS);

  v8::base::CheckedNumeric<int64_t> absolute_micros(
      thread_info_data.user_time.seconds +
      thread_info_data.system_time.seconds);
  absolute_micros *= v8::base::Time::kMicrosecondsPerSecond;
  absolute_micros += (thread_info_data.user_time.microseconds +
                      thread_info_data.system_time.microseconds);
  return absolute_micros.ValueOrDie();
}
#elif V8_OS_POSIX
// Helper function to get results from clock_gettime() and convert to a
// microsecond timebase. Minimum requirement is MONOTONIC_CLOCK to be supported
// on the system. FreeBSD 6 has CLOCK_MONOTONIC but defines
// _POSIX_MONOTONIC_CLOCK to -1.
V8_INLINE int64_t ClockNow(clockid_t clk_id) {
#if (defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK >= 0) || \
  defined(V8_OS_BSD) || defined(V8_OS_ANDROID)
// On AIX clock_gettime for CLOCK_THREAD_CPUTIME_ID outputs time with
// resolution of 10ms. thread_cputime API provides the time in ns
#if defined(V8_OS_AIX)
  thread_cputime_t tc;
  if (clk_id == CLOCK_THREAD_CPUTIME_ID) {
    if (thread_cputime(-1, &tc) != 0) {
      UNREACHABLE();
    }
  }
#endif
  struct timespec ts;
  if (clock_gettime(clk_id, &ts) != 0) {
    UNREACHABLE();
  }
  v8::base::internal::CheckedNumeric<int64_t> result(ts.tv_sec);
  result *= v8::base::Time::kMicrosecondsPerSecond;
#if defined(V8_OS_AIX)
  if (clk_id == CLOCK_THREAD_CPUTIME_ID) {
    result += (tc.stime / v8::base::Time::kNanosecondsPerMicrosecond);
  } else {
    result += (ts.tv_nsec / v8::base::Time::kNanosecondsPerMicrosecond);
  }
#else
  result += (ts.tv_nsec / v8::base::Time::kNanosecondsPerMicrosecond);
#endif
  return result.ValueOrDie();
#else  // Monotonic clock not supported.
  return 0;
#endif
}
#elif V8_OS_WIN
V8_INLINE bool IsQPCReliable() {
  v8::base::CPU cpu;
  // On Athlon X2 CPUs (e.g. model 15) QueryPerformanceCounter is unreliable.
  return strcmp(cpu.vendor(), "AuthenticAMD") == 0 && cpu.family() == 15;
}

// Returns the current value of the performance counter.
V8_INLINE uint64_t QPCNowRaw() {
  LARGE_INTEGER perf_counter_now = {};
  // According to the MSDN documentation for QueryPerformanceCounter(), this
  // will never fail on systems that run XP or later.
  // https://msdn.microsoft.com/library/windows/desktop/ms644904.aspx
  BOOL result = ::QueryPerformanceCounter(&perf_counter_now);
  DCHECK(result);
  USE(result);
  return perf_counter_now.QuadPart;
}
#endif  // V8_OS_MACOSX


}  // namespace

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
  DCHECK_GE(delta_, 0);
  ts.tv_sec = static_cast<unsigned>(delta_ / Time::kMicrosecondsPerSecond);
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
  ts.tv_sec = static_cast<time_t>(delta_ / Time::kMicrosecondsPerSecond);
  ts.tv_nsec = (delta_ % Time::kMicrosecondsPerSecond) *
      Time::kNanosecondsPerMicrosecond;
  return ts;
}

#endif  // V8_OS_POSIX


#if V8_OS_WIN

// We implement time using the high-resolution timers so that we can get
// timeouts which are smaller than 10-15ms. To avoid any drift, we
// periodically resync the internal clock to the system clock.
class Clock final {
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
static const int64_t kTimeToEpochInMicroseconds = int64_t{11644473600000000};

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
  DCHECK_GE(us_, 0);
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
  int result = gettimeofday(&tv, nullptr);
  DCHECK_EQ(0, result);
  USE(result);
  return FromTimeval(tv);
}


Time Time::NowFromSystemTime() {
  return Now();
}


Time Time::FromTimespec(struct timespec ts) {
  DCHECK_GE(ts.tv_nsec, 0);
  DCHECK_LT(ts.tv_nsec, kNanosecondsPerSecond);
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
  ts.tv_sec = static_cast<time_t>(us_ / kMicrosecondsPerSecond);
  ts.tv_nsec = (us_ % kMicrosecondsPerSecond) * kNanosecondsPerMicrosecond;
  return ts;
}


Time Time::FromTimeval(struct timeval tv) {
  DCHECK_GE(tv.tv_usec, 0);
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
  tv.tv_sec = static_cast<time_t>(us_ / kMicrosecondsPerSecond);
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


std::ostream& operator<<(std::ostream& os, const Time& time) {
  return os << time.ToJsTime();
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
class HighResolutionTickClock final : public TickClock {
 public:
  explicit HighResolutionTickClock(int64_t ticks_per_second)
      : ticks_per_second_(ticks_per_second) {
    DCHECK_LT(0, ticks_per_second);
  }
  virtual ~HighResolutionTickClock() {}

  int64_t Now() override {
    uint64_t now = QPCNowRaw();

    // Intentionally calculate microseconds in a round about manner to avoid
    // overflow and precision issues. Think twice before simplifying!
    int64_t whole_seconds = now / ticks_per_second_;
    int64_t leftover_ticks = now % ticks_per_second_;
    int64_t ticks = (whole_seconds * Time::kMicrosecondsPerSecond) +
        ((leftover_ticks * Time::kMicrosecondsPerSecond) / ticks_per_second_);

    // Make sure we never return 0 here, so that TimeTicks::HighResolutionNow()
    // will never return 0.
    return ticks + 1;
  }

  bool IsHighResolution() override { return true; }

 private:
  int64_t ticks_per_second_;
};


class RolloverProtectedTickClock final : public TickClock {
 public:
  RolloverProtectedTickClock() : rollover_(0) {}
  virtual ~RolloverProtectedTickClock() {}

  int64_t Now() override {
    // We use timeGetTime() to implement TimeTicks::Now(), which rolls over
    // every ~49.7 days. We try to track rollover ourselves, which works if
    // TimeTicks::Now() is called at least every 24 days.
    // Note that we do not use GetTickCount() here, since timeGetTime() gives
    // more predictable delta values, as described here:
    // http://blogs.msdn.com/b/larryosterman/archive/2009/09/02/what-s-the-difference-between-gettickcount-and-timegettime.aspx
    // timeGetTime() provides 1ms granularity when combined with
    // timeBeginPeriod(). If the host application for V8 wants fast timers, it
    // can use timeBeginPeriod() to increase the resolution.
    // We use a lock-free version because the sampler thread calls it
    // while having the rest of the world stopped, that could cause a deadlock.
    base::Atomic32 rollover = base::Acquire_Load(&rollover_);
    uint32_t now = static_cast<uint32_t>(timeGetTime());
    if ((now >> 31) != static_cast<uint32_t>(rollover & 1)) {
      base::Release_CompareAndSwap(&rollover_, rollover, rollover + 1);
      ++rollover;
    }
    uint64_t ms = (static_cast<uint64_t>(rollover) << 31) | now;
    return static_cast<int64_t>(ms * Time::kMicrosecondsPerMillisecond);
  }

  bool IsHighResolution() override { return false; }

 private:
  base::Atomic32 rollover_;
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

    // If QPC not reliable, fallback to low-resolution tick clock.
    if (IsQPCReliable()) {
      return tick_clock.Pointer();
    }

    return new HighResolutionTickClock(ticks_per_second.QuadPart);
  }
};


static LazyDynamicInstance<TickClock, CreateHighResTickClockTrait,
                           ThreadSafeInitOnceTrait>::type high_res_tick_clock =
    LAZY_DYNAMIC_INSTANCE_INITIALIZER;

// static
TimeTicks TimeTicks::Now() {
  // Make sure we never return 0 here.
  TimeTicks ticks(tick_clock.Pointer()->Now());
  DCHECK(!ticks.IsNull());
  return ticks;
}

// static
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
#elif V8_OS_POSIX
  ticks = ClockNow(CLOCK_MONOTONIC);
#else
#error platform does not implement TimeTicks::HighResolutionNow.
#endif  // V8_OS_MACOSX
  // Make sure we never return 0 here.
  return TimeTicks(ticks + 1);
}


// static
bool TimeTicks::IsHighResolutionClockWorking() {
  return true;
}

#endif  // V8_OS_WIN


bool ThreadTicks::IsSupported() {
#if (defined(_POSIX_THREAD_CPUTIME) && (_POSIX_THREAD_CPUTIME >= 0)) || \
    defined(V8_OS_MACOSX) || defined(V8_OS_ANDROID) || defined(V8_OS_SOLARIS)
  return true;
#elif defined(V8_OS_WIN)
  return IsSupportedWin();
#else
  return false;
#endif
}


ThreadTicks ThreadTicks::Now() {
#if V8_OS_MACOSX
  return ThreadTicks(ComputeThreadTicks());
#elif(defined(_POSIX_THREAD_CPUTIME) && (_POSIX_THREAD_CPUTIME >= 0)) || \
  defined(V8_OS_ANDROID)
  return ThreadTicks(ClockNow(CLOCK_THREAD_CPUTIME_ID));
#elif V8_OS_SOLARIS
  return ThreadTicks(gethrvtime() / Time::kNanosecondsPerMicrosecond);
#elif V8_OS_WIN
  return ThreadTicks::GetForThread(::GetCurrentThread());
#else
  UNREACHABLE();
#endif
}


#if V8_OS_WIN
ThreadTicks ThreadTicks::GetForThread(const HANDLE& thread_handle) {
  DCHECK(IsSupported());

  // Get the number of TSC ticks used by the current thread.
  ULONG64 thread_cycle_time = 0;
  ::QueryThreadCycleTime(thread_handle, &thread_cycle_time);

  // Get the frequency of the TSC.
  double tsc_ticks_per_second = TSCTicksPerSecond();
  if (tsc_ticks_per_second == 0)
    return ThreadTicks();

  // Return the CPU time of the current thread.
  double thread_time_seconds = thread_cycle_time / tsc_ticks_per_second;
  return ThreadTicks(
      static_cast<int64_t>(thread_time_seconds * Time::kMicrosecondsPerSecond));
}

// static
bool ThreadTicks::IsSupportedWin() {
  static bool is_supported = base::CPU().has_non_stop_time_stamp_counter() &&
                             !IsQPCReliable();
  return is_supported;
}

// static
void ThreadTicks::WaitUntilInitializedWin() {
  while (TSCTicksPerSecond() == 0)
    ::Sleep(10);
}

double ThreadTicks::TSCTicksPerSecond() {
  DCHECK(IsSupported());

  // The value returned by QueryPerformanceFrequency() cannot be used as the TSC
  // frequency, because there is no guarantee that the TSC frequency is equal to
  // the performance counter frequency.

  // The TSC frequency is cached in a static variable because it takes some time
  // to compute it.
  static double tsc_ticks_per_second = 0;
  if (tsc_ticks_per_second != 0)
    return tsc_ticks_per_second;

  // Increase the thread priority to reduces the chances of having a context
  // switch during a reading of the TSC and the performance counter.
  int previous_priority = ::GetThreadPriority(::GetCurrentThread());
  ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

  // The first time that this function is called, make an initial reading of the
  // TSC and the performance counter.
  static const uint64_t tsc_initial = __rdtsc();
  static const uint64_t perf_counter_initial = QPCNowRaw();

  // Make a another reading of the TSC and the performance counter every time
  // that this function is called.
  uint64_t tsc_now = __rdtsc();
  uint64_t perf_counter_now = QPCNowRaw();

  // Reset the thread priority.
  ::SetThreadPriority(::GetCurrentThread(), previous_priority);

  // Make sure that at least 50 ms elapsed between the 2 readings. The first
  // time that this function is called, we don't expect this to be the case.
  // Note: The longer the elapsed time between the 2 readings is, the more
  //   accurate the computed TSC frequency will be. The 50 ms value was
  //   chosen because local benchmarks show that it allows us to get a
  //   stddev of less than 1 tick/us between multiple runs.
  // Note: According to the MSDN documentation for QueryPerformanceFrequency(),
  //   this will never fail on systems that run XP or later.
  //   https://msdn.microsoft.com/library/windows/desktop/ms644905.aspx
  LARGE_INTEGER perf_counter_frequency = {};
  ::QueryPerformanceFrequency(&perf_counter_frequency);
  DCHECK_GE(perf_counter_now, perf_counter_initial);
  uint64_t perf_counter_ticks = perf_counter_now - perf_counter_initial;
  double elapsed_time_seconds =
      perf_counter_ticks / static_cast<double>(perf_counter_frequency.QuadPart);

  const double kMinimumEvaluationPeriodSeconds = 0.05;
  if (elapsed_time_seconds < kMinimumEvaluationPeriodSeconds)
    return 0;

  // Compute the frequency of the TSC.
  DCHECK_GE(tsc_now, tsc_initial);
  uint64_t tsc_ticks = tsc_now - tsc_initial;
  tsc_ticks_per_second = tsc_ticks / elapsed_time_seconds;

  return tsc_ticks_per_second;
}
#endif  // V8_OS_WIN

}  // namespace base
}  // namespace v8
