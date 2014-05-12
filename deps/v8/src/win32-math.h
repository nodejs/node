// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Extra POSIX/ANSI routines for Win32 when using Visual Studio C++. Please
// refer to The Open Group Base Specification for specification of the correct
// semantics for these functions.
// (http://www.opengroup.org/onlinepubs/000095399/)

#ifndef V8_WIN32_MATH_H_
#define V8_WIN32_MATH_H_

#ifndef _MSC_VER
#error Wrong environment, expected MSVC.
#endif  // _MSC_VER

// MSVC 2013+ provides implementations of all standard math functions.
#if (_MSC_VER < 1800)
enum {
  FP_NAN,
  FP_INFINITE,
  FP_ZERO,
  FP_SUBNORMAL,
  FP_NORMAL
};


namespace std {

int isfinite(double x);
int isinf(double x);
int isnan(double x);
int isless(double x, double y);
int isgreater(double x, double y);
int fpclassify(double x);
int signbit(double x);

}  // namespace std

#endif  // _MSC_VER < 1800

#endif  // V8_WIN32_MATH_H_
