// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2004-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: January 16 2004
* Since: ICU 2.8
**********************************************************************
*/
#ifndef LOCBASED_H
#define LOCBASED_H

#include "unicode/locid.h"
#include "unicode/uobject.h"

/**
 * Macro to declare a locale LocaleBased wrapper object for the given
 * object, which must have two members named `validLocale' and
 * `actualLocale' of which are pointers to the internal icu::CharString.
 */
#define U_LOCALE_BASED(varname, objname) \
  LocaleBased varname((objname).validLocale, (objname).actualLocale)

U_NAMESPACE_BEGIN

class CharString;
/**
 * A utility class that unifies the implementation of getLocale() by
 * various ICU services.  This class is likely to be removed in the
 * ICU 3.0 time frame in favor of an integrated approach with the
 * services framework.
 * @since ICU 2.8
 */
class U_COMMON_API LocaleBased : public UMemory {

 public:

    /**
     * Construct a LocaleBased wrapper around the two pointers.  These
     * will be aliased for the lifetime of this object.
     */
    inline LocaleBased(CharString*& validAlias, CharString*& actualAlias);

    /**
     * Return locale meta-data for the service object wrapped by this
     * object.  Either the valid or the actual locale may be
     * retrieved.
     * @param valid The valid locale.
     * @param actual The actual locale.
     * @param type either ULOC_VALID_LOCALE or ULOC_ACTUAL_LOCALE
     * @param status input-output error code
     * @return the indicated locale
     */
    static Locale getLocale(
        const CharString* valid, const CharString* actual,
        ULocDataLocaleType type, UErrorCode& status);

    /**
     * Return the locale ID for the service object wrapped by this
     * object.  Either the valid or the actual locale may be
     * retrieved.
     * @param valid The valid locale.
     * @param actual The actual locale.
     * @param type either ULOC_VALID_LOCALE or ULOC_ACTUAL_LOCALE
     * @param status input-output error code
     * @return the indicated locale ID
     */
    static const char* getLocaleID(
        const CharString* valid, const CharString* actual,
        ULocDataLocaleType type, UErrorCode& status);

    /**
     * Set the locale meta-data for the service object wrapped by this
     * object.  If either parameter is zero, it is ignored.
     * @param valid the ID of the valid locale
     * @param actual the ID of the actual locale
     */
    void setLocaleIDs(const char* valid, const char* actual, UErrorCode& status);
    void setLocaleIDs(const CharString* valid, const CharString* actual, UErrorCode& status);

    static void setLocaleID(const char* id, CharString*& dest, UErrorCode& status);
    static void setLocaleID(const CharString* id, CharString*& dest, UErrorCode& status);

    static bool equalIDs(const CharString* left, const CharString* right);

 private:

    void setValidLocaleID(const CharString* id, UErrorCode& status);
    void setActualLocaleID(const CharString* id, UErrorCode& status);
    void setValidLocaleID(const char* id, UErrorCode& status);
    void setActualLocaleID(const char* id, UErrorCode& status);

    CharString*& valid;
    CharString*& actual;
};

inline LocaleBased::LocaleBased(CharString*& validAlias, CharString*& actualAlias) :
    valid(validAlias), actual(actualAlias) {
}

inline void LocaleBased::setValidLocaleID(const CharString* id, UErrorCode& status) {
    setLocaleID(id, valid, status);
}
inline void LocaleBased::setActualLocaleID(const CharString* id, UErrorCode& status) {
    setLocaleID(id, actual, status);
}
inline void LocaleBased::setValidLocaleID(const char* id, UErrorCode& status) {
    setLocaleID(id, valid, status);
}
inline void LocaleBased::setActualLocaleID(const char* id, UErrorCode& status) {
    setLocaleID(id, actual, status);
}

U_NAMESPACE_END

#endif
