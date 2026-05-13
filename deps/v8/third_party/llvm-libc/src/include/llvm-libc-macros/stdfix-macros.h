//===-- Definitions from stdfix.h -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_STDFIX_MACROS_H
#define LLVM_LIBC_MACROS_STDFIX_MACROS_H

#ifdef __FRACT_FBIT__
// _Fract and _Accum types are available
#define LIBC_COMPILER_HAS_FIXED_POINT
#endif // __FRACT_FBIT__

#ifdef LIBC_COMPILER_HAS_FIXED_POINT

#define fract _Fract
#define accum _Accum
#define sat _Sat

// Default values: from ISO/IEC TR 18037:2008 standard - Annex A.3 - Typical
// desktop processor.

#ifdef __SFRACT_FBIT__
#define SFRACT_FBIT __SFRACT_FBIT__
#else
#define SFRACT_FBIT 7
#endif // SFRACT_FBIT

#ifdef __SFRACT_MIN__
#define SFRACT_MIN __SFRACT_MIN__
#else
#define SFRACT_MIN (-0.5HR - 0.5HR)
#endif // SFRACT_MIN

#ifdef __SFRACT_MAX__
#define SFRACT_MAX __SFRACT_MAX__
#else
#define SFRACT_MAX 0x1.FCp-1HR
#endif // SFRACT_MAX

#ifdef __SFRACT_EPSILON__
#define SFRACT_EPSILON __SFRACT_EPSILON__
#else
#define SFRACT_EPSILON 0x1.0p-7HR
#endif // SFRACT_EPSILON

#ifdef __USFRACT_FBIT__
#define USFRACT_FBIT __USFRACT_FBIT__
#else
#define USFRACT_FBIT 8
#endif // USFRACT_FBIT

#define USFRACT_MIN 0.0UHR

#ifdef __USFRACT_MAX__
#define USFRACT_MAX __USFRACT_MAX__
#else
#define USFRACT_MAX 0x1.FEp-1UHR
#endif // USFRACT_MAX

#ifdef __USFRACT_EPSILON__
#define USFRACT_EPSILON __USFRACT_EPSILON__
#else
#define USFRACT_EPSILON 0x1.0p-8UHR
#endif // USFRACT_EPSILON

#ifdef __FRACT_FBIT__
#define FRACT_FBIT __FRACT_FBIT__
#else
#define FRACT_FBIT 15
#endif // FRACT_FBIT

#ifdef __FRACT_MIN__
#define FRACT_MIN __FRACT_MIN__
#else
#define FRACT_MIN (-0.5R - 0.5R)
#endif // FRACT_MIN

#ifdef __FRACT_MAX__
#define FRACT_MAX __FRACT_MAX__
#else
#define FRACT_MAX 0x1.FFFCp-1R
#endif // FRACT_MAX

#ifdef __FRACT_EPSILON__
#define FRACT_EPSILON __FRACT_EPSILON__
#else
#define FRACT_EPSILON 0x1.0p-15R
#endif // FRACT_EPSILON

#ifdef __UFRACT_FBIT__
#define UFRACT_FBIT __UFRACT_FBIT__
#else
#define UFRACT_FBIT 16
#endif // UFRACT_FBIT

#define UFRACT_MIN 0.0UR

#ifdef __UFRACT_MAX__
#define UFRACT_MAX __UFRACT_MAX__
#else
#define UFRACT_MAX 0x1.FFFEp-1UR
#endif // UFRACT_MAX

#ifdef __UFRACT_EPSILON__
#define UFRACT_EPSILON __UFRACT_EPSILON__
#else
#define UFRACT_EPSILON 0x1.0p-16UR
#endif // UFRACT_EPSILON

#ifdef __LFRACT_FBIT__
#define LFRACT_FBIT __LFRACT_FBIT__
#else
#define LFRACT_FBIT 31
#endif // LFRACT_FBIT

#ifdef __LFRACT_MIN__
#define LFRACT_MIN __LFRACT_MIN__
#else
#define LFRACT_MIN (-0.5LR - 0.5LR)
#endif // LFRACT_MIN

#ifdef __LFRACT_MAX__
#define LFRACT_MAX __LFRACT_MAX__
#else
#define LFRACT_MAX 0x1.FFFFFFFCp-1LR
#endif // LFRACT_MAX

#ifdef __LFRACT_EPSILON__
#define LFRACT_EPSILON __LFRACT_EPSILON__
#else
#define LFRACT_EPSILON 0x1.0p-31LR
#endif // LFRACT_EPSILON

#ifdef __ULFRACT_FBIT__
#define ULFRACT_FBIT __ULFRACT_FBIT__
#else
#define ULFRACT_FBIT 32
#endif // ULFRACT_FBIT

#define ULFRACT_MIN 0.0ULR

#ifdef __ULFRACT_MAX__
#define ULFRACT_MAX __ULFRACT_MAX__
#else
#define ULFRACT_MAX 0x1.FFFFFFFEp-1ULR
#endif // ULFRACT_MAX

#ifdef __ULFRACT_EPSILON__
#define ULFRACT_EPSILON __ULFRACT_EPSILON__
#else
#define ULFRACT_EPSILON 0x1.0p-32ULR
#endif // ULFRACT_EPSILON

#ifdef __SACCUM_FBIT__
#define SACCUM_FBIT __SACCUM_FBIT__
#else
#define SACCUM_FBIT 7
#endif // SACCUM_FBIT

#ifdef __SACCUM_IBIT__
#define SACCUM_IBIT __SACCUM_IBIT__
#else
#define SACCUM_IBIT 8
#endif // SACCUM_IBIT

#ifdef __SACCUM_MIN__
#define SACCUM_MIN __SACCUM_MIN__
#else
#define SACCUM_MIN (-0x1.0p+7HK - 0x1.0p+7HK)
#endif // SACCUM_MIN

#ifdef __SACCUM_MAX__
#define SACCUM_MAX __SACCUM_MAX__
#else
#define SACCUM_MAX 0x1.FFFCp+7HK
#endif // SACCUM_MAX

#ifdef __SACCUM_EPSILON__
#define SACCUM_EPSILON __SACCUM_EPSILON__
#else
#define SACCUM_EPSILON 0x1.0p-7HK
#endif // SACCUM_EPSILON

#ifdef __USACCUM_FBIT__
#define USACCUM_FBIT __USACCUM_FBIT__
#else
#define USACCUM_FBIT 8
#endif // USACCUM_FBIT

#ifdef __USACCUM_IBIT__
#define USACCUM_IBIT __USACCUM_IBIT__
#else
#define USACCUM_IBIT 8
#endif // USACCUM_IBIT

#define USACCUM_MIN 0.0UHK

#ifdef __USACCUM_MAX__
#define USACCUM_MAX __USACCUM_MAX__
#else
#define USACCUM_MAX 0x1.FFFEp+7UHK
#endif // USACCUM_MAX

#ifdef __USACCUM_EPSILON__
#define USACCUM_EPSILON __USACCUM_EPSILON__
#else
#define USACCUM_EPSILON 0x1.0p-8UHK
#endif // USACCUM_EPSILON

#ifdef __ACCUM_FBIT__
#define ACCUM_FBIT __ACCUM_FBIT__
#else
#define ACCUM_FBIT 15
#endif // ACCUM_FBIT

#ifdef __ACCUM_IBIT__
#define ACCUM_IBIT __ACCUM_IBIT__
#else
#define ACCUM_IBIT 16
#endif // ACCUM_IBIT

#ifdef __ACCUM_MIN__
#define ACCUM_MIN __ACCUM_MIN__
#else
#define ACCUM_MIN (-0x1.0p+15K - 0x1.0p+15K)
#endif // ACCUM_MIN

#ifdef __ACCUM_MAX__
#define ACCUM_MAX __ACCUM_MAX__
#else
#define ACCUM_MAX 0x1.FFFFFFFCp+15K
#endif // ACCUM_MAX

#ifdef __ACCUM_EPSILON__
#define ACCUM_EPSILON __ACCUM_EPSILON__
#else
#define ACCUM_EPSILON 0x1.0p-15K
#endif // ACCUM_EPSILON

#ifdef __UACCUM_FBIT__
#define UACCUM_FBIT __UACCUM_FBIT__
#else
#define UACCUM_FBIT 16
#endif // UACCUM_FBIT

#ifdef __UACCUM_IBIT__
#define UACCUM_IBIT __UACCUM_IBIT__
#else
#define UACCUM_IBIT 16
#endif // UACCUM_IBIT

#define UACCUM_MIN 0.0UK

#ifdef __UACCUM_MAX__
#define UACCUM_MAX __UACCUM_MAX__
#else
#define UACCUM_MAX 0x1.FFFFFFFEp+15UK
#endif // UACCUM_MAX

#ifdef __UACCUM_EPSILON__
#define UACCUM_EPSILON __UACCUM_EPSILON__
#else
#define UACCUM_EPSILON 0x1.0p-16UK
#endif // UACCUM_EPSILON

#ifdef __LACCUM_FBIT__
#define LACCUM_FBIT __LACCUM_FBIT__
#else
#define LACCUM_FBIT 31
#endif // LACCUM_FBIT

#ifdef __LACCUM_IBIT__
#define LACCUM_IBIT __LACCUM_IBIT__
#else
#define LACCUM_IBIT 32
#endif // LACCUM_IBIT

#ifdef __LACCUM_MIN__
#define LACCUM_MIN __LACCUM_MIN__
#else
#define LACCUM_MIN (-0x1.0p+31LK - 0x1.0p+31LK)
#endif // LACCUM_MIN

#ifdef __LACCUM_MAX__
#define LACCUM_MAX __LACCUM_MAX__
#else
#define LACCUM_MAX 0x1.FFFFFFFFFFFFFFFCp+31LK
#endif // LACCUM_MAX

#ifdef __LACCUM_EPSILON__
#define LACCUM_EPSILON __LACCUM_EPSILON__
#else
#define LACCUM_EPSILON 0x1.0p-31LK
#endif // LACCUM_EPSILON

#ifdef __ULACCUM_FBIT__
#define ULACCUM_FBIT __ULACCUM_FBIT__
#else
#define ULACCUM_FBIT 32
#endif // ULACCUM_FBIT

#ifdef __ULACCUM_IBIT__
#define ULACCUM_IBIT __ULACCUM_IBIT__
#else
#define ULACCUM_IBIT 32
#endif // ULACCUM_IBIT

#define ULACCUM_MIN 0.0ULK

#ifdef __ULACCUM_MAX__
#define ULACCUM_MAX __ULACCUM_MAX__
#else
#define ULACCUM_MAX 0x1.FFFFFFFFFFFFFFFEp+31ULK
#endif // ULACCUM_MAX

#ifdef __ULACCUM_EPSILON__
#define ULACCUM_EPSILON __ULACCUM_EPSILON__
#else
#define ULACCUM_EPSILON 0x1.0p-32ULK
#endif // ULACCUM_EPSILON

#define absfx(x)                                                               \
  _Generic((x),                                                                \
      fract: absr,                                                             \
      short fract: abshr,                                                      \
      long fract: abslr,                                                       \
      accum: absk,                                                             \
      short accum: abshk,                                                      \
      long accum: abslk)(x)

#define countlsfx(x)                                                           \
  _Generic((x),                                                                \
      fract: countlsr,                                                         \
      short fract: countlshr,                                                  \
      long fract: countlslr,                                                   \
      accum: countlsk,                                                         \
      short accum: countlshk,                                                  \
      long accum: countlslk,                                                   \
      unsigned fract: countlsur,                                               \
      unsigned short fract: countlsuhr,                                        \
      unsigned long fract: countlsulr,                                         \
      unsigned accum: countlsuk,                                               \
      unsigned short accum: countlsuhk,                                        \
      unsigned long accum: countlsulk)(x)

#define roundfx(x, y)                                                          \
  _Generic((x),                                                                \
      fract: roundr,                                                           \
      short fract: roundhr,                                                    \
      long fract: roundlr,                                                     \
      accum: roundk,                                                           \
      short accum: roundhk,                                                    \
      long accum: roundlk,                                                     \
      unsigned fract: roundur,                                                 \
      unsigned short fract: rounduhr,                                          \
      unsigned long fract: roundulr,                                           \
      unsigned accum: rounduk,                                                 \
      unsigned short accum: rounduhk,                                          \
      unsigned long accum: roundulk)(x, y)

#endif // LIBC_COMPILER_HAS_FIXED_POINT

#endif // LLVM_LIBC_MACROS_STDFIX_MACROS_H
