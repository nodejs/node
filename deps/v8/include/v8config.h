// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8CONFIG_H_
#define V8CONFIG_H_

#ifdef V8_GN_HEADER
#if __cplusplus >= 201703L && !__has_include("v8-gn.h")
#error Missing v8-gn.h. The configuration for v8 is missing from the include \
path. Add it with -I<path> to the command line
#endif
#include "v8-gn.h"  // NOLINT(build/include_directory)
#endif

// clang-format off

// Platform headers for feature detection below.
#if defined(__ANDROID__)
# include <sys/cdefs.h>
#elif defined(__APPLE__)
# include <TargetConditionals.h>
#elif defined(__linux__)
# include <features.h>
#endif


// This macro allows to test for the version of the GNU C library (or
// a compatible C library that masquerades as glibc). It evaluates to
// 0 if libc is not GNU libc or compatible.
// Use like:
//  #if V8_GLIBC_PREREQ(2, 3)
//   ...
//  #endif
#if defined(__GLIBC__) && defined(__GLIBC_MINOR__)
# define V8_GLIBC_PREREQ(major, minor)                                    \
    ((__GLIBC__ * 100 + __GLIBC_MINOR__) >= ((major) * 100 + (minor)))
#else
# define V8_GLIBC_PREREQ(major, minor) 0
#endif


// This macro allows to test for the version of the GNU C++ compiler.
// Note that this also applies to compilers that masquerade as GCC,
// for example clang and the Intel C++ compiler for Linux.
// Use like:
//  #if V8_GNUC_PREREQ(4, 3, 1)
//   ...
//  #endif
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
# define V8_GNUC_PREREQ(major, minor, patchlevel)                         \
    ((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) >=   \
     ((major) * 10000 + (minor) * 100 + (patchlevel)))
#elif defined(__GNUC__) && defined(__GNUC_MINOR__)
# define V8_GNUC_PREREQ(major, minor, patchlevel)      \
    ((__GNUC__ * 10000 + __GNUC_MINOR__ * 100) >=      \
     ((major) * 10000 + (minor) * 100 + (patchlevel)))
#else
# define V8_GNUC_PREREQ(major, minor, patchlevel) 0
#endif



// -----------------------------------------------------------------------------
// Operating system detection (host)
//
//  V8_OS_ANDROID       - Android
//  V8_OS_BSD           - BSDish (Mac OS X, Net/Free/Open/DragonFlyBSD)
//  V8_OS_CYGWIN        - Cygwin
//  V8_OS_DRAGONFLYBSD  - DragonFlyBSD
//  V8_OS_FREEBSD       - FreeBSD
//  V8_OS_FUCHSIA       - Fuchsia
//  V8_OS_LINUX         - Linux
//  V8_OS_MACOSX        - Mac OS X
//  V8_OS_IOS           - iOS
//  V8_OS_NETBSD        - NetBSD
//  V8_OS_OPENBSD       - OpenBSD
//  V8_OS_POSIX         - POSIX compatible (mostly everything except Windows)
//  V8_OS_QNX           - QNX Neutrino
//  V8_OS_SOLARIS       - Sun Solaris and OpenSolaris
//  V8_OS_STARBOARD     - Starboard (platform abstraction for Cobalt)
//  V8_OS_AIX           - AIX
//  V8_OS_WIN           - Microsoft Windows

#if defined(__ANDROID__)
# define V8_OS_ANDROID 1
# define V8_OS_LINUX 1
# define V8_OS_POSIX 1
#elif defined(__APPLE__)
# define V8_OS_BSD 1
# define V8_OS_MACOSX 1
# define V8_OS_POSIX 1
# if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#  define V8_OS_IOS 1
# endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#elif defined(__CYGWIN__)
# define V8_OS_CYGWIN 1
# define V8_OS_POSIX 1
#elif defined(__linux__)
# define V8_OS_LINUX 1
# define V8_OS_POSIX 1
#elif defined(__sun)
# define V8_OS_POSIX 1
# define V8_OS_SOLARIS 1
#elif defined(STARBOARD)
# define V8_OS_STARBOARD 1
#elif defined(_AIX)
#define V8_OS_POSIX 1
#define V8_OS_AIX 1
#elif defined(__FreeBSD__)
# define V8_OS_BSD 1
# define V8_OS_FREEBSD 1
# define V8_OS_POSIX 1
#elif defined(__Fuchsia__)
# define V8_OS_FUCHSIA 1
# define V8_OS_POSIX 1
#elif defined(__DragonFly__)
# define V8_OS_BSD 1
# define V8_OS_DRAGONFLYBSD 1
# define V8_OS_POSIX 1
#elif defined(__NetBSD__)
# define V8_OS_BSD 1
# define V8_OS_NETBSD 1
# define V8_OS_POSIX 1
#elif defined(__OpenBSD__)
# define V8_OS_BSD 1
# define V8_OS_OPENBSD 1
# define V8_OS_POSIX 1
#elif defined(__QNXNTO__)
# define V8_OS_POSIX 1
# define V8_OS_QNX 1
#elif defined(_WIN32)
# define V8_OS_WIN 1
#endif

// -----------------------------------------------------------------------------
// Operating system detection (target)
//
//  V8_TARGET_OS_ANDROID
//  V8_TARGET_OS_FUCHSIA
//  V8_TARGET_OS_IOS
//  V8_TARGET_OS_LINUX
//  V8_TARGET_OS_MACOSX
//  V8_TARGET_OS_WIN
//
// If not set explicitly, these fall back to corresponding V8_OS_ values.

#ifdef V8_HAVE_TARGET_OS

// The target OS is provided, just check that at least one known value is set.
# if !defined(V8_TARGET_OS_ANDROID) \
  && !defined(V8_TARGET_OS_FUCHSIA) \
  && !defined(V8_TARGET_OS_IOS) \
  && !defined(V8_TARGET_OS_LINUX) \
  && !defined(V8_TARGET_OS_MACOSX) \
  && !defined(V8_TARGET_OS_WIN)
#  error No known target OS defined.
# endif

#else  // V8_HAVE_TARGET_OS

# if defined(V8_TARGET_OS_ANDROID) \
  || defined(V8_TARGET_OS_FUCHSIA) \
  || defined(V8_TARGET_OS_IOS) \
  || defined(V8_TARGET_OS_LINUX) \
  || defined(V8_TARGET_OS_MACOSX) \
  || defined(V8_TARGET_OS_WIN)
#  error A target OS is defined but V8_HAVE_TARGET_OS is unset.
# endif

// Fall back to the detected host OS.
#ifdef V8_OS_ANDROID
# define V8_TARGET_OS_ANDROID
#endif

#ifdef V8_OS_FUCHSIA
# define V8_TARGET_OS_FUCHSIA
#endif

#ifdef V8_OS_IOS
# define V8_TARGET_OS_IOS
#endif

#ifdef V8_OS_LINUX
# define V8_TARGET_OS_LINUX
#endif

#ifdef V8_OS_MACOSX
# define V8_TARGET_OS_MACOSX
#endif

#ifdef V8_OS_WIN
# define V8_TARGET_OS_WIN
#endif

#endif  // V8_HAVE_TARGET_OS

// -----------------------------------------------------------------------------
// C library detection
//
//  V8_LIBC_MSVCRT  - MSVC libc
//  V8_LIBC_BIONIC  - Bionic libc
//  V8_LIBC_BSD     - BSD libc derivate
//  V8_LIBC_GLIBC   - GNU C library
//  V8_LIBC_UCLIBC  - uClibc
//
// Note that testing for libc must be done using #if not #ifdef. For example,
// to test for the GNU C library, use:
//  #if V8_LIBC_GLIBC
//   ...
//  #endif

#if defined (_MSC_VER)
# define V8_LIBC_MSVCRT 1
#elif defined(__BIONIC__)
# define V8_LIBC_BIONIC 1
# define V8_LIBC_BSD 1
#elif defined(__UCLIBC__)
// Must test for UCLIBC before GLIBC, as UCLIBC pretends to be GLIBC.
# define V8_LIBC_UCLIBC 1
#elif defined(__GLIBC__) || defined(__GNU_LIBRARY__)
# define V8_LIBC_GLIBC 1
#else
# define V8_LIBC_BSD V8_OS_BSD
#endif


// -----------------------------------------------------------------------------
// Compiler detection
//
//  V8_CC_GNU     - GCC, or clang in gcc mode
//  V8_CC_INTEL   - Intel C++
//  V8_CC_MINGW   - Minimalist GNU for Windows
//  V8_CC_MINGW32 - Minimalist GNU for Windows (mingw32)
//  V8_CC_MINGW64 - Minimalist GNU for Windows (mingw-w64)
//  V8_CC_MSVC    - Microsoft Visual C/C++, or clang in cl.exe mode
//
// C++11 feature detection
//
// Compiler-specific feature detection
//
//  V8_HAS_ATTRIBUTE_ALWAYS_INLINE      - __attribute__((always_inline))
//                                        supported
//  V8_HAS_ATTRIBUTE_NONNULL            - __attribute__((nonnull)) supported
//  V8_HAS_ATTRIBUTE_NOINLINE           - __attribute__((noinline)) supported
//  V8_HAS_ATTRIBUTE_UNUSED             - __attribute__((unused)) supported
//  V8_HAS_ATTRIBUTE_VISIBILITY         - __attribute__((visibility)) supported
//  V8_HAS_ATTRIBUTE_WARN_UNUSED_RESULT - __attribute__((warn_unused_result))
//                                        supported
//  V8_HAS_CPP_ATTRIBUTE_NODISCARD      - [[nodiscard]] supported
//  V8_HAS_BUILTIN_BSWAP16              - __builtin_bswap16() supported
//  V8_HAS_BUILTIN_BSWAP32              - __builtin_bswap32() supported
//  V8_HAS_BUILTIN_BSWAP64              - __builtin_bswap64() supported
//  V8_HAS_BUILTIN_CLZ                  - __builtin_clz() supported
//  V8_HAS_BUILTIN_CTZ                  - __builtin_ctz() supported
//  V8_HAS_BUILTIN_EXPECT               - __builtin_expect() supported
//  V8_HAS_BUILTIN_FRAME_ADDRESS        - __builtin_frame_address() supported
//  V8_HAS_BUILTIN_POPCOUNT             - __builtin_popcount() supported
//  V8_HAS_BUILTIN_SADD_OVERFLOW        - __builtin_sadd_overflow() supported
//  V8_HAS_BUILTIN_SSUB_OVERFLOW        - __builtin_ssub_overflow() supported
//  V8_HAS_BUILTIN_UADD_OVERFLOW        - __builtin_uadd_overflow() supported
//  V8_HAS_COMPUTED_GOTO                - computed goto/labels as values
//                                        supported
//  V8_HAS_DECLSPEC_NOINLINE            - __declspec(noinline) supported
//  V8_HAS_DECLSPEC_SELECTANY           - __declspec(selectany) supported
//  V8_HAS___FORCEINLINE                - __forceinline supported
//
// Note that testing for compilers and/or features must be done using #if
// not #ifdef. For example, to test for Intel C++ Compiler, use:
//  #if V8_CC_INTEL
//   ...
//  #endif

#if defined(__has_cpp_attribute)
#define V8_HAS_CPP_ATTRIBUTE(FEATURE) __has_cpp_attribute(FEATURE)
#else
#define V8_HAS_CPP_ATTRIBUTE(FEATURE) 0
#endif

#if defined(__clang__)

#if defined(__GNUC__)  // Clang in gcc mode.
# define V8_CC_GNU 1
#endif

# define V8_HAS_ATTRIBUTE_ALWAYS_INLINE (__has_attribute(always_inline))
# define V8_HAS_ATTRIBUTE_NONNULL (__has_attribute(nonnull))
# define V8_HAS_ATTRIBUTE_NOINLINE (__has_attribute(noinline))
# define V8_HAS_ATTRIBUTE_UNUSED (__has_attribute(unused))
# define V8_HAS_ATTRIBUTE_VISIBILITY (__has_attribute(visibility))
# define V8_HAS_ATTRIBUTE_WARN_UNUSED_RESULT \
    (__has_attribute(warn_unused_result))

# define V8_HAS_CPP_ATTRIBUTE_NODISCARD (V8_HAS_CPP_ATTRIBUTE(nodiscard))

# define V8_HAS_BUILTIN_ASSUME_ALIGNED (__has_builtin(__builtin_assume_aligned))
# define V8_HAS_BUILTIN_BSWAP16 (__has_builtin(__builtin_bswap16))
# define V8_HAS_BUILTIN_BSWAP32 (__has_builtin(__builtin_bswap32))
# define V8_HAS_BUILTIN_BSWAP64 (__has_builtin(__builtin_bswap64))
# define V8_HAS_BUILTIN_CLZ (__has_builtin(__builtin_clz))
# define V8_HAS_BUILTIN_CTZ (__has_builtin(__builtin_ctz))
# define V8_HAS_BUILTIN_EXPECT (__has_builtin(__builtin_expect))
# define V8_HAS_BUILTIN_FRAME_ADDRESS (__has_builtin(__builtin_frame_address))
# define V8_HAS_BUILTIN_POPCOUNT (__has_builtin(__builtin_popcount))
# define V8_HAS_BUILTIN_SADD_OVERFLOW (__has_builtin(__builtin_sadd_overflow))
# define V8_HAS_BUILTIN_SSUB_OVERFLOW (__has_builtin(__builtin_ssub_overflow))
# define V8_HAS_BUILTIN_UADD_OVERFLOW (__has_builtin(__builtin_uadd_overflow))

// Clang has no __has_feature for computed gotos.
// GCC doc: https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
# define V8_HAS_COMPUTED_GOTO 1

// Whether constexpr has full C++14 semantics, in particular that non-constexpr
// code is allowed as long as it's not executed for any constexpr instantiation.
# define V8_HAS_CXX14_CONSTEXPR 1

#elif defined(__GNUC__)

# define V8_CC_GNU 1
# if defined(__INTEL_COMPILER)  // Intel C++ also masquerades as GCC 3.2.0
#  define V8_CC_INTEL 1
# endif
# if defined(__MINGW32__)
#  define V8_CC_MINGW32 1
# endif
# if defined(__MINGW64__)
#  define V8_CC_MINGW64 1
# endif
# define V8_CC_MINGW (V8_CC_MINGW32 || V8_CC_MINGW64)

// always_inline is available in gcc 4.0 but not very reliable until 4.4.
// Works around "sorry, unimplemented: inlining failed" build errors with
// older compilers.
# define V8_HAS_ATTRIBUTE_ALWAYS_INLINE 1
# define V8_HAS_ATTRIBUTE_NOINLINE 1
# define V8_HAS_ATTRIBUTE_UNUSED 1
# define V8_HAS_ATTRIBUTE_VISIBILITY 1
# define V8_HAS_ATTRIBUTE_WARN_UNUSED_RESULT (!V8_CC_INTEL)
# define V8_HAS_CPP_ATTRIBUTE_NODISCARD (V8_HAS_CPP_ATTRIBUTE(nodiscard))

# define V8_HAS_BUILTIN_ASSUME_ALIGNED 1
# define V8_HAS_BUILTIN_CLZ 1
# define V8_HAS_BUILTIN_CTZ 1
# define V8_HAS_BUILTIN_EXPECT 1
# define V8_HAS_BUILTIN_FRAME_ADDRESS 1
# define V8_HAS_BUILTIN_POPCOUNT 1

// GCC doc: https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html
#define V8_HAS_COMPUTED_GOTO 1

// Whether constexpr has full C++14 semantics, in particular that non-constexpr
// code is allowed as long as it's not executed for any constexpr instantiation.
// GCC only supports this since version 6.
# define V8_HAS_CXX14_CONSTEXPR (V8_GNUC_PREREQ(6, 0, 0))

#endif

#if defined(_MSC_VER)
# define V8_CC_MSVC 1

# define V8_HAS_DECLSPEC_NOINLINE 1
# define V8_HAS_DECLSPEC_SELECTANY 1

# define V8_HAS___FORCEINLINE 1

#endif


// -----------------------------------------------------------------------------
// Helper macros

// A macro used to make better inlining. Don't bother for debug builds.
// Use like:
//   V8_INLINE int GetZero() { return 0; }
#if !defined(DEBUG) && V8_HAS_ATTRIBUTE_ALWAYS_INLINE
# define V8_INLINE inline __attribute__((always_inline))
#elif !defined(DEBUG) && V8_HAS___FORCEINLINE
# define V8_INLINE __forceinline
#else
# define V8_INLINE inline
#endif

#if V8_HAS_BUILTIN_ASSUME_ALIGNED
# define V8_ASSUME_ALIGNED(ptr, alignment) \
  __builtin_assume_aligned((ptr), (alignment))
#else
# define V8_ASSUME_ALIGNED(ptr, alignment) (ptr)
#endif


// A macro to mark specific arguments as non-null.
// Use like:
//   int add(int* x, int y, int* z) V8_NONNULL(1, 3) { return *x + y + *z; }
#if V8_HAS_ATTRIBUTE_NONNULL
# define V8_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#else
# define V8_NONNULL(...) /* NOT SUPPORTED */
#endif


// A macro used to tell the compiler to never inline a particular function.
// Use like:
//   V8_NOINLINE int GetMinusOne() { return -1; }
#if V8_HAS_ATTRIBUTE_NOINLINE
# define V8_NOINLINE __attribute__((noinline))
#elif V8_HAS_DECLSPEC_NOINLINE
# define V8_NOINLINE __declspec(noinline)
#else
# define V8_NOINLINE /* NOT SUPPORTED */
#endif


// A macro (V8_DEPRECATED) to mark classes or functions as deprecated.
#if defined(V8_DEPRECATION_WARNINGS)
# define V8_DEPRECATED(message) [[deprecated(message)]]
#else
# define V8_DEPRECATED(message)
#endif


// A macro (V8_DEPRECATE_SOON) to make it easier to see what will be deprecated.
#if defined(V8_IMMINENT_DEPRECATION_WARNINGS)
# define V8_DEPRECATE_SOON(message) [[deprecated(message)]]
#else
# define V8_DEPRECATE_SOON(message)
#endif


#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ < 6)
# define V8_ENUM_DEPRECATED(message)
# define V8_ENUM_DEPRECATE_SOON(message)
#else
# define V8_ENUM_DEPRECATED(message) V8_DEPRECATED(message)
# define V8_ENUM_DEPRECATE_SOON(message) V8_DEPRECATE_SOON(message)
#endif


// A macro to provide the compiler with branch prediction information.
#if V8_HAS_BUILTIN_EXPECT
# define V8_UNLIKELY(condition) (__builtin_expect(!!(condition), 0))
# define V8_LIKELY(condition) (__builtin_expect(!!(condition), 1))
#else
# define V8_UNLIKELY(condition) (condition)
# define V8_LIKELY(condition) (condition)
#endif


// Annotate a function indicating the caller must examine the return value.
// Use like:
//   int foo() V8_WARN_UNUSED_RESULT;
#if V8_HAS_ATTRIBUTE_WARN_UNUSED_RESULT
#define V8_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define V8_WARN_UNUSED_RESULT /* NOT SUPPORTED */
#endif


// Annotate a class or constructor indicating the caller must assign the
// constructed instances.
// Apply to the whole class like:
//   class V8_NODISCARD Foo() { ... };
// or apply to just one constructor like:
//   V8_NODISCARD Foo() { ... };
// [[nodiscard]] comes in C++17 but supported in clang with -std >= c++11.
#if V8_HAS_CPP_ATTRIBUTE_NODISCARD
#define V8_NODISCARD [[nodiscard]]
#else
#define V8_NODISCARD /* NOT SUPPORTED */
#endif

// Helper macro to define no_sanitize attributes only with clang.
#if defined(__clang__) && defined(__has_attribute)
#if __has_attribute(no_sanitize)
#define V8_CLANG_NO_SANITIZE(what) __attribute__((no_sanitize(what)))
#endif
#endif
#if !defined(V8_CLANG_NO_SANITIZE)
#define V8_CLANG_NO_SANITIZE(what)
#endif

#if defined(BUILDING_V8_SHARED) && defined(USING_V8_SHARED)
#error Inconsistent build configuration: To build the V8 shared library \
set BUILDING_V8_SHARED, to include its headers for linking against the \
V8 shared library set USING_V8_SHARED.
#endif

#ifdef V8_OS_WIN

// Setup for Windows DLL export/import. When building the V8 DLL the
// BUILDING_V8_SHARED needs to be defined. When building a program which uses
// the V8 DLL USING_V8_SHARED needs to be defined. When either building the V8
// static library or building a program which uses the V8 static library neither
// BUILDING_V8_SHARED nor USING_V8_SHARED should be defined.
#ifdef BUILDING_V8_SHARED
# define V8_EXPORT __declspec(dllexport)
#elif USING_V8_SHARED
# define V8_EXPORT __declspec(dllimport)
#else
# define V8_EXPORT
#endif  // BUILDING_V8_SHARED

#else  // V8_OS_WIN

// Setup for Linux shared library export.
#if V8_HAS_ATTRIBUTE_VISIBILITY
# ifdef BUILDING_V8_SHARED
#  define V8_EXPORT __attribute__ ((visibility("default")))
# else
#  define V8_EXPORT
# endif
#else
# define V8_EXPORT
#endif

#endif  // V8_OS_WIN

// clang-format on

#undef V8_HAS_CPP_ATTRIBUTE

#endif  // V8CONFIG_H_
