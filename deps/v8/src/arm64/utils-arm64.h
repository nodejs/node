// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_UTILS_ARM64_H_
#define V8_ARM64_UTILS_ARM64_H_

#include <cmath>

#include "src/arm64/constants-arm64.h"

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


template <typename T>
T ReverseBytes(T value, int block_bytes_log2) {
  DCHECK((sizeof(value) == 4) || (sizeof(value) == 8));
  DCHECK((1U << block_bytes_log2) <= sizeof(value));
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

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_UTILS_ARM64_H_
