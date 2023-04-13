// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2002-2011, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: October 30 2002
* Since: ICU 2.4
* 2010nov19 Markus Scherer  Rewrite for formatVersion 2.
**********************************************************************
*/
#ifndef PROPNAME_H
#define PROPNAME_H

#include "unicode/utypes.h"
#include "unicode/bytestrie.h"
#include "unicode/uchar.h"
#include "udataswp.h"
#include "uprops.h"

/*
 * This header defines the in-memory layout of the property names data
 * structure representing the UCD data files PropertyAliases.txt and
 * PropertyValueAliases.txt.  It is used by:
 *   propname.cpp - reads data
 *   genpname     - creates data
 */

/* low-level char * property name comparison -------------------------------- */

U_CDECL_BEGIN

/**
 * \var uprv_comparePropertyNames
 * Unicode property names and property value names are compared "loosely".
 *
 * UCD.html 4.0.1 says:
 *   For all property names, property value names, and for property values for
 *   Enumerated, Binary, or Catalog properties, use the following
 *   loose matching rule:
 *
 *   LM3. Ignore case, whitespace, underscore ('_'), and hyphens.
 *
 * This function does just that, for (char *) name strings.
 * It is almost identical to ucnv_compareNames() but also ignores
 * C0 White_Space characters (U+0009..U+000d, and U+0085 on EBCDIC).
 *
 * @internal
 */

U_CAPI int32_t U_EXPORT2
uprv_compareASCIIPropertyNames(const char *name1, const char *name2);

U_CAPI int32_t U_EXPORT2
uprv_compareEBCDICPropertyNames(const char *name1, const char *name2);

#if U_CHARSET_FAMILY==U_ASCII_FAMILY
#   define uprv_comparePropertyNames uprv_compareASCIIPropertyNames
#elif U_CHARSET_FAMILY==U_EBCDIC_FAMILY
#   define uprv_comparePropertyNames uprv_compareEBCDICPropertyNames
#else
#   error U_CHARSET_FAMILY is not valid
#endif

U_CDECL_END

/* UDataMemory structure and signatures ------------------------------------- */

#define PNAME_DATA_NAME "pnames"
#define PNAME_DATA_TYPE "icu"

/* Fields in UDataInfo: */

/* PNAME_SIG[] is encoded as numeric literals for compatibility with the HP compiler */
#define PNAME_SIG_0 ((uint8_t)0x70) /* p */
#define PNAME_SIG_1 ((uint8_t)0x6E) /* n */
#define PNAME_SIG_2 ((uint8_t)0x61) /* a */
#define PNAME_SIG_3 ((uint8_t)0x6D) /* m */

U_NAMESPACE_BEGIN

class PropNameData {
public:
    enum {
        // Byte offsets from the start of the data, after the generic header.
        IX_VALUE_MAPS_OFFSET,
        IX_BYTE_TRIES_OFFSET,
        IX_NAME_GROUPS_OFFSET,
        IX_RESERVED3_OFFSET,
        IX_RESERVED4_OFFSET,
        IX_TOTAL_SIZE,

        // Other values.
        IX_MAX_NAME_LENGTH,
        IX_RESERVED7,
        IX_COUNT
    };

    static const char *getPropertyName(int32_t property, int32_t nameChoice);
    static const char *getPropertyValueName(int32_t property, int32_t value, int32_t nameChoice);

    static int32_t getPropertyEnum(const char *alias);
    static int32_t getPropertyValueEnum(int32_t property, const char *alias);

private:
    static int32_t findProperty(int32_t property);
    static int32_t findPropertyValueNameGroup(int32_t valueMapIndex, int32_t value);
    static const char *getName(const char *nameGroup, int32_t nameIndex);
    static UBool containsName(BytesTrie &trie, const char *name);

    static int32_t getPropertyOrValueEnum(int32_t bytesTrieOffset, const char *alias);

    static const int32_t indexes[];
    static const int32_t valueMaps[];
    static const uint8_t bytesTries[];
    static const char nameGroups[];
};

/*
 * pnames.icu formatVersion 2
 *
 * formatVersion 2 is new in ICU 4.8.
 * In ICU 4.8, the pnames.icu data file is used only in ICU4J.
 * ICU4C 4.8 has the same data structures hardcoded in source/common/propname_data.h.
 *
 * For documentation of pnames.icu formatVersion 1 see ICU4C 4.6 (2010-dec-01)
 * or earlier versions of this header file (source/common/propname.h).
 *
 * The pnames.icu begins with the standard ICU DataHeader/UDataInfo.
 * After that:
 *
 * int32_t indexes[8];
 *
 *      (See the PropNameData::IX_... constants.)
 *
 *      The first 6 indexes are byte offsets from the beginning of the data
 *      (beginning of indexes[]) to following structures.
 *      The length of each structure is the difference between its offset
 *      and the next one.
 *      All offsets are filled in: Where there is no data between two offsets,
 *      those two offsets are the same.
 *      The last offset (indexes[PropNameData::IX_TOTAL_SIZE]) indicates the
 *      total number of bytes in the file. (Not counting the standard headers.)
 *
 *      The sixth index (indexes[PropNameData::IX_MAX_NAME_LENGTH]) has the
 *      maximum length of any Unicode property (or property value) alias.
 *      (Without normalization, that is, including underscores etc.)
 *
 * int32_t valueMaps[];
 *
 *      The valueMaps[] begins with a map from UProperty enums to properties,
 *      followed by the per-property value maps from property values to names,
 *      for those properties that have named values.
 *      (Binary & enumerated, plus General_Category_Mask.)
 *
 *      valueMaps[0] contains the number of UProperty enum ranges.
 *      For each range:
 *        int32_t start, limit -- first and last+1 UProperty enum of a dense range
 *        Followed by (limit-start) pairs of
 *          int32_t nameGroupOffset;
 *            Offset into nameGroups[] for the property's names/aliases.
 *          int32_t valueMapIndex;
 *            Offset of the property's value map in the valueMaps[] array.
 *            If the valueMapIndex is 0, then the property does not have named values.
 *
 *      For each property's value map:
 *      int32_t bytesTrieOffset; -- Offset into bytesTries[] for name->value mapping.
 *      int32_t numRanges;
 *        If numRanges is in the range 1..15, then that many ranges of values follow.
 *        Per range:
 *          int32_t start, limit -- first and last+1 UProperty enum of a range
 *          Followed by (limit-start) entries of
 *            int32_t nameGroupOffset;
 *              Offset into nameGroups[] for the property value's names/aliases.
 *              If the nameGroupOffset is 0, then this is not a named value for this property.
 *              (That is, the ranges need not be dense.)
 *        If numRanges is >=0x10, then (numRanges-0x10) sorted values
 *        and then (numRanges-0x10) corresponding nameGroupOffsets follow.
 *        Values are sorted as signed integers.
 *        In this case, the set of values is dense; no nameGroupOffset will be 0.
 *
 *      For both properties and property values, ranges are sorted by their start/limit values.
 *
 * uint8_t bytesTries[];
 *
 *      This is a sequence of BytesTrie structures, byte-serialized tries for
 *      mapping from names/aliases to values.
 *      The first one maps from property names/aliases to UProperty enum constants.
 *      The following ones are indexed by property value map bytesTrieOffsets
 *      for mapping each property's names/aliases to their property values.
 *
 * char nameGroups[];
 *
 *      This is a sequence of property name groups.
 *      Each group is a list of names/aliases (invariant-character strings) for
 *      one property or property value, in the order of UCharNameChoice.
 *      The first byte of each group is the number of names in the group.
 *      It is followed by that many NUL-terminated strings.
 *      The first string is for the short name; if there is no short name,
 *      then the first string is empty.
 *      The second string is the long name. Further strings are additional aliases.
 *
 *      The first name group is for a property rather than a property value,
 *      so that a nameGroupOffset of 0 can be used to indicate "no value"
 *      in a property's sparse value ranges.
 */

U_NAMESPACE_END

#endif
