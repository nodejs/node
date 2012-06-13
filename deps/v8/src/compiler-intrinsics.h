// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_COMPILER_INTRINSICS_H_
#define V8_COMPILER_INTRINSICS_H_

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
