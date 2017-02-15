// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  ustrcase_locale.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011may31
*   created by: Markus W. Scherer
*
*   Locale-sensitive case mapping functions (ones that call uloc_getDefault())
*   were moved here to break dependency cycles among parts of the common library.
*/

#include "unicode/utypes.h"
#include "unicode/ucasemap.h"
#include "unicode/uloc.h"
#include "unicode/ustring.h"
#include "ucase.h"
#include "ustr_imp.h"

U_CFUNC void
ustrcase_setTempCaseMapLocale(UCaseMap *csm, const char *locale) {
    /*
     * We could call ucasemap_setLocale(), but here we really only care about
     * the initial language subtag, we need not return the real string via
     * ucasemap_getLocale(), and we don't care about only getting "x" from
     * "x-some-thing" etc.
     *
     * We ignore locales with a longer-than-3 initial subtag.
     *
     * We also do not fill in the locCache because it is rarely used,
     * and not worth setting unless we reuse it for many case mapping operations.
     * (That's why UCaseMap was created.)
     */
    int i;
    char c;

    /* the internal functions require locale!=NULL */
    if(locale==NULL) {
        // Do not call uprv_getDefaultLocaleID() because that does not see
        // changes to the default locale via uloc_setDefault().
        // It would also be inefficient if used frequently because uprv_getDefaultLocaleID()
        // does not cache the locale ID.
        //
        // Unfortunately, uloc_getDefault() has many dependencies.
        // We only care about a small set of language subtags,
        // and we do not need the locale ID to be canonicalized.
        //
        // Best is to not call case mapping functions with a NULL locale ID.
        locale=uloc_getDefault();
    }
    for(i=0; i<4 && (c=locale[i])!=0 && c!='-' && c!='_'; ++i) {
        csm->locale[i]=c;
    }
    if(i<=3) {
        csm->locale[i]=0;  /* Up to 3 non-separator characters. */
    } else {
        csm->locale[0]=0;  /* Longer-than-3 initial subtag: Ignore. */
    }
}

/*
 * Set parameters on an empty UCaseMap, for UCaseMap-less API functions.
 * Do this fast because it is called with every function call.
 */
static inline void
setTempCaseMap(UCaseMap *csm, const char *locale) {
    if(csm->csp==NULL) {
        csm->csp=ucase_getSingleton();
    }
    if(locale!=NULL && locale[0]==0) {
        csm->locale[0]=0;
    } else {
        ustrcase_setTempCaseMapLocale(csm, locale);
    }
}

/* public API functions */

U_CAPI int32_t U_EXPORT2
u_strToLower(UChar *dest, int32_t destCapacity,
             const UChar *src, int32_t srcLength,
             const char *locale,
             UErrorCode *pErrorCode) {
    UCaseMap csm=UCASEMAP_INITIALIZER;
    setTempCaseMap(&csm, locale);
    return ustrcase_map(
        &csm,
        dest, destCapacity,
        src, srcLength,
        ustrcase_internalToLower, pErrorCode);
}

U_CAPI int32_t U_EXPORT2
u_strToUpper(UChar *dest, int32_t destCapacity,
             const UChar *src, int32_t srcLength,
             const char *locale,
             UErrorCode *pErrorCode) {
    UCaseMap csm=UCASEMAP_INITIALIZER;
    setTempCaseMap(&csm, locale);
    return ustrcase_map(
        &csm,
        dest, destCapacity,
        src, srcLength,
        ustrcase_internalToUpper, pErrorCode);
}
