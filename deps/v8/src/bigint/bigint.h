// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BIGINT_BIGINT_H_
#define V8_BIGINT_BIGINT_H_

#include <stdint.h>

#include <algorithm>
#include <cstring>
#include <iostream>

namespace v8 {
namespace bigint {

// To play nice with embedders' macros, we define our own DCHECK here.
// It's only used in this file, and undef'ed at the end.
#ifdef DEBUG
#define BIGINT_H_DCHECK(cond)                         \
  if (!(cond)) {                                      \
    std::cerr << __FILE__ << ":" << __LINE__ << ": "; \
    std::cerr << "Assertion failed: " #cond "\n";     \
    abort();                                          \
  }
#else
#define BIGINT_H_DCHECK(cond) (void(0))
#endif

// The type of a digit: a register-width unsigned integer.
using digit_t = uintptr_t;
using signed_digit_t = intptr_t;
#if UINTPTR_MAX == 0xFFFFFFFF
// 32-bit platform.
using twodigit_t = uint64_t;
#define HAVE_TWODIGIT_T 1
static constexpr int kLog2DigitBits = 5;
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFF
// 64-bit platform.
static constexpr int kLog2DigitBits = 6;
#if defined(__SIZEOF_INT128__)
using twodigit_t = __uint128_t;
#define HAVE_TWODIGIT_T 1
#endif  // defined(__SIZEOF_INT128__)
#else
#error Unsupported platform.
#endif
static constexpr int kDigitBits = 1 << kLog2DigitBits;
static_assert(kDigitBits == 8 * sizeof(digit_t), "inconsistent type sizes");

// Describes an array of digits, also known as a BigInt. Unsigned.
// Does not own the memory it points at, and only gives read-only access to it.
// Digits are stored in little-endian order.
class Digits {
 public:
  // This is the constructor intended for public consumption.
  Digits(digit_t* mem, int len) : digits_(mem), len_(len) {
    // Require 4-byte alignment (even on 64-bit platforms).
    // TODO(jkummerow): See if we can tighten BigInt alignment in V8 to
    // system pointer size, and raise this requirement to that.
    BIGINT_H_DCHECK((reinterpret_cast<uintptr_t>(mem) & 3) == 0);
  }
  // Provides a "slice" view into another Digits object.
  Digits(Digits src, int offset, int len)
      : digits_(src.digits_ + offset),
        len_(std::max(0, std::min(src.len_ - offset, len))) {
    BIGINT_H_DCHECK(offset >= 0);
  }
  // Alternative way to get a "slice" view into another Digits object.
  Digits operator+(int i) {
    BIGINT_H_DCHECK(i >= 0 && i <= len_);
    return Digits(digits_ + i, len_ - i);
  }

  // Provides access to individual digits.
  digit_t operator[](int i) {
    BIGINT_H_DCHECK(i >= 0 && i < len_);
    return read_4byte_aligned(i);
  }
  // Convenience accessor for the most significant digit.
  digit_t msd() {
    BIGINT_H_DCHECK(len_ > 0);
    return read_4byte_aligned(len_ - 1);
  }
  // Checks "pointer equality" (does not compare digits contents).
  bool operator==(const Digits& other) const {
    return digits_ == other.digits_ && len_ == other.len_;
  }

  // Decrements {len_} until there are no leading zero digits left.
  void Normalize() {
    while (len_ > 0 && msd() == 0) len_--;
  }
  // Unconditionally drops exactly one leading zero digit.
  void TrimOne() {
    BIGINT_H_DCHECK(len_ > 0 && msd() == 0);
    len_--;
  }

  int len() { return len_; }
  const digit_t* digits() const { return digits_; }

 protected:
  friend class TemporaryLeftShift;
  digit_t* digits_;
  int len_;

 private:
  // We require externally-provided digits arrays to be 4-byte aligned, but
  // not necessarily 8-byte aligned; so on 64-bit platforms we use memcpy
  // to allow unaligned reads.
  digit_t read_4byte_aligned(int i) {
    if (sizeof(digit_t) == 4) {
      return digits_[i];
    } else {
      digit_t result;
      memcpy(&result, digits_ + i, sizeof(result));
      return result;
    }
  }
};

// Returns r such that r < 0 if A < B; r > 0 if A > B; r == 0 if A == B.
int Compare(Digits A, Digits B);

}  // namespace bigint
}  // namespace v8

#undef BIGINT_H_DCHECK

#endif  // V8_BIGINT_BIGINT_H_
