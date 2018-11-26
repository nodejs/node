// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 1996-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  ucol_res.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
* Description:
* This file contains dependencies that the collation run-time doesn't normally
* need. This mainly contains resource bundle usage and collation meta information
*
* Modification history
* Date        Name      Comments
* 1996-1999   various members of ICU team maintained C API for collation framework
* 02/16/2001  synwee    Added internal method getPrevSpecialCE
* 03/01/2001  synwee    Added maxexpansion functionality.
* 03/16/2001  weiv      Collation framework is rewritten in C and made UCA compliant
* 12/08/2004  grhoten   Split part of ucol.cpp into ucol_res.cpp
* 2012-2014   markus    Rewritten in C++ again.
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/coll.h"
#include "unicode/localpointer.h"
#include "unicode/locid.h"
#include "unicode/tblcoll.h"
#include "unicode/ucol.h"
#include "unicode/uloc.h"
#include "unicode/unistr.h"
#include "unicode/ures.h"
#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"
#include "collationdatareader.h"
#include "collationroot.h"
#include "collationtailoring.h"
#include "resource.h"
#include "putilimp.h"
#include "uassert.h"
#include "ucln_in.h"
#include "ucol_imp.h"
#include "uenumimp.h"
#include "ulist.h"
#include "umutex.h"
#include "unifiedcache.h"
#include "uresimp.h"
#include "ustrenum.h"
#include "utracimp.h"

U_NAMESPACE_BEGIN

namespace {

static const UChar *rootRules = NULL;
static int32_t rootRulesLength = 0;
static UResourceBundle *rootBundle = NULL;
static UInitOnce gInitOnceUcolRes = U_INITONCE_INITIALIZER;

}  // namespace

U_CDECL_BEGIN

static UBool U_CALLCONV
ucol_res_cleanup() {
    rootRules = NULL;
    rootRulesLength = 0;
    ures_close(rootBundle);
    rootBundle = NULL;
    gInitOnceUcolRes.reset();
    return TRUE;
}

void U_CALLCONV
CollationLoader::loadRootRules(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    rootBundle = ures_open(U_ICUDATA_COLL, kRootLocaleName, &errorCode);
    if(U_FAILURE(errorCode)) { return; }
    rootRules = ures_getStringByKey(rootBundle, "UCARules", &rootRulesLength, &errorCode);
    if(U_FAILURE(errorCode)) {
        ures_close(rootBundle);
        rootBundle = NULL;
        return;
    }
    ucln_i18n_registerCleanup(UCLN_I18N_UCOL_RES, ucol_res_cleanup);
}

U_CDECL_END

void
CollationLoader::appendRootRules(UnicodeString &s) {
    UErrorCode errorCode = U_ZERO_ERROR;
    umtx_initOnce(gInitOnceUcolRes, CollationLoader::loadRootRules, errorCode);
    if(U_SUCCESS(errorCode)) {
        s.append(rootRules, rootRulesLength);
    }
}

void
CollationLoader::loadRules(const char *localeID, const char *collationType,
                           UnicodeString &rules, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    U_ASSERT(collationType != NULL && *collationType != 0);
    // Copy the type for lowercasing.
    char type[16];
    int32_t typeLength = static_cast<int32_t>(uprv_strlen(collationType));
    if(typeLength >= UPRV_LENGTHOF(type)) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    uprv_memcpy(type, collationType, typeLength + 1);
    T_CString_toLowerCase(type);

    LocalUResourceBundlePointer bundle(ures_open(U_ICUDATA_COLL, localeID, &errorCode));
    LocalUResourceBundlePointer collations(
            ures_getByKey(bundle.getAlias(), "collations", NULL, &errorCode));
    LocalUResourceBundlePointer data(
            ures_getByKeyWithFallback(collations.getAlias(), type, NULL, &errorCode));
    int32_t length;
    const UChar *s =  ures_getStringByKey(data.getAlias(), "Sequence", &length, &errorCode);
    if(U_FAILURE(errorCode)) { return; }

    // No string pointer aliasing so that we need not hold onto the resource bundle.
    rules.setTo(s, length);
    if(rules.isBogus()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
}

template<> U_I18N_API
const CollationCacheEntry *
LocaleCacheKey<CollationCacheEntry>::createObject(const void *creationContext,
                                                  UErrorCode &errorCode) const {
    CollationLoader *loader =
            reinterpret_cast<CollationLoader *>(
                    const_cast<void *>(creationContext));
    return loader->createCacheEntry(errorCode);
}

const CollationCacheEntry *
CollationLoader::loadTailoring(const Locale &locale, UErrorCode &errorCode) {
    const CollationCacheEntry *rootEntry = CollationRoot::getRootCacheEntry(errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    const char *name = locale.getName();
    if(*name == 0 || uprv_strcmp(name, "root") == 0) {

        // Have to add a ref.
        rootEntry->addRef();
        return rootEntry;
    }

    // Clear warning codes before loading where they get cached.
    errorCode = U_ZERO_ERROR;
    CollationLoader loader(rootEntry, locale, errorCode);

    // getCacheEntry adds a ref for us.
    return loader.getCacheEntry(errorCode);
}

CollationLoader::CollationLoader(const CollationCacheEntry *re, const Locale &requested,
                                 UErrorCode &errorCode)
        : cache(UnifiedCache::getInstance(errorCode)), rootEntry(re),
          validLocale(re->validLocale), locale(requested),
          typesTried(0), typeFallback(FALSE),
          bundle(NULL), collations(NULL), data(NULL) {
    type[0] = 0;
    defaultType[0] = 0;
    if(U_FAILURE(errorCode)) { return; }

    // Canonicalize the locale ID: Ignore all irrelevant keywords.
    const char *baseName = locale.getBaseName();
    if(uprv_strcmp(locale.getName(), baseName) != 0) {
        locale = Locale(baseName);

        // Fetch the collation type from the locale ID.
        int32_t typeLength = requested.getKeywordValue("collation",
                type, UPRV_LENGTHOF(type) - 1, errorCode);
        if(U_FAILURE(errorCode)) {
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        type[typeLength] = 0;  // in case of U_NOT_TERMINATED_WARNING
        if(typeLength == 0) {
            // No collation type.
        } else if(uprv_stricmp(type, "default") == 0) {
            // Ignore "default" (case-insensitive).
            type[0] = 0;
        } else {
            // Copy the collation type.
            T_CString_toLowerCase(type);
            locale.setKeywordValue("collation", type, errorCode);
        }
    }
}

CollationLoader::~CollationLoader() {
    ures_close(data);
    ures_close(collations);
    ures_close(bundle);
}

const CollationCacheEntry *
CollationLoader::createCacheEntry(UErrorCode &errorCode) {
    // This is a linear lookup and fallback flow turned into a state machine.
    // Most local variables have been turned into instance fields.
    // In a cache miss, cache.get() calls CacheKey::createObject(),
    // which means that we progress via recursion.
    // loadFromCollations() will recurse to itself as well for collation type fallback.
    if(bundle == NULL) {
        return loadFromLocale(errorCode);
    } else if(collations == NULL) {
        return loadFromBundle(errorCode);
    } else if(data == NULL) {
        return loadFromCollations(errorCode);
    } else {
        return loadFromData(errorCode);
    }
}

const CollationCacheEntry *
CollationLoader::loadFromLocale(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    U_ASSERT(bundle == NULL);
    bundle = ures_openNoDefault(U_ICUDATA_COLL, locale.getBaseName(), &errorCode);
    if(errorCode == U_MISSING_RESOURCE_ERROR) {
        errorCode = U_USING_DEFAULT_WARNING;

        // Have to add that ref that we promise.
        rootEntry->addRef();
        return rootEntry;
    }
    Locale requestedLocale(locale);
    const char *vLocale = ures_getLocaleByType(bundle, ULOC_ACTUAL_LOCALE, &errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    locale = validLocale = Locale(vLocale);  // no type until loadFromCollations()
    if(type[0] != 0) {
        locale.setKeywordValue("collation", type, errorCode);
    }
    if(locale != requestedLocale) {
        return getCacheEntry(errorCode);
    } else {
        return loadFromBundle(errorCode);
    }
}

const CollationCacheEntry *
CollationLoader::loadFromBundle(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    U_ASSERT(collations == NULL);
    // There are zero or more tailorings in the collations table.
    collations = ures_getByKey(bundle, "collations", NULL, &errorCode);
    if(errorCode == U_MISSING_RESOURCE_ERROR) {
        errorCode = U_USING_DEFAULT_WARNING;
        // Return the root tailoring with the validLocale, without collation type.
        return makeCacheEntryFromRoot(validLocale, errorCode);
    }
    if(U_FAILURE(errorCode)) { return NULL; }

    // Fetch the default type from the data.
    {
        UErrorCode internalErrorCode = U_ZERO_ERROR;
        LocalUResourceBundlePointer def(
                ures_getByKeyWithFallback(collations, "default", NULL, &internalErrorCode));
        int32_t length;
        const UChar *s = ures_getString(def.getAlias(), &length, &internalErrorCode);
        if(U_SUCCESS(internalErrorCode) && 0 < length && length < UPRV_LENGTHOF(defaultType)) {
            u_UCharsToChars(s, defaultType, length + 1);
        } else {
            uprv_strcpy(defaultType, "standard");
        }
    }

    // Record which collation types we have looked for already,
    // so that we do not deadlock in the cache.
    //
    // If there is no explicit type, then we look in the cache
    // for the entry with the default type.
    // If the explicit type is the default type, then we do not look in the cache
    // for the entry with an empty type.
    // Otherwise, two concurrent requests with opposite fallbacks would deadlock each other.
    // Also, it is easier to always enter the next method with a non-empty type.
    if(type[0] == 0) {
        uprv_strcpy(type, defaultType);
        typesTried |= TRIED_DEFAULT;
        if(uprv_strcmp(type, "search") == 0) {
            typesTried |= TRIED_SEARCH;
        }
        if(uprv_strcmp(type, "standard") == 0) {
            typesTried |= TRIED_STANDARD;
        }
        locale.setKeywordValue("collation", type, errorCode);
        return getCacheEntry(errorCode);
    } else {
        if(uprv_strcmp(type, defaultType) == 0) {
            typesTried |= TRIED_DEFAULT;
        }
        if(uprv_strcmp(type, "search") == 0) {
            typesTried |= TRIED_SEARCH;
        }
        if(uprv_strcmp(type, "standard") == 0) {
            typesTried |= TRIED_STANDARD;
        }
        return loadFromCollations(errorCode);
    }
}

const CollationCacheEntry *
CollationLoader::loadFromCollations(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    U_ASSERT(data == NULL);
    // Load the collations/type tailoring, with type fallback.
    LocalUResourceBundlePointer localData(
            ures_getByKeyWithFallback(collations, type, NULL, &errorCode));
    int32_t typeLength = static_cast<int32_t>(uprv_strlen(type));
    if(errorCode == U_MISSING_RESOURCE_ERROR) {
        errorCode = U_USING_DEFAULT_WARNING;
        typeFallback = TRUE;
        if((typesTried & TRIED_SEARCH) == 0 &&
                typeLength > 6 && uprv_strncmp(type, "search", 6) == 0) {
            // fall back from something like "searchjl" to "search"
            typesTried |= TRIED_SEARCH;
            type[6] = 0;
        } else if((typesTried & TRIED_DEFAULT) == 0) {
            // fall back to the default type
            typesTried |= TRIED_DEFAULT;
            uprv_strcpy(type, defaultType);
        } else if((typesTried & TRIED_STANDARD) == 0) {
            // fall back to the "standard" type
            typesTried |= TRIED_STANDARD;
            uprv_strcpy(type, "standard");
        } else {
            // Return the root tailoring with the validLocale, without collation type.
            return makeCacheEntryFromRoot(validLocale, errorCode);
        }
        locale.setKeywordValue("collation", type, errorCode);
        return getCacheEntry(errorCode);
    }
    if(U_FAILURE(errorCode)) { return NULL; }

    data = localData.orphan();
    const char *actualLocale = ures_getLocaleByType(data, ULOC_ACTUAL_LOCALE, &errorCode);
    if(U_FAILURE(errorCode)) { return NULL; }
    const char *vLocale = validLocale.getBaseName();
    UBool actualAndValidLocalesAreDifferent = uprv_strcmp(actualLocale, vLocale) != 0;

    // Set the collation types on the informational locales,
    // except when they match the default types (for brevity and backwards compatibility).
    // For the valid locale, suppress the default type.
    if(uprv_strcmp(type, defaultType) != 0) {
        validLocale.setKeywordValue("collation", type, errorCode);
        if(U_FAILURE(errorCode)) { return NULL; }
    }

    // Is this the same as the root collator? If so, then use that instead.
    if((*actualLocale == 0 || uprv_strcmp(actualLocale, "root") == 0) &&
            uprv_strcmp(type, "standard") == 0) {
        if(typeFallback) {
            errorCode = U_USING_DEFAULT_WARNING;
        }
        return makeCacheEntryFromRoot(validLocale, errorCode);
    }

    locale = Locale(actualLocale);
    if(actualAndValidLocalesAreDifferent) {
        locale.setKeywordValue("collation", type, errorCode);
        const CollationCacheEntry *entry = getCacheEntry(errorCode);
        return makeCacheEntry(validLocale, entry, errorCode);
    } else {
        return loadFromData(errorCode);
    }
}

const CollationCacheEntry *
CollationLoader::loadFromData(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return NULL; }
    LocalPointer<CollationTailoring> t(new CollationTailoring(rootEntry->tailoring->settings));
    if(t.isNull() || t->isBogus()) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }

    // deserialize
    LocalUResourceBundlePointer binary(ures_getByKey(data, "%%CollationBin", NULL, &errorCode));
    // Note: U_MISSING_RESOURCE_ERROR --> The old code built from rules if available
    // but that created undesirable dependencies.
    int32_t length;
    const uint8_t *inBytes = ures_getBinary(binary.getAlias(), &length, &errorCode);
    CollationDataReader::read(rootEntry->tailoring, inBytes, length, *t, errorCode);
    // Note: U_COLLATOR_VERSION_MISMATCH --> The old code built from rules if available
    // but that created undesirable dependencies.
    if(U_FAILURE(errorCode)) { return NULL; }

    // Try to fetch the optional rules string.
    {
        UErrorCode internalErrorCode = U_ZERO_ERROR;
        int32_t len;
        const UChar *s = ures_getStringByKey(data, "Sequence", &len,
                                             &internalErrorCode);
        if(U_SUCCESS(internalErrorCode)) {
            t->rules.setTo(TRUE, s, len);
        }
    }

    const char *actualLocale = locale.getBaseName();  // without type
    const char *vLocale = validLocale.getBaseName();
    UBool actualAndValidLocalesAreDifferent = uprv_strcmp(actualLocale, vLocale) != 0;

    // For the actual locale, suppress the default type *according to the actual locale*.
    // For example, zh has default=pinyin and contains all of the Chinese tailorings.
    // zh_Hant has default=stroke but has no other data.
    // For the valid locale "zh_Hant" we need to suppress stroke.
    // For the actual locale "zh" we need to suppress pinyin instead.
    if(actualAndValidLocalesAreDifferent) {
        // Opening a bundle for the actual locale should always succeed.
        LocalUResourceBundlePointer actualBundle(
                ures_open(U_ICUDATA_COLL, actualLocale, &errorCode));
        if(U_FAILURE(errorCode)) { return NULL; }
        UErrorCode internalErrorCode = U_ZERO_ERROR;
        LocalUResourceBundlePointer def(
                ures_getByKeyWithFallback(actualBundle.getAlias(), "collations/default", NULL,
                                          &internalErrorCode));
        int32_t len;
        const UChar *s = ures_getString(def.getAlias(), &len, &internalErrorCode);
        if(U_SUCCESS(internalErrorCode) && len < UPRV_LENGTHOF(defaultType)) {
            u_UCharsToChars(s, defaultType, len + 1);
        } else {
            uprv_strcpy(defaultType, "standard");
        }
    }
    t->actualLocale = locale;
    if(uprv_strcmp(type, defaultType) != 0) {
        t->actualLocale.setKeywordValue("collation", type, errorCode);
    } else if(uprv_strcmp(locale.getName(), locale.getBaseName()) != 0) {
        // Remove the collation keyword if it was set.
        t->actualLocale.setKeywordValue("collation", NULL, errorCode);
    }
    if(U_FAILURE(errorCode)) { return NULL; }

    if(typeFallback) {
        errorCode = U_USING_DEFAULT_WARNING;
    }
    t->bundle = bundle;
    bundle = NULL;
    const CollationCacheEntry *entry = new CollationCacheEntry(validLocale, t.getAlias());
    if(entry == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    } else {
        t.orphan();
    }
    // Have to add that reference that we promise.
    entry->addRef();
    return entry;
}

const CollationCacheEntry *
CollationLoader::getCacheEntry(UErrorCode &errorCode) {
    LocaleCacheKey<CollationCacheEntry> key(locale);
    const CollationCacheEntry *entry = NULL;
    cache->get(key, this, entry, errorCode);
    return entry;
}

const CollationCacheEntry *
CollationLoader::makeCacheEntryFromRoot(
        const Locale &/*loc*/,
        UErrorCode &errorCode) const {
    if (U_FAILURE(errorCode)) {
        return NULL;
    }
    rootEntry->addRef();
    return makeCacheEntry(validLocale, rootEntry, errorCode);
}

const CollationCacheEntry *
CollationLoader::makeCacheEntry(
        const Locale &loc,
        const CollationCacheEntry *entryFromCache,
        UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || loc == entryFromCache->validLocale) {
        return entryFromCache;
    }
    CollationCacheEntry *entry = new CollationCacheEntry(loc, entryFromCache->tailoring);
    if(entry == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        entryFromCache->removeRef();
        return NULL;
    }
    entry->addRef();
    entryFromCache->removeRef();
    return entry;
}

U_NAMESPACE_END

U_NAMESPACE_USE

U_CAPI UCollator*
ucol_open(const char *loc,
          UErrorCode *status)
{
    UTRACE_ENTRY_OC(UTRACE_UCOL_OPEN);
    UTRACE_DATA1(UTRACE_INFO, "locale = \"%s\"", loc);
    UCollator *result = NULL;

    Collator *coll = Collator::createInstance(loc, *status);
    if(U_SUCCESS(*status)) {
        result = coll->toUCollator();
    }
    UTRACE_EXIT_PTR_STATUS(result, *status);
    return result;
}


U_CAPI int32_t U_EXPORT2
ucol_getDisplayName(    const    char        *objLoc,
                    const    char        *dispLoc,
                    UChar             *result,
                    int32_t         resultLength,
                    UErrorCode        *status)
{
    if(U_FAILURE(*status)) return -1;
    UnicodeString dst;
    if(!(result==NULL && resultLength==0)) {
        // NULL destination for pure preflighting: empty dummy string
        // otherwise, alias the destination buffer
        dst.setTo(result, 0, resultLength);
    }
    Collator::getDisplayName(Locale(objLoc), Locale(dispLoc), dst);
    return dst.extract(result, resultLength, *status);
}

U_CAPI const char* U_EXPORT2
ucol_getAvailable(int32_t index)
{
    int32_t count = 0;
    const Locale *loc = Collator::getAvailableLocales(count);
    if (loc != NULL && index < count) {
        return loc[index].getName();
    }
    return NULL;
}

U_CAPI int32_t U_EXPORT2
ucol_countAvailable()
{
    int32_t count = 0;
    Collator::getAvailableLocales(count);
    return count;
}

#if !UCONFIG_NO_SERVICE
U_CAPI UEnumeration* U_EXPORT2
ucol_openAvailableLocales(UErrorCode *status) {
    // This is a wrapper over Collator::getAvailableLocales()
    if (U_FAILURE(*status)) {
        return NULL;
    }
    StringEnumeration *s = icu::Collator::getAvailableLocales();
    if (s == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    return uenum_openFromStringEnumeration(s, status);
}
#endif

// Note: KEYWORDS[0] != RESOURCE_NAME - alan

static const char RESOURCE_NAME[] = "collations";

static const char* const KEYWORDS[] = { "collation" };

#define KEYWORD_COUNT UPRV_LENGTHOF(KEYWORDS)

U_CAPI UEnumeration* U_EXPORT2
ucol_getKeywords(UErrorCode *status) {
    UEnumeration *result = NULL;
    if (U_SUCCESS(*status)) {
        return uenum_openCharStringsEnumeration(KEYWORDS, KEYWORD_COUNT, status);
    }
    return result;
}

U_CAPI UEnumeration* U_EXPORT2
ucol_getKeywordValues(const char *keyword, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return NULL;
    }
    // hard-coded to accept exactly one collation keyword
    // modify if additional collation keyword is added later
    if (keyword==NULL || uprv_strcmp(keyword, KEYWORDS[0])!=0)
    {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    return ures_getKeywordValues(U_ICUDATA_COLL, RESOURCE_NAME, status);
}

static const UEnumeration defaultKeywordValues = {
    NULL,
    NULL,
    ulist_close_keyword_values_iterator,
    ulist_count_keyword_values,
    uenum_unextDefault,
    ulist_next_keyword_value,
    ulist_reset_keyword_values_iterator
};

namespace {

struct KeywordsSink : public ResourceSink {
public:
    KeywordsSink(UErrorCode &errorCode) :
            values(ulist_createEmptyList(&errorCode)), hasDefault(FALSE) {}
    virtual ~KeywordsSink();

    virtual void put(const char *key, ResourceValue &value, UBool /*noFallback*/,
                     UErrorCode &errorCode) {
        if (U_FAILURE(errorCode)) { return; }
        ResourceTable collations = value.getTable(errorCode);
        for (int32_t i = 0; collations.getKeyAndValue(i, key, value); ++i) {
            UResType type = value.getType();
            if (type == URES_STRING) {
                if (!hasDefault && uprv_strcmp(key, "default") == 0) {
                    CharString defcoll;
                    defcoll.appendInvariantChars(value.getUnicodeString(errorCode), errorCode);
                    if (U_SUCCESS(errorCode) && !defcoll.isEmpty()) {
                        char *ownedDefault = uprv_strdup(defcoll.data());
                        if (ownedDefault == NULL) {
                            errorCode = U_MEMORY_ALLOCATION_ERROR;
                            return;
                        }
                        ulist_removeString(values, defcoll.data());
                        ulist_addItemBeginList(values, ownedDefault, TRUE, &errorCode);
                        hasDefault = TRUE;
                    }
                }
            } else if (type == URES_TABLE && uprv_strncmp(key, "private-", 8) != 0) {
                if (!ulist_containsString(values, key, (int32_t)uprv_strlen(key))) {
                    ulist_addItemEndList(values, key, FALSE, &errorCode);
                }
            }
            if (U_FAILURE(errorCode)) { return; }
        }
    }

    UList *values;
    UBool hasDefault;
};

KeywordsSink::~KeywordsSink() {
    ulist_deleteList(values);
}

}  // namespace

U_CAPI UEnumeration* U_EXPORT2
ucol_getKeywordValuesForLocale(const char* /*key*/, const char* locale,
                               UBool /*commonlyUsed*/, UErrorCode* status) {
    // Note: The parameter commonlyUsed is not used.
    // The switch is in the method signature for consistency
    // with other locale services.

    // Read available collation values from collation bundles.
    LocalUResourceBundlePointer bundle(ures_open(U_ICUDATA_COLL, locale, status));
    KeywordsSink sink(*status);
    ures_getAllItemsWithFallback(bundle.getAlias(), RESOURCE_NAME, sink, *status);
    if (U_FAILURE(*status)) { return NULL; }

    UEnumeration *en = (UEnumeration *)uprv_malloc(sizeof(UEnumeration));
    if (en == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    memcpy(en, &defaultKeywordValues, sizeof(UEnumeration));
    ulist_resetList(sink.values);  // Initialize the iterator.
    en->context = sink.values;
    sink.values = NULL;  // Avoid deletion in the sink destructor.
    return en;
}

U_CAPI int32_t U_EXPORT2
ucol_getFunctionalEquivalent(char* result, int32_t resultCapacity,
                             const char* keyword, const char* locale,
                             UBool* isAvailable, UErrorCode* status)
{
    // N.B.: Resource name is "collations" but keyword is "collation"
    return ures_getFunctionalEquivalent(result, resultCapacity, U_ICUDATA_COLL,
        "collations", keyword, locale,
        isAvailable, TRUE, status);
}

#endif /* #if !UCONFIG_NO_COLLATION */
