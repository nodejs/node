// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1997-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  loclikely.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010feb25
*   created by: Markus W. Scherer
*
*   Code for miscellaneous locale-related resource bundle data access,
*   separated out from other .cpp files
*   that then do not depend on resource bundle code and this data.
*/

#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "cstring.h"
#include "ulocimp.h"
#include "uresimp.h"

/*
 * Lookup a resource bundle table item with fallback on the table level.
 * Regular resource bundle lookups perform fallback to parent locale bundles
 * and eventually the root bundle, but only for top-level items.
 * This function takes the name of a top-level table and of an item in that table
 * and performs a lookup of both, falling back until a bundle contains a table
 * with this item.
 *
 * Note: Only the opening of entire bundles falls back through the default locale
 * before root. Once a bundle is open, item lookups do not go through the
 * default locale because that would result in a mix of languages that is
 * unpredictable to the programmer and most likely useless.
 */
U_CAPI const UChar * U_EXPORT2
uloc_getTableStringWithFallback(const char *path, const char *locale,
                              const char *tableKey, const char *subTableKey,
                              const char *itemKey,
                              int32_t *pLength,
                              UErrorCode *pErrorCode)
{
/*    char localeBuffer[ULOC_FULLNAME_CAPACITY*4];*/
    UResourceBundle *rb=NULL, table, subTable;
    const UChar *item=NULL;
    UErrorCode errorCode;
    char explicitFallbackName[ULOC_FULLNAME_CAPACITY] = {0};

    /*
     * open the bundle for the current locale
     * this falls back through the locale's chain to root
     */
    errorCode=U_ZERO_ERROR;
    rb=ures_open(path, locale, &errorCode);

    if(U_FAILURE(errorCode)) {
        /* total failure, not even root could be opened */
        *pErrorCode=errorCode;
        return NULL;
    } else if(errorCode==U_USING_DEFAULT_WARNING ||
                (errorCode==U_USING_FALLBACK_WARNING && *pErrorCode!=U_USING_DEFAULT_WARNING)
    ) {
        /* set the "strongest" error code (success->fallback->default->failure) */
        *pErrorCode=errorCode;
    }

    for(;;){
        ures_initStackObject(&table);
        ures_initStackObject(&subTable);
        ures_getByKeyWithFallback(rb, tableKey, &table, &errorCode);

        if (subTableKey != NULL) {
            /*
            ures_getByKeyWithFallback(&table,subTableKey, &subTable, &errorCode);
            item = ures_getStringByKeyWithFallback(&subTable, itemKey, pLength, &errorCode);
            if(U_FAILURE(errorCode)){
                *pErrorCode = errorCode;
            }

            break;*/

            ures_getByKeyWithFallback(&table,subTableKey, &table, &errorCode);
        }
        if(U_SUCCESS(errorCode)){
            item = ures_getStringByKeyWithFallback(&table, itemKey, pLength, &errorCode);
            if(U_FAILURE(errorCode)){
                const char* replacement = NULL;
                *pErrorCode = errorCode; /*save the errorCode*/
                errorCode = U_ZERO_ERROR;
                /* may be a deprecated code */
                if(uprv_strcmp(tableKey, "Countries")==0){
                    replacement =  uloc_getCurrentCountryID(itemKey);
                }else if(uprv_strcmp(tableKey, "Languages")==0){
                    replacement =  uloc_getCurrentLanguageID(itemKey);
                }
                /*pointer comparison is ok since uloc_getCurrentCountryID & uloc_getCurrentLanguageID return the key itself is replacement is not found*/
                if(replacement!=NULL && itemKey != replacement){
                    item = ures_getStringByKeyWithFallback(&table, replacement, pLength, &errorCode);
                    if(U_SUCCESS(errorCode)){
                        *pErrorCode = errorCode;
                        break;
                    }
                }
            }else{
                break;
            }
        }

        if(U_FAILURE(errorCode)){

            /* still can't figure out ?.. try the fallback mechanism */
            int32_t len = 0;
            const UChar* fallbackLocale =  NULL;
            *pErrorCode = errorCode;
            errorCode = U_ZERO_ERROR;

            fallbackLocale = ures_getStringByKeyWithFallback(&table, "Fallback", &len, &errorCode);
            if(U_FAILURE(errorCode)){
               *pErrorCode = errorCode;
                break;
            }

            u_UCharsToChars(fallbackLocale, explicitFallbackName, len);

            /* guard against recursive fallback */
            if(uprv_strcmp(explicitFallbackName, locale)==0){
                *pErrorCode = U_INTERNAL_PROGRAM_ERROR;
                break;
            }
            ures_close(rb);
            rb = ures_open(path, explicitFallbackName, &errorCode);
            if(U_FAILURE(errorCode)){
                *pErrorCode = errorCode;
                break;
            }
            /* succeeded in opening the fallback bundle .. continue and try to fetch the item */
        }else{
            break;
        }
    }
    /* done with the locale string - ready to close table and rb */
    ures_close(&subTable);
    ures_close(&table);
    ures_close(rb);
    return item;
}

static ULayoutType
_uloc_getOrientationHelper(const char* localeId,
                           const char* key,
                           UErrorCode *status)
{
    ULayoutType result = ULOC_LAYOUT_UNKNOWN;

    if (!U_FAILURE(*status)) {
        int32_t length = 0;
        char localeBuffer[ULOC_FULLNAME_CAPACITY];

        uloc_canonicalize(localeId, localeBuffer, sizeof(localeBuffer), status);

        if (!U_FAILURE(*status)) {
            const UChar* const value =
                uloc_getTableStringWithFallback(
                    NULL,
                    localeBuffer,
                    "layout",
                    NULL,
                    key,
                    &length,
                    status);

            if (!U_FAILURE(*status) && length != 0) {
                switch(value[0])
                {
                case 0x0062: /* 'b' */
                    result = ULOC_LAYOUT_BTT;
                    break;
                case 0x006C: /* 'l' */
                    result = ULOC_LAYOUT_LTR;
                    break;
                case 0x0072: /* 'r' */
                    result = ULOC_LAYOUT_RTL;
                    break;
                case 0x0074: /* 't' */
                    result = ULOC_LAYOUT_TTB;
                    break;
                default:
                    *status = U_INTERNAL_PROGRAM_ERROR;
                    break;
                }
            }
        }
    }

    return result;
}

U_CAPI ULayoutType U_EXPORT2
uloc_getCharacterOrientation(const char* localeId,
                             UErrorCode *status)
{
    return _uloc_getOrientationHelper(localeId, "characters", status);
}

/**
 * Get the layout line orientation for the specified locale.
 *
 * @param localeID locale name
 * @param status Error status
 * @return an enum indicating the layout orientation for lines.
 */
U_CAPI ULayoutType U_EXPORT2
uloc_getLineOrientation(const char* localeId,
                        UErrorCode *status)
{
    return _uloc_getOrientationHelper(localeId, "lines", status);
}
