// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_INL_H_
#define V8_UTILS_INL_H_

#include "src/utils.h"

#include "include/v8-platform.h"
#include "src/base/platform/time.h"
#include "src/char-predicates-inl.h"
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

template <typename Char>
bool TryAddIndexChar(uint32_t* index, Char c) {
  if (!IsDecimalDigit(c)) return false;
  int d = c - '0';
  if (*index > 429496729U - ((d + 3) >> 3)) return false;
  *index = (*index) * 10 + d;
  return true;
}

template <typename Stream>
bool StringToArrayIndex(Stream* stream, uint32_t* index) {
  uint16_t ch = stream->GetNext();

  // If the string begins with a '0' character, it must only consist
  // of it to be a legal array index.
  if (ch == '0') {
    *index = 0;
    return !stream->HasMore();
  }

  // Convert string to uint32 array index; character by character.
  if (!IsDecimalDigit(ch)) return false;
  int d = ch - '0';
  uint32_t result = d;
  while (stream->HasMore()) {
    if (!TryAddIndexChar(&result, stream->GetNext())) return false;
  }

  *index = result;
  return true;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_INL_H_
