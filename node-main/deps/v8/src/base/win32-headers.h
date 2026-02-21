// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_WIN32_HEADERS_H_
#define V8_BASE_WIN32_HEADERS_H_

// This file contains defines and typedefs that allow popular Windows types to
// be used without the overhead of including windows.h.
// This file no longer includes windows.h but it still sets the defines that
// tell windows.h to omit some includes so that the V8 source files that do
// include windows.h will still get the minimal version.

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
#ifndef _WIN32_WINNT
#error This should be set in build config files. See build\config\win\BUILD.gn
#endif

#include <signal.h>  // For raise().
#include <time.h>  // For LocalOffset() implementation.
#if !defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR)
#include <errno.h>           // For STRUNCATE
#endif  // !defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR)
#include <limits.h>  // For INT_MAX and al.
#include <process.h>  // For _beginthreadex().
#include <stdlib.h>

// typedef and define the most commonly used Windows integer types.

typedef int BOOL;             // NOLINT(runtime/int)
typedef unsigned long DWORD;  // NOLINT(runtime/int)
typedef long LONG;            // NOLINT(runtime/int)
typedef void* LPVOID;
typedef void* PVOID;
typedef void* HANDLE;

#define WINAPI __stdcall

#if defined(_WIN64)
typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
#else
typedef __w64 unsigned long ULONG_PTR, *PULONG_PTR;  // NOLINT(runtime/int)
#endif

typedef struct _RTL_SRWLOCK SRWLOCK;
typedef struct _RTL_CONDITION_VARIABLE CONDITION_VARIABLE;
typedef struct _RTL_CRITICAL_SECTION CRITICAL_SECTION;
typedef struct _RTL_CRITICAL_SECTION_DEBUG* PRTL_CRITICAL_SECTION_DEBUG;

// Declare V8 versions of some Windows structures. These are needed for
// when we need a concrete type but don't want to pull in Windows.h. We can't
// declare the Windows types so we declare our types and cast to the Windows
// types in a few places. The sizes must match the Windows types so we verify
// that with static asserts in platform-win32.cc.
// ChromeToWindowsType functions are provided for pointer conversions.

struct V8_SRWLOCK {
  PVOID Ptr;
};

struct V8_CONDITION_VARIABLE {
  PVOID Ptr;
};

struct V8_CRITICAL_SECTION {
  PRTL_CRITICAL_SECTION_DEBUG DebugInfo;
  LONG LockCount;
  LONG RecursionCount;
  HANDLE OwningThread;
  HANDLE LockSemaphore;
  ULONG_PTR SpinCount;
};

inline SRWLOCK* V8ToWindowsType(V8_SRWLOCK* p) {
  return reinterpret_cast<SRWLOCK*>(p);
}

inline const SRWLOCK* V8ToWindowsType(const V8_SRWLOCK* p) {
  return reinterpret_cast<const SRWLOCK*>(p);
}

inline CONDITION_VARIABLE* V8ToWindowsType(V8_CONDITION_VARIABLE* p) {
  return reinterpret_cast<CONDITION_VARIABLE*>(p);
}

inline const CONDITION_VARIABLE* V8ToWindowsType(
    const V8_CONDITION_VARIABLE* p) {
  return reinterpret_cast<const CONDITION_VARIABLE*>(p);
}

inline CRITICAL_SECTION* V8ToWindowsType(V8_CRITICAL_SECTION* p) {
  return reinterpret_cast<CRITICAL_SECTION*>(p);
}

inline const CRITICAL_SECTION* V8ToWindowsType(const V8_CRITICAL_SECTION* p) {
  return reinterpret_cast<const CRITICAL_SECTION*>(p);
}

#endif  // V8_BASE_WIN32_HEADERS_H_
