// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
********************************************************************************
*   Copyright (C) 2008-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINTZIMPL.H
*
********************************************************************************
*/

#ifndef __WINTZIMPL
#define __WINTZIMPL

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

/*
 * This method was moved over from common/wintz.h to allow for access to i18n functions
 * needed to get the Windows time zone information without using static tables.
 */
U_CAPI UBool U_EXPORT2
uprv_getWindowsTimeZoneInfo(TIME_ZONE_INFORMATION *zoneInfo, const UChar *icuid, int32_t length);


#endif /* U_PLATFORM_HAS_WIN32_API */

#endif /* __WINTZIMPL */
