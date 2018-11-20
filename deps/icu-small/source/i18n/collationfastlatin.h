// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2013-2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationfastlatin.h
*
* created on: 2013aug09
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONFASTLATIN_H__
#define __COLLATIONFASTLATIN_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

U_NAMESPACE_BEGIN

struct CollationData;
struct CollationSettings;

class U_I18N_API CollationFastLatin /* all static */ {
public:
    /**
     * Fast Latin format version (one byte 1..FF).
     * Must be incremented for any runtime-incompatible changes,
     * in particular, for changes to any of the following constants.
     *
     * When the major version number of the main data format changes,
     * we can reset this fast Latin version to 1.
     */
    static const uint16_t VERSION = 2;

    static const int32_t LATIN_MAX = 0x17f;
    static const int32_t LATIN_LIMIT = LATIN_MAX + 1;

    static const int32_t LATIN_MAX_UTF8_LEAD = 0xc5;  // UTF-8 lead byte of LATIN_MAX

    static const int32_t PUNCT_START = 0x2000;
    static const int32_t PUNCT_LIMIT = 0x2040;

    // excludes U+FFFE & U+FFFF
    static const int32_t NUM_FAST_CHARS = LATIN_LIMIT + (PUNCT_LIMIT - PUNCT_START);

    // Note on the supported weight ranges:
    // Analysis of UCA 6.3 and CLDR 23 non-search tailorings shows that
    // the CEs for characters in the above ranges, excluding expansions with length >2,
    // excluding contractions of >2 characters, and other restrictions
    // (see the builder's getCEsFromCE32()),
    // use at most about 150 primary weights,
    // where about 94 primary weights are possibly-variable (space/punct/symbol/currency),
    // at most 4 secondary before-common weights,
    // at most 4 secondary after-common weights,
    // at most 16 secondary high weights (in secondary CEs), and
    // at most 4 tertiary after-common weights.
    // The following ranges are designed to support slightly more weights than that.
    // (en_US_POSIX is unusual: It creates about 64 variable + 116 Latin primaries.)

    // Digits may use long primaries (preserving more short ones)
    // or short primaries (faster) without changing this data structure.
    // (If we supported numeric collation, then digits would have to have long primaries
    // so that special handling does not affect the fast path.)

    static const uint32_t SHORT_PRIMARY_MASK = 0xfc00;  // bits 15..10
    static const uint32_t INDEX_MASK = 0x3ff;  // bits 9..0 for expansions & contractions
    static const uint32_t SECONDARY_MASK = 0x3e0;  // bits 9..5
    static const uint32_t CASE_MASK = 0x18;  // bits 4..3
    static const uint32_t LONG_PRIMARY_MASK = 0xfff8;  // bits 15..3
    static const uint32_t TERTIARY_MASK = 7;  // bits 2..0
    static const uint32_t CASE_AND_TERTIARY_MASK = CASE_MASK | TERTIARY_MASK;

    static const uint32_t TWO_SHORT_PRIMARIES_MASK =
            (SHORT_PRIMARY_MASK << 16) | SHORT_PRIMARY_MASK;  // 0xfc00fc00
    static const uint32_t TWO_LONG_PRIMARIES_MASK =
            (LONG_PRIMARY_MASK << 16) | LONG_PRIMARY_MASK;  // 0xfff8fff8
    static const uint32_t TWO_SECONDARIES_MASK =
            (SECONDARY_MASK << 16) | SECONDARY_MASK;  // 0x3e003e0
    static const uint32_t TWO_CASES_MASK =
            (CASE_MASK << 16) | CASE_MASK;  // 0x180018
    static const uint32_t TWO_TERTIARIES_MASK =
            (TERTIARY_MASK << 16) | TERTIARY_MASK;  // 0x70007

    /**
     * Contraction with one fast Latin character.
     * Use INDEX_MASK to find the start of the contraction list after the fixed table.
     * The first entry contains the default mapping.
     * Otherwise use CONTR_CHAR_MASK for the contraction character index
     * (in ascending order).
     * Use CONTR_LENGTH_SHIFT for the length of the entry
     * (1=BAIL_OUT, 2=one CE, 3=two CEs).
     *
     * Also, U+0000 maps to a contraction entry, so that the fast path need not
     * check for NUL termination.
     * It usually maps to a contraction list with only the completely ignorable default value.
     */
    static const uint32_t CONTRACTION = 0x400;
    /**
     * An expansion encodes two CEs.
     * Use INDEX_MASK to find the pair of CEs after the fixed table.
     *
     * The higher a mini CE value, the easier it is to process.
     * For expansions and higher, no context needs to be considered.
     */
    static const uint32_t EXPANSION = 0x800;
    /**
     * Encodes one CE with a long/low mini primary (there are 128).
     * All potentially-variable primaries must be in this range,
     * to make the short-primary path as fast as possible.
     */
    static const uint32_t MIN_LONG = 0xc00;
    static const uint32_t LONG_INC = 8;
    static const uint32_t MAX_LONG = 0xff8;
    /**
     * Encodes one CE with a short/high primary (there are 60),
     * plus a secondary CE if the secondary weight is high.
     * Fast handling: At least all letter primaries should be in this range.
     */
    static const uint32_t MIN_SHORT = 0x1000;
    static const uint32_t SHORT_INC = 0x400;
    /** The highest primary weight is reserved for U+FFFF. */
    static const uint32_t MAX_SHORT = SHORT_PRIMARY_MASK;

    static const uint32_t MIN_SEC_BEFORE = 0;  // must add SEC_OFFSET
    static const uint32_t SEC_INC = 0x20;
    static const uint32_t MAX_SEC_BEFORE = MIN_SEC_BEFORE + 4 * SEC_INC;  // 5 before common
    static const uint32_t COMMON_SEC = MAX_SEC_BEFORE + SEC_INC;
    static const uint32_t MIN_SEC_AFTER = COMMON_SEC + SEC_INC;
    static const uint32_t MAX_SEC_AFTER = MIN_SEC_AFTER + 5 * SEC_INC;  // 6 after common
    static const uint32_t MIN_SEC_HIGH = MAX_SEC_AFTER + SEC_INC;  // 20 high secondaries
    static const uint32_t MAX_SEC_HIGH = SECONDARY_MASK;

    /**
     * Lookup: Add this offset to secondary weights, except for completely ignorable CEs.
     * Must be greater than any special value, e.g., MERGE_WEIGHT.
     * The exact value is not relevant for the format version.
     */
    static const uint32_t SEC_OFFSET = SEC_INC;
    static const uint32_t COMMON_SEC_PLUS_OFFSET = COMMON_SEC + SEC_OFFSET;

    static const uint32_t TWO_SEC_OFFSETS =
            (SEC_OFFSET << 16) | SEC_OFFSET;  // 0x200020
    static const uint32_t TWO_COMMON_SEC_PLUS_OFFSET =
            (COMMON_SEC_PLUS_OFFSET << 16) | COMMON_SEC_PLUS_OFFSET;

    static const uint32_t LOWER_CASE = 8;  // case bits include this offset
    static const uint32_t TWO_LOWER_CASES = (LOWER_CASE << 16) | LOWER_CASE;  // 0x80008

    static const uint32_t COMMON_TER = 0;  // must add TER_OFFSET
    static const uint32_t MAX_TER_AFTER = 7;  // 7 after common

    /**
     * Lookup: Add this offset to tertiary weights, except for completely ignorable CEs.
     * Must be greater than any special value, e.g., MERGE_WEIGHT.
     * Must be greater than case bits as well, so that with combined case+tertiary weights
     * plus the offset the tertiary bits does not spill over into the case bits.
     * The exact value is not relevant for the format version.
     */
    static const uint32_t TER_OFFSET = SEC_OFFSET;
    static const uint32_t COMMON_TER_PLUS_OFFSET = COMMON_TER + TER_OFFSET;

    static const uint32_t TWO_TER_OFFSETS = (TER_OFFSET << 16) | TER_OFFSET;
    static const uint32_t TWO_COMMON_TER_PLUS_OFFSET =
            (COMMON_TER_PLUS_OFFSET << 16) | COMMON_TER_PLUS_OFFSET;

    static const uint32_t MERGE_WEIGHT = 3;
    static const uint32_t EOS = 2;  // end of string
    static const uint32_t BAIL_OUT = 1;

    /**
     * Contraction result first word bits 8..0 contain the
     * second contraction character, as a char index 0..NUM_FAST_CHARS-1.
     * Each contraction list is terminated with a word containing CONTR_CHAR_MASK.
     */
    static const uint32_t CONTR_CHAR_MASK = 0x1ff;
    /**
     * Contraction result first word bits 10..9 contain the result length:
     * 1=bail out, 2=one mini CE, 3=two mini CEs
     */
    static const uint32_t CONTR_LENGTH_SHIFT = 9;

    /**
     * Comparison return value when the regular comparison must be used.
     * The exact value is not relevant for the format version.
     */
    static const int32_t BAIL_OUT_RESULT = -2;

    static inline int32_t getCharIndex(UChar c) {
        if(c <= LATIN_MAX) {
            return c;
        } else if(PUNCT_START <= c && c < PUNCT_LIMIT) {
            return c - (PUNCT_START - LATIN_LIMIT);
        } else {
            // Not a fast Latin character.
            // Note: U+FFFE & U+FFFF are forbidden in tailorings
            // and thus do not occur in any contractions.
            return -1;
        }
    }

    /**
     * Computes the options value for the compare functions
     * and writes the precomputed primary weights.
     * Returns -1 if the Latin fastpath is not supported for the data and settings.
     * The capacity must be LATIN_LIMIT.
     */
    static int32_t getOptions(const CollationData *data, const CollationSettings &settings,
                              uint16_t *primaries, int32_t capacity);

    static int32_t compareUTF16(const uint16_t *table, const uint16_t *primaries, int32_t options,
                                const UChar *left, int32_t leftLength,
                                const UChar *right, int32_t rightLength);

    static int32_t compareUTF8(const uint16_t *table, const uint16_t *primaries, int32_t options,
                               const uint8_t *left, int32_t leftLength,
                               const uint8_t *right, int32_t rightLength);

private:
    static uint32_t lookup(const uint16_t *table, UChar32 c);
    static uint32_t lookupUTF8(const uint16_t *table, UChar32 c,
                               const uint8_t *s8, int32_t &sIndex, int32_t sLength);
    static uint32_t lookupUTF8Unsafe(const uint16_t *table, UChar32 c,
                                     const uint8_t *s8, int32_t &sIndex);

    static uint32_t nextPair(const uint16_t *table, UChar32 c, uint32_t ce,
                             const UChar *s16, const uint8_t *s8, int32_t &sIndex, int32_t &sLength);

    static inline uint32_t getPrimaries(uint32_t variableTop, uint32_t pair) {
        uint32_t ce = pair & 0xffff;
        if(ce >= MIN_SHORT) { return pair & TWO_SHORT_PRIMARIES_MASK; }
        if(ce > variableTop) { return pair & TWO_LONG_PRIMARIES_MASK; }
        if(ce >= MIN_LONG) { return 0; }  // variable
        return pair;  // special mini CE
    }
    static inline uint32_t getSecondariesFromOneShortCE(uint32_t ce) {
        ce &= SECONDARY_MASK;
        if(ce < MIN_SEC_HIGH) {
            return ce + SEC_OFFSET;
        } else {
            return ((ce + SEC_OFFSET) << 16) | COMMON_SEC_PLUS_OFFSET;
        }
    }
    static uint32_t getSecondaries(uint32_t variableTop, uint32_t pair);
    static uint32_t getCases(uint32_t variableTop, UBool strengthIsPrimary, uint32_t pair);
    static uint32_t getTertiaries(uint32_t variableTop, UBool withCaseBits, uint32_t pair);
    static uint32_t getQuaternaries(uint32_t variableTop, uint32_t pair);

private:
    CollationFastLatin();  // no constructor
};

/*
 * Format of the CollationFastLatin data table.
 * CollationFastLatin::VERSION = 2.
 *
 * This table contains data for a Latin-text collation fastpath.
 * The data is stored as an array of uint16_t which contains the following parts.
 *
 * uint16_t  -- version & header length
 *   Bits 15..8: version, must match the VERSION
 *         7..0: length of the header
 *
 * uint16_t varTops[header length - 1]
 *   Version 2:
 *   varTops[m] is the highest CollationFastLatin long-primary weight
 *   of supported maxVariable group m
 *   (special reorder group space, punct, symbol, currency).
 *
 *   Version 1:
 *   Each of these values maps the variable top lead byte of a supported maxVariable group
 *   to the highest CollationFastLatin long-primary weight.
 *   The values are stored in ascending order.
 *   Bits 15..7: max fast-Latin long-primary weight (bits 11..3 shifted left by 4 bits)
 *         6..0: regular primary lead byte
 *
 * uint16_t miniCEs[0x1c0]
 *   A mini collation element for each character U+0000..U+017F and U+2000..U+203F.
 *   Each value encodes one or two mini CEs (two are possible if the first one
 *   has a short mini primary and the second one is a secondary CE, i.e., primary == 0),
 *   or points to an expansion or to a contraction table.
 *   U+0000 always has a contraction entry,
 *   so that NUL-termination need not be tested in the fastpath.
 *   If the collation elements for a character or contraction cannot be encoded in this format,
 *   then the BAIL_OUT value is stored.
 *   For details see the comments for the class constants.
 *
 * uint16_t expansions[variable length];
 *   Expansion mini CEs contain an offset relative to just after the miniCEs table.
 *   An expansions contains exactly 2 mini CEs.
 *
 * uint16_t contractions[variable length];
 *   Contraction mini CEs contain an offset relative to just after the miniCEs table.
 *   It points to a list of tuples which map from a contraction suffix character to a result.
 *   First uint16_t of each tuple:
 *     Bits 10..9: Length of the result (1..3), see comments on CONTR_LENGTH_SHIFT.
 *     Bits  8..0: Contraction character, see comments on CONTR_CHAR_MASK.
 *   This is followed by 0, 1, or 2 uint16_t according to the length.
 *   Each list is terminated by an entry with CONTR_CHAR_MASK.
 *   Each list starts with such an entry which also contains the default result
 *   for when there is no contraction match.
 *
 * -----------------
 * Changes for version 2 (ICU 55)
 *
 * Special reorder groups do not necessarily start on whole primary lead bytes any more.
 * Therefore, the varTops data has a new format:
 * Version 1 stored the lead bytes of the highest root primaries for
 * the maxVariable-supported special reorder groups.
 * Now the top 16 bits would need to be stored,
 * and it is simpler to store only the fast-Latin weights.
 */

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONFASTLATIN_H__
