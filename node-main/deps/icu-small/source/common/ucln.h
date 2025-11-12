// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
* Copyright (C) 2001-2013, International Business Machines
*                Corporation and others. All Rights Reserved.
*
******************************************************************************
*   file name:  ucln.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001July05
*   created by: George Rhoten
*/

#ifndef __UCLN_H__
#define __UCLN_H__

#include "unicode/utypes.h"

/** These are the functions used to register a library's memory cleanup
 * functions.  Each library should define a single library register function
 * to call this API.  In the i18n library, it is ucln_i18n_registerCleanup().
 *
 * None of the cleanup functions should use a mutex to clean up an API's
 * allocated memory because a cleanup function is not meant to be thread safe,
 * and plenty of data cannot be reference counted in order to make sure that
 * no one else needs the allocated data.
 *
 * In order to make a cleanup function get called when u_cleanup is called,
 * You should add your function to the library specific cleanup function.
 * If the cleanup function is not in the common library, the code that
 * allocates the memory should call the library specific cleanup function.
 * For instance, in the i18n library, any memory allocated statically must
 * call ucln_i18n_registerCleanup() from the ucln_in.h header.  These library
 * cleanup functions are needed in order to prevent a circular dependency
 * between the common library and any other library.
 *
 * The order of the cleanup is very important.  In general, an API that
 * depends on a second API should be cleaned up before the second API.
 * For instance, the default converter in ustring depends upon the converter
 * API.  So the default converter should be closed before the converter API
 * has its cache flushed.  This will prevent any memory leaks due to
 * reference counting.
 *
 * Please see common/ucln_cmn.{h,c} and i18n/ucln_in.{h,c} for examples.
 */

/**
 * Data Type for cleanup function selector. These roughly correspond to libraries.
 */
typedef enum ECleanupLibraryType {
    UCLN_START = -1,
    UCLN_UPLUG,     /* ICU plugins */
    UCLN_CUSTOM,    /* Custom is for anyone else. */
    UCLN_CTESTFW,
    UCLN_TOOLUTIL,
    UCLN_LAYOUTEX,
    UCLN_LAYOUT,
    UCLN_IO,
    UCLN_I18N,
    UCLN_COMMON /* This must be the last one to cleanup. */
} ECleanupLibraryType;

/**
 * Data type for cleanup function pointer
 */
U_CDECL_BEGIN
typedef UBool U_CALLCONV cleanupFunc(void);
typedef void U_CALLCONV initFunc(UErrorCode *);
U_CDECL_END

/**
 * Register a cleanup function
 * @param type which library to register for.
 * @param func the function pointer
 */
U_CAPI void U_EXPORT2 ucln_registerCleanup(ECleanupLibraryType type,
                                           cleanupFunc *func);

/**
 * Request cleanup for one specific library.
 * Not thread safe.
 * @param type which library to cleanup
 */
U_CAPI void U_EXPORT2 ucln_cleanupOne(ECleanupLibraryType type);

#endif
