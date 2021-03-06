// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <limits>

#include "src/base/build_config.h"

#if !defined(UNDEFINED_SANITIZER) || !defined(V8_TARGET_ARCH_32_BIT)
#error "This file is only needed for 32-bit UBSan builds."
#endif

// Compiling with -fsanitize=undefined on 32-bit platforms requires __mulodi4
// to be available. Usually it comes from libcompiler_rt, which our build
// doesn't provide, so here is a custom implementation (inspired by digit_mul
// in src/objects/bigint.cc).
extern "C" int64_t __mulodi4(int64_t a, int64_t b, int* overflow) {
  // Multiply in 32-bit chunks.
  // For inputs [AH AL]*[BH BL], the result is:
  //
  //            [AL*BL]  // r_low
  //    +    [AL*BH]     // r_mid1
  //    +    [AH*BL]     // r_mid2
  //    + [AH*BH]        // r_high
  //    = [R4 R3 R2 R1]  // high = [R4 R3], low = [R2 R1]
  //
  // Where of course we must be careful with carries between the columns.
  uint64_t a_low = a & 0xFFFFFFFFu;
  uint64_t a_high = static_cast<uint64_t>(a) >> 32;
  uint64_t b_low = b & 0xFFFFFFFFu;
  uint64_t b_high = static_cast<uint64_t>(b) >> 32;

  uint64_t r_low = a_low * b_low;
  uint64_t r_mid1 = a_low * b_high;
  uint64_t r_mid2 = a_high * b_low;
  uint64_t r_high = a_high * b_high;

  uint64_t result1 = r_low + (r_mid1 << 32);
  if (result1 < r_low) r_high++;
  uint64_t result2 = result1 + (r_mid2 << 32);
  if (result2 < result1) r_high++;
  r_high += (r_mid1 >> 32) + (r_mid2 >> 32);
  int64_t result = static_cast<int64_t>(result2);
  uint64_t result_sign = (result >> 63);
  uint64_t expected_result_sign = (a >> 63) ^ (b >> 63);

  *overflow = (r_high > 0 || result_sign != expected_result_sign) ? 1 : 0;
  return result;
}
