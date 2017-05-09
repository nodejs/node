// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*
* File UNIFIEDCACHE.CPP
******************************************************************************
*/

#include "uhash.h"
#include "unifiedcache.h"
#include "umutex.h"
#include "mutex.h"
#include "uassert.h"
#include "ucln_cmn.h"

static icu::UnifiedCache *gCache = NULL;
static icu::SharedObject *gNoValue = NULL;
static UMutex gCacheMutex = U_MUTEX_INITIALIZER;
static UConditionVar gInProgressValueAddedCond = U_CONDITION_INITIALIZER;
static icu::UInitOnce gCacheInitOnce = U_INITONCE_INITIALIZER;
static const int32_t MAX_EVICT_ITERATIONS = 10;

static int32_t DEFAULT_MAX_UNUSED = 1000;
static int32_t DEFAULT_PERCENTAGE_OF_IN_USE = 100;


U_CDECL_BEGIN
static UBool U_CALLCONV unifiedcache_cleanup() {
    gCacheInitOnce.reset();
    if (gCache) {
        delete gCache;
        gCache = NULL;
    }
    if (gNoValue) {
        delete gNoValue;
        gNoValue = NULL;
    }
    return TRUE;
}
U_CDECL_END


U_NAMESPACE_BEGIN

U_CAPI int32_t U_EXPORT2
ucache_hashKeys(const UHashTok key) {
    const CacheKeyBase *ckey = (const CacheKeyBase *) key.pointer;
    return ckey->hashCode();
}

U_CAPI UBool U_EXPORT2
ucache_compareKeys(const UHashTok key1, const UHashTok key2) {
    const CacheKeyBase *p1 = (const CacheKeyBase *) key1.pointer;
    const CacheKeyBase *p2 = (const CacheKeyBase *) key2.pointer;
    return *p1 == *p2;
}

U_CAPI void U_EXPORT2
ucache_deleteKey(void *obj) {
    CacheKeyBase *p = (CacheKeyBase *) obj;
    delete p;
}

CacheKeyBase::~CacheKeyBase() {
}

static void U_CALLCONV cacheInit(UErrorCode &status) {
    U_ASSERT(gCache == NULL);
    ucln_common_registerCleanup(
            UCLN_COMMON_UNIFIED_CACHE, unifiedcache_cleanup);

    // gNoValue must be created first to avoid assertion error in
    // cache constructor.
    gNoValue = new SharedObject();
    gCache = new UnifiedCache(status);
    if (gCache == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(status)) {
        delete gCache;
        delete gNoValue;
        gCache = NULL;
        gNoValue = NULL;
        return;
    }
    // We add a softref because we want hash elements with gNoValue to be
    // elligible for purging but we don't ever want gNoValue to be deleted.
    gNoValue->addSoftRef();
}

UnifiedCache *UnifiedCache::getInstance(UErrorCode &status) {
    umtx_initOnce(gCacheInitOnce, &cacheInit, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    U_ASSERT(gCache != NULL);
    return gCache;
}

UnifiedCache::UnifiedCache(UErrorCode &status) :
        fHashtable(NULL),
        fEvictPos(UHASH_FIRST),
        fItemsInUseCount(0),
        fMaxUnused(DEFAULT_MAX_UNUSED),
        fMaxPercentageOfInUse(DEFAULT_PERCENTAGE_OF_IN_USE),
        fAutoEvictedCount(0) {
    if (U_FAILURE(status)) {
        return;
    }
    U_ASSERT(gNoValue != NULL);
    fHashtable = uhash_open(
            &ucache_hashKeys,
            &ucache_compareKeys,
            NULL,
            &status);
    if (U_FAILURE(status)) {
        return;
    }
    uhash_setKeyDeleter(fHashtable, &ucache_deleteKey);
}

void UnifiedCache::setEvictionPolicy(
        int32_t count, int32_t percentageOfInUseItems, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if (count < 0 || percentageOfInUseItems < 0) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    Mutex lock(&gCacheMutex);
    fMaxUnused = count;
    fMaxPercentageOfInUse = percentageOfInUseItems;
}

int32_t UnifiedCache::unusedCount() const {
    Mutex lock(&gCacheMutex);
    return uhash_count(fHashtable) - fItemsInUseCount;
}

int64_t UnifiedCache::autoEvictedCount() const {
    Mutex lock(&gCacheMutex);
    return fAutoEvictedCount;
}

int32_t UnifiedCache::keyCount() const {
    Mutex lock(&gCacheMutex);
    return uhash_count(fHashtable);
}

void UnifiedCache::flush() const {
    Mutex lock(&gCacheMutex);

    // Use a loop in case cache items that are flushed held hard references to
    // other cache items making those additional cache items eligible for
    // flushing.
    while (_flush(FALSE));
}

#ifdef UNIFIED_CACHE_DEBUG
#include <stdio.h>

void UnifiedCache::dump() {
    UErrorCode status = U_ZERO_ERROR;
    const UnifiedCache *cache = getInstance(status);
    if (U_FAILURE(status)) {
        fprintf(stderr, "Unified Cache: Error fetching cache.\n");
        return;
    }
    cache->dumpContents();
}

void UnifiedCache::dumpContents() const {
    Mutex lock(&gCacheMutex);
    _dumpContents();
}

// Dumps content of cache.
// On entry, gCacheMutex must be held.
// On exit, cache contents dumped to stderr.
void UnifiedCache::_dumpContents() const {
    int32_t pos = UHASH_FIRST;
    const UHashElement *element = uhash_nextElement(fHashtable, &pos);
    char buffer[256];
    int32_t cnt = 0;
    for (; element != NULL; element = uhash_nextElement(fHashtable, &pos)) {
        const SharedObject *sharedObject =
                (const SharedObject *) element->value.pointer;
        const CacheKeyBase *key =
                (const CacheKeyBase *) element->key.pointer;
        if (sharedObject->hasHardReferences()) {
            ++cnt;
            fprintf(
                    stderr,
                    "Unified Cache: Key '%s', error %d, value %p, total refcount %d, soft refcount %d\n",
                    key->writeDescription(buffer, 256),
                    key->creationStatus,
                    sharedObject == gNoValue ? NULL :sharedObject,
                    sharedObject->getRefCount(),
                    sharedObject->getSoftRefCount());
        }
    }
    fprintf(stderr, "Unified Cache: %d out of a total of %d still have hard references\n", cnt, uhash_count(fHashtable));
}
#endif

UnifiedCache::~UnifiedCache() {
    // Try our best to clean up first.
    flush();
    {
        // Now all that should be left in the cache are entries that refer to
        // each other and entries with hard references from outside the cache.
        // Nothing we can do about these so proceed to wipe out the cache.
        Mutex lock(&gCacheMutex);
        _flush(TRUE);
    }
    uhash_close(fHashtable);
}

// Returns the next element in the cache round robin style.
// On entry, gCacheMutex must be held.
const UHashElement *
UnifiedCache::_nextElement() const {
    const UHashElement *element = uhash_nextElement(fHashtable, &fEvictPos);
    if (element == NULL) {
        fEvictPos = UHASH_FIRST;
        return uhash_nextElement(fHashtable, &fEvictPos);
    }
    return element;
}

// Flushes the contents of the cache. If cache values hold references to other
// cache values then _flush should be called in a loop until it returns FALSE.
// On entry, gCacheMutex must be held.
// On exit, those values with are evictable are flushed. If all is true
// then every value is flushed even if it is not evictable.
// Returns TRUE if any value in cache was flushed or FALSE otherwise.
UBool UnifiedCache::_flush(UBool all) const {
    UBool result = FALSE;
    int32_t origSize = uhash_count(fHashtable);
    for (int32_t i = 0; i < origSize; ++i) {
        const UHashElement *element = _nextElement();
        if (all || _isEvictable(element)) {
            const SharedObject *sharedObject =
                    (const SharedObject *) element->value.pointer;
            uhash_removeElement(fHashtable, element);
            sharedObject->removeSoftRef();
            result = TRUE;
        }
    }
    return result;
}

// Computes how many items should be evicted.
// On entry, gCacheMutex must be held.
// Returns number of items that should be evicted or a value <= 0 if no
// items need to be evicted.
int32_t UnifiedCache::_computeCountOfItemsToEvict() const {
    int32_t maxPercentageOfInUseCount =
            fItemsInUseCount * fMaxPercentageOfInUse / 100;
    int32_t maxUnusedCount = fMaxUnused;
    if (maxUnusedCount < maxPercentageOfInUseCount) {
        maxUnusedCount = maxPercentageOfInUseCount;
    }
    return uhash_count(fHashtable) - fItemsInUseCount - maxUnusedCount;
}

// Run an eviction slice.
// On entry, gCacheMutex must be held.
// _runEvictionSlice runs a slice of the evict pipeline by examining the next
// 10 entries in the cache round robin style evicting them if they are eligible.
void UnifiedCache::_runEvictionSlice() const {
    int32_t maxItemsToEvict = _computeCountOfItemsToEvict();
    if (maxItemsToEvict <= 0) {
        return;
    }
    for (int32_t i = 0; i < MAX_EVICT_ITERATIONS; ++i) {
        const UHashElement *element = _nextElement();
        if (_isEvictable(element)) {
            const SharedObject *sharedObject =
                    (const SharedObject *) element->value.pointer;
            uhash_removeElement(fHashtable, element);
            sharedObject->removeSoftRef();
            ++fAutoEvictedCount;
            if (--maxItemsToEvict == 0) {
                break;
            }
        }
    }
}


// Places a new value and creationStatus in the cache for the given key.
// On entry, gCacheMutex must be held. key must not exist in the cache.
// On exit, value and creation status placed under key. Soft reference added
// to value on successful add. On error sets status.
void UnifiedCache::_putNew(
        const CacheKeyBase &key,
        const SharedObject *value,
        const UErrorCode creationStatus,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return;
    }
    CacheKeyBase *keyToAdopt = key.clone();
    if (keyToAdopt == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    keyToAdopt->fCreationStatus = creationStatus;
    if (value->noSoftReferences()) {
        _registerMaster(keyToAdopt, value);
    }
    uhash_put(fHashtable, keyToAdopt, (void *) value, &status);
    if (U_SUCCESS(status)) {
        value->addSoftRef();
    }
}

// Places value and status at key if there is no value at key or if cache
// entry for key is in progress. Otherwise, it leaves the current value and
// status there.
// On entry. gCacheMutex must not be held. value must be
// included in the reference count of the object to which it points.
// On exit, value and status are changed to what was already in the cache if
// something was there and not in progress. Otherwise, value and status are left
// unchanged in which case they are placed in the cache on a best-effort basis.
// Caller must call removeRef() on value.
void UnifiedCache::_putIfAbsentAndGet(
        const CacheKeyBase &key,
        const SharedObject *&value,
        UErrorCode &status) const {
    Mutex lock(&gCacheMutex);
    const UHashElement *element = uhash_find(fHashtable, &key);
    if (element != NULL && !_inProgress(element)) {
        _fetch(element, value, status);
        return;
    }
    if (element == NULL) {
        UErrorCode putError = U_ZERO_ERROR;
        // best-effort basis only.
        _putNew(key, value, status, putError);
    } else {
        _put(element, value, status);
    }
    // Run an eviction slice. This will run even if we added a master entry
    // which doesn't increase the unused count, but that is still o.k
    _runEvictionSlice();
}

// Attempts to fetch value and status for key from cache.
// On entry, gCacheMutex must not be held value must be NULL and status must
// be U_ZERO_ERROR.
// On exit, either returns FALSE (In this
// case caller should try to create the object) or returns TRUE with value
// pointing to the fetched value and status set to fetched status. When
// FALSE is returned status may be set to failure if an in progress hash
// entry could not be made but value will remain unchanged. When TRUE is
// returned, caler must call removeRef() on value.
UBool UnifiedCache::_poll(
        const CacheKeyBase &key,
        const SharedObject *&value,
        UErrorCode &status) const {
    U_ASSERT(value == NULL);
    U_ASSERT(status == U_ZERO_ERROR);
    Mutex lock(&gCacheMutex);
    const UHashElement *element = uhash_find(fHashtable, &key);
    while (element != NULL && _inProgress(element)) {
        umtx_condWait(&gInProgressValueAddedCond, &gCacheMutex);
        element = uhash_find(fHashtable, &key);
    }
    if (element != NULL) {
        _fetch(element, value, status);
        return TRUE;
    }
    _putNew(key, gNoValue, U_ZERO_ERROR, status);
    return FALSE;
}

// Gets value out of cache.
// On entry. gCacheMutex must not be held. value must be NULL. status
// must be U_ZERO_ERROR.
// On exit. value and status set to what is in cache at key or on cache
// miss the key's createObject() is called and value and status are set to
// the result of that. In this latter case, best effort is made to add the
// value and status to the cache. If createObject() fails to create a value,
// gNoValue is stored in cache, and value is set to NULL. Caller must call
// removeRef on value if non NULL.
void UnifiedCache::_get(
        const CacheKeyBase &key,
        const SharedObject *&value,
        const void *creationContext,
        UErrorCode &status) const {
    U_ASSERT(value == NULL);
    U_ASSERT(status == U_ZERO_ERROR);
    if (_poll(key, value, status)) {
        if (value == gNoValue) {
            SharedObject::clearPtr(value);
        }
        return;
    }
    if (U_FAILURE(status)) {
        return;
    }
    value = key.createObject(creationContext, status);
    U_ASSERT(value == NULL || value->hasHardReferences());
    U_ASSERT(value != NULL || status != U_ZERO_ERROR);
    if (value == NULL) {
        SharedObject::copyPtr(gNoValue, value);
    }
    _putIfAbsentAndGet(key, value, status);
    if (value == gNoValue) {
        SharedObject::clearPtr(value);
    }
}

void UnifiedCache::decrementItemsInUseWithLockingAndEviction() const {
    Mutex mutex(&gCacheMutex);
    decrementItemsInUse();
    _runEvictionSlice();
}

void UnifiedCache::incrementItemsInUse() const {
    ++fItemsInUseCount;
}

void UnifiedCache::decrementItemsInUse() const {
    --fItemsInUseCount;
}

// Register a master cache entry.
// On entry, gCacheMutex must be held.
// On exit, items in use count incremented, entry is marked as a master
// entry, and value registered with cache so that subsequent calls to
// addRef() and removeRef() on it correctly updates items in use count
void UnifiedCache::_registerMaster(
        const CacheKeyBase *theKey, const SharedObject *value) const {
    theKey->fIsMaster = TRUE;
    ++fItemsInUseCount;
    value->registerWithCache(this);
}

// Store a value and error in given hash entry.
// On entry, gCacheMutex must be held. Hash entry element must be in progress.
// value must be non NULL.
// On Exit, soft reference added to value. value and status stored in hash
// entry. Soft reference removed from previous stored value. Waiting
// threads notified.
void UnifiedCache::_put(
        const UHashElement *element,
        const SharedObject *value,
        const UErrorCode status) const {
    U_ASSERT(_inProgress(element));
    const CacheKeyBase *theKey = (const CacheKeyBase *) element->key.pointer;
    const SharedObject *oldValue = (const SharedObject *) element->value.pointer;
    theKey->fCreationStatus = status;
    if (value->noSoftReferences()) {
        _registerMaster(theKey, value);
    }
    value->addSoftRef();
    UHashElement *ptr = const_cast<UHashElement *>(element);
    ptr->value.pointer = (void *) value;
    oldValue->removeSoftRef();

    // Tell waiting threads that we replace in-progress status with
    // an error.
    umtx_condBroadcast(&gInProgressValueAddedCond);
}

void
UnifiedCache::copyPtr(const SharedObject *src, const SharedObject *&dest) {
    if(src != dest) {
        if(dest != NULL) {
            dest->removeRefWhileHoldingCacheLock();
        }
        dest = src;
        if(src != NULL) {
            src->addRefWhileHoldingCacheLock();
        }
    }
}

void
UnifiedCache::clearPtr(const SharedObject *&ptr) {
    if (ptr != NULL) {
        ptr->removeRefWhileHoldingCacheLock();
        ptr = NULL;
    }
}


// Fetch value and error code from a particular hash entry.
// On entry, gCacheMutex must be held. value must be either NULL or must be
// included in the ref count of the object to which it points.
// On exit, value and status set to what is in the hash entry. Caller must
// eventually call removeRef on value.
// If hash entry is in progress, value will be set to gNoValue and status will
// be set to U_ZERO_ERROR.
void UnifiedCache::_fetch(
        const UHashElement *element,
        const SharedObject *&value,
        UErrorCode &status) {
    const CacheKeyBase *theKey = (const CacheKeyBase *) element->key.pointer;
    status = theKey->fCreationStatus;

    // Since we have the cache lock, calling regular SharedObject methods
    // could cause us to deadlock on ourselves since they may need to lock
    // the cache mutex.
    UnifiedCache::copyPtr((const SharedObject *) element->value.pointer, value);
}

// Determine if given hash entry is in progress.
// On entry, gCacheMutex must be held.
UBool UnifiedCache::_inProgress(const UHashElement *element) {
    const SharedObject *value = NULL;
    UErrorCode status = U_ZERO_ERROR;
    _fetch(element, value, status);
    UBool result = _inProgress(value, status);

    // Since we have the cache lock, calling regular SharedObject methods
    // could cause us to deadlock on ourselves since they may need to lock
    // the cache mutex.
    UnifiedCache::clearPtr(value);
    return result;
}

// Determine if given hash entry is in progress.
// On entry, gCacheMutex must be held.
UBool UnifiedCache::_inProgress(
        const SharedObject *theValue, UErrorCode creationStatus) {
    return (theValue == gNoValue && creationStatus == U_ZERO_ERROR);
}

// Determine if given hash entry is eligible for eviction.
// On entry, gCacheMutex must be held.
UBool UnifiedCache::_isEvictable(const UHashElement *element) {
    const CacheKeyBase *theKey = (const CacheKeyBase *) element->key.pointer;
    const SharedObject *theValue =
            (const SharedObject *) element->value.pointer;

    // Entries that are under construction are never evictable
    if (_inProgress(theValue, theKey->fCreationStatus)) {
        return FALSE;
    }

    // We can evict entries that are either not a master or have just
    // one reference (The one reference being from the cache itself).
    return (!theKey->fIsMaster || (theValue->getSoftRefCount() == 1 && theValue->noHardReferences()));
}

U_NAMESPACE_END
