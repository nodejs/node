// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2001-2016, International Business Machines
*                Corporation and others. All Rights Reserved.
******************************************************************************
*   file name:  ucln_in.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001July05
*   created by: George Rhoten
*/

#ifndef __UCLN_IN_H__
#define __UCLN_IN_H__

#include "unicode/utypes.h"
#include "ucln.h"

/*
Please keep the order of enums declared in same order
as the functions are suppose to be called.
It's usually best to have child dependencies called first. */
typedef enum ECleanupI18NType {
    UCLN_I18N_START = -1,
    UCLN_I18N_NUMBER_SKELETONS,
    UCLN_I18N_CURRENCY_SPACING,
    UCLN_I18N_SPOOF,
    UCLN_I18N_SPOOFDATA,
    UCLN_I18N_TRANSLITERATOR,
    UCLN_I18N_REGEX,
    UCLN_I18N_JAPANESE_CALENDAR,
    UCLN_I18N_ISLAMIC_CALENDAR,
    UCLN_I18N_CHINESE_CALENDAR,
    UCLN_I18N_HEBREW_CALENDAR,
    UCLN_I18N_ASTRO_CALENDAR,
    UCLN_I18N_DANGI_CALENDAR,
    UCLN_I18N_CALENDAR,
    UCLN_I18N_TIMEZONEFORMAT,
    UCLN_I18N_TZDBTIMEZONENAMES,
    UCLN_I18N_TIMEZONEGENERICNAMES,
    UCLN_I18N_TIMEZONENAMES,
    UCLN_I18N_ZONEMETA,
    UCLN_I18N_TIMEZONE,
    UCLN_I18N_DIGITLIST,
    UCLN_I18N_DECFMT,
    UCLN_I18N_NUMFMT,
    UCLN_I18N_ALLOWED_HOUR_FORMATS,
    UCLN_I18N_DAYPERIODRULES,
    UCLN_I18N_SMPDTFMT,
    UCLN_I18N_USEARCH,
    UCLN_I18N_COLLATOR,
    UCLN_I18N_UCOL_RES,
    UCLN_I18N_CSDET,
    UCLN_I18N_COLLATION_ROOT,
    UCLN_I18N_GENDERINFO,
    UCLN_I18N_CDFINFO,
    UCLN_I18N_REGION,
    UCLN_I18N_LIST_FORMATTER,
    UCLN_I18N_NUMSYS,
    UCLN_I18N_COUNT /* This must be last */
} ECleanupI18NType;

/* Main library cleanup registration function. */
/* See common/ucln.h for details on adding a cleanup function. */
/* Note: the global mutex must not be held when calling this function. */
U_CFUNC void U_EXPORT2 ucln_i18n_registerCleanup(ECleanupI18NType type,
                                                 cleanupFunc *func);

#endif
