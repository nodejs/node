// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PLATFORM_TIME_H_
#define V8_BASE_PLATFORM_TIME_H_

#include <stdint.h>

#include <ctime>
#include <iosfwd>
#include <limits>

#include "src/base/base-export.h"
#include "src/base/bits.h"
#include "src/base/macros.h"
#include "src/base/safe_math.h"
#if V8_OS_WIN
#include "src/base/win32-headers.h"
#endif

// Forward declarations.
extern "C" {
struct _FILETIME;
struct mach_timespec;
struct timespec;
struct timeval;
}

namespace v8 {
namespace base {

class Time;
class TimeDelta;
class TimeTicks;

namespace time_internal {
template<class TimeClass>
class TimeBase;
}

class TimeConstants {
 public:
  static constexpr int64_t kHoursPerDay = 24;
  static constexpr int64_t kMillisecondsPerSecond = 1000;
  static constexpr int64_t kMillisecondsPerDay =
      kMillisecondsPerSecond * 60 * 60 * kHoursPerDay;
  static constexpr int64_t kMicrosecondsPerMillisecond = 1000;
  static constexpr int64_t kMicrosecondsPerSecond =
      kMicrosecondsPerMillisecond * kMillisecondsPerSecond;
  static constexpr int64_t kMicrosecondsPerMinute = kMicrosecondsPerSecond * 60;
  static constexpr int64_t kMicrosecondsPerHour = kMicrosecondsPerMinute * 60;
  static constexpr int64_t kMicrosecondsPerDay =
      kMicrosecondsPerHour * kHoursPerDay;
  static constexpr int64_t kMicrosecondsPerWeek = kMicrosecondsPerDay * 7;
  static constexpr int64_t kNanosecondsPerMicrosecond = 1000;
  static constexpr int64_t kNanosecondsPerSecond =
      kNanosecondsPerMicrosecond * kMicrosecondsPerSecond;
};

// -----------------------------------------------------------------------------
// TimeDelta
//
// This class represents a duration of time, internally represented in
// microseonds.

class V8_BASE_EXPORT TimeDelta final {
 public:
  constexpr TimeDelta() : delta_(0) {}

  // Converts units of time to TimeDeltas.
  static constexpr TimeDelta FromDays(int days) {
    return TimeDelta(days * TimeConstants::kMicrosecondsPerDay);
  }
  static constexpr TimeDelta FromHours(int hours) {
    return TimeDelta(hours * TimeConstants::kMicrosecondsPerHour);
  }
  static constexpr TimeDelta FromMinutes(int minutes) {
    return TimeDelta(minutes * TimeConstants::kMicrosecondsPerMinute);
  }
  static constexpr TimeDelta FromSeconds(int64_t seconds) {
    return TimeDelta(seconds * TimeConstants::kMicrosecondsPerSecond);
  }
  static constexpr TimeDelta FromMilliseconds(int64_t milliseconds) {
    return TimeDelta(milliseconds * TimeConstants::kMicrosecondsPerMillisecond);
  }
  static constexpr TimeDelta FromMicroseconds(int64_t microseconds) {
    return TimeDelta(microseconds);
  }
  static constexpr TimeDelta FromNanoseconds(int64_t nanoseconds) {
    return TimeDelta(nanoseconds / TimeConstants::kNanosecondsPerMicrosecond);
  }

  // Returns the maximum time delta, which should be greater than any reasonable
  // time delta we might compare it to. Adding or subtracting the maximum time
  // delta to a time or another time delta has an undefined result.
  static constexpr TimeDelta Max();

  // Returns the minimum time delta, which should be less than than any
  // reasonable time delta we might compare it to. Adding or subtracting the
  // minimum time delta to a time or another time delta has an undefined result.
  static constexpr TimeDelta Min();

  // Returns true if the time delta is zero.
  constexpr bool IsZero() const { return delta_ == 0; }

  // Returns true if the time delta is the maximum/minimum time delta.
  constexpr bool IsMax() const {
    return delta_ == std::numeric_limits<int64_t>::max();
  }
  constexpr bool IsMin() const {
    return delta_ == std::numeric_limits<int64_t>::min();
  }

  // Returns the time delta in some unit. The F versions return a floating
  // point value, the "regular" versions return a rounded-down value.
  //
  // InMillisecondsRoundedUp() instead returns an integer that is rounded up
  // to the next full millisecond.
  int InDays() const;
  int InHours() const;
  int InMinutes() const;
  double InSecondsF() const;
  int64_t InSeconds() const;
  double InMillisecondsF() const;
  int64_t InMilliseconds() const;
  int64_t InMillisecondsRoundedUp() const;
  int64_t InMicroseconds() const;
  int64_t InNanoseconds() const;

  // Converts to/from Mach time specs.
  static TimeDelta FromMachTimespec(struct mach_timespec ts);
  struct mach_timespec ToMachTimespec() const;

  // Converts to/from POSIX time specs.
  static TimeDelta FromTimespec(struct timespec ts);
  struct timespec ToTimespec() const;

  // Computations with other deltas.
  TimeDelta operator+(const TimeDelta& other) const {
    return TimeDelta(delta_ + other.delta_);
  }
  TimeDelta operator-(const TimeDelta& other) const {
    return TimeDelta(delta_ - other.delta_);
  }

  TimeDelta& operator+=(const TimeDelta& other) {
    delta_ += other.delta_;
    return *this;
  }
  TimeDelta& operator-=(const TimeDelta& other) {
    delta_ -= other.delta_;
    return *this;
  }
  constexpr TimeDelta operator-() const { return TimeDelta(-delta_); }

  double TimesOf(const TimeDelta& other) const {
    return static_cast<double>(delta_) / static_cast<double>(other.delta_);
  }
  double PercentOf(const TimeDelta& other) const {
    return TimesOf(other) * 100.0;
  }

  // Computations with ints, note that we only allow multiplicative operations
  // with ints, and additive operations with other deltas.
  TimeDelta operator*(int64_t a) const {
    return TimeDelta(delta_ * a);
  }
  TimeDelta operator/(int64_t a) const {
    return TimeDelta(delta_ / a);
  }
  TimeDelta& operator*=(int64_t a) {
    delta_ *= a;
    return *this;
  }
  TimeDelta& operator/=(int64_t a) {
    delta_ /= a;
    return *this;
  }
  int64_t operator/(const TimeDelta& other) const {
    return delta_ / other.delta_;
  }

  // Comparison operators.
  constexpr bool operator==(const TimeDelta& other) const {
    return delta_ == other.delta_;
  }
  constexpr bool operator!=(const TimeDelta& other) const {
    return delta_ != other.delta_;
  }
  constexpr bool operator<(const TimeDelta& other) const {
    return delta_ < other.delta_;
  }
  constexpr bool operator<=(const TimeDelta& other) const {
    return delta_ <= other.delta_;
  }
  constexpr bool operator>(const TimeDelta& other) const {
    return delta_ > other.delta_;
  }
  constexpr bool operator>=(const TimeDelta& other) const {
    return delta_ >= other.delta_;
  }

 private:
  template<class TimeClass> friend class time_internal::TimeBase;
  // Constructs a delta given the duration in microseconds. This is private
  // to avoid confusion by callers with an integer constructor. Use
  // FromSeconds, FromMilliseconds, etc. instead.
  explicit constexpr TimeDelta(int64_t delta) : delta_(delta) {}

  // Delta in microseconds.
  int64_t delta_;
};

// static
constexpr TimeDelta TimeDelta::Max() {
  return TimeDelta(std::numeric_limits<int64_t>::max());
}

// static
constexpr TimeDelta TimeDelta::Min() {
  return TimeDelta(std::numeric_limits<int64_t>::min());
}

namespace time_internal {

// TimeBase--------------------------------------------------------------------

// Provides value storage and comparison/math operations common to all time
// classes. Each subclass provides for strong type-checking to ensure
// semantically meaningful comparison/math of time values from the same clock
// source or timeline.
template <class TimeClass>
class TimeBase : public TimeConstants {
 public:
#if V8_OS_WIN
  // To avoid overflow in QPC to Microseconds calculations, since we multiply
  // by kMicrosecondsPerSecond, then the QPC value should not exceed
  // (2^63 - 1) / 1E6. If it exceeds that threshold, we divide then multiply.
  static constexpr int64_t kQPCOverflowThreshold = INT64_C(0x8637BD05AF7);
#endif

  // Returns true if this object has not been initialized.
  //
  // Warning: Be careful when writing code that performs math on time values,
  // since it's possible to produce a valid "zero" result that should not be
  // interpreted as a "null" value.
  constexpr bool IsNull() const { return us_ == 0; }

  // Returns the maximum/minimum times, which should be greater/less than any
  // reasonable time with which we might compare it.
  static TimeClass Max() {
    return TimeClass(std::numeric_limits<int64_t>::max());
  }
  static TimeClass Min() {
    return TimeClass(std::numeric_limits<int64_t>::min());
  }

  // Returns true if this object represents the maximum/minimum time.
  constexpr bool IsMax() const {
    return us_ == std::numeric_limits<int64_t>::max();
  }
  constexpr bool IsMin() const {
    return us_ == std::numeric_limits<int64_t>::min();
  }

  // For serializing only. Use FromInternalValue() to reconstitute. Please don't
  // use this and do arithmetic on it, as it is more error prone than using the
  // provided operators.
  int64_t ToInternalValue() const { return us_; }

  TimeClass& operator=(TimeClass other) {
    us_ = other.us_;
    return *(static_cast<TimeClass*>(this));
  }

  // Compute the difference between two times.
  TimeDelta operator-(TimeClass other) const {
    return TimeDelta::FromMicroseconds(us_ - other.us_);
  }

  // Return a new time modified by some delta.
  TimeClass operator+(TimeDelta delta) const {
    return TimeClass(bits::SignedSaturatedAdd64(delta.delta_, us_));
  }
  TimeClass operator-(TimeDelta delta) const {
    return TimeClass(-bits::SignedSaturatedSub64(delta.delta_, us_));
  }

  // Modify by some time delta.
  TimeClass& operator+=(TimeDelta delta) {
    return static_cast<TimeClass&>(*this = (*this + delta));
  }
  TimeClass& operator-=(TimeDelta delta) {
    return static_cast<TimeClass&>(*this = (*this - delta));
  }

  // Comparison operators
  bool operator==(TimeClass other) const {
    return us_ == other.us_;
  }
  bool operator!=(TimeClass other) const {
    return us_ != other.us_;
  }
  bool operator<(TimeClass other) const {
    return us_ < other.us_;
  }
  bool operator<=(TimeClass other) const {
    return us_ <= other.us_;
  }
  bool operator>(TimeClass other) const {
    return us_ > other.us_;
  }
  bool operator>=(TimeClass other) const {
    return us_ >= other.us_;
  }

  // Converts an integer value representing TimeClass to a class. This is used
  // when deserializing a |TimeClass| structure, using a value known to be
  // compatible. It is not provided as a constructor because the integer type
  // may be unclear from the perspective of a caller.
  static TimeClass FromInternalValue(int64_t us) { return TimeClass(us); }

 protected:
  explicit constexpr TimeBase(int64_t us) : us_(us) {}

  // Time value in a microsecond timebase.
  int64_t us_;
};

}  // namespace time_internal


// -----------------------------------------------------------------------------
// Time
//
// This class represents an absolute point in time, internally represented as
// microseconds (s/1,000,000) since 00:00:00 UTC, January 1, 1970.

class V8_BASE_EXPORT Time final : public time_internal::TimeBase<Time> {
 public:
  // Contains the nullptr time. Use Time::Now() to get the current time.
  constexpr Time() : TimeBase(0) {}

  // Returns the current time. Watch out, the system might adjust its clock
  // in which case time will actually go backwards. We don't guarantee that
  // times are increasing, or that two calls to Now() won't be the same.
  static Time Now();

  // Returns the current time. Same as Now() except that this function always
  // uses system time so that there are no discrepancies between the returned
  // time and system time even on virtual environments including our test bot.
  // For timing sensitive unittests, this function should be used.
  static Time NowFromSystemTime();

  // Returns the time for epoch in Unix-like system (Jan 1, 1970).
  static Time UnixEpoch() { return Time(0); }

  // Converts to/from POSIX time specs.
  static Time FromTimespec(struct timespec ts);
  struct timespec ToTimespec() const;

  // Converts to/from POSIX time values.
  static Time FromTimeval(struct timeval tv);
  struct timeval ToTimeval() const;

  // Converts to/from Windows file times.
  static Time FromFiletime(struct _FILETIME ft);
  struct _FILETIME ToFiletime() const;

  // Converts to/from the Javascript convention for times, a number of
  // milliseconds since the epoch:
  static Time FromJsTime(double ms_since_epoch);
  double ToJsTime() const;

 private:
  friend class time_internal::TimeBase<Time>;
  explicit constexpr Time(int64_t us) : TimeBase(us) {}
};

V8_BASE_EXPORT std::ostream& operator<<(std::ostream&, const Time&);

inline Time operator+(const TimeDelta& delta, const Time& time) {
  return time + delta;
}


// -----------------------------------------------------------------------------
// TimeTicks
//
// This class represents an abstract time that is most of the time incrementing
// for use in measuring time durations. It is internally represented in
// microseconds.  It can not be converted to a human-readable time, but is
// guaranteed not to decrease (if the user changes the computer clock,
// Time::Now() may actually decrease or jump).  But note that TimeTicks may
// "stand still", for example if the computer suspended.

class V8_BASE_EXPORT TimeTicks final
    : public time_internal::TimeBase<TimeTicks> {
 public:
  constexpr TimeTicks() : TimeBase(0) {}

  // Platform-dependent tick count representing "right now." When
  // IsHighResolution() returns false, the resolution of the clock could be as
  // coarse as ~15.6ms. Otherwise, the resolution should be no worse than one
  // microsecond.
  // This method never returns a null TimeTicks.
  static TimeTicks Now();

  // This is equivalent to Now() but DCHECKs that IsHighResolution(). Useful for
  // test frameworks that rely on high resolution clocks (in practice all
  // platforms but low-end Windows devices have high resolution clocks).
  static TimeTicks HighResolutionNow();

  // Returns true if the high-resolution clock is working on this system.
  static bool IsHighResolution();

 private:
  friend class time_internal::TimeBase<TimeTicks>;

  // Please use Now() to create a new object. This is for internal use
  // and testing. Ticks are in microseconds.
  explicit constexpr TimeTicks(int64_t ticks) : TimeBase(ticks) {}
};

inline TimeTicks operator+(const TimeDelta& delta, const TimeTicks& ticks) {
  return ticks + delta;
}


// ThreadTicks ----------------------------------------------------------------

// Represents a clock, specific to a particular thread, than runs only while the
// thread is running.
class V8_BASE_EXPORT ThreadTicks final
    : public time_internal::TimeBase<ThreadTicks> {
 public:
  constexpr ThreadTicks() : TimeBase(0) {}

  // Returns true if ThreadTicks::Now() is supported on this system.
  static bool IsSupported();

  // Waits until the initialization is completed. Needs to be guarded with a
  // call to IsSupported().
  static void WaitUntilInitialized() {
#if V8_OS_WIN
    WaitUntilInitializedWin();
#endif
  }

  // Returns thread-specific CPU-time on systems that support this feature.
  // Needs to be guarded with a call to IsSupported(). Use this timer
  // to (approximately) measure how much time the calling thread spent doing
  // actual work vs. being de-scheduled. May return bogus results if the thread
  // migrates to another CPU between two calls. Returns an empty ThreadTicks
  // object until the initialization is completed. If a clock reading is
  // absolutely needed, call WaitUntilInitialized() before this method.
  static ThreadTicks Now();

#if V8_OS_WIN
  // Similar to Now() above except this returns thread-specific CPU time for an
  // arbitrary thread. All comments for Now() method above apply apply to this
  // method as well.
  static ThreadTicks GetForThread(const HANDLE& thread_handle);
#endif

 private:
  template <class TimeClass>
  friend class time_internal::TimeBase;

  // Please use Now() or GetForThread() to create a new object. This is for
  // internal use and testing. Ticks are in microseconds.
  explicit constexpr ThreadTicks(int64_t ticks) : TimeBase(ticks) {}

#if V8_OS_WIN
  // Returns the frequency of the TSC in ticks per second, or 0 if it hasn't
  // been measured yet. Needs to be guarded with a call to IsSupported().
  static double TSCTicksPerSecond();
  static bool IsSupportedWin();
  static void WaitUntilInitializedWin();
#endif
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_PLATFORM_TIME_H_
