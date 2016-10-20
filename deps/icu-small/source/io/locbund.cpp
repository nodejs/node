// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1998-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File locbund.cpp
*
* Modification History:
*
*   Date        Name        Description
*   11/18/98    stephen        Creation.
*   12/10/1999  bobbyr(at)optiosoftware.com       Fix for memory leak + string allocation bugs
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_CONVERSION

#include "locbund.h"

#include "cmemory.h"
#include "cstring.h"
#include "ucln_io.h"
#include "mutex.h"
#include "umutex.h"
#include "unicode/ustring.h"
#include "unicode/uloc.h"

static UNumberFormat *gPosixNumberFormat[ULOCALEBUNDLE_NUMBERFORMAT_COUNT];

U_CDECL_BEGIN
static UBool U_CALLCONV locbund_cleanup(void) {
    int32_t style;
    for (style = 0; style < ULOCALEBUNDLE_NUMBERFORMAT_COUNT; style++) {
        unum_close(gPosixNumberFormat[style]);
        gPosixNumberFormat[style] = NULL;
    }
    return TRUE;
}
U_CDECL_END

static UMutex gLock = U_MUTEX_INITIALIZER;
static inline UNumberFormat * copyInvariantFormatter(ULocaleBundle *result, UNumberFormatStyle style) {
    U_NAMESPACE_USE
    Mutex lock(&gLock);
    if (result->fNumberFormat[style-1] == NULL) {
        if (gPosixNumberFormat[style-1] == NULL) {
            UErrorCode status = U_ZERO_ERROR;
            UNumberFormat *formatAlias = unum_open(style, NULL, 0, "en_US_POSIX", NULL, &status);
            if (U_SUCCESS(status)) {
                gPosixNumberFormat[style-1] = formatAlias;
                ucln_io_registerCleanup(UCLN_IO_LOCBUND, locbund_cleanup);
            }
        }
        /* Copy the needed formatter. */
        if (gPosixNumberFormat[style-1] != NULL) {
            UErrorCode status = U_ZERO_ERROR;
            result->fNumberFormat[style-1] = unum_clone(gPosixNumberFormat[style-1], &status);
        }
    }
    return result->fNumberFormat[style-1];
}

U_CAPI ULocaleBundle *
u_locbund_init(ULocaleBundle *result, const char *loc)
{
    int32_t len;

    if(result == 0)
        return 0;

    if (loc == NULL) {
        loc = uloc_getDefault();
    }

    uprv_memset(result, 0, sizeof(ULocaleBundle));

    len = (int32_t)strlen(loc);
    result->fLocale = (char*) uprv_malloc(len + 1);
    if(result->fLocale == 0) {
        return 0;
    }

    uprv_strcpy(result->fLocale, loc);

    result->isInvariantLocale = uprv_strcmp(result->fLocale, "en_US_POSIX") == 0;

    return result;
}

/*U_CAPI ULocaleBundle *
u_locbund_new(const char *loc)
{
    ULocaleBundle *result = (ULocaleBundle*) uprv_malloc(sizeof(ULocaleBundle));
    return u_locbund_init(result, loc);
}

U_CAPI ULocaleBundle *
u_locbund_clone(const ULocaleBundle *bundle)
{
    ULocaleBundle *result = (ULocaleBundle*)uprv_malloc(sizeof(ULocaleBundle));
    UErrorCode status = U_ZERO_ERROR;
    int32_t styleIdx;
    
    if(result == 0)
        return 0;
    
    result->fLocale = (char*) uprv_malloc(strlen(bundle->fLocale) + 1);
    if(result->fLocale == 0) {
        uprv_free(result);
        return 0;
    }
    
    strcpy(result->fLocale, bundle->fLocale );
    
    for (styleIdx = 0; styleIdx < ULOCALEBUNDLE_NUMBERFORMAT_COUNT; styleIdx++) {
        status = U_ZERO_ERROR;
        if (result->fNumberFormat[styleIdx]) {
            result->fNumberFormat[styleIdx] = unum_clone(bundle->fNumberFormat[styleIdx], &status);
            if (U_FAILURE(status)) {
                result->fNumberFormat[styleIdx] = NULL;
            }
        }
        else {
            result->fNumberFormat[styleIdx] = NULL;
        }
    }
    result->fDateFormat         = (bundle->fDateFormat == 0 ? 0 :
        udat_clone(bundle->fDateFormat, &status));
    result->fTimeFormat         = (bundle->fTimeFormat == 0 ? 0 :
        udat_clone(bundle->fTimeFormat, &status));
    
    return result;
}*/

U_CAPI void
u_locbund_close(ULocaleBundle *bundle)
{
    int32_t styleIdx;

    uprv_free(bundle->fLocale);
    
    for (styleIdx = 0; styleIdx < ULOCALEBUNDLE_NUMBERFORMAT_COUNT; styleIdx++) {
        if (bundle->fNumberFormat[styleIdx]) {
            unum_close(bundle->fNumberFormat[styleIdx]);
        }
    }
    
    uprv_memset(bundle, 0, sizeof(ULocaleBundle));
/*    uprv_free(bundle);*/
}

U_CAPI UNumberFormat *
u_locbund_getNumberFormat(ULocaleBundle *bundle, UNumberFormatStyle style)
{
    UNumberFormat *formatAlias = NULL;
    if (style > UNUM_IGNORE) {
        formatAlias = bundle->fNumberFormat[style-1];
        if (formatAlias == NULL) {
            if (bundle->isInvariantLocale) {
                formatAlias = copyInvariantFormatter(bundle, style);
            }
            else {
                UErrorCode status = U_ZERO_ERROR;
                formatAlias = unum_open(style, NULL, 0, bundle->fLocale, NULL, &status);
                if (U_FAILURE(status)) {
                    unum_close(formatAlias);
                    formatAlias = NULL;
                }
                else {
                    bundle->fNumberFormat[style-1] = formatAlias;
                }
            }
        }
    }
    return formatAlias;
}

#endif /* #if !UCONFIG_NO_FORMATTING */
