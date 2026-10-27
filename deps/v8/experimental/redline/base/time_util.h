// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_BASE_TIME_UTIL_H_
#define V8_EXPERIMENTAL_REDLINE_BASE_TIME_UTIL_H_

#include <time.h>

#include <cstdint>

namespace redline {

// Monotonic clock in microseconds.
inline uint64_t MonotonicNowUs() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000000 +
         static_cast<uint64_t>(ts.tv_nsec) / 1000;
}

// Wall clock in milliseconds since the epoch (for event timestamps).
inline uint64_t WallClockNowMs() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000 +
         static_cast<uint64_t>(ts.tv_nsec) / 1000000;
}

// Simple stopwatch.
class Stopwatch {
 public:
  Stopwatch() : start_(MonotonicNowUs()) {}
  uint64_t ElapsedUs() const { return MonotonicNowUs() - start_; }
  double ElapsedSeconds() const {
    return static_cast<double>(ElapsedUs()) / 1e6;
  }
  void Restart() { start_ = MonotonicNowUs(); }

 private:
  uint64_t start_;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_BASE_TIME_UTIL_H_
