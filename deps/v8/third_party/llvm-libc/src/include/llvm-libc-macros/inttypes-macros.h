//===-- Definition of macros from inttypes.h ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_MACROS_INTTYPES_MACROS_H
#define LLVM_LIBC_MACROS_INTTYPES_MACROS_H

// fprintf/scanf format macros.
#define __STDC_VERSION_INTTYPES_H__ 202311L

// clang provides these macros, so we don't need to define them.
#ifndef __clang__
#if __UINTPTR_MAX__ == __UINT64_MAX__
#define __PRI64 "l"
#define __PRIPTR "l"
#elif __UINTPTR_MAX__ == __UINT32_MAX__
#define __PRI64 "ll"
#define __PRIPTR ""
#else
// CHERI achitecture for example, has 128-bit pointers that use special "P"
// format.
#error "Unsupported pointer format"
#endif
#define __INT8_FMTd__ "hhd"
#define __INT16_FMTd__ "hd"
#define __INT32_FMTd__ "d"
#define __INT64_FMTd__ __PRI64 "d"
#define __INT_LEAST8_FMTd__ "hhd"
#define __INT_LEAST16_FMTd__ "hd"
#define __INT_LEAST32_FMTd__ "d"
#define __INT_LEAST64_FMTd__ __PRI64 "d"
#define __INT_FAST8_FMTd__ "hhd"
#define __INT_FAST16_FMTd__ "hd"
#define __INT_FAST32_FMTd__ "d"
#define __INT_FAST64_FMTd__ __PRI64 "d"
#define __INTMAX_FMTd__ __PRI64 "d"
#define __INTPTR_FMTd__ __PRIPTR "d"

#define __INT8_FMTi__ "hhi"
#define __INT16_FMTi__ "hi"
#define __INT32_FMTi__ "i"
#define __INT64_FMTi__ __PRI64 "i"
#define __INT_LEAST8_FMTi__ "hhi"
#define __INT_LEAST16_FMTi__ "hi"
#define __INT_LEAST32_FMTi__ "i"
#define __INT_LEAST64_FMTi__ __PRI64 "i"
#define __INT_FAST8_FMTi__ "hhi"
#define __INT_FAST16_FMTi__ "hi"
#define __INT_FAST32_FMTi__ "i"
#define __INT_FAST64_FMTi__ __PRI64 "i"
#define __INTMAX_FMTi__ __PRI64 "i"
#define __INTPTR_FMTi__ __PRIPTR "i"

#define __UINT8_FMTo__ "hho"
#define __UINT16_FMTo__ "ho"
#define __UINT32_FMTo__ "o"
#define __UINT64_FMTo__ __PRI64 "o"
#define __UINT_LEAST8_FMTo__ "hho"
#define __UINT_LEAST16_FMTo__ "ho"
#define __UINT_LEAST32_FMTo__ "o"
#define __UINT_LEAST64_FMTo__ __PRI64 "o"
#define __UINT_FAST8_FMTo__ "hho"
#define __UINT_FAST16_FMTo__ "ho"
#define __UINT_FAST32_FMTo__ "o"
#define __UINT_FAST64_FMTo__ __PRI64 "o"
#define __UINTMAX_FMTo__ __PRI64 "o"
#define __UINTPTR_FMTo__ __PRIPTR "o"

#define __UINT8_FMTu__ "hhu"
#define __UINT16_FMTu__ "hu"
#define __UINT32_FMTu__ "u"
#define __UINT64_FMTu__ __PRI64 "u"
#define __UINT_LEAST8_FMTu__ "hhu"
#define __UINT_LEAST16_FMTu__ "hu"
#define __UINT_LEAST32_FMTu__ "u"
#define __UINT_LEAST64_FMTu__ __PRI64 "u"
#define __UINT_FAST8_FMTu__ "hhu"
#define __UINT_FAST16_FMTu__ "hu"
#define __UINT_FAST32_FMTu__ "u"
#define __UINT_FAST64_FMTu__ __PRI64 "u"
#define __UINTMAX_FMTu__ __PRI64 "u"
#define __UINTPTR_FMTu__ __PRIPTR "u"

#define __UINT8_FMTx__ "hhx"
#define __UINT16_FMTx__ "hx"
#define __UINT32_FMTx__ "x"
#define __UINT64_FMTx__ __PRI64 "x"
#define __UINT_LEAST8_FMTx__ "hhx"
#define __UINT_LEAST16_FMTx__ "hx"
#define __UINT_LEAST32_FMTx__ "x"
#define __UINT_LEAST64_FMTx__ __PRI64 "x"
#define __UINT_FAST8_FMTx__ "hhx"
#define __UINT_FAST16_FMTx__ "hx"
#define __UINT_FAST32_FMTx__ "x"
#define __UINT_FAST64_FMTx__ __PRI64 "x"
#define __UINTMAX_FMTx__ __PRI64 "x"
#define __UINTPTR_FMTx__ __PRIPTR "x"

#define __UINT8_FMTX__ "hhX"
#define __UINT16_FMTX__ "hX"
#define __UINT32_FMTX__ "X"
#define __UINT64_FMTX__ __PRI64 "X"
#define __UINT_LEAST8_FMTX__ "hhX"
#define __UINT_LEAST16_FMTX__ "hX"
#define __UINT_LEAST32_FMTX__ "X"
#define __UINT_LEAST64_FMTX__ __PRI64 "X"
#define __UINT_FAST8_FMTX__ "hhX"
#define __UINT_FAST16_FMTX__ "hX"
#define __UINT_FAST32_FMTX__ "X"
#define __UINT_FAST64_FMTX__ __PRI64 "X"
#define __UINTMAX_FMTX__ __PRI64 "X"
#define __UINTPTR_FMTX__ __PRIPTR "X"
#endif

// only recent clang provides these macros, so we need to check if they are
// available.
#ifndef __UINT8_FMTb__
#define __UINT8_FMTb__ "hhb"
#endif
#ifndef __UINT16_FMTb__
#define __UINT16_FMTb__ "hb"
#endif
#ifndef __UINT32_FMTb__
#define __UINT32_FMTb__ "b"
#endif
#ifndef __UINT64_FMTb__
#define __UINT64_FMTb__ __PRI64 "b"
#endif
#ifndef __UINT_LEAST8_FMTb__
#define __UINT_LEAST8_FMTb__ "hhb"
#endif
#ifndef __UINT_LEAST16_FMTb__
#define __UINT_LEAST16_FMTb__ "hb"
#endif
#ifndef __UINT_LEAST32_FMTb__
#define __UINT_LEAST32_FMTb__ "b"
#endif
#ifndef __UINT_LEAST64_FMTb__
#define __UINT_LEAST64_FMTb__ __PRI64 "b"
#endif
#ifndef __UINT_FAST8_FMTb__
#define __UINT_FAST8_FMTb__ "hhb"
#endif
#ifndef __UINT_FAST16_FMTb__
#define __UINT_FAST16_FMTb__ "hb"
#endif
#ifndef __UINT_FAST32_FMTb__
#define __UINT_FAST32_FMTb__ "b"
#endif
#ifndef __UINT_FAST64_FMTb__
#define __UINT_FAST64_FMTb__ __PRI64 "b"
#endif
#ifndef __UINTMAX_FMTb__
#define __UINTMAX_FMTb__ __PRI64 "b"
#endif
#ifndef __UINTPTR_FMTb__
#define __UINTPTR_FMTb__ __PRIPTR "b"
#endif

#ifndef __UINT8_FMTB__
#define __UINT8_FMTB__ "hhB"
#endif
#ifndef __UINT16_FMTB__
#define __UINT16_FMTB__ "hB"
#endif
#ifndef __UINT32_FMTB__
#define __UINT32_FMTB__ "B"
#endif
#ifndef __UINT64_FMTB__
#define __UINT64_FMTB__ __PRI64 "B"
#endif
#ifndef __UINT_LEAST8_FMTB__
#define __UINT_LEAST8_FMTB__ "hhB"
#endif
#ifndef __UINT_LEAST16_FMTB__
#define __UINT_LEAST16_FMTB__ "hB"
#endif
#ifndef __UINT_LEAST32_FMTB__
#define __UINT_LEAST32_FMTB__ "B"
#endif
#ifndef __UINT_LEAST64_FMTB__
#define __UINT_LEAST64_FMTB__ __PRI64 "B"
#endif
#ifndef __UINT_FAST8_FMTB__
#define __UINT_FAST8_FMTB__ "hhB"
#endif
#ifndef __UINT_FAST16_FMTB__
#define __UINT_FAST16_FMTB__ "hB"
#endif
#ifndef __UINT_FAST32_FMTB__
#define __UINT_FAST32_FMTB__ "B"
#endif
#ifndef __UINT_FAST64_FMTB__
#define __UINT_FAST64_FMTB__ __PRI64 "B"
#endif
#ifndef __UINTMAX_FMTB__
#define __UINTMAX_FMTB__ __PRI64 "B"
#endif
#ifndef __UINTPTR_FMTB__
#define __UINTPTR_FMTB__ __PRIPTR "B"
#endif

// The fprintf() macros for signed integers.
#define PRId8 __INT8_FMTd__
#define PRId16 __INT16_FMTd__
#define PRId32 __INT32_FMTd__
#define PRId64 __INT64_FMTd__
#define PRIdLEAST8 __INT_LEAST8_FMTd__
#define PRIdLEAST16 __INT_LEAST16_FMTd__
#define PRIdLEAST32 __INT_LEAST32_FMTd__
#define PRIdLEAST64 __INT_LEAST64_FMTd__
#define PRIdFAST8 __INT_FAST8_FMTd__
#define PRIdFAST16 __INT_FAST16_FMTd__
#define PRIdFAST32 __INT_FAST32_FMTd__
#define PRIdFAST64 __INT_FAST64_FMTd__
#define PRIdMAX __INTMAX_FMTd__
#define PRIdPTR __INTPTR_FMTd__

#define PRIi8 __INT8_FMTi__
#define PRIi16 __INT16_FMTi__
#define PRIi32 __INT32_FMTi__
#define PRIi64 __INT64_FMTi__
#define PRIiLEAST8 __INT_LEAST8_FMTi__
#define PRIiLEAST16 __INT_LEAST16_FMTi__
#define PRIiLEAST32 __INT_LEAST32_FMTi__
#define PRIiLEAST64 __INT_LEAST64_FMTi__
#define PRIiFAST8 __INT_FAST8_FMTi__
#define PRIiFAST16 __INT_FAST16_FMTi__
#define PRIiFAST32 __INT_FAST32_FMTi__
#define PRIiFAST64 __INT_FAST64_FMTi__
#define PRIiMAX __INTMAX_FMTi__
#define PRIiPTR __INTPTR_FMTi__

// The fprintf() macros for unsigned integers.
#define PRIo8 __UINT8_FMTo__
#define PRIo16 __UINT16_FMTo__
#define PRIo32 __UINT32_FMTo__
#define PRIo64 __UINT64_FMTo__
#define PRIoLEAST8 __UINT_LEAST8_FMTo__
#define PRIoLEAST16 __UINT_LEAST16_FMTo__
#define PRIoLEAST32 __UINT_LEAST32_FMTo__
#define PRIoLEAST64 __UINT_LEAST64_FMTo__
#define PRIoFAST8 __UINT_FAST8_FMTo__
#define PRIoFAST16 __UINT_FAST16_FMTo__
#define PRIoFAST32 __UINT_FAST32_FMTo__
#define PRIoFAST64 __UINT_FAST64_FMTo__
#define PRIoMAX __UINTMAX_FMTo__
#define PRIoPTR __UINTPTR_FMTo__

#define PRIu8 __UINT8_FMTu__
#define PRIu16 __UINT16_FMTu__
#define PRIu32 __UINT32_FMTu__
#define PRIu64 __UINT64_FMTu__
#define PRIuLEAST8 __UINT_LEAST8_FMTu__
#define PRIuLEAST16 __UINT_LEAST16_FMTu__
#define PRIuLEAST32 __UINT_LEAST32_FMTu__
#define PRIuLEAST64 __UINT_LEAST64_FMTu__
#define PRIuFAST8 __UINT_FAST8_FMTu__
#define PRIuFAST16 __UINT_FAST16_FMTu__
#define PRIuFAST32 __UINT_FAST32_FMTu__
#define PRIuFAST64 __UINT_FAST64_FMTu__
#define PRIuMAX __UINTMAX_FMTu__
#define PRIuPTR __UINTPTR_FMTu__

#define PRIx8 __UINT8_FMTx__
#define PRIx16 __UINT16_FMTx__
#define PRIx32 __UINT32_FMTx__
#define PRIx64 __UINT64_FMTx__
#define PRIxLEAST8 __UINT_LEAST8_FMTx__
#define PRIxLEAST16 __UINT_LEAST16_FMTx__
#define PRIxLEAST32 __UINT_LEAST32_FMTx__
#define PRIxLEAST64 __UINT_LEAST64_FMTx__
#define PRIxFAST8 __UINT_FAST8_FMTx__
#define PRIxFAST16 __UINT_FAST16_FMTx__
#define PRIxFAST32 __UINT_FAST32_FMTx__
#define PRIxFAST64 __UINT_FAST64_FMTx__
#define PRIxMAX __UINTMAX_FMTx__
#define PRIxPTR __UINTPTR_FMTx__

#define PRIX8 __UINT8_FMTX__
#define PRIX16 __UINT16_FMTX__
#define PRIX32 __UINT32_FMTX__
#define PRIX64 __UINT64_FMTX__
#define PRIXLEAST8 __UINT_LEAST8_FMTX__
#define PRIXLEAST16 __UINT_LEAST16_FMTX__
#define PRIXLEAST32 __UINT_LEAST32_FMTX__
#define PRIXLEAST64 __UINT_LEAST64_FMTX__
#define PRIXFAST8 __UINT_FAST8_FMTX__
#define PRIXFAST16 __UINT_FAST16_FMTX__
#define PRIXFAST32 __UINT_FAST32_FMTX__
#define PRIXFAST64 __UINT_FAST64_FMTX__
#define PRIXMAX __UINTMAX_FMTX__
#define PRIXPTR __UINTPTR_FMTX__

#define PRIb8 __UINT8_FMTb__
#define PRIb16 __UINT16_FMTb__
#define PRIb32 __UINT32_FMTb__
#define PRIb64 __UINT64_FMTb__
#define PRIbLEAST8 __UINT_LEAST8_FMTb__
#define PRIbLEAST16 __UINT_LEAST16_FMTb__
#define PRIbLEAST32 __UINT_LEAST32_FMTb__
#define PRIbLEAST64 __UINT_LEAST64_FMTb__
#define PRIbFAST8 __UINT_FAST8_FMTb__
#define PRIbFAST16 __UINT_FAST16_FMTb__
#define PRIbFAST32 __UINT_FAST32_FMTb__
#define PRIbFAST64 __UINT_FAST64_FMTb__
#define PRIbMAX __UINTMAX_FMTb__
#define PRIbPTR __UINTPTR_FMTb__

#define PRIB8 __UINT8_FMTB__
#define PRIB16 __UINT16_FMTB__
#define PRIB32 __UINT32_FMTB__
#define PRIB64 __UINT64_FMTB__
#define PRIBLEAST8 __UINT_LEAST8_FMTB__
#define PRIBLEAST16 __UINT_LEAST16_FMTB__
#define PRIBLEAST32 __UINT_LEAST32_FMTB__
#define PRIBLEAST64 __UINT_LEAST64_FMTB__
#define PRIBFAST8 __UINT_FAST8_FMTB__
#define PRIBFAST16 __UINT_FAST16_FMTB__
#define PRIBFAST32 __UINT_FAST32_FMTB__
#define PRIBFAST64 __UINT_FAST64_FMTB__
#define PRIBMAX __UINTMAX_FMTB__
#define PRIBPTR __UINTPTR_FMTB__

// The fscanf() macros for signed integers.
#define SCNd8 __INT8_FMTd__
#define SCNd16 __INT16_FMTd__
#define SCNd32 __INT32_FMTd__
#define SCNd64 __INT64_FMTd__
#define SCNdLEAST8 __INT_LEAST8_FMTd__
#define SCNdLEAST16 __INT_LEAST16_FMTd__
#define SCNdLEAST32 __INT_LEAST32_FMTd__
#define SCNdLEAST64 __INT_LEAST64_FMTd__
#define SCNdFAST8 __INT_FAST8_FMTd__
#define SCNdFAST16 __INT_FAST16_FMTd__
#define SCNdFAST32 __INT_FAST32_FMTd__
#define SCNdFAST64 __INT_FAST64_FMTd__
#define SCNdMAX __INTMAX_FMTd__
#define SCNdPTR __INTPTR_FMTd__

#define SCNi8 __INT8_FMTi__
#define SCNi16 __INT16_FMTi__
#define SCNi32 __INT32_FMTi__
#define SCNi64 __INT64_FMTi__
#define SCNiLEAST8 __INT_LEAST8_FMTi__
#define SCNiLEAST16 __INT_LEAST16_FMTi__
#define SCNiLEAST32 __INT_LEAST32_FMTi__
#define SCNiLEAST64 __INT_LEAST64_FMTi__
#define SCNiFAST8 __INT_FAST8_FMTi__
#define SCNiFAST16 __INT_FAST16_FMTi__
#define SCNiFAST32 __INT_FAST32_FMTi__
#define SCNiFAST64 __INT_FAST64_FMTi__
#define SCNiMAX __INTMAX_FMTi__
#define SCNiPTR __INTPTR_FMTi__

// The fscanf() macros for unsigned integers.
#define SCNo8 __UINT8_FMTo__
#define SCNo16 __UINT16_FMTo__
#define SCNo32 __UINT32_FMTo__
#define SCNo64 __UINT64_FMTo__
#define SCNoLEAST8 __UINT_LEAST8_FMTo__
#define SCNoLEAST16 __UINT_LEAST16_FMTo__
#define SCNoLEAST32 __UINT_LEAST32_FMTo__
#define SCNoLEAST64 __UINT_LEAST64_FMTo__
#define SCNoFAST8 __UINT_FAST8_FMTo__
#define SCNoFAST16 __UINT_FAST16_FMTo__
#define SCNoFAST32 __UINT_FAST32_FMTo__
#define SCNoFAST64 __UINT_FAST64_FMTo__
#define SCNoMAX __UINTMAX_FMTo__
#define SCNoPTR __UINTPTR_FMTo__

#define SCNu8 __UINT8_FMTu__
#define SCNu16 __UINT16_FMTu__
#define SCNu32 __UINT32_FMTu__
#define SCNu64 __UINT64_FMTu__
#define SCNuLEAST8 __UINT_LEAST8_FMTu__
#define SCNuLEAST16 __UINT_LEAST16_FMTu__
#define SCNuLEAST32 __UINT_LEAST32_FMTu__
#define SCNuLEAST64 __UINT_LEAST64_FMTu__
#define SCNuFAST8 __UINT_FAST8_FMTu__
#define SCNuFAST16 __UINT_FAST16_FMTu__
#define SCNuFAST32 __UINT_FAST32_FMTu__
#define SCNuFAST64 __UINT_FAST64_FMTu__
#define SCNuMAX __UINTMAX_FMTu__
#define SCNuPTR __UINTPTR_FMTu__

#define SCNx8 __UINT8_FMTx__
#define SCNx16 __UINT16_FMTx__
#define SCNx32 __UINT32_FMTx__
#define SCNx64 __UINT64_FMTx__
#define SCNxLEAST8 __UINT_LEAST8_FMTx__
#define SCNxLEAST16 __UINT_LEAST16_FMTx__
#define SCNxLEAST32 __UINT_LEAST32_FMTx__
#define SCNxLEAST64 __UINT_LEAST64_FMTx__
#define SCNxFAST8 __UINT_FAST8_FMTx__
#define SCNxFAST16 __UINT_FAST16_FMTx__
#define SCNxFAST32 __UINT_FAST32_FMTx__
#define SCNxFAST64 __UINT_FAST64_FMTx__
#define SCNxMAX __UINTMAX_FMTx__
#define SCNxPTR __UINTPTR_FMTx__

#define SCNb8 __UINT8_FMTb__
#define SCNb16 __UINT16_FMTb__
#define SCNb32 __UINT32_FMTb__
#define SCNb64 __UINT64_FMTb__
#define SCNbLEAST8 __UINT_LEAST8_FMTb__
#define SCNbLEAST16 __UINT_LEAST16_FMTb__
#define SCNbLEAST32 __UINT_LEAST32_FMTb__
#define SCNbLEAST64 __UINT_LEAST64_FMTb__
#define SCNbFAST8 __UINT_FAST8_FMTb__
#define SCNbFAST16 __UINT_FAST16_FMTb__
#define SCNbFAST32 __UINT_FAST32_FMTb__
#define SCNbFAST64 __UINT_FAST64_FMTb__
#define SCNbMAX __UINTMAX_FMTb__
#define SCNbPTR __UINTPTR_FMTb__

#endif // LLVM_LIBC_MACROS_INTTYPES_MACROS_H
