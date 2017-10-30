// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_IEEE754_H_
#define V8_BASE_IEEE754_H_

#include "src/base/base-export.h"

namespace v8 {
namespace base {
namespace ieee754 {

// Returns the arc cosine of |x|; that is the value whose cosine is |x|.
V8_BASE_EXPORT double acos(double x);

// Returns the inverse hyperbolic cosine of |x|; that is the value whose
// hyperbolic cosine is |x|.
V8_BASE_EXPORT double acosh(double x);

// Returns the arc sine of |x|; that is the value whose sine is |x|.
V8_BASE_EXPORT double asin(double x);

// Returns the inverse hyperbolic sine of |x|; that is the value whose
// hyperbolic sine is |x|.
V8_BASE_EXPORT double asinh(double x);

// Returns the principal value of the arc tangent of |x|; that is the value
// whose tangent is |x|.
V8_BASE_EXPORT double atan(double x);

// Returns the principal value of the arc tangent of |y/x|, using the signs of
// the two arguments to determine the quadrant of the result.
V8_BASE_EXPORT double atan2(double y, double x);

// Returns the cosine of |x|, where |x| is given in radians.
V8_BASE_EXPORT double cos(double x);

// Returns the base-e exponential of |x|.
V8_BASE_EXPORT double exp(double x);

V8_BASE_EXPORT double atanh(double x);

// Returns the natural logarithm of |x|.
V8_BASE_EXPORT double log(double x);

// Returns a value equivalent to |log(1+x)|, but computed in a way that is
// accurate even if the value of |x| is near zero.
V8_BASE_EXPORT double log1p(double x);

// Returns the base 2 logarithm of |x|.
V8_BASE_EXPORT double log2(double x);

// Returns the base 10 logarithm of |x|.
V8_BASE_EXPORT double log10(double x);

// Returns the cube root of |x|.
V8_BASE_EXPORT double cbrt(double x);

// Returns exp(x)-1, the exponential of |x| minus 1.
V8_BASE_EXPORT double expm1(double x);

// Returns the sine of |x|, where |x| is given in radians.
V8_BASE_EXPORT double sin(double x);

// Returns the tangent of |x|, where |x| is given in radians.
V8_BASE_EXPORT double tan(double x);

// Returns the hyperbolic cosine of |x|, where |x| is given radians.
V8_BASE_EXPORT double cosh(double x);

// Returns the hyperbolic sine of |x|, where |x| is given radians.
V8_BASE_EXPORT double sinh(double x);

// Returns the hyperbolic tangent of |x|, where |x| is given radians.
V8_BASE_EXPORT double tanh(double x);

}  // namespace ieee754
}  // namespace base
}  // namespace v8

#endif  // V8_BASE_IEEE754_H_
