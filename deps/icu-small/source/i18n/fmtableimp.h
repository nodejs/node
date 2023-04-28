// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2010-2014, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#ifndef FMTABLEIMP_H
#define FMTABLEIMP_H

#include "number_decimalquantity.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

/** 
 * Maximum int64_t value that can be stored in a double without chancing losing precision.
 *   IEEE doubles have 53 bits of mantissa, 10 bits exponent, 1 bit sign.
 *   IBM Mainframes have 56 bits of mantissa, 7 bits of base 16 exponent, 1 bit sign.
 * Define this constant to the smallest value from those for supported platforms.
 * @internal
 */
static const int64_t MAX_INT64_IN_DOUBLE = 0x001FFFFFFFFFFFFFLL;

U_NAMESPACE_END

#endif // #if !UCONFIG_NO_FORMATTING
#endif
