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

#if U_PLATFORM_HAS_WIN32_API

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

#endif /* U_PLATFORM_HAS_WIN32_API */

#endif /* __WINTZ */
