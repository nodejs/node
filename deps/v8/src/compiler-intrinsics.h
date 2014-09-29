// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INTRINSICS_H_
#define V8_COMPILER_INTRINSICS_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {

class CompilerIntrinsics {
 public:
  // Returns number of zero bits preceding least significant 1 bit.
  // Undefined for zero value.
  INLINE(static int CountTrailingZeros(uint32_t value));

  // Returns number of zero bits following most significant 1 bit.
  // Undefined for zero value.
  INLINE(static int CountLeadingZeros(uint32_t value));

  // Returns the number of bits set.
  INLINE(static int CountSetBits(uint32_t value));
};

#ifdef __GNUC__
int CompilerIntrinsics::CountTrailingZeros(uint32_t value) {
  return __builtin_ctz(value);
}

int CompilerIntrinsics::CountLeadingZeros(uint32_t value) {
  return __builtin_clz(value);
}

int CompilerIntrinsics::CountSetBits(uint32_t value) {
  return __builtin_popcount(value);
}

#elif defined(_MSC_VER)

#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_BitScanReverse)

int CompilerIntrinsics::CountTrailingZeros(uint32_t value) {
  unsigned long result;  //NOLINT
  _BitScanForward(&result, static_cast<long>(value));  //NOLINT
  return static_cast<int>(result);
}

int CompilerIntrinsics::CountLeadingZeros(uint32_t value) {
  unsigned long result;  //NOLINT
  _BitScanReverse(&result, static_cast<long>(value));  //NOLINT
  return 31 - static_cast<int>(result);
}

int CompilerIntrinsics::CountSetBits(uint32_t value) {
  // Manually count set bits.
  value = ((value >>  1) & 0x55555555) + (value & 0x55555555);
  value = ((value >>  2) & 0x33333333) + (value & 0x33333333);
  value = ((value >>  4) & 0x0f0f0f0f) + (value & 0x0f0f0f0f);
  value = ((value >>  8) & 0x00ff00ff) + (value & 0x00ff00ff);
  value = ((value >> 16) & 0x0000ffff) + (value & 0x0000ffff);
  return value;
}

#else
#error Unsupported compiler
#endif

} }  // namespace v8::internal

#endif  // V8_COMPILER_INTRINSICS_H_
