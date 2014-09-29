// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Extra POSIX/ANSI routines for Win32 when using Visual Studio C++. Please
// refer to The Open Group Base Specification for specification of the correct
// semantics for these functions.
// (http://www.opengroup.org/onlinepubs/000095399/)
#if defined(_MSC_VER) && (_MSC_VER < 1800)

#include "src/base/win32-headers.h"
#include <float.h>         // Required for DBL_MAX and on Win32 for finite()
#include <limits.h>        // Required for INT_MAX etc.
#include <cmath>
#include "src/base/win32-math.h"

#include "src/base/logging.h"


namespace std {

// Test for a NaN (not a number) value - usually defined in math.h
int isnan(double x) {
  return _isnan(x);
}


// Test for infinity - usually defined in math.h
int isinf(double x) {
  return (_fpclass(x) & (_FPCLASS_PINF | _FPCLASS_NINF)) != 0;
}


// Test for finite value - usually defined in math.h
int isfinite(double x) {
  return _finite(x);
}


// Test if x is less than y and both nominal - usually defined in math.h
int isless(double x, double y) {
  return isnan(x) || isnan(y) ? 0 : x < y;
}


// Test if x is greater than y and both nominal - usually defined in math.h
int isgreater(double x, double y) {
  return isnan(x) || isnan(y) ? 0 : x > y;
}


// Classify floating point number - usually defined in math.h
int fpclassify(double x) {
  // Use the MS-specific _fpclass() for classification.
  int flags = _fpclass(x);

  // Determine class. We cannot use a switch statement because
  // the _FPCLASS_ constants are defined as flags.
  if (flags & (_FPCLASS_PN | _FPCLASS_NN)) return FP_NORMAL;
  if (flags & (_FPCLASS_PZ | _FPCLASS_NZ)) return FP_ZERO;
  if (flags & (_FPCLASS_PD | _FPCLASS_ND)) return FP_SUBNORMAL;
  if (flags & (_FPCLASS_PINF | _FPCLASS_NINF)) return FP_INFINITE;

  // All cases should be covered by the code above.
  DCHECK(flags & (_FPCLASS_SNAN | _FPCLASS_QNAN));
  return FP_NAN;
}


// Test sign - usually defined in math.h
int signbit(double x) {
  // We need to take care of the special case of both positive
  // and negative versions of zero.
  if (x == 0)
    return _fpclass(x) & _FPCLASS_NZ;
  else
    return x < 0;
}

}  // namespace std

#endif  // _MSC_VER
