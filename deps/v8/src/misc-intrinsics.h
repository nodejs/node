// Copyright 2011 the V8 project authors. All rights reserved.
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
