// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_WIN32_HEADERS_H_
#define V8_WIN32_HEADERS_H_

#ifndef WIN32_LEAN_AND_MEAN
// WIN32_LEAN_AND_MEAN implies NOCRYPT and NOGDI.
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOKERNEL
#define NOKERNEL
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOSERVICE
#define NOSERVICE
#endif
#ifndef NOSOUND
#define NOSOUND
#endif
#ifndef NOMCX
#define NOMCX
#endif
// Require Windows XP or higher (this is required for the RtlCaptureContext
// function to be present).
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x501
#endif

#include <windows.h>

#include <signal.h>  // For raise().
#include <time.h>  // For LocalOffset() implementation.
#include <mmsystem.h>  // For timeGetTime().
#ifdef __MINGW32__
// Require Windows XP or higher when compiling with MinGW. This is for MinGW
// header files to expose getaddrinfo.
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x501
#endif  // __MINGW32__
#if !defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR)
#include <dbghelp.h>  // For SymLoadModule64 and al.
#include <errno.h>  // For STRUNCATE
#endif  // !defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR)
#include <limits.h>  // For INT_MAX and al.
#include <tlhelp32.h>  // For Module32First and al.

// These additional WIN32 includes have to be right here as the #undef's below
// makes it impossible to have them elsewhere.
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef __MINGW32__
#include <wspiapi.h>
#endif  // __MINGW32__
#include <process.h>  // For _beginthreadex().
#include <stdlib.h>

#undef VOID
#undef DELETE
#undef IN
#undef THIS
#undef CONST
#undef NAN
#undef UNKNOWN
#undef NONE
#undef ANY
#undef IGNORE
#undef STRICT
#undef GetObject
#undef CreateSemaphore
#undef Yield

#endif  // V8_WIN32_HEADERS_H_
