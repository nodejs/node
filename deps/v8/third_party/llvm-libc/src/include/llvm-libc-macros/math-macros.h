//===-- Definition of macros from math.h ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_MATH_MACROS_H
#define LLVM_LIBC_MACROS_MATH_MACROS_H

#include "limits-macros.h"

#define FP_NAN 0
#define FP_INFINITE 1
#define FP_ZERO 2
#define FP_SUBNORMAL 3
#define FP_NORMAL 4

#define FP_INT_UPWARD 0
#define FP_INT_DOWNWARD 1
#define FP_INT_TOWARDZERO 2
#define FP_INT_TONEARESTFROMZERO 3
#define FP_INT_TONEAREST 4

#define MATH_ERRNO 1
#define MATH_ERREXCEPT 2

#define HUGE_VAL __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")

#define FP_ILOGB0 (-INT_MAX - 1)
#define FP_LLOGB0 (-LONG_MAX - 1)

#ifdef __FP_LOGBNAN_MIN
#define FP_ILOGBNAN (-INT_MAX - 1)
#define FP_LLOGBNAN (-LONG_MAX - 1)
#else
#define FP_ILOGBNAN INT_MAX
#define FP_LLOGBNAN LONG_MAX
#endif

// Math error handling. Target support is assumed to be existent unless
// explicitly disabled.
#if defined(__NVPTX__) || defined(__AMDGPU__) || defined(__SPIRV__) ||         \
    defined(__FAST_MATH__) || defined(__NO_MATH_ERRNO__)
#define __LIBC_SUPPORTS_MATH_ERRNO 0
#else
#define __LIBC_SUPPORTS_MATH_ERRNO 1
#endif

#if defined(__FAST_MATH__) ||                                                  \
    ((defined(__arm__) || defined(_M_ARM) || defined(__thumb__) ||             \
      defined(__aarch64__) || defined(_M_ARM64)) &&                            \
     !defined(__ARM_FP))
#define __LIBC_SUPPORTS_MATH_ERREXCEPT 0
#else
#define __LIBC_SUPPORTS_MATH_ERREXCEPT 1
#endif

#if __LIBC_SUPPORTS_MATH_ERRNO && __LIBC_SUPPORTS_MATH_ERREXCEPT
#define math_errhandling (MATH_ERRNO | MATH_ERREXCEPT)
#elif __LIBC_SUPPORTS_MATH_ERRNO
#define math_errhandling (MATH_ERRNO)
#elif __LIBC_SUPPORTS_MATH_ERREXCEPT
#define math_errhandling (MATH_ERREXCEPT)
#else
#define math_errhandling 0
#endif

#undef __LIBC_SUPPORTS_MATH_ERRNO
#undef __LIBC_SUPPORTS_MATH_ERREXCEPT

// POSIX math constants
// https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/math.h.html
#define M_E (__extension__ 0x1.5bf0a8b145769p1)
#define M_EGAMMA (__extension__ 0x1.2788cfc6fb619p-1)
#define M_LOG2E (__extension__ 0x1.71547652b82fep0)
#define M_LOG10E (__extension__ 0x1.bcb7b1526e50ep-2)
#define M_LN2 (__extension__ 0x1.62e42fefa39efp-1)
#define M_LN10 (__extension__ 0x1.26bb1bbb55516p1)
#define M_PHI (__extension__ 0x1.9e3779b97f4a8p0)
#define M_PI (__extension__ 0x1.921fb54442d18p1)
#define M_PI_2 (__extension__ 0x1.921fb54442d18p0)
#define M_PI_4 (__extension__ 0x1.921fb54442d18p-1)
#define M_1_PI (__extension__ 0x1.45f306dc9c883p-2)
#define M_1_SQRTPI (__extension__ 0x1.20dd750429b6dp-1)
#define M_2_PI (__extension__ 0x1.45f306dc9c883p-1)
#define M_2_SQRTPI (__extension__ 0x1.20dd750429b6dp0)
#define M_SQRT2 (__extension__ 0x1.6a09e667f3bcdp0)
#define M_SQRT3 (__extension__ 0x1.bb67ae8584caap0)
#define M_SQRT1_2 (__extension__ 0x1.6a09e667f3bcdp-1)
#define M_SQRT1_3 (__extension__ 0x1.279a74590331cp-1)

#define M_Ef (__extension__ 0x1.5bf0a8p1f)
#define M_EGAMMAf (__extension__ 0x1.2788dp-1f)
#define M_LOG2Ef (__extension__ 0x1.715476p0f)
#define M_LOG10Ef (__extension__ 0x1.bcb7b2p-2f)
#define M_LN2f (__extension__ 0x1.62e43p-1f)
#define M_LN10f (__extension__ 0x1.26bb1cp1f)
#define M_PHIf (__extension__ 0x1.9e377ap0f)
#define M_PIf (__extension__ 0x1.921fb6p1f)
#define M_PI_2f (__extension__ 0x1.921fb6p0f)
#define M_PI_4f (__extension__ 0x1.921fb6p-1f)
#define M_1_PIf (__extension__ 0x1.45f306p-2f)
#define M_1_SQRTPIf (__extension__ 0x1.20dd76p-1f)
#define M_2_PIf (__extension__ 0x1.45f306p-1f)
#define M_2_SQRTPIf (__extension__ 0x1.20dd76p0f)
#define M_SQRT2f (__extension__ 0x1.6a09e6p0f)
#define M_SQRT3f (__extension__ 0x1.bb67aep0f)
#define M_SQRT1_2f (__extension__ 0x1.6a09e6p-1f)
#define M_SQRT1_3f (__extension__ 0x1.279a74p-1f)

#define M_El (__extension__ 0x1.5bf0a8b1457695355fb8ac404e7ap1L)
#define M_EGAMMAl (__extension__ 0x1.2788cfc6fb618f49a37c7f0202a6p-1L)
#define M_LOG2El (__extension__ 0x1.71547652b82fe1777d0ffda0d23ap0L)
#define M_LOG10El (__extension__ 0x1.bcb7b1526e50e32a6ab7555f5a68p-2L)
#define M_LN2l (__extension__ 0x1.62e42fefa39ef35793c7673007e6p-1L)
#define M_LN10l (__extension__ 0x1.26bb1bbb5551582dd4adac5705a6p1L)
#define M_PHIl (__extension__ 0x1.9e3779b97f4a7c15f39cc0605ceep0L)
#define M_PIl (__extension__ 0x1.921fb54442d18469898cc51701b8p1L)
#define M_PI_2l (__extension__ 0x1.921fb54442d18469898cc51701b8p0L)
#define M_PI_4l (__extension__ 0x1.921fb54442d18469898cc51701b8p-1L)
#define M_1_PIl (__extension__ 0x1.45f306dc9c882a53f84eafa3ea6ap-2L)
#define M_1_SQRTPIl (__extension__ 0x1.20dd750429b6d11ae3a914fed7fep-1L)
#define M_2_PIl (__extension__ 0x1.45f306dc9c882a53f84eafa3ea6ap-1L)
#define M_2_SQRTPIl (__extension__ 0x1.20dd750429b6d11ae3a914fed7fep0L)
#define M_SQRT2l (__extension__ 0x1.6a09e667f3bcc908b2fb1366ea95p0L)
#define M_SQRT3l (__extension__ 0x1.bb67ae8584caa73b25742d7078b8p0L)
#define M_SQRT1_2l (__extension__ 0x1.6a09e667f3bcc908b2fb1366ea95p-1L)
#define M_SQRT1_3l (__extension__ 0x1.279a74590331c4d218f81e4afb25p-1L)

#ifdef __FLT16_MANT_DIG__
#define M_Ef16 (__extension__ 0x1.5cp1f16)
#define M_EGAMMAf16 (__extension__ 0x1.278p-1f16)
#define M_LOG2Ef16 (__extension__ 0x1.714f16)
#define M_LOG10Ef16 (__extension__ 0x1.bccp-2f16)
#define M_LN2f16 (__extension__ 0x1.63p-1f16)
#define M_LN10f16 (__extension__ 0x1.26cp1f16)
#define M_PHIf16 (__extension__ 0x1.9e4p0f16)
#define M_PIf16 (__extension__ 0x1.92p1f16)
#define M_PI_2f16 (__extension__ 0x1.92p0f16)
#define M_PI_4f16 (__extension__ 0x1.92p-1f16)
#define M_1_PIf16 (__extension__ 0x1.46p-2f16)
#define M_1_SQRTPIf16 (__extension__ 0x1.20cp-1f16)
#define M_2_PIf16 (__extension__ 0x1.46p-1f16)
#define M_2_SQRTPIf16 (__extension__ 0x1.20cp0f16)
#define M_SQRT2f16 (__extension__ 0x1.6ap0f16)
#define M_SQRT3f16 (__extension__ 0x1.bb8p0f16)
#define M_SQRT1_2f16 (__extension__ 0x1.6ap-1f16)
#define M_SQRT1_3f16 (__extension__ 0x1.278p-1f16)
#endif // __FLT16_MANT_DIG__

#ifdef __SIZEOF_FLOAT128__
#define M_Ef128 (__extension__ 0x1.5bf0a8b1457695355fb8ac404e7ap1q)
#define M_EGAMMAf128 (__extension__ 0x1.2788cfc6fb618f49a37c7f0202a6p-1q)
#define M_LOG2Ef128 (__extension__ 0x1.71547652b82fe1777d0ffda0d23ap0q)
#define M_LOG10Ef128 (__extension__ 0x1.bcb7b1526e50e32a6ab7555f5a68p-2q)
#define M_LN2f128 (__extension__ 0x1.62e42fefa39ef35793c7673007e6p-1q)
#define M_LN10f128 (__extension__ 0x1.26bb1bbb5551582dd4adac5705a6p1q)
#define M_PHIf128 (__extension__ 0x1.9e3779b97f4a7c15f39cc0605ceep0q)
#define M_PIf128 (__extension__ 0x1.921fb54442d18469898cc51701b8p1q)
#define M_PI_2f128 (__extension__ 0x1.921fb54442d18469898cc51701b8p0q)
#define M_PI_4f128 (__extension__ 0x1.921fb54442d18469898cc51701b8p-1q)
#define M_1_PIf128 (__extension__ 0x1.45f306dc9c882a53f84eafa3ea6ap-2q)
#define M_1_SQRTPIf128 (__extension__ 0x1.20dd750429b6d11ae3a914fed7fep-1q)
#define M_2_PIf128 (__extension__ 0x1.45f306dc9c882a53f84eafa3ea6ap-1q)
#define M_2_SQRTPIf128 (__extension__ 0x1.20dd750429b6d11ae3a914fed7fep0q)
#define M_SQRT2f128 (__extension__ 0x1.6a09e667f3bcc908b2fb1366ea95p0q)
#define M_SQRT3f128 (__extension__ 0x1.bb67ae8584caa73b25742d7078b8p0q)
#define M_SQRT1_2f128 (__extension__ 0x1.6a09e667f3bcc908b2fb1366ea95p-1q)
#define M_SQRT1_3f128 (__extension__ 0x1.279a74590331c4d218f81e4afb25p-1q)
#endif // __SIZEOF_FLOAT128__

#endif // LLVM_LIBC_MACROS_MATH_MACROS_H
