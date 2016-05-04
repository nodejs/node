/**
 *******************************************************************************
 * Copyright (C) 2001-2011, International Business Machines Corporation and    *
 * others. All Rights Reserved.                                                *
 *******************************************************************************
 *
 *******************************************************************************
 */
#ifndef ICULSERV_H
#define ICULSERV_H

#include "unicode/utypes.h"

#if UCONFIG_NO_SERVICE

U_NAMESPACE_BEGIN

/*
 * Allow the declaration of APIs with pointers to ICUService
 * even when service is removed from the build.
 */
class ICULocaleService;

U_NAMESPACE_END

#else

#include "unicode/unistr.h"
#include "unicode/locid.h"
#include "unicode/strenum.h"

#include "hash.h"
#include "uvector.h"

#include "serv.h"
#include "locutil.h"

U_NAMESPACE_BEGIN

class ICULocaleService;

class LocaleKey;
class LocaleKeyFactory;
class SimpleLocaleKeyFactory;
class ServiceListener;

/*
 ******************************************************************
 */

/**
 * A subclass of Key that implements a locale fallback mechanism.
 * The first locale to search for is the locale provided by the
 * client, and the fallback locale to search for is the current
 * default locale.  If a prefix is present, the currentDescriptor
 * includes it before the locale proper, separated by "/".  This
 * is the default key instantiated by ICULocaleService.</p>
 *
 * <p>Canonicalization adjusts the locale string so that the
 * section before the first understore is in lower case, and the rest
 * is in upper case, with no trailing underscores.</p> 
 */

class U_COMMON_API LocaleKey : public ICUServiceKey {
  private: 
    int32_t _kind;
    UnicodeString _primaryID;
    UnicodeString _fallbackID;
    UnicodeString _currentID;

  public:
    enum {
        KIND_ANY = -1
    };

    /**
     * Create a LocaleKey with canonical primary and fallback IDs.
     */
    static LocaleKey* createWithCanonicalFallback(const UnicodeString* primaryID, 
                                                  const UnicodeString* canonicalFallbackID,
                                                  UErrorCode& status);

    /**
     * Create a LocaleKey with canonical primary and fallback IDs.
     */
    static LocaleKey* createWithCanonicalFallback(const UnicodeString* primaryID, 
                                                  const UnicodeString* canonicalFallbackID, 
                                                  int32_t kind,
                                                  UErrorCode& status);

  protected:
    /**
     * PrimaryID is the user's requested locale string,
     * canonicalPrimaryID is this string in canonical form,
     * fallbackID is the current default locale's string in
     * canonical form.
     */
    LocaleKey(const UnicodeString& primaryID, 
              const UnicodeString& canonicalPrimaryID, 
              const UnicodeString* canonicalFallbackID, 
              int32_t kind);

 public:
    /**
     * Append the prefix associated with the kind, or nothing if the kind is KIND_ANY.
     */
    virtual UnicodeString& prefix(UnicodeString& result) const;

    /**
     * Return the kind code associated with this key.
     */
    virtual int32_t kind() const;

    /**
     * Return the canonicalID.
     */
    virtual UnicodeString& canonicalID(UnicodeString& result) const;

    /**
     * Return the currentID.
     */
    virtual UnicodeString& currentID(UnicodeString& result) const;

    /**
     * Return the (canonical) current descriptor, or null if no current id.
     */
    virtual UnicodeString& currentDescriptor(UnicodeString& result) const;

    /**
     * Convenience method to return the locale corresponding to the (canonical) original ID.
     */
    virtual Locale& canonicalLocale(Locale& result) const;

    /**
     * Convenience method to return the locale corresponding to the (canonical) current ID.
     */
    virtual Locale& currentLocale(Locale& result) const;

    /**
     * If the key has a fallback, modify the key and return true,
     * otherwise return false.</p>
     *
     * <p>First falls back through the primary ID, then through
     * the fallbackID.  The final fallback is the empty string,
     * unless the primary id was the empty string, in which case
     * there is no fallback.  
     */
    virtual UBool fallback();

    /**
     * Return true if a key created from id matches, or would eventually
     * fallback to match, the canonical ID of this key.  
     */
    virtual UBool isFallbackOf(const UnicodeString& id) const;
    
 public:
    /**
     * UObject boilerplate.
     */
    static UClassID U_EXPORT2 getStaticClassID();

    virtual UClassID getDynamicClassID() const;

    /**
     * Destructor.
     */
    virtual ~LocaleKey();

#ifdef SERVICE_DEBUG
 public:
    virtual UnicodeString& debug(UnicodeString& result) const;
    virtual UnicodeString& debugClass(UnicodeString& result) const;
#endif

};

/*
 ******************************************************************
 */

/**
 * A subclass of ICUServiceFactory that uses LocaleKeys, and is able to
 * 'cover' more specific locales with more general locales that it
 * supports.  
 *
 * <p>Coverage may be either of the values VISIBLE or INVISIBLE.
 *
 * <p>'Visible' indicates that the specific locale(s) supported by
 * the factory are registered in getSupportedIDs, 'Invisible'
 * indicates that they are not.
 *
 * <p>Localization of visible ids is handled
 * by the handling factory, regardless of kind.
 */
class U_COMMON_API LocaleKeyFactory : public ICUServiceFactory {
protected:
    const UnicodeString _name;
    const int32_t _coverage;

public:
    enum {
        /**
         * Coverage value indicating that the factory makes
         * its locales visible, and does not cover more specific 
         * locales.
         */
        VISIBLE = 0,

        /**
         * Coverage value indicating that the factory does not make
         * its locales visible, and does not cover more specific
         * locales.
         */
        INVISIBLE = 1
    };

    /**
     * Destructor.
     */
    virtual ~LocaleKeyFactory();

protected:
    /**
     * Constructor used by subclasses.
     */
    LocaleKeyFactory(int32_t coverage);

    /**
     * Constructor used by subclasses.
     */
    LocaleKeyFactory(int32_t coverage, const UnicodeString& name);

    /**
     * Implement superclass abstract method.  This checks the currentID of
     * the key against the supported IDs, and passes the canonicalLocale and
     * kind off to handleCreate (which subclasses must implement).
     */
public:
    virtual UObject* create(const ICUServiceKey& key, const ICUService* service, UErrorCode& status) const;

protected:
    virtual UBool handlesKey(const ICUServiceKey& key, UErrorCode& status) const;

public:
    /**
     * Override of superclass method.  This adjusts the result based
     * on the coverage rule for this factory.
     */
    virtual void updateVisibleIDs(Hashtable& result, UErrorCode& status) const;

    /**
     * Return a localized name for the locale represented by id.
     */
    virtual UnicodeString& getDisplayName(const UnicodeString& id, const Locale& locale, UnicodeString& result) const;

protected:
    /**
     * Utility method used by create(ICUServiceKey, ICUService).  Subclasses can implement
     * this instead of create.  The default returns NULL.
     */
    virtual UObject* handleCreate(const Locale& loc, int32_t kind, const ICUService* service, UErrorCode& status) const;

   /**
     * Return true if this id is one the factory supports (visible or 
     * otherwise).
     */
 //   virtual UBool isSupportedID(const UnicodeString& id, UErrorCode& status) const;

   /**
     * Return the set of ids that this factory supports (visible or 
     * otherwise).  This can be called often and might need to be
     * cached if it is expensive to create.
     */
    virtual const Hashtable* getSupportedIDs(UErrorCode& status) const;

public:
    /**
     * UObject boilerplate.
     */
    static UClassID U_EXPORT2 getStaticClassID();

    virtual UClassID getDynamicClassID() const;

#ifdef SERVICE_DEBUG
 public:
    virtual UnicodeString& debug(UnicodeString& result) const;
    virtual UnicodeString& debugClass(UnicodeString& result) const;
#endif

};

/*
 ******************************************************************
 */

/**
 * A LocaleKeyFactory that just returns a single object for a kind/locale.
 */

class U_COMMON_API SimpleLocaleKeyFactory : public LocaleKeyFactory {
 private:
    UObject* _obj;
    UnicodeString _id;
    const int32_t _kind;

 public:
    SimpleLocaleKeyFactory(UObject* objToAdopt, 
                           const UnicodeString& locale, 
                           int32_t kind, 
                           int32_t coverage);

    SimpleLocaleKeyFactory(UObject* objToAdopt, 
                           const Locale& locale, 
                           int32_t kind, 
                           int32_t coverage);

    /**
     * Destructor.
     */
    virtual ~SimpleLocaleKeyFactory();

    /**
     * Override of superclass method.  Returns the service object if kind/locale match.  Service is not used.
     */
    virtual UObject* create(const ICUServiceKey& key, const ICUService* service, UErrorCode& status) const;

    /**
     * Override of superclass method.  This adjusts the result based
     * on the coverage rule for this factory.
     */
    virtual void updateVisibleIDs(Hashtable& result, UErrorCode& status) const;

 protected:
    /**
     * Return true if this id is equal to the locale name.
     */
    //virtual UBool isSupportedID(const UnicodeString& id, UErrorCode& status) const;


public:
    /**
     * UObject boilerplate.
     */
    static UClassID U_EXPORT2 getStaticClassID();

    virtual UClassID getDynamicClassID() const;

#ifdef SERVICE_DEBUG
 public:
    virtual UnicodeString& debug(UnicodeString& result) const;
    virtual UnicodeString& debugClass(UnicodeString& result) const;
#endif

};

/*
 ******************************************************************
 */

/**
 * A LocaleKeyFactory that creates a service based on the ICU locale data.
 * This is a base class for most ICU factories.  Subclasses instantiate it
 * with a constructor that takes a bundle name, which determines the supported
 * IDs.  Subclasses then override handleCreate to create the actual service
 * object.  The default implementation returns a resource bundle.
 */
class U_COMMON_API ICUResourceBundleFactory : public LocaleKeyFactory 
{
 protected:
    UnicodeString _bundleName;

 public:
    /**
     * Convenience constructor that uses the main ICU bundle name.
     */
    ICUResourceBundleFactory();

    /**
     * A service factory based on ICU resource data in resources with
     * the given name.  This should be a 'path' that can be passed to
     * ures_openAvailableLocales, such as U_ICUDATA or U_ICUDATA_COLL.
     * The empty string is equivalent to U_ICUDATA.
     */
    ICUResourceBundleFactory(const UnicodeString& bundleName);

    /**
     * Destructor
     */
    virtual ~ICUResourceBundleFactory();

protected:
    /**
     * Return the supported IDs.  This is the set of all locale names in ICULocaleData.
     */
    virtual const Hashtable* getSupportedIDs(UErrorCode& status) const;

    /**
     * Create the service.  The default implementation returns the resource bundle
     * for the locale, ignoring kind, and service.
     */
    virtual UObject* handleCreate(const Locale& loc, int32_t kind, const ICUService* service, UErrorCode& status) const;

public:
    /**
     * UObject boilerplate.
     */
    static UClassID U_EXPORT2 getStaticClassID();
    virtual UClassID getDynamicClassID() const;


#ifdef SERVICE_DEBUG
 public:
    virtual UnicodeString& debug(UnicodeString& result) const;
    virtual UnicodeString& debugClass(UnicodeString& result) const;
#endif

};

/*
 ******************************************************************
 */

class U_COMMON_API ICULocaleService : public ICUService 
{
 private:
  Locale fallbackLocale;
  UnicodeString fallbackLocaleName;

 public:
  /**
   * Construct an ICULocaleService.
   */
  ICULocaleService();

  /**
   * Construct an ICULocaleService with a name (useful for debugging).
   */
  ICULocaleService(const UnicodeString& name);

  /**
   * Destructor.
   */
  virtual ~ICULocaleService();

#if 0
  // redeclare because of overload resolution rules?
  // no, causes ambiguities since both UnicodeString and Locale have constructors that take a const char*
  // need some compiler flag to remove warnings 
  UObject* get(const UnicodeString& descriptor, UErrorCode& status) const {
    return ICUService::get(descriptor, status);
  }

  UObject* get(const UnicodeString& descriptor, UnicodeString* actualReturn, UErrorCode& status) const {
    return ICUService::get(descriptor, actualReturn, status);
  }
#endif

  /**
   * Convenience override for callers using locales.  This calls
   * get(Locale, int, Locale[]) with KIND_ANY for kind and null for
   * actualReturn.
   */
  UObject* get(const Locale& locale, UErrorCode& status) const;

  /**
   * Convenience override for callers using locales.  This calls
   * get(Locale, int, Locale[]) with a null actualReturn.
   */
  UObject* get(const Locale& locale, int32_t kind, UErrorCode& status) const;

  /**
   * Convenience override for callers using locales. This calls
   * get(Locale, String, Locale[]) with a null kind.
   */
  UObject* get(const Locale& locale, Locale* actualReturn, UErrorCode& status) const;
                   
  /**
   * Convenience override for callers using locales.  This uses
   * createKey(Locale.toString(), kind) to create a key, calls getKey, and then
   * if actualReturn is not null, returns the actualResult from
   * getKey (stripping any prefix) into a Locale.  
   */
  UObject* get(const Locale& locale, int32_t kind, Locale* actualReturn, UErrorCode& status) const;

  /**
   * Convenience override for callers using locales.  This calls
   * registerObject(Object, Locale, int32_t kind, int coverage)
   * passing KIND_ANY for the kind, and VISIBLE for the coverage.
   */
  virtual URegistryKey registerInstance(UObject* objToAdopt, const Locale& locale, UErrorCode& status);

  /**
   * Convenience function for callers using locales.  This calls
   * registerObject(Object, Locale, int kind, int coverage)
   * passing VISIBLE for the coverage.
   */
  virtual URegistryKey registerInstance(UObject* objToAdopt, const Locale& locale, int32_t kind, UErrorCode& status);

  /**
   * Convenience function for callers using locales.  This  instantiates
   * a SimpleLocaleKeyFactory, and registers the factory.
   */
  virtual URegistryKey registerInstance(UObject* objToAdopt, const Locale& locale, int32_t kind, int32_t coverage, UErrorCode& status);


  /**
   * (Stop compiler from complaining about hidden overrides.)
   * Since both UnicodeString and Locale have constructors that take const char*, adding a public
   * method that takes UnicodeString causes ambiguity at call sites that use const char*.
   * We really need a flag that is understood by all compilers that will suppress the warning about
   * hidden overrides.
   */
  virtual URegistryKey registerInstance(UObject* objToAdopt, const UnicodeString& locale, UBool visible, UErrorCode& status);

  /**
   * Convenience method for callers using locales.  This returns the standard
   * service ID enumeration.
   */
  virtual StringEnumeration* getAvailableLocales(void) const;

 protected:

  /**
   * Return the name of the current fallback locale.  If it has changed since this was
   * last accessed, the service cache is cleared.
   */
  const UnicodeString& validateFallbackLocale() const;

  /**
   * Override superclass createKey method.
   */
  virtual ICUServiceKey* createKey(const UnicodeString* id, UErrorCode& status) const;

  /**
   * Additional createKey that takes a kind.
   */
  virtual ICUServiceKey* createKey(const UnicodeString* id, int32_t kind, UErrorCode& status) const;

  friend class ServiceEnumeration;
};

U_NAMESPACE_END

    /* UCONFIG_NO_SERVICE */
#endif

    /* ICULSERV_H */
#endif

