// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1997-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
* This file contains platform independent math.
*/

#include "putilimp.h"

U_CAPI int32_t U_EXPORT2
uprv_max(int32_t x, int32_t y)
{
    return (x > y ? x : y);
}

U_CAPI int32_t U_EXPORT2
uprv_min(int32_t x, int32_t y)
{
    return (x > y ? y : x);
}

