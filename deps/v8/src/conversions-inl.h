// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CONVERSIONS_INL_H_
#define V8_CONVERSIONS_INL_H_

#include <float.h>         // Required for DBL_MAX and on Win32 for finite()
#include <limits.h>        // Required for INT_MAX etc.
#include <stdarg.h>
#include <cmath>
#include "src/globals.h"       // Required for V8_INFINITY

// ----------------------------------------------------------------------------
// Extra POSIX/ANSI functions for Win32/MSVC.

#include "src/base/bits.h"
#include "src/base/platform/platform.h"
#include "src/conversions.h"
#include "src/double.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// The fast double-to-unsigned-int conversion routine does not guarantee
// rounding towards zero, or any reasonable value if the argument is larger
// than what fits in an unsigned 32-bit integer.
inline unsigned int FastD2UI(double x) {
  // There is no unsigned version of lrint, so there is no fast path
  // in this function as there is in FastD2I. Using lrint doesn't work
  // for values of 2^31 and above.

  // Convert "small enough" doubles to uint32_t by fixing the 32
  // least significant non-fractional bits in the low 32 bits of the
  // double, and reading them from there.
  const double k2Pow52 = 4503599627370496.0;
  bool negative = x < 0;
  if (negative) {
    x = -x;
  }
  if (x < k2Pow52) {
    x += k2Pow52;
    uint32_t result;
#ifndef V8_TARGET_BIG_ENDIAN
    void* mantissa_ptr = reinterpret_cast<void*>(&x);
#else
    void* mantissa_ptr =
        reinterpret_cast<void*>(reinterpret_cast<Address>(&x) + kInt32Size);
#endif
    // Copy least significant 32 bits of mantissa.
    memcpy(&result, mantissa_ptr, sizeof(result));
    return negative ? ~result + 1 : result;
  }
  // Large number (outside uint32 range), Infinity or NaN.
  return 0x80000000u;  // Return integer indefinite.
}


inline float DoubleToFloat32(double x) {
  // TODO(yangguo): This static_cast is implementation-defined behaviour in C++,
  // so we may need to do the conversion manually instead to match the spec.
  volatile float f = static_cast<float>(x);
  return f;
}


inline double DoubleToInteger(double x) {
  if (std::isnan(x)) return 0;
  if (!std::isfinite(x) || x == 0) return x;
  return (x >= 0) ? std::floor(x) : std::ceil(x);
}


int32_t DoubleToInt32(double x) {
  if ((std::isfinite(x)) && (x <= INT_MAX) && (x >= INT_MIN)) {
    int32_t i = static_cast<int32_t>(x);
    if (FastI2D(i) == x) return i;
  }
  Double d(x);
  int exponent = d.Exponent();
  if (exponent < 0) {
    if (exponent <= -Double::kSignificandSize) return 0;
    return d.Sign() * static_cast<int32_t>(d.Significand() >> -exponent);
  } else {
    if (exponent > 31) return 0;
    return d.Sign() * static_cast<int32_t>(d.Significand() << exponent);
  }
}

bool DoubleToSmiInteger(double value, int* smi_int_value) {
  if (!IsSmiDouble(value)) return false;
  *smi_int_value = FastD2I(value);
  DCHECK(Smi::IsValid(*smi_int_value));
  return true;
}

bool IsSmiDouble(double value) {
  return value >= Smi::kMinValue && value <= Smi::kMaxValue &&
         !IsMinusZero(value) && value == FastI2D(FastD2I(value));
}


bool IsInt32Double(double value) {
  return value >= kMinInt && value <= kMaxInt && !IsMinusZero(value) &&
         value == FastI2D(FastD2I(value));
}


bool IsUint32Double(double value) {
  return !IsMinusZero(value) && value >= 0 && value <= kMaxUInt32 &&
         value == FastUI2D(FastD2UI(value));
}

bool DoubleToUint32IfEqualToSelf(double value, uint32_t* uint32_value) {
  const double k2Pow52 = 4503599627370496.0;
  const uint32_t kValidTopBits = 0x43300000;
  const uint64_t kBottomBitMask = V8_2PART_UINT64_C(0x00000000, FFFFFFFF);

  // Add 2^52 to the double, to place valid uint32 values in the low-significant
  // bits of the exponent, by effectively setting the (implicit) top bit of the
  // significand. Note that this addition also normalises 0.0 and -0.0.
  double shifted_value = value + k2Pow52;

  // At this point, a valid uint32 valued double will be represented as:
  //
  // sign = 0
  // exponent = 52
  // significand = 1. 00...00 <value>
  //       implicit^          ^^^^^^^ 32 bits
  //                  ^^^^^^^^^^^^^^^ 52 bits
  //
  // Therefore, we can first check the top 32 bits to make sure that the sign,
  // exponent and remaining significand bits are valid, and only then check the
  // value in the bottom 32 bits.

  uint64_t result = bit_cast<uint64_t>(shifted_value);
  if ((result >> 32) == kValidTopBits) {
    *uint32_value = result & kBottomBitMask;
    return FastUI2D(result & kBottomBitMask) == value;
  }
  return false;
}

int32_t NumberToInt32(Object* number) {
  if (number->IsSmi()) return Smi::ToInt(number);
  return DoubleToInt32(number->Number());
}

uint32_t NumberToUint32(Object* number) {
  if (number->IsSmi()) return Smi::ToInt(number);
  return DoubleToUint32(number->Number());
}

uint32_t PositiveNumberToUint32(Object* number) {
  if (number->IsSmi()) {
    int value = Smi::ToInt(number);
    if (value <= 0) return 0;
    return value;
  }
  DCHECK(number->IsHeapNumber());
  double value = number->Number();
  // Catch all values smaller than 1 and use the double-negation trick for NANs.
  if (!(value >= 1)) return 0;
  uint32_t max = std::numeric_limits<uint32_t>::max();
  if (value < max) return static_cast<uint32_t>(value);
  return max;
}

int64_t NumberToInt64(Object* number) {
  if (number->IsSmi()) return Smi::ToInt(number);
  double d = number->Number();
  if (std::isnan(d)) return 0;
  if (d >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    return std::numeric_limits<int64_t>::max();
  }
  if (d <= static_cast<double>(std::numeric_limits<int64_t>::min())) {
    return std::numeric_limits<int64_t>::min();
  }
  return static_cast<int64_t>(d);
}

uint64_t PositiveNumberToUint64(Object* number) {
  if (number->IsSmi()) {
    int value = Smi::ToInt(number);
    if (value <= 0) return 0;
    return value;
  }
  DCHECK(number->IsHeapNumber());
  double value = number->Number();
  // Catch all values smaller than 1 and use the double-negation trick for NANs.
  if (!(value >= 1)) return 0;
  uint64_t max = std::numeric_limits<uint64_t>::max();
  if (value < max) return static_cast<uint64_t>(value);
  return max;
}

bool TryNumberToSize(Object* number, size_t* result) {
  // Do not create handles in this function! Don't use SealHandleScope because
  // the function can be used concurrently.
  if (number->IsSmi()) {
    int value = Smi::ToInt(number);
    DCHECK(static_cast<unsigned>(Smi::kMaxValue) <=
           std::numeric_limits<size_t>::max());
    if (value >= 0) {
      *result = static_cast<size_t>(value);
      return true;
    }
    return false;
  } else {
    DCHECK(number->IsHeapNumber());
    double value = HeapNumber::cast(number)->value();
    // If value is compared directly to the limit, the limit will be
    // casted to a double and could end up as limit + 1,
    // because a double might not have enough mantissa bits for it.
    // So we might as well cast the limit first, and use < instead of <=.
    double maxSize = static_cast<double>(std::numeric_limits<size_t>::max());
    if (value >= 0 && value < maxSize) {
      *result = static_cast<size_t>(value);
      return true;
    } else {
      return false;
    }
  }
}

size_t NumberToSize(Object* number) {
  size_t result = 0;
  bool is_valid = TryNumberToSize(number, &result);
  CHECK(is_valid);
  return result;
}


uint32_t DoubleToUint32(double x) {
  return static_cast<uint32_t>(DoubleToInt32(x));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CONVERSIONS_INL_H_
