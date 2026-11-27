/* ******************************************************************
 * hist : Histogram functions
 * part of Finite State Entropy project
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 *  You can contact the author at :
 *  - FSE source repository : https://github.com/Cyan4973/FiniteStateEntropy
 *  - Public forum : https://groups.google.com/forum/#!forum/lz4c
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
****************************************************************** */

/* --- dependencies --- */
#include "../common/mem.h"             /* U32, BYTE, etc. */
#include "../common/debug.h"           /* assert, DEBUGLOG */
#include "../common/error_private.h"   /* ERROR */
#include "hist.h"

#if defined(ZSTD_ARCH_ARM_SVE2)
#define HIST_FAST_THRESHOLD 500
#else
#define HIST_FAST_THRESHOLD 1500
#endif


/* --- Error management --- */
unsigned HIST_isError(size_t code) { return ERR_isError(code); }

/*-**************************************************************
 *  Histogram functions
 ****************************************************************/
void HIST_add(unsigned* count, const void* src, size_t srcSize)
{
    const BYTE* ip = (const BYTE*)src;
    const BYTE* const end = ip + srcSize;

    while (ip<end) {
        count[*ip++]++;
    }
}

unsigned HIST_count_simple(unsigned* count, unsigned* maxSymbolValuePtr,
                           const void* src, size_t srcSize)
{
    const BYTE* ip = (const BYTE*)src;
    const BYTE* const end = ip + srcSize;
    unsigned maxSymbolValue = *maxSymbolValuePtr;
    unsigned largestCount=0;

    ZSTD_memset(count, 0, (maxSymbolValue+1) * sizeof(*count));
    if (srcSize==0) { *maxSymbolValuePtr = 0; return 0; }

    while (ip<end) {
        assert(*ip <= maxSymbolValue);
        count[*ip++]++;
    }

    while (!count[maxSymbolValue]) maxSymbolValue--;
    *maxSymbolValuePtr = maxSymbolValue;

    {   U32 s;
        for (s=0; s<=maxSymbolValue; s++)
            if (count[s] > largestCount) largestCount = count[s];
    }

    return largestCount;
}

typedef enum { trustInput, checkMaxSymbolValue } HIST_checkInput_e;

#if defined(ZSTD_ARCH_ARM_SVE2)
/* U16 counters must hold even an all-identical chunk. Round the limit down
 * to 16 bytes so zero-padding the final vector cannot overflow them. */
#define HIST_SVE2_MAX_CHUNK_SIZE ((1U << 16) - 16)
FORCE_INLINE_TEMPLATE size_t min_size(size_t a, size_t b) { return a < b ? a : b; }

static
svuint16_t HIST_count_6_sve2(const BYTE* const src, size_t size, U32* const dst,
                             const svuint8_t c0, const svuint8_t c1,
                             const svuint8_t c2, const svuint8_t c3,
                             const svuint8_t c4, const svuint8_t c5,
                             const svuint16_t histmax, size_t maxCount)
{
    const svbool_t vl128 = svptrue_pat_b8(SV_VL16);
    svuint16_t hh0 = svdup_n_u16(0);
    svuint16_t hh1 = svdup_n_u16(0);
    svuint16_t hh2 = svdup_n_u16(0);
    svuint16_t hh3 = svdup_n_u16(0);
    svuint16_t hh4 = svdup_n_u16(0);
    svuint16_t hh5 = svdup_n_u16(0);
    svuint16_t hh6 = svdup_n_u16(0);
    svuint16_t hh7 = svdup_n_u16(0);
    svuint16_t hh8 = svdup_n_u16(0);
    svuint16_t hh9 = svdup_n_u16(0);
    svuint16_t hha = svdup_n_u16(0);
    svuint16_t hhb = svdup_n_u16(0);

    size_t i = 0;
    while (i < size) {
        /* We can only accumulate 15 (15 * 16 <= 255) iterations of histogram
         * in 8-bit accumulators! */
        const size_t size240 = min_size(i + 240, size);

        svbool_t pred = svwhilelt_b8_u64(i, size);
        svuint8_t c = svld1rq_u8(pred, src + i);
        svuint8_t h0 = svhistseg_u8(c0, c);
        svuint8_t h1 = svhistseg_u8(c1, c);
        svuint8_t h2 = svhistseg_u8(c2, c);
        svuint8_t h3 = svhistseg_u8(c3, c);
        svuint8_t h4 = svhistseg_u8(c4, c);
        svuint8_t h5 = svhistseg_u8(c5, c);

        for (i += 16; i < size240; i += 16) {
            pred = svwhilelt_b8_u64(i, size);
            c = svld1rq_u8(pred, src + i);
            h0 = svadd_u8_x(vl128, h0, svhistseg_u8(c0, c));
            h1 = svadd_u8_x(vl128, h1, svhistseg_u8(c1, c));
            h2 = svadd_u8_x(vl128, h2, svhistseg_u8(c2, c));
            h3 = svadd_u8_x(vl128, h3, svhistseg_u8(c3, c));
            h4 = svadd_u8_x(vl128, h4, svhistseg_u8(c4, c));
            h5 = svadd_u8_x(vl128, h5, svhistseg_u8(c5, c));
        }

        hh0 = svaddwb_u16(hh0, h0);
        hh1 = svaddwt_u16(hh1, h0);
        hh2 = svaddwb_u16(hh2, h1);
        hh3 = svaddwt_u16(hh3, h1);
        hh4 = svaddwb_u16(hh4, h2);
        hh5 = svaddwt_u16(hh5, h2);
        hh6 = svaddwb_u16(hh6, h3);
        hh7 = svaddwt_u16(hh7, h3);
        hh8 = svaddwb_u16(hh8, h4);
        hh9 = svaddwt_u16(hh9, h4);
        hha = svaddwb_u16(hha, h5);
        hhb = svaddwt_u16(hhb, h5);
    }

    svst1_u32(svwhilelt_b32_u64( 0, maxCount), dst +  0, svshllb_n_u32(hh0, 0));
    svst1_u32(svwhilelt_b32_u64( 4, maxCount), dst +  4, svshllt_n_u32(hh0, 0));
    svst1_u32(svwhilelt_b32_u64( 8, maxCount), dst +  8, svshllb_n_u32(hh1, 0));
    svst1_u32(svwhilelt_b32_u64(12, maxCount), dst + 12, svshllt_n_u32(hh1, 0));
    svst1_u32(svwhilelt_b32_u64(16, maxCount), dst + 16, svshllb_n_u32(hh2, 0));
    svst1_u32(svwhilelt_b32_u64(20, maxCount), dst + 20, svshllt_n_u32(hh2, 0));
    svst1_u32(svwhilelt_b32_u64(24, maxCount), dst + 24, svshllb_n_u32(hh3, 0));
    svst1_u32(svwhilelt_b32_u64(28, maxCount), dst + 28, svshllt_n_u32(hh3, 0));
    svst1_u32(svwhilelt_b32_u64(32, maxCount), dst + 32, svshllb_n_u32(hh4, 0));
    svst1_u32(svwhilelt_b32_u64(36, maxCount), dst + 36, svshllt_n_u32(hh4, 0));
    svst1_u32(svwhilelt_b32_u64(40, maxCount), dst + 40, svshllb_n_u32(hh5, 0));
    svst1_u32(svwhilelt_b32_u64(44, maxCount), dst + 44, svshllt_n_u32(hh5, 0));
    svst1_u32(svwhilelt_b32_u64(48, maxCount), dst + 48, svshllb_n_u32(hh6, 0));
    svst1_u32(svwhilelt_b32_u64(52, maxCount), dst + 52, svshllt_n_u32(hh6, 0));
    svst1_u32(svwhilelt_b32_u64(56, maxCount), dst + 56, svshllb_n_u32(hh7, 0));
    svst1_u32(svwhilelt_b32_u64(60, maxCount), dst + 60, svshllt_n_u32(hh7, 0));
    svst1_u32(svwhilelt_b32_u64(64, maxCount), dst + 64, svshllb_n_u32(hh8, 0));
    svst1_u32(svwhilelt_b32_u64(68, maxCount), dst + 68, svshllt_n_u32(hh8, 0));
    svst1_u32(svwhilelt_b32_u64(72, maxCount), dst + 72, svshllb_n_u32(hh9, 0));
    svst1_u32(svwhilelt_b32_u64(76, maxCount), dst + 76, svshllt_n_u32(hh9, 0));
    svst1_u32(svwhilelt_b32_u64(80, maxCount), dst + 80, svshllb_n_u32(hha, 0));
    svst1_u32(svwhilelt_b32_u64(84, maxCount), dst + 84, svshllt_n_u32(hha, 0));
    svst1_u32(svwhilelt_b32_u64(88, maxCount), dst + 88, svshllb_n_u32(hhb, 0));
    svst1_u32(svwhilelt_b32_u64(92, maxCount), dst + 92, svshllt_n_u32(hhb, 0));

    hh0 = svmax_u16_x(vl128, hh0, hh1);
    hh2 = svmax_u16_x(vl128, hh2, hh3);
    hh4 = svmax_u16_x(vl128, hh4, hh5);
    hh6 = svmax_u16_x(vl128, hh6, hh7);
    hh8 = svmax_u16_x(vl128, hh8, hh9);
    hha = svmax_u16_x(vl128, hha, hhb);
    hh0 = svmax_u16_x(vl128, hh0, hh2);
    hh4 = svmax_u16_x(vl128, hh4, hh6);
    hh8 = svmax_u16_x(vl128, hh8, hha);
    hh0 = svmax_u16_x(vl128, hh0, hh4);
    hh8 = svmax_u16_x(vl128, hh8, histmax);
    return svmax_u16_x(vl128, hh0, hh8);
}

static size_t HIST_count_sve2_chunk(unsigned* count, unsigned* maxSymbolValuePtr,
                                   const void* source, size_t sourceSize,
                                   HIST_checkInput_e check)
{
    const BYTE* ip = (const BYTE*)source;
    const size_t maxCount = *maxSymbolValuePtr + 1;

    assert(*maxSymbolValuePtr <= 255);
    assert(sourceSize <= HIST_SVE2_MAX_CHUNK_SIZE);
    if (!sourceSize) {
        ZSTD_memset(count, 0, maxCount * sizeof(*count));
        *maxSymbolValuePtr = 0;
        return 0;
    }

    {   const svbool_t vl128 = svptrue_pat_b8(SV_VL16);
        const svuint8_t c0 = svreinterpret_u8(svindex_u32(0x0C040800, 0x01010101));
        const svuint8_t c1 = svadd_n_u8_x(vl128, c0, 16);
        const svuint8_t c2 = svadd_n_u8_x(vl128, c0, 32);
        const svuint8_t c3 = svadd_n_u8_x(vl128, c1, 32);

        svuint8_t symbolMax = svdup_n_u8(0);
        svuint16_t hh0 = svdup_n_u16(0);
        svuint16_t hh1 = svdup_n_u16(0);
        svuint16_t hh2 = svdup_n_u16(0);
        svuint16_t hh3 = svdup_n_u16(0);
        svuint16_t hh4 = svdup_n_u16(0);
        svuint16_t hh5 = svdup_n_u16(0);
        svuint16_t hh6 = svdup_n_u16(0);
        svuint16_t hh7 = svdup_n_u16(0);
        svuint16_t max;
        size_t maxSymbolValue;

        size_t i = 0;
        while (i < sourceSize) {
            /* We can only accumulate 15 (15 * 16 <= 255) iterations of
             * histogram in 8-bit accumulators! */
            const size_t size240 = min_size(i + 240, sourceSize);

            svbool_t pred = svwhilelt_b8_u64(i, sourceSize);
            svuint8_t c = svld1rq_u8(pred, ip + i);
            svuint8_t h0 = svhistseg_u8(c0, c);
            svuint8_t h1 = svhistseg_u8(c1, c);
            svuint8_t h2 = svhistseg_u8(c2, c);
            svuint8_t h3 = svhistseg_u8(c3, c);
            symbolMax = svmax_u8_x(vl128, symbolMax, c);

            for (i += 16; i < size240; i += 16) {
                pred = svwhilelt_b8_u64(i, sourceSize);
                c = svld1rq_u8(pred, ip + i);
                h0 = svadd_u8_x(vl128, h0, svhistseg_u8(c0, c));
                h1 = svadd_u8_x(vl128, h1, svhistseg_u8(c1, c));
                h2 = svadd_u8_x(vl128, h2, svhistseg_u8(c2, c));
                h3 = svadd_u8_x(vl128, h3, svhistseg_u8(c3, c));
                symbolMax = svmax_u8_x(vl128, symbolMax, c);
            }

            hh0 = svaddwb_u16(hh0, h0);
            hh1 = svaddwt_u16(hh1, h0);
            hh2 = svaddwb_u16(hh2, h1);
            hh3 = svaddwt_u16(hh3, h1);
            hh4 = svaddwb_u16(hh4, h2);
            hh5 = svaddwt_u16(hh5, h2);
            hh6 = svaddwb_u16(hh6, h3);
            hh7 = svaddwt_u16(hh7, h3);
        }
        maxSymbolValue = svmaxv_u8(vl128, symbolMax);

        if (check && maxSymbolValue > *maxSymbolValuePtr) return ERROR(maxSymbolValue_tooSmall);
        *maxSymbolValuePtr = maxSymbolValue;

        /* If the buffer size is not divisible by 16, the last elements of the final
         * vector register read will be zeros, and these elements must be subtracted
         * from the histogram.
         */
        hh0 = svsub_n_u16_m(svptrue_pat_b32(SV_VL1), hh0, -sourceSize & 15);

        svst1_u32(svwhilelt_b32_u64( 0, maxCount), count +  0, svshllb_n_u32(hh0, 0));
        svst1_u32(svwhilelt_b32_u64( 4, maxCount), count +  4, svshllt_n_u32(hh0, 0));
        svst1_u32(svwhilelt_b32_u64( 8, maxCount), count +  8, svshllb_n_u32(hh1, 0));
        svst1_u32(svwhilelt_b32_u64(12, maxCount), count + 12, svshllt_n_u32(hh1, 0));
        svst1_u32(svwhilelt_b32_u64(16, maxCount), count + 16, svshllb_n_u32(hh2, 0));
        svst1_u32(svwhilelt_b32_u64(20, maxCount), count + 20, svshllt_n_u32(hh2, 0));
        svst1_u32(svwhilelt_b32_u64(24, maxCount), count + 24, svshllb_n_u32(hh3, 0));
        svst1_u32(svwhilelt_b32_u64(28, maxCount), count + 28, svshllt_n_u32(hh3, 0));
        svst1_u32(svwhilelt_b32_u64(32, maxCount), count + 32, svshllb_n_u32(hh4, 0));
        svst1_u32(svwhilelt_b32_u64(36, maxCount), count + 36, svshllt_n_u32(hh4, 0));
        svst1_u32(svwhilelt_b32_u64(40, maxCount), count + 40, svshllb_n_u32(hh5, 0));
        svst1_u32(svwhilelt_b32_u64(44, maxCount), count + 44, svshllt_n_u32(hh5, 0));
        svst1_u32(svwhilelt_b32_u64(48, maxCount), count + 48, svshllb_n_u32(hh6, 0));
        svst1_u32(svwhilelt_b32_u64(52, maxCount), count + 52, svshllt_n_u32(hh6, 0));
        svst1_u32(svwhilelt_b32_u64(56, maxCount), count + 56, svshllb_n_u32(hh7, 0));
        svst1_u32(svwhilelt_b32_u64(60, maxCount), count + 60, svshllt_n_u32(hh7, 0));

        hh0 = svmax_u16_x(vl128, hh0, hh1);
        hh2 = svmax_u16_x(vl128, hh2, hh3);
        hh4 = svmax_u16_x(vl128, hh4, hh5);
        hh6 = svmax_u16_x(vl128, hh6, hh7);
        hh0 = svmax_u16_x(vl128, hh0, hh2);
        hh4 = svmax_u16_x(vl128, hh4, hh6);
        max = svmax_u16_x(vl128, hh0, hh4);

        maxSymbolValue = min_size(maxSymbolValue, maxCount);
        if (maxSymbolValue >= 64) {
            const svuint8_t c4 = svadd_n_u8_x(vl128, c0,  64);
            const svuint8_t c5 = svadd_n_u8_x(vl128, c1,  64);
            const svuint8_t c6 = svadd_n_u8_x(vl128, c2,  64);
            const svuint8_t c7 = svadd_n_u8_x(vl128, c3,  64);
            const svuint8_t c8 = svadd_n_u8_x(vl128, c0, 128);
            const svuint8_t c9 = svadd_n_u8_x(vl128, c1, 128);

            max = HIST_count_6_sve2(ip, sourceSize, count + 64, c4, c5, c6, c7,
                                    c8, c9, max, maxCount - 64);

            if (maxSymbolValue >= 160) {
                const svuint8_t ca = svadd_n_u8_x(vl128, c2, 128);
                const svuint8_t cb = svadd_n_u8_x(vl128, c3, 128);
                const svuint8_t cc = svadd_n_u8_x(vl128, c4, 128);
                const svuint8_t cd = svadd_n_u8_x(vl128, c5, 128);
                const svuint8_t ce = svadd_n_u8_x(vl128, c6, 128);
                const svuint8_t cf = svadd_n_u8_x(vl128, c7, 128);

                max = HIST_count_6_sve2(ip, sourceSize, count + 160, ca, cb, cc,
                                        cd, ce, cf, max, maxCount - 160);
            } else if (maxCount > 160) {
                ZSTD_memset(count + 160, 0, (maxCount - 160) * sizeof(*count));
            }
        } else if (maxCount > 64) {
            ZSTD_memset(count + 64, 0, (maxCount - 64) * sizeof(*count));
        }

        return svmaxv_u16(vl128, max);
    }
}

static size_t HIST_count_sve2(unsigned* count, unsigned* maxSymbolValuePtr,
                              const void* source, size_t sourceSize,
                              HIST_checkInput_e check)
{
    if (sourceSize <= HIST_SVE2_MAX_CHUNK_SIZE)
        return HIST_count_sve2_chunk(count, maxSymbolValuePtr, source, sourceSize, check);

    {   const BYTE* const input = (const BYTE*)source;
        unsigned const maxSymbolLimit = *maxSymbolValuePtr;
        unsigned maxSymbolValue = maxSymbolLimit;
        U32 chunkCounts[256];
        size_t largestCount = HIST_count_sve2_chunk(count, &maxSymbolValue, input,
                                                   HIST_SVE2_MAX_CHUNK_SIZE, check);
        size_t offset = HIST_SVE2_MAX_CHUNK_SIZE;
        if (HIST_isError(largestCount)) return largestCount;

        /* Merge each bounded histogram into U32 totals. Wrapping a present
         * symbol to zero breaks Huffman coding, and undercounting breaks RLE. */
        while (offset < sourceSize) {
            size_t const chunkSize = min_size(sourceSize - offset, HIST_SVE2_MAX_CHUNK_SIZE);
            unsigned chunkMaxSymbol = maxSymbolLimit;
            size_t const result = HIST_count_sve2_chunk(chunkCounts, &chunkMaxSymbol,
                                                        input + offset, chunkSize, check);
            unsigned symbol;
            if (HIST_isError(result)) return result;
            if (chunkMaxSymbol > maxSymbolValue) maxSymbolValue = chunkMaxSymbol;
            for (symbol = 0; symbol <= chunkMaxSymbol; ++symbol) {
                count[symbol] += chunkCounts[symbol];
                if (count[symbol] > largestCount) largestCount = count[symbol];
            }
            offset += chunkSize;
        }

        *maxSymbolValuePtr = maxSymbolValue;
        return largestCount;
    }
}
#endif

/* HIST_count_parallel_wksp() :
 * store histogram into 4 intermediate tables, recombined at the end.
 * this design makes better use of OoO cpus,
 * and is noticeably faster when some values are heavily repeated.
 * But it needs some additional workspace for intermediate tables.
 * `workSpace` must be a U32 table of size >= HIST_WKSP_SIZE_U32.
 * @return : largest histogram frequency,
 *           or an error code (notably when histogram's alphabet is larger than *maxSymbolValuePtr) */
static UNUSED_ATTR
size_t HIST_count_parallel_wksp(unsigned* count, unsigned* maxSymbolValuePtr,
                                const void* source, size_t sourceSize,
                                HIST_checkInput_e check,
                                U32* const workSpace)
{
    const BYTE* ip = (const BYTE*)source;
    const BYTE* const iend = ip+sourceSize;
    size_t const countSize = (*maxSymbolValuePtr + 1) * sizeof(*count);
    unsigned max=0;
    U32* const Counting1 = workSpace;
    U32* const Counting2 = Counting1 + 256;
    U32* const Counting3 = Counting2 + 256;
    U32* const Counting4 = Counting3 + 256;

    /* safety checks */
    assert(*maxSymbolValuePtr <= 255);
    if (!sourceSize) {
        ZSTD_memset(count, 0, countSize);
        *maxSymbolValuePtr = 0;
        return 0;
    }
    ZSTD_memset(workSpace, 0, 4*256*sizeof(unsigned));

    /* by stripes of 16 bytes */
    {   U32 cached = MEM_read32(ip); ip += 4;
        while (ip < iend-15) {
            U32 c = cached; cached = MEM_read32(ip); ip += 4;
            Counting1[(BYTE) c     ]++;
            Counting2[(BYTE)(c>>8) ]++;
            Counting3[(BYTE)(c>>16)]++;
            Counting4[       c>>24 ]++;
            c = cached; cached = MEM_read32(ip); ip += 4;
            Counting1[(BYTE) c     ]++;
            Counting2[(BYTE)(c>>8) ]++;
            Counting3[(BYTE)(c>>16)]++;
            Counting4[       c>>24 ]++;
            c = cached; cached = MEM_read32(ip); ip += 4;
            Counting1[(BYTE) c     ]++;
            Counting2[(BYTE)(c>>8) ]++;
            Counting3[(BYTE)(c>>16)]++;
            Counting4[       c>>24 ]++;
            c = cached; cached = MEM_read32(ip); ip += 4;
            Counting1[(BYTE) c     ]++;
            Counting2[(BYTE)(c>>8) ]++;
            Counting3[(BYTE)(c>>16)]++;
            Counting4[       c>>24 ]++;
        }
        ip-=4;
    }

    /* finish last symbols */
    while (ip<iend) Counting1[*ip++]++;

    {   U32 s;
        for (s=0; s<256; s++) {
            Counting1[s] += Counting2[s] + Counting3[s] + Counting4[s];
            if (Counting1[s] > max) max = Counting1[s];
    }   }

    {   unsigned maxSymbolValue = 255;
        while (!Counting1[maxSymbolValue]) maxSymbolValue--;
        if (check && maxSymbolValue > *maxSymbolValuePtr) return ERROR(maxSymbolValue_tooSmall);
        *maxSymbolValuePtr = maxSymbolValue;
        ZSTD_memmove(count, Counting1, countSize);   /* in case count & Counting1 are overlapping */
    }
    return (size_t)max;
}

/* HIST_countFast_wksp() :
 * Same as HIST_countFast(), but using an externally provided scratch buffer.
 * `workSpace` is a writable buffer which must be 4-bytes aligned,
 * `workSpaceSize` must be >= HIST_WKSP_SIZE
 */
size_t HIST_countFast_wksp(unsigned* count, unsigned* maxSymbolValuePtr,
                          const void* source, size_t sourceSize,
                          void* workSpace, size_t workSpaceSize)
{
    if (sourceSize < HIST_FAST_THRESHOLD) /* heuristic threshold */
        return HIST_count_simple(count, maxSymbolValuePtr, source, sourceSize);
#if defined(ZSTD_ARCH_ARM_SVE2)
    (void)workSpace;
    (void)workSpaceSize;
    return HIST_count_sve2(count, maxSymbolValuePtr, source, sourceSize, trustInput);
#else
    if ((size_t)workSpace & 3) return ERROR(GENERIC);  /* must be aligned on 4-bytes boundaries */
    if (workSpaceSize < HIST_WKSP_SIZE) return ERROR(workSpace_tooSmall);
    return HIST_count_parallel_wksp(count, maxSymbolValuePtr, source, sourceSize, trustInput, (U32*)workSpace);
#endif
}

/* HIST_count_wksp() :
 * Same as HIST_count(), but using an externally provided scratch buffer.
 * `workSpace` size must be table of >= HIST_WKSP_SIZE_U32 unsigned */
size_t HIST_count_wksp(unsigned* count, unsigned* maxSymbolValuePtr,
                       const void* source, size_t sourceSize,
                       void* workSpace, size_t workSpaceSize)
{
#if defined(ZSTD_ARCH_ARM_SVE2)
    if (*maxSymbolValuePtr < 255)
        return HIST_count_sve2(count, maxSymbolValuePtr, source, sourceSize, checkMaxSymbolValue);
#else
    if ((size_t)workSpace & 3) return ERROR(GENERIC);  /* must be aligned on 4-bytes boundaries */
    if (workSpaceSize < HIST_WKSP_SIZE) return ERROR(workSpace_tooSmall);
    if (*maxSymbolValuePtr < 255)
        return HIST_count_parallel_wksp(count, maxSymbolValuePtr, source, sourceSize, checkMaxSymbolValue, (U32*)workSpace);
#endif
    *maxSymbolValuePtr = 255;
    return HIST_countFast_wksp(count, maxSymbolValuePtr, source, sourceSize, workSpace, workSpaceSize);
}

#ifndef ZSTD_NO_UNUSED_FUNCTIONS
/* fast variant (unsafe : won't check if src contains values beyond count[] limit) */
size_t HIST_countFast(unsigned* count, unsigned* maxSymbolValuePtr,
                     const void* source, size_t sourceSize)
{
    unsigned tmpCounters[HIST_WKSP_SIZE_U32];
    return HIST_countFast_wksp(count, maxSymbolValuePtr, source, sourceSize, tmpCounters, sizeof(tmpCounters));
}

size_t HIST_count(unsigned* count, unsigned* maxSymbolValuePtr,
                 const void* src, size_t srcSize)
{
    unsigned tmpCounters[HIST_WKSP_SIZE_U32];
    return HIST_count_wksp(count, maxSymbolValuePtr, src, srcSize, tmpCounters, sizeof(tmpCounters));
}
#endif
