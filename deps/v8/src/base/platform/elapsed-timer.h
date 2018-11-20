// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PLATFORM_ELAPSED_TIMER_H_
#define V8_BASE_PLATFORM_ELAPSED_TIMER_H_

#include "src/base/logging.h"
#include "src/base/platform/time.h"

namespace v8 {
namespace base {

class ElapsedTimer FINAL {
 public:
#ifdef DEBUG
  ElapsedTimer() : started_(false) {}
#endif

  // Starts this timer. Once started a timer can be checked with
  // |Elapsed()| or |HasExpired()|, and may be restarted using |Restart()|.
  // This method must not be called on an already started timer.
  void Start() {
    DCHECK(!IsStarted());
    start_ticks_ = Now();
#ifdef DEBUG
    started_ = true;
#endif
    DCHECK(IsStarted());
  }

  // Stops this timer. Must not be called on a timer that was not
  // started before.
  void Stop() {
    DCHECK(IsStarted());
    start_ticks_ = TimeTicks();
#ifdef DEBUG
    started_ = false;
#endif
    DCHECK(!IsStarted());
  }

  // Returns |true| if this timer was started previously.
  bool IsStarted() const {
    DCHECK(started_ || start_ticks_.IsNull());
    DCHECK(!started_ || !start_ticks_.IsNull());
    return !start_ticks_.IsNull();
  }

  // Restarts the timer and returns the time elapsed since the previous start.
  // This method is equivalent to obtaining the elapsed time with |Elapsed()|
  // and then starting the timer again, but does so in one single operation,
  // avoiding the need to obtain the clock value twice. It may only be called
  // on a previously started timer.
  TimeDelta Restart() {
    DCHECK(IsStarted());
    TimeTicks ticks = Now();
    TimeDelta elapsed = ticks - start_ticks_;
    DCHECK(elapsed.InMicroseconds() >= 0);
    start_ticks_ = ticks;
    DCHECK(IsStarted());
    return elapsed;
  }

  // Returns the time elapsed since the previous start. This method may only
  // be called on a previously started timer.
  TimeDelta Elapsed() const {
    DCHECK(IsStarted());
    TimeDelta elapsed = Now() - start_ticks_;
    DCHECK(elapsed.InMicroseconds() >= 0);
    return elapsed;
  }

  // Returns |true| if the specified |time_delta| has elapsed since the
  // previous start, or |false| if not. This method may only be called on
  // a previously started timer.
  bool HasExpired(TimeDelta time_delta) const {
    DCHECK(IsStarted());
    return Elapsed() >= time_delta;
  }

 private:
  static V8_INLINE TimeTicks Now() {
    TimeTicks now = TimeTicks::HighResolutionNow();
    DCHECK(!now.IsNull());
    return now;
  }

  TimeTicks start_ticks_;
#ifdef DEBUG
  bool started_;
#endif
};

} }  // namespace v8::base

#endif  // V8_BASE_PLATFORM_ELAPSED_TIMER_H_
