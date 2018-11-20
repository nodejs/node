// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_UTILS_ARM64_H_
#define V8_ARM64_UTILS_ARM64_H_

#include <cmath>
#include "src/v8.h"

#include "src/arm64/constants-arm64.h"

#define REGISTER_CODE_LIST(R)                                                  \
R(0)  R(1)  R(2)  R(3)  R(4)  R(5)  R(6)  R(7)                                 \
R(8)  R(9)  R(10) R(11) R(12) R(13) R(14) R(15)                                \
R(16) R(17) R(18) R(19) R(20) R(21) R(22) R(23)                                \
R(24) R(25) R(26) R(27) R(28) R(29) R(30) R(31)

namespace v8 {
namespace internal {

// These are global assumptions in v8.
STATIC_ASSERT((static_cast<int32_t>(-1) >> 1) == -1);
STATIC_ASSERT((static_cast<uint32_t>(-1) >> 1) == 0x7FFFFFFF);

// Floating point representation.
static inline uint32_t float_to_rawbits(float value) {
  uint32_t bits = 0;
  memcpy(&bits, &value, 4);
  return bits;
}


static inline uint64_t double_to_rawbits(double value) {
  uint64_t bits = 0;
  memcpy(&bits, &value, 8);
  return bits;
}


static inline float rawbits_to_float(uint32_t bits) {
  float value = 0.0;
  memcpy(&value, &bits, 4);
  return value;
}


static inline double rawbits_to_double(uint64_t bits) {
  double value = 0.0;
  memcpy(&value, &bits, 8);
  return value;
}


// Bit counting.
int CountLeadingZeros(uint64_t value, int width);
int CountLeadingSignBits(int64_t value, int width);
int CountTrailingZeros(uint64_t value, int width);
int CountSetBits(uint64_t value, int width);
uint64_t LargestPowerOf2Divisor(uint64_t value);
int MaskToBit(uint64_t mask);


// NaN tests.
inline bool IsSignallingNaN(double num) {
  uint64_t raw = double_to_rawbits(num);
  if (std::isnan(num) && ((raw & kDQuietNanMask) == 0)) {
    return true;
  }
  return false;
}


inline bool IsSignallingNaN(float num) {
  uint32_t raw = float_to_rawbits(num);
  if (std::isnan(num) && ((raw & kSQuietNanMask) == 0)) {
    return true;
  }
  return false;
}


template <typename T>
inline bool IsQuietNaN(T num) {
  return std::isnan(num) && !IsSignallingNaN(num);
}


// Convert the NaN in 'num' to a quiet NaN.
inline double ToQuietNaN(double num) {
  DCHECK(std::isnan(num));
  return rawbits_to_double(double_to_rawbits(num) | kDQuietNanMask);
}


inline float ToQuietNaN(float num) {
  DCHECK(std::isnan(num));
  return rawbits_to_float(float_to_rawbits(num) | kSQuietNanMask);
}


// Fused multiply-add.
inline double FusedMultiplyAdd(double op1, double op2, double a) {
  return fma(op1, op2, a);
}


inline float FusedMultiplyAdd(float op1, float op2, float a) {
  return fmaf(op1, op2, a);
}

} }  // namespace v8::internal

#endif  // V8_ARM64_UTILS_ARM64_H_
