// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_COMPILER_SPECIFIC_H_
#define V8_BASE_COMPILER_SPECIFIC_H_

#include "include/v8config.h"

// Annotate a typedef or function indicating it's ok if it's not used.
// Use like:
//   typedef Foo Bar ALLOW_UNUSED_TYPE;
#if V8_HAS_ATTRIBUTE_UNUSED
#define ALLOW_UNUSED_TYPE __attribute__((unused))
#else
#define ALLOW_UNUSED_TYPE
#endif

// Tell the compiler a function is using a printf-style format string.
// |format_param| is the one-based index of the format string parameter;
// |dots_param| is the one-based index of the "..." parameter.
// For v*printf functions (which take a va_list), pass 0 for dots_param.
// (This is undocumented but matches what the system C headers do.)
#if defined(__GNUC__)
#define PRINTF_FORMAT(format_param, dots_param) \
  __attribute__((format(printf, format_param, dots_param)))
#else
#define PRINTF_FORMAT(format_param, dots_param)
#endif

// The C++ standard requires that static const members have an out-of-class
// definition (in a single compilation unit), but MSVC chokes on this (when
// language extensions, which are required, are enabled). (You're only likely to
// notice the need for a definition if you take the address of the member or,
// more commonly, pass it to a function that takes it as a reference argument --
// probably an STL function.) This macro makes MSVC do the right thing. See
// http://msdn.microsoft.com/en-us/library/34h23df8(v=vs.100).aspx for more
// information. Use like:
//
// In .h file:
//   struct Foo {
//     static const int kBar = 5;
//   };
//
// In .cc file:
//   STATIC_CONST_MEMBER_DEFINITION const int Foo::kBar;
#if V8_HAS_DECLSPEC_SELECTANY
#define STATIC_CONST_MEMBER_DEFINITION __declspec(selectany)
#else
#define STATIC_CONST_MEMBER_DEFINITION
#endif

#if V8_CC_MSVC

#include <sal.h>

// Macros for suppressing and disabling warnings on MSVC.
//
// Warning numbers are enumerated at:
// http://msdn.microsoft.com/en-us/library/8x5x43k7(VS.80).aspx
//
// The warning pragma:
// http://msdn.microsoft.com/en-us/library/2c8f766e(VS.80).aspx
//
// Using __pragma instead of #pragma inside macros:
// http://msdn.microsoft.com/en-us/library/d9x1s805.aspx

// MSVC_SUPPRESS_WARNING disables warning |n| for the remainder of the line and
// for the next line of the source file.
#define MSVC_SUPPRESS_WARNING(n) __pragma(warning(suppress : n))

// Allows exporting a class that inherits from a non-exported base class.
// This uses suppress instead of push/pop because the delimiter after the
// declaration (either "," or "{") has to be placed before the pop macro.
//
// Example usage:
// class EXPORT_API Foo : NON_EXPORTED_BASE(public Bar) {
//
// MSVC Compiler warning C4275:
// non dll-interface class 'Bar' used as base for dll-interface class 'Foo'.
// Note that this is intended to be used only when no access to the base class'
// static data is done through derived classes or inline methods. For more info,
// see http://msdn.microsoft.com/en-us/library/3tdb471s(VS.80).aspx
#define NON_EXPORTED_BASE(code) \
  MSVC_SUPPRESS_WARNING(4275)   \
  code

#else  // Not MSVC

#define MSVC_SUPPRESS_WARNING(n)
#define NON_EXPORTED_BASE(code) code

#endif  // V8_CC_MSVC

// Allowing the use of noexcept by removing the keyword on older compilers that
// do not support adding noexcept to default members.
#if ((!defined(V8_CC_GNU) && !defined(V8_TARGET_ARCH_MIPS) &&        \
      !defined(V8_TARGET_ARCH_MIPS64) && !defined(V8_TARGET_ARCH_PPC) && \
      !defined(V8_TARGET_ARCH_PPC64)) ||                                 \
     (defined(__clang__) && __cplusplus > 201300L))
#define V8_NOEXCEPT noexcept
#else
#define V8_NOEXCEPT
#endif

#endif  // V8_BASE_COMPILER_SPECIFIC_H_
