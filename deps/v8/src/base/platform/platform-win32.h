// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PLATFORM_PLATFORM_WIN32_H_
#define V8_BASE_PLATFORM_PLATFORM_WIN32_H_

#include "src/base/platform/time.h"

namespace v8 {
namespace base {

// A timer which allows more precise sleep intervals. Sleeping on Windows is
// generally limited by the granularity of the system timer (64 Hz by default),
// but on Windows 10 version 1803 and newer, this class allows much shorter
// sleeps including sub-millisecond intervals.
class V8_BASE_EXPORT PreciseSleepTimer {
 public:
  PreciseSleepTimer();
  ~PreciseSleepTimer();

  // Moving is supported but copying is not, because this class owns a
  // platform handle.
  PreciseSleepTimer(const PreciseSleepTimer& other) = delete;
  PreciseSleepTimer& operator=(const PreciseSleepTimer& other) = delete;
  PreciseSleepTimer(PreciseSleepTimer&& other) V8_NOEXCEPT;
  PreciseSleepTimer& operator=(PreciseSleepTimer&& other) V8_NOEXCEPT;

  // Attempts to initialize this timer. Precise timers are only available on
  // Windows 10 version 1803 and later. To check whether initialization worked,
  // use IsInitialized.
  void TryInit();

  bool IsInitialized() const;

  // Sleeps for a specified time interval. This function requires that the timer
  // has been initialized, as can be checked with IsInitialized. A single
  // PreciseSleepTimer instance must not be used simultaneously on multiple
  // threads.
  void Sleep(TimeDelta interval) const;

 private:
  void Close();
  HANDLE timer_;
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_PLATFORM_PLATFORM_WIN32_H_
