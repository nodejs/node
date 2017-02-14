// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2015-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* sharedobject.h
*/

#ifndef __SHAREDOBJECT_H__
#define __SHAREDOBJECT_H__


#include "unicode/uobject.h"
#include "umutex.h"

U_NAMESPACE_BEGIN

/**
 * Base class for unified cache exposing enough methods to SharedObject
 * instances to allow their addRef() and removeRef() methods to
 * update cache metrics. No other part of ICU, except for SharedObject,
 * should directly call the methods of this base class.
 */
class U_COMMON_API UnifiedCacheBase : public UObject {
public:
    UnifiedCacheBase() { }

    /**
     * Called by addRefWhileHoldingCacheLock() when the hard reference count
     * of its instance goes from 0 to 1.
     */
    virtual void incrementItemsInUse() const = 0;

    /**
     * Called by removeRef() when the hard reference count of its instance
     * drops from 1 to 0.
     */
    virtual void decrementItemsInUseWithLockingAndEviction() const = 0;

    /**
     * Called by removeRefWhileHoldingCacheLock() when the hard reference
     * count of its instance drops from 1 to 0.
     */
    virtual void decrementItemsInUse() const = 0;
    virtual ~UnifiedCacheBase();
private:
    UnifiedCacheBase(const UnifiedCacheBase &);
    UnifiedCacheBase &operator=(const UnifiedCacheBase &);
};

/**
 * Base class for shared, reference-counted, auto-deleted objects.
 * Subclasses can be immutable.
 * If they are mutable, then they must implement their copy constructor
 * so that copyOnWrite() works.
 *
 * Either stack-allocate, use LocalPointer, or use addRef()/removeRef().
 * Sharing requires reference-counting.
 */
class U_COMMON_API SharedObject : public UObject {
public:
    /** Initializes totalRefCount, softRefCount to 0. */
    SharedObject() :
            totalRefCount(0),
            softRefCount(0),
            hardRefCount(0),
            cachePtr(NULL) {}

    /** Initializes totalRefCount, softRefCount to 0. */
    SharedObject(const SharedObject &other) :
            UObject(other),
            totalRefCount(0),
            softRefCount(0),
            hardRefCount(0),
            cachePtr(NULL) {}

    virtual ~SharedObject();

    /**
     * Increments the number of references to this object. Thread-safe.
     */
    void addRef() const { addRef(FALSE); }

    /**
     * Increments the number of references to this object.
     * Must be called only from within the internals of UnifiedCache and
     * only while the cache global mutex is held.
     */
    void addRefWhileHoldingCacheLock() const { addRef(TRUE); }

    /**
     * Increments the number of soft references to this object.
     * Must be called only from within the internals of UnifiedCache and
     * only while the cache global mutex is held.
     */
    void addSoftRef() const;

    /**
     * Decrements the number of references to this object. Thread-safe.
     */
    void removeRef() const { removeRef(FALSE); }

    /**
     * Decrements the number of references to this object.
     * Must be called only from within the internals of UnifiedCache and
     * only while the cache global mutex is held.
     */
    void removeRefWhileHoldingCacheLock() const { removeRef(TRUE); }

    /**
     * Decrements the number of soft references to this object.
     * Must be called only from within the internals of UnifiedCache and
     * only while the cache global mutex is held.
     */
    void removeSoftRef() const;

    /**
     * Returns the reference counter including soft references.
     * Uses a memory barrier.
     */
    int32_t getRefCount() const;

    /**
     * Returns the count of soft references only.
     * Must be called only from within the internals of UnifiedCache and
     * only while the cache global mutex is held.
     */
    int32_t getSoftRefCount() const { return softRefCount; }

    /**
     * Returns the count of hard references only. Uses a memory barrier.
     * Used for testing the cache. Regular clients won't need this.
     */
    int32_t getHardRefCount() const;

    /**
     * If noHardReferences() == TRUE then this object has no hard references.
     * Must be called only from within the internals of UnifiedCache.
     */
    inline UBool noHardReferences() const { return getHardRefCount() == 0; }

    /**
     * If hasHardReferences() == TRUE then this object has hard references.
     * Must be called only from within the internals of UnifiedCache.
     */
    inline UBool hasHardReferences() const { return getHardRefCount() != 0; }

    /**
     * If noSoftReferences() == TRUE then this object has no soft references.
     * Must be called only from within the internals of UnifiedCache and
     * only while the cache global mutex is held.
     */
    UBool noSoftReferences() const { return (softRefCount == 0); }

    /**
     * Deletes this object if it has no references or soft references.
     */
    void deleteIfZeroRefCount() const;

    /**
     * @internal For UnifedCache use only to register this object with itself.
     *   Must be called before this object is exposed to multiple threads.
     */
    void registerWithCache(const UnifiedCacheBase *ptr) const {
        cachePtr = ptr;
    }

    /**
     * Returns a writable version of ptr.
     * If there is exactly one owner, then ptr itself is returned as a
     *  non-const pointer.
     * If there are multiple owners, then ptr is replaced with a
     * copy-constructed clone,
     * and that is returned.
     * Returns NULL if cloning failed.
     *
     * T must be a subclass of SharedObject.
     */
    template<typename T>
    static T *copyOnWrite(const T *&ptr) {
        const T *p = ptr;
        if(p->getRefCount() <= 1) { return const_cast<T *>(p); }
        T *p2 = new T(*p);
        if(p2 == NULL) { return NULL; }
        p->removeRef();
        ptr = p2;
        p2->addRef();
        return p2;
    }

    /**
     * Makes dest an owner of the object pointed to by src while adjusting
     * reference counts and deleting the previous object dest pointed to
     * if necessary. Before this call is made, dest must either be NULL or
     * be included in the reference count of the object it points to.
     *
     * T must be a subclass of SharedObject.
     */
    template<typename T>
    static void copyPtr(const T *src, const T *&dest) {
        if(src != dest) {
            if(dest != NULL) { dest->removeRef(); }
            dest = src;
            if(src != NULL) { src->addRef(); }
        }
    }

    /**
     * Equivalent to copyPtr(NULL, dest).
     */
    template<typename T>
    static void clearPtr(const T *&ptr) {
        if (ptr != NULL) {
            ptr->removeRef();
            ptr = NULL;
        }
    }

private:
    mutable u_atomic_int32_t totalRefCount;

    // Any thread modifying softRefCount must hold the global cache mutex
    mutable int32_t softRefCount;

    mutable u_atomic_int32_t hardRefCount;
    mutable const UnifiedCacheBase *cachePtr;
    void addRef(UBool withCacheLock) const;
    void removeRef(UBool withCacheLock) const;

};

U_NAMESPACE_END

#endif
