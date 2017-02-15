// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2011-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  uposixdefs.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011jul25
*   created by: Markus W. Scherer
*
*   Common definitions for implementation files working with POSIX functions.
*   *Important*: #include this file before any other header files!
*/

#ifndef __UPOSIXDEFS_H__
#define __UPOSIXDEFS_H__

/*
 * Define _XOPEN_SOURCE for access to POSIX functions.
 *
 * We cannot use U_PLATFORM from platform.h/utypes.h because
 * "The Open Group Base Specifications"
 * chapter "2.2 The Compilation Environment" says:
 * "In the compilation of an application that #defines a feature test macro
 * specified by IEEE Std 1003.1-2001,
 * no header defined by IEEE Std 1003.1-2001 shall be included prior to
 * the definition of the feature test macro."
 */
#ifdef _XOPEN_SOURCE
    /* Use the predefined value. */
#else
    /*
     * Version 6.0:
     * The Open Group Base Specifications Issue 6 (IEEE Std 1003.1, 2004 Edition)
     * also known as
     * SUSv3 = Open Group Single UNIX Specification, Version 3 (UNIX03)
     *
     * Note: This definition used to be in C source code (e.g., putil.c)
     * and define _XOPEN_SOURCE to different values depending on __STDC_VERSION__.
     * In C++ source code (e.g., putil.cpp), __STDC_VERSION__ is not defined at all.
     */
#   define _XOPEN_SOURCE 600
#endif

/*
 * Make sure things like readlink and such functions work.
 * Poorly upgraded Solaris machines can't have this defined.
 * Cleanly installed Solaris can use this #define.
 *
 * z/OS needs this definition for timeval and to get usleep.
 */
#if !defined(_XOPEN_SOURCE_EXTENDED)
#   define _XOPEN_SOURCE_EXTENDED 1
#endif

/*
 * There is an issue with turning on _XOPEN_SOURCE_EXTENDED on certain platforms.
 * A compatibility issue exists between turning on _XOPEN_SOURCE_EXTENDED and using
 * standard C++ string class. As a result, standard C++ string class needs to be
 * turned off for the follwing platforms:
 *  -AIX/VACPP
 *  -Solaris/GCC
 */
#if (U_PLATFORM == U_PF_AIX && !defined(__GNUC__)) || (U_PLATFORM == U_PF_SOLARIS && defined(__GNUC__))
#   if _XOPEN_SOURCE_EXTENDED && !defined(U_HAVE_STD_STRING)
#   define U_HAVE_STD_STRING 0
#   endif
#endif

#endif  /* __UPOSIXDEFS_H__ */
