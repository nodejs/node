/* Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
   Licensed under the Apache 2.0 License. */

#ifndef __KREMLIN_CALLCONV_H
#define __KREMLIN_CALLCONV_H

/******************************************************************************/
/* Some macros to ease compatibility                                          */
/******************************************************************************/

/* We want to generate __cdecl safely without worrying about it being undefined.
 * When using MSVC, these are always defined. When using MinGW, these are
 * defined too. They have no meaning for other platforms, so we define them to
 * be empty macros in other situations. */
#ifndef _MSC_VER
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif
#endif

/* Since KreMLin emits the inline keyword unconditionally, we follow the
 * guidelines at https://gcc.gnu.org/onlinedocs/gcc/Inline.html and make this
 * __inline__ to ensure the code compiles with -std=c90 and earlier. */
#if defined(__GNUC__)
#  define inline __inline__
#elif defined(_MSC_VER)
#  define inline __inline
#endif

/* GCC-specific attribute syntax; everyone else gets the standard C inline
 * attribute. */
#ifdef __GNU_C__
#  ifndef __clang__
#    define force_inline inline __attribute__((always_inline))
#  else
#    define force_inline inline
#  endif
#else
#  define force_inline inline
#endif

#endif
