// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_NUMBERS_DIY_FP_H_
#define V8_BASE_NUMBERS_DIY_FP_H_

#include <stdint.h>

#include "src/base/logging.h"

namespace v8 {
namespace base {

// This "Do It Yourself Floating Point" class implements a floating-point number
// with a uint64 significand and an int exponent. Normalized DiyFp numbers will
// have the most significant bit of the significand set.
// Multiplication and Subtraction do not normalize their results.
// DiyFp are not designed to contain special doubles (NaN and Infinity).
class DiyFp {
 public:
  static const int kSignificandSize = 64;

  constexpr DiyFp() : f_(0), e_(0) {}
  constexpr DiyFp(uint64_t f, int e) : f_(f), e_(e) {}

  // this = this - other.
  // The exponents of both numbers must be the same and the significand of this
  // must be bigger than the significand of other.
  // The result will not be normalized.
  void Subtract(const DiyFp& other) {
    DCHECK(e_ == other.e_);
    DCHECK(f_ >= other.f_);
    f_ -= other.f_;
  }

  // Returns a - b.
  // The exponents of both numbers must be the same and this must be bigger
  // than other. The result will not be normalized.
  static DiyFp Minus(const DiyFp& a, const DiyFp& b) {
    DiyFp result = a;
    result.Subtract(b);
    return result;
  }

  // this = this * other.
  V8_BASE_EXPORT void Multiply(const DiyFp& other);

  // returns a * b;
  static DiyFp Times(const DiyFp& a, const DiyFp& b) {
#ifdef __SIZEOF_INT128__
    // If we have compiler-assisted 64x64 -> 128 muls (e.g. x86-64 and
    // aarch64), we can use that for a faster, inlined implementation.
    // This rounds the same way as Multiply().
    uint64_t hi = (a.f_ * static_cast<unsigned __int128>(b.f_)) >> 64;
    uint64_t lo = (a.f_ * static_cast<unsigned __int128>(b.f_));
    return {hi + (lo >> 63), a.e_ + b.e_ + 64};
#else
    DiyFp result = a;
    result.Multiply(b);
    return result;
#endif
  }

  void Normalize() {
    DCHECK_NE(f_, 0);
    uint64_t f = f_;
    int e = e_;

    // This method is mainly called for normalizing boundaries. In general
    // boundaries need to be shifted by 10 bits. We thus optimize for this case.
    const uint64_t k10MSBits = static_cast<uint64_t>(0x3FF) << 54;
    while ((f & k10MSBits) == 0) {
      f <<= 10;
      e -= 10;
    }
    while ((f & kUint64MSB) == 0) {
      f <<= 1;
      e--;
    }
    f_ = f;
    e_ = e;
  }

  static DiyFp Normalize(const DiyFp& a) {
    DiyFp result = a;
    result.Normalize();
    return result;
  }

  constexpr uint64_t f() const { return f_; }
  constexpr int e() const { return e_; }

  constexpr void set_f(uint64_t new_value) { f_ = new_value; }
  constexpr void set_e(int new_value) { e_ = new_value; }

 private:
  static const uint64_t kUint64MSB = static_cast<uint64_t>(1) << 63;

  uint64_t f_;
  int e_;
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_NUMBERS_DIY_FP_H_
