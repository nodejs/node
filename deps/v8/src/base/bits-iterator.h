// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_BITS_ITERATOR_H_
#define V8_BASE_BITS_ITERATOR_H_

#include <type_traits>

#include "src/base/bits.h"
#include "src/base/iterator.h"

namespace v8 {
namespace base {
namespace bits {

template <typename T, bool kMSBFirst = false>
class BitsIterator : public iterator<std::forward_iterator_tag, int> {
  static_assert(std::is_integral<T>::value);

 public:
  explicit BitsIterator(T bits) : bits_(bits) {}

  int operator*() const {
    return kMSBFirst ? 8 * sizeof(T) - 1 - CountLeadingZeros(bits_)
                     : CountTrailingZeros(bits_);
  }

  BitsIterator& operator++() {
    bits_ &= ~(T{1} << **this);
    return *this;
  }

  bool operator==(BitsIterator other) { return bits_ == other.bits_; }
  bool operator!=(BitsIterator other) { return bits_ != other.bits_; }

 private:
  T bits_;
};

// Returns an iterable over the bits in {bits}, from LSB to MSB.
template <typename T>
auto IterateBits(T bits) {
  return make_iterator_range(BitsIterator<T>{bits}, BitsIterator<T>{0});
}

// Returns an iterable over the bits in {bits}, from MSB to LSB.
template <typename T>
auto IterateBitsBackwards(T bits) {
  return make_iterator_range(BitsIterator<T, true>{bits},
                             BitsIterator<T, true>{0});
}

}  // namespace bits
}  // namespace base
}  // namespace v8

#endif  // V8_BASE_BITS_ITERATOR_H_
