// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1998-2004, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
*  ucnvdisp.c:
*  Implements APIs for the ICU's codeset conversion library display names.
*
* Modification History:
*
*   Date        Name        Description
*   04/04/99    helena      Fixed internal header inclusion.
*   05/09/00    helena      Added implementation to handle fallback mappings.
*   06/20/2000  helena      OS/400 port changes; mostly typecast.
*   09/08/2004  grhoten     split from ucnv.c
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "unicode/ustring.h"
#include "unicode/ures.h"
#include "unicode/ucnv.h"
#include "cstring.h"
#include "ustr_imp.h"
#include "ucnv_imp.h"
#include "putilimp.h"

U_CAPI int32_t U_EXPORT2
ucnv_getDisplayName(const UConverter *cnv,
                    const char *displayLocale,
                    UChar *displayName, int32_t displayNameCapacity,
                    UErrorCode *pErrorCode) {
    UResourceBundle *rb;
    const UChar *name;
    int32_t length;
    UErrorCode localStatus = U_ZERO_ERROR;

    /* check arguments */
    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)) {
        return 0;
    }

    if(cnv==NULL || displayNameCapacity<0 || (displayNameCapacity>0 && displayName==NULL)) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    /* open the resource bundle and get the display name string */
    rb=ures_open(NULL, displayLocale, pErrorCode);
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }

    /* use the internal name as the key */
    name=ures_getStringByKey(rb, cnv->sharedData->staticData->name, &length, &localStatus);
    ures_close(rb);

    if(U_SUCCESS(localStatus)) {
        /* copy the string */
        if (*pErrorCode == U_ZERO_ERROR) {
            *pErrorCode = localStatus;
        }
        u_memcpy(displayName, name, uprv_min(length, displayNameCapacity)*U_SIZEOF_UCHAR);
    } else {
        /* convert the internal name into a Unicode string */
        length=(int32_t)uprv_strlen(cnv->sharedData->staticData->name);
        u_charsToUChars(cnv->sharedData->staticData->name, displayName, uprv_min(length, displayNameCapacity));
    }
    return u_terminateUChars(displayName, displayNameCapacity, length, pErrorCode);
}

#endif

/*
 * Hey, Emacs, please set the following:
 *
 * Local Variables:
 * indent-tabs-mode: nil
 * End:
 *
 */
