// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  denseranges.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010sep25
*   created by: Markus W. Scherer
*
* Helper code for finding a small number of dense ranges.
*/

#ifndef __DENSERANGES_H__
#define __DENSERANGES_H__

#include "unicode/utypes.h"

/**
 * Does it make sense to write 1..capacity ranges?
 * Returns 0 if not, otherwise the number of ranges.
 * @param values Sorted array of signed-integer values.
 * @param length Number of values.
 * @param density Minimum average range density, in 256th. (0x100=100%=perfectly dense.)
 *                Should be 0x80..0x100, must be 1..0x100.
 * @param ranges Output ranges array.
 * @param capacity Maximum number of ranges.
 * @return Minimum number of ranges (at most capacity) that have the desired density,
 *         or 0 if that density cannot be achieved.
 */
U_CAPI int32_t U_EXPORT2
uprv_makeDenseRanges(const int32_t values[], int32_t length,
                     int32_t density,
                     int32_t ranges[][2], int32_t capacity);

#endif  // __DENSERANGES_H__
