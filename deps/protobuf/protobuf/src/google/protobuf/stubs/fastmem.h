// Protocol Buffers - Google's data interchange format
// Copyright 2014 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

// Fast memory copying and comparison routines.
//   strings::fastmemcmp_inlined() replaces memcmp()
//   strings::memcpy_inlined() replaces memcpy()
//   strings::memeq(a, b, n) replaces memcmp(a, b, n) == 0
//
// strings::*_inlined() routines are inline versions of the
// routines exported by this module.  Sometimes using the inlined
// versions is faster.  Measure before using the inlined versions.
//
// Performance measurement:
//   strings::fastmemcmp_inlined
//     Analysis: memcmp, fastmemcmp_inlined, fastmemcmp
//     2012-01-30

#ifndef GOOGLE_PROTOBUF_STUBS_FASTMEM_H_
#define GOOGLE_PROTOBUF_STUBS_FASTMEM_H_

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <google/protobuf/stubs/common.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace internal {

// Return true if the n bytes at a equal the n bytes at b.
// The regions are allowed to overlap.
//
// The performance is similar to the performance memcmp(), but faster for
// moderately-sized inputs, or inputs that share a common prefix and differ
// somewhere in their last 8 bytes. Further optimizations can be added later
// if it makes sense to do so.:w
inline bool memeq(const char* a, const char* b, size_t n) {
  size_t n_rounded_down = n & ~static_cast<size_t>(7);
  if (PROTOBUF_PREDICT_FALSE(n_rounded_down == 0)) {  // n <= 7
    return memcmp(a, b, n) == 0;
  }
  // n >= 8
  uint64 u = GOOGLE_UNALIGNED_LOAD64(a) ^ GOOGLE_UNALIGNED_LOAD64(b);
  uint64 v = GOOGLE_UNALIGNED_LOAD64(a + n - 8) ^ GOOGLE_UNALIGNED_LOAD64(b + n - 8);
  if ((u | v) != 0) {  // The first or last 8 bytes differ.
    return false;
  }
  a += 8;
  b += 8;
  n = n_rounded_down - 8;
  if (n > 128) {
    // As of 2012, memcmp on x86-64 uses a big unrolled loop with SSE2
    // instructions, and while we could try to do something faster, it
    // doesn't seem worth pursuing.
    return memcmp(a, b, n) == 0;
  }
  for (; n >= 16; n -= 16) {
    uint64 x = GOOGLE_UNALIGNED_LOAD64(a) ^ GOOGLE_UNALIGNED_LOAD64(b);
    uint64 y = GOOGLE_UNALIGNED_LOAD64(a + 8) ^ GOOGLE_UNALIGNED_LOAD64(b + 8);
    if ((x | y) != 0) {
      return false;
    }
    a += 16;
    b += 16;
  }
  // n must be 0 or 8 now because it was a multiple of 8 at the top of the loop.
  return n == 0 || GOOGLE_UNALIGNED_LOAD64(a) == GOOGLE_UNALIGNED_LOAD64(b);
}

inline int fastmemcmp_inlined(const char *a, const char *b, size_t n) {
  if (n >= 64) {
    return memcmp(a, b, n);
  }
  const char* a_limit = a + n;
  while (a + sizeof(uint64) <= a_limit &&
         GOOGLE_UNALIGNED_LOAD64(a) == GOOGLE_UNALIGNED_LOAD64(b)) {
    a += sizeof(uint64);
    b += sizeof(uint64);
  }
  if (a + sizeof(uint32) <= a_limit &&
      GOOGLE_UNALIGNED_LOAD32(a) == GOOGLE_UNALIGNED_LOAD32(b)) {
    a += sizeof(uint32);
    b += sizeof(uint32);
  }
  while (a < a_limit) {
    int d =
        static_cast<int>(static_cast<uint32>(*a++) - static_cast<uint32>(*b++));
    if (d) return d;
  }
  return 0;
}

// The standard memcpy operation is slow for variable small sizes.
// This implementation inlines the optimal realization for sizes 1 to 16.
// To avoid code bloat don't use it in case of not performance-critical spots,
// nor when you don't expect very frequent values of size <= 16.
inline void memcpy_inlined(char *dst, const char *src, size_t size) {
  // Compiler inlines code with minimal amount of data movement when third
  // parameter of memcpy is a constant.
  switch (size) {
    case  1: memcpy(dst, src, 1); break;
    case  2: memcpy(dst, src, 2); break;
    case  3: memcpy(dst, src, 3); break;
    case  4: memcpy(dst, src, 4); break;
    case  5: memcpy(dst, src, 5); break;
    case  6: memcpy(dst, src, 6); break;
    case  7: memcpy(dst, src, 7); break;
    case  8: memcpy(dst, src, 8); break;
    case  9: memcpy(dst, src, 9); break;
    case 10: memcpy(dst, src, 10); break;
    case 11: memcpy(dst, src, 11); break;
    case 12: memcpy(dst, src, 12); break;
    case 13: memcpy(dst, src, 13); break;
    case 14: memcpy(dst, src, 14); break;
    case 15: memcpy(dst, src, 15); break;
    case 16: memcpy(dst, src, 16); break;
    default: memcpy(dst, src, size); break;
  }
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_STUBS_FASTMEM_H_
