// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 1997-2016, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*
* File uresbund.cpp
*
* Modification History:
*
*   Date        Name        Description
*   04/01/97    aliu        Creation.
*   06/14/99    stephen     Removed functions taking a filename suffix.
*   07/20/99    stephen     Changed for UResourceBundle typedef'd to void*
*   11/09/99    weiv            Added ures_getLocale()
*   March 2000  weiv        Total overhaul - using data in DLLs
*   06/20/2000  helena      OS/400 port changes; mostly typecast.
*   06/24/02    weiv        Added support for resource sharing
******************************************************************************
*/

#include "unicode/ures.h"
#include "unicode/ustring.h"
#include "unicode/ucnv.h"
#include "charstr.h"
#include "uresimp.h"
#include "ustr_imp.h"
#include "cwchar.h"
#include "ucln_cmn.h"
#include "cmemory.h"
#include "cstring.h"
#include "mutex.h"
#include "uhash.h"
#include "unicode/uenum.h"
#include "uenumimp.h"
#include "ulocimp.h"
#include "umutex.h"
#include "putilimp.h"
#include "uassert.h"
#include "uresdata.h"

using namespace icu;

/*
Static cache for already opened resource bundles - mostly for keeping fallback info
TODO: This cache should probably be removed when the deprecated code is
      completely removed.
*/
static UHashtable *cache = NULL;
static icu::UInitOnce gCacheInitOnce = U_INITONCE_INITIALIZER;

static UMutex resbMutex;

/* INTERNAL: hashes an entry  */
static int32_t U_CALLCONV hashEntry(const UHashTok parm) {
    UResourceDataEntry *b = (UResourceDataEntry *)parm.pointer;
    UHashTok namekey, pathkey;
    namekey.pointer = b->fName;
    pathkey.pointer = b->fPath;
    return uhash_hashChars(namekey)+37u*uhash_hashChars(pathkey);
}

/* INTERNAL: compares two entries */
static UBool U_CALLCONV compareEntries(const UHashTok p1, const UHashTok p2) {
    UResourceDataEntry *b1 = (UResourceDataEntry *)p1.pointer;
    UResourceDataEntry *b2 = (UResourceDataEntry *)p2.pointer;
    UHashTok name1, name2, path1, path2;
    name1.pointer = b1->fName;
    name2.pointer = b2->fName;
    path1.pointer = b1->fPath;
    path2.pointer = b2->fPath;
    return (UBool)(uhash_compareChars(name1, name2) &&
        uhash_compareChars(path1, path2));
}


/**
 *  Internal function, gets parts of locale name according 
 *  to the position of '_' character
 */
static UBool chopLocale(char *name) {
    char *i = uprv_strrchr(name, '_');

    if(i != NULL) {
        *i = '\0';
        return TRUE;
    }

    return FALSE;
}

/**
 *  Called to check whether a name without '_' needs to be checked for a parent.
 *  Some code had assumed that locale IDs with '_' could not have a non-root parent.
 *  We may want a better way of doing this.
 */
static UBool mayHaveParent(char *name) {
    return (name[0] != 0 && uprv_strstr("nb nn",name) != nullptr);
}

/**
 *  Internal function
 */
static void entryIncrease(UResourceDataEntry *entry) {
    Mutex lock(&resbMutex);
    entry->fCountExisting++;
    while(entry->fParent != NULL) {
      entry = entry->fParent;
      entry->fCountExisting++;
    }
}

/**
 *  Internal function. Tries to find a resource in given Resource
 *  Bundle, as well as in its parents
 */
static UResourceDataEntry *getFallbackData(
        const UResourceBundle *resBundle,
        const char **resTag, Resource *res, UErrorCode *status) {
    UResourceDataEntry *dataEntry = resBundle->fData;
    int32_t indexR = -1;
    int32_t i = 0;
    *res = RES_BOGUS;
    if(dataEntry == nullptr) {
        *status = U_MISSING_RESOURCE_ERROR;
        return nullptr;
    }
    if(dataEntry->fBogus == U_ZERO_ERROR) { /* if this resource is real, */
        *res = res_getTableItemByKey(&(dataEntry->fData), dataEntry->fData.rootRes, &indexR, resTag); /* try to get data from there */
        i++;
    }
    if(resBundle->fHasFallback) {
        // Otherwise, we'll look in parents.
        while(*res == RES_BOGUS && dataEntry->fParent != nullptr) {
            dataEntry = dataEntry->fParent;
            if(dataEntry->fBogus == U_ZERO_ERROR) {
                i++;
                *res = res_getTableItemByKey(&(dataEntry->fData), dataEntry->fData.rootRes, &indexR, resTag);
            }
        }
    }

    if(*res == RES_BOGUS) {
        // If the resource is not found, we need to give an error.
        *status = U_MISSING_RESOURCE_ERROR;
        return nullptr;
    }
    // If the resource is found in parents, we need to adjust the error.
    if(i>1) {
        if(uprv_strcmp(dataEntry->fName, uloc_getDefault())==0 || uprv_strcmp(dataEntry->fName, kRootLocaleName)==0) {
            *status = U_USING_DEFAULT_WARNING;
        } else {
            *status = U_USING_FALLBACK_WARNING;
        }
    }
    return dataEntry;
}

static void
free_entry(UResourceDataEntry *entry) {
    UResourceDataEntry *alias;
    res_unload(&(entry->fData));
    if(entry->fName != NULL && entry->fName != entry->fNameBuffer) {
        uprv_free(entry->fName);
    }
    if(entry->fPath != NULL) {
        uprv_free(entry->fPath);
    }
    if(entry->fPool != NULL) {
        --entry->fPool->fCountExisting;
    }
    alias = entry->fAlias;
    if(alias != NULL) {
        while(alias->fAlias != NULL) {
            alias = alias->fAlias;
        }
        --alias->fCountExisting;
    }
    uprv_free(entry);
}

/* Works just like ucnv_flushCache() */
static int32_t ures_flushCache()
{
    UResourceDataEntry *resB;
    int32_t pos;
    int32_t rbDeletedNum = 0;
    const UHashElement *e;
    UBool deletedMore;

    /*if shared data hasn't even been lazy evaluated yet
    * return 0
    */
    Mutex lock(&resbMutex);
    if (cache == NULL) {
        return 0;
    }

    do {
        deletedMore = FALSE;
        /*creates an enumeration to iterate through every element in the table */
        pos = UHASH_FIRST;
        while ((e = uhash_nextElement(cache, &pos)) != NULL)
        {
            resB = (UResourceDataEntry *) e->value.pointer;
            /* Deletes only if reference counter == 0
             * Don't worry about the children of this node.
             * Those will eventually get deleted too, if not already.
             * Don't worry about the parents of this node.
             * Those will eventually get deleted too, if not already.
             */
            /* 04/05/2002 [weiv] fCountExisting should now be accurate. If it's not zero, that means that    */
            /* some resource bundles are still open somewhere. */

            if (resB->fCountExisting == 0) {
                rbDeletedNum++;
                deletedMore = TRUE;
                uhash_removeElement(cache, e);
                free_entry(resB);
            }
        }
        /*
         * Do it again to catch bundles (aliases, pool bundle) whose fCountExisting
         * got decremented by free_entry().
         */
    } while(deletedMore);

    return rbDeletedNum;
}

#ifdef URES_DEBUG
#include <stdio.h>

U_CAPI UBool U_EXPORT2 ures_dumpCacheContents(void) {
  UBool cacheNotEmpty = FALSE;
  int32_t pos = UHASH_FIRST;
  const UHashElement *e;
  UResourceDataEntry *resB;
  
    Mutex lock(&resbMutex);
    if (cache == NULL) {
      fprintf(stderr,"%s:%d: RB Cache is NULL.\n", __FILE__, __LINE__);
      return FALSE;
    }

    while ((e = uhash_nextElement(cache, &pos)) != NULL) {
      cacheNotEmpty=TRUE;
      resB = (UResourceDataEntry *) e->value.pointer;
      fprintf(stderr,"%s:%d: RB Cache: Entry @0x%p, refcount %d, name %s:%s.  Pool 0x%p, alias 0x%p, parent 0x%p\n",
              __FILE__, __LINE__,
              (void*)resB, resB->fCountExisting,
              resB->fName?resB->fName:"NULL",
              resB->fPath?resB->fPath:"NULL",
              (void*)resB->fPool,
              (void*)resB->fAlias,
              (void*)resB->fParent);       
    }
    
    fprintf(stderr,"%s:%d: RB Cache still contains %d items.\n", __FILE__, __LINE__, uhash_count(cache));
    return cacheNotEmpty;
}

#endif

static UBool U_CALLCONV ures_cleanup(void)
{
    if (cache != NULL) {
        ures_flushCache();
        uhash_close(cache);
        cache = NULL;
    }
    gCacheInitOnce.reset();
    return TRUE;
}

/** INTERNAL: Initializes the cache for resources */
static void U_CALLCONV createCache(UErrorCode &status) {
    U_ASSERT(cache == NULL);
    cache = uhash_open(hashEntry, compareEntries, NULL, &status);
    ucln_common_registerCleanup(UCLN_COMMON_URES, ures_cleanup);
}
     
static void initCache(UErrorCode *status) {
    umtx_initOnce(gCacheInitOnce, &createCache, *status);
}

/** INTERNAL: sets the name (locale) of the resource bundle to given name */

static void setEntryName(UResourceDataEntry *res, const char *name, UErrorCode *status) {
    int32_t len = (int32_t)uprv_strlen(name);
    if(res->fName != NULL && res->fName != res->fNameBuffer) {
        uprv_free(res->fName);
    }
    if (len < (int32_t)sizeof(res->fNameBuffer)) {
        res->fName = res->fNameBuffer;
    }
    else {
        res->fName = (char *)uprv_malloc(len+1);
    }
    if(res->fName == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
    } else {
        uprv_strcpy(res->fName, name);
    }
}

static UResourceDataEntry *
getPoolEntry(const char *path, UErrorCode *status);

/**
 *  INTERNAL: Inits and opens an entry from a data DLL.
 *    CAUTION:  resbMutex must be locked when calling this function.
 */
static UResourceDataEntry *init_entry(const char *localeID, const char *path, UErrorCode *status) {
    UResourceDataEntry *r = NULL;
    UResourceDataEntry find;
    /*int32_t hashValue;*/
    const char *name;
    char aliasName[100] = { 0 };
    int32_t aliasLen = 0;
    /*UBool isAlias = FALSE;*/
    /*UHashTok hashkey; */

    if(U_FAILURE(*status)) {
        return NULL;
    }

    /* here we try to deduce the right locale name */
    if(localeID == NULL) { /* if localeID is NULL, we're trying to open default locale */
        name = uloc_getDefault();
    } else if(*localeID == 0) { /* if localeID is "" then we try to open root locale */
        name = kRootLocaleName;
    } else { /* otherwise, we'll open what we're given */
        name = localeID;
    }

    find.fName = (char *)name;
    find.fPath = (char *)path;

    /* calculate the hash value of the entry */
    /*hashkey.pointer = (void *)&find;*/
    /*hashValue = hashEntry(hashkey);*/

    /* check to see if we already have this entry */
    r = (UResourceDataEntry *)uhash_get(cache, &find);
    if(r == NULL) {
        /* if the entry is not yet in the hash table, we'll try to construct a new one */
        r = (UResourceDataEntry *) uprv_malloc(sizeof(UResourceDataEntry));
        if(r == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }

        uprv_memset(r, 0, sizeof(UResourceDataEntry));
        /*r->fHashKey = hashValue;*/

        setEntryName(r, name, status);
        if (U_FAILURE(*status)) {
            uprv_free(r);
            return NULL;
        }

        if(path != NULL) {
            r->fPath = (char *)uprv_strdup(path);
            if(r->fPath == NULL) {
                *status = U_MEMORY_ALLOCATION_ERROR;
                uprv_free(r);
                return NULL;
            }
        }

        /* this is the actual loading */
        res_load(&(r->fData), r->fPath, r->fName, status);

        if (U_FAILURE(*status)) {
            /* if we failed to load due to an out-of-memory error, exit early. */
            if (*status == U_MEMORY_ALLOCATION_ERROR) {
                uprv_free(r);
                return NULL;
            }
            /* we have no such entry in dll, so it will always use fallback */
            *status = U_USING_FALLBACK_WARNING;
            r->fBogus = U_USING_FALLBACK_WARNING;
        } else { /* if we have a regular entry */
            Resource aliasres;
            if (r->fData.usesPoolBundle) {
                r->fPool = getPoolEntry(r->fPath, status);
                if (U_SUCCESS(*status)) {
                    const int32_t *poolIndexes = r->fPool->fData.pRoot + 1;
                    if(r->fData.pRoot[1 + URES_INDEX_POOL_CHECKSUM] == poolIndexes[URES_INDEX_POOL_CHECKSUM]) {
                        r->fData.poolBundleKeys = (const char *)(poolIndexes + (poolIndexes[URES_INDEX_LENGTH] & 0xff));
                        r->fData.poolBundleStrings = r->fPool->fData.p16BitUnits;
                    } else {
                        r->fBogus = *status = U_INVALID_FORMAT_ERROR;
                    }
                } else {
                    r->fBogus = *status;
                }
            }
            if (U_SUCCESS(*status)) {
                /* handle the alias by trying to get out the %%Alias tag.*/
                /* We'll try to get alias string from the bundle */
                aliasres = res_getResource(&(r->fData), "%%ALIAS");
                if (aliasres != RES_BOGUS) {
                    // No tracing: called during initial data loading
                    const UChar *alias = res_getStringNoTrace(&(r->fData), aliasres, &aliasLen);
                    if(alias != NULL && aliasLen > 0) { /* if there is actual alias - unload and load new data */
                        u_UCharsToChars(alias, aliasName, aliasLen+1);
                        r->fAlias = init_entry(aliasName, path, status);
                    }
                }
            }
        }

        {
            UResourceDataEntry *oldR = NULL;
            if((oldR = (UResourceDataEntry *)uhash_get(cache, r)) == NULL) { /* if the data is not cached */
                /* just insert it in the cache */
                UErrorCode cacheStatus = U_ZERO_ERROR;
                uhash_put(cache, (void *)r, r, &cacheStatus);
                if (U_FAILURE(cacheStatus)) {
                    *status = cacheStatus;
                    free_entry(r);
                    r = NULL;
                }
            } else {
                /* somebody have already inserted it while we were working, discard newly opened data */
                /* Also, we could get here IF we opened an alias */
                free_entry(r);
                r = oldR;
            }
        }

    }
    if(r != NULL) {
        /* return the real bundle */
        while(r->fAlias != NULL) {
            r = r->fAlias;
        }
        r->fCountExisting++; /* we increase its reference count */
        /* if the resource has a warning */
        /* we don't want to overwrite a status with no error */
        if(r->fBogus != U_ZERO_ERROR && U_SUCCESS(*status)) {
             *status = r->fBogus; /* set the returning status */
        }
    }
    return r;
}

static UResourceDataEntry *
getPoolEntry(const char *path, UErrorCode *status) {
    UResourceDataEntry *poolBundle = init_entry(kPoolBundleName, path, status);
    if( U_SUCCESS(*status) &&
        (poolBundle == NULL || poolBundle->fBogus != U_ZERO_ERROR || !poolBundle->fData.isPoolBundle)
    ) {
        *status = U_INVALID_FORMAT_ERROR;
    }
    return poolBundle;
}

/* INTERNAL: */
/*   CAUTION:  resbMutex must be locked when calling this function! */
static UResourceDataEntry *
findFirstExisting(const char* path, char* name, const char* defaultLocale,
                  UBool *isRoot, UBool *hasChopped, UBool *isDefault, UErrorCode* status) {
    UResourceDataEntry *r = NULL;
    UBool hasRealData = FALSE;
    *hasChopped = TRUE; /* we're starting with a fresh name */

    while(*hasChopped && !hasRealData) {
        r = init_entry(name, path, status);
        /* Null pointer test */
        if (U_FAILURE(*status)) {
            return NULL;
        }
        *isDefault = (UBool)(uprv_strncmp(name, defaultLocale, uprv_strlen(name)) == 0);
        hasRealData = (UBool)(r->fBogus == U_ZERO_ERROR);
        if(!hasRealData) {
            /* this entry is not real. We will discard it. */
            /* However, the parent line for this entry is  */
            /* not to be used - as there might be parent   */
            /* lines in cache from previous openings that  */
            /* are not updated yet. */
            r->fCountExisting--;
            /*entryCloseInt(r);*/
            r = NULL;
            *status = U_USING_FALLBACK_WARNING;
        } else {
            uprv_strcpy(name, r->fName); /* this is needed for supporting aliases */
        }

        *isRoot = (UBool)(uprv_strcmp(name, kRootLocaleName) == 0);

        /*Fallback data stuff*/
        *hasChopped = chopLocale(name);
        if (*hasChopped && *name == '\0') {
            uprv_strcpy(name, "und");
        }
    }
    return r;
}

static void ures_setIsStackObject( UResourceBundle* resB, UBool state) {
    if(state) {
        resB->fMagic1 = 0;
        resB->fMagic2 = 0;
    } else {
        resB->fMagic1 = MAGIC1;
        resB->fMagic2 = MAGIC2;
    }
}

static UBool ures_isStackObject(const UResourceBundle* resB) {
  return((resB->fMagic1 == MAGIC1 && resB->fMagic2 == MAGIC2)?FALSE:TRUE);
}


U_CFUNC void ures_initStackObject(UResourceBundle* resB) {
  uprv_memset(resB, 0, sizeof(UResourceBundle));
  ures_setIsStackObject(resB, TRUE);
}

U_NAMESPACE_BEGIN

StackUResourceBundle::StackUResourceBundle() {
    ures_initStackObject(&bundle);
}

StackUResourceBundle::~StackUResourceBundle() {
    ures_close(&bundle);
}

U_NAMESPACE_END

static UBool  // returns U_SUCCESS(*status)
loadParentsExceptRoot(UResourceDataEntry *&t1,
                      char name[], int32_t nameCapacity,
                      UBool usingUSRData, char usrDataPath[], UErrorCode *status) {
    if (U_FAILURE(*status)) { return FALSE; }
    UBool checkParent = TRUE;
    while (checkParent && t1->fParent == NULL && !t1->fData.noFallback &&
            res_getResource(&t1->fData,"%%ParentIsRoot") == RES_BOGUS) {
        Resource parentRes = res_getResource(&t1->fData, "%%Parent");
        if (parentRes != RES_BOGUS) {  // An explicit parent was found.
            int32_t parentLocaleLen = 0;
            // No tracing: called during initial data loading
            const UChar *parentLocaleName = res_getStringNoTrace(&(t1->fData), parentRes, &parentLocaleLen);
            if(parentLocaleName != NULL && 0 < parentLocaleLen && parentLocaleLen < nameCapacity) {
                u_UCharsToChars(parentLocaleName, name, parentLocaleLen + 1);
                if (uprv_strcmp(name, kRootLocaleName) == 0) {
                    return TRUE;
                }
            }
        }
        // Insert regular parents.
        UErrorCode parentStatus = U_ZERO_ERROR;
        UResourceDataEntry *t2 = init_entry(name, t1->fPath, &parentStatus);
        if (U_FAILURE(parentStatus)) {
            *status = parentStatus;
            return FALSE;
        }
        UResourceDataEntry *u2 = NULL;
        UErrorCode usrStatus = U_ZERO_ERROR;
        if (usingUSRData) {  // This code inserts user override data into the inheritance chain.
            u2 = init_entry(name, usrDataPath, &usrStatus);
            // If we failed due to out-of-memory, report that to the caller and exit early.
            if (usrStatus == U_MEMORY_ALLOCATION_ERROR) {
                *status = usrStatus;
                return FALSE;
            }
        }

        if (usingUSRData && U_SUCCESS(usrStatus) && u2->fBogus == U_ZERO_ERROR) {
            t1->fParent = u2;
            u2->fParent = t2;
        } else {
            t1->fParent = t2;
            if (usingUSRData) {
                // The USR override data wasn't found, set it to be deleted.
                u2->fCountExisting = 0;
            }
        }
        t1 = t2;
        checkParent = chopLocale(name) || mayHaveParent(name);
    }
    return TRUE;
}

static UBool  // returns U_SUCCESS(*status)
insertRootBundle(UResourceDataEntry *&t1, UErrorCode *status) {
    if (U_FAILURE(*status)) { return FALSE; }
    UErrorCode parentStatus = U_ZERO_ERROR;
    UResourceDataEntry *t2 = init_entry(kRootLocaleName, t1->fPath, &parentStatus);
    if (U_FAILURE(parentStatus)) {
        *status = parentStatus;
        return FALSE;
    }
    t1->fParent = t2;
    t1 = t2;
    return TRUE;
}

enum UResOpenType {
    /**
     * Open a resource bundle for the locale;
     * if there is not even a base language bundle, then fall back to the default locale;
     * if there is no bundle for that either, then load the root bundle.
     *
     * This is the default bundle loading behavior.
     */
    URES_OPEN_LOCALE_DEFAULT_ROOT,
    // TODO: ICU ticket #11271 "consistent default locale across locale trees"
    // Add an option to look at the main locale tree for whether to
    // fall back to root directly (if the locale has main data) or
    // fall back to the default locale first (if the locale does not even have main data).
    /**
     * Open a resource bundle for the locale;
     * if there is not even a base language bundle, then load the root bundle;
     * never fall back to the default locale.
     *
     * This is used for algorithms that have good pan-Unicode default behavior,
     * such as case mappings, collation, and segmentation (BreakIterator).
     */
    URES_OPEN_LOCALE_ROOT,
    /**
     * Open a resource bundle for the exact bundle name as requested;
     * no fallbacks, do not load parent bundles.
     *
     * This is used for supplemental (non-locale) data.
     */
    URES_OPEN_DIRECT
};
typedef enum UResOpenType UResOpenType;

static UResourceDataEntry *entryOpen(const char* path, const char* localeID,
                                     UResOpenType openType, UErrorCode* status) {
    U_ASSERT(openType != URES_OPEN_DIRECT);
    UErrorCode intStatus = U_ZERO_ERROR;
    UResourceDataEntry *r = NULL;
    UResourceDataEntry *t1 = NULL;
    UBool isDefault = FALSE;
    UBool isRoot = FALSE;
    UBool hasRealData = FALSE;
    UBool hasChopped = TRUE;
    UBool usingUSRData = U_USE_USRDATA && ( path == NULL || uprv_strncmp(path,U_ICUDATA_NAME,8) == 0);

    char name[ULOC_FULLNAME_CAPACITY];
    char usrDataPath[96];

    initCache(status);

    if(U_FAILURE(*status)) {
        return NULL;
    }

    uprv_strncpy(name, localeID, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;

    if ( usingUSRData ) {
        if ( path == NULL ) {
            uprv_strcpy(usrDataPath, U_USRDATA_NAME);
        } else {
            uprv_strncpy(usrDataPath, path, sizeof(usrDataPath) - 1);
            usrDataPath[0] = 'u';
            usrDataPath[1] = 's';
            usrDataPath[2] = 'r';
            usrDataPath[sizeof(usrDataPath) - 1] = 0;
        }
    }
 
    // Note: We need to query the default locale *before* locking resbMutex.
    const char *defaultLocale = uloc_getDefault();

    Mutex lock(&resbMutex);    // Lock resbMutex until the end of this function.

    /* We're going to skip all the locales that do not have any data */
    r = findFirstExisting(path, name, defaultLocale, &isRoot, &hasChopped, &isDefault, &intStatus);

    // If we failed due to out-of-memory, report the failure and exit early.
    if (intStatus == U_MEMORY_ALLOCATION_ERROR) {
        *status = intStatus;
        goto finish;
    }

    if(r != NULL) { /* if there is one real locale, we can look for parents. */
        t1 = r;
        hasRealData = TRUE;
        if ( usingUSRData ) {  /* This code inserts user override data into the inheritance chain */
            UErrorCode usrStatus = U_ZERO_ERROR;
            UResourceDataEntry *u1 = init_entry(t1->fName, usrDataPath, &usrStatus);
            // If we failed due to out-of-memory, report the failure and exit early.
            if (intStatus == U_MEMORY_ALLOCATION_ERROR) {
                *status = intStatus;
                goto finish;
            }
            if ( u1 != NULL ) {
                if(u1->fBogus == U_ZERO_ERROR) {
                    u1->fParent = t1;
                    r = u1;
                } else {
                    /* the USR override data wasn't found, set it to be deleted */
                    u1->fCountExisting = 0;
                }
            }
        }
        if ((hasChopped || mayHaveParent(name)) && !isRoot) {
            if (!loadParentsExceptRoot(t1, name, UPRV_LENGTHOF(name), usingUSRData, usrDataPath, status)) {
                goto finish;
            }
        }
    }

    /* we could have reached this point without having any real data */
    /* if that is the case, we need to chain in the default locale   */
    if(r==NULL && openType == URES_OPEN_LOCALE_DEFAULT_ROOT && !isDefault && !isRoot) {
        /* insert default locale */
        uprv_strcpy(name, defaultLocale);
        r = findFirstExisting(path, name, defaultLocale, &isRoot, &hasChopped, &isDefault, &intStatus);
        // If we failed due to out-of-memory, report the failure and exit early.
        if (intStatus == U_MEMORY_ALLOCATION_ERROR) {
            *status = intStatus;
            goto finish;
        }
        intStatus = U_USING_DEFAULT_WARNING;
        if(r != NULL) { /* the default locale exists */
            t1 = r;
            hasRealData = TRUE;
            isDefault = TRUE;
            // TODO: Why not if (usingUSRData) { ... } like in the non-default-locale code path?
            if ((hasChopped || mayHaveParent(name)) && !isRoot) {
                if (!loadParentsExceptRoot(t1, name, UPRV_LENGTHOF(name), usingUSRData, usrDataPath, status)) {
                    goto finish;
                }
            }
        }
    }

    /* we could still have r == NULL at this point - maybe even default locale is not */
    /* present */
    if(r == NULL) {
        uprv_strcpy(name, kRootLocaleName);
        r = findFirstExisting(path, name, defaultLocale, &isRoot, &hasChopped, &isDefault, &intStatus);
        // If we failed due to out-of-memory, report the failure and exit early.
        if (intStatus == U_MEMORY_ALLOCATION_ERROR) {
            *status = intStatus;
            goto finish;
        }
        if(r != NULL) {
            t1 = r;
            intStatus = U_USING_DEFAULT_WARNING;
            hasRealData = TRUE;
        } else { /* we don't even have the root locale */
            *status = U_MISSING_RESOURCE_ERROR;
            goto finish;
        }
    } else if(!isRoot && uprv_strcmp(t1->fName, kRootLocaleName) != 0 &&
            t1->fParent == NULL && !r->fData.noFallback) {
        if (!insertRootBundle(t1, status)) {
            goto finish;
        }
        if(!hasRealData) {
            r->fBogus = U_USING_DEFAULT_WARNING;
        }
    }

    // TODO: Does this ever loop?
    while(r != NULL && !isRoot && t1->fParent != NULL) {
        t1->fParent->fCountExisting++;
        t1 = t1->fParent;
    }

finish:
    if(U_SUCCESS(*status)) {
        if(intStatus != U_ZERO_ERROR) {
            *status = intStatus;  
        }
        return r;
    } else {
        return NULL;
    }
}

/**
 * Version of entryOpen() and findFirstExisting() for ures_openDirect(),
 * with no fallbacks.
 * Parent and root locale bundles are loaded if
 * the requested bundle does not have the "nofallback" flag.
 */
static UResourceDataEntry *
entryOpenDirect(const char* path, const char* localeID, UErrorCode* status) {
    initCache(status);
    if(U_FAILURE(*status)) {
        return NULL;
    }

    // Note: We need to query the default locale *before* locking resbMutex.
    // If the localeID is NULL, then we want to use the default locale.
    if (localeID == NULL) {
        localeID = uloc_getDefault();
    } else if (*localeID == 0) {
        // If the localeID is "", then we want to use the root locale.
        localeID = kRootLocaleName;
    }

    Mutex lock(&resbMutex);

    // findFirstExisting() without fallbacks.
    UResourceDataEntry *r = init_entry(localeID, path, status);
    if(U_SUCCESS(*status)) {
        if(r->fBogus != U_ZERO_ERROR) {
            r->fCountExisting--;
            r = NULL;
        }
    } else {
        r = NULL;
    }

    // Some code depends on the ures_openDirect() bundle to have a parent bundle chain,
    // unless it is marked with "nofallback".
    UResourceDataEntry *t1 = r;
    if(r != NULL && uprv_strcmp(localeID, kRootLocaleName) != 0 &&  // not root
            r->fParent == NULL && !r->fData.noFallback &&
            uprv_strlen(localeID) < ULOC_FULLNAME_CAPACITY) {
        char name[ULOC_FULLNAME_CAPACITY];
        uprv_strcpy(name, localeID);
        if(!chopLocale(name) || uprv_strcmp(name, kRootLocaleName) == 0 ||
                loadParentsExceptRoot(t1, name, UPRV_LENGTHOF(name), FALSE, NULL, status)) {
            if(uprv_strcmp(t1->fName, kRootLocaleName) != 0 && t1->fParent == NULL) {
                insertRootBundle(t1, status);
            }
        }
        if(U_FAILURE(*status)) {
            r = NULL;
        }
    }

    if(r != NULL) {
        // TODO: Does this ever loop?
        while(t1->fParent != NULL) {
            t1->fParent->fCountExisting++;
            t1 = t1->fParent;
        }
    }
    return r;
}

/**
 * Functions to create and destroy resource bundles.
 *     CAUTION:  resbMutex must be locked when calling this function.
 */
/* INTERNAL: */
static void entryCloseInt(UResourceDataEntry *resB) {
    UResourceDataEntry *p = resB;

    while(resB != NULL) {
        p = resB->fParent;
        resB->fCountExisting--;

        /* Entries are left in the cache. TODO: add ures_flushCache() to force a flush
         of the cache. */
/*
        if(resB->fCountExisting <= 0) {
            uhash_remove(cache, resB);
            if(resB->fBogus == U_ZERO_ERROR) {
                res_unload(&(resB->fData));
            }
            if(resB->fName != NULL) {
                uprv_free(resB->fName);
            }
            if(resB->fPath != NULL) {
                uprv_free(resB->fPath);
            }
            uprv_free(resB);
        }
*/

        resB = p;
    }
}

/** 
 *  API: closes a resource bundle and cleans up.
 */

static void entryClose(UResourceDataEntry *resB) {
  Mutex lock(&resbMutex);
  entryCloseInt(resB);
}

/*
U_CFUNC void ures_setResPath(UResourceBundle *resB, const char* toAdd) {
  if(resB->fResPath == NULL) {
    resB->fResPath = resB->fResBuf;
    *(resB->fResPath) = 0;
  } 
  resB->fResPathLen = uprv_strlen(toAdd);
  if(RES_BUFSIZE <= resB->fResPathLen+1) {
    if(resB->fResPath == resB->fResBuf) {
      resB->fResPath = (char *)uprv_malloc((resB->fResPathLen+1)*sizeof(char));
    } else {
      resB->fResPath = (char *)uprv_realloc(resB->fResPath, (resB->fResPathLen+1)*sizeof(char));
    }
  }
  uprv_strcpy(resB->fResPath, toAdd);
}
*/
static void ures_appendResPath(UResourceBundle *resB, const char* toAdd, int32_t lenToAdd, UErrorCode *status) {
    int32_t resPathLenOrig = resB->fResPathLen;
    if(resB->fResPath == NULL) {
        resB->fResPath = resB->fResBuf;
        *(resB->fResPath) = 0;
        resB->fResPathLen = 0;
    } 
    resB->fResPathLen += lenToAdd;
    if(RES_BUFSIZE <= resB->fResPathLen+1) {
        if(resB->fResPath == resB->fResBuf) {
            resB->fResPath = (char *)uprv_malloc((resB->fResPathLen+1)*sizeof(char));
            /* Check that memory was allocated correctly. */
            if (resB->fResPath == NULL) {
                *status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            uprv_strcpy(resB->fResPath, resB->fResBuf);
        } else {
            char *temp = (char *)uprv_realloc(resB->fResPath, (resB->fResPathLen+1)*sizeof(char));
            /* Check that memory was reallocated correctly. */
            if (temp == NULL) {
                *status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            resB->fResPath = temp;
        }
    }
    uprv_strcpy(resB->fResPath + resPathLenOrig, toAdd);
}

static void ures_freeResPath(UResourceBundle *resB) {
    if (resB->fResPath && resB->fResPath != resB->fResBuf) {
        uprv_free(resB->fResPath);
    }
    resB->fResPath = NULL;
    resB->fResPathLen = 0;
}

static void
ures_closeBundle(UResourceBundle* resB, UBool freeBundleObj)
{
    if(resB != NULL) {
        if(resB->fData != NULL) {
            entryClose(resB->fData);
        }
        if(resB->fVersion != NULL) {
            uprv_free(resB->fVersion);
        }
        ures_freeResPath(resB);

        if(ures_isStackObject(resB) == FALSE && freeBundleObj) {
            uprv_free(resB);
        }
#if 0 /*U_DEBUG*/
        else {
            /* poison the data */
            uprv_memset(resB, -1, sizeof(UResourceBundle));
        }
#endif
    }
}

U_CAPI void  U_EXPORT2
ures_close(UResourceBundle* resB)
{
    ures_closeBundle(resB, TRUE);
}

namespace {

UResourceBundle *init_resb_result(
        UResourceDataEntry *dataEntry, Resource r, const char *key, int32_t idx,
        UResourceDataEntry *validLocaleDataEntry, const char *containerResPath,
        int32_t recursionDepth,
        UResourceBundle *resB, UErrorCode *status);

// TODO: Try to refactor further, so that we output a dataEntry + Resource + (optionally) resPath,
// rather than a UResourceBundle.
// May need to entryIncrease() the resulting dataEntry.
UResourceBundle *getAliasTargetAsResourceBundle(
        const ResourceData &resData, Resource r, const char *key, int32_t idx,
        UResourceDataEntry *validLocaleDataEntry, const char *containerResPath,
        int32_t recursionDepth,
        UResourceBundle *resB, UErrorCode *status) {
    // TODO: When an error occurs: Should we return nullptr vs. resB?
    if (U_FAILURE(*status)) { return resB; }
    U_ASSERT(RES_GET_TYPE(r) == URES_ALIAS);
    int32_t len = 0;
    const UChar *alias = res_getAlias(&resData, r, &len);
    if(len <= 0) {
        // bad alias
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return resB;
    }

    // Copy the UTF-16 alias string into an invariant-character string.
    //
    // We do this so that res_findResource() can modify the path,
    // which allows us to remove redundant _res_findResource() variants
    // in uresdata.c.
    // res_findResource() now NUL-terminates each segment so that table keys
    // can always be compared with strcmp() instead of strncmp().
    // Saves code there and simplifies testing and code coverage.
    //
    // markus 2003oct17
    CharString chAlias;
    chAlias.appendInvariantChars(alias, len, *status);
    if (U_FAILURE(*status)) {
        return nullptr;
    }

    // We have an alias, now let's cut it up.
    const char *path = nullptr, *locale = nullptr, *keyPath = nullptr;
    if(chAlias[0] == RES_PATH_SEPARATOR) {
        // There is a path included.
        char *chAliasData = chAlias.data();
        char *sep = chAliasData + 1;
        path = sep;
        sep = uprv_strchr(sep, RES_PATH_SEPARATOR);
        if(sep != nullptr) {
            *sep++ = 0;
        }
        if(uprv_strcmp(path, "LOCALE") == 0) {
            // This is an XPath alias, starting with "/LOCALE/".
            // It contains the path to a resource which should be looked up
            // starting in the valid locale.
            // TODO: Can/should we forbid a /LOCALE alias without key path?
            //   It seems weird to alias to the same path, just starting from the valid locale.
            //   That will often yield an infinite loop.
            keyPath = sep;
            // Read from the valid locale which we already have.
            path = locale = nullptr;
        } else {
            if(uprv_strcmp(path, "ICUDATA") == 0) { /* want ICU data */
                path = nullptr;
            }
            if (sep == nullptr) {
                // TODO: This ends up using the root bundle. Can/should we forbid this?
                locale = "";
            } else {
                locale = sep;
                sep = uprv_strchr(sep, RES_PATH_SEPARATOR);
                if(sep != nullptr) {
                    *sep++ = 0;
                }
                keyPath = sep;
            }
        }
    } else {
        // No path, start with a locale.
        char *sep = chAlias.data();
        locale = sep;
        sep = uprv_strchr(sep, RES_PATH_SEPARATOR);
        if(sep != nullptr) {
            *sep++ = 0;
        }
        keyPath = sep;
        path = validLocaleDataEntry->fPath;
    }

    // Got almost everything, let's try to open.
    // First, open the bundle with real data.
    LocalUResourceBundlePointer mainRes;
    UResourceDataEntry *dataEntry;
    if (locale == nullptr) {
        // alias = /LOCALE/keyPath
        // Read from the valid locale which we already have.
        dataEntry = validLocaleDataEntry;
    } else {
        UErrorCode intStatus = U_ZERO_ERROR;
        // TODO: Shouldn't we use ures_open() for locale data bundles (!noFallback)?
        mainRes.adoptInstead(ures_openDirect(path, locale, &intStatus));
        if(U_FAILURE(intStatus)) {
            // We failed to open the resource bundle we're aliasing to.
            *status = intStatus;
            return resB;
        }
        dataEntry = mainRes->fData;
    }

    const char* temp = nullptr;
    if(keyPath == nullptr) {
        // No key path. This means that we are going to to use the corresponding resource from
        // another bundle.
        // TODO: Why the special code path?
        //   Why not put together a key path from containerResPath + key or idx,
        //   as a comment below suggests, and go into the regular code branch?
        // First, we are going to get a corresponding container
        // resource to the one we are searching.
        r = dataEntry->fData.rootRes;
        if(containerResPath) {
            chAlias.clear().append(containerResPath, *status);
            if (U_FAILURE(*status)) {
                return nullptr;
            }
            char *aKey = chAlias.data();
            // TODO: should res_findResource() return a new dataEntry, too?
            r = res_findResource(&dataEntry->fData, r, &aKey, &temp);
        }
        if(key) {
            // We need to make keyPath from the containerResPath and
            // current key, if there is a key associated.
            chAlias.clear().append(key, *status);
            if (U_FAILURE(*status)) {
                return nullptr;
            }
            char *aKey = chAlias.data();
            r = res_findResource(&dataEntry->fData, r, &aKey, &temp);
        } else if(idx != -1) {
            // If there is no key, but there is an index, try to get by the index.
            // Here we have either a table or an array, so get the element.
            int32_t type = RES_GET_TYPE(r);
            if(URES_IS_TABLE(type)) {
                const char *aKey;
                r = res_getTableItemByIndex(&dataEntry->fData, r, idx, &aKey);
            } else { /* array */
                r = res_getArrayItem(&dataEntry->fData, r, idx);
            }
        }
        if(r != RES_BOGUS) {
            resB = init_resb_result(
                dataEntry, r, temp, -1, validLocaleDataEntry, nullptr, recursionDepth+1,
                resB, status);
        } else {
            *status = U_MISSING_RESOURCE_ERROR;
        }
    } else {
        // This one is a bit trickier.
        // We start finding keys, but after we resolve one alias, the path might continue.
        // Consider:
        //     aliastest:alias { "testtypes/anotheralias/Sequence" }
        //     anotheralias:alias { "/ICUDATA/sh/CollationElements" }
        // aliastest resource should finally have the sequence, not collation elements.
        CharString pathBuf(keyPath, *status);
        if (U_FAILURE(*status)) {
            return nullptr;
        }
        char *myPath = pathBuf.data();
        containerResPath = nullptr;
        // Now we have fallback following here.
        for(;;) {
            r = dataEntry->fData.rootRes;
            // TODO: Move  containerResPath = nullptr  to here,
            // consistent with restarting from the rootRes of another bundle?!

            // This loop handles 'found' resources over several levels.
            while(*myPath && U_SUCCESS(*status)) {
                r = res_findResource(&(dataEntry->fData), r, &myPath, &temp);
                if(r == RES_BOGUS) {
                    // No resource found, we don't really want to look anymore on this level.
                    break;
                }
                // Found a resource, but it might be an indirection.
                resB = init_resb_result(
                    dataEntry, r, temp, -1,
                    validLocaleDataEntry, containerResPath, recursionDepth+1,
                    resB, status);
                if (U_FAILURE(*status)) {
                    break;
                }
                if (temp == nullptr || uprv_strcmp(keyPath, temp) != 0) {
                    // The call to init_resb_result() above will set resB->fKeyPath to be
                    // the same as resB->fKey,
                    // throwing away any additional path elements if we had them --
                    // if the key path wasn't just a single resource ID, clear out
                    // the bundle's key path and re-set it to be equal to keyPath.
                    ures_freeResPath(resB);
                    ures_appendResPath(resB, keyPath, (int32_t)uprv_strlen(keyPath), status);
                    if(resB->fResPath[resB->fResPathLen-1] != RES_PATH_SEPARATOR) {
                        ures_appendResPath(resB, RES_PATH_SEPARATOR_S, 1, status);
                    }
                    if (U_FAILURE(*status)) {
                        break;
                    }
                }
                r = resB->fRes; /* switch to a new resource, possibly a new tree */
                dataEntry = resB->fData;
                containerResPath = resB->fResPath;
            }
            if (U_FAILURE(*status) || r != RES_BOGUS) {
                break;
            }
            // Fall back to the parent bundle, if there is one.
            dataEntry = dataEntry->fParent;
            if (dataEntry == nullptr) {
                *status = U_MISSING_RESOURCE_ERROR;
                break;
            }
            // Copy the same keyPath again.
            myPath = pathBuf.data();
            uprv_strcpy(myPath, keyPath);
        }
    }
    if(mainRes.getAlias() == resB) {
        mainRes.orphan();
    }
    ResourceTracer(resB).maybeTrace("getalias");
    return resB;
}

// Recursive function, should be called only by itself, by its simpler wrapper,
// or by getAliasTargetAsResourceBundle().
UResourceBundle *init_resb_result(
        UResourceDataEntry *dataEntry, Resource r, const char *key, int32_t idx,
        UResourceDataEntry *validLocaleDataEntry, const char *containerResPath,
        int32_t recursionDepth,
        UResourceBundle *resB, UErrorCode *status) {
    // TODO: When an error occurs: Should we return nullptr vs. resB?
    if(status == NULL || U_FAILURE(*status)) {
        return resB;
    }
    if (validLocaleDataEntry == nullptr) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    if(RES_GET_TYPE(r) == URES_ALIAS) {
        // This is an alias, need to exchange with real data.
        if(recursionDepth >= URES_MAX_ALIAS_LEVEL) {
            *status = U_TOO_MANY_ALIASES_ERROR;
            return resB;
        }
        return getAliasTargetAsResourceBundle(
            dataEntry->fData, r, key, idx,
            validLocaleDataEntry, containerResPath, recursionDepth, resB, status);
    }
    if(resB == NULL) {
        resB = (UResourceBundle *)uprv_malloc(sizeof(UResourceBundle));
        if (resB == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        ures_setIsStackObject(resB, FALSE);
        resB->fResPath = NULL;
        resB->fResPathLen = 0;
    } else {
        if(resB->fData != NULL) {
            entryClose(resB->fData);
        }
        if(resB->fVersion != NULL) {
            uprv_free(resB->fVersion);
        }
        /* 
        weiv: if stack object was passed in, it doesn't really need to be reinited,
        since the purpose of initing is to remove stack junk. However, at this point 
        we would not do anything to an allocated object, so stack object should be
        treated the same
        */
        /*
        if(ures_isStackObject(resB) != FALSE) {
        ures_initStackObject(resB);
        }
        */
        if(containerResPath != resB->fResPath) {
            ures_freeResPath(resB);
        }
    }
    resB->fData = dataEntry;
    entryIncrease(resB->fData);
    resB->fHasFallback = FALSE;
    resB->fIsTopLevel = FALSE;
    resB->fIndex = -1;
    resB->fKey = key; 
    resB->fValidLocaleDataEntry = validLocaleDataEntry;
    if(containerResPath != resB->fResPath) {
        ures_appendResPath(
            resB, containerResPath, static_cast<int32_t>(uprv_strlen(containerResPath)), status);
    }
    if(key != NULL) {
        ures_appendResPath(resB, key, (int32_t)uprv_strlen(key), status);
        if(resB->fResPath[resB->fResPathLen-1] != RES_PATH_SEPARATOR) {
            ures_appendResPath(resB, RES_PATH_SEPARATOR_S, 1, status);
        }
    } else if(idx >= 0) {
        char buf[256];
        int32_t len = T_CString_integerToString(buf, idx, 10);
        ures_appendResPath(resB, buf, len, status);
        if(resB->fResPath[resB->fResPathLen-1] != RES_PATH_SEPARATOR) {
            ures_appendResPath(resB, RES_PATH_SEPARATOR_S, 1, status);
        }
    }
    /* Make sure that Purify doesn't complain about uninitialized memory copies. */
    {
        int32_t usedLen = ((resB->fResBuf == resB->fResPath) ? resB->fResPathLen : 0);
        uprv_memset(resB->fResBuf + usedLen, 0, sizeof(resB->fResBuf) - usedLen);
    }

    resB->fVersion = NULL;
    resB->fRes = r;
    resB->fSize = res_countArrayItems(&resB->getResData(), resB->fRes);
    ResourceTracer(resB).trace("get");
    return resB;
}

UResourceBundle *init_resb_result(
        UResourceDataEntry *dataEntry, Resource r, const char *key, int32_t idx,
        // validLocaleDataEntry + containerResPath
        const UResourceBundle *container,
        UResourceBundle *resB, UErrorCode *status) {
    return init_resb_result(
        dataEntry, r, key, idx,
        container->fValidLocaleDataEntry, container->fResPath, 0, resB, status);
}

}  // namespace

UResourceBundle *ures_copyResb(UResourceBundle *r, const UResourceBundle *original, UErrorCode *status) {
    UBool isStackObject;
    if(U_FAILURE(*status) || r == original) {
        return r;
    }
    if(original != NULL) {
        if(r == NULL) {
            isStackObject = FALSE;
            r = (UResourceBundle *)uprv_malloc(sizeof(UResourceBundle));
            /* test for NULL */
            if (r == NULL) {
                *status = U_MEMORY_ALLOCATION_ERROR;
                return NULL;
            }
        } else {
            isStackObject = ures_isStackObject(r);
            ures_closeBundle(r, FALSE);
        }
        uprv_memcpy(r, original, sizeof(UResourceBundle));
        r->fResPath = NULL;
        r->fResPathLen = 0;
        if(original->fResPath) {
            ures_appendResPath(r, original->fResPath, original->fResPathLen, status);
        }
        ures_setIsStackObject(r, isStackObject);
        if(r->fData != NULL) {
            entryIncrease(r->fData);
        }
    }
    return r;
}

/**
 * Functions to retrieve data from resource bundles.
 */

U_CAPI const UChar* U_EXPORT2 ures_getString(const UResourceBundle* resB, int32_t* len, UErrorCode* status) {
    const UChar *s;
    if (status==NULL || U_FAILURE(*status)) {
        return NULL;
    }
    if(resB == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    s = res_getString({resB}, &resB->getResData(), resB->fRes, len);
    if (s == NULL) {
        *status = U_RESOURCE_TYPE_MISMATCH;
    }
    return s;
}

static const char *
ures_toUTF8String(const UChar *s16, int32_t length16,
                  char *dest, int32_t *pLength,
                  UBool forceCopy,
                  UErrorCode *status) {
    int32_t capacity;

    if (U_FAILURE(*status)) {
        return NULL;
    }
    if (pLength != NULL) {
        capacity = *pLength;
    } else {
        capacity = 0;
    }
    if (capacity < 0 || (capacity > 0 && dest == NULL)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if (length16 == 0) {
        /* empty string, return as read-only pointer */
        if (pLength != NULL) {
            *pLength = 0;
        }
        if (forceCopy) {
            u_terminateChars(dest, capacity, 0, status);
            return dest;
        } else {
            return "";
        }
    } else {
        /* We need to transform the string to the destination buffer. */
        if (capacity < length16) {
            /* No chance for the string to fit. Pure preflighting. */
            return u_strToUTF8(NULL, 0, pLength, s16, length16, status);
        }
        if (!forceCopy && (length16 <= 0x2aaaaaaa)) {
            /*
             * We know the string will fit into dest because each UChar turns
             * into at most three UTF-8 bytes. Fill the latter part of dest
             * so that callers do not expect to use dest as a string pointer,
             * hopefully leading to more robust code for when resource bundles
             * may store UTF-8 natively.
             * (In which case dest would not be used at all.)
             *
             * We do not do this if forceCopy=TRUE because then the caller
             * expects the string to start exactly at dest.
             *
             * The test above for <= 0x2aaaaaaa prevents overflows.
             * The +1 is for the NUL terminator.
             */
            int32_t maxLength = 3 * length16 + 1;
            if (capacity > maxLength) {
                dest += capacity - maxLength;
                capacity = maxLength;
            }
        }
        return u_strToUTF8(dest, capacity, pLength, s16, length16, status);
    }
}

U_CAPI const char * U_EXPORT2
ures_getUTF8String(const UResourceBundle *resB,
                   char *dest, int32_t *pLength,
                   UBool forceCopy,
                   UErrorCode *status) {
    int32_t length16;
    const UChar *s16 = ures_getString(resB, &length16, status);
    return ures_toUTF8String(s16, length16, dest, pLength, forceCopy, status);
}

U_CAPI const uint8_t* U_EXPORT2 ures_getBinary(const UResourceBundle* resB, int32_t* len, 
                                               UErrorCode*               status) {
  const uint8_t *p;
  if (status==NULL || U_FAILURE(*status)) {
    return NULL;
  }
  if(resB == NULL) {
    *status = U_ILLEGAL_ARGUMENT_ERROR;
    return NULL;
  }
  p = res_getBinary({resB}, &resB->getResData(), resB->fRes, len);
  if (p == NULL) {
    *status = U_RESOURCE_TYPE_MISMATCH;
  }
  return p;
}

U_CAPI const int32_t* U_EXPORT2 ures_getIntVector(const UResourceBundle* resB, int32_t* len, 
                                                   UErrorCode*               status) {
  const int32_t *p;
  if (status==NULL || U_FAILURE(*status)) {
    return NULL;
  }
  if(resB == NULL) {
    *status = U_ILLEGAL_ARGUMENT_ERROR;
    return NULL;
  }
  p = res_getIntVector({resB}, &resB->getResData(), resB->fRes, len);
  if (p == NULL) {
    *status = U_RESOURCE_TYPE_MISMATCH;
  }
  return p;
}

/* this function returns a signed integer */ 
/* it performs sign extension */
U_CAPI int32_t U_EXPORT2 ures_getInt(const UResourceBundle* resB, UErrorCode *status) {
  if (status==NULL || U_FAILURE(*status)) {
    return 0xffffffff;
  }
  if(resB == NULL) {
    *status = U_ILLEGAL_ARGUMENT_ERROR;
    return 0xffffffff;
  }
  if(RES_GET_TYPE(resB->fRes) != URES_INT) {
    *status = U_RESOURCE_TYPE_MISMATCH;
    return 0xffffffff;
  }
  return res_getInt({resB}, resB->fRes);
}

U_CAPI uint32_t U_EXPORT2 ures_getUInt(const UResourceBundle* resB, UErrorCode *status) {
  if (status==NULL || U_FAILURE(*status)) {
    return 0xffffffff;
  }
  if(resB == NULL) {
    *status = U_ILLEGAL_ARGUMENT_ERROR;
    return 0xffffffff;
  }
  if(RES_GET_TYPE(resB->fRes) != URES_INT) {
    *status = U_RESOURCE_TYPE_MISMATCH;
    return 0xffffffff;
  }
  return res_getUInt({resB}, resB->fRes);
}

U_CAPI UResType U_EXPORT2 ures_getType(const UResourceBundle *resB) {
  if(resB == NULL) {
    return URES_NONE;
  }
  return res_getPublicType(resB->fRes);
}

U_CAPI const char * U_EXPORT2 ures_getKey(const UResourceBundle *resB) {
  //
  // TODO: Trace ures_getKey? I guess not usually.
  //
  // We usually get the key string to decide whether we want the value, or to
  // make a key-value pair. Tracing the value should suffice.
  //
  // However, I believe we have some data (e.g., in res_index) where the key
  // strings are the data. Tracing the enclosing table should suffice.
  //
  if(resB == NULL) {
    return NULL;
  }
  return(resB->fKey);
}

U_CAPI int32_t U_EXPORT2 ures_getSize(const UResourceBundle *resB) {
  if(resB == NULL) {
    return 0;
  }
  
  return resB->fSize;
}

static const UChar* ures_getStringWithAlias(const UResourceBundle *resB, Resource r, int32_t sIndex, int32_t *len, UErrorCode *status) {
  if(RES_GET_TYPE(r) == URES_ALIAS) {
    const UChar* result = 0;
    UResourceBundle *tempRes = ures_getByIndex(resB, sIndex, NULL, status);
    result = ures_getString(tempRes, len, status);
    ures_close(tempRes);
    return result;
  } else {
    return res_getString({resB, sIndex}, &resB->getResData(), r, len); 
  }
}

U_CAPI void U_EXPORT2 ures_resetIterator(UResourceBundle *resB){
  if(resB == NULL) {
    return;
  }
  resB->fIndex = -1;
}

U_CAPI UBool U_EXPORT2 ures_hasNext(const UResourceBundle *resB) {
  if(resB == NULL) {
    return FALSE;
  }
  return (UBool)(resB->fIndex < resB->fSize-1);
}

U_CAPI const UChar* U_EXPORT2 ures_getNextString(UResourceBundle *resB, int32_t* len, const char ** key, UErrorCode *status) {
  Resource r = RES_BOGUS;
  
  if (status==NULL || U_FAILURE(*status)) {
    return NULL;
  }
  if(resB == NULL) {
    *status = U_ILLEGAL_ARGUMENT_ERROR;
    return NULL;
  }
  
  if(resB->fIndex == resB->fSize-1) {
    *status = U_INDEX_OUTOFBOUNDS_ERROR;
  } else {
    resB->fIndex++;
    switch(RES_GET_TYPE(resB->fRes)) {
    case URES_STRING:
    case URES_STRING_V2:
      return res_getString({resB}, &resB->getResData(), resB->fRes, len);
    case URES_TABLE:
    case URES_TABLE16:
    case URES_TABLE32:
      r = res_getTableItemByIndex(&resB->getResData(), resB->fRes, resB->fIndex, key);
      if(r == RES_BOGUS && resB->fHasFallback) {
        /* TODO: do the fallback */
      }
      return ures_getStringWithAlias(resB, r, resB->fIndex, len, status);
    case URES_ARRAY:
    case URES_ARRAY16:
      r = res_getArrayItem(&resB->getResData(), resB->fRes, resB->fIndex);
      if(r == RES_BOGUS && resB->fHasFallback) {
        /* TODO: do the fallback */
      }
      return ures_getStringWithAlias(resB, r, resB->fIndex, len, status);
    case URES_ALIAS:
      return ures_getStringWithAlias(resB, resB->fRes, resB->fIndex, len, status);
    case URES_INT:
    case URES_BINARY:
    case URES_INT_VECTOR:
        *status = U_RESOURCE_TYPE_MISMATCH;
        U_FALLTHROUGH;
    default:
      return NULL;
    }
  }

  return NULL;
}

U_CAPI UResourceBundle* U_EXPORT2 ures_getNextResource(UResourceBundle *resB, UResourceBundle *fillIn, UErrorCode *status) {
    const char *key = NULL;
    Resource r = RES_BOGUS;

    if (status==NULL || U_FAILURE(*status)) {
            /*return NULL;*/
            return fillIn;
    }
    if(resB == NULL) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            /*return NULL;*/
            return fillIn;
    }

    if(resB->fIndex == resB->fSize-1) {
      *status = U_INDEX_OUTOFBOUNDS_ERROR;
      /*return NULL;*/
    } else {
        resB->fIndex++;
        switch(RES_GET_TYPE(resB->fRes)) {
        case URES_INT:
        case URES_BINARY:
        case URES_STRING:
        case URES_STRING_V2:
        case URES_INT_VECTOR:
            return ures_copyResb(fillIn, resB, status);
        case URES_TABLE:
        case URES_TABLE16:
        case URES_TABLE32:
            r = res_getTableItemByIndex(&resB->getResData(), resB->fRes, resB->fIndex, &key);
            if(r == RES_BOGUS && resB->fHasFallback) {
                /* TODO: do the fallback */
            }
            return init_resb_result(resB->fData, r, key, resB->fIndex, resB, fillIn, status);
        case URES_ARRAY:
        case URES_ARRAY16:
            r = res_getArrayItem(&resB->getResData(), resB->fRes, resB->fIndex);
            if(r == RES_BOGUS && resB->fHasFallback) {
                /* TODO: do the fallback */
            }
            return init_resb_result(resB->fData, r, key, resB->fIndex, resB, fillIn, status);
        default:
            /*return NULL;*/
            return fillIn;
        }
    }
    /*return NULL;*/
    return fillIn;
}

U_CAPI UResourceBundle* U_EXPORT2 ures_getByIndex(const UResourceBundle *resB, int32_t indexR, UResourceBundle *fillIn, UErrorCode *status) {
    const char* key = NULL;
    Resource r = RES_BOGUS;

    if (status==NULL || U_FAILURE(*status)) {
        /*return NULL;*/
        return fillIn;
    }
    if(resB == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        /*return NULL;*/
        return fillIn;
    }

    if(indexR >= 0 && resB->fSize > indexR) {
        switch(RES_GET_TYPE(resB->fRes)) {
        case URES_INT:
        case URES_BINARY:
        case URES_STRING:
        case URES_STRING_V2:
        case URES_INT_VECTOR:
            return ures_copyResb(fillIn, resB, status);
        case URES_TABLE:
        case URES_TABLE16:
        case URES_TABLE32:
            r = res_getTableItemByIndex(&resB->getResData(), resB->fRes, indexR, &key);
            if(r == RES_BOGUS && resB->fHasFallback) {
                /* TODO: do the fallback */
            }
            return init_resb_result(resB->fData, r, key, indexR, resB, fillIn, status);
        case URES_ARRAY:
        case URES_ARRAY16:
            r = res_getArrayItem(&resB->getResData(), resB->fRes, indexR);
            if(r == RES_BOGUS && resB->fHasFallback) {
                /* TODO: do the fallback */
            }
            return init_resb_result(resB->fData, r, key, indexR, resB, fillIn, status);
        default:
            /*return NULL;*/
            return fillIn;
        }
    } else {
        *status = U_MISSING_RESOURCE_ERROR;
    }
    /*return NULL;*/
    return fillIn;
}

U_CAPI const UChar* U_EXPORT2 ures_getStringByIndex(const UResourceBundle *resB, int32_t indexS, int32_t* len, UErrorCode *status) {
    const char* key = NULL;
    Resource r = RES_BOGUS;

    if (status==NULL || U_FAILURE(*status)) {
        return NULL;
    }
    if(resB == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if(indexS >= 0 && resB->fSize > indexS) {
        switch(RES_GET_TYPE(resB->fRes)) {
        case URES_STRING:
        case URES_STRING_V2:
            return res_getString({resB}, &resB->getResData(), resB->fRes, len);
        case URES_TABLE:
        case URES_TABLE16:
        case URES_TABLE32:
            r = res_getTableItemByIndex(&resB->getResData(), resB->fRes, indexS, &key);
            if(r == RES_BOGUS && resB->fHasFallback) {
                /* TODO: do the fallback */
            }
            return ures_getStringWithAlias(resB, r, indexS, len, status);
        case URES_ARRAY:
        case URES_ARRAY16:
            r = res_getArrayItem(&resB->getResData(), resB->fRes, indexS);
            if(r == RES_BOGUS && resB->fHasFallback) {
                /* TODO: do the fallback */
            }
            return ures_getStringWithAlias(resB, r, indexS, len, status);
        case URES_ALIAS:
            return ures_getStringWithAlias(resB, resB->fRes, indexS, len, status);
        case URES_INT:
        case URES_BINARY:
        case URES_INT_VECTOR:
            *status = U_RESOURCE_TYPE_MISMATCH;
            break;
        default:
          /* must not occur */
          *status = U_INTERNAL_PROGRAM_ERROR;
          break;
        }
    } else {
        *status = U_MISSING_RESOURCE_ERROR;
    }
    return NULL;
}

U_CAPI const char * U_EXPORT2
ures_getUTF8StringByIndex(const UResourceBundle *resB,
                          int32_t idx,
                          char *dest, int32_t *pLength,
                          UBool forceCopy,
                          UErrorCode *status) {
    int32_t length16;
    const UChar *s16 = ures_getStringByIndex(resB, idx, &length16, status);
    return ures_toUTF8String(s16, length16, dest, pLength, forceCopy, status);
}

/*U_CAPI const char *ures_getResPath(UResourceBundle *resB) {
  return resB->fResPath;
}*/

U_CAPI UResourceBundle* U_EXPORT2
ures_findResource(const char* path, UResourceBundle *fillIn, UErrorCode *status) 
{
  UResourceBundle *first = NULL; 
  UResourceBundle *result = fillIn;
  char *packageName = NULL;
  char *pathToResource = NULL, *save = NULL;
  char *locale = NULL, *localeEnd = NULL;
  int32_t length;

  if(status == NULL || U_FAILURE(*status)) {
    return result;
  }

  length = (int32_t)(uprv_strlen(path)+1);
  save = pathToResource = (char *)uprv_malloc(length*sizeof(char));
  /* test for NULL */
  if(pathToResource == NULL) {
    *status = U_MEMORY_ALLOCATION_ERROR;
    return result;
  }
  uprv_memcpy(pathToResource, path, length);

  locale = pathToResource;
  if(*pathToResource == RES_PATH_SEPARATOR) { /* there is a path specification */
    pathToResource++;
    packageName = pathToResource;
    pathToResource = uprv_strchr(pathToResource, RES_PATH_SEPARATOR);
    if(pathToResource == NULL) {
      *status = U_ILLEGAL_ARGUMENT_ERROR;
    } else {
      *pathToResource = 0;
      locale = pathToResource+1;
    }
  }

  localeEnd = uprv_strchr(locale, RES_PATH_SEPARATOR);
  if(localeEnd != NULL) {
    *localeEnd = 0;
  }

  first = ures_open(packageName, locale, status);

  if(U_SUCCESS(*status)) {
    if(localeEnd) {
      result = ures_findSubResource(first, localeEnd+1, fillIn, status);
    } else {
      result = ures_copyResb(fillIn, first, status);
    }
    ures_close(first);
  }
  uprv_free(save);
  return result;
}

U_CAPI UResourceBundle* U_EXPORT2
ures_findSubResource(const UResourceBundle *resB, char* path, UResourceBundle *fillIn, UErrorCode *status) 
{
  Resource res = RES_BOGUS;
  UResourceBundle *result = fillIn;
  const char *key;

  if(status == NULL || U_FAILURE(*status)) {
    return result;
  }

  /* here we do looping and circular alias checking */
  /* this loop is here because aliasing is resolved on this level, not on res level */
  /* so, when we encounter an alias, it is not an aggregate resource, so we return */
  do {
    res = res_findResource(&resB->getResData(), resB->fRes, &path, &key); 
    if(res != RES_BOGUS) {
        result = init_resb_result(resB->fData, res, key, -1, resB, fillIn, status);
        resB = result;
    } else {
        *status = U_MISSING_RESOURCE_ERROR;
        break;
    }
  } while(*path); /* there is more stuff in the path */

  return result;
}
U_CAPI const UChar* U_EXPORT2 
ures_getStringByKeyWithFallback(const UResourceBundle *resB, 
                                const char* inKey, 
                                int32_t* len,
                                UErrorCode *status) {

    UResourceBundle stack;
    const UChar* retVal = NULL;
    ures_initStackObject(&stack);
    ures_getByKeyWithFallback(resB, inKey, &stack, status);
    int32_t length;
    retVal = ures_getString(&stack, &length, status);
    ures_close(&stack);
    if (U_FAILURE(*status)) {
        return NULL;
    }
    if (length == 3 && retVal[0] == EMPTY_SET && retVal[1] == EMPTY_SET && retVal[2] == EMPTY_SET ) {
        retVal = NULL;
        length = 0;
        *status = U_MISSING_RESOURCE_ERROR;
    }
    if (len != NULL) {
        *len = length;
    }
    return retVal;
}

/*
  Like res_getTableItemByKey but accepts full paths like "NumberElements/latn/patternsShort".
*/  
static Resource getTableItemByKeyPath(const ResourceData *pResData, Resource table, const char *key) {
  Resource resource = table;  /* The current resource */
  icu::CharString path;
  UErrorCode errorCode = U_ZERO_ERROR;
  path.append(key, errorCode);
  if (U_FAILURE(errorCode)) { return RES_BOGUS; }
  char *pathPart = path.data();  /* Path from current resource to desired resource */
  UResType type = (UResType)RES_GET_TYPE(resource);  /* the current resource type */
  while (*pathPart && resource != RES_BOGUS && URES_IS_CONTAINER(type)) {
    char *nextPathPart = uprv_strchr(pathPart, RES_PATH_SEPARATOR);
    if (nextPathPart != NULL) {
      *nextPathPart = 0;  /* Terminating null for this part of path. */
      nextPathPart++;
    } else {
      nextPathPart = uprv_strchr(pathPart, 0);
    }
    int32_t t;
    const char *pathP = pathPart;
    resource = res_getTableItemByKey(pResData, resource, &t, &pathP);
    type = (UResType)RES_GET_TYPE(resource);
    pathPart = nextPathPart; 
  }
  if (*pathPart) {
    return RES_BOGUS;
  }
  return resource;
}

static void createPath(const char* origResPath,
                       int32_t     origResPathLen,
                       const char* resPath,
                       int32_t     resPathLen,
                       const char* inKey,
                       CharString& path,
                       UErrorCode* status) {
    // This is a utility function used by ures_getByKeyWithFallback() below.  This function builds a path from
    // resPath and inKey, returning the result in `path`.  Originally, this function just cleared `path` and
    // appended resPath and inKey to it, but that caused problems for horizontal inheritance.
    //
    // In normal cases, resPath is the same as origResPath, but if ures_getByKeyWithFallback() has followed an
    // alias, resPath may be different from origResPath.  Not only may the existing path elements be different,
    // but resPath may also have MORE path elements than origResPath did.  If it does, those additional path
    // elements SUPERSEDE the corresponding elements of inKey.  So this code counts the number of elements in
    // resPath and origResPath and, for each path element in resPath that doesn't have a counterpart in origResPath,
    // deletes a path element from the beginning of inKey.  The remainder of inKey is then appended to
    // resPath to form the result.  (We're not using uprv_strchr() here because resPath and origResPath may
    // not be zero-terminated.)
    path.clear();
    const char* key = inKey;
    if (resPathLen > 0) {
        path.append(resPath, resPathLen, *status);
        if (U_SUCCESS(*status)) {
            const char* resPathLimit = resPath + resPathLen;
            const char* origResPathLimit = origResPath + origResPathLen;
            const char* resPathPtr = resPath;
            const char* origResPathPtr = origResPath;
            
            // Remove from the beginning of resPath the number of segments that are contained in origResPath.
            // If origResPath has MORE segments than resPath, this will leave resPath as the empty string.
            while (origResPathPtr < origResPathLimit && resPathPtr < resPathLimit) {
                while (origResPathPtr < origResPathLimit && *origResPathPtr != RES_PATH_SEPARATOR) {
                    ++origResPathPtr;
                }
                if (origResPathPtr < origResPathLimit && *origResPathPtr == RES_PATH_SEPARATOR) {
                    ++origResPathPtr;
                }
                while (resPathPtr < resPathLimit && *resPathPtr != RES_PATH_SEPARATOR) {
                    ++resPathPtr;
                }
                if (resPathPtr < resPathLimit && *resPathPtr == RES_PATH_SEPARATOR) {
                    ++resPathPtr;
                }
            }
            
            // New remove from the beginning of `key` the number of segments remaining in resPath.
            // If resPath has more segments than `key` does, `key` will end up empty.
            while (resPathPtr < resPathLimit && *key != '\0') {
                while (resPathPtr < resPathLimit && *resPathPtr != RES_PATH_SEPARATOR) {
                    ++resPathPtr;
                }
                if (resPathPtr < resPathLimit && *resPathPtr == RES_PATH_SEPARATOR) {
                    ++resPathPtr;
                }
                while (*key != '\0' && *key != RES_PATH_SEPARATOR) {
                    ++key;
                }
                if (*key == RES_PATH_SEPARATOR) {
                    ++key;
                }
            }
        }
        // Finally, append what's left of `key` to `path`.  What you end up with here is `resPath`, plus
        // any pieces of `key` that aren't superseded by `resPath`.
        // Or, to put it another way, calculate <#-segments-in-key> - (<#-segments-in-resPath> - <#-segments-in-origResPath>),
        // and append that many segments from the end of `key` to `resPath` to produce the result.
        path.append(key, *status);
    } else {
        path.append(inKey, *status);
    }
}

U_CAPI UResourceBundle* U_EXPORT2
ures_getByKeyWithFallback(const UResourceBundle *resB,
                          const char* inKey,
                          UResourceBundle *fillIn,
                          UErrorCode *status) {
    Resource res = RES_BOGUS, rootRes = RES_BOGUS;
    UResourceBundle *helper = NULL;

    if (status==NULL || U_FAILURE(*status)) {
        return fillIn;
    }
    if(resB == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return fillIn;
    }

    int32_t type = RES_GET_TYPE(resB->fRes);
    if(URES_IS_TABLE(type)) {
        const char* origResPath = resB->fResPath;
        int32_t origResPathLen = resB->fResPathLen;
        res = getTableItemByKeyPath(&resB->getResData(), resB->fRes, inKey);
        const char* key = inKey;
        bool didRootOnce = false;
        if(res == RES_BOGUS) {
            UResourceDataEntry *dataEntry = resB->fData;
            CharString path;
            char *myPath = NULL;
            const char* resPath = resB->fResPath;
            int32_t len = resB->fResPathLen;
            while(res == RES_BOGUS && (dataEntry->fParent != NULL || !didRootOnce)) { /* Otherwise, we'll look in parents */
                if (dataEntry->fParent != NULL) {
                    dataEntry = dataEntry->fParent;
                } else {
                    // We can't just stop when we get to a bundle whose fParent is NULL.  That'll work most of the time,
                    // but if the bundle that the caller passed to us was "root" (which happens in getAllItemsWithFallback(),
                    // this function will drop right out without doing anything if "root" doesn't contain the exact key path
                    // specified.  In that case, we need one extra time through this loop to make sure we follow any
                    // applicable aliases at the root level.
                    didRootOnce = true;
                }
                rootRes = dataEntry->fData.rootRes;

                if(dataEntry->fBogus == U_ZERO_ERROR) {
                    createPath(origResPath, origResPathLen, resPath, len, inKey, path, status);
                    if (U_FAILURE(*status)) {
                        ures_close(helper);
                        return fillIn;
                    }
                    myPath = path.data();
                    key = inKey;
                    do {
                        res = res_findResource(&(dataEntry->fData), rootRes, &myPath, &key);
                        if (RES_GET_TYPE(res) == URES_ALIAS && *myPath) {
                            /* We hit an alias, but we didn't finish following the path. */
                            helper = init_resb_result(dataEntry, res, NULL, -1, resB, helper, status);
                            /*helper = init_resb_result(dataEntry, res, inKey, -1, resB, helper, status);*/
                            if(helper) {
                              dataEntry = helper->fData;
                              rootRes = helper->fRes;
                              resPath = helper->fResPath;
                              len = helper->fResPathLen;

                            } else {
                              break;
                            }
                        } else if (res == RES_BOGUS) {
                            break;
                        }
                    } while(*myPath); /* Continue until the whole path is consumed */
                }
            }
            /*dataEntry = getFallbackData(resB, &key, &res, status);*/
            if(res != RES_BOGUS) {
              /* check if resB->fResPath gives the right name here */
                if(uprv_strcmp(dataEntry->fName, uloc_getDefault())==0 || uprv_strcmp(dataEntry->fName, kRootLocaleName)==0) {
                    *status = U_USING_DEFAULT_WARNING;
                } else {
                    *status = U_USING_FALLBACK_WARNING;
                }

                fillIn = init_resb_result(dataEntry, res, key, -1, resB, fillIn, status);
                if (resPath != nullptr) {
                    createPath(origResPath, origResPathLen, resPath, len, inKey, path, status);
                } else {
                    const char* separator = nullptr;
                    if (fillIn->fResPath != nullptr) {
                        separator = uprv_strchr(fillIn->fResPath, RES_PATH_SEPARATOR);
                    }
                    if (separator != nullptr && separator[1] != '\0') {
                        createPath(origResPath, origResPathLen, fillIn->fResPath,
                                   static_cast<int32_t>(uprv_strlen(fillIn->fResPath)), inKey, path, status);
                    } else {
                        createPath(origResPath, origResPathLen, "", 0, inKey, path, status);
                    }
                }
                ures_freeResPath(fillIn);
                ures_appendResPath(fillIn, path.data(), path.length(), status);
                if(fillIn->fResPath[fillIn->fResPathLen-1] != RES_PATH_SEPARATOR) {
                    ures_appendResPath(fillIn, RES_PATH_SEPARATOR_S, 1, status);
                }
            } else {
                *status = U_MISSING_RESOURCE_ERROR;
            }
        } else {
            fillIn = init_resb_result(resB->fData, res, key, -1, resB, fillIn, status);
        }
    } 
    else {
        *status = U_RESOURCE_TYPE_MISMATCH;
    }
    ures_close(helper);
    return fillIn;
}

namespace {

void getAllItemsWithFallback(
        const UResourceBundle *bundle, ResourceDataValue &value,
        ResourceSink &sink, UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return; }
    // We recursively enumerate child-first,
    // only storing parent items in the absence of child items.
    // The sink needs to store a placeholder value for the no-fallback/no-inheritance marker
    // to prevent a parent item from being stored.
    //
    // It would be possible to recursively enumerate parent-first,
    // overriding parent items with child items.
    // When the sink sees the no-fallback/no-inheritance marker,
    // then it would remove the parent's item.
    // We would deserialize parent values even though they are overridden in a child bundle.
    value.setData(bundle->getResData());
    value.setValidLocaleDataEntry(bundle->fValidLocaleDataEntry);
    UResourceDataEntry *parentEntry = bundle->fData->fParent;
    UBool hasParent = parentEntry != NULL && U_SUCCESS(parentEntry->fBogus);
    value.setResource(bundle->fRes, ResourceTracer(bundle));
    sink.put(bundle->fKey, value, !hasParent, errorCode);
    if (hasParent) {
        // We might try to query the sink whether
        // any fallback from the parent bundle is still possible.

        // Turn the parent UResourceDataEntry into a UResourceBundle,
        // much like in ures_openWithType().
        // TODO: See if we can refactor ures_getByKeyWithFallback()
        // and pull out an inner function that takes and returns a UResourceDataEntry
        // so that we need not create UResourceBundle objects.
        StackUResourceBundle parentBundle;
        UResourceBundle &parentRef = parentBundle.ref();
        parentRef.fData = parentEntry;
        parentRef.fValidLocaleDataEntry = bundle->fValidLocaleDataEntry;
        parentRef.fHasFallback = !parentRef.getResData().noFallback;
        parentRef.fIsTopLevel = TRUE;
        parentRef.fRes = parentRef.getResData().rootRes;
        parentRef.fSize = res_countArrayItems(&parentRef.getResData(), parentRef.fRes);
        parentRef.fIndex = -1;
        entryIncrease(parentEntry);

        // Look up the container item in the parent bundle.
        StackUResourceBundle containerBundle;
        const UResourceBundle *rb;
        UErrorCode pathErrorCode = U_ZERO_ERROR;  // Ignore if parents up to root do not have this path.
        if (bundle->fResPath == NULL || *bundle->fResPath == 0) {
            rb = parentBundle.getAlias();
        } else {
            rb = ures_getByKeyWithFallback(parentBundle.getAlias(), bundle->fResPath,
                                           containerBundle.getAlias(), &pathErrorCode);
        }
        if (U_SUCCESS(pathErrorCode)) {
            getAllItemsWithFallback(rb, value, sink, errorCode);
        }
    }
}

struct GetAllChildrenSink : public ResourceSink {
    // Destination sink
    ResourceSink& dest;

    GetAllChildrenSink(ResourceSink& dest)
        : dest(dest) {}
    virtual ~GetAllChildrenSink() override;
    virtual void put(const char *key, ResourceValue &value, UBool isRoot,
           UErrorCode &errorCode) override {
        ResourceTable itemsTable = value.getTable(errorCode);
        if (U_FAILURE(errorCode)) { return; }
        for (int32_t i = 0; itemsTable.getKeyAndValue(i, key, value); ++i) {
            if (value.getType() == URES_ALIAS) {
                ResourceDataValue& rdv = static_cast<ResourceDataValue&>(value);
                StackUResourceBundle stackTempBundle;
                UResourceBundle* aliasRB = getAliasTargetAsResourceBundle(rdv.getData(), rdv.getResource(), nullptr, -1,
                                                                          rdv.getValidLocaleDataEntry(), nullptr, 0,
                                                                          stackTempBundle.getAlias(), &errorCode);
                if (U_SUCCESS(errorCode)) {
                    ResourceDataValue aliasedValue;
                    aliasedValue.setData(aliasRB->getResData());
                    aliasedValue.setValidLocaleDataEntry(aliasRB->fValidLocaleDataEntry);
                    aliasedValue.setResource(aliasRB->fRes, ResourceTracer(aliasRB));
                    dest.put(key, aliasedValue, isRoot, errorCode);
                }
            } else {
                dest.put(key, value, isRoot, errorCode);
            }
            if (U_FAILURE(errorCode)) { return; }
        }
    }
};

// Virtual destructors must be defined out of line.
GetAllChildrenSink::~GetAllChildrenSink() {}

U_CAPI void U_EXPORT2
ures_getAllChildrenWithFallback(const UResourceBundle *bundle, const char *path,
                                icu::ResourceSink &sink, UErrorCode &errorCode) {
    GetAllChildrenSink allChildrenSink(sink);
    ures_getAllItemsWithFallback(bundle, path, allChildrenSink, errorCode);
}

}  // namespace

// Requires a ResourceDataValue fill-in, so that we need not cast from a ResourceValue.
// Unfortunately, the caller must know which subclass to make and pass in.
// Alternatively, we could make it as polymorphic as in Java by
// returning a ResourceValue pointer (possibly wrapped into a LocalPointer)
// that the caller then owns.
//
// Also requires a UResourceBundle fill-in, so that the value's ResourceTracer
// can point to a non-local bundle.
// Without tracing, the child bundle could be a function-local object.
U_CAPI void U_EXPORT2
ures_getValueWithFallback(const UResourceBundle *bundle, const char *path,
                          UResourceBundle *tempFillIn,
                          ResourceDataValue &value, UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return; }
    if (path == nullptr) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    const UResourceBundle *rb;
    if (*path == 0) {
        // empty path
        rb = bundle;
    } else {
        rb = ures_getByKeyWithFallback(bundle, path, tempFillIn, &errorCode);
        if (U_FAILURE(errorCode)) {
            return;
        }
    }
    value.setData(rb->getResData());
    value.setValidLocaleDataEntry(rb->fValidLocaleDataEntry);
    value.setResource(rb->fRes, ResourceTracer(rb));
}

U_CAPI void U_EXPORT2
ures_getAllItemsWithFallback(const UResourceBundle *bundle, const char *path,
                             icu::ResourceSink &sink, UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return; }
    if (path == nullptr) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    StackUResourceBundle stackBundle;
    const UResourceBundle *rb;
    if (*path == 0) {
        // empty path
        rb = bundle;
    } else {
        rb = ures_getByKeyWithFallback(bundle, path, stackBundle.getAlias(), &errorCode);
        if (U_FAILURE(errorCode)) {
            return;
        }
    }
    // Get all table items with fallback.
    ResourceDataValue value;
    getAllItemsWithFallback(rb, value, sink, errorCode);
}

U_CAPI UResourceBundle* U_EXPORT2 ures_getByKey(const UResourceBundle *resB, const char* inKey, UResourceBundle *fillIn, UErrorCode *status) {
    Resource res = RES_BOGUS;
    UResourceDataEntry *dataEntry = NULL;
    const char *key = inKey;

    if (status==NULL || U_FAILURE(*status)) {
        return fillIn;
    }
    if(resB == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return fillIn;
    }

    int32_t type = RES_GET_TYPE(resB->fRes);
    if(URES_IS_TABLE(type)) {
        int32_t t;
        res = res_getTableItemByKey(&resB->getResData(), resB->fRes, &t, &key);
        if(res == RES_BOGUS) {
            key = inKey;
            if(resB->fHasFallback == TRUE) {
                dataEntry = getFallbackData(resB, &key, &res, status);
                if(U_SUCCESS(*status)) {
                    /* check if resB->fResPath gives the right name here */
                    return init_resb_result(dataEntry, res, key, -1, resB, fillIn, status);
                } else {
                    *status = U_MISSING_RESOURCE_ERROR;
                }
            } else {
                *status = U_MISSING_RESOURCE_ERROR;
            }
        } else {
            return init_resb_result(resB->fData, res, key, -1, resB, fillIn, status);
        }
    } 
#if 0
    /* this is a kind of TODO item. If we have an array with an index table, we could do this. */
    /* not currently */
    else if(RES_GET_TYPE(resB->fRes) == URES_ARRAY && resB->fHasFallback == TRUE) {
        /* here should go a first attempt to locate the key using index table */
        dataEntry = getFallbackData(resB, &key, &res, status);
        if(U_SUCCESS(*status)) {
            return init_resb_result(dataEntry, res, key, resB, fillIn, status);
        } else {
            *status = U_MISSING_RESOURCE_ERROR;
        }
    }
#endif    
    else {
        *status = U_RESOURCE_TYPE_MISMATCH;
    }
    return fillIn;
}

U_CAPI const UChar* U_EXPORT2 ures_getStringByKey(const UResourceBundle *resB, const char* inKey, int32_t* len, UErrorCode *status) {
    Resource res = RES_BOGUS;
    UResourceDataEntry *dataEntry = NULL;
    const char* key = inKey;

    if (status==NULL || U_FAILURE(*status)) {
        return NULL;
    }
    if(resB == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    int32_t type = RES_GET_TYPE(resB->fRes);
    if(URES_IS_TABLE(type)) {
        int32_t t=0;

        res = res_getTableItemByKey(&resB->getResData(), resB->fRes, &t, &key);

        if(res == RES_BOGUS) {
            key = inKey;
            if(resB->fHasFallback == TRUE) {
                dataEntry = getFallbackData(resB, &key, &res, status);
                if(U_SUCCESS(*status)) {
                    switch (RES_GET_TYPE(res)) {
                    case URES_STRING:
                    case URES_STRING_V2:
                        return res_getString({resB, key}, &dataEntry->fData, res, len);
                    case URES_ALIAS:
                      {
                        const UChar* result = 0;
                        UResourceBundle *tempRes = ures_getByKey(resB, inKey, NULL, status);
                        result = ures_getString(tempRes, len, status);
                        ures_close(tempRes);
                        return result;
                      }
                    default:
                        *status = U_RESOURCE_TYPE_MISMATCH;
                    }
                } else {
                    *status = U_MISSING_RESOURCE_ERROR;
                }
            } else {
                *status = U_MISSING_RESOURCE_ERROR;
            }
        } else {
            switch (RES_GET_TYPE(res)) {
            case URES_STRING:
            case URES_STRING_V2:
                return res_getString({resB, key}, &resB->getResData(), res, len);
            case URES_ALIAS:
              {
                const UChar* result = 0;
                UResourceBundle *tempRes = ures_getByKey(resB, inKey, NULL, status);
                result = ures_getString(tempRes, len, status);
                ures_close(tempRes);
                return result;
              }
            default:
                *status = U_RESOURCE_TYPE_MISMATCH;
            }
        }
    } 
#if 0 
    /* this is a kind of TODO item. If we have an array with an index table, we could do this. */
    /* not currently */   
    else if(RES_GET_TYPE(resB->fRes) == URES_ARRAY && resB->fHasFallback == TRUE) {
        /* here should go a first attempt to locate the key using index table */
        dataEntry = getFallbackData(resB, &key, &res, status);
        if(U_SUCCESS(*status)) {
            // TODO: Tracing
            return res_getString(rd, res, len);
        } else {
            *status = U_MISSING_RESOURCE_ERROR;
        }
    } 
#endif    
    else {
        *status = U_RESOURCE_TYPE_MISMATCH;
    }
    return NULL;
}

U_CAPI const char * U_EXPORT2
ures_getUTF8StringByKey(const UResourceBundle *resB,
                        const char *key,
                        char *dest, int32_t *pLength,
                        UBool forceCopy,
                        UErrorCode *status) {
    int32_t length16;
    const UChar *s16 = ures_getStringByKey(resB, key, &length16, status);
    return ures_toUTF8String(s16, length16, dest, pLength, forceCopy, status);
}

/* TODO: clean from here down */

/**
 *  INTERNAL: Get the name of the first real locale (not placeholder) 
 *  that has resource bundle data.
 */
U_CAPI const char*  U_EXPORT2
ures_getLocaleInternal(const UResourceBundle* resourceBundle, UErrorCode* status)
{
    if (status==NULL || U_FAILURE(*status)) {
        return NULL;
    }
    if (!resourceBundle) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    } else {
      return resourceBundle->fData->fName;
    }
}

U_CAPI const char* U_EXPORT2 
ures_getLocale(const UResourceBundle* resourceBundle, 
               UErrorCode* status)
{
  return ures_getLocaleInternal(resourceBundle, status);
}


U_CAPI const char* U_EXPORT2 
ures_getLocaleByType(const UResourceBundle* resourceBundle, 
                     ULocDataLocaleType type, 
                     UErrorCode* status) {
    if (status==NULL || U_FAILURE(*status)) {
        return NULL;
    }
    if (!resourceBundle) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    } else {
        switch(type) {
        case ULOC_ACTUAL_LOCALE:
            return resourceBundle->fData->fName;
        case ULOC_VALID_LOCALE:
            return resourceBundle->fValidLocaleDataEntry->fName;
        case ULOC_REQUESTED_LOCALE:
        default:
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return NULL;
        }
    }
}

U_CFUNC const char* ures_getName(const UResourceBundle* resB) {
  if(resB == NULL) {
    return NULL;
  }

  return resB->fData->fName;
}

#ifdef URES_DEBUG
U_CFUNC const char* ures_getPath(const UResourceBundle* resB) {
  if(resB == NULL) {
    return NULL;
  }

  return resB->fData->fPath;
}
#endif

static UResourceBundle*
ures_openWithType(UResourceBundle *r, const char* path, const char* localeID,
                  UResOpenType openType, UErrorCode* status) {
    if(U_FAILURE(*status)) {
        return NULL;
    }

    UResourceDataEntry *entry;
    if(openType != URES_OPEN_DIRECT) {
        /* first "canonicalize" the locale ID */
        char canonLocaleID[ULOC_FULLNAME_CAPACITY];
        uloc_getBaseName(localeID, canonLocaleID, UPRV_LENGTHOF(canonLocaleID), status);
        if(U_FAILURE(*status) || *status == U_STRING_NOT_TERMINATED_WARNING) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return NULL;
        }
        entry = entryOpen(path, canonLocaleID, openType, status);
    } else {
        entry = entryOpenDirect(path, localeID, status);
    }
    if(U_FAILURE(*status)) {
        return NULL;
    }
    if(entry == NULL) {
        *status = U_MISSING_RESOURCE_ERROR;
        return NULL;
    }

    UBool isStackObject;
    if(r == NULL) {
        r = (UResourceBundle *)uprv_malloc(sizeof(UResourceBundle));
        if(r == NULL) {
            entryClose(entry);
            *status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        isStackObject = FALSE;
    } else {  // fill-in
        isStackObject = ures_isStackObject(r);
        ures_closeBundle(r, FALSE);
    }
    uprv_memset(r, 0, sizeof(UResourceBundle));
    ures_setIsStackObject(r, isStackObject);

    r->fValidLocaleDataEntry = r->fData = entry;
    r->fHasFallback = openType != URES_OPEN_DIRECT && !r->getResData().noFallback;
    r->fIsTopLevel = TRUE;
    r->fRes = r->getResData().rootRes;
    r->fSize = res_countArrayItems(&r->getResData(), r->fRes);
    r->fIndex = -1;

    ResourceTracer(r).traceOpen();

    return r;
}

U_CAPI UResourceBundle* U_EXPORT2
ures_open(const char* path, const char* localeID, UErrorCode* status) {
    return ures_openWithType(NULL, path, localeID, URES_OPEN_LOCALE_DEFAULT_ROOT, status);
}

U_CAPI UResourceBundle* U_EXPORT2
ures_openNoDefault(const char* path, const char* localeID, UErrorCode* status) {
    return ures_openWithType(NULL, path, localeID, URES_OPEN_LOCALE_ROOT, status);
}

/**
 *  Opens a resource bundle without "canonicalizing" the locale name. No fallback will be performed 
 *  or sought. However, alias substitution will happen!
 */
U_CAPI UResourceBundle*  U_EXPORT2
ures_openDirect(const char* path, const char* localeID, UErrorCode* status) {
    return ures_openWithType(NULL, path, localeID, URES_OPEN_DIRECT, status);
}

/**
 *  Internal API: This function is used to open a resource bundle 
 *  proper fallback chaining is executed while initialization. 
 *  The result is stored in cache for later fallback search.
 * 
 * Same as ures_open(), but uses the fill-in parameter and does not allocate a new bundle.
 */
U_CAPI void U_EXPORT2
ures_openFillIn(UResourceBundle *r, const char* path,
                const char* localeID, UErrorCode* status) {
    if(U_SUCCESS(*status) && r == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    ures_openWithType(r, path, localeID, URES_OPEN_LOCALE_DEFAULT_ROOT, status);
}

/**
 * Same as ures_openDirect(), but uses the fill-in parameter and does not allocate a new bundle.
 */
U_CAPI void U_EXPORT2
ures_openDirectFillIn(UResourceBundle *r, const char* path, const char* localeID, UErrorCode* status) {
    if(U_SUCCESS(*status) && r == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    ures_openWithType(r, path, localeID, URES_OPEN_DIRECT, status);
}

/**
 *  API: Counts members. For arrays and tables, returns number of resources.
 *  For strings, returns 1.
 */
U_CAPI int32_t  U_EXPORT2
ures_countArrayItems(const UResourceBundle* resourceBundle,
                  const char* resourceKey,
                  UErrorCode* status)
{
    UResourceBundle resData;
    ures_initStackObject(&resData);
    if (status==NULL || U_FAILURE(*status)) {
        return 0;
    }
    if(resourceBundle == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    ures_getByKey(resourceBundle, resourceKey, &resData, status);
    
    if(resData.getResData().data != NULL) {
        int32_t result = res_countArrayItems(&resData.getResData(), resData.fRes);
        ures_close(&resData);
        return result;
    } else {
        *status = U_MISSING_RESOURCE_ERROR;
        ures_close(&resData);
        return 0;
    }
}

/**
 * Internal function.
 * Return the version number associated with this ResourceBundle as a string.
 *
 * @param resourceBundle The resource bundle for which the version is checked.
 * @return  A version number string as specified in the resource bundle or its parent.
 *          The caller does not own this string.
 * @see ures_getVersion
 * @internal
 */
U_CAPI const char* U_EXPORT2 
ures_getVersionNumberInternal(const UResourceBundle *resourceBundle)
{
    if (!resourceBundle) return NULL;

    if(resourceBundle->fVersion == NULL) {

        /* If the version ID has not been built yet, then do so.  Retrieve */
        /* the minor version from the file. */
        UErrorCode status = U_ZERO_ERROR;
        int32_t minor_len = 0;
        int32_t len;

        const UChar* minor_version = ures_getStringByKey(resourceBundle, kVersionTag, &minor_len, &status);
        
        /* Determine the length of of the final version string.  This is */
        /* the length of the major part + the length of the separator */
        /* (==1) + the length of the minor part (+ 1 for the zero byte at */
        /* the end). */

        len = (minor_len > 0) ? minor_len : 1;
    
        /* Allocate the string, and build it up. */
        /* + 1 for zero byte */


        ((UResourceBundle *)resourceBundle)->fVersion = (char *)uprv_malloc(1 + len); 
        /* Check for null pointer. */
        if (((UResourceBundle *)resourceBundle)->fVersion == NULL) {
            return NULL;
        }
       
        if(minor_len > 0) {
            u_UCharsToChars(minor_version, resourceBundle->fVersion , minor_len);
            resourceBundle->fVersion[len] =  '\0';
        }
        else {
            uprv_strcpy(resourceBundle->fVersion, kDefaultMinorVersion);
        }
    }

    return resourceBundle->fVersion;
}

U_CAPI const char*  U_EXPORT2
ures_getVersionNumber(const UResourceBundle*   resourceBundle)
{
    return ures_getVersionNumberInternal(resourceBundle);
}

U_CAPI void U_EXPORT2 ures_getVersion(const UResourceBundle* resB, UVersionInfo versionInfo) {
    if (!resB) return;

    u_versionFromString(versionInfo, ures_getVersionNumberInternal(resB));
}

/** Tree support functions *******************************/
#define INDEX_LOCALE_NAME "res_index"
#define INDEX_TAG         "InstalledLocales"
#define DEFAULT_TAG       "default"

#if defined(URES_TREE_DEBUG)
#include <stdio.h>
#endif

typedef struct ULocalesContext {
    UResourceBundle installed;
    UResourceBundle curr;
} ULocalesContext;

static void U_CALLCONV
ures_loc_closeLocales(UEnumeration *enumerator) {
    ULocalesContext *ctx = (ULocalesContext *)enumerator->context;
    ures_close(&ctx->curr);
    ures_close(&ctx->installed);
    uprv_free(ctx);
    uprv_free(enumerator);
}

static int32_t U_CALLCONV
ures_loc_countLocales(UEnumeration *en, UErrorCode * /*status*/) {
    ULocalesContext *ctx = (ULocalesContext *)en->context;
    return ures_getSize(&ctx->installed);
}

U_CDECL_BEGIN


static const char * U_CALLCONV
ures_loc_nextLocale(UEnumeration* en,
                    int32_t* resultLength,
                    UErrorCode* status) {
    ULocalesContext *ctx = (ULocalesContext *)en->context;
    UResourceBundle *res = &(ctx->installed);
    UResourceBundle *k = NULL;
    const char *result = NULL;
    int32_t len = 0;
    if(ures_hasNext(res) && (k = ures_getNextResource(res, &ctx->curr, status)) != 0) {
        result = ures_getKey(k);
        len = (int32_t)uprv_strlen(result);
    }
    if (resultLength) {
        *resultLength = len;
    }
    return result;
}

static void U_CALLCONV 
ures_loc_resetLocales(UEnumeration* en, 
                      UErrorCode* /*status*/) {
    UResourceBundle *res = &((ULocalesContext *)en->context)->installed;
    ures_resetIterator(res);
}

U_CDECL_END

static const UEnumeration gLocalesEnum = {
    NULL,
        NULL,
        ures_loc_closeLocales,
        ures_loc_countLocales,
        uenum_unextDefault,
        ures_loc_nextLocale,
        ures_loc_resetLocales
};


U_CAPI UEnumeration* U_EXPORT2
ures_openAvailableLocales(const char *path, UErrorCode *status)
{
    UResourceBundle *idx = NULL;
    UEnumeration *en = NULL;
    ULocalesContext *myContext = NULL;

    if(U_FAILURE(*status)) {
        return NULL;
    }
    myContext = static_cast<ULocalesContext *>(uprv_malloc(sizeof(ULocalesContext)));
    en =  (UEnumeration *)uprv_malloc(sizeof(UEnumeration));
    if(!en || !myContext) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        uprv_free(en);
        uprv_free(myContext);
        return NULL;
    }
    uprv_memcpy(en, &gLocalesEnum, sizeof(UEnumeration));

    ures_initStackObject(&myContext->installed);
    ures_initStackObject(&myContext->curr);
    idx = ures_openDirect(path, INDEX_LOCALE_NAME, status);
    ures_getByKey(idx, INDEX_TAG, &myContext->installed, status);
    if(U_SUCCESS(*status)) {
#if defined(URES_TREE_DEBUG)
        fprintf(stderr, "Got %s::%s::[%s] : %s\n", 
            path, INDEX_LOCALE_NAME, INDEX_TAG, ures_getKey(&myContext->installed));
#endif
        en->context = myContext;
    } else {
#if defined(URES_TREE_DEBUG)
        fprintf(stderr, "%s open failed - %s\n", path, u_errorName(*status));
#endif
        ures_close(&myContext->installed);
        uprv_free(myContext);
        uprv_free(en);
        en = NULL;
    }
    
    ures_close(idx);
    
    return en;
}

static UBool isLocaleInList(UEnumeration *locEnum, const char *locToSearch, UErrorCode *status) {
    const char *loc;
    while ((loc = uenum_next(locEnum, NULL, status)) != NULL) {
        if (uprv_strcmp(loc, locToSearch) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

U_CAPI int32_t U_EXPORT2
ures_getFunctionalEquivalent(char *result, int32_t resultCapacity,
                             const char *path, const char *resName, const char *keyword, const char *locid,
                             UBool *isAvailable, UBool omitDefault, UErrorCode *status)
{
    char kwVal[1024] = ""; /* value of keyword 'keyword' */
    char defVal[1024] = ""; /* default value for given locale */
    char defLoc[1024] = ""; /* default value for given locale */
    char base[1024] = ""; /* base locale */
    char found[1024] = "";
    char parent[1024] = "";
    char full[1024] = "";
    UResourceBundle bund1, bund2;
    UResourceBundle *res = NULL;
    UErrorCode subStatus = U_ZERO_ERROR;
    int32_t length = 0;
    if(U_FAILURE(*status)) return 0;
    uloc_getKeywordValue(locid, keyword, kwVal, 1024-1,&subStatus);
    if(!uprv_strcmp(kwVal, DEFAULT_TAG)) {
        kwVal[0]=0;
    }
    uloc_getBaseName(locid, base, 1024-1,&subStatus);
#if defined(URES_TREE_DEBUG)
    fprintf(stderr, "getFunctionalEquivalent: \"%s\" [%s=%s] in %s - %s\n", 
            locid, keyword, kwVal, base, u_errorName(subStatus));
#endif
    ures_initStackObject(&bund1);
    ures_initStackObject(&bund2);
    
    
    uprv_strcpy(parent, base);
    uprv_strcpy(found, base);

    if(isAvailable) { 
        UEnumeration *locEnum = ures_openAvailableLocales(path, &subStatus);
        *isAvailable = TRUE;
        if (U_SUCCESS(subStatus)) {
            *isAvailable = isLocaleInList(locEnum, parent, &subStatus);
        }
        uenum_close(locEnum);
    }

    if(U_FAILURE(subStatus)) {
        *status = subStatus;
        return 0;
    }
    
    do {
        subStatus = U_ZERO_ERROR;
        res = ures_open(path, parent, &subStatus);
        if(((subStatus == U_USING_FALLBACK_WARNING) ||
            (subStatus == U_USING_DEFAULT_WARNING)) && isAvailable)
        {
            *isAvailable = FALSE;
        }
        isAvailable = NULL; /* only want to set this the first time around */
        
#if defined(URES_TREE_DEBUG)
        fprintf(stderr, "%s;%s -> %s [%s]\n", path?path:"ICUDATA", parent, u_errorName(subStatus), ures_getLocale(res, &subStatus));
#endif
        if(U_FAILURE(subStatus)) {
            *status = subStatus;
        } else if(subStatus == U_ZERO_ERROR) {
            ures_getByKey(res,resName,&bund1, &subStatus);
            if(subStatus == U_ZERO_ERROR) {
                const UChar *defUstr;
                int32_t defLen;
                /* look for default item */
#if defined(URES_TREE_DEBUG)
                fprintf(stderr, "%s;%s : loaded default -> %s\n",
                    path?path:"ICUDATA", parent, u_errorName(subStatus));
#endif
                defUstr = ures_getStringByKey(&bund1, DEFAULT_TAG, &defLen, &subStatus);
                if(U_SUCCESS(subStatus) && defLen) {
                    u_UCharsToChars(defUstr, defVal, u_strlen(defUstr));
#if defined(URES_TREE_DEBUG)
                    fprintf(stderr, "%s;%s -> default %s=%s,  %s\n", 
                        path?path:"ICUDATA", parent, keyword, defVal, u_errorName(subStatus));
#endif
                    uprv_strcpy(defLoc, parent);
                    if(kwVal[0]==0) {
                        uprv_strcpy(kwVal, defVal);
#if defined(URES_TREE_DEBUG)
                        fprintf(stderr, "%s;%s -> kwVal =  %s\n", 
                            path?path:"ICUDATA", parent, keyword, kwVal);
#endif
                    }
                }
            }
        }
        
        subStatus = U_ZERO_ERROR;

        if (res != NULL) {
            uprv_strcpy(found, ures_getLocaleByType(res, ULOC_VALID_LOCALE, &subStatus));
        }

        uloc_getParent(found,parent,sizeof(parent),&subStatus);
        ures_close(res);
    } while(!defVal[0] && *found && uprv_strcmp(found, "root") != 0 && U_SUCCESS(*status));
    
    /* Now, see if we can find the kwVal collator.. start the search over.. */
    uprv_strcpy(parent, base);
    uprv_strcpy(found, base);
    
    do {
        subStatus = U_ZERO_ERROR;
        res = ures_open(path, parent, &subStatus);
        if((subStatus == U_USING_FALLBACK_WARNING) && isAvailable) {
            *isAvailable = FALSE;
        }
        isAvailable = NULL; /* only want to set this the first time around */
        
#if defined(URES_TREE_DEBUG)
        fprintf(stderr, "%s;%s -> %s (looking for %s)\n", 
            path?path:"ICUDATA", parent, u_errorName(subStatus), kwVal);
#endif
        if(U_FAILURE(subStatus)) {
            *status = subStatus;
        } else if(subStatus == U_ZERO_ERROR) {
            ures_getByKey(res,resName,&bund1, &subStatus);
#if defined(URES_TREE_DEBUG)
/**/ fprintf(stderr,"@%d [%s] %s\n", __LINE__, resName, u_errorName(subStatus));
#endif
            if(subStatus == U_ZERO_ERROR) {
                ures_getByKey(&bund1, kwVal, &bund2, &subStatus);
#if defined(URES_TREE_DEBUG)
/**/ fprintf(stderr,"@%d [%s] %s\n", __LINE__, kwVal, u_errorName(subStatus));
#endif
                if(subStatus == U_ZERO_ERROR) {
#if defined(URES_TREE_DEBUG)
                    fprintf(stderr, "%s;%s -> full0 %s=%s,  %s\n", 
                        path?path:"ICUDATA", parent, keyword, kwVal, u_errorName(subStatus));
#endif
                    uprv_strcpy(full, parent);
                    if(*full == 0) {
                        uprv_strcpy(full, "root");
                    }
                        /* now, recalculate default kw if need be */
                        if(uprv_strlen(defLoc) > uprv_strlen(full)) {
                          const UChar *defUstr;
                          int32_t defLen;
                          /* look for default item */
#if defined(URES_TREE_DEBUG)
                            fprintf(stderr, "%s;%s -> recalculating Default0\n", 
                                    path?path:"ICUDATA", full);
#endif
                          defUstr = ures_getStringByKey(&bund1, DEFAULT_TAG, &defLen, &subStatus);
                          if(U_SUCCESS(subStatus) && defLen) {
                            u_UCharsToChars(defUstr, defVal, u_strlen(defUstr));
#if defined(URES_TREE_DEBUG)
                            fprintf(stderr, "%s;%s -> default0 %s=%s,  %s\n", 
                                    path?path:"ICUDATA", full, keyword, defVal, u_errorName(subStatus));
#endif
                            uprv_strcpy(defLoc, full);
                          }
                        } /* end of recalculate default KW */
#if defined(URES_TREE_DEBUG)
                        else {
                          fprintf(stderr, "No trim0,  %s <= %s\n", defLoc, full);
                        }
#endif
                } else {
#if defined(URES_TREE_DEBUG)
                    fprintf(stderr, "err=%s in %s looking for %s\n", 
                        u_errorName(subStatus), parent, kwVal);
#endif
                }
            }
        }
        
        subStatus = U_ZERO_ERROR;
        
        uprv_strcpy(found, parent);
        uloc_getParent(found,parent,1023,&subStatus);
        ures_close(res);
    } while(!full[0] && *found && U_SUCCESS(*status));
    
    if((full[0]==0) && uprv_strcmp(kwVal, defVal)) {
#if defined(URES_TREE_DEBUG)
        fprintf(stderr, "Failed to locate kw %s - try default %s\n", kwVal, defVal);
#endif
        uprv_strcpy(kwVal, defVal);
        uprv_strcpy(parent, base);
        uprv_strcpy(found, base);
        
        do { /* search for 'default' named item */
            subStatus = U_ZERO_ERROR;
            res = ures_open(path, parent, &subStatus);
            if((subStatus == U_USING_FALLBACK_WARNING) && isAvailable) {
                *isAvailable = FALSE;
            }
            isAvailable = NULL; /* only want to set this the first time around */
            
#if defined(URES_TREE_DEBUG)
            fprintf(stderr, "%s;%s -> %s (looking for default %s)\n",
                path?path:"ICUDATA", parent, u_errorName(subStatus), kwVal);
#endif
            if(U_FAILURE(subStatus)) {
                *status = subStatus;
            } else if(subStatus == U_ZERO_ERROR) {
                ures_getByKey(res,resName,&bund1, &subStatus);
                if(subStatus == U_ZERO_ERROR) {
                    ures_getByKey(&bund1, kwVal, &bund2, &subStatus);
                    if(subStatus == U_ZERO_ERROR) {
#if defined(URES_TREE_DEBUG)
                        fprintf(stderr, "%s;%s -> full1 %s=%s,  %s\n", path?path:"ICUDATA",
                            parent, keyword, kwVal, u_errorName(subStatus));
#endif
                        uprv_strcpy(full, parent);
                        if(*full == 0) {
                            uprv_strcpy(full, "root");
                        }
                        
                        /* now, recalculate default kw if need be */
                        if(uprv_strlen(defLoc) > uprv_strlen(full)) {
                          const UChar *defUstr;
                          int32_t defLen;
                          /* look for default item */
#if defined(URES_TREE_DEBUG)
                            fprintf(stderr, "%s;%s -> recalculating Default1\n", 
                                    path?path:"ICUDATA", full);
#endif
                          defUstr = ures_getStringByKey(&bund1, DEFAULT_TAG, &defLen, &subStatus);
                          if(U_SUCCESS(subStatus) && defLen) {
                            u_UCharsToChars(defUstr, defVal, u_strlen(defUstr));
#if defined(URES_TREE_DEBUG)
                            fprintf(stderr, "%s;%s -> default %s=%s,  %s\n", 
                                    path?path:"ICUDATA", full, keyword, defVal, u_errorName(subStatus));
#endif
                            uprv_strcpy(defLoc, full);
                          }
                        } /* end of recalculate default KW */
#if defined(URES_TREE_DEBUG)
                        else {
                          fprintf(stderr, "No trim1,  %s <= %s\n", defLoc, full);
                        }
#endif
                    }
                }
            }
            subStatus = U_ZERO_ERROR;
            
            uprv_strcpy(found, parent);
            uloc_getParent(found,parent,1023,&subStatus);
            ures_close(res);
        } while(!full[0] && *found && U_SUCCESS(*status));
    }
    
    if(U_SUCCESS(*status)) {
        if(!full[0]) {
#if defined(URES_TREE_DEBUG)
          fprintf(stderr, "Still could not load keyword %s=%s\n", keyword, kwVal);
#endif
          *status = U_MISSING_RESOURCE_ERROR;
        } else if(omitDefault) {
#if defined(URES_TREE_DEBUG)
          fprintf(stderr,"Trim? full=%s, defLoc=%s, found=%s\n", full, defLoc, found);
#endif        
          if(uprv_strlen(defLoc) <= uprv_strlen(full)) {
            /* found the keyword in a *child* of where the default tag was present. */
            if(!uprv_strcmp(kwVal, defVal)) { /* if the requested kw is default, */
              /* and the default is in or in an ancestor of the current locale */
#if defined(URES_TREE_DEBUG)
              fprintf(stderr, "Removing unneeded var %s=%s\n", keyword, kwVal);
#endif
              kwVal[0]=0;
            }
          }
        }
        uprv_strcpy(found, full);
        if(kwVal[0]) {
            uprv_strcat(found, "@");
            uprv_strcat(found, keyword);
            uprv_strcat(found, "=");
            uprv_strcat(found, kwVal);
        } else if(!omitDefault) {
            uprv_strcat(found, "@");
            uprv_strcat(found, keyword);
            uprv_strcat(found, "=");
            uprv_strcat(found, defVal);
        }
    }
    /* we found the default locale - no need to repeat it.*/
    
    ures_close(&bund1);
    ures_close(&bund2);
    
    length = (int32_t)uprv_strlen(found);

    if(U_SUCCESS(*status)) {
        int32_t copyLength = uprv_min(length, resultCapacity);
        if(copyLength>0) {
            uprv_strncpy(result, found, copyLength);
        }
        if(length == 0) {
          *status = U_MISSING_RESOURCE_ERROR; 
        }
    } else {
        length = 0;
        result[0]=0;
    }
    return u_terminateChars(result, resultCapacity, length, status);
}

U_CAPI UEnumeration* U_EXPORT2
ures_getKeywordValues(const char *path, const char *keyword, UErrorCode *status)
{
#define VALUES_BUF_SIZE 2048
#define VALUES_LIST_SIZE 512
    
    char       valuesBuf[VALUES_BUF_SIZE];
    int32_t    valuesIndex = 0;
    const char *valuesList[VALUES_LIST_SIZE];
    int32_t    valuesCount = 0;
    
    const char *locale;
    int32_t     locLen;
    
    UEnumeration *locs = NULL;
    
    UResourceBundle    item;
    UResourceBundle    subItem;
    
    ures_initStackObject(&item);
    ures_initStackObject(&subItem);
    locs = ures_openAvailableLocales(path, status);
    
    if(U_FAILURE(*status)) {
        ures_close(&item);
        ures_close(&subItem);
        return NULL;
    }
    
    valuesBuf[0]=0;
    valuesBuf[1]=0;
    
    while((locale = uenum_next(locs, &locLen, status)) != 0) {
        UResourceBundle   *bund = NULL;
        UResourceBundle   *subPtr = NULL;
        UErrorCode subStatus = U_ZERO_ERROR; /* don't fail if a bundle is unopenable */
        bund = ures_open(path, locale, &subStatus);
        
#if defined(URES_TREE_DEBUG)
        if(!bund || U_FAILURE(subStatus)) {
            fprintf(stderr, "%s-%s values: Can't open %s locale - skipping. (%s)\n", 
                path?path:"<ICUDATA>", keyword, locale, u_errorName(subStatus));
        }
#endif
        
        ures_getByKey(bund, keyword, &item, &subStatus);
        
        if(!bund || U_FAILURE(subStatus)) {
#if defined(URES_TREE_DEBUG)
            fprintf(stderr, "%s-%s values: Can't find in %s - skipping. (%s)\n", 
                path?path:"<ICUDATA>", keyword, locale, u_errorName(subStatus));
#endif
            ures_close(bund);
            bund = NULL;
            continue;
        }
        
        while((subPtr = ures_getNextResource(&item,&subItem,&subStatus)) != 0
            && U_SUCCESS(subStatus)) {
            const char *k;
            int32_t i;
            k = ures_getKey(subPtr);

#if defined(URES_TREE_DEBUG)
            /* fprintf(stderr, "%s | %s | %s | %s\n", path?path:"<ICUDATA>", keyword, locale, k); */
#endif
            if(k == NULL || *k == 0 ||
                    uprv_strcmp(k, DEFAULT_TAG) == 0 || uprv_strncmp(k, "private-", 8) == 0) {
                // empty or "default" or unlisted type
                continue;
            }
            for(i=0; i<valuesCount; i++) {
                if(!uprv_strcmp(valuesList[i],k)) {
                    k = NULL; /* found duplicate */
                    break;
                }
            }
            if(k != NULL) {
                int32_t kLen = (int32_t)uprv_strlen(k);
                if((valuesCount >= (VALUES_LIST_SIZE-1)) ||       /* no more space in list .. */
                    ((valuesIndex+kLen+1+1) >= VALUES_BUF_SIZE)) { /* no more space in buffer (string + 2 nulls) */
                    *status = U_ILLEGAL_ARGUMENT_ERROR; /* out of space.. */
                } else {
                    uprv_strcpy(valuesBuf+valuesIndex, k);
                    valuesList[valuesCount++] = valuesBuf+valuesIndex;
                    valuesIndex += kLen;
#if defined(URES_TREE_DEBUG)
                    fprintf(stderr, "%s | %s | %s | [%s]   (UNIQUE)\n",
                        path?path:"<ICUDATA>", keyword, locale, k);
#endif
                    valuesBuf[valuesIndex++] = 0; /* terminate */
                }
            }
        }
        ures_close(bund);
    }
    valuesBuf[valuesIndex++] = 0; /* terminate */
    
    ures_close(&item);
    ures_close(&subItem);
    uenum_close(locs);
#if defined(URES_TREE_DEBUG)
    fprintf(stderr, "%s:  size %d, #%d\n", u_errorName(*status), 
        valuesIndex, valuesCount);
#endif
    return uloc_openKeywordList(valuesBuf, valuesIndex, status);
}
#if 0
/* This code isn't needed, and given the documentation warnings the implementation is suspect */
U_CAPI UBool U_EXPORT2
ures_equal(const UResourceBundle* res1, const UResourceBundle* res2){
    if(res1==NULL || res2==NULL){
        return res1==res2; /* pointer comparison */
    }
    if(res1->fKey==NULL||  res2->fKey==NULL){
        return (res1->fKey==res2->fKey);
    }else{
        if(uprv_strcmp(res1->fKey, res2->fKey)!=0){
            return FALSE;
        }
    }
    if(uprv_strcmp(res1->fData->fName, res2->fData->fName)!=0){
        return FALSE;
    }
    if(res1->fData->fPath == NULL||  res2->fData->fPath==NULL){
        return (res1->fData->fPath == res2->fData->fPath);
    }else{
        if(uprv_strcmp(res1->fData->fPath, res2->fData->fPath)!=0){
            return FALSE;
        }
    }
    if(uprv_strcmp(res1->fData->fParent->fName, res2->fData->fParent->fName)!=0){
        return FALSE;
    }
    if(uprv_strcmp(res1->fData->fParent->fPath, res2->fData->fParent->fPath)!=0){
        return FALSE;
    }
    if(uprv_strncmp(res1->fResPath, res2->fResPath, res1->fResPathLen)!=0){
        return FALSE;
    }
    if(res1->fRes != res2->fRes){
        return FALSE;
    }
    return TRUE;
}
U_CAPI UResourceBundle* U_EXPORT2
ures_clone(const UResourceBundle* res, UErrorCode* status){
    UResourceBundle* bundle = NULL;
    UResourceBundle* ret = NULL;
    if(U_FAILURE(*status) || res == NULL){
        return NULL;
    }
    bundle = ures_open(res->fData->fPath, res->fData->fName, status);
    if(res->fResPath!=NULL){
        ret = ures_findSubResource(bundle, res->fResPath, NULL, status);
        ures_close(bundle);
    }else{
        ret = bundle;
    }
    return ret;
}
U_CAPI const UResourceBundle* U_EXPORT2
ures_getParentBundle(const UResourceBundle* res){
    if(res==NULL){
        return NULL;
    }
    return res->fParentRes;
}
#endif

U_CAPI void U_EXPORT2
ures_getVersionByKey(const UResourceBundle* res, const char *key, UVersionInfo ver, UErrorCode *status) {
  const UChar *str;
  int32_t len;
  str = ures_getStringByKey(res, key, &len, status);
  if(U_SUCCESS(*status)) {
    u_versionFromUString(ver, str);
  } 
}

/* eof */
