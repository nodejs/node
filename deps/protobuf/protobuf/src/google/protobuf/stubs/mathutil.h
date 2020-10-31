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
#ifndef GOOGLE_PROTOBUF_STUBS_MATHUTIL_H_
#define GOOGLE_PROTOBUF_STUBS_MATHUTIL_H_

#include <float.h>
#include <math.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/mathlimits.h>

namespace google {
namespace protobuf {
namespace internal {
template<typename T>
bool AlmostEquals(T a, T b) {
  return a == b;
}
template<>
inline bool AlmostEquals(float a, float b) {
  return fabs(a - b) < 32 * FLT_EPSILON;
}

template<>
inline bool AlmostEquals(double a, double b) {
  return fabs(a - b) < 32 * DBL_EPSILON;
}
}  // namespace internal

class MathUtil {
 public:
  template<typename T>
  static T Sign(T value) {
    if (value == T(0) || MathLimits<T>::IsNaN(value)) {
      return value;
    }
    return value > T(0) ? 1 : -1;
  }

  template<typename T>
  static bool AlmostEquals(T a, T b) {
    return internal::AlmostEquals(a, b);
  }

  // Largest of two values.
  // Works correctly for special floating point values.
  // Note: 0.0 and -0.0 are not differentiated by Max (Max(0.0, -0.0) is -0.0),
  // which should be OK because, although they (can) have different
  // bit representation, they are observably the same when examined
  // with arithmetic and (in)equality operators.
  template<typename T>
  static T Max(const T x, const T y) {
    return MathLimits<T>::IsNaN(x) || x > y ? x : y;
  }

  // Absolute value of x
  // Works correctly for unsigned types and
  // for special floating point values.
  // Note: 0.0 and -0.0 are not differentiated by Abs (Abs(0.0) is -0.0),
  // which should be OK: see the comment for Max above.
  template<typename T>
  static T Abs(const T x) {
    return x > T(0) ? x : -x;
  }

  // Absolute value of the difference between two numbers.
  // Works correctly for signed types and special floating point values.
  template<typename T>
  static typename MathLimits<T>::UnsignedType AbsDiff(const T x, const T y) {
    // Carries out arithmetic as unsigned to avoid overflow.
    typedef typename MathLimits<T>::UnsignedType R;
    return x > y ? R(x) - R(y) : R(y) - R(x);
  }

  // If two (usually floating point) numbers are within a certain
  // fraction of their magnitude or within a certain absolute margin of error.
  // This is the same as the following but faster:
  //   WithinFraction(x, y, fraction)  ||  WithinMargin(x, y, margin)
  // E.g. WithinFraction(0.0, 1e-10, 1e-5) is false but
  //      WithinFractionOrMargin(0.0, 1e-10, 1e-5, 1e-5) is true.
  template<typename T>
  static bool WithinFractionOrMargin(const T x, const T y,
                                     const T fraction, const T margin);
};

template<typename T>
bool MathUtil::WithinFractionOrMargin(const T x, const T y,
                                      const T fraction, const T margin) {
  // Not just "0 <= fraction" to fool the compiler for unsigned types.
  GOOGLE_DCHECK((T(0) < fraction || T(0) == fraction) &&
         fraction < T(1) &&
         margin >= T(0));

  // Template specialization will convert the if() condition to a constant,
  // which will cause the compiler to generate code for either the "if" part
  // or the "then" part.  In this way we avoid a compiler warning
  // about a potential integer overflow in crosstool v12 (gcc 4.3.1).
  if (MathLimits<T>::kIsInteger) {
    return x == y;
  } else {
    if (!MathLimits<T>::IsFinite(x) || !MathLimits<T>::IsFinite(y)) {
      return false;
    }
    T relative_margin = static_cast<T>(fraction * Max(Abs(x), Abs(y)));
    return AbsDiff(x, y) <= Max(margin, relative_margin);
  }
}

}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_STUBS_MATHUTIL_H_
