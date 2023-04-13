// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2005, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  swapimpl.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2005jul29
*   created by: Markus W. Scherer
*
*   Declarations for data file swapping functions not declared in internal
*   library headers.
*/

#ifndef __SWAPIMPL_H__
#define __SWAPIMPL_H__

#include "unicode/utypes.h"
#include "udataswp.h"

/**
 * Identifies and then transforms the ICU data piece in-place, or determines
 * its length. See UDataSwapFn.
 * This function handles single data pieces (but not .dat data packages)
 * and internally dispatches to per-type swap functions.
 * Sets a U_UNSUPPORTED_ERROR if the data format is not recognized.
 *
 * @see UDataSwapFn
 * @see udata_openSwapper
 * @see udata_openSwapperForInputData
 * @internal ICU 2.8
 */
U_CAPI int32_t U_EXPORT2
udata_swap(const UDataSwapper *ds,
           const void *inData, int32_t length, void *outData,
           UErrorCode *pErrorCode);

#endif
