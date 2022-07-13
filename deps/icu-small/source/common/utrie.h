// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 2001-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  utrie.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001nov08
*   created by: Markus W. Scherer
*/

#ifndef __UTRIE_H__
#define __UTRIE_H__

#include "unicode/utypes.h"
#include "unicode/utf16.h"

U_CDECL_BEGIN

/**
 * \file
 *
 * This is a common implementation of a "folded" trie.
 * It is a kind of compressed, serializable table of 16- or 32-bit values associated with
 * Unicode code points (0..0x10ffff).
 *
 * This implementation is optimized for getting values while walking forward
 * through a UTF-16 string.
 * Therefore, the simplest and fastest access macros are the
 * _FROM_LEAD() and _FROM_OFFSET_TRAIL() macros.
 *
 * The _FROM_BMP() macros are a little more complicated; they get values
 * even for lead surrogate code _points_, while the _FROM_LEAD() macros
 * get special "folded" values for lead surrogate code _units_ if
 * there is relevant data associated with them.
 * From such a folded value, an offset needs to be extracted to supply
 * to the _FROM_OFFSET_TRAIL() macros.
 *
 * Most of the more complex (and more convenient) functions/macros call a callback function
 * to get that offset from the folded value for a lead surrogate unit.
 */

/**
 * Trie constants, defining shift widths, index array lengths, etc.
 */
enum {
    /** Shift size for shifting right the input index. 1..9 */
    UTRIE_SHIFT=5,

    /** Number of data values in a stage 2 (data array) block. 2, 4, 8, .., 0x200 */
    UTRIE_DATA_BLOCK_LENGTH=1<<UTRIE_SHIFT,

    /** Mask for getting the lower bits from the input index. */
    UTRIE_MASK=UTRIE_DATA_BLOCK_LENGTH-1,

    /**
     * Lead surrogate code points' index displacement in the index array.
     * 0x10000-0xd800=0x2800
     */
    UTRIE_LEAD_INDEX_DISP=0x2800>>UTRIE_SHIFT,

    /**
     * Shift size for shifting left the index array values.
     * Increases possible data size with 16-bit index values at the cost
     * of compactability.
     * This requires blocks of stage 2 data to be aligned by UTRIE_DATA_GRANULARITY.
     * 0..UTRIE_SHIFT
     */
    UTRIE_INDEX_SHIFT=2,

    /** The alignment size of a stage 2 data block. Also the granularity for compaction. */
    UTRIE_DATA_GRANULARITY=1<<UTRIE_INDEX_SHIFT,

    /** Number of bits of a trail surrogate that are used in index table lookups. */
    UTRIE_SURROGATE_BLOCK_BITS=10-UTRIE_SHIFT,

    /**
     * Number of index (stage 1) entries per lead surrogate.
     * Same as number of index entries for 1024 trail surrogates,
     * ==0x400>>UTRIE_SHIFT
     */
    UTRIE_SURROGATE_BLOCK_COUNT=(1<<UTRIE_SURROGATE_BLOCK_BITS),

    /** Length of the BMP portion of the index (stage 1) array. */
    UTRIE_BMP_INDEX_LENGTH=0x10000>>UTRIE_SHIFT
};

/**
 * Length of the index (stage 1) array before folding.
 * Maximum number of Unicode code points (0x110000) shifted right by UTRIE_SHIFT.
 */
#define UTRIE_MAX_INDEX_LENGTH (0x110000>>UTRIE_SHIFT)

/**
 * Maximum length of the runtime data (stage 2) array.
 * Limited by 16-bit index values that are left-shifted by UTRIE_INDEX_SHIFT.
 */
#define UTRIE_MAX_DATA_LENGTH (0x10000<<UTRIE_INDEX_SHIFT)

/**
 * Maximum length of the build-time data (stage 2) array.
 * The maximum length is 0x110000+UTRIE_DATA_BLOCK_LENGTH+0x400.
 * (Number of Unicode code points + one all-initial-value block +
 *  possible duplicate entries for 1024 lead surrogates.)
 */
#define UTRIE_MAX_BUILD_TIME_DATA_LENGTH (0x110000+UTRIE_DATA_BLOCK_LENGTH+0x400)

/**
 * Number of bytes for a dummy trie.
 * A dummy trie is an empty runtime trie, used when a real data trie cannot
 * be loaded.
 * The number of bytes works for Latin-1-linear tries with 32-bit data
 * (worst case).
 *
 * Calculation:
 *   BMP index + 1 index block for lead surrogate code points +
 *   Latin-1-linear array + 1 data block for lead surrogate code points
 *
 * Latin-1: if(UTRIE_SHIFT<=8) { 256 } else { included in first data block }
 *
 * @see utrie_unserializeDummy
 */
#define UTRIE_DUMMY_SIZE ((UTRIE_BMP_INDEX_LENGTH+UTRIE_SURROGATE_BLOCK_COUNT)*2+(UTRIE_SHIFT<=8?256:UTRIE_DATA_BLOCK_LENGTH)*4+UTRIE_DATA_BLOCK_LENGTH*4)

/**
 * Runtime UTrie callback function.
 * Extract from a lead surrogate's data the
 * index array offset of the indexes for that lead surrogate.
 *
 * @param data data value for a surrogate from the trie, including the folding offset
 * @return offset>=UTRIE_BMP_INDEX_LENGTH, or 0 if there is no data for the lead surrogate
 */
typedef int32_t U_CALLCONV
UTrieGetFoldingOffset(uint32_t data);

/**
 * Run-time Trie structure.
 *
 * Either the data table is 16 bits wide and accessed via the index
 * pointer, with each index item increased by indexLength;
 * in this case, data32==NULL.
 *
 * Or the data table is 32 bits wide and accessed via the data32 pointer.
 */
struct UTrie {
    const uint16_t *index;
    const uint32_t *data32; /* NULL if 16b data is used via index */

    /**
     * This function is not used in _FROM_LEAD, _FROM_BMP, and _FROM_OFFSET_TRAIL macros.
     * If convenience macros like _GET16 or _NEXT32 are used, this function must be set.
     *
     * utrie_unserialize() sets a default function which simply returns
     * the lead surrogate's value itself - which is the inverse of the default
     * folding function used by utrie_serialize().
     *
     * @see UTrieGetFoldingOffset
     */
    UTrieGetFoldingOffset *getFoldingOffset;

    int32_t indexLength, dataLength;
    uint32_t initialValue;
    UBool isLatin1Linear;
};

#ifndef __UTRIE2_H__
typedef struct UTrie UTrie;
#endif

/** Internal trie getter from an offset (0 if c16 is a BMP/lead units) and a 16-bit unit */
#define _UTRIE_GET_RAW(trie, data, offset, c16) \
    (trie)->data[ \
        ((int32_t)((trie)->index[(offset)+((c16)>>UTRIE_SHIFT)])<<UTRIE_INDEX_SHIFT)+ \
        ((c16)&UTRIE_MASK) \
    ]

/** Internal trie getter from a pair of surrogates */
#define _UTRIE_GET_FROM_PAIR(trie, data, c, c2, result, resultType) UPRV_BLOCK_MACRO_BEGIN { \
    int32_t __offset; \
\
    /* get data for lead surrogate */ \
    (result)=_UTRIE_GET_RAW((trie), data, 0, (c)); \
    __offset=(trie)->getFoldingOffset(result); \
\
    /* get the real data from the folded lead/trail units */ \
    if(__offset>0) { \
        (result)=_UTRIE_GET_RAW((trie), data, __offset, (c2)&0x3ff); \
    } else { \
        (result)=(resultType)((trie)->initialValue); \
    } \
} UPRV_BLOCK_MACRO_END

/** Internal trie getter from a BMP code point, treating a lead surrogate as a normal code point */
#define _UTRIE_GET_FROM_BMP(trie, data, c16) \
    _UTRIE_GET_RAW(trie, data, 0xd800<=(c16) && (c16)<=0xdbff ? UTRIE_LEAD_INDEX_DISP : 0, c16)

/**
 * Internal trie getter from a code point.
 * Could be faster(?) but longer with
 *   if((c32)<=0xd7ff) { (result)=_UTRIE_GET_RAW(trie, data, 0, c32); }
 */
#define _UTRIE_GET(trie, data, c32, result, resultType) UPRV_BLOCK_MACRO_BEGIN { \
    if((uint32_t)(c32)<=0xffff) { \
        /* BMP code points */ \
        (result)=_UTRIE_GET_FROM_BMP(trie, data, c32); \
    } else if((uint32_t)(c32)<=0x10ffff) { \
        /* supplementary code point */ \
        UChar __lead16=U16_LEAD(c32); \
        _UTRIE_GET_FROM_PAIR(trie, data, __lead16, c32, result, resultType); \
    } else { \
        /* out of range */ \
        (result)=(resultType)((trie)->initialValue); \
    } \
} UPRV_BLOCK_MACRO_END

/** Internal next-post-increment: get the next code point (c, c2) and its data */
#define _UTRIE_NEXT(trie, data, src, limit, c, c2, result, resultType) UPRV_BLOCK_MACRO_BEGIN { \
    (c)=*(src)++; \
    if(!U16_IS_LEAD(c)) { \
        (c2)=0; \
        (result)=_UTRIE_GET_RAW((trie), data, 0, (c)); \
    } else if((src)!=(limit) && U16_IS_TRAIL((c2)=*(src))) { \
        ++(src); \
        _UTRIE_GET_FROM_PAIR((trie), data, (c), (c2), (result), resultType); \
    } else { \
        /* unpaired lead surrogate code point */ \
        (c2)=0; \
        (result)=_UTRIE_GET_RAW((trie), data, UTRIE_LEAD_INDEX_DISP, (c)); \
    } \
} UPRV_BLOCK_MACRO_END

/** Internal previous: get the previous code point (c, c2) and its data */
#define _UTRIE_PREVIOUS(trie, data, start, src, c, c2, result, resultType) UPRV_BLOCK_MACRO_BEGIN { \
    (c)=*--(src); \
    if(!U16_IS_SURROGATE(c)) { \
        (c2)=0; \
        (result)=_UTRIE_GET_RAW((trie), data, 0, (c)); \
    } else if(!U16_IS_SURROGATE_LEAD(c)) { \
        /* trail surrogate */ \
        if((start)!=(src) && U16_IS_LEAD((c2)=*((src)-1))) { \
            --(src); \
            (result)=(c); (c)=(c2); (c2)=(UChar)(result); /* swap c, c2 */ \
            _UTRIE_GET_FROM_PAIR((trie), data, (c), (c2), (result), resultType); \
        } else { \
            /* unpaired trail surrogate code point */ \
            (c2)=0; \
            (result)=_UTRIE_GET_RAW((trie), data, 0, (c)); \
        } \
    } else { \
        /* unpaired lead surrogate code point */ \
        (c2)=0; \
        (result)=_UTRIE_GET_RAW((trie), data, UTRIE_LEAD_INDEX_DISP, (c)); \
    } \
} UPRV_BLOCK_MACRO_END

/* Public UTrie API ---------------------------------------------------------*/

/**
 * Get a pointer to the contiguous part of the data array
 * for the Latin-1 range (U+0000..U+00ff).
 * Must be used only if the Latin-1 range is in fact linear
 * (trie->isLatin1Linear).
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @return (const uint16_t *) pointer to values for Latin-1 code points
 */
#define UTRIE_GET16_LATIN1(trie) ((trie)->index+(trie)->indexLength+UTRIE_DATA_BLOCK_LENGTH)

/**
 * Get a pointer to the contiguous part of the data array
 * for the Latin-1 range (U+0000..U+00ff).
 * Must be used only if the Latin-1 range is in fact linear
 * (trie->isLatin1Linear).
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @return (const uint32_t *) pointer to values for Latin-1 code points
 */
#define UTRIE_GET32_LATIN1(trie) ((trie)->data32+UTRIE_DATA_BLOCK_LENGTH)

/**
 * Get a 16-bit trie value from a BMP code point (UChar, <=U+ffff).
 * c16 may be a lead surrogate, which may have a value including a folding offset.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param c16 (UChar, in) the input BMP code point
 * @return (uint16_t) trie lookup result
 */
#define UTRIE_GET16_FROM_LEAD(trie, c16) _UTRIE_GET_RAW(trie, index, 0, c16)

/**
 * Get a 32-bit trie value from a BMP code point (UChar, <=U+ffff).
 * c16 may be a lead surrogate, which may have a value including a folding offset.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param c16 (UChar, in) the input BMP code point
 * @return (uint32_t) trie lookup result
 */
#define UTRIE_GET32_FROM_LEAD(trie, c16) _UTRIE_GET_RAW(trie, data32, 0, c16)

/**
 * Get a 16-bit trie value from a BMP code point (UChar, <=U+ffff).
 * Even lead surrogate code points are treated as normal code points,
 * with unfolded values that may differ from _FROM_LEAD() macro results for them.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param c16 (UChar, in) the input BMP code point
 * @return (uint16_t) trie lookup result
 */
#define UTRIE_GET16_FROM_BMP(trie, c16) _UTRIE_GET_FROM_BMP(trie, index, c16)

/**
 * Get a 32-bit trie value from a BMP code point (UChar, <=U+ffff).
 * Even lead surrogate code points are treated as normal code points,
 * with unfolded values that may differ from _FROM_LEAD() macro results for them.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param c16 (UChar, in) the input BMP code point
 * @return (uint32_t) trie lookup result
 */
#define UTRIE_GET32_FROM_BMP(trie, c16) _UTRIE_GET_FROM_BMP(trie, data32, c16)

/**
 * Get a 16-bit trie value from a code point.
 * Even lead surrogate code points are treated as normal code points,
 * with unfolded values that may differ from _FROM_LEAD() macro results for them.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param c32 (UChar32, in) the input code point
 * @param result (uint16_t, out) uint16_t variable for the trie lookup result
 */
#define UTRIE_GET16(trie, c32, result) _UTRIE_GET(trie, index, c32, result, uint16_t)

/**
 * Get a 32-bit trie value from a code point.
 * Even lead surrogate code points are treated as normal code points,
 * with unfolded values that may differ from _FROM_LEAD() macro results for them.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param c32 (UChar32, in) the input code point
 * @param result (uint32_t, out) uint32_t variable for the trie lookup result
 */
#define UTRIE_GET32(trie, c32, result) _UTRIE_GET(trie, data32, c32, result, uint32_t)

/**
 * Get the next code point (c, c2), post-increment src,
 * and get a 16-bit value from the trie.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param src (const UChar *, in/out) the source text pointer
 * @param limit (const UChar *, in) the limit pointer for the text, or NULL
 * @param c (UChar, out) variable for the BMP or lead code unit
 * @param c2 (UChar, out) variable for 0 or the trail code unit
 * @param result (uint16_t, out) uint16_t variable for the trie lookup result
 */
#define UTRIE_NEXT16(trie, src, limit, c, c2, result) _UTRIE_NEXT(trie, index, src, limit, c, c2, result, uint16_t)

/**
 * Get the next code point (c, c2), post-increment src,
 * and get a 32-bit value from the trie.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param src (const UChar *, in/out) the source text pointer
 * @param limit (const UChar *, in) the limit pointer for the text, or NULL
 * @param c (UChar, out) variable for the BMP or lead code unit
 * @param c2 (UChar, out) variable for 0 or the trail code unit
 * @param result (uint32_t, out) uint32_t variable for the trie lookup result
 */
#define UTRIE_NEXT32(trie, src, limit, c, c2, result) _UTRIE_NEXT(trie, data32, src, limit, c, c2, result, uint32_t)

/**
 * Get the previous code point (c, c2), pre-decrement src,
 * and get a 16-bit value from the trie.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param start (const UChar *, in) the start pointer for the text, or NULL
 * @param src (const UChar *, in/out) the source text pointer
 * @param c (UChar, out) variable for the BMP or lead code unit
 * @param c2 (UChar, out) variable for 0 or the trail code unit
 * @param result (uint16_t, out) uint16_t variable for the trie lookup result
 */
#define UTRIE_PREVIOUS16(trie, start, src, c, c2, result) _UTRIE_PREVIOUS(trie, index, start, src, c, c2, result, uint16_t)

/**
 * Get the previous code point (c, c2), pre-decrement src,
 * and get a 32-bit value from the trie.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param start (const UChar *, in) the start pointer for the text, or NULL
 * @param src (const UChar *, in/out) the source text pointer
 * @param c (UChar, out) variable for the BMP or lead code unit
 * @param c2 (UChar, out) variable for 0 or the trail code unit
 * @param result (uint32_t, out) uint32_t variable for the trie lookup result
 */
#define UTRIE_PREVIOUS32(trie, start, src, c, c2, result) _UTRIE_PREVIOUS(trie, data32, start, src, c, c2, result, uint32_t)

/**
 * Get a 16-bit trie value from a pair of surrogates.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param c (UChar, in) a lead surrogate
 * @param c2 (UChar, in) a trail surrogate
 * @param result (uint16_t, out) uint16_t variable for the trie lookup result
 */
#define UTRIE_GET16_FROM_PAIR(trie, c, c2, result) _UTRIE_GET_FROM_PAIR(trie, index, c, c2, result, uint16_t)

/**
 * Get a 32-bit trie value from a pair of surrogates.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param c (UChar, in) a lead surrogate
 * @param c2 (UChar, in) a trail surrogate
 * @param result (uint32_t, out) uint32_t variable for the trie lookup result
 */
#define UTRIE_GET32_FROM_PAIR(trie, c, c2, result) _UTRIE_GET_FROM_PAIR(trie, data32, c, c2, result, uint32_t)

/**
 * Get a 16-bit trie value from a folding offset (from the value of a lead surrogate)
 * and a trail surrogate.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param offset (int32_t, in) the folding offset from the value of a lead surrogate
 * @param c2 (UChar, in) a trail surrogate (only the 10 low bits are significant)
 * @return (uint16_t) trie lookup result
 */
#define UTRIE_GET16_FROM_OFFSET_TRAIL(trie, offset, c2) _UTRIE_GET_RAW(trie, index, offset, (c2)&0x3ff)

/**
 * Get a 32-bit trie value from a folding offset (from the value of a lead surrogate)
 * and a trail surrogate.
 *
 * @param trie (const UTrie *, in) a pointer to the runtime trie structure
 * @param offset (int32_t, in) the folding offset from the value of a lead surrogate
 * @param c2 (UChar, in) a trail surrogate (only the 10 low bits are significant)
 * @return (uint32_t) trie lookup result
 */
#define UTRIE_GET32_FROM_OFFSET_TRAIL(trie, offset, c2) _UTRIE_GET_RAW(trie, data32, offset, (c2)&0x3ff)

/* enumeration callback types */

/**
 * Callback from utrie_enum(), extracts a uint32_t value from a
 * trie value. This value will be passed on to the UTrieEnumRange function.
 *
 * @param context an opaque pointer, as passed into utrie_enum()
 * @param value a value from the trie
 * @return the value that is to be passed on to the UTrieEnumRange function
 */
typedef uint32_t U_CALLCONV
UTrieEnumValue(const void *context, uint32_t value);

/**
 * Callback from utrie_enum(), is called for each contiguous range
 * of code points with the same value as retrieved from the trie and
 * transformed by the UTrieEnumValue function.
 *
 * The callback function can stop the enumeration by returning false.
 *
 * @param context an opaque pointer, as passed into utrie_enum()
 * @param start the first code point in a contiguous range with value
 * @param limit one past the last code point in a contiguous range with value
 * @param value the value that is set for all code points in [start..limit[
 * @return false to stop the enumeration
 */
typedef UBool U_CALLCONV
UTrieEnumRange(const void *context, UChar32 start, UChar32 limit, uint32_t value);

/**
 * Enumerate efficiently all values in a trie.
 * For each entry in the trie, the value to be delivered is passed through
 * the UTrieEnumValue function.
 * The value is unchanged if that function pointer is NULL.
 *
 * For each contiguous range of code points with a given value,
 * the UTrieEnumRange function is called.
 *
 * @param trie a pointer to the runtime trie structure
 * @param enumValue a pointer to a function that may transform the trie entry value,
 *                  or NULL if the values from the trie are to be used directly
 * @param enumRange a pointer to a function that is called for each contiguous range
 *                  of code points with the same value
 * @param context an opaque pointer that is passed on to the callback functions
 */
U_CAPI void U_EXPORT2
utrie_enum(const UTrie *trie,
           UTrieEnumValue *enumValue, UTrieEnumRange *enumRange, const void *context);

/**
 * Unserialize a trie from 32-bit-aligned memory.
 * Inverse of utrie_serialize().
 * Fills the UTrie runtime trie structure with the settings for the trie data.
 *
 * @param trie a pointer to the runtime trie structure
 * @param data a pointer to 32-bit-aligned memory containing trie data
 * @param length the number of bytes available at data
 * @param pErrorCode an in/out ICU UErrorCode
 * @return the number of bytes at data taken up by the trie data
 */
U_CAPI int32_t U_EXPORT2
utrie_unserialize(UTrie *trie, const void *data, int32_t length, UErrorCode *pErrorCode);

/**
 * "Unserialize" a dummy trie.
 * A dummy trie is an empty runtime trie, used when a real data trie cannot
 * be loaded.
 *
 * The input memory is filled so that the trie always returns the initialValue,
 * or the leadUnitValue for lead surrogate code points.
 * The Latin-1 part is always set up to be linear.
 *
 * @param trie a pointer to the runtime trie structure
 * @param data a pointer to 32-bit-aligned memory to be filled with the dummy trie data
 * @param length the number of bytes available at data (recommended to use UTRIE_DUMMY_SIZE)
 * @param initialValue the initial value that is set for all code points
 * @param leadUnitValue the value for lead surrogate code _units_ that do not
 *                      have associated supplementary data
 * @param pErrorCode an in/out ICU UErrorCode
 *
 * @see UTRIE_DUMMY_SIZE
 * @see utrie_open
 */
U_CAPI int32_t U_EXPORT2
utrie_unserializeDummy(UTrie *trie,
                       void *data, int32_t length,
                       uint32_t initialValue, uint32_t leadUnitValue,
                       UBool make16BitTrie,
                       UErrorCode *pErrorCode);

/**
 * Default implementation for UTrie.getFoldingOffset, set automatically by
 * utrie_unserialize().
 * Simply returns the lead surrogate's value itself - which is the inverse
 * of the default folding function used by utrie_serialize().
 * Exported for static const UTrie structures.
 *
 * @see UTrieGetFoldingOffset
 */
U_CAPI int32_t U_EXPORT2
utrie_defaultGetFoldingOffset(uint32_t data);

/* Building a trie ----------------------------------------------------------*/

/**
 * Build-time trie structure.
 * Opaque definition, here only to make fillIn parameters possible
 * for utrie_open() and utrie_clone().
 */
struct UNewTrie {
    /**
     * Index values at build-time are 32 bits wide for easier processing.
     * Bit 31 is set if the data block is used by multiple index values (from utrie_setRange()).
     */
    int32_t index[UTRIE_MAX_INDEX_LENGTH+UTRIE_SURROGATE_BLOCK_COUNT];
    uint32_t *data;

    uint32_t leadUnitValue;
    int32_t indexLength, dataCapacity, dataLength;
    UBool isAllocated, isDataAllocated;
    UBool isLatin1Linear, isCompacted;

    /**
     * Map of adjusted indexes, used in utrie_compact().
     * Maps from original indexes to new ones.
     */
    int32_t map[UTRIE_MAX_BUILD_TIME_DATA_LENGTH>>UTRIE_SHIFT];
};

typedef struct UNewTrie UNewTrie;

/**
 * Build-time trie callback function, used with utrie_serialize().
 * This function calculates a lead surrogate's value including a folding offset
 * from the 1024 supplementary code points [start..start+1024[ .
 * It is U+10000 <= start <= U+10fc00 and (start&0x3ff)==0.
 *
 * The folding offset is provided by the caller.
 * It is offset=UTRIE_BMP_INDEX_LENGTH+n*UTRIE_SURROGATE_BLOCK_COUNT with n=0..1023.
 * Instead of the offset itself, n can be stored in 10 bits -
 * or fewer if it can be assumed that few lead surrogates have associated data.
 *
 * The returned value must be
 * - not zero if and only if there is relevant data
 *   for the corresponding 1024 supplementary code points
 * - such that UTrie.getFoldingOffset(UNewTrieGetFoldedValue(..., offset))==offset
 *
 * @return a folded value, or 0 if there is no relevant data for the lead surrogate.
 */
typedef uint32_t U_CALLCONV
UNewTrieGetFoldedValue(UNewTrie *trie, UChar32 start, int32_t offset);

/**
 * Open a build-time trie structure.
 * The size of the build-time data array is specified to avoid allocating a large
 * array in all cases. The array itself can also be passed in.
 *
 * Although the trie is never fully expanded to a linear array, especially when
 * utrie_setRange32() is used, the data array could be large during build time.
 * The maximum length is
 * UTRIE_MAX_BUILD_TIME_DATA_LENGTH=0x110000+UTRIE_DATA_BLOCK_LENGTH+0x400.
 * (Number of Unicode code points + one all-initial-value block +
 *  possible duplicate entries for 1024 lead surrogates.)
 * (UTRIE_DATA_BLOCK_LENGTH<=0x200 in all cases.)
 *
 * @param fillIn a pointer to a UNewTrie structure to be initialized (will not be released), or
 *               NULL if one is to be allocated
 * @param aliasData a pointer to a data array to be used (will not be released), or
 *                  NULL if one is to be allocated
 * @param maxDataLength the capacity of aliasData (if not NULL) or
 *                      the length of the data array to be allocated
 * @param initialValue the initial value that is set for all code points
 * @param leadUnitValue the value for lead surrogate code _units_ that do not
 *                      have associated supplementary data
 * @param latin1Linear a flag indicating whether the Latin-1 range is to be allocated and
 *                     kept in a linear, contiguous part of the data array
 * @return a pointer to the initialized fillIn or the allocated and initialized new UNewTrie
 */
U_CAPI UNewTrie * U_EXPORT2
utrie_open(UNewTrie *fillIn,
           uint32_t *aliasData, int32_t maxDataLength,
           uint32_t initialValue, uint32_t leadUnitValue,
           UBool latin1Linear);

/**
 * Clone a build-time trie structure with all entries.
 *
 * @param fillIn like in utrie_open()
 * @param other the build-time trie structure to clone
 * @param aliasData like in utrie_open(),
 *                  used if aliasDataLength>=(capacity of other's data array)
 * @param aliasDataLength the length of aliasData
 * @return a pointer to the initialized fillIn or the allocated and initialized new UNewTrie
 */
U_CAPI UNewTrie * U_EXPORT2
utrie_clone(UNewTrie *fillIn, const UNewTrie *other, uint32_t *aliasData, int32_t aliasDataLength);

/**
 * Close a build-time trie structure, and release memory
 * that was allocated by utrie_open() or utrie_clone().
 *
 * @param trie the build-time trie
 */
U_CAPI void U_EXPORT2
utrie_close(UNewTrie *trie);

/**
 * Get the data array of a build-time trie.
 * The data may be modified, but entries that are equal before
 * must still be equal after modification.
 *
 * @param trie the build-time trie
 * @param pLength (out) a pointer to a variable that receives the number
 *                of entries in the data array
 * @return the data array
 */
U_CAPI uint32_t * U_EXPORT2
utrie_getData(UNewTrie *trie, int32_t *pLength);

/**
 * Set a value for a code point.
 *
 * @param trie the build-time trie
 * @param c the code point
 * @param value the value
 * @return false if a failure occurred (illegal argument or data array overrun)
 */
U_CAPI UBool U_EXPORT2
utrie_set32(UNewTrie *trie, UChar32 c, uint32_t value);

/**
 * Get a value from a code point as stored in the build-time trie.
 *
 * @param trie the build-time trie
 * @param c the code point
 * @param pInBlockZero if not NULL, then *pInBlockZero is set to true
 *                     iff the value is retrieved from block 0;
 *                     block 0 is the all-initial-value initial block
 * @return the value
 */
U_CAPI uint32_t U_EXPORT2
utrie_get32(UNewTrie *trie, UChar32 c, UBool *pInBlockZero);

/**
 * Set a value in a range of code points [start..limit[.
 * All code points c with start<=c<limit will get the value if
 * overwrite is true or if the old value is 0.
 *
 * @param trie the build-time trie
 * @param start the first code point to get the value
 * @param limit one past the last code point to get the value
 * @param value the value
 * @param overwrite flag for whether old non-initial values are to be overwritten
 * @return false if a failure occurred (illegal argument or data array overrun)
 */
U_CAPI UBool U_EXPORT2
utrie_setRange32(UNewTrie *trie, UChar32 start, UChar32 limit, uint32_t value, UBool overwrite);

/**
 * Compact the build-time trie after all values are set, and then
 * serialize it into 32-bit aligned memory.
 *
 * After this, the trie can only be serizalized again and/or closed;
 * no further values can be added.
 *
 * @see utrie_unserialize()
 *
 * @param trie the build-time trie
 * @param data a pointer to 32-bit-aligned memory for the trie data
 * @param capacity the number of bytes available at data
 * @param getFoldedValue a callback function that calculates the value for
 *                       a lead surrogate from all of its supplementary code points
 *                       and the folding offset;
 *                       if NULL, then a default function is used which returns just
 *                       the input offset when there are any non-initial-value entries
 * @param reduceTo16Bits flag for whether the values are to be reduced to a
 *                       width of 16 bits for serialization and runtime
 * @param pErrorCode a UErrorCode argument; among other possible error codes:
 * - U_BUFFER_OVERFLOW_ERROR if the data storage block is too small for serialization
 * - U_MEMORY_ALLOCATION_ERROR if the trie data array is too small
 * - U_INDEX_OUTOFBOUNDS_ERROR if the index or data arrays are too long after compaction for serialization
 *
 * @return the number of bytes written for the trie
 */
U_CAPI int32_t U_EXPORT2
utrie_serialize(UNewTrie *trie, void *data, int32_t capacity,
                UNewTrieGetFoldedValue *getFoldedValue,
                UBool reduceTo16Bits,
                UErrorCode *pErrorCode);

/* serialization ------------------------------------------------------------ */

// UTrie signature values, in platform endianness and opposite endianness.
// The UTrie signature ASCII byte values spell "Trie".
#define UTRIE_SIG       0x54726965
#define UTRIE_OE_SIG    0x65697254

/**
 * Trie data structure in serialized form:
 *
 * UTrieHeader header;
 * uint16_t index[header.indexLength];
 * uint16_t data[header.dataLength];
 * @internal
 */
typedef struct UTrieHeader {
    /** "Trie" in big-endian US-ASCII (0x54726965) */
    uint32_t signature;

    /**
     * options bit field:
     *     9    1=Latin-1 data is stored linearly at data+UTRIE_DATA_BLOCK_LENGTH
     *     8    0=16-bit data, 1=32-bit data
     *  7..4    UTRIE_INDEX_SHIFT   // 0..UTRIE_SHIFT
     *  3..0    UTRIE_SHIFT         // 1..9
     */
    uint32_t options;

    /** indexLength is a multiple of UTRIE_SURROGATE_BLOCK_COUNT */
    int32_t indexLength;

    /** dataLength>=UTRIE_DATA_BLOCK_LENGTH */
    int32_t dataLength;
} UTrieHeader;

/**
 * Constants for use with UTrieHeader.options.
 * @internal
 */
enum {
    /** Mask to get the UTRIE_SHIFT value from options. */
    UTRIE_OPTIONS_SHIFT_MASK=0xf,

    /** Shift options right this much to get the UTRIE_INDEX_SHIFT value. */
    UTRIE_OPTIONS_INDEX_SHIFT=4,

    /** If set, then the data (stage 2) array is 32 bits wide. */
    UTRIE_OPTIONS_DATA_IS_32_BIT=0x100,

    /**
     * If set, then Latin-1 data (for U+0000..U+00ff) is stored in the data (stage 2) array
     * as a simple, linear array at data+UTRIE_DATA_BLOCK_LENGTH.
     */
    UTRIE_OPTIONS_LATIN1_IS_LINEAR=0x200
};

U_CDECL_END

#endif
