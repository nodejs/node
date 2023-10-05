// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_RING_BUFFER_H_
#define V8_BASE_RING_BUFFER_H_

#include <cstdint>

namespace v8::base {

template <typename T, uint8_t _SIZE = 10>
class RingBuffer final {
 public:
  static constexpr uint8_t kSize = _SIZE;

  constexpr RingBuffer() = default;

  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  constexpr void Push(const T& value) {
    elements_[pos_++] = value;
    if (pos_ == kSize) {
      pos_ = 0;
      is_full_ = true;
    }
  }

  constexpr uint8_t Size() const { return is_full_ ? kSize : pos_; }

  constexpr bool Empty() const { return Size() == 0; }

  constexpr void Clear() {
    pos_ = 0;
    is_full_ = false;
  }

  template <typename Callback>
  constexpr T Reduce(Callback callback, const T& initial) const {
    T result = initial;
    for (uint8_t i = pos_; i > 0; --i) {
      result = callback(result, elements_[i - 1]);
    }
    if (!is_full_) {
      return result;
    }
    for (uint8_t i = kSize; i > pos_; --i) {
      result = callback(result, elements_[i - 1]);
    }
    return result;
  }

 private:
  T elements_[kSize];
  uint8_t pos_ = 0;
  bool is_full_ = false;
};

}  // namespace v8::base

#endif  // V8_BASE_RING_BUFFER_H_
