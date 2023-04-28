// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#if V8_TARGET_ARCH_ARM64

#include "src/codegen/arm64/utils-arm64.h"

namespace v8 {
namespace internal {

#define __ assm->

uint32_t float_sign(float val) {
  uint32_t bits = base::bit_cast<uint32_t>(val);
  return unsigned_bitextract_32(31, 31, bits);
}

uint32_t float_exp(float val) {
  uint32_t bits = base::bit_cast<uint32_t>(val);
  return unsigned_bitextract_32(30, 23, bits);
}

uint32_t float_mantissa(float val) {
  uint32_t bits = base::bit_cast<uint32_t>(val);
  return unsigned_bitextract_32(22, 0, bits);
}

uint32_t double_sign(double val) {
  uint64_t bits = base::bit_cast<uint64_t>(val);
  return static_cast<uint32_t>(unsigned_bitextract_64(63, 63, bits));
}

uint32_t double_exp(double val) {
  uint64_t bits = base::bit_cast<uint64_t>(val);
  return static_cast<uint32_t>(unsigned_bitextract_64(62, 52, bits));
}

uint64_t double_mantissa(double val) {
  uint64_t bits = base::bit_cast<uint64_t>(val);
  return unsigned_bitextract_64(51, 0, bits);
}

float float_pack(uint32_t sign, uint32_t exp, uint32_t mantissa) {
  uint32_t bits = sign << kFloatExponentBits | exp;
  return base::bit_cast<float>((bits << kFloatMantissaBits) | mantissa);
}

double double_pack(uint64_t sign, uint64_t exp, uint64_t mantissa) {
  uint64_t bits = sign << kDoubleExponentBits | exp;
  return base::bit_cast<double>((bits << kDoubleMantissaBits) | mantissa);
}

int float16classify(float16 value) {
  const uint16_t exponent_max = (1 << kFloat16ExponentBits) - 1;
  const uint16_t exponent_mask = exponent_max << kFloat16MantissaBits;
  const uint16_t mantissa_mask = (1 << kFloat16MantissaBits) - 1;

  const uint16_t exponent = (value & exponent_mask) >> kFloat16MantissaBits;
  const uint16_t mantissa = value & mantissa_mask;
  if (exponent == 0) {
    if (mantissa == 0) {
      return FP_ZERO;
    }
    return FP_SUBNORMAL;
  } else if (exponent == exponent_max) {
    if (mantissa == 0) {
      return FP_INFINITE;
    }
    return FP_NAN;
  }
  return FP_NORMAL;
}

int CountLeadingZeros(uint64_t value, int width) {
  DCHECK(base::bits::IsPowerOfTwo(width) && (width <= 64));
  if (value == 0) {
    return width;
  }
  return base::bits::CountLeadingZeros64(value << (64 - width));
}

int CountLeadingSignBits(int64_t value, int width) {
  DCHECK(base::bits::IsPowerOfTwo(width) && (width <= 64));
  if (value >= 0) {
    return CountLeadingZeros(value, width) - 1;
  } else {
    return CountLeadingZeros(~value, width) - 1;
  }
}

int CountSetBits(uint64_t value, int width) {
  DCHECK((width == 32) || (width == 64));
  if (width == 64) {
    return static_cast<int>(base::bits::CountPopulation(value));
  }
  return static_cast<int>(
      base::bits::CountPopulation(static_cast<uint32_t>(value & 0xFFFFFFFFF)));
}

int LowestSetBitPosition(uint64_t value) {
  DCHECK_NE(value, 0U);
  return base::bits::CountTrailingZeros(value) + 1;
}

int HighestSetBitPosition(uint64_t value) {
  DCHECK_NE(value, 0U);
  return 63 - CountLeadingZeros(value, 64);
}

uint64_t LargestPowerOf2Divisor(uint64_t value) {
  // Simulate two's complement (instead of casting to signed and negating) to
  // avoid undefined behavior on signed overflow.
  return value & ((~value) + 1);
}

int MaskToBit(uint64_t mask) {
  DCHECK_EQ(CountSetBits(mask, 64), 1);
  return base::bits::CountTrailingZeros(mask);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
