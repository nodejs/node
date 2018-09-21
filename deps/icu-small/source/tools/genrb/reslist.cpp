// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2000-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File reslist.cpp
*
* Modification History:
*
*   Date        Name        Description
*   02/21/00    weiv        Creation.
*******************************************************************************
*/

// Safer use of UnicodeString.
#ifndef UNISTR_FROM_CHAR_EXPLICIT
#   define UNISTR_FROM_CHAR_EXPLICIT explicit
#endif

// Less important, but still a good idea.
#ifndef UNISTR_FROM_STRING_EXPLICIT
#   define UNISTR_FROM_STRING_EXPLICIT explicit
#endif

#include <assert.h>
#include <stdio.h>
#include "unicode/localpointer.h"
#include "reslist.h"
#include "unewdata.h"
#include "unicode/ures.h"
#include "unicode/putil.h"
#include "errmsg.h"

#include "uarrsort.h"
#include "uelement.h"
#include "uhash.h"
#include "uinvchar.h"
#include "ustr_imp.h"
#include "unicode/utf16.h"
/*
 * Align binary data at a 16-byte offset from the start of the resource bundle,
 * to be safe for any data type it may contain.
 */
#define BIN_ALIGNMENT 16

// This numeric constant must be at least 1.
// If StringResource.fNumUnitsSaved == 0 then the string occurs only once,
// and it makes no sense to move it to the pool bundle.
// The larger the threshold for fNumUnitsSaved
// the smaller the savings, and the smaller the pool bundle.
// We trade some total size reduction to reduce the pool bundle a bit,
// so that one can reasonably save data size by
// removing bundle files without rebuilding the pool bundle.
// This can also help to keep the pool and total (pool+local) string indexes
// within 16 bits, that is, within range of Table16 and Array16 containers.
#ifndef GENRB_MIN_16BIT_UNITS_SAVED_FOR_POOL_STRING
#   define GENRB_MIN_16BIT_UNITS_SAVED_FOR_POOL_STRING 10
#endif

U_NAMESPACE_USE

static UBool gIncludeCopyright = FALSE;
static UBool gUsePoolBundle = FALSE;
static UBool gIsDefaultFormatVersion = TRUE;
static int32_t gFormatVersion = 3;

/* How do we store string values? */
enum {
    STRINGS_UTF16_V1,   /* formatVersion 1: int length + UChars + NUL + padding to 4 bytes */
    STRINGS_UTF16_V2    /* formatVersion 2 & up: optional length in 1..3 UChars + UChars + NUL */
};

static const int32_t MAX_IMPLICIT_STRING_LENGTH = 40;  /* do not store the length explicitly for such strings */

static const ResFile kNoPoolBundle;

/*
 * res_none() returns the address of kNoResource,
 * for use in non-error cases when no resource is to be added to the bundle.
 * (NULL is used in error cases.)
 */
static SResource kNoResource;  // TODO: const

static UDataInfo dataInfo= {
    sizeof(UDataInfo),
    0,

    U_IS_BIG_ENDIAN,
    U_CHARSET_FAMILY,
    sizeof(UChar),
    0,

    {0x52, 0x65, 0x73, 0x42},     /* dataFormat="ResB" */
    {1, 3, 0, 0},                 /* formatVersion */
    {1, 4, 0, 0}                  /* dataVersion take a look at version inside parsed resb*/
};

static const UVersionInfo gFormatVersions[4] = {  /* indexed by a major-formatVersion integer */
    { 0, 0, 0, 0 },
    { 1, 3, 0, 0 },
    { 2, 0, 0, 0 },
    { 3, 0, 0, 0 }
};
// Remember to update genrb.h GENRB_VERSION when changing the data format.
// (Or maybe we should remove GENRB_VERSION and report the ICU version number?)

static uint8_t calcPadding(uint32_t size) {
    /* returns space we need to pad */
    return (uint8_t) ((size % sizeof(uint32_t)) ? (sizeof(uint32_t) - (size % sizeof(uint32_t))) : 0);

}

void setIncludeCopyright(UBool val){
    gIncludeCopyright=val;
}

UBool getIncludeCopyright(void){
    return gIncludeCopyright;
}

void setFormatVersion(int32_t formatVersion) {
    gIsDefaultFormatVersion = FALSE;
    gFormatVersion = formatVersion;
}

int32_t getFormatVersion() {
    return gFormatVersion;
}

void setUsePoolBundle(UBool use) {
    gUsePoolBundle = use;
}

// TODO: return const pointer, or find another way to express "none"
struct SResource* res_none() {
    return &kNoResource;
}

SResource::SResource()
        : fType(URES_NONE), fWritten(FALSE), fRes(RES_BOGUS), fRes16(-1), fKey(-1), fKey16(-1),
          line(0), fNext(NULL) {
    ustr_init(&fComment);
}

SResource::SResource(SRBRoot *bundle, const char *tag, int8_t type, const UString* comment,
                     UErrorCode &errorCode)
        : fType(type), fWritten(FALSE), fRes(RES_BOGUS), fRes16(-1),
          fKey(bundle != NULL ? bundle->addTag(tag, errorCode) : -1), fKey16(-1),
          line(0), fNext(NULL) {
    ustr_init(&fComment);
    if(comment != NULL) {
        ustr_cpy(&fComment, comment, &errorCode);
    }
}

SResource::~SResource() {
    ustr_deinit(&fComment);
}

ContainerResource::~ContainerResource() {
    SResource *current = fFirst;
    while (current != NULL) {
        SResource *next = current->fNext;
        delete current;
        current = next;
    }
}

TableResource::~TableResource() {}

// TODO: clarify that containers adopt new items, even in error cases; use LocalPointer
void TableResource::add(SResource *res, int linenumber, UErrorCode &errorCode) {
    if (U_FAILURE(errorCode) || res == NULL || res == &kNoResource) {
        return;
    }

    /* remember this linenumber to report to the user if there is a duplicate key */
    res->line = linenumber;

    /* here we need to traverse the list */
    ++fCount;

    /* is the list still empty? */
    if (fFirst == NULL) {
        fFirst = res;
        res->fNext = NULL;
        return;
    }

    const char *resKeyString = fRoot->fKeys + res->fKey;

    SResource *current = fFirst;

    SResource *prev = NULL;
    while (current != NULL) {
        const char *currentKeyString = fRoot->fKeys + current->fKey;
        int diff;
        /*
         * formatVersion 1: compare key strings in native-charset order
         * formatVersion 2 and up: compare key strings in ASCII order
         */
        if (gFormatVersion == 1 || U_CHARSET_FAMILY == U_ASCII_FAMILY) {
            diff = uprv_strcmp(currentKeyString, resKeyString);
        } else {
            diff = uprv_compareInvCharsAsAscii(currentKeyString, resKeyString);
        }
        if (diff < 0) {
            prev    = current;
            current = current->fNext;
        } else if (diff > 0) {
            /* we're either in front of the list, or in the middle */
            if (prev == NULL) {
                /* front of the list */
                fFirst = res;
            } else {
                /* middle of the list */
                prev->fNext = res;
            }

            res->fNext = current;
            return;
        } else {
            /* Key already exists! ERROR! */
            error(linenumber, "duplicate key '%s' in table, first appeared at line %d", currentKeyString, current->line);
            errorCode = U_UNSUPPORTED_ERROR;
            return;
        }
    }

    /* end of list */
    prev->fNext = res;
    res->fNext  = NULL;
}

ArrayResource::~ArrayResource() {}

void ArrayResource::add(SResource *res) {
    if (res != NULL && res != &kNoResource) {
        if (fFirst == NULL) {
            fFirst = res;
        } else {
            fLast->fNext = res;
        }
        fLast = res;
        ++fCount;
    }
}

PseudoListResource::~PseudoListResource() {}

void PseudoListResource::add(SResource *res) {
    if (res != NULL && res != &kNoResource) {
        res->fNext = fFirst;
        fFirst = res;
        ++fCount;
    }
}

StringBaseResource::StringBaseResource(SRBRoot *bundle, const char *tag, int8_t type,
                                       const UChar *value, int32_t len,
                                       const UString* comment, UErrorCode &errorCode)
        : SResource(bundle, tag, type, comment, errorCode) {
    if (len == 0 && gFormatVersion > 1) {
        fRes = URES_MAKE_EMPTY_RESOURCE(type);
        fWritten = TRUE;
        return;
    }

    fString.setTo(ConstChar16Ptr(value), len);
    fString.getTerminatedBuffer();  // Some code relies on NUL-termination.
    if (U_SUCCESS(errorCode) && fString.isBogus()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
}

StringBaseResource::StringBaseResource(SRBRoot *bundle, int8_t type,
                                       const icu::UnicodeString &value, UErrorCode &errorCode)
        : SResource(bundle, NULL, type, NULL, errorCode), fString(value) {
    if (value.isEmpty() && gFormatVersion > 1) {
        fRes = URES_MAKE_EMPTY_RESOURCE(type);
        fWritten = TRUE;
        return;
    }

    fString.getTerminatedBuffer();  // Some code relies on NUL-termination.
    if (U_SUCCESS(errorCode) && fString.isBogus()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
}

// Pool bundle string, alias the buffer. Guaranteed NUL-terminated and not empty.
StringBaseResource::StringBaseResource(int8_t type, const UChar *value, int32_t len,
                                       UErrorCode &errorCode)
        : SResource(NULL, NULL, type, NULL, errorCode), fString(TRUE, value, len) {
    assert(len > 0);
    assert(!fString.isBogus());
}

StringBaseResource::~StringBaseResource() {}

static int32_t U_CALLCONV
string_hash(const UElement key) {
    const StringResource *res = static_cast<const StringResource *>(key.pointer);
    return res->fString.hashCode();
}

static UBool U_CALLCONV
string_comp(const UElement key1, const UElement key2) {
    const StringResource *res1 = static_cast<const StringResource *>(key1.pointer);
    const StringResource *res2 = static_cast<const StringResource *>(key2.pointer);
    return res1->fString == res2->fString;
}

StringResource::~StringResource() {}

AliasResource::~AliasResource() {}

IntResource::IntResource(SRBRoot *bundle, const char *tag, int32_t value,
                         const UString* comment, UErrorCode &errorCode)
        : SResource(bundle, tag, URES_INT, comment, errorCode) {
    fValue = value;
    fRes = URES_MAKE_RESOURCE(URES_INT, value & RES_MAX_OFFSET);
    fWritten = TRUE;
}

IntResource::~IntResource() {}

IntVectorResource::IntVectorResource(SRBRoot *bundle, const char *tag,
                  const UString* comment, UErrorCode &errorCode)
        : SResource(bundle, tag, URES_INT_VECTOR, comment, errorCode),
          fCount(0), fArray(new uint32_t[RESLIST_MAX_INT_VECTOR]) {
    if (fArray == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}

IntVectorResource::~IntVectorResource() {
    delete[] fArray;
}

void IntVectorResource::add(int32_t value, UErrorCode &errorCode) {
    if (U_SUCCESS(errorCode)) {
        fArray[fCount++] = value;
    }
}

BinaryResource::BinaryResource(SRBRoot *bundle, const char *tag,
                               uint32_t length, uint8_t *data, const char* fileName,
                               const UString* comment, UErrorCode &errorCode)
        : SResource(bundle, tag, URES_BINARY, comment, errorCode),
          fLength(length), fData(NULL), fFileName(NULL) {
    if (U_FAILURE(errorCode)) {
        return;
    }
    if (fileName != NULL && *fileName != 0){
        fFileName = new char[uprv_strlen(fileName)+1];
        if (fFileName == NULL) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        uprv_strcpy(fFileName, fileName);
    }
    if (length > 0) {
        fData = new uint8_t[length];
        if (fData == NULL) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        uprv_memcpy(fData, data, length);
    } else {
        if (gFormatVersion > 1) {
            fRes = URES_MAKE_EMPTY_RESOURCE(URES_BINARY);
            fWritten = TRUE;
        }
    }
}

BinaryResource::~BinaryResource() {
    delete[] fData;
    delete[] fFileName;
}

/* Writing Functions */

void
StringResource::handlePreflightStrings(SRBRoot *bundle, UHashtable *stringSet,
                                       UErrorCode &errorCode) {
    assert(fSame == NULL);
    fSame = static_cast<StringResource *>(uhash_get(stringSet, this));
    if (fSame != NULL) {
        // This is a duplicate of a pool bundle string or of an earlier-visited string.
        if (++fSame->fNumCopies == 1) {
            assert(fSame->fWritten);
            int32_t poolStringIndex = (int32_t)RES_GET_OFFSET(fSame->fRes);
            if (poolStringIndex >= bundle->fPoolStringIndexLimit) {
                bundle->fPoolStringIndexLimit = poolStringIndex + 1;
            }
        }
        return;
    }
    /* Put this string into the set for finding duplicates. */
    fNumCopies = 1;
    uhash_put(stringSet, this, this, &errorCode);

    if (bundle->fStringsForm != STRINGS_UTF16_V1) {
        int32_t len = length();
        if (len <= MAX_IMPLICIT_STRING_LENGTH &&
                !U16_IS_TRAIL(fString[0]) && fString.indexOf((UChar)0) < 0) {
            /*
             * This string will be stored without an explicit length.
             * Runtime will detect !U16_IS_TRAIL(s[0]) and call u_strlen().
             */
            fNumCharsForLength = 0;
        } else if (len <= 0x3ee) {
            fNumCharsForLength = 1;
        } else if (len <= 0xfffff) {
            fNumCharsForLength = 2;
        } else {
            fNumCharsForLength = 3;
        }
        bundle->f16BitStringsLength += fNumCharsForLength + len + 1;  /* +1 for the NUL */
    }
}

void
ContainerResource::handlePreflightStrings(SRBRoot *bundle, UHashtable *stringSet,
                                          UErrorCode &errorCode) {
    for (SResource *current = fFirst; current != NULL; current = current->fNext) {
        current->preflightStrings(bundle, stringSet, errorCode);
    }
}

void
SResource::preflightStrings(SRBRoot *bundle, UHashtable *stringSet, UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }
    if (fRes != RES_BOGUS) {
        /*
         * The resource item word was already precomputed, which means
         * no further data needs to be written.
         * This might be an integer, or an empty string/binary/etc.
         */
        return;
    }
    handlePreflightStrings(bundle, stringSet, errorCode);
}

void
SResource::handlePreflightStrings(SRBRoot * /*bundle*/, UHashtable * /*stringSet*/,
                                  UErrorCode & /*errorCode*/) {
    /* Neither a string nor a container. */
}

int32_t
SRBRoot::makeRes16(uint32_t resWord) const {
    if (resWord == 0) {
        return 0;  /* empty string */
    }
    uint32_t type = RES_GET_TYPE(resWord);
    int32_t offset = (int32_t)RES_GET_OFFSET(resWord);
    if (type == URES_STRING_V2) {
        assert(offset > 0);
        if (offset < fPoolStringIndexLimit) {
            if (offset < fPoolStringIndex16Limit) {
                return offset;
            }
        } else {
            offset = offset - fPoolStringIndexLimit + fPoolStringIndex16Limit;
            if (offset <= 0xffff) {
                return offset;
            }
        }
    }
    return -1;
}

int32_t
SRBRoot::mapKey(int32_t oldpos) const {
    const KeyMapEntry *map = fKeyMap;
    if (map == NULL) {
        return oldpos;
    }
    int32_t i, start, limit;

    /* do a binary search for the old, pre-compactKeys() key offset */
    start = fUsePoolBundle->fKeysCount;
    limit = start + fKeysCount;
    while (start < limit - 1) {
        i = (start + limit) / 2;
        if (oldpos < map[i].oldpos) {
            limit = i;
        } else {
            start = i;
        }
    }
    assert(oldpos == map[start].oldpos);
    return map[start].newpos;
}

/*
 * Only called for UTF-16 v1 strings and duplicate UTF-16 v2 strings.
 * For unique UTF-16 v2 strings, write16() sees fRes != RES_BOGUS
 * and exits early.
 */
void
StringResource::handleWrite16(SRBRoot * /*bundle*/) {
    SResource *same;
    if ((same = fSame) != NULL) {
        /* This is a duplicate. */
        assert(same->fRes != RES_BOGUS && same->fWritten);
        fRes = same->fRes;
        fWritten = same->fWritten;
    }
}

void
ContainerResource::writeAllRes16(SRBRoot *bundle) {
    for (SResource *current = fFirst; current != NULL; current = current->fNext) {
        bundle->f16BitUnits.append((UChar)current->fRes16);
    }
    fWritten = TRUE;
}

void
ArrayResource::handleWrite16(SRBRoot *bundle) {
    if (fCount == 0 && gFormatVersion > 1) {
        fRes = URES_MAKE_EMPTY_RESOURCE(URES_ARRAY);
        fWritten = TRUE;
        return;
    }

    int32_t res16 = 0;
    for (SResource *current = fFirst; current != NULL; current = current->fNext) {
        current->write16(bundle);
        res16 |= current->fRes16;
    }
    if (fCount <= 0xffff && res16 >= 0 && gFormatVersion > 1) {
        fRes = URES_MAKE_RESOURCE(URES_ARRAY16, bundle->f16BitUnits.length());
        bundle->f16BitUnits.append((UChar)fCount);
        writeAllRes16(bundle);
    }
}

void
TableResource::handleWrite16(SRBRoot *bundle) {
    if (fCount == 0 && gFormatVersion > 1) {
        fRes = URES_MAKE_EMPTY_RESOURCE(URES_TABLE);
        fWritten = TRUE;
        return;
    }
    /* Find the smallest table type that fits the data. */
    int32_t key16 = 0;
    int32_t res16 = 0;
    for (SResource *current = fFirst; current != NULL; current = current->fNext) {
        current->write16(bundle);
        key16 |= current->fKey16;
        res16 |= current->fRes16;
    }
    if(fCount > (uint32_t)bundle->fMaxTableLength) {
        bundle->fMaxTableLength = fCount;
    }
    if (fCount <= 0xffff && key16 >= 0) {
        if (res16 >= 0 && gFormatVersion > 1) {
            /* 16-bit count, key offsets and values */
            fRes = URES_MAKE_RESOURCE(URES_TABLE16, bundle->f16BitUnits.length());
            bundle->f16BitUnits.append((UChar)fCount);
            for (SResource *current = fFirst; current != NULL; current = current->fNext) {
                bundle->f16BitUnits.append((UChar)current->fKey16);
            }
            writeAllRes16(bundle);
        } else {
            /* 16-bit count, 16-bit key offsets, 32-bit values */
            fTableType = URES_TABLE;
        }
    } else {
        /* 32-bit count, key offsets and values */
        fTableType = URES_TABLE32;
    }
}

void
PseudoListResource::handleWrite16(SRBRoot * /*bundle*/) {
    fRes = URES_MAKE_EMPTY_RESOURCE(URES_TABLE);
    fWritten = TRUE;
}

void
SResource::write16(SRBRoot *bundle) {
    if (fKey >= 0) {
        // A tagged resource has a non-negative key index into the parsed key strings.
        // compactKeys() built a map from parsed key index to the final key index.
        // After the mapping, negative key indexes are used for shared pool bundle keys.
        fKey = bundle->mapKey(fKey);
        // If the key index fits into a Key16 for a Table or Table16,
        // then set the fKey16 field accordingly.
        // Otherwise keep it at -1.
        if (fKey >= 0) {
            if (fKey < bundle->fLocalKeyLimit) {
                fKey16 = fKey;
            }
        } else {
            int32_t poolKeyIndex = fKey & 0x7fffffff;
            if (poolKeyIndex <= 0xffff) {
                poolKeyIndex += bundle->fLocalKeyLimit;
                if (poolKeyIndex <= 0xffff) {
                    fKey16 = poolKeyIndex;
                }
            }
        }
    }
    /*
     * fRes != RES_BOGUS:
     * The resource item word was already precomputed, which means
     * no further data needs to be written.
     * This might be an integer, or an empty or UTF-16 v2 string,
     * an empty binary, etc.
     */
    if (fRes == RES_BOGUS) {
        handleWrite16(bundle);
    }
    // Compute fRes16 for precomputed as well as just-computed fRes.
    fRes16 = bundle->makeRes16(fRes);
}

void
SResource::handleWrite16(SRBRoot * /*bundle*/) {
    /* Only a few resource types write 16-bit units. */
}

/*
 * Only called for UTF-16 v1 strings, and for aliases.
 * For UTF-16 v2 strings, preWrite() sees fRes != RES_BOGUS
 * and exits early.
 */
void
StringBaseResource::handlePreWrite(uint32_t *byteOffset) {
    /* Write the UTF-16 v1 string. */
    fRes = URES_MAKE_RESOURCE(fType, *byteOffset >> 2);
    *byteOffset += 4 + (length() + 1) * U_SIZEOF_UCHAR;
}

void
IntVectorResource::handlePreWrite(uint32_t *byteOffset) {
    if (fCount == 0 && gFormatVersion > 1) {
        fRes = URES_MAKE_EMPTY_RESOURCE(URES_INT_VECTOR);
        fWritten = TRUE;
    } else {
        fRes = URES_MAKE_RESOURCE(URES_INT_VECTOR, *byteOffset >> 2);
        *byteOffset += (1 + fCount) * 4;
    }
}

void
BinaryResource::handlePreWrite(uint32_t *byteOffset) {
    uint32_t pad       = 0;
    uint32_t dataStart = *byteOffset + sizeof(fLength);

    if (dataStart % BIN_ALIGNMENT) {
        pad = (BIN_ALIGNMENT - dataStart % BIN_ALIGNMENT);
        *byteOffset += pad;  /* pad == 4 or 8 or 12 */
    }
    fRes = URES_MAKE_RESOURCE(URES_BINARY, *byteOffset >> 2);
    *byteOffset += 4 + fLength;
}

void
ContainerResource::preWriteAllRes(uint32_t *byteOffset) {
    for (SResource *current = fFirst; current != NULL; current = current->fNext) {
        current->preWrite(byteOffset);
    }
}

void
ArrayResource::handlePreWrite(uint32_t *byteOffset) {
    preWriteAllRes(byteOffset);
    fRes = URES_MAKE_RESOURCE(URES_ARRAY, *byteOffset >> 2);
    *byteOffset += (1 + fCount) * 4;
}

void
TableResource::handlePreWrite(uint32_t *byteOffset) {
    preWriteAllRes(byteOffset);
    if (fTableType == URES_TABLE) {
        /* 16-bit count, 16-bit key offsets, 32-bit values */
        fRes = URES_MAKE_RESOURCE(URES_TABLE, *byteOffset >> 2);
        *byteOffset += 2 + fCount * 6;
    } else {
        /* 32-bit count, key offsets and values */
        fRes = URES_MAKE_RESOURCE(URES_TABLE32, *byteOffset >> 2);
        *byteOffset += 4 + fCount * 8;
    }
}

void
SResource::preWrite(uint32_t *byteOffset) {
    if (fRes != RES_BOGUS) {
        /*
         * The resource item word was already precomputed, which means
         * no further data needs to be written.
         * This might be an integer, or an empty or UTF-16 v2 string,
         * an empty binary, etc.
         */
        return;
    }
    handlePreWrite(byteOffset);
    *byteOffset += calcPadding(*byteOffset);
}

void
SResource::handlePreWrite(uint32_t * /*byteOffset*/) {
    assert(FALSE);
}

/*
 * Only called for UTF-16 v1 strings, and for aliases. For UTF-16 v2 strings,
 * write() sees fWritten and exits early.
 */
void
StringBaseResource::handleWrite(UNewDataMemory *mem, uint32_t *byteOffset) {
    /* Write the UTF-16 v1 string. */
    int32_t len = length();
    udata_write32(mem, len);
    udata_writeUString(mem, getBuffer(), len + 1);
    *byteOffset += 4 + (len + 1) * U_SIZEOF_UCHAR;
    fWritten = TRUE;
}

void
ContainerResource::writeAllRes(UNewDataMemory *mem, uint32_t *byteOffset) {
    uint32_t i = 0;
    for (SResource *current = fFirst; current != NULL; ++i, current = current->fNext) {
        current->write(mem, byteOffset);
    }
    assert(i == fCount);
}

void
ContainerResource::writeAllRes32(UNewDataMemory *mem, uint32_t *byteOffset) {
    for (SResource *current = fFirst; current != NULL; current = current->fNext) {
        udata_write32(mem, current->fRes);
    }
    *byteOffset += fCount * 4;
}

void
ArrayResource::handleWrite(UNewDataMemory *mem, uint32_t *byteOffset) {
    writeAllRes(mem, byteOffset);
    udata_write32(mem, fCount);
    *byteOffset += 4;
    writeAllRes32(mem, byteOffset);
}

void
IntVectorResource::handleWrite(UNewDataMemory *mem, uint32_t *byteOffset) {
    udata_write32(mem, fCount);
    for(uint32_t i = 0; i < fCount; ++i) {
      udata_write32(mem, fArray[i]);
    }
    *byteOffset += (1 + fCount) * 4;
}

void
BinaryResource::handleWrite(UNewDataMemory *mem, uint32_t *byteOffset) {
    uint32_t pad       = 0;
    uint32_t dataStart = *byteOffset + sizeof(fLength);

    if (dataStart % BIN_ALIGNMENT) {
        pad = (BIN_ALIGNMENT - dataStart % BIN_ALIGNMENT);
        udata_writePadding(mem, pad);  /* pad == 4 or 8 or 12 */
        *byteOffset += pad;
    }

    udata_write32(mem, fLength);
    if (fLength > 0) {
        udata_writeBlock(mem, fData, fLength);
    }
    *byteOffset += 4 + fLength;
}

void
TableResource::handleWrite(UNewDataMemory *mem, uint32_t *byteOffset) {
    writeAllRes(mem, byteOffset);
    if(fTableType == URES_TABLE) {
        udata_write16(mem, (uint16_t)fCount);
        for (SResource *current = fFirst; current != NULL; current = current->fNext) {
            udata_write16(mem, current->fKey16);
        }
        *byteOffset += (1 + fCount)* 2;
        if ((fCount & 1) == 0) {
            /* 16-bit count and even number of 16-bit key offsets need padding before 32-bit resource items */
            udata_writePadding(mem, 2);
            *byteOffset += 2;
        }
    } else /* URES_TABLE32 */ {
        udata_write32(mem, fCount);
        for (SResource *current = fFirst; current != NULL; current = current->fNext) {
            udata_write32(mem, (uint32_t)current->fKey);
        }
        *byteOffset += (1 + fCount)* 4;
    }
    writeAllRes32(mem, byteOffset);
}

void
SResource::write(UNewDataMemory *mem, uint32_t *byteOffset) {
    if (fWritten) {
        assert(fRes != RES_BOGUS);
        return;
    }
    handleWrite(mem, byteOffset);
    uint8_t paddingSize = calcPadding(*byteOffset);
    if (paddingSize > 0) {
        udata_writePadding(mem, paddingSize);
        *byteOffset += paddingSize;
    }
    fWritten = TRUE;
}

void
SResource::handleWrite(UNewDataMemory * /*mem*/, uint32_t * /*byteOffset*/) {
    assert(FALSE);
}

void SRBRoot::write(const char *outputDir, const char *outputPkg,
                    char *writtenFilename, int writtenFilenameLen,
                    UErrorCode &errorCode) {
    UNewDataMemory *mem        = NULL;
    uint32_t        byteOffset = 0;
    uint32_t        top, size;
    char            dataName[1024];
    int32_t         indexes[URES_INDEX_TOP];

    compactKeys(errorCode);
    /*
     * Add padding bytes to fKeys so that fKeysTop is 4-aligned.
     * Safe because the capacity is a multiple of 4.
     */
    while (fKeysTop & 3) {
        fKeys[fKeysTop++] = (char)0xaa;
    }
    /*
     * In URES_TABLE, use all local key offsets that fit into 16 bits,
     * and use the remaining 16-bit offsets for pool key offsets
     * if there are any.
     * If there are no local keys, then use the whole 16-bit space
     * for pool key offsets.
     * Note: This cannot be changed without changing the major formatVersion.
     */
    if (fKeysBottom < fKeysTop) {
        if (fKeysTop <= 0x10000) {
            fLocalKeyLimit = fKeysTop;
        } else {
            fLocalKeyLimit = 0x10000;
        }
    } else {
        fLocalKeyLimit = 0;
    }

    UHashtable *stringSet;
    if (gFormatVersion > 1) {
        stringSet = uhash_open(string_hash, string_comp, string_comp, &errorCode);
        if (U_SUCCESS(errorCode) &&
                fUsePoolBundle != NULL && fUsePoolBundle->fStrings != NULL) {
            for (SResource *current = fUsePoolBundle->fStrings->fFirst;
                    current != NULL;
                    current = current->fNext) {
                StringResource *sr = static_cast<StringResource *>(current);
                sr->fNumCopies = 0;
                sr->fNumUnitsSaved = 0;
                uhash_put(stringSet, sr, sr, &errorCode);
            }
        }
        fRoot->preflightStrings(this, stringSet, errorCode);
    } else {
        stringSet = NULL;
    }
    if (fStringsForm == STRINGS_UTF16_V2 && f16BitStringsLength > 0) {
        compactStringsV2(stringSet, errorCode);
    }
    uhash_close(stringSet);
    if (U_FAILURE(errorCode)) {
        return;
    }

    int32_t formatVersion = gFormatVersion;
    if (fPoolStringIndexLimit != 0) {
        int32_t sum = fPoolStringIndexLimit + fLocalStringIndexLimit;
        if ((sum - 1) > RES_MAX_OFFSET) {
            errorCode = U_BUFFER_OVERFLOW_ERROR;
            return;
        }
        if (fPoolStringIndexLimit < 0x10000 && sum <= 0x10000) {
            // 16-bit indexes work for all pool + local strings.
            fPoolStringIndex16Limit = fPoolStringIndexLimit;
        } else {
            // Set the pool index threshold so that 16-bit indexes work
            // for some pool strings and some local strings.
            fPoolStringIndex16Limit = (int32_t)(
                    ((int64_t)fPoolStringIndexLimit * 0xffff) / sum);
        }
    } else if (gIsDefaultFormatVersion && formatVersion == 3 && !fIsPoolBundle) {
        // If we just default to formatVersion 3
        // but there are no pool bundle strings to share
        // and we do not write a pool bundle,
        // then write formatVersion 2 which is just as good.
        formatVersion = 2;
    }

    fRoot->write16(this);
    if (f16BitUnits.isBogus()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    if (f16BitUnits.length() & 1) {
        f16BitUnits.append((UChar)0xaaaa);  /* pad to multiple of 4 bytes */
    }
    /* all keys have been mapped */
    uprv_free(fKeyMap);
    fKeyMap = NULL;

    byteOffset = fKeysTop + f16BitUnits.length() * 2;
    fRoot->preWrite(&byteOffset);

    /* total size including the root item */
    top = byteOffset;

    if (writtenFilename && writtenFilenameLen) {
        *writtenFilename = 0;
    }

    if (writtenFilename) {
       int32_t off = 0, len = 0;
       if (outputDir) {
           len = (int32_t)uprv_strlen(outputDir);
           if (len > writtenFilenameLen) {
               len = writtenFilenameLen;
           }
           uprv_strncpy(writtenFilename, outputDir, len);
       }
       if (writtenFilenameLen -= len) {
           off += len;
           writtenFilename[off] = U_FILE_SEP_CHAR;
           if (--writtenFilenameLen) {
               ++off;
               if(outputPkg != NULL)
               {
                   uprv_strcpy(writtenFilename+off, outputPkg);
                   off += (int32_t)uprv_strlen(outputPkg);
                   writtenFilename[off] = '_';
                   ++off;
               }

               len = (int32_t)uprv_strlen(fLocale);
               if (len > writtenFilenameLen) {
                   len = writtenFilenameLen;
               }
               uprv_strncpy(writtenFilename + off, fLocale, len);
               if (writtenFilenameLen -= len) {
                   off += len;
                   len = 5;
                   if (len > writtenFilenameLen) {
                       len = writtenFilenameLen;
                   }
                   uprv_strncpy(writtenFilename +  off, ".res", len);
               }
           }
       }
    }

    if(outputPkg)
    {
        uprv_strcpy(dataName, outputPkg);
        uprv_strcat(dataName, "_");
        uprv_strcat(dataName, fLocale);
    }
    else
    {
        uprv_strcpy(dataName, fLocale);
    }

    uprv_memcpy(dataInfo.formatVersion, gFormatVersions + formatVersion, sizeof(UVersionInfo));

    mem = udata_create(outputDir, "res", dataName,
                       &dataInfo, (gIncludeCopyright==TRUE)? U_COPYRIGHT_STRING:NULL, &errorCode);
    if(U_FAILURE(errorCode)){
        return;
    }

    /* write the root item */
    udata_write32(mem, fRoot->fRes);

    /*
     * formatVersion 1.1 (ICU 2.8):
     * write int32_t indexes[] after root and before the key strings
     * to make it easier to parse resource bundles in icuswap or from Java etc.
     */
    uprv_memset(indexes, 0, sizeof(indexes));
    indexes[URES_INDEX_LENGTH]=             fIndexLength;
    indexes[URES_INDEX_KEYS_TOP]=           fKeysTop>>2;
    indexes[URES_INDEX_RESOURCES_TOP]=      (int32_t)(top>>2);
    indexes[URES_INDEX_BUNDLE_TOP]=         indexes[URES_INDEX_RESOURCES_TOP];
    indexes[URES_INDEX_MAX_TABLE_LENGTH]=   fMaxTableLength;

    /*
     * formatVersion 1.2 (ICU 3.6):
     * write indexes[URES_INDEX_ATTRIBUTES] with URES_ATT_NO_FALLBACK set or not set
     * the memset() above initialized all indexes[] to 0
     */
    if (fNoFallback) {
        indexes[URES_INDEX_ATTRIBUTES]=URES_ATT_NO_FALLBACK;
    }
    /*
     * formatVersion 2.0 (ICU 4.4):
     * more compact string value storage, optional pool bundle
     */
    if (URES_INDEX_16BIT_TOP < fIndexLength) {
        indexes[URES_INDEX_16BIT_TOP] = (fKeysTop>>2) + (f16BitUnits.length()>>1);
    }
    if (URES_INDEX_POOL_CHECKSUM < fIndexLength) {
        if (fIsPoolBundle) {
            indexes[URES_INDEX_ATTRIBUTES] |= URES_ATT_IS_POOL_BUNDLE | URES_ATT_NO_FALLBACK;
            uint32_t checksum = computeCRC((const char *)(fKeys + fKeysBottom),
                                           (uint32_t)(fKeysTop - fKeysBottom), 0);
            if (f16BitUnits.length() <= 1) {
                // no pool strings to checksum
            } else if (U_IS_BIG_ENDIAN) {
                checksum = computeCRC(reinterpret_cast<const char *>(f16BitUnits.getBuffer()),
                                      (uint32_t)f16BitUnits.length() * 2, checksum);
            } else {
                // Swap to big-endian so we get the same checksum on all platforms
                // (except for charset family, due to the key strings).
                UnicodeString s(f16BitUnits);
                s.append((UChar)1);  // Ensure that we own this buffer.
                assert(!s.isBogus());
                uint16_t *p = const_cast<uint16_t *>(reinterpret_cast<const uint16_t *>(s.getBuffer()));
                for (int32_t count = f16BitUnits.length(); count > 0; --count) {
                    uint16_t x = *p;
                    *p++ = (uint16_t)((x << 8) | (x >> 8));
                }
                checksum = computeCRC((const char *)p,
                                      (uint32_t)f16BitUnits.length() * 2, checksum);
            }
            indexes[URES_INDEX_POOL_CHECKSUM] = (int32_t)checksum;
        } else if (gUsePoolBundle) {
            indexes[URES_INDEX_ATTRIBUTES] |= URES_ATT_USES_POOL_BUNDLE;
            indexes[URES_INDEX_POOL_CHECKSUM] = fUsePoolBundle->fChecksum;
        }
    }
    // formatVersion 3 (ICU 56):
    // share string values via pool bundle strings
    indexes[URES_INDEX_LENGTH] |= fPoolStringIndexLimit << 8;  // bits 23..0 -> 31..8
    indexes[URES_INDEX_ATTRIBUTES] |= (fPoolStringIndexLimit >> 12) & 0xf000;  // bits 27..24 -> 15..12
    indexes[URES_INDEX_ATTRIBUTES] |= fPoolStringIndex16Limit << 16;

    /* write the indexes[] */
    udata_writeBlock(mem, indexes, fIndexLength*4);

    /* write the table key strings */
    udata_writeBlock(mem, fKeys+fKeysBottom,
                          fKeysTop-fKeysBottom);

    /* write the v2 UTF-16 strings, URES_TABLE16 and URES_ARRAY16 */
    udata_writeBlock(mem, f16BitUnits.getBuffer(), f16BitUnits.length()*2);

    /* write all of the bundle contents: the root item and its children */
    byteOffset = fKeysTop + f16BitUnits.length() * 2;
    fRoot->write(mem, &byteOffset);
    assert(byteOffset == top);

    size = udata_finish(mem, &errorCode);
    if(top != size) {
        fprintf(stderr, "genrb error: wrote %u bytes but counted %u\n",
                (int)size, (int)top);
        errorCode = U_INTERNAL_PROGRAM_ERROR;
    }
}

/* Opening Functions */

TableResource* table_open(struct SRBRoot *bundle, const char *tag, const struct UString* comment, UErrorCode *status) {
    LocalPointer<TableResource> res(new TableResource(bundle, tag, comment, *status), *status);
    return U_SUCCESS(*status) ? res.orphan() : NULL;
}

ArrayResource* array_open(struct SRBRoot *bundle, const char *tag, const struct UString* comment, UErrorCode *status) {
    LocalPointer<ArrayResource> res(new ArrayResource(bundle, tag, comment, *status), *status);
    return U_SUCCESS(*status) ? res.orphan() : NULL;
}

struct SResource *string_open(struct SRBRoot *bundle, const char *tag, const UChar *value, int32_t len, const struct UString* comment, UErrorCode *status) {
    LocalPointer<SResource> res(
            new StringResource(bundle, tag, value, len, comment, *status), *status);
    return U_SUCCESS(*status) ? res.orphan() : NULL;
}

struct SResource *alias_open(struct SRBRoot *bundle, const char *tag, UChar *value, int32_t len, const struct UString* comment, UErrorCode *status) {
    LocalPointer<SResource> res(
            new AliasResource(bundle, tag, value, len, comment, *status), *status);
    return U_SUCCESS(*status) ? res.orphan() : NULL;
}

IntVectorResource *intvector_open(struct SRBRoot *bundle, const char *tag, const struct UString* comment, UErrorCode *status) {
    LocalPointer<IntVectorResource> res(
            new IntVectorResource(bundle, tag, comment, *status), *status);
    return U_SUCCESS(*status) ? res.orphan() : NULL;
}

struct SResource *int_open(struct SRBRoot *bundle, const char *tag, int32_t value, const struct UString* comment, UErrorCode *status) {
    LocalPointer<SResource> res(new IntResource(bundle, tag, value, comment, *status), *status);
    return U_SUCCESS(*status) ? res.orphan() : NULL;
}

struct SResource *bin_open(struct SRBRoot *bundle, const char *tag, uint32_t length, uint8_t *data, const char* fileName, const struct UString* comment, UErrorCode *status) {
    LocalPointer<SResource> res(
            new BinaryResource(bundle, tag, length, data, fileName, comment, *status), *status);
    return U_SUCCESS(*status) ? res.orphan() : NULL;
}

SRBRoot::SRBRoot(const UString *comment, UBool isPoolBundle, UErrorCode &errorCode)
        : fRoot(NULL), fLocale(NULL), fIndexLength(0), fMaxTableLength(0), fNoFallback(FALSE),
          fStringsForm(STRINGS_UTF16_V1), fIsPoolBundle(isPoolBundle),
          fKeys(NULL), fKeyMap(NULL),
          fKeysBottom(0), fKeysTop(0), fKeysCapacity(0), fKeysCount(0), fLocalKeyLimit(0),
          f16BitUnits(), f16BitStringsLength(0),
          fUsePoolBundle(&kNoPoolBundle),
          fPoolStringIndexLimit(0), fPoolStringIndex16Limit(0), fLocalStringIndexLimit(0),
          fWritePoolBundle(NULL) {
    if (U_FAILURE(errorCode)) {
        return;
    }

    if (gFormatVersion > 1) {
        // f16BitUnits must start with a zero for empty resources.
        // We might be able to omit it if there are no empty 16-bit resources.
        f16BitUnits.append((UChar)0);
    }

    fKeys = (char *) uprv_malloc(sizeof(char) * KEY_SPACE_SIZE);
    if (isPoolBundle) {
        fRoot = new PseudoListResource(this, errorCode);
    } else {
        fRoot = new TableResource(this, NULL, comment, errorCode);
    }
    if (fKeys == NULL || fRoot == NULL || U_FAILURE(errorCode)) {
        if (U_SUCCESS(errorCode)) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
        }
        return;
    }

    fKeysCapacity = KEY_SPACE_SIZE;
    /* formatVersion 1.1 and up: start fKeysTop after the root item and indexes[] */
    if (gUsePoolBundle || isPoolBundle) {
        fIndexLength = URES_INDEX_POOL_CHECKSUM + 1;
    } else if (gFormatVersion >= 2) {
        fIndexLength = URES_INDEX_16BIT_TOP + 1;
    } else /* formatVersion 1 */ {
        fIndexLength = URES_INDEX_ATTRIBUTES + 1;
    }
    fKeysBottom = (1 /* root */ + fIndexLength) * 4;
    uprv_memset(fKeys, 0, fKeysBottom);
    fKeysTop = fKeysBottom;

    if (gFormatVersion == 1) {
        fStringsForm = STRINGS_UTF16_V1;
    } else {
        fStringsForm = STRINGS_UTF16_V2;
    }
}

/* Closing Functions */

void res_close(struct SResource *res) {
    delete res;
}

SRBRoot::~SRBRoot() {
    delete fRoot;
    uprv_free(fLocale);
    uprv_free(fKeys);
    uprv_free(fKeyMap);
}

/* Misc Functions */

void SRBRoot::setLocale(UChar *locale, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return;
    }

    uprv_free(fLocale);
    fLocale = (char*) uprv_malloc(sizeof(char) * (u_strlen(locale)+1));
    if(fLocale == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    u_UCharsToChars(locale, fLocale, u_strlen(locale)+1);
}

const char *
SRBRoot::getKeyString(int32_t key) const {
    if (key < 0) {
        return fUsePoolBundle->fKeys + (key & 0x7fffffff);
    } else {
        return fKeys + key;
    }
}

const char *
SResource::getKeyString(const SRBRoot *bundle) const {
    if (fKey == -1) {
        return NULL;
    }
    return bundle->getKeyString(fKey);
}

const char *
SRBRoot::getKeyBytes(int32_t *pLength) const {
    *pLength = fKeysTop - fKeysBottom;
    return fKeys + fKeysBottom;
}

int32_t
SRBRoot::addKeyBytes(const char *keyBytes, int32_t length, UErrorCode &errorCode) {
    int32_t keypos;

    if (U_FAILURE(errorCode)) {
        return -1;
    }
    if (length < 0 || (keyBytes == NULL && length != 0)) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return -1;
    }
    if (length == 0) {
        return fKeysTop;
    }

    keypos = fKeysTop;
    fKeysTop += length;
    if (fKeysTop >= fKeysCapacity) {
        /* overflow - resize the keys buffer */
        fKeysCapacity += KEY_SPACE_SIZE;
        fKeys = static_cast<char *>(uprv_realloc(fKeys, fKeysCapacity));
        if(fKeys == NULL) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return -1;
        }
    }

    uprv_memcpy(fKeys + keypos, keyBytes, length);

    return keypos;
}

int32_t
SRBRoot::addTag(const char *tag, UErrorCode &errorCode) {
    int32_t keypos;

    if (U_FAILURE(errorCode)) {
        return -1;
    }

    if (tag == NULL) {
        /* no error: the root table and array items have no keys */
        return -1;
    }

    keypos = addKeyBytes(tag, (int32_t)(uprv_strlen(tag) + 1), errorCode);
    if (U_SUCCESS(errorCode)) {
        ++fKeysCount;
    }
    return keypos;
}

static int32_t
compareInt32(int32_t lPos, int32_t rPos) {
    /*
     * Compare possibly-negative key offsets. Don't just return lPos - rPos
     * because that is prone to negative-integer underflows.
     */
    if (lPos < rPos) {
        return -1;
    } else if (lPos > rPos) {
        return 1;
    } else {
        return 0;
    }
}

static int32_t U_CALLCONV
compareKeySuffixes(const void *context, const void *l, const void *r) {
    const struct SRBRoot *bundle=(const struct SRBRoot *)context;
    int32_t lPos = ((const KeyMapEntry *)l)->oldpos;
    int32_t rPos = ((const KeyMapEntry *)r)->oldpos;
    const char *lStart = bundle->getKeyString(lPos);
    const char *lLimit = lStart;
    const char *rStart = bundle->getKeyString(rPos);
    const char *rLimit = rStart;
    int32_t diff;
    while (*lLimit != 0) { ++lLimit; }
    while (*rLimit != 0) { ++rLimit; }
    /* compare keys in reverse character order */
    while (lStart < lLimit && rStart < rLimit) {
        diff = (int32_t)(uint8_t)*--lLimit - (int32_t)(uint8_t)*--rLimit;
        if (diff != 0) {
            return diff;
        }
    }
    /* sort equal suffixes by descending key length */
    diff = (int32_t)(rLimit - rStart) - (int32_t)(lLimit - lStart);
    if (diff != 0) {
        return diff;
    }
    /* Sort pool bundle keys first (negative oldpos), and otherwise keys in parsing order. */
    return compareInt32(lPos, rPos);
}

static int32_t U_CALLCONV
compareKeyNewpos(const void * /*context*/, const void *l, const void *r) {
    return compareInt32(((const KeyMapEntry *)l)->newpos, ((const KeyMapEntry *)r)->newpos);
}

static int32_t U_CALLCONV
compareKeyOldpos(const void * /*context*/, const void *l, const void *r) {
    return compareInt32(((const KeyMapEntry *)l)->oldpos, ((const KeyMapEntry *)r)->oldpos);
}

void
SRBRoot::compactKeys(UErrorCode &errorCode) {
    KeyMapEntry *map;
    char *keys;
    int32_t i;
    int32_t keysCount = fUsePoolBundle->fKeysCount + fKeysCount;
    if (U_FAILURE(errorCode) || fKeysCount == 0 || fKeyMap != NULL) {
        return;
    }
    map = (KeyMapEntry *)uprv_malloc(keysCount * sizeof(KeyMapEntry));
    if (map == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    keys = (char *)fUsePoolBundle->fKeys;
    for (i = 0; i < fUsePoolBundle->fKeysCount; ++i) {
        map[i].oldpos =
            (int32_t)(keys - fUsePoolBundle->fKeys) | 0x80000000;  /* negative oldpos */
        map[i].newpos = 0;
        while (*keys != 0) { ++keys; }  /* skip the key */
        ++keys;  /* skip the NUL */
    }
    keys = fKeys + fKeysBottom;
    for (; i < keysCount; ++i) {
        map[i].oldpos = (int32_t)(keys - fKeys);
        map[i].newpos = 0;
        while (*keys != 0) { ++keys; }  /* skip the key */
        ++keys;  /* skip the NUL */
    }
    /* Sort the keys so that each one is immediately followed by all of its suffixes. */
    uprv_sortArray(map, keysCount, (int32_t)sizeof(KeyMapEntry),
                   compareKeySuffixes, this, FALSE, &errorCode);
    /*
     * Make suffixes point into earlier, longer strings that contain them
     * and mark the old, now unused suffix bytes as deleted.
     */
    if (U_SUCCESS(errorCode)) {
        keys = fKeys;
        for (i = 0; i < keysCount;) {
            /*
             * This key is not a suffix of the previous one;
             * keep this one and delete the following ones that are
             * suffixes of this one.
             */
            const char *key;
            const char *keyLimit;
            int32_t j = i + 1;
            map[i].newpos = map[i].oldpos;
            if (j < keysCount && map[j].oldpos < 0) {
                /* Key string from the pool bundle, do not delete. */
                i = j;
                continue;
            }
            key = getKeyString(map[i].oldpos);
            for (keyLimit = key; *keyLimit != 0; ++keyLimit) {}
            for (; j < keysCount && map[j].oldpos >= 0; ++j) {
                const char *k;
                char *suffix;
                const char *suffixLimit;
                int32_t offset;
                suffix = keys + map[j].oldpos;
                for (suffixLimit = suffix; *suffixLimit != 0; ++suffixLimit) {}
                offset = (int32_t)(keyLimit - key) - (suffixLimit - suffix);
                if (offset < 0) {
                    break;  /* suffix cannot be longer than the original */
                }
                /* Is it a suffix of the earlier, longer key? */
                for (k = keyLimit; suffix < suffixLimit && *--k == *--suffixLimit;) {}
                if (suffix == suffixLimit && *k == *suffixLimit) {
                    map[j].newpos = map[i].oldpos + offset;  /* yes, point to the earlier key */
                    /* mark the suffix as deleted */
                    while (*suffix != 0) { *suffix++ = 1; }
                    *suffix = 1;
                } else {
                    break;  /* not a suffix, restart from here */
                }
            }
            i = j;
        }
        /*
         * Re-sort by newpos, then modify the key characters array in-place
         * to squeeze out unused bytes, and readjust the newpos offsets.
         */
        uprv_sortArray(map, keysCount, (int32_t)sizeof(KeyMapEntry),
                       compareKeyNewpos, NULL, FALSE, &errorCode);
        if (U_SUCCESS(errorCode)) {
            int32_t oldpos, newpos, limit;
            oldpos = newpos = fKeysBottom;
            limit = fKeysTop;
            /* skip key offsets that point into the pool bundle rather than this new bundle */
            for (i = 0; i < keysCount && map[i].newpos < 0; ++i) {}
            if (i < keysCount) {
                while (oldpos < limit) {
                    if (keys[oldpos] == 1) {
                        ++oldpos;  /* skip unused bytes */
                    } else {
                        /* adjust the new offsets for keys starting here */
                        while (i < keysCount && map[i].newpos == oldpos) {
                            map[i++].newpos = newpos;
                        }
                        /* move the key characters to their new position */
                        keys[newpos++] = keys[oldpos++];
                    }
                }
                assert(i == keysCount);
            }
            fKeysTop = newpos;
            /* Re-sort once more, by old offsets for binary searching. */
            uprv_sortArray(map, keysCount, (int32_t)sizeof(KeyMapEntry),
                           compareKeyOldpos, NULL, FALSE, &errorCode);
            if (U_SUCCESS(errorCode)) {
                /* key size reduction by limit - newpos */
                fKeyMap = map;
                map = NULL;
            }
        }
    }
    uprv_free(map);
}

static int32_t U_CALLCONV
compareStringSuffixes(const void * /*context*/, const void *l, const void *r) {
    const StringResource *left = *((const StringResource **)l);
    const StringResource *right = *((const StringResource **)r);
    const UChar *lStart = left->getBuffer();
    const UChar *lLimit = lStart + left->length();
    const UChar *rStart = right->getBuffer();
    const UChar *rLimit = rStart + right->length();
    int32_t diff;
    /* compare keys in reverse character order */
    while (lStart < lLimit && rStart < rLimit) {
        diff = (int32_t)*--lLimit - (int32_t)*--rLimit;
        if (diff != 0) {
            return diff;
        }
    }
    /* sort equal suffixes by descending string length */
    return right->length() - left->length();
}

static int32_t U_CALLCONV
compareStringLengths(const void * /*context*/, const void *l, const void *r) {
    const StringResource *left = *((const StringResource **)l);
    const StringResource *right = *((const StringResource **)r);
    int32_t diff;
    /* Make "is suffix of another string" compare greater than a non-suffix. */
    diff = (int)(left->fSame != NULL) - (int)(right->fSame != NULL);
    if (diff != 0) {
        return diff;
    }
    /* sort by ascending string length */
    diff = left->length() - right->length();
    if (diff != 0) {
        return diff;
    }
    // sort by descending size reduction
    diff = right->fNumUnitsSaved - left->fNumUnitsSaved;
    if (diff != 0) {
        return diff;
    }
    // sort lexically
    return left->fString.compare(right->fString);
}

void
StringResource::writeUTF16v2(int32_t base, UnicodeString &dest) {
    int32_t len = length();
    fRes = URES_MAKE_RESOURCE(URES_STRING_V2, base + dest.length());
    fWritten = TRUE;
    switch(fNumCharsForLength) {
    case 0:
        break;
    case 1:
        dest.append((UChar)(0xdc00 + len));
        break;
    case 2:
        dest.append((UChar)(0xdfef + (len >> 16)));
        dest.append((UChar)len);
        break;
    case 3:
        dest.append((UChar)0xdfff);
        dest.append((UChar)(len >> 16));
        dest.append((UChar)len);
        break;
    default:
        break;  /* will not occur */
    }
    dest.append(fString);
    dest.append((UChar)0);
}

void
SRBRoot::compactStringsV2(UHashtable *stringSet, UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }
    // Store the StringResource pointers in an array for
    // easy sorting and processing.
    // We enumerate a set of strings, so there are no duplicates.
    int32_t count = uhash_count(stringSet);
    LocalArray<StringResource *> array(new StringResource *[count], errorCode);
    if (U_FAILURE(errorCode)) {
        return;
    }
    for (int32_t pos = UHASH_FIRST, i = 0; i < count; ++i) {
        array[i] = (StringResource *)uhash_nextElement(stringSet, &pos)->key.pointer;
    }
    /* Sort the strings so that each one is immediately followed by all of its suffixes. */
    uprv_sortArray(array.getAlias(), count, (int32_t)sizeof(struct SResource **),
                   compareStringSuffixes, NULL, FALSE, &errorCode);
    if (U_FAILURE(errorCode)) {
        return;
    }
    /*
     * Make suffixes point into earlier, longer strings that contain them.
     * Temporarily use fSame and fSuffixOffset for suffix strings to
     * refer to the remaining ones.
     */
    for (int32_t i = 0; i < count;) {
        /*
         * This string is not a suffix of the previous one;
         * write this one and subsume the following ones that are
         * suffixes of this one.
         */
        StringResource *res = array[i];
        res->fNumUnitsSaved = (res->fNumCopies - 1) * res->get16BitStringsLength();
        // Whole duplicates of pool strings are already account for in fPoolStringIndexLimit,
        // see StringResource::handlePreflightStrings().
        int32_t j;
        for (j = i + 1; j < count; ++j) {
            StringResource *suffixRes = array[j];
            /* Is it a suffix of the earlier, longer string? */
            if (res->fString.endsWith(suffixRes->fString)) {
                assert(res->length() != suffixRes->length());  // Set strings are unique.
                if (suffixRes->fWritten) {
                    // Pool string, skip.
                } else if (suffixRes->fNumCharsForLength == 0) {
                    /* yes, point to the earlier string */
                    suffixRes->fSame = res;
                    suffixRes->fSuffixOffset = res->length() - suffixRes->length();
                    if (res->fWritten) {
                        // Suffix-share res which is a pool string.
                        // Compute the resource word and collect the maximum.
                        suffixRes->fRes =
                                res->fRes + res->fNumCharsForLength + suffixRes->fSuffixOffset;
                        int32_t poolStringIndex = (int32_t)RES_GET_OFFSET(suffixRes->fRes);
                        if (poolStringIndex >= fPoolStringIndexLimit) {
                            fPoolStringIndexLimit = poolStringIndex + 1;
                        }
                        suffixRes->fWritten = TRUE;
                    }
                    res->fNumUnitsSaved += suffixRes->fNumCopies * suffixRes->get16BitStringsLength();
                } else {
                    /* write the suffix by itself if we need explicit length */
                }
            } else {
                break;  /* not a suffix, restart from here */
            }
        }
        i = j;
    }
    /*
     * Re-sort the strings by ascending length (except suffixes last)
     * to optimize for URES_TABLE16 and URES_ARRAY16:
     * Keep as many as possible within reach of 16-bit offsets.
     */
    uprv_sortArray(array.getAlias(), count, (int32_t)sizeof(struct SResource **),
                   compareStringLengths, NULL, FALSE, &errorCode);
    if (U_FAILURE(errorCode)) {
        return;
    }
    if (fIsPoolBundle) {
        // Write strings that are sufficiently shared.
        // Avoid writing other strings.
        int32_t numStringsWritten = 0;
        int32_t numUnitsSaved = 0;
        int32_t numUnitsNotSaved = 0;
        for (int32_t i = 0; i < count; ++i) {
            StringResource *res = array[i];
            // Maximum pool string index when suffix-sharing the last character.
            int32_t maxStringIndex =
                    f16BitUnits.length() + res->fNumCharsForLength + res->length() - 1;
            if (res->fNumUnitsSaved >= GENRB_MIN_16BIT_UNITS_SAVED_FOR_POOL_STRING &&
                    maxStringIndex < RES_MAX_OFFSET) {
                res->writeUTF16v2(0, f16BitUnits);
                ++numStringsWritten;
                numUnitsSaved += res->fNumUnitsSaved;
            } else {
                numUnitsNotSaved += res->fNumUnitsSaved;
                res->fRes = URES_MAKE_EMPTY_RESOURCE(URES_STRING);
                res->fWritten = TRUE;
            }
        }
        if (f16BitUnits.isBogus()) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
        }
        if (getShowWarning()) {  // not quiet
            printf("number of shared strings: %d\n", (int)numStringsWritten);
            printf("16-bit units for strings: %6d = %6d bytes\n",
                   (int)f16BitUnits.length(), (int)f16BitUnits.length() * 2);
            printf("16-bit units saved:       %6d = %6d bytes\n",
                   (int)numUnitsSaved, (int)numUnitsSaved * 2);
            printf("16-bit units not saved:   %6d = %6d bytes\n",
                   (int)numUnitsNotSaved, (int)numUnitsNotSaved * 2);
        }
    } else {
        assert(fPoolStringIndexLimit <= fUsePoolBundle->fStringIndexLimit);
        /* Write the non-suffix strings. */
        int32_t i;
        for (i = 0; i < count && array[i]->fSame == NULL; ++i) {
            StringResource *res = array[i];
            if (!res->fWritten) {
                int32_t localStringIndex = f16BitUnits.length();
                if (localStringIndex >= fLocalStringIndexLimit) {
                    fLocalStringIndexLimit = localStringIndex + 1;
                }
                res->writeUTF16v2(fPoolStringIndexLimit, f16BitUnits);
            }
        }
        if (f16BitUnits.isBogus()) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        if (fWritePoolBundle != NULL && gFormatVersion >= 3) {
            PseudoListResource *poolStrings =
                    static_cast<PseudoListResource *>(fWritePoolBundle->fRoot);
            for (i = 0; i < count && array[i]->fSame == NULL; ++i) {
                assert(!array[i]->fString.isEmpty());
                StringResource *poolString =
                        new StringResource(fWritePoolBundle, array[i]->fString, errorCode);
                if (poolString == NULL) {
                    errorCode = U_MEMORY_ALLOCATION_ERROR;
                    break;
                }
                poolStrings->add(poolString);
            }
        }
        /* Write the suffix strings. Make each point to the real string. */
        for (; i < count; ++i) {
            StringResource *res = array[i];
            if (res->fWritten) {
                continue;
            }
            StringResource *same = res->fSame;
            assert(res->length() != same->length());  // Set strings are unique.
            res->fRes = same->fRes + same->fNumCharsForLength + res->fSuffixOffset;
            int32_t localStringIndex = (int32_t)RES_GET_OFFSET(res->fRes) - fPoolStringIndexLimit;
            // Suffixes of pool strings have been set already.
            assert(localStringIndex >= 0);
            if (localStringIndex >= fLocalStringIndexLimit) {
                fLocalStringIndexLimit = localStringIndex + 1;
            }
            res->fWritten = TRUE;
        }
    }
    // +1 to account for the initial zero in f16BitUnits
    assert(f16BitUnits.length() <= (f16BitStringsLength + 1));
}
