// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2001-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  unormimp.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001may25
*   created by: Markus W. Scherer
*/

#ifndef __UNORMIMP_H__
#define __UNORMIMP_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include "udataswp.h"

/*
 * The 2001-2010 implementation of the normalization code loads its data from
 * unorm.icu, which is generated with the gennorm tool.
 * The format of that file is described at the end of this file.
 */

/* norm32 value constants */
enum {
    /* quick check flags 0..3 set mean "no" for their forms */
    _NORM_QC_NFC=0x11,          /* no|maybe */
    _NORM_QC_NFKC=0x22,         /* no|maybe */
    _NORM_QC_NFD=4,             /* no */
    _NORM_QC_NFKD=8,            /* no */

    _NORM_QC_ANY_NO=0xf,

    /* quick check flags 4..5 mean "maybe" for their forms; test flags>=_NORM_QC_MAYBE */
    _NORM_QC_MAYBE=0x10,
    _NORM_QC_ANY_MAYBE=0x30,

    _NORM_QC_MASK=0x3f,

    _NORM_COMBINES_FWD=0x40,
    _NORM_COMBINES_BACK=0x80,
    _NORM_COMBINES_ANY=0xc0,

    _NORM_CC_SHIFT=8,           /* UnicodeData.txt combining class in bits 15..8 */
    _NORM_CC_MASK=0xff00,

    _NORM_EXTRA_SHIFT=16,               /* 16 bits for the index to UChars and other extra data */
    _NORM_EXTRA_INDEX_TOP=0xfc00,       /* start of surrogate specials after shift */

    _NORM_EXTRA_SURROGATE_MASK=0x3ff,
    _NORM_EXTRA_SURROGATE_TOP=0x3f0,    /* hangul etc. */

    _NORM_EXTRA_HANGUL=_NORM_EXTRA_SURROGATE_TOP,
    _NORM_EXTRA_JAMO_L,
    _NORM_EXTRA_JAMO_V,
    _NORM_EXTRA_JAMO_T
};

/* norm32 value constants using >16 bits */
#define _NORM_MIN_SPECIAL       0xfc000000
#define _NORM_SURROGATES_TOP    0xfff00000
#define _NORM_MIN_HANGUL        0xfff00000
#define _NORM_MIN_JAMO_V        0xfff20000
#define _NORM_JAMO_V_TOP        0xfff30000

/* value constants for auxTrie */
enum {
    _NORM_AUX_COMP_EX_SHIFT=10,
    _NORM_AUX_UNSAFE_SHIFT=11,
    _NORM_AUX_NFC_SKIPPABLE_F_SHIFT=12
};

#define _NORM_AUX_MAX_FNC           ((int32_t)1<<_NORM_AUX_COMP_EX_SHIFT)

#define _NORM_AUX_FNC_MASK          (uint32_t)(_NORM_AUX_MAX_FNC-1)
#define _NORM_AUX_COMP_EX_MASK      ((uint32_t)1<<_NORM_AUX_COMP_EX_SHIFT)
#define _NORM_AUX_UNSAFE_MASK       ((uint32_t)1<<_NORM_AUX_UNSAFE_SHIFT)
#define _NORM_AUX_NFC_SKIP_F_MASK   ((uint32_t)1<<_NORM_AUX_NFC_SKIPPABLE_F_SHIFT)

/* canonStartSets[0..31] contains indexes for what is in the array */
enum {
    _NORM_SET_INDEX_CANON_SETS_LENGTH,      /* number of uint16_t in canonical starter sets */
    _NORM_SET_INDEX_CANON_BMP_TABLE_LENGTH, /* number of uint16_t in the BMP search table (contains pairs) */
    _NORM_SET_INDEX_CANON_SUPP_TABLE_LENGTH,/* number of uint16_t in the supplementary search table (contains triplets) */

    /* from formatVersion 2.3: */
    _NORM_SET_INDEX_NX_CJK_COMPAT_OFFSET,   /* uint16_t offset from canonStartSets[0] to the
                                               exclusion set for CJK compatibility characters */
    _NORM_SET_INDEX_NX_UNICODE32_OFFSET,    /* uint16_t offset from canonStartSets[0] to the
                                               exclusion set for Unicode 3.2 characters */
    _NORM_SET_INDEX_NX_RESERVED_OFFSET,     /* uint16_t offset from canonStartSets[0] to the
                                               end of the previous exclusion set */

    _NORM_SET_INDEX_TOP=32                  /* changing this requires a new formatVersion */
};

/* more constants for canonical starter sets */

/* 14 bit indexes to canonical USerializedSets */
#define _NORM_MAX_CANON_SETS            0x4000

/* single-code point BMP sets are encoded directly in the search table except if result=0x4000..0x7fff */
#define _NORM_CANON_SET_BMP_MASK        0xc000
#define _NORM_CANON_SET_BMP_IS_INDEX    0x4000

/* indexes[] value names */
enum {
    _NORM_INDEX_TRIE_SIZE,              /* number of bytes in normalization trie */
    _NORM_INDEX_UCHAR_COUNT,            /* number of UChars in extra data */

    _NORM_INDEX_COMBINE_DATA_COUNT,     /* number of uint16_t words for combining data */
    _NORM_INDEX_COMBINE_FWD_COUNT,      /* number of code points that combine forward */
    _NORM_INDEX_COMBINE_BOTH_COUNT,     /* number of code points that combine forward and backward */
    _NORM_INDEX_COMBINE_BACK_COUNT,     /* number of code points that combine backward */

    _NORM_INDEX_MIN_NFC_NO_MAYBE,       /* first code point with quick check NFC NO/MAYBE */
    _NORM_INDEX_MIN_NFKC_NO_MAYBE,      /* first code point with quick check NFKC NO/MAYBE */
    _NORM_INDEX_MIN_NFD_NO_MAYBE,       /* first code point with quick check NFD NO/MAYBE */
    _NORM_INDEX_MIN_NFKD_NO_MAYBE,      /* first code point with quick check NFKD NO/MAYBE */

    _NORM_INDEX_FCD_TRIE_SIZE,          /* number of bytes in FCD trie */

    _NORM_INDEX_AUX_TRIE_SIZE,          /* number of bytes in the auxiliary trie */
    _NORM_INDEX_CANON_SET_COUNT,        /* number of uint16_t in the array of serialized USet */

    _NORM_INDEX_TOP=32                  /* changing this requires a new formatVersion */
};

enum {
    /* FCD check: everything below this code point is known to have a 0 lead combining class */
    _NORM_MIN_WITH_LEAD_CC=0x300
};

enum {
    /**
     * Bit 7 of the length byte for a decomposition string in extra data is
     * a flag indicating whether the decomposition string is
     * preceded by a 16-bit word with the leading and trailing cc
     * of the decomposition (like for A-umlaut);
     * if not, then both cc's are zero (like for compatibility ideographs).
     */
    _NORM_DECOMP_FLAG_LENGTH_HAS_CC=0x80,
    /**
     * Bits 6..0 of the length byte contain the actual length.
     */
    _NORM_DECOMP_LENGTH_MASK=0x7f
};

/** Constants for options flags for normalization. */
enum {
    /** Options bit 0, do not decompose Hangul syllables. */
    UNORM_NX_HANGUL=1,
    /** Options bit 1, do not decompose CJK compatibility characters. */
    UNORM_NX_CJK_COMPAT=2
};

/**
 * Description of the format of unorm.icu version 2.3.
 *
 * Main change from version 1 to version 2:
 * Use of new, common UTrie instead of normalization-specific tries.
 * Change to version 2.1: add third/auxiliary trie with associated data.
 * Change to version 2.2: add skippable (f) flag data (_NORM_AUX_NFC_SKIP_F_MASK).
 * Change to version 2.3: add serialized sets for normalization exclusions
 *                        stored inside canonStartSets[]
 *
 * For more details of how to use the data structures see the code
 * in unorm.cpp (runtime normalization code) and
 * in gennorm.c and gennorm/store.c (build-time data generation).
 *
 * For the serialized format of UTrie see utrie.c/UTrieHeader.
 *
 * - Overall partition
 *
 * unorm.dat customarily begins with a UDataInfo structure, see udata.h and .c.
 * After that there are the following structures:
 *
 * int32_t indexes[_NORM_INDEX_TOP];            -- _NORM_INDEX_TOP=32, see enum in this file
 *
 * UTrie normTrie;                              -- size in bytes=indexes[_NORM_INDEX_TRIE_SIZE]
 * 
 * uint16_t extraData[extraDataTop];            -- extraDataTop=indexes[_NORM_INDEX_UCHAR_COUNT]
 *                                                 extraData[0] contains the number of units for
 *                                                 FC_NFKC_Closure (formatVersion>=2.1)
 *
 * uint16_t combiningTable[combiningTableTop];  -- combiningTableTop=indexes[_NORM_INDEX_COMBINE_DATA_COUNT]
 *                                                 combiningTableTop may include one 16-bit padding unit
 *                                                 to make sure that fcdTrie is 32-bit-aligned
 *
 * UTrie fcdTrie;                               -- size in bytes=indexes[_NORM_INDEX_FCD_TRIE_SIZE]
 *
 * UTrie auxTrie;                               -- size in bytes=indexes[_NORM_INDEX_AUX_TRIE_SIZE]
 *
 * uint16_t canonStartSets[canonStartSetsTop]   -- canonStartSetsTop=indexes[_NORM_INDEX_CANON_SET_COUNT]
 *                                                 serialized USets and binary search tables, see below
 *
 *
 * The indexes array contains lengths and sizes of the following arrays and structures
 * as well as the following values:
 *  indexes[_NORM_INDEX_COMBINE_FWD_COUNT]=combineFwdTop
 *      -- one more than the highest combining index computed for forward-only-combining characters
 *  indexes[_NORM_INDEX_COMBINE_BOTH_COUNT]=combineBothTop-combineFwdTop
 *      -- number of combining indexes computed for both-ways-combining characters
 *  indexes[_NORM_INDEX_COMBINE_BACK_COUNT]=combineBackTop-combineBothTop
 *      -- number of combining indexes computed for backward-only-combining characters
 *
 *  indexes[_NORM_INDEX_MIN_NF*_NO_MAYBE] (where *={ C, D, KC, KD })
 *      -- first code point with a quick check NF* value of NO/MAYBE
 *
 *
 * - Tries
 *
 * The main structures are two UTrie tables ("compact arrays"),
 * each with one index array and one data array.
 * See utrie.h and utrie.c.
 *
 *
 * - Tries in unorm.dat
 *
 * The first trie (normTrie above)
 * provides data for the NF* quick checks and normalization.
 * The second trie (fcdTrie above) provides data just for FCD checks.
 *
 *
 * - norm32 data words from the first trie
 *
 * The norm32Table contains one 32-bit word "norm32" per code point.
 * It contains the following bit fields:
 * 31..16   extra data index, _NORM_EXTRA_SHIFT is used to shift this field down
 *          if this index is <_NORM_EXTRA_INDEX_TOP then it is an index into
 *              extraData[] where variable-length normalization data for this
 *              code point is found
 *          if this index is <_NORM_EXTRA_INDEX_TOP+_NORM_EXTRA_SURROGATE_TOP
 *              then this is a norm32 for a leading surrogate, and the index
 *              value is used together with the following trailing surrogate
 *              code unit in the second trie access
 *          if this index is >=_NORM_EXTRA_INDEX_TOP+_NORM_EXTRA_SURROGATE_TOP
 *              then this is a norm32 for a "special" character,
 *              i.e., the character is a Hangul syllable or a Jamo
 *              see _NORM_EXTRA_HANGUL etc.
 *          generally, instead of extracting this index from the norm32 and
 *              comparing it with the above constants,
 *              the normalization code compares the entire norm32 value
 *              with _NORM_MIN_SPECIAL, _NORM_SURROGATES_TOP, _NORM_MIN_HANGUL etc.
 *
 * 15..8    combining class (cc) according to UnicodeData.txt
 *
 *  7..6    _NORM_COMBINES_ANY flags, used in composition to see if a character
 *              combines with any following or preceding character(s)
 *              at all
 *     7    _NORM_COMBINES_BACK
 *     6    _NORM_COMBINES_FWD
 *
 *  5..0    quick check flags, set for "no" or "maybe", with separate flags for
 *              each normalization form
 *              the higher bits are "maybe" flags; for NF*D there are no such flags
 *              the lower bits are "no" flags for all forms, in the same order
 *              as the "maybe" flags,
 *              which is (MSB to LSB): NFKD NFD NFKC NFC
 *  5..4    _NORM_QC_ANY_MAYBE
 *  3..0    _NORM_QC_ANY_NO
 *              see further related constants
 *
 *
 * - Extra data per code point
 *
 * "Extra data" is referenced by the index in norm32.
 * It is variable-length data. It is only present, and only those parts
 * of it are, as needed for a given character.
 * The norm32 extra data index is added to the beginning of extraData[]
 * to get to a vector of 16-bit words with data at the following offsets:
 *
 * [-1]     Combining index for composition.
 *              Stored only if norm32&_NORM_COMBINES_ANY .
 * [0]      Lengths of the canonical and compatibility decomposition strings.
 *              Stored only if there are decompositions, i.e.,
 *              if norm32&(_NORM_QC_NFD|_NORM_QC_NFKD)
 *          High byte: length of NFKD, or 0 if none
 *          Low byte: length of NFD, or 0 if none
 *          Each length byte also has another flag:
 *              Bit 7 of a length byte is set if there are non-zero
 *              combining classes (cc's) associated with the respective
 *              decomposition. If this flag is set, then the decomposition
 *              is preceded by a 16-bit word that contains the
 *              leading and trailing cc's.
 *              Bits 6..0 of a length byte are the length of the
 *              decomposition string, not counting the cc word.
 * [1..n]   NFD
 * [n+1..]  NFKD
 *
 * Each of the two decompositions consists of up to two parts:
 * - The 16-bit words with the leading and trailing cc's.
 *   This is only stored if bit 7 of the corresponding length byte
 *   is set. In this case, at least one of the cc's is not zero.
 *   High byte: leading cc==cc of the first code point in the decomposition string
 *   Low byte: trailing cc==cc of the last code point in the decomposition string
 * - The decomposition string in UTF-16, with length code units.
 *
 *
 * - Combining indexes and combiningTable[]
 *
 * Combining indexes are stored at the [-1] offset of the extra data
 * if the character combines forward or backward with any other characters.
 * They are used for (re)composition in NF*C.
 * Values of combining indexes are arranged according to whether a character
 * combines forward, backward, or both ways:
 *    forward-only < both ways < backward-only
 *
 * The index values for forward-only and both-ways combining characters
 * are indexes into the combiningTable[].
 * The index values for backward-only combining characters are simply
 * incremented from the preceding index values to be unique.
 *
 * In the combiningTable[], a variable-length list
 * of variable-length (back-index, code point) pair entries is stored
 * for each forward-combining character.
 *
 * These back-indexes are the combining indexes of both-ways or backward-only
 * combining characters that the forward-combining character combines with.
 *
 * Each list is sorted in ascending order of back-indexes.
 * Each list is terminated with the last back-index having bit 15 set.
 *
 * Each pair (back-index, code point) takes up either 2 or 3
 * 16-bit words.
 * The first word of a list entry is the back-index, with its bit 15 set if
 * this is the last pair in the list.
 *
 * The second word contains flags in bits 15..13 that determine
 * if there is a third word and how the combined character is encoded:
 * 15   set if there is a third word in this list entry
 * 14   set if the result is a supplementary character
 * 13   set if the result itself combines forward
 *
 * According to these bits 15..14 of the second word,
 * the result character is encoded as follows:
 * 00 or 01 The result is <=0x1fff and stored in bits 12..0 of
 *          the second word.
 * 10       The result is 0x2000..0xffff and stored in the third word.
 *          Bits 12..0 of the second word are not used.
 * 11       The result is a supplementary character.
 *          Bits 9..0 of the leading surrogate are in bits 9..0 of
 *          the second word.
 *          Add 0xd800 to these bits to get the complete surrogate.
 *          Bits 12..10 of the second word are not used.
 *          The trailing surrogate is stored in the third word.
 *
 *
 * - FCD trie
 *
 * The FCD trie is very simple.
 * It is a folded trie with 16-bit data words.
 * In each word, the high byte contains the leading cc of the character,
 * and the low byte contains the trailing cc of the character.
 * These cc's are the cc's of the first and last code points in the
 * canonical decomposition of the character.
 *
 * Since all 16 bits are used for cc's, lead surrogates must be tested
 * by checking the code unit instead of the trie data.
 * This is done only if the 16-bit data word is not zero.
 * If the code unit is a leading surrogate and the data word is not zero,
 * then instead of cc's it contains the offset for the second trie lookup.
 *
 *
 * - Auxiliary trie and data
 *
 * The auxiliary 16-bit trie contains data for additional properties.
 * Bits
 * 15..13   reserved
 *     12   not NFC_Skippable (f) (formatVersion>=2.2)
 *     11   flag: not a safe starter for canonical closure
 *     10   composition exclusion
 *  9.. 0   index into extraData[] to FC_NFKC_Closure string
 *          (not for lead surrogate),
 *          or lead surrogate offset (for lead surrogate, if 9..0 not zero)
 *
 * - FC_NFKC_Closure strings in extraData[]
 *
 * Strings are either stored as a single code unit or as the length
 * followed by that many units.
 *   const UChar *s=extraData+(index from auxTrie data bits 9..0);
 *   int32_t length;
 *   if(*s<0xff00) {
 *     // s points to the single-unit string
 *     length=1;
 *   } else {
 *     length=*s&0xff;
 *     ++s;
 *   }
 *
 * Conditions for "NF* Skippable" from Mark Davis' com.ibm.text.UCD.NFSkippable:
 * (used in NormalizerTransliterator)
 *
 * A skippable character is
 * a) unassigned, or ALL of the following:
 * b) of combining class 0.
 * c) not decomposed by this normalization form.
 * AND if NFC or NFKC,
 * d) can never compose with a previous character.
 * e) can never compose with a following character.
 * f) can never change if another character is added.
 *    Example: a-breve might satisfy all but f, but if you
 *    add an ogonek it changes to a-ogonek + breve
 *
 * a)..e) must be tested from norm32.
 * Since f) is more complicated, the (not-)NFC_Skippable flag (f) is built
 * into the auxiliary trie.
 * The same bit is used for NFC and NFKC; (c) differs for them.
 * As usual, we build the "not skippable" flags so that unassigned
 * code points get a 0 bit.
 * This bit is only valid after (a)..(e) test FALSE; test NFD_NO before (f) as well.
 * Test Hangul LV syllables entirely in code.
 *
 *
 * - structure inside canonStartSets[]
 *
 * This array maps from code points c to sets of code points (USerializedSet).
 * The result sets are the code points whose canonical decompositions start
 * with c.
 *
 * canonStartSets[] contains the following sub-arrays:
 *
 * indexes[_NORM_SET_INDEX_TOP]
 *   - contains lengths of sub-arrays etc.
 *
 * startSets[indexes[_NORM_SET_INDEX_CANON_SETS_LENGTH]-_NORM_SET_INDEX_TOP]
 *   - contains serialized sets (USerializedSet) of canonical starters for
 *     enumerating canonically equivalent strings
 *     indexes[_NORM_SET_INDEX_CANON_SETS_LENGTH] includes _NORM_SET_INDEX_TOP
 *     for details about the structure see uset.c
 *
 * bmpTable[indexes[_NORM_SET_INDEX_CANON_BMP_TABLE_LENGTH]]
 *   - a sorted search table for BMP code points whose results are
 *     either indexes to USerializedSets or single code points for
 *     single-code point sets;
 *     each entry is a pair of { code point, result } with result=(binary) yy xxxxxx xxxxxxxx
 *     if yy==01 then there is a USerializedSet at canonStartSets+x
 *     else build a USerializedSet with result as the single code point
 *
 * suppTable[indexes[_NORM_SET_INDEX_CANON_SUPP_TABLE_LENGTH]]
 *   - a sorted search table for supplementary code points whose results are
 *     either indexes to USerializedSets or single code points for
 *     single-code point sets;
 *     each entry is a triplet of { high16(cp), low16(cp), result }
 *     each code point's high-word may contain extra data in bits 15..5:
 *     if the high word has bit 15 set, then build a set with a single code point
 *     which is (((high16(cp)&0x1f00)<<8)|result;
 *     else there is a USerializedSet at canonStartSets+result
 *
 * FormatVersion 2.3 adds 2 serialized sets for normalization exclusions.
 * They are stored in the data file so that the runtime normalization code need
 * not depend on other properties and their data and implementation files.
 * The _NORM_SET_INDEX_NX_..._OFFSET offsets in the canonStartSets index table
 * give the location for each set.
 * There is no set stored for UNORM_NX_HANGUL because it's trivial to create
 * without using properties.
 *
 * Set contents:
 *
 * _NORM_SET_INDEX_NX_CJK_COMPAT_OFFSET (for UNORM_NX_CJK_COMPAT)
 *     [[:Ideographic:]&[:NFD_QC=No:]]
 *     =[CJK Ideographs]&[has canonical decomposition]
 *
 * _NORM_SET_INDEX_NX_UNICODE32_OFFSET (for UNORM_UNICODE_3_2)
 *     [:^Age=3.2:]
 *     =set with all code points that were not designated by the specified Unicode version
 *
 * _NORM_SET_INDEX_NX_RESERVED_OFFSET
 *     This is an offset that points to where the next, future set would start.
 *     Currently it indicates where the previous set ends, and thus its length.
 *     The name for this enum constant may in the future be applied to different
 *     index slots. In order to get the limit of a set, use its index slot and
 *     the immediately following one regardless of that one's enum name.
 */

#endif /* #if !UCONFIG_NO_NORMALIZATION */

#endif
