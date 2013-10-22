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

#ifndef V8_PLATFORM_ELAPSED_TIMER_H_
#define V8_PLATFORM_ELAPSED_TIMER_H_

#include "checks.h"
#include "platform/time.h"

namespace v8 {
namespace internal {

class ElapsedTimer V8_FINAL BASE_EMBEDDED {
 public:
#ifdef DEBUG
  ElapsedTimer() : started_(false) {}
#endif

  // Starts this timer. Once started a timer can be checked with
  // |Elapsed()| or |HasExpired()|, and may be restarted using |Restart()|.
  // This method must not be called on an already started timer.
  void Start() {
    ASSERT(!IsStarted());
    start_ticks_ = Now();
#ifdef DEBUG
    started_ = true;
#endif
    ASSERT(IsStarted());
  }

  // Stops this timer. Must not be called on a timer that was not
  // started before.
  void Stop() {
    ASSERT(IsStarted());
    start_ticks_ = TimeTicks();
#ifdef DEBUG
    started_ = false;
#endif
    ASSERT(!IsStarted());
  }

  // Returns |true| if this timer was started previously.
  bool IsStarted() const {
    ASSERT(started_ || start_ticks_.IsNull());
    ASSERT(!started_ || !start_ticks_.IsNull());
    return !start_ticks_.IsNull();
  }

  // Restarts the timer and returns the time elapsed since the previous start.
  // This method is equivalent to obtaining the elapsed time with |Elapsed()|
  // and then starting the timer again, but does so in one single operation,
  // avoiding the need to obtain the clock value twice. It may only be called
  // on a previously started timer.
  TimeDelta Restart() {
    ASSERT(IsStarted());
    TimeTicks ticks = Now();
    TimeDelta elapsed = ticks - start_ticks_;
    ASSERT(elapsed.InMicroseconds() >= 0);
    start_ticks_ = ticks;
    ASSERT(IsStarted());
    return elapsed;
  }

  // Returns the time elapsed since the previous start. This method may only
  // be called on a previously started timer.
  TimeDelta Elapsed() const {
    ASSERT(IsStarted());
    TimeDelta elapsed = Now() - start_ticks_;
    ASSERT(elapsed.InMicroseconds() >= 0);
    return elapsed;
  }

  // Returns |true| if the specified |time_delta| has elapsed since the
  // previous start, or |false| if not. This method may only be called on
  // a previously started timer.
  bool HasExpired(TimeDelta time_delta) const {
    ASSERT(IsStarted());
    return Elapsed() >= time_delta;
  }

 private:
  static V8_INLINE TimeTicks Now() {
    TimeTicks now = TimeTicks::HighResNow();
    ASSERT(!now.IsNull());
    return now;
  }

  TimeTicks start_ticks_;
#ifdef DEBUG
  bool started_;
#endif
};

} }  // namespace v8::internal

#endif  // V8_PLATFORM_ELAPSED_TIMER_H_
