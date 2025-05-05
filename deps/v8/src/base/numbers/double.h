// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_NUMBERS_DOUBLE_H_
#define V8_BASE_NUMBERS_DOUBLE_H_

#include <bit>

#include "src/base/macros.h"
#include "src/base/numbers/diy-fp.h"

namespace v8 {
namespace base {

// We assume that doubles and uint64_t have the same endianness.
inline constexpr uint64_t double_to_uint64(double d) {
  return std::bit_cast<uint64_t>(d);
}
inline constexpr double uint64_to_double(uint64_t d64) {
  return std::bit_cast<double>(d64);
}

// Helper functions for doubles.
class Double {
 public:
  static constexpr uint64_t kSignMask = 0x8000'0000'0000'0000;
  static constexpr uint64_t kExponentMask = 0x7FF0'0000'0000'0000;
  static constexpr uint64_t kSignificandMask = 0x000F'FFFF'FFFF'FFFF;
  static constexpr uint64_t kHiddenBit = 0x0010'0000'0000'0000;
  static constexpr int kPhysicalSignificandSize =
      52;  // Excludes the hidden bit.
  static constexpr int kSignificandSize = 53;

  constexpr Double() : d64_(0) {}
  constexpr explicit Double(double d) : d64_(double_to_uint64(d)) {}
  constexpr explicit Double(uint64_t d64) : d64_(d64) {}
  constexpr explicit Double(DiyFp diy_fp) : d64_(DiyFpToUint64(diy_fp)) {}

  // The value encoded by this Double must be greater or equal to +0.0.
  // It must not be special (infinity, or NaN).
  DiyFp AsDiyFp() const {
    DCHECK_GT(Sign(), 0);
    DCHECK(!IsSpecial());
    return DiyFp(Significand(), Exponent());
  }

  // The value encoded by this Double must be strictly greater than 0.
  DiyFp AsNormalizedDiyFp() const {
    DCHECK_GT(value(), 0.0);
    uint64_t f = Significand();
    int e = Exponent();

    // The current double could be a denormal.
    while ((f & kHiddenBit) == 0) {
      f <<= 1;
      e--;
    }
    // Do the final shifts in one go.
    f <<= DiyFp::kSignificandSize - kSignificandSize;
    e -= DiyFp::kSignificandSize - kSignificandSize;
    return DiyFp(f, e);
  }

  // Returns the double's bit as uint64.
  constexpr uint64_t AsUint64() const { return d64_; }

  // Returns the next greater double. Returns +infinity on input +infinity.
  constexpr double NextDouble() const {
    if (d64_ == kInfinity) return Double(kInfinity).value();
    if (Sign() < 0 && Significand() == 0) {
      // -0.0
      return 0.0;
    }
    if (Sign() < 0) {
      return Double(d64_ - 1).value();
    } else {
      return Double(d64_ + 1).value();
    }
  }

  constexpr int Exponent() const {
    if (IsDenormal()) return kDenormalExponent;

    uint64_t d64 = AsUint64();
    int biased_e =
        static_cast<int>((d64 & kExponentMask) >> kPhysicalSignificandSize);
    return biased_e - kExponentBias;
  }

  constexpr uint64_t Significand() const {
    uint64_t d64 = AsUint64();
    uint64_t significand = d64 & kSignificandMask;
    if (!IsDenormal()) {
      return significand + kHiddenBit;
    } else {
      return significand;
    }
  }

  // Returns true if the double is a denormal.
  constexpr bool IsDenormal() const {
    uint64_t d64 = AsUint64();
    return (d64 & kExponentMask) == 0;
  }

  // We consider denormals not to be special.
  // Hence only Infinity and NaN are special.
  constexpr bool IsSpecial() const {
    uint64_t d64 = AsUint64();
    return (d64 & kExponentMask) == kExponentMask;
  }

  constexpr bool IsInfinite() const {
    uint64_t d64 = AsUint64();
    return ((d64 & kExponentMask) == kExponentMask) &&
           ((d64 & kSignificandMask) == 0);
  }

  constexpr int Sign() const {
    uint64_t d64 = AsUint64();
    return (d64 & kSignMask) == 0 ? 1 : -1;
  }

  // Precondition: the value encoded by this Double must be greater or equal
  // than +0.0.
  DiyFp UpperBoundary() const {
    DCHECK_GT(Sign(), 0);
    return DiyFp(Significand() * 2 + 1, Exponent() - 1);
  }

  // Returns the two boundaries of this.
  // The bigger boundary (m_plus) is normalized. The lower boundary has the same
  // exponent as m_plus.
  // Precondition: the value encoded by this Double must be greater than 0.
  void NormalizedBoundaries(DiyFp* out_m_minus, DiyFp* out_m_plus) const {
    DCHECK_GT(value(), 0.0);
    DiyFp v = this->AsDiyFp();
    DiyFp m_plus = DiyFp::Normalize(DiyFp((v.f() << 1) + 1, v.e() - 1));
    DiyFp m_minus;
    if ((AsUint64() & kSignificandMask) == 0 && v.e() != kDenormalExponent) {
      // The boundary is closer. Think of v = 1000e10 and v- = 9999e9.
      // Then the boundary (== (v - v-)/2) is not just at a distance of 1e9 but
      // at a distance of 1e8.
      // The only exception is for the smallest normal: the largest denormal is
      // at the same distance as its successor.
      // Note: denormals have the same exponent as the smallest normals.
      m_minus = DiyFp((v.f() << 2) - 1, v.e() - 2);
    } else {
      m_minus = DiyFp((v.f() << 1) - 1, v.e() - 1);
    }
    m_minus.set_f(m_minus.f() << (m_minus.e() - m_plus.e()));
    m_minus.set_e(m_plus.e());
    *out_m_plus = m_plus;
    *out_m_minus = m_minus;
  }

  constexpr double value() const { return uint64_to_double(d64_); }

  // Returns the significand size for a given order of magnitude.
  // If v = f*2^e with 2^p-1 <= f <= 2^p then p+e is v's order of magnitude.
  // This function returns the number of significant binary digits v will have
  // once its encoded into a double. In almost all cases this is equal to
  // kSignificandSize. The only exception are denormals. They start with leading
  // zeroes and their effective significand-size is hence smaller.
  static int SignificandSizeForOrderOfMagnitude(int order) {
    if (order >= (kDenormalExponent + kSignificandSize)) {
      return kSignificandSize;
    }
    if (order <= kDenormalExponent) return 0;
    return order - kDenormalExponent;
  }

 private:
  static constexpr int kExponentBias = 0x3FF + kPhysicalSignificandSize;
  static constexpr int kDenormalExponent = -kExponentBias + 1;
  static constexpr int kMaxExponent = 0x7FF - kExponentBias;
  static constexpr uint64_t kInfinity = 0x7FF0'0000'0000'0000;

  // The field d64_ is not marked as const to permit the usage of the copy
  // constructor.
  uint64_t d64_;

  static constexpr uint64_t DiyFpToUint64(DiyFp diy_fp) {
    uint64_t significand = diy_fp.f();
    int exponent = diy_fp.e();
    while (significand > kHiddenBit + kSignificandMask) {
      significand >>= 1;
      exponent++;
    }
    if (exponent >= kMaxExponent) {
      return kInfinity;
    }
    if (exponent < kDenormalExponent) {
      return 0;
    }
    while (exponent > kDenormalExponent && (significand & kHiddenBit) == 0) {
      significand <<= 1;
      exponent--;
    }
    uint64_t biased_exponent;
    if (exponent == kDenormalExponent && (significand & kHiddenBit) == 0) {
      biased_exponent = 0;
    } else {
      biased_exponent = static_cast<uint64_t>(exponent + kExponentBias);
    }
    return (significand & kSignificandMask) |
           (biased_exponent << kPhysicalSignificandSize);
  }
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_NUMBERS_DOUBLE_H_
