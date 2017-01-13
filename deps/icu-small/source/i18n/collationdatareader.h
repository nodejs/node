// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2013-2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdatareader.h
*
* created on: 2013feb07
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONDATAREADER_H__
#define __COLLATIONDATAREADER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/udata.h"

struct UDataMemory;

U_NAMESPACE_BEGIN

struct CollationTailoring;

/**
 * Collation binary data reader.
 */
struct U_I18N_API CollationDataReader /* all static */ {
    // The following constants are also copied into source/common/ucol_swp.cpp.
    // Keep them in sync!
    enum {
        /**
         * Number of int32_t indexes.
         *
         * Can be 2 if there are only options.
         * Can be 7 or 8 if there are only options and a script reordering.
         * The loader treats any index>=indexes[IX_INDEXES_LENGTH] as 0.
         */
        IX_INDEXES_LENGTH,  // 0
        /**
         * Bits 31..24: numericPrimary, for numeric collation
         *      23..16: fast Latin format version (0 = no fast Latin table)
         *      15.. 0: options bit set
         */
        IX_OPTIONS,
        IX_RESERVED2,
        IX_RESERVED3,

        /** Array offset to Jamo CE32s in ce32s[], or <0 if none. */
        IX_JAMO_CE32S_START,  // 4

        // Byte offsets from the start of the data, after the generic header.
        // The indexes[] are at byte offset 0, other data follows.
        // Each data item is aligned properly.
        // The data items should be in descending order of unit size,
        // to minimize the need for padding.
        // Each item's byte length is given by the difference between its offset and
        // the next index/offset value.
        /** Byte offset to int32_t reorderCodes[]. */
        IX_REORDER_CODES_OFFSET,
        /**
         * Byte offset to uint8_t reorderTable[].
         * Empty table if <256 bytes (padding only).
         * Otherwise 256 bytes or more (with padding).
         */
        IX_REORDER_TABLE_OFFSET,
        /** Byte offset to the collation trie. Its length is a multiple of 8 bytes. */
        IX_TRIE_OFFSET,

        IX_RESERVED8_OFFSET,  // 8
        /** Byte offset to int64_t ces[]. */
        IX_CES_OFFSET,
        IX_RESERVED10_OFFSET,
        /** Byte offset to uint32_t ce32s[]. */
        IX_CE32S_OFFSET,

        /** Byte offset to uint32_t rootElements[]. */
        IX_ROOT_ELEMENTS_OFFSET,  // 12
        /** Byte offset to UChar *contexts[]. */
        IX_CONTEXTS_OFFSET,
        /** Byte offset to uint16_t [] with serialized unsafeBackwardSet. */
        IX_UNSAFE_BWD_OFFSET,
        /** Byte offset to uint16_t fastLatinTable[]. */
        IX_FAST_LATIN_TABLE_OFFSET,

        /** Byte offset to uint16_t scripts[]. */
        IX_SCRIPTS_OFFSET,  // 16
        /**
         * Byte offset to UBool compressibleBytes[].
         * Empty table if <256 bytes (padding only).
         * Otherwise 256 bytes or more (with padding).
         */
        IX_COMPRESSIBLE_BYTES_OFFSET,
        IX_RESERVED18_OFFSET,
        IX_TOTAL_SIZE
    };

    static void read(const CollationTailoring *base, const uint8_t *inBytes, int32_t inLength,
                     CollationTailoring &tailoring, UErrorCode &errorCode);

    static UBool U_CALLCONV
    isAcceptable(void *context, const char *type, const char *name, const UDataInfo *pInfo);

private:
    CollationDataReader();  // no constructor
};

/*
 * Format of collation data (ucadata.icu, binary data in coll/ *.res files).
 * Format version 5.
 *
 * The root collation data is stored in the ucadata.icu file.
 * Tailorings are stored inside .res resource bundle files, with a complete file header.
 *
 * Collation data begins with a standard ICU data file header
 * (DataHeader, see ucmndata.h and unicode/udata.h).
 * The UDataInfo.dataVersion field contains the UCA and other version numbers,
 * see the comments for CollationTailoring.version.
 *
 * After the header, the file contains the following parts.
 * Constants are defined as enum values of the CollationDataReader class.
 * See also the Collation class.
 *
 * int32_t indexes[indexesLength];
 *      The indexes array has variable length.
 *      Some tailorings only need the length and the options,
 *      others only add reorderCodes and the reorderTable,
 *      some need to store mappings.
 *      Only as many indexes are stored as needed to read all of the data.
 *
 *      Index 0: indexesLength
 *      Index 1: numericPrimary, CollationFastLatin::VERSION, and options: see IX_OPTIONS
 *      Index 2..3: Unused/reserved/0.
 *      Index 4: Index into the ce32s array where the CE32s of the conjoining Jamo
 *               are stored in a short, contiguous part of the ce32s array.
 *
 *      Indexes 5..19 are byte offsets in ascending order.
 *      Each byte offset marks the start of the next part in the data file,
 *      and the end of the previous one.
 *      When two consecutive byte offsets are the same (or too short),
 *      then the corresponding part is empty.
 *      Byte offsets are offsets from after the header,
 *      that is, from the beginning of the indexes[].
 *      Each part starts at an offset with proper alignment for its data.
 *      If necessary, the previous part may include padding bytes to achieve this alignment.
 *      The last byte offset that is stored in the indexes indicates the total size of the data
 *      (starting with the indexes).
 *
 * int32_t reorderCodes[]; -- empty in root
 *      The list of script and reordering codes.
 *
 *      Beginning with format version 5, this array may optionally
 *      have trailing entries with a full list of reorder ranges
 *      as described for CollationSettings::reorderRanges.
 *
 *      Script or reorder codes are first and do not exceed 16-bit values.
 *      Range limits are stored in the upper 16 bits, and are never 0.
 *      Split this array into reorder codes and ranges at the first entry
 *      with non-zero upper 16 bits.
 *
 *      If the ranges are missing but needed for split-reordered primary lead bytes,
 *      then they are regenerated at load time.
 *
 * uint8_t reorderTable[256]; -- empty in root; can be longer to include padding bytes
 *      Primary-weight lead byte permutation table.
 *      Normally present when the reorderCodes are, but can be built at load time.
 *
 *      Beginning with format version 5, a 0 entry at a non-zero index
 *      (which is otherwise an illegal value)
 *      means that the primary lead byte is "split"
 *      (there are different offsets for primaries that share that lead byte)
 *      and the reordering offset must be determined via the reorder ranges
 *      that are either stored as part of the reorderCodes array
 *      or regenerated at load time.
 *
 * UTrie2 trie; -- see utrie2_impl.h and utrie2.h
 *      The trie holds the main collation data. Each code point is mapped to a 32-bit value.
 *      It encodes a simple collation element (CE) in compact form, unless bits 7..6 are both set,
 *      in which case it is a special CE32 and contains a 4-bit tag and further data.
 *      See the Collation class for details.
 *
 *      The trie has a value for each lead surrogate code unit with some bits encoding
 *      collective properties of the 1024 supplementary characters whose UTF-16 form starts with
 *      the lead surrogate. See Collation::LEAD_SURROGATE_TAG..
 *
 * int64_t ces[];
 *      64-bit CEs and expansions that cannot be stored in a more compact form.
 *
 * uint32_t ce32s[];
 *      CE32s for expansions in compact form, and for characters whose trie values
 *      contain special data.
 *
 * uint32_t rootElements[]; -- empty in all tailorings
 *      Compact storage for all of the CEs that occur in the root collation.
 *      See the CollationRootElements class.
 *
 * UChar *contexts[];
 *      Serialized UCharsTrie structures with prefix (pre-context) and contraction mappings.
 *
 * uint16_t unsafeBackwardSet[]; -- see UnicodeSet::serialize()
 *      Serialized form of characters that are unsafe when iterating backwards,
 *      and at the end of an identical string prefix.
 *      Back up to a safe character.
 *      Lead surrogates are "unsafe" when any of their corresponding supplementary
 *      code points are unsafe.
 *      Does not include [:^lccc=0:][:^tccc=0:].
 *      For each tailoring, the root unsafeBackwardSet is subtracted.
 *      (As a result, in many tailorings no set needs to be stored.)
 *
 * uint16_t fastLatinTable[];
 *      Optional optimization for Latin text.
 *      See the CollationFastLatin class.
 *
 * uint16_t scripts[]; -- empty in all tailorings
 *      Format version 5:
 *      uint16_t numScripts;
 *      uint16_t scriptsIndex[numScripts+16];
 *      uint16_t scriptStarts[];
 *      See CollationData::numScripts etc.
 *
 *      Format version 4:
 *      Table of the reordering groups with their first and last lead bytes,
 *      and their script and reordering codes.
 *      See CollationData::scripts.
 *
 * UBool compressibleBytes[]; -- empty in all tailorings
 *      Flag for getSortKey(), indicating primary weight lead bytes that are compressible.
 *
 * -----------------
 * Changes for formatVersion 5 (ICU 55)
 *
 * Reordering moves single scripts, not groups of scripts.
 * Reorder ranges are optionally appended to the reorderCodes,
 * and a 0 entry in the reorderTable indicates a split lead byte.
 * The scripts data has a new format.
 *
 * The rootElements may contain secondary and tertiary weights below common=05.
 * (Used for small Hiragana letters.)
 * Where is occurs, there is also an explicit unit with common secondary & tertiary weights.
 * There are no other data structure changes, but builder code needs to be able to handle such data.
 *
 * The collation element for the merge separator code point U+FFFE
 * does not necessarily have special, unique secondary/tertiary weights any more.
 */

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONDATAREADER_H__
