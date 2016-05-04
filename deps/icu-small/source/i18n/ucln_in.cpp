/*
******************************************************************************
*                                                                            *
* Copyright (C) 2001-2014, International Business Machines                   *
*                Corporation and others. All Rights Reserved.                *
*                                                                            *
******************************************************************************
*   file name:  ucln_in.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001July05
*   created by: George Rhoten
*/

#include "ucln.h"
#include "ucln_in.h"
#include "mutex.h"
#include "uassert.h"

/**  Auto-client for UCLN_I18N **/
#define UCLN_TYPE UCLN_I18N
#include "ucln_imp.h"

/* Leave this copyright notice here! It needs to go somewhere in this library. */
static const char copyright[] = U_COPYRIGHT_STRING;

static cleanupFunc *gCleanupFunctions[UCLN_I18N_COUNT];

static UBool i18n_cleanup(void)
{
    int32_t libType = UCLN_I18N_START;
    (void)copyright;   /* Suppress unused variable warning with clang. */

    while (++libType<UCLN_I18N_COUNT) {
        if (gCleanupFunctions[libType])
        {
            gCleanupFunctions[libType]();
            gCleanupFunctions[libType] = NULL;
        }
    }
#if !UCLN_NO_AUTO_CLEANUP && (defined(UCLN_AUTO_ATEXIT) || defined(UCLN_AUTO_LOCAL))
    ucln_unRegisterAutomaticCleanup();
#endif
    return TRUE;
}

void ucln_i18n_registerCleanup(ECleanupI18NType type,
                               cleanupFunc *func) {
    U_ASSERT(UCLN_I18N_START < type && type < UCLN_I18N_COUNT);
    {
        icu::Mutex m;   // See ticket 10295 for discussion.
        ucln_registerCleanup(UCLN_I18N, i18n_cleanup);
        if (UCLN_I18N_START < type && type < UCLN_I18N_COUNT) {
            gCleanupFunctions[type] = func;
        }
    }
#if !UCLN_NO_AUTO_CLEANUP && (defined(UCLN_AUTO_ATEXIT) || defined(UCLN_AUTO_LOCAL))
    ucln_registerAutomaticCleanup();
#endif
}

