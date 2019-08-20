// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2014-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/
#include <algorithm>

#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "unicode/uobject.h"

#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"
#include "uassert.h"
#include "ucln_cmn.h"
#include "uhash.h"
#include "umutex.h"
#include "uresimp.h"
#include "uvector.h"
#include "udataswp.h" /* for InvChar functions */

static UHashtable* gLocExtKeyMap = NULL;
static icu::UInitOnce gLocExtKeyMapInitOnce = U_INITONCE_INITIALIZER;

// bit flags for special types
typedef enum {
    SPECIALTYPE_NONE = 0,
    SPECIALTYPE_CODEPOINTS = 1,
    SPECIALTYPE_REORDER_CODE = 2,
    SPECIALTYPE_RG_KEY_VALUE = 4
} SpecialType;

struct LocExtKeyData : public icu::UMemory {
    const char*     legacyId;
    const char*     bcpId;
    icu::LocalUHashtablePointer typeMap;
    uint32_t        specialTypes;
};

struct LocExtType : public icu::UMemory {
    const char*     legacyId;
    const char*     bcpId;
};

static icu::MemoryPool<icu::CharString>* gKeyTypeStringPool = NULL;
static icu::MemoryPool<LocExtKeyData>* gLocExtKeyDataEntries = NULL;
static icu::MemoryPool<LocExtType>* gLocExtTypeEntries = NULL;

U_CDECL_BEGIN

static UBool U_CALLCONV
uloc_key_type_cleanup(void) {
    if (gLocExtKeyMap != NULL) {
        uhash_close(gLocExtKeyMap);
        gLocExtKeyMap = NULL;
    }

    delete gLocExtKeyDataEntries;
    gLocExtKeyDataEntries = NULL;

    delete gLocExtTypeEntries;
    gLocExtTypeEntries = NULL;

    delete gKeyTypeStringPool;
    gKeyTypeStringPool = NULL;

    gLocExtKeyMapInitOnce.reset();
    return TRUE;
}

U_CDECL_END


static void U_CALLCONV
initFromResourceBundle(UErrorCode& sts) {
    U_NAMESPACE_USE
    ucln_common_registerCleanup(UCLN_COMMON_LOCALE_KEY_TYPE, uloc_key_type_cleanup);

    gLocExtKeyMap = uhash_open(uhash_hashIChars, uhash_compareIChars, NULL, &sts);

    LocalUResourceBundlePointer keyTypeDataRes(ures_openDirect(NULL, "keyTypeData", &sts));
    LocalUResourceBundlePointer keyMapRes(ures_getByKey(keyTypeDataRes.getAlias(), "keyMap", NULL, &sts));
    LocalUResourceBundlePointer typeMapRes(ures_getByKey(keyTypeDataRes.getAlias(), "typeMap", NULL, &sts));

    if (U_FAILURE(sts)) {
        return;
    }

    UErrorCode tmpSts = U_ZERO_ERROR;
    LocalUResourceBundlePointer typeAliasRes(ures_getByKey(keyTypeDataRes.getAlias(), "typeAlias", NULL, &tmpSts));
    tmpSts = U_ZERO_ERROR;
    LocalUResourceBundlePointer bcpTypeAliasRes(ures_getByKey(keyTypeDataRes.getAlias(), "bcpTypeAlias", NULL, &tmpSts));

    // initialize pools storing dynamically allocated objects
    gKeyTypeStringPool = new icu::MemoryPool<icu::CharString>;
    if (gKeyTypeStringPool == NULL) {
        sts = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    gLocExtKeyDataEntries = new icu::MemoryPool<LocExtKeyData>;
    if (gLocExtKeyDataEntries == NULL) {
        sts = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    gLocExtTypeEntries = new icu::MemoryPool<LocExtType>;
    if (gLocExtTypeEntries == NULL) {
        sts = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    // iterate through keyMap resource
    LocalUResourceBundlePointer keyMapEntry;

    while (ures_hasNext(keyMapRes.getAlias())) {
        keyMapEntry.adoptInstead(ures_getNextResource(keyMapRes.getAlias(), keyMapEntry.orphan(), &sts));
        if (U_FAILURE(sts)) {
            break;
        }
        const char* legacyKeyId = ures_getKey(keyMapEntry.getAlias());
        UnicodeString uBcpKeyId = ures_getUnicodeString(keyMapEntry.getAlias(), &sts);
        if (U_FAILURE(sts)) {
            break;
        }

        // empty value indicates that BCP key is same with the legacy key.
        const char* bcpKeyId = legacyKeyId;
        if (!uBcpKeyId.isEmpty()) {
            icu::CharString* bcpKeyIdBuf = gKeyTypeStringPool->create();
            if (bcpKeyIdBuf == NULL) {
                sts = U_MEMORY_ALLOCATION_ERROR;
                break;
            }
            bcpKeyIdBuf->appendInvariantChars(uBcpKeyId, sts);
            if (U_FAILURE(sts)) {
                break;
            }
            bcpKeyId = bcpKeyIdBuf->data();
        }

        UBool isTZ = uprv_strcmp(legacyKeyId, "timezone") == 0;

        UHashtable* typeDataMap = uhash_open(uhash_hashIChars, uhash_compareIChars, NULL, &sts);
        if (U_FAILURE(sts)) {
            break;
        }
        uint32_t specialTypes = SPECIALTYPE_NONE;

        LocalUResourceBundlePointer typeAliasResByKey;
        LocalUResourceBundlePointer bcpTypeAliasResByKey;

        if (typeAliasRes.isValid()) {
            tmpSts = U_ZERO_ERROR;
            typeAliasResByKey.adoptInstead(ures_getByKey(typeAliasRes.getAlias(), legacyKeyId, NULL, &tmpSts));
            if (U_FAILURE(tmpSts)) {
                typeAliasResByKey.orphan();
            }
        }
        if (bcpTypeAliasRes.isValid()) {
            tmpSts = U_ZERO_ERROR;
            bcpTypeAliasResByKey.adoptInstead(ures_getByKey(bcpTypeAliasRes.getAlias(), bcpKeyId, NULL, &tmpSts));
            if (U_FAILURE(tmpSts)) {
                bcpTypeAliasResByKey.orphan();
            }
        }

        // look up type map for the key, and walk through the mapping data
        tmpSts = U_ZERO_ERROR;
        LocalUResourceBundlePointer typeMapResByKey(ures_getByKey(typeMapRes.getAlias(), legacyKeyId, NULL, &tmpSts));
        if (U_FAILURE(tmpSts)) {
            // type map for each key must exist
            UPRV_UNREACHABLE;
        } else {
            LocalUResourceBundlePointer typeMapEntry;

            while (ures_hasNext(typeMapResByKey.getAlias())) {
                typeMapEntry.adoptInstead(ures_getNextResource(typeMapResByKey.getAlias(), typeMapEntry.orphan(), &sts));
                if (U_FAILURE(sts)) {
                    break;
                }
                const char* legacyTypeId = ures_getKey(typeMapEntry.getAlias());

                // special types
                if (uprv_strcmp(legacyTypeId, "CODEPOINTS") == 0) {
                    specialTypes |= SPECIALTYPE_CODEPOINTS;
                    continue;
                }
                if (uprv_strcmp(legacyTypeId, "REORDER_CODE") == 0) {
                    specialTypes |= SPECIALTYPE_REORDER_CODE;
                    continue;
                }
                if (uprv_strcmp(legacyTypeId, "RG_KEY_VALUE") == 0) {
                    specialTypes |= SPECIALTYPE_RG_KEY_VALUE;
                    continue;
                }

                if (isTZ) {
                    // a timezone key uses a colon instead of a slash in the resource.
                    // e.g. America:Los_Angeles
                    if (uprv_strchr(legacyTypeId, ':') != NULL) {
                        icu::CharString* legacyTypeIdBuf =
                                gKeyTypeStringPool->create(legacyTypeId, sts);
                        if (legacyTypeIdBuf == NULL) {
                            sts = U_MEMORY_ALLOCATION_ERROR;
                            break;
                        }
                        if (U_FAILURE(sts)) {
                            break;
                        }
                        std::replace(
                                legacyTypeIdBuf->data(),
                                legacyTypeIdBuf->data() + legacyTypeIdBuf->length(),
                                ':', '/');
                        legacyTypeId = legacyTypeIdBuf->data();
                    }
                }

                UnicodeString uBcpTypeId = ures_getUnicodeString(typeMapEntry.getAlias(), &sts);
                if (U_FAILURE(sts)) {
                    break;
                }

                // empty value indicates that BCP type is same with the legacy type.
                const char* bcpTypeId = legacyTypeId;
                if (!uBcpTypeId.isEmpty()) {
                    icu::CharString* bcpTypeIdBuf = gKeyTypeStringPool->create();
                    if (bcpTypeIdBuf == NULL) {
                        sts = U_MEMORY_ALLOCATION_ERROR;
                        break;
                    }
                    bcpTypeIdBuf->appendInvariantChars(uBcpTypeId, sts);
                    if (U_FAILURE(sts)) {
                        break;
                    }
                    bcpTypeId = bcpTypeIdBuf->data();
                }

                // Note: legacy type value should never be
                // equivalent to bcp type value of a different
                // type under the same key. So we use a single
                // map for lookup.
                LocExtType* t = gLocExtTypeEntries->create();
                if (t == NULL) {
                    sts = U_MEMORY_ALLOCATION_ERROR;
                    break;
                }
                t->bcpId = bcpTypeId;
                t->legacyId = legacyTypeId;

                uhash_put(typeDataMap, (void*)legacyTypeId, t, &sts);
                if (bcpTypeId != legacyTypeId) {
                    // different type value
                    uhash_put(typeDataMap, (void*)bcpTypeId, t, &sts);
                }
                if (U_FAILURE(sts)) {
                    break;
                }

                // also put aliases in the map
                if (typeAliasResByKey.isValid()) {
                    LocalUResourceBundlePointer typeAliasDataEntry;

                    ures_resetIterator(typeAliasResByKey.getAlias());
                    while (ures_hasNext(typeAliasResByKey.getAlias()) && U_SUCCESS(sts)) {
                        int32_t toLen;
                        typeAliasDataEntry.adoptInstead(ures_getNextResource(typeAliasResByKey.getAlias(), typeAliasDataEntry.orphan(), &sts));
                        const UChar* to = ures_getString(typeAliasDataEntry.getAlias(), &toLen, &sts);
                        if (U_FAILURE(sts)) {
                            break;
                        }
                        // check if this is an alias of canoncal legacy type
                        if (uprv_compareInvWithUChar(NULL, legacyTypeId, -1, to, toLen) == 0) {
                            const char* from = ures_getKey(typeAliasDataEntry.getAlias());
                            if (isTZ) {
                                // replace colon with slash if necessary
                                if (uprv_strchr(from, ':') != NULL) {
                                    icu::CharString* fromBuf =
                                            gKeyTypeStringPool->create(from, sts);
                                    if (fromBuf == NULL) {
                                        sts = U_MEMORY_ALLOCATION_ERROR;
                                        break;
                                    }
                                    if (U_FAILURE(sts)) {
                                        break;
                                    }
                                    std::replace(
                                            fromBuf->data(),
                                            fromBuf->data() + fromBuf->length(),
                                            ':', '/');
                                    from = fromBuf->data();
                                }
                            }
                            uhash_put(typeDataMap, (void*)from, t, &sts);
                        }
                    }
                    if (U_FAILURE(sts)) {
                        break;
                    }
                }

                if (bcpTypeAliasResByKey.isValid()) {
                    LocalUResourceBundlePointer bcpTypeAliasDataEntry;

                    ures_resetIterator(bcpTypeAliasResByKey.getAlias());
                    while (ures_hasNext(bcpTypeAliasResByKey.getAlias()) && U_SUCCESS(sts)) {
                        int32_t toLen;
                        bcpTypeAliasDataEntry.adoptInstead(ures_getNextResource(bcpTypeAliasResByKey.getAlias(), bcpTypeAliasDataEntry.orphan(), &sts));
                        const UChar* to = ures_getString(bcpTypeAliasDataEntry.getAlias(), &toLen, &sts);
                        if (U_FAILURE(sts)) {
                            break;
                        }
                        // check if this is an alias of bcp type
                        if (uprv_compareInvWithUChar(NULL, bcpTypeId, -1, to, toLen) == 0) {
                            const char* from = ures_getKey(bcpTypeAliasDataEntry.getAlias());
                            uhash_put(typeDataMap, (void*)from, t, &sts);
                        }
                    }
                    if (U_FAILURE(sts)) {
                        break;
                    }
                }
            }
        }
        if (U_FAILURE(sts)) {
            break;
        }

        LocExtKeyData* keyData = gLocExtKeyDataEntries->create();
        if (keyData == NULL) {
            sts = U_MEMORY_ALLOCATION_ERROR;
            break;
        }
        keyData->bcpId = bcpKeyId;
        keyData->legacyId = legacyKeyId;
        keyData->specialTypes = specialTypes;
        keyData->typeMap.adoptInstead(typeDataMap);

        uhash_put(gLocExtKeyMap, (void*)legacyKeyId, keyData, &sts);
        if (legacyKeyId != bcpKeyId) {
            // different key value
            uhash_put(gLocExtKeyMap, (void*)bcpKeyId, keyData, &sts);
        }
        if (U_FAILURE(sts)) {
            break;
        }
    }
}

static UBool
init() {
    UErrorCode sts = U_ZERO_ERROR;
    umtx_initOnce(gLocExtKeyMapInitOnce, &initFromResourceBundle, sts);
    if (U_FAILURE(sts)) {
        return FALSE;
    }
    return TRUE;
}

static UBool
isSpecialTypeCodepoints(const char* val) {
    int32_t subtagLen = 0;
    const char* p = val;
    while (*p) {
        if (*p == '-') {
            if (subtagLen < 4 || subtagLen > 6) {
                return FALSE;
            }
            subtagLen = 0;
        } else if ((*p >= '0' && *p <= '9') ||
                    (*p >= 'A' && *p <= 'F') || // A-F/a-f are contiguous
                    (*p >= 'a' && *p <= 'f')) { // also in EBCDIC
            subtagLen++;
        } else {
            return FALSE;
        }
        p++;
    }
    return (subtagLen >= 4 && subtagLen <= 6);
}

static UBool
isSpecialTypeReorderCode(const char* val) {
    int32_t subtagLen = 0;
    const char* p = val;
    while (*p) {
        if (*p == '-') {
            if (subtagLen < 3 || subtagLen > 8) {
                return FALSE;
            }
            subtagLen = 0;
        } else if (uprv_isASCIILetter(*p)) {
            subtagLen++;
        } else {
            return FALSE;
        }
        p++;
    }
    return (subtagLen >=3 && subtagLen <=8);
}

static UBool
isSpecialTypeRgKeyValue(const char* val) {
    int32_t subtagLen = 0;
    const char* p = val;
    while (*p) {
        if ( (subtagLen < 2 && uprv_isASCIILetter(*p)) ||
                    (subtagLen >= 2 && (*p == 'Z' || *p == 'z')) ) {
            subtagLen++;
        } else {
            return FALSE;
        }
        p++;
    }
    return (subtagLen == 6);
}

U_CFUNC const char*
ulocimp_toBcpKey(const char* key) {
    if (!init()) {
        return NULL;
    }

    LocExtKeyData* keyData = (LocExtKeyData*)uhash_get(gLocExtKeyMap, key);
    if (keyData != NULL) {
        return keyData->bcpId;
    }
    return NULL;
}

U_CFUNC const char*
ulocimp_toLegacyKey(const char* key) {
    if (!init()) {
        return NULL;
    }

    LocExtKeyData* keyData = (LocExtKeyData*)uhash_get(gLocExtKeyMap, key);
    if (keyData != NULL) {
        return keyData->legacyId;
    }
    return NULL;
}

U_CFUNC const char*
ulocimp_toBcpType(const char* key, const char* type, UBool* isKnownKey, UBool* isSpecialType) {
    if (isKnownKey != NULL) {
        *isKnownKey = FALSE;
    }
    if (isSpecialType != NULL) {
        *isSpecialType = FALSE;
    }

    if (!init()) {
        return NULL;
    }

    LocExtKeyData* keyData = (LocExtKeyData*)uhash_get(gLocExtKeyMap, key);
    if (keyData != NULL) {
        if (isKnownKey != NULL) {
            *isKnownKey = TRUE;
        }
        LocExtType* t = (LocExtType*)uhash_get(keyData->typeMap.getAlias(), type);
        if (t != NULL) {
            return t->bcpId;
        }
        if (keyData->specialTypes != SPECIALTYPE_NONE) {
            UBool matched = FALSE;
            if (keyData->specialTypes & SPECIALTYPE_CODEPOINTS) {
                matched = isSpecialTypeCodepoints(type);
            }
            if (!matched && keyData->specialTypes & SPECIALTYPE_REORDER_CODE) {
                matched = isSpecialTypeReorderCode(type);
            }
            if (!matched && keyData->specialTypes & SPECIALTYPE_RG_KEY_VALUE) {
                matched = isSpecialTypeRgKeyValue(type);
            }
            if (matched) {
                if (isSpecialType != NULL) {
                    *isSpecialType = TRUE;
                }
                return type;
            }
        }
    }
    return NULL;
}


U_CFUNC const char*
ulocimp_toLegacyType(const char* key, const char* type, UBool* isKnownKey, UBool* isSpecialType) {
    if (isKnownKey != NULL) {
        *isKnownKey = FALSE;
    }
    if (isSpecialType != NULL) {
        *isSpecialType = FALSE;
    }

    if (!init()) {
        return NULL;
    }

    LocExtKeyData* keyData = (LocExtKeyData*)uhash_get(gLocExtKeyMap, key);
    if (keyData != NULL) {
        if (isKnownKey != NULL) {
            *isKnownKey = TRUE;
        }
        LocExtType* t = (LocExtType*)uhash_get(keyData->typeMap.getAlias(), type);
        if (t != NULL) {
            return t->legacyId;
        }
        if (keyData->specialTypes != SPECIALTYPE_NONE) {
            UBool matched = FALSE;
            if (keyData->specialTypes & SPECIALTYPE_CODEPOINTS) {
                matched = isSpecialTypeCodepoints(type);
            }
            if (!matched && keyData->specialTypes & SPECIALTYPE_REORDER_CODE) {
                matched = isSpecialTypeReorderCode(type);
            }
            if (!matched && keyData->specialTypes & SPECIALTYPE_RG_KEY_VALUE) {
                matched = isSpecialTypeRgKeyValue(type);
            }
            if (matched) {
                if (isSpecialType != NULL) {
                    *isSpecialType = TRUE;
                }
                return type;
            }
        }
    }
    return NULL;
}
