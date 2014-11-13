// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This also contains public domain code from MurmurHash. From the
// MurmurHash header:
//
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#include "src/base/functional.h"

#include <limits>

#include "src/base/bits.h"

namespace v8 {
namespace base {

namespace {

// Thomas Wang, Integer Hash Functions.
// https://gist.github.com/badboy/6267743
template <typename T>
V8_INLINE size_t hash_value_unsigned(T v) {
  switch (sizeof(T)) {
    case 4: {
      // "32 bit Mix Functions"
      v = ~v + (v << 15);  // v = (v << 15) - v - 1;
      v = v ^ (v >> 12);
      v = v + (v << 2);
      v = v ^ (v >> 4);
      v = v * 2057;  // v = (v + (v << 3)) + (v << 11);
      v = v ^ (v >> 16);
      return static_cast<size_t>(v);
    }
    case 8: {
      switch (sizeof(size_t)) {
        case 4: {
          // "64 bit to 32 bit Hash Functions"
          v = ~v + (v << 18);  // v = (v << 18) - v - 1;
          v = v ^ (v >> 31);
          v = v * 21;  // v = (v + (v << 2)) + (v << 4);
          v = v ^ (v >> 11);
          v = v + (v << 6);
          v = v ^ (v >> 22);
          return static_cast<size_t>(v);
        }
        case 8: {
          // "64 bit Mix Functions"
          v = ~v + (v << 21);  // v = (v << 21) - v - 1;
          v = v ^ (v >> 24);
          v = (v + (v << 3)) + (v << 8);  // v * 265
          v = v ^ (v >> 14);
          v = (v + (v << 2)) + (v << 4);  // v * 21
          v = v ^ (v >> 28);
          v = v + (v << 31);
          return static_cast<size_t>(v);
        }
      }
    }
  }
  UNREACHABLE();
  return static_cast<size_t>(v);
}

}  // namespace


// This code was taken from MurmurHash.
size_t hash_combine(size_t seed, size_t value) {
#if V8_HOST_ARCH_32_BIT
  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  value *= c1;
  value = bits::RotateRight32(value, 15);
  value *= c2;

  seed ^= value;
  seed = bits::RotateRight32(seed, 13);
  seed = seed * 5 + 0xe6546b64;
#else
  const uint64_t m = V8_UINT64_C(0xc6a4a7935bd1e995);
  const uint32_t r = 47;

  value *= m;
  value ^= value >> r;
  value *= m;

  seed ^= value;
  seed *= m;
#endif  // V8_HOST_ARCH_32_BIT
  return seed;
}


size_t hash_value(unsigned int v) { return hash_value_unsigned(v); }


size_t hash_value(unsigned long v) {  // NOLINT(runtime/int)
  return hash_value_unsigned(v);
}


size_t hash_value(unsigned long long v) {  // NOLINT(runtime/int)
  return hash_value_unsigned(v);
}

}  // namespace base
}  // namespace v8
