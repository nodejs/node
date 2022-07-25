// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// ucptrie_impl.h (modified from utrie2_impl.h)
// created: 2017dec29 Markus W. Scherer

#ifndef __UCPTRIE_IMPL_H__
#define __UCPTRIE_IMPL_H__

#include "unicode/ucptrie.h"
#ifdef UCPTRIE_DEBUG
#include "unicode/umutablecptrie.h"
#endif

// UCPTrie signature values, in platform endianness and opposite endianness.
// The UCPTrie signature ASCII byte values spell "Tri3".
#define UCPTRIE_SIG     0x54726933
#define UCPTRIE_OE_SIG  0x33697254

/**
 * Header data for the binary, memory-mappable representation of a UCPTrie/CodePointTrie.
 * @internal
 */
struct UCPTrieHeader {
    /** "Tri3" in big-endian US-ASCII (0x54726933) */
    uint32_t signature;

    /**
     * Options bit field:
     * Bits 15..12: Data length bits 19..16.
     * Bits 11..8: Data null block offset bits 19..16.
     * Bits 7..6: UCPTrieType
     * Bits 5..3: Reserved (0).
     * Bits 2..0: UCPTrieValueWidth
     */
    uint16_t options;

    /** Total length of the index tables. */
    uint16_t indexLength;

    /** Data length bits 15..0. */
    uint16_t dataLength;

    /** Index-3 null block offset, 0x7fff or 0xffff if none. */
    uint16_t index3NullOffset;

    /** Data null block offset bits 15..0, 0xfffff if none. */
    uint16_t dataNullOffset;

    /**
     * First code point of the single-value range ending with U+10ffff,
     * rounded up and then shifted right by UCPTRIE_SHIFT_2.
     */
    uint16_t shiftedHighStart;
};

/**
 * Constants for use with UCPTrieHeader.options.
 * @internal
 */
enum {
    UCPTRIE_OPTIONS_DATA_LENGTH_MASK = 0xf000,
    UCPTRIE_OPTIONS_DATA_NULL_OFFSET_MASK = 0xf00,
    UCPTRIE_OPTIONS_RESERVED_MASK = 0x38,
    UCPTRIE_OPTIONS_VALUE_BITS_MASK = 7,
    /**
     * Value for index3NullOffset which indicates that there is no index-3 null block.
     * Bit 15 is unused for this value because this bit is used if the index-3 contains
     * 18-bit indexes.
     */
    UCPTRIE_NO_INDEX3_NULL_OFFSET = 0x7fff,
    UCPTRIE_NO_DATA_NULL_OFFSET = 0xfffff
};

// Internal constants.
enum {
    /** The length of the BMP index table. 1024=0x400 */
    UCPTRIE_BMP_INDEX_LENGTH = 0x10000 >> UCPTRIE_FAST_SHIFT,

    UCPTRIE_SMALL_LIMIT = 0x1000,
    UCPTRIE_SMALL_INDEX_LENGTH = UCPTRIE_SMALL_LIMIT >> UCPTRIE_FAST_SHIFT,

    /** Shift size for getting the index-3 table offset. */
    UCPTRIE_SHIFT_3 = 4,

    /** Shift size for getting the index-2 table offset. */
    UCPTRIE_SHIFT_2 = 5 + UCPTRIE_SHIFT_3,

    /** Shift size for getting the index-1 table offset. */
    UCPTRIE_SHIFT_1 = 5 + UCPTRIE_SHIFT_2,

    /**
     * Difference between two shift sizes,
     * for getting an index-2 offset from an index-3 offset. 5=9-4
     */
    UCPTRIE_SHIFT_2_3 = UCPTRIE_SHIFT_2 - UCPTRIE_SHIFT_3,

    /**
     * Difference between two shift sizes,
     * for getting an index-1 offset from an index-2 offset. 5=14-9
     */
    UCPTRIE_SHIFT_1_2 = UCPTRIE_SHIFT_1 - UCPTRIE_SHIFT_2,

    /**
     * Number of index-1 entries for the BMP. (4)
     * This part of the index-1 table is omitted from the serialized form.
     */
    UCPTRIE_OMITTED_BMP_INDEX_1_LENGTH = 0x10000 >> UCPTRIE_SHIFT_1,

    /** Number of entries in an index-2 block. 32=0x20 */
    UCPTRIE_INDEX_2_BLOCK_LENGTH = 1 << UCPTRIE_SHIFT_1_2,

    /** Mask for getting the lower bits for the in-index-2-block offset. */
    UCPTRIE_INDEX_2_MASK = UCPTRIE_INDEX_2_BLOCK_LENGTH - 1,

    /** Number of code points per index-2 table entry. 512=0x200 */
    UCPTRIE_CP_PER_INDEX_2_ENTRY = 1 << UCPTRIE_SHIFT_2,

    /** Number of entries in an index-3 block. 32=0x20 */
    UCPTRIE_INDEX_3_BLOCK_LENGTH = 1 << UCPTRIE_SHIFT_2_3,

    /** Mask for getting the lower bits for the in-index-3-block offset. */
    UCPTRIE_INDEX_3_MASK = UCPTRIE_INDEX_3_BLOCK_LENGTH - 1,

    /** Number of entries in a small data block. 16=0x10 */
    UCPTRIE_SMALL_DATA_BLOCK_LENGTH = 1 << UCPTRIE_SHIFT_3,

    /** Mask for getting the lower bits for the in-small-data-block offset. */
    UCPTRIE_SMALL_DATA_MASK = UCPTRIE_SMALL_DATA_BLOCK_LENGTH - 1
};

typedef UChar32
UCPTrieGetRange(const void *trie, UChar32 start,
                UCPMapValueFilter *filter, const void *context, uint32_t *pValue);

U_CFUNC UChar32
ucptrie_internalGetRange(UCPTrieGetRange *getRange,
                         const void *trie, UChar32 start,
                         UCPMapRangeOption option, uint32_t surrogateValue,
                         UCPMapValueFilter *filter, const void *context, uint32_t *pValue);

#ifdef UCPTRIE_DEBUG
U_CFUNC void
ucptrie_printLengths(const UCPTrie *trie, const char *which);

U_CFUNC void umutablecptrie_setName(UMutableCPTrie *builder, const char *name);
#endif

/*
 * Format of the binary, memory-mappable representation of a UCPTrie/CodePointTrie.
 * For overview information see https://icu.unicode.org/design/struct/utrie
 *
 * The binary trie data should be 32-bit-aligned.
 * The overall layout is:
 *
 * UCPTrieHeader header; -- 16 bytes, see struct definition above
 * uint16_t index[header.indexLength];
 * uintXY_t data[header.dataLength];
 *
 * The trie data array is an array of uint16_t, uint32_t, or uint8_t,
 * specified via the UCPTrieValueWidth when building the trie.
 * The data array is 32-bit-aligned for uint32_t, otherwise 16-bit-aligned.
 * The overall length of the trie data is a multiple of 4 bytes.
 * (Padding is added at the end of the index array and/or near the end of the data array as needed.)
 *
 * The length of the data array (dataLength) is stored as an integer split across two fields
 * of the header struct (high bits in header.options).
 *
 * The trie type can be "fast" or "small" which determines the index structure,
 * specified via the UCPTrieType when building the trie.
 *
 * The type and valueWidth are stored in the header.options.
 * There are reserved type and valueWidth values, and reserved header.options bits.
 * They could be used in future format extensions.
 * Code reading the trie structure must fail with an error when unknown values or options are set.
 *
 * Values for ASCII character (U+0000..U+007F) can always be found at the start of the data array.
 *
 * Values for code points below a type-specific fast-indexing limit are found via two-stage lookup.
 * For a "fast" trie, the limit is the BMP/supplementary boundary at U+10000.
 * For a "small" trie, the limit is UCPTRIE_SMALL_MAX+1=U+1000.
 *
 * All code points in the range highStart..U+10FFFF map to a single highValue
 * which is stored at the second-to-last position of the data array.
 * (See UCPTRIE_HIGH_VALUE_NEG_DATA_OFFSET.)
 * The highStart value is header.shiftedHighStart<<UCPTRIE_SHIFT_2.
 * (UCPTRIE_SHIFT_2=9)
 *
 * Values for code points fast_limit..highStart-1 are found via four-stage lookup.
 * The data block size is smaller for this range than for the fast range.
 * This together with more index stages with small blocks makes this range
 * more easily compactable.
 *
 * There is also a trie error value stored at the last position of the data array.
 * (See UCPTRIE_ERROR_VALUE_NEG_DATA_OFFSET.)
 * It is intended to be returned for inputs that are not Unicode code points
 * (outside U+0000..U+10FFFF), or in string processing for ill-formed input
 * (unpaired surrogate in UTF-16, ill-formed UTF-8 subsequence).
 *
 * For a "fast" trie:
 *
 * The index array starts with the BMP index table for BMP code point lookup.
 * Its length is 1024=0x400.
 *
 * The supplementary index-1 table follows the BMP index table.
 * Variable length, for code points up to highStart-1.
 * Maximum length 64=0x40=0x100000>>UCPTRIE_SHIFT_1.
 * (For 0x100000 supplementary code points U+10000..U+10ffff.)
 *
 * After this index-1 table follow the variable-length index-3 and index-2 tables.
 *
 * The supplementary index tables are omitted completely
 * if there is only BMP data (highStart<=U+10000).
 *
 * For a "small" trie:
 *
 * The index array starts with a fast-index table for lookup of code points U+0000..U+0FFF.
 *
 * The "supplementary" index tables are always stored.
 * The index-1 table starts from U+0000, its maximum length is 68=0x44=0x110000>>UCPTRIE_SHIFT_1.
 *
 * For both trie types:
 *
 * The last index-2 block may be a partial block, storing indexes only for code points
 * below highStart.
 *
 * Lookup for ASCII code point c:
 *
 * Linear access from the start of the data array.
 *
 * value = data[c];
 *
 * Lookup for fast-range code point c:
 *
 * Shift the code point right by UCPTRIE_FAST_SHIFT=6 bits,
 * fetch the index array value at that offset,
 * add the lower code point bits, index into the data array.
 *
 * value = data[index[c>>6] + (c&0x3f)];
 *
 * (This works for ASCII as well.)
 *
 * Lookup for small-range code point c below highStart:
 *
 * Split the code point into four bit fields using several sets of shifts & masks
 * to read consecutive values from the index-1, index-2, index-3 and data tables.
 *
 * If all of the data block offsets in an index-3 block fit within 16 bits (up to 0xffff),
 * then the data block offsets are stored directly as uint16_t.
 *
 * Otherwise (this is very unusual but possible), the index-2 entry for the index-3 block
 * has bit 15 (0x8000) set, and each set of 8 index-3 entries is preceded by
 * an additional uint16_t word. Data block offsets are 18 bits wide, with the top 2 bits stored
 * in the additional word.
 *
 * See ucptrie_internalSmallIndex() for details.
 *
 * (In a "small" trie, this works for ASCII and below-fast_limit code points as well.)
 *
 * Compaction:
 *
 * Multiple code point ranges ("blocks") that are aligned on certain boundaries
 * (determined by the shifting/bit fields of code points) and
 * map to the same data values normally share a single subsequence of the data array.
 * Data blocks can also overlap partially.
 * (Depending on the builder code finding duplicate and overlapping blocks.)
 *
 * Iteration over same-value ranges:
 *
 * Range iteration (ucptrie_getRange()) walks the structure from a start code point
 * until some code point is found that maps to a different value;
 * the end of the returned range is just before that.
 *
 * The header.dataNullOffset (split across two header fields, high bits in header.options)
 * is the offset of a widely shared data block filled with one single value.
 * It helps quickly skip over large ranges of data with that value.
 * The builder must ensure that if the start of any data block (fast or small)
 * matches the dataNullOffset, then the whole block must be filled with the null value.
 * Special care must be taken if there is no fast null data block
 * but a small one, which is shorter, and it matches the *start* of some fast data block.
 *
 * Similarly, the header.index3NullOffset is the index-array offset of an index-3 block
 * where all index entries point to the dataNullOffset.
 * If there is no such data or index-3 block, then these offsets are set to
 * values that cannot be reached (data offset out of range/reserved index offset),
 * normally UCPTRIE_NO_DATA_NULL_OFFSET or UCPTRIE_NO_INDEX3_NULL_OFFSET respectively.
 */

#endif
