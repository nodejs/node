/*
 * Copyright (c) Yann Collet, Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


 /******************************************
 *  Includes
 ******************************************/
#include <stddef.h>    /* size_t, ptrdiff_t */
#include <string.h>    /* memcpy */

#include "zstd_v04.h"
#include "../common/compiler.h"
#include "../common/error_private.h"


/* ******************************************************************
 *   mem.h
 *******************************************************************/
#ifndef MEM_H_MODULE
#define MEM_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif


/******************************************
*  Compiler-specific
******************************************/
#if defined(_MSC_VER)   /* Visual Studio */
#   include <stdlib.h>  /* _byteswap_ulong */
#   include <intrin.h>  /* _byteswap_* */
#endif


/****************************************************************
*  Basic Types
*****************************************************************/
#if defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
# if defined(_AIX)
#  include <inttypes.h>
# else
#  include <stdint.h> /* intptr_t */
# endif
  typedef  uint8_t BYTE;
  typedef uint16_t U16;
  typedef  int16_t S16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
  typedef  int64_t S64;
#else
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef   signed short      S16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
  typedef   signed long long  S64;
#endif


/*-*************************************
*  Debug
***************************************/
#include "../common/debug.h"
#ifndef assert
#  define assert(condition) ((void)0)
#endif


/****************************************************************
*  Memory I/O
*****************************************************************/

MEM_STATIC unsigned MEM_32bits(void) { return sizeof(void*)==4; }
MEM_STATIC unsigned MEM_64bits(void) { return sizeof(void*)==8; }

MEM_STATIC unsigned MEM_isLittleEndian(void)
{
    const union { U32 u; BYTE c[4]; } one = { 1 };   /* don't use static : performance detrimental  */
    return one.c[0];
}

MEM_STATIC U16 MEM_read16(const void* memPtr)
{
    U16 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC U32 MEM_read32(const void* memPtr)
{
    U32 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC U64 MEM_read64(const void* memPtr)
{
    U64 val; memcpy(&val, memPtr, sizeof(val)); return val;
}

MEM_STATIC void MEM_write16(void* memPtr, U16 value)
{
    memcpy(memPtr, &value, sizeof(value));
}

MEM_STATIC U16 MEM_readLE16(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read16(memPtr);
    else
    {
        const BYTE* p = (const BYTE*)memPtr;
        return (U16)(p[0] + (p[1]<<8));
    }
}

MEM_STATIC void MEM_writeLE16(void* memPtr, U16 val)
{
    if (MEM_isLittleEndian())
    {
        MEM_write16(memPtr, val);
    }
    else
    {
        BYTE* p = (BYTE*)memPtr;
        p[0] = (BYTE)val;
        p[1] = (BYTE)(val>>8);
    }
}

MEM_STATIC U32 MEM_readLE24(const void* memPtr)
{
    return MEM_readLE16(memPtr) + (((const BYTE*)memPtr)[2] << 16);
}

MEM_STATIC U32 MEM_readLE32(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read32(memPtr);
    else
    {
        const BYTE* p = (const BYTE*)memPtr;
        return (U32)((U32)p[0] + ((U32)p[1]<<8) + ((U32)p[2]<<16) + ((U32)p[3]<<24));
    }
}


MEM_STATIC U64 MEM_readLE64(const void* memPtr)
{
    if (MEM_isLittleEndian())
        return MEM_read64(memPtr);
    else
    {
        const BYTE* p = (const BYTE*)memPtr;
        return (U64)((U64)p[0] + ((U64)p[1]<<8) + ((U64)p[2]<<16) + ((U64)p[3]<<24)
                     + ((U64)p[4]<<32) + ((U64)p[5]<<40) + ((U64)p[6]<<48) + ((U64)p[7]<<56));
    }
}


MEM_STATIC size_t MEM_readLEST(const void* memPtr)
{
    if (MEM_32bits())
        return (size_t)MEM_readLE32(memPtr);
    else
        return (size_t)MEM_readLE64(memPtr);
}


#if defined (__cplusplus)
}
#endif

#endif /* MEM_H_MODULE */

/*
    zstd - standard compression library
    Header File for static linking only
*/
#ifndef ZSTD_STATIC_H
#define ZSTD_STATIC_H


/* *************************************
*  Types
***************************************/
#define ZSTD_WINDOWLOG_ABSOLUTEMIN 11

/** from faster to stronger */
typedef enum { ZSTD_fast, ZSTD_greedy, ZSTD_lazy, ZSTD_lazy2, ZSTD_btlazy2 } ZSTD_strategy;

typedef struct
{
    U64 srcSize;       /* optional : tells how much bytes are present in the frame. Use 0 if not known. */
    U32 windowLog;     /* largest match distance : larger == more compression, more memory needed during decompression */
    U32 contentLog;    /* full search segment : larger == more compression, slower, more memory (useless for fast) */
    U32 hashLog;       /* dispatch table : larger == more memory, faster */
    U32 searchLog;     /* nb of searches : larger == more compression, slower */
    U32 searchLength;  /* size of matches : larger == faster decompression, sometimes less compression */
    ZSTD_strategy strategy;
} ZSTD_parameters;

typedef ZSTDv04_Dctx ZSTD_DCtx;

/* *************************************
*  Advanced functions
***************************************/
/** ZSTD_decompress_usingDict
*   Same as ZSTD_decompressDCtx, using a Dictionary content as prefix
*   Note : dict can be NULL, in which case, it's equivalent to ZSTD_decompressDCtx() */
static size_t ZSTD_decompress_usingDict(ZSTD_DCtx* ctx,
                                             void* dst, size_t maxDstSize,
                                       const void* src, size_t srcSize,
                                       const void* dict,size_t dictSize);


/* **************************************
*  Streaming functions (direct mode)
****************************************/
static size_t ZSTD_resetDCtx(ZSTD_DCtx* dctx);
static size_t ZSTD_getFrameParams(ZSTD_parameters* params, const void* src, size_t srcSize);
static void   ZSTD_decompress_insertDictionary(ZSTD_DCtx* ctx, const void* src, size_t srcSize);

static size_t ZSTD_nextSrcSizeToDecompress(ZSTD_DCtx* dctx);
static size_t ZSTD_decompressContinue(ZSTD_DCtx* dctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize);

/**
  Streaming decompression, bufferless mode

  A ZSTD_DCtx object is required to track streaming operations.
  Use ZSTD_createDCtx() / ZSTD_freeDCtx() to manage it.
  A ZSTD_DCtx object can be re-used multiple times. Use ZSTD_resetDCtx() to return to fresh status.

  First operation is to retrieve frame parameters, using ZSTD_getFrameParams().
  This function doesn't consume its input. It needs enough input data to properly decode the frame header.
  Objective is to retrieve *params.windowlog, to know minimum amount of memory required during decoding.
  Result : 0 when successful, it means the ZSTD_parameters structure has been filled.
           >0 : means there is not enough data into src. Provides the expected size to successfully decode header.
           errorCode, which can be tested using ZSTD_isError() (For example, if it's not a ZSTD header)

  Then, you can optionally insert a dictionary.
  This operation must mimic the compressor behavior, otherwise decompression will fail or be corrupted.

  Then it's possible to start decompression.
  Use ZSTD_nextSrcSizeToDecompress() and ZSTD_decompressContinue() alternatively.
  ZSTD_nextSrcSizeToDecompress() tells how much bytes to provide as 'srcSize' to ZSTD_decompressContinue().
  ZSTD_decompressContinue() requires this exact amount of bytes, or it will fail.
  ZSTD_decompressContinue() needs previous data blocks during decompression, up to (1 << windowlog).
  They should preferably be located contiguously, prior to current block. Alternatively, a round buffer is also possible.

  @result of ZSTD_decompressContinue() is the number of bytes regenerated within 'dst'.
  It can be zero, which is not an error; it just means ZSTD_decompressContinue() has decoded some header.

  A frame is fully decoded when ZSTD_nextSrcSizeToDecompress() returns zero.
  Context can then be reset to start a new decompression.
*/




#endif  /* ZSTD_STATIC_H */


/*
    zstd_internal - common functions to include
    Header File for include
*/
#ifndef ZSTD_CCOMMON_H_MODULE
#define ZSTD_CCOMMON_H_MODULE

/* *************************************
*  Common macros
***************************************/
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))


/* *************************************
*  Common constants
***************************************/
#define ZSTD_MAGICNUMBER 0xFD2FB524   /* v0.4 */

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define BLOCKSIZE (128 KB)                 /* define, for static allocation */

static const size_t ZSTD_blockHeaderSize = 3;
static const size_t ZSTD_frameHeaderSize_min = 5;
#define ZSTD_frameHeaderSize_max 5         /* define, for static allocation */

#define BIT7 128
#define BIT6  64
#define BIT5  32
#define BIT4  16
#define BIT1   2
#define BIT0   1

#define IS_RAW BIT0
#define IS_RLE BIT1

#define MINMATCH 4
#define REPCODE_STARTVALUE 4

#define MLbits   7
#define LLbits   6
#define Offbits  5
#define MaxML  ((1<<MLbits) - 1)
#define MaxLL  ((1<<LLbits) - 1)
#define MaxOff ((1<<Offbits)- 1)
#define MLFSELog   10
#define LLFSELog   10
#define OffFSELog   9
#define MaxSeq MAX(MaxLL, MaxML)

#define MIN_SEQUENCES_SIZE (2 /*seqNb*/ + 2 /*dumps*/ + 3 /*seqTables*/ + 1 /*bitStream*/)
#define MIN_CBLOCK_SIZE (3 /*litCSize*/ + MIN_SEQUENCES_SIZE)

#define ZSTD_CONTENTSIZE_ERROR   (0ULL - 2)

typedef enum { bt_compressed, bt_raw, bt_rle, bt_end } blockType_t;


/* ******************************************
*  Shared functions to include for inlining
********************************************/
static void ZSTD_copy8(void* dst, const void* src) { memcpy(dst, src, 8); }

#define COPY8(d,s) { ZSTD_copy8(d,s); d+=8; s+=8; }

/*! ZSTD_wildcopy : custom version of memcpy(), can copy up to 7-8 bytes too many */
static void ZSTD_wildcopy(void* dst, const void* src, ptrdiff_t length)
{
    const BYTE* ip = (const BYTE*)src;
    BYTE* op = (BYTE*)dst;
    BYTE* const oend = op + length;
    do
        COPY8(op, ip)
    while (op < oend);
}



/* ******************************************************************
   FSE : Finite State Entropy coder
   header file
****************************************************************** */
#ifndef FSE_H
#define FSE_H

#if defined (__cplusplus)
extern "C" {
#endif


/* *****************************************
*  Includes
******************************************/
#include <stddef.h>    /* size_t, ptrdiff_t */


/* *****************************************
*  FSE simple functions
******************************************/
static size_t FSE_decompress(void* dst,  size_t maxDstSize,
                const void* cSrc, size_t cSrcSize);
/*!
FSE_decompress():
    Decompress FSE data from buffer 'cSrc', of size 'cSrcSize',
    into already allocated destination buffer 'dst', of size 'maxDstSize'.
    return : size of regenerated data (<= maxDstSize)
             or an error code, which can be tested using FSE_isError()

    ** Important ** : FSE_decompress() doesn't decompress non-compressible nor RLE data !!!
    Why ? : making this distinction requires a header.
    Header management is intentionally delegated to the user layer, which can better manage special cases.
*/


/* *****************************************
*  Tool functions
******************************************/
/* Error Management */
static unsigned    FSE_isError(size_t code);        /* tells if a return value is an error code */



/* *****************************************
*  FSE detailed API
******************************************/
/*!
FSE_compress() does the following:
1. count symbol occurrence from source[] into table count[]
2. normalize counters so that sum(count[]) == Power_of_2 (2^tableLog)
3. save normalized counters to memory buffer using writeNCount()
4. build encoding table 'CTable' from normalized counters
5. encode the data stream using encoding table 'CTable'

FSE_decompress() does the following:
1. read normalized counters with readNCount()
2. build decoding table 'DTable' from normalized counters
3. decode the data stream using decoding table 'DTable'

The following API allows targeting specific sub-functions for advanced tasks.
For example, it's possible to compress several blocks using the same 'CTable',
or to save and provide normalized distribution using external method.
*/


/* *** DECOMPRESSION *** */

/*!
FSE_readNCount():
   Read compactly saved 'normalizedCounter' from 'rBuffer'.
   return : size read from 'rBuffer'
            or an errorCode, which can be tested using FSE_isError()
            maxSymbolValuePtr[0] and tableLogPtr[0] will also be updated with their respective values */
static  size_t FSE_readNCount (short* normalizedCounter, unsigned* maxSymbolValuePtr, unsigned* tableLogPtr, const void* rBuffer, size_t rBuffSize);

/*!
Constructor and Destructor of type FSE_DTable
    Note that its size depends on 'tableLog' */
typedef unsigned FSE_DTable;   /* don't allocate that. It's just a way to be more restrictive than void* */

/*!
FSE_buildDTable():
   Builds 'dt', which must be already allocated, using FSE_createDTable()
   return : 0,
            or an errorCode, which can be tested using FSE_isError() */
static size_t FSE_buildDTable ( FSE_DTable* dt, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog);

/*!
FSE_decompress_usingDTable():
   Decompress compressed source 'cSrc' of size 'cSrcSize' using 'dt'
   into 'dst' which must be already allocated.
   return : size of regenerated data (necessarily <= maxDstSize)
            or an errorCode, which can be tested using FSE_isError() */
static  size_t FSE_decompress_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const FSE_DTable* dt);

/*!
Tutorial :
----------
(Note : these functions only decompress FSE-compressed blocks.
 If block is uncompressed, use memcpy() instead
 If block is a single repeated byte, use memset() instead )

The first step is to obtain the normalized frequencies of symbols.
This can be performed by FSE_readNCount() if it was saved using FSE_writeNCount().
'normalizedCounter' must be already allocated, and have at least 'maxSymbolValuePtr[0]+1' cells of signed short.
In practice, that means it's necessary to know 'maxSymbolValue' beforehand,
or size the table to handle worst case situations (typically 256).
FSE_readNCount() will provide 'tableLog' and 'maxSymbolValue'.
The result of FSE_readNCount() is the number of bytes read from 'rBuffer'.
Note that 'rBufferSize' must be at least 4 bytes, even if useful information is less than that.
If there is an error, the function will return an error code, which can be tested using FSE_isError().

The next step is to build the decompression tables 'FSE_DTable' from 'normalizedCounter'.
This is performed by the function FSE_buildDTable().
The space required by 'FSE_DTable' must be already allocated using FSE_createDTable().
If there is an error, the function will return an error code, which can be tested using FSE_isError().

'FSE_DTable' can then be used to decompress 'cSrc', with FSE_decompress_usingDTable().
'cSrcSize' must be strictly correct, otherwise decompression will fail.
FSE_decompress_usingDTable() result will tell how many bytes were regenerated (<=maxDstSize).
If there is an error, the function will return an error code, which can be tested using FSE_isError(). (ex: dst buffer too small)
*/


#if defined (__cplusplus)
}
#endif

#endif  /* FSE_H */


/* ******************************************************************
   bitstream
   Part of NewGen Entropy library
   header file (to include)
   Copyright (C) 2013-2015, Yann Collet.

   BSD 2-Clause License (https://opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
   - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
#ifndef BITSTREAM_H_MODULE
#define BITSTREAM_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif


/*
*  This API consists of small unitary functions, which highly benefit from being inlined.
*  Since link-time-optimization is not available for all compilers,
*  these functions are defined into a .h to be included.
*/

/**********************************************
*  bitStream decompression API (read backward)
**********************************************/
typedef struct
{
    size_t   bitContainer;
    unsigned bitsConsumed;
    const char* ptr;
    const char* start;
} BIT_DStream_t;

typedef enum { BIT_DStream_unfinished = 0,
               BIT_DStream_endOfBuffer = 1,
               BIT_DStream_completed = 2,
               BIT_DStream_overflow = 3 } BIT_DStream_status;  /* result of BIT_reloadDStream() */
               /* 1,2,4,8 would be better for bitmap combinations, but slows down performance a bit ... :( */

MEM_STATIC size_t   BIT_initDStream(BIT_DStream_t* bitD, const void* srcBuffer, size_t srcSize);
MEM_STATIC size_t   BIT_readBits(BIT_DStream_t* bitD, unsigned nbBits);
MEM_STATIC BIT_DStream_status BIT_reloadDStream(BIT_DStream_t* bitD);
MEM_STATIC unsigned BIT_endOfDStream(const BIT_DStream_t* bitD);




/******************************************
*  unsafe API
******************************************/
MEM_STATIC size_t BIT_readBitsFast(BIT_DStream_t* bitD, unsigned nbBits);
/* faster, but works only if nbBits >= 1 */



/****************************************************************
*  Helper functions
****************************************************************/
MEM_STATIC unsigned BIT_highbit32 (U32 val)
{
#   if defined(_MSC_VER)   /* Visual */
    unsigned long r;
    return _BitScanReverse(&r, val) ? (unsigned)r : 0;
#   elif defined(__GNUC__) && (__GNUC__ >= 3)   /* Use GCC Intrinsic */
    return __builtin_clz (val) ^ 31;
#   else   /* Software version */
    static const unsigned DeBruijnClz[32] = { 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
    U32 v = val;
    unsigned r;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    r = DeBruijnClz[ (U32) (v * 0x07C4ACDDU) >> 27];
    return r;
#   endif
}


/**********************************************************
* bitStream decoding
**********************************************************/

/*!BIT_initDStream
*  Initialize a BIT_DStream_t.
*  @bitD : a pointer to an already allocated BIT_DStream_t structure
*  @srcBuffer must point at the beginning of a bitStream
*  @srcSize must be the exact size of the bitStream
*  @result : size of stream (== srcSize) or an errorCode if a problem is detected
*/
MEM_STATIC size_t BIT_initDStream(BIT_DStream_t* bitD, const void* srcBuffer, size_t srcSize)
{
    if (srcSize < 1) { memset(bitD, 0, sizeof(*bitD)); return ERROR(srcSize_wrong); }

    if (srcSize >=  sizeof(size_t))   /* normal case */
    {
        U32 contain32;
        bitD->start = (const char*)srcBuffer;
        bitD->ptr   = (const char*)srcBuffer + srcSize - sizeof(size_t);
        bitD->bitContainer = MEM_readLEST(bitD->ptr);
        contain32 = ((const BYTE*)srcBuffer)[srcSize-1];
        if (contain32 == 0) return ERROR(GENERIC);   /* endMark not present */
        bitD->bitsConsumed = 8 - BIT_highbit32(contain32);
    }
    else
    {
        U32 contain32;
        bitD->start = (const char*)srcBuffer;
        bitD->ptr   = bitD->start;
        bitD->bitContainer = *(const BYTE*)(bitD->start);
        switch(srcSize)
        {
            case 7: bitD->bitContainer += (size_t)(((const BYTE*)(bitD->start))[6]) << (sizeof(size_t)*8 - 16);/* fall-through */
            case 6: bitD->bitContainer += (size_t)(((const BYTE*)(bitD->start))[5]) << (sizeof(size_t)*8 - 24);/* fall-through */
            case 5: bitD->bitContainer += (size_t)(((const BYTE*)(bitD->start))[4]) << (sizeof(size_t)*8 - 32);/* fall-through */
            case 4: bitD->bitContainer += (size_t)(((const BYTE*)(bitD->start))[3]) << 24; /* fall-through */
            case 3: bitD->bitContainer += (size_t)(((const BYTE*)(bitD->start))[2]) << 16; /* fall-through */
            case 2: bitD->bitContainer += (size_t)(((const BYTE*)(bitD->start))[1]) <<  8; /* fall-through */
            default: break;
        }
        contain32 = ((const BYTE*)srcBuffer)[srcSize-1];
        if (contain32 == 0) return ERROR(GENERIC);   /* endMark not present */
        bitD->bitsConsumed = 8 - BIT_highbit32(contain32);
        bitD->bitsConsumed += (U32)(sizeof(size_t) - srcSize)*8;
    }

    return srcSize;
}

MEM_STATIC size_t BIT_lookBits(BIT_DStream_t* bitD, U32 nbBits)
{
    const U32 bitMask = sizeof(bitD->bitContainer)*8 - 1;
    return ((bitD->bitContainer << (bitD->bitsConsumed & bitMask)) >> 1) >> ((bitMask-nbBits) & bitMask);
}

/*! BIT_lookBitsFast :
*   unsafe version; only works if nbBits >= 1 */
MEM_STATIC size_t BIT_lookBitsFast(BIT_DStream_t* bitD, U32 nbBits)
{
    const U32 bitMask = sizeof(bitD->bitContainer)*8 - 1;
    return (bitD->bitContainer << (bitD->bitsConsumed & bitMask)) >> (((bitMask+1)-nbBits) & bitMask);
}

MEM_STATIC void BIT_skipBits(BIT_DStream_t* bitD, U32 nbBits)
{
    bitD->bitsConsumed += nbBits;
}

MEM_STATIC size_t BIT_readBits(BIT_DStream_t* bitD, U32 nbBits)
{
    size_t value = BIT_lookBits(bitD, nbBits);
    BIT_skipBits(bitD, nbBits);
    return value;
}

/*!BIT_readBitsFast :
*  unsafe version; only works if nbBits >= 1 */
MEM_STATIC size_t BIT_readBitsFast(BIT_DStream_t* bitD, U32 nbBits)
{
    size_t value = BIT_lookBitsFast(bitD, nbBits);
    BIT_skipBits(bitD, nbBits);
    return value;
}

MEM_STATIC BIT_DStream_status BIT_reloadDStream(BIT_DStream_t* bitD)
{
    if (bitD->bitsConsumed > (sizeof(bitD->bitContainer)*8))  /* should never happen */
        return BIT_DStream_overflow;

    if (bitD->ptr >= bitD->start + sizeof(bitD->bitContainer))
    {
        bitD->ptr -= bitD->bitsConsumed >> 3;
        bitD->bitsConsumed &= 7;
        bitD->bitContainer = MEM_readLEST(bitD->ptr);
        return BIT_DStream_unfinished;
    }
    if (bitD->ptr == bitD->start)
    {
        if (bitD->bitsConsumed < sizeof(bitD->bitContainer)*8) return BIT_DStream_endOfBuffer;
        return BIT_DStream_completed;
    }
    {
        U32 nbBytes = bitD->bitsConsumed >> 3;
        BIT_DStream_status result = BIT_DStream_unfinished;
        if (bitD->ptr - nbBytes < bitD->start)
        {
            nbBytes = (U32)(bitD->ptr - bitD->start);  /* ptr > start */
            result = BIT_DStream_endOfBuffer;
        }
        bitD->ptr -= nbBytes;
        bitD->bitsConsumed -= nbBytes*8;
        bitD->bitContainer = MEM_readLEST(bitD->ptr);   /* reminder : srcSize > sizeof(bitD) */
        return result;
    }
}

/*! BIT_endOfDStream
*   @return Tells if DStream has reached its exact end
*/
MEM_STATIC unsigned BIT_endOfDStream(const BIT_DStream_t* DStream)
{
    return ((DStream->ptr == DStream->start) && (DStream->bitsConsumed == sizeof(DStream->bitContainer)*8));
}

#if defined (__cplusplus)
}
#endif

#endif /* BITSTREAM_H_MODULE */



/* ******************************************************************
   FSE : Finite State Entropy coder
   header file for static linking (only)
   Copyright (C) 2013-2015, Yann Collet

   BSD 2-Clause License (https://opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
   - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
#ifndef FSE_STATIC_H
#define FSE_STATIC_H

#if defined (__cplusplus)
extern "C" {
#endif


/* *****************************************
*  Static allocation
*******************************************/
/* FSE buffer bounds */
#define FSE_NCOUNTBOUND 512
#define FSE_BLOCKBOUND(size) (size + (size>>7))
#define FSE_COMPRESSBOUND(size) (FSE_NCOUNTBOUND + FSE_BLOCKBOUND(size))   /* Macro version, useful for static allocation */

/* It is possible to statically allocate FSE CTable/DTable as a table of unsigned using below macros */
#define FSE_CTABLE_SIZE_U32(maxTableLog, maxSymbolValue)   (1 + (1<<(maxTableLog-1)) + ((maxSymbolValue+1)*2))
#define FSE_DTABLE_SIZE_U32(maxTableLog)                   (1 + (1<<maxTableLog))


/* *****************************************
*  FSE advanced API
*******************************************/
static size_t FSE_buildDTable_raw (FSE_DTable* dt, unsigned nbBits);
/* build a fake FSE_DTable, designed to read an uncompressed bitstream where each symbol uses nbBits */

static size_t FSE_buildDTable_rle (FSE_DTable* dt, unsigned char symbolValue);
/* build a fake FSE_DTable, designed to always generate the same symbolValue */



/* *****************************************
*  FSE symbol decompression API
*******************************************/
typedef struct
{
    size_t      state;
    const void* table;   /* precise table may vary, depending on U16 */
} FSE_DState_t;


static void     FSE_initDState(FSE_DState_t* DStatePtr, BIT_DStream_t* bitD, const FSE_DTable* dt);

static unsigned char FSE_decodeSymbol(FSE_DState_t* DStatePtr, BIT_DStream_t* bitD);

static unsigned FSE_endOfDState(const FSE_DState_t* DStatePtr);


/* *****************************************
*  FSE unsafe API
*******************************************/
static unsigned char FSE_decodeSymbolFast(FSE_DState_t* DStatePtr, BIT_DStream_t* bitD);
/* faster, but works only if nbBits is always >= 1 (otherwise, result will be corrupted) */


/* *****************************************
*  Implementation of inlined functions
*******************************************/
/* decompression */

typedef struct {
    U16 tableLog;
    U16 fastMode;
} FSE_DTableHeader;   /* sizeof U32 */

typedef struct
{
    unsigned short newState;
    unsigned char  symbol;
    unsigned char  nbBits;
} FSE_decode_t;   /* size == U32 */

MEM_STATIC void FSE_initDState(FSE_DState_t* DStatePtr, BIT_DStream_t* bitD, const FSE_DTable* dt)
{
    FSE_DTableHeader DTableH;
    memcpy(&DTableH, dt, sizeof(DTableH));
    DStatePtr->state = BIT_readBits(bitD, DTableH.tableLog);
    BIT_reloadDStream(bitD);
    DStatePtr->table = dt + 1;
}

MEM_STATIC BYTE FSE_decodeSymbol(FSE_DState_t* DStatePtr, BIT_DStream_t* bitD)
{
    const FSE_decode_t DInfo = ((const FSE_decode_t*)(DStatePtr->table))[DStatePtr->state];
    const U32  nbBits = DInfo.nbBits;
    BYTE symbol = DInfo.symbol;
    size_t lowBits = BIT_readBits(bitD, nbBits);

    DStatePtr->state = DInfo.newState + lowBits;
    return symbol;
}

MEM_STATIC BYTE FSE_decodeSymbolFast(FSE_DState_t* DStatePtr, BIT_DStream_t* bitD)
{
    const FSE_decode_t DInfo = ((const FSE_decode_t*)(DStatePtr->table))[DStatePtr->state];
    const U32 nbBits = DInfo.nbBits;
    BYTE symbol = DInfo.symbol;
    size_t lowBits = BIT_readBitsFast(bitD, nbBits);

    DStatePtr->state = DInfo.newState + lowBits;
    return symbol;
}

MEM_STATIC unsigned FSE_endOfDState(const FSE_DState_t* DStatePtr)
{
    return DStatePtr->state == 0;
}


#if defined (__cplusplus)
}
#endif

#endif  /* FSE_STATIC_H */

/* ******************************************************************
   FSE : Finite State Entropy coder
   Copyright (C) 2013-2015, Yann Collet.

   BSD 2-Clause License (https://opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - FSE source repository : https://github.com/Cyan4973/FiniteStateEntropy
    - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */

#ifndef FSE_COMMONDEFS_ONLY

/* **************************************************************
*  Tuning parameters
****************************************************************/
/*!MEMORY_USAGE :
*  Memory usage formula : N->2^N Bytes (examples : 10 -> 1KB; 12 -> 4KB ; 16 -> 64KB; 20 -> 1MB; etc.)
*  Increasing memory usage improves compression ratio
*  Reduced memory usage can improve speed, due to cache effect
*  Recommended max value is 14, for 16KB, which nicely fits into Intel x86 L1 cache */
#define FSE_MAX_MEMORY_USAGE 14
#define FSE_DEFAULT_MEMORY_USAGE 13

/*!FSE_MAX_SYMBOL_VALUE :
*  Maximum symbol value authorized.
*  Required for proper stack allocation */
#define FSE_MAX_SYMBOL_VALUE 255


/* **************************************************************
*  template functions type & suffix
****************************************************************/
#define FSE_FUNCTION_TYPE BYTE
#define FSE_FUNCTION_EXTENSION
#define FSE_DECODE_TYPE FSE_decode_t


#endif   /* !FSE_COMMONDEFS_ONLY */

/* **************************************************************
*  Compiler specifics
****************************************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  define FORCE_INLINE static __forceinline
#  include <intrin.h>                    /* For Visual 2005 */
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4214)        /* disable: C4214: non-int bitfields */
#else
#  if defined (__cplusplus) || defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* C99 */
#    ifdef __GNUC__
#      define FORCE_INLINE static inline __attribute__((always_inline))
#    else
#      define FORCE_INLINE static inline
#    endif
#  else
#    define FORCE_INLINE static
#  endif /* __STDC_VERSION__ */
#endif


/* **************************************************************
*  Dependencies
****************************************************************/
#include <stdlib.h>     /* malloc, free, qsort */
#include <string.h>     /* memcpy, memset */
#include <stdio.h>      /* printf (debug) */


/* ***************************************************************
*  Constants
*****************************************************************/
#define FSE_MAX_TABLELOG  (FSE_MAX_MEMORY_USAGE-2)
#define FSE_MAX_TABLESIZE (1U<<FSE_MAX_TABLELOG)
#define FSE_MAXTABLESIZE_MASK (FSE_MAX_TABLESIZE-1)
#define FSE_DEFAULT_TABLELOG (FSE_DEFAULT_MEMORY_USAGE-2)
#define FSE_MIN_TABLELOG 5

#define FSE_TABLELOG_ABSOLUTE_MAX 15
#if FSE_MAX_TABLELOG > FSE_TABLELOG_ABSOLUTE_MAX
#error "FSE_MAX_TABLELOG > FSE_TABLELOG_ABSOLUTE_MAX is not supported"
#endif


/* **************************************************************
*  Error Management
****************************************************************/
#define FSE_STATIC_ASSERT(c) { enum { FSE_static_assert = 1/(int)(!!(c)) }; }   /* use only *after* variable declarations */


/* **************************************************************
*  Complex types
****************************************************************/
typedef U32 DTable_max_t[FSE_DTABLE_SIZE_U32(FSE_MAX_TABLELOG)];


/*-**************************************************************
*  Templates
****************************************************************/
/*
  designed to be included
  for type-specific functions (template emulation in C)
  Objective is to write these functions only once, for improved maintenance
*/

/* safety checks */
#ifndef FSE_FUNCTION_EXTENSION
#  error "FSE_FUNCTION_EXTENSION must be defined"
#endif
#ifndef FSE_FUNCTION_TYPE
#  error "FSE_FUNCTION_TYPE must be defined"
#endif

/* Function names */
#define FSE_CAT(X,Y) X##Y
#define FSE_FUNCTION_NAME(X,Y) FSE_CAT(X,Y)
#define FSE_TYPE_NAME(X,Y) FSE_CAT(X,Y)

static U32 FSE_tableStep(U32 tableSize) { return (tableSize>>1) + (tableSize>>3) + 3; }


static size_t FSE_buildDTable(FSE_DTable* dt, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog)
{
    FSE_DTableHeader DTableH;
    void* const tdPtr = dt+1;   /* because dt is unsigned, 32-bits aligned on 32-bits */
    FSE_DECODE_TYPE* const tableDecode = (FSE_DECODE_TYPE*) (tdPtr);
    const U32 tableSize = 1 << tableLog;
    const U32 tableMask = tableSize-1;
    const U32 step = FSE_tableStep(tableSize);
    U16 symbolNext[FSE_MAX_SYMBOL_VALUE+1];
    U32 position = 0;
    U32 highThreshold = tableSize-1;
    const S16 largeLimit= (S16)(1 << (tableLog-1));
    U32 noLarge = 1;
    U32 s;

    /* Sanity Checks */
    if (maxSymbolValue > FSE_MAX_SYMBOL_VALUE) return ERROR(maxSymbolValue_tooLarge);
    if (tableLog > FSE_MAX_TABLELOG) return ERROR(tableLog_tooLarge);

    /* Init, lay down lowprob symbols */
    memset(tableDecode, 0, sizeof(FSE_DECODE_TYPE) * (maxSymbolValue+1) );   /* useless init, but keep static analyzer happy, and we don't need to performance optimize legacy decoders */
    DTableH.tableLog = (U16)tableLog;
    for (s=0; s<=maxSymbolValue; s++)
    {
        if (normalizedCounter[s]==-1)
        {
            tableDecode[highThreshold--].symbol = (FSE_FUNCTION_TYPE)s;
            symbolNext[s] = 1;
        }
        else
        {
            if (normalizedCounter[s] >= largeLimit) noLarge=0;
            symbolNext[s] = normalizedCounter[s];
        }
    }

    /* Spread symbols */
    for (s=0; s<=maxSymbolValue; s++)
    {
        int i;
        for (i=0; i<normalizedCounter[s]; i++)
        {
            tableDecode[position].symbol = (FSE_FUNCTION_TYPE)s;
            position = (position + step) & tableMask;
            while (position > highThreshold) position = (position + step) & tableMask;   /* lowprob area */
        }
    }

    if (position!=0) return ERROR(GENERIC);   /* position must reach all cells once, otherwise normalizedCounter is incorrect */

    /* Build Decoding table */
    {
        U32 i;
        for (i=0; i<tableSize; i++)
        {
            FSE_FUNCTION_TYPE symbol = (FSE_FUNCTION_TYPE)(tableDecode[i].symbol);
            U16 nextState = symbolNext[symbol]++;
            tableDecode[i].nbBits = (BYTE) (tableLog - BIT_highbit32 ((U32)nextState) );
            tableDecode[i].newState = (U16) ( (nextState << tableDecode[i].nbBits) - tableSize);
        }
    }

    DTableH.fastMode = (U16)noLarge;
    memcpy(dt, &DTableH, sizeof(DTableH));
    return 0;
}


#ifndef FSE_COMMONDEFS_ONLY
/******************************************
*  FSE helper functions
******************************************/
static unsigned FSE_isError(size_t code) { return ERR_isError(code); }


/****************************************************************
*  FSE NCount encoding-decoding
****************************************************************/
static short FSE_abs(short a)
{
    return a<0 ? -a : a;
}

static size_t FSE_readNCount (short* normalizedCounter, unsigned* maxSVPtr, unsigned* tableLogPtr,
                 const void* headerBuffer, size_t hbSize)
{
    const BYTE* const istart = (const BYTE*) headerBuffer;
    const BYTE* const iend = istart + hbSize;
    const BYTE* ip = istart;
    int nbBits;
    int remaining;
    int threshold;
    U32 bitStream;
    int bitCount;
    unsigned charnum = 0;
    int previous0 = 0;

    if (hbSize < 4) return ERROR(srcSize_wrong);
    bitStream = MEM_readLE32(ip);
    nbBits = (bitStream & 0xF) + FSE_MIN_TABLELOG;   /* extract tableLog */
    if (nbBits > FSE_TABLELOG_ABSOLUTE_MAX) return ERROR(tableLog_tooLarge);
    bitStream >>= 4;
    bitCount = 4;
    *tableLogPtr = nbBits;
    remaining = (1<<nbBits)+1;
    threshold = 1<<nbBits;
    nbBits++;

    while ((remaining>1) && (charnum<=*maxSVPtr))
    {
        if (previous0)
        {
            unsigned n0 = charnum;
            while ((bitStream & 0xFFFF) == 0xFFFF)
            {
                n0+=24;
                if (ip < iend-5)
                {
                    ip+=2;
                    bitStream = MEM_readLE32(ip) >> bitCount;
                }
                else
                {
                    bitStream >>= 16;
                    bitCount+=16;
                }
            }
            while ((bitStream & 3) == 3)
            {
                n0+=3;
                bitStream>>=2;
                bitCount+=2;
            }
            n0 += bitStream & 3;
            bitCount += 2;
            if (n0 > *maxSVPtr) return ERROR(maxSymbolValue_tooSmall);
            while (charnum < n0) normalizedCounter[charnum++] = 0;
            if ((ip <= iend-7) || (ip + (bitCount>>3) <= iend-4))
            {
                ip += bitCount>>3;
                bitCount &= 7;
                bitStream = MEM_readLE32(ip) >> bitCount;
            }
            else
                bitStream >>= 2;
        }
        {
            const short max = (short)((2*threshold-1)-remaining);
            short count;

            if ((bitStream & (threshold-1)) < (U32)max)
            {
                count = (short)(bitStream & (threshold-1));
                bitCount   += nbBits-1;
            }
            else
            {
                count = (short)(bitStream & (2*threshold-1));
                if (count >= threshold) count -= max;
                bitCount   += nbBits;
            }

            count--;   /* extra accuracy */
            remaining -= FSE_abs(count);
            normalizedCounter[charnum++] = count;
            previous0 = !count;
            while (remaining < threshold)
            {
                nbBits--;
                threshold >>= 1;
            }

            {
                if ((ip <= iend-7) || (ip + (bitCount>>3) <= iend-4))
                {
                    ip += bitCount>>3;
                    bitCount &= 7;
                }
                else
                {
                    bitCount -= (int)(8 * (iend - 4 - ip));
                    ip = iend - 4;
                }
                bitStream = MEM_readLE32(ip) >> (bitCount & 31);
            }
        }
    }
    if (remaining != 1) return ERROR(GENERIC);
    *maxSVPtr = charnum-1;

    ip += (bitCount+7)>>3;
    if ((size_t)(ip-istart) > hbSize) return ERROR(srcSize_wrong);
    return ip-istart;
}


/*********************************************************
*  Decompression (Byte symbols)
*********************************************************/
static size_t FSE_buildDTable_rle (FSE_DTable* dt, BYTE symbolValue)
{
    void* ptr = dt;
    FSE_DTableHeader* const DTableH = (FSE_DTableHeader*)ptr;
    void* dPtr = dt + 1;
    FSE_decode_t* const cell = (FSE_decode_t*)dPtr;

    DTableH->tableLog = 0;
    DTableH->fastMode = 0;

    cell->newState = 0;
    cell->symbol = symbolValue;
    cell->nbBits = 0;

    return 0;
}


static size_t FSE_buildDTable_raw (FSE_DTable* dt, unsigned nbBits)
{
    void* ptr = dt;
    FSE_DTableHeader* const DTableH = (FSE_DTableHeader*)ptr;
    void* dPtr = dt + 1;
    FSE_decode_t* const dinfo = (FSE_decode_t*)dPtr;
    const unsigned tableSize = 1 << nbBits;
    const unsigned tableMask = tableSize - 1;
    const unsigned maxSymbolValue = tableMask;
    unsigned s;

    /* Sanity checks */
    if (nbBits < 1) return ERROR(GENERIC);         /* min size */

    /* Build Decoding Table */
    DTableH->tableLog = (U16)nbBits;
    DTableH->fastMode = 1;
    for (s=0; s<=maxSymbolValue; s++)
    {
        dinfo[s].newState = 0;
        dinfo[s].symbol = (BYTE)s;
        dinfo[s].nbBits = (BYTE)nbBits;
    }

    return 0;
}

FORCE_INLINE size_t FSE_decompress_usingDTable_generic(
          void* dst, size_t maxDstSize,
    const void* cSrc, size_t cSrcSize,
    const FSE_DTable* dt, const unsigned fast)
{
    BYTE* const ostart = (BYTE*) dst;
    BYTE* op = ostart;
    BYTE* const omax = op + maxDstSize;
    BYTE* const olimit = omax-3;

    BIT_DStream_t bitD;
    FSE_DState_t state1;
    FSE_DState_t state2;
    size_t errorCode;

    /* Init */
    errorCode = BIT_initDStream(&bitD, cSrc, cSrcSize);   /* replaced last arg by maxCompressed Size */
    if (FSE_isError(errorCode)) return errorCode;

    FSE_initDState(&state1, &bitD, dt);
    FSE_initDState(&state2, &bitD, dt);

#define FSE_GETSYMBOL(statePtr) fast ? FSE_decodeSymbolFast(statePtr, &bitD) : FSE_decodeSymbol(statePtr, &bitD)

    /* 4 symbols per loop */
    for ( ; (BIT_reloadDStream(&bitD)==BIT_DStream_unfinished) && (op<olimit) ; op+=4)
    {
        op[0] = FSE_GETSYMBOL(&state1);

        if (FSE_MAX_TABLELOG*2+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            BIT_reloadDStream(&bitD);

        op[1] = FSE_GETSYMBOL(&state2);

        if (FSE_MAX_TABLELOG*4+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            { if (BIT_reloadDStream(&bitD) > BIT_DStream_unfinished) { op+=2; break; } }

        op[2] = FSE_GETSYMBOL(&state1);

        if (FSE_MAX_TABLELOG*2+7 > sizeof(bitD.bitContainer)*8)    /* This test must be static */
            BIT_reloadDStream(&bitD);

        op[3] = FSE_GETSYMBOL(&state2);
    }

    /* tail */
    /* note : BIT_reloadDStream(&bitD) >= FSE_DStream_partiallyFilled; Ends at exactly BIT_DStream_completed */
    while (1)
    {
        if ( (BIT_reloadDStream(&bitD)>BIT_DStream_completed) || (op==omax) || (BIT_endOfDStream(&bitD) && (fast || FSE_endOfDState(&state1))) )
            break;

        *op++ = FSE_GETSYMBOL(&state1);

        if ( (BIT_reloadDStream(&bitD)>BIT_DStream_completed) || (op==omax) || (BIT_endOfDStream(&bitD) && (fast || FSE_endOfDState(&state2))) )
            break;

        *op++ = FSE_GETSYMBOL(&state2);
    }

    /* end ? */
    if (BIT_endOfDStream(&bitD) && FSE_endOfDState(&state1) && FSE_endOfDState(&state2))
        return op-ostart;

    if (op==omax) return ERROR(dstSize_tooSmall);   /* dst buffer is full, but cSrc unfinished */

    return ERROR(corruption_detected);
}


static size_t FSE_decompress_usingDTable(void* dst, size_t originalSize,
                            const void* cSrc, size_t cSrcSize,
                            const FSE_DTable* dt)
{
    FSE_DTableHeader DTableH;
    U32 fastMode;

    memcpy(&DTableH, dt, sizeof(DTableH));
    fastMode = DTableH.fastMode;

    /* select fast mode (static) */
    if (fastMode) return FSE_decompress_usingDTable_generic(dst, originalSize, cSrc, cSrcSize, dt, 1);
    return FSE_decompress_usingDTable_generic(dst, originalSize, cSrc, cSrcSize, dt, 0);
}


static size_t FSE_decompress(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize)
{
    const BYTE* const istart = (const BYTE*)cSrc;
    const BYTE* ip = istart;
    short counting[FSE_MAX_SYMBOL_VALUE+1];
    DTable_max_t dt;   /* Static analyzer seems unable to understand this table will be properly initialized later */
    unsigned tableLog;
    unsigned maxSymbolValue = FSE_MAX_SYMBOL_VALUE;
    size_t errorCode;

    if (cSrcSize<2) return ERROR(srcSize_wrong);   /* too small input size */

    /* normal FSE decoding mode */
    errorCode = FSE_readNCount (counting, &maxSymbolValue, &tableLog, istart, cSrcSize);
    if (FSE_isError(errorCode)) return errorCode;
    if (errorCode >= cSrcSize) return ERROR(srcSize_wrong);   /* too small input size */
    ip += errorCode;
    cSrcSize -= errorCode;

    errorCode = FSE_buildDTable (dt, counting, maxSymbolValue, tableLog);
    if (FSE_isError(errorCode)) return errorCode;

    /* always return, even if it is an error code */
    return FSE_decompress_usingDTable (dst, maxDstSize, ip, cSrcSize, dt);
}



#endif   /* FSE_COMMONDEFS_ONLY */


/* ******************************************************************
   Huff0 : Huffman coder, part of New Generation Entropy library
   header file
   Copyright (C) 2013-2015, Yann Collet.

   BSD 2-Clause License (https://opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
   - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
#ifndef HUFF0_H
#define HUFF0_H

#if defined (__cplusplus)
extern "C" {
#endif


/* ****************************************
*  Dependency
******************************************/
#include <stddef.h>    /* size_t */


/* ****************************************
*  Huff0 simple functions
******************************************/
static size_t HUF_decompress(void* dst,  size_t dstSize,
                const void* cSrc, size_t cSrcSize);
/*!
HUF_decompress():
    Decompress Huff0 data from buffer 'cSrc', of size 'cSrcSize',
    into already allocated destination buffer 'dst', of size 'dstSize'.
    'dstSize' must be the exact size of original (uncompressed) data.
    Note : in contrast with FSE, HUF_decompress can regenerate RLE (cSrcSize==1) and uncompressed (cSrcSize==dstSize) data, because it knows size to regenerate.
    @return : size of regenerated data (== dstSize)
              or an error code, which can be tested using HUF_isError()
*/


/* ****************************************
*  Tool functions
******************************************/
/* Error Management */
static unsigned    HUF_isError(size_t code);        /* tells if a return value is an error code */


#if defined (__cplusplus)
}
#endif

#endif   /* HUFF0_H */


/* ******************************************************************
   Huff0 : Huffman coder, part of New Generation Entropy library
   header file for static linking (only)
   Copyright (C) 2013-2015, Yann Collet

   BSD 2-Clause License (https://opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
   - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
#ifndef HUFF0_STATIC_H
#define HUFF0_STATIC_H

#if defined (__cplusplus)
extern "C" {
#endif



/* ****************************************
*  Static allocation macros
******************************************/
/* static allocation of Huff0's DTable */
#define HUF_DTABLE_SIZE(maxTableLog)   (1 + (1<<maxTableLog))  /* nb Cells; use unsigned short for X2, unsigned int for X4 */
#define HUF_CREATE_STATIC_DTABLEX2(DTable, maxTableLog) \
        unsigned short DTable[HUF_DTABLE_SIZE(maxTableLog)] = { maxTableLog }
#define HUF_CREATE_STATIC_DTABLEX4(DTable, maxTableLog) \
        unsigned int DTable[HUF_DTABLE_SIZE(maxTableLog)] = { maxTableLog }
#define HUF_CREATE_STATIC_DTABLEX6(DTable, maxTableLog) \
        unsigned int DTable[HUF_DTABLE_SIZE(maxTableLog) * 3 / 2] = { maxTableLog }


/* ****************************************
*  Advanced decompression functions
******************************************/
static size_t HUF_decompress4X2 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /* single-symbol decoder */
static size_t HUF_decompress4X4 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);   /* double-symbols decoder */


/* ****************************************
*  Huff0 detailed API
******************************************/
/*!
HUF_decompress() does the following:
1. select the decompression algorithm (X2, X4, X6) based on pre-computed heuristics
2. build Huffman table from save, using HUF_readDTableXn()
3. decode 1 or 4 segments in parallel using HUF_decompressSXn_usingDTable

*/
static size_t HUF_readDTableX2 (unsigned short* DTable, const void* src, size_t srcSize);
static size_t HUF_readDTableX4 (unsigned* DTable, const void* src, size_t srcSize);

static size_t HUF_decompress4X2_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const unsigned short* DTable);
static size_t HUF_decompress4X4_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const unsigned* DTable);


#if defined (__cplusplus)
}
#endif

#endif /* HUFF0_STATIC_H */



/* ******************************************************************
   Huff0 : Huffman coder, part of New Generation Entropy library
   Copyright (C) 2013-2015, Yann Collet.

   BSD 2-Clause License (https://opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - FSE+Huff0 source repository : https://github.com/Cyan4973/FiniteStateEntropy
****************************************************************** */

/* **************************************************************
*  Compiler specifics
****************************************************************/
#if defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
/* inline is defined */
#elif defined(_MSC_VER)
#  define inline __inline
#else
#  define inline /* disable inline */
#endif


#ifdef _MSC_VER    /* Visual Studio */
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#endif


/* **************************************************************
*  Includes
****************************************************************/
#include <stdlib.h>     /* malloc, free, qsort */
#include <string.h>     /* memcpy, memset */
#include <stdio.h>      /* printf (debug) */


/* **************************************************************
*  Constants
****************************************************************/
#define HUF_ABSOLUTEMAX_TABLELOG  16   /* absolute limit of HUF_MAX_TABLELOG. Beyond that value, code does not work */
#define HUF_MAX_TABLELOG  12           /* max configured tableLog (for static allocation); can be modified up to HUF_ABSOLUTEMAX_TABLELOG */
#define HUF_DEFAULT_TABLELOG  HUF_MAX_TABLELOG   /* tableLog by default, when not specified */
#define HUF_MAX_SYMBOL_VALUE 255
#if (HUF_MAX_TABLELOG > HUF_ABSOLUTEMAX_TABLELOG)
#  error "HUF_MAX_TABLELOG is too large !"
#endif


/* **************************************************************
*  Error Management
****************************************************************/
static unsigned HUF_isError(size_t code) { return ERR_isError(code); }
#define HUF_STATIC_ASSERT(c) { enum { HUF_static_assert = 1/(int)(!!(c)) }; }   /* use only *after* variable declarations */



/*-*******************************************************
*  Huff0 : Huffman block decompression
*********************************************************/
typedef struct { BYTE byte; BYTE nbBits; } HUF_DEltX2;   /* single-symbol decoding */

typedef struct { U16 sequence; BYTE nbBits; BYTE length; } HUF_DEltX4;  /* double-symbols decoding */

typedef struct { BYTE symbol; BYTE weight; } sortedSymbol_t;

/*! HUF_readStats
    Read compact Huffman tree, saved by HUF_writeCTable
    @huffWeight : destination buffer
    @return : size read from `src`
*/
static size_t HUF_readStats(BYTE* huffWeight, size_t hwSize, U32* rankStats,
                            U32* nbSymbolsPtr, U32* tableLogPtr,
                            const void* src, size_t srcSize)
{
    U32 weightTotal;
    U32 tableLog;
    const BYTE* ip = (const BYTE*) src;
    size_t iSize;
    size_t oSize;
    U32 n;

    if (!srcSize) return ERROR(srcSize_wrong);
    iSize = ip[0];
    //memset(huffWeight, 0, hwSize);   /* is not necessary, even though some analyzer complain ... */

    if (iSize >= 128)  /* special header */
    {
        if (iSize >= (242))   /* RLE */
        {
            static int l[14] = { 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128 };
            oSize = l[iSize-242];
            memset(huffWeight, 1, hwSize);
            iSize = 0;
        }
        else   /* Incompressible */
        {
            oSize = iSize - 127;
            iSize = ((oSize+1)/2);
            if (iSize+1 > srcSize) return ERROR(srcSize_wrong);
            if (oSize >= hwSize) return ERROR(corruption_detected);
            ip += 1;
            for (n=0; n<oSize; n+=2)
            {
                huffWeight[n]   = ip[n/2] >> 4;
                huffWeight[n+1] = ip[n/2] & 15;
            }
        }
    }
    else  /* header compressed with FSE (normal case) */
    {
        if (iSize+1 > srcSize) return ERROR(srcSize_wrong);
        oSize = FSE_decompress(huffWeight, hwSize-1, ip+1, iSize);   /* max (hwSize-1) values decoded, as last one is implied */
        if (FSE_isError(oSize)) return oSize;
    }

    /* collect weight stats */
    memset(rankStats, 0, (HUF_ABSOLUTEMAX_TABLELOG + 1) * sizeof(U32));
    weightTotal = 0;
    for (n=0; n<oSize; n++)
    {
        if (huffWeight[n] >= HUF_ABSOLUTEMAX_TABLELOG) return ERROR(corruption_detected);
        rankStats[huffWeight[n]]++;
        weightTotal += (1 << huffWeight[n]) >> 1;
    }
    if (weightTotal == 0) return ERROR(corruption_detected);

    /* get last non-null symbol weight (implied, total must be 2^n) */
    tableLog = BIT_highbit32(weightTotal) + 1;
    if (tableLog > HUF_ABSOLUTEMAX_TABLELOG) return ERROR(corruption_detected);
    {
        U32 total = 1 << tableLog;
        U32 rest = total - weightTotal;
        U32 verif = 1 << BIT_highbit32(rest);
        U32 lastWeight = BIT_highbit32(rest) + 1;
        if (verif != rest) return ERROR(corruption_detected);    /* last value must be a clean power of 2 */
        huffWeight[oSize] = (BYTE)lastWeight;
        rankStats[lastWeight]++;
    }

    /* check tree construction validity */
    if ((rankStats[1] < 2) || (rankStats[1] & 1)) return ERROR(corruption_detected);   /* by construction : at least 2 elts of rank 1, must be even */

    /* results */
    *nbSymbolsPtr = (U32)(oSize+1);
    *tableLogPtr = tableLog;
    return iSize+1;
}


/**************************/
/* single-symbol decoding */
/**************************/

static size_t HUF_readDTableX2 (U16* DTable, const void* src, size_t srcSize)
{
    BYTE huffWeight[HUF_MAX_SYMBOL_VALUE + 1];
    U32 rankVal[HUF_ABSOLUTEMAX_TABLELOG + 1];   /* large enough for values from 0 to 16 */
    U32 tableLog = 0;
    size_t iSize;
    U32 nbSymbols = 0;
    U32 n;
    U32 nextRankStart;
    void* const dtPtr = DTable + 1;
    HUF_DEltX2* const dt = (HUF_DEltX2*)dtPtr;

    HUF_STATIC_ASSERT(sizeof(HUF_DEltX2) == sizeof(U16));   /* if compilation fails here, assertion is false */
    //memset(huffWeight, 0, sizeof(huffWeight));   /* is not necessary, even though some analyzer complain ... */

    iSize = HUF_readStats(huffWeight, HUF_MAX_SYMBOL_VALUE + 1, rankVal, &nbSymbols, &tableLog, src, srcSize);
    if (HUF_isError(iSize)) return iSize;

    /* check result */
    if (tableLog > DTable[0]) return ERROR(tableLog_tooLarge);   /* DTable is too small */
    DTable[0] = (U16)tableLog;   /* maybe should separate sizeof DTable, as allocated, from used size of DTable, in case of DTable re-use */

    /* Prepare ranks */
    nextRankStart = 0;
    for (n=1; n<=tableLog; n++)
    {
        U32 current = nextRankStart;
        nextRankStart += (rankVal[n] << (n-1));
        rankVal[n] = current;
    }

    /* fill DTable */
    for (n=0; n<nbSymbols; n++)
    {
        const U32 w = huffWeight[n];
        const U32 length = (1 << w) >> 1;
        U32 i;
        HUF_DEltX2 D;
        D.byte = (BYTE)n; D.nbBits = (BYTE)(tableLog + 1 - w);
        for (i = rankVal[w]; i < rankVal[w] + length; i++)
            dt[i] = D;
        rankVal[w] += length;
    }

    return iSize;
}

static BYTE HUF_decodeSymbolX2(BIT_DStream_t* Dstream, const HUF_DEltX2* dt, const U32 dtLog)
{
        const size_t val = BIT_lookBitsFast(Dstream, dtLog); /* note : dtLog >= 1 */
        const BYTE c = dt[val].byte;
        BIT_skipBits(Dstream, dt[val].nbBits);
        return c;
}

#define HUF_DECODE_SYMBOLX2_0(ptr, DStreamPtr) \
    *ptr++ = HUF_decodeSymbolX2(DStreamPtr, dt, dtLog)

#define HUF_DECODE_SYMBOLX2_1(ptr, DStreamPtr) \
    if (MEM_64bits() || (HUF_MAX_TABLELOG<=12)) \
        HUF_DECODE_SYMBOLX2_0(ptr, DStreamPtr)

#define HUF_DECODE_SYMBOLX2_2(ptr, DStreamPtr) \
    if (MEM_64bits()) \
        HUF_DECODE_SYMBOLX2_0(ptr, DStreamPtr)

static inline size_t HUF_decodeStreamX2(BYTE* p, BIT_DStream_t* const bitDPtr, BYTE* const pEnd, const HUF_DEltX2* const dt, const U32 dtLog)
{
    BYTE* const pStart = p;

    /* up to 4 symbols at a time */
    while ((BIT_reloadDStream(bitDPtr) == BIT_DStream_unfinished) && (p <= pEnd-4))
    {
        HUF_DECODE_SYMBOLX2_2(p, bitDPtr);
        HUF_DECODE_SYMBOLX2_1(p, bitDPtr);
        HUF_DECODE_SYMBOLX2_2(p, bitDPtr);
        HUF_DECODE_SYMBOLX2_0(p, bitDPtr);
    }

    /* closer to the end */
    while ((BIT_reloadDStream(bitDPtr) == BIT_DStream_unfinished) && (p < pEnd))
        HUF_DECODE_SYMBOLX2_0(p, bitDPtr);

    /* no more data to retrieve from bitstream, hence no need to reload */
    while (p < pEnd)
        HUF_DECODE_SYMBOLX2_0(p, bitDPtr);

    return pEnd-pStart;
}


static size_t HUF_decompress4X2_usingDTable(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const U16* DTable)
{
    if (cSrcSize < 10) return ERROR(corruption_detected);   /* strict minimum : jump table + 1 byte per stream */

    {
        const BYTE* const istart = (const BYTE*) cSrc;
        BYTE* const ostart = (BYTE*) dst;
        BYTE* const oend = ostart + dstSize;
        const void* const dtPtr = DTable;
        const HUF_DEltX2* const dt = ((const HUF_DEltX2*)dtPtr) +1;
        const U32 dtLog = DTable[0];
        size_t errorCode;

        /* Init */
        BIT_DStream_t bitD1;
        BIT_DStream_t bitD2;
        BIT_DStream_t bitD3;
        BIT_DStream_t bitD4;
        const size_t length1 = MEM_readLE16(istart);
        const size_t length2 = MEM_readLE16(istart+2);
        const size_t length3 = MEM_readLE16(istart+4);
        size_t length4;
        const BYTE* const istart1 = istart + 6;  /* jumpTable */
        const BYTE* const istart2 = istart1 + length1;
        const BYTE* const istart3 = istart2 + length2;
        const BYTE* const istart4 = istart3 + length3;
        const size_t segmentSize = (dstSize+3) / 4;
        BYTE* const opStart2 = ostart + segmentSize;
        BYTE* const opStart3 = opStart2 + segmentSize;
        BYTE* const opStart4 = opStart3 + segmentSize;
        BYTE* op1 = ostart;
        BYTE* op2 = opStart2;
        BYTE* op3 = opStart3;
        BYTE* op4 = opStart4;
        U32 endSignal;

        length4 = cSrcSize - (length1 + length2 + length3 + 6);
        if (length4 > cSrcSize) return ERROR(corruption_detected);   /* overflow */
        errorCode = BIT_initDStream(&bitD1, istart1, length1);
        if (HUF_isError(errorCode)) return errorCode;
        errorCode = BIT_initDStream(&bitD2, istart2, length2);
        if (HUF_isError(errorCode)) return errorCode;
        errorCode = BIT_initDStream(&bitD3, istart3, length3);
        if (HUF_isError(errorCode)) return errorCode;
        errorCode = BIT_initDStream(&bitD4, istart4, length4);
        if (HUF_isError(errorCode)) return errorCode;

        /* 16-32 symbols per loop (4-8 symbols per stream) */
        endSignal = BIT_reloadDStream(&bitD1) | BIT_reloadDStream(&bitD2) | BIT_reloadDStream(&bitD3) | BIT_reloadDStream(&bitD4);
        for ( ; (endSignal==BIT_DStream_unfinished) && (op4<(oend-7)) ; )
        {
            HUF_DECODE_SYMBOLX2_2(op1, &bitD1);
            HUF_DECODE_SYMBOLX2_2(op2, &bitD2);
            HUF_DECODE_SYMBOLX2_2(op3, &bitD3);
            HUF_DECODE_SYMBOLX2_2(op4, &bitD4);
            HUF_DECODE_SYMBOLX2_1(op1, &bitD1);
            HUF_DECODE_SYMBOLX2_1(op2, &bitD2);
            HUF_DECODE_SYMBOLX2_1(op3, &bitD3);
            HUF_DECODE_SYMBOLX2_1(op4, &bitD4);
            HUF_DECODE_SYMBOLX2_2(op1, &bitD1);
            HUF_DECODE_SYMBOLX2_2(op2, &bitD2);
            HUF_DECODE_SYMBOLX2_2(op3, &bitD3);
            HUF_DECODE_SYMBOLX2_2(op4, &bitD4);
            HUF_DECODE_SYMBOLX2_0(op1, &bitD1);
            HUF_DECODE_SYMBOLX2_0(op2, &bitD2);
            HUF_DECODE_SYMBOLX2_0(op3, &bitD3);
            HUF_DECODE_SYMBOLX2_0(op4, &bitD4);

            endSignal = BIT_reloadDStream(&bitD1) | BIT_reloadDStream(&bitD2) | BIT_reloadDStream(&bitD3) | BIT_reloadDStream(&bitD4);
        }

        /* check corruption */
        if (op1 > opStart2) return ERROR(corruption_detected);
        if (op2 > opStart3) return ERROR(corruption_detected);
        if (op3 > opStart4) return ERROR(corruption_detected);
        /* note : op4 supposed already verified within main loop */

        /* finish bitStreams one by one */
        HUF_decodeStreamX2(op1, &bitD1, opStart2, dt, dtLog);
        HUF_decodeStreamX2(op2, &bitD2, opStart3, dt, dtLog);
        HUF_decodeStreamX2(op3, &bitD3, opStart4, dt, dtLog);
        HUF_decodeStreamX2(op4, &bitD4, oend,     dt, dtLog);

        /* check */
        endSignal = BIT_endOfDStream(&bitD1) & BIT_endOfDStream(&bitD2) & BIT_endOfDStream(&bitD3) & BIT_endOfDStream(&bitD4);
        if (!endSignal) return ERROR(corruption_detected);

        /* decoded size */
        return dstSize;
    }
}


static size_t HUF_decompress4X2 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    HUF_CREATE_STATIC_DTABLEX2(DTable, HUF_MAX_TABLELOG);
    const BYTE* ip = (const BYTE*) cSrc;
    size_t errorCode;

    errorCode = HUF_readDTableX2 (DTable, cSrc, cSrcSize);
    if (HUF_isError(errorCode)) return errorCode;
    if (errorCode >= cSrcSize) return ERROR(srcSize_wrong);
    ip += errorCode;
    cSrcSize -= errorCode;

    return HUF_decompress4X2_usingDTable (dst, dstSize, ip, cSrcSize, DTable);
}


/***************************/
/* double-symbols decoding */
/***************************/

static void HUF_fillDTableX4Level2(HUF_DEltX4* DTable, U32 sizeLog, const U32 consumed,
                           const U32* rankValOrigin, const int minWeight,
                           const sortedSymbol_t* sortedSymbols, const U32 sortedListSize,
                           U32 nbBitsBaseline, U16 baseSeq)
{
    HUF_DEltX4 DElt;
    U32 rankVal[HUF_ABSOLUTEMAX_TABLELOG + 1];
    U32 s;

    /* get pre-calculated rankVal */
    memcpy(rankVal, rankValOrigin, sizeof(rankVal));

    /* fill skipped values */
    if (minWeight>1)
    {
        U32 i, skipSize = rankVal[minWeight];
        MEM_writeLE16(&(DElt.sequence), baseSeq);
        DElt.nbBits   = (BYTE)(consumed);
        DElt.length   = 1;
        for (i = 0; i < skipSize; i++)
            DTable[i] = DElt;
    }

    /* fill DTable */
    for (s=0; s<sortedListSize; s++)   /* note : sortedSymbols already skipped */
    {
        const U32 symbol = sortedSymbols[s].symbol;
        const U32 weight = sortedSymbols[s].weight;
        const U32 nbBits = nbBitsBaseline - weight;
        const U32 length = 1 << (sizeLog-nbBits);
        const U32 start = rankVal[weight];
        U32 i = start;
        const U32 end = start + length;

        MEM_writeLE16(&(DElt.sequence), (U16)(baseSeq + (symbol << 8)));
        DElt.nbBits = (BYTE)(nbBits + consumed);
        DElt.length = 2;
        do { DTable[i++] = DElt; } while (i<end);   /* since length >= 1 */

        rankVal[weight] += length;
    }
}

typedef U32 rankVal_t[HUF_ABSOLUTEMAX_TABLELOG][HUF_ABSOLUTEMAX_TABLELOG + 1];

static void HUF_fillDTableX4(HUF_DEltX4* DTable, const U32 targetLog,
                           const sortedSymbol_t* sortedList, const U32 sortedListSize,
                           const U32* rankStart, rankVal_t rankValOrigin, const U32 maxWeight,
                           const U32 nbBitsBaseline)
{
    U32 rankVal[HUF_ABSOLUTEMAX_TABLELOG + 1];
    const int scaleLog = nbBitsBaseline - targetLog;   /* note : targetLog >= srcLog, hence scaleLog <= 1 */
    const U32 minBits  = nbBitsBaseline - maxWeight;
    U32 s;

    memcpy(rankVal, rankValOrigin, sizeof(rankVal));

    /* fill DTable */
    for (s=0; s<sortedListSize; s++)
    {
        const U16 symbol = sortedList[s].symbol;
        const U32 weight = sortedList[s].weight;
        const U32 nbBits = nbBitsBaseline - weight;
        const U32 start = rankVal[weight];
        const U32 length = 1 << (targetLog-nbBits);

        if (targetLog-nbBits >= minBits)   /* enough room for a second symbol */
        {
            U32 sortedRank;
            int minWeight = nbBits + scaleLog;
            if (minWeight < 1) minWeight = 1;
            sortedRank = rankStart[minWeight];
            HUF_fillDTableX4Level2(DTable+start, targetLog-nbBits, nbBits,
                           rankValOrigin[nbBits], minWeight,
                           sortedList+sortedRank, sortedListSize-sortedRank,
                           nbBitsBaseline, symbol);
        }
        else
        {
            U32 i;
            const U32 end = start + length;
            HUF_DEltX4 DElt;

            MEM_writeLE16(&(DElt.sequence), symbol);
            DElt.nbBits   = (BYTE)(nbBits);
            DElt.length   = 1;
            for (i = start; i < end; i++)
                DTable[i] = DElt;
        }
        rankVal[weight] += length;
    }
}

static size_t HUF_readDTableX4 (U32* DTable, const void* src, size_t srcSize)
{
    BYTE weightList[HUF_MAX_SYMBOL_VALUE + 1];
    sortedSymbol_t sortedSymbol[HUF_MAX_SYMBOL_VALUE + 1];
    U32 rankStats[HUF_ABSOLUTEMAX_TABLELOG + 1] = { 0 };
    U32 rankStart0[HUF_ABSOLUTEMAX_TABLELOG + 2] = { 0 };
    U32* const rankStart = rankStart0+1;
    rankVal_t rankVal;
    U32 tableLog, maxW, sizeOfSort, nbSymbols;
    const U32 memLog = DTable[0];
    size_t iSize;
    void* dtPtr = DTable;
    HUF_DEltX4* const dt = ((HUF_DEltX4*)dtPtr) + 1;

    HUF_STATIC_ASSERT(sizeof(HUF_DEltX4) == sizeof(U32));   /* if compilation fails here, assertion is false */
    if (memLog > HUF_ABSOLUTEMAX_TABLELOG) return ERROR(tableLog_tooLarge);
    //memset(weightList, 0, sizeof(weightList));   /* is not necessary, even though some analyzer complain ... */

    iSize = HUF_readStats(weightList, HUF_MAX_SYMBOL_VALUE + 1, rankStats, &nbSymbols, &tableLog, src, srcSize);
    if (HUF_isError(iSize)) return iSize;

    /* check result */
    if (tableLog > memLog) return ERROR(tableLog_tooLarge);   /* DTable can't fit code depth */

    /* find maxWeight */
    for (maxW = tableLog; rankStats[maxW]==0; maxW--)
        { if (!maxW) return ERROR(GENERIC); }  /* necessarily finds a solution before maxW==0 */

    /* Get start index of each weight */
    {
        U32 w, nextRankStart = 0;
        for (w=1; w<=maxW; w++)
        {
            U32 current = nextRankStart;
            nextRankStart += rankStats[w];
            rankStart[w] = current;
        }
        rankStart[0] = nextRankStart;   /* put all 0w symbols at the end of sorted list*/
        sizeOfSort = nextRankStart;
    }

    /* sort symbols by weight */
    {
        U32 s;
        for (s=0; s<nbSymbols; s++)
        {
            U32 w = weightList[s];
            U32 r = rankStart[w]++;
            sortedSymbol[r].symbol = (BYTE)s;
            sortedSymbol[r].weight = (BYTE)w;
        }
        rankStart[0] = 0;   /* forget 0w symbols; this is beginning of weight(1) */
    }

    /* Build rankVal */
    {
        const U32 minBits = tableLog+1 - maxW;
        U32 nextRankVal = 0;
        U32 w, consumed;
        const int rescale = (memLog-tableLog) - 1;   /* tableLog <= memLog */
        U32* rankVal0 = rankVal[0];
        for (w=1; w<=maxW; w++)
        {
            U32 current = nextRankVal;
            nextRankVal += rankStats[w] << (w+rescale);
            rankVal0[w] = current;
        }
        for (consumed = minBits; consumed <= memLog - minBits; consumed++)
        {
            U32* rankValPtr = rankVal[consumed];
            for (w = 1; w <= maxW; w++)
            {
                rankValPtr[w] = rankVal0[w] >> consumed;
            }
        }
    }

    HUF_fillDTableX4(dt, memLog,
                   sortedSymbol, sizeOfSort,
                   rankStart0, rankVal, maxW,
                   tableLog+1);

    return iSize;
}


static U32 HUF_decodeSymbolX4(void* op, BIT_DStream_t* DStream, const HUF_DEltX4* dt, const U32 dtLog)
{
    const size_t val = BIT_lookBitsFast(DStream, dtLog);   /* note : dtLog >= 1 */
    memcpy(op, dt+val, 2);
    BIT_skipBits(DStream, dt[val].nbBits);
    return dt[val].length;
}

static U32 HUF_decodeLastSymbolX4(void* op, BIT_DStream_t* DStream, const HUF_DEltX4* dt, const U32 dtLog)
{
    const size_t val = BIT_lookBitsFast(DStream, dtLog);   /* note : dtLog >= 1 */
    memcpy(op, dt+val, 1);
    if (dt[val].length==1) BIT_skipBits(DStream, dt[val].nbBits);
    else
    {
        if (DStream->bitsConsumed < (sizeof(DStream->bitContainer)*8))
        {
            BIT_skipBits(DStream, dt[val].nbBits);
            if (DStream->bitsConsumed > (sizeof(DStream->bitContainer)*8))
                DStream->bitsConsumed = (sizeof(DStream->bitContainer)*8);   /* ugly hack; works only because it's the last symbol. Note : can't easily extract nbBits from just this symbol */
        }
    }
    return 1;
}


#define HUF_DECODE_SYMBOLX4_0(ptr, DStreamPtr) \
    ptr += HUF_decodeSymbolX4(ptr, DStreamPtr, dt, dtLog)

#define HUF_DECODE_SYMBOLX4_1(ptr, DStreamPtr) \
    if (MEM_64bits() || (HUF_MAX_TABLELOG<=12)) \
        ptr += HUF_decodeSymbolX4(ptr, DStreamPtr, dt, dtLog)

#define HUF_DECODE_SYMBOLX4_2(ptr, DStreamPtr) \
    if (MEM_64bits()) \
        ptr += HUF_decodeSymbolX4(ptr, DStreamPtr, dt, dtLog)

static inline size_t HUF_decodeStreamX4(BYTE* p, BIT_DStream_t* bitDPtr, BYTE* const pEnd, const HUF_DEltX4* const dt, const U32 dtLog)
{
    BYTE* const pStart = p;

    /* up to 8 symbols at a time */
    while ((BIT_reloadDStream(bitDPtr) == BIT_DStream_unfinished) && (p < pEnd-7))
    {
        HUF_DECODE_SYMBOLX4_2(p, bitDPtr);
        HUF_DECODE_SYMBOLX4_1(p, bitDPtr);
        HUF_DECODE_SYMBOLX4_2(p, bitDPtr);
        HUF_DECODE_SYMBOLX4_0(p, bitDPtr);
    }

    /* closer to the end */
    while ((BIT_reloadDStream(bitDPtr) == BIT_DStream_unfinished) && (p <= pEnd-2))
        HUF_DECODE_SYMBOLX4_0(p, bitDPtr);

    while (p <= pEnd-2)
        HUF_DECODE_SYMBOLX4_0(p, bitDPtr);   /* no need to reload : reached the end of DStream */

    if (p < pEnd)
        p += HUF_decodeLastSymbolX4(p, bitDPtr, dt, dtLog);

    return p-pStart;
}

static size_t HUF_decompress4X4_usingDTable(
          void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const U32* DTable)
{
    if (cSrcSize < 10) return ERROR(corruption_detected);   /* strict minimum : jump table + 1 byte per stream */

    {
        const BYTE* const istart = (const BYTE*) cSrc;
        BYTE* const ostart = (BYTE*) dst;
        BYTE* const oend = ostart + dstSize;
        const void* const dtPtr = DTable;
        const HUF_DEltX4* const dt = ((const HUF_DEltX4*)dtPtr) +1;
        const U32 dtLog = DTable[0];
        size_t errorCode;

        /* Init */
        BIT_DStream_t bitD1;
        BIT_DStream_t bitD2;
        BIT_DStream_t bitD3;
        BIT_DStream_t bitD4;
        const size_t length1 = MEM_readLE16(istart);
        const size_t length2 = MEM_readLE16(istart+2);
        const size_t length3 = MEM_readLE16(istart+4);
        size_t length4;
        const BYTE* const istart1 = istart + 6;  /* jumpTable */
        const BYTE* const istart2 = istart1 + length1;
        const BYTE* const istart3 = istart2 + length2;
        const BYTE* const istart4 = istart3 + length3;
        const size_t segmentSize = (dstSize+3) / 4;
        BYTE* const opStart2 = ostart + segmentSize;
        BYTE* const opStart3 = opStart2 + segmentSize;
        BYTE* const opStart4 = opStart3 + segmentSize;
        BYTE* op1 = ostart;
        BYTE* op2 = opStart2;
        BYTE* op3 = opStart3;
        BYTE* op4 = opStart4;
        U32 endSignal;

        length4 = cSrcSize - (length1 + length2 + length3 + 6);
        if (length4 > cSrcSize) return ERROR(corruption_detected);   /* overflow */
        errorCode = BIT_initDStream(&bitD1, istart1, length1);
        if (HUF_isError(errorCode)) return errorCode;
        errorCode = BIT_initDStream(&bitD2, istart2, length2);
        if (HUF_isError(errorCode)) return errorCode;
        errorCode = BIT_initDStream(&bitD3, istart3, length3);
        if (HUF_isError(errorCode)) return errorCode;
        errorCode = BIT_initDStream(&bitD4, istart4, length4);
        if (HUF_isError(errorCode)) return errorCode;

        /* 16-32 symbols per loop (4-8 symbols per stream) */
        endSignal = BIT_reloadDStream(&bitD1) | BIT_reloadDStream(&bitD2) | BIT_reloadDStream(&bitD3) | BIT_reloadDStream(&bitD4);
        for ( ; (endSignal==BIT_DStream_unfinished) && (op4<(oend-7)) ; )
        {
            HUF_DECODE_SYMBOLX4_2(op1, &bitD1);
            HUF_DECODE_SYMBOLX4_2(op2, &bitD2);
            HUF_DECODE_SYMBOLX4_2(op3, &bitD3);
            HUF_DECODE_SYMBOLX4_2(op4, &bitD4);
            HUF_DECODE_SYMBOLX4_1(op1, &bitD1);
            HUF_DECODE_SYMBOLX4_1(op2, &bitD2);
            HUF_DECODE_SYMBOLX4_1(op3, &bitD3);
            HUF_DECODE_SYMBOLX4_1(op4, &bitD4);
            HUF_DECODE_SYMBOLX4_2(op1, &bitD1);
            HUF_DECODE_SYMBOLX4_2(op2, &bitD2);
            HUF_DECODE_SYMBOLX4_2(op3, &bitD3);
            HUF_DECODE_SYMBOLX4_2(op4, &bitD4);
            HUF_DECODE_SYMBOLX4_0(op1, &bitD1);
            HUF_DECODE_SYMBOLX4_0(op2, &bitD2);
            HUF_DECODE_SYMBOLX4_0(op3, &bitD3);
            HUF_DECODE_SYMBOLX4_0(op4, &bitD4);

            endSignal = BIT_reloadDStream(&bitD1) | BIT_reloadDStream(&bitD2) | BIT_reloadDStream(&bitD3) | BIT_reloadDStream(&bitD4);
        }

        /* check corruption */
        if (op1 > opStart2) return ERROR(corruption_detected);
        if (op2 > opStart3) return ERROR(corruption_detected);
        if (op3 > opStart4) return ERROR(corruption_detected);
        /* note : op4 supposed already verified within main loop */

        /* finish bitStreams one by one */
        HUF_decodeStreamX4(op1, &bitD1, opStart2, dt, dtLog);
        HUF_decodeStreamX4(op2, &bitD2, opStart3, dt, dtLog);
        HUF_decodeStreamX4(op3, &bitD3, opStart4, dt, dtLog);
        HUF_decodeStreamX4(op4, &bitD4, oend,     dt, dtLog);

        /* check */
        endSignal = BIT_endOfDStream(&bitD1) & BIT_endOfDStream(&bitD2) & BIT_endOfDStream(&bitD3) & BIT_endOfDStream(&bitD4);
        if (!endSignal) return ERROR(corruption_detected);

        /* decoded size */
        return dstSize;
    }
}


static size_t HUF_decompress4X4 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    HUF_CREATE_STATIC_DTABLEX4(DTable, HUF_MAX_TABLELOG);
    const BYTE* ip = (const BYTE*) cSrc;

    size_t hSize = HUF_readDTableX4 (DTable, cSrc, cSrcSize);
    if (HUF_isError(hSize)) return hSize;
    if (hSize >= cSrcSize) return ERROR(srcSize_wrong);
    ip += hSize;
    cSrcSize -= hSize;

    return HUF_decompress4X4_usingDTable (dst, dstSize, ip, cSrcSize, DTable);
}


/**********************************/
/* Generic decompression selector */
/**********************************/

typedef struct { U32 tableTime; U32 decode256Time; } algo_time_t;
static const algo_time_t algoTime[16 /* Quantization */][3 /* single, double, quad */] =
{
    /* single, double, quad */
    {{0,0}, {1,1}, {2,2}},  /* Q==0 : impossible */
    {{0,0}, {1,1}, {2,2}},  /* Q==1 : impossible */
    {{  38,130}, {1313, 74}, {2151, 38}},   /* Q == 2 : 12-18% */
    {{ 448,128}, {1353, 74}, {2238, 41}},   /* Q == 3 : 18-25% */
    {{ 556,128}, {1353, 74}, {2238, 47}},   /* Q == 4 : 25-32% */
    {{ 714,128}, {1418, 74}, {2436, 53}},   /* Q == 5 : 32-38% */
    {{ 883,128}, {1437, 74}, {2464, 61}},   /* Q == 6 : 38-44% */
    {{ 897,128}, {1515, 75}, {2622, 68}},   /* Q == 7 : 44-50% */
    {{ 926,128}, {1613, 75}, {2730, 75}},   /* Q == 8 : 50-56% */
    {{ 947,128}, {1729, 77}, {3359, 77}},   /* Q == 9 : 56-62% */
    {{1107,128}, {2083, 81}, {4006, 84}},   /* Q ==10 : 62-69% */
    {{1177,128}, {2379, 87}, {4785, 88}},   /* Q ==11 : 69-75% */
    {{1242,128}, {2415, 93}, {5155, 84}},   /* Q ==12 : 75-81% */
    {{1349,128}, {2644,106}, {5260,106}},   /* Q ==13 : 81-87% */
    {{1455,128}, {2422,124}, {4174,124}},   /* Q ==14 : 87-93% */
    {{ 722,128}, {1891,145}, {1936,146}},   /* Q ==15 : 93-99% */
};

typedef size_t (*decompressionAlgo)(void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);

static size_t HUF_decompress (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
    static const decompressionAlgo decompress[3] = { HUF_decompress4X2, HUF_decompress4X4, NULL };
    /* estimate decompression time */
    U32 Q;
    const U32 D256 = (U32)(dstSize >> 8);
    U32 Dtime[3];
    U32 algoNb = 0;
    int n;

    /* validation checks */
    if (dstSize == 0) return ERROR(dstSize_tooSmall);
    if (cSrcSize > dstSize) return ERROR(corruption_detected);   /* invalid */
    if (cSrcSize == dstSize) { memcpy(dst, cSrc, dstSize); return dstSize; }   /* not compressed */
    if (cSrcSize == 1) { memset(dst, *(const BYTE*)cSrc, dstSize); return dstSize; }   /* RLE */

    /* decoder timing evaluation */
    Q = (U32)(cSrcSize * 16 / dstSize);   /* Q < 16 since dstSize > cSrcSize */
    for (n=0; n<3; n++)
        Dtime[n] = algoTime[Q][n].tableTime + (algoTime[Q][n].decode256Time * D256);

    Dtime[1] += Dtime[1] >> 4; Dtime[2] += Dtime[2] >> 3; /* advantage to algorithms using less memory, for cache eviction */

    if (Dtime[1] < Dtime[0]) algoNb = 1;

    return decompress[algoNb](dst, dstSize, cSrc, cSrcSize);

    //return HUF_decompress4X2(dst, dstSize, cSrc, cSrcSize);   /* multi-streams single-symbol decoding */
    //return HUF_decompress4X4(dst, dstSize, cSrc, cSrcSize);   /* multi-streams double-symbols decoding */
    //return HUF_decompress4X6(dst, dstSize, cSrc, cSrcSize);   /* multi-streams quad-symbols decoding */
}



#endif   /* ZSTD_CCOMMON_H_MODULE */


/*
    zstd - decompression module fo v0.4 legacy format
    Copyright (C) 2015-2016, Yann Collet.

    BSD 2-Clause License (https://opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - zstd source repository : https://github.com/Cyan4973/zstd
    - ztsd public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* ***************************************************************
*  Tuning parameters
*****************************************************************/
/*!
 * HEAPMODE :
 * Select how default decompression function ZSTD_decompress() will allocate memory,
 * in memory stack (0), or in memory heap (1, requires malloc())
 */
#ifndef ZSTD_HEAPMODE
#  define ZSTD_HEAPMODE 1
#endif


/* *******************************************************
*  Includes
*********************************************************/
#include <stdlib.h>      /* calloc */
#include <string.h>      /* memcpy, memmove */
#include <stdio.h>       /* debug : printf */


/* *******************************************************
*  Compiler specifics
*********************************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  include <intrin.h>                    /* For Visual 2005 */
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4324)        /* disable: C4324: padded structure */
#endif


/* *************************************
*  Local types
***************************************/
typedef struct
{
    blockType_t blockType;
    U32 origSize;
} blockProperties_t;


/* *******************************************************
*  Memory operations
**********************************************************/
static void ZSTD_copy4(void* dst, const void* src) { memcpy(dst, src, 4); }


/* *************************************
*  Error Management
***************************************/

/*! ZSTD_isError
*   tells if a return value is an error code */
static unsigned ZSTD_isError(size_t code) { return ERR_isError(code); }


/* *************************************************************
*   Context management
***************************************************************/
typedef enum { ZSTDds_getFrameHeaderSize, ZSTDds_decodeFrameHeader,
               ZSTDds_decodeBlockHeader, ZSTDds_decompressBlock } ZSTD_dStage;

struct ZSTDv04_Dctx_s
{
    U32 LLTable[FSE_DTABLE_SIZE_U32(LLFSELog)];
    U32 OffTable[FSE_DTABLE_SIZE_U32(OffFSELog)];
    U32 MLTable[FSE_DTABLE_SIZE_U32(MLFSELog)];
    const void* previousDstEnd;
    const void* base;
    const void* vBase;
    const void* dictEnd;
    size_t expected;
    size_t headerSize;
    ZSTD_parameters params;
    blockType_t bType;
    ZSTD_dStage stage;
    const BYTE* litPtr;
    size_t litSize;
    BYTE litBuffer[BLOCKSIZE + 8 /* margin for wildcopy */];
    BYTE headerBuffer[ZSTD_frameHeaderSize_max];
};  /* typedef'd to ZSTD_DCtx within "zstd_static.h" */

static size_t ZSTD_resetDCtx(ZSTD_DCtx* dctx)
{
    dctx->expected = ZSTD_frameHeaderSize_min;
    dctx->stage = ZSTDds_getFrameHeaderSize;
    dctx->previousDstEnd = NULL;
    dctx->base = NULL;
    dctx->vBase = NULL;
    dctx->dictEnd = NULL;
    return 0;
}

static ZSTD_DCtx* ZSTD_createDCtx(void)
{
    ZSTD_DCtx* dctx = (ZSTD_DCtx*)malloc(sizeof(ZSTD_DCtx));
    if (dctx==NULL) return NULL;
    ZSTD_resetDCtx(dctx);
    return dctx;
}

static size_t ZSTD_freeDCtx(ZSTD_DCtx* dctx)
{
    free(dctx);
    return 0;
}


/* *************************************************************
*   Decompression section
***************************************************************/
/** ZSTD_decodeFrameHeader_Part1
*   decode the 1st part of the Frame Header, which tells Frame Header size.
*   srcSize must be == ZSTD_frameHeaderSize_min
*   @return : the full size of the Frame Header */
static size_t ZSTD_decodeFrameHeader_Part1(ZSTD_DCtx* zc, const void* src, size_t srcSize)
{
    U32 magicNumber;
    if (srcSize != ZSTD_frameHeaderSize_min) return ERROR(srcSize_wrong);
    magicNumber = MEM_readLE32(src);
    if (magicNumber != ZSTD_MAGICNUMBER) return ERROR(prefix_unknown);
    zc->headerSize = ZSTD_frameHeaderSize_min;
    return zc->headerSize;
}


static size_t ZSTD_getFrameParams(ZSTD_parameters* params, const void* src, size_t srcSize)
{
    U32 magicNumber;
    if (srcSize < ZSTD_frameHeaderSize_min) return ZSTD_frameHeaderSize_max;
    magicNumber = MEM_readLE32(src);
    if (magicNumber != ZSTD_MAGICNUMBER) return ERROR(prefix_unknown);
    memset(params, 0, sizeof(*params));
    params->windowLog = (((const BYTE*)src)[4] & 15) + ZSTD_WINDOWLOG_ABSOLUTEMIN;
    if ((((const BYTE*)src)[4] >> 4) != 0) return ERROR(frameParameter_unsupported);   /* reserved bits */
    return 0;
}

/** ZSTD_decodeFrameHeader_Part2
*   decode the full Frame Header
*   srcSize must be the size provided by ZSTD_decodeFrameHeader_Part1
*   @return : 0, or an error code, which can be tested using ZSTD_isError() */
static size_t ZSTD_decodeFrameHeader_Part2(ZSTD_DCtx* zc, const void* src, size_t srcSize)
{
    size_t result;
    if (srcSize != zc->headerSize) return ERROR(srcSize_wrong);
    result = ZSTD_getFrameParams(&(zc->params), src, srcSize);
    if ((MEM_32bits()) && (zc->params.windowLog > 25)) return ERROR(frameParameter_unsupported);
    return result;
}


static size_t ZSTD_getcBlockSize(const void* src, size_t srcSize, blockProperties_t* bpPtr)
{
    const BYTE* const in = (const BYTE* const)src;
    BYTE headerFlags;
    U32 cSize;

    if (srcSize < 3) return ERROR(srcSize_wrong);

    headerFlags = *in;
    cSize = in[2] + (in[1]<<8) + ((in[0] & 7)<<16);

    bpPtr->blockType = (blockType_t)(headerFlags >> 6);
    bpPtr->origSize = (bpPtr->blockType == bt_rle) ? cSize : 0;

    if (bpPtr->blockType == bt_end) return 0;
    if (bpPtr->blockType == bt_rle) return 1;
    return cSize;
}

static size_t ZSTD_copyRawBlock(void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    if (srcSize > maxDstSize) return ERROR(dstSize_tooSmall);
    if (srcSize > 0) {
        memcpy(dst, src, srcSize);
    }
    return srcSize;
}


/** ZSTD_decompressLiterals
    @return : nb of bytes read from src, or an error code*/
static size_t ZSTD_decompressLiterals(void* dst, size_t* maxDstSizePtr,
                                const void* src, size_t srcSize)
{
    const BYTE* ip = (const BYTE*)src;

    const size_t litSize = (MEM_readLE32(src) & 0x1FFFFF) >> 2;   /* no buffer issue : srcSize >= MIN_CBLOCK_SIZE */
    const size_t litCSize = (MEM_readLE32(ip+2) & 0xFFFFFF) >> 5;   /* no buffer issue : srcSize >= MIN_CBLOCK_SIZE */

    if (litSize > *maxDstSizePtr) return ERROR(corruption_detected);
    if (litCSize + 5 > srcSize) return ERROR(corruption_detected);

    if (HUF_isError(HUF_decompress(dst, litSize, ip+5, litCSize))) return ERROR(corruption_detected);

    *maxDstSizePtr = litSize;
    return litCSize + 5;
}


/** ZSTD_decodeLiteralsBlock
    @return : nb of bytes read from src (< srcSize ) */
static size_t ZSTD_decodeLiteralsBlock(ZSTD_DCtx* dctx,
                          const void* src, size_t srcSize)   /* note : srcSize < BLOCKSIZE */
{
    const BYTE* const istart = (const BYTE*) src;

    /* any compressed block with literals segment must be at least this size */
    if (srcSize < MIN_CBLOCK_SIZE) return ERROR(corruption_detected);

    switch(*istart & 3)
    {
    /* compressed */
    case 0:
        {
            size_t litSize = BLOCKSIZE;
            const size_t readSize = ZSTD_decompressLiterals(dctx->litBuffer, &litSize, src, srcSize);
            dctx->litPtr = dctx->litBuffer;
            dctx->litSize = litSize;
            memset(dctx->litBuffer + dctx->litSize, 0, 8);
            return readSize;   /* works if it's an error too */
        }
    case IS_RAW:
        {
            const size_t litSize = (MEM_readLE32(istart) & 0xFFFFFF) >> 2;   /* no buffer issue : srcSize >= MIN_CBLOCK_SIZE */
            if (litSize > srcSize-11)   /* risk of reading too far with wildcopy */
            {
                if (litSize > BLOCKSIZE) return ERROR(corruption_detected);
                if (litSize > srcSize-3) return ERROR(corruption_detected);
                memcpy(dctx->litBuffer, istart, litSize);
                dctx->litPtr = dctx->litBuffer;
                dctx->litSize = litSize;
                memset(dctx->litBuffer + dctx->litSize, 0, 8);
                return litSize+3;
            }
            /* direct reference into compressed stream */
            dctx->litPtr = istart+3;
            dctx->litSize = litSize;
            return litSize+3;        }
    case IS_RLE:
        {
            const size_t litSize = (MEM_readLE32(istart) & 0xFFFFFF) >> 2;   /* no buffer issue : srcSize >= MIN_CBLOCK_SIZE */
            if (litSize > BLOCKSIZE) return ERROR(corruption_detected);
            memset(dctx->litBuffer, istart[3], litSize + 8);
            dctx->litPtr = dctx->litBuffer;
            dctx->litSize = litSize;
            return 4;
        }
    default:
        return ERROR(corruption_detected);   /* forbidden nominal case */
    }
}


static size_t ZSTD_decodeSeqHeaders(int* nbSeq, const BYTE** dumpsPtr, size_t* dumpsLengthPtr,
                         FSE_DTable* DTableLL, FSE_DTable* DTableML, FSE_DTable* DTableOffb,
                         const void* src, size_t srcSize)
{
    const BYTE* const istart = (const BYTE* const)src;
    const BYTE* ip = istart;
    const BYTE* const iend = istart + srcSize;
    U32 LLtype, Offtype, MLtype;
    U32 LLlog, Offlog, MLlog;
    size_t dumpsLength;

    /* check */
    if (srcSize < 5) return ERROR(srcSize_wrong);

    /* SeqHead */
    *nbSeq = MEM_readLE16(ip); ip+=2;
    LLtype  = *ip >> 6;
    Offtype = (*ip >> 4) & 3;
    MLtype  = (*ip >> 2) & 3;
    if (*ip & 2)
    {
        dumpsLength  = ip[2];
        dumpsLength += ip[1] << 8;
        ip += 3;
    }
    else
    {
        dumpsLength  = ip[1];
        dumpsLength += (ip[0] & 1) << 8;
        ip += 2;
    }
    *dumpsPtr = ip;
    ip += dumpsLength;
    *dumpsLengthPtr = dumpsLength;

    /* check */
    if (ip > iend-3) return ERROR(srcSize_wrong); /* min : all 3 are "raw", hence no header, but at least xxLog bits per type */

    /* sequences */
    {
        S16 norm[MaxML+1];    /* assumption : MaxML >= MaxLL >= MaxOff */
        size_t headerSize;

        /* Build DTables */
        switch(LLtype)
        {
        case bt_rle :
            LLlog = 0;
            FSE_buildDTable_rle(DTableLL, *ip++); break;
        case bt_raw :
            LLlog = LLbits;
            FSE_buildDTable_raw(DTableLL, LLbits); break;
        default :
            {   U32 max = MaxLL;
                headerSize = FSE_readNCount(norm, &max, &LLlog, ip, iend-ip);
                if (FSE_isError(headerSize)) return ERROR(GENERIC);
                if (LLlog > LLFSELog) return ERROR(corruption_detected);
                ip += headerSize;
                FSE_buildDTable(DTableLL, norm, max, LLlog);
        }   }

        switch(Offtype)
        {
        case bt_rle :
            Offlog = 0;
            if (ip > iend-2) return ERROR(srcSize_wrong);   /* min : "raw", hence no header, but at least xxLog bits */
            FSE_buildDTable_rle(DTableOffb, *ip++ & MaxOff); /* if *ip > MaxOff, data is corrupted */
            break;
        case bt_raw :
            Offlog = Offbits;
            FSE_buildDTable_raw(DTableOffb, Offbits); break;
        default :
            {   U32 max = MaxOff;
                headerSize = FSE_readNCount(norm, &max, &Offlog, ip, iend-ip);
                if (FSE_isError(headerSize)) return ERROR(GENERIC);
                if (Offlog > OffFSELog) return ERROR(corruption_detected);
                ip += headerSize;
                FSE_buildDTable(DTableOffb, norm, max, Offlog);
        }   }

        switch(MLtype)
        {
        case bt_rle :
            MLlog = 0;
            if (ip > iend-2) return ERROR(srcSize_wrong); /* min : "raw", hence no header, but at least xxLog bits */
            FSE_buildDTable_rle(DTableML, *ip++); break;
        case bt_raw :
            MLlog = MLbits;
            FSE_buildDTable_raw(DTableML, MLbits); break;
        default :
            {   U32 max = MaxML;
                headerSize = FSE_readNCount(norm, &max, &MLlog, ip, iend-ip);
                if (FSE_isError(headerSize)) return ERROR(GENERIC);
                if (MLlog > MLFSELog) return ERROR(corruption_detected);
                ip += headerSize;
                FSE_buildDTable(DTableML, norm, max, MLlog);
    }   }   }

    return ip-istart;
}


typedef struct {
    size_t litLength;
    size_t offset;
    size_t matchLength;
} seq_t;

typedef struct {
    BIT_DStream_t DStream;
    FSE_DState_t stateLL;
    FSE_DState_t stateOffb;
    FSE_DState_t stateML;
    size_t prevOffset;
    const BYTE* dumps;
    const BYTE* dumpsEnd;
} seqState_t;


static void ZSTD_decodeSequence(seq_t* seq, seqState_t* seqState)
{
    size_t litLength;
    size_t prevOffset;
    size_t offset;
    size_t matchLength;
    const BYTE* dumps = seqState->dumps;
    const BYTE* const de = seqState->dumpsEnd;

    /* Literal length */
    litLength = FSE_decodeSymbol(&(seqState->stateLL), &(seqState->DStream));
    prevOffset = litLength ? seq->offset : seqState->prevOffset;
    if (litLength == MaxLL) {
        const U32 add = dumps<de ? *dumps++ : 0;
        if (add < 255) litLength += add;
        else if (dumps + 3 <= de) {
            litLength = MEM_readLE24(dumps);
            dumps += 3;
        }
        if (dumps >= de) { dumps = de-1; }  /* late correction, to avoid read overflow (data is now corrupted anyway) */
    }

    /* Offset */
    {   static const U32 offsetPrefix[MaxOff+1] = {
                1 /*fake*/, 1, 2, 4, 8, 16, 32, 64, 128, 256,
                512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144,
                524288, 1048576, 2097152, 4194304, 8388608, 16777216, 33554432, /*fake*/ 1, 1, 1, 1, 1 };
        U32 offsetCode, nbBits;
        offsetCode = FSE_decodeSymbol(&(seqState->stateOffb), &(seqState->DStream));   /* <= maxOff, by table construction */
        if (MEM_32bits()) BIT_reloadDStream(&(seqState->DStream));
        nbBits = offsetCode - 1;
        if (offsetCode==0) nbBits = 0;   /* cmove */
        offset = offsetPrefix[offsetCode] + BIT_readBits(&(seqState->DStream), nbBits);
        if (MEM_32bits()) BIT_reloadDStream(&(seqState->DStream));
        if (offsetCode==0) offset = prevOffset;   /* cmove */
        if (offsetCode | !litLength) seqState->prevOffset = seq->offset;   /* cmove */
    }

    /* MatchLength */
    matchLength = FSE_decodeSymbol(&(seqState->stateML), &(seqState->DStream));
    if (matchLength == MaxML) {
        const U32 add = dumps<de ? *dumps++ : 0;
        if (add < 255) matchLength += add;
        else if (dumps + 3 <= de){
            matchLength = MEM_readLE24(dumps);
            dumps += 3;
        }
        if (dumps >= de) { dumps = de-1; }  /* late correction, to avoid read overflow (data is now corrupted anyway) */
    }
    matchLength += MINMATCH;

    /* save result */
    seq->litLength = litLength;
    seq->offset = offset;
    seq->matchLength = matchLength;
    seqState->dumps = dumps;
}


static size_t ZSTD_execSequence(BYTE* op,
                                BYTE* const oend, seq_t sequence,
                                const BYTE** litPtr, const BYTE* const litLimit,
                                const BYTE* const base, const BYTE* const vBase, const BYTE* const dictEnd)
{
    static const int dec32table[] = { 0, 1, 2, 1, 4, 4, 4, 4 };   /* added */
    static const int dec64table[] = { 8, 8, 8, 7, 8, 9,10,11 };   /* subtracted */
    BYTE* const oLitEnd = op + sequence.litLength;
    const size_t sequenceLength = sequence.litLength + sequence.matchLength;
    BYTE* const oMatchEnd = op + sequenceLength;   /* risk : address space overflow (32-bits) */
    BYTE* const oend_8 = oend-8;
    const BYTE* const litEnd = *litPtr + sequence.litLength;
    const BYTE* match = oLitEnd - sequence.offset;

    /* checks */
    size_t const seqLength = sequence.litLength + sequence.matchLength;

    if (seqLength > (size_t)(oend - op)) return ERROR(dstSize_tooSmall);
    if (sequence.litLength > (size_t)(litLimit - *litPtr)) return ERROR(corruption_detected);
    /* Now we know there are no overflow in literal nor match lengths, can use pointer checks */
    if (oLitEnd > oend_8) return ERROR(dstSize_tooSmall);

    if (oMatchEnd > oend) return ERROR(dstSize_tooSmall);   /* overwrite beyond dst buffer */
    if (litEnd > litLimit) return ERROR(corruption_detected);   /* overRead beyond lit buffer */

    /* copy Literals */
    ZSTD_wildcopy(op, *litPtr, (ptrdiff_t)sequence.litLength);   /* note : oLitEnd <= oend-8 : no risk of overwrite beyond oend */
    op = oLitEnd;
    *litPtr = litEnd;   /* update for next sequence */

    /* copy Match */
    if (sequence.offset > (size_t)(oLitEnd - base))
    {
        /* offset beyond prefix */
        if (sequence.offset > (size_t)(oLitEnd - vBase))
            return ERROR(corruption_detected);
        match = dictEnd - (base-match);
        if (match + sequence.matchLength <= dictEnd)
        {
            memmove(oLitEnd, match, sequence.matchLength);
            return sequenceLength;
        }
        /* span extDict & currentPrefixSegment */
        {
            size_t length1 = dictEnd - match;
            memmove(oLitEnd, match, length1);
            op = oLitEnd + length1;
            sequence.matchLength -= length1;
            match = base;
            if (op > oend_8 || sequence.matchLength < MINMATCH) {
              while (op < oMatchEnd) *op++ = *match++;
              return sequenceLength;
            }
        }
    }
    /* Requirement: op <= oend_8 */

    /* match within prefix */
    if (sequence.offset < 8) {
        /* close range match, overlap */
        const int sub2 = dec64table[sequence.offset];
        op[0] = match[0];
        op[1] = match[1];
        op[2] = match[2];
        op[3] = match[3];
        match += dec32table[sequence.offset];
        ZSTD_copy4(op+4, match);
        match -= sub2;
    } else {
        ZSTD_copy8(op, match);
    }
    op += 8; match += 8;

    if (oMatchEnd > oend-(16-MINMATCH))
    {
        if (op < oend_8)
        {
            ZSTD_wildcopy(op, match, oend_8 - op);
            match += oend_8 - op;
            op = oend_8;
        }
        while (op < oMatchEnd) *op++ = *match++;
    }
    else
    {
        ZSTD_wildcopy(op, match, (ptrdiff_t)sequence.matchLength-8);   /* works even if matchLength < 8, but must be signed */
    }
    return sequenceLength;
}


static size_t ZSTD_decompressSequences(
                               ZSTD_DCtx* dctx,
                               void* dst, size_t maxDstSize,
                         const void* seqStart, size_t seqSize)
{
    const BYTE* ip = (const BYTE*)seqStart;
    const BYTE* const iend = ip + seqSize;
    BYTE* const ostart = (BYTE* const)dst;
    BYTE* op = ostart;
    BYTE* const oend = ostart + maxDstSize;
    size_t errorCode, dumpsLength;
    const BYTE* litPtr = dctx->litPtr;
    const BYTE* const litEnd = litPtr + dctx->litSize;
    int nbSeq;
    const BYTE* dumps;
    U32* DTableLL = dctx->LLTable;
    U32* DTableML = dctx->MLTable;
    U32* DTableOffb = dctx->OffTable;
    const BYTE* const base = (const BYTE*) (dctx->base);
    const BYTE* const vBase = (const BYTE*) (dctx->vBase);
    const BYTE* const dictEnd = (const BYTE*) (dctx->dictEnd);

    /* Build Decoding Tables */
    errorCode = ZSTD_decodeSeqHeaders(&nbSeq, &dumps, &dumpsLength,
                                      DTableLL, DTableML, DTableOffb,
                                      ip, iend-ip);
    if (ZSTD_isError(errorCode)) return errorCode;
    ip += errorCode;

    /* Regen sequences */
    {
        seq_t sequence;
        seqState_t seqState;

        memset(&sequence, 0, sizeof(sequence));
        sequence.offset = 4;
        seqState.dumps = dumps;
        seqState.dumpsEnd = dumps + dumpsLength;
        seqState.prevOffset = 4;
        errorCode = BIT_initDStream(&(seqState.DStream), ip, iend-ip);
        if (ERR_isError(errorCode)) return ERROR(corruption_detected);
        FSE_initDState(&(seqState.stateLL), &(seqState.DStream), DTableLL);
        FSE_initDState(&(seqState.stateOffb), &(seqState.DStream), DTableOffb);
        FSE_initDState(&(seqState.stateML), &(seqState.DStream), DTableML);

        for ( ; (BIT_reloadDStream(&(seqState.DStream)) <= BIT_DStream_completed) && nbSeq ; )
        {
            size_t oneSeqSize;
            nbSeq--;
            ZSTD_decodeSequence(&sequence, &seqState);
            oneSeqSize = ZSTD_execSequence(op, oend, sequence, &litPtr, litEnd, base, vBase, dictEnd);
            if (ZSTD_isError(oneSeqSize)) return oneSeqSize;
            op += oneSeqSize;
        }

        /* check if reached exact end */
        if ( !BIT_endOfDStream(&(seqState.DStream)) ) return ERROR(corruption_detected);   /* DStream should be entirely and exactly consumed; otherwise data is corrupted */

        /* last literal segment */
        {
            size_t lastLLSize = litEnd - litPtr;
            if (litPtr > litEnd) return ERROR(corruption_detected);
            if (op+lastLLSize > oend) return ERROR(dstSize_tooSmall);
            if (lastLLSize > 0) {
                if (op != litPtr) memcpy(op, litPtr, lastLLSize);
                op += lastLLSize;
            }
        }
    }

    return op-ostart;
}


static void ZSTD_checkContinuity(ZSTD_DCtx* dctx, const void* dst)
{
    if (dst != dctx->previousDstEnd)   /* not contiguous */
    {
        dctx->dictEnd = dctx->previousDstEnd;
        dctx->vBase = (const char*)dst - ((const char*)(dctx->previousDstEnd) - (const char*)(dctx->base));
        dctx->base = dst;
        dctx->previousDstEnd = dst;
    }
}


static size_t ZSTD_decompressBlock_internal(ZSTD_DCtx* dctx,
                            void* dst, size_t maxDstSize,
                      const void* src, size_t srcSize)
{
    /* blockType == blockCompressed */
    const BYTE* ip = (const BYTE*)src;
    size_t litCSize;

    if (srcSize > BLOCKSIZE) return ERROR(corruption_detected);

    /* Decode literals sub-block */
    litCSize = ZSTD_decodeLiteralsBlock(dctx, src, srcSize);
    if (ZSTD_isError(litCSize)) return litCSize;
    ip += litCSize;
    srcSize -= litCSize;

    return ZSTD_decompressSequences(dctx, dst, maxDstSize, ip, srcSize);
}


static size_t ZSTD_decompress_usingDict(ZSTD_DCtx* ctx,
                                 void* dst, size_t maxDstSize,
                                 const void* src, size_t srcSize,
                                 const void* dict, size_t dictSize)
{
    const BYTE* ip = (const BYTE*)src;
    const BYTE* iend = ip + srcSize;
    BYTE* const ostart = (BYTE* const)dst;
    BYTE* op = ostart;
    BYTE* const oend = ostart + maxDstSize;
    size_t remainingSize = srcSize;
    blockProperties_t blockProperties;

    /* init */
    ZSTD_resetDCtx(ctx);
    if (dict)
    {
        ZSTD_decompress_insertDictionary(ctx, dict, dictSize);
        ctx->dictEnd = ctx->previousDstEnd;
        ctx->vBase = (const char*)dst - ((const char*)(ctx->previousDstEnd) - (const char*)(ctx->base));
        ctx->base = dst;
    }
    else
    {
        ctx->vBase = ctx->base = ctx->dictEnd = dst;
    }

    /* Frame Header */
    {
        size_t frameHeaderSize;
        if (srcSize < ZSTD_frameHeaderSize_min+ZSTD_blockHeaderSize) return ERROR(srcSize_wrong);
        frameHeaderSize = ZSTD_decodeFrameHeader_Part1(ctx, src, ZSTD_frameHeaderSize_min);
        if (ZSTD_isError(frameHeaderSize)) return frameHeaderSize;
        if (srcSize < frameHeaderSize+ZSTD_blockHeaderSize) return ERROR(srcSize_wrong);
        ip += frameHeaderSize; remainingSize -= frameHeaderSize;
        frameHeaderSize = ZSTD_decodeFrameHeader_Part2(ctx, src, frameHeaderSize);
        if (ZSTD_isError(frameHeaderSize)) return frameHeaderSize;
    }

    /* Loop on each block */
    while (1)
    {
        size_t decodedSize=0;
        size_t cBlockSize = ZSTD_getcBlockSize(ip, iend-ip, &blockProperties);
        if (ZSTD_isError(cBlockSize)) return cBlockSize;

        ip += ZSTD_blockHeaderSize;
        remainingSize -= ZSTD_blockHeaderSize;
        if (cBlockSize > remainingSize) return ERROR(srcSize_wrong);

        switch(blockProperties.blockType)
        {
        case bt_compressed:
            decodedSize = ZSTD_decompressBlock_internal(ctx, op, oend-op, ip, cBlockSize);
            break;
        case bt_raw :
            decodedSize = ZSTD_copyRawBlock(op, oend-op, ip, cBlockSize);
            break;
        case bt_rle :
            return ERROR(GENERIC);   /* not yet supported */
            break;
        case bt_end :
            /* end of frame */
            if (remainingSize) return ERROR(srcSize_wrong);
            break;
        default:
            return ERROR(GENERIC);   /* impossible */
        }
        if (cBlockSize == 0) break;   /* bt_end */

        if (ZSTD_isError(decodedSize)) return decodedSize;
        op += decodedSize;
        ip += cBlockSize;
        remainingSize -= cBlockSize;
    }

    return op-ostart;
}

/* ZSTD_errorFrameSizeInfoLegacy() :
   assumes `cSize` and `dBound` are _not_ NULL */
static void ZSTD_errorFrameSizeInfoLegacy(size_t* cSize, unsigned long long* dBound, size_t ret)
{
    *cSize = ret;
    *dBound = ZSTD_CONTENTSIZE_ERROR;
}

void ZSTDv04_findFrameSizeInfoLegacy(const void *src, size_t srcSize, size_t* cSize, unsigned long long* dBound)
{
    const BYTE* ip = (const BYTE*)src;
    size_t remainingSize = srcSize;
    size_t nbBlocks = 0;
    blockProperties_t blockProperties;

    /* Frame Header */
    if (srcSize < ZSTD_frameHeaderSize_min) {
        ZSTD_errorFrameSizeInfoLegacy(cSize, dBound, ERROR(srcSize_wrong));
        return;
    }
    if (MEM_readLE32(src) != ZSTD_MAGICNUMBER) {
        ZSTD_errorFrameSizeInfoLegacy(cSize, dBound, ERROR(prefix_unknown));
        return;
    }
    ip += ZSTD_frameHeaderSize_min; remainingSize -= ZSTD_frameHeaderSize_min;

    /* Loop on each block */
    while (1)
    {
        size_t cBlockSize = ZSTD_getcBlockSize(ip, remainingSize, &blockProperties);
        if (ZSTD_isError(cBlockSize)) {
            ZSTD_errorFrameSizeInfoLegacy(cSize, dBound, cBlockSize);
            return;
        }

        ip += ZSTD_blockHeaderSize;
        remainingSize -= ZSTD_blockHeaderSize;
        if (cBlockSize > remainingSize) {
            ZSTD_errorFrameSizeInfoLegacy(cSize, dBound, ERROR(srcSize_wrong));
            return;
        }

        if (cBlockSize == 0) break;   /* bt_end */

        ip += cBlockSize;
        remainingSize -= cBlockSize;
        nbBlocks++;
    }

    *cSize = ip - (const BYTE*)src;
    *dBound = nbBlocks * BLOCKSIZE;
}

/* ******************************
*  Streaming Decompression API
********************************/
static size_t ZSTD_nextSrcSizeToDecompress(ZSTD_DCtx* dctx)
{
    return dctx->expected;
}

static size_t ZSTD_decompressContinue(ZSTD_DCtx* ctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    /* Sanity check */
    if (srcSize != ctx->expected) return ERROR(srcSize_wrong);
    ZSTD_checkContinuity(ctx, dst);

    /* Decompress : frame header; part 1 */
    switch (ctx->stage)
    {
    case ZSTDds_getFrameHeaderSize :
        /* get frame header size */
        if (srcSize != ZSTD_frameHeaderSize_min) return ERROR(srcSize_wrong);   /* impossible */
        ctx->headerSize = ZSTD_decodeFrameHeader_Part1(ctx, src, ZSTD_frameHeaderSize_min);
        if (ZSTD_isError(ctx->headerSize)) return ctx->headerSize;
        memcpy(ctx->headerBuffer, src, ZSTD_frameHeaderSize_min);
        if (ctx->headerSize > ZSTD_frameHeaderSize_min) return ERROR(GENERIC);   /* impossible */
        ctx->expected = 0;   /* not necessary to copy more */
        /* fallthrough */
    case ZSTDds_decodeFrameHeader:
        /* get frame header */
        {   size_t const result = ZSTD_decodeFrameHeader_Part2(ctx, ctx->headerBuffer, ctx->headerSize);
            if (ZSTD_isError(result)) return result;
            ctx->expected = ZSTD_blockHeaderSize;
            ctx->stage = ZSTDds_decodeBlockHeader;
            return 0;
        }
    case ZSTDds_decodeBlockHeader:
        /* Decode block header */
        {   blockProperties_t bp;
            size_t const blockSize = ZSTD_getcBlockSize(src, ZSTD_blockHeaderSize, &bp);
            if (ZSTD_isError(blockSize)) return blockSize;
            if (bp.blockType == bt_end)
            {
                ctx->expected = 0;
                ctx->stage = ZSTDds_getFrameHeaderSize;
            }
            else
            {
                ctx->expected = blockSize;
                ctx->bType = bp.blockType;
                ctx->stage = ZSTDds_decompressBlock;
            }
            return 0;
        }
    case ZSTDds_decompressBlock:
        {
            /* Decompress : block content */
            size_t rSize;
            switch(ctx->bType)
            {
            case bt_compressed:
                rSize = ZSTD_decompressBlock_internal(ctx, dst, maxDstSize, src, srcSize);
                break;
            case bt_raw :
                rSize = ZSTD_copyRawBlock(dst, maxDstSize, src, srcSize);
                break;
            case bt_rle :
                return ERROR(GENERIC);   /* not yet handled */
                break;
            case bt_end :   /* should never happen (filtered at phase 1) */
                rSize = 0;
                break;
            default:
                return ERROR(GENERIC);
            }
            ctx->stage = ZSTDds_decodeBlockHeader;
            ctx->expected = ZSTD_blockHeaderSize;
            if (ZSTD_isError(rSize)) return rSize;
            ctx->previousDstEnd = (char*)dst + rSize;
            return rSize;
        }
    default:
        return ERROR(GENERIC);   /* impossible */
    }
}


static void ZSTD_decompress_insertDictionary(ZSTD_DCtx* ctx, const void* dict, size_t dictSize)
{
    ctx->dictEnd = ctx->previousDstEnd;
    ctx->vBase = (const char*)dict - ((const char*)(ctx->previousDstEnd) - (const char*)(ctx->base));
    ctx->base = dict;
    ctx->previousDstEnd = (const char*)dict + dictSize;
}



/*
    Buffered version of Zstd compression library
    Copyright (C) 2015, Yann Collet.

    BSD 2-Clause License (https://opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    You can contact the author at :
    - zstd source repository : https://github.com/Cyan4973/zstd
    - ztsd public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* The objects defined into this file should be considered experimental.
 * They are not labelled stable, as their prototype may change in the future.
 * You can use them for tests, provide feedback, or if you can endure risk of future changes.
 */

/* *************************************
*  Includes
***************************************/
#include <stdlib.h>


/** ************************************************
*  Streaming decompression
*
*  A ZBUFF_DCtx object is required to track streaming operation.
*  Use ZBUFF_createDCtx() and ZBUFF_freeDCtx() to create/release resources.
*  Use ZBUFF_decompressInit() to start a new decompression operation.
*  ZBUFF_DCtx objects can be reused multiple times.
*
*  Use ZBUFF_decompressContinue() repetitively to consume your input.
*  *srcSizePtr and *maxDstSizePtr can be any size.
*  The function will report how many bytes were read or written by modifying *srcSizePtr and *maxDstSizePtr.
*  Note that it may not consume the entire input, in which case it's up to the caller to call again the function with remaining input.
*  The content of dst will be overwritten (up to *maxDstSizePtr) at each function call, so save its content if it matters or change dst .
*  return : a hint to preferred nb of bytes to use as input for next function call (it's only a hint, to improve latency)
*            or 0 when a frame is completely decoded
*            or an error code, which can be tested using ZBUFF_isError().
*
*  Hint : recommended buffer sizes (not compulsory)
*  output : 128 KB block size is the internal unit, it ensures it's always possible to write a full block when it's decoded.
*  input : just follow indications from ZBUFF_decompressContinue() to minimize latency. It should always be <= 128 KB + 3 .
* **************************************************/

typedef enum { ZBUFFds_init, ZBUFFds_readHeader, ZBUFFds_loadHeader, ZBUFFds_decodeHeader,
               ZBUFFds_read, ZBUFFds_load, ZBUFFds_flush } ZBUFF_dStage;

/* *** Resource management *** */

#define ZSTD_frameHeaderSize_max 5   /* too magical, should come from reference */
struct ZBUFFv04_DCtx_s {
    ZSTD_DCtx* zc;
    ZSTD_parameters params;
    char* inBuff;
    size_t inBuffSize;
    size_t inPos;
    char* outBuff;
    size_t outBuffSize;
    size_t outStart;
    size_t outEnd;
    size_t hPos;
    const char* dict;
    size_t dictSize;
    ZBUFF_dStage stage;
    unsigned char headerBuffer[ZSTD_frameHeaderSize_max];
};   /* typedef'd to ZBUFF_DCtx within "zstd_buffered.h" */

typedef ZBUFFv04_DCtx ZBUFF_DCtx;


static ZBUFF_DCtx* ZBUFF_createDCtx(void)
{
    ZBUFF_DCtx* zbc = (ZBUFF_DCtx*)malloc(sizeof(ZBUFF_DCtx));
    if (zbc==NULL) return NULL;
    memset(zbc, 0, sizeof(*zbc));
    zbc->zc = ZSTD_createDCtx();
    zbc->stage = ZBUFFds_init;
    return zbc;
}

static size_t ZBUFF_freeDCtx(ZBUFF_DCtx* zbc)
{
    if (zbc==NULL) return 0;   /* support free on null */
    ZSTD_freeDCtx(zbc->zc);
    free(zbc->inBuff);
    free(zbc->outBuff);
    free(zbc);
    return 0;
}


/* *** Initialization *** */

static size_t ZBUFF_decompressInit(ZBUFF_DCtx* zbc)
{
    zbc->stage = ZBUFFds_readHeader;
    zbc->hPos = zbc->inPos = zbc->outStart = zbc->outEnd = zbc->dictSize = 0;
    return ZSTD_resetDCtx(zbc->zc);
}


static size_t ZBUFF_decompressWithDictionary(ZBUFF_DCtx* zbc, const void* src, size_t srcSize)
{
    zbc->dict = (const char*)src;
    zbc->dictSize = srcSize;
    return 0;
}

static size_t ZBUFF_limitCopy(void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    size_t length = MIN(maxDstSize, srcSize);
    if (length > 0) {
        memcpy(dst, src, length);
    }
    return length;
}

/* *** Decompression *** */

static size_t ZBUFF_decompressContinue(ZBUFF_DCtx* zbc, void* dst, size_t* maxDstSizePtr, const void* src, size_t* srcSizePtr)
{
    const char* const istart = (const char*)src;
    const char* ip = istart;
    const char* const iend = istart + *srcSizePtr;
    char* const ostart = (char*)dst;
    char* op = ostart;
    char* const oend = ostart + *maxDstSizePtr;
    U32 notDone = 1;

    DEBUGLOG(5, "ZBUFF_decompressContinue");
    while (notDone)
    {
        switch(zbc->stage)
        {

        case ZBUFFds_init :
            DEBUGLOG(5, "ZBUFF_decompressContinue: stage==ZBUFFds_init => ERROR(init_missing)");
            return ERROR(init_missing);

        case ZBUFFds_readHeader :
            /* read header from src */
            {   size_t const headerSize = ZSTD_getFrameParams(&(zbc->params), src, *srcSizePtr);
                if (ZSTD_isError(headerSize)) return headerSize;
                if (headerSize) {
                    /* not enough input to decode header : tell how many bytes would be necessary */
                    memcpy(zbc->headerBuffer+zbc->hPos, src, *srcSizePtr);
                    zbc->hPos += *srcSizePtr;
                    *maxDstSizePtr = 0;
                    zbc->stage = ZBUFFds_loadHeader;
                    return headerSize - zbc->hPos;
                }
                zbc->stage = ZBUFFds_decodeHeader;
                break;
            }

        case ZBUFFds_loadHeader:
            /* complete header from src */
            {   size_t headerSize = ZBUFF_limitCopy(
                    zbc->headerBuffer + zbc->hPos, ZSTD_frameHeaderSize_max - zbc->hPos,
                    src, *srcSizePtr);
                zbc->hPos += headerSize;
                ip += headerSize;
                headerSize = ZSTD_getFrameParams(&(zbc->params), zbc->headerBuffer, zbc->hPos);
                if (ZSTD_isError(headerSize)) return headerSize;
                if (headerSize) {
                    /* not enough input to decode header : tell how many bytes would be necessary */
                    *maxDstSizePtr = 0;
                    return headerSize - zbc->hPos;
            }   }
            /* intentional fallthrough */

        case ZBUFFds_decodeHeader:
                /* apply header to create / resize buffers */
                {   size_t const neededOutSize = (size_t)1 << zbc->params.windowLog;
                    size_t const neededInSize = BLOCKSIZE;   /* a block is never > BLOCKSIZE */
                    if (zbc->inBuffSize < neededInSize) {
                        free(zbc->inBuff);
                        zbc->inBuffSize = neededInSize;
                        zbc->inBuff = (char*)malloc(neededInSize);
                        if (zbc->inBuff == NULL) return ERROR(memory_allocation);
                    }
                    if (zbc->outBuffSize < neededOutSize) {
                        free(zbc->outBuff);
                        zbc->outBuffSize = neededOutSize;
                        zbc->outBuff = (char*)malloc(neededOutSize);
                        if (zbc->outBuff == NULL) return ERROR(memory_allocation);
                }   }
                if (zbc->dictSize)
                    ZSTD_decompress_insertDictionary(zbc->zc, zbc->dict, zbc->dictSize);
                if (zbc->hPos) {
                    /* some data already loaded into headerBuffer : transfer into inBuff */
                    memcpy(zbc->inBuff, zbc->headerBuffer, zbc->hPos);
                    zbc->inPos = zbc->hPos;
                    zbc->hPos = 0;
                    zbc->stage = ZBUFFds_load;
                    break;
                }
                zbc->stage = ZBUFFds_read;
		/* fall-through */
        case ZBUFFds_read:
            {
                size_t neededInSize = ZSTD_nextSrcSizeToDecompress(zbc->zc);
                if (neededInSize==0)   /* end of frame */
                {
                    zbc->stage = ZBUFFds_init;
                    notDone = 0;
                    break;
                }
                if ((size_t)(iend-ip) >= neededInSize)
                {
                    /* directly decode from src */
                    size_t decodedSize = ZSTD_decompressContinue(zbc->zc,
                        zbc->outBuff + zbc->outStart, zbc->outBuffSize - zbc->outStart,
                        ip, neededInSize);
                    if (ZSTD_isError(decodedSize)) return decodedSize;
                    ip += neededInSize;
                    if (!decodedSize) break;   /* this was just a header */
                    zbc->outEnd = zbc->outStart +  decodedSize;
                    zbc->stage = ZBUFFds_flush;
                    break;
                }
                if (ip==iend) { notDone = 0; break; }   /* no more input */
                zbc->stage = ZBUFFds_load;
            }
	    /* fall-through */
        case ZBUFFds_load:
            {
                size_t neededInSize = ZSTD_nextSrcSizeToDecompress(zbc->zc);
                size_t toLoad = neededInSize - zbc->inPos;   /* should always be <= remaining space within inBuff */
                size_t loadedSize;
                if (toLoad > zbc->inBuffSize - zbc->inPos) return ERROR(corruption_detected);   /* should never happen */
                loadedSize = ZBUFF_limitCopy(zbc->inBuff + zbc->inPos, toLoad, ip, iend-ip);
                ip += loadedSize;
                zbc->inPos += loadedSize;
                if (loadedSize < toLoad) { notDone = 0; break; }   /* not enough input, wait for more */
                {
                    size_t decodedSize = ZSTD_decompressContinue(zbc->zc,
                        zbc->outBuff + zbc->outStart, zbc->outBuffSize - zbc->outStart,
                        zbc->inBuff, neededInSize);
                    if (ZSTD_isError(decodedSize)) return decodedSize;
                    zbc->inPos = 0;   /* input is consumed */
                    if (!decodedSize) { zbc->stage = ZBUFFds_read; break; }   /* this was just a header */
                    zbc->outEnd = zbc->outStart +  decodedSize;
                    zbc->stage = ZBUFFds_flush;
                    /* ZBUFFds_flush follows */
                }
            }
	    /* fall-through */
        case ZBUFFds_flush:
            {
                size_t toFlushSize = zbc->outEnd - zbc->outStart;
                size_t flushedSize = ZBUFF_limitCopy(op, oend-op, zbc->outBuff + zbc->outStart, toFlushSize);
                op += flushedSize;
                zbc->outStart += flushedSize;
                if (flushedSize == toFlushSize)
                {
                    zbc->stage = ZBUFFds_read;
                    if (zbc->outStart + BLOCKSIZE > zbc->outBuffSize)
                        zbc->outStart = zbc->outEnd = 0;
                    break;
                }
                /* cannot flush everything */
                notDone = 0;
                break;
            }
        default: return ERROR(GENERIC);   /* impossible */
        }
    }

    *srcSizePtr = ip-istart;
    *maxDstSizePtr = op-ostart;

    {
        size_t nextSrcSizeHint = ZSTD_nextSrcSizeToDecompress(zbc->zc);
        if (nextSrcSizeHint > 3) nextSrcSizeHint+= 3;   /* get the next block header while at it */
        nextSrcSizeHint -= zbc->inPos;   /* already loaded*/
        return nextSrcSizeHint;
    }
}


/* *************************************
*  Tool functions
***************************************/
unsigned ZBUFFv04_isError(size_t errorCode) { return ERR_isError(errorCode); }
const char* ZBUFFv04_getErrorName(size_t errorCode) { return ERR_getErrorName(errorCode); }

size_t ZBUFFv04_recommendedDInSize(void)  { return BLOCKSIZE + 3; }
size_t ZBUFFv04_recommendedDOutSize(void) { return BLOCKSIZE; }



/*- ========================================================================= -*/

/* final wrapping stage */

size_t ZSTDv04_decompressDCtx(ZSTD_DCtx* dctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_decompress_usingDict(dctx, dst, maxDstSize, src, srcSize, NULL, 0);
}

size_t ZSTDv04_decompress(void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
#if defined(ZSTD_HEAPMODE) && (ZSTD_HEAPMODE==1)
    size_t regenSize;
    ZSTD_DCtx* dctx = ZSTD_createDCtx();
    if (dctx==NULL) return ERROR(memory_allocation);
    regenSize = ZSTDv04_decompressDCtx(dctx, dst, maxDstSize, src, srcSize);
    ZSTD_freeDCtx(dctx);
    return regenSize;
#else
    ZSTD_DCtx dctx;
    return ZSTDv04_decompressDCtx(&dctx, dst, maxDstSize, src, srcSize);
#endif
}

size_t ZSTDv04_resetDCtx(ZSTDv04_Dctx* dctx) { return ZSTD_resetDCtx(dctx); }

size_t ZSTDv04_nextSrcSizeToDecompress(ZSTDv04_Dctx* dctx)
{
    return ZSTD_nextSrcSizeToDecompress(dctx);
}

size_t ZSTDv04_decompressContinue(ZSTDv04_Dctx* dctx, void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return ZSTD_decompressContinue(dctx, dst, maxDstSize, src, srcSize);
}



ZBUFFv04_DCtx* ZBUFFv04_createDCtx(void) { return ZBUFF_createDCtx(); }
size_t ZBUFFv04_freeDCtx(ZBUFFv04_DCtx* dctx) { return ZBUFF_freeDCtx(dctx); }

size_t ZBUFFv04_decompressInit(ZBUFFv04_DCtx* dctx) { return ZBUFF_decompressInit(dctx); }
size_t ZBUFFv04_decompressWithDictionary(ZBUFFv04_DCtx* dctx, const void* src, size_t srcSize)
{ return ZBUFF_decompressWithDictionary(dctx, src, srcSize); }

size_t ZBUFFv04_decompressContinue(ZBUFFv04_DCtx* dctx, void* dst, size_t* maxDstSizePtr, const void* src, size_t* srcSizePtr)
{
    DEBUGLOG(5, "ZBUFFv04_decompressContinue");
    return ZBUFF_decompressContinue(dctx, dst, maxDstSizePtr, src, srcSizePtr);
}

ZSTD_DCtx* ZSTDv04_createDCtx(void) { return ZSTD_createDCtx(); }
size_t ZSTDv04_freeDCtx(ZSTD_DCtx* dctx) { return ZSTD_freeDCtx(dctx); }
