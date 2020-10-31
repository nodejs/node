// Protocol Buffers - Google's data interchange format
// Copyright 2012 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

#ifndef GOOGLE_PROTOBUF_PLATFORM_MACROS_H_
#define GOOGLE_PROTOBUF_PLATFORM_MACROS_H_

#define GOOGLE_PROTOBUF_PLATFORM_ERROR \
#error "Host platform was not detected as supported by protobuf"

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#define GOOGLE_PROTOBUF_ARCH_X64 1
#define GOOGLE_PROTOBUF_ARCH_64_BIT 1
#elif defined(_M_IX86) || defined(__i386__)
#define GOOGLE_PROTOBUF_ARCH_IA32 1
#define GOOGLE_PROTOBUF_ARCH_32_BIT 1
#elif defined(__QNX__)
#define GOOGLE_PROTOBUF_ARCH_ARM_QNX 1
#define GOOGLE_PROTOBUF_ARCH_32_BIT 1
#elif defined(_M_ARM) || defined(__ARMEL__)
#define GOOGLE_PROTOBUF_ARCH_ARM 1
#define GOOGLE_PROTOBUF_ARCH_32_BIT 1
#elif defined(_M_ARM64)
#define GOOGLE_PROTOBUF_ARCH_ARM 1
#define GOOGLE_PROTOBUF_ARCH_64_BIT 1
#elif defined(__aarch64__)
#define GOOGLE_PROTOBUF_ARCH_AARCH64 1
#define GOOGLE_PROTOBUF_ARCH_64_BIT 1
#elif defined(__mips__)
#if defined(__LP64__)
#define GOOGLE_PROTOBUF_ARCH_MIPS64 1
#define GOOGLE_PROTOBUF_ARCH_64_BIT 1
#else
#define GOOGLE_PROTOBUF_ARCH_MIPS 1
#define GOOGLE_PROTOBUF_ARCH_32_BIT 1
#endif
#elif defined(__pnacl__)
#define GOOGLE_PROTOBUF_ARCH_32_BIT 1
#elif defined(sparc)
#define GOOGLE_PROTOBUF_ARCH_SPARC 1
#if defined(__sparc_v9__) || defined(__sparcv9) || defined(__arch64__)
#define GOOGLE_PROTOBUF_ARCH_64_BIT 1
#else
#define GOOGLE_PROTOBUF_ARCH_32_BIT 1
#endif
#elif defined(_POWER) || defined(__powerpc64__) || defined(__PPC64__)
#define GOOGLE_PROTOBUF_ARCH_POWER 1
#define GOOGLE_PROTOBUF_ARCH_64_BIT 1
#elif defined(__PPC__)
#define GOOGLE_PROTOBUF_ARCH_PPC 1
#define GOOGLE_PROTOBUF_ARCH_32_BIT 1
#elif defined(__GNUC__)
# if (((__GNUC__ == 4) && (__GNUC_MINOR__ >= 7)) || (__GNUC__ > 4))
// We fallback to the generic Clang/GCC >= 4.7 implementation in atomicops.h
# elif defined(__clang__)
#  if !__has_extension(c_atomic)
GOOGLE_PROTOBUF_PLATFORM_ERROR
#  endif
// We fallback to the generic Clang/GCC >= 4.7 implementation in atomicops.h
# endif
# if __LP64__
#  define GOOGLE_PROTOBUF_ARCH_64_BIT 1
# else
#  define GOOGLE_PROTOBUF_ARCH_32_BIT 1
# endif
#else
GOOGLE_PROTOBUF_PLATFORM_ERROR
#endif

#if defined(__APPLE__)
#define GOOGLE_PROTOBUF_OS_APPLE
#include <Availability.h>
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define GOOGLE_PROTOBUF_OS_IPHONE
#endif
#elif defined(__EMSCRIPTEN__)
#define GOOGLE_PROTOBUF_OS_EMSCRIPTEN
#elif defined(__native_client__)
#define GOOGLE_PROTOBUF_OS_NACL
#elif defined(sun)
#define GOOGLE_PROTOBUF_OS_SOLARIS
#elif defined(_AIX)
#define GOOGLE_PROTOBUF_OS_AIX
#elif defined(__ANDROID__)
#define GOOGLE_PROTOBUF_OS_ANDROID
#endif

#undef GOOGLE_PROTOBUF_PLATFORM_ERROR

#if defined(GOOGLE_PROTOBUF_OS_ANDROID) || defined(GOOGLE_PROTOBUF_OS_IPHONE) || defined(__OpenBSD__)
// Android ndk does not support the __thread keyword very well yet. Here
// we use pthread_key_create()/pthread_getspecific()/... methods for
// TLS support on android.
// iOS and OpenBSD also do not support the __thread keyword.
#define GOOGLE_PROTOBUF_NO_THREADLOCAL
#endif

#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED < 1070
// __thread keyword requires at least 10.7
#define GOOGLE_PROTOBUF_NO_THREADLOCAL
#endif

#endif  // GOOGLE_PROTOBUF_PLATFORM_MACROS_H_
