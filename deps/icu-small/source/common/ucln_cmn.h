// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2001-2016, International Business Machines
*                Corporation and others. All Rights Reserved.
******************************************************************************
*   file name:  ucln_cmn.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001July05
*   created by: George Rhoten
*/

#ifndef __UCLN_CMN_H__
#define __UCLN_CMN_H__

#include "unicode/utypes.h"
#include "ucln.h"

/* These are the cleanup functions for various APIs. */
/* @return true if cleanup complete successfully.*/
U_CFUNC UBool umtx_cleanup(void);

U_CFUNC UBool utrace_cleanup(void);

U_CFUNC UBool ucln_lib_cleanup(void);

/*
Please keep the order of enums declared in same order
as the cleanup functions are suppose to be called. */
typedef enum ECleanupCommonType {
    UCLN_COMMON_START = -1,
    UCLN_COMMON_NUMPARSE_UNISETS,
    UCLN_COMMON_USPREP,
    UCLN_COMMON_BREAKITERATOR,
    UCLN_COMMON_RBBI,
    UCLN_COMMON_SERVICE,
    UCLN_COMMON_LOCALE_KEY_TYPE,
    UCLN_COMMON_LOCALE,
    UCLN_COMMON_LOCALE_AVAILABLE,
    UCLN_COMMON_ULOC,
    UCLN_COMMON_CURRENCY,
    UCLN_COMMON_LOADED_NORMALIZER2,
    UCLN_COMMON_NORMALIZER2,
    UCLN_COMMON_USET,
    UCLN_COMMON_UNAMES,
    UCLN_COMMON_UPROPS,
    UCLN_COMMON_UCNV,
    UCLN_COMMON_UCNV_IO,
    UCLN_COMMON_UDATA,
    UCLN_COMMON_PUTIL,
    UCLN_COMMON_LIST_FORMATTER,
    UCLN_COMMON_UINIT,

    /*
       Unified caches caches collation stuff. Collation data structures
       contain resource bundles which means that unified cache cleanup
       must happen before resource bundle clean up.
    */
    UCLN_COMMON_UNIFIED_CACHE,
    UCLN_COMMON_URES,
    UCLN_COMMON_COUNT /* This must be last */
} ECleanupCommonType;

/* Main library cleanup registration function. */
/* See common/ucln.h for details on adding a cleanup function. */
/* Note: the global mutex must not be held when calling this function. */
U_CFUNC void U_EXPORT2 ucln_common_registerCleanup(ECleanupCommonType type,
                                                   cleanupFunc *func);

#endif
