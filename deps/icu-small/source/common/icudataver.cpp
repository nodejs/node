// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 2009-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/icudataver.h"
#include "unicode/ures.h"
#include "uresimp.h" /* for ures_getVersionByKey */

U_CAPI void U_EXPORT2 u_getDataVersion(UVersionInfo dataVersionFillin, UErrorCode *status) {
    UResourceBundle *icudatares = nullptr;
    
    if (U_FAILURE(*status)) {
        return;
    }
    
    if (dataVersionFillin != nullptr) {
        icudatares = ures_openDirect(nullptr, U_ICU_VERSION_BUNDLE , status);
        if (U_SUCCESS(*status)) {
            ures_getVersionByKey(icudatares, U_ICU_DATA_KEY, dataVersionFillin, status);
        }
        ures_close(icudatares);
    }
}
