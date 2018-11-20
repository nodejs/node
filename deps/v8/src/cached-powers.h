// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CACHED_POWERS_H_
#define V8_CACHED_POWERS_H_

#include "src/base/logging.h"
#include "src/diy-fp.h"

namespace v8 {
namespace internal {

class PowersOfTenCache {
 public:
  // Not all powers of ten are cached. The decimal exponent of two neighboring
  // cached numbers will differ by kDecimalExponentDistance.
  static const int kDecimalExponentDistance;

  static const int kMinDecimalExponent;
  static const int kMaxDecimalExponent;

  // Returns a cached power-of-ten with a binary exponent in the range
  // [min_exponent; max_exponent] (boundaries included).
  static void GetCachedPowerForBinaryExponentRange(int min_exponent,
                                                   int max_exponent,
                                                   DiyFp* power,
                                                   int* decimal_exponent);

  // Returns a cached power of ten x ~= 10^k such that
  //   k <= decimal_exponent < k + kCachedPowersDecimalDistance.
  // The given decimal_exponent must satisfy
  //   kMinDecimalExponent <= requested_exponent, and
  //   requested_exponent < kMaxDecimalExponent + kDecimalExponentDistance.
  static void GetCachedPowerForDecimalExponent(int requested_exponent,
                                               DiyFp* power,
                                               int* found_exponent);
};

} }  // namespace v8::internal

#endif  // V8_CACHED_POWERS_H_
