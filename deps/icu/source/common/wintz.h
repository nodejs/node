// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
********************************************************************************
*   Copyright (C) 2005-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINTZ.H
*
********************************************************************************
*/

#ifndef __WINTZ
#define __WINTZ

#include "unicode/utypes.h"

// This file contains only desktop windows behavior
// Windows UWP calls Windows::Globalization directly, so this isn't needed there.
#if U_PLATFORM_USES_ONLY_WIN32_API && (U_PLATFORM_HAS_WINUWP_API == 0)

/**
 * \file 
 * \brief C API: Utilities for dealing w/ Windows time zones.
 */

U_CDECL_BEGIN
/* Forward declarations for Windows types... */
typedef struct _TIME_ZONE_INFORMATION TIME_ZONE_INFORMATION;
U_CDECL_END

U_CFUNC const char* U_EXPORT2
uprv_detectWindowsTimeZone();

#endif /* U_PLATFORM_USES_ONLY_WIN32_API && (U_PLATFORM_HAS_WINUWP_API == 0) */

#endif /* __WINTZ */
