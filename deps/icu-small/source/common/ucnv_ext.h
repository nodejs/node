// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 2003-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  ucnv_ext.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003jun13
*   created by: Markus W. Scherer
*
*   Conversion extensions
*/

#ifndef __UCNV_EXT_H__
#define __UCNV_EXT_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "unicode/ucnv.h"
#include "ucnv_cnv.h"

/*
 * See icuhtml/design/conversion/conversion_extensions.html
 *
 * Conversion extensions serve three purposes:
 * 1. They support m:n mappings.
 * 2. They support extension-only conversion files that are used together
 *    with the regular conversion data in base files.
 * 3. They support mappings with more complicated meta data,
 *    for example "good one-way" mappings (|4).
 *
 * A base file may contain an extension table (explicitly requested or
 * implicitly generated for m:n mappings), but its extension table is not
 * used when an extension-only file is used.
 *
 * It is an error if a base file contains any regular (not extension) mapping
 * from the same sequence as a mapping in the extension file
 * because the base mapping would hide the extension mapping.
 *
 *
 * Data for conversion extensions:
 *
 * One set of data structures per conversion direction (to/from Unicode).
 * The data structures are sorted by input units to allow for binary search.
 * Input sequences of more than one unit are handled like contraction tables
 * in collation:
 * The lookup value of a unit points to another table that is to be searched
 * for the next unit, recursively.
 *
 * For conversion from Unicode, the initial code point is looked up in
 * a 3-stage trie for speed,
 * with an additional table of unique results to save space.
 *
 * Long output strings are stored in separate arrays, with length and index
 * in the lookup tables.
 * Output results also include a flag distinguishing roundtrip from
 * (reverse) fallback mappings.
 *
 * Input Unicode strings must not begin or end with unpaired surrogates
 * to avoid problems with matches on parts of surrogate pairs.
 *
 * Mappings from multiple characters (code points or codepage state
 * table sequences) must be searched preferring the longest match.
 * For this to work and be efficient, the variable-width table must contain
 * all mappings that contain prefixes of the multiple characters.
 * If an extension table is built on top of a base table in another file
 * and a base table entry is a prefix of a multi-character mapping, then
 * this is an error.
 *
 *
 * Implementation note:
 *
 * Currently, the parser and several checks in the code limit the number
 * of UChars or bytes in a mapping to
 * UCNV_EXT_MAX_UCHARS and UCNV_EXT_MAX_BYTES, respectively,
 * which are output value limits in the data structure.
 *
 * For input, this is not strictly necessary - it is a hard limit only for the
 * buffers in UConverter that are used to store partial matches.
 *
 * Input sequences could otherwise be arbitrarily long if partial matches
 * need not be stored (i.e., if a sequence does not span several buffers with too
 * many units before the last buffer), although then results would differ
 * depending on whether partial matches exceed the limits or not,
 * which depends on the pattern of buffer sizes.
 *
 *
 * Data structure:
 *
 * int32_t indexes[>=32];
 *
 *   Array of indexes and lengths etc. The length of the array is at least 32.
 *   The actual length is stored in indexes[0] to be forward compatible.
 *
 *   Each index to another array is the number of bytes from indexes[].
 *   Each length of an array is the number of array base units in that array.
 *
 *   Some of the structures may not be present, in which case their indexes
 *   and lengths are 0.
 *
 *   Usage of indexes[i]:
 *   [0]  length of indexes[]
 *
 *   // to Unicode table
 *   [1]  index of toUTable[] (array of uint32_t)
 *   [2]  length of toUTable[]
 *   [3]  index of toUUChars[] (array of UChar)
 *   [4]  length of toUUChars[]
 *
 *   // from Unicode table, not for the initial code point
 *   [5]  index of fromUTableUChars[] (array of UChar)
 *   [6]  index of fromUTableValues[] (array of uint32_t)
 *   [7]  length of fromUTableUChars[] and fromUTableValues[]
 *   [8]  index of fromUBytes[] (array of char)
 *   [9]  length of fromUBytes[]
 *
 *   // from Unicode trie for initial-code point lookup
 *   [10] index of fromUStage12[] (combined array of uint16_t for stages 1 & 2)
 *   [11] length of stage 1 portion of fromUStage12[]
 *   [12] length of fromUStage12[]
 *   [13] index of fromUStage3[] (array of uint16_t indexes into fromUStage3b[])
 *   [14] length of fromUStage3[]
 *   [15] index of fromUStage3b[] (array of uint32_t like fromUTableValues[])
 *   [16] length of fromUStage3b[]
 *
 *   [17] Bit field containing numbers of bytes:
 *        31..24 reserved, 0
 *        23..16 maximum input bytes
 *        15.. 8 maximum output bytes
 *         7.. 0 maximum bytes per UChar
 *
 *   [18] Bit field containing numbers of UChars:
 *        31..24 reserved, 0
 *        23..16 maximum input UChars
 *        15.. 8 maximum output UChars
 *         7.. 0 maximum UChars per byte
 *
 *   [19] Bit field containing flags:
 *               (extension table unicodeMask)
 *         1     UCNV_HAS_SURROGATES flag for the extension table
 *         0     UCNV_HAS_SUPPLEMENTARY flag for the extension table
 *
 *   [20]..[30] reserved, 0
 *   [31] number of bytes for the entire extension structure
 *   [>31] reserved; there are indexes[0] indexes
 *
 *
 * uint32_t toUTable[];
 *
 *   Array of byte/value pairs for lookups for toUnicode conversion.
 *   The array is partitioned into sections like collation contraction tables.
 *   Each section contains one word with the number of following words and
 *   a default value for when the lookup in this section yields no match.
 *
 *   A section is sorted in ascending order of input bytes,
 *   allowing for fast linear or binary searches.
 *   The builder may store entries for a contiguous range of byte values
 *   (compare difference between the first and last one with count),
 *   which then allows for direct array access.
 *   The builder should always do this for the initial table section.
 *
 *   Entries may have 0 values, see below.
 *   No two entries in a section have the same byte values.
 *
 *   Each uint32_t contains an input byte value in bits 31..24 and the
 *   corresponding lookup value in bits 23..0.
 *   Interpret the value as follows:
 *     if(value==0) {
 *       no match, see below
 *     } else if(value<0x1f0000) {
 *       partial match - use value as index to the next toUTable section
 *       and match the next unit; (value indexes toUTable[value])
 *     } else {
 *       if(bit 23 set) {
 *         roundtrip;
 *       } else {
 *         fallback;
 *       }
 *       unset value bit 23;
 *       if(value<=0x2fffff) {
 *         (value-0x1f0000) is a code point; (BMP: value<=0x1fffff)
 *       } else {
 *         bits 17..0 (value&0x3ffff) is an index to
 *           the result UChars in toUUChars[]; (0 indexes toUUChars[0])
 *         length of the result=((value>>18)-12); (length=0..19)
 *       }
 *     }
 *
 *   The first word in a section contains the number of following words in the
 *   input byte position (bits 31..24, number=1..0xff).
 *   The value of the initial word is used when the current byte is not found
 *   in this section.
 *   If the value is not 0, then it represents a result as above.
 *   If the value is 0, then the search has to return a shorter match with an
 *   earlier default value as the result, or result in "unmappable" even for the
 *   initial bytes.
 *   If the value is 0 for the initial toUTable entry, then the initial byte
 *   does not start any mapping input.
 *
 *
 * UChar toUUChars[];
 *
 *   Contains toUnicode mapping results, stored as sequences of UChars.
 *   Indexes and lengths stored in the toUTable[].
 *
 *
 * UChar fromUTableUChars[];
 * uint32_t fromUTableValues[];
 *
 *   The fromUTable is split into two arrays, but works otherwise much like
 *   the toUTable. The array is partitioned into sections like collation
 *   contraction tables and toUTable.
 *   A row in the table consists of same-index entries in fromUTableUChars[]
 *   and fromUTableValues[].
 *
 *   Interpret a value as follows:
 *     if(value==0) {
 *       no match, see below
 *     } else if(value<=0xffffff) { (bits 31..24 are 0)
 *       partial match - use value as index to the next fromUTable section
 *       and match the next unit; (value indexes fromUTable[value])
 *     } else {
 *       if(value==0x80000001) {
 *         return no mapping, but request for <subchar1>;
 *       }
 *       if(bit 31 set) {
 *         roundtrip (|0);
 *       } else if(bit 30 set) {
 *         "good one-way" mapping (|4); -- new in ICU4C 51, _MBCSHeader.version 5.4/4.4
 *       } else {
 *         normal fallback (|1);
 *       }
 *       // bit 29 reserved, 0
 *       length=(value>>24)&0x1f; (bits 28..24)
 *       if(length==1..3) {
 *         bits 23..0 contain 1..3 bytes, padded with 00s on the left;
 *       } else {
 *         bits 23..0 (value&0xffffff) is an index to
 *           the result bytes in fromUBytes[]; (0 indexes fromUBytes[0])
 *       }
 *     }
 *
 *   The first pair in a section contains the number of following pairs in the
 *   UChar position (16 bits, number=1..0xffff).
 *   The value of the initial pair is used when the current UChar is not found
 *   in this section.
 *   If the value is not 0, then it represents a result as above.
 *   If the value is 0, then the search has to return a shorter match with an
 *   earlier default value as the result, or result in "unmappable" even for the
 *   initial UChars.
 *
 *   If the from Unicode trie is present, then the from Unicode search tables
 *   are not used for initial code points.
 *   In this case, the first entries (index 0) in the tables are not used
 *   (reserved, set to 0) because a value of 0 is used in trie results
 *   to indicate no mapping.
 *
 *
 * uint16_t fromUStage12[];
 *
 *   Stages 1 & 2 of a trie that maps an initial code point.
 *   Indexes in stage 1 are all offset by the length of stage 1 so that the
 *   same array pointer can be used for both stages.
 *   If (c>>10)>=(length of stage 1) then c does not start any mapping.
 *   Same bit distribution as for regular conversion tries.
 *
 *
 * uint16_t fromUStage3[];
 * uint32_t fromUStage3b[];
 *
 *   Stage 3 of the trie. The first array simply contains indexes to the second,
 *   which contains words in the same format as fromUTableValues[].
 *   Use a stage 3 granularity of 4, which allows for 256k stage 3 entries,
 *   and 16-bit entries in stage 3 allow for 64k stage 3b entries.
 *   The stage 3 granularity means that the stage 2 entry needs to be left-shifted.
 *
 *   Two arrays are used because it is expected that more than half of the stage 3
 *   entries will be zero. The 16-bit index stage 3 array saves space even
 *   considering storing a total of 6 bytes per non-zero entry in both arrays
 *   together.
 *   Using a stage 3 granularity of >1 diminishes the compactability in that stage
 *   but provides a larger effective addressing space in stage 2.
 *   All but the final result stage use 16-bit entries to save space.
 *
 *   fromUStage3b[] contains a zero for "no mapping" at its index 0,
 *   and may contain UCNV_EXT_FROM_U_SUBCHAR1 at index 1 for "<subchar1> SUB mapping"
 *   (i.e., "no mapping" with preference for <subchar1> rather than <subchar>),
 *   and all other items are unique non-zero results.
 *
 *   The default value of a fromUTableValues[] section that is referenced
 *   _directly_ from a fromUStage3b[] item may also be UCNV_EXT_FROM_U_SUBCHAR1,
 *   but this value must not occur anywhere else in fromUTableValues[]
 *   because "no mapping" is always a property of a single code point,
 *   never of multiple.
 *
 *
 * char fromUBytes[];
 *
 *   Contains fromUnicode mapping results, stored as sequences of chars.
 *   Indexes and lengths stored in the fromUTableValues[].
 */
enum {
    UCNV_EXT_INDEXES_LENGTH,            /* 0 */

    UCNV_EXT_TO_U_INDEX,                /* 1 */
    UCNV_EXT_TO_U_LENGTH,
    UCNV_EXT_TO_U_UCHARS_INDEX,
    UCNV_EXT_TO_U_UCHARS_LENGTH,

    UCNV_EXT_FROM_U_UCHARS_INDEX,       /* 5 */
    UCNV_EXT_FROM_U_VALUES_INDEX,
    UCNV_EXT_FROM_U_LENGTH,
    UCNV_EXT_FROM_U_BYTES_INDEX,
    UCNV_EXT_FROM_U_BYTES_LENGTH,

    UCNV_EXT_FROM_U_STAGE_12_INDEX,     /* 10 */
    UCNV_EXT_FROM_U_STAGE_1_LENGTH,
    UCNV_EXT_FROM_U_STAGE_12_LENGTH,
    UCNV_EXT_FROM_U_STAGE_3_INDEX,
    UCNV_EXT_FROM_U_STAGE_3_LENGTH,
    UCNV_EXT_FROM_U_STAGE_3B_INDEX,
    UCNV_EXT_FROM_U_STAGE_3B_LENGTH,

    UCNV_EXT_COUNT_BYTES,               /* 17 */
    UCNV_EXT_COUNT_UCHARS,
    UCNV_EXT_FLAGS,

    UCNV_EXT_RESERVED_INDEX,            /* 20, moves with additional indexes */

    UCNV_EXT_SIZE=31,
    UCNV_EXT_INDEXES_MIN_LENGTH=32
};

/* get the pointer to an extension array from indexes[index] */
#define UCNV_EXT_ARRAY(indexes, index, itemType) \
    ((const itemType *)((const char *)(indexes)+(indexes)[index]))

#define UCNV_GET_MAX_BYTES_PER_UCHAR(indexes) \
    ((indexes)[UCNV_EXT_COUNT_BYTES]&0xff)

/* internal API ------------------------------------------------------------- */

U_CFUNC UBool
ucnv_extInitialMatchToU(UConverter *cnv, const int32_t *cx,
                        int32_t firstLength,
                        const char **src, const char *srcLimit,
                        UChar **target, const UChar *targetLimit,
                        int32_t **offsets, int32_t srcIndex,
                        UBool flush,
                        UErrorCode *pErrorCode);

U_CFUNC UChar32
ucnv_extSimpleMatchToU(const int32_t *cx,
                       const char *source, int32_t length,
                       UBool useFallback);

U_CFUNC void
ucnv_extContinueMatchToU(UConverter *cnv,
                         UConverterToUnicodeArgs *pArgs, int32_t srcIndex,
                         UErrorCode *pErrorCode);


U_CFUNC UBool
ucnv_extInitialMatchFromU(UConverter *cnv, const int32_t *cx,
                          UChar32 cp,
                          const UChar **src, const UChar *srcLimit,
                          char **target, const char *targetLimit,
                          int32_t **offsets, int32_t srcIndex,
                          UBool flush,
                          UErrorCode *pErrorCode);

U_CFUNC int32_t
ucnv_extSimpleMatchFromU(const int32_t *cx,
                         UChar32 cp, uint32_t *pValue,
                         UBool useFallback);

U_CFUNC void
ucnv_extContinueMatchFromU(UConverter *cnv,
                           UConverterFromUnicodeArgs *pArgs, int32_t srcIndex,
                           UErrorCode *pErrorCode);

/*
 * Add code points and strings to the set according to the extension mappings.
 * Limitation on the UConverterSetFilter:
 * The filters currently assume that they are used with 1:1 mappings.
 * They only apply to single input code points, and then they pass through
 * only mappings with single-charset-code results.
 * For example, the Shift-JIS filter only works for 2-byte results and tests
 * that those 2 bytes are in the JIS X 0208 range of Shift-JIS.
 */
U_CFUNC void
ucnv_extGetUnicodeSet(const UConverterSharedData *sharedData,
                      const USetAdder *sa,
                      UConverterUnicodeSet which,
                      UConverterSetFilter filter,
                      UErrorCode *pErrorCode);

/* toUnicode helpers -------------------------------------------------------- */

#define UCNV_EXT_TO_U_BYTE_SHIFT 24
#define UCNV_EXT_TO_U_VALUE_MASK 0xffffff
#define UCNV_EXT_TO_U_MIN_CODE_POINT 0x1f0000
#define UCNV_EXT_TO_U_MAX_CODE_POINT 0x2fffff
#define UCNV_EXT_TO_U_ROUNDTRIP_FLAG ((uint32_t)1<<23)
#define UCNV_EXT_TO_U_INDEX_MASK 0x3ffff
#define UCNV_EXT_TO_U_LENGTH_SHIFT 18
#define UCNV_EXT_TO_U_LENGTH_OFFSET 12

/* maximum number of indexed UChars */
#define UCNV_EXT_MAX_UCHARS 19

#define UCNV_EXT_TO_U_MAKE_WORD(byte, value) (((uint32_t)(byte)<<UCNV_EXT_TO_U_BYTE_SHIFT)|(value))

#define UCNV_EXT_TO_U_GET_BYTE(word) ((word)>>UCNV_EXT_TO_U_BYTE_SHIFT)
#define UCNV_EXT_TO_U_GET_VALUE(word) ((word)&UCNV_EXT_TO_U_VALUE_MASK)

#define UCNV_EXT_TO_U_IS_PARTIAL(value) ((value)<UCNV_EXT_TO_U_MIN_CODE_POINT)
#define UCNV_EXT_TO_U_GET_PARTIAL_INDEX(value) (value)

#define UCNV_EXT_TO_U_IS_ROUNDTRIP(value) (((value)&UCNV_EXT_TO_U_ROUNDTRIP_FLAG)!=0)
#define UCNV_EXT_TO_U_MASK_ROUNDTRIP(value) ((value)&~UCNV_EXT_TO_U_ROUNDTRIP_FLAG)

/* use after masking off the roundtrip flag */
#define UCNV_EXT_TO_U_IS_CODE_POINT(value) ((value)<=UCNV_EXT_TO_U_MAX_CODE_POINT)
#define UCNV_EXT_TO_U_GET_CODE_POINT(value) ((value)-UCNV_EXT_TO_U_MIN_CODE_POINT)

#define UCNV_EXT_TO_U_GET_INDEX(value) ((value)&UCNV_EXT_TO_U_INDEX_MASK)
#define UCNV_EXT_TO_U_GET_LENGTH(value) (((value)>>UCNV_EXT_TO_U_LENGTH_SHIFT)-UCNV_EXT_TO_U_LENGTH_OFFSET)

/* fromUnicode helpers ------------------------------------------------------ */

/* most trie constants are shared with ucnvmbcs.h */

/* see similar utrie.h UTRIE_INDEX_SHIFT and UTRIE_DATA_GRANULARITY */
#define UCNV_EXT_STAGE_2_LEFT_SHIFT 2
#define UCNV_EXT_STAGE_3_GRANULARITY 4

/* trie access, returns the stage 3 value=index to stage 3b; s1Index=c>>10 */
#define UCNV_EXT_FROM_U(stage12, stage3, s1Index, c) \
    (stage3)[ ((int32_t)(stage12)[ (stage12)[s1Index] +(((c)>>4)&0x3f) ]<<UCNV_EXT_STAGE_2_LEFT_SHIFT) +((c)&0xf) ]

#define UCNV_EXT_FROM_U_LENGTH_SHIFT 24
#define UCNV_EXT_FROM_U_ROUNDTRIP_FLAG ((uint32_t)1<<31)
#define UCNV_EXT_FROM_U_GOOD_ONE_WAY_FLAG 0x40000000
#define UCNV_EXT_FROM_U_STATUS_MASK 0xc0000000
#define UCNV_EXT_FROM_U_RESERVED_MASK 0x20000000
#define UCNV_EXT_FROM_U_DATA_MASK 0xffffff

/* special value for "no mapping" to <subchar1> (impossible roundtrip to 0 bytes, value 01) */
#define UCNV_EXT_FROM_U_SUBCHAR1 0x80000001

/* at most 3 bytes in the lower part of the value */
#define UCNV_EXT_FROM_U_MAX_DIRECT_LENGTH 3

/* maximum number of indexed bytes */
#define UCNV_EXT_MAX_BYTES 0x1f

#define UCNV_EXT_FROM_U_IS_PARTIAL(value) (((value)>>UCNV_EXT_FROM_U_LENGTH_SHIFT)==0)
#define UCNV_EXT_FROM_U_GET_PARTIAL_INDEX(value) (value)

#define UCNV_EXT_FROM_U_IS_ROUNDTRIP(value) (((value)&UCNV_EXT_FROM_U_ROUNDTRIP_FLAG)!=0)
#define UCNV_EXT_FROM_U_MASK_ROUNDTRIP(value) ((value)&~UCNV_EXT_FROM_U_ROUNDTRIP_FLAG)

/* get length; masks away all other bits */
#define UCNV_EXT_FROM_U_GET_LENGTH(value) (int32_t)(((value)>>UCNV_EXT_FROM_U_LENGTH_SHIFT)&UCNV_EXT_MAX_BYTES)

/* get bytes or bytes index */
#define UCNV_EXT_FROM_U_GET_DATA(value) ((value)&UCNV_EXT_FROM_U_DATA_MASK)

#endif

#endif
