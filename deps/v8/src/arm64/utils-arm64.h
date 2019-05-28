// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_UTILS_ARM64_H_
#define V8_ARM64_UTILS_ARM64_H_

#include <cmath>

#include "src/arm64/constants-arm64.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// These are global assumptions in v8.
STATIC_ASSERT((static_cast<int32_t>(-1) >> 1) == -1);
STATIC_ASSERT((static_cast<uint32_t>(-1) >> 1) == 0x7FFFFFFF);

uint32_t float_sign(float val);
uint32_t float_exp(float val);
uint32_t float_mantissa(float val);
uint32_t double_sign(double val);
uint32_t double_exp(double val);
uint64_t double_mantissa(double val);

float float_pack(uint32_t sign, uint32_t exp, uint32_t mantissa);
double double_pack(uint64_t sign, uint64_t exp, uint64_t mantissa);

// An fpclassify() function for 16-bit half-precision floats.
int float16classify(float16 value);

// Bit counting.
int CountLeadingZeros(uint64_t value, int width);
int CountLeadingSignBits(int64_t value, int width);
V8_EXPORT_PRIVATE int CountTrailingZeros(uint64_t value, int width);
V8_EXPORT_PRIVATE int CountSetBits(uint64_t value, int width);
int LowestSetBitPosition(uint64_t value);
int HighestSetBitPosition(uint64_t value);
uint64_t LargestPowerOf2Divisor(uint64_t value);
int MaskToBit(uint64_t mask);


template <typename T>
T ReverseBytes(T value, int block_bytes_log2) {
  DCHECK((sizeof(value) == 4) || (sizeof(value) == 8));
  DCHECK((1ULL << block_bytes_log2) <= sizeof(value));
  // Split the 64-bit value into an 8-bit array, where b[0] is the least
  // significant byte, and b[7] is the most significant.
  uint8_t bytes[8];
  uint64_t mask = 0xff00000000000000;
  for (int i = 7; i >= 0; i--) {
    bytes[i] = (static_cast<uint64_t>(value) & mask) >> (i * 8);
    mask >>= 8;
  }

  // Permutation tables for REV instructions.
  //  permute_table[0] is used by REV16_x, REV16_w
  //  permute_table[1] is used by REV32_x, REV_w
  //  permute_table[2] is used by REV_x
  DCHECK((0 < block_bytes_log2) && (block_bytes_log2 < 4));
  static const uint8_t permute_table[3][8] = {{6, 7, 4, 5, 2, 3, 0, 1},
                                              {4, 5, 6, 7, 0, 1, 2, 3},
                                              {0, 1, 2, 3, 4, 5, 6, 7}};
  T result = 0;
  for (int i = 0; i < 8; i++) {
    result <<= 8;
    result |= bytes[permute_table[block_bytes_log2 - 1][i]];
  }
  return result;
}


// NaN tests.
inline bool IsSignallingNaN(double num) {
  uint64_t raw = bit_cast<uint64_t>(num);
  if (std::isnan(num) && ((raw & kDQuietNanMask) == 0)) {
    return true;
  }
  return false;
}


inline bool IsSignallingNaN(float num) {
  uint32_t raw = bit_cast<uint32_t>(num);
  if (std::isnan(num) && ((raw & kSQuietNanMask) == 0)) {
    return true;
  }
  return false;
}

inline bool IsSignallingNaN(float16 num) {
  const uint16_t kFP16QuietNaNMask = 0x0200;
  return (float16classify(num) == FP_NAN) && ((num & kFP16QuietNaNMask) == 0);
}

template <typename T>
inline bool IsQuietNaN(T num) {
  return std::isnan(num) && !IsSignallingNaN(num);
}


// Convert the NaN in 'num' to a quiet NaN.
inline double ToQuietNaN(double num) {
  DCHECK(std::isnan(num));
  return bit_cast<double>(bit_cast<uint64_t>(num) | kDQuietNanMask);
}


inline float ToQuietNaN(float num) {
  DCHECK(std::isnan(num));
  return bit_cast<float>(bit_cast<uint32_t>(num) |
                         static_cast<uint32_t>(kSQuietNanMask));
}


// Fused multiply-add.
inline double FusedMultiplyAdd(double op1, double op2, double a) {
  return fma(op1, op2, a);
}


inline float FusedMultiplyAdd(float op1, float op2, float a) {
  return fmaf(op1, op2, a);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_UTILS_ARM64_H_
