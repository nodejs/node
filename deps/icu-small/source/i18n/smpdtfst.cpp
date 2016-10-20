// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2009-2013, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* This file contains the class SimpleDateFormatStaticSets
*
* SimpleDateFormatStaticSets holds the UnicodeSets that are needed for lenient
* parsing of literal characters in date/time strings.
********************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uniset.h"
#include "unicode/udat.h"
#include "cmemory.h"
#include "uassert.h"
#include "ucln_in.h"
#include "umutex.h"


#include "smpdtfst.h"

U_NAMESPACE_BEGIN

SimpleDateFormatStaticSets *gStaticSets = NULL;
UInitOnce gSimpleDateFormatStaticSetsInitOnce = U_INITONCE_INITIALIZER;

SimpleDateFormatStaticSets::SimpleDateFormatStaticSets(UErrorCode &status)
: fDateIgnorables(NULL),
  fTimeIgnorables(NULL),
  fOtherIgnorables(NULL)
{
    fDateIgnorables  = new UnicodeSet(UNICODE_STRING("[-,./[:whitespace:]]", 20), status);
    fTimeIgnorables  = new UnicodeSet(UNICODE_STRING("[-.:[:whitespace:]]", 19),  status);
    fOtherIgnorables = new UnicodeSet(UNICODE_STRING("[:whitespace:]", 14),       status);

    // Check for null pointers
    if (fDateIgnorables == NULL || fTimeIgnorables == NULL || fOtherIgnorables == NULL) {
        goto ExitConstrDeleteAll;
    }

    // Freeze all the sets
    fDateIgnorables->freeze();
    fTimeIgnorables->freeze();
    fOtherIgnorables->freeze();

    return; // If we reached this point, everything is fine so just exit

ExitConstrDeleteAll: // Remove all sets and return error
    delete fDateIgnorables;  fDateIgnorables = NULL;
    delete fTimeIgnorables;  fTimeIgnorables = NULL;
    delete fOtherIgnorables; fOtherIgnorables = NULL;

    status = U_MEMORY_ALLOCATION_ERROR;
}


SimpleDateFormatStaticSets::~SimpleDateFormatStaticSets() {
    delete fDateIgnorables;  fDateIgnorables = NULL;
    delete fTimeIgnorables;  fTimeIgnorables = NULL;
    delete fOtherIgnorables; fOtherIgnorables = NULL;
}


//------------------------------------------------------------------------------
//
//   smpdtfmt_cleanup     Memory cleanup function, free/delete all
//                      cached memory.  Called by ICU's u_cleanup() function.
//
//------------------------------------------------------------------------------
UBool
SimpleDateFormatStaticSets::cleanup(void)
{
    delete gStaticSets;
    gStaticSets = NULL;
    gSimpleDateFormatStaticSetsInitOnce.reset();
    return TRUE;
}

U_CDECL_BEGIN
static UBool U_CALLCONV
smpdtfmt_cleanup(void)
{
    return SimpleDateFormatStaticSets::cleanup();
}

static void U_CALLCONV smpdtfmt_initSets(UErrorCode &status) {
    ucln_i18n_registerCleanup(UCLN_I18N_SMPDTFMT, smpdtfmt_cleanup);
    U_ASSERT(gStaticSets == NULL);
    gStaticSets = new SimpleDateFormatStaticSets(status);
    if (gStaticSets == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}

U_CDECL_END

UnicodeSet *SimpleDateFormatStaticSets::getIgnorables(UDateFormatField fieldIndex)
{
    UErrorCode status = U_ZERO_ERROR;
    umtx_initOnce(gSimpleDateFormatStaticSetsInitOnce, &smpdtfmt_initSets, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    
    switch (fieldIndex) {
        case UDAT_YEAR_FIELD:
        case UDAT_MONTH_FIELD:
        case UDAT_DATE_FIELD:
        case UDAT_STANDALONE_DAY_FIELD:
        case UDAT_STANDALONE_MONTH_FIELD:
            return gStaticSets->fDateIgnorables;
            
        case UDAT_HOUR_OF_DAY1_FIELD:
        case UDAT_HOUR_OF_DAY0_FIELD:
        case UDAT_MINUTE_FIELD:
        case UDAT_SECOND_FIELD:
        case UDAT_HOUR1_FIELD:
        case UDAT_HOUR0_FIELD:
            return gStaticSets->fTimeIgnorables;
            
        default:
            return gStaticSets->fOtherIgnorables;
    }
}

U_NAMESPACE_END

#endif // #if !UCONFIG_NO_FORMATTING
