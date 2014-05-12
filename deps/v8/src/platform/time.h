// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PLATFORM_TIME_H_
#define V8_PLATFORM_TIME_H_

#include <time.h>
#include <limits>

#include "../allocation.h"

// Forward declarations.
extern "C" {
struct _FILETIME;
struct mach_timespec;
struct timespec;
struct timeval;
}

namespace v8 {
namespace internal {

class Time;
class TimeTicks;

// -----------------------------------------------------------------------------
// TimeDelta
//
// This class represents a duration of time, internally represented in
// microseonds.

class TimeDelta V8_FINAL BASE_EMBEDDED {
 public:
  TimeDelta() : delta_(0) {}

  // Converts units of time to TimeDeltas.
  static TimeDelta FromDays(int days);
  static TimeDelta FromHours(int hours);
  static TimeDelta FromMinutes(int minutes);
  static TimeDelta FromSeconds(int64_t seconds);
  static TimeDelta FromMilliseconds(int64_t milliseconds);
  static TimeDelta FromMicroseconds(int64_t microseconds) {
    return TimeDelta(microseconds);
  }
  static TimeDelta FromNanoseconds(int64_t nanoseconds);

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
  int64_t InMicroseconds() const { return delta_; }
  int64_t InNanoseconds() const;

  // Converts to/from Mach time specs.
  static TimeDelta FromMachTimespec(struct mach_timespec ts);
  struct mach_timespec ToMachTimespec() const;

  // Converts to/from POSIX time specs.
  static TimeDelta FromTimespec(struct timespec ts);
  struct timespec ToTimespec() const;

  TimeDelta& operator=(const TimeDelta& other) {
    delta_ = other.delta_;
    return *this;
  }

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
  TimeDelta operator-() const {
    return TimeDelta(-delta_);
  }

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
  bool operator==(const TimeDelta& other) const {
    return delta_ == other.delta_;
  }
  bool operator!=(const TimeDelta& other) const {
    return delta_ != other.delta_;
  }
  bool operator<(const TimeDelta& other) const {
    return delta_ < other.delta_;
  }
  bool operator<=(const TimeDelta& other) const {
    return delta_ <= other.delta_;
  }
  bool operator>(const TimeDelta& other) const {
    return delta_ > other.delta_;
  }
  bool operator>=(const TimeDelta& other) const {
    return delta_ >= other.delta_;
  }

 private:
  // Constructs a delta given the duration in microseconds. This is private
  // to avoid confusion by callers with an integer constructor. Use
  // FromSeconds, FromMilliseconds, etc. instead.
  explicit TimeDelta(int64_t delta) : delta_(delta) {}

  // Delta in microseconds.
  int64_t delta_;
};


// -----------------------------------------------------------------------------
// Time
//
// This class represents an absolute point in time, internally represented as
// microseconds (s/1,000,000) since 00:00:00 UTC, January 1, 1970.

class Time V8_FINAL BASE_EMBEDDED {
 public:
  static const int64_t kMillisecondsPerSecond = 1000;
  static const int64_t kMicrosecondsPerMillisecond = 1000;
  static const int64_t kMicrosecondsPerSecond = kMicrosecondsPerMillisecond *
                                                kMillisecondsPerSecond;
  static const int64_t kMicrosecondsPerMinute = kMicrosecondsPerSecond * 60;
  static const int64_t kMicrosecondsPerHour = kMicrosecondsPerMinute * 60;
  static const int64_t kMicrosecondsPerDay = kMicrosecondsPerHour * 24;
  static const int64_t kMicrosecondsPerWeek = kMicrosecondsPerDay * 7;
  static const int64_t kNanosecondsPerMicrosecond = 1000;
  static const int64_t kNanosecondsPerSecond = kNanosecondsPerMicrosecond *
                                               kMicrosecondsPerSecond;

  // Contains the NULL time. Use Time::Now() to get the current time.
  Time() : us_(0) {}

  // Returns true if the time object has not been initialized.
  bool IsNull() const { return us_ == 0; }

  // Returns true if the time object is the maximum time.
  bool IsMax() const { return us_ == std::numeric_limits<int64_t>::max(); }

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

  // Returns the maximum time, which should be greater than any reasonable time
  // with which we might compare it.
  static Time Max() { return Time(std::numeric_limits<int64_t>::max()); }

  // Converts to/from internal values. The meaning of the "internal value" is
  // completely up to the implementation, so it should be treated as opaque.
  static Time FromInternalValue(int64_t value) {
    return Time(value);
  }
  int64_t ToInternalValue() const {
    return us_;
  }

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

  Time& operator=(const Time& other) {
    us_ = other.us_;
    return *this;
  }

  // Compute the difference between two times.
  TimeDelta operator-(const Time& other) const {
    return TimeDelta::FromMicroseconds(us_ - other.us_);
  }

  // Modify by some time delta.
  Time& operator+=(const TimeDelta& delta) {
    us_ += delta.InMicroseconds();
    return *this;
  }
  Time& operator-=(const TimeDelta& delta) {
    us_ -= delta.InMicroseconds();
    return *this;
  }

  // Return a new time modified by some delta.
  Time operator+(const TimeDelta& delta) const {
    return Time(us_ + delta.InMicroseconds());
  }
  Time operator-(const TimeDelta& delta) const {
    return Time(us_ - delta.InMicroseconds());
  }

  // Comparison operators
  bool operator==(const Time& other) const {
    return us_ == other.us_;
  }
  bool operator!=(const Time& other) const {
    return us_ != other.us_;
  }
  bool operator<(const Time& other) const {
    return us_ < other.us_;
  }
  bool operator<=(const Time& other) const {
    return us_ <= other.us_;
  }
  bool operator>(const Time& other) const {
    return us_ > other.us_;
  }
  bool operator>=(const Time& other) const {
    return us_ >= other.us_;
  }

 private:
  explicit Time(int64_t us) : us_(us) {}

  // Time in microseconds in UTC.
  int64_t us_;
};

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

class TimeTicks V8_FINAL BASE_EMBEDDED {
 public:
  TimeTicks() : ticks_(0) {}

  // Platform-dependent tick count representing "right now."
  // The resolution of this clock is ~1-15ms.  Resolution varies depending
  // on hardware/operating system configuration.
  // This method never returns a null TimeTicks.
  static TimeTicks Now();

  // Returns a platform-dependent high-resolution tick count. Implementation
  // is hardware dependent and may or may not return sub-millisecond
  // resolution.  THIS CALL IS GENERALLY MUCH MORE EXPENSIVE THAN Now() AND
  // SHOULD ONLY BE USED WHEN IT IS REALLY NEEDED.
  // This method never returns a null TimeTicks.
  static TimeTicks HighResolutionNow();

  // Returns true if the high-resolution clock is working on this system.
  static bool IsHighResolutionClockWorking();

  // Returns true if this object has not been initialized.
  bool IsNull() const { return ticks_ == 0; }

  // Converts to/from internal values. The meaning of the "internal value" is
  // completely up to the implementation, so it should be treated as opaque.
  static TimeTicks FromInternalValue(int64_t value) {
    return TimeTicks(value);
  }
  int64_t ToInternalValue() const {
    return ticks_;
  }

  TimeTicks& operator=(const TimeTicks other) {
    ticks_ = other.ticks_;
    return *this;
  }

  // Compute the difference between two times.
  TimeDelta operator-(const TimeTicks other) const {
    return TimeDelta::FromMicroseconds(ticks_ - other.ticks_);
  }

  // Modify by some time delta.
  TimeTicks& operator+=(const TimeDelta& delta) {
    ticks_ += delta.InMicroseconds();
    return *this;
  }
  TimeTicks& operator-=(const TimeDelta& delta) {
    ticks_ -= delta.InMicroseconds();
    return *this;
  }

  // Return a new TimeTicks modified by some delta.
  TimeTicks operator+(const TimeDelta& delta) const {
    return TimeTicks(ticks_ + delta.InMicroseconds());
  }
  TimeTicks operator-(const TimeDelta& delta) const {
    return TimeTicks(ticks_ - delta.InMicroseconds());
  }

  // Comparison operators
  bool operator==(const TimeTicks& other) const {
    return ticks_ == other.ticks_;
  }
  bool operator!=(const TimeTicks& other) const {
    return ticks_ != other.ticks_;
  }
  bool operator<(const TimeTicks& other) const {
    return ticks_ < other.ticks_;
  }
  bool operator<=(const TimeTicks& other) const {
    return ticks_ <= other.ticks_;
  }
  bool operator>(const TimeTicks& other) const {
    return ticks_ > other.ticks_;
  }
  bool operator>=(const TimeTicks& other) const {
    return ticks_ >= other.ticks_;
  }

 private:
  // Please use Now() to create a new object. This is for internal use
  // and testing. Ticks is in microseconds.
  explicit TimeTicks(int64_t ticks) : ticks_(ticks) {}

  // Tick count in microseconds.
  int64_t ticks_;
};

inline TimeTicks operator+(const TimeDelta& delta, const TimeTicks& ticks) {
  return ticks + delta;
}

} }  // namespace v8::internal

#endif  // V8_PLATFORM_TIME_H_
