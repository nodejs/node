// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2011-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  uposixdefs.h
*   encoding:   UTF-8
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
#if !defined(_XOPEN_SOURCE_EXTENDED) && defined(__TOS_MVS__)
#   define _XOPEN_SOURCE_EXTENDED 1
#endif

/**
 * Solaris says:
 *   "...it is invalid to compile an XPG6 or a POSIX.1-2001 application with anything other
 *   than a c99 or later compiler."
 * Apparently C++11 is not "or later". Work around this.
 */
#if defined(__cplusplus) && (defined(sun) || defined(__sun)) && !defined (_STDC_C99)
#   define _STDC_C99
#endif

#endif  /* __UPOSIXDEFS_H__ */
