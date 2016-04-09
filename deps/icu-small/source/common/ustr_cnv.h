/*  
**********************************************************************
*   Copyright (C) 1999-2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   file name:  ustr_cnv.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2004Aug27
*   created by: George Rhoten
*/

#ifndef USTR_CNV_IMP_H
#define USTR_CNV_IMP_H

#include "unicode/utypes.h"
#include "unicode/ucnv.h"

#if !UCONFIG_NO_CONVERSION

/**
 * Get the default converter. This is a commonly used converter
 * that is used for the ustring and UnicodeString API.
 * Remember to use the u_releaseDefaultConverter when you are done.
 * @internal
 */
U_CAPI UConverter* U_EXPORT2
u_getDefaultConverter(UErrorCode *status);


/**
 * Release the default converter to the converter cache.
 * @internal
 */
U_CAPI void U_EXPORT2
u_releaseDefaultConverter(UConverter *converter);

/**
 * Flush the default converter, if cached. 
 * @internal
 */
U_CAPI void U_EXPORT2
u_flushDefaultConverter(void);

#endif

#endif
