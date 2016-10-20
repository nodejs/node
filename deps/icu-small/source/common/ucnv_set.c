// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2003-2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  ucnv_set.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2004sep07
*   created by: Markus W. Scherer
*
*   Conversion API functions using USet (ucnv_getUnicodeSet())
*   moved here from ucnv.c for removing the dependency of other ucnv_
*   implementation functions on the USet implementation.
*/

#include "unicode/utypes.h"
#include "unicode/uset.h"
#include "unicode/ucnv.h"
#include "ucnv_bld.h"
#include "uset_imp.h"

#if !UCONFIG_NO_CONVERSION

U_CAPI void U_EXPORT2
ucnv_getUnicodeSet(const UConverter *cnv,
                   USet *setFillIn,
                   UConverterUnicodeSet whichSet,
                   UErrorCode *pErrorCode) {
    /* argument checking */
    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)) {
        return;
    }
    if(cnv==NULL || setFillIn==NULL || whichSet<UCNV_ROUNDTRIP_SET || UCNV_SET_COUNT<=whichSet) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    /* does this converter support this function? */
    if(cnv->sharedData->impl->getUnicodeSet==NULL) {
        *pErrorCode=U_UNSUPPORTED_ERROR;
        return;
    }

    {
        USetAdder sa={
            NULL,
            uset_add,
            uset_addRange,
            uset_addString,
            uset_remove,
            uset_removeRange
        };
        sa.set=setFillIn;

        /* empty the set */
        uset_clear(setFillIn);

        /* call the converter to add the code points it supports */
        cnv->sharedData->impl->getUnicodeSet(cnv, &sa, whichSet, pErrorCode);
    }
}

#endif
