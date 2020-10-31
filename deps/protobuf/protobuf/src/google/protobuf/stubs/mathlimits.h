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
// Useful integer and floating point limits and type traits.
//
// This partially replaces/duplictes numeric_limits<> from <limits>.
// We get a Google-style class that we have a greater control over
// and thus can add new features to it or fix whatever happens to be broken in
// numeric_limits for the compilers we use.
//

#ifndef UTIL_MATH_MATHLIMITS_H__
#define UTIL_MATH_MATHLIMITS_H__

// Note that for Windows we do something different because it does not support
// the plain isinf and isnan.
#if __cplusplus >= 201103L
// GCC 4.9 has a bug that makes isinf and isnan ambigious when both <math.h>
// and <cmath> get pulled into the same translation unit. We use the ones in
// std:: namespace explicitly for C++11
#include <cmath>
#define GOOGLE_PROTOBUF_USE_STD_CMATH
#elif _GLIBCXX_USE_C99_MATH && !_GLIBCXX_USE_C99_FP_MACROS_DYNAMIC
// libstdc++ <cmath> header undefines the global macros and put functions in
// std:: namespace even before C++11. Use the ones in std:: instead too.
#include <cmath>
#define GOOGLE_PROTOBUF_USE_STD_CMATH
#else
#include <math.h>
#endif

#include <string.h>

#include <cfloat>

#include <google/protobuf/stubs/common.h>

#include <google/protobuf/port_def.inc>

// ========================================================================= //

// Useful integer and floating point limits and type traits.
// This is just for the documentation;
// real members are defined in our specializations below.
namespace google {
namespace protobuf {
template<typename T> struct MathLimits {
  // Type name.
  typedef T Type;
  // Unsigned version of the Type with the same byte size.
  // Same as Type for floating point and unsigned types.
  typedef T UnsignedType;
  // If the type supports negative values.
  static const bool kIsSigned;
  // If the type supports only integer values.
  static const bool kIsInteger;
  // Magnitude-wise smallest representable positive value.
  static const Type kPosMin;
  // Magnitude-wise largest representable positive value.
  static const Type kPosMax;
  // Smallest representable value.
  static const Type kMin;
  // Largest representable value.
  static const Type kMax;
  // Magnitude-wise smallest representable negative value.
  // Present only if kIsSigned.
  static const Type kNegMin;
  // Magnitude-wise largest representable negative value.
  // Present only if kIsSigned.
  static const Type kNegMax;
  // Smallest integer x such that 10^x is representable.
  static const int kMin10Exp;
  // Largest integer x such that 10^x is representable.
  static const int kMax10Exp;
  // Smallest positive value such that Type(1) + kEpsilon != Type(1)
  static const Type kEpsilon;
  // Typical rounding error that is enough to cover
  // a few simple floating-point operations.
  // Slightly larger than kEpsilon to account for a few rounding errors.
  // Is zero if kIsInteger.
  static const Type kStdError;
  // Number of decimal digits of mantissa precision.
  // Present only if !kIsInteger.
  static const int kPrecisionDigits;
  // Not a number, i.e. result of 0/0.
  // Present only if !kIsInteger.
  static const Type kNaN;
  // Positive infinity, i.e. result of 1/0.
  // Present only if !kIsInteger.
  static const Type kPosInf;
  // Negative infinity, i.e. result of -1/0.
  // Present only if !kIsInteger.
  static const Type kNegInf;

  // NOTE: Special floating point values behave
  // in a special (but mathematically-logical) way
  // in terms of (in)equalty comparison and mathematical operations
  // -- see out unittest for examples.

  // Special floating point value testers.
  // Present in integer types for convenience.
  static bool IsFinite(const Type x);
  static bool IsNaN(const Type x);
  static bool IsInf(const Type x);
  static bool IsPosInf(const Type x);
  static bool IsNegInf(const Type x);
};

// ========================================================================= //

// All #define-s below are simply to refactor the declarations of
// MathLimits template specializations.
// They are all #undef-ined below.

// The hoop-jumping in *_INT_(MAX|MIN) below is so that the compiler does not
// get an overflow while computing the constants.

#define SIGNED_INT_MAX(Type) \
  (((Type(1) << (sizeof(Type)*8 - 2)) - 1) + (Type(1) << (sizeof(Type)*8 - 2)))

#define SIGNED_INT_MIN(Type) \
  (-(Type(1) << (sizeof(Type)*8 - 2)) - (Type(1) << (sizeof(Type)*8 - 2)))

#define UNSIGNED_INT_MAX(Type) \
  (((Type(1) << (sizeof(Type)*8 - 1)) - 1) + (Type(1) << (sizeof(Type)*8 - 1)))

// Compile-time selected log10-related constants for integer types.
#define SIGNED_MAX_10_EXP(Type) \
  (sizeof(Type) == 1 ? 2 : ( \
    sizeof(Type) == 2 ? 4 : ( \
      sizeof(Type) == 4 ? 9 : ( \
        sizeof(Type) == 8 ? 18 : -1))))

#define UNSIGNED_MAX_10_EXP(Type) \
  (sizeof(Type) == 1 ? 2 : ( \
    sizeof(Type) == 2 ? 4 : ( \
      sizeof(Type) == 4 ? 9 : ( \
        sizeof(Type) == 8 ? 19 : -1))))

#define DECL_INT_LIMIT_FUNCS \
  static bool IsFinite(const Type /*x*/) { return true; } \
  static bool IsNaN(const Type /*x*/) { return false; } \
  static bool IsInf(const Type /*x*/) { return false; } \
  static bool IsPosInf(const Type /*x*/) { return false; } \
  static bool IsNegInf(const Type /*x*/) { return false; }

#define DECL_SIGNED_INT_LIMITS(IntType, UnsignedIntType)  \
  template <>                                             \
  struct PROTOBUF_EXPORT MathLimits<IntType> {            \
    typedef IntType Type;                                 \
    typedef UnsignedIntType UnsignedType;                 \
    static const bool kIsSigned = true;                   \
    static const bool kIsInteger = true;                  \
    static const Type kPosMin = 1;                        \
    static const Type kPosMax = SIGNED_INT_MAX(Type);     \
    static const Type kMin = SIGNED_INT_MIN(Type);        \
    static const Type kMax = kPosMax;                     \
    static const Type kNegMin = -1;                       \
    static const Type kNegMax = kMin;                     \
    static const int kMin10Exp = 0;                       \
    static const int kMax10Exp = SIGNED_MAX_10_EXP(Type); \
    static const Type kEpsilon = 1;                       \
    static const Type kStdError = 0;                      \
    DECL_INT_LIMIT_FUNCS                                  \
  };

#define DECL_UNSIGNED_INT_LIMITS(IntType)                   \
  template <>                                               \
  struct PROTOBUF_EXPORT MathLimits<IntType> {              \
    typedef IntType Type;                                   \
    typedef IntType UnsignedType;                           \
    static const bool kIsSigned = false;                    \
    static const bool kIsInteger = true;                    \
    static const Type kPosMin = 1;                          \
    static const Type kPosMax = UNSIGNED_INT_MAX(Type);     \
    static const Type kMin = 0;                             \
    static const Type kMax = kPosMax;                       \
    static const int kMin10Exp = 0;                         \
    static const int kMax10Exp = UNSIGNED_MAX_10_EXP(Type); \
    static const Type kEpsilon = 1;                         \
    static const Type kStdError = 0;                        \
    DECL_INT_LIMIT_FUNCS                                    \
  };

DECL_SIGNED_INT_LIMITS(signed char, unsigned char)
DECL_SIGNED_INT_LIMITS(signed short int, unsigned short int)
DECL_SIGNED_INT_LIMITS(signed int, unsigned int)
DECL_SIGNED_INT_LIMITS(signed long int, unsigned long int)
DECL_SIGNED_INT_LIMITS(signed long long int, unsigned long long int)
DECL_UNSIGNED_INT_LIMITS(unsigned char)
DECL_UNSIGNED_INT_LIMITS(unsigned short int)
DECL_UNSIGNED_INT_LIMITS(unsigned int)
DECL_UNSIGNED_INT_LIMITS(unsigned long int)
DECL_UNSIGNED_INT_LIMITS(unsigned long long int)

#undef DECL_SIGNED_INT_LIMITS
#undef DECL_UNSIGNED_INT_LIMITS
#undef SIGNED_INT_MAX
#undef SIGNED_INT_MIN
#undef UNSIGNED_INT_MAX
#undef SIGNED_MAX_10_EXP
#undef UNSIGNED_MAX_10_EXP
#undef DECL_INT_LIMIT_FUNCS

// For non-Windows builds we use the std:: versions of isinf and isnan if they
// are available; see the comment about <cmath> at the top of this file for the
// details on why we need to do this.
#ifdef GOOGLE_PROTOBUF_USE_STD_CMATH
#define ISINF std::isinf
#define ISNAN std::isnan
#else
#define ISINF isinf
#define ISNAN isnan
#endif

// ========================================================================= //
#if defined(_WIN32) && !defined(__MINGW32__) // Lacks built-in isnan() and isinf()
#define DECL_FP_LIMIT_FUNCS \
  static bool IsFinite(const Type x) { return _finite(x); } \
  static bool IsNaN(const Type x) { return _isnan(x); } \
  static bool IsInf(const Type x) { return (_fpclass(x) & (_FPCLASS_NINF | _FPCLASS_PINF)) != 0; } \
  static bool IsPosInf(const Type x) { return _fpclass(x) == _FPCLASS_PINF; } \
  static bool IsNegInf(const Type x) { return _fpclass(x) == _FPCLASS_NINF; }
#else
#define DECL_FP_LIMIT_FUNCS \
  static bool IsFinite(const Type x) { return !ISINF(x) && !ISNAN(x); } \
  static bool IsNaN(const Type x) { return ISNAN(x); } \
  static bool IsInf(const Type x) { return ISINF(x); } \
  static bool IsPosInf(const Type x) { return ISINF(x) && x > 0; } \
  static bool IsNegInf(const Type x) { return ISINF(x) && x < 0; }
#endif

// We can't put floating-point constant values in the header here because
// such constants are not considered to be primitive-type constants by gcc.
// CAVEAT: Hence, they are going to be initialized only during
// the global objects construction time.
#define DECL_FP_LIMITS(FP_Type, PREFIX)               \
  template <>                                         \
  struct PROTOBUF_EXPORT MathLimits<FP_Type> {        \
    typedef FP_Type Type;                             \
    typedef FP_Type UnsignedType;                     \
    static const bool kIsSigned = true;               \
    static const bool kIsInteger = false;             \
    static const Type kPosMin;                        \
    static const Type kPosMax;                        \
    static const Type kMin;                           \
    static const Type kMax;                           \
    static const Type kNegMin;                        \
    static const Type kNegMax;                        \
    static const int kMin10Exp = PREFIX##_MIN_10_EXP; \
    static const int kMax10Exp = PREFIX##_MAX_10_EXP; \
    static const Type kEpsilon;                       \
    static const Type kStdError;                      \
    static const int kPrecisionDigits = PREFIX##_DIG; \
    static const Type kNaN;                           \
    static const Type kPosInf;                        \
    static const Type kNegInf;                        \
    DECL_FP_LIMIT_FUNCS                               \
  };

DECL_FP_LIMITS(float, FLT)
DECL_FP_LIMITS(double, DBL)
DECL_FP_LIMITS(long double, LDBL)

#undef ISINF
#undef ISNAN
#undef DECL_FP_LIMITS
#undef DECL_FP_LIMIT_FUNCS

// ========================================================================= //
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // UTIL_MATH_MATHLIMITS_H__
