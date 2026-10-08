/* Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
   Licensed under the Apache 2.0 License. */

#ifndef KRML_TYPES_H
#define KRML_TYPES_H

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/* Types which are either abstract, meaning that have to be implemented in C, or
 * which are models, meaning that they are swapped out at compile-time for
 * hand-written C types (in which case they're marked as noextract). */

typedef uint64_t FStar_UInt64_t, FStar_UInt64_t_;
typedef int64_t FStar_Int64_t, FStar_Int64_t_;
typedef uint32_t FStar_UInt32_t, FStar_UInt32_t_;
typedef int32_t FStar_Int32_t, FStar_Int32_t_;
typedef uint16_t FStar_UInt16_t, FStar_UInt16_t_;
typedef int16_t FStar_Int16_t, FStar_Int16_t_;
typedef uint8_t FStar_UInt8_t, FStar_UInt8_t_;
typedef int8_t FStar_Int8_t, FStar_Int8_t_;

/* Only useful when building Kremlib, because it's in the dependency graph of
 * FStar.Int.Cast. */
typedef uint64_t FStar_UInt63_t, FStar_UInt63_t_;
typedef int64_t FStar_Int63_t, FStar_Int63_t_;

typedef double FStar_Float_float;
typedef uint32_t FStar_Char_char;
typedef FILE *FStar_IO_fd_read, *FStar_IO_fd_write;

typedef void *FStar_Dyn_dyn;

typedef const char *C_String_t, *C_String_t_;

typedef int exit_code;
typedef FILE *channel;

typedef unsigned long long TestLib_cycles;

typedef uint64_t FStar_Date_dateTime, FStar_Date_timeSpan;

/* The uint128 type is a special case since we offer several implementations of
 * it, depending on the compiler and whether the user wants the verified
 * implementation or not. */
#if !defined(KRML_VERIFIED_UINT128) && defined(_MSC_VER) && defined(_M_X64)
#  include <emmintrin.h>
typedef __m128i FStar_UInt128_uint128;
#elif !defined(KRML_VERIFIED_UINT128) && !defined(_MSC_VER)
typedef unsigned __int128 FStar_UInt128_uint128;
#else
typedef struct FStar_UInt128_uint128_s {
  uint64_t low;
  uint64_t high;
} FStar_UInt128_uint128;
#endif

typedef FStar_UInt128_uint128 FStar_UInt128_t, FStar_UInt128_t_, uint128_t;

#endif
