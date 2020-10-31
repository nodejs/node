// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
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

// All Rights Reserved.
//
// Author: Maxim Lifantsev
//

#include <google/protobuf/stubs/mathlimits.h>

#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

// http://en.wikipedia.org/wiki/Quadruple_precision_floating-point_format#Double-double_arithmetic
// With some compilers (gcc 4.6.x) on some platforms (powerpc64),
// "long double" is implemented as a pair of double: "double double" format.
// This causes a problem with epsilon (eps).
// eps is the smallest positive number such that 1.0 + eps > 1.0
//
// Normal format:  1.0 + e = 1.0...01      // N-1 zeros for N fraction bits
// D-D format:     1.0 + e = 1.000...0001  // epsilon can be very small
//
// In the normal format, 1.0 + e has to fit in one stretch of bits.
// The maximum rounding error is half of eps.
//
// In the double-double format, 1.0 + e splits across two doubles:
// 1.0 in the high double, e in the low double, and they do not have to
// be contiguous.  The maximum rounding error on a value close to 1.0 is
// much larger than eps.
//
// Some code checks for errors by comparing a computed value to a golden
// value +/- some multiple of the maximum rounding error.  The maximum
// rounding error is not available so we use eps as an approximation
// instead.  That fails when long double is in the double-double format.
// Therefore, we define kStdError as a multiple of
// max(DBL_EPSILON * DBL_EPSILON, kEpsilon) rather than a multiple of kEpsilon.

#define DEF_FP_LIMITS(Type, PREFIX) \
const Type MathLimits<Type>::kPosMin = PREFIX##_MIN; \
const Type MathLimits<Type>::kPosMax = PREFIX##_MAX; \
const Type MathLimits<Type>::kMin = -MathLimits<Type>::kPosMax; \
const Type MathLimits<Type>::kMax = MathLimits<Type>::kPosMax; \
const Type MathLimits<Type>::kNegMin = -MathLimits<Type>::kPosMin; \
const Type MathLimits<Type>::kNegMax = -MathLimits<Type>::kPosMax; \
const Type MathLimits<Type>::kEpsilon = PREFIX##_EPSILON; \
/* 32 is 5 bits of mantissa error; should be adequate for common errors */ \
const Type MathLimits<Type>::kStdError = \
  32 * (DBL_EPSILON * DBL_EPSILON > MathLimits<Type>::kEpsilon \
      ? DBL_EPSILON * DBL_EPSILON : MathLimits<Type>::kEpsilon); \
const Type MathLimits<Type>::kNaN = HUGE_VAL - HUGE_VAL; \
const Type MathLimits<Type>::kPosInf = HUGE_VAL; \
const Type MathLimits<Type>::kNegInf = -HUGE_VAL;

DEF_FP_LIMITS(float, FLT)
DEF_FP_LIMITS(double, DBL)
DEF_FP_LIMITS(long double, LDBL);

#undef DEF_FP_LIMITS
}  // namespace protobuf
}  // namespace google
