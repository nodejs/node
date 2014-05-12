// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MISC_INTRINSICS_H_
#define V8_MISC_INTRINSICS_H_

#include "../include/v8.h"
#include "globals.h"

namespace v8 {
namespace internal {

// Returns the index of the leading 1 bit, counting the least significant bit at
// index 0.  (1 << IntegerLog2(x)) is a mask for the most significant bit of x.
// Result is undefined if input is zero.
int IntegerLog2(uint32_t value);

#if defined(__GNUC__)

inline int IntegerLog2(uint32_t value) {
  return 31 - __builtin_clz(value);
}

#elif defined(_MSC_VER)

#pragma intrinsic(_BitScanReverse)

inline int IntegerLog2(uint32_t value) {
  unsigned long result;             // NOLINT: MSVC intrinsic demands this type.
  _BitScanReverse(&result, value);
  return result;
}

#else

// Default version using regular operations. Code taken from:
// http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog
inline int IntegerLog2(uint32_t value) {
  int result, shift;

  shift = (value > 0xFFFF) << 4;
  value >>= shift;
  result = shift;

  shift = (value > 0xFF) << 3;
  value >>= shift;
  result |= shift;

  shift = (value > 0xF) << 2;
  value >>= shift;
  result |= shift;

  shift = (value > 0x3) << 1;
  value >>= shift;
  result |= shift;

  result |= (value >> 1);

  return result;
}
#endif

} }  // namespace v8::internal

#endif  // V8_MISC_INTRINSICS_H_
