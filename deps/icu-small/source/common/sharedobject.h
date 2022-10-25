// © 2016 and later: Unicode, Inc. and others.
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

class SharedObject;

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
     * Notify the cache implementation that an object was seen transitioning to
     * zero hard references. The cache may use this to keep track the number of
     * unreferenced SharedObjects, and to trigger evictions.
     */
    virtual void handleUnreferencedObject() const = 0;

    virtual ~UnifiedCacheBase();
private:
    UnifiedCacheBase(const UnifiedCacheBase &) = delete;
    UnifiedCacheBase &operator=(const UnifiedCacheBase &) = delete;
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
            softRefCount(0),
            hardRefCount(0),
            cachePtr(NULL) {}

    /** Initializes totalRefCount, softRefCount to 0. */
    SharedObject(const SharedObject &other) :
            UObject(other),
            softRefCount(0),
            hardRefCount(0),
            cachePtr(NULL) {}

    virtual ~SharedObject();

    /**
     * Increments the number of hard references to this object. Thread-safe.
     * Not for use from within the Unified Cache implementation.
     */
    void addRef() const;

    /**
     * Decrements the number of hard references to this object, and
     * arrange for possible cache-eviction and/or deletion if ref
     * count goes to zero. Thread-safe.
     * 
     * Not for use from within the UnifiedCache implementation.
     */
    void removeRef() const;

    /**
     * Returns the number of hard references for this object.
     * Uses a memory barrier.
     */
    int32_t getRefCount() const;

    /**
     * If noHardReferences() == true then this object has no hard references.
     * Must be called only from within the internals of UnifiedCache.
     */
    inline UBool noHardReferences() const { return getRefCount() == 0; }

    /**
     * If hasHardReferences() == true then this object has hard references.
     * Must be called only from within the internals of UnifiedCache.
     */
    inline UBool hasHardReferences() const { return getRefCount() != 0; }

    /**
     * Deletes this object if it has no references.
     * Available for non-cached SharedObjects only. Ownership of cached objects
     * is with the UnifiedCache, which is solely responsible for eviction and deletion.
     */
    void deleteIfZeroRefCount() const;

        
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
    /**
     * The number of references from the UnifiedCache, which is
     * the number of times that the sharedObject is stored as a hash table value.
     * For use by UnifiedCache implementation code only.
     * All access is synchronized by UnifiedCache's gCacheMutex
     */
    mutable int32_t softRefCount;
    friend class UnifiedCache;

    /**
     * Reference count, excluding references from within the UnifiedCache implementation.
     */
    mutable u_atomic_int32_t hardRefCount;
    
    mutable const UnifiedCacheBase *cachePtr;

};

U_NAMESPACE_END

#endif
