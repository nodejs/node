// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_RING_BUFFER_H_
#define V8_BASE_RING_BUFFER_H_

#include "src/base/macros.h"

namespace v8 {
namespace base {

template <typename T>
class RingBuffer {
 public:
  RingBuffer() { Reset(); }
  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  static const int kSize = 10;

  void Push(const T& value) {
    if (count_ == kSize) {
      elements_[start_++] = value;
      if (start_ == kSize) start_ = 0;
    } else {
      DCHECK_EQ(start_, 0);
      elements_[count_++] = value;
    }
  }

  int Count() const { return count_; }

  template <typename Callback>
  T Sum(Callback callback, const T& initial) const {
    int j = start_ + count_ - 1;
    if (j >= kSize) j -= kSize;
    T result = initial;
    for (int i = 0; i < count_; i++) {
      result = callback(result, elements_[j]);
      if (--j == -1) j += kSize;
    }
    return result;
  }

  void Reset() { start_ = count_ = 0; }

 private:
  T elements_[kSize];
  int start_;
  int count_;
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_RING_BUFFER_H_
