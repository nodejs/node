// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/**
*******************************************************************************
* Copyright (C) 2001-2014, International Business Machines Corporation.
* All Rights Reserved.
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_SERVICE

#include "serv.h"
#include "umutex.h"

#undef SERVICE_REFCOUNT

// in case we use the refcount stuff

U_NAMESPACE_BEGIN

/*
******************************************************************
*/

const UChar ICUServiceKey::PREFIX_DELIMITER = 0x002F;   /* '/' */

ICUServiceKey::ICUServiceKey(const UnicodeString& id)
: _id(id) {
}

ICUServiceKey::~ICUServiceKey()
{
}

const UnicodeString&
ICUServiceKey::getID() const
{
    return _id;
}

UnicodeString&
ICUServiceKey::canonicalID(UnicodeString& result) const
{
    return result.append(_id);
}

UnicodeString&
ICUServiceKey::currentID(UnicodeString& result) const
{
    return canonicalID(result);
}

UnicodeString&
ICUServiceKey::currentDescriptor(UnicodeString& result) const
{
    prefix(result);
    result.append(PREFIX_DELIMITER);
    return currentID(result);
}

UBool
ICUServiceKey::fallback()
{
    return FALSE;
}

UBool
ICUServiceKey::isFallbackOf(const UnicodeString& id) const
{
    return id == _id;
}

UnicodeString&
ICUServiceKey::prefix(UnicodeString& result) const
{
    return result;
}

UnicodeString&
ICUServiceKey::parsePrefix(UnicodeString& result)
{
    int32_t n = result.indexOf(PREFIX_DELIMITER);
    if (n < 0) {
        n = 0;
    }
    result.remove(n);
    return result;
}

UnicodeString&
ICUServiceKey::parseSuffix(UnicodeString& result)
{
    int32_t n = result.indexOf(PREFIX_DELIMITER);
    if (n >= 0) {
        result.remove(0, n+1);
    }
    return result;
}

#ifdef SERVICE_DEBUG
UnicodeString&
ICUServiceKey::debug(UnicodeString& result) const
{
    debugClass(result);
    result.append((UnicodeString)" id: ");
    result.append(_id);
    return result;
}

UnicodeString&
ICUServiceKey::debugClass(UnicodeString& result) const
{
    return result.append((UnicodeString)"ICUServiceKey");
}
#endif

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(ICUServiceKey)

/*
******************************************************************
*/

ICUServiceFactory::~ICUServiceFactory() {}

SimpleFactory::SimpleFactory(UObject* instanceToAdopt, const UnicodeString& id, UBool visible)
: _instance(instanceToAdopt), _id(id), _visible(visible)
{
}

SimpleFactory::~SimpleFactory()
{
    delete _instance;
}

UObject*
SimpleFactory::create(const ICUServiceKey& key, const ICUService* service, UErrorCode& status) const
{
    if (U_SUCCESS(status)) {
        UnicodeString temp;
        if (_id == key.currentID(temp)) {
            return service->cloneInstance(_instance);
        }
    }
    return NULL;
}

void
SimpleFactory::updateVisibleIDs(Hashtable& result, UErrorCode& status) const
{
    if (_visible) {
        result.put(_id, (void*)this, status); // cast away const
    } else {
        result.remove(_id);
    }
}

UnicodeString&
SimpleFactory::getDisplayName(const UnicodeString& id, const Locale& /* locale */, UnicodeString& result) const
{
    if (_visible && _id == id) {
        result = _id;
    } else {
        result.setToBogus();
    }
    return result;
}

#ifdef SERVICE_DEBUG
UnicodeString&
SimpleFactory::debug(UnicodeString& toAppendTo) const
{
    debugClass(toAppendTo);
    toAppendTo.append((UnicodeString)" id: ");
    toAppendTo.append(_id);
    toAppendTo.append((UnicodeString)", visible: ");
    toAppendTo.append(_visible ? (UnicodeString)"T" : (UnicodeString)"F");
    return toAppendTo;
}

UnicodeString&
SimpleFactory::debugClass(UnicodeString& toAppendTo) const
{
    return toAppendTo.append((UnicodeString)"SimpleFactory");
}
#endif

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SimpleFactory)

/*
******************************************************************
*/

ServiceListener::~ServiceListener() {}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(ServiceListener)

/*
******************************************************************
*/

// Record the actual id for this service in the cache, so we can return it
// even if we succeed later with a different id.
class CacheEntry : public UMemory {
private:
    int32_t refcount;

public:
    UnicodeString actualDescriptor;
    UObject* service;

    /**
    * Releases a reference to the shared resource.
    */
    ~CacheEntry() {
        delete service;
    }

    CacheEntry(const UnicodeString& _actualDescriptor, UObject* _service)
        : refcount(1), actualDescriptor(_actualDescriptor), service(_service) {
    }

    /**
    * Instantiation creates an initial reference, so don't call this
    * unless you're creating a new pointer to this.  Management of
    * that pointer will have to know how to deal with refcounts.
    * Return true if the resource has not already been released.
    */
    CacheEntry* ref() {
        ++refcount;
        return this;
    }

    /**
    * Destructions removes a reference, so don't call this unless
    * you're removing pointer to this somewhere.  Management of that
    * pointer will have to know how to deal with refcounts.  Once
    * the refcount drops to zero, the resource is released.  Return
    * false if the resouce has been released.
    */
    CacheEntry* unref() {
        if ((--refcount) == 0) {
            delete this;
            return NULL;
        }
        return this;
    }

    /**
    * Return TRUE if there is at least one reference to this and the
    * resource has not been released.
    */
    UBool isShared() const {
        return refcount > 1;
    }
};

// UObjectDeleter for serviceCache
U_CDECL_BEGIN
static void U_CALLCONV
cacheDeleter(void* obj) {
    U_NAMESPACE_USE ((CacheEntry*)obj)->unref();
}

/**
* Deleter for UObjects
*/
static void U_CALLCONV
deleteUObject(void *obj) {
    U_NAMESPACE_USE delete (UObject*) obj;
}
U_CDECL_END

/*
******************************************************************
*/

class DNCache : public UMemory {
public:
    Hashtable cache;
    const Locale locale;

    DNCache(const Locale& _locale)
        : cache(), locale(_locale)
    {
        // cache.setKeyDeleter(uprv_deleteUObject);
    }
};


/*
******************************************************************
*/

StringPair*
StringPair::create(const UnicodeString& displayName,
                   const UnicodeString& id,
                   UErrorCode& status)
{
    if (U_SUCCESS(status)) {
        StringPair* sp = new StringPair(displayName, id);
        if (sp == NULL || sp->isBogus()) {
            status = U_MEMORY_ALLOCATION_ERROR;
            delete sp;
            return NULL;
        }
        return sp;
    }
    return NULL;
}

UBool
StringPair::isBogus() const {
    return displayName.isBogus() || id.isBogus();
}

StringPair::StringPair(const UnicodeString& _displayName,
                       const UnicodeString& _id)
: displayName(_displayName)
, id(_id)
{
}

U_CDECL_BEGIN
static void U_CALLCONV
userv_deleteStringPair(void *obj) {
    U_NAMESPACE_USE delete (StringPair*) obj;
}
U_CDECL_END

/*
******************************************************************
*/

static UMutex lock = U_MUTEX_INITIALIZER;

ICUService::ICUService()
: name()
, timestamp(0)
, factories(NULL)
, serviceCache(NULL)
, idCache(NULL)
, dnCache(NULL)
{
}

ICUService::ICUService(const UnicodeString& newName)
: name(newName)
, timestamp(0)
, factories(NULL)
, serviceCache(NULL)
, idCache(NULL)
, dnCache(NULL)
{
}

ICUService::~ICUService()
{
    {
        Mutex mutex(&lock);
        clearCaches();
        delete factories;
        factories = NULL;
    }
}

UObject*
ICUService::get(const UnicodeString& descriptor, UErrorCode& status) const
{
    return get(descriptor, NULL, status);
}

UObject*
ICUService::get(const UnicodeString& descriptor, UnicodeString* actualReturn, UErrorCode& status) const
{
    UObject* result = NULL;
    ICUServiceKey* key = createKey(&descriptor, status);
    if (key) {
        result = getKey(*key, actualReturn, status);
        delete key;
    }
    return result;
}

UObject*
ICUService::getKey(ICUServiceKey& key, UErrorCode& status) const
{
    return getKey(key, NULL, status);
}

// this is a vector that subclasses of ICUService can override to further customize the result object
// before returning it.  All other public get functions should call this one.

UObject*
ICUService::getKey(ICUServiceKey& key, UnicodeString* actualReturn, UErrorCode& status) const
{
    return getKey(key, actualReturn, NULL, status);
}

// make it possible to call reentrantly on systems that don't have reentrant mutexes.
// we can use this simple approach since we know the situation where we're calling
// reentrantly even without knowing the thread.
class XMutex : public UMemory {
public:
    inline XMutex(UMutex *mutex, UBool reentering)
        : fMutex(mutex)
        , fActive(!reentering)
    {
        if (fActive) umtx_lock(fMutex);
    }
    inline ~XMutex() {
        if (fActive) umtx_unlock(fMutex);
    }

private:
    UMutex  *fMutex;
    UBool fActive;
};

struct UVectorDeleter {
    UVector* _obj;
    UVectorDeleter() : _obj(NULL) {}
    ~UVectorDeleter() { delete _obj; }
};

// called only by factories, treat as private
UObject*
ICUService::getKey(ICUServiceKey& key, UnicodeString* actualReturn, const ICUServiceFactory* factory, UErrorCode& status) const
{
    if (U_FAILURE(status)) {
        return NULL;
    }

    if (isDefault()) {
        return handleDefault(key, actualReturn, status);
    }

    ICUService* ncthis = (ICUService*)this; // cast away semantic const

    CacheEntry* result = NULL;
    {
        // The factory list can't be modified until we're done,
        // otherwise we might update the cache with an invalid result.
        // The cache has to stay in synch with the factory list.
        // ICU doesn't have monitors so we can't use rw locks, so
        // we single-thread everything using this service, for now.

        // if factory is not null, we're calling from within the mutex,
        // and since some unix machines don't have reentrant mutexes we
        // need to make sure not to try to lock it again.
        XMutex mutex(&lock, factory != NULL);

        if (serviceCache == NULL) {
            ncthis->serviceCache = new Hashtable(status);
            if (ncthis->serviceCache == NULL) {
                return NULL;
            }
            if (U_FAILURE(status)) {
                delete serviceCache;
                return NULL;
            }
            serviceCache->setValueDeleter(cacheDeleter);
        }

        UnicodeString currentDescriptor;
        UVectorDeleter cacheDescriptorList;
        UBool putInCache = FALSE;

        int32_t startIndex = 0;
        int32_t limit = factories->size();
        UBool cacheResult = TRUE;

        if (factory != NULL) {
            for (int32_t i = 0; i < limit; ++i) {
                if (factory == (const ICUServiceFactory*)factories->elementAt(i)) {
                    startIndex = i + 1;
                    break;
                }
            }
            if (startIndex == 0) {
                // throw new InternalError("Factory " + factory + "not registered with service: " + this);
                status = U_ILLEGAL_ARGUMENT_ERROR;
                return NULL;
            }
            cacheResult = FALSE;
        }

        do {
            currentDescriptor.remove();
            key.currentDescriptor(currentDescriptor);
            result = (CacheEntry*)serviceCache->get(currentDescriptor);
            if (result != NULL) {
                break;
            }

            // first test of cache failed, so we'll have to update
            // the cache if we eventually succeed-- that is, if we're
            // going to update the cache at all.
            putInCache = TRUE;

            int32_t index = startIndex;
            while (index < limit) {
                ICUServiceFactory* f = (ICUServiceFactory*)factories->elementAt(index++);
                UObject* service = f->create(key, this, status);
                if (U_FAILURE(status)) {
                    delete service;
                    return NULL;
                }
                if (service != NULL) {
                    result = new CacheEntry(currentDescriptor, service);
                    if (result == NULL) {
                        delete service;
                        status = U_MEMORY_ALLOCATION_ERROR;
                        return NULL;
                    }

                    goto outerEnd;
                }
            }

            // prepare to load the cache with all additional ids that
            // will resolve to result, assuming we'll succeed.  We
            // don't want to keep querying on an id that's going to
            // fallback to the one that succeeded, we want to hit the
            // cache the first time next goaround.
            if (cacheDescriptorList._obj == NULL) {
                cacheDescriptorList._obj = new UVector(uprv_deleteUObject, NULL, 5, status);
                if (U_FAILURE(status)) {
                    return NULL;
                }
            }
            UnicodeString* idToCache = new UnicodeString(currentDescriptor);
            if (idToCache == NULL || idToCache->isBogus()) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return NULL;
            }

            cacheDescriptorList._obj->addElement(idToCache, status);
            if (U_FAILURE(status)) {
                return NULL;
            }
        } while (key.fallback());
outerEnd:

        if (result != NULL) {
            if (putInCache && cacheResult) {
                serviceCache->put(result->actualDescriptor, result, status);
                if (U_FAILURE(status)) {
                    delete result;
                    return NULL;
                }

                if (cacheDescriptorList._obj != NULL) {
                    for (int32_t i = cacheDescriptorList._obj->size(); --i >= 0;) {
                        UnicodeString* desc = (UnicodeString*)cacheDescriptorList._obj->elementAt(i);
                        serviceCache->put(*desc, result, status);
                        if (U_FAILURE(status)) {
                            delete result;
                            return NULL;
                        }

                        result->ref();
                        cacheDescriptorList._obj->removeElementAt(i);
                    }
                }
            }

            if (actualReturn != NULL) {
                // strip null prefix
                if (result->actualDescriptor.indexOf((UChar)0x2f) == 0) { // U+002f=slash (/)
                    actualReturn->remove();
                    actualReturn->append(result->actualDescriptor,
                        1,
                        result->actualDescriptor.length() - 1);
                } else {
                    *actualReturn = result->actualDescriptor;
                }

                if (actualReturn->isBogus()) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    delete result;
                    return NULL;
                }
            }

            UObject* service = cloneInstance(result->service);
            if (putInCache && !cacheResult) {
                delete result;
            }
            return service;
        }
    }

    return handleDefault(key, actualReturn, status);
}

UObject*
ICUService::handleDefault(const ICUServiceKey& /* key */, UnicodeString* /* actualIDReturn */, UErrorCode& /* status */) const
{
    return NULL;
}

UVector&
ICUService::getVisibleIDs(UVector& result, UErrorCode& status) const {
    return getVisibleIDs(result, NULL, status);
}

UVector&
ICUService::getVisibleIDs(UVector& result, const UnicodeString* matchID, UErrorCode& status) const
{
    result.removeAllElements();

    if (U_FAILURE(status)) {
        return result;
    }

    {
        Mutex mutex(&lock);
        const Hashtable* map = getVisibleIDMap(status);
        if (map != NULL) {
            ICUServiceKey* fallbackKey = createKey(matchID, status);

            for (int32_t pos = UHASH_FIRST;;) {
                const UHashElement* e = map->nextElement(pos);
                if (e == NULL) {
                    break;
                }

                const UnicodeString* id = (const UnicodeString*)e->key.pointer;
                if (fallbackKey != NULL) {
                    if (!fallbackKey->isFallbackOf(*id)) {
                        continue;
                    }
                }

                UnicodeString* idClone = new UnicodeString(*id);
                if (idClone == NULL || idClone->isBogus()) {
                    delete idClone;
                    status = U_MEMORY_ALLOCATION_ERROR;
                    break;
                }
                result.addElement(idClone, status);
                if (U_FAILURE(status)) {
                    delete idClone;
                    break;
                }
            }
            delete fallbackKey;
        }
    }
    if (U_FAILURE(status)) {
        result.removeAllElements();
    }
    return result;
}

const Hashtable*
ICUService::getVisibleIDMap(UErrorCode& status) const {
    if (U_FAILURE(status)) return NULL;

    // must only be called when lock is already held

    ICUService* ncthis = (ICUService*)this; // cast away semantic const
    if (idCache == NULL) {
        ncthis->idCache = new Hashtable(status);
        if (idCache == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        } else if (factories != NULL) {
            for (int32_t pos = factories->size(); --pos >= 0;) {
                ICUServiceFactory* f = (ICUServiceFactory*)factories->elementAt(pos);
                f->updateVisibleIDs(*idCache, status);
            }
            if (U_FAILURE(status)) {
                delete idCache;
                ncthis->idCache = NULL;
            }
        }
    }

    return idCache;
}


UnicodeString&
ICUService::getDisplayName(const UnicodeString& id, UnicodeString& result) const
{
    return getDisplayName(id, result, Locale::getDefault());
}

UnicodeString&
ICUService::getDisplayName(const UnicodeString& id, UnicodeString& result, const Locale& locale) const
{
    {
        UErrorCode status = U_ZERO_ERROR;
        Mutex mutex(&lock);
        const Hashtable* map = getVisibleIDMap(status);
        if (map != NULL) {
            ICUServiceFactory* f = (ICUServiceFactory*)map->get(id);
            if (f != NULL) {
                f->getDisplayName(id, locale, result);
                return result;
            }

            // fallback
            UErrorCode status = U_ZERO_ERROR;
            ICUServiceKey* fallbackKey = createKey(&id, status);
            while (fallbackKey->fallback()) {
                UnicodeString us;
                fallbackKey->currentID(us);
                f = (ICUServiceFactory*)map->get(us);
                if (f != NULL) {
                    f->getDisplayName(id, locale, result);
                    delete fallbackKey;
                    return result;
                }
            }
            delete fallbackKey;
        }
    }
    result.setToBogus();
    return result;
}

UVector&
ICUService::getDisplayNames(UVector& result, UErrorCode& status) const
{
    return getDisplayNames(result, Locale::getDefault(), NULL, status);
}


UVector&
ICUService::getDisplayNames(UVector& result, const Locale& locale, UErrorCode& status) const
{
    return getDisplayNames(result, locale, NULL, status);
}

UVector&
ICUService::getDisplayNames(UVector& result,
                            const Locale& locale,
                            const UnicodeString* matchID,
                            UErrorCode& status) const
{
    result.removeAllElements();
    result.setDeleter(userv_deleteStringPair);
    if (U_SUCCESS(status)) {
        ICUService* ncthis = (ICUService*)this; // cast away semantic const
        Mutex mutex(&lock);

        if (dnCache != NULL && dnCache->locale != locale) {
            delete dnCache;
            ncthis->dnCache = NULL;
        }

        if (dnCache == NULL) {
            const Hashtable* m = getVisibleIDMap(status);
            if (U_FAILURE(status)) {
                return result;
            }
            ncthis->dnCache = new DNCache(locale);
            if (dnCache == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return result;
            }

            int32_t pos = UHASH_FIRST;
            const UHashElement* entry = NULL;
            while ((entry = m->nextElement(pos)) != NULL) {
                const UnicodeString* id = (const UnicodeString*)entry->key.pointer;
                ICUServiceFactory* f = (ICUServiceFactory*)entry->value.pointer;
                UnicodeString dname;
                f->getDisplayName(*id, locale, dname);
                if (dname.isBogus()) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                } else {
                    dnCache->cache.put(dname, (void*)id, status); // share pointer with visibleIDMap
                    if (U_SUCCESS(status)) {
                        continue;
                    }
                }
                delete dnCache;
                ncthis->dnCache = NULL;
                return result;
            }
        }
    }

    ICUServiceKey* matchKey = createKey(matchID, status);
    /* To ensure that all elements in the hashtable are iterated, set pos to -1.
     * nextElement(pos) will skip the position at pos and begin the iteration
     * at the next position, which in this case will be 0.
     */
    int32_t pos = UHASH_FIRST;
    const UHashElement *entry = NULL;
    while ((entry = dnCache->cache.nextElement(pos)) != NULL) {
        const UnicodeString* id = (const UnicodeString*)entry->value.pointer;
        if (matchKey != NULL && !matchKey->isFallbackOf(*id)) {
            continue;
        }
        const UnicodeString* dn = (const UnicodeString*)entry->key.pointer;
        StringPair* sp = StringPair::create(*id, *dn, status);
        result.addElement(sp, status);
        if (U_FAILURE(status)) {
            result.removeAllElements();
            break;
        }
    }
    delete matchKey;

    return result;
}

URegistryKey
ICUService::registerInstance(UObject* objToAdopt, const UnicodeString& id, UErrorCode& status)
{
    return registerInstance(objToAdopt, id, TRUE, status);
}

URegistryKey
ICUService::registerInstance(UObject* objToAdopt, const UnicodeString& id, UBool visible, UErrorCode& status)
{
    ICUServiceKey* key = createKey(&id, status);
    if (key != NULL) {
        UnicodeString canonicalID;
        key->canonicalID(canonicalID);
        delete key;

        ICUServiceFactory* f = createSimpleFactory(objToAdopt, canonicalID, visible, status);
        if (f != NULL) {
            return registerFactory(f, status);
        }
    }
    delete objToAdopt;
    return NULL;
}

ICUServiceFactory*
ICUService::createSimpleFactory(UObject* objToAdopt, const UnicodeString& id, UBool visible, UErrorCode& status)
{
    if (U_SUCCESS(status)) {
        if ((objToAdopt != NULL) && (!id.isBogus())) {
            return new SimpleFactory(objToAdopt, id, visible);
        }
        status = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return NULL;
}

URegistryKey
ICUService::registerFactory(ICUServiceFactory* factoryToAdopt, UErrorCode& status)
{
    if (U_SUCCESS(status) && factoryToAdopt != NULL) {
        Mutex mutex(&lock);

        if (factories == NULL) {
            factories = new UVector(deleteUObject, NULL, status);
            if (U_FAILURE(status)) {
                delete factories;
                return NULL;
            }
        }
        factories->insertElementAt(factoryToAdopt, 0, status);
        if (U_SUCCESS(status)) {
            clearCaches();
        } else {
            delete factoryToAdopt;
            factoryToAdopt = NULL;
        }
    }

    if (factoryToAdopt != NULL) {
        notifyChanged();
    }

    return (URegistryKey)factoryToAdopt;
}

UBool
ICUService::unregister(URegistryKey rkey, UErrorCode& status)
{
    ICUServiceFactory *factory = (ICUServiceFactory*)rkey;
    UBool result = FALSE;
    if (factory != NULL && factories != NULL) {
        Mutex mutex(&lock);

        if (factories->removeElement(factory)) {
            clearCaches();
            result = TRUE;
        } else {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            delete factory;
        }
    }
    if (result) {
        notifyChanged();
    }
    return result;
}

void
ICUService::reset()
{
    {
        Mutex mutex(&lock);
        reInitializeFactories();
        clearCaches();
    }
    notifyChanged();
}

void
ICUService::reInitializeFactories()
{
    if (factories != NULL) {
        factories->removeAllElements();
    }
}

UBool
ICUService::isDefault() const
{
    return countFactories() == 0;
}

ICUServiceKey*
ICUService::createKey(const UnicodeString* id, UErrorCode& status) const
{
    return (U_FAILURE(status) || id == NULL) ? NULL : new ICUServiceKey(*id);
}

void
ICUService::clearCaches()
{
    // callers synchronize before use
    ++timestamp;
    delete dnCache;
    dnCache = NULL;
    delete idCache;
    idCache = NULL;
    delete serviceCache; serviceCache = NULL;
}

void
ICUService::clearServiceCache()
{
    // callers synchronize before use
    delete serviceCache; serviceCache = NULL;
}

UBool
ICUService::acceptsListener(const EventListener& l) const
{
    return dynamic_cast<const ServiceListener*>(&l) != NULL;
}

void
ICUService::notifyListener(EventListener& l) const
{
    ((ServiceListener&)l).serviceChanged(*this);
}

UnicodeString&
ICUService::getName(UnicodeString& result) const
{
    return result.append(name);
}

int32_t
ICUService::countFactories() const
{
    return factories == NULL ? 0 : factories->size();
}

int32_t
ICUService::getTimestamp() const
{
    return timestamp;
}

U_NAMESPACE_END

/* UCONFIG_NO_SERVICE */
#endif
