/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef MEM_H_MODULE
#define MEM_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif

/*-****************************************
*  Dependencies
******************************************/
#include <stddef.h>  /* size_t, ptrdiff_t */
#include "compiler.h"  /* __has_builtin */
#include "debug.h"  /* DEBUG_STATIC_ASSERT */
#include "zstd_deps.h"  /* ZSTD_memcpy */


/*-****************************************
*  Compiler specifics
******************************************/
#if defined(_MSC_VER)   /* Visual Studio */
#   include <stdlib.h>  /* _byteswap_ulong */
#   include <intrin.h>  /* _byteswap_* */
#endif

/*-**************************************************************
*  Basic Types
*****************************************************************/
#if  !defined (__VMS) && (defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */) )
#  if defined(_AIX)
#    include <inttypes.h>
#  else
#    include <stdint.h> /* intptr_t */
#  endif
  typedef   uint8_t BYTE;
  typedef   uint8_t U8;
  typedef    int8_t S8;
  typedef  uint16_t U16;
  typedef   int16_t S16;
  typedef  uint32_t U32;
  typedef   int32_t S32;
  typedef  uint64_t U64;
  typedef   int64_t S64;
#else
# include <limits.h>
#if CHAR_BIT != 8
#  error "this implementation requires char to be exactly 8-bit type"
#endif
  typedef unsigned char      BYTE;
  typedef unsigned char      U8;
  typedef   signed char      S8;
#if USHRT_MAX != 65535
#  error "this implementation requires short to be exactly 16-bit type"
#endif
  typedef unsigned short      U16;
  typedef   signed short      S16;
#if UINT_MAX != 4294967295
#  error "this implementation requires int to be exactly 32-bit type"
#endif
  typedef unsigned int        U32;
  typedef   signed int        S32;
/* note : there are no limits defined for long long type in C90.
 * limits exist in C99, however, in such case, <stdint.h> is preferred */
  typedef unsigned long long  U64;
  typedef   signed long long  S64;
#endif


/*-**************************************************************
*  Memory I/O API
*****************************************************************/
/*=== Static platform detection ===*/
MEM_STATIC unsigned MEM_32bits(void);
MEM_STATIC unsigned MEM_64bits(void);
MEM_STATIC unsigned MEM_isLittleEndian(void);

/*=== Native unaligned read/write ===*/
MEM_STATIC U16 MEM_read16(const void* memPtr);
MEM_STATIC U32 MEM_read32(const void* memPtr);
MEM_STATIC U64 MEM_read64(const void* memPtr);
MEM_STATIC size_t MEM_readST(const void* memPtr);

MEM_STATIC void MEM_write16(void* memPtr, U16 value);
MEM_STATIC void MEM_write32(void* memPtr, U32 value);
MEM_STATIC void MEM_write64(void* memPtr, U64 value);

/*=== Little endian unaligned read/write ===*/
MEM_STATIC U16 MEM_readLE16(const void* memPtr);
MEM_STATIC U32 MEM_readLE24(const void* memPtr);
MEM_STATIC U32 MEM_readLE32(const void* memPtr);
MEM_STATIC U64 MEM_readLE64(const void* memPtr);
MEM_STATIC size_t MEM_readLEST(const void* memPtr);

MEM_STATIC void MEM_writeLE16(void* memPtr, U16 val);
MEM_STATIC void MEM_writeLE24(void* memPtr, U32 val);
MEM_STATIC void MEM_writeLE32(void* memPtr, U32 val32);
MEM_STATIC void MEM_writeLE64(void* memPtr, U64 val64);
MEM_STATIC void MEM_writeLEST(void* memPtr, size_t val);

/*=== Big endian unaligned read/write ===*/
MEM_STATIC U32 MEM_readBE32(const void* memPtr);
MEM_STATIC U64 MEM_readBE64(const void* memPtr);
MEM_STATIC size_t MEM_readBEST(const void* memPtr);

MEM_STATIC void MEM_writeBE32(void* memPtr, U32 val32);
MEM_STATIC void MEM_writeBE64(void* memPtr, U64 val64);
MEM_STATIC void MEM_writeBEST(void* memPtr, size_t val);

/*=== Byteswap ===*/
MEM_STATIC U32 MEM_swap32(U32 in);
MEM_STATIC U64 MEM_swap64(U64 in);
MEM_STATIC size_t MEM_swapST(size_t in);


/*-**************************************************************
*  Memory I/O Implementation
*****************************************************************/
/* MEM_FORCE_MEMORY_ACCESS : For accessing unaligned memory:
 * Method 0 : always use `memcpy()`. Safe and portable.
 * Method 1 : Use compiler extension to set unaligned access.
 * Method 2 : direct access. This method is portable but violate C standard.
 *            It can generate buggy code on targets depending on alignment.
 * Default  : method 1 if supported, else method 0
 */
#ifndef MEM_FORCE_MEMORY_ACCESS   /* can be defined externally, on command line for example */
#  ifdef __GNUC__
#    define MEM_FORCE_MEMORY_ACCESS 1
#  endif
#endif

MEM_STATIC unsigned MEM_32bits(void) { return sizeof(size_t)==4; }
MEM_STATIC unsigned MEM_64bits(void) { return sizeof(size_t)==8; }

MEM_STATIC unsigned MEM_isLittleEndian(void)
{
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    return 1;
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return 0;
#elif defined(__clang__) && __LITTLE_ENDIAN__
    return 1;
#elif defined(__clang__) && __BIG_ENDIAN__
    return 0;
#elif defined(_MSC_VER) && (_M_AMD64 || _M_IX86)
    return 1;
#elif defined(__DMC__) && defined(_M_IX86)
    return 1;
#else
    const union { U32 u; BYTE c[4]; } one = { 1 };   /* don't use static : performance detrimental  */
    return one.c[0];
#endif
}

#if defined(MEM_FORCE_MEMORY_ACCESS) && (MEM_FORCE_MEMORY_ACCESS==2)

/* violates C standard, by lying on structure alignment.
Only use if no other choice to achieve best performance on target platform */
MEM_STATIC U16 MEM_read16(const void* memPtr) { return *(const U16*) memPtr; }
MEM_STATIC U32 MEM_read32(const void* memPtr) { return *(const U32*) memPtr; }
MEM_STATIC U64 MEM_read64(const void* memPtr) { return *(const U64*) memPtr; }
MEM_STATIC size_t MEM_readST(const void* memPtr) { return *(const size_t*) memPtr; }

MEM_STATIC void MEM_write16(void* memPtr, U16 value) { *(U16*)memPtr = value; }
MEM_STATIC void MEM_write32(void* memPtr, U32 value) { *(U32*)memPtr = value; }
MEM_STATIC void MEM_write64(void* memPtr, U64 value) { *(U64*)memPtr = value; }

#elif defined(MEM_FORCE_MEMORY_ACCESS) && (MEM_FORCE_MEMORY_ACCESS==1)

typedef __attribute__((aligned(1))) U16 unalign16;
typedef __attribute__((aligned(1))) U32 unalign32;
typedef __attribute__((aligned(1))) U64 unalign64;
typedef __attribute__((aligned(1))) size_t unalignArch;

MEM_STATIC U16 MEM_read16(const void* ptr) { return *(const unalign16*)ptr; }
MEM_STATIC U32 MEM_read32(const void* ptr) { return *(const unalign32*)ptr; }
MEM_STATIC U64 MEM_read64(const void* ptr) { return *(const unalign64*)ptr; }
MEM_STATIC size_t MEM_readST(const void* ptr) { return *(const unalignArch*)ptr; }

MEM_STATIC void MEM_write16(void* memPtr, U16 value) { *(unalign16*)memPtr = value; }
MEM_STATIC void MEM_write32(void* memPtr, U32 value) { *(unalign32*)memPtr = value; }
MEM_STATIC void MEM_write64(void* memPtr, U64 value) { *(unalign64*)memPtr = value; }

#else

/* default method, safe and standard.
   can sometimes prove slower */

MEM_STATIC U16 MEM_read16(const void* memPtr)
{
    U16 val; ZSTD_memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC U32 MEM_read32(const void* memPtr)
{
    U32 val; ZSTD_memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC U64 MEM_read64(const void* memPtr)
{
    U64 val; ZSTD_memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC size_t MEM_readST(const void* memPtr)
{
    size_t val; ZSTD_memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC void MEM_write16(void* memPtr, U16 value)
{
    ZSTD_memcpy(memPtr, &value, sizeof(value));
}

MEM_STATIC void MEM_write32(void* memPtr, U32 value)
{
    ZSTD_memcpy(memPtr, &value, sizeof(value));
}

MEM_STATIC void MEM_write64(void* memPtr, U64 value)
{
    ZSTD_memcpy(memPtr, &value, sizeof(value));
}

#endif /* MEM_FORCE_MEMORY_ACCESS */

MEM_STATIC U32 MEM_swap32_fallback(U32 in)
{
    return  ((in << 24) & 0xff000000 ) |
            ((in <<  8) & 0x00ff0000 ) |
            ((in >>  8) & 0x0000ff00 ) |
            ((in >> 24) & 0x000000ff );
}

MEM_STATIC U32 MEM_swap32(U32 in)
{
#if defined(_MSC_VER)     /* Visual Studio */
    return _byteswap_ulong(in);
#elif (defined (__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 403)) \
  || (defined(__clang__) && __has_builtin(__builtin_bswap32))
    return __builtin_bswap32(in);
#else
    return MEM_swap32_fallback(in);
#endif
}

MEM_STATIC U64 MEM_swap64_fallback(U64 in)
{
     return  ((in << 56) & 0xff00000000000000ULL) |
            ((in << 40) & 0x00ff000000000000ULL) |
            ((in << 24) & 0x0000ff0000000000ULL) |
            ((in << 8)  & 0x000000ff00000000ULL) |
            ((in >> 8)  & 0x00000000ff000000ULL) |
            ((in >> 24) & 0x0000000000ff0000ULL) |
            ((in >> 40) & 0x000000000000ff00ULL) |
            ((in >> 56) & 0x00000000000000ffULL);
}

MEM_STATIC U64 MEM_swap64(U64 in)
{
#if defined(_MSC_VER)     /* Visual Studio */
    return _byteswap_uint64(in);
#elif (defined (__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 403)) \
  || (defined(__clang__) && __has_builtin(__builtin_bswap64))
    return __builtin_bswap64(in);
#else
    return MEM_swap64_fallback(in);
#endif
}

MEM_STATIC size_t MEM_swapST(size_t in)
{
    if (MEM_32bits())
        return (size_t)MEM_swap32((U32)in);
    else
        return (size_t)MEM_swap64((U64)in);
}

/*=== Little endian r/w ===*/

MEM_STATIC U16 MEM_readLE16(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read16(memPtr);
    else {
        const BYTE* p = (const BYTE*)memPtr;
        return (U16)(p[0] + (p[1]<<8));
    }
}

MEM_STATIC void MEM_writeLE16(void* memPtr, U16 val)
{
    if (MEM_isLittleEndian()) {
        MEM_write16(memPtr, val);
    } else {
        BYTE* p = (BYTE*)memPtr;
        p[0] = (BYTE)val;
        p[1] = (BYTE)(val>>8);
    }
}

MEM_STATIC U32 MEM_readLE24(const void* memPtr)
{
    return (U32)MEM_readLE16(memPtr) + ((U32)(((const BYTE*)memPtr)[2]) << 16);
}

MEM_STATIC void MEM_writeLE24(void* memPtr, U32 val)
{
    MEM_writeLE16(memPtr, (U16)val);
    ((BYTE*)memPtr)[2] = (BYTE)(val>>16);
}

MEM_STATIC U32 MEM_readLE32(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read32(memPtr);
    else
        return MEM_swap32(MEM_read32(memPtr));
}

MEM_STATIC void MEM_writeLE32(void* memPtr, U32 val32)
{
    if (MEM_isLittleEndian())
        MEM_write32(memPtr, val32);
    else
        MEM_write32(memPtr, MEM_swap32(val32));
}

MEM_STATIC U64 MEM_readLE64(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read64(memPtr);
    else
        return MEM_swap64(MEM_read64(memPtr));
}

MEM_STATIC void MEM_writeLE64(void* memPtr, U64 val64)
{
    if (MEM_isLittleEndian())
        MEM_write64(memPtr, val64);
    else
        MEM_write64(memPtr, MEM_swap64(val64));
}

MEM_STATIC size_t MEM_readLEST(const void* memPtr)
{
    if (MEM_32bits())
        return (size_t)MEM_readLE32(memPtr);
    else
        return (size_t)MEM_readLE64(memPtr);
}

MEM_STATIC void MEM_writeLEST(void* memPtr, size_t val)
{
    if (MEM_32bits())
        MEM_writeLE32(memPtr, (U32)val);
    else
        MEM_writeLE64(memPtr, (U64)val);
}

/*=== Big endian r/w ===*/

MEM_STATIC U32 MEM_readBE32(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_swap32(MEM_read32(memPtr));
    else
        return MEM_read32(memPtr);
}

MEM_STATIC void MEM_writeBE32(void* memPtr, U32 val32)
{
    if (MEM_isLittleEndian())
        MEM_write32(memPtr, MEM_swap32(val32));
    else
        MEM_write32(memPtr, val32);
}

MEM_STATIC U64 MEM_readBE64(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_swap64(MEM_read64(memPtr));
    else
        return MEM_read64(memPtr);
}

MEM_STATIC void MEM_writeBE64(void* memPtr, U64 val64)
{
    if (MEM_isLittleEndian())
        MEM_write64(memPtr, MEM_swap64(val64));
    else
        MEM_write64(memPtr, val64);
}

MEM_STATIC size_t MEM_readBEST(const void* memPtr)
{
    if (MEM_32bits())
        return (size_t)MEM_readBE32(memPtr);
    else
        return (size_t)MEM_readBE64(memPtr);
}

MEM_STATIC void MEM_writeBEST(void* memPtr, size_t val)
{
    if (MEM_32bits())
        MEM_writeBE32(memPtr, (U32)val);
    else
        MEM_writeBE64(memPtr, (U64)val);
}

/* code only tested on 32 and 64 bits systems */
MEM_STATIC void MEM_check(void) { DEBUG_STATIC_ASSERT((sizeof(size_t)==4) || (sizeof(size_t)==8)); }


#if defined (__cplusplus)
}
#endif

#endif /* MEM_H_MODULE */
