// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_DOUBLE_H_
#define V8_DOUBLE_H_

#include "diy_fp.h"

namespace v8 {
namespace internal {

// We assume that doubles and uint64_t have the same endianness.
static uint64_t double_to_uint64(double d) { return BitCast<uint64_t>(d); }
static double uint64_to_double(uint64_t d64) { return BitCast<double>(d64); }

// Helper functions for doubles.
class Double {
 public:
  static const uint64_t kSignMask = V8_2PART_UINT64_C(0x80000000, 00000000);
  static const uint64_t kExponentMask = V8_2PART_UINT64_C(0x7FF00000, 00000000);
  static const uint64_t kSignificandMask =
      V8_2PART_UINT64_C(0x000FFFFF, FFFFFFFF);
  static const uint64_t kHiddenBit = V8_2PART_UINT64_C(0x00100000, 00000000);

  Double() : d64_(0) {}
  explicit Double(double d) : d64_(double_to_uint64(d)) {}
  explicit Double(uint64_t d64) : d64_(d64) {}

  DiyFp AsDiyFp() const {
    ASSERT(!IsSpecial());
    return DiyFp(Significand(), Exponent());
  }

  // this->Significand() must not be 0.
  DiyFp AsNormalizedDiyFp() const {
    uint64_t f = Significand();
    int e = Exponent();

    ASSERT(f != 0);

    // The current double could be a denormal.
    while ((f & kHiddenBit) == 0) {
      f <<= 1;
      e--;
    }
    // Do the final shifts in one go. Don't forget the hidden bit (the '-1').
    f <<= DiyFp::kSignificandSize - kSignificandSize - 1;
    e -= DiyFp::kSignificandSize - kSignificandSize - 1;
    return DiyFp(f, e);
  }

  // Returns the double's bit as uint64.
  uint64_t AsUint64() const {
    return d64_;
  }

  int Exponent() const {
    if (IsDenormal()) return kDenormalExponent;

    uint64_t d64 = AsUint64();
    int biased_e = static_cast<int>((d64 & kExponentMask) >> kSignificandSize);
    return biased_e - kExponentBias;
  }

  uint64_t Significand() const {
    uint64_t d64 = AsUint64();
    uint64_t significand = d64 & kSignificandMask;
    if (!IsDenormal()) {
      return significand + kHiddenBit;
    } else {
      return significand;
    }
  }

  // Returns true if the double is a denormal.
  bool IsDenormal() const {
    uint64_t d64 = AsUint64();
    return (d64 & kExponentMask) == 0;
  }

  // We consider denormals not to be special.
  // Hence only Infinity and NaN are special.
  bool IsSpecial() const {
    uint64_t d64 = AsUint64();
    return (d64 & kExponentMask) == kExponentMask;
  }

  bool IsNan() const {
    uint64_t d64 = AsUint64();
    return ((d64 & kExponentMask) == kExponentMask) &&
        ((d64 & kSignificandMask) != 0);
  }


  bool IsInfinite() const {
    uint64_t d64 = AsUint64();
    return ((d64 & kExponentMask) == kExponentMask) &&
        ((d64 & kSignificandMask) == 0);
  }


  int Sign() const {
    uint64_t d64 = AsUint64();
    return (d64 & kSignMask) == 0? 1: -1;
  }


  // Returns the two boundaries of this.
  // The bigger boundary (m_plus) is normalized. The lower boundary has the same
  // exponent as m_plus.
  void NormalizedBoundaries(DiyFp* out_m_minus, DiyFp* out_m_plus) const {
    DiyFp v = this->AsDiyFp();
    bool significand_is_zero = (v.f() == kHiddenBit);
    DiyFp m_plus = DiyFp::Normalize(DiyFp((v.f() << 1) + 1, v.e() - 1));
    DiyFp m_minus;
    if (significand_is_zero && v.e() != kDenormalExponent) {
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

  double value() const { return uint64_to_double(d64_); }

 private:
  static const int kSignificandSize = 52;  // Excludes the hidden bit.
  static const int kExponentBias = 0x3FF + kSignificandSize;
  static const int kDenormalExponent = -kExponentBias + 1;

  uint64_t d64_;
};

} }  // namespace v8::internal

#endif  // V8_DOUBLE_H_
