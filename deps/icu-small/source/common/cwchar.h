// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*  
******************************************************************************
*
*   Copyright (C) 2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  cwchar.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001may25
*   created by: Markus W. Scherer
*
*   This file contains ICU-internal definitions of wchar_t operations.
*   These definitions were moved here from cstring.h so that fewer
*   ICU implementation files include wchar.h.
*/

#ifndef __CWCHAR_H__
#define __CWCHAR_H__

#include <string.h>
#include <stdlib.h>
#include "unicode/utypes.h"

/* Do this after utypes.h so that we have U_HAVE_WCHAR_H . */
#if U_HAVE_WCHAR_H
#   include <wchar.h>
#endif

/*===========================================================================*/
/* Wide-character functions                                                  */
/*===========================================================================*/

/* The following are not available on all systems, defined in wchar.h or string.h. */
#if U_HAVE_WCSCPY
#   define uprv_wcscpy wcscpy
#   define uprv_wcscat wcscat
#   define uprv_wcslen wcslen
#else
U_CAPI wchar_t* U_EXPORT2 
uprv_wcscpy(wchar_t *dst, const wchar_t *src);
U_CAPI wchar_t* U_EXPORT2 
uprv_wcscat(wchar_t *dst, const wchar_t *src);
U_CAPI size_t U_EXPORT2 
uprv_wcslen(const wchar_t *src);
#endif

/* The following are part of the ANSI C standard, defined in stdlib.h . */
#define uprv_wcstombs(mbstr, wcstr, count) U_STANDARD_CPP_NAMESPACE wcstombs(mbstr, wcstr, count)
#define uprv_mbstowcs(wcstr, mbstr, count) U_STANDARD_CPP_NAMESPACE mbstowcs(wcstr, mbstr, count)


#endif
