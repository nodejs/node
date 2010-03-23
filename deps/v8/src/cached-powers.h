// Copyright 2010 the V8 project authors. All rights reserved.
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

#ifndef V8_CACHED_POWERS_H_
#define V8_CACHED_POWERS_H_

#include "diy-fp.h"

namespace v8 {
namespace internal {

struct CachedPower {
  uint64_t significand;
  int16_t binary_exponent;
  int16_t decimal_exponent;
};

// The following defines implement the interface between this file and the
// generated 'powers_ten.h'.
// GRISU_CACHE_NAME(1) contains all possible cached powers.
// GRISU_CACHE_NAME(i) contains GRISU_CACHE_NAME(1) where only every 'i'th
// element is kept. More formally GRISU_CACHE_NAME(i) contains the elements j*i
// with 0 <= j < k with k such that j*k < the size of GRISU_CACHE_NAME(1).
// The higher 'i' is the fewer elements we use.
// Given that there are less elements, the exponent-distance between two
// elements in the cache grows. The variable GRISU_CACHE_MAX_DISTANCE(i) stores
// the maximum distance between two elements.
#define GRISU_CACHE_STRUCT CachedPower
#define GRISU_CACHE_NAME(i) kCachedPowers##i
#define GRISU_CACHE_MAX_DISTANCE(i) kCachedPowersMaxDistance##i
#define GRISU_CACHE_OFFSET kCachedPowerOffset
#define GRISU_UINT64_C V8_2PART_UINT64_C
// The following include imports the precompiled cached powers.
#include "powers-ten.h"  // NOLINT

static const double kD_1_LOG2_10 = 0.30102999566398114;  //  1 / lg(10)

// We can't use a function since we reference variables depending on the 'i'.
// This way the compiler is able to see at compile time that only one
// cache-array variable is used and thus can remove all the others.
#define COMPUTE_FOR_CACHE(i) \
  if (!found && (gamma - alpha + 1 >= GRISU_CACHE_MAX_DISTANCE(i))) {   \
    int kQ = DiyFp::kSignificandSize;                                   \
    double k = ceiling((alpha - e + kQ - 1) * kD_1_LOG2_10);            \
    int index = (GRISU_CACHE_OFFSET + static_cast<int>(k) - 1) / i + 1; \
    cached_power = GRISU_CACHE_NAME(i)[index];                          \
    found = true;                                                       \
  }                                                                     \

static void GetCachedPower(int e, int alpha, int gamma, int* mk, DiyFp* c_mk) {
  // The following if statement should be optimized by the compiler so that only
  // one array is referenced and the others are not included in the object file.
  bool found = false;
  CachedPower cached_power;
  COMPUTE_FOR_CACHE(20);
  COMPUTE_FOR_CACHE(19);
  COMPUTE_FOR_CACHE(18);
  COMPUTE_FOR_CACHE(17);
  COMPUTE_FOR_CACHE(16);
  COMPUTE_FOR_CACHE(15);
  COMPUTE_FOR_CACHE(14);
  COMPUTE_FOR_CACHE(13);
  COMPUTE_FOR_CACHE(12);
  COMPUTE_FOR_CACHE(11);
  COMPUTE_FOR_CACHE(10);
  COMPUTE_FOR_CACHE(9);
  COMPUTE_FOR_CACHE(8);
  COMPUTE_FOR_CACHE(7);
  COMPUTE_FOR_CACHE(6);
  COMPUTE_FOR_CACHE(5);
  COMPUTE_FOR_CACHE(4);
  COMPUTE_FOR_CACHE(3);
  COMPUTE_FOR_CACHE(2);
  COMPUTE_FOR_CACHE(1);
  if (!found) {
    UNIMPLEMENTED();
    // Silence compiler warnings.
    cached_power.significand = 0;
    cached_power.binary_exponent = 0;
    cached_power.decimal_exponent = 0;
  }
  *c_mk = DiyFp(cached_power.significand, cached_power.binary_exponent);
  *mk = cached_power.decimal_exponent;
  ASSERT((alpha <= c_mk->e() + e) && (c_mk->e() + e <= gamma));
}
#undef GRISU_REDUCTION
#undef GRISU_CACHE_STRUCT
#undef GRISU_CACHE_NAME
#undef GRISU_CACHE_MAX_DISTANCE
#undef GRISU_CACHE_OFFSET
#undef GRISU_UINT64_C

} }  // namespace v8::internal

#endif  // V8_CACHED_POWERS_H_
