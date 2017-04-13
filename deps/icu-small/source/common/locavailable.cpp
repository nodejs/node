// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1997-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  locavailable.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010feb25
*   created by: Markus W. Scherer
*
*   Code for available locales, separated out from other .cpp files
*   that then do not depend on resource bundle code and res_index bundles.
*/

#include "unicode/utypes.h"
#include "unicode/locid.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "cmemory.h"
#include "ucln_cmn.h"
#include "uassert.h"
#include "umutex.h"
#include "uresimp.h"

// C++ API ----------------------------------------------------------------- ***

U_NAMESPACE_BEGIN

static icu::Locale*  availableLocaleList = NULL;
static int32_t  availableLocaleListCount;
static icu::UInitOnce gInitOnce = U_INITONCE_INITIALIZER;

U_NAMESPACE_END

U_CDECL_BEGIN

static UBool U_CALLCONV locale_available_cleanup(void)
{
    U_NAMESPACE_USE

    if (availableLocaleList) {
        delete []availableLocaleList;
        availableLocaleList = NULL;
    }
    availableLocaleListCount = 0;
    gInitOnce.reset();

    return TRUE;
}

U_CDECL_END

U_NAMESPACE_BEGIN

void U_CALLCONV locale_available_init() {
    // This function is a friend of class Locale.
    // This function is only invoked via umtx_initOnce().

    // for now, there is a hardcoded list, so just walk through that list and set it up.
    //  Note: this function is a friend of class Locale.
    availableLocaleListCount = uloc_countAvailable();
    if(availableLocaleListCount) {
       availableLocaleList = new Locale[availableLocaleListCount];
    }
    if (availableLocaleList == NULL) {
        availableLocaleListCount= 0;
    }
    for (int32_t locCount=availableLocaleListCount-1; locCount>=0; --locCount) {
        availableLocaleList[locCount].setFromPOSIXID(uloc_getAvailable(locCount));
    }
    ucln_common_registerCleanup(UCLN_COMMON_LOCALE_AVAILABLE, locale_available_cleanup);
}

const Locale* U_EXPORT2
Locale::getAvailableLocales(int32_t& count)
{
    umtx_initOnce(gInitOnce, &locale_available_init);
    count = availableLocaleListCount;
    return availableLocaleList;
}


U_NAMESPACE_END

// C API ------------------------------------------------------------------- ***

U_NAMESPACE_USE

/* ### Constants **************************************************/

/* These strings describe the resources we attempt to load from
 the locale ResourceBundle data file.*/
static const char _kIndexLocaleName[] = "res_index";
static const char _kIndexTag[]        = "InstalledLocales";

static char** _installedLocales = NULL;
static int32_t _installedLocalesCount = 0;
static icu::UInitOnce _installedLocalesInitOnce;

/* ### Get available **************************************************/

static UBool U_CALLCONV uloc_cleanup(void) {
    char ** temp;

    if (_installedLocales) {
        temp = _installedLocales;
        _installedLocales = NULL;

        _installedLocalesCount = 0;
        _installedLocalesInitOnce.reset();

        uprv_free(temp);
    }
    return TRUE;
}

// Load Installed Locales. This function will be called exactly once
//   via the initOnce mechanism.

static void U_CALLCONV loadInstalledLocales() {
    UResourceBundle *indexLocale = NULL;
    UResourceBundle installed;
    UErrorCode status = U_ZERO_ERROR;
    int32_t i = 0;
    int32_t localeCount;

    U_ASSERT(_installedLocales == NULL);
    U_ASSERT(_installedLocalesCount == 0);

    _installedLocalesCount = 0;
    ures_initStackObject(&installed);
    indexLocale = ures_openDirect(NULL, _kIndexLocaleName, &status);
    ures_getByKey(indexLocale, _kIndexTag, &installed, &status);

    if(U_SUCCESS(status)) {
        localeCount = ures_getSize(&installed);
        _installedLocales = (char **) uprv_malloc(sizeof(char*) * (localeCount+1));
        if (_installedLocales != NULL) {
            ures_resetIterator(&installed);
            while(ures_hasNext(&installed)) {
                ures_getNextString(&installed, NULL, (const char **)&_installedLocales[i++], &status);
            }
            _installedLocales[i] = NULL;
            _installedLocalesCount = localeCount;
            ucln_common_registerCleanup(UCLN_COMMON_ULOC, uloc_cleanup);
        }
    }
    ures_close(&installed);
    ures_close(indexLocale);
}

static void _load_installedLocales()
{
    umtx_initOnce(_installedLocalesInitOnce, &loadInstalledLocales);
}

U_CAPI const char* U_EXPORT2
uloc_getAvailable(int32_t offset)
{

    _load_installedLocales();

    if (offset > _installedLocalesCount)
        return NULL;
    return _installedLocales[offset];
}

U_CAPI int32_t  U_EXPORT2
uloc_countAvailable()
{
    _load_installedLocales();
    return _installedLocalesCount;
}
