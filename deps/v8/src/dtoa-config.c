/*
 * Copyright 2007-2008 the V8 project authors. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Google Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Dtoa needs to have a particular environment set up for it so
 * instead of using it directly you should use this file.
 *
 * The way it works is that when you link with it, its definitions
 * of dtoa, strtod etc. override the default ones.  So if you fail
 * to link with this library everything will still work, it's just
 * subtly wrong.
 */

#if !(defined(__APPLE__) && defined(__MACH__)) && \
    !defined(WIN32) && !defined(__FreeBSD__)
#include <endian.h>
#endif
#include <math.h>
#include <float.h>

/* The floating point word order on ARM is big endian when floating point
 * emulation is used, even if the byte order is little endian */
#if !(defined(__APPLE__) && defined(__MACH__)) && !defined(WIN32) && \
    !defined(__FreeBSD__) && __FLOAT_WORD_ORDER == __BIG_ENDIAN
#define  IEEE_MC68k
#else
#define  IEEE_8087
#endif

#define __MATH_H__
#if defined(__APPLE__) && defined(__MACH__) || defined(__FreeBSD__)
/* stdlib.h on FreeBSD and Apple's 10.5 and later SDKs will mangle the
 * name of strtod.  If it's included after strtod is redefined as
 * gay_strtod, it will mangle the name of gay_strtod, which is
 * unwanted. */
#include <stdlib.h>

#endif
/* stdlib.h on Windows adds __declspec(dllimport) to all functions when using
 * the DLL version of the CRT (compiling with /MD or /MDd). If stdlib.h is
 * included after strtod is redefined as gay_strtod, it will add
 * __declspec(dllimport) to gay_strtod, which causes the compilation of
 * gay_strtod in dtoa.c to fail.
*/
#if defined(WIN32) && defined(_DLL)
#include "stdlib.h"
#endif

/* For MinGW, turn on __NO_ISOCEXT so that its strtod doesn't get added */
#ifdef __MINGW32__
#define __NO_ISOCEXT
#endif  /* __MINGW32__ */

/* Make sure we use the David M. Gay version of strtod(). On Linux, we
 * cannot use the same name (maybe the function does not have weak
 * linkage?). */
#define strtod gay_strtod
#include "third_party/dtoa/dtoa.c"
