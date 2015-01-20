// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8CONFIG_H_
#define V8CONFIG_H_

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
# define V8_GNUC_PREREQ(major, minor, patchlevel)       \
    ((__GNUC__ * 10000 + __GNUC_MINOR__) >=             \
     ((major) * 10000 + (minor) * 100 + (patchlevel)))
#else
# define V8_GNUC_PREREQ(major, minor, patchlevel) 0
#endif



// -----------------------------------------------------------------------------
// Operating system detection
//
//  V8_OS_ANDROID       - Android
//  V8_OS_BSD           - BSDish (Mac OS X, Net/Free/Open/DragonFlyBSD)
//  V8_OS_CYGWIN        - Cygwin
//  V8_OS_DRAGONFLYBSD  - DragonFlyBSD
//  V8_OS_FREEBSD       - FreeBSD
//  V8_OS_LINUX         - Linux
//  V8_OS_MACOSX        - Mac OS X
//  V8_OS_NACL          - Native Client
//  V8_OS_NETBSD        - NetBSD
//  V8_OS_OPENBSD       - OpenBSD
//  V8_OS_POSIX         - POSIX compatible (mostly everything except Windows)
//  V8_OS_QNX           - QNX Neutrino
//  V8_OS_SOLARIS       - Sun Solaris and OpenSolaris
//  V8_OS_WIN           - Microsoft Windows

#if defined(__ANDROID__)
# define V8_OS_ANDROID 1
# define V8_OS_LINUX 1
# define V8_OS_POSIX 1
#elif defined(__APPLE__)
# define V8_OS_BSD 1
# define V8_OS_MACOSX 1
# define V8_OS_POSIX 1
#elif defined(__native_client__)
# define V8_OS_NACL 1
# define V8_OS_POSIX 1
#elif defined(__CYGWIN__)
# define V8_OS_CYGWIN 1
# define V8_OS_POSIX 1
#elif defined(__linux__)
# define V8_OS_LINUX 1
# define V8_OS_POSIX 1
#elif defined(__sun)
# define V8_OS_POSIX 1
# define V8_OS_SOLARIS 1
#elif defined(__FreeBSD__)
# define V8_OS_BSD 1
# define V8_OS_FREEBSD 1
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
// C library detection
//
//  V8_LIBC_MSVCRT  - MSVC libc
//  V8_LIBC_BIONIC  - Bionic libc
//  V8_LIBC_BSD     - BSD libc derivate
//  V8_LIBC_GLIBC   - GNU C library
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
//  V8_HAS_CXX11_ALIGNAS        - alignas specifier supported
//  V8_HAS_CXX11_ALIGNOF        - alignof(type) operator supported
//  V8_HAS_CXX11_STATIC_ASSERT  - static_assert() supported
//  V8_HAS_CXX11_DELETE         - deleted functions supported
//  V8_HAS_CXX11_FINAL          - final marker supported
//  V8_HAS_CXX11_OVERRIDE       - override marker supported
//
// Compiler-specific feature detection
//
//  V8_HAS___ALIGNOF                    - __alignof(type) operator supported
//  V8_HAS___ALIGNOF__                  - __alignof__(type) operator supported
//  V8_HAS_ATTRIBUTE_ALIGNED            - __attribute__((aligned(n))) supported
//  V8_HAS_ATTRIBUTE_ALWAYS_INLINE      - __attribute__((always_inline))
//                                        supported
//  V8_HAS_ATTRIBUTE_DEPRECATED         - __attribute__((deprecated)) supported
//  V8_HAS_ATTRIBUTE_NOINLINE           - __attribute__((noinline)) supported
//  V8_HAS_ATTRIBUTE_UNUSED             - __attribute__((unused)) supported
//  V8_HAS_ATTRIBUTE_VISIBILITY         - __attribute__((visibility)) supported
//  V8_HAS_ATTRIBUTE_WARN_UNUSED_RESULT - __attribute__((warn_unused_result))
//                                        supported
//  V8_HAS_BUILTIN_CLZ                  - __builtin_clz() supported
//  V8_HAS_BUILTIN_CTZ                  - __builtin_ctz() supported
//  V8_HAS_BUILTIN_EXPECT               - __builtin_expect() supported
//  V8_HAS_BUILTIN_FRAME_ADDRESS        - __builtin_frame_address() supported
//  V8_HAS_BUILTIN_POPCOUNT             - __builtin_popcount() supported
//  V8_HAS_BUILTIN_SADD_OVERFLOW        - __builtin_sadd_overflow() supported
//  V8_HAS_BUILTIN_SSUB_OVERFLOW        - __builtin_ssub_overflow() supported
//  V8_HAS_DECLSPEC_ALIGN               - __declspec(align(n)) supported
//  V8_HAS_DECLSPEC_DEPRECATED          - __declspec(deprecated) supported
//  V8_HAS_DECLSPEC_NOINLINE            - __declspec(noinline) supported
//  V8_HAS___FINAL                      - __final supported in non-C++11 mode
//  V8_HAS___FORCEINLINE                - __forceinline supported
//
// Note that testing for compilers and/or features must be done using #if
// not #ifdef. For example, to test for Intel C++ Compiler, use:
//  #if V8_CC_INTEL
//   ...
//  #endif

#if defined(__clang__)

#if defined(__GNUC__)  // Clang in gcc mode.
# define V8_CC_GNU 1
#elif defined(_MSC_VER)  // Clang in cl mode.
# define V8_CC_MSVC 1
#endif

// Clang defines __alignof__ as alias for __alignof
# define V8_HAS___ALIGNOF 1
# define V8_HAS___ALIGNOF__ V8_HAS___ALIGNOF

# define V8_HAS_ATTRIBUTE_ALIGNED (__has_attribute(aligned))
# define V8_HAS_ATTRIBUTE_ALWAYS_INLINE (__has_attribute(always_inline))
# define V8_HAS_ATTRIBUTE_DEPRECATED (__has_attribute(deprecated))
# define V8_HAS_ATTRIBUTE_NOINLINE (__has_attribute(noinline))
# define V8_HAS_ATTRIBUTE_UNUSED (__has_attribute(unused))
# define V8_HAS_ATTRIBUTE_VISIBILITY (__has_attribute(visibility))
# define V8_HAS_ATTRIBUTE_WARN_UNUSED_RESULT \
    (__has_attribute(warn_unused_result))

# define V8_HAS_BUILTIN_CLZ (__has_builtin(__builtin_clz))
# define V8_HAS_BUILTIN_CTZ (__has_builtin(__builtin_ctz))
# define V8_HAS_BUILTIN_EXPECT (__has_builtin(__builtin_expect))
# define V8_HAS_BUILTIN_FRAME_ADDRESS (__has_builtin(__builtin_frame_address))
# define V8_HAS_BUILTIN_POPCOUNT (__has_builtin(__builtin_popcount))
# define V8_HAS_BUILTIN_SADD_OVERFLOW (__has_builtin(__builtin_sadd_overflow))
# define V8_HAS_BUILTIN_SSUB_OVERFLOW (__has_builtin(__builtin_ssub_overflow))

# define V8_HAS_CXX11_ALIGNAS (__has_feature(cxx_alignas))
# define V8_HAS_CXX11_STATIC_ASSERT (__has_feature(cxx_static_assert))
# define V8_HAS_CXX11_DELETE (__has_feature(cxx_deleted_functions))
# define V8_HAS_CXX11_FINAL (__has_feature(cxx_override_control))
# define V8_HAS_CXX11_OVERRIDE (__has_feature(cxx_override_control))

#elif defined(__GNUC__)

# define V8_CC_GNU 1
// Intel C++ also masquerades as GCC 3.2.0
# define V8_CC_INTEL (defined(__INTEL_COMPILER))
# define V8_CC_MINGW32 (defined(__MINGW32__))
# define V8_CC_MINGW64 (defined(__MINGW64__))
# define V8_CC_MINGW (V8_CC_MINGW32 || V8_CC_MINGW64)

# define V8_HAS___ALIGNOF__ (V8_GNUC_PREREQ(4, 3, 0))

# define V8_HAS_ATTRIBUTE_ALIGNED (V8_GNUC_PREREQ(2, 95, 0))
// always_inline is available in gcc 4.0 but not very reliable until 4.4.
// Works around "sorry, unimplemented: inlining failed" build errors with
// older compilers.
# define V8_HAS_ATTRIBUTE_ALWAYS_INLINE (V8_GNUC_PREREQ(4, 4, 0))
# define V8_HAS_ATTRIBUTE_DEPRECATED (V8_GNUC_PREREQ(3, 4, 0))
# define V8_HAS_ATTRIBUTE_DEPRECATED_MESSAGE (V8_GNUC_PREREQ(4, 5, 0))
# define V8_HAS_ATTRIBUTE_NOINLINE (V8_GNUC_PREREQ(3, 4, 0))
# define V8_HAS_ATTRIBUTE_UNUSED (V8_GNUC_PREREQ(2, 95, 0))
# define V8_HAS_ATTRIBUTE_VISIBILITY (V8_GNUC_PREREQ(4, 3, 0))
# define V8_HAS_ATTRIBUTE_WARN_UNUSED_RESULT \
    (!V8_CC_INTEL && V8_GNUC_PREREQ(4, 1, 0))

# define V8_HAS_BUILTIN_CLZ (V8_GNUC_PREREQ(3, 4, 0))
# define V8_HAS_BUILTIN_CTZ (V8_GNUC_PREREQ(3, 4, 0))
# define V8_HAS_BUILTIN_EXPECT (V8_GNUC_PREREQ(2, 96, 0))
# define V8_HAS_BUILTIN_FRAME_ADDRESS (V8_GNUC_PREREQ(2, 96, 0))
# define V8_HAS_BUILTIN_POPCOUNT (V8_GNUC_PREREQ(3, 4, 0))

// g++ requires -std=c++0x or -std=gnu++0x to support C++11 functionality
// without warnings (functionality used by the macros below).  These modes
// are detectable by checking whether __GXX_EXPERIMENTAL_CXX0X__ is defined or,
// more standardly, by checking whether __cplusplus has a C++11 or greater
// value. Current versions of g++ do not correctly set __cplusplus, so we check
// both for forward compatibility.
# if defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L
#  define V8_HAS_CXX11_ALIGNAS (V8_GNUC_PREREQ(4, 8, 0))
#  define V8_HAS_CXX11_ALIGNOF (V8_GNUC_PREREQ(4, 8, 0))
#  define V8_HAS_CXX11_STATIC_ASSERT (V8_GNUC_PREREQ(4, 3, 0))
#  define V8_HAS_CXX11_DELETE (V8_GNUC_PREREQ(4, 4, 0))
#  define V8_HAS_CXX11_OVERRIDE (V8_GNUC_PREREQ(4, 7, 0))
#  define V8_HAS_CXX11_FINAL (V8_GNUC_PREREQ(4, 7, 0))
# else
// '__final' is a non-C++11 GCC synonym for 'final', per GCC r176655.
#  define V8_HAS___FINAL (V8_GNUC_PREREQ(4, 7, 0))
# endif

#elif defined(_MSC_VER)

# define V8_CC_MSVC 1

# define V8_HAS___ALIGNOF 1

# define V8_HAS_CXX11_FINAL 1
# define V8_HAS_CXX11_OVERRIDE 1

# define V8_HAS_DECLSPEC_ALIGN 1
# define V8_HAS_DECLSPEC_DEPRECATED 1
# define V8_HAS_DECLSPEC_NOINLINE 1

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


// A macro used to tell the compiler to never inline a particular function.
// Don't bother for debug builds.
// Use like:
//   V8_NOINLINE int GetMinusOne() { return -1; }
#if !defined(DEBUG) && V8_HAS_ATTRIBUTE_NOINLINE
# define V8_NOINLINE __attribute__((noinline))
#elif !defined(DEBUG) && V8_HAS_DECLSPEC_NOINLINE
# define V8_NOINLINE __declspec(noinline)
#else
# define V8_NOINLINE /* NOT SUPPORTED */
#endif


// A macro to mark classes or functions as deprecated.
#if defined(V8_DEPRECATION_WARNINGS) && V8_HAS_ATTRIBUTE_DEPRECATED_MESSAGE
# define V8_DEPRECATED(message, declarator) \
declarator __attribute__((deprecated(message)))
#elif defined(V8_DEPRECATION_WARNINGS) && V8_HAS_ATTRIBUTE_DEPRECATED
# define V8_DEPRECATED(message, declarator) \
declarator __attribute__((deprecated))
#elif defined(V8_DEPRECATION_WARNINGS) && V8_HAS_DECLSPEC_DEPRECATED
# define V8_DEPRECATED(message, declarator) __declspec(deprecated) declarator
#else
# define V8_DEPRECATED(message, declarator) declarator
#endif


// A macro to provide the compiler with branch prediction information.
#if V8_HAS_BUILTIN_EXPECT
# define V8_UNLIKELY(condition) (__builtin_expect(!!(condition), 0))
# define V8_LIKELY(condition) (__builtin_expect(!!(condition), 1))
#else
# define V8_UNLIKELY(condition) (condition)
# define V8_LIKELY(condition) (condition)
#endif


// A macro to specify that a method is deleted from the corresponding class.
// Any attempt to use the method will always produce an error at compile time
// when this macro can be implemented (i.e. if the compiler supports C++11).
// If the current compiler does not support C++11, use of the annotated method
// will still cause an error, but the error will most likely occur at link time
// rather than at compile time. As a backstop, method declarations using this
// macro should be private.
// Use like:
//   class A {
//    private:
//     A(const A& other) V8_DELETE;
//     A& operator=(const A& other) V8_DELETE;
//   };
#if V8_HAS_CXX11_DELETE
# define V8_DELETE = delete
#else
# define V8_DELETE /* NOT SUPPORTED */
#endif


// This macro allows to specify memory alignment for structs, classes, etc.
// Use like:
//   class V8_ALIGNED(16) MyClass { ... };
//   V8_ALIGNED(32) int array[42];
#if V8_HAS_CXX11_ALIGNAS
# define V8_ALIGNED(n) alignas(n)
#elif V8_HAS_ATTRIBUTE_ALIGNED
# define V8_ALIGNED(n) __attribute__((aligned(n)))
#elif V8_HAS_DECLSPEC_ALIGN
# define V8_ALIGNED(n) __declspec(align(n))
#else
# define V8_ALIGNED(n) /* NOT SUPPORTED */
#endif


// This macro is similar to V8_ALIGNED(), but takes a type instead of size
// in bytes. If the compiler does not supports using the alignment of the
// |type|, it will align according to the |alignment| instead. For example,
// Visual Studio C++ cannot combine __declspec(align) and __alignof. The
// |alignment| must be a literal that is used as a kind of worst-case fallback
// alignment.
// Use like:
//   struct V8_ALIGNAS(AnotherClass, 16) NewClass { ... };
//   V8_ALIGNAS(double, 8) int array[100];
#if V8_HAS_CXX11_ALIGNAS
# define V8_ALIGNAS(type, alignment) alignas(type)
#elif V8_HAS___ALIGNOF__ && V8_HAS_ATTRIBUTE_ALIGNED
# define V8_ALIGNAS(type, alignment) __attribute__((aligned(__alignof__(type))))
#else
# define V8_ALIGNAS(type, alignment) V8_ALIGNED(alignment)
#endif


// This macro returns alignment in bytes (an integer power of two) required for
// any instance of the given type, which is either complete type, an array type,
// or a reference type.
// Use like:
//   size_t alignment = V8_ALIGNOF(double);
#if V8_HAS_CXX11_ALIGNOF
# define V8_ALIGNOF(type) alignof(type)
#elif V8_HAS___ALIGNOF
# define V8_ALIGNOF(type) __alignof(type)
#elif V8_HAS___ALIGNOF__
# define V8_ALIGNOF(type) __alignof__(type)
#else
// Note that alignment of a type within a struct can be less than the
// alignment of the type stand-alone (because of ancient ABIs), so this
// should only be used as a last resort.
namespace v8 { template <typename T> class AlignOfHelper { char c; T t; }; }
# define V8_ALIGNOF(type) (sizeof(::v8::AlignOfHelper<type>) - sizeof(type))
#endif

#endif  // V8CONFIG_H_
