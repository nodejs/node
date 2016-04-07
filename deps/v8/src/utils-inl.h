// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_INL_H_
#define V8_UTILS_INL_H_

#include "src/utils.h"

#include "include/v8-platform.h"
#include "src/base/platform/time.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

class TimedScope {
 public:
  explicit TimedScope(double* result)
      : start_(TimestampMs()), result_(result) {}

  ~TimedScope() { *result_ = TimestampMs() - start_; }

 private:
  static inline double TimestampMs() {
    return V8::GetCurrentPlatform()->MonotonicallyIncreasingTime() *
           static_cast<double>(base::Time::kMillisecondsPerSecond);
  }

  double start_;
  double* result_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_INL_H_
