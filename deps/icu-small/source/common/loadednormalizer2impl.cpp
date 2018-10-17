// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* loadednormalizer2impl.cpp
*
* created on: 2014sep03
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/udata.h"
#include "unicode/localpointer.h"
#include "unicode/normalizer2.h"
#include "unicode/ucptrie.h"
#include "unicode/unistr.h"
#include "unicode/unorm.h"
#include "cstring.h"
#include "mutex.h"
#include "norm2allmodes.h"
#include "normalizer2impl.h"
#include "uassert.h"
#include "ucln_cmn.h"
#include "uhash.h"

U_NAMESPACE_BEGIN

class LoadedNormalizer2Impl : public Normalizer2Impl {
public:
    LoadedNormalizer2Impl() : memory(NULL), ownedTrie(NULL) {}
    virtual ~LoadedNormalizer2Impl();

    void load(const char *packageName, const char *name, UErrorCode &errorCode);

private:
    static UBool U_CALLCONV
    isAcceptable(void *context, const char *type, const char *name, const UDataInfo *pInfo);

    UDataMemory *memory;
    UCPTrie *ownedTrie;
};

LoadedNormalizer2Impl::~LoadedNormalizer2Impl() {
    udata_close(memory);
    ucptrie_close(ownedTrie);
}

UBool U_CALLCONV
LoadedNormalizer2Impl::isAcceptable(void * /*context*/,
                                    const char * /* type */, const char * /*name*/,
                                    const UDataInfo *pInfo) {
    if(
        pInfo->size>=20 &&
        pInfo->isBigEndian==U_IS_BIG_ENDIAN &&
        pInfo->charsetFamily==U_CHARSET_FAMILY &&
        pInfo->dataFormat[0]==0x4e &&    /* dataFormat="Nrm2" */
        pInfo->dataFormat[1]==0x72 &&
        pInfo->dataFormat[2]==0x6d &&
        pInfo->dataFormat[3]==0x32 &&
        pInfo->formatVersion[0]==4
    ) {
        // Normalizer2Impl *me=(Normalizer2Impl *)context;
        // uprv_memcpy(me->dataVersion, pInfo->dataVersion, 4);
        return TRUE;
    } else {
        return FALSE;
    }
}

void
LoadedNormalizer2Impl::load(const char *packageName, const char *name, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return;
    }
    memory=udata_openChoice(packageName, "nrm", name, isAcceptable, this, &errorCode);
    if(U_FAILURE(errorCode)) {
        return;
    }
    const uint8_t *inBytes=(const uint8_t *)udata_getMemory(memory);
    const int32_t *inIndexes=(const int32_t *)inBytes;
    int32_t indexesLength=inIndexes[IX_NORM_TRIE_OFFSET]/4;
    if(indexesLength<=IX_MIN_LCCC_CP) {
        errorCode=U_INVALID_FORMAT_ERROR;  // Not enough indexes.
        return;
    }

    int32_t offset=inIndexes[IX_NORM_TRIE_OFFSET];
    int32_t nextOffset=inIndexes[IX_EXTRA_DATA_OFFSET];
    ownedTrie=ucptrie_openFromBinary(UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_16,
                                     inBytes+offset, nextOffset-offset, NULL,
                                     &errorCode);
    if(U_FAILURE(errorCode)) {
        return;
    }

    offset=nextOffset;
    nextOffset=inIndexes[IX_SMALL_FCD_OFFSET];
    const uint16_t *inExtraData=(const uint16_t *)(inBytes+offset);

    // smallFCD: new in formatVersion 2
    offset=nextOffset;
    const uint8_t *inSmallFCD=inBytes+offset;

    init(inIndexes, ownedTrie, inExtraData, inSmallFCD);
}

// instance cache ---------------------------------------------------------- ***

Norm2AllModes *
Norm2AllModes::createInstance(const char *packageName,
                              const char *name,
                              UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return NULL;
    }
    LoadedNormalizer2Impl *impl=new LoadedNormalizer2Impl;
    if(impl==NULL) {
        errorCode=U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    impl->load(packageName, name, errorCode);
    return createInstance(impl, errorCode);
}

U_CDECL_BEGIN
static UBool U_CALLCONV uprv_loaded_normalizer2_cleanup();
U_CDECL_END

#if !NORM2_HARDCODE_NFC_DATA
static Norm2AllModes *nfcSingleton;
static icu::UInitOnce nfcInitOnce = U_INITONCE_INITIALIZER;
#endif

static Norm2AllModes *nfkcSingleton;
static icu::UInitOnce nfkcInitOnce = U_INITONCE_INITIALIZER;

static Norm2AllModes *nfkc_cfSingleton;
static icu::UInitOnce nfkc_cfInitOnce = U_INITONCE_INITIALIZER;

static UHashtable    *cache=NULL;

// UInitOnce singleton initialization function
static void U_CALLCONV initSingletons(const char *what, UErrorCode &errorCode) {
#if !NORM2_HARDCODE_NFC_DATA
    if (uprv_strcmp(what, "nfc") == 0) {
        nfcSingleton    = Norm2AllModes::createInstance(NULL, "nfc", errorCode);
    } else
#endif
    if (uprv_strcmp(what, "nfkc") == 0) {
        nfkcSingleton    = Norm2AllModes::createInstance(NULL, "nfkc", errorCode);
    } else if (uprv_strcmp(what, "nfkc_cf") == 0) {
        nfkc_cfSingleton = Norm2AllModes::createInstance(NULL, "nfkc_cf", errorCode);
    } else {
        U_ASSERT(FALSE);   // Unknown singleton
    }
    ucln_common_registerCleanup(UCLN_COMMON_LOADED_NORMALIZER2, uprv_loaded_normalizer2_cleanup);
}

U_CDECL_BEGIN

static void U_CALLCONV deleteNorm2AllModes(void *allModes) {
    delete (Norm2AllModes *)allModes;
}

static UBool U_CALLCONV uprv_loaded_normalizer2_cleanup() {
#if !NORM2_HARDCODE_NFC_DATA
    delete nfcSingleton;
    nfcSingleton = NULL;
    nfcInitOnce.reset();
#endif

    delete nfkcSingleton;
    nfkcSingleton = NULL;
    nfkcInitOnce.reset();

    delete nfkc_cfSingleton;
    nfkc_cfSingleton = NULL;
    nfkc_cfInitOnce.reset();

    uhash_close(cache);
    cache=NULL;
    return TRUE;
}

U_CDECL_END

#if !NORM2_HARDCODE_NFC_DATA
const Norm2AllModes *
Norm2AllModes::getNFCInstance(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    umtx_initOnce(nfcInitOnce, &initSingletons, "nfc", errorCode);
    return nfcSingleton;
}
#endif

const Norm2AllModes *
Norm2AllModes::getNFKCInstance(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    umtx_initOnce(nfkcInitOnce, &initSingletons, "nfkc", errorCode);
    return nfkcSingleton;
}

const Norm2AllModes *
Norm2AllModes::getNFKC_CFInstance(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    umtx_initOnce(nfkc_cfInitOnce, &initSingletons, "nfkc_cf", errorCode);
    return nfkc_cfSingleton;
}

#if !NORM2_HARDCODE_NFC_DATA
const Normalizer2 *
Normalizer2::getNFCInstance(UErrorCode &errorCode) {
    const Norm2AllModes *allModes=Norm2AllModes::getNFCInstance(errorCode);
    return allModes!=NULL ? &allModes->comp : NULL;
}

const Normalizer2 *
Normalizer2::getNFDInstance(UErrorCode &errorCode) {
    const Norm2AllModes *allModes=Norm2AllModes::getNFCInstance(errorCode);
    return allModes!=NULL ? &allModes->decomp : NULL;
}

const Normalizer2 *Normalizer2Factory::getFCDInstance(UErrorCode &errorCode) {
    const Norm2AllModes *allModes=Norm2AllModes::getNFCInstance(errorCode);
    return allModes!=NULL ? &allModes->fcd : NULL;
}

const Normalizer2 *Normalizer2Factory::getFCCInstance(UErrorCode &errorCode) {
    const Norm2AllModes *allModes=Norm2AllModes::getNFCInstance(errorCode);
    return allModes!=NULL ? &allModes->fcc : NULL;
}

const Normalizer2Impl *
Normalizer2Factory::getNFCImpl(UErrorCode &errorCode) {
    const Norm2AllModes *allModes=Norm2AllModes::getNFCInstance(errorCode);
    return allModes!=NULL ? allModes->impl : NULL;
}
#endif

const Normalizer2 *
Normalizer2::getNFKCInstance(UErrorCode &errorCode) {
    const Norm2AllModes *allModes=Norm2AllModes::getNFKCInstance(errorCode);
    return allModes!=NULL ? &allModes->comp : NULL;
}

const Normalizer2 *
Normalizer2::getNFKDInstance(UErrorCode &errorCode) {
    const Norm2AllModes *allModes=Norm2AllModes::getNFKCInstance(errorCode);
    return allModes!=NULL ? &allModes->decomp : NULL;
}

const Normalizer2 *
Normalizer2::getNFKCCasefoldInstance(UErrorCode &errorCode) {
    const Norm2AllModes *allModes=Norm2AllModes::getNFKC_CFInstance(errorCode);
    return allModes!=NULL ? &allModes->comp : NULL;
}

const Normalizer2 *
Normalizer2::getInstance(const char *packageName,
                         const char *name,
                         UNormalization2Mode mode,
                         UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return NULL;
    }
    if(name==NULL || *name==0) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    const Norm2AllModes *allModes=NULL;
    if(packageName==NULL) {
        if(0==uprv_strcmp(name, "nfc")) {
            allModes=Norm2AllModes::getNFCInstance(errorCode);
        } else if(0==uprv_strcmp(name, "nfkc")) {
            allModes=Norm2AllModes::getNFKCInstance(errorCode);
        } else if(0==uprv_strcmp(name, "nfkc_cf")) {
            allModes=Norm2AllModes::getNFKC_CFInstance(errorCode);
        }
    }
    if(allModes==NULL && U_SUCCESS(errorCode)) {
        {
            Mutex lock;
            if(cache!=NULL) {
                allModes=(Norm2AllModes *)uhash_get(cache, name);
            }
        }
        if(allModes==NULL) {
            ucln_common_registerCleanup(UCLN_COMMON_LOADED_NORMALIZER2, uprv_loaded_normalizer2_cleanup);
            LocalPointer<Norm2AllModes> localAllModes(
                Norm2AllModes::createInstance(packageName, name, errorCode));
            if(U_SUCCESS(errorCode)) {
                Mutex lock;
                if(cache==NULL) {
                    cache=uhash_open(uhash_hashChars, uhash_compareChars, NULL, &errorCode);
                    if(U_FAILURE(errorCode)) {
                        return NULL;
                    }
                    uhash_setKeyDeleter(cache, uprv_free);
                    uhash_setValueDeleter(cache, deleteNorm2AllModes);
                }
                void *temp=uhash_get(cache, name);
                if(temp==NULL) {
                    int32_t keyLength= static_cast<int32_t>(uprv_strlen(name)+1);
                    char *nameCopy=(char *)uprv_malloc(keyLength);
                    if(nameCopy==NULL) {
                        errorCode=U_MEMORY_ALLOCATION_ERROR;
                        return NULL;
                    }
                    uprv_memcpy(nameCopy, name, keyLength);
                    allModes=localAllModes.getAlias();
                    uhash_put(cache, nameCopy, localAllModes.orphan(), &errorCode);
                } else {
                    // race condition
                    allModes=(Norm2AllModes *)temp;
                }
            }
        }
    }
    if(allModes!=NULL && U_SUCCESS(errorCode)) {
        switch(mode) {
        case UNORM2_COMPOSE:
            return &allModes->comp;
        case UNORM2_DECOMPOSE:
            return &allModes->decomp;
        case UNORM2_FCD:
            return &allModes->fcd;
        case UNORM2_COMPOSE_CONTIGUOUS:
            return &allModes->fcc;
        default:
            break;  // do nothing
        }
    }
    return NULL;
}

const Normalizer2 *
Normalizer2Factory::getInstance(UNormalizationMode mode, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return NULL;
    }
    switch(mode) {
    case UNORM_NFD:
        return Normalizer2::getNFDInstance(errorCode);
    case UNORM_NFKD:
        return Normalizer2::getNFKDInstance(errorCode);
    case UNORM_NFC:
        return Normalizer2::getNFCInstance(errorCode);
    case UNORM_NFKC:
        return Normalizer2::getNFKCInstance(errorCode);
    case UNORM_FCD:
        return getFCDInstance(errorCode);
    default:  // UNORM_NONE
        return getNoopInstance(errorCode);
    }
}

const Normalizer2Impl *
Normalizer2Factory::getNFKCImpl(UErrorCode &errorCode) {
    const Norm2AllModes *allModes=Norm2AllModes::getNFKCInstance(errorCode);
    return allModes!=NULL ? allModes->impl : NULL;
}

const Normalizer2Impl *
Normalizer2Factory::getNFKC_CFImpl(UErrorCode &errorCode) {
    const Norm2AllModes *allModes=Norm2AllModes::getNFKC_CFInstance(errorCode);
    return allModes!=NULL ? allModes->impl : NULL;
}

U_NAMESPACE_END

// C API ------------------------------------------------------------------- ***

U_NAMESPACE_USE

U_CAPI const UNormalizer2 * U_EXPORT2
unorm2_getNFKCInstance(UErrorCode *pErrorCode) {
    return (const UNormalizer2 *)Normalizer2::getNFKCInstance(*pErrorCode);
}

U_CAPI const UNormalizer2 * U_EXPORT2
unorm2_getNFKDInstance(UErrorCode *pErrorCode) {
    return (const UNormalizer2 *)Normalizer2::getNFKDInstance(*pErrorCode);
}

U_CAPI const UNormalizer2 * U_EXPORT2
unorm2_getNFKCCasefoldInstance(UErrorCode *pErrorCode) {
    return (const UNormalizer2 *)Normalizer2::getNFKCCasefoldInstance(*pErrorCode);
}

U_CAPI const UNormalizer2 * U_EXPORT2
unorm2_getInstance(const char *packageName,
                   const char *name,
                   UNormalization2Mode mode,
                   UErrorCode *pErrorCode) {
    return (const UNormalizer2 *)Normalizer2::getInstance(packageName, name, mode, *pErrorCode);
}

U_CFUNC UNormalizationCheckResult
unorm_getQuickCheck(UChar32 c, UNormalizationMode mode) {
    if(mode<=UNORM_NONE || UNORM_FCD<=mode) {
        return UNORM_YES;
    }
    UErrorCode errorCode=U_ZERO_ERROR;
    const Normalizer2 *norm2=Normalizer2Factory::getInstance(mode, errorCode);
    if(U_SUCCESS(errorCode)) {
        return ((const Normalizer2WithImpl *)norm2)->getQuickCheck(c);
    } else {
        return UNORM_MAYBE;
    }
}

#endif  // !UCONFIG_NO_NORMALIZATION
