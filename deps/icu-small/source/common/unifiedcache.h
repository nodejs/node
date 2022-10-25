// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*
* File UNIFIEDCACHE.H - The ICU Unified cache.
******************************************************************************
*/

#ifndef __UNIFIED_CACHE_H__
#define __UNIFIED_CACHE_H__

#include "utypeinfo.h"  // for 'typeid' to work

#include "unicode/uobject.h"
#include "unicode/locid.h"
#include "sharedobject.h"
#include "unicode/unistr.h"
#include "cstring.h"
#include "ustr_imp.h"

struct UHashtable;
struct UHashElement;

U_NAMESPACE_BEGIN

class UnifiedCache;

/**
 * A base class for all cache keys.
 */
class U_COMMON_API CacheKeyBase : public UObject {
 public:
   CacheKeyBase() : fCreationStatus(U_ZERO_ERROR), fIsPrimary(false) {}

   /**
    * Copy constructor. Needed to support cloning.
    */
   CacheKeyBase(const CacheKeyBase &other) 
           : UObject(other), fCreationStatus(other.fCreationStatus), fIsPrimary(false) { }
   virtual ~CacheKeyBase();

   /**
    * Returns the hash code for this object.
    */
   virtual int32_t hashCode() const = 0;

   /**
    * Clones this object polymorphically. Caller owns returned value.
    */
   virtual CacheKeyBase *clone() const = 0;

   /**
    * Create a new object for this key. Called by cache on cache miss.
    * createObject must add a reference to the object it returns. Note
    * that getting an object from the cache and returning it without calling
    * removeRef on it satisfies this requirement. It can also return NULL
    * and set status to an error.
    *
    * @param creationContext the context in which the object is being
    *                        created. May be NULL.
    * @param status          Implementations can return a failure here.
    *                        In addition, implementations may return a
    *                        non NULL object and set a warning status.
    */
   virtual const SharedObject *createObject(
           const void *creationContext, UErrorCode &status) const = 0;

   /**
    * Writes a description of this key to buffer and returns buffer. Written
    * description is NULL terminated.
    */
   virtual char *writeDescription(char *buffer, int32_t bufSize) const = 0;

   friend inline bool operator==(const CacheKeyBase& lhs,
                                 const CacheKeyBase& rhs) {
       return lhs.equals(rhs);
   }

   friend inline bool operator!=(const CacheKeyBase& lhs,
                                 const CacheKeyBase& rhs) {
       return !lhs.equals(rhs);
   }

 protected:
   virtual bool equals(const CacheKeyBase& other) const = 0;

 private:
   mutable UErrorCode fCreationStatus;
   mutable UBool fIsPrimary;
   friend class UnifiedCache;
};



/**
 * Templated version of CacheKeyBase. 
 * A key of type LocaleCacheKey<T> maps to a value of type T.
 */
template<typename T>
class CacheKey : public CacheKeyBase {
 public:
   virtual ~CacheKey() { }
   /**
    * The template parameter, T, determines the hash code returned.
    */
   virtual int32_t hashCode() const override {
       const char *s = typeid(T).name();
       return ustr_hashCharsN(s, static_cast<int32_t>(uprv_strlen(s)));
   }

   /**
    * Use the value type, T,  as the description.
    */
   virtual char *writeDescription(char *buffer, int32_t bufLen) const override {
       const char *s = typeid(T).name();
       uprv_strncpy(buffer, s, bufLen);
       buffer[bufLen - 1] = 0;
       return buffer;
   }

 protected:
   /**
    * Two objects are equal if they are of the same type.
    */
   virtual bool equals(const CacheKeyBase &other) const override {
       return this == &other || typeid(*this) == typeid(other);
   }
};

/**
 * Cache key based on locale.
 * A key of type LocaleCacheKey<T> maps to a value of type T.
 */
template<typename T>
class LocaleCacheKey : public CacheKey<T> {
 protected:
   Locale   fLoc;
   virtual bool equals(const CacheKeyBase &other) const override {
       if (!CacheKey<T>::equals(other)) {
           return false;
       }
       // We know this and other are of same class because equals() on
       // CacheKey returned true.
       return operator==(static_cast<const LocaleCacheKey<T> &>(other));
   }
 public:
   LocaleCacheKey(const Locale &loc) : fLoc(loc) {}
   LocaleCacheKey(const LocaleCacheKey<T> &other)
           : CacheKey<T>(other), fLoc(other.fLoc) { }
   virtual ~LocaleCacheKey() { }
   virtual int32_t hashCode() const override {
       return (int32_t)(37u * (uint32_t)CacheKey<T>::hashCode() + (uint32_t)fLoc.hashCode());
   }
   inline bool operator == (const LocaleCacheKey<T> &other) const {
       return fLoc == other.fLoc;
   }
   virtual CacheKeyBase *clone() const override {
       return new LocaleCacheKey<T>(*this);
   }
   virtual const T *createObject(
           const void *creationContext, UErrorCode &status) const override;
   /**
    * Use the locale id as the description.
    */
   virtual char *writeDescription(char *buffer, int32_t bufLen) const override {
       const char *s = fLoc.getName();
       uprv_strncpy(buffer, s, bufLen);
       buffer[bufLen - 1] = 0;
       return buffer;
   }

};

/**
 * The unified cache. A singleton type.
 * Design doc here:
 * https://docs.google.com/document/d/1RwGQJs4N4tawNbf809iYDRCvXoMKqDJihxzYt1ysmd8/edit?usp=sharing
 */
class U_COMMON_API UnifiedCache : public UnifiedCacheBase {
 public:
   /**
    * @internal
    * Do not call directly. Instead use UnifiedCache::getInstance() as
    * there should be only one UnifiedCache in an application.
    */
   UnifiedCache(UErrorCode &status);

   /**
    * Return a pointer to the global cache instance.
    */
   static UnifiedCache *getInstance(UErrorCode &status);

   /**
    * Fetches a value from the cache by key. Equivalent to
    * get(key, NULL, ptr, status);
    */
   template<typename T>
   void get(
           const CacheKey<T>& key,
           const T *&ptr,
           UErrorCode &status) const {
       get(key, NULL, ptr, status);
   }

   /**
    * Fetches value from the cache by key.
    *
    * @param key             the cache key.
    * @param creationContext passed verbatim to createObject method of key
    * @param ptr             On entry, ptr must be NULL or be included if
    *                        the reference count of the object it points
    *                        to. On exit, ptr points to the fetched object
    *                        from the cache or is left unchanged on
    *                        failure. Caller must call removeRef on ptr
    *                        if set to a non NULL value.
    * @param status          Any error returned here. May be set to a
    *                        warning value even if ptr is set.
    */
   template<typename T>
   void get(
           const CacheKey<T>& key,
           const void *creationContext,
           const T *&ptr,
           UErrorCode &status) const {
       if (U_FAILURE(status)) {
           return;
       }
       UErrorCode creationStatus = U_ZERO_ERROR;
       const SharedObject *value = NULL;
       _get(key, value, creationContext, creationStatus);
       const T *tvalue = (const T *) value;
       if (U_SUCCESS(creationStatus)) {
           SharedObject::copyPtr(tvalue, ptr);
       }
       SharedObject::clearPtr(tvalue);
       // Take care not to overwrite a warning status passed in with
       // another warning or U_ZERO_ERROR.
       if (status == U_ZERO_ERROR || U_FAILURE(creationStatus)) {
           status = creationStatus;
       }
   }

#ifdef UNIFIED_CACHE_DEBUG
   /**
    * Dumps the contents of this cache to standard error. Used for testing of
    * cache only.
    */
   void dumpContents() const;
#endif

   /**
    * Convenience method to get a value of type T from cache for a
    * particular locale with creationContext == NULL.
    * @param loc    the locale
    * @param ptr    On entry, must be NULL or included in the ref count
    *               of the object to which it points.
    *               On exit, fetched value stored here or is left
    *               unchanged on failure. Caller must call removeRef on
    *               ptr if set to a non NULL value.
    * @param status Any error returned here. May be set to a
    *               warning value even if ptr is set.
    */
   template<typename T>
   static void getByLocale(
           const Locale &loc, const T *&ptr, UErrorCode &status) {
       const UnifiedCache *cache = getInstance(status);
       if (U_FAILURE(status)) {
           return;
       }
       cache->get(LocaleCacheKey<T>(loc), ptr, status);
   }

#ifdef UNIFIED_CACHE_DEBUG
   /**
    * Dumps the cache contents to stderr. For testing only.
    */
   static void dump();
#endif

   /**
    * Returns the number of keys in this cache. For testing only.
    */
   int32_t keyCount() const;

   /**
    * Removes any values from cache that are not referenced outside
    * the cache.
    */
   void flush() const;

   /**
    * Configures at what point eviction of unused entries will begin.
    * Eviction is triggered whenever the number of evictable keys exceeds
    * BOTH count AND (number of in-use items) * (percentageOfInUseItems / 100).
    * Once the number of unused entries drops below one of these,
    * eviction ceases. Because eviction happens incrementally,
    * the actual unused entry count may exceed both these numbers
    * from time to time.
    *
    * A cache entry is defined as unused if it is not essential to guarantee
    * that for a given key X, the cache returns the same reference to the
    * same value as long as the client already holds a reference to that
    * value.
    *
    * If this method is never called, the default settings are 1000 and 100%.
    *
    * Although this method is thread-safe, it is designed to be called at
    * application startup. If it is called in the middle of execution, it
    * will have no immediate effect on the cache. However over time, the
    * cache will perform eviction slices in an attempt to honor the new
    * settings.
    *
    * If a client already holds references to many different unique values
    * in the cache such that the number of those unique values far exceeds
    * "count" then the cache may not be able to maintain this maximum.
    * However, if this happens, the cache still guarantees that the number of
    * unused entries will remain only a small percentage of the total cache
    * size.
    *
    * If the parameters passed are negative, setEvctionPolicy sets status to
    * U_ILLEGAL_ARGUMENT_ERROR.
    */
   void setEvictionPolicy(
           int32_t count, int32_t percentageOfInUseItems, UErrorCode &status);


   /**
    * Returns how many entries have been auto evicted during the lifetime
    * of this cache. This only includes auto evicted entries, not
    * entries evicted because of a call to flush().
    */
   int64_t autoEvictedCount() const;

   /**
    * Returns the unused entry count in this cache. For testing only,
    * Regular clients will not need this.
    */
   int32_t unusedCount() const;

   virtual void handleUnreferencedObject() const override;
   virtual ~UnifiedCache();
   
 private:
   UHashtable *fHashtable;
   mutable int32_t fEvictPos;
   mutable int32_t fNumValuesTotal;
   mutable int32_t fNumValuesInUse;
   int32_t fMaxUnused;
   int32_t fMaxPercentageOfInUse;
   mutable int64_t fAutoEvictedCount;
   SharedObject *fNoValue;
   
   UnifiedCache(const UnifiedCache &other) = delete;
   UnifiedCache &operator=(const UnifiedCache &other) = delete;
   
   /**
    * Flushes the contents of the cache. If cache values hold references to other
    * cache values then _flush should be called in a loop until it returns false.
    * 
    * On entry, gCacheMutex must be held.
    * On exit, those values with are evictable are flushed.
    * 
    *  @param all if false flush evictable items only, which are those with no external
    *                    references, plus those that can be safely recreated.<br>
    *            if true, flush all elements. Any values (sharedObjects) with remaining
    *                     hard (external) references are not deleted, but are detached from
    *                     the cache, so that a subsequent removeRefs can delete them.
    *                     _flush is not thread safe when all is true.
    *   @return true if any value in cache was flushed or false otherwise.
    */
   UBool _flush(UBool all) const;
   
   /**
    * Gets value out of cache.
    * On entry. gCacheMutex must not be held. value must be NULL. status
    * must be U_ZERO_ERROR.
    * On exit. value and status set to what is in cache at key or on cache
    * miss the key's createObject() is called and value and status are set to
    * the result of that. In this latter case, best effort is made to add the
    * value and status to the cache. If createObject() fails to create a value,
    * fNoValue is stored in cache, and value is set to NULL. Caller must call
    * removeRef on value if non NULL.
    */
   void _get(
           const CacheKeyBase &key,
           const SharedObject *&value,
           const void *creationContext,
           UErrorCode &status) const;

    /**
     * Attempts to fetch value and status for key from cache.
     * On entry, gCacheMutex must not be held value must be NULL and status must
     * be U_ZERO_ERROR.
     * On exit, either returns false (In this
     * case caller should try to create the object) or returns true with value
     * pointing to the fetched value and status set to fetched status. When
     * false is returned status may be set to failure if an in progress hash
     * entry could not be made but value will remain unchanged. When true is
     * returned, caller must call removeRef() on value.
     */
    UBool _poll(
            const CacheKeyBase &key,
            const SharedObject *&value,
            UErrorCode &status) const;
    
    /**
     * Places a new value and creationStatus in the cache for the given key.
     * On entry, gCacheMutex must be held. key must not exist in the cache. 
     * On exit, value and creation status placed under key. Soft reference added
     * to value on successful add. On error sets status.
     */
    void _putNew(
        const CacheKeyBase &key,
        const SharedObject *value,
        const UErrorCode creationStatus,
        UErrorCode &status) const;
           
    /**
     * Places value and status at key if there is no value at key or if cache
     * entry for key is in progress. Otherwise, it leaves the current value and
     * status there.
     * 
     * On entry. gCacheMutex must not be held. Value must be
     * included in the reference count of the object to which it points.
     * 
     * On exit, value and status are changed to what was already in the cache if
     * something was there and not in progress. Otherwise, value and status are left
     * unchanged in which case they are placed in the cache on a best-effort basis.
     * Caller must call removeRef() on value.
     */
   void _putIfAbsentAndGet(
           const CacheKeyBase &key,
           const SharedObject *&value,
           UErrorCode &status) const;

    /**
     * Returns the next element in the cache round robin style.
     * Returns nullptr if the cache is empty.
     * On entry, gCacheMutex must be held.
     */
    const UHashElement *_nextElement() const;
   
   /**
    * Return the number of cache items that would need to be evicted
    * to bring usage into conformance with eviction policy.
    * 
    * An item corresponds to an entry in the hash table, a hash table element.
    * 
    * On entry, gCacheMutex must be held.
    */
   int32_t _computeCountOfItemsToEvict() const;
   
   /**
    * Run an eviction slice.
    * On entry, gCacheMutex must be held.
    * _runEvictionSlice runs a slice of the evict pipeline by examining the next
    * 10 entries in the cache round robin style evicting them if they are eligible.
    */
   void _runEvictionSlice() const;
 
   /**
    * Register a primary cache entry. A primary key is the first key to create
    * a given  SharedObject value. Subsequent keys whose create function
    * produce references to an already existing SharedObject are not primary -
    * they can be evicted and subsequently recreated.
    * 
    * On entry, gCacheMutex must be held.
    * On exit, items in use count incremented, entry is marked as a primary
    * entry, and value registered with cache so that subsequent calls to
    * addRef() and removeRef() on it correctly interact with the cache.
    */
   void _registerPrimary(const CacheKeyBase *theKey, const SharedObject *value) const;
        
   /**
    * Store a value and creation error status in given hash entry.
    * On entry, gCacheMutex must be held. Hash entry element must be in progress.
    * value must be non NULL.
    * On Exit, soft reference added to value. value and status stored in hash
    * entry. Soft reference removed from previous stored value. Waiting
    * threads notified.
    */
   void _put(
           const UHashElement *element,
           const SharedObject *value,
           const UErrorCode status) const;
    /**
     * Remove a soft reference, and delete the SharedObject if no references remain.
     * To be used from within the UnifiedCache implementation only.
     * gCacheMutex must be held by caller.
     * @param value the SharedObject to be acted on.
     */
   void removeSoftRef(const SharedObject *value) const;
   
   /**
    * Increment the hard reference count of the given SharedObject.
    * gCacheMutex must be held by the caller.
    * Update numValuesEvictable on transitions between zero and one reference.
    * 
    * @param value The SharedObject to be referenced.
    * @return the hard reference count after the addition.
    */
   int32_t addHardRef(const SharedObject *value) const;
   
  /**
    * Decrement the hard reference count of the given SharedObject.
    * gCacheMutex must be held by the caller.
    * Update numValuesEvictable on transitions between one and zero reference.
    * 
    * @param value The SharedObject to be referenced.
    * @return the hard reference count after the removal.
    */
   int32_t removeHardRef(const SharedObject *value) const;

   
#ifdef UNIFIED_CACHE_DEBUG
   void _dumpContents() const;
#endif
   
   /**
    *  Fetch value and error code from a particular hash entry.
    *  On entry, gCacheMutex must be held. value must be either NULL or must be
    *  included in the ref count of the object to which it points.
    *  On exit, value and status set to what is in the hash entry. Caller must
    *  eventually call removeRef on value.
    *  If hash entry is in progress, value will be set to gNoValue and status will
    *  be set to U_ZERO_ERROR.
    */
   void _fetch(const UHashElement *element, const SharedObject *&value,
                       UErrorCode &status) const;
                       
    /**
     * Determine if given hash entry is in progress.
     * On entry, gCacheMutex must be held.
     */
   UBool _inProgress(const UHashElement *element) const;
   
   /**
    * Determine if given hash entry is in progress.
    * On entry, gCacheMutex must be held.
    */
   UBool _inProgress(const SharedObject *theValue, UErrorCode creationStatus) const;
   
   /**
    * Determine if given hash entry is eligible for eviction.
    * On entry, gCacheMutex must be held.
    */
   UBool _isEvictable(const UHashElement *element) const;
};

U_NAMESPACE_END

#endif
