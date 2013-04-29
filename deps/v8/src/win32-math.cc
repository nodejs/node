// Copyright 2011 the V8 project authors. All rights reserved.
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

// Extra POSIX/ANSI routines for Win32 when using Visual Studio C++. Please
// refer to The Open Group Base Specification for specification of the correct
// semantics for these functions.
// (http://www.opengroup.org/onlinepubs/000095399/)
#ifdef _MSC_VER

#undef V8_WIN32_LEAN_AND_MEAN
#define V8_WIN32_HEADERS_FULL
#include "win32-headers.h"
#include <limits.h>        // Required for INT_MAX etc.
#include <float.h>         // Required for DBL_MAX and on Win32 for finite()
#include <cmath>
#include "win32-math.h"

#include "checks.h"


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
  ASSERT(flags & (_FPCLASS_SNAN | _FPCLASS_QNAN));
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
