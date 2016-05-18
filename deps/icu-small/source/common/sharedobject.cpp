/*
******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* sharedobject.cpp
*/
#include "sharedobject.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

SharedObject::~SharedObject() {}

UnifiedCacheBase::~UnifiedCacheBase() {}

void
SharedObject::addRef(UBool fromWithinCache) const {
    umtx_atomic_inc(&totalRefCount);

    // Although items in use may not be correct immediately, it
    // will be correct eventually.
    if (umtx_atomic_inc(&hardRefCount) == 1 && cachePtr != NULL) {
        // If this object is cached, and the hardRefCount goes from 0 to 1,
        // then the increment must happen from within the cache while the
        // cache global mutex is locked. In this way, we can be rest assured
        // that data races can't happen if the cache performs some task if
        // the hardRefCount is zero while the global cache mutex is locked.
        (void)fromWithinCache;   // Suppress unused variable warning in non-debug builds.
        U_ASSERT(fromWithinCache);
        cachePtr->incrementItemsInUse();
    }
}

void
SharedObject::removeRef(UBool fromWithinCache) const {
    UBool decrementItemsInUse = (umtx_atomic_dec(&hardRefCount) == 0);
    UBool allReferencesGone = (umtx_atomic_dec(&totalRefCount) == 0);

    // Although items in use may not be correct immediately, it
    // will be correct eventually.
    if (decrementItemsInUse && cachePtr != NULL) {
        if (fromWithinCache) {
            cachePtr->decrementItemsInUse();
        } else {
            cachePtr->decrementItemsInUseWithLockingAndEviction();
        }
    }
    if (allReferencesGone) {
        delete this;
    }
}

void
SharedObject::addSoftRef() const {
    umtx_atomic_inc(&totalRefCount);
    ++softRefCount;
}

void
SharedObject::removeSoftRef() const {
    --softRefCount;
    if (umtx_atomic_dec(&totalRefCount) == 0) {
        delete this;
    }
}

int32_t
SharedObject::getRefCount() const {
    return umtx_loadAcquire(totalRefCount);
}

int32_t
SharedObject::getHardRefCount() const {
    return umtx_loadAcquire(hardRefCount);
}

void
SharedObject::deleteIfZeroRefCount() const {
    if(getRefCount() == 0) {
        delete this;
    }
}

U_NAMESPACE_END
