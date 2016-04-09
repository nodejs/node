/*
******************************************************************************
*
*   Copyright (C) 1997-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
*  FILE NAME : ptypes.h
*
*   Date        Name        Description
*   05/13/98    nos         Creation (content moved here from ptypes.h).
*   03/02/99    stephen     Added AS400 support.
*   03/30/99    stephen     Added Linux support.
*   04/13/99    stephen     Reworked for autoconf.
*   09/18/08    srl         Moved basic types back to ptypes.h from platform.h
******************************************************************************
*/

/**
 * \file
 * \brief C API: Definitions of integer types of various widths
 */

#ifndef _PTYPES_H
#define _PTYPES_H

/**
 * \def __STDC_LIMIT_MACROS
 * According to the Linux stdint.h, the ISO C99 standard specifies that in C++ implementations
 * macros like INT32_MIN and UINTPTR_MAX should only be defined if explicitly requested.
 * We need to define __STDC_LIMIT_MACROS before including stdint.h in C++ code
 * that uses such limit macros.
 * @internal
 */
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

/* NULL, size_t, wchar_t */
#include <stddef.h>

/*
 * If all compilers provided all of the C99 headers and types,
 * we would just unconditionally #include <stdint.h> here
 * and not need any of the stuff after including platform.h.
 */

/* Find out if we have stdint.h etc. */
#include "unicode/platform.h"

/*===========================================================================*/
/* Generic data types                                                        */
/*===========================================================================*/

/* If your platform does not have the <stdint.h> header, you may
   need to edit the typedefs in the #else section below.
   Use #if...#else...#endif with predefined compiler macros if possible. */
#if U_HAVE_STDINT_H

/*
 * We mostly need <stdint.h> (which defines the standard integer types) but not <inttypes.h>.
 * <inttypes.h> includes <stdint.h> and adds the printf/scanf helpers PRId32, SCNx16 etc.
 * which we almost never use, plus stuff like imaxabs() which we never use.
 */
#include <stdint.h>

#if U_PLATFORM == U_PF_OS390
/* The features header is needed to get (u)int64_t sometimes. */
#include <features.h>
/* z/OS has <stdint.h>, but some versions are missing uint8_t (APAR PK62248). */
#if !defined(__uint8_t)
#define __uint8_t 1
typedef unsigned char uint8_t;
#endif
#endif /* U_PLATFORM == U_PF_OS390 */

#elif U_HAVE_INTTYPES_H

#   include <inttypes.h>

#else /* neither U_HAVE_STDINT_H nor U_HAVE_INTTYPES_H */

#if ! U_HAVE_INT8_T
typedef signed char int8_t;
#endif

#if ! U_HAVE_UINT8_T
typedef unsigned char uint8_t;
#endif

#if ! U_HAVE_INT16_T
typedef signed short int16_t;
#endif

#if ! U_HAVE_UINT16_T
typedef unsigned short uint16_t;
#endif

#if ! U_HAVE_INT32_T
typedef signed int int32_t;
#endif

#if ! U_HAVE_UINT32_T
typedef unsigned int uint32_t;
#endif

#if ! U_HAVE_INT64_T
#ifdef _MSC_VER
    typedef signed __int64 int64_t;
#else
    typedef signed long long int64_t;
#endif
#endif

#if ! U_HAVE_UINT64_T
#ifdef _MSC_VER
    typedef unsigned __int64 uint64_t;
#else
    typedef unsigned long long uint64_t;
#endif
#endif

#endif /* U_HAVE_STDINT_H / U_HAVE_INTTYPES_H */

#endif /* _PTYPES_H */
