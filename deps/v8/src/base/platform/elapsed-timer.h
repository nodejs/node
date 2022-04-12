// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PLATFORM_ELAPSED_TIMER_H_
#define V8_BASE_PLATFORM_ELAPSED_TIMER_H_

#include "src/base/logging.h"
#include "src/base/platform/time.h"

namespace v8 {
namespace base {

class ElapsedTimer final {
 public:
  ElapsedTimer() : start_ticks_() {}

  // Starts this timer. Once started a timer can be checked with
  // |Elapsed()| or |HasExpired()|, and may be restarted using |Restart()|.
  // This method must not be called on an already started timer.
  void Start() { Start(Now()); }

  void Start(TimeTicks now) {
    DCHECK(!now.IsNull());
    DCHECK(!IsStarted());
    set_start_ticks(now);
#ifdef DEBUG
    started_ = true;
#endif
    DCHECK(IsStarted());
  }

  // Stops this timer. Must not be called on a timer that was not
  // started before.
  void Stop() {
    DCHECK(IsStarted());
    set_start_ticks(TimeTicks());
#ifdef DEBUG
    started_ = false;
#endif
    DCHECK(!IsStarted());
  }

  // Returns |true| if this timer was started previously.
  bool IsStarted() const {
    DCHECK(!paused_);
    DCHECK_NE(started_, start_ticks_.IsNull());
    return !start_ticks_.IsNull();
  }

#if DEBUG
  bool IsPaused() const { return paused_; }
#endif

  // Restarts the timer and returns the time elapsed since the previous start.
  // This method is equivalent to obtaining the elapsed time with |Elapsed()|
  // and then starting the timer again, but does so in one single operation,
  // avoiding the need to obtain the clock value twice. It may only be called
  // on a previously started timer.
  TimeDelta Restart() { return Restart(Now()); }

  TimeDelta Restart(TimeTicks now) {
    DCHECK(!now.IsNull());
    DCHECK(IsStarted());
    TimeDelta elapsed = now - start_ticks_;
    DCHECK_GE(elapsed.InMicroseconds(), 0);
    set_start_ticks(now);
    DCHECK(IsStarted());
    return elapsed;
  }

  void Pause() { Pause(Now()); }

  void Pause(TimeTicks now) {
    TimeDelta elapsed = Elapsed(now);
    DCHECK(IsStarted());
#ifdef DEBUG
    paused_ = true;
#endif
    set_paused_elapsed(elapsed);
  }

  void Resume() { Resume(Now()); }

  void Resume(TimeTicks now) {
    DCHECK(!now.IsNull());
    DCHECK(started_);
    DCHECK(paused_);
    TimeDelta elapsed = paused_elapsed();
#ifdef DEBUG
    paused_ = false;
#endif
    set_start_ticks(now - elapsed);
    DCHECK(IsStarted());
  }

  // Returns the time elapsed since the previous start. This method may only
  // be called on a previously started timer.
  TimeDelta Elapsed() const { return Elapsed(Now()); }

  TimeDelta Elapsed(TimeTicks now) const {
    DCHECK(!now.IsNull());
    DCHECK(IsStarted());
    TimeDelta elapsed = now - start_ticks();
    DCHECK_GE(elapsed.InMicroseconds(), 0);
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
    TimeTicks now = TimeTicks::Now();
    DCHECK(!now.IsNull());
    return now;
  }

  TimeDelta paused_elapsed() {
    // Only used started_ since paused_elapsed_ can be 0.
    DCHECK(paused_);
    DCHECK(started_);
    return paused_elapsed_;
  }

  void set_paused_elapsed(TimeDelta delta) {
    DCHECK(paused_);
    DCHECK(started_);
    paused_elapsed_ = delta;
  }

  TimeTicks start_ticks() const {
    DCHECK(!paused_);
    return start_ticks_;
  }
  void set_start_ticks(TimeTicks start_ticks) {
    DCHECK(!paused_);
    start_ticks_ = start_ticks;
  }

  union {
    TimeTicks start_ticks_;
    TimeDelta paused_elapsed_;
  };
#ifdef DEBUG
  bool started_ = false;
  bool paused_ = false;
#endif
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_PLATFORM_ELAPSED_TIMER_H_
