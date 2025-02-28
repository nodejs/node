// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 2002-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File uassert.h
*
*  Contains the U_ASSERT and UPRV_UNREACHABLE_* macros
*
******************************************************************************
*/
#ifndef U_ASSERT_H
#define U_ASSERT_H

/* utypes.h is included to get the proper define for uint8_t */
#include "unicode/utypes.h"
/* for abort */
#include <stdlib.h>

/**
 * \def U_ASSERT
 * By default, U_ASSERT just wraps the C library assert macro.
 * By changing the definition here, the assert behavior for ICU can be changed
 * without affecting other non - ICU uses of the C library assert().
*/
#if U_DEBUG
#   include <assert.h>
#   define U_ASSERT(exp) assert(exp)
#elif U_CPLUSPLUS_VERSION
#   define U_ASSERT(exp) (void)0
#else
#   define U_ASSERT(exp)
#endif

/**
 * \def UPRV_UNREACHABLE_ASSERT
 * This macro is used in places that we had believed were unreachable, but
 * experience has shown otherwise (possibly due to memory corruption, etc).
 * In this case we call assert() in debug versions as with U_ASSERT, instead
 * of unconditionally calling abort(). However we also allow redefinition as
 * with UPRV_UNREACHABLE_EXIT.
 * @internal
*/
#if defined(UPRV_UNREACHABLE_ASSERT)
    // Use the predefined value.
#elif U_DEBUG
#   include <assert.h>
#   define UPRV_UNREACHABLE_ASSERT assert(false)
#elif U_CPLUSPLUS_VERSION
#   define UPRV_UNREACHABLE_ASSERT (void)0
#else
#   define UPRV_UNREACHABLE_ASSERT
#endif

/**
 * \def UPRV_UNREACHABLE_EXIT
 * This macro is used to unconditionally abort if unreachable code is ever executed.
 * @internal
*/
#if defined(UPRV_UNREACHABLE_EXIT)
    // Use the predefined value.
#else
#   define UPRV_UNREACHABLE_EXIT abort()
#endif

#endif
