// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_IEEE754_H_
#define V8_BASE_IEEE754_H_

namespace v8 {
namespace base {
namespace ieee754 {

// Returns the arc cosine of |x|; that is the value whose cosine is |x|.
double acos(double x);

// Returns the inverse hyperbolic cosine of |x|; that is the value whose
// hyperbolic cosine is |x|.
double acosh(double x);

// Returns the arc sine of |x|; that is the value whose sine is |x|.
double asin(double x);

// Returns the inverse hyperbolic sine of |x|; that is the value whose
// hyperbolic sine is |x|.
double asinh(double x);

// Returns the principal value of the arc tangent of |x|; that is the value
// whose tangent is |x|.
double atan(double x);

// Returns the principal value of the arc tangent of |y/x|, using the signs of
// the two arguments to determine the quadrant of the result.
double atan2(double y, double x);

// Returns the cosine of |x|, where |x| is given in radians.
double cos(double x);

// Returns the base-e exponential of |x|.
double exp(double x);

double atanh(double x);

// Returns the natural logarithm of |x|.
double log(double x);

// Returns a value equivalent to |log(1+x)|, but computed in a way that is
// accurate even if the value of |x| is near zero.
double log1p(double x);

// Returns the base 2 logarithm of |x|.
double log2(double x);

// Returns the base 10 logarithm of |x|.
double log10(double x);

// Returns the cube root of |x|.
double cbrt(double x);

// Returns exp(x)-1, the exponential of |x| minus 1.
double expm1(double x);

// Returns the sine of |x|, where |x| is given in radians.
double sin(double x);

// Returns the tangent of |x|, where |x| is given in radians.
double tan(double x);

// Returns the hyperbolic cosine of |x|, where |x| is given radians.
double cosh(double x);

// Returns the hyperbolic sine of |x|, where |x| is given radians.
double sinh(double x);

// Returns the hyperbolic tangent of |x|, where |x| is given radians.
double tanh(double x);

}  // namespace ieee754
}  // namespace base
}  // namespace v8

#endif  // V8_BASE_IEEE754_H_
