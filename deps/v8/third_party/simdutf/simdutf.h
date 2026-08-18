/* auto-generated on 2026-08-18 10:18:07 -0400. Do not edit! */
/* begin file include/simdutf.h */
#ifndef SIMDUTF_H
#define SIMDUTF_H
#include <cstring>

/* begin file include/simdutf/compiler_check.h */
#ifndef SIMDUTF_COMPILER_CHECK_H
#define SIMDUTF_COMPILER_CHECK_H

#ifndef __cplusplus
  #error simdutf requires a C++ compiler
#endif

#ifndef SIMDUTF_CPLUSPLUS
  #if defined(_MSVC_LANG) && !defined(__clang__)
    #define SIMDUTF_CPLUSPLUS (_MSC_VER == 1900 ? 201103L : _MSVC_LANG)
  #else
    #define SIMDUTF_CPLUSPLUS __cplusplus
  #endif
#endif

// C++ 26
#if !defined(SIMDUTF_CPLUSPLUS26) && (SIMDUTF_CPLUSPLUS >= 202602L)
  #define SIMDUTF_CPLUSPLUS26 1
#endif

// C++ 23
#if !defined(SIMDUTF_CPLUSPLUS23) && (SIMDUTF_CPLUSPLUS >= 202302L)
  #define SIMDUTF_CPLUSPLUS23 1
#endif

// C++ 20
#if !defined(SIMDUTF_CPLUSPLUS20) && (SIMDUTF_CPLUSPLUS >= 202002L)
  #define SIMDUTF_CPLUSPLUS20 1
#endif

// C++ 17
#if !defined(SIMDUTF_CPLUSPLUS17) && (SIMDUTF_CPLUSPLUS >= 201703L)
  #define SIMDUTF_CPLUSPLUS17 1
#endif

// C++ 14
#if !defined(SIMDUTF_CPLUSPLUS14) && (SIMDUTF_CPLUSPLUS >= 201402L)
  #define SIMDUTF_CPLUSPLUS14 1
#endif

// C++ 11
#if !defined(SIMDUTF_CPLUSPLUS11) && (SIMDUTF_CPLUSPLUS >= 201103L)
  #define SIMDUTF_CPLUSPLUS11 1
#endif

#ifndef SIMDUTF_CPLUSPLUS17
  #error simdutf requires a compiler compliant with the C++17 standard
#endif

#endif // SIMDUTF_COMPILER_CHECK_H
/* end file include/simdutf/compiler_check.h */
/* begin file include/simdutf/common_defs.h */
#ifndef SIMDUTF_COMMON_DEFS_H
#define SIMDUTF_COMMON_DEFS_H

/* begin file include/simdutf/portability.h */
#ifndef SIMDUTF_PORTABILITY_H
#define SIMDUTF_PORTABILITY_H


#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#ifndef _WIN32
  // strcasecmp, strncasecmp
  #include <strings.h>
#endif

#if defined(__apple_build_version__)
  #if __apple_build_version__ < 14000000
    #define SIMDUTF_SPAN_DISABLED                                              \
      1 // apple-clang/13 doesn't support std::convertible_to
  #endif
#endif

#if SIMDUTF_CPLUSPLUS20
  #include <version>
  #if __cpp_concepts >= 201907L && __cpp_lib_span >= 202002L &&                \
      !defined(SIMDUTF_SPAN_DISABLED)
    #define SIMDUTF_SPAN 1
  #endif // __cpp_concepts >= 201907L && __cpp_lib_span >= 202002L
  #if __cpp_lib_atomic_ref >= 201806L
    #define SIMDUTF_ATOMIC_REF 1
  #endif // __cpp_lib_atomic_ref
  #if __has_cpp_attribute(maybe_unused) >= 201603L
    #define SIMDUTF_MAYBE_UNUSED_AVAILABLE 1
  #endif // __has_cpp_attribute(maybe_unused) >= 201603L
#endif

/**
 * We want to check that it is actually a little endian system at
 * compile-time.
 */

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
  #define SIMDUTF_IS_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#elif defined(_WIN32)
  #define SIMDUTF_IS_BIG_ENDIAN 0
#else
  #if defined(__APPLE__) ||                                                    \
      defined(__FreeBSD__) // defined __BYTE_ORDER__ && defined
                           // __ORDER_BIG_ENDIAN__
    #include <machine/endian.h>
  #elif defined(sun) ||                                                        \
      defined(__sun) // defined(__APPLE__) || defined(__FreeBSD__)
    #include <sys/byteorder.h>
  #else // defined(__APPLE__) || defined(__FreeBSD__)

    #ifdef __has_include
      #if __has_include(<endian.h>)
        #include <endian.h>
      #endif //__has_include(<endian.h>)
    #endif   //__has_include

  #endif // defined(__APPLE__) || defined(__FreeBSD__)

  #ifndef !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__)
    #define SIMDUTF_IS_BIG_ENDIAN 0
  #endif

  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define SIMDUTF_IS_BIG_ENDIAN 0
  #else // __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define SIMDUTF_IS_BIG_ENDIAN 1
  #endif // __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#endif // defined __BYTE_ORDER__ && defined __ORDER_BIG_ENDIAN__

/**
 * At this point in time, SIMDUTF_IS_BIG_ENDIAN is defined.
 */

#ifdef _MSC_VER
  #define SIMDUTF_VISUAL_STUDIO 1
  /**
   * We want to differentiate carefully between
   * clang under visual studio and regular visual
   * studio.
   *
   * Under clang for Windows, we enable:
   *  * target pragmas so that part and only part of the
   *     code gets compiled for advanced instructions.
   *
   */
  #ifdef __clang__
    // clang under visual studio
    #define SIMDUTF_CLANG_VISUAL_STUDIO 1
  #else
    // just regular visual studio (best guess)
    #define SIMDUTF_REGULAR_VISUAL_STUDIO 1
  #endif // __clang__
#endif   // _MSC_VER

#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
  // https://en.wikipedia.org/wiki/C_alternative_tokens
  // This header should have no effect, except maybe
  // under Visual Studio.
  #include <iso646.h>
#endif

#if (defined(__x86_64__) || defined(_M_AMD64)) && !defined(_M_ARM64EC)
  #define SIMDUTF_IS_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
  #define SIMDUTF_IS_ARM64 1
#elif defined(__PPC64__) || defined(_M_PPC64)
  #if defined(__VEC__) && defined(__ALTIVEC__)
    #define SIMDUTF_IS_PPC64 1
  #endif
#elif defined(__s390__)
// s390 IBM system. Big endian.
#elif (defined(__riscv) || defined(__riscv__)) && __riscv_xlen == 64
  // RISC-V 64-bit
  #define SIMDUTF_IS_RISCV64 1

  // #if __riscv_v_intrinsic >= 1000000
  //   #define SIMDUTF_HAS_RVV_INTRINSICS 1
  //   #define SIMDUTF_HAS_RVV_TARGET_REGION 1
  // #elif ...
  //  Check for special compiler versions that implement pre v1.0 intrinsics
  #if __riscv_v_intrinsic >= 11000
    #define SIMDUTF_HAS_RVV_INTRINSICS 1
  #endif

  #define SIMDUTF_HAS_ZVBB_INTRINSICS                                          \
    0 // there is currently no way to detect this

  #if SIMDUTF_HAS_RVV_INTRINSICS && __riscv_vector &&                          \
      __riscv_v_min_vlen >= 128 && __riscv_v_elen >= 64
    // RISC-V V extension
    #define SIMDUTF_IS_RVV 1
    #if SIMDUTF_HAS_ZVBB_INTRINSICS && __riscv_zvbb >= 1000000
      // RISC-V Vector Basic Bit-manipulation
      #define SIMDUTF_IS_ZVBB 1
    #endif
  #endif

#elif defined(__loongarch_lp64)
  #if defined(__loongarch_sx) && defined(__loongarch_asx)
    #define SIMDUTF_IS_LSX 1
    #define SIMDUTF_IS_LASX 1 // We can always run both
  #elif defined(__loongarch_sx)
    #define SIMDUTF_IS_LSX 1
    // Adjust for runtime dispatching support.
    #if defined(__GNUC__) && !defined(__clang__) &&                            \
        !defined(__INTEL_COMPILER) && !defined(__NVCOMPILER)
      #if __GNUC__ > 15 || (__GNUC__ == 15 && __GNUC_MINOR__ >= 0)
        // We are ok, we will support runtime dispatch for LASX.
      #else
        // We disable runtime dispatch for LASX, which means that we will not be
        // able to use LASX even if it is supported by the hardware. Loongson
        // users should update to GCC 15 or better.
        #define SIMDUTF_IMPLEMENTATION_LASX 0
      #endif
    #else
      // We are not using GCC, so we assume that we can support runtime dispatch
      // for LASX. https://godbolt.org/z/jcMnrjYhs
      #define SIMDUTF_IMPLEMENTATION_LASX 0
    #endif
  #endif
#else
  // The simdutf library is designed
  // for 64-bit processors and it seems that you are not
  // compiling for a known 64-bit platform. Please
  // use a 64-bit target such as x64 or 64-bit ARM for best performance.
  #define SIMDUTF_IS_32BITS 1

  // We do not support 32-bit platforms, but it can be
  // handy to identify them.
  #if defined(_M_IX86) || defined(__i386__)
    #define SIMDUTF_IS_X86_32BITS 1
  #elif defined(__arm__) || defined(_M_ARM)
    #define SIMDUTF_IS_ARM_32BITS 1
  #elif defined(__PPC__) || defined(_M_PPC)
    #define SIMDUTF_IS_PPC_32BITS 1
  #endif

#endif // defined(__x86_64__) || defined(_M_AMD64)

#ifdef SIMDUTF_IS_32BITS
  #ifndef SIMDUTF_NO_PORTABILITY_WARNING
  // In the future, we may want to warn users of 32-bit systems that
  // the simdutf does not support accelerated kernels for such systems.
  #endif // SIMDUTF_NO_PORTABILITY_WARNING
#endif   // SIMDUTF_IS_32BITS

// this is almost standard?
#define SIMDUTF_STRINGIFY_IMPLEMENTATION_(a) #a
#define SIMDUTF_STRINGIFY(a) SIMDUTF_STRINGIFY_IMPLEMENTATION_(a)

// Our fast kernels require 64-bit systems.
//
// On 32-bit x86, we lack 64-bit popcnt, lzcnt, blsr instructions.
// Furthermore, the number of SIMD registers is reduced.
//
// On 32-bit ARM, we would have smaller registers.
//
// The simdutf users should still have the fallback kernel. It is
// slower, but it should run everywhere.

//
// Enable valid runtime implementations, and select
// SIMDUTF_BUILTIN_IMPLEMENTATION
//

// We are going to use runtime dispatch.
#if defined(SIMDUTF_IS_X86_64) || defined(SIMDUTF_IS_LSX)
  #ifdef __clang__
    // clang does not have GCC push pop
    // warning: clang attribute push can't be used within a namespace in clang
    // up til 8.0 so SIMDUTF_TARGET_REGION and SIMDUTF_UNTARGET_REGION must be
    // *outside* of a namespace.
    #define SIMDUTF_TARGET_REGION(T)                                           \
      _Pragma(SIMDUTF_STRINGIFY(clang attribute push(                          \
          __attribute__((target(T))), apply_to = function)))
    #define SIMDUTF_UNTARGET_REGION _Pragma("clang attribute pop")
  #elif defined(__GNUC__)
    // GCC is easier
    #define SIMDUTF_TARGET_REGION(T)                                           \
      _Pragma("GCC push_options") _Pragma(SIMDUTF_STRINGIFY(GCC target(T)))
    #define SIMDUTF_UNTARGET_REGION _Pragma("GCC pop_options")
  #endif // clang then gcc

#endif // defined(SIMDUTF_IS_X86_64) || defined(SIMDUTF_IS_LSX)

// Default target region macros don't do anything.
#ifndef SIMDUTF_TARGET_REGION
  #define SIMDUTF_TARGET_REGION(T)
  #define SIMDUTF_UNTARGET_REGION
#endif

// Is threading enabled?
#if defined(_REENTRANT) || defined(_MT)
  #ifndef SIMDUTF_THREADS_ENABLED
    #define SIMDUTF_THREADS_ENABLED
  #endif
#endif

// workaround for large stack sizes under -O0.
// https://github.com/simdutf/simdutf/issues/691
#ifdef __APPLE__
  #ifndef __OPTIMIZE__
    // Apple systems have small stack sizes in secondary threads.
    // Lack of compiler optimization may generate high stack usage.
    // Users may want to disable threads for safety, but only when
    // in debug mode which we detect by the fact that the __OPTIMIZE__
    // macro is not defined.
    #undef SIMDUTF_THREADS_ENABLED
  #endif
#endif

#ifdef SIMDUTF_VISUAL_STUDIO
  // This is one case where we do not distinguish between
  // regular visual studio and clang under visual studio.
  // clang under Windows has _stricmp (like visual studio) but not strcasecmp
  // (as clang normally has)
  #define simdutf_strcasecmp _stricmp
  #define simdutf_strncasecmp _strnicmp
#else
  // The strcasecmp, strncasecmp, and strcasestr functions do not work with
  // multibyte strings (e.g. UTF-8). So they are only useful for ASCII in our
  // context.
  // https://www.gnu.org/software/libunistring/manual/libunistring.html#char-_002a-strings
  #define simdutf_strcasecmp strcasecmp
  #define simdutf_strncasecmp strncasecmp
#endif

#if defined(__GNUC__) && !defined(__clang__)
  #if __GNUC__ >= 11
    #define SIMDUTF_GCC11ORMORE 1
  #endif //  __GNUC__ >= 11
  #if __GNUC__ == 10
    #define SIMDUTF_GCC10 1
  #endif //  __GNUC__ == 10
  #if __GNUC__ < 10
    #define SIMDUTF_GCC9OROLDER 1
  #endif //  __GNUC__ == 10
#endif   // defined(__GNUC__) && !defined(__clang__)

#endif // SIMDUTF_PORTABILITY_H
/* end file include/simdutf/portability.h */
/* begin file include/simdutf/avx512.h */
#ifndef SIMDUTF_AVX512_H_
#define SIMDUTF_AVX512_H_

/*
    It's possible to override AVX512 settings with cmake DCMAKE_CXX_FLAGS.

    All preprocessor directives has form `SIMDUTF_HAS_AVX512{feature}`,
    where a feature is a code name for extensions.

    Please see the listing below to find which are supported.
*/

#ifndef SIMDUTF_HAS_AVX512F
  #if defined(__AVX512F__) && __AVX512F__ == 1
    #define SIMDUTF_HAS_AVX512F 1
  #endif
#endif

#ifndef SIMDUTF_HAS_AVX512DQ
  #if defined(__AVX512DQ__) && __AVX512DQ__ == 1
    #define SIMDUTF_HAS_AVX512DQ 1
  #endif
#endif

#ifndef SIMDUTF_HAS_AVX512IFMA
  #if defined(__AVX512IFMA__) && __AVX512IFMA__ == 1
    #define SIMDUTF_HAS_AVX512IFMA 1
  #endif
#endif

#ifndef SIMDUTF_HAS_AVX512CD
  #if defined(__AVX512CD__) && __AVX512CD__ == 1
    #define SIMDUTF_HAS_AVX512CD 1
  #endif
#endif

#ifndef SIMDUTF_HAS_AVX512BW
  #if defined(__AVX512BW__) && __AVX512BW__ == 1
    #define SIMDUTF_HAS_AVX512BW 1
  #endif
#endif

#ifndef SIMDUTF_HAS_AVX512VL
  #if defined(__AVX512VL__) && __AVX512VL__ == 1
    #define SIMDUTF_HAS_AVX512VL 1
  #endif
#endif

#ifndef SIMDUTF_HAS_AVX512VBMI
  #if defined(__AVX512VBMI__) && __AVX512VBMI__ == 1
    #define SIMDUTF_HAS_AVX512VBMI 1
  #endif
#endif

#ifndef SIMDUTF_HAS_AVX512VBMI2
  #if defined(__AVX512VBMI2__) && __AVX512VBMI2__ == 1
    #define SIMDUTF_HAS_AVX512VBMI2 1
  #endif
#endif

#ifndef SIMDUTF_HAS_AVX512VNNI
  #if defined(__AVX512VNNI__) && __AVX512VNNI__ == 1
    #define SIMDUTF_HAS_AVX512VNNI 1
  #endif
#endif

#ifndef SIMDUTF_HAS_AVX512BITALG
  #if defined(__AVX512BITALG__) && __AVX512BITALG__ == 1
    #define SIMDUTF_HAS_AVX512BITALG 1
  #endif
#endif

#ifndef SIMDUTF_HAS_AVX512VPOPCNTDQ
  #if defined(__AVX512VPOPCNTDQ__) && __AVX512VPOPCNTDQ__ == 1
    #define SIMDUTF_HAS_AVX512VPOPCNTDQ 1
  #endif
#endif

#endif // SIMDUTF_AVX512_H_
/* end file include/simdutf/avx512.h */

// Sometimes logging is useful, but we want it disabled by default
// and free of any logging code in release builds.
#ifdef SIMDUTF_LOGGING
  #include <cstdlib>
  #include <iostream>
  #define simdutf_log(msg)                                                     \
    std::cout << "[" << __FUNCTION__ << "]: " << msg << std::endl              \
              << "\t" << __FILE__ << ":" << __LINE__ << std::endl;
  #define simdutf_log_assert(cond, msg)                                        \
    do {                                                                       \
      if (!(cond)) {                                                           \
        std::cerr << "[" << __FUNCTION__ << "]: " << msg << std::endl          \
                  << "\t" << __FILE__ << ":" << __LINE__ << std::endl;         \
        std::abort();                                                          \
      }                                                                        \
    } while (0)
#else
  #define simdutf_log(msg)
  #define simdutf_log_assert(cond, msg)
#endif

#if SIMDUTF_CPLUSPLUS17
  #define simdutf_unused [[maybe_unused]]
#endif // SIMDUTF_CPLUSPLUS17

#if defined(SIMDUTF_REGULAR_VISUAL_STUDIO)
  #define SIMDUTF_DEPRECATED __declspec(deprecated)

  #define simdutf_really_inline __forceinline // really inline in release mode
  #define simdutf_always_inline __forceinline // always inline, no matter what
  #define simdutf_never_inline __declspec(noinline)

  #ifndef simdutf_unused
    #define simdutf_unused
  #endif // simdutf_unused
  #define simdutf_warn_unused

  #ifndef simdutf_likely
    #define simdutf_likely(x) x
  #endif
  #ifndef simdutf_unlikely
    #define simdutf_unlikely(x) x
  #endif

  #define SIMDUTF_PUSH_DISABLE_WARNINGS __pragma(warning(push))
  #define SIMDUTF_PUSH_DISABLE_ALL_WARNINGS __pragma(warning(push, 0))
  #define SIMDUTF_DISABLE_VS_WARNING(WARNING_NUMBER)                           \
    __pragma(warning(disable : WARNING_NUMBER))
  // Get rid of Intellisense-only warnings (Code Analysis)
  // Though __has_include is C++17, it is supported in Visual Studio 2017 or
  // better (_MSC_VER>=1910).
  #ifdef __has_include
    #if __has_include(<CppCoreCheck\Warnings.h>)
      #include <CppCoreCheck\Warnings.h>
      #define SIMDUTF_DISABLE_UNDESIRED_WARNINGS                               \
        SIMDUTF_DISABLE_VS_WARNING(ALL_CPPCORECHECK_WARNINGS)
    #endif
  #endif

  #ifndef SIMDUTF_DISABLE_UNDESIRED_WARNINGS
    #define SIMDUTF_DISABLE_UNDESIRED_WARNINGS
  #endif

  #define SIMDUTF_DISABLE_DEPRECATED_WARNING SIMDUTF_DISABLE_VS_WARNING(4996)
  #define SIMDUTF_DISABLE_STRICT_OVERFLOW_WARNING
  #define SIMDUTF_POP_DISABLE_WARNINGS __pragma(warning(pop))
  #define SIMDUTF_DISABLE_UNUSED_WARNING
#else // SIMDUTF_REGULAR_VISUAL_STUDIO
  #if defined(__OPTIMIZE__) || defined(NDEBUG)
    #define simdutf_really_inline inline __attribute__((always_inline))
  #else
    #define simdutf_really_inline inline
  #endif
  #define simdutf_always_inline                                                \
    inline __attribute__((always_inline)) // always inline, no matter what
  #define SIMDUTF_DEPRECATED __attribute__((deprecated))
  #define simdutf_never_inline inline __attribute__((noinline))
  #ifndef simdutf_unused
    #define simdutf_unused __attribute__((unused))
  #endif // simdutf_unused
  #define simdutf_warn_unused __attribute__((warn_unused_result))

  #ifndef simdutf_likely
    #define simdutf_likely(x) __builtin_expect(!!(x), 1)
  #endif
  #ifndef simdutf_unlikely
    #define simdutf_unlikely(x) __builtin_expect(!!(x), 0)
  #endif
  // clang-format off
  #define SIMDUTF_PUSH_DISABLE_WARNINGS _Pragma("GCC diagnostic push")
  // gcc doesn't seem to disable all warnings with all and extra, add warnings
  // here as necessary
  #define SIMDUTF_PUSH_DISABLE_ALL_WARNINGS                                    \
    SIMDUTF_PUSH_DISABLE_WARNINGS                                              \
    SIMDUTF_DISABLE_GCC_WARNING(-Weffc++)                                      \
    SIMDUTF_DISABLE_GCC_WARNING(-Wall)                                         \
    SIMDUTF_DISABLE_GCC_WARNING(-Wconversion)                                  \
    SIMDUTF_DISABLE_GCC_WARNING(-Wextra)                                       \
    SIMDUTF_DISABLE_GCC_WARNING(-Wattributes)                                  \
    SIMDUTF_DISABLE_GCC_WARNING(-Wimplicit-fallthrough)                        \
    SIMDUTF_DISABLE_GCC_WARNING(-Wnon-virtual-dtor)                            \
    SIMDUTF_DISABLE_GCC_WARNING(-Wreturn-type)                                 \
    SIMDUTF_DISABLE_GCC_WARNING(-Wshadow)                                      \
    SIMDUTF_DISABLE_GCC_WARNING(-Wunused-parameter)                            \
    SIMDUTF_DISABLE_GCC_WARNING(-Wunused-variable)
  #define SIMDUTF_PRAGMA(P) _Pragma(#P)
  #define SIMDUTF_DISABLE_GCC_WARNING(WARNING)                                 \
    SIMDUTF_PRAGMA(GCC diagnostic ignored #WARNING)
  #if defined(SIMDUTF_CLANG_VISUAL_STUDIO)
    #define SIMDUTF_DISABLE_UNDESIRED_WARNINGS                                 \
      SIMDUTF_DISABLE_GCC_WARNING(-Wmicrosoft-include)
  #else
    #define SIMDUTF_DISABLE_UNDESIRED_WARNINGS
  #endif
  #define SIMDUTF_DISABLE_DEPRECATED_WARNING                                   \
    SIMDUTF_DISABLE_GCC_WARNING(-Wdeprecated-declarations)
  #define SIMDUTF_DISABLE_STRICT_OVERFLOW_WARNING                              \
    SIMDUTF_DISABLE_GCC_WARNING(-Wstrict-overflow)
  #define SIMDUTF_POP_DISABLE_WARNINGS _Pragma("GCC diagnostic pop")
  #define SIMDUTF_DISABLE_UNUSED_WARNING                                       \
    SIMDUTF_PUSH_DISABLE_WARNINGS                                              \
    SIMDUTF_DISABLE_GCC_WARNING(-Wunused-function)                             \
    SIMDUTF_DISABLE_GCC_WARNING(-Wunused-const-variable)
  // clang-format on

#endif // MSC_VER

// Will evaluate to constexpr in C++23 or later. This makes it possible to mark
// functions constexpr if the "if consteval" feature is available to use.
#if SIMDUTF_CPLUSPLUS23
  #define simdutf_constexpr23 constexpr
#else
  #define simdutf_constexpr23
#endif

#ifndef SIMDUTF_DLLIMPORTEXPORT
  #if defined(SIMDUTF_VISUAL_STUDIO) // Visual Studio
                                     /**
                                      * Windows users need to do some extra work when building
                                      * or using a dynamic library (DLL). When building, we need
                                      * to set SIMDUTF_DLLIMPORTEXPORT to __declspec(dllexport).
                                      * When *using* the DLL, the user needs to set
                                      * SIMDUTF_DLLIMPORTEXPORT __declspec(dllimport).
                                      *
                                      * Static libraries not need require such work.
                                      *
                                      * It does not matter here whether you are using
                                      * the regular visual studio or clang under visual
                                      * studio, you still need to handle these issues.
                                      *
                                      * Non-Windows systems do not have this complexity.
                                      */
    #if SIMDUTF_BUILDING_WINDOWS_DYNAMIC_LIBRARY

      // We set SIMDUTF_BUILDING_WINDOWS_DYNAMIC_LIBRARY when we build a DLL
      // under Windows. It should never happen that both
      // SIMDUTF_BUILDING_WINDOWS_DYNAMIC_LIBRARY and
      // SIMDUTF_USING_WINDOWS_DYNAMIC_LIBRARY are set.
      #define SIMDUTF_DLLIMPORTEXPORT __declspec(dllexport)
    #elif SIMDUTF_USING_WINDOWS_DYNAMIC_LIBRARY
      // Windows user who call a dynamic library should set
      // SIMDUTF_USING_WINDOWS_DYNAMIC_LIBRARY to 1.

      #define SIMDUTF_DLLIMPORTEXPORT __declspec(dllimport)
    #else
      // We assume by default static linkage
      #define SIMDUTF_DLLIMPORTEXPORT
    #endif
  #else // defined(SIMDUTF_VISUAL_STUDIO)
    // Non-Windows systems do not have this complexity.
    #define SIMDUTF_DLLIMPORTEXPORT
  #endif // defined(SIMDUTF_VISUAL_STUDIO)
#endif

#if SIMDUTF_MAYBE_UNUSED_AVAILABLE
  #define simdutf_maybe_unused [[maybe_unused]]
#else
  #define simdutf_maybe_unused
#endif

#endif // SIMDUTF_COMMON_DEFS_H
/* end file include/simdutf/common_defs.h */
/* begin file include/simdutf/encoding_types.h */
#ifndef SIMDUTF_ENCODING_TYPES_H
#define SIMDUTF_ENCODING_TYPES_H
#include <string_view>

#if !defined(SIMDUTF_NO_STD_TEXT_ENCODING) &&                                  \
    defined(__cpp_lib_text_encoding) && __cpp_lib_text_encoding >= 202306L
  #define SIMDUTF_HAS_STD_TEXT_ENCODING 1
  #include <text_encoding>
#endif

namespace simdutf {

enum encoding_type {
  UTF8 = 1,      // BOM 0xef 0xbb 0xbf
  UTF16_LE = 2,  // BOM 0xff 0xfe
  UTF16_BE = 4,  // BOM 0xfe 0xff
  UTF32_LE = 8,  // BOM 0xff 0xfe 0x00 0x00
  UTF32_BE = 16, // BOM 0x00 0x00 0xfe 0xff
  Latin1 = 32,

  unspecified = 0
};

#ifndef SIMDUTF_IS_BIG_ENDIAN
  #error "SIMDUTF_IS_BIG_ENDIAN needs to be defined."
#endif

enum endianness {
  LITTLE = 0,
  BIG = 1,
  NATIVE =
#if SIMDUTF_IS_BIG_ENDIAN
      BIG
#else
      LITTLE
#endif
};

simdutf_warn_unused simdutf_really_inline constexpr bool
match_system(endianness e) {
  return e == endianness::NATIVE;
}

simdutf_warn_unused std::string_view to_string(encoding_type bom);

// Note that BOM for UTF8 is discouraged.
namespace BOM {

/**
 * Checks for a BOM. If not, returns unspecified
 * @param byte          the string to process
 * @param length        the length of the string in code units
 * @return the corresponding encoding
 */

simdutf_warn_unused encoding_type check_bom(const uint8_t *byte, size_t length);
simdutf_warn_unused encoding_type check_bom(const char *byte, size_t length);
/**
 * Returns the size, in bytes, of the BOM for a given encoding type.
 * Note that UTF8 BOM are discouraged.
 * @param bom         the encoding type
 * @return the size in bytes of the corresponding BOM
 */
simdutf_warn_unused size_t bom_byte_size(encoding_type bom);

} // namespace BOM

#ifdef SIMDUTF_HAS_STD_TEXT_ENCODING
/**
 * Convert a simdutf encoding type to a std::text_encoding.
 *
 * @param enc  the simdutf encoding type
 * @return     the corresponding std::text_encoding, or
 *             std::text_encoding::id::unknown for unspecified/unsupported
 */
simdutf_warn_unused constexpr std::text_encoding
to_std_encoding(encoding_type enc) noexcept {
  switch (enc) {
  case UTF8:
    return std::text_encoding(std::text_encoding::id::UTF8);
  case UTF16_LE:
    return std::text_encoding(std::text_encoding::id::UTF16LE);
  case UTF16_BE:
    return std::text_encoding(std::text_encoding::id::UTF16BE);
  case UTF32_LE:
    return std::text_encoding(std::text_encoding::id::UTF32LE);
  case UTF32_BE:
    return std::text_encoding(std::text_encoding::id::UTF32BE);
  case Latin1:
    return std::text_encoding(std::text_encoding::id::ISOLatin1);
  case unspecified:
  default:
    return std::text_encoding(std::text_encoding::id::unknown);
  }
}

/**
 * Convert a std::text_encoding to a simdutf encoding type.
 *
 * @param enc  the std::text_encoding
 * @return     the corresponding simdutf encoding type, or
 *             encoding_type::unspecified if the encoding is not supported
 */
simdutf_warn_unused constexpr encoding_type
from_std_encoding(const std::text_encoding &enc) noexcept {
  switch (enc.mib()) {
  case std::text_encoding::id::UTF8:
    return UTF8;
  case std::text_encoding::id::UTF16LE:
    return UTF16_LE;
  case std::text_encoding::id::UTF16BE:
    return UTF16_BE;
  case std::text_encoding::id::UTF32LE:
    return UTF32_LE;
  case std::text_encoding::id::UTF32BE:
    return UTF32_BE;
  case std::text_encoding::id::ISOLatin1:
    return Latin1;
  default:
    return unspecified;
  }
}

/**
 * Get the native-endian UTF-16 encoding type for this system.
 *
 * @return UTF16_LE on little-endian systems, UTF16_BE on big-endian systems
 */
simdutf_warn_unused constexpr encoding_type native_utf16_encoding() noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return UTF16_BE;
  #else
  return UTF16_LE;
  #endif
}

/**
 * Get the native-endian UTF-32 encoding type for this system.
 *
 * @return UTF32_LE on little-endian systems, UTF32_BE on big-endian systems
 */
simdutf_warn_unused constexpr encoding_type native_utf32_encoding() noexcept {
  #if SIMDUTF_IS_BIG_ENDIAN
  return UTF32_BE;
  #else
  return UTF32_LE;
  #endif
}

/**
 * Convert a std::text_encoding to a simdutf encoding type,
 * using native endianness for UTF-16/UTF-32 without explicit endianness.
 *
 * When the input is std::text_encoding::id::UTF16 or UTF32 (without LE/BE
 * suffix), this returns the native-endian simdutf variant.
 *
 * @param enc  the std::text_encoding
 * @return     the corresponding simdutf encoding type, or
 *             encoding_type::unspecified if the encoding is not supported
 */
simdutf_warn_unused constexpr encoding_type
from_std_encoding_native(const std::text_encoding &enc) noexcept {
  switch (enc.mib()) {
  case std::text_encoding::id::UTF8:
    return UTF8;
  case std::text_encoding::id::UTF16:
    return native_utf16_encoding();
  case std::text_encoding::id::UTF16LE:
    return UTF16_LE;
  case std::text_encoding::id::UTF16BE:
    return UTF16_BE;
  case std::text_encoding::id::UTF32:
    return native_utf32_encoding();
  case std::text_encoding::id::UTF32LE:
    return UTF32_LE;
  case std::text_encoding::id::UTF32BE:
    return UTF32_BE;
  case std::text_encoding::id::ISOLatin1:
    return Latin1;
  default:
    return unspecified;
  }
}
#endif // SIMDUTF_HAS_STD_TEXT_ENCODING

} // namespace simdutf
#endif
/* end file include/simdutf/encoding_types.h */
/* begin file include/simdutf/error.h */
#ifndef SIMDUTF_ERROR_H
#define SIMDUTF_ERROR_H
#include <string_view>

namespace simdutf {

enum error_code {
  SUCCESS = 0,
  HEADER_BITS, // Any byte must have fewer than 5 header bits.
  TOO_SHORT,   // The leading byte must be followed by N-1 continuation bytes,
               // where N is the UTF-8 character length This is also the error
               // when the input is truncated.
  TOO_LONG,    // We either have too many consecutive continuation bytes or the
               // string starts with a continuation byte.
  OVERLONG, // The decoded character must be above U+7F for two-byte characters,
            // U+7FF for three-byte characters, and U+FFFF for four-byte
            // characters.
  TOO_LARGE, // The decoded character must be less than or equal to
             // U+10FFFF,less than or equal than U+7F for ASCII OR less than
             // equal than U+FF for Latin1
  SURROGATE, // The decoded character must be not be in U+D800...DFFF (UTF-8 or
             // UTF-32)
             // OR
             // a high surrogate must be followed by a low surrogate
             // and a low surrogate must be preceded by a high surrogate
             // (UTF-16)
             // OR
             // there must be no surrogate at all and one is
             // found (Latin1 functions)
             // OR
             // *specifically* for the function
             // utf8_length_from_utf16_with_replacement, a surrogate (whether
             // in error or not) has been found (I.e., whether we are in the
             // Basic Multilingual Plane or not).
  INVALID_BASE64_CHARACTER, // Found a character that cannot be part of a valid
                            // base64 string. This may include a misplaced
                            // padding character ('=').
  BASE64_INPUT_REMAINDER,   // The base64 input terminates with a single
                            // character, excluding padding (=). It is also used
                            // in strict mode when padding is not adequate.
  BASE64_EXTRA_BITS,        // The base64 input terminates with non-zero
                            // padding bits.
  OUTPUT_BUFFER_TOO_SMALL,  // The provided buffer is too small.
  OTHER                     // Not related to validation/transcoding.
};

inline std::string_view error_to_string(error_code code) noexcept {
  switch (code) {
  case SUCCESS:
    return "SUCCESS";
  case HEADER_BITS:
    return "HEADER_BITS";
  case TOO_SHORT:
    return "TOO_SHORT";
  case TOO_LONG:
    return "TOO_LONG";
  case OVERLONG:
    return "OVERLONG";
  case TOO_LARGE:
    return "TOO_LARGE";
  case SURROGATE:
    return "SURROGATE";
  case INVALID_BASE64_CHARACTER:
    return "INVALID_BASE64_CHARACTER";
  case BASE64_INPUT_REMAINDER:
    return "BASE64_INPUT_REMAINDER";
  case BASE64_EXTRA_BITS:
    return "BASE64_EXTRA_BITS";
  case OUTPUT_BUFFER_TOO_SMALL:
    return "OUTPUT_BUFFER_TOO_SMALL";
  default:
    return "OTHER";
  }
}

struct result {
  error_code error;
  size_t count; // In case of error, indicates the position of the error. In
                // case of success, indicates the number of code units
                // validated/written.

  simdutf_really_inline simdutf_constexpr23 result() noexcept
      : error{error_code::SUCCESS}, count{0} {}

  simdutf_really_inline simdutf_constexpr23 result(error_code err,
                                                   size_t pos) noexcept
      : error{err}, count{pos} {}

  simdutf_really_inline simdutf_constexpr23 bool is_ok() const noexcept {
    return error == error_code::SUCCESS;
  }

  simdutf_really_inline simdutf_constexpr23 bool is_err() const noexcept {
    return error != error_code::SUCCESS;
  }
};

struct full_result {
  error_code error;
  size_t input_count;
  size_t output_count;
  bool padding_error = false; // true if the error is due to padding, only
                              // meaningful when error is not SUCCESS

  simdutf_really_inline simdutf_constexpr23 full_result() noexcept
      : error{error_code::SUCCESS}, input_count{0}, output_count{0} {}

  simdutf_really_inline simdutf_constexpr23 full_result(error_code err,
                                                        size_t pos_in,
                                                        size_t pos_out) noexcept
      : error{err}, input_count{pos_in}, output_count{pos_out} {}
  simdutf_really_inline simdutf_constexpr23 full_result(
      error_code err, size_t pos_in, size_t pos_out, bool padding_err) noexcept
      : error{err}, input_count{pos_in}, output_count{pos_out},
        padding_error{padding_err} {}

  simdutf_really_inline simdutf_constexpr23 operator result() const noexcept {
    if (error == error_code::SUCCESS) {
      return result{error, output_count};
    } else {
      return result{error, input_count};
    }
  }
};

} // namespace simdutf
#endif
/* end file include/simdutf/error.h */

SIMDUTF_PUSH_DISABLE_WARNINGS
SIMDUTF_DISABLE_UNDESIRED_WARNINGS

// Public API
/* begin file include/simdutf/simdutf_version.h */
// /include/simdutf/simdutf_version.h automatically generated by release.py,
// do not change by hand
#ifndef SIMDUTF_SIMDUTF_VERSION_H
#define SIMDUTF_SIMDUTF_VERSION_H

/** The version of simdutf being used (major.minor.revision) */
#define SIMDUTF_VERSION "9.1.0"

namespace simdutf {
enum {
  /**
   * The major version (MAJOR.minor.revision) of simdutf being used.
   */
  SIMDUTF_VERSION_MAJOR = 9,
  /**
   * The minor version (major.MINOR.revision) of simdutf being used.
   */
  SIMDUTF_VERSION_MINOR = 1,
  /**
   * The revision (major.minor.REVISION) of simdutf being used.
   */
  SIMDUTF_VERSION_REVISION = 0
};
} // namespace simdutf

#endif // SIMDUTF_SIMDUTF_VERSION_H
/* end file include/simdutf/simdutf_version.h */
/* begin file include/simdutf/implementation.h */
#ifndef SIMDUTF_IMPLEMENTATION_H
#define SIMDUTF_IMPLEMENTATION_H
#if !defined(SIMDUTF_NO_THREADS)
  #include <atomic>
#endif
#ifdef SIMDUTF_INTERNAL_TESTS
  #include <vector>
#endif
/* begin file include/simdutf/internal/isadetection.h */
/* From
https://github.com/endorno/pytorch/blob/master/torch/lib/TH/generic/simd/simd.h
Highly modified.

Copyright (c) 2016-     Facebook, Inc            (Adam Paszke)
Copyright (c) 2014-     Facebook, Inc            (Soumith Chintala)
Copyright (c) 2011-2014 Idiap Research Institute (Ronan Collobert)
Copyright (c) 2012-2014 Deepmind Technologies    (Koray Kavukcuoglu)
Copyright (c) 2011-2012 NEC Laboratories America (Koray Kavukcuoglu)
Copyright (c) 2011-2013 NYU                      (Clement Farabet)
Copyright (c) 2006-2010 NEC Laboratories America (Ronan Collobert, Leon Bottou,
Iain Melvin, Jason Weston) Copyright (c) 2006      Idiap Research Institute
(Samy Bengio) Copyright (c) 2001-2004 Idiap Research Institute (Ronan Collobert,
Samy Bengio, Johnny Mariethoz)

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

3. Neither the names of Facebook, Deepmind Technologies, NYU, NEC Laboratories
America and IDIAP Research Institute nor the names of its contributors may be
   used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef SIMDutf_INTERNAL_ISADETECTION_H
#define SIMDutf_INTERNAL_ISADETECTION_H

#include <cstdint>
#include <cstdlib>
#if defined(_MSC_VER)
  #include <intrin.h>
#elif (defined(HAVE_GCC_GET_CPUID) && defined(USE_GCC_GET_CPUID)) ||           \
    defined(__FILC__)
  #include <cpuid.h>
#endif

#ifdef __FILC__
  #include <stdfil.h>
#endif


// RISC-V ISA detection utilities
#if SIMDUTF_IS_RISCV64 && defined(__linux__)
  #include <unistd.h> // for syscall
// We define these ourselves, for backwards compatibility
struct simdutf_riscv_hwprobe {
  int64_t key;
  uint64_t value;
};
  #define simdutf_riscv_hwprobe(...) syscall(258, __VA_ARGS__)
  #define SIMDUTF_RISCV_HWPROBE_KEY_IMA_EXT_0 4
  #define SIMDUTF_RISCV_HWPROBE_IMA_V (1 << 2)
  #define SIMDUTF_RISCV_HWPROBE_EXT_ZVBB (1 << 17)
#endif // SIMDUTF_IS_RISCV64 && defined(__linux__)

#if defined(__loongarch__) && defined(__linux__)
  #include <sys/auxv.h>
// bits/hwcap.h
// #define HWCAP_LOONGARCH_LSX             (1 << 4)
// #define HWCAP_LOONGARCH_LASX            (1 << 5)
#endif

namespace simdutf {
namespace internal {

enum instruction_set {
  DEFAULT = 0x0,
  NEON = 0x1,
  AVX2 = 0x4,
  SSE42 = 0x8,
  PCLMULQDQ = 0x10,
  BMI1 = 0x20,
  BMI2 = 0x40,
  ALTIVEC = 0x80,
  AVX512F = 0x100,
  AVX512DQ = 0x200,
  AVX512IFMA = 0x400,
  AVX512PF = 0x800,
  AVX512ER = 0x1000,
  AVX512CD = 0x2000,
  AVX512BW = 0x4000,
  AVX512VL = 0x8000,
  AVX512VBMI2 = 0x10000,
  AVX512VPOPCNTDQ = 0x2000,
  RVV = 0x4000,
  ZVBB = 0x8000,
  LSX = 0x40000,
  LASX = 0x80000,
};

#if defined(__PPC64__)

static inline uint32_t detect_supported_architectures() {
  return instruction_set::ALTIVEC;
}

#elif SIMDUTF_IS_RISCV64

static inline uint32_t detect_supported_architectures() {
  uint32_t host_isa = instruction_set::DEFAULT;
  #if SIMDUTF_IS_RVV
  host_isa |= instruction_set::RVV;
  #endif
  #if SIMDUTF_IS_ZVBB
  host_isa |= instruction_set::ZVBB;
  #endif
  #if defined(__linux__)
  simdutf_riscv_hwprobe probes[] = {{SIMDUTF_RISCV_HWPROBE_KEY_IMA_EXT_0, 0}};
  long ret = simdutf_riscv_hwprobe(&probes, sizeof probes / sizeof *probes, 0,
                                   nullptr, 0);
  if (ret == 0) {
    uint64_t extensions = probes[0].value;
    if (extensions & SIMDUTF_RISCV_HWPROBE_IMA_V)
      host_isa |= instruction_set::RVV;
    if (extensions & SIMDUTF_RISCV_HWPROBE_EXT_ZVBB)
      host_isa |= instruction_set::ZVBB;
  }
  #endif
  #if defined(RUN_IN_SPIKE_SIMULATOR)
  // Proxy Kernel does not implement yet hwprobe syscall
  host_isa |= instruction_set::RVV;
  #endif
  return host_isa;
}

#elif defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)

static inline uint32_t detect_supported_architectures() {
  return instruction_set::NEON;
}

#elif defined(__x86_64__) || defined(_M_AMD64) // x64

namespace {
namespace cpuid_bit {
// Can be found on Intel ISA Reference for CPUID

// EAX = 0x01
constexpr uint32_t pclmulqdq = uint32_t(1)
                               << 1; ///< @private bit  1 of ECX for EAX=0x1
constexpr uint32_t sse42 = uint32_t(1)
                           << 20; ///< @private bit 20 of ECX for EAX=0x1
constexpr uint32_t osxsave =
    (uint32_t(1) << 26) |
    (uint32_t(1) << 27); ///< @private bits 26+27 of ECX for EAX=0x1

// EAX = 0x7f (Structured Extended Feature Flags), ECX = 0x00 (Sub-leaf)
// See: "Table 3-8. Information Returned by CPUID Instruction"
namespace ebx {
constexpr uint32_t bmi1 = uint32_t(1) << 3;
constexpr uint32_t avx2 = uint32_t(1) << 5;
constexpr uint32_t bmi2 = uint32_t(1) << 8;
constexpr uint32_t avx512f = uint32_t(1) << 16;
constexpr uint32_t avx512dq = uint32_t(1) << 17;
constexpr uint32_t avx512ifma = uint32_t(1) << 21;
constexpr uint32_t avx512cd = uint32_t(1) << 28;
constexpr uint32_t avx512bw = uint32_t(1) << 30;
constexpr uint32_t avx512vl = uint32_t(1) << 31;
} // namespace ebx

namespace ecx {
constexpr uint32_t avx512vbmi = uint32_t(1) << 1;
constexpr uint32_t avx512vbmi2 = uint32_t(1) << 6;
constexpr uint32_t avx512vnni = uint32_t(1) << 11;
constexpr uint32_t avx512bitalg = uint32_t(1) << 12;
constexpr uint32_t avx512vpopcnt = uint32_t(1) << 14;
} // namespace ecx
namespace edx {
constexpr uint32_t avx512vp2intersect = uint32_t(1) << 8;
}
namespace xcr0_bit {
constexpr uint64_t avx256_saved = uint64_t(1) << 2; ///< @private bit 2 = AVX
constexpr uint64_t avx512_saved =
    uint64_t(7) << 5; ///< @private bits 5,6,7 = opmask, ZMM_hi256, hi16_ZMM
} // namespace xcr0_bit
} // namespace cpuid_bit
} // namespace

static inline void cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx,
                         uint32_t *edx) {
  #if defined(_MSC_VER)
  int cpu_info[4];
  __cpuidex(cpu_info, *eax, *ecx);
  *eax = cpu_info[0];
  *ebx = cpu_info[1];
  *ecx = cpu_info[2];
  *edx = cpu_info[3];
  #elif (defined(HAVE_GCC_GET_CPUID) && defined(USE_GCC_GET_CPUID)) ||         \
      defined(__FILC__)
  uint32_t level = *eax;
  __get_cpuid(level, eax, ebx, ecx, edx);
  #else
  uint32_t a = *eax, b, c = *ecx, d;
  asm volatile("cpuid\n\t" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
  *eax = a;
  *ebx = b;
  *ecx = c;
  *edx = d;
  #endif
}

static inline uint64_t xgetbv() {
  #if defined(_MSC_VER)
  return _xgetbv(0);
  #elif defined(__FILC__)
  return zxgetbv();
  #else
  uint32_t xcr0_lo, xcr0_hi;
  asm volatile("xgetbv\n\t" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
  return xcr0_lo | ((uint64_t)xcr0_hi << 32);
  #endif
}

static inline uint32_t detect_supported_architectures() {
  uint32_t eax;
  uint32_t ebx = 0;
  uint32_t ecx = 0;
  uint32_t edx = 0;
  uint32_t host_isa = 0x0;

  // EBX for EAX=0x1
  eax = 0x1;
  cpuid(&eax, &ebx, &ecx, &edx);

  if (ecx & cpuid_bit::sse42) {
    host_isa |= instruction_set::SSE42;
  }

  if (ecx & cpuid_bit::pclmulqdq) {
    host_isa |= instruction_set::PCLMULQDQ;
  }

  if ((ecx & cpuid_bit::osxsave) != cpuid_bit::osxsave) {
    return host_isa;
  }

  // xgetbv for checking if the OS saves registers
  uint64_t xcr0 = xgetbv();

  if ((xcr0 & cpuid_bit::xcr0_bit::avx256_saved) == 0) {
    return host_isa;
  }
  // ECX for EAX=0x7
  eax = 0x7;
  ecx = 0x0; // Sub-leaf = 0
  cpuid(&eax, &ebx, &ecx, &edx);
  if (ebx & cpuid_bit::ebx::avx2) {
    host_isa |= instruction_set::AVX2;
  }
  if (ebx & cpuid_bit::ebx::bmi1) {
    host_isa |= instruction_set::BMI1;
  }
  if (ebx & cpuid_bit::ebx::bmi2) {
    host_isa |= instruction_set::BMI2;
  }
  if (!((xcr0 & cpuid_bit::xcr0_bit::avx512_saved) ==
        cpuid_bit::xcr0_bit::avx512_saved)) {
    return host_isa;
  }
  if (ebx & cpuid_bit::ebx::avx512f) {
    host_isa |= instruction_set::AVX512F;
  }
  if (ebx & cpuid_bit::ebx::avx512bw) {
    host_isa |= instruction_set::AVX512BW;
  }
  if (ebx & cpuid_bit::ebx::avx512cd) {
    host_isa |= instruction_set::AVX512CD;
  }
  if (ebx & cpuid_bit::ebx::avx512dq) {
    host_isa |= instruction_set::AVX512DQ;
  }
  if (ebx & cpuid_bit::ebx::avx512vl) {
    host_isa |= instruction_set::AVX512VL;
  }
  if (ecx & cpuid_bit::ecx::avx512vbmi2) {
    host_isa |= instruction_set::AVX512VBMI2;
  }
  if (ecx & cpuid_bit::ecx::avx512vpopcnt) {
    host_isa |= instruction_set::AVX512VPOPCNTDQ;
  }
  return host_isa;
}
#elif defined(__loongarch__)

static inline uint32_t detect_supported_architectures() {
  uint32_t host_isa = instruction_set::DEFAULT;
  #if defined(__linux__)
  uint64_t hwcap = 0;
  hwcap = getauxval(AT_HWCAP);
  if (hwcap & HWCAP_LOONGARCH_LSX) {
    host_isa |= instruction_set::LSX;
  }
  if (hwcap & HWCAP_LOONGARCH_LASX) {
    host_isa |= instruction_set::LASX;
  }
  #endif
  return host_isa;
}
#else // fallback

// includes 32-bit ARM.
static inline uint32_t detect_supported_architectures() {
  return instruction_set::DEFAULT;
}

#endif // end SIMD extension detection code

} // namespace internal
} // namespace simdutf

#endif // SIMDutf_INTERNAL_ISADETECTION_H
/* end file include/simdutf/internal/isadetection.h */

#include <string_view>
#if SIMDUTF_SPAN
  #include <concepts>
  #include <type_traits>
  #include <span>
  #include <tuple>
  #include <utility> // for std::unreachable
#endif
// The following defines are conditionally enabled/disabled during amalgamation.
// By default all features are enabled, regular code shouldn't check them. Only
// when user code really relies of a selected subset, it's good to verify these
// flags, like:
//
//      #if !SIMDUTF_FEATURE_UTF16
//      #   error("Please amalgamate simdutf with UTF-16 support")
//      #endif
//
#ifndef SIMDUTF_FEATURE_DETECT_ENCODING
  #define SIMDUTF_FEATURE_DETECT_ENCODING 1
#endif
#ifndef SIMDUTF_FEATURE_ASCII
  #define SIMDUTF_FEATURE_ASCII 1
#endif
#ifndef SIMDUTF_FEATURE_LATIN1
  #define SIMDUTF_FEATURE_LATIN1 1
#endif
#ifndef SIMDUTF_FEATURE_UTF8
  #define SIMDUTF_FEATURE_UTF8 1
#endif
#ifndef SIMDUTF_FEATURE_UTF16
  #define SIMDUTF_FEATURE_UTF16 1
#endif
#ifndef SIMDUTF_FEATURE_UTF32
  #define SIMDUTF_FEATURE_UTF32 1
#endif
#ifndef SIMDUTF_FEATURE_BASE64
  #define SIMDUTF_FEATURE_BASE64 1
#endif

/// helpers placed in namespace detail are not a part of the public API
namespace simdutf {
namespace detail {
namespace {
// this is to avoid including <algorithm> just for min
constexpr std::size_t min(std::size_t a, std::size_t b) {
  return a < b ? a : b;
}
template <typename T, typename U>
constexpr std::size_t min(const T &a, const U &b) = delete;
} // namespace
} // namespace detail
} // namespace simdutf

#if SIMDUTF_CPLUSPLUS23
/* begin file include/simdutf/constexpr_ptr.h */
#ifndef SIMDUTF_CONSTEXPR_PTR_H
#define SIMDUTF_CONSTEXPR_PTR_H

#include <cstddef>

namespace simdutf {
namespace detail {
/**
 * The constexpr_ptr class is a workaround for reinterpret_cast not being
 * allowed during constant evaluation.
 */
template <typename to, typename from>
  requires(sizeof(to) == sizeof(from))
struct constexpr_ptr {
  const from *p;

  constexpr explicit constexpr_ptr(const from *ptr) noexcept : p(ptr) {}

  constexpr to operator*() const noexcept { return static_cast<to>(*p); }

  constexpr constexpr_ptr &operator++() noexcept {
    ++p;
    return *this;
  }

  constexpr constexpr_ptr operator++(int) noexcept {
    auto old = *this;
    ++p;
    return old;
  }

  constexpr constexpr_ptr &operator--() noexcept {
    --p;
    return *this;
  }

  constexpr constexpr_ptr operator--(int) noexcept {
    auto old = *this;
    --p;
    return old;
  }

  constexpr constexpr_ptr &operator+=(std::ptrdiff_t n) noexcept {
    p += n;
    return *this;
  }

  constexpr constexpr_ptr &operator-=(std::ptrdiff_t n) noexcept {
    p -= n;
    return *this;
  }

  constexpr constexpr_ptr operator+(std::ptrdiff_t n) const noexcept {
    return constexpr_ptr{p + n};
  }

  constexpr constexpr_ptr operator-(std::ptrdiff_t n) const noexcept {
    return constexpr_ptr{p - n};
  }

  constexpr std::ptrdiff_t operator-(const constexpr_ptr &o) const noexcept {
    return p - o.p;
  }

  constexpr to operator[](std::ptrdiff_t n) const noexcept {
    return static_cast<to>(*(p + n));
  }

  // to prevent compilation errors for memcpy, even if it is never
  // called during constant evaluation
  constexpr operator const void *() const noexcept { return p; }
};

template <typename to, typename from>
constexpr constexpr_ptr<to, from> constexpr_cast_ptr(from *p) noexcept {
  return constexpr_ptr<to, from>{p};
}

/**
 * helper type for constexpr_writeptr, so it is possible to
 * do "*ptr = val;"
 */
template <typename SrcType, typename TargetType>
struct constexpr_write_ptr_proxy {

  constexpr explicit constexpr_write_ptr_proxy(TargetType *raw) : p(raw) {}

  constexpr constexpr_write_ptr_proxy &operator=(SrcType v) {
    *p = static_cast<TargetType>(v);
    return *this;
  }

  TargetType *p;
};

/**
 * helper for working around reinterpret_cast not being allowed during constexpr
 * evaluation. will try to act as a SrcType* but actually write to the pointer
 * given in the constructor, which is of another type TargetType
 */
template <typename SrcType, typename TargetType> struct constexpr_write_ptr {
  constexpr explicit constexpr_write_ptr(TargetType *raw) : p(raw) {}

  constexpr constexpr_write_ptr_proxy<SrcType, TargetType> operator*() const {
    return constexpr_write_ptr_proxy<SrcType, TargetType>{p};
  }

  constexpr constexpr_write_ptr_proxy<SrcType, TargetType>
  operator[](std::ptrdiff_t n) const {
    return constexpr_write_ptr_proxy<SrcType, TargetType>{p + n};
  }

  constexpr constexpr_write_ptr &operator++() {
    ++p;
    return *this;
  }

  constexpr constexpr_write_ptr operator++(int) {
    constexpr_write_ptr old = *this;
    ++p;
    return old;
  }

  constexpr std::ptrdiff_t operator-(const constexpr_write_ptr &other) const {
    return p - other.p;
  }

  TargetType *p;
};

template <typename SrcType, typename TargetType>
constexpr auto constexpr_cast_writeptr(TargetType *raw) {
  return constexpr_write_ptr<SrcType, TargetType>{raw};
}

} // namespace detail
} // namespace simdutf
#endif
/* end file include/simdutf/constexpr_ptr.h */
#endif

#if SIMDUTF_SPAN
/// helpers placed in namespace detail are not a part of the public API
namespace simdutf {
namespace detail {
/**
 * matches a byte, in the many ways C++ allows. note that these
 * are all distinct types.
 */
template <typename T>
concept byte_like = std::is_same_v<T, std::byte> ||     //
                    std::is_same_v<T, char> ||          //
                    std::is_same_v<T, signed char> ||   //
                    std::is_same_v<T, unsigned char> || //
                    std::is_same_v<T, char8_t>;

template <typename T>
concept is_byte_like = byte_like<std::remove_cvref_t<T>>;

template <typename T>
concept is_pointer = std::is_pointer_v<T>;

/**
 * matches anything that behaves like std::span and points to character-like
 * data such as: std::byte, char, unsigned char, signed char, std::int8_t,
 * std::uint8_t
 */
template <typename T>
concept input_span_of_byte_like = requires(const T &t) {
  { t.size() } noexcept -> std::convertible_to<std::size_t>;
  { t.data() } noexcept -> is_pointer;
  { *t.data() } noexcept -> is_byte_like;
};

template <typename T>
concept is_mutable = !std::is_const_v<std::remove_reference_t<T>>;

/**
 * like span_of_byte_like, but for an output span (intended to be written to)
 */
template <typename T>
concept output_span_of_byte_like = requires(T &t) {
  { t.size() } noexcept -> std::convertible_to<std::size_t>;
  { t.data() } noexcept -> is_pointer;
  { *t.data() } noexcept -> is_byte_like;
  { *t.data() } noexcept -> is_mutable;
};

/**
 * a pointer like object, when indexed, results in a byte like result.
 * valid examples: char*, const char*, std::array<char,10>
 * invalid examples: int*, std::array<int,10>
 */
template <class InputPtr>
concept indexes_into_byte_like = requires(InputPtr p) {
  { std::decay_t<decltype(p[0])>{} } -> simdutf::detail::byte_like;
};
template <class InputPtr>
concept indexes_into_utf16 = requires(InputPtr p) {
  { std::decay_t<decltype(p[0])>{} } -> std::same_as<char16_t>;
};
template <class InputPtr>
concept indexes_into_utf32 = requires(InputPtr p) {
  { std::decay_t<decltype(p[0])>{} } -> std::same_as<char32_t>;
};

template <class InputPtr>
concept index_assignable_from_char = requires(InputPtr p, char s) {
  { p[0] = s };
};

/**
 * a pointer like object that results in a uint32_t when indexed.
 * valid examples: uint32_t*
 */
template <class InputPtr>
concept indexes_into_uint32 = requires(InputPtr p) {
  { std::decay_t<decltype(p[0])>{} } -> std::same_as<std::uint32_t>;
};
} // namespace detail
} // namespace simdutf
#endif // SIMDUTF_SPAN

// these includes are needed for constexpr support. they are
// not part of the public api.
/* begin file include/simdutf/scalar/swap_bytes.h */
#ifndef SIMDUTF_SWAP_BYTES_H
#define SIMDUTF_SWAP_BYTES_H

namespace simdutf {
namespace scalar {

constexpr inline simdutf_warn_unused uint16_t
u16_swap_bytes(const uint16_t word) {
  return uint16_t((word >> 8) | (word << 8));
}

constexpr inline simdutf_warn_unused uint32_t
u32_swap_bytes(const uint32_t word) {
  return ((word >> 24) & 0xff) |      // move byte 3 to byte 0
         ((word << 8) & 0xff0000) |   // move byte 1 to byte 2
         ((word >> 8) & 0xff00) |     // move byte 2 to byte 1
         ((word << 24) & 0xff000000); // byte 0 to byte 3
}

namespace utf32 {
template <endianness big_endian> constexpr uint32_t swap_if_needed(uint32_t c) {
  return !match_system(big_endian) ? scalar::u32_swap_bytes(c) : c;
}
} // namespace utf32

namespace utf16 {
template <endianness big_endian> constexpr uint16_t swap_if_needed(uint16_t c) {
  return !match_system(big_endian) ? scalar::u16_swap_bytes(c) : c;
}
} // namespace utf16

} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/swap_bytes.h */
/* begin file include/simdutf/scalar/ascii.h */
#ifndef SIMDUTF_ASCII_H
#define SIMDUTF_ASCII_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace ascii {

template <class InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_warn_unused simdutf_constexpr23 bool validate(InputPtr data,
                                                      size_t len) noexcept {
  uint64_t pos = 0;

#if SIMDUTF_CPLUSPLUS23
  // avoid memcpy during constant evaluation
  if !consteval
#endif
  // process in blocks of 16 bytes when possible
  {
    for (; pos + 16 <= len; pos += 16) {
      uint64_t v1;
      std::memcpy(&v1, data + pos, sizeof(uint64_t));
      uint64_t v2;
      std::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
      uint64_t v{v1 | v2};
      if ((v & 0x8080808080808080) != 0) {
        return false;
      }
    }
  }

  // process the tail byte-by-byte
  for (; pos < len; pos++) {
    if (static_cast<std::uint8_t>(data[pos]) >= 0b10000000) {
      return false;
    }
  }
  return true;
}
template <class InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_warn_unused simdutf_constexpr23 result
validate_with_errors(InputPtr data, size_t len) noexcept {
  size_t pos = 0;
#if SIMDUTF_CPLUSPLUS23
  // avoid memcpy during constant evaluation
  if !consteval
#endif
  {
    // process in blocks of 16 bytes when possible
    for (; pos + 16 <= len; pos += 16) {
      uint64_t v1;
      std::memcpy(&v1, data + pos, sizeof(uint64_t));
      uint64_t v2;
      std::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
      uint64_t v{v1 | v2};
      if ((v & 0x8080808080808080) != 0) {
        for (; pos < len; pos++) {
          if (static_cast<std::uint8_t>(data[pos]) >= 0b10000000) {
            return result(error_code::TOO_LARGE, pos);
          }
        }
      }
    }
  }

  // process the tail byte-by-byte
  for (; pos < len; pos++) {
    if (static_cast<std::uint8_t>(data[pos]) >= 0b10000000) {
      return result(error_code::TOO_LARGE, pos);
    }
  }
  return result(error_code::SUCCESS, pos);
}

} // namespace ascii
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/ascii.h */
/* begin file include/simdutf/scalar/atomic_util.h */
#ifndef SIMDUTF_ATOMIC_UTIL_H
#define SIMDUTF_ATOMIC_UTIL_H
#if SIMDUTF_ATOMIC_REF
  #include <atomic>
  #include <cstring>
namespace simdutf {
namespace scalar {

// This function is a memcpy that uses atomic operations to read from the
// source.
inline void memcpy_atomic_read(char *dst, const char *src, size_t len) {
  static_assert(std::atomic_ref<char>::required_alignment == sizeof(char),
                "std::atomic_ref requires the same alignment as char_type");
  // We expect all 64-bit systems to be able to read 64-bit words from an
  // aligned memory region atomically. You might be able to do better on
  // specific systems, e.g., x64 systems can read 128-bit words atomically.
  constexpr size_t alignment = sizeof(uint64_t);

  // Lambda for atomic byte-by-byte copy
  auto bbb_memcpy_atomic_read = [](char *bytedst, const char *bytesrc,
                                   size_t bytelen) noexcept {
    char *mutable_src = const_cast<char *>(bytesrc);
    for (size_t j = 0; j < bytelen; ++j) {
      bytedst[j] =
          std::atomic_ref<char>(mutable_src[j]).load(std::memory_order_relaxed);
    }
  };

  // Handle unaligned start
  size_t offset = reinterpret_cast<std::uintptr_t>(src) % alignment;
  if (offset) {
    size_t to_align = detail::min(len, alignment - offset);
    bbb_memcpy_atomic_read(dst, src, to_align);
    src += to_align;
    dst += to_align;
    len -= to_align;
  }

  // Process aligned 64-bit chunks
  while (len >= alignment) {
    auto *src_aligned = reinterpret_cast<uint64_t *>(const_cast<char *>(src));
    const auto dst_value =
        std::atomic_ref<uint64_t>(*src_aligned).load(std::memory_order_relaxed);
    std::memcpy(dst, &dst_value, sizeof(uint64_t));
    src += alignment;
    dst += alignment;
    len -= alignment;
  }

  // Handle remaining bytes
  if (len) {
    bbb_memcpy_atomic_read(dst, src, len);
  }
}

// This function is a memcpy that uses atomic operations to write to the
// destination.
inline void memcpy_atomic_write(char *dst, const char *src, size_t len) {
  static_assert(std::atomic_ref<char>::required_alignment == sizeof(char),
                "std::atomic_ref requires the same alignment as char");
  // We expect all 64-bit systems to be able to write 64-bit words to an aligned
  // memory region atomically.
  // You might be able to do better on specific systems, e.g., x64 systems can
  // write 128-bit words atomically.
  constexpr size_t alignment = sizeof(uint64_t);

  // Lambda for atomic byte-by-byte write
  auto bbb_memcpy_atomic_write = [](char *bytedst, const char *bytesrc,
                                    size_t bytelen) noexcept {
    for (size_t j = 0; j < bytelen; ++j) {
      std::atomic_ref<char>(bytedst[j])
          .store(bytesrc[j], std::memory_order_relaxed);
    }
  };

  // Handle unaligned start
  size_t offset = reinterpret_cast<std::uintptr_t>(dst) % alignment;
  if (offset) {
    size_t to_align = detail::min(len, alignment - offset);
    bbb_memcpy_atomic_write(dst, src, to_align);
    dst += to_align;
    src += to_align;
    len -= to_align;
  }

  // Process aligned 64-bit chunks
  while (len >= alignment) {
    auto *dst_aligned = reinterpret_cast<uint64_t *>(dst);
    uint64_t src_val;
    std::memcpy(&src_val, src, sizeof(uint64_t)); // Non-atomic read from src
    std::atomic_ref<uint64_t>(*dst_aligned)
        .store(src_val, std::memory_order_relaxed);
    dst += alignment;
    src += alignment;
    len -= alignment;
  }

  // Handle remaining bytes
  if (len) {
    bbb_memcpy_atomic_write(dst, src, len);
  }
}
} // namespace scalar
} // namespace simdutf
#endif // SIMDUTF_ATOMIC_REF
#endif // SIMDUTF_ATOMIC_UTIL_H
/* end file include/simdutf/scalar/atomic_util.h */
/* begin file include/simdutf/scalar/latin1.h */
#ifndef SIMDUTF_LATIN1_H
#define SIMDUTF_LATIN1_H

namespace simdutf {
namespace scalar {
namespace {
namespace latin1 {

simdutf_really_inline size_t utf8_length_from_latin1(const char *buf,
                                                     size_t len) {
  const uint8_t *c = reinterpret_cast<const uint8_t *>(buf);
  size_t answer = 0;
  for (size_t i = 0; i < len; i++) {
    if ((c[i] >> 7)) {
      answer++;
    }
  }
  return answer + len;
}

} // namespace latin1
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/latin1.h */
/* begin file include/simdutf/scalar/latin1_to_utf16/latin1_to_utf16.h */
#ifndef SIMDUTF_LATIN1_TO_UTF16_H
#define SIMDUTF_LATIN1_TO_UTF16_H

namespace simdutf {
namespace scalar {
namespace {
namespace latin1_to_utf16 {

template <endianness big_endian, typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 size_t convert(InputPtr data, size_t len,
                                   char16_t *utf16_output) {
  size_t pos = 0;
  char16_t *start{utf16_output};

  while (pos < len) {
    uint16_t word =
        uint8_t(data[pos]); // extend Latin-1 char to 16-bit Unicode code point
    *utf16_output++ =
        char16_t(match_system(big_endian) ? word : u16_swap_bytes(word));
    pos++;
  }

  return utf16_output - start;
}

template <endianness big_endian>
inline result convert_with_errors(const char *buf, size_t len,
                                  char16_t *utf16_output) {
  const uint8_t *data = reinterpret_cast<const uint8_t *>(buf);
  size_t pos = 0;
  char16_t *start{utf16_output};

  while (pos < len) {
    uint16_t word =
        uint16_t(data[pos]); // extend Latin-1 char to 16-bit Unicode code point
    *utf16_output++ =
        char16_t(match_system(big_endian) ? word : u16_swap_bytes(word));
    pos++;
  }

  return result(error_code::SUCCESS, utf16_output - start);
}

} // namespace latin1_to_utf16
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/latin1_to_utf16/latin1_to_utf16.h */
/* begin file include/simdutf/scalar/latin1_to_utf32/latin1_to_utf32.h */
#ifndef SIMDUTF_LATIN1_TO_UTF32_H
#define SIMDUTF_LATIN1_TO_UTF32_H

namespace simdutf {
namespace scalar {
namespace {
namespace latin1_to_utf32 {

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 size_t convert(InputPtr data, size_t len,
                                   char32_t *utf32_output) {
  char32_t *start{utf32_output};
  for (size_t i = 0; i < len; i++) {
    *utf32_output++ = uint8_t(data[i]);
  }
  return utf32_output - start;
}

} // namespace latin1_to_utf32
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/latin1_to_utf32/latin1_to_utf32.h */
/* begin file include/simdutf/scalar/latin1_to_utf8/latin1_to_utf8.h */
#ifndef SIMDUTF_LATIN1_TO_UTF8_H
#define SIMDUTF_LATIN1_TO_UTF8_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace latin1_to_utf8 {

template <typename InputPtr, typename OutputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires(simdutf::detail::indexes_into_byte_like<InputPtr> &&
           simdutf::detail::index_assignable_from_char<OutputPtr>)
#endif
simdutf_constexpr23 size_t convert(InputPtr data, size_t len,
                                   OutputPtr utf8_output) {
  // const unsigned char *data = reinterpret_cast<const unsigned char *>(buf);
  size_t pos = 0;
  size_t utf8_pos = 0;

  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 16 ASCII bytes
      if (pos + 16 <= len) { // if it is safe to read 16 more bytes, check that
                             // they are ascii
        uint64_t v1;
        ::memcpy(&v1, data + pos, sizeof(uint64_t));
        uint64_t v2;
        ::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
        uint64_t v{v1 |
                   v2}; // We are only interested in these bits: 1000 1000 1000
                        // 1000, so it makes sense to concatenate everything
        if ((v & 0x8080808080808080) ==
            0) { // if NONE of these are set, e.g. all of them are zero, then
                 // everything is ASCII
          size_t final_pos = pos + 16;
          while (pos < final_pos) {
            utf8_output[utf8_pos++] = char(data[pos]);
            pos++;
          }
          continue;
        }
      } // if (pos + 16 <= len)
    } // !consteval scope

    unsigned char byte = data[pos];
    if ((byte & 0x80) == 0) { // if ASCII
      // will generate one UTF-8 bytes
      utf8_output[utf8_pos++] = char(byte);
      pos++;
    } else {
      // will generate two UTF-8 bytes
      utf8_output[utf8_pos++] = char((byte >> 6) | 0b11000000);
      utf8_output[utf8_pos++] = char((byte & 0b111111) | 0b10000000);
      pos++;
    }
  } // while
  return utf8_pos;
}

simdutf_really_inline size_t convert(const char *buf, size_t len,
                                     char *utf8_output) {
  return convert(reinterpret_cast<const unsigned char *>(buf), len,
                 utf8_output);
}

inline size_t convert_safe(const char *buf, size_t len, char *utf8_output,
                           size_t utf8_len) {
  const unsigned char *data = reinterpret_cast<const unsigned char *>(buf);
  size_t pos = 0;
  size_t skip_pos = 0;
  size_t utf8_pos = 0;
  while (pos < len && utf8_pos < utf8_len) {
    // try to convert the next block of 16 ASCII bytes
    if (pos >= skip_pos && pos + 16 <= len &&
        utf8_pos + 16 <= utf8_len) { // if it is safe to read 16 more bytes,
                                     // check that they are ascii
      uint64_t v1;
      ::memcpy(&v1, data + pos, sizeof(uint64_t));
      uint64_t v2;
      ::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
      uint64_t v{v1 |
                 v2}; // We are only interested in these bits: 1000 1000 1000
                      // 1000, so it makes sense to concatenate everything
      if ((v & 0x8080808080808080) ==
          0) { // if NONE of these are set, e.g. all of them are zero, then
               // everything is ASCII
        ::memcpy(utf8_output + utf8_pos, buf + pos, 16);
        utf8_pos += 16;
        pos += 16;
      } else {
        // At least one of the next 16 bytes are not ASCII, we will process them
        // one by one
        skip_pos = pos + 16;
      }
    } else {
      const auto byte = data[pos];
      if ((byte & 0x80) == 0) { // if ASCII
        // will generate one UTF-8 bytes
        utf8_output[utf8_pos++] = char(byte);
        pos++;
      } else if (utf8_pos + 2 <= utf8_len) {
        // will generate two UTF-8 bytes
        utf8_output[utf8_pos++] = char((byte >> 6) | 0b11000000);
        utf8_output[utf8_pos++] = char((byte & 0b111111) | 0b10000000);
        pos++;
      } else {
        break;
      }
    }
  }
  return utf8_pos;
}

template <typename InputPtr, typename OutputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires(simdutf::detail::indexes_into_byte_like<InputPtr> &&
           simdutf::detail::index_assignable_from_char<OutputPtr>)
#endif
simdutf_constexpr23 size_t convert_safe_constexpr(InputPtr data, size_t len,
                                                  OutputPtr utf8_output,
                                                  size_t utf8_len) {
  size_t pos = 0;
  size_t utf8_pos = 0;
  while (pos < len && utf8_pos < utf8_len) {
    const unsigned char byte = data[pos];
    if ((byte & 0x80) == 0) { // if ASCII
      // will generate one UTF-8 bytes
      utf8_output[utf8_pos++] = char(byte);
      pos++;
    } else if (utf8_pos + 2 <= utf8_len) {
      // will generate two UTF-8 bytes
      utf8_output[utf8_pos++] = char((byte >> 6) | 0b11000000);
      utf8_output[utf8_pos++] = char((byte & 0b111111) | 0b10000000);
      pos++;
    } else {
      break;
    }
  }
  return utf8_pos;
}

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 simdutf_warn_unused size_t
utf8_length_from_latin1(InputPtr input, size_t length) noexcept {
  size_t answer = length;
  size_t i = 0;

#if SIMDUTF_CPLUSPLUS23
  if !consteval
#endif
  {
    auto pop = [](uint64_t v) {
      return (size_t)(((v >> 7) & UINT64_C(0x0101010101010101)) *
                          UINT64_C(0x0101010101010101) >>
                      56);
    };
    for (; i + 32 <= length; i += 32) {
      uint64_t v;
      memcpy(&v, input + i, 8);
      answer += pop(v);
      memcpy(&v, input + i + 8, sizeof(v));
      answer += pop(v);
      memcpy(&v, input + i + 16, sizeof(v));
      answer += pop(v);
      memcpy(&v, input + i + 24, sizeof(v));
      answer += pop(v);
    }
    for (; i + 8 <= length; i += 8) {
      uint64_t v;
      memcpy(&v, input + i, sizeof(v));
      answer += pop(v);
    }
  } // !consteval scope
  for (; i + 1 <= length; i += 1) {
    answer += static_cast<uint8_t>(input[i]) >> 7;
  }
  return answer;
}

} // namespace latin1_to_utf8
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/latin1_to_utf8/latin1_to_utf8.h */
/* begin file include/simdutf/scalar/utf16.h */
#ifndef SIMDUTF_UTF16_H
#define SIMDUTF_UTF16_H

namespace simdutf {
namespace scalar {
namespace utf16 {

template <endianness big_endian>
simdutf_warn_unused simdutf_constexpr23 bool
validate_as_ascii(const char16_t *data, size_t len) noexcept {
  for (size_t pos = 0; pos < len; pos++) {
    char16_t word = scalar::utf16::swap_if_needed<big_endian>(data[pos]);
    if (word >= 0x80) {
      return false;
    }
  }
  return true;
}

template <endianness big_endian>
inline simdutf_warn_unused simdutf_constexpr23 bool
validate(const char16_t *data, size_t len) noexcept {
  uint64_t pos = 0;
  while (pos < len) {
    char16_t word = scalar::utf16::swap_if_needed<big_endian>(data[pos]);
    if ((word & 0xF800) == 0xD800) {
      if (pos + 1 >= len) {
        return false;
      }
      char16_t diff = char16_t(word - 0xD800);
      if (diff > 0x3FF) {
        return false;
      }
      char16_t next_word = !match_system(big_endian)
                               ? u16_swap_bytes(data[pos + 1])
                               : data[pos + 1];
      char16_t diff2 = char16_t(next_word - 0xDC00);
      if (diff2 > 0x3FF) {
        return false;
      }
      pos += 2;
    } else {
      pos++;
    }
  }
  return true;
}

template <endianness big_endian>
inline simdutf_warn_unused simdutf_constexpr23 result
validate_with_errors(const char16_t *data, size_t len) noexcept {
  size_t pos = 0;
  while (pos < len) {
    char16_t word = scalar::utf16::swap_if_needed<big_endian>(data[pos]);
    if ((word & 0xF800) == 0xD800) {
      if (pos + 1 >= len) {
        return result(error_code::SURROGATE, pos);
      }
      char16_t diff = char16_t(word - 0xD800);
      if (diff > 0x3FF) {
        return result(error_code::SURROGATE, pos);
      }
      char16_t next_word = !match_system(big_endian)
                               ? u16_swap_bytes(data[pos + 1])
                               : data[pos + 1];
      char16_t diff2 = uint16_t(next_word - 0xDC00);
      if (diff2 > 0x3FF) {
        return result(error_code::SURROGATE, pos);
      }
      pos += 2;
    } else {
      pos++;
    }
  }
  return result(error_code::SUCCESS, pos);
}

template <endianness big_endian>
simdutf_constexpr23 size_t count_code_points(const char16_t *p, size_t len) {
  // We are not BOM aware.
  size_t counter{0};
  for (size_t i = 0; i < len; i++) {
    char16_t word = scalar::utf16::swap_if_needed<big_endian>(p[i]);
    counter += ((word & 0xFC00) != 0xDC00);
  }
  return counter;
}

template <endianness big_endian>
simdutf_constexpr23 size_t utf8_length_from_utf16(const char16_t *p,
                                                  size_t len) {
  // We are not BOM aware.
  size_t counter{0};
  for (size_t i = 0; i < len; i++) {
    char16_t word = scalar::utf16::swap_if_needed<big_endian>(p[i]);
    counter++; // ASCII
    counter += static_cast<size_t>(
        word >
        0x7F); // non-ASCII is at least 2 bytes, surrogates are 2*2 == 4 bytes
    counter += static_cast<size_t>((word > 0x7FF && word <= 0xD7FF) ||
                                   (word >= 0xE000)); // three-byte
  }
  return counter;
}

template <endianness big_endian>
simdutf_constexpr23 size_t utf32_length_from_utf16(const char16_t *p,
                                                   size_t len) {
  // We are not BOM aware.
  size_t counter{0};
  for (size_t i = 0; i < len; i++) {
    char16_t word = scalar::utf16::swap_if_needed<big_endian>(p[i]);
    counter += ((word & 0xFC00) != 0xDC00);
  }
  return counter;
}

simdutf_really_inline simdutf_constexpr23 void
change_endianness_utf16(const char16_t *input, size_t size, char16_t *output) {
  for (size_t i = 0; i < size; i++) {
    *output++ = char16_t(input[i] >> 8 | input[i] << 8);
  }
}

template <endianness big_endian>
simdutf_warn_unused simdutf_constexpr23 size_t
trim_partial_utf16(const char16_t *input, size_t length) {
  if (length == 0) {
    return 0;
  }
  uint16_t last_word = uint16_t(input[length - 1]);
  last_word = scalar::utf16::swap_if_needed<big_endian>(last_word);
  length -= ((last_word & 0xFC00) == 0xD800);
  return length;
}

template <endianness big_endian> constexpr bool is_high_surrogate(char16_t c) {
  c = scalar::utf16::swap_if_needed<big_endian>(c);
  return (0xd800 <= c && c <= 0xdbff);
}

template <endianness big_endian> constexpr bool is_low_surrogate(char16_t c) {
  c = scalar::utf16::swap_if_needed<big_endian>(c);
  return (0xdc00 <= c && c <= 0xdfff);
}

simdutf_unused simdutf_really_inline constexpr bool high_surrogate(char16_t c) {
  return (0xd800 <= c && c <= 0xdbff);
}

template <endianness big_endian>
simdutf_constexpr23 result
utf8_length_from_utf16_with_replacement(const char16_t *p, size_t len) {
  bool any_surrogates = false;
  // We are not BOM aware.
  size_t counter{0};
  for (size_t i = 0; i < len; i++) {
    if (is_high_surrogate<big_endian>(p[i])) {
      any_surrogates = true;
      // surrogate pair
      if (i + 1 < len && is_low_surrogate<big_endian>(p[i + 1])) {
        counter += 4;
        i++; // skip low surrogate
      } else {
        counter += 3; // unpaired high surrogate replaced by U+FFFD
      }
      continue;
    } else if (is_low_surrogate<big_endian>(p[i])) {
      any_surrogates = true;
      counter += 3; // unpaired low surrogate replaced by U+FFFD
      continue;
    }
    char16_t word = !match_system(big_endian) ? u16_swap_bytes(p[i]) : p[i];
    counter++; // at least 1 byte
    counter +=
        static_cast<size_t>(word > 0x7F); // non-ASCII is at least 2 bytes
    counter += static_cast<size_t>(word > 0x7FF); // three-byte
  }
  return {any_surrogates ? error_code::SURROGATE : error_code::SUCCESS,
          counter};
}

// variable templates are a C++14 extension
template <endianness big_endian> constexpr char16_t replacement() {
  return !match_system(big_endian) ? scalar::u16_swap_bytes(0xfffd) : 0xfffd;
}

template <endianness big_endian>
simdutf_constexpr23 void to_well_formed_utf16(const char16_t *input, size_t len,
                                              char16_t *output) {
  const char16_t replacement = utf16::replacement<big_endian>();
  bool high_surrogate_prev = false, high_surrogate, low_surrogate;
  size_t i = 0;
  for (; i < len; i++) {
    char16_t c = input[i];
    high_surrogate = is_high_surrogate<big_endian>(c);
    low_surrogate = is_low_surrogate<big_endian>(c);
    if (high_surrogate_prev && !low_surrogate) {
      output[i - 1] = replacement;
    }

    if (!high_surrogate_prev && low_surrogate) {
      output[i] = replacement;
    } else {
      output[i] = input[i];
    }
    high_surrogate_prev = high_surrogate;
  }

  /* string may not end with high surrogate */
  if (high_surrogate_prev) {
    output[i - 1] = replacement;
  }
}

} // namespace utf16
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf16.h */
/* begin file include/simdutf/scalar/utf16_to_latin1/utf16_to_latin1.h */
#ifndef SIMDUTF_UTF16_TO_LATIN1_H
#define SIMDUTF_UTF16_TO_LATIN1_H

#include <cstring> // for std::memcpy

namespace simdutf {
namespace scalar {
namespace {
namespace utf16_to_latin1 {

template <endianness big_endian, typename InputPtr, typename OutputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires(simdutf::detail::indexes_into_utf16<InputPtr> &&
           simdutf::detail::index_assignable_from_char<OutputPtr>)
#endif
simdutf_constexpr23 size_t convert(InputPtr data, size_t len,
                                   OutputPtr latin_output) {
  if (len == 0) {
    return 0;
  }
  size_t pos = 0;
  const auto latin_output_start = latin_output;
  uint16_t word = 0;
  uint16_t too_large = 0;

  while (pos < len) {
    word = !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    too_large |= word;
    *latin_output++ = char(word & 0xFF);
    pos++;
  }
  if ((too_large & 0xFF00) != 0) {
    return 0;
  }

  return latin_output - latin_output_start;
}

template <endianness big_endian, typename InputPtr, typename OutputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires(simdutf::detail::indexes_into_utf16<InputPtr> &&
           simdutf::detail::index_assignable_from_char<OutputPtr>)
#endif
simdutf_constexpr23 result convert_with_errors(InputPtr data, size_t len,
                                               OutputPtr latin_output) {
  if (len == 0) {
    return result(error_code::SUCCESS, 0);
  }
  size_t pos = 0;
  auto start = latin_output;
  uint16_t word;

  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      if (pos + 16 <= len) { // if it is safe to read 32 more bytes, check that
                             // they are Latin1
        uint64_t v1, v2, v3, v4;
        ::memcpy(&v1, data + pos, sizeof(uint64_t));
        ::memcpy(&v2, data + pos + 4, sizeof(uint64_t));
        ::memcpy(&v3, data + pos + 8, sizeof(uint64_t));
        ::memcpy(&v4, data + pos + 12, sizeof(uint64_t));

        if constexpr (!match_system(big_endian)) {
          v1 = (v1 >> 8) | (v1 << (64 - 8));
        }
        if constexpr (!match_system(big_endian)) {
          v2 = (v2 >> 8) | (v2 << (64 - 8));
        }
        if constexpr (!match_system(big_endian)) {
          v3 = (v3 >> 8) | (v3 << (64 - 8));
        }
        if constexpr (!match_system(big_endian)) {
          v4 = (v4 >> 8) | (v4 << (64 - 8));
        }

        if (((v1 | v2 | v3 | v4) & 0xFF00FF00FF00FF00) == 0) {
          size_t final_pos = pos + 16;
          while (pos < final_pos) {
            *latin_output++ = !match_system(big_endian)
                                  ? char(u16_swap_bytes(data[pos]))
                                  : char(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }

    word = !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    if ((word & 0xFF00) == 0) {
      *latin_output++ = char(word & 0xFF);
      pos++;
    } else {
      return result(error_code::TOO_LARGE, pos);
    }
  }
  return result(error_code::SUCCESS, latin_output - start);
}

} // namespace utf16_to_latin1
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf16_to_latin1/utf16_to_latin1.h */
/* begin file include/simdutf/scalar/utf16_to_latin1/valid_utf16_to_latin1.h */
#ifndef SIMDUTF_VALID_UTF16_TO_LATIN1_H
#define SIMDUTF_VALID_UTF16_TO_LATIN1_H

namespace simdutf {
namespace scalar {
namespace {
namespace utf16_to_latin1 {

template <endianness big_endian, class InputIterator, class OutputIterator>
simdutf_constexpr23 inline size_t
convert_valid_impl(InputIterator data, size_t len,
                   OutputIterator latin_output) {
  static_assert(
      std::is_same<typename std::decay<decltype(*data)>::type, uint16_t>::value,
      "must decay to uint16_t");
  size_t pos = 0;
  const auto start = latin_output;
  uint16_t word = 0;

  while (pos < len) {
    word = !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    *latin_output++ = char(word);
    pos++;
  }

  return latin_output - start;
}

template <endianness big_endian>
simdutf_really_inline size_t convert_valid(const char16_t *buf, size_t len,
                                           char *latin_output) {
  return convert_valid_impl<big_endian>(reinterpret_cast<const uint16_t *>(buf),
                                        len, latin_output);
}
} // namespace utf16_to_latin1
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf16_to_latin1/valid_utf16_to_latin1.h */
/* begin file include/simdutf/scalar/utf16_to_utf32/utf16_to_utf32.h */
#ifndef SIMDUTF_UTF16_TO_UTF32_H
#define SIMDUTF_UTF16_TO_UTF32_H

namespace simdutf {
namespace scalar {
namespace {
namespace utf16_to_utf32 {

template <endianness big_endian>
simdutf_constexpr23 size_t convert(const char16_t *data, size_t len,
                                   char32_t *utf32_output) {
  size_t pos = 0;
  char32_t *start{utf32_output};
  while (pos < len) {
    uint16_t word =
        !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    if ((word & 0xF800) != 0xD800) {
      // No surrogate pair, extend 16-bit word to 32-bit word
      *utf32_output++ = char32_t(word);
      pos++;
    } else {
      // must be a surrogate pair
      uint16_t diff = uint16_t(word - 0xD800);
      if (diff > 0x3FF) {
        return 0;
      }
      if (pos + 1 >= len) {
        return 0;
      } // minimal bound checking
      uint16_t next_word = !match_system(big_endian)
                               ? u16_swap_bytes(data[pos + 1])
                               : data[pos + 1];
      uint16_t diff2 = uint16_t(next_word - 0xDC00);
      if (diff2 > 0x3FF) {
        return 0;
      }
      uint32_t value = (diff << 10) + diff2 + 0x10000;
      *utf32_output++ = char32_t(value);
      pos += 2;
    }
  }
  return utf32_output - start;
}

template <endianness big_endian>
simdutf_constexpr23 result convert_with_errors(const char16_t *data, size_t len,
                                               char32_t *utf32_output) {
  size_t pos = 0;
  char32_t *start{utf32_output};
  while (pos < len) {
    uint16_t word =
        !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    if ((word & 0xF800) != 0xD800) {
      // No surrogate pair, extend 16-bit word to 32-bit word
      *utf32_output++ = char32_t(word);
      pos++;
    } else {
      // must be a surrogate pair
      uint16_t diff = uint16_t(word - 0xD800);
      if (diff > 0x3FF) {
        return result(error_code::SURROGATE, pos);
      }
      if (pos + 1 >= len) {
        return result(error_code::SURROGATE, pos);
      } // minimal bound checking
      uint16_t next_word = !match_system(big_endian)
                               ? u16_swap_bytes(data[pos + 1])
                               : data[pos + 1];
      uint16_t diff2 = uint16_t(next_word - 0xDC00);
      if (diff2 > 0x3FF) {
        return result(error_code::SURROGATE, pos);
      }
      uint32_t value = (diff << 10) + diff2 + 0x10000;
      *utf32_output++ = char32_t(value);
      pos += 2;
    }
  }
  return result(error_code::SUCCESS, utf32_output - start);
}

} // namespace utf16_to_utf32
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf16_to_utf32/utf16_to_utf32.h */
/* begin file include/simdutf/scalar/utf16_to_utf32/valid_utf16_to_utf32.h */
#ifndef SIMDUTF_VALID_UTF16_TO_UTF32_H
#define SIMDUTF_VALID_UTF16_TO_UTF32_H

namespace simdutf {
namespace scalar {
namespace {
namespace utf16_to_utf32 {

template <endianness big_endian>
simdutf_constexpr23 size_t convert_valid(const char16_t *data, size_t len,
                                         char32_t *utf32_output) {
  size_t pos = 0;
  char32_t *start{utf32_output};
  while (pos < len) {
    uint16_t word =
        !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    if ((word & 0xF800) != 0xD800) {
      // No surrogate pair, extend 16-bit word to 32-bit word
      *utf32_output++ = char32_t(word);
      pos++;
    } else {
      // must be a surrogate pair
      uint16_t diff = uint16_t(word - 0xD800);
      if (pos + 1 >= len) {
        return 0;
      } // minimal bound checking
      uint16_t next_word = !match_system(big_endian)
                               ? u16_swap_bytes(data[pos + 1])
                               : data[pos + 1];
      uint16_t diff2 = uint16_t(next_word - 0xDC00);
      uint32_t value = (diff << 10) + diff2 + 0x10000;
      *utf32_output++ = char32_t(value);
      pos += 2;
    }
  }
  return utf32_output - start;
}

} // namespace utf16_to_utf32
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf16_to_utf32/valid_utf16_to_utf32.h */
/* begin file include/simdutf/scalar/utf16_to_utf8/utf16_to_utf8.h */
#ifndef SIMDUTF_UTF16_TO_UTF8_H
#define SIMDUTF_UTF16_TO_UTF8_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf16_to_utf8 {

template <endianness big_endian, typename InputPtr, typename OutputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_utf16<InputPtr>
// FIXME constrain output as well
#endif
simdutf_constexpr23 size_t convert(InputPtr data, size_t len,
                                   OutputPtr utf8_output) {
  size_t pos = 0;
  const auto start = utf8_output;
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 8 bytes
      if (pos + 4 <= len) { // if it is safe to read 8 more bytes, check that
                            // they are ascii
        uint64_t v;
        ::memcpy(&v, data + pos, sizeof(uint64_t));
        if constexpr (!match_system(big_endian)) {
          v = (v >> 8) | (v << (64 - 8));
        }
        if ((v & 0xFF80FF80FF80FF80) == 0) {
          size_t final_pos = pos + 4;
          while (pos < final_pos) {
            *utf8_output++ = !match_system(big_endian)
                                 ? char(u16_swap_bytes(data[pos]))
                                 : char(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }
    uint16_t word =
        !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    if ((word & 0xFF80) == 0) {
      // will generate one UTF-8 bytes
      *utf8_output++ = char(word);
      pos++;
    } else if ((word & 0xF800) == 0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else if ((word & 0xF800) != 0xD800) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 12) | 0b11100000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {
      // must be a surrogate pair
      if (pos + 1 >= len) {
        return 0;
      }
      uint16_t diff = uint16_t(word - 0xD800);
      if (diff > 0x3FF) {
        return 0;
      }
      uint16_t next_word = !match_system(big_endian)
                               ? u16_swap_bytes(data[pos + 1])
                               : data[pos + 1];
      uint16_t diff2 = uint16_t(next_word - 0xDC00);
      if (diff2 > 0x3FF) {
        return 0;
      }
      uint32_t value = (diff << 10) + diff2 + 0x10000;
      // will generate four UTF-8 bytes
      // we have 0b11110XXX 0b10XXXXXX 0b10XXXXXX 0b10XXXXXX
      *utf8_output++ = char((value >> 18) | 0b11110000);
      *utf8_output++ = char(((value >> 12) & 0b111111) | 0b10000000);
      *utf8_output++ = char(((value >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((value & 0b111111) | 0b10000000);
      pos += 2;
    }
  }
  return utf8_output - start;
}

template <endianness big_endian, bool check_output = false, typename InputPtr,
          typename OutputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires(simdutf::detail::indexes_into_utf16<InputPtr> &&
           simdutf::detail::index_assignable_from_char<OutputPtr>)
#endif
simdutf_constexpr23 full_result convert_with_errors(InputPtr data, size_t len,
                                                    OutputPtr utf8_output,
                                                    size_t utf8_len = 0) {
  if (check_output && utf8_len == 0) {
    return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, 0, 0);
  }

  size_t pos = 0;
  auto start = utf8_output;
  auto end = utf8_output + utf8_len;

  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 8 bytes
      if (pos + 4 <= len) { // if it is safe to read 8 more bytes, check that
                            // they are ascii
        uint64_t v;
        ::memcpy(&v, data + pos, sizeof(uint64_t));
        if constexpr (!match_system(big_endian))
          v = (v >> 8) | (v << (64 - 8));
        if ((v & 0xFF80FF80FF80FF80) == 0) {
          size_t final_pos = pos + 4;
          while (pos < final_pos) {
            if (check_output && size_t(end - utf8_output) < 1) {
              return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, pos,
                                 utf8_output - start);
            }
            *utf8_output++ = !match_system(big_endian)
                                 ? char(u16_swap_bytes(data[pos]))
                                 : char(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }

    uint16_t word =
        !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    if ((word & 0xFF80) == 0) {
      // will generate one UTF-8 bytes
      if (check_output && size_t(end - utf8_output) < 1) {
        return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, pos,
                           utf8_output - start);
      }
      *utf8_output++ = char(word);
      pos++;
    } else if ((word & 0xF800) == 0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      if (check_output && size_t(end - utf8_output) < 2) {
        return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, pos,
                           utf8_output - start);
      }
      *utf8_output++ = char((word >> 6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;

    } else if ((word & 0xF800) != 0xD800) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
      if (check_output && size_t(end - utf8_output) < 3) {
        return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, pos,
                           utf8_output - start);
      }
      *utf8_output++ = char((word >> 12) | 0b11100000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {

      if (check_output && size_t(end - utf8_output) < 4) {
        return full_result(error_code::OUTPUT_BUFFER_TOO_SMALL, pos,
                           utf8_output - start);
      }
      // must be a surrogate pair
      if (pos + 1 >= len) {
        return full_result(error_code::SURROGATE, pos, utf8_output - start);
      }
      uint16_t diff = uint16_t(word - 0xD800);
      if (diff > 0x3FF) {
        return full_result(error_code::SURROGATE, pos, utf8_output - start);
      }
      uint16_t next_word = !match_system(big_endian)
                               ? u16_swap_bytes(data[pos + 1])
                               : data[pos + 1];
      uint16_t diff2 = uint16_t(next_word - 0xDC00);
      if (diff2 > 0x3FF) {
        return full_result(error_code::SURROGATE, pos, utf8_output - start);
      }
      uint32_t value = (diff << 10) + diff2 + 0x10000;
      // will generate four UTF-8 bytes
      // we have 0b11110XXX 0b10XXXXXX 0b10XXXXXX 0b10XXXXXX
      *utf8_output++ = char((value >> 18) | 0b11110000);
      *utf8_output++ = char(((value >> 12) & 0b111111) | 0b10000000);
      *utf8_output++ = char(((value >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((value & 0b111111) | 0b10000000);
      pos += 2;
    }
  }
  return full_result(error_code::SUCCESS, pos, utf8_output - start);
}

template <endianness big_endian>
inline result simple_convert_with_errors(const char16_t *buf, size_t len,
                                         char *utf8_output) {
  return convert_with_errors<big_endian, false>(buf, len, utf8_output, 0);
}

template <endianness big_endian>
simdutf_constexpr23 size_t convert_with_replacement(const char16_t *data,
                                                    size_t len,
                                                    char *utf8_output) {
  size_t pos = 0;
  char *start = utf8_output;
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 8 bytes
      if (pos + 4 <= len) { // if it is safe to read 8 more bytes, check that
                            // they are ascii
        uint64_t v;
        ::memcpy(&v, data + pos, sizeof(uint64_t));
        if constexpr (!match_system(big_endian)) {
          v = (v >> 8) | (v << (64 - 8));
        }
        if ((v & 0xFF80FF80FF80FF80) == 0) {
          size_t final_pos = pos + 4;
          while (pos < final_pos) {
            *utf8_output++ = !match_system(big_endian)
                                 ? char(u16_swap_bytes(data[pos]))
                                 : char(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }
    uint16_t word =
        !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    if ((word & 0xFF80) == 0) {
      // will generate one UTF-8 bytes
      *utf8_output++ = char(word);
      pos++;
    } else if ((word & 0xF800) == 0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else if ((word & 0xF800) != 0xD800) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 12) | 0b11100000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {
      // surrogate range
      uint16_t diff = uint16_t(word - 0xD800);
      if (diff <= 0x3FF && pos + 1 < len) {
        // high surrogate, check for valid pair
        uint16_t next_word = !match_system(big_endian)
                                 ? u16_swap_bytes(data[pos + 1])
                                 : data[pos + 1];
        uint16_t diff2 = uint16_t(next_word - 0xDC00);
        if (diff2 <= 0x3FF) {
          // valid surrogate pair
          uint32_t value = (diff << 10) + diff2 + 0x10000;
          // will generate four UTF-8 bytes
          *utf8_output++ = char((value >> 18) | 0b11110000);
          *utf8_output++ = char(((value >> 12) & 0b111111) | 0b10000000);
          *utf8_output++ = char(((value >> 6) & 0b111111) | 0b10000000);
          *utf8_output++ = char((value & 0b111111) | 0b10000000);
          pos += 2;
          continue;
        }
      }
      // unpaired surrogate: replace with U+FFFD (0xEF 0xBF 0xBD)
      *utf8_output++ = char(0xef);
      *utf8_output++ = char(0xbf);
      *utf8_output++ = char(0xbd);
      pos++;
    }
  }
  return utf8_output - start;
}

} // namespace utf16_to_utf8
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf16_to_utf8/utf16_to_utf8.h */
/* begin file include/simdutf/scalar/utf16_to_utf8/valid_utf16_to_utf8.h */
#ifndef SIMDUTF_VALID_UTF16_TO_UTF8_H
#define SIMDUTF_VALID_UTF16_TO_UTF8_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf16_to_utf8 {

template <endianness big_endian, typename InputPtr, typename OutputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires(simdutf::detail::indexes_into_utf16<InputPtr> &&
           simdutf::detail::index_assignable_from_char<OutputPtr>)
#endif
simdutf_constexpr23 size_t convert_valid(InputPtr data, size_t len,
                                         OutputPtr utf8_output) {
  size_t pos = 0;
  auto start = utf8_output;
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 4 ASCII characters
      if (pos + 4 <= len) { // if it is safe to read 8 more bytes, check that
                            // they are ascii
        uint64_t v;
        ::memcpy(&v, data + pos, sizeof(uint64_t));
        if constexpr (!match_system(big_endian)) {
          v = (v >> 8) | (v << (64 - 8));
        }
        if ((v & 0xFF80FF80FF80FF80) == 0) {
          size_t final_pos = pos + 4;
          while (pos < final_pos) {
            *utf8_output++ = !match_system(big_endian)
                                 ? char(u16_swap_bytes(data[pos]))
                                 : char(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }

    uint16_t word =
        !match_system(big_endian) ? u16_swap_bytes(data[pos]) : data[pos];
    if ((word & 0xFF80) == 0) {
      // will generate one UTF-8 bytes
      *utf8_output++ = char(word);
      pos++;
    } else if ((word & 0xF800) == 0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else if ((word & 0xF800) != 0xD800) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 12) | 0b11100000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {
      // must be a surrogate pair
      uint16_t diff = uint16_t(word - 0xD800);
      if (pos + 1 >= len) {
        return 0;
      } // minimal bound checking
      uint16_t next_word = !match_system(big_endian)
                               ? u16_swap_bytes(data[pos + 1])
                               : data[pos + 1];
      uint16_t diff2 = uint16_t(next_word - 0xDC00);
      uint32_t value = (diff << 10) + diff2 + 0x10000;
      // will generate four UTF-8 bytes
      // we have 0b11110XXX 0b10XXXXXX 0b10XXXXXX 0b10XXXXXX
      *utf8_output++ = char((value >> 18) | 0b11110000);
      *utf8_output++ = char(((value >> 12) & 0b111111) | 0b10000000);
      *utf8_output++ = char(((value >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((value & 0b111111) | 0b10000000);
      pos += 2;
    }
  }
  return utf8_output - start;
}

} // namespace utf16_to_utf8
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf16_to_utf8/valid_utf16_to_utf8.h */
/* begin file include/simdutf/scalar/utf32.h */
#ifndef SIMDUTF_UTF32_H
#define SIMDUTF_UTF32_H

namespace simdutf {
namespace scalar {
namespace utf32 {

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_uint32<InputPtr>
#endif
simdutf_warn_unused simdutf_constexpr23 bool validate(InputPtr data,
                                                      size_t len) noexcept {
  uint64_t pos = 0;
  for (; pos < len; pos++) {
    uint32_t word = data[pos];
    if (word > 0x10FFFF || (word >= 0xD800 && word <= 0xDFFF)) {
      return false;
    }
  }
  return true;
}

simdutf_warn_unused simdutf_really_inline bool validate(const char32_t *buf,
                                                        size_t len) noexcept {
  return validate(reinterpret_cast<const uint32_t *>(buf), len);
}

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_uint32<InputPtr>
#endif
simdutf_warn_unused simdutf_constexpr23 result
validate_with_errors(InputPtr data, size_t len) noexcept {
  size_t pos = 0;
  for (; pos < len; pos++) {
    uint32_t word = data[pos];
    if (word > 0x10FFFF) {
      return result(error_code::TOO_LARGE, pos);
    }
    if (word >= 0xD800 && word <= 0xDFFF) {
      return result(error_code::SURROGATE, pos);
    }
  }
  return result(error_code::SUCCESS, pos);
}

simdutf_warn_unused simdutf_really_inline result
validate_with_errors(const char32_t *buf, size_t len) noexcept {
  return validate_with_errors(reinterpret_cast<const uint32_t *>(buf), len);
}

inline simdutf_constexpr23 size_t utf8_length_from_utf32(const char32_t *p,
                                                         size_t len) {
  // We are not BOM aware.
  size_t counter{0};
  for (size_t i = 0; i < len; i++) {
    // credit: @ttsugriy  for the vectorizable approach
    counter++;                                     // ASCII
    counter += static_cast<size_t>(p[i] > 0x7F);   // two-byte
    counter += static_cast<size_t>(p[i] > 0x7FF);  // three-byte
    counter += static_cast<size_t>(p[i] > 0xFFFF); // four-bytes
  }
  return counter;
}

inline simdutf_warn_unused simdutf_constexpr23 size_t
utf16_length_from_utf32(const char32_t *p, size_t len) {
  // We are not BOM aware.
  size_t counter{0};
  for (size_t i = 0; i < len; i++) {
    counter++;                                     // non-surrogate word
    counter += static_cast<size_t>(p[i] > 0xFFFF); // surrogate pair
  }
  return counter;
}

} // namespace utf32
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf32.h */
/* begin file include/simdutf/scalar/utf32_to_latin1/utf32_to_latin1.h */
#ifndef SIMDUTF_UTF32_TO_LATIN1_H
#define SIMDUTF_UTF32_TO_LATIN1_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf32_to_latin1 {

inline simdutf_constexpr23 size_t convert(const char32_t *data, size_t len,
                                          char *latin1_output) {
  char *start = latin1_output;
  uint32_t utf32_char;
  size_t pos = 0;
  uint32_t too_large = 0;

  while (pos < len) {
    utf32_char = (uint32_t)data[pos];
    too_large |= utf32_char;
    *latin1_output++ = (char)(utf32_char & 0xFF);
    pos++;
  }
  if ((too_large & 0xFFFFFF00) != 0) {
    return 0;
  }
  return latin1_output - start;
}

inline simdutf_constexpr23 result convert_with_errors(const char32_t *data,
                                                      size_t len,
                                                      char *latin1_output) {
  char *start{latin1_output};
  size_t pos = 0;
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      if (pos + 2 <= len) { // if it is safe to read 8 more bytes, check that
                            // they are Latin1
        uint64_t v;
        ::memcpy(&v, data + pos, sizeof(uint64_t));
        if ((v & 0xFFFFFF00FFFFFF00) == 0) {
          *latin1_output++ = char(data[pos]);
          *latin1_output++ = char(data[pos + 1]);
          pos += 2;
          continue;
        }
      }
    }

    uint32_t utf32_char = data[pos];
    if ((utf32_char & 0xFFFFFF00) ==
        0) { // Check if the character can be represented in Latin-1
      *latin1_output++ = (char)(utf32_char & 0xFF);
      pos++;
    } else {
      return result(error_code::TOO_LARGE, pos);
    };
  }
  return result(error_code::SUCCESS, latin1_output - start);
}

} // namespace utf32_to_latin1
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf32_to_latin1/utf32_to_latin1.h */
/* begin file include/simdutf/scalar/utf32_to_latin1/valid_utf32_to_latin1.h */
#ifndef SIMDUTF_VALID_UTF32_TO_LATIN1_H
#define SIMDUTF_VALID_UTF32_TO_LATIN1_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf32_to_latin1 {

template <typename ReadPtr, typename WritePtr>
simdutf_constexpr23 size_t convert_valid(ReadPtr data, size_t len,
                                         WritePtr latin1_output) {
  static_assert(
      std::is_same<typename std::decay<decltype(*data)>::type, uint32_t>::value,
      "dereferencing the data pointer must result in a uint32_t");
  auto start = latin1_output;
  uint32_t utf32_char;
  size_t pos = 0;

  while (pos < len) {
    utf32_char = data[pos];

#if SIMDUTF_CPLUSPLUS23
    // avoid using the 8 byte at a time optimization in constant evaluation
    // mode. memcpy can't be used and replacing it with bitwise or gave worse
    // codegen (when not during constant evaluation).
    if !consteval {
#endif
      if (pos + 2 <= len) {
        // if it is safe to read 8 more bytes, check that they are Latin1
        uint64_t v;
        std::memcpy(&v, data + pos, sizeof(uint64_t));
        if ((v & 0xFFFFFF00FFFFFF00) == 0) {
          *latin1_output++ = char(data[pos]);
          *latin1_output++ = char(data[pos + 1]);
          pos += 2;
          continue;
        } else {
          // output can not be represented in latin1
          return 0;
        }
      }
#if SIMDUTF_CPLUSPLUS23
    } // if ! consteval
#endif
    if ((utf32_char & 0xFFFFFF00) == 0) {
      *latin1_output++ = char(utf32_char);
    } else {
      // output can not be represented in latin1
      return 0;
    }
    pos++;
  }
  return latin1_output - start;
}

simdutf_really_inline size_t convert_valid(const char32_t *buf, size_t len,
                                           char *latin1_output) {
  return convert_valid(reinterpret_cast<const uint32_t *>(buf), len,
                       latin1_output);
}

} // namespace utf32_to_latin1
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf32_to_latin1/valid_utf32_to_latin1.h */
/* begin file include/simdutf/scalar/utf32_to_utf16/utf32_to_utf16.h */
#ifndef SIMDUTF_UTF32_TO_UTF16_H
#define SIMDUTF_UTF32_TO_UTF16_H

namespace simdutf {
namespace scalar {
namespace {
namespace utf32_to_utf16 {

template <endianness big_endian>
simdutf_constexpr23 size_t convert(const char32_t *data, size_t len,
                                   char16_t *utf16_output) {
  size_t pos = 0;
  char16_t *start{utf16_output};
  while (pos < len) {
    uint32_t word = data[pos];
    if ((word & 0xFFFF0000) == 0) {
      if (word >= 0xD800 && word <= 0xDFFF) {
        return 0;
      }
      // will not generate a surrogate pair
      *utf16_output++ = !match_system(big_endian)
                            ? char16_t(u16_swap_bytes(uint16_t(word)))
                            : char16_t(word);
    } else {
      // will generate a surrogate pair
      if (word > 0x10FFFF) {
        return 0;
      }
      word -= 0x10000;
      uint16_t high_surrogate = uint16_t(0xD800 + (word >> 10));
      uint16_t low_surrogate = uint16_t(0xDC00 + (word & 0x3FF));
      if constexpr (!match_system(big_endian)) {
        high_surrogate = u16_swap_bytes(high_surrogate);
        low_surrogate = u16_swap_bytes(low_surrogate);
      }
      *utf16_output++ = char16_t(high_surrogate);
      *utf16_output++ = char16_t(low_surrogate);
    }
    pos++;
  }
  return utf16_output - start;
}

template <endianness big_endian>
simdutf_constexpr23 result convert_with_errors(const char32_t *data, size_t len,
                                               char16_t *utf16_output) {
  size_t pos = 0;
  char16_t *start{utf16_output};
  while (pos < len) {
    uint32_t word = data[pos];
    if ((word & 0xFFFF0000) == 0) {
      if (word >= 0xD800 && word <= 0xDFFF) {
        return result(error_code::SURROGATE, pos);
      }
      // will not generate a surrogate pair
      *utf16_output++ = !match_system(big_endian)
                            ? char16_t(u16_swap_bytes(uint16_t(word)))
                            : char16_t(word);
    } else {
      // will generate a surrogate pair
      if (word > 0x10FFFF) {
        return result(error_code::TOO_LARGE, pos);
      }
      word -= 0x10000;
      uint16_t high_surrogate = uint16_t(0xD800 + (word >> 10));
      uint16_t low_surrogate = uint16_t(0xDC00 + (word & 0x3FF));
      if constexpr (!match_system(big_endian)) {
        high_surrogate = u16_swap_bytes(high_surrogate);
        low_surrogate = u16_swap_bytes(low_surrogate);
      }
      *utf16_output++ = char16_t(high_surrogate);
      *utf16_output++ = char16_t(low_surrogate);
    }
    pos++;
  }
  return result(error_code::SUCCESS, utf16_output - start);
}

} // namespace utf32_to_utf16
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf32_to_utf16/utf32_to_utf16.h */
/* begin file include/simdutf/scalar/utf32_to_utf16/valid_utf32_to_utf16.h */
#ifndef SIMDUTF_VALID_UTF32_TO_UTF16_H
#define SIMDUTF_VALID_UTF32_TO_UTF16_H

namespace simdutf {
namespace scalar {
namespace {
namespace utf32_to_utf16 {

template <endianness big_endian>
simdutf_constexpr23 size_t convert_valid(const char32_t *data, size_t len,
                                         char16_t *utf16_output) {
  size_t pos = 0;
  char16_t *start{utf16_output};
  while (pos < len) {
    uint32_t word = data[pos];
    if ((word & 0xFFFF0000) == 0) {
      // will not generate a surrogate pair
      *utf16_output++ = !match_system(big_endian)
                            ? char16_t(u16_swap_bytes(uint16_t(word)))
                            : char16_t(word);
      pos++;
    } else {
      // will generate a surrogate pair
      word -= 0x10000;
      uint16_t high_surrogate = uint16_t(0xD800 + (word >> 10));
      uint16_t low_surrogate = uint16_t(0xDC00 + (word & 0x3FF));
      if constexpr (!match_system(big_endian)) {
        high_surrogate = u16_swap_bytes(high_surrogate);
        low_surrogate = u16_swap_bytes(low_surrogate);
      }
      *utf16_output++ = char16_t(high_surrogate);
      *utf16_output++ = char16_t(low_surrogate);
      pos++;
    }
  }
  return utf16_output - start;
}

} // namespace utf32_to_utf16
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf32_to_utf16/valid_utf32_to_utf16.h */
/* begin file include/simdutf/scalar/utf32_to_utf8/utf32_to_utf8.h */
#ifndef SIMDUTF_UTF32_TO_UTF8_H
#define SIMDUTF_UTF32_TO_UTF8_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf32_to_utf8 {

template <typename InputPtr, typename OutputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires(simdutf::detail::indexes_into_utf32<InputPtr> &&
           simdutf::detail::index_assignable_from_char<OutputPtr>)
#endif
simdutf_constexpr23 size_t convert(InputPtr data, size_t len,
                                   OutputPtr utf8_output) {
  size_t pos = 0;
  auto start = utf8_output;
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    { // try to convert the next block of 2 ASCII characters
      if (pos + 2 <= len) { // if it is safe to read 8 more bytes, check that
                            // they are ascii
        uint64_t v;
        ::memcpy(&v, data + pos, sizeof(uint64_t));
        if ((v & 0xFFFFFF80FFFFFF80) == 0) {
          *utf8_output++ = char(data[pos]);
          *utf8_output++ = char(data[pos + 1]);
          pos += 2;
          continue;
        }
      }
    }

    uint32_t word = data[pos];
    if ((word & 0xFFFFFF80) == 0) {
      // will generate one UTF-8 bytes
      *utf8_output++ = char(word);
      pos++;
    } else if ((word & 0xFFFFF800) == 0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else if ((word & 0xFFFF0000) == 0) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
      if (word >= 0xD800 && word <= 0xDFFF) {
        return 0;
      }
      *utf8_output++ = char((word >> 12) | 0b11100000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {
      // will generate four UTF-8 bytes
      // we have 0b11110XXX 0b10XXXXXX 0b10XXXXXX 0b10XXXXXX
      if (word > 0x10FFFF) {
        return 0;
      }
      *utf8_output++ = char((word >> 18) | 0b11110000);
      *utf8_output++ = char(((word >> 12) & 0b111111) | 0b10000000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    }
  }
  return utf8_output - start;
}

template <typename InputPtr, typename OutputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires(simdutf::detail::indexes_into_utf32<InputPtr> &&
           simdutf::detail::index_assignable_from_char<OutputPtr>)
#endif
simdutf_constexpr23 result convert_with_errors(InputPtr data, size_t len,
                                               OutputPtr utf8_output) {
  size_t pos = 0;
  auto start = utf8_output;
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    { // try to convert the next block of 2 ASCII characters
      if (pos + 2 <= len) { // if it is safe to read 8 more bytes, check that
                            // they are ascii
        uint64_t v;
        ::memcpy(&v, data + pos, sizeof(uint64_t));
        if ((v & 0xFFFFFF80FFFFFF80) == 0) {
          *utf8_output++ = char(data[pos]);
          *utf8_output++ = char(data[pos + 1]);
          pos += 2;
          continue;
        }
      }
    }

    uint32_t word = data[pos];
    if ((word & 0xFFFFFF80) == 0) {
      // will generate one UTF-8 bytes
      *utf8_output++ = char(word);
      pos++;
    } else if ((word & 0xFFFFF800) == 0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else if ((word & 0xFFFF0000) == 0) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
      if (word >= 0xD800 && word <= 0xDFFF) {
        return result(error_code::SURROGATE, pos);
      }
      *utf8_output++ = char((word >> 12) | 0b11100000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {
      // will generate four UTF-8 bytes
      // we have 0b11110XXX 0b10XXXXXX 0b10XXXXXX 0b10XXXXXX
      if (word > 0x10FFFF) {
        return result(error_code::TOO_LARGE, pos);
      }
      *utf8_output++ = char((word >> 18) | 0b11110000);
      *utf8_output++ = char(((word >> 12) & 0b111111) | 0b10000000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    }
  }
  return result(error_code::SUCCESS, utf8_output - start);
}

} // namespace utf32_to_utf8
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf32_to_utf8/utf32_to_utf8.h */
/* begin file include/simdutf/scalar/utf32_to_utf8/valid_utf32_to_utf8.h */
#ifndef SIMDUTF_VALID_UTF32_TO_UTF8_H
#define SIMDUTF_VALID_UTF32_TO_UTF8_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf32_to_utf8 {

template <typename InputPtr, typename OutputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires(simdutf::detail::indexes_into_utf32<InputPtr> &&
           simdutf::detail::index_assignable_from_char<OutputPtr>)
#endif
simdutf_constexpr23 size_t convert_valid(InputPtr data, size_t len,
                                         OutputPtr utf8_output) {
  size_t pos = 0;
  auto start = utf8_output;
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    { // try to convert the next block of 2 ASCII characters
      if (pos + 2 <= len) { // if it is safe to read 8 more bytes, check that
                            // they are ascii
        uint64_t v;
        ::memcpy(&v, data + pos, sizeof(uint64_t));
        if ((v & 0xFFFFFF80FFFFFF80) == 0) {
          *utf8_output++ = char(data[pos]);
          *utf8_output++ = char(data[pos + 1]);
          pos += 2;
          continue;
        }
      }
    }

    uint32_t word = data[pos];
    if ((word & 0xFFFFFF80) == 0) {
      // will generate one UTF-8 bytes
      *utf8_output++ = char(word);
      pos++;
    } else if ((word & 0xFFFFF800) == 0) {
      // will generate two UTF-8 bytes
      // we have 0b110XXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 6) | 0b11000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else if ((word & 0xFFFF0000) == 0) {
      // will generate three UTF-8 bytes
      // we have 0b1110XXXX 0b10XXXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 12) | 0b11100000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    } else {
      // will generate four UTF-8 bytes
      // we have 0b11110XXX 0b10XXXXXX 0b10XXXXXX 0b10XXXXXX
      *utf8_output++ = char((word >> 18) | 0b11110000);
      *utf8_output++ = char(((word >> 12) & 0b111111) | 0b10000000);
      *utf8_output++ = char(((word >> 6) & 0b111111) | 0b10000000);
      *utf8_output++ = char((word & 0b111111) | 0b10000000);
      pos++;
    }
  }
  return utf8_output - start;
}

} // namespace utf32_to_utf8
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf32_to_utf8/valid_utf32_to_utf8.h */
/* begin file include/simdutf/scalar/utf8.h */
#ifndef SIMDUTF_UTF8_H
#define SIMDUTF_UTF8_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf8 {

// credit: based on code from Google Fuchsia (Apache Licensed)
template <class BytePtr>
simdutf_constexpr23 simdutf_warn_unused bool validate(BytePtr data,
                                                      size_t len) noexcept {
  static_assert(
      std::is_same<typename std::decay<decltype(*data)>::type, uint8_t>::value,
      "dereferencing the data pointer must result in a uint8_t");
  uint64_t pos = 0;
  uint32_t code_point = 0;
  while (pos < len) {
    uint64_t next_pos;
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    { // check if the next 16 bytes are ascii.
      next_pos = pos + 16;
      if (next_pos <= len) { // if it is safe to read 16 more bytes, check
                             // that they are ascii
        uint64_t v1{};
        std::memcpy(&v1, data + pos, sizeof(uint64_t));
        uint64_t v2{};
        std::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
        uint64_t v{v1 | v2};
        if ((v & 0x8080808080808080) == 0) {
          pos = next_pos;
          continue;
        }
      }
    }

    unsigned char byte = data[pos];

    while (byte < 0b10000000) {
      if (++pos == len) {
        return true;
      }
      byte = data[pos];
    }

    if ((byte & 0b11100000) == 0b11000000) {
      next_pos = pos + 2;
      if (next_pos > len) {
        return false;
      }
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return false;
      }
      // range check
      code_point = (byte & 0b00011111) << 6 | (data[pos + 1] & 0b00111111);
      if (code_point < 0x80) {
        return false;
      }
    } else if ((byte & 0b11110000) == 0b11100000) {
      next_pos = pos + 3;
      if (next_pos > len) {
        return false;
      }
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return false;
      }
      if ((data[pos + 2] & 0b11000000) != 0b10000000) {
        return false;
      }
      // range check
      code_point = (byte & 0b00001111) << 12 |
                   (data[pos + 1] & 0b00111111) << 6 |
                   (data[pos + 2] & 0b00111111);
      if ((code_point < 0x800) ||
          (0xd7ff < code_point && code_point < 0xe000)) {
        return false;
      }
    } else if ((byte & 0b11111000) == 0b11110000) { // 0b11110000
      next_pos = pos + 4;
      if (next_pos > len) {
        return false;
      }
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return false;
      }
      if ((data[pos + 2] & 0b11000000) != 0b10000000) {
        return false;
      }
      if ((data[pos + 3] & 0b11000000) != 0b10000000) {
        return false;
      }
      // range check
      code_point =
          (byte & 0b00000111) << 18 | (data[pos + 1] & 0b00111111) << 12 |
          (data[pos + 2] & 0b00111111) << 6 | (data[pos + 3] & 0b00111111);
      if (code_point <= 0xffff || 0x10ffff < code_point) {
        return false;
      }
    } else {
      // we may have a continuation
      return false;
    }
    pos = next_pos;
  }
  return true;
}

simdutf_really_inline simdutf_warn_unused bool validate(const char *buf,
                                                        size_t len) noexcept {
  return validate(reinterpret_cast<const uint8_t *>(buf), len);
}

template <class BytePtr>
simdutf_constexpr23 simdutf_warn_unused result
validate_with_errors(BytePtr data, size_t len) noexcept {
  static_assert(
      std::is_same<typename std::decay<decltype(*data)>::type, uint8_t>::value,
      "dereferencing the data pointer must result in a uint8_t");
  size_t pos = 0;
  uint32_t code_point = 0;
  while (pos < len) {
    // check of the next 16 bytes are ascii.
    size_t next_pos = pos + 16;
    if (next_pos <=
        len) { // if it is safe to read 16 more bytes, check that they are ascii
      uint64_t v1;
      std::memcpy(&v1, data + pos, sizeof(uint64_t));
      uint64_t v2;
      std::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
      uint64_t v{v1 | v2};
      if ((v & 0x8080808080808080) == 0) {
        pos = next_pos;
        continue;
      }
    }
    unsigned char byte = data[pos];

    while (byte < 0b10000000) {
      if (++pos == len) {
        return result(error_code::SUCCESS, len);
      }
      byte = data[pos];
    }

    if ((byte & 0b11100000) == 0b11000000) {
      next_pos = pos + 2;
      if (next_pos > len) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      // range check
      code_point = (byte & 0b00011111) << 6 | (data[pos + 1] & 0b00111111);
      if (code_point < 0x80) {
        return result(error_code::OVERLONG, pos);
      }
    } else if ((byte & 0b11110000) == 0b11100000) {
      next_pos = pos + 3;
      if (next_pos > len) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((data[pos + 2] & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      // range check
      code_point = (byte & 0b00001111) << 12 |
                   (data[pos + 1] & 0b00111111) << 6 |
                   (data[pos + 2] & 0b00111111);
      if (code_point < 0x800) {
        return result(error_code::OVERLONG, pos);
      }
      if (0xd7ff < code_point && code_point < 0xe000) {
        return result(error_code::SURROGATE, pos);
      }
    } else if ((byte & 0b11111000) == 0b11110000) { // 0b11110000
      next_pos = pos + 4;
      if (next_pos > len) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((data[pos + 2] & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((data[pos + 3] & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      // range check
      code_point =
          (byte & 0b00000111) << 18 | (data[pos + 1] & 0b00111111) << 12 |
          (data[pos + 2] & 0b00111111) << 6 | (data[pos + 3] & 0b00111111);
      if (code_point <= 0xffff) {
        return result(error_code::OVERLONG, pos);
      }
      if (0x10ffff < code_point) {
        return result(error_code::TOO_LARGE, pos);
      }
    } else {
      // we either have too many continuation bytes or an invalid leading byte
      if ((byte & 0b11000000) == 0b10000000) {
        return result(error_code::TOO_LONG, pos);
      } else {
        return result(error_code::HEADER_BITS, pos);
      }
    }
    pos = next_pos;
  }
  return result(error_code::SUCCESS, len);
}

simdutf_really_inline simdutf_warn_unused result
validate_with_errors(const char *buf, size_t len) noexcept {
  return validate_with_errors(reinterpret_cast<const uint8_t *>(buf), len);
}

// Finds the previous leading byte starting backward from buf and validates with
// errors from there Used to pinpoint the location of an error when an invalid
// chunk is detected We assume that the stream starts with a leading byte, and
// to check that it is the case, we ask that you pass a pointer to the start of
// the stream (start). Note that the resulting count is underflowed if an error
// is encountered in the rewinded segment.
inline simdutf_warn_unused result rewind_and_validate_with_errors(
    const char *start, const char *buf, size_t len) noexcept {
  // First check that we start with a leading byte
  if ((*start & 0b11000000) == 0b10000000) {
    return result(error_code::TOO_LONG, 0);
  }
  size_t extra_len{0};
  // A leading byte cannot be further than 4 bytes away
  for (int i = 0; i < 5; i++) {
    unsigned char byte = *buf;
    if ((byte & 0b11000000) != 0b10000000) {
      break;
    } else {
      buf--;
      extra_len++;
    }
  }

  result res = validate_with_errors(buf, len + extra_len);
  res.count -= extra_len; // Might underflow
  return res;
}

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 size_t count_code_points(InputPtr data, size_t len) {
  size_t counter{0};
  for (size_t i = 0; i < len; i++) {
    // -65 is 0b10111111, anything larger in two-complement's should start a new
    // code point.
    if (int8_t(data[i]) > -65) {
      counter++;
    }
  }
  return counter;
}

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 size_t utf16_length_from_utf8(InputPtr data, size_t len) {
  size_t counter{0};
  for (size_t i = 0; i < len; i++) {
    if (int8_t(data[i]) > -65) {
      counter++;
    }
    if (uint8_t(data[i]) >= 240) {
      counter++;
    }
  }
  return counter;
}

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_warn_unused simdutf_constexpr23 size_t
trim_partial_utf8(InputPtr input, size_t length) {
  if (length < 3) {
    switch (length) {
    case 2:
      if (uint8_t(input[length - 1]) >= 0xc0) {
        return length - 1;
      } // 2-, 3- and 4-byte characters with only 1 byte left
      if (uint8_t(input[length - 2]) >= 0xe0) {
        return length - 2;
      } // 3- and 4-byte characters with only 2 bytes left
      return length;
    case 1:
      if (uint8_t(input[length - 1]) >= 0xc0) {
        return length - 1;
      } // 2-, 3- and 4-byte characters with only 1 byte left
      return length;
    case 0:
      return length;
    }
  }
  if (uint8_t(input[length - 1]) >= 0xc0) {
    return length - 1;
  } // 2-, 3- and 4-byte characters with only 1 byte left
  if (uint8_t(input[length - 2]) >= 0xe0) {
    return length - 2;
  } // 3- and 4-byte characters with only 1 byte left
  if (uint8_t(input[length - 3]) >= 0xf0) {
    return length - 3;
  } // 4-byte characters with only 3 bytes left
  return length;
}

} // namespace utf8
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf8.h */
/* begin file include/simdutf/scalar/utf8_to_latin1/utf8_to_latin1.h */
#ifndef SIMDUTF_UTF8_TO_LATIN1_H
#define SIMDUTF_UTF8_TO_LATIN1_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf8_to_latin1 {

template <typename InputPtr, typename OutputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires(simdutf::detail::indexes_into_byte_like<InputPtr> &&
           simdutf::detail::indexes_into_byte_like<OutputPtr>)
#endif
simdutf_constexpr23 size_t convert(InputPtr data, size_t len,
                                   OutputPtr latin_output) {
  size_t pos = 0;
  auto start = latin_output;

  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 16 ASCII bytes
      if (pos + 16 <= len) { // if it is safe to read 16 more bytes, check that
                             // they are ascii
        uint64_t v1;
        ::memcpy(&v1, data + pos, sizeof(uint64_t));
        uint64_t v2;
        ::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
        uint64_t v{v1 | v2}; // We are only interested in these bits: 1000 1000
                             // 1000 1000 .... etc
        if ((v & 0x8080808080808080) ==
            0) { // if NONE of these are set, e.g. all of them are zero, then
                 // everything is ASCII
          size_t final_pos = pos + 16;
          while (pos < final_pos) {
            *latin_output++ = char(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }

    // suppose it is not an all ASCII byte sequence
    uint8_t leading_byte = data[pos]; // leading byte
    if (leading_byte < 0b10000000) {
      // converting one ASCII byte !!!
      *latin_output++ = char(leading_byte);
      pos++;
    } else if ((leading_byte & 0b11100000) ==
               0b11000000) { // the first three bits indicate:
      // We have a two-byte UTF-8
      if (pos + 1 >= len) {
        return 0;
      } // minimal bound checking
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return 0;
      } // checks if the next byte is a valid continuation byte in UTF-8. A
        // valid continuation byte starts with 10.
      // range check -
      uint32_t code_point =
          (leading_byte & 0b00011111) << 6 |
          (data[pos + 1] &
           0b00111111); // assembles the Unicode code point from the two bytes.
                        // It does this by discarding the leading 110 and 10
                        // bits from the two bytes, shifting the remaining bits
                        // of the first byte, and then combining the results
                        // with a bitwise OR operation.
      if (code_point < 0x80 || 0xFF < code_point) {
        return 0; // We only care about the range 129-255 which is Non-ASCII
                  // latin1 characters. A code_point beneath 0x80 is invalid as
                  // it is already covered by bytes whose leading bit is zero.
      }
      *latin_output++ = char(code_point);
      pos += 2;
    } else {
      return 0;
    }
  }
  return latin_output - start;
}

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 result convert_with_errors(InputPtr data, size_t len,
                                               char *latin_output) {
  size_t pos = 0;
  char *start{latin_output};

  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 16 ASCII bytes
      if (pos + 16 <= len) { // if it is safe to read 16 more bytes, check that
                             // they are ascii
        uint64_t v1;
        ::memcpy(&v1, data + pos, sizeof(uint64_t));
        uint64_t v2;
        ::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
        uint64_t v{v1 | v2}; // We are only interested in these bits: 1000 1000
                             // 1000 1000...etc
        if ((v & 0x8080808080808080) ==
            0) { // if NONE of these are set, e.g. all of them are zero, then
                 // everything is ASCII
          size_t final_pos = pos + 16;
          while (pos < final_pos) {
            *latin_output++ = char(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }
    // suppose it is not an all ASCII byte sequence
    uint8_t leading_byte = data[pos]; // leading byte
    if (leading_byte < 0b10000000) {
      // converting one ASCII byte !!!
      *latin_output++ = char(leading_byte);
      pos++;
    } else if ((leading_byte & 0b11100000) ==
               0b11000000) { // the first three bits indicate:
      // We have a two-byte UTF-8
      if (pos + 1 >= len) {
        return result(error_code::TOO_SHORT, pos);
      } // minimal bound checking
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      } // checks if the next byte is a valid continuation byte in UTF-8. A
        // valid continuation byte starts with 10.
      // range check -
      uint32_t code_point =
          (leading_byte & 0b00011111) << 6 |
          (data[pos + 1] &
           0b00111111); // assembles the Unicode code point from the two bytes.
                        // It does this by discarding the leading 110 and 10
                        // bits from the two bytes, shifting the remaining bits
                        // of the first byte, and then combining the results
                        // with a bitwise OR operation.
      if (code_point < 0x80) {
        return result(error_code::OVERLONG, pos);
      }
      if (0xFF < code_point) {
        return result(error_code::TOO_LARGE, pos);
      } // We only care about the range 129-255 which is Non-ASCII latin1
        // characters
      *latin_output++ = char(code_point);
      pos += 2;
    } else if ((leading_byte & 0b11110000) == 0b11100000) {
      // We have a three-byte UTF-8
      return result(error_code::TOO_LARGE, pos);
    } else if ((leading_byte & 0b11111000) == 0b11110000) { // 0b11110000
      // we have a 4-byte UTF-8 word.
      return result(error_code::TOO_LARGE, pos);
    } else {
      // we either have too many continuation bytes or an invalid leading byte
      if ((leading_byte & 0b11000000) == 0b10000000) {
        return result(error_code::TOO_LONG, pos);
      }

      return result(error_code::HEADER_BITS, pos);
    }
  }
  return result(error_code::SUCCESS, latin_output - start);
}

inline result rewind_and_convert_with_errors(size_t prior_bytes,
                                             const char *buf, size_t len,
                                             char *latin1_output) {
  size_t extra_len{0};
  // We potentially need to go back in time and find a leading byte.
  // In theory '3' would be sufficient, but sometimes the error can go back
  // quite far.
  size_t how_far_back = prior_bytes;
  // size_t how_far_back = 3; // 3 bytes in the past + current position
  // if(how_far_back >= prior_bytes) { how_far_back = prior_bytes; }
  bool found_leading_bytes{false};
  // important: it is i <= how_far_back and not 'i < how_far_back'.
  for (size_t i = 0; i <= how_far_back; i++) {
    unsigned char byte = buf[-static_cast<std::ptrdiff_t>(i)];
    found_leading_bytes = ((byte & 0b11000000) != 0b10000000);
    if (found_leading_bytes) {
      if (i > 0 && byte < 128) {
        // If we had to go back and the leading byte is ascii
        // then we can stop right away.
        return result(error_code::TOO_LONG, 0 - i + 1);
      }
      buf -= i;
      extra_len = i;
      break;
    }
  }
  //
  // It is possible for this function to return a negative count in its result.
  // C++ Standard Section 18.1 defines size_t is in <cstddef> which is described
  // in C Standard as <stddef.h>. C Standard Section 4.1.5 defines size_t as an
  // unsigned integral type of the result of the sizeof operator
  //
  // An unsigned type will simply wrap round arithmetically (well defined).
  //
  if (!found_leading_bytes) {
    // If how_far_back == 3, we may have four consecutive continuation bytes!!!
    // [....] [continuation] [continuation] [continuation] | [buf is
    // continuation] Or we possibly have a stream that does not start with a
    // leading byte.
    return result(error_code::TOO_LONG, 0 - how_far_back);
  }
  result res = convert_with_errors(buf, len + extra_len, latin1_output);
  if (res.error) {
    res.count -= extra_len;
  }
  return res;
}

} // namespace utf8_to_latin1
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf8_to_latin1/utf8_to_latin1.h */
/* begin file include/simdutf/scalar/utf8_to_latin1/valid_utf8_to_latin1.h */
#ifndef SIMDUTF_VALID_UTF8_TO_LATIN1_H
#define SIMDUTF_VALID_UTF8_TO_LATIN1_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf8_to_latin1 {

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 size_t convert_valid(InputPtr data, size_t len,
                                         char *latin_output) {

  size_t pos = 0;
  char *start{latin_output};

  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 16 ASCII bytes
      if (pos + 16 <= len) { // if it is safe to read 16 more bytes, check that
                             // they are ascii
        uint64_t v1;
        ::memcpy(&v1, data + pos, sizeof(uint64_t));
        uint64_t v2;
        ::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
        uint64_t v{v1 |
                   v2}; // We are only interested in these bits: 1000 1000 1000
                        // 1000, so it makes sense to concatenate everything
        if ((v & 0x8080808080808080) ==
            0) { // if NONE of these are set, e.g. all of them are zero, then
                 // everything is ASCII
          size_t final_pos = pos + 16;
          while (pos < final_pos) {
            *latin_output++ = uint8_t(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }

    // suppose it is not an all ASCII byte sequence
    auto leading_byte = uint8_t(data[pos]); // leading byte
    if (leading_byte < 0b10000000) {
      // converting one ASCII byte !!!
      *latin_output++ = char(leading_byte);
      pos++;
    } else if ((leading_byte & 0b11100000) ==
               0b11000000) { // the first three bits indicate:
      // We have a two-byte UTF-8
      if (pos + 1 >= len) {
        break;
      } // minimal bound checking
      if ((uint8_t(data[pos + 1]) & 0b11000000) != 0b10000000) {
        return 0;
      } // checks if the next byte is a valid continuation byte in UTF-8. A
        // valid continuation byte starts with 10.
      // range check -
      uint32_t code_point =
          (leading_byte & 0b00011111) << 6 |
          (uint8_t(data[pos + 1]) &
           0b00111111); // assembles the Unicode code point from the two bytes.
                        // It does this by discarding the leading 110 and 10
                        // bits from the two bytes, shifting the remaining bits
                        // of the first byte, and then combining the results
                        // with a bitwise OR operation.
      *latin_output++ = char(code_point);
      pos += 2;
    } else {
      // we may have a continuation but we do not do error checking
      return 0;
    }
  }
  return latin_output - start;
}

} // namespace utf8_to_latin1
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf8_to_latin1/valid_utf8_to_latin1.h */
/* begin file include/simdutf/scalar/utf8_to_utf16/utf8_to_utf16.h */
#ifndef SIMDUTF_UTF8_TO_UTF16_H
#define SIMDUTF_UTF8_TO_UTF16_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf8_to_utf16 {

template <endianness big_endian, typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 size_t convert(InputPtr data, size_t len,
                                   char16_t *utf16_output) {
  size_t pos = 0;
  char16_t *start{utf16_output};
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    // try to convert the next block of 16 ASCII bytes
    {
      if (pos + 16 <= len) { // if it is safe to read 16 more bytes, check that
                             // they are ascii
        uint64_t v1;
        ::memcpy(&v1, data + pos, sizeof(uint64_t));
        uint64_t v2;
        ::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
        uint64_t v{v1 | v2};
        if ((v & 0x8080808080808080) == 0) {
          size_t final_pos = pos + 16;
          while (pos < final_pos) {
            *utf16_output++ = !match_system(big_endian)
                                  ? char16_t(u16_swap_bytes(data[pos]))
                                  : char16_t(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }

    uint8_t leading_byte = data[pos]; // leading byte
    if (leading_byte < 0b10000000) {
      // converting one ASCII byte !!!
      *utf16_output++ = !match_system(big_endian)
                            ? char16_t(u16_swap_bytes(leading_byte))
                            : char16_t(leading_byte);
      pos++;
    } else if ((leading_byte & 0b11100000) == 0b11000000) {
      // We have a two-byte UTF-8, it should become
      // a single UTF-16 word.
      if (pos + 1 >= len) {
        return 0;
      } // minimal bound checking
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return 0;
      }
      // range check
      uint32_t code_point =
          (leading_byte & 0b00011111) << 6 | (data[pos + 1] & 0b00111111);
      if (code_point < 0x80) {
        return 0;
      }
      if constexpr (!match_system(big_endian)) {
        code_point = uint32_t(u16_swap_bytes(uint16_t(code_point)));
      }
      *utf16_output++ = char16_t(code_point);
      pos += 2;
    } else if ((leading_byte & 0b11110000) == 0b11100000) {
      // We have a three-byte UTF-8, it should become
      // a single UTF-16 word.
      if (pos + 2 >= len) {
        return 0;
      } // minimal bound checking

      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return 0;
      }
      if ((data[pos + 2] & 0b11000000) != 0b10000000) {
        return 0;
      }
      // range check
      uint32_t code_point = (leading_byte & 0b00001111) << 12 |
                            (data[pos + 1] & 0b00111111) << 6 |
                            (data[pos + 2] & 0b00111111);
      if (code_point < 0x800 || (0xd7ff < code_point && code_point < 0xe000)) {
        return 0;
      }
      if constexpr (!match_system(big_endian)) {
        code_point = uint32_t(u16_swap_bytes(uint16_t(code_point)));
      }
      *utf16_output++ = char16_t(code_point);
      pos += 3;
    } else if ((leading_byte & 0b11111000) == 0b11110000) { // 0b11110000
      // we have a 4-byte UTF-8 word.
      if (pos + 3 >= len) {
        return 0;
      } // minimal bound checking
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return 0;
      }
      if ((data[pos + 2] & 0b11000000) != 0b10000000) {
        return 0;
      }
      if ((data[pos + 3] & 0b11000000) != 0b10000000) {
        return 0;
      }

      // range check
      uint32_t code_point = (leading_byte & 0b00000111) << 18 |
                            (data[pos + 1] & 0b00111111) << 12 |
                            (data[pos + 2] & 0b00111111) << 6 |
                            (data[pos + 3] & 0b00111111);
      if (code_point <= 0xffff || 0x10ffff < code_point) {
        return 0;
      }
      code_point -= 0x10000;
      uint16_t high_surrogate = uint16_t(0xD800 + (code_point >> 10));
      uint16_t low_surrogate = uint16_t(0xDC00 + (code_point & 0x3FF));
      if constexpr (!match_system(big_endian)) {
        high_surrogate = u16_swap_bytes(high_surrogate);
        low_surrogate = u16_swap_bytes(low_surrogate);
      }
      *utf16_output++ = char16_t(high_surrogate);
      *utf16_output++ = char16_t(low_surrogate);
      pos += 4;
    } else {
      return 0;
    }
  }
  return utf16_output - start;
}

template <endianness big_endian, typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 result convert_with_errors(InputPtr data, size_t len,
                                               char16_t *utf16_output) {
  size_t pos = 0;
  char16_t *start{utf16_output};
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 16 ASCII bytes
      if (pos + 16 <= len) { // if it is safe to read 16 more bytes, check that
                             // they are ascii
        uint64_t v1;
        ::memcpy(&v1, data + pos, sizeof(uint64_t));
        uint64_t v2;
        ::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
        uint64_t v{v1 | v2};
        if ((v & 0x8080808080808080) == 0) {
          size_t final_pos = pos + 16;
          while (pos < final_pos) {
            const char16_t byte = uint8_t(data[pos]);
            *utf16_output++ =
                !match_system(big_endian) ? u16_swap_bytes(byte) : byte;
            pos++;
          }
          continue;
        }
      }
    }

    auto leading_byte = uint8_t(data[pos]); // leading byte
    if (leading_byte < 0b10000000) {
      // converting one ASCII byte !!!
      *utf16_output++ = !match_system(big_endian)
                            ? char16_t(u16_swap_bytes(leading_byte))
                            : char16_t(leading_byte);
      pos++;
    } else if ((leading_byte & 0b11100000) == 0b11000000) {
      // We have a two-byte UTF-8, it should become
      // a single UTF-16 word.
      if (pos + 1 >= len) {
        return result(error_code::TOO_SHORT, pos);
      } // minimal bound checking
      if ((uint8_t(data[pos + 1]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      // range check
      uint32_t code_point = (leading_byte & 0b00011111) << 6 |
                            (uint8_t(data[pos + 1]) & 0b00111111);
      if (code_point < 0x80) {
        return result(error_code::OVERLONG, pos);
      }
      if constexpr (!match_system(big_endian)) {
        code_point = uint32_t(u16_swap_bytes(uint16_t(code_point)));
      }
      *utf16_output++ = char16_t(code_point);
      pos += 2;
    } else if ((leading_byte & 0b11110000) == 0b11100000) {
      // We have a three-byte UTF-8, it should become
      // a single UTF-16 word.
      if (pos + 2 >= len) {
        return result(error_code::TOO_SHORT, pos);
      } // minimal bound checking

      if ((uint8_t(data[pos + 1]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((uint8_t(data[pos + 2]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      // range check
      uint32_t code_point = (leading_byte & 0b00001111) << 12 |
                            (uint8_t(data[pos + 1]) & 0b00111111) << 6 |
                            (uint8_t(data[pos + 2]) & 0b00111111);
      if (code_point < 0x800) {
        return result(error_code::OVERLONG, pos);
      }
      if (0xd7ff < code_point && code_point < 0xe000) {
        return result(error_code::SURROGATE, pos);
      }
      if constexpr (!match_system(big_endian)) {
        code_point = uint32_t(u16_swap_bytes(uint16_t(code_point)));
      }
      *utf16_output++ = char16_t(code_point);
      pos += 3;
    } else if ((leading_byte & 0b11111000) == 0b11110000) { // 0b11110000
      // we have a 4-byte UTF-8 word.
      if (pos + 3 >= len) {
        return result(error_code::TOO_SHORT, pos);
      } // minimal bound checking
      if ((uint8_t(data[pos + 1]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((uint8_t(data[pos + 2]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((uint8_t(data[pos + 3]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }

      // range check
      uint32_t code_point = (leading_byte & 0b00000111) << 18 |
                            (uint8_t(data[pos + 1]) & 0b00111111) << 12 |
                            (uint8_t(data[pos + 2]) & 0b00111111) << 6 |
                            (uint8_t(data[pos + 3]) & 0b00111111);
      if (code_point <= 0xffff) {
        return result(error_code::OVERLONG, pos);
      }
      if (0x10ffff < code_point) {
        return result(error_code::TOO_LARGE, pos);
      }
      code_point -= 0x10000;
      uint16_t high_surrogate = uint16_t(0xD800 + (code_point >> 10));
      uint16_t low_surrogate = uint16_t(0xDC00 + (code_point & 0x3FF));
      if constexpr (!match_system(big_endian)) {
        high_surrogate = u16_swap_bytes(high_surrogate);
        low_surrogate = u16_swap_bytes(low_surrogate);
      }
      *utf16_output++ = char16_t(high_surrogate);
      *utf16_output++ = char16_t(low_surrogate);
      pos += 4;
    } else {
      // we either have too many continuation bytes or an invalid leading byte
      if ((leading_byte & 0b11000000) == 0b10000000) {
        return result(error_code::TOO_LONG, pos);
      } else {
        return result(error_code::HEADER_BITS, pos);
      }
    }
  }
  return result(error_code::SUCCESS, utf16_output - start);
}

/**
 * When rewind_and_convert_with_errors is called, we are pointing at 'buf' and
 * we have up to len input bytes left, and we encountered some error. It is
 * possible that the error is at 'buf' exactly, but it could also be in the
 * previous bytes  (up to 3 bytes back).
 *
 * prior_bytes indicates how many bytes, prior to 'buf' may belong to the
 * current memory section and can be safely accessed. We prior_bytes to access
 * safely up to three bytes before 'buf'.
 *
 * The caller is responsible to ensure that len > 0.
 *
 * If the error is believed to have occurred prior to 'buf', the count value
 * contain in the result will be SIZE_T - 1, SIZE_T - 2, or SIZE_T - 3.
 */
template <endianness endian>
inline result rewind_and_convert_with_errors(size_t prior_bytes,
                                             const char *buf, size_t len,
                                             char16_t *utf16_output) {
  size_t extra_len{0};
  // We potentially need to go back in time and find a leading byte.
  // In theory '3' would be sufficient, but sometimes the error can go back
  // quite far.
  size_t how_far_back = prior_bytes;
  // size_t how_far_back = 3; // 3 bytes in the past + current position
  // if(how_far_back >= prior_bytes) { how_far_back = prior_bytes; }
  bool found_leading_bytes{false};
  // important: it is i <= how_far_back and not 'i < how_far_back'.
  for (size_t i = 0; i <= how_far_back; i++) {
    unsigned char byte = buf[-static_cast<std::ptrdiff_t>(i)];
    found_leading_bytes = ((byte & 0b11000000) != 0b10000000);
    if (found_leading_bytes) {
      if (i > 0 && byte < 128) {
        // If we had to go back and the leading byte is ascii
        // then we can stop right away.
        return result(error_code::TOO_LONG, 0 - i + 1);
      }
      buf -= i;
      extra_len = i;
      break;
    }
  }
  //
  // It is possible for this function to return a negative count in its result.
  // C++ Standard Section 18.1 defines size_t is in <cstddef> which is described
  // in C Standard as <stddef.h>. C Standard Section 4.1.5 defines size_t as an
  // unsigned integral type of the result of the sizeof operator
  //
  // An unsigned type will simply wrap round arithmetically (well defined).
  //
  if (!found_leading_bytes) {
    // If how_far_back == 3, we may have four consecutive continuation bytes!!!
    // [....] [continuation] [continuation] [continuation] | [buf is
    // continuation] Or we possibly have a stream that does not start with a
    // leading byte.
    return result(error_code::TOO_LONG, 0 - how_far_back);
  }
  result res = convert_with_errors<endian>(buf, len + extra_len, utf16_output);
  if (res.error) {
    res.count -= extra_len;
  }
  return res;
}

} // namespace utf8_to_utf16
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf8_to_utf16/utf8_to_utf16.h */
/* begin file include/simdutf/scalar/utf8_to_utf16/valid_utf8_to_utf16.h */
#ifndef SIMDUTF_VALID_UTF8_TO_UTF16_H
#define SIMDUTF_VALID_UTF8_TO_UTF16_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf8_to_utf16 {

template <endianness big_endian, typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 size_t convert_valid(InputPtr data, size_t len,
                                         char16_t *utf16_output) {
  size_t pos = 0;
  char16_t *start{utf16_output};
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {                       // try to convert the next block of 8 ASCII bytes
      if (pos + 8 <= len) { // if it is safe to read 8 more bytes, check that
                            // they are ascii
        uint64_t v;
        ::memcpy(&v, data + pos, sizeof(uint64_t));
        if ((v & 0x8080808080808080) == 0) {
          size_t final_pos = pos + 8;
          while (pos < final_pos) {
            const char16_t byte = uint8_t(data[pos]);
            *utf16_output++ =
                !match_system(big_endian) ? u16_swap_bytes(byte) : byte;
            pos++;
          }
          continue;
        }
      }
    }

    auto leading_byte = uint8_t(data[pos]); // leading byte
    if (leading_byte < 0b10000000) {
      // converting one ASCII byte !!!
      *utf16_output++ = !match_system(big_endian)
                            ? char16_t(u16_swap_bytes(leading_byte))
                            : char16_t(leading_byte);
      pos++;
    } else if ((leading_byte & 0b11100000) == 0b11000000) {
      // We have a two-byte UTF-8, it should become
      // a single UTF-16 word.
      if (pos + 1 >= len) {
        break;
      } // minimal bound checking
      uint16_t code_point = uint16_t(((leading_byte & 0b00011111) << 6) |
                                     (uint8_t(data[pos + 1]) & 0b00111111));
      if constexpr (!match_system(big_endian)) {
        code_point = u16_swap_bytes(uint16_t(code_point));
      }
      *utf16_output++ = char16_t(code_point);
      pos += 2;
    } else if ((leading_byte & 0b11110000) == 0b11100000) {
      // We have a three-byte UTF-8, it should become
      // a single UTF-16 word.
      if (pos + 2 >= len) {
        break;
      } // minimal bound checking
      uint16_t code_point =
          uint16_t(((leading_byte & 0b00001111) << 12) |
                   ((uint8_t(data[pos + 1]) & 0b00111111) << 6) |
                   (uint8_t(data[pos + 2]) & 0b00111111));
      if constexpr (!match_system(big_endian)) {
        code_point = u16_swap_bytes(uint16_t(code_point));
      }
      *utf16_output++ = char16_t(code_point);
      pos += 3;
    } else if ((leading_byte & 0b11111000) == 0b11110000) { // 0b11110000
      // we have a 4-byte UTF-8 word.
      if (pos + 3 >= len) {
        break;
      } // minimal bound checking
      uint32_t code_point = ((leading_byte & 0b00000111) << 18) |
                            ((uint8_t(data[pos + 1]) & 0b00111111) << 12) |
                            ((uint8_t(data[pos + 2]) & 0b00111111) << 6) |
                            (uint8_t(data[pos + 3]) & 0b00111111);
      code_point -= 0x10000;
      uint16_t high_surrogate = uint16_t(0xD800 + (code_point >> 10));
      uint16_t low_surrogate = uint16_t(0xDC00 + (code_point & 0x3FF));
      if constexpr (!match_system(big_endian)) {
        high_surrogate = u16_swap_bytes(high_surrogate);
        low_surrogate = u16_swap_bytes(low_surrogate);
      }
      *utf16_output++ = char16_t(high_surrogate);
      *utf16_output++ = char16_t(low_surrogate);
      pos += 4;
    } else {
      // we may have a continuation but we do not do error checking
      return 0;
    }
  }
  return utf16_output - start;
}

} // namespace utf8_to_utf16
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf8_to_utf16/valid_utf8_to_utf16.h */
/* begin file include/simdutf/scalar/utf8_to_utf32/utf8_to_utf32.h */
#ifndef SIMDUTF_UTF8_TO_UTF32_H
#define SIMDUTF_UTF8_TO_UTF32_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf8_to_utf32 {

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 size_t convert(InputPtr data, size_t len,
                                   char32_t *utf32_output) {
  size_t pos = 0;
  char32_t *start{utf32_output};
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 16 ASCII bytes
      if (pos + 16 <= len) { // if it is safe to read 16 more bytes, check that
                             // they are ascii
        uint64_t v1;
        ::memcpy(&v1, data + pos, sizeof(uint64_t));
        uint64_t v2;
        ::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
        uint64_t v{v1 | v2};
        if ((v & 0x8080808080808080) == 0) {
          size_t final_pos = pos + 16;
          while (pos < final_pos) {
            *utf32_output++ = uint8_t(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }
    auto leading_byte = uint8_t(data[pos]); // leading byte
    if (leading_byte < 0b10000000) {
      // converting one ASCII byte !!!
      *utf32_output++ = char32_t(leading_byte);
      pos++;
    } else if ((leading_byte & 0b11100000) == 0b11000000) {
      // We have a two-byte UTF-8
      if (pos + 1 >= len) {
        return 0;
      } // minimal bound checking
      if ((data[pos + 1] & 0b11000000) != 0b10000000) {
        return 0;
      }
      // range check
      uint32_t code_point = (leading_byte & 0b00011111) << 6 |
                            (uint8_t(data[pos + 1]) & 0b00111111);
      if (code_point < 0x80) {
        return 0;
      }
      *utf32_output++ = char32_t(code_point);
      pos += 2;
    } else if ((leading_byte & 0b11110000) == 0b11100000) {
      // We have a three-byte UTF-8
      if (pos + 2 >= len) {
        return 0;
      } // minimal bound checking

      if ((uint8_t(data[pos + 1]) & 0b11000000) != 0b10000000) {
        return 0;
      }
      if ((uint8_t(data[pos + 2]) & 0b11000000) != 0b10000000) {
        return 0;
      }
      // range check
      uint32_t code_point = (leading_byte & 0b00001111) << 12 |
                            (uint8_t(data[pos + 1]) & 0b00111111) << 6 |
                            (uint8_t(data[pos + 2]) & 0b00111111);
      if (code_point < 0x800 || (0xd7ff < code_point && code_point < 0xe000)) {
        return 0;
      }
      *utf32_output++ = char32_t(code_point);
      pos += 3;
    } else if ((leading_byte & 0b11111000) == 0b11110000) { // 0b11110000
      // we have a 4-byte UTF-8 word.
      if (pos + 3 >= len) {
        return 0;
      } // minimal bound checking
      if ((uint8_t(data[pos + 1]) & 0b11000000) != 0b10000000) {
        return 0;
      }
      if ((uint8_t(data[pos + 2]) & 0b11000000) != 0b10000000) {
        return 0;
      }
      if ((uint8_t(data[pos + 3]) & 0b11000000) != 0b10000000) {
        return 0;
      }

      // range check
      uint32_t code_point = (leading_byte & 0b00000111) << 18 |
                            (uint8_t(data[pos + 1]) & 0b00111111) << 12 |
                            (uint8_t(data[pos + 2]) & 0b00111111) << 6 |
                            (uint8_t(data[pos + 3]) & 0b00111111);
      if (code_point <= 0xffff || 0x10ffff < code_point) {
        return 0;
      }
      *utf32_output++ = char32_t(code_point);
      pos += 4;
    } else {
      return 0;
    }
  }
  return utf32_output - start;
}

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 result convert_with_errors(InputPtr data, size_t len,
                                               char32_t *utf32_output) {
  size_t pos = 0;
  char32_t *start{utf32_output};
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 16 ASCII bytes
      if (pos + 16 <= len) { // if it is safe to read 16 more bytes, check that
                             // they are ascii
        uint64_t v1;
        ::memcpy(&v1, data + pos, sizeof(uint64_t));
        uint64_t v2;
        ::memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
        uint64_t v{v1 | v2};
        if ((v & 0x8080808080808080) == 0) {
          size_t final_pos = pos + 16;
          while (pos < final_pos) {
            *utf32_output++ = uint8_t(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }
    auto leading_byte = uint8_t(data[pos]); // leading byte
    if (leading_byte < 0b10000000) {
      // converting one ASCII byte !!!
      *utf32_output++ = char32_t(leading_byte);
      pos++;
    } else if ((leading_byte & 0b11100000) == 0b11000000) {
      // We have a two-byte UTF-8
      if (pos + 1 >= len) {
        return result(error_code::TOO_SHORT, pos);
      } // minimal bound checking
      if ((uint8_t(data[pos + 1]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      // range check
      uint32_t code_point = (leading_byte & 0b00011111) << 6 |
                            (uint8_t(data[pos + 1]) & 0b00111111);
      if (code_point < 0x80) {
        return result(error_code::OVERLONG, pos);
      }
      *utf32_output++ = char32_t(code_point);
      pos += 2;
    } else if ((leading_byte & 0b11110000) == 0b11100000) {
      // We have a three-byte UTF-8
      if (pos + 2 >= len) {
        return result(error_code::TOO_SHORT, pos);
      } // minimal bound checking

      if ((uint8_t(data[pos + 1]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((uint8_t(data[pos + 2]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      // range check
      uint32_t code_point = (leading_byte & 0b00001111) << 12 |
                            (uint8_t(data[pos + 1]) & 0b00111111) << 6 |
                            (uint8_t(data[pos + 2]) & 0b00111111);
      if (code_point < 0x800) {
        return result(error_code::OVERLONG, pos);
      }
      if (0xd7ff < code_point && code_point < 0xe000) {
        return result(error_code::SURROGATE, pos);
      }
      *utf32_output++ = char32_t(code_point);
      pos += 3;
    } else if ((leading_byte & 0b11111000) == 0b11110000) { // 0b11110000
      // we have a 4-byte UTF-8 word.
      if (pos + 3 >= len) {
        return result(error_code::TOO_SHORT, pos);
      } // minimal bound checking
      if ((uint8_t(data[pos + 1]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((uint8_t(data[pos + 2]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }
      if ((uint8_t(data[pos + 3]) & 0b11000000) != 0b10000000) {
        return result(error_code::TOO_SHORT, pos);
      }

      // range check
      uint32_t code_point = (leading_byte & 0b00000111) << 18 |
                            (uint8_t(data[pos + 1]) & 0b00111111) << 12 |
                            (uint8_t(data[pos + 2]) & 0b00111111) << 6 |
                            (uint8_t(data[pos + 3]) & 0b00111111);
      if (code_point <= 0xffff) {
        return result(error_code::OVERLONG, pos);
      }
      if (0x10ffff < code_point) {
        return result(error_code::TOO_LARGE, pos);
      }
      *utf32_output++ = char32_t(code_point);
      pos += 4;
    } else {
      // we either have too many continuation bytes or an invalid leading byte
      if ((leading_byte & 0b11000000) == 0b10000000) {
        return result(error_code::TOO_LONG, pos);
      } else {
        return result(error_code::HEADER_BITS, pos);
      }
    }
  }
  return result(error_code::SUCCESS, utf32_output - start);
}

/**
 * When rewind_and_convert_with_errors is called, we are pointing at 'buf' and
 * we have up to len input bytes left, and we encountered some error. It is
 * possible that the error is at 'buf' exactly, but it could also be in the
 * previous bytes location (up to 3 bytes back).
 *
 * prior_bytes indicates how many bytes, prior to 'buf' may belong to the
 * current memory section and can be safely accessed. We prior_bytes to access
 * safely up to three bytes before 'buf'.
 *
 * The caller is responsible to ensure that len > 0.
 *
 * If the error is believed to have occurred prior to 'buf', the count value
 * contain in the result will be SIZE_T - 1, SIZE_T - 2, or SIZE_T - 3.
 */
inline result rewind_and_convert_with_errors(size_t prior_bytes,
                                             const char *buf, size_t len,
                                             char32_t *utf32_output) {
  size_t extra_len{0};
  // We potentially need to go back in time and find a leading byte.
  size_t how_far_back = 3; // 3 bytes in the past + current position
  if (how_far_back > prior_bytes) {
    how_far_back = prior_bytes;
  }
  bool found_leading_bytes{false};
  // important: it is i <= how_far_back and not 'i < how_far_back'.
  for (size_t i = 0; i <= how_far_back; i++) {
    unsigned char byte = buf[-static_cast<std::ptrdiff_t>(i)];
    found_leading_bytes = ((byte & 0b11000000) != 0b10000000);
    if (found_leading_bytes) {
      if (i > 0 && byte < 128) {
        // If we had to go back and the leading byte is ascii
        // then we can stop right away.
        return result(error_code::TOO_LONG, 0 - i + 1);
      }
      buf -= i;
      extra_len = i;
      break;
    }
  }
  //
  // It is possible for this function to return a negative count in its result.
  // C++ Standard Section 18.1 defines size_t is in <cstddef> which is described
  // in C Standard as <stddef.h>. C Standard Section 4.1.5 defines size_t as an
  // unsigned integral type of the result of the sizeof operator
  //
  // An unsigned type will simply wrap round arithmetically (well defined).
  //
  if (!found_leading_bytes) {
    // If how_far_back == 3, we may have four consecutive continuation bytes!!!
    // [....] [continuation] [continuation] [continuation] | [buf is
    // continuation] Or we possibly have a stream that does not start with a
    // leading byte.
    return result(error_code::TOO_LONG, 0 - how_far_back);
  }

  result res = convert_with_errors(buf, len + extra_len, utf32_output);
  if (res.error) {
    res.count -= extra_len;
  }
  return res;
}

} // namespace utf8_to_utf32
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf8_to_utf32/utf8_to_utf32.h */
/* begin file include/simdutf/scalar/utf8_to_utf32/valid_utf8_to_utf32.h */
#ifndef SIMDUTF_VALID_UTF8_TO_UTF32_H
#define SIMDUTF_VALID_UTF8_TO_UTF32_H

#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace utf8_to_utf32 {

template <typename InputPtr>
#if SIMDUTF_CPLUSPLUS20
  requires simdutf::detail::indexes_into_byte_like<InputPtr>
#endif
simdutf_constexpr23 size_t convert_valid(InputPtr data, size_t len,
                                         char32_t *utf32_output) {
  size_t pos = 0;
  char32_t *start{utf32_output};
  while (pos < len) {
#if SIMDUTF_CPLUSPLUS23
    if !consteval
#endif
    {
      // try to convert the next block of 8 ASCII bytes
      if (pos + 8 <= len) { // if it is safe to read 8 more bytes, check that
                            // they are ascii
        uint64_t v;
        ::memcpy(&v, data + pos, sizeof(uint64_t));
        if ((v & 0x8080808080808080) == 0) {
          size_t final_pos = pos + 8;
          while (pos < final_pos) {
            *utf32_output++ = uint8_t(data[pos]);
            pos++;
          }
          continue;
        }
      }
    }
    auto leading_byte = uint8_t(data[pos]); // leading byte
    if (leading_byte < 0b10000000) {
      // converting one ASCII byte !!!
      *utf32_output++ = char32_t(leading_byte);
      pos++;
    } else if ((leading_byte & 0b11100000) == 0b11000000) {
      // We have a two-byte UTF-8
      if (pos + 1 >= len) {
        break;
      } // minimal bound checking
      *utf32_output++ = char32_t(((leading_byte & 0b00011111) << 6) |
                                 (uint8_t(data[pos + 1]) & 0b00111111));
      pos += 2;
    } else if ((leading_byte & 0b11110000) == 0b11100000) {
      // We have a three-byte UTF-8
      if (pos + 2 >= len) {
        break;
      } // minimal bound checking
      *utf32_output++ = char32_t(((leading_byte & 0b00001111) << 12) |
                                 ((uint8_t(data[pos + 1]) & 0b00111111) << 6) |
                                 (uint8_t(data[pos + 2]) & 0b00111111));
      pos += 3;
    } else if ((leading_byte & 0b11111000) == 0b11110000) { // 0b11110000
      // we have a 4-byte UTF-8 word.
      if (pos + 3 >= len) {
        break;
      } // minimal bound checking
      uint32_t code_word = ((leading_byte & 0b00000111) << 18) |
                           ((uint8_t(data[pos + 1]) & 0b00111111) << 12) |
                           ((uint8_t(data[pos + 2]) & 0b00111111) << 6) |
                           (uint8_t(data[pos + 3]) & 0b00111111);
      *utf32_output++ = char32_t(code_word);
      pos += 4;
    } else {
      // we may have a continuation but we do not do error checking
      return 0;
    }
  }
  return utf32_output - start;
}

} // namespace utf8_to_utf32
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/utf8_to_utf32/valid_utf8_to_utf32.h */

namespace simdutf {

constexpr size_t default_line_length =
    76; ///< default line length for base64 encoding with lines

#if SIMDUTF_FEATURE_DETECT_ENCODING
/**
 * Autodetect the encoding of the input, a single encoding is recommended.
 * E.g., the function might return simdutf::encoding_type::UTF8,
 * simdutf::encoding_type::UTF16_LE, simdutf::encoding_type::UTF16_BE, or
 * simdutf::encoding_type::UTF32_LE.
 *
 * @param input the string to analyze.
 * @param length the length of the string in bytes.
 * @return the detected encoding type
 */
simdutf_warn_unused simdutf::encoding_type
autodetect_encoding(const char *input, size_t length) noexcept;
simdutf_really_inline simdutf_warn_unused simdutf::encoding_type
autodetect_encoding(const uint8_t *input, size_t length) noexcept {
  return autodetect_encoding(reinterpret_cast<const char *>(input), length);
}
  #if SIMDUTF_SPAN
/**
 * Autodetect the encoding of the input, a single encoding is recommended.
 * E.g., the function might return simdutf::encoding_type::UTF8,
 * simdutf::encoding_type::UTF16_LE, simdutf::encoding_type::UTF16_BE, or
 * simdutf::encoding_type::UTF32_LE.
 *
 * @param input the string to analyze. can be a anything span-like that has a
 * data() and size() that points to character data: std::string,
 * std::string_view, std::vector<char>, std::span<const std::byte> etc.
 * @return the detected encoding type
 */
simdutf_really_inline simdutf_warn_unused simdutf::encoding_type
autodetect_encoding(
    const detail::input_span_of_byte_like auto &input) noexcept {
  return autodetect_encoding(reinterpret_cast<const char *>(input.data()),
                             input.size());
}
  #endif // SIMDUTF_SPAN

/**
 * Autodetect the possible encodings of the input in one pass.
 * E.g., if the input might be UTF-16LE or UTF-8, this function returns
 * the value (simdutf::encoding_type::UTF8 | simdutf::encoding_type::UTF16_LE).
 *
 * Overridden by each implementation.
 *
 * @param input the string to analyze.
 * @param length the length of the string in bytes.
 * @return the detected encoding type
 */
simdutf_warn_unused int detect_encodings(const char *input,
                                         size_t length) noexcept;
simdutf_really_inline simdutf_warn_unused int
detect_encodings(const uint8_t *input, size_t length) noexcept {
  return detect_encodings(reinterpret_cast<const char *>(input), length);
}
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused int
detect_encodings(const detail::input_span_of_byte_like auto &input) noexcept {
  return detect_encodings(reinterpret_cast<const char *>(input.data()),
                          input.size());
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF8 || SIMDUTF_FEATURE_DETECT_ENCODING
/**
 * Validate the UTF-8 string. This function may be best when you expect
 * the input to be almost always valid. Otherwise, consider using
 * validate_utf8_with_errors.
 *
 * Overridden by each implementation.
 *
 * @param buf the UTF-8 string to validate.
 * @param len the length of the string in bytes.
 * @return true if and only if the string is valid UTF-8.
 */
simdutf_warn_unused bool validate_utf8(const char *buf, size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_constexpr23 simdutf_really_inline simdutf_warn_unused bool
validate_utf8(const detail::input_span_of_byte_like auto &input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8::validate(
        detail::constexpr_cast_ptr<uint8_t>(input.data()), input.size());
  } else
    #endif
  {
    return validate_utf8(reinterpret_cast<const char *>(input.data()),
                         input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 || SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF8
/**
 * Validate the UTF-8 string and stop on error.
 *
 * Overridden by each implementation.
 *
 * @param buf the UTF-8 string to validate.
 * @param len the length of the string in bytes.
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of code units validated if
 * successful.
 */
simdutf_warn_unused result validate_utf8_with_errors(const char *buf,
                                                     size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_constexpr23 simdutf_warn_unused result
validate_utf8_with_errors(
    const detail::input_span_of_byte_like auto &input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8::validate_with_errors(
        detail::constexpr_cast_ptr<uint8_t>(input.data()), input.size());
  } else
    #endif
  {
    return validate_utf8_with_errors(
        reinterpret_cast<const char *>(input.data()), input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8

#if SIMDUTF_FEATURE_ASCII
/**
 * Validate the ASCII string.
 *
 * Overridden by each implementation.
 *
 * @param buf the ASCII string to validate.
 * @param len the length of the string in bytes.
 * @return true if and only if the string is valid ASCII.
 */
simdutf_warn_unused bool validate_ascii(const char *buf, size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 bool
validate_ascii(const detail::input_span_of_byte_like auto &input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::ascii::validate(
        detail::constexpr_cast_ptr<std::uint8_t>(input.data()), input.size());
  } else
    #endif
  {
    return validate_ascii(reinterpret_cast<const char *>(input.data()),
                          input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Validate the ASCII string and stop on error. It might be faster than
 * validate_utf8 when an error is expected to occur early.
 *
 * Overridden by each implementation.
 *
 * @param buf the ASCII string to validate.
 * @param len the length of the string in bytes.
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of code units validated if
 * successful.
 */
simdutf_warn_unused result validate_ascii_with_errors(const char *buf,
                                                      size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
validate_ascii_with_errors(
    const detail::input_span_of_byte_like auto &input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::ascii::validate_with_errors(
        detail::constexpr_cast_ptr<std::uint8_t>(input.data()), input.size());
  } else
    #endif
  {
    return validate_ascii_with_errors(
        reinterpret_cast<const char *>(input.data()), input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_ASCII

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_ASCII
/**
 * Validate the ASCII string as a UTF-16 sequence.
 * An UTF-16 sequence is considered an ASCII sequence
 * if it could be converted to an ASCII string losslessly.
 *
 * Overridden by each implementation.
 *
 * @param buf the UTF-16 string to validate.
 * @param len the length of the string in bytes.
 * @return true if and only if the string is valid ASCII.
 */
simdutf_warn_unused bool validate_utf16_as_ascii(const char16_t *buf,
                                                 size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 bool
validate_utf16_as_ascii(std::span<const char16_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::validate_as_ascii<endianness::NATIVE>(input.data(),
                                                                input.size());
  } else
    #endif
  {
    return validate_utf16_as_ascii(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Validate the ASCII string as a UTF-16BE sequence.
 * An UTF-16 sequence is considered an ASCII sequence
 * if it could be converted to an ASCII string losslessly.
 *
 * Overridden by each implementation.
 *
 * @param buf the UTF-16BE string to validate.
 * @param len the length of the string in bytes.
 * @return true if and only if the string is valid ASCII.
 */
simdutf_warn_unused bool validate_utf16be_as_ascii(const char16_t *buf,
                                                   size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 bool
validate_utf16be_as_ascii(std::span<const char16_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::validate_as_ascii<endianness::BIG>(input.data(),
                                                             input.size());
  } else
    #endif
  {
    return validate_utf16be_as_ascii(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Validate the ASCII string as a UTF-16LE sequence.
 * An UTF-16 sequence is considered an ASCII sequence
 * if it could be converted to an ASCII string losslessly.
 *
 * Overridden by each implementation.
 *
 * @param buf the UTF-16LE string to validate.
 * @param len the length of the string in bytes.
 * @return true if and only if the string is valid ASCII.
 */
simdutf_warn_unused bool validate_utf16le_as_ascii(const char16_t *buf,
                                                   size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 bool
validate_utf16le_as_ascii(std::span<const char16_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::validate_as_ascii<endianness::LITTLE>(input.data(),
                                                                input.size());
  } else
    #endif
  {
    return validate_utf16le_as_ascii(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_ASCII

#if SIMDUTF_FEATURE_UTF16
/**
 * Using native endianness; Validate the UTF-16 string.
 * This function may be best when you expect the input to be almost always
 * valid. Otherwise, consider using validate_utf16_with_errors.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16 string to validate.
 * @param len the length of the string in number of 2-byte code units
 * (char16_t).
 * @return true if and only if the string is valid UTF-16.
 */
simdutf_warn_unused bool validate_utf16(const char16_t *buf,
                                        size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 bool
validate_utf16(std::span<const char16_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::validate<endianness::NATIVE>(input.data(),
                                                       input.size());
  } else
    #endif
  {
    return validate_utf16(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF16 || SIMDUTF_FEATURE_DETECT_ENCODING
/**
 * Validate the UTF-16LE string. This function may be best when you expect
 * the input to be almost always valid. Otherwise, consider using
 * validate_utf16le_with_errors.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16LE string to validate.
 * @param len the length of the string in number of 2-byte code units
 * (char16_t).
 * @return true if and only if the string is valid UTF-16LE.
 */
simdutf_warn_unused bool validate_utf16le(const char16_t *buf,
                                          size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_constexpr23 simdutf_warn_unused bool
validate_utf16le(std::span<const char16_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::validate<endianness::LITTLE>(input.data(),
                                                       input.size());
  } else
    #endif
  {
    return validate_utf16le(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16 || SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF16
/**
 * Validate the UTF-16BE string. This function may be best when you expect
 * the input to be almost always valid. Otherwise, consider using
 * validate_utf16be_with_errors.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16BE string to validate.
 * @param len the length of the string in number of 2-byte code units
 * (char16_t).
 * @return true if and only if the string is valid UTF-16BE.
 */
simdutf_warn_unused bool validate_utf16be(const char16_t *buf,
                                          size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 bool
validate_utf16be(std::span<const char16_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::validate<endianness::BIG>(input.data(), input.size());
  } else
    #endif
  {
    return validate_utf16be(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Using native endianness; Validate the UTF-16 string and stop on error.
 * It might be faster than validate_utf16 when an error is expected to occur
 * early.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16 string to validate.
 * @param len the length of the string in number of 2-byte code units
 * (char16_t).
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of code units validated if
 * successful.
 */
simdutf_warn_unused result validate_utf16_with_errors(const char16_t *buf,
                                                      size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
validate_utf16_with_errors(std::span<const char16_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::validate_with_errors<endianness::NATIVE>(
        input.data(), input.size());
  } else
    #endif
  {
    return validate_utf16_with_errors(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Validate the UTF-16LE string and stop on error. It might be faster than
 * validate_utf16le when an error is expected to occur early.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16LE string to validate.
 * @param len the length of the string in number of 2-byte code units
 * (char16_t).
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of code units validated if
 * successful.
 */
simdutf_warn_unused result validate_utf16le_with_errors(const char16_t *buf,
                                                        size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
validate_utf16le_with_errors(std::span<const char16_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::validate_with_errors<endianness::LITTLE>(
        input.data(), input.size());
  } else
    #endif
  {
    return validate_utf16le_with_errors(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Validate the UTF-16BE string and stop on error. It might be faster than
 * validate_utf16be when an error is expected to occur early.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16BE string to validate.
 * @param len the length of the string in number of 2-byte code units
 * (char16_t).
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of code units validated if
 * successful.
 */
simdutf_warn_unused result validate_utf16be_with_errors(const char16_t *buf,
                                                        size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
validate_utf16be_with_errors(std::span<const char16_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::validate_with_errors<endianness::BIG>(input.data(),
                                                                input.size());
  } else
    #endif
  {
    return validate_utf16be_with_errors(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Fixes an ill-formed UTF-16LE string by replacing mismatched surrogates with
 * the Unicode replacement character U+FFFD. If input and output points to
 * different memory areas, the procedure copies string, and it's expected that
 * output memory is at least as big as the input. It's also possible to set
 * input equal output, that makes replacements an in-place operation.
 *
 * @param input the UTF-16LE string to correct.
 * @param len the length of the string in number of 2-byte code units
 * (char16_t).
 * @param output the output buffer.
 */
void to_well_formed_utf16le(const char16_t *input, size_t len,
                            char16_t *output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_constexpr23 void
to_well_formed_utf16le(std::span<const char16_t> input,
                       std::span<char16_t> output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    scalar::utf16::to_well_formed_utf16<endianness::LITTLE>(
        input.data(), input.size(), output.data());
  } else
    #endif
  {
    to_well_formed_utf16le(input.data(), input.size(), output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Fixes an ill-formed UTF-16BE string by replacing mismatched surrogates with
 * the Unicode replacement character U+FFFD. If input and output points to
 * different memory areas, the procedure copies string, and it's expected that
 * output memory is at least as big as the input. It's also possible to set
 * input equal output, that makes replacements an in-place operation.
 *
 * @param input the UTF-16BE string to correct.
 * @param len the length of the string in number of 2-byte code units
 * (char16_t).
 * @param output the output buffer.
 */
void to_well_formed_utf16be(const char16_t *input, size_t len,
                            char16_t *output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_constexpr23 void
to_well_formed_utf16be(std::span<const char16_t> input,
                       std::span<char16_t> output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    scalar::utf16::to_well_formed_utf16<endianness::BIG>(
        input.data(), input.size(), output.data());
  } else
    #endif
  {
    to_well_formed_utf16be(input.data(), input.size(), output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Fixes an ill-formed UTF-16 string by replacing mismatched surrogates with the
 * Unicode replacement character U+FFFD. If input and output points to different
 * memory areas, the procedure copies string, and it's expected that output
 * memory is at least as big as the input. It's also possible to set input equal
 * output, that makes replacements an in-place operation.
 *
 * @param input the UTF-16 string to correct.
 * @param len the length of the string in number of 2-byte code units
 * (char16_t).
 * @param output the output buffer.
 */
void to_well_formed_utf16(const char16_t *input, size_t len,
                          char16_t *output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_constexpr23 void
to_well_formed_utf16(std::span<const char16_t> input,
                     std::span<char16_t> output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    scalar::utf16::to_well_formed_utf16<endianness::NATIVE>(
        input.data(), input.size(), output.data());
  } else
    #endif
  {
    to_well_formed_utf16(input.data(), input.size(), output.data());
  }
}
  #endif // SIMDUTF_SPAN

#endif // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF32 || SIMDUTF_FEATURE_DETECT_ENCODING
/**
 * Validate the UTF-32 string. This function may be best when you expect
 * the input to be almost always valid. Otherwise, consider using
 * validate_utf32_with_errors.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-32 string to validate.
 * @param len the length of the string in number of 4-byte code units
 * (char32_t).
 * @return true if and only if the string is valid UTF-32.
 */
simdutf_warn_unused bool validate_utf32(const char32_t *buf,
                                        size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 bool
validate_utf32(std::span<const char32_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32::validate(
        detail::constexpr_cast_ptr<std::uint32_t>(input.data()), input.size());
  } else
    #endif
  {
    return validate_utf32(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF32 || SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF32
/**
 * Validate the UTF-32 string and stop on error. It might be faster than
 * validate_utf32 when an error is expected to occur early.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-32 string to validate.
 * @param len the length of the string in number of 4-byte code units
 * (char32_t).
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of code units validated if
 * successful.
 */
simdutf_warn_unused result validate_utf32_with_errors(const char32_t *buf,
                                                      size_t len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
validate_utf32_with_errors(std::span<const char32_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32::validate_with_errors(
        detail::constexpr_cast_ptr<std::uint32_t>(input.data()), input.size());
  } else
    #endif
  {
    return validate_utf32_with_errors(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
/**
 * Convert Latin1 string into UTF-8 string.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf8_output   the pointer to buffer that can hold conversion result
 * @return the number of written char; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_latin1_to_utf8(const char *input,
                                                  size_t length,
                                                  char *utf8_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_latin1_to_utf8(
    const detail::input_span_of_byte_like auto &latin1_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::latin1_to_utf8::convert(
        detail::constexpr_cast_ptr<char>(latin1_input.data()),
        latin1_input.size(),
        detail::constexpr_cast_writeptr<char>(utf8_output.data()));
  } else
    #endif
  {
    return convert_latin1_to_utf8(
        reinterpret_cast<const char *>(latin1_input.data()),
        latin1_input.size(), reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert Latin1 string into UTF-8 string with output limit.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * We write as many characters as possible.
 *
 * Using convert_latin1_to_utf8_safe instead of convert_latin1_to_utf8 comes
 * with a significant penalty in some cases, being up to four times slower,
 * especially on short inputs. If you have allocated the output buffer so that
 * it contains utf8_length_from_latin1(input, length) bytes, then prefer
 * convert_latin1_to_utf8.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf8_output  	the pointer to buffer that can hold conversion result
 * @param utf8_len      the maximum output length
 * @return the number of written char; 0 if conversion is not possible
 */
simdutf_warn_unused size_t
convert_latin1_to_utf8_safe(const char *input, size_t length, char *utf8_output,
                            size_t utf8_len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_latin1_to_utf8_safe(
    const detail::input_span_of_byte_like auto &input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
      // implementation note: outputspan is a forwarding ref to avoid copying
      // and allow both lvalues and rvalues. std::span can be copied without
      // problems, but std::vector should not, and this function should accept
      // both. it will allow using an owning rvalue ref (example: passing a
      // temporary std::string) as output, but the user will quickly find out
      // that he has no way of getting the data out of the object in that case.
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::latin1_to_utf8::convert_safe_constexpr(
        input.data(), input.size(), utf8_output.data(), utf8_output.size());
  } else
    #endif
  {
    return convert_latin1_to_utf8_safe(
        reinterpret_cast<const char *>(input.data()), input.size(),
        reinterpret_cast<char *>(utf8_output.data()), utf8_output.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1
/**
 * Convert possibly Latin1 string into UTF-16LE string.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_output  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_latin1_to_utf16le(
    const char *input, size_t length, char16_t *utf16_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_latin1_to_utf16le(
    const detail::input_span_of_byte_like auto &latin1_input,
    std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::latin1_to_utf16::convert<endianness::LITTLE>(
        latin1_input.data(), latin1_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_latin1_to_utf16le(
        reinterpret_cast<const char *>(latin1_input.data()),
        latin1_input.size(), utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert Latin1 string into UTF-16BE string.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_output  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_latin1_to_utf16be(
    const char *input, size_t length, char16_t *utf16_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_latin1_to_utf16be(const detail::input_span_of_byte_like auto &input,
                          std::span<char16_t> output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::latin1_to_utf16::convert<endianness::BIG>(
        input.data(), input.size(), output.data());
  } else
    #endif
  {
    return convert_latin1_to_utf16be(
        reinterpret_cast<const char *>(input.data()), input.size(),
        output.data());
  }
}
  #endif // SIMDUTF_SPAN
/**
 * Compute the number of bytes that this UTF-16 string would require in Latin1
 * format.
 *
 * @param length        the length of the string in Latin1 code units (char)
 * @return the length of the string in Latin1 code units (char) required to
 * encode the UTF-16 string as Latin1
 */
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
latin1_length_from_utf16(size_t length) noexcept {
  return length;
}

/**
 * Compute the number of code units that this Latin1 string would require in
 * UTF-16 format.
 *
 * @param length        the length of the string in Latin1 code units (char)
 * @return the length of the string in 2-byte code units (char16_t) required to
 * encode the Latin1 string as UTF-16
 */
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
utf16_length_from_latin1(size_t length) noexcept {
  return length;
}
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1
/**
 * Convert Latin1 string into UTF-32 string.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf32_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char32_t; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_latin1_to_utf32(
    const char *input, size_t length, char32_t *utf32_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_latin1_to_utf32(
    const detail::input_span_of_byte_like auto &latin1_input,
    std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::latin1_to_utf32::convert(
        latin1_input.data(), latin1_input.size(), utf32_output.data());
  } else
    #endif
  {
    return convert_latin1_to_utf32(
        reinterpret_cast<const char *>(latin1_input.data()),
        latin1_input.size(), utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
/**
 * Convert possibly broken UTF-8 string into latin1 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param latin1_output  the pointer to buffer that can hold conversion result
 * @return the number of written char; 0 if the input was not valid UTF-8 string
 * or if it cannot be represented as Latin1
 */
simdutf_warn_unused size_t convert_utf8_to_latin1(const char *input,
                                                  size_t length,
                                                  char *latin1_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf8_to_latin1(
    const detail::input_span_of_byte_like auto &input,
    detail::output_span_of_byte_like auto &&output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_latin1::convert(input.data(), input.size(),
                                           output.data());
  } else
    #endif
  {
    return convert_utf8_to_latin1(reinterpret_cast<const char *>(input.data()),
                                  input.size(),
                                  reinterpret_cast<char *>(output.data()));
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
/**
 * Using native endianness, convert possibly broken UTF-8 string into a UTF-16
 * string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_output  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t; 0 if the input was not valid UTF-8
 * string
 */
simdutf_warn_unused size_t convert_utf8_to_utf16(
    const char *input, size_t length, char16_t *utf16_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf8_to_utf16(const detail::input_span_of_byte_like auto &input,
                      std::span<char16_t> output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf16::convert<endianness::NATIVE>(
        input.data(), input.size(), output.data());
  } else
    #endif
  {
    return convert_utf8_to_utf16(reinterpret_cast<const char *>(input.data()),
                                 input.size(), output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Compute the number of bytes that this UTF-16LE string would require in UTF-8
 * format even when the UTF-16LE content contains mismatched surrogates
 * that have to be replaced by the replacement character (0xFFFD).
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) where the count is the number of bytes required to
 * encode the UTF-16LE string as UTF-8, and the error code is either SUCCESS or
 * SURROGATE. The count is correct regardless of the error field.
 * When SURROGATE is returned, it does not indicate an error in the case of this
 * function: it indicates that at least one surrogate has been encountered: the
 * surrogates may be matched or not (thus this function does not validate). If
 * the returned error code is SUCCESS, then the input contains no surrogate, is
 * in the Basic Multilingual Plane, and is necessarily valid.
 */
simdutf_warn_unused result utf8_length_from_utf16le_with_replacement(
    const char16_t *input, size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_constexpr23 simdutf_warn_unused result
utf8_length_from_utf16le_with_replacement(
    std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::utf8_length_from_utf16_with_replacement<
        endianness::LITTLE>(valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return utf8_length_from_utf16le_with_replacement(valid_utf16_input.data(),
                                                     valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Compute the number of bytes that this UTF-16BE string would require in UTF-8
 * format even when the UTF-16BE content contains mismatched surrogates
 * that have to be replaced by the replacement character (0xFFFD).
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) where the count is the number of bytes required to
 * encode the UTF-16BE string as UTF-8, and the error code is either SUCCESS or
 * SURROGATE. The count is correct regardless of the error field.
 * When SURROGATE is returned, it does not indicate an error in the case of this
 * function: it indicates that at least one surrogate has been encountered: the
 * surrogates may be matched or not (thus this function does not validate). If
 * the returned error code is SUCCESS, then the input contains no surrogate, is
 * in the Basic Multilingual Plane, and is necessarily valid.
 */
simdutf_warn_unused result utf8_length_from_utf16be_with_replacement(
    const char16_t *input, size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
utf8_length_from_utf16be_with_replacement(
    std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::utf8_length_from_utf16_with_replacement<
        endianness::BIG>(valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return utf8_length_from_utf16be_with_replacement(valid_utf16_input.data(),
                                                     valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN

#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1
/**
 * Using native endianness, convert a Latin1 string into a UTF-16 string.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_output  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t.
 */
simdutf_warn_unused size_t convert_latin1_to_utf16(
    const char *input, size_t length, char16_t *utf16_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_latin1_to_utf16(const detail::input_span_of_byte_like auto &input,
                        std::span<char16_t> output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::latin1_to_utf16::convert<endianness::NATIVE>(
        input.data(), input.size(), output.data());
  } else
    #endif
  {
    return convert_latin1_to_utf16(reinterpret_cast<const char *>(input.data()),
                                   input.size(), output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
/**
 * Convert possibly broken UTF-8 string into UTF-16LE string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_output  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t; 0 if the input was not valid UTF-8
 * string
 */
simdutf_warn_unused size_t convert_utf8_to_utf16le(
    const char *input, size_t length, char16_t *utf16_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf8_to_utf16le(const detail::input_span_of_byte_like auto &utf8_input,
                        std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf16::convert<endianness::LITTLE>(
        utf8_input.data(), utf8_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_utf8_to_utf16le(
        reinterpret_cast<const char *>(utf8_input.data()), utf8_input.size(),
        utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-8 string into UTF-16BE string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_output  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t; 0 if the input was not valid UTF-8
 * string
 */
simdutf_warn_unused size_t convert_utf8_to_utf16be(
    const char *input, size_t length, char16_t *utf16_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf8_to_utf16be(const detail::input_span_of_byte_like auto &utf8_input,
                        std::span<char16_t> utf16_output) noexcept {

    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf16::convert<endianness::BIG>(
        utf8_input.data(), utf8_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_utf8_to_utf16be(
        reinterpret_cast<const char *>(utf8_input.data()), utf8_input.size(),
        utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
/**
 * Convert possibly broken UTF-8 string into latin1 string with errors.
 * If the string cannot be represented as Latin1, an error
 * code is returned.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param latin1_output  the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of code units validated if
 * successful.
 */
simdutf_warn_unused result convert_utf8_to_latin1_with_errors(
    const char *input, size_t length, char *latin1_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf8_to_latin1_with_errors(
    const detail::input_span_of_byte_like auto &utf8_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_latin1::convert_with_errors(
        utf8_input.data(), utf8_input.size(), latin1_output.data());
  } else
    #endif
  {
    return convert_utf8_to_latin1_with_errors(
        reinterpret_cast<const char *>(utf8_input.data()), utf8_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
/**
 * Using native endianness, convert possibly broken UTF-8 string into UTF-16
 * string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_output  the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char16_t written if
 * successful.
 */
simdutf_warn_unused result convert_utf8_to_utf16_with_errors(
    const char *input, size_t length, char16_t *utf16_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf8_to_utf16_with_errors(
    const detail::input_span_of_byte_like auto &utf8_input,
    std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf16::convert_with_errors<endianness::NATIVE>(
        utf8_input.data(), utf8_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_utf8_to_utf16_with_errors(
        reinterpret_cast<const char *>(utf8_input.data()), utf8_input.size(),
        utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-8 string into UTF-16LE string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_output  the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char16_t written if
 * successful.
 */
simdutf_warn_unused result convert_utf8_to_utf16le_with_errors(
    const char *input, size_t length, char16_t *utf16_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf8_to_utf16le_with_errors(
    const detail::input_span_of_byte_like auto &utf8_input,
    std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf16::convert_with_errors<endianness::LITTLE>(
        utf8_input.data(), utf8_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_utf8_to_utf16le_with_errors(
        reinterpret_cast<const char *>(utf8_input.data()), utf8_input.size(),
        utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-8 string into UTF-16BE string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_output  the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char16_t written if
 * successful.
 */
simdutf_warn_unused result convert_utf8_to_utf16be_with_errors(
    const char *input, size_t length, char16_t *utf16_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf8_to_utf16be_with_errors(
    const detail::input_span_of_byte_like auto &utf8_input,
    std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf16::convert_with_errors<endianness::BIG>(
        utf8_input.data(), utf8_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_utf8_to_utf16be_with_errors(
        reinterpret_cast<const char *>(utf8_input.data()), utf8_input.size(),
        utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
/**
 * Convert possibly broken UTF-8 string into UTF-32 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf32_output  the pointer to buffer that can hold conversion result
 * @return the number of written char32_t; 0 if the input was not valid UTF-8
 * string
 */
simdutf_warn_unused size_t convert_utf8_to_utf32(
    const char *input, size_t length, char32_t *utf32_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf8_to_utf32(const detail::input_span_of_byte_like auto &utf8_input,
                      std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf32::convert(utf8_input.data(), utf8_input.size(),
                                          utf32_output.data());
  } else
    #endif
  {
    return convert_utf8_to_utf32(
        reinterpret_cast<const char *>(utf8_input.data()), utf8_input.size(),
        utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-8 string into UTF-32 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf32_output  the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char32_t written if
 * successful.
 */
simdutf_warn_unused result convert_utf8_to_utf32_with_errors(
    const char *input, size_t length, char32_t *utf32_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf8_to_utf32_with_errors(
    const detail::input_span_of_byte_like auto &utf8_input,
    std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf32::convert_with_errors(
        utf8_input.data(), utf8_input.size(), utf32_output.data());
  } else
    #endif
  {
    return convert_utf8_to_utf32_with_errors(
        reinterpret_cast<const char *>(utf8_input.data()), utf8_input.size(),
        utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
/**
 * Convert valid UTF-8 string into latin1 string.
 *
 * This function assumes that the input string is valid UTF-8 and that it can be
 * represented as Latin1. If you violate this assumption, the result is
 * implementation defined and may include system-dependent behavior such as
 * crashes.
 *
 * This function is for expert users only and not part of our public API. Use
 * convert_utf8_to_latin1 instead. The function may be removed from the library
 * in the future.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param latin1_output  the pointer to buffer that can hold conversion result
 * @return the number of written char; 0 if the input was not valid UTF-8 string
 */
simdutf_warn_unused size_t convert_valid_utf8_to_latin1(
    const char *input, size_t length, char *latin1_output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf8_to_latin1(
    const detail::input_span_of_byte_like auto &valid_utf8_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_latin1::convert_valid(
        valid_utf8_input.data(), valid_utf8_input.size(), latin1_output.data());
  } else
    #endif
  {
    return convert_valid_utf8_to_latin1(
        reinterpret_cast<const char *>(valid_utf8_input.data()),
        valid_utf8_input.size(), latin1_output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
/**
 * Using native endianness, convert valid UTF-8 string into a UTF-16 string.
 *
 * This function assumes that the input string is valid UTF-8.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t
 */
simdutf_warn_unused size_t convert_valid_utf8_to_utf16(
    const char *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf8_to_utf16(
    const detail::input_span_of_byte_like auto &valid_utf8_input,
    std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf16::convert_valid<endianness::NATIVE>(
        valid_utf8_input.data(), valid_utf8_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_valid_utf8_to_utf16(
        reinterpret_cast<const char *>(valid_utf8_input.data()),
        valid_utf8_input.size(), utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert valid UTF-8 string into UTF-16LE string.
 *
 * This function assumes that the input string is valid UTF-8.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t
 */
simdutf_warn_unused size_t convert_valid_utf8_to_utf16le(
    const char *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf8_to_utf16le(
    const detail::input_span_of_byte_like auto &valid_utf8_input,
    std::span<char16_t> utf16_output) noexcept {

    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf16::convert_valid<endianness::LITTLE>(
        valid_utf8_input.data(), valid_utf8_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_valid_utf8_to_utf16le(
        reinterpret_cast<const char *>(valid_utf8_input.data()),
        valid_utf8_input.size(), utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert valid UTF-8 string into UTF-16BE string.
 *
 * This function assumes that the input string is valid UTF-8.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t
 */
simdutf_warn_unused size_t convert_valid_utf8_to_utf16be(
    const char *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf8_to_utf16be(
    const detail::input_span_of_byte_like auto &valid_utf8_input,
    std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf16::convert_valid<endianness::BIG>(
        valid_utf8_input.data(), valid_utf8_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_valid_utf8_to_utf16be(
        reinterpret_cast<const char *>(valid_utf8_input.data()),
        valid_utf8_input.size(), utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
/**
 * Convert valid UTF-8 string into UTF-32 string.
 *
 * This function assumes that the input string is valid UTF-8.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf32_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char32_t
 */
simdutf_warn_unused size_t convert_valid_utf8_to_utf32(
    const char *input, size_t length, char32_t *utf32_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf8_to_utf32(
    const detail::input_span_of_byte_like auto &valid_utf8_input,
    std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8_to_utf32::convert_valid(
        valid_utf8_input.data(), valid_utf8_input.size(), utf32_output.data());
  } else
    #endif
  {
    return convert_valid_utf8_to_utf32(
        reinterpret_cast<const char *>(valid_utf8_input.data()),
        valid_utf8_input.size(), utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
/**
 * Return the number of bytes that this Latin1 string would require in UTF-8
 * format.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string bytes
 * @return the number of bytes required to encode the Latin1 string as UTF-8
 */
simdutf_warn_unused size_t utf8_length_from_latin1(const char *input,
                                                   size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
utf8_length_from_latin1(
    const detail::input_span_of_byte_like auto &latin1_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::latin1_to_utf8::utf8_length_from_latin1(latin1_input.data(),
                                                           latin1_input.size());
  } else
    #endif
  {
    return utf8_length_from_latin1(
        reinterpret_cast<const char *>(latin1_input.data()),
        latin1_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Compute the number of bytes that this UTF-8 string would require in Latin1
 * format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-8 strings but in such cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in byte
 * @return the number of bytes required to encode the UTF-8 string as Latin1
 */
simdutf_warn_unused size_t latin1_length_from_utf8(const char *input,
                                                   size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
latin1_length_from_utf8(
    const detail::input_span_of_byte_like auto &valid_utf8_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8::count_code_points(valid_utf8_input.data(),
                                           valid_utf8_input.size());
  } else
    #endif
  {
    return latin1_length_from_utf8(
        reinterpret_cast<const char *>(valid_utf8_input.data()),
        valid_utf8_input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
/**
 * Compute the number of 2-byte code units that this UTF-8 string would require
 * in UTF-16LE format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-8 strings but in such cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-8 string to process
 * @param length        the length of the string in bytes
 * @return the number of char16_t code units required to encode the UTF-8 string
 * as UTF-16LE
 */
simdutf_warn_unused size_t utf16_length_from_utf8(const char *input,
                                                  size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
utf16_length_from_utf8(
    const detail::input_span_of_byte_like auto &valid_utf8_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8::utf16_length_from_utf8(valid_utf8_input.data(),
                                                valid_utf8_input.size());
  } else
    #endif
  {
    return utf16_length_from_utf8(
        reinterpret_cast<const char *>(valid_utf8_input.data()),
        valid_utf8_input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
/**
 * Compute the number of 4-byte code units that this UTF-8 string would require
 * in UTF-32 format.
 *
 * This function is equivalent to count_utf8
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-8 strings but in such cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-8 string to process
 * @param length        the length of the string in bytes
 * @return the number of char32_t code units required to encode the UTF-8 string
 * as UTF-32
 */
simdutf_warn_unused size_t utf32_length_from_utf8(const char *input,
                                                  size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
utf32_length_from_utf8(
    const detail::input_span_of_byte_like auto &valid_utf8_input) noexcept {

    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8::count_code_points(valid_utf8_input.data(),
                                           valid_utf8_input.size());
  } else
    #endif
  {
    return utf32_length_from_utf8(
        reinterpret_cast<const char *>(valid_utf8_input.data()),
        valid_utf8_input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
/**
 * Using native endianness, convert possibly broken UTF-16 string into UTF-8
 * string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE
 * string
 */
simdutf_warn_unused size_t convert_utf16_to_utf8(const char16_t *input,
                                                 size_t length,
                                                 char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16_to_utf8(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert<endianness::NATIVE>(
        utf16_input.data(), utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_utf16_to_utf8(utf16_input.data(), utf16_input.size(),
                                 reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Using native endianness, convert possibly broken UTF-16 string into UTF-8
 * string with output limit.
 *
 * We write as many characters as possible into the output buffer,
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * Using convert_utf16_to_utf8_safe instead of convert_utf16_to_utf8 comes with
 * a significant penalty in some cases, being up to three times slower,
 * especially on short inputs. If you have allocated the output buffer so that
 * it contains utf8_length_from_utf16(input, length) bytes, then prefer
 * convert_utf16_to_utf8.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 16-bit code units (char16_t)
 * @param utf8_output  	the pointer to buffer that can hold conversion result
 * @param utf8_len      the maximum output length
 * @return the number of written char; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_utf16_to_utf8_safe(const char16_t *input,
                                                      size_t length,
                                                      char *utf8_output,
                                                      size_t utf8_len) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16_to_utf8_safe(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
      // implementation note: outputspan is a forwarding ref to avoid copying
      // and allow both lvalues and rvalues. std::span can be copied without
      // problems, but std::vector should not, and this function should accept
      // both. it will allow using an owning rvalue ref (example: passing a
      // temporary std::string) as output, but the user will quickly find out
      // that he has no way of getting the data out of the object in that case.
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    const full_result r =
        scalar::utf16_to_utf8::convert_with_errors<endianness::NATIVE, true>(
            utf16_input.data(), utf16_input.size(), utf8_output.data(),
            utf8_output.size());
    if (r.error != error_code::SUCCESS &&
        r.error != error_code::OUTPUT_BUFFER_TOO_SMALL) {
      return 0;
    }
    return r.output_count;
  } else
    #endif
  {
    return convert_utf16_to_utf8_safe(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()), utf8_output.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1
/**
 * Using native endianness, convert possibly broken UTF-16 string into Latin1
 * string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16 string
 * or if it cannot be represented as Latin1
 */
simdutf_warn_unused size_t convert_utf16_to_latin1(
    const char16_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16_to_latin1(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_latin1::convert<endianness::NATIVE>(
        utf16_input.data(), utf16_input.size(), latin1_output.data());
  } else
    #endif
  {
    return convert_utf16_to_latin1(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16LE string into Latin1 string.
 * If the string cannot be represented as Latin1, an error
 * is returned.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE
 * string or if it cannot be represented as Latin1
 */
simdutf_warn_unused size_t convert_utf16le_to_latin1(
    const char16_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16le_to_latin1(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_latin1::convert<endianness::LITTLE>(
        utf16_input.data(), utf16_input.size(), latin1_output.data());
  } else
    #endif
  {
    return convert_utf16le_to_latin1(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16BE string into Latin1 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16BE
 * string or if it cannot be represented as Latin1
 */
simdutf_warn_unused size_t convert_utf16be_to_latin1(
    const char16_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16be_to_latin1(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_latin1::convert<endianness::BIG>(
        utf16_input.data(), utf16_input.size(), latin1_output.data());
  } else
    #endif
  {
    return convert_utf16be_to_latin1(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
/**
 * Convert possibly broken UTF-16LE string into UTF-8 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE
 * string
 */
simdutf_warn_unused size_t convert_utf16le_to_utf8(const char16_t *input,
                                                   size_t length,
                                                   char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16le_to_utf8(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert<endianness::LITTLE>(
        utf16_input.data(), utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_utf16le_to_utf8(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16BE string into UTF-8 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE
 * string
 */
simdutf_warn_unused size_t convert_utf16be_to_utf8(const char16_t *input,
                                                   size_t length,
                                                   char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16be_to_utf8(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert<endianness::BIG>(
        utf16_input.data(), utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_utf16be_to_utf8(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1
/**
 * Using native endianness, convert possibly broken UTF-16 string into Latin1
 * string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char written if
 * successful.
 */
simdutf_warn_unused result convert_utf16_to_latin1_with_errors(
    const char16_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf16_to_latin1_with_errors(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_latin1::convert_with_errors<endianness::NATIVE>(
        utf16_input.data(), utf16_input.size(), latin1_output.data());
  } else
    #endif
  {
    return convert_utf16_to_latin1_with_errors(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16LE string into Latin1 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char written if
 * successful.
 */
simdutf_warn_unused result convert_utf16le_to_latin1_with_errors(
    const char16_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf16le_to_latin1_with_errors(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_latin1::convert_with_errors<endianness::LITTLE>(
        utf16_input.data(), utf16_input.size(), latin1_output.data());
  } else
    #endif
  {
    return convert_utf16le_to_latin1_with_errors(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16BE string into Latin1 string.
 * If the string cannot be represented as Latin1, an error
 * is returned.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char written if
 * successful.
 */
simdutf_warn_unused result convert_utf16be_to_latin1_with_errors(
    const char16_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf16be_to_latin1_with_errors(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_latin1::convert_with_errors<endianness::BIG>(
        utf16_input.data(), utf16_input.size(), latin1_output.data());
  } else
    #endif
  {
    return convert_utf16be_to_latin1_with_errors(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
/**
 * Using native endianness, convert possibly broken UTF-16 string into UTF-8
 * string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char written if
 * successful.
 */
simdutf_warn_unused result convert_utf16_to_utf8_with_errors(
    const char16_t *input, size_t length, char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf16_to_utf8_with_errors(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert_with_errors<endianness::NATIVE>(
        utf16_input.data(), utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_utf16_to_utf8_with_errors(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16LE string into UTF-8 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char written if
 * successful.
 */
simdutf_warn_unused result convert_utf16le_to_utf8_with_errors(
    const char16_t *input, size_t length, char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf16le_to_utf8_with_errors(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert_with_errors<endianness::LITTLE>(
        utf16_input.data(), utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_utf16le_to_utf8_with_errors(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16BE string into UTF-8 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char written if
 * successful.
 */
simdutf_warn_unused result convert_utf16be_to_utf8_with_errors(
    const char16_t *input, size_t length, char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf16be_to_utf8_with_errors(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert_with_errors<endianness::BIG>(
        utf16_input.data(), utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_utf16be_to_utf8_with_errors(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16LE string into UTF-8 string, replacing
 * unpaired surrogates with the Unicode replacement character U+FFFD.
 *
 * This function always succeeds: unpaired surrogates are replaced with
 * U+FFFD (3 bytes in UTF-8: 0xEF 0xBF 0xBD).
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units
 */
simdutf_warn_unused size_t convert_utf16le_to_utf8_with_replacement(
    const char16_t *input, size_t length, char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16le_to_utf8_with_replacement(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert_with_replacement<endianness::LITTLE>(
        utf16_input.data(), utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_utf16le_to_utf8_with_replacement(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16BE string into UTF-8 string, replacing
 * unpaired surrogates with the Unicode replacement character U+FFFD.
 *
 * This function always succeeds: unpaired surrogates are replaced with
 * U+FFFD (3 bytes in UTF-8: 0xEF 0xBF 0xBD).
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units
 */
simdutf_warn_unused size_t convert_utf16be_to_utf8_with_replacement(
    const char16_t *input, size_t length, char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16be_to_utf8_with_replacement(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert_with_replacement<endianness::BIG>(
        utf16_input.data(), utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_utf16be_to_utf8_with_replacement(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16 string (native endianness) into UTF-8 string,
 * replacing unpaired surrogates with the Unicode replacement character U+FFFD.
 *
 * This function always succeeds: unpaired surrogates are replaced with
 * U+FFFD (3 bytes in UTF-8: 0xEF 0xBF 0xBD).
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units
 */
simdutf_warn_unused size_t convert_utf16_to_utf8_with_replacement(
    const char16_t *input, size_t length, char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16_to_utf8_with_replacement(
    std::span<const char16_t> utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert_with_replacement<endianness::NATIVE>(
        utf16_input.data(), utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_utf16_to_utf8_with_replacement(
        utf16_input.data(), utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
/**
 * Using native endianness, convert valid UTF-16 string into UTF-8 string.
 *
 * This function assumes that the input string is valid UTF-16.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to a buffer that can hold the conversion
 * result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16_to_utf8(
    const char16_t *input, size_t length, char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf16_to_utf8(
    std::span<const char16_t> valid_utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert_valid<endianness::NATIVE>(
        valid_utf16_input.data(), valid_utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_valid_utf16_to_utf8(
        valid_utf16_input.data(), valid_utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1
/**
 * Using native endianness, convert UTF-16 string into Latin1 string.
 *
 * This function assumes that the input string is valid UTF-16 and that it can
 * be represented as Latin1. If you violate this assumption, the result is
 * implementation defined and may include system-dependent behavior such as
 * crashes.
 *
 * This function is for expert users only and not part of our public API. Use
 * convert_utf16_to_latin1 instead. The function may be removed from the library
 * in the future.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16_to_latin1(
    const char16_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf16_to_latin1(
    std::span<const char16_t> valid_utf16_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_latin1::convert_valid_impl<endianness::NATIVE>(
        detail::constexpr_cast_ptr<uint16_t>(valid_utf16_input.data()),
        valid_utf16_input.size(),
        detail::constexpr_cast_writeptr<char>(latin1_output.data()));
  } else
    #endif
  {
    return convert_valid_utf16_to_latin1(
        valid_utf16_input.data(), valid_utf16_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert valid UTF-16LE string into Latin1 string.
 *
 * This function assumes that the input string is valid UTF-16LE and that it can
 * be represented as Latin1. If you violate this assumption, the result is
 * implementation defined and may include system-dependent behavior such as
 * crashes.
 *
 * This function is for expert users only and not part of our public API. Use
 * convert_utf16le_to_latin1 instead. The function may be removed from the
 * library in the future.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16le_to_latin1(
    const char16_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_constexpr23 simdutf_warn_unused size_t
convert_valid_utf16le_to_latin1(
    std::span<const char16_t> valid_utf16_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_latin1::convert_valid_impl<endianness::LITTLE>(
        detail::constexpr_cast_ptr<uint16_t>(valid_utf16_input.data()),
        valid_utf16_input.size(),
        detail::constexpr_cast_writeptr<char>(latin1_output.data()));
  } else
    #endif
  {
    return convert_valid_utf16le_to_latin1(
        valid_utf16_input.data(), valid_utf16_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert valid UTF-16BE string into Latin1 string.
 *
 * This function assumes that the input string is valid UTF-16BE and that it can
 * be represented as Latin1. If you violate this assumption, the result is
 * implementation defined and may include system-dependent behavior such as
 * crashes.
 *
 * This function is for expert users only and not part of our public API. Use
 * convert_utf16be_to_latin1 instead. The function may be removed from the
 * library in the future.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16be_to_latin1(
    const char16_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_constexpr23 simdutf_warn_unused size_t
convert_valid_utf16be_to_latin1(
    std::span<const char16_t> valid_utf16_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_latin1::convert_valid_impl<endianness::BIG>(
        detail::constexpr_cast_ptr<uint16_t>(valid_utf16_input.data()),
        valid_utf16_input.size(),
        detail::constexpr_cast_writeptr<char>(latin1_output.data()));
  } else
    #endif
  {
    return convert_valid_utf16be_to_latin1(
        valid_utf16_input.data(), valid_utf16_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
/**
 * Convert valid UTF-16LE string into UTF-8 string.
 *
 * This function assumes that the input string is valid UTF-16LE
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to a buffer that can hold the conversion
 * result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16le_to_utf8(
    const char16_t *input, size_t length, char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf16le_to_utf8(
    std::span<const char16_t> valid_utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert_valid<endianness::NATIVE>(
        valid_utf16_input.data(), valid_utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_valid_utf16le_to_utf8(
        valid_utf16_input.data(), valid_utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert valid UTF-16BE string into UTF-8 string.
 *
 * This function assumes that the input string is valid UTF-16BE.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to a buffer that can hold the conversion
 * result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16be_to_utf8(
    const char16_t *input, size_t length, char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf16be_to_utf8(
    std::span<const char16_t> valid_utf16_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf8::convert_valid<endianness::BIG>(
        valid_utf16_input.data(), valid_utf16_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_valid_utf16be_to_utf8(
        valid_utf16_input.data(), valid_utf16_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
/**
 * Using native endianness, convert possibly broken UTF-16 string into UTF-32
 * string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE
 * string
 */
simdutf_warn_unused size_t convert_utf16_to_utf32(
    const char16_t *input, size_t length, char32_t *utf32_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16_to_utf32(std::span<const char16_t> utf16_input,
                       std::span<char32_t> utf32_output) noexcept {

    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf32::convert<endianness::NATIVE>(
        utf16_input.data(), utf16_input.size(), utf32_output.data());
  } else
    #endif
  {
    return convert_utf16_to_utf32(utf16_input.data(), utf16_input.size(),
                                  utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16LE string into UTF-32 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE
 * string
 */
simdutf_warn_unused size_t convert_utf16le_to_utf32(
    const char16_t *input, size_t length, char32_t *utf32_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16le_to_utf32(std::span<const char16_t> utf16_input,
                         std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf32::convert<endianness::LITTLE>(
        utf16_input.data(), utf16_input.size(), utf32_output.data());
  } else
    #endif
  {
    return convert_utf16le_to_utf32(utf16_input.data(), utf16_input.size(),
                                    utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16BE string into UTF-32 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE
 * string
 */
simdutf_warn_unused size_t convert_utf16be_to_utf32(
    const char16_t *input, size_t length, char32_t *utf32_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf16be_to_utf32(std::span<const char16_t> utf16_input,
                         std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf32::convert<endianness::BIG>(
        utf16_input.data(), utf16_input.size(), utf32_output.data());
  } else
    #endif
  {
    return convert_utf16be_to_utf32(utf16_input.data(), utf16_input.size(),
                                    utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Using native endianness, convert possibly broken UTF-16 string into
 * UTF-32 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char32_t written if
 * successful.
 */
simdutf_warn_unused result convert_utf16_to_utf32_with_errors(
    const char16_t *input, size_t length, char32_t *utf32_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf16_to_utf32_with_errors(std::span<const char16_t> utf16_input,
                                   std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf32::convert_with_errors<endianness::NATIVE>(
        utf16_input.data(), utf16_input.size(), utf32_output.data());
  } else
    #endif
  {
    return convert_utf16_to_utf32_with_errors(
        utf16_input.data(), utf16_input.size(), utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16LE string into UTF-32 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char32_t written if
 * successful.
 */
simdutf_warn_unused result convert_utf16le_to_utf32_with_errors(
    const char16_t *input, size_t length, char32_t *utf32_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf16le_to_utf32_with_errors(
    std::span<const char16_t> utf16_input,
    std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf32::convert_with_errors<endianness::LITTLE>(
        utf16_input.data(), utf16_input.size(), utf32_output.data());
  } else
    #endif
  {
    return convert_utf16le_to_utf32_with_errors(
        utf16_input.data(), utf16_input.size(), utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-16BE string into UTF-32 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char32_t written if
 * successful.
 */
simdutf_warn_unused result convert_utf16be_to_utf32_with_errors(
    const char16_t *input, size_t length, char32_t *utf32_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf16be_to_utf32_with_errors(
    std::span<const char16_t> utf16_input,
    std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf32::convert_with_errors<endianness::BIG>(
        utf16_input.data(), utf16_input.size(), utf32_output.data());
  } else
    #endif
  {
    return convert_utf16be_to_utf32_with_errors(
        utf16_input.data(), utf16_input.size(), utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Using native endianness, convert valid UTF-16 string into UTF-32 string.
 *
 * This function assumes that the input string is valid UTF-16 (native
 * endianness).
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to a buffer that can hold the conversion
 * result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16_to_utf32(
    const char16_t *input, size_t length, char32_t *utf32_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf16_to_utf32(std::span<const char16_t> valid_utf16_input,
                             std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf32::convert_valid<endianness::NATIVE>(
        valid_utf16_input.data(), valid_utf16_input.size(),
        utf32_output.data());
  } else
    #endif
  {
    return convert_valid_utf16_to_utf32(valid_utf16_input.data(),
                                        valid_utf16_input.size(),
                                        utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert valid UTF-16LE string into UTF-32 string.
 *
 * This function assumes that the input string is valid UTF-16LE.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to a buffer that can hold the conversion
 * result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16le_to_utf32(
    const char16_t *input, size_t length, char32_t *utf32_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf16le_to_utf32(std::span<const char16_t> valid_utf16_input,
                               std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf32::convert_valid<endianness::LITTLE>(
        valid_utf16_input.data(), valid_utf16_input.size(),
        utf32_output.data());
  } else
    #endif
  {
    return convert_valid_utf16le_to_utf32(valid_utf16_input.data(),
                                          valid_utf16_input.size(),
                                          utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert valid UTF-16BE string into UTF-32 string.
 *
 * This function assumes that the input string is valid UTF-16LE.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to a buffer that can hold the conversion
 * result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf16be_to_utf32(
    const char16_t *input, size_t length, char32_t *utf32_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf16be_to_utf32(std::span<const char16_t> valid_utf16_input,
                               std::span<char32_t> utf32_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16_to_utf32::convert_valid<endianness::BIG>(
        valid_utf16_input.data(), valid_utf16_input.size(),
        utf32_output.data());
  } else
    #endif
  {
    return convert_valid_utf16be_to_utf32(valid_utf16_input.data(),
                                          valid_utf16_input.size(),
                                          utf32_output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
/**
 * Using native endianness; Compute the number of bytes that this UTF-16
 * string would require in UTF-8 format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-16 strings but in such cases the result is implementation defined.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16LE string as UTF-8
 */
simdutf_warn_unused size_t utf8_length_from_utf16(const char16_t *input,
                                                  size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
utf8_length_from_utf16(std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::utf8_length_from_utf16<endianness::NATIVE>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return utf8_length_from_utf16(valid_utf16_input.data(),
                                  valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Using native endianness; compute the number of bytes that this UTF-16
 * string would require in UTF-8 format even when the UTF-16LE content contains
 * mismatched surrogates that have to be replaced by the replacement character
 * (0xFFFD).
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) where the count is the number of bytes required to
 * encode the UTF-16 string as UTF-8, and the error code is either SUCCESS or
 * SURROGATE. The count is correct regardless of the error field.
 * When SURROGATE is returned, it does not indicate an error in the case of this
 * function: it indicates that at least one surrogate has been encountered: the
 * surrogates may be matched or not (thus this function does not validate). If
 * the returned error code is SUCCESS, then the input contains no surrogate, is
 * in the Basic Multilingual Plane, and is necessarily valid.
 */
simdutf_warn_unused result utf8_length_from_utf16_with_replacement(
    const char16_t *input, size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
utf8_length_from_utf16_with_replacement(
    std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::utf8_length_from_utf16_with_replacement<
        endianness::NATIVE>(valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return utf8_length_from_utf16_with_replacement(valid_utf16_input.data(),
                                                   valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Compute the number of bytes that this UTF-16LE string would require in UTF-8
 * format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-16 strings but in such cases the result is implementation defined.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16LE string as UTF-8
 */
simdutf_warn_unused size_t utf8_length_from_utf16le(const char16_t *input,
                                                    size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_constexpr23 simdutf_warn_unused size_t
utf8_length_from_utf16le(std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::utf8_length_from_utf16<endianness::LITTLE>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return utf8_length_from_utf16le(valid_utf16_input.data(),
                                    valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Compute the number of bytes that this UTF-16BE string would require in UTF-8
 * format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-16 strings but in such cases the result is implementation defined.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16BE string as UTF-8
 */
simdutf_warn_unused size_t utf8_length_from_utf16be(const char16_t *input,
                                                    size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
utf8_length_from_utf16be(std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::utf8_length_from_utf16<endianness::BIG>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return utf8_length_from_utf16be(valid_utf16_input.data(),
                                    valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
/**
 * Convert possibly broken UTF-32 string into UTF-8 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string
 */
simdutf_warn_unused size_t convert_utf32_to_utf8(const char32_t *input,
                                                 size_t length,
                                                 char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf32_to_utf8(
    std::span<const char32_t> utf32_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf8::convert(
        utf32_input.data(), utf32_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_utf32_to_utf8(utf32_input.data(), utf32_input.size(),
                                 reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-32 string into UTF-8 string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char written if
 * successful.
 */
simdutf_warn_unused result convert_utf32_to_utf8_with_errors(
    const char32_t *input, size_t length, char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf32_to_utf8_with_errors(
    std::span<const char32_t> utf32_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf8::convert_with_errors(
        utf32_input.data(), utf32_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_utf32_to_utf8_with_errors(
        utf32_input.data(), utf32_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert valid UTF-32 string into UTF-8 string.
 *
 * This function assumes that the input string is valid UTF-32.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf8_buffer   the pointer to a buffer that can hold the conversion
 * result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf32_to_utf8(
    const char32_t *input, size_t length, char *utf8_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf32_to_utf8(
    std::span<const char32_t> valid_utf32_input,
    detail::output_span_of_byte_like auto &&utf8_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf8::convert_valid(
        valid_utf32_input.data(), valid_utf32_input.size(), utf8_output.data());
  } else
    #endif
  {
    return convert_valid_utf32_to_utf8(
        valid_utf32_input.data(), valid_utf32_input.size(),
        reinterpret_cast<char *>(utf8_output.data()));
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
/**
 * Using native endianness, convert possibly broken UTF-32 string into a UTF-16
 * string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string
 */
simdutf_warn_unused size_t convert_utf32_to_utf16(
    const char32_t *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf32_to_utf16(std::span<const char32_t> utf32_input,
                       std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf16::convert<endianness::NATIVE>(
        utf32_input.data(), utf32_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_utf32_to_utf16(utf32_input.data(), utf32_input.size(),
                                  utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-32 string into UTF-16LE string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string
 */
simdutf_warn_unused size_t convert_utf32_to_utf16le(
    const char32_t *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf32_to_utf16le(std::span<const char32_t> utf32_input,
                         std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf16::convert<endianness::LITTLE>(
        utf32_input.data(), utf32_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_utf32_to_utf16le(utf32_input.data(), utf32_input.size(),
                                    utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1
/**
 * Convert possibly broken UTF-32 string into Latin1 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string
 * or if it cannot be represented as Latin1
 */
simdutf_warn_unused size_t convert_utf32_to_latin1(
    const char32_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf32_to_latin1(
    std::span<const char32_t> utf32_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_latin1::convert(
        utf32_input.data(), utf32_input.size(), latin1_output.data());
  } else
    #endif
  {
    return convert_utf32_to_latin1(
        utf32_input.data(), utf32_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-32 string into Latin1 string and stop on error.
 * If the string cannot be represented as Latin1, an error is returned.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param latin1_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char written if
 * successful.
 */
simdutf_warn_unused result convert_utf32_to_latin1_with_errors(
    const char32_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf32_to_latin1_with_errors(
    std::span<const char32_t> utf32_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_latin1::convert_with_errors(
        utf32_input.data(), utf32_input.size(), latin1_output.data());
  } else
    #endif
  {
    return convert_utf32_to_latin1_with_errors(
        utf32_input.data(), utf32_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert valid UTF-32 string into Latin1 string.
 *
 * This function assumes that the input string is valid UTF-32 and that it can
 * be represented as Latin1. If you violate this assumption, the result is
 * implementation defined and may include system-dependent behavior such as
 * crashes.
 *
 * This function is for expert users only and not part of our public API. Use
 * convert_utf32_to_latin1 instead. The function may be removed from the library
 * in the future.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param latin1_buffer   the pointer to a buffer that can hold the conversion
 * result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf32_to_latin1(
    const char32_t *input, size_t length, char *latin1_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_constexpr23 simdutf_warn_unused size_t
convert_valid_utf32_to_latin1(
    std::span<const char32_t> valid_utf32_input,
    detail::output_span_of_byte_like auto &&latin1_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_latin1::convert_valid(
        detail::constexpr_cast_ptr<uint32_t>(valid_utf32_input.data()),
        valid_utf32_input.size(),
        detail::constexpr_cast_writeptr<char>(latin1_output.data()));
  }
    #endif
  {
    return convert_valid_utf32_to_latin1(
        valid_utf32_input.data(), valid_utf32_input.size(),
        reinterpret_cast<char *>(latin1_output.data()));
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Compute the number of bytes that this UTF-32 string would require in Latin1
 * format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-32 strings but in such cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @return the number of bytes required to encode the UTF-32 string as Latin1
 */
simdutf_warn_unused simdutf_really_inline simdutf_constexpr23 size_t
latin1_length_from_utf32(size_t length) noexcept {
  return length;
}

/**
 * Compute the number of bytes that this Latin1 string would require in UTF-32
 * format.
 *
 * @param length        the length of the string in Latin1 code units (char)
 * @return the length of the string in 4-byte code units (char32_t) required to
 * encode the Latin1 string as UTF-32
 */
simdutf_warn_unused simdutf_really_inline simdutf_constexpr23 size_t
utf32_length_from_latin1(size_t length) noexcept {
  return length;
}
#endif // SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
/**
 * Convert possibly broken UTF-32 string into UTF-16BE string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string
 */
simdutf_warn_unused size_t convert_utf32_to_utf16be(
    const char32_t *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_utf32_to_utf16be(std::span<const char32_t> utf32_input,
                         std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf16::convert<endianness::BIG>(
        utf32_input.data(), utf32_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_utf32_to_utf16be(utf32_input.data(), utf32_input.size(),
                                    utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Using native endianness, convert possibly broken UTF-32 string into UTF-16
 * string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char16_t written if
 * successful.
 */
simdutf_warn_unused result convert_utf32_to_utf16_with_errors(
    const char32_t *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf32_to_utf16_with_errors(std::span<const char32_t> utf32_input,
                                   std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf16::convert_with_errors<endianness::NATIVE>(
        utf32_input.data(), utf32_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_utf32_to_utf16_with_errors(
        utf32_input.data(), utf32_input.size(), utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-32 string into UTF-16LE string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char16_t written if
 * successful.
 */
simdutf_warn_unused result convert_utf32_to_utf16le_with_errors(
    const char32_t *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf32_to_utf16le_with_errors(
    std::span<const char32_t> utf32_input,
    std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf16::convert_with_errors<endianness::LITTLE>(
        utf32_input.data(), utf32_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_utf32_to_utf16le_with_errors(
        utf32_input.data(), utf32_input.size(), utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert possibly broken UTF-32 string into UTF-16BE string and stop on error.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of char16_t written if
 * successful.
 */
simdutf_warn_unused result convert_utf32_to_utf16be_with_errors(
    const char32_t *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
convert_utf32_to_utf16be_with_errors(
    std::span<const char32_t> utf32_input,
    std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf16::convert_with_errors<endianness::BIG>(
        utf32_input.data(), utf32_input.size(), utf16_output.data());
  } else
    #endif
  {
    return convert_utf32_to_utf16be_with_errors(
        utf32_input.data(), utf32_input.size(), utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Using native endianness, convert valid UTF-32 string into a UTF-16 string.
 *
 * This function assumes that the input string is valid UTF-32.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to a buffer that can hold the conversion
 * result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf32_to_utf16(
    const char32_t *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf32_to_utf16(std::span<const char32_t> valid_utf32_input,
                             std::span<char16_t> utf16_output) noexcept {

    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf16::convert_valid<endianness::NATIVE>(
        valid_utf32_input.data(), valid_utf32_input.size(),
        utf16_output.data());
  } else
    #endif
  {
    return convert_valid_utf32_to_utf16(valid_utf32_input.data(),
                                        valid_utf32_input.size(),
                                        utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert valid UTF-32 string into UTF-16LE string.
 *
 * This function assumes that the input string is valid UTF-32.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to a buffer that can hold the conversion
 * result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf32_to_utf16le(
    const char32_t *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf32_to_utf16le(std::span<const char32_t> valid_utf32_input,
                               std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf16::convert_valid<endianness::LITTLE>(
        valid_utf32_input.data(), valid_utf32_input.size(),
        utf16_output.data());
  } else
    #endif
  {
    return convert_valid_utf32_to_utf16le(valid_utf32_input.data(),
                                          valid_utf32_input.size(),
                                          utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert valid UTF-32 string into UTF-16BE string.
 *
 * This function assumes that the input string is valid UTF-32.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to a buffer that can hold the conversion
 * result
 * @return number of written code units; 0 if conversion is not possible
 */
simdutf_warn_unused size_t convert_valid_utf32_to_utf16be(
    const char32_t *input, size_t length, char16_t *utf16_buffer) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
convert_valid_utf32_to_utf16be(std::span<const char32_t> valid_utf32_input,
                               std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32_to_utf16::convert_valid<endianness::BIG>(
        valid_utf32_input.data(), valid_utf32_input.size(),
        utf16_output.data());
  } else
    #endif
  {
    return convert_valid_utf32_to_utf16be(valid_utf32_input.data(),
                                          valid_utf32_input.size(),
                                          utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16
/**
 * Change the endianness of the input. Can be used to go from UTF-16LE to
 * UTF-16BE or from UTF-16BE to UTF-16LE.
 *
 * This function does not validate the input.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to process
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param output        the pointer to a buffer that can hold the conversion
 * result
 */
void change_endianness_utf16(const char16_t *input, size_t length,
                             char16_t *output) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_constexpr23 void
change_endianness_utf16(std::span<const char16_t> utf16_input,
                        std::span<char16_t> utf16_output) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::change_endianness_utf16(
        utf16_input.data(), utf16_input.size(), utf16_output.data());
  } else
    #endif
  {
    return change_endianness_utf16(utf16_input.data(), utf16_input.size(),
                                   utf16_output.data());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
/**
 * Compute the number of bytes that this UTF-32 string would require in UTF-8
 * format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-32 strings but in such cases the result is implementation defined.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @return the number of bytes required to encode the UTF-32 string as UTF-8
 */
simdutf_warn_unused size_t utf8_length_from_utf32(const char32_t *input,
                                                  size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
utf8_length_from_utf32(std::span<const char32_t> valid_utf32_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32::utf8_length_from_utf32(valid_utf32_input.data(),
                                                 valid_utf32_input.size());
  } else
    #endif
  {
    return utf8_length_from_utf32(valid_utf32_input.data(),
                                  valid_utf32_input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
/**
 * Compute the number of two-byte code units that this UTF-32 string would
 * require in UTF-16 format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-32 strings but in such cases the result is implementation defined.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @return the number of bytes required to encode the UTF-32 string as UTF-16
 */
simdutf_warn_unused size_t utf16_length_from_utf32(const char32_t *input,
                                                   size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
utf16_length_from_utf32(std::span<const char32_t> valid_utf32_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf32::utf16_length_from_utf32(valid_utf32_input.data(),
                                                  valid_utf32_input.size());
  } else
    #endif
  {
    return utf16_length_from_utf32(valid_utf32_input.data(),
                                   valid_utf32_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Using native endianness; Compute the number of bytes that this UTF-16
 * string would require in UTF-32 format.
 *
 * This function is equivalent to count_utf16.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-16 strings but in such cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16LE string as UTF-32
 */
simdutf_warn_unused size_t utf32_length_from_utf16(const char16_t *input,
                                                   size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
utf32_length_from_utf16(std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::utf32_length_from_utf16<endianness::NATIVE>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return utf32_length_from_utf16(valid_utf16_input.data(),
                                   valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Compute the number of bytes that this UTF-16LE string would require in UTF-32
 * format.
 *
 * This function is equivalent to count_utf16le.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-16 strings but in such cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16LE string as UTF-32
 */
simdutf_warn_unused size_t utf32_length_from_utf16le(const char16_t *input,
                                                     size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
utf32_length_from_utf16le(
    std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::utf32_length_from_utf16<endianness::LITTLE>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return utf32_length_from_utf16le(valid_utf16_input.data(),
                                     valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Compute the number of bytes that this UTF-16BE string would require in UTF-32
 * format.
 *
 * This function is equivalent to count_utf16be.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-16 strings but in such cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16BE string as UTF-32
 */
simdutf_warn_unused size_t utf32_length_from_utf16be(const char16_t *input,
                                                     size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
utf32_length_from_utf16be(
    std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::utf32_length_from_utf16<endianness::BIG>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return utf32_length_from_utf16be(valid_utf16_input.data(),
                                     valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16
/**
 * Count the number of code points (characters) in the string assuming that
 * it is valid.
 *
 * This function assumes that the input string is valid UTF-16 (native
 * endianness). It is acceptable to pass invalid UTF-16 strings but in such
 * cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to process
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return number of code points
 */
simdutf_warn_unused size_t count_utf16(const char16_t *input,
                                       size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
count_utf16(std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::count_code_points<endianness::NATIVE>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return count_utf16(valid_utf16_input.data(), valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Count the number of code points (characters) in the string assuming that
 * it is valid.
 *
 * This function assumes that the input string is valid UTF-16LE.
 * It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16LE string to process
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return number of code points
 */
simdutf_warn_unused size_t count_utf16le(const char16_t *input,
                                         size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
count_utf16le(std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::count_code_points<endianness::LITTLE>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return count_utf16le(valid_utf16_input.data(), valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Count the number of code points (characters) in the string assuming that
 * it is valid.
 *
 * This function assumes that the input string is valid UTF-16BE.
 * It is acceptable to pass invalid UTF-16 strings but in such cases
 * the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16BE string to process
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return number of code points
 */
simdutf_warn_unused size_t count_utf16be(const char16_t *input,
                                         size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
count_utf16be(std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::count_code_points<endianness::BIG>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return count_utf16be(valid_utf16_input.data(), valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8
/**
 * Count the number of code points (characters) in the string assuming that
 * it is valid.
 *
 * This function assumes that the input string is valid UTF-8.
 * It is acceptable to pass invalid UTF-8 strings but in such cases
 * the result is implementation defined.
 *
 * @param input         the UTF-8 string to process
 * @param length        the length of the string in bytes
 * @return number of code points
 */
simdutf_warn_unused size_t count_utf8(const char *input,
                                      size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t count_utf8(
    const detail::input_span_of_byte_like auto &valid_utf8_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8::count_code_points(valid_utf8_input.data(),
                                           valid_utf8_input.size());
  } else
    #endif
  {
    return count_utf8(reinterpret_cast<const char *>(valid_utf8_input.data()),
                      valid_utf8_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Given a valid UTF-8 string having a possibly truncated last character,
 * this function checks the end of string. If the last character is truncated
 * (or partial), then it returns a shorter length (shorter by 1 to 3 bytes) so
 * that the short UTF-8 strings only contain complete characters. If there is no
 * truncated character, the original length is returned.
 *
 * This function assumes that the input string is valid UTF-8, but possibly
 * truncated.
 *
 * @param input         the UTF-8 string to process
 * @param length        the length of the string in bytes
 * @return the length of the string in bytes, possibly shorter by 1 to 3 bytes
 */
simdutf_warn_unused size_t trim_partial_utf8(const char *input, size_t length);
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
trim_partial_utf8(
    const detail::input_span_of_byte_like auto &valid_utf8_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf8::trim_partial_utf8(valid_utf8_input.data(),
                                           valid_utf8_input.size());
  } else
    #endif
  {
    return trim_partial_utf8(
        reinterpret_cast<const char *>(valid_utf8_input.data()),
        valid_utf8_input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF8

#if SIMDUTF_FEATURE_UTF16
/**
 * Given a valid UTF-16BE string having a possibly truncated last character,
 * this function checks the end of string. If the last character is truncated
 * (or partial), then it returns a shorter length (shorter by 1 unit) so that
 * the short UTF-16BE strings only contain complete characters. If there is no
 * truncated character, the original length is returned.
 *
 * This function assumes that the input string is valid UTF-16BE, but possibly
 * truncated.
 *
 * @param input         the UTF-16BE string to process
 * @param length        the length of the string in bytes
 * @return the length of the string in bytes, possibly shorter by 1 unit
 */
simdutf_warn_unused size_t trim_partial_utf16be(const char16_t *input,
                                                size_t length);
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
trim_partial_utf16be(std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::trim_partial_utf16<endianness::BIG>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return trim_partial_utf16be(valid_utf16_input.data(),
                                valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Given a valid UTF-16LE string having a possibly truncated last character,
 * this function checks the end of string. If the last character is truncated
 * (or partial), then it returns a shorter length (shorter by 1 unit) so that
 * the short UTF-16LE strings only contain complete characters. If there is no
 * truncated character, the original length is returned.
 *
 * This function assumes that the input string is valid UTF-16LE, but possibly
 * truncated.
 *
 * @param input         the UTF-16LE string to process
 * @param length        the length of the string in bytes
 * @return the length of the string in unit, possibly shorter by 1 unit
 */
simdutf_warn_unused size_t trim_partial_utf16le(const char16_t *input,
                                                size_t length);
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
trim_partial_utf16le(std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::trim_partial_utf16<endianness::LITTLE>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return trim_partial_utf16le(valid_utf16_input.data(),
                                valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Given a valid UTF-16 string having a possibly truncated last character,
 * this function checks the end of string. If the last character is truncated
 * (or partial), then it returns a shorter length (shorter by 1 unit) so that
 * the short UTF-16 strings only contain complete characters. If there is no
 * truncated character, the original length is returned.
 *
 * This function assumes that the input string is valid UTF-16, but possibly
 * truncated. We use the native endianness.
 *
 * @param input         the UTF-16 string to process
 * @param length        the length of the string in bytes
 * @return the length of the string in unit, possibly shorter by 1 unit
 */
simdutf_warn_unused size_t trim_partial_utf16(const char16_t *input,
                                              size_t length);
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
trim_partial_utf16(std::span<const char16_t> valid_utf16_input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::utf16::trim_partial_utf16<endianness::NATIVE>(
        valid_utf16_input.data(), valid_utf16_input.size());
  } else
    #endif
  {
    return trim_partial_utf16(valid_utf16_input.data(),
                              valid_utf16_input.size());
  }
}
  #endif // SIMDUTF_SPAN
#endif   // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_BASE64 || SIMDUTF_FEATURE_UTF16 ||                         \
    SIMDUTF_FEATURE_DETECT_ENCODING
  #ifndef SIMDUTF_NEED_TRAILING_ZEROES
    #define SIMDUTF_NEED_TRAILING_ZEROES 1
  #endif
#endif // SIMDUTF_FEATURE_BASE64 || SIMDUTF_FEATURE_UTF16 ||
       // SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_BASE64
// base64_options are used to specify the base64 encoding options.
// ASCII spaces are ' ', '\t', '\n', '\r', '\f'
// garbage characters are characters that are not part of the base64 alphabet
// nor ASCII spaces.
constexpr uint64_t base64_reverse_padding =
    2; /* modifier for base64_default and base64_url */
enum base64_options : uint64_t {
  base64_default = 0, /* standard base64 format (with padding) */
  base64_url = 1,     /* base64url format (no padding) */
  base64_default_no_padding =
      base64_default |
      base64_reverse_padding, /* standard base64 format without padding */
  base64_url_with_padding =
      base64_url | base64_reverse_padding, /* base64url with padding */
  base64_default_accept_garbage =
      4, /* standard base64 format accepting garbage characters, the input stops
            with the first '=' if any */
  base64_url_accept_garbage =
      5, /* base64url format accepting garbage characters, the input stops with
            the first '=' if any */
  base64_default_or_url =
      8, /* standard/base64url hybrid format (only meaningful for decoding!) */
  base64_default_or_url_accept_garbage =
      12, /* standard/base64url hybrid format accepting garbage characters
             (only meaningful for decoding!), the input stops with the first '='
             if any */
};

// last_chunk_handling_options are used to specify the handling of the last
// chunk in base64 decoding.
// https://tc39.es/proposal-arraybuffer-base64/spec/#sec-frombase64
enum last_chunk_handling_options : uint64_t {
  loose = 0,  /* standard base64 format, decode partial final chunk */
  strict = 1, /* error when the last chunk is partial, 2 or 3 chars, and
                 unpadded, or non-zero bit padding */
  stop_before_partial =
      2, /* if the last chunk is partial, ignore it (no error) */
  only_full_chunks =
      3 /* only decode full blocks (4 base64 characters, no padding) */
};

inline simdutf_constexpr23 bool
is_partial(last_chunk_handling_options options) {
  return (options == stop_before_partial) || (options == only_full_chunks);
}

namespace detail {
simdutf_warn_unused const char *find(const char *start, const char *end,
                                     char character) noexcept;
simdutf_warn_unused const char16_t *
find(const char16_t *start, const char16_t *end, char16_t character) noexcept;
} // namespace detail

/**
 * Find the first occurrence of a character in a string. If the character is
 * not found, return a pointer to the end of the string.
 * @param start        the start of the string
 * @param end          the end of the string
 * @param character    the character to find
 * @return a pointer to the first occurrence of the character in the string,
 * or a pointer to the end of the string if the character is not found.
 *
 */
simdutf_warn_unused simdutf_really_inline simdutf_constexpr23 const char *
find(const char *start, const char *end, char character) noexcept {
  #if SIMDUTF_CPLUSPLUS23
  if consteval {
    for (; start != end; ++start)
      if (*start == character)
        return start;
    return end;
  } else
  #endif
  {
    return detail::find(start, end, character);
  }
}
simdutf_warn_unused simdutf_really_inline simdutf_constexpr23 const char16_t *
find(const char16_t *start, const char16_t *end, char16_t character) noexcept {
    // implementation note: this is repeated instead of a template, to ensure
    // the api is still a function and compiles without concepts
  #if SIMDUTF_CPLUSPLUS23
  if consteval {
    for (; start != end; ++start)
      if (*start == character)
        return start;
    return end;
  } else
  #endif
  {
    return detail::find(start, end, character);
  }
}
}
  // We include base64_tables once.
/* begin file include/simdutf/base64_tables.h */
#ifndef SIMDUTF_BASE64_TABLES_H
#define SIMDUTF_BASE64_TABLES_H
#include <cstdint>

namespace simdutf {
namespace {
namespace tables {
namespace base64 {
namespace base64_default {

constexpr char e0[256] = {
    'A', 'A', 'A', 'A', 'B', 'B', 'B', 'B', 'C', 'C', 'C', 'C', 'D', 'D', 'D',
    'D', 'E', 'E', 'E', 'E', 'F', 'F', 'F', 'F', 'G', 'G', 'G', 'G', 'H', 'H',
    'H', 'H', 'I', 'I', 'I', 'I', 'J', 'J', 'J', 'J', 'K', 'K', 'K', 'K', 'L',
    'L', 'L', 'L', 'M', 'M', 'M', 'M', 'N', 'N', 'N', 'N', 'O', 'O', 'O', 'O',
    'P', 'P', 'P', 'P', 'Q', 'Q', 'Q', 'Q', 'R', 'R', 'R', 'R', 'S', 'S', 'S',
    'S', 'T', 'T', 'T', 'T', 'U', 'U', 'U', 'U', 'V', 'V', 'V', 'V', 'W', 'W',
    'W', 'W', 'X', 'X', 'X', 'X', 'Y', 'Y', 'Y', 'Y', 'Z', 'Z', 'Z', 'Z', 'a',
    'a', 'a', 'a', 'b', 'b', 'b', 'b', 'c', 'c', 'c', 'c', 'd', 'd', 'd', 'd',
    'e', 'e', 'e', 'e', 'f', 'f', 'f', 'f', 'g', 'g', 'g', 'g', 'h', 'h', 'h',
    'h', 'i', 'i', 'i', 'i', 'j', 'j', 'j', 'j', 'k', 'k', 'k', 'k', 'l', 'l',
    'l', 'l', 'm', 'm', 'm', 'm', 'n', 'n', 'n', 'n', 'o', 'o', 'o', 'o', 'p',
    'p', 'p', 'p', 'q', 'q', 'q', 'q', 'r', 'r', 'r', 'r', 's', 's', 's', 's',
    't', 't', 't', 't', 'u', 'u', 'u', 'u', 'v', 'v', 'v', 'v', 'w', 'w', 'w',
    'w', 'x', 'x', 'x', 'x', 'y', 'y', 'y', 'y', 'z', 'z', 'z', 'z', '0', '0',
    '0', '0', '1', '1', '1', '1', '2', '2', '2', '2', '3', '3', '3', '3', '4',
    '4', '4', '4', '5', '5', '5', '5', '6', '6', '6', '6', '7', '7', '7', '7',
    '8', '8', '8', '8', '9', '9', '9', '9', '+', '+', '+', '+', '/', '/', '/',
    '/'};

constexpr char e1[256] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
    't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '+', '/', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
    'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
    'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', 'A', 'B', 'C',
    'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
    'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+',
    '/'};

constexpr char e2[256] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
    't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '+', '/', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
    'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
    'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', 'A', 'B', 'C',
    'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
    'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+',
    '/'};

constexpr uint32_t d0[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x000000f8, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x000000fc,
    0x000000d0, 0x000000d4, 0x000000d8, 0x000000dc, 0x000000e0, 0x000000e4,
    0x000000e8, 0x000000ec, 0x000000f0, 0x000000f4, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00000004, 0x00000008, 0x0000000c, 0x00000010, 0x00000014, 0x00000018,
    0x0000001c, 0x00000020, 0x00000024, 0x00000028, 0x0000002c, 0x00000030,
    0x00000034, 0x00000038, 0x0000003c, 0x00000040, 0x00000044, 0x00000048,
    0x0000004c, 0x00000050, 0x00000054, 0x00000058, 0x0000005c, 0x00000060,
    0x00000064, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x00000068, 0x0000006c, 0x00000070, 0x00000074, 0x00000078,
    0x0000007c, 0x00000080, 0x00000084, 0x00000088, 0x0000008c, 0x00000090,
    0x00000094, 0x00000098, 0x0000009c, 0x000000a0, 0x000000a4, 0x000000a8,
    0x000000ac, 0x000000b0, 0x000000b4, 0x000000b8, 0x000000bc, 0x000000c0,
    0x000000c4, 0x000000c8, 0x000000cc, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};

constexpr uint32_t d1[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x0000e003, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x0000f003,
    0x00004003, 0x00005003, 0x00006003, 0x00007003, 0x00008003, 0x00009003,
    0x0000a003, 0x0000b003, 0x0000c003, 0x0000d003, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00001000, 0x00002000, 0x00003000, 0x00004000, 0x00005000, 0x00006000,
    0x00007000, 0x00008000, 0x00009000, 0x0000a000, 0x0000b000, 0x0000c000,
    0x0000d000, 0x0000e000, 0x0000f000, 0x00000001, 0x00001001, 0x00002001,
    0x00003001, 0x00004001, 0x00005001, 0x00006001, 0x00007001, 0x00008001,
    0x00009001, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x0000a001, 0x0000b001, 0x0000c001, 0x0000d001, 0x0000e001,
    0x0000f001, 0x00000002, 0x00001002, 0x00002002, 0x00003002, 0x00004002,
    0x00005002, 0x00006002, 0x00007002, 0x00008002, 0x00009002, 0x0000a002,
    0x0000b002, 0x0000c002, 0x0000d002, 0x0000e002, 0x0000f002, 0x00000003,
    0x00001003, 0x00002003, 0x00003003, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};

constexpr uint32_t d2[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x00800f00, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00c00f00,
    0x00000d00, 0x00400d00, 0x00800d00, 0x00c00d00, 0x00000e00, 0x00400e00,
    0x00800e00, 0x00c00e00, 0x00000f00, 0x00400f00, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00400000, 0x00800000, 0x00c00000, 0x00000100, 0x00400100, 0x00800100,
    0x00c00100, 0x00000200, 0x00400200, 0x00800200, 0x00c00200, 0x00000300,
    0x00400300, 0x00800300, 0x00c00300, 0x00000400, 0x00400400, 0x00800400,
    0x00c00400, 0x00000500, 0x00400500, 0x00800500, 0x00c00500, 0x00000600,
    0x00400600, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x00800600, 0x00c00600, 0x00000700, 0x00400700, 0x00800700,
    0x00c00700, 0x00000800, 0x00400800, 0x00800800, 0x00c00800, 0x00000900,
    0x00400900, 0x00800900, 0x00c00900, 0x00000a00, 0x00400a00, 0x00800a00,
    0x00c00a00, 0x00000b00, 0x00400b00, 0x00800b00, 0x00c00b00, 0x00000c00,
    0x00400c00, 0x00800c00, 0x00c00c00, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};

constexpr uint32_t d3[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x003e0000, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x003f0000,
    0x00340000, 0x00350000, 0x00360000, 0x00370000, 0x00380000, 0x00390000,
    0x003a0000, 0x003b0000, 0x003c0000, 0x003d0000, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00010000, 0x00020000, 0x00030000, 0x00040000, 0x00050000, 0x00060000,
    0x00070000, 0x00080000, 0x00090000, 0x000a0000, 0x000b0000, 0x000c0000,
    0x000d0000, 0x000e0000, 0x000f0000, 0x00100000, 0x00110000, 0x00120000,
    0x00130000, 0x00140000, 0x00150000, 0x00160000, 0x00170000, 0x00180000,
    0x00190000, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x001a0000, 0x001b0000, 0x001c0000, 0x001d0000, 0x001e0000,
    0x001f0000, 0x00200000, 0x00210000, 0x00220000, 0x00230000, 0x00240000,
    0x00250000, 0x00260000, 0x00270000, 0x00280000, 0x00290000, 0x002a0000,
    0x002b0000, 0x002c0000, 0x002d0000, 0x002e0000, 0x002f0000, 0x00300000,
    0x00310000, 0x00320000, 0x00330000, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};
} // namespace base64_default

namespace base64_url {

constexpr char e0[256] = {
    'A', 'A', 'A', 'A', 'B', 'B', 'B', 'B', 'C', 'C', 'C', 'C', 'D', 'D', 'D',
    'D', 'E', 'E', 'E', 'E', 'F', 'F', 'F', 'F', 'G', 'G', 'G', 'G', 'H', 'H',
    'H', 'H', 'I', 'I', 'I', 'I', 'J', 'J', 'J', 'J', 'K', 'K', 'K', 'K', 'L',
    'L', 'L', 'L', 'M', 'M', 'M', 'M', 'N', 'N', 'N', 'N', 'O', 'O', 'O', 'O',
    'P', 'P', 'P', 'P', 'Q', 'Q', 'Q', 'Q', 'R', 'R', 'R', 'R', 'S', 'S', 'S',
    'S', 'T', 'T', 'T', 'T', 'U', 'U', 'U', 'U', 'V', 'V', 'V', 'V', 'W', 'W',
    'W', 'W', 'X', 'X', 'X', 'X', 'Y', 'Y', 'Y', 'Y', 'Z', 'Z', 'Z', 'Z', 'a',
    'a', 'a', 'a', 'b', 'b', 'b', 'b', 'c', 'c', 'c', 'c', 'd', 'd', 'd', 'd',
    'e', 'e', 'e', 'e', 'f', 'f', 'f', 'f', 'g', 'g', 'g', 'g', 'h', 'h', 'h',
    'h', 'i', 'i', 'i', 'i', 'j', 'j', 'j', 'j', 'k', 'k', 'k', 'k', 'l', 'l',
    'l', 'l', 'm', 'm', 'm', 'm', 'n', 'n', 'n', 'n', 'o', 'o', 'o', 'o', 'p',
    'p', 'p', 'p', 'q', 'q', 'q', 'q', 'r', 'r', 'r', 'r', 's', 's', 's', 's',
    't', 't', 't', 't', 'u', 'u', 'u', 'u', 'v', 'v', 'v', 'v', 'w', 'w', 'w',
    'w', 'x', 'x', 'x', 'x', 'y', 'y', 'y', 'y', 'z', 'z', 'z', 'z', '0', '0',
    '0', '0', '1', '1', '1', '1', '2', '2', '2', '2', '3', '3', '3', '3', '4',
    '4', '4', '4', '5', '5', '5', '5', '6', '6', '6', '6', '7', '7', '7', '7',
    '8', '8', '8', '8', '9', '9', '9', '9', '-', '-', '-', '-', '_', '_', '_',
    '_'};

constexpr char e1[256] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
    't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '-', '_', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
    'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '-', '_', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
    'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_', 'A', 'B', 'C',
    'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
    'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-',
    '_'};

constexpr char e2[256] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
    't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '-', '_', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
    'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '-', '_', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
    'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_', 'A', 'B', 'C',
    'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
    'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-',
    '_'};

constexpr uint32_t d0[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x000000f8, 0x01ffffff, 0x01ffffff,
    0x000000d0, 0x000000d4, 0x000000d8, 0x000000dc, 0x000000e0, 0x000000e4,
    0x000000e8, 0x000000ec, 0x000000f0, 0x000000f4, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00000004, 0x00000008, 0x0000000c, 0x00000010, 0x00000014, 0x00000018,
    0x0000001c, 0x00000020, 0x00000024, 0x00000028, 0x0000002c, 0x00000030,
    0x00000034, 0x00000038, 0x0000003c, 0x00000040, 0x00000044, 0x00000048,
    0x0000004c, 0x00000050, 0x00000054, 0x00000058, 0x0000005c, 0x00000060,
    0x00000064, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x000000fc,
    0x01ffffff, 0x00000068, 0x0000006c, 0x00000070, 0x00000074, 0x00000078,
    0x0000007c, 0x00000080, 0x00000084, 0x00000088, 0x0000008c, 0x00000090,
    0x00000094, 0x00000098, 0x0000009c, 0x000000a0, 0x000000a4, 0x000000a8,
    0x000000ac, 0x000000b0, 0x000000b4, 0x000000b8, 0x000000bc, 0x000000c0,
    0x000000c4, 0x000000c8, 0x000000cc, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};
constexpr uint32_t d1[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x0000e003, 0x01ffffff, 0x01ffffff,
    0x00004003, 0x00005003, 0x00006003, 0x00007003, 0x00008003, 0x00009003,
    0x0000a003, 0x0000b003, 0x0000c003, 0x0000d003, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00001000, 0x00002000, 0x00003000, 0x00004000, 0x00005000, 0x00006000,
    0x00007000, 0x00008000, 0x00009000, 0x0000a000, 0x0000b000, 0x0000c000,
    0x0000d000, 0x0000e000, 0x0000f000, 0x00000001, 0x00001001, 0x00002001,
    0x00003001, 0x00004001, 0x00005001, 0x00006001, 0x00007001, 0x00008001,
    0x00009001, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x0000f003,
    0x01ffffff, 0x0000a001, 0x0000b001, 0x0000c001, 0x0000d001, 0x0000e001,
    0x0000f001, 0x00000002, 0x00001002, 0x00002002, 0x00003002, 0x00004002,
    0x00005002, 0x00006002, 0x00007002, 0x00008002, 0x00009002, 0x0000a002,
    0x0000b002, 0x0000c002, 0x0000d002, 0x0000e002, 0x0000f002, 0x00000003,
    0x00001003, 0x00002003, 0x00003003, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};
constexpr uint32_t d2[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00800f00, 0x01ffffff, 0x01ffffff,
    0x00000d00, 0x00400d00, 0x00800d00, 0x00c00d00, 0x00000e00, 0x00400e00,
    0x00800e00, 0x00c00e00, 0x00000f00, 0x00400f00, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00400000, 0x00800000, 0x00c00000, 0x00000100, 0x00400100, 0x00800100,
    0x00c00100, 0x00000200, 0x00400200, 0x00800200, 0x00c00200, 0x00000300,
    0x00400300, 0x00800300, 0x00c00300, 0x00000400, 0x00400400, 0x00800400,
    0x00c00400, 0x00000500, 0x00400500, 0x00800500, 0x00c00500, 0x00000600,
    0x00400600, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00c00f00,
    0x01ffffff, 0x00800600, 0x00c00600, 0x00000700, 0x00400700, 0x00800700,
    0x00c00700, 0x00000800, 0x00400800, 0x00800800, 0x00c00800, 0x00000900,
    0x00400900, 0x00800900, 0x00c00900, 0x00000a00, 0x00400a00, 0x00800a00,
    0x00c00a00, 0x00000b00, 0x00400b00, 0x00800b00, 0x00c00b00, 0x00000c00,
    0x00400c00, 0x00800c00, 0x00c00c00, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};
constexpr uint32_t d3[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x003e0000, 0x01ffffff, 0x01ffffff,
    0x00340000, 0x00350000, 0x00360000, 0x00370000, 0x00380000, 0x00390000,
    0x003a0000, 0x003b0000, 0x003c0000, 0x003d0000, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00010000, 0x00020000, 0x00030000, 0x00040000, 0x00050000, 0x00060000,
    0x00070000, 0x00080000, 0x00090000, 0x000a0000, 0x000b0000, 0x000c0000,
    0x000d0000, 0x000e0000, 0x000f0000, 0x00100000, 0x00110000, 0x00120000,
    0x00130000, 0x00140000, 0x00150000, 0x00160000, 0x00170000, 0x00180000,
    0x00190000, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x003f0000,
    0x01ffffff, 0x001a0000, 0x001b0000, 0x001c0000, 0x001d0000, 0x001e0000,
    0x001f0000, 0x00200000, 0x00210000, 0x00220000, 0x00230000, 0x00240000,
    0x00250000, 0x00260000, 0x00270000, 0x00280000, 0x00290000, 0x002a0000,
    0x002b0000, 0x002c0000, 0x002d0000, 0x002e0000, 0x002f0000, 0x00300000,
    0x00310000, 0x00320000, 0x00330000, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};
} // namespace base64_url

namespace base64_default_or_url {
constexpr uint32_t d0[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x000000f8, 0x01ffffff, 0x000000f8, 0x01ffffff, 0x000000fc,
    0x000000d0, 0x000000d4, 0x000000d8, 0x000000dc, 0x000000e0, 0x000000e4,
    0x000000e8, 0x000000ec, 0x000000f0, 0x000000f4, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00000004, 0x00000008, 0x0000000c, 0x00000010, 0x00000014, 0x00000018,
    0x0000001c, 0x00000020, 0x00000024, 0x00000028, 0x0000002c, 0x00000030,
    0x00000034, 0x00000038, 0x0000003c, 0x00000040, 0x00000044, 0x00000048,
    0x0000004c, 0x00000050, 0x00000054, 0x00000058, 0x0000005c, 0x00000060,
    0x00000064, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x000000fc,
    0x01ffffff, 0x00000068, 0x0000006c, 0x00000070, 0x00000074, 0x00000078,
    0x0000007c, 0x00000080, 0x00000084, 0x00000088, 0x0000008c, 0x00000090,
    0x00000094, 0x00000098, 0x0000009c, 0x000000a0, 0x000000a4, 0x000000a8,
    0x000000ac, 0x000000b0, 0x000000b4, 0x000000b8, 0x000000bc, 0x000000c0,
    0x000000c4, 0x000000c8, 0x000000cc, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};
constexpr uint32_t d1[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x0000e003, 0x01ffffff, 0x0000e003, 0x01ffffff, 0x0000f003,
    0x00004003, 0x00005003, 0x00006003, 0x00007003, 0x00008003, 0x00009003,
    0x0000a003, 0x0000b003, 0x0000c003, 0x0000d003, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00001000, 0x00002000, 0x00003000, 0x00004000, 0x00005000, 0x00006000,
    0x00007000, 0x00008000, 0x00009000, 0x0000a000, 0x0000b000, 0x0000c000,
    0x0000d000, 0x0000e000, 0x0000f000, 0x00000001, 0x00001001, 0x00002001,
    0x00003001, 0x00004001, 0x00005001, 0x00006001, 0x00007001, 0x00008001,
    0x00009001, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x0000f003,
    0x01ffffff, 0x0000a001, 0x0000b001, 0x0000c001, 0x0000d001, 0x0000e001,
    0x0000f001, 0x00000002, 0x00001002, 0x00002002, 0x00003002, 0x00004002,
    0x00005002, 0x00006002, 0x00007002, 0x00008002, 0x00009002, 0x0000a002,
    0x0000b002, 0x0000c002, 0x0000d002, 0x0000e002, 0x0000f002, 0x00000003,
    0x00001003, 0x00002003, 0x00003003, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};
constexpr uint32_t d2[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x00800f00, 0x01ffffff, 0x00800f00, 0x01ffffff, 0x00c00f00,
    0x00000d00, 0x00400d00, 0x00800d00, 0x00c00d00, 0x00000e00, 0x00400e00,
    0x00800e00, 0x00c00e00, 0x00000f00, 0x00400f00, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00400000, 0x00800000, 0x00c00000, 0x00000100, 0x00400100, 0x00800100,
    0x00c00100, 0x00000200, 0x00400200, 0x00800200, 0x00c00200, 0x00000300,
    0x00400300, 0x00800300, 0x00c00300, 0x00000400, 0x00400400, 0x00800400,
    0x00c00400, 0x00000500, 0x00400500, 0x00800500, 0x00c00500, 0x00000600,
    0x00400600, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00c00f00,
    0x01ffffff, 0x00800600, 0x00c00600, 0x00000700, 0x00400700, 0x00800700,
    0x00c00700, 0x00000800, 0x00400800, 0x00800800, 0x00c00800, 0x00000900,
    0x00400900, 0x00800900, 0x00c00900, 0x00000a00, 0x00400a00, 0x00800a00,
    0x00c00a00, 0x00000b00, 0x00400b00, 0x00800b00, 0x00c00b00, 0x00000c00,
    0x00400c00, 0x00800c00, 0x00c00c00, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};
constexpr uint32_t d3[256] = {
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x003e0000, 0x01ffffff, 0x003e0000, 0x01ffffff, 0x003f0000,
    0x00340000, 0x00350000, 0x00360000, 0x00370000, 0x00380000, 0x00390000,
    0x003a0000, 0x003b0000, 0x003c0000, 0x003d0000, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x00000000,
    0x00010000, 0x00020000, 0x00030000, 0x00040000, 0x00050000, 0x00060000,
    0x00070000, 0x00080000, 0x00090000, 0x000a0000, 0x000b0000, 0x000c0000,
    0x000d0000, 0x000e0000, 0x000f0000, 0x00100000, 0x00110000, 0x00120000,
    0x00130000, 0x00140000, 0x00150000, 0x00160000, 0x00170000, 0x00180000,
    0x00190000, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x003f0000,
    0x01ffffff, 0x001a0000, 0x001b0000, 0x001c0000, 0x001d0000, 0x001e0000,
    0x001f0000, 0x00200000, 0x00210000, 0x00220000, 0x00230000, 0x00240000,
    0x00250000, 0x00260000, 0x00270000, 0x00280000, 0x00290000, 0x002a0000,
    0x002b0000, 0x002c0000, 0x002d0000, 0x002e0000, 0x002f0000, 0x00300000,
    0x00310000, 0x00320000, 0x00330000, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff,
    0x01ffffff, 0x01ffffff, 0x01ffffff, 0x01ffffff};
} // namespace base64_default_or_url
constexpr uint64_t thintable_epi8[256] = {
    0x0706050403020100, 0x0007060504030201, 0x0007060504030200,
    0x0000070605040302, 0x0007060504030100, 0x0000070605040301,
    0x0000070605040300, 0x0000000706050403, 0x0007060504020100,
    0x0000070605040201, 0x0000070605040200, 0x0000000706050402,
    0x0000070605040100, 0x0000000706050401, 0x0000000706050400,
    0x0000000007060504, 0x0007060503020100, 0x0000070605030201,
    0x0000070605030200, 0x0000000706050302, 0x0000070605030100,
    0x0000000706050301, 0x0000000706050300, 0x0000000007060503,
    0x0000070605020100, 0x0000000706050201, 0x0000000706050200,
    0x0000000007060502, 0x0000000706050100, 0x0000000007060501,
    0x0000000007060500, 0x0000000000070605, 0x0007060403020100,
    0x0000070604030201, 0x0000070604030200, 0x0000000706040302,
    0x0000070604030100, 0x0000000706040301, 0x0000000706040300,
    0x0000000007060403, 0x0000070604020100, 0x0000000706040201,
    0x0000000706040200, 0x0000000007060402, 0x0000000706040100,
    0x0000000007060401, 0x0000000007060400, 0x0000000000070604,
    0x0000070603020100, 0x0000000706030201, 0x0000000706030200,
    0x0000000007060302, 0x0000000706030100, 0x0000000007060301,
    0x0000000007060300, 0x0000000000070603, 0x0000000706020100,
    0x0000000007060201, 0x0000000007060200, 0x0000000000070602,
    0x0000000007060100, 0x0000000000070601, 0x0000000000070600,
    0x0000000000000706, 0x0007050403020100, 0x0000070504030201,
    0x0000070504030200, 0x0000000705040302, 0x0000070504030100,
    0x0000000705040301, 0x0000000705040300, 0x0000000007050403,
    0x0000070504020100, 0x0000000705040201, 0x0000000705040200,
    0x0000000007050402, 0x0000000705040100, 0x0000000007050401,
    0x0000000007050400, 0x0000000000070504, 0x0000070503020100,
    0x0000000705030201, 0x0000000705030200, 0x0000000007050302,
    0x0000000705030100, 0x0000000007050301, 0x0000000007050300,
    0x0000000000070503, 0x0000000705020100, 0x0000000007050201,
    0x0000000007050200, 0x0000000000070502, 0x0000000007050100,
    0x0000000000070501, 0x0000000000070500, 0x0000000000000705,
    0x0000070403020100, 0x0000000704030201, 0x0000000704030200,
    0x0000000007040302, 0x0000000704030100, 0x0000000007040301,
    0x0000000007040300, 0x0000000000070403, 0x0000000704020100,
    0x0000000007040201, 0x0000000007040200, 0x0000000000070402,
    0x0000000007040100, 0x0000000000070401, 0x0000000000070400,
    0x0000000000000704, 0x0000000703020100, 0x0000000007030201,
    0x0000000007030200, 0x0000000000070302, 0x0000000007030100,
    0x0000000000070301, 0x0000000000070300, 0x0000000000000703,
    0x0000000007020100, 0x0000000000070201, 0x0000000000070200,
    0x0000000000000702, 0x0000000000070100, 0x0000000000000701,
    0x0000000000000700, 0x0000000000000007, 0x0006050403020100,
    0x0000060504030201, 0x0000060504030200, 0x0000000605040302,
    0x0000060504030100, 0x0000000605040301, 0x0000000605040300,
    0x0000000006050403, 0x0000060504020100, 0x0000000605040201,
    0x0000000605040200, 0x0000000006050402, 0x0000000605040100,
    0x0000000006050401, 0x0000000006050400, 0x0000000000060504,
    0x0000060503020100, 0x0000000605030201, 0x0000000605030200,
    0x0000000006050302, 0x0000000605030100, 0x0000000006050301,
    0x0000000006050300, 0x0000000000060503, 0x0000000605020100,
    0x0000000006050201, 0x0000000006050200, 0x0000000000060502,
    0x0000000006050100, 0x0000000000060501, 0x0000000000060500,
    0x0000000000000605, 0x0000060403020100, 0x0000000604030201,
    0x0000000604030200, 0x0000000006040302, 0x0000000604030100,
    0x0000000006040301, 0x0000000006040300, 0x0000000000060403,
    0x0000000604020100, 0x0000000006040201, 0x0000000006040200,
    0x0000000000060402, 0x0000000006040100, 0x0000000000060401,
    0x0000000000060400, 0x0000000000000604, 0x0000000603020100,
    0x0000000006030201, 0x0000000006030200, 0x0000000000060302,
    0x0000000006030100, 0x0000000000060301, 0x0000000000060300,
    0x0000000000000603, 0x0000000006020100, 0x0000000000060201,
    0x0000000000060200, 0x0000000000000602, 0x0000000000060100,
    0x0000000000000601, 0x0000000000000600, 0x0000000000000006,
    0x0000050403020100, 0x0000000504030201, 0x0000000504030200,
    0x0000000005040302, 0x0000000504030100, 0x0000000005040301,
    0x0000000005040300, 0x0000000000050403, 0x0000000504020100,
    0x0000000005040201, 0x0000000005040200, 0x0000000000050402,
    0x0000000005040100, 0x0000000000050401, 0x0000000000050400,
    0x0000000000000504, 0x0000000503020100, 0x0000000005030201,
    0x0000000005030200, 0x0000000000050302, 0x0000000005030100,
    0x0000000000050301, 0x0000000000050300, 0x0000000000000503,
    0x0000000005020100, 0x0000000000050201, 0x0000000000050200,
    0x0000000000000502, 0x0000000000050100, 0x0000000000000501,
    0x0000000000000500, 0x0000000000000005, 0x0000000403020100,
    0x0000000004030201, 0x0000000004030200, 0x0000000000040302,
    0x0000000004030100, 0x0000000000040301, 0x0000000000040300,
    0x0000000000000403, 0x0000000004020100, 0x0000000000040201,
    0x0000000000040200, 0x0000000000000402, 0x0000000000040100,
    0x0000000000000401, 0x0000000000000400, 0x0000000000000004,
    0x0000000003020100, 0x0000000000030201, 0x0000000000030200,
    0x0000000000000302, 0x0000000000030100, 0x0000000000000301,
    0x0000000000000300, 0x0000000000000003, 0x0000000000020100,
    0x0000000000000201, 0x0000000000000200, 0x0000000000000002,
    0x0000000000000100, 0x0000000000000001, 0x0000000000000000,
    0x0000000000000000,
};

constexpr uint8_t pshufb_combine_table[272] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x01, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

constexpr unsigned char BitsSetTable256mul2[256] = {
    0,  2,  2,  4,  2,  4,  4,  6,  2,  4,  4,  6,  4,  6,  6,  8,  2,  4,  4,
    6,  4,  6,  6,  8,  4,  6,  6,  8,  6,  8,  8,  10, 2,  4,  4,  6,  4,  6,
    6,  8,  4,  6,  6,  8,  6,  8,  8,  10, 4,  6,  6,  8,  6,  8,  8,  10, 6,
    8,  8,  10, 8,  10, 10, 12, 2,  4,  4,  6,  4,  6,  6,  8,  4,  6,  6,  8,
    6,  8,  8,  10, 4,  6,  6,  8,  6,  8,  8,  10, 6,  8,  8,  10, 8,  10, 10,
    12, 4,  6,  6,  8,  6,  8,  8,  10, 6,  8,  8,  10, 8,  10, 10, 12, 6,  8,
    8,  10, 8,  10, 10, 12, 8,  10, 10, 12, 10, 12, 12, 14, 2,  4,  4,  6,  4,
    6,  6,  8,  4,  6,  6,  8,  6,  8,  8,  10, 4,  6,  6,  8,  6,  8,  8,  10,
    6,  8,  8,  10, 8,  10, 10, 12, 4,  6,  6,  8,  6,  8,  8,  10, 6,  8,  8,
    10, 8,  10, 10, 12, 6,  8,  8,  10, 8,  10, 10, 12, 8,  10, 10, 12, 10, 12,
    12, 14, 4,  6,  6,  8,  6,  8,  8,  10, 6,  8,  8,  10, 8,  10, 10, 12, 6,
    8,  8,  10, 8,  10, 10, 12, 8,  10, 10, 12, 10, 12, 12, 14, 6,  8,  8,  10,
    8,  10, 10, 12, 8,  10, 10, 12, 10, 12, 12, 14, 8,  10, 10, 12, 10, 12, 12,
    14, 10, 12, 12, 14, 12, 14, 14, 16};

constexpr uint8_t to_base64_value[] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 64,  64,  255, 64,  64,  255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 64,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62,  255,
    255, 255, 63,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  255, 255,
    255, 255, 255, 255, 255, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
    10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
    25,  255, 255, 255, 255, 255, 255, 26,  27,  28,  29,  30,  31,  32,  33,
    34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
    49,  50,  51,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255};

constexpr uint8_t to_base64_url_value[] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 64,  64,  255, 64,  64,  255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 64,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    62,  255, 255, 52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  255, 255,
    255, 255, 255, 255, 255, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
    10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
    25,  255, 255, 255, 255, 63,  255, 26,  27,  28,  29,  30,  31,  32,  33,
    34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
    49,  50,  51,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255};

constexpr uint8_t to_base64_default_or_url_value[] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 64,  64,  255, 64,  64,  255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 64,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62,  255,
    62,  255, 63,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  255, 255,
    255, 255, 255, 255, 255, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
    10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
    25,  255, 255, 255, 255, 63,  255, 26,  27,  28,  29,  30,  31,  32,  33,
    34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
    49,  50,  51,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255};

static_assert(sizeof(to_base64_value) == 256,
              "to_base64_value must have 256 elements");
static_assert(sizeof(to_base64_url_value) == 256,
              "to_base64_url_value must have 256 elements");
static_assert(to_base64_value[uint8_t(' ')] == 64,
              "space must be == 64 in to_base64_value");
static_assert(to_base64_url_value[uint8_t(' ')] == 64,
              "space must be == 64 in to_base64_url_value");
static_assert(to_base64_value[uint8_t('\t')] == 64,
              "tab must be == 64 in to_base64_value");
static_assert(to_base64_url_value[uint8_t('\t')] == 64,
              "tab must be == 64 in to_base64_url_value");
static_assert(to_base64_value[uint8_t('\r')] == 64,
              "cr must be == 64 in to_base64_value");
static_assert(to_base64_url_value[uint8_t('\r')] == 64,
              "cr must be == 64 in to_base64_url_value");
static_assert(to_base64_value[uint8_t('\n')] == 64,
              "lf must be == 64 in to_base64_value");
static_assert(to_base64_url_value[uint8_t('\n')] == 64,
              "lf must be == 64 in to_base64_url_value");
static_assert(to_base64_value[uint8_t('\f')] == 64,
              "ff must be == 64 in to_base64_value");
static_assert(to_base64_url_value[uint8_t('\f')] == 64,
              "ff must be == 64 in to_base64_url_value");
static_assert(to_base64_value[uint8_t('+')] == 62,
              "+ must be == 62 in to_base64_value");
static_assert(to_base64_url_value[uint8_t('-')] == 62,
              "- must be == 62 in to_base64_url_value");
static_assert(to_base64_value[uint8_t('/')] == 63,
              "/ must be == 63 in to_base64_value");
static_assert(to_base64_url_value[uint8_t('_')] == 63,
              "_ must be == 63 in to_base64_url_value");
} // namespace base64
} // namespace tables
} // unnamed namespace
} // namespace simdutf

#endif // SIMDUTF_BASE64_TABLES_H
/* end file include/simdutf/base64_tables.h */
/* begin file include/simdutf/scalar/base64.h */
#ifndef SIMDUTF_BASE64_H
#define SIMDUTF_BASE64_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace simdutf {
namespace scalar {
namespace {
namespace base64 {

// This function is not expected to be fast. Do not use in long loops.
// In most instances you should be using is_ignorable.
template <class char_type> bool is_ascii_white_space(char_type c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

template <class char_type> simdutf_constexpr23 bool is_eight_byte(char_type c) {
  if constexpr (sizeof(char_type) == 1) {
    return true;
  }
  return uint8_t(c) == c;
}

template <class char_type>
simdutf_constexpr23 bool is_ignorable(char_type c,
                                      simdutf::base64_options options) {
  const uint8_t *to_base64 =
      (options & base64_default_or_url)
          ? tables::base64::to_base64_default_or_url_value
          : ((options & base64_url) ? tables::base64::to_base64_url_value
                                    : tables::base64::to_base64_value);
  const bool ignore_garbage =
      (options == base64_options::base64_url_accept_garbage) ||
      (options == base64_options::base64_default_accept_garbage) ||
      (options == base64_options::base64_default_or_url_accept_garbage);
  uint8_t code = to_base64[uint8_t(c)];
  if (is_eight_byte(c) && code <= 63) {
    return false;
  }
  if (is_eight_byte(c) && code == 64) {
    return true;
  }
  return ignore_garbage;
}
template <class char_type>
simdutf_constexpr23 bool is_base64(char_type c,
                                   simdutf::base64_options options) {
  const uint8_t *to_base64 =
      (options & base64_default_or_url)
          ? tables::base64::to_base64_default_or_url_value
          : ((options & base64_url) ? tables::base64::to_base64_url_value
                                    : tables::base64::to_base64_value);
  uint8_t code = to_base64[uint8_t(c)];
  if (is_eight_byte(c) && code <= 63) {
    return true;
  }
  return false;
}

template <class char_type>
simdutf_constexpr23 bool is_base64_or_padding(char_type c,
                                              simdutf::base64_options options) {
  const uint8_t *to_base64 =
      (options & base64_default_or_url)
          ? tables::base64::to_base64_default_or_url_value
          : ((options & base64_url) ? tables::base64::to_base64_url_value
                                    : tables::base64::to_base64_value);
  if (c == '=') {
    return true;
  }
  uint8_t code = to_base64[uint8_t(c)];
  if (is_eight_byte(c) && code <= 63) {
    return true;
  }
  return false;
}

template <class char_type>
bool is_ignorable_or_padding(char_type c, simdutf::base64_options options) {
  return is_ignorable(c, options) || c == '=';
}

struct reduced_input {
  size_t equalsigns;    // number of padding characters '=', typically 0, 1, 2.
  size_t equallocation; // location of the first padding character if any
  size_t srclen;        // length of the input buffer before padding
  size_t full_input_length; // length of the input buffer with padding but
                            // without ignorable characters
};

// find the end of the base64 input buffer
// It returns the number of padding characters, the location of the first
// padding character if any, the length of the input buffer before padding
// and the length of the input buffer with padding. The input buffer is not
// modified. The function assumes that there are at most two padding characters.
template <class char_type>
simdutf_constexpr23 reduced_input find_end(const char_type *src, size_t srclen,
                                           simdutf::base64_options options) {
  const uint8_t *to_base64 =
      (options & base64_default_or_url)
          ? tables::base64::to_base64_default_or_url_value
          : ((options & base64_url) ? tables::base64::to_base64_url_value
                                    : tables::base64::to_base64_value);
  const bool ignore_garbage =
      (options == base64_options::base64_url_accept_garbage) ||
      (options == base64_options::base64_default_accept_garbage) ||
      (options == base64_options::base64_default_or_url_accept_garbage);

  size_t equalsigns = 0;
  // We intentionally include trailing spaces in the full input length.
  // See https://github.com/simdutf/simdutf/issues/824
  size_t full_input_length = srclen;
  // skip trailing spaces
  while (!ignore_garbage && srclen > 0 &&
         scalar::base64::is_eight_byte(src[srclen - 1]) &&
         to_base64[uint8_t(src[srclen - 1])] == 64) {
    srclen--;
  }
  size_t equallocation =
      srclen; // location of the first padding character if any
  if (ignore_garbage) {
    // Technically, we don't need to find the first padding character, we can
    // just change our algorithms, but it adds substantial complexity.
    auto it = simdutf::find(src, src + srclen, '=');
    if (it != src + srclen) {
      equallocation = it - src;
      equalsigns = 1;
      srclen = equallocation;
      full_input_length = equallocation + 1;
    }
    return {equalsigns, equallocation, srclen, full_input_length};
  }
  if (!ignore_garbage && srclen > 0 && src[srclen - 1] == '=') {
    // This is the last '=' sign.
    equallocation = srclen - 1;
    srclen--;
    equalsigns = 1;
    // skip trailing spaces
    while (srclen > 0 && scalar::base64::is_eight_byte(src[srclen - 1]) &&
           to_base64[uint8_t(src[srclen - 1])] == 64) {
      srclen--;
    }
    if (srclen > 0 && src[srclen - 1] == '=') {
      // This is the second '=' sign.
      equallocation = srclen - 1;
      srclen--;
      equalsigns = 2;
    }
  }
  return {equalsigns, equallocation, srclen, full_input_length};
}

// Returns true upon success. The destination buffer must be large enough.
// This functions assumes that the padding (=) has been removed.
// if check_capacity is true, it will check that the destination buffer is
// large enough. If it is not, it will return OUTPUT_BUFFER_TOO_SMALL.
template <bool check_capacity, class char_type>
simdutf_constexpr23 full_result base64_tail_decode_impl(
    char *dst, size_t outlen, const char_type *src, size_t length,
    size_t padding_characters, // number of padding characters
                               // '=', typically 0, 1, 2.
    base64_options options, last_chunk_handling_options last_chunk_options) {
  char *dstend = dst + outlen;
  (void)dstend;
  // This looks like 10 branches, but we expect the compiler to resolve this to
  // two branches (easily predicted):
  const uint8_t *to_base64 =
      (options & base64_default_or_url)
          ? tables::base64::to_base64_default_or_url_value
          : ((options & base64_url) ? tables::base64::to_base64_url_value
                                    : tables::base64::to_base64_value);
  const uint32_t *d0 =
      (options & base64_default_or_url)
          ? tables::base64::base64_default_or_url::d0
          : ((options & base64_url) ? tables::base64::base64_url::d0
                                    : tables::base64::base64_default::d0);
  const uint32_t *d1 =
      (options & base64_default_or_url)
          ? tables::base64::base64_default_or_url::d1
          : ((options & base64_url) ? tables::base64::base64_url::d1
                                    : tables::base64::base64_default::d1);
  const uint32_t *d2 =
      (options & base64_default_or_url)
          ? tables::base64::base64_default_or_url::d2
          : ((options & base64_url) ? tables::base64::base64_url::d2
                                    : tables::base64::base64_default::d2);
  const uint32_t *d3 =
      (options & base64_default_or_url)
          ? tables::base64::base64_default_or_url::d3
          : ((options & base64_url) ? tables::base64::base64_url::d3
                                    : tables::base64::base64_default::d3);
  const bool ignore_garbage =
      (options == base64_options::base64_url_accept_garbage) ||
      (options == base64_options::base64_default_accept_garbage) ||
      (options == base64_options::base64_default_or_url_accept_garbage);

  const char_type *srcend = src + length;
  const char_type *srcinit = src;
  const char *dstinit = dst;

  uint32_t x;
  size_t idx;
  uint8_t buffer[4];
  while (true) {
    while (srcend - src >= 4 && is_eight_byte(src[0]) &&
           is_eight_byte(src[1]) && is_eight_byte(src[2]) &&
           is_eight_byte(src[3]) &&
           (x = d0[uint8_t(src[0])] | d1[uint8_t(src[1])] |
                d2[uint8_t(src[2])] | d3[uint8_t(src[3])]) < 0x01FFFFFF) {
      if (check_capacity && dstend - dst < 3) {
        return {OUTPUT_BUFFER_TOO_SMALL, size_t(src - srcinit),
                size_t(dst - dstinit)};
      }
      *dst++ = static_cast<char>(x & 0xFF);
      *dst++ = static_cast<char>((x >> 8) & 0xFF);
      *dst++ = static_cast<char>((x >> 16) & 0xFF);
      src += 4;
    }
    const char_type *srccur = src;
    idx = 0;
    // we need at least four characters.
#ifdef __clang__
    // If possible, we read four characters at a time. (It is an optimization.)
    if (ignore_garbage && src + 4 <= srcend) {
      char_type c0 = src[0];
      char_type c1 = src[1];
      char_type c2 = src[2];
      char_type c3 = src[3];

      uint8_t code0 = to_base64[uint8_t(c0)];
      uint8_t code1 = to_base64[uint8_t(c1)];
      uint8_t code2 = to_base64[uint8_t(c2)];
      uint8_t code3 = to_base64[uint8_t(c3)];

      buffer[idx] = code0;
      idx += (is_eight_byte(c0) && code0 <= 63);
      buffer[idx] = code1;
      idx += (is_eight_byte(c1) && code1 <= 63);
      buffer[idx] = code2;
      idx += (is_eight_byte(c2) && code2 <= 63);
      buffer[idx] = code3;
      idx += (is_eight_byte(c3) && code3 <= 63);
      src += 4;
    }
#endif
    while ((idx < 4) && (src < srcend)) {
      char_type c = *src;

      uint8_t code = to_base64[uint8_t(c)];
      buffer[idx] = uint8_t(code);
      if (is_eight_byte(c) && code <= 63) {
        idx++;
      } else if (!ignore_garbage &&
                 (code > 64 || !scalar::base64::is_eight_byte(c))) {
        return {INVALID_BASE64_CHARACTER, size_t(src - srcinit),
                size_t(dst - dstinit)};
      } else {
        // We have a space or a newline or garbage. We ignore it.
      }
      src++;
    }
    if (idx != 4) {
      simdutf_log_assert(idx < 4, "idx should be less than 4");
      // We never should have that the number of base64 characters + the
      // number of padding characters is more than 4.
      if (!ignore_garbage && (idx + padding_characters > 4)) {
        return {INVALID_BASE64_CHARACTER, size_t(src - srcinit),
                size_t(dst - dstinit), true};
      }

      // The idea here is that in loose mode,
      // if there is padding at all, it must be used
      // to form 4-wise chunk. However, in loose mode,
      // we do accept no padding at all.
      if (!ignore_garbage &&
          last_chunk_options == last_chunk_handling_options::loose &&
          (idx >= 2) && padding_characters > 0 &&
          ((idx + padding_characters) & 3) != 0) {
        return {INVALID_BASE64_CHARACTER, size_t(src - srcinit),
                size_t(dst - dstinit), true};
      } else

        // The idea here is that in strict mode, we do not want to accept
        // incomplete base64 chunks. So if the chunk was otherwise valid, we
        // return BASE64_INPUT_REMAINDER.
        if (!ignore_garbage &&
            last_chunk_options == last_chunk_handling_options::strict &&
            (idx >= 2) && ((idx + padding_characters) & 3) != 0) {
          // The partial chunk was at src - idx
          return {BASE64_INPUT_REMAINDER, size_t(src - srcinit),
                  size_t(dst - dstinit), true};
        } else
          // If there is a partial chunk with insufficient padding, with
          // stop_before_partial, we need to just ignore it. In "only full"
          // mode, skip the minute there are padding characters.
          if ((last_chunk_options ==
                   last_chunk_handling_options::stop_before_partial &&
               (padding_characters + idx < 4) && (idx != 0) &&
               (idx >= 2 || padding_characters == 0)) ||
              (last_chunk_options ==
                   last_chunk_handling_options::only_full_chunks &&
               (idx >= 2 || padding_characters == 0))) {
            // partial means that we are *not* going to consume the read
            // characters. We need to rewind the src pointer.
            src = srccur;
            return {SUCCESS, size_t(src - srcinit), size_t(dst - dstinit)};
          } else {
            if (idx == 2) {
              uint32_t triple = (uint32_t(buffer[0]) << 3 * 6) +
                                (uint32_t(buffer[1]) << 2 * 6);
              if (!ignore_garbage &&
                  (last_chunk_options == last_chunk_handling_options::strict) &&
                  (triple & 0xffff)) {
                return {BASE64_EXTRA_BITS, size_t(src - srcinit),
                        size_t(dst - dstinit)};
              }
              if (check_capacity && dstend - dst < 1) {
                return {OUTPUT_BUFFER_TOO_SMALL, size_t(srccur - srcinit),
                        size_t(dst - dstinit)};
              }
              *dst++ = static_cast<char>((triple >> 16) & 0xFF);
            } else if (idx == 3) {
              uint32_t triple = (uint32_t(buffer[0]) << 3 * 6) +
                                (uint32_t(buffer[1]) << 2 * 6) +
                                (uint32_t(buffer[2]) << 1 * 6);
              if (!ignore_garbage &&
                  (last_chunk_options == last_chunk_handling_options::strict) &&
                  (triple & 0xff)) {
                return {BASE64_EXTRA_BITS, size_t(src - srcinit),
                        size_t(dst - dstinit)};
              }
              if (check_capacity && dstend - dst < 2) {
                return {OUTPUT_BUFFER_TOO_SMALL, size_t(srccur - srcinit),
                        size_t(dst - dstinit)};
              }
              *dst++ = static_cast<char>((triple >> 16) & 0xFF);
              *dst++ = static_cast<char>((triple >> 8) & 0xFF);
            } else if (!ignore_garbage && idx == 1 &&
                       (!is_partial(last_chunk_options) ||
                        (is_partial(last_chunk_options) &&
                         padding_characters > 0))) {
              return {BASE64_INPUT_REMAINDER, size_t(src - srcinit),
                      size_t(dst - dstinit)};
            } else if (!ignore_garbage && idx == 0 && padding_characters > 0) {
              return {INVALID_BASE64_CHARACTER, size_t(src - srcinit),
                      size_t(dst - dstinit), true};
            }
            return {SUCCESS, size_t(src - srcinit), size_t(dst - dstinit)};
          }
    }
    if (check_capacity && dstend - dst < 3) {
      return {OUTPUT_BUFFER_TOO_SMALL, size_t(srccur - srcinit),
              size_t(dst - dstinit)};
    }
    uint32_t triple =
        (uint32_t(buffer[0]) << 3 * 6) + (uint32_t(buffer[1]) << 2 * 6) +
        (uint32_t(buffer[2]) << 1 * 6) + (uint32_t(buffer[3]) << 0 * 6);
    *dst++ = static_cast<char>((triple >> 16) & 0xFF);
    *dst++ = static_cast<char>((triple >> 8) & 0xFF);
    *dst++ = static_cast<char>(triple & 0xFF);
  }
}

template <class char_type>
simdutf_constexpr23 full_result base64_tail_decode(
    char *dst, const char_type *src, size_t length,
    size_t padding_characters, // number of padding characters
                               // '=', typically 0, 1, 2.
    base64_options options, last_chunk_handling_options last_chunk_options) {
  return base64_tail_decode_impl<false>(dst, 0, src, length, padding_characters,
                                        options, last_chunk_options);
}

// like base64_tail_decode, but it will not write past the end of the output
// buffer. The outlen parameter is modified to reflect the number of bytes
// written. This functions assumes that the padding (=) has been removed.
//
template <class char_type>
simdutf_constexpr23 full_result base64_tail_decode_safe(
    char *dst, size_t outlen, const char_type *src, size_t length,
    size_t padding_characters, // number of padding characters
                               // '=', typically 0, 1, 2.
    base64_options options, last_chunk_handling_options last_chunk_options) {
  return base64_tail_decode_impl<true>(dst, outlen, src, length,
                                       padding_characters, options,
                                       last_chunk_options);
}

inline simdutf_constexpr23 full_result
patch_tail_result(full_result r, size_t previous_input, size_t previous_output,
                  size_t equallocation, size_t full_input_length,
                  last_chunk_handling_options last_chunk_options) {
  r.input_count += previous_input;
  r.output_count += previous_output;
  if (r.padding_error) {
    r.input_count = equallocation;
  }

  if (r.error == error_code::SUCCESS) {
    if (!is_partial(last_chunk_options)) {
      // A success when we are not in stop_before_partial mode.
      // means that we have consumed the whole input buffer.
      r.input_count = full_input_length;
    } else if (r.output_count % 3 != 0) {
      r.input_count = full_input_length;
    }
  }
  return r;
}

// Returns the number of bytes written. The destination buffer must be large
// enough. It will add padding (=) if needed.
template <bool use_lines = false>
simdutf_constexpr23 size_t tail_encode_base64_impl(
    char *dst, const char *src, size_t srclen, base64_options options,
    size_t line_length = simdutf::default_line_length, size_t line_offset = 0) {
  if constexpr (use_lines) {
    // sanitize line_length and starting_line_offset.
    // line_length must be greater than 3.
    if (line_length < 4) {
      line_length = 4;
    }
    simdutf_log_assert(line_offset <= line_length,
                       "line_offset should be less than line_length");
  }
  // By default, we use padding if we are not using the URL variant.
  // This is check with ((options & base64_url) == 0) which returns true if we
  // are not using the URL variant. However, we also allow 'inversion' of the
  // convention with the base64_reverse_padding option. If the
  // base64_reverse_padding option is set, we use padding if we are using the
  // URL variant, and we omit it if we are not using the URL variant. This is
  // checked with
  // ((options & base64_reverse_padding) == base64_reverse_padding).
  bool use_padding =
      ((options & base64_url) == 0) ^
      ((options & base64_reverse_padding) == base64_reverse_padding);
  // This looks like 3 branches, but we expect the compiler to resolve this to
  // a single branch:
  const char *e0 = (options & base64_url) ? tables::base64::base64_url::e0
                                          : tables::base64::base64_default::e0;
  const char *e1 = (options & base64_url) ? tables::base64::base64_url::e1
                                          : tables::base64::base64_default::e1;
  const char *e2 = (options & base64_url) ? tables::base64::base64_url::e2
                                          : tables::base64::base64_default::e2;
  char *out = dst;
  size_t i = 0;
  uint8_t t1, t2, t3;
  for (; i + 2 < srclen; i += 3) {
    t1 = uint8_t(src[i]);
    t2 = uint8_t(src[i + 1]);
    t3 = uint8_t(src[i + 2]);
    if constexpr (use_lines) {
      if (line_offset + 3 >= line_length) {
        if (line_offset == line_length) {
          *out++ = '\n';
          *out++ = e0[t1];
          *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
          *out++ = e1[((t2 & 0x0F) << 2) | ((t3 >> 6) & 0x03)];
          *out++ = e2[t3];
          line_offset = 4;
        } else if (line_offset + 1 == line_length) {
          *out++ = e0[t1];
          *out++ = '\n';
          *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
          *out++ = e1[((t2 & 0x0F) << 2) | ((t3 >> 6) & 0x03)];
          *out++ = e2[t3];
          line_offset = 3;
        } else if (line_offset + 2 == line_length) {
          *out++ = e0[t1];
          *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
          *out++ = '\n';
          *out++ = e1[((t2 & 0x0F) << 2) | ((t3 >> 6) & 0x03)];
          *out++ = e2[t3];
          line_offset = 2;
        } else if (line_offset + 3 == line_length) {
          *out++ = e0[t1];
          *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
          *out++ = e1[((t2 & 0x0F) << 2) | ((t3 >> 6) & 0x03)];
          *out++ = '\n';
          *out++ = e2[t3];
          line_offset = 1;
        }
      } else {
        *out++ = e0[t1];
        *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
        *out++ = e1[((t2 & 0x0F) << 2) | ((t3 >> 6) & 0x03)];
        *out++ = e2[t3];
        line_offset += 4;
      }
    } else {
      *out++ = e0[t1];
      *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
      *out++ = e1[((t2 & 0x0F) << 2) | ((t3 >> 6) & 0x03)];
      *out++ = e2[t3];
    }
  }
  switch (srclen - i) {
  case 0:
    break;
  case 1:
    t1 = uint8_t(src[i]);
    if constexpr (use_lines) {
      if (use_padding) {
        if (line_offset + 3 >= line_length) {
          if (line_offset == line_length) {
            *out++ = '\n';
            *out++ = e0[t1];
            *out++ = e1[(t1 & 0x03) << 4];
            *out++ = '=';
            *out++ = '=';
          } else if (line_offset + 1 == line_length) {
            *out++ = e0[t1];
            *out++ = '\n';
            *out++ = e1[(t1 & 0x03) << 4];
            *out++ = '=';
            *out++ = '=';
          } else if (line_offset + 2 == line_length) {
            *out++ = e0[t1];
            *out++ = e1[(t1 & 0x03) << 4];
            *out++ = '\n';
            *out++ = '=';
            *out++ = '=';
          } else if (line_offset + 3 == line_length) {
            *out++ = e0[t1];
            *out++ = e1[(t1 & 0x03) << 4];
            *out++ = '=';
            *out++ = '\n';
            *out++ = '=';
          }
        } else {
          *out++ = e0[t1];
          *out++ = e1[(t1 & 0x03) << 4];
          *out++ = '=';
          *out++ = '=';
        }
      } else {
        if (line_offset + 2 >= line_length) {
          if (line_offset == line_length) {
            *out++ = '\n';
            *out++ = e0[uint8_t(src[i])];
            *out++ = e1[(uint8_t(src[i]) & 0x03) << 4];
          } else if (line_offset + 1 == line_length) {
            *out++ = e0[uint8_t(src[i])];
            *out++ = '\n';
            *out++ = e1[(uint8_t(src[i]) & 0x03) << 4];
          } else {
            *out++ = e0[uint8_t(src[i])];
            *out++ = e1[(uint8_t(src[i]) & 0x03) << 4];
            // *out++ = '\n'; ==> no newline at the end of the output
          }
        } else {
          *out++ = e0[uint8_t(src[i])];
          *out++ = e1[(uint8_t(src[i]) & 0x03) << 4];
        }
      }
    } else {
      *out++ = e0[t1];
      *out++ = e1[(t1 & 0x03) << 4];
      if (use_padding) {
        *out++ = '=';
        *out++ = '=';
      }
    }
    break;
  default: /* case 2 */
    t1 = uint8_t(src[i]);
    t2 = uint8_t(src[i + 1]);
    if constexpr (use_lines) {
      if (use_padding) {
        if (line_offset + 3 >= line_length) {
          if (line_offset == line_length) {
            *out++ = '\n';
            *out++ = e0[t1];
            *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
            *out++ = e2[(t2 & 0x0F) << 2];
            *out++ = '=';
          } else if (line_offset + 1 == line_length) {
            *out++ = e0[t1];
            *out++ = '\n';
            *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
            *out++ = e2[(t2 & 0x0F) << 2];
            *out++ = '=';
          } else if (line_offset + 2 == line_length) {
            *out++ = e0[t1];
            *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
            *out++ = '\n';
            *out++ = e2[(t2 & 0x0F) << 2];
            *out++ = '=';
          } else if (line_offset + 3 == line_length) {
            *out++ = e0[t1];
            *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
            *out++ = e2[(t2 & 0x0F) << 2];
            *out++ = '\n';
            *out++ = '=';
          }
        } else {
          *out++ = e0[t1];
          *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
          *out++ = e2[(t2 & 0x0F) << 2];
          *out++ = '=';
        }
      } else {
        if (line_offset + 3 >= line_length) {
          if (line_offset == line_length) {
            *out++ = '\n';
            *out++ = e0[t1];
            *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
            *out++ = e2[(t2 & 0x0F) << 2];
          } else if (line_offset + 1 == line_length) {
            *out++ = e0[t1];
            *out++ = '\n';
            *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
            *out++ = e2[(t2 & 0x0F) << 2];
          } else if (line_offset + 2 == line_length) {
            *out++ = e0[t1];
            *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
            *out++ = '\n';
            *out++ = e2[(t2 & 0x0F) << 2];
          } else {
            *out++ = e0[t1];
            *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
            *out++ = e2[(t2 & 0x0F) << 2];
            // *out++ = '\n'; ==> no newline at the end of the output
          }
        } else {
          *out++ = e0[t1];
          *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
          *out++ = e2[(t2 & 0x0F) << 2];
        }
      }
    } else {
      *out++ = e0[t1];
      *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
      *out++ = e2[(t2 & 0x0F) << 2];
      if (use_padding) {
        *out++ = '=';
      }
    }
  }
  return (size_t)(out - dst);
}

// Returns the number of bytes written. The destination buffer must be large
// enough. It will add padding (=) if needed.
simdutf_unused inline simdutf_constexpr23 size_t tail_encode_base64(
    char *dst, const char *src, size_t srclen, base64_options options) {
  return tail_encode_base64_impl(dst, src, srclen, options);
}

template <class InputPtr>
simdutf_warn_unused simdutf_constexpr23 size_t
maximal_binary_length_from_base64(InputPtr input, size_t length) noexcept {
  // We process the padding characters ('=') at the end to make sure
  // that we return an exact result when the input has no ignorable characters
  // (e.g., spaces).
  size_t padding = 0;
  if (length > 0) {
    if (input[length - 1] == '=') {
      padding++;
      if (length > 1 && input[length - 2] == '=') {
        padding++;
      }
    }
  }
  // The input is not otherwise processed for ignorable characters or
  // validation, so that the function runs in constant time (very fast). In
  // practice, base64 inputs without ignorable characters are common and the
  // common case are line separated inputs with relatively long lines (e.g., 76
  // characters) which leads this function to a slight (1%) overestimation of
  // the output size.
  //
  // Of course, some inputs might contain an arbitrary number of spaces or
  // newlines, which would make this function return a very pessimistic output
  // size but systems that produce base64 outputs typically do not do that and
  // if they do, they do not care much about minimizing memory usage.
  //
  // In specialized applications, users may know that their input is line
  // separated, which can be checked very quickly by by iterating (e.g., over 76
  // character chunks, looking for the linefeed characters only). We could
  // provide a specialized function for that, but it is not clear that the added
  // complexity is worth it for us.
  //
  size_t actual_length = length - padding;
  if (actual_length % 4 <= 1) {
    return actual_length / 4 * 3;
  }
  // if we have a valid input, then the remainder must be 2 or 3 adding one or
  // two extra bytes.
  return actual_length / 4 * 3 + (actual_length % 4) - 1;
}

// This function computes the binary length by iterating through the input
// and counting non-whitespace characters (excluding padding characters).
// We use a simple check (c > ' ') which is easy to parallelize and matches
// SIMD behavior. Only the last few characters are checked for padding '='.
template <class char_type>
simdutf_warn_unused simdutf_constexpr23 size_t
binary_length_from_base64(const char_type *input, size_t length) noexcept {
  // Count non-whitespace characters (c > ' ') with loop unrolling
  size_t count = 0;
  for (size_t i = 0; i < length; i++) {
    count += (input[i] > ' ');
  }

  // Check for padding '=' at the end (at most 2 padding characters)
  // Scan backwards, skipping whitespace, to find padding
  size_t padding = 0;
  size_t pos = length;
  // Skip trailing whitespace
  while (pos > 0 && padding < 2) {
    char_type c = input[--pos];
    if (c == '=') {
      padding++;
    } else if (c > ' ') {
      break;
    }
  }
  return ((count - padding) * 3) / 4;
}

template <typename char_type>
simdutf_warn_unused simdutf_constexpr23 full_result
base64_to_binary_details_impl(
    const char_type *input, size_t length, char *output, base64_options options,
    last_chunk_handling_options last_chunk_options) noexcept {
  const bool ignore_garbage =
      (options == base64_options::base64_url_accept_garbage) ||
      (options == base64_options::base64_default_accept_garbage) ||
      (options == base64_options::base64_default_or_url_accept_garbage);
  auto ri = simdutf::scalar::base64::find_end(input, length, options);
  size_t equallocation = ri.equallocation;
  size_t equalsigns = ri.equalsigns;
  length = ri.srclen;
  size_t full_input_length = ri.full_input_length;
  if (length == 0) {
    if (!ignore_garbage && equalsigns > 0) {
      return {INVALID_BASE64_CHARACTER, equallocation, 0, true};
    }
    return {SUCCESS, full_input_length, 0};
  }
  full_result r = scalar::base64::base64_tail_decode(
      output, input, length, equalsigns, options, last_chunk_options);
  r = scalar::base64::patch_tail_result(r, 0, 0, equallocation,
                                        full_input_length, last_chunk_options);
  if (!is_partial(last_chunk_options) && r.error == error_code::SUCCESS &&
      equalsigns > 0 && !ignore_garbage) {
    // additional checks
    if ((r.output_count % 3 == 0) ||
        ((r.output_count % 3) + 1 + equalsigns != 4)) {
      return {INVALID_BASE64_CHARACTER, equallocation, r.output_count, true};
    }
  }
  // When is_partial(last_chunk_options) is true, we must either end with
  // the end of the stream (beyond whitespace) or right after a non-ignorable
  // character or at the very beginning of the stream.
  // See https://tc39.es/proposal-arraybuffer-base64/spec/#sec-frombase64
  if (is_partial(last_chunk_options) && r.error == error_code::SUCCESS &&
      r.input_count < full_input_length) {
    // First check if we can extend the input to the end of the stream
    while (r.input_count < full_input_length &&
           base64_ignorable(*(input + r.input_count), options)) {
      r.input_count++;
    }
    // If we are still not at the end of the stream, then we must backtrack
    // to the last non-ignorable character.
    if (r.input_count < full_input_length) {
      while (r.input_count > 0 &&
             base64_ignorable(*(input + r.input_count - 1), options)) {
        r.input_count--;
      }
    }
  }
  return r;
}

template <typename char_type>
simdutf_constexpr23 simdutf_warn_unused full_result
base64_to_binary_details_safe_impl(
    const char_type *input, size_t length, char *output, size_t outlen,
    base64_options options,
    last_chunk_handling_options last_chunk_options) noexcept {
  const bool ignore_garbage =
      (options == base64_options::base64_url_accept_garbage) ||
      (options == base64_options::base64_default_accept_garbage) ||
      (options == base64_options::base64_default_or_url_accept_garbage);
  auto ri = simdutf::scalar::base64::find_end(input, length, options);
  size_t equallocation = ri.equallocation;
  size_t equalsigns = ri.equalsigns;
  length = ri.srclen;
  size_t full_input_length = ri.full_input_length;
  if (length == 0) {
    if (!ignore_garbage && equalsigns > 0) {
      return {INVALID_BASE64_CHARACTER, equallocation, 0};
    }
    return {SUCCESS, full_input_length, 0};
  }
  full_result r = scalar::base64::base64_tail_decode_safe(
      output, outlen, input, length, equalsigns, options, last_chunk_options);
  r = scalar::base64::patch_tail_result(r, 0, 0, equallocation,
                                        full_input_length, last_chunk_options);
  if (!is_partial(last_chunk_options) && r.error == error_code::SUCCESS &&
      equalsigns > 0 && !ignore_garbage) {
    // additional checks
    if ((r.output_count % 3 == 0) ||
        ((r.output_count % 3) + 1 + equalsigns != 4)) {
      return {INVALID_BASE64_CHARACTER, equallocation, r.output_count};
    }
  }

  // When is_partial(last_chunk_options) is true, we must either end with
  // the end of the stream (beyond whitespace) or right after a non-ignorable
  // character or at the very beginning of the stream.
  // See https://tc39.es/proposal-arraybuffer-base64/spec/#sec-frombase64
  if (is_partial(last_chunk_options) && r.error == error_code::SUCCESS &&
      r.input_count < full_input_length) {
    // First check if we can extend the input to the end of the stream
    while (r.input_count < full_input_length &&
           base64_ignorable(*(input + r.input_count), options)) {
      r.input_count++;
    }
    // If we are still not at the end of the stream, then we must backtrack
    // to the last non-ignorable character.
    if (r.input_count < full_input_length) {
      while (r.input_count > 0 &&
             base64_ignorable(*(input + r.input_count - 1), options)) {
        r.input_count--;
      }
    }
  }
  return r;
}

simdutf_warn_unused simdutf_constexpr23 size_t
base64_length_from_binary(size_t length, base64_options options) noexcept {
  // By default, we use padding if we are not using the URL variant.
  // This is check with ((options & base64_url) == 0) which returns true if we
  // are not using the URL variant. However, we also allow 'inversion' of the
  // convention with the base64_reverse_padding option. If the
  // base64_reverse_padding option is set, we use padding if we are using the
  // URL variant, and we omit it if we are not using the URL variant. This is
  // checked with
  // ((options & base64_reverse_padding) == base64_reverse_padding).
  bool use_padding =
      ((options & base64_url) == 0) ^
      ((options & base64_reverse_padding) == base64_reverse_padding);
  if (!use_padding) {
    return length / 3 * 4 + ((length % 3) ? (length % 3) + 1 : 0);
  }
  return (length + 2) / 3 *
         4; // We use padding to make the length a multiple of 4.
}

simdutf_warn_unused simdutf_constexpr23 size_t
base64_length_from_binary_with_lines(size_t length, base64_options options,
                                     size_t line_length) noexcept {
  if (length == 0) {
    return 0;
  }
  size_t base64_length =
      scalar::base64::base64_length_from_binary(length, options);
  if (line_length < 4) {
    line_length = 4;
  }
  size_t lines =
      (base64_length + line_length - 1) / line_length; // number of lines
  return base64_length + lines - 1;
}

// Return the length of the prefix that contains count base64 characters.
// Thus, if count is 3, the function returns the length of the prefix
// that contains 3 base64 characters.
// The function returns (size_t)-1 if there is not enough base64 characters in
// the input.
template <typename char_type>
simdutf_warn_unused size_t prefix_length(size_t count,
                                         simdutf::base64_options options,
                                         const char_type *input,
                                         size_t length) noexcept {
  size_t i = 0;
  while (i < length && is_ignorable(input[i], options)) {
    i++;
  }
  if (count == 0) {
    return i; // duh!
  }
  for (; i < length; i++) {
    if (is_ignorable(input[i], options)) {
      continue;
    }
    // We have a base64 character or a padding character.
    count--;
    if (count == 0) {
      return i + 1;
    }
  }
  simdutf_log_assert(false, "You never get here");

  return -1; // should never happen
}

} // namespace base64
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
/* end file include/simdutf/scalar/base64.h */

namespace simdutf {

inline std::string_view to_string(base64_options options) {
  switch (options) {
  case base64_default:
    return "base64_default";
  case base64_url:
    return "base64_url";
  case base64_reverse_padding:
    return "base64_reverse_padding";
  case base64_url_with_padding:
    return "base64_url_with_padding";
  case base64_default_accept_garbage:
    return "base64_default_accept_garbage";
  case base64_url_accept_garbage:
    return "base64_url_accept_garbage";
  case base64_default_or_url:
    return "base64_default_or_url";
  case base64_default_or_url_accept_garbage:
    return "base64_default_or_url_accept_garbage";
  }
  return "<unknown>";
}

inline std::string_view to_string(last_chunk_handling_options options) {
  switch (options) {
  case loose:
    return "loose";
  case strict:
    return "strict";
  case stop_before_partial:
    return "stop_before_partial";
  case only_full_chunks:
    return "only_full_chunks";
  }
  return "<unknown>";
}

/**
 * Provide the maximal binary length in bytes given the base64 input.
 * As long as the input does not contain ignorable characters (e.g., ASCII
 * spaces or linefeed characters), the result is exact. In particular, the
 * function checks for padding characters.
 *
 * The function is fast (constant time). It checks up to two characters at
 * the end of the string. The input is not otherwise validated or read.
 *
 * @param input         the base64 input to process
 * @param length        the length of the base64 input in bytes
 * @return maximum number of binary bytes
 */
simdutf_warn_unused size_t
maximal_binary_length_from_base64(const char *input, size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
maximal_binary_length_from_base64(
    const detail::input_span_of_byte_like auto &input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::base64::maximal_binary_length_from_base64(
        detail::constexpr_cast_ptr<uint8_t>(input.data()), input.size());
  } else
    #endif
  {
    return maximal_binary_length_from_base64(
        reinterpret_cast<const char *>(input.data()), input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Provide the maximal binary length in bytes given the base64 input.
 * As long as the input does not contain ignorable characters (e.g., ASCII
 * spaces or linefeed characters), the result is exact. In particular, the
 * function checks for padding characters.
 *
 * The function is fast (constant time). It checks up to two characters at
 * the end of the string. The input is not otherwise validated or read.
 *
 * @param input         the base64 input to process, in ASCII stored as 16-bit
 * units
 * @param length        the length of the base64 input in 16-bit units
 * @return maximal number of binary bytes
 */
simdutf_warn_unused size_t maximal_binary_length_from_base64(
    const char16_t *input, size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
maximal_binary_length_from_base64(std::span<const char16_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::base64::maximal_binary_length_from_base64(input.data(),
                                                             input.size());
  } else
    #endif
  {
    return maximal_binary_length_from_base64(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Compute the binary length from a base64 input.
 * This function is useful for base64 inputs that may contain ASCII whitespaces
 * (such as line breaks). For such inputs, the result is exact, and for any
 * inputs the result can be used to size the output buffer passed to
 * `base64_to_binary`.
 *
 * The function ignores whitespace and does not require padding characters
 * ('=').
 *
 * @param input         the base64 input to process
 * @param length        the length of the base64 input in bytes
 * @return number of binary bytes
 */
simdutf_warn_unused size_t binary_length_from_base64(const char *input,
                                                     size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
binary_length_from_base64(
    const detail::input_span_of_byte_like auto &input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::base64::binary_length_from_base64(input.data(),
                                                     input.size());
  } else
    #endif
  {
    return binary_length_from_base64(
        reinterpret_cast<const char *>(input.data()), input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Compute the binary length from a base64 input.
 * This function is useful for base64 inputs that may contain ASCII whitespaces
 * (such as line breaks). For such inputs, the result is exact, and for any
 * inputs the result can be used to size the output buffer passed to
 * `base64_to_binary`.
 *
 * The function ignores whitespace and does not require padding characters
 * ('=').
 *
 * @param input         the base64 input to process, in ASCII stored as 16-bit
 * units
 * @param length        the length of the base64 input in 16-bit units
 * @return number of binary bytes
 */
simdutf_warn_unused size_t binary_length_from_base64(const char16_t *input,
                                                     size_t length) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
binary_length_from_base64(std::span<const char16_t> input) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::base64::binary_length_from_base64(input.data(),
                                                     input.size());
  } else
    #endif
  {
    return binary_length_from_base64(input.data(), input.size());
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert a base64 input to a binary output.
 *
 * This function follows the WHATWG forgiving-base64 format, which means that it
 * will ignore any ASCII spaces in the input. You may provide a padded input
 * (with one or two equal signs at the end) or an unpadded input (without any
 * equal signs at the end).
 *
 * See https://infra.spec.whatwg.org/#forgiving-base64-decode
 *
 * This function will fail in case of invalid input. When last_chunk_options =
 * loose, there are two possible reasons for failure: the input contains a
 * number of base64 characters that when divided by 4, leaves a single remainder
 * character (BASE64_INPUT_REMAINDER), or the input contains a character that is
 * not a valid base64 character (INVALID_BASE64_CHARACTER).
 *
 * When the error is INVALID_BASE64_CHARACTER, r.count contains the index in the
 * input where the invalid character was found. When the error is
 * BASE64_INPUT_REMAINDER, then r.count contains the number of bytes decoded.
 *
 * The default option (simdutf::base64_default) expects the characters `+` and
 * `/` as part of its alphabet. The URL option (simdutf::base64_url) expects the
 * characters `-` and `_` as part of its alphabet.
 *
 * The padding (`=`) is validated if present. There may be at most two padding
 * characters at the end of the input. If there are any padding characters, the
 * total number of characters (excluding spaces but including padding
 * characters) must be divisible by four.
 *
 * You should call this function with a buffer that is at least
 * maximal_binary_length_from_base64(input, length) bytes long. If you fail to
 * provide that much space, the function may cause a buffer overflow.
 *
 * Advanced users may want to tailor how the last chunk is handled. By default,
 * we use a loose (forgiving) approach but we also support a strict approach
 * as well as a stop_before_partial approach, as per the following proposal:
 *
 * https://tc39.es/proposal-arraybuffer-base64/spec/#sec-frombase64
 *
 * @param input         the base64 string to process
 * @param length        the length of the string in bytes
 * @param output        the pointer to a buffer that can hold the conversion
 * result (should be at least maximal_binary_length_from_base64(input, length)
 * bytes long).
 * @param options       the base64 options to use, usually base64_default or
 * base64_url, and base64_default by default.
 * @param last_chunk_options the last chunk handling options,
 * last_chunk_handling_options::loose by default
 * but can also be last_chunk_handling_options::strict or
 * last_chunk_handling_options::stop_before_partial.
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in bytes) if any, or the number of bytes written if successful.
 */
simdutf_warn_unused result base64_to_binary(
    const char *input, size_t length, char *output,
    base64_options options = base64_default,
    last_chunk_handling_options last_chunk_options = loose) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
base64_to_binary(
    const detail::input_span_of_byte_like auto &input,
    detail::output_span_of_byte_like auto &&binary_output,
    base64_options options = base64_default,
    last_chunk_handling_options last_chunk_options = loose) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::base64::base64_to_binary_details_impl(
        input.data(), input.size(), binary_output.data(), options,
        last_chunk_options);
  } else
    #endif
  {
    return base64_to_binary(reinterpret_cast<const char *>(input.data()),
                            input.size(),
                            reinterpret_cast<char *>(binary_output.data()),
                            options, last_chunk_options);
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Provide the base64 length in bytes given the length of a binary input.
 *
 * @param length        the length of the input in bytes
 * @param options       the base64 options to use (default: base64_default)
 * @return number of base64 bytes
 */
inline simdutf_warn_unused simdutf_constexpr23 size_t base64_length_from_binary(
    size_t length, base64_options options = base64_default) noexcept {
  return scalar::base64::base64_length_from_binary(length, options);
}

/**
 * Provide the base64 length in bytes given the length of a binary input,
 * taking into account line breaks.
 *
 * @param length        the length of the input in bytes
 * @param options       the base64 options to use (default: base64_default)
 * @param line_length   the length of lines, must be at least 4 (otherwise it is
 * interpreted as 4),
 * @return number of base64 bytes
 */
inline simdutf_warn_unused simdutf_constexpr23 size_t
base64_length_from_binary_with_lines(
    size_t length, base64_options options = base64_default,
    size_t line_length = default_line_length) noexcept {
  return scalar::base64::base64_length_from_binary_with_lines(length, options,
                                                              line_length);
}

/**
 * Convert a binary input to a base64 output.
 *
 * The default option (simdutf::base64_default) uses the characters `+` and `/`
 * as part of its alphabet. Further, it adds padding (`=`) at the end of the
 * output to ensure that the output length is a multiple of four.
 *
 * The URL option (simdutf::base64_url) uses the characters `-` and `_` as part
 * of its alphabet. No padding is added at the end of the output.
 *
 * This function always succeeds.
 *
 * @param input         the binary to process
 * @param length        the length of the input in bytes
 * @param output        the pointer to a buffer that can hold the conversion
 * result (should be at least base64_length_from_binary(length) bytes long)
 * @param options       the base64 options to use, can be base64_default or
 * base64_url, is base64_default by default.
 * @return number of written bytes, will be equal to
 * base64_length_from_binary(length, options)
 */
size_t binary_to_base64(const char *input, size_t length, char *output,
                        base64_options options = base64_default) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
binary_to_base64(const detail::input_span_of_byte_like auto &input,
                 detail::output_span_of_byte_like auto &&binary_output,
                 base64_options options = base64_default) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::base64::tail_encode_base64(
        binary_output.data(), input.data(), input.size(), options);
  } else
    #endif
  {
    return binary_to_base64(
        reinterpret_cast<const char *>(input.data()), input.size(),
        reinterpret_cast<char *>(binary_output.data()), options);
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert a binary input to a base64 output with line breaks.
 *
 * The default option (simdutf::base64_default) uses the characters `+` and `/`
 * as part of its alphabet. Further, it adds padding (`=`) at the end of the
 * output to ensure that the output length is a multiple of four.
 *
 * The URL option (simdutf::base64_url) uses the characters `-` and `_` as part
 * of its alphabet. No padding is added at the end of the output.
 *
 * This function always succeeds.
 *
 * @param input         the binary to process
 * @param length        the length of the input in bytes
 * @param output        the pointer to a buffer that can hold the conversion
 * result (should be at least base64_length_from_binary_with_lines(length,
 * options, line_length) bytes long)
 * @param line_length   the length of lines, must be at least 4 (otherwise it is
 * interpreted as 4),
 * @param options       the base64 options to use, can be base64_default or
 * base64_url, is base64_default by default.
 * @return number of written bytes, will be equal to
 * base64_length_from_binary_with_lines(length, options)
 */
size_t
binary_to_base64_with_lines(const char *input, size_t length, char *output,
                            size_t line_length = simdutf::default_line_length,
                            base64_options options = base64_default) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 size_t
binary_to_base64_with_lines(
    const detail::input_span_of_byte_like auto &input,
    detail::output_span_of_byte_like auto &&binary_output,
    size_t line_length = simdutf::default_line_length,
    base64_options options = base64_default) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::base64::tail_encode_base64_impl<true>(
        binary_output.data(), input.data(), input.size(), options, line_length);
  } else
    #endif
  {
    return binary_to_base64_with_lines(
        reinterpret_cast<const char *>(input.data()), input.size(),
        reinterpret_cast<char *>(binary_output.data()), line_length, options);
  }
}
  #endif // SIMDUTF_SPAN

  #if SIMDUTF_ATOMIC_REF
/**
 * Convert a binary input to a base64 output, using atomic accesses.
 * This function comes with a potentially significant performance
 * penalty, but it may be useful in some cases where the input
 * buffers are shared between threads, to avoid undefined
 * behavior in case of data races.
 *
 * The function is for advanced users. Its main use case is when
 * to silence sanitizer warnings. We have no documented use case
 * where this function is actually necessary in terms of practical correctness.
 *
 * This function is only available when simdutf is compiled with
 * C++20 support and __cpp_lib_atomic_ref >= 201806L. You may check
 * the availability of this function by checking the macro
 * SIMDUTF_ATOMIC_REF.
 *
 * The default option (simdutf::base64_default) uses the characters `+` and `/`
 * as part of its alphabet. Further, it adds padding (`=`) at the end of the
 * output to ensure that the output length is a multiple of four.
 *
 * The URL option (simdutf::base64_url) uses the characters `-` and `_` as part
 * of its alphabet. No padding is added at the end of the output.
 *
 * This function always succeeds.
 *
 * This function is considered experimental. It is not tested by default
 * (see the CMake option SIMDUTF_ATOMIC_BASE64_TESTS) nor is it fuzz tested.
 * It is not documented in the public API documentation (README). It is
 * offered on a best effort basis. We rely on the community for further
 * testing and feedback.
 *
 * @brief atomic_binary_to_base64
 * @param input         the binary to process
 * @param length        the length of the input in bytes
 * @param output        the pointer to a buffer that can hold the conversion
 * result (should be at least base64_length_from_binary(length) bytes long)
 * @param options       the base64 options to use, can be base64_default or
 * base64_url, is base64_default by default.
 * @return number of written bytes, will be equal to
 * base64_length_from_binary(length, options)
 */
size_t
atomic_binary_to_base64(const char *input, size_t length, char *output,
                        base64_options options = base64_default) noexcept;
    #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused size_t
atomic_binary_to_base64(const detail::input_span_of_byte_like auto &input,
                        detail::output_span_of_byte_like auto &&binary_output,
                        base64_options options = base64_default) noexcept {
  return atomic_binary_to_base64(
      reinterpret_cast<const char *>(input.data()), input.size(),
      reinterpret_cast<char *>(binary_output.data()), options);
}
    #endif // SIMDUTF_SPAN
  #endif   // SIMDUTF_ATOMIC_REF

/**
 * Convert a base64 input to a binary output.
 *
 * This function follows the WHATWG forgiving-base64 format, which means that it
 * will ignore any ASCII spaces in the input. You may provide a padded input
 * (with one or two equal signs at the end) or an unpadded input (without any
 * equal signs at the end).
 *
 * See https://infra.spec.whatwg.org/#forgiving-base64-decode
 *
 * This function will fail in case of invalid input. When last_chunk_options =
 * loose, there are two possible reasons for failure: the input contains a
 * number of base64 characters that when divided by 4, leaves a single remainder
 * character (BASE64_INPUT_REMAINDER), or the input contains a character that is
 * not a valid base64 character (INVALID_BASE64_CHARACTER).
 *
 * When the error is INVALID_BASE64_CHARACTER, r.count contains the index in the
 * input where the invalid character was found. When the error is
 * BASE64_INPUT_REMAINDER, then r.count contains the number of bytes decoded.
 *
 * The default option (simdutf::base64_default) expects the characters `+` and
 * `/` as part of its alphabet. The URL option (simdutf::base64_url) expects the
 * characters `-` and `_` as part of its alphabet.
 *
 * The padding (`=`) is validated if present. There may be at most two padding
 * characters at the end of the input. If there are any padding characters, the
 * total number of characters (excluding spaces but including padding
 * characters) must be divisible by four.
 *
 * You should call this function with a buffer that is at least
 * maximal_binary_length_from_base64(input, length) bytes long. If you fail
 * to provide that much space, the function may cause a buffer overflow.
 *
 * Advanced users may want to tailor how the last chunk is handled. By default,
 * we use a loose (forgiving) approach but we also support a strict approach
 * as well as a stop_before_partial approach, as per the following proposal:
 *
 * https://tc39.es/proposal-arraybuffer-base64/spec/#sec-frombase64
 *
 * @param input         the base64 string to process, in ASCII stored as 16-bit
 * units
 * @param length        the length of the string in 16-bit units
 * @param output        the pointer to a buffer that can hold the conversion
 * result (should be at least maximal_binary_length_from_base64(input, length)
 * bytes long).
 * @param options       the base64 options to use, can be base64_default or
 * base64_url, is base64_default by default.
 * @param last_chunk_options the last chunk handling options,
 * last_chunk_handling_options::loose by default
 * but can also be last_chunk_handling_options::strict or
 * last_chunk_handling_options::stop_before_partial.
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and position of the
 * INVALID_BASE64_CHARACTER error (in the input in units) if any, or the number
 * of bytes written if successful.
 */
simdutf_warn_unused result
base64_to_binary(const char16_t *input, size_t length, char *output,
                 base64_options options = base64_default,
                 last_chunk_handling_options last_chunk_options =
                     last_chunk_handling_options::loose) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 result
base64_to_binary(
    std::span<const char16_t> input,
    detail::output_span_of_byte_like auto &&binary_output,
    base64_options options = base64_default,
    last_chunk_handling_options last_chunk_options = loose) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::base64::base64_to_binary_details_impl(
        input.data(), input.size(), binary_output.data(), options,
        last_chunk_options);
  } else
    #endif
  {
    return base64_to_binary(input.data(), input.size(),
                            reinterpret_cast<char *>(binary_output.data()),
                            options, last_chunk_options);
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert a base64 input to a binary output while returning more details
 * than base64_to_binary.
 *
 * This function follows the WHATWG forgiving-base64 format, which means that it
 * will ignore any ASCII spaces in the input. You may provide a padded input
 * (with one or two equal signs at the end) or an unpadded input (without any
 * equal signs at the end).
 *
 * See https://infra.spec.whatwg.org/#forgiving-base64-decode
 *
 * Unlike base64_to_binary, this function returns a full_result with both
 * input_count and output_count, so you always know how much input was consumed
 * and how much output was written. There are three cases where the input may
 * not be fully consumed:
 *
 * 1. stop_before_partial: When last_chunk_options is set to
 *    stop_before_partial, any incomplete 4-character group at the end of the
 *    input is left unconsumed. This is useful for streaming/chunked decoding
 *    where you can carry over the unconsumed input to the next chunk.
 *
 * 2. INVALID_BASE64_CHARACTER: The input contains a character that is not a
 *    valid base64 character. In this case, input_count indicates where the
 *    invalid character was found.
 *
 * 3. BASE64_INPUT_REMAINDER: When last_chunk_options is loose, the input
 *    contains a number of base64 characters that, when divided by 4, leaves
 *    a single remainder character (which cannot encode any bytes).
 *
 * You should call this function with a buffer that is at least
 * maximal_binary_length_from_base64(input, length) bytes long. If you fail to
 * provide that much space, the function may cause a buffer overflow.
 *
 * @param input         the base64 string to process
 * @param length        the length of the string in bytes
 * @param output        the pointer to a buffer that can hold the conversion
 * result (should be at least maximal_binary_length_from_base64(input, length)
 * bytes long).
 * @param options       the base64 options to use, can be base64_default or
 * base64_url, is base64_default by default.
 * @param last_chunk_options the last chunk handling options,
 * last_chunk_handling_options::loose by default
 * but can also be last_chunk_handling_options::strict or
 * last_chunk_handling_options::stop_before_partial.
 * @return a full_result struct (of type simdutf::full_result containing the
 * three fields error, input_count and output_count).
 */
simdutf_warn_unused full_result
base64_to_binary_details(const char *input, size_t length, char *output,
                         base64_options options = base64_default,
                         last_chunk_handling_options last_chunk_options =
                             last_chunk_handling_options::loose) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 full_result
base64_to_binary_details(
    const detail::input_span_of_byte_like auto &input,
    detail::output_span_of_byte_like auto &&binary_output,
    base64_options options = base64_default,
    last_chunk_handling_options last_chunk_options = loose) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::base64::base64_to_binary_details_impl(
        input.data(), input.size(), binary_output.data(), options,
        last_chunk_options);
  } else
    #endif
  {
    return base64_to_binary_details(
        reinterpret_cast<const char *>(input.data()), input.size(),
        reinterpret_cast<char *>(binary_output.data()), options,
        last_chunk_options);
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Convert a base64 input to a binary output while returning more details
 * than base64_to_binary.
 *
 * This function follows the WHATWG forgiving-base64 format, which means that it
 * will ignore any ASCII spaces in the input. You may provide a padded input
 * (with one or two equal signs at the end) or an unpadded input (without any
 * equal signs at the end).
 *
 * See https://infra.spec.whatwg.org/#forgiving-base64-decode
 *
 * Unlike base64_to_binary, this function returns a full_result with both
 * input_count and output_count, so you always know how much input was consumed
 * and how much output was written. There are three cases where the input may
 * not be fully consumed:
 *
 * 1. stop_before_partial: When last_chunk_options is set to
 *    stop_before_partial, any incomplete 4-character group at the end of the
 *    input is left unconsumed. This is useful for streaming/chunked decoding
 *    where you can carry over the unconsumed input to the next chunk.
 *
 * 2. INVALID_BASE64_CHARACTER: The input contains a character that is not a
 *    valid base64 character. In this case, input_count indicates where the
 *    invalid character was found.
 *
 * 3. BASE64_INPUT_REMAINDER: When last_chunk_options is loose, the input
 *    contains a number of base64 characters that, when divided by 4, leaves
 *    a single remainder character (which cannot encode any bytes).
 *
 * You should call this function with a buffer that is at least
 * maximal_binary_length_from_base64(input, length) bytes long. If you fail to
 * provide that much space, the function may cause a buffer overflow.
 *
 * @param input         the base64 string to process, in ASCII stored as 16-bit
 * units
 * @param length        the length of the string in 16-bit units
 * @param output        the pointer to a buffer that can hold the conversion
 * result (should be at least maximal_binary_length_from_base64(input, length)
 * bytes long).
 * @param options       the base64 options to use, can be base64_default or
 * base64_url, is base64_default by default.
 * @param last_chunk_options the last chunk handling options,
 * last_chunk_handling_options::loose by default
 * but can also be last_chunk_handling_options::strict or
 * last_chunk_handling_options::stop_before_partial.
 * @return a full_result struct (of type simdutf::full_result containing the
 * three fields error, input_count and output_count).
 */
simdutf_warn_unused full_result
base64_to_binary_details(const char16_t *input, size_t length, char *output,
                         base64_options options = base64_default,
                         last_chunk_handling_options last_chunk_options =
                             last_chunk_handling_options::loose) noexcept;
  #if SIMDUTF_SPAN
simdutf_really_inline simdutf_warn_unused simdutf_constexpr23 full_result
base64_to_binary_details(
    std::span<const char16_t> input,
    detail::output_span_of_byte_like auto &&binary_output,
    base64_options options = base64_default,
    last_chunk_handling_options last_chunk_options = loose) noexcept {
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    return scalar::base64::base64_to_binary_details_impl(
        input.data(), input.size(), binary_output.data(), options,
        last_chunk_options);
  } else
    #endif
  {
    return base64_to_binary_details(
        input.data(), input.size(),
        reinterpret_cast<char *>(binary_output.data()), options,
        last_chunk_options);
  }
}
  #endif // SIMDUTF_SPAN

/**
 * Check if a character is an ignorable base64 character.
 * Checking a large input, character by character, is not computationally
 * efficient.
 *
 * @param input         the character to check
 * @param options       the base64 options to use, is base64_default by default.
 * @return true if the character is an ignorable base64 character, false
 * otherwise.
 */
simdutf_warn_unused simdutf_really_inline simdutf_constexpr23 bool
base64_ignorable(char input, base64_options options = base64_default) noexcept {
  return scalar::base64::is_ignorable(input, options);
}
simdutf_warn_unused simdutf_really_inline simdutf_constexpr23 bool
base64_ignorable(char16_t input,
                 base64_options options = base64_default) noexcept {
  return scalar::base64::is_ignorable(input, options);
}

/**
 * Check if a character is a valid base64 character.
 * Checking a large input, character by character, is not computationally
 * efficient.
 * Note that padding characters are not considered valid base64 characters in
 * this context, nor are spaces.
 *
 * @param input         the character to check
 * @param options       the base64 options to use, is base64_default by default.
 * @return true if the character is a base64 character, false otherwise.
 */
simdutf_warn_unused simdutf_really_inline simdutf_constexpr23 bool
base64_valid(char input, base64_options options = base64_default) noexcept {
  return scalar::base64::is_base64(input, options);
}
simdutf_warn_unused simdutf_really_inline simdutf_constexpr23 bool
base64_valid(char16_t input, base64_options options = base64_default) noexcept {
  return scalar::base64::is_base64(input, options);
}

/**
 * Check if a character is a valid base64 character or the padding character
 * ('='). Checking a large input, character by character, is not computationally
 * efficient.
 *
 * @param input         the character to check
 * @param options       the base64 options to use, is base64_default by default.
 * @return true if the character is a base64 character, false otherwise.
 */
simdutf_warn_unused simdutf_really_inline simdutf_constexpr23 bool
base64_valid_or_padding(char input,
                        base64_options options = base64_default) noexcept {
  return scalar::base64::is_base64_or_padding(input, options);
}
simdutf_warn_unused simdutf_really_inline simdutf_constexpr23 bool
base64_valid_or_padding(char16_t input,
                        base64_options options = base64_default) noexcept {
  return scalar::base64::is_base64_or_padding(input, options);
}

/**
 * Convert a base64 input to a binary output.
 *
 * This function follows the WHATWG forgiving-base64 format, which means that it
 * will ignore any ASCII spaces in the input. You may provide a padded input
 * (with one or two equal signs at the end) or an unpadded input (without any
 * equal signs at the end).
 *
 * See https://infra.spec.whatwg.org/#forgiving-base64-decode
 *
 * This function will fail in case of invalid input. When last_chunk_options =
 * loose, there are three possible reasons for failure: the input contains a
 * number of base64 characters that when divided by 4, leaves a single remainder
 * character (BASE64_INPUT_REMAINDER), the input contains a character that is
 * not a valid base64 character (INVALID_BASE64_CHARACTER), or the output buffer
 * is too small (OUTPUT_BUFFER_TOO_SMALL).
 *
 * When OUTPUT_BUFFER_TOO_SMALL, we return both the number of bytes written
 * and the number of units processed, see description of the parameters and
 * returned value.
 *
 * When the error is INVALID_BASE64_CHARACTER, r.count contains the index in the
 * input where the invalid character was found. When the error is
 * BASE64_INPUT_REMAINDER, then r.count contains the number of bytes decoded.
 *
 * The default option (simdutf::base64_default) expects the characters `+` and
 * `/` as part of its alphabet. The URL option (simdutf::base64_url) expects the
 * characters `-` and `_` as part of its alphabet.
 *
 * The padding (`=`) is validated if present. There may be at most two padding
 * characters at the end of the input. If there are any padding characters, the
 * total number of characters (excluding spaces but including padding
 * characters) must be divisible by four.
 *
 * The INVALID_BASE64_CHARACTER cases are considered fatal and you are expected
 * to discard the output unless the parameter decode_up_to_bad_char is set to
 * true. In that case, the function will decode up to the first invalid
 * character. Extra padding characters ('=') are considered invalid characters.
 *
 * Advanced users may want to tailor how the last chunk is handled. By default,
 * we use a loose (forgiving) approach but we also support a strict approach
 * as well as a stop_before_partial approach, as per the following proposal:
 *
 * https://tc39.es/proposal-arraybuffer-base64/spec/#sec-frombase64
 *
 * The base64_to_binary_safe function has negligible overhead compared with
 * base64_to_binary in the absence of ignorable characters; however, on short
 * inputs containing ignorable characters, it can be up to three times slower.
 *
 * @param input         the base64 string to process, in ASCII stored as 8-bit
 * or 16-bit units
 * @param length        the length of the string in 8-bit or 16-bit units.
 * @param output        the pointer to a buffer that can hold the conversion
 * result.
 * @param outlen        the number of bytes that can be written in the output
 * buffer. Upon return, it is modified to reflect how many bytes were written.
 * @param options       the base64 options to use, can be base64_default or
 * base64_url, is base64_default by default.
 * @param last_chunk_options the last chunk handling options,
 * last_chunk_handling_options::loose by default
 * but can also be last_chunk_handling_options::strict or
 * last_chunk_handling_options::stop_before_partial.
 * @param decode_up_to_bad_char if true, the function will decode up to the
 * first invalid character. By default (false), it is assumed that the output
 * buffer is to be discarded. When there are multiple errors in the input,
 * using decode_up_to_bad_char might trigger a different error.
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and position of the
 * INVALID_BASE64_CHARACTER error (in the input in units) if any, or the number
 * of units processed if successful.
 */
simdutf_warn_unused result
base64_to_binary_safe(const char *input, size_t length, char *output,
                      size_t &outlen, base64_options options = base64_default,
                      last_chunk_handling_options last_chunk_options =
                          last_chunk_handling_options::loose,
                      bool decode_up_to_bad_char = false) noexcept;
// the span overload has moved to the bottom of the file

simdutf_warn_unused result
base64_to_binary_safe(const char16_t *input, size_t length, char *output,
                      size_t &outlen, base64_options options = base64_default,
                      last_chunk_handling_options last_chunk_options =
                          last_chunk_handling_options::loose,
                      bool decode_up_to_bad_char = false) noexcept;
  // span overload moved to bottom of file

  #if SIMDUTF_ATOMIC_REF
/**
 * Convert a base64 input to a binary output with a size limit and using atomic
 * operations.
 *
 * Like `base64_to_binary_safe` but using atomic operations, this function is
 * thread-safe for concurrent memory access, allowing the output
 * buffers to be shared between threads without undefined behavior in case of
 * data races.
 *
 * This function comes with a potentially significant performance penalty, but
 * is useful when thread safety is needed during base64 decoding.
 *
 * This function is only available when simdutf is compiled with
 * C++20 support and __cpp_lib_atomic_ref >= 201806L. You may check
 * the availability of this function by checking the macro
 * SIMDUTF_ATOMIC_REF.
 *
 * This function is considered experimental. It is not tested by default
 * (see the CMake option SIMDUTF_ATOMIC_BASE64_TESTS) nor is it fuzz tested.
 * It is not documented in the public API documentation (README). It is
 * offered on a best effort basis. We rely on the community for further
 * testing and feedback.
 *
 * @param input         the base64 input to decode
 * @param length        the length of the input in bytes
 * @param output        the pointer to buffer that can hold the conversion
 * result
 * @param outlen        the number of bytes that can be written in the output
 * buffer. Upon return, it is modified to reflect how many bytes were written.
 * @param options       the base64 options to use (default, url, etc.)
 * @param last_chunk_options the last chunk handling options (loose, strict,
 * stop_before_partial)
 * @param decode_up_to_bad_char if true, the function will decode up to the
 * first invalid character. By default (false), it is assumed that the output
 * buffer is to be discarded. When there are multiple errors in the input,
 * using decode_up_to_bad_char might trigger a different error.
 * @return a result struct with an error code and count indicating error
 * position or success
 */
simdutf_warn_unused result atomic_base64_to_binary_safe(
    const char *input, size_t length, char *output, size_t &outlen,
    base64_options options = base64_default,
    last_chunk_handling_options last_chunk_options =
        last_chunk_handling_options::loose,
    bool decode_up_to_bad_char = false) noexcept;
simdutf_warn_unused result atomic_base64_to_binary_safe(
    const char16_t *input, size_t length, char *output, size_t &outlen,
    base64_options options = base64_default,
    last_chunk_handling_options last_chunk_options = loose,
    bool decode_up_to_bad_char = false) noexcept;
    #if SIMDUTF_SPAN
/**
 * @brief span overload
 * @return a tuple of result and outlen
 */
simdutf_really_inline simdutf_warn_unused std::tuple<result, std::size_t>
atomic_base64_to_binary_safe(
    const detail::input_span_of_byte_like auto &binary_input,
    detail::output_span_of_byte_like auto &&output,
    base64_options options = base64_default,
    last_chunk_handling_options last_chunk_options =
        last_chunk_handling_options::loose,
    bool decode_up_to_bad_char = false) noexcept {
  size_t outlen = output.size();
  auto ret = atomic_base64_to_binary_safe(
      reinterpret_cast<const char *>(binary_input.data()), binary_input.size(),
      reinterpret_cast<char *>(output.data()), outlen, options,
      last_chunk_options, decode_up_to_bad_char);
  return {ret, outlen};
}
/**
 * @brief span overload
 * @return a tuple of result and outlen
 */
simdutf_warn_unused std::tuple<result, std::size_t>
atomic_base64_to_binary_safe(
    std::span<const char16_t> base64_input,
    detail::output_span_of_byte_like auto &&binary_output,
    base64_options options = base64_default,
    last_chunk_handling_options last_chunk_options = loose,
    bool decode_up_to_bad_char = false) noexcept {
  size_t outlen = binary_output.size();
  auto ret = atomic_base64_to_binary_safe(
      base64_input.data(), base64_input.size(),
      reinterpret_cast<char *>(binary_output.data()), outlen, options,
      last_chunk_options, decode_up_to_bad_char);
  return {ret, outlen};
}
    #endif // SIMDUTF_SPAN
  #endif   // SIMDUTF_ATOMIC_REF

#endif // SIMDUTF_FEATURE_BASE64

/**
 * An implementation of simdutf for a particular CPU architecture.
 *
 * Also used to maintain the currently active implementation. The active
 * implementation is automatically initialized on first use to the most advanced
 * implementation supported by the host.
 */
class implementation {
public:
  /**
   * The name of this implementation.
   *
   *     const implementation *impl = simdutf::active_implementation;
   *     cout << "simdutf is optimized for " << impl->name() << "(" <<
   * impl->description() << ")" << endl;
   *
   * @return the name of the implementation, e.g. "haswell", "westmere", "arm64"
   */
  virtual std::string_view name() const noexcept { return _name; }

  /**
   * The description of this implementation.
   *
   *     const implementation *impl = simdutf::active_implementation;
   *     cout << "simdutf is optimized for " << impl->name() << "(" <<
   * impl->description() << ")" << endl;
   *
   * @return the name of the implementation, e.g. "haswell", "westmere", "arm64"
   */
  virtual std::string_view description() const noexcept { return _description; }

  /**
   * The instruction sets this implementation is compiled against
   * and the current CPU match. This function may poll the current CPU/system
   * and should therefore not be called too often if performance is a concern.
   *
   *
   * @return true if the implementation can be safely used on the current system
   * (determined at runtime)
   */
  bool supported_by_runtime_system() const;

#if SIMDUTF_FEATURE_DETECT_ENCODING
  /**
   * This function will try to detect the encoding
   * @param input the string to identify
   * @param length the length of the string in bytes.
   * @return the encoding type detected
   */
  virtual encoding_type autodetect_encoding(const char *input,
                                            size_t length) const noexcept;

  /**
   * This function will try to detect the possible encodings in one pass
   * @param input the string to identify
   * @param length the length of the string in bytes.
   * @return the encoding type detected
   */
  virtual int detect_encodings(const char *input,
                               size_t length) const noexcept = 0;
#endif // SIMDUTF_FEATURE_DETECT_ENCODING

  /**
   * @private For internal implementation use
   *
   * The instruction sets this implementation is compiled against.
   *
   * @return a mask of all required `internal::instruction_set::` values
   */
  virtual uint32_t required_instruction_sets() const {
    return _required_instruction_sets;
  }

#if SIMDUTF_FEATURE_UTF8 || SIMDUTF_FEATURE_DETECT_ENCODING
  /**
   * Validate the UTF-8 string.
   *
   * Overridden by each implementation.
   *
   * @param buf the UTF-8 string to validate.
   * @param len the length of the string in bytes.
   * @return true if and only if the string is valid UTF-8.
   */
  simdutf_warn_unused virtual bool validate_utf8(const char *buf,
                                                 size_t len) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 || SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF8
  /**
   * Validate the UTF-8 string and stop on errors.
   *
   * Overridden by each implementation.
   *
   * @param buf the UTF-8 string to validate.
   * @param len the length of the string in bytes.
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of code units validated
   * if successful.
   */
  simdutf_warn_unused virtual result
  validate_utf8_with_errors(const char *buf, size_t len) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8

#if SIMDUTF_FEATURE_ASCII
  /**
   * Validate the ASCII string.
   *
   * Overridden by each implementation.
   *
   * @param buf the ASCII string to validate.
   * @param len the length of the string in bytes.
   * @return true if and only if the string is valid ASCII.
   */
  simdutf_warn_unused virtual bool
  validate_ascii(const char *buf, size_t len) const noexcept = 0;

  /**
   * Validate the ASCII string and stop on error.
   *
   * Overridden by each implementation.
   *
   * @param buf the ASCII string to validate.
   * @param len the length of the string in bytes.
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of code units validated
   * if successful.
   */
  simdutf_warn_unused virtual result
  validate_ascii_with_errors(const char *buf, size_t len) const noexcept = 0;

#endif // SIMDUTF_FEATURE_ASCII

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_ASCII
  /**
   * Validate the ASCII string as a UTF-16BE sequence.
   * An UTF-16 sequence is considered an ASCII sequence
   * if it could be converted to an ASCII string losslessly.
   *
   * Overridden by each implementation.
   *
   * @param buf the UTF-16BE string to validate.
   * @param len the length of the string in bytes.
   * @return true if and only if the string is valid ASCII.
   */
  simdutf_warn_unused virtual bool
  validate_utf16be_as_ascii(const char16_t *buf, size_t len) const noexcept = 0;

  /**
   * Validate the ASCII string as a UTF-16LE sequence.
   * An UTF-16 sequence is considered an ASCII sequence
   * if it could be converted to an ASCII string losslessly.
   *
   * Overridden by each implementation.
   *
   * @param buf the UTF-16LE string to validate.
   * @param len the length of the string in bytes.
   * @return true if and only if the string is valid ASCII.
   */
  simdutf_warn_unused virtual bool
  validate_utf16le_as_ascii(const char16_t *buf, size_t len) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_ASCII

#if SIMDUTF_FEATURE_UTF16 || SIMDUTF_FEATURE_DETECT_ENCODING
  /**
   * Validate the UTF-16LE string.This function may be best when you expect
   * the input to be almost always valid. Otherwise, consider using
   * validate_utf16le_with_errors.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-16LE string to validate.
   * @param len the length of the string in number of 2-byte code units
   * (char16_t).
   * @return true if and only if the string is valid UTF-16LE.
   */
  simdutf_warn_unused virtual bool
  validate_utf16le(const char16_t *buf, size_t len) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF16 || SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF16
  /**
   * Validate the UTF-16BE string. This function may be best when you expect
   * the input to be almost always valid. Otherwise, consider using
   * validate_utf16be_with_errors.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-16BE string to validate.
   * @param len the length of the string in number of 2-byte code units
   * (char16_t).
   * @return true if and only if the string is valid UTF-16BE.
   */
  simdutf_warn_unused virtual bool
  validate_utf16be(const char16_t *buf, size_t len) const noexcept = 0;

  /**
   * Validate the UTF-16LE string and stop on error.  It might be faster than
   * validate_utf16le when an error is expected to occur early.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-16LE string to validate.
   * @param len the length of the string in number of 2-byte code units
   * (char16_t).
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of code units validated
   * if successful.
   */
  simdutf_warn_unused virtual result
  validate_utf16le_with_errors(const char16_t *buf,
                               size_t len) const noexcept = 0;

  /**
   * Validate the UTF-16BE string and stop on error. It might be faster than
   * validate_utf16be when an error is expected to occur early.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-16BE string to validate.
   * @param len the length of the string in number of 2-byte code units
   * (char16_t).
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of code units validated
   * if successful.
   */
  simdutf_warn_unused virtual result
  validate_utf16be_with_errors(const char16_t *buf,
                               size_t len) const noexcept = 0;
  /**
   * Copies the UTF-16LE string while replacing mismatched surrogates with the
   * Unicode replacement character U+FFFD. We allow the input and output to be
   * the same buffer so that the correction is done in-place.
   *
   * Overridden by each implementation.
   *
   * @param input the UTF-16LE string to correct.
   * @param len the length of the string in number of 2-byte code units
   * (char16_t).
   * @param output the output buffer.
   */
  virtual void to_well_formed_utf16le(const char16_t *input, size_t len,
                                      char16_t *output) const noexcept = 0;
  /**
   * Copies the UTF-16BE string while replacing mismatched surrogates with the
   * Unicode replacement character U+FFFD. We allow the input and output to be
   * the same buffer so that the correction is done in-place.
   *
   * Overridden by each implementation.
   *
   * @param input the UTF-16BE string to correct.
   * @param len the length of the string in number of 2-byte code units
   * (char16_t).
   * @param output the output buffer.
   */
  virtual void to_well_formed_utf16be(const char16_t *input, size_t len,
                                      char16_t *output) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF32 || SIMDUTF_FEATURE_DETECT_ENCODING
  /**
   * Validate the UTF-32 string.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-32 string to validate.
   * @param len the length of the string in number of 4-byte code units
   * (char32_t).
   * @return true if and only if the string is valid UTF-32.
   */
  simdutf_warn_unused virtual bool
  validate_utf32(const char32_t *buf, size_t len) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF32 || SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF32
  /**
   * Validate the UTF-32 string and stop on error.
   *
   * Overridden by each implementation.
   *
   * This function is not BOM-aware.
   *
   * @param buf the UTF-32 string to validate.
   * @param len the length of the string in number of 4-byte code units
   * (char32_t).
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of code units validated
   * if successful.
   */
  simdutf_warn_unused virtual result
  validate_utf32_with_errors(const char32_t *buf,
                             size_t len) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
  /**
   * Convert Latin1 string into UTF-8 string.
   *
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the Latin1 string to convert
   * @param length        the length of the string in bytes
   * @param utf8_output  the pointer to buffer that can hold conversion result
   * @return the number of written char; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_latin1_to_utf8(const char *input, size_t length,
                         char *utf8_output) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1
  /**
   * Convert possibly Latin1 string into UTF-16LE string.
   *
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the Latin1  string to convert
   * @param length        the length of the string in bytes
   * @param utf16_output  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_latin1_to_utf16le(const char *input, size_t length,
                            char16_t *utf16_output) const noexcept = 0;

  /**
   * Convert Latin1 string into UTF-16BE string.
   *
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the Latin1 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_output  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_latin1_to_utf16be(const char *input, size_t length,
                            char16_t *utf16_output) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1
  /**
   * Convert Latin1 string into UTF-32 string.
   *
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the Latin1 string to convert
   * @param length        the length of the string in bytes
   * @param utf32_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char32_t; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_latin1_to_utf32(const char *input, size_t length,
                          char32_t *utf32_buffer) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
  /**
   * Convert possibly broken UTF-8 string into latin1 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param latin1_output  the pointer to buffer that can hold conversion result
   * @return the number of written char; 0 if the input was not valid UTF-8
   * string or if it cannot be represented as Latin1
   */
  simdutf_warn_unused virtual size_t
  convert_utf8_to_latin1(const char *input, size_t length,
                         char *latin1_output) const noexcept = 0;

  /**
   * Convert possibly broken UTF-8 string into latin1 string with errors.
   * If the string cannot be represented as Latin1, an error
   * code is returned.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param latin1_output  the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of code units validated
   * if successful.
   */
  simdutf_warn_unused virtual result
  convert_utf8_to_latin1_with_errors(const char *input, size_t length,
                                     char *latin1_output) const noexcept = 0;

  /**
   * Convert valid UTF-8 string into latin1 string.
   *
   * This function assumes that the input string is valid UTF-8 and that it can
   * be represented as Latin1. If you violate this assumption, the result is
   * implementation defined and may include system-dependent behavior such as
   * crashes.
   *
   * This function is for expert users only and not part of our public API. Use
   * convert_utf8_to_latin1 instead.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param latin1_output  the pointer to buffer that can hold conversion result
   * @return the number of written char; 0 if the input was not valid UTF-8
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf8_to_latin1(const char *input, size_t length,
                               char *latin1_output) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
  /**
   * Convert possibly broken UTF-8 string into UTF-16LE string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_output  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t; 0 if the input was not valid UTF-8
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_utf8_to_utf16le(const char *input, size_t length,
                          char16_t *utf16_output) const noexcept = 0;

  /**
   * Convert possibly broken UTF-8 string into UTF-16BE string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_output  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t; 0 if the input was not valid UTF-8
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_utf8_to_utf16be(const char *input, size_t length,
                          char16_t *utf16_output) const noexcept = 0;

  /**
   * Convert possibly broken UTF-8 string into UTF-16LE string and stop on
   * error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_output  the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of code units validated
   * if successful.
   */
  simdutf_warn_unused virtual result convert_utf8_to_utf16le_with_errors(
      const char *input, size_t length,
      char16_t *utf16_output) const noexcept = 0;

  /**
   * Convert possibly broken UTF-8 string into UTF-16BE string and stop on
   * error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_output  the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of code units validated
   * if successful.
   */
  simdutf_warn_unused virtual result convert_utf8_to_utf16be_with_errors(
      const char *input, size_t length,
      char16_t *utf16_output) const noexcept = 0;
  /**
   * Compute the number of bytes that this UTF-16LE string would require in
   * UTF-8 format even when the UTF-16LE content contains mismatched
   * surrogates that have to be replaced by the replacement character (0xFFFD).
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) where the count is the number of bytes required to
   * encode the UTF-16LE string as UTF-8, and the error code is either SUCCESS
   * or SURROGATE. The count is correct regardless of the error field.
   * When SURROGATE is returned, it does not indicate an error in the case of
   * this function: it indicates that at least one surrogate has been
   * encountered: the surrogates may be matched or not (thus this function does
   * not validate). If the returned error code is SUCCESS, then the input
   * contains no surrogate, is in the Basic Multilingual Plane, and is
   * necessarily valid.
   */
  virtual simdutf_warn_unused result utf8_length_from_utf16le_with_replacement(
      const char16_t *input, size_t length) const noexcept = 0;

  /**
   * Compute the number of bytes that this UTF-16BE string would require in
   * UTF-8 format even when the UTF-16BE content contains mismatched
   * surrogates that have to be replaced by the replacement character (0xFFFD).
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) where the count is the number of bytes required to
   * encode the UTF-16BE string as UTF-8, and the error code is either SUCCESS
   * or SURROGATE. The count is correct regardless of the error field.
   * When SURROGATE is returned, it does not indicate an error in the case of
   * this function: it indicates that at least one surrogate has been
   * encountered: the surrogates may be matched or not (thus this function does
   * not validate). If the returned error code is SUCCESS, then the input
   * contains no surrogate, is in the Basic Multilingual Plane, and is
   * necessarily valid.
   */
  virtual simdutf_warn_unused result utf8_length_from_utf16be_with_replacement(
      const char16_t *input, size_t length) const noexcept = 0;

#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
  /**
   * Convert possibly broken UTF-8 string into UTF-32 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf32_output  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t; 0 if the input was not valid UTF-8
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_utf8_to_utf32(const char *input, size_t length,
                        char32_t *utf32_output) const noexcept = 0;

  /**
   * Convert possibly broken UTF-8 string into UTF-32 string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf32_output  the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of char32_t written if
   * successful.
   */
  simdutf_warn_unused virtual result
  convert_utf8_to_utf32_with_errors(const char *input, size_t length,
                                    char32_t *utf32_output) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
  /**
   * Convert valid UTF-8 string into UTF-16LE string.
   *
   * This function assumes that the input string is valid UTF-8.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf8_to_utf16le(const char *input, size_t length,
                                char16_t *utf16_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-8 string into UTF-16BE string.
   *
   * This function assumes that the input string is valid UTF-8.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf16_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char16_t
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf8_to_utf16be(const char *input, size_t length,
                                char16_t *utf16_buffer) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
  /**
   * Convert valid UTF-8 string into UTF-32 string.
   *
   * This function assumes that the input string is valid UTF-8.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in bytes
   * @param utf32_buffer  the pointer to buffer that can hold conversion result
   * @return the number of written char32_t
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf8_to_utf32(const char *input, size_t length,
                              char32_t *utf32_buffer) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
  /**
   * Compute the number of 2-byte code units that this UTF-8 string would
   * require in UTF-16LE format.
   *
   * This function does not validate the input. It is acceptable to pass invalid
   * UTF-8 strings but in such cases the result is implementation defined.
   *
   * @param input         the UTF-8 string to process
   * @param length        the length of the string in bytes
   * @return the number of char16_t code units required to encode the UTF-8
   * string as UTF-16LE
   */
  simdutf_warn_unused virtual size_t
  utf16_length_from_utf8(const char *input, size_t length) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
  /**
   * Compute the number of 4-byte code units that this UTF-8 string would
   * require in UTF-32 format.
   *
   * This function is equivalent to count_utf8. It is acceptable to pass invalid
   * UTF-8 strings but in such cases the result is implementation defined.
   *
   * This function does not validate the input.
   *
   * @param input         the UTF-8 string to process
   * @param length        the length of the string in bytes
   * @return the number of char32_t code units required to encode the UTF-8
   * string as UTF-32
   */
  simdutf_warn_unused virtual size_t
  utf32_length_from_utf8(const char *input, size_t length) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1
  /**
   * Convert possibly broken UTF-16LE string into Latin1 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion
   * result
   * @return number of written code units; 0 if input is not a valid UTF-16LE
   * string or if it cannot be represented as Latin1
   */
  simdutf_warn_unused virtual size_t
  convert_utf16le_to_latin1(const char16_t *input, size_t length,
                            char *latin1_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into Latin1 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion
   * result
   * @return number of written code units; 0 if input is not a valid UTF-16BE
   * string or if it cannot be represented as Latin1
   */
  simdutf_warn_unused virtual size_t
  convert_utf16be_to_latin1(const char16_t *input, size_t length,
                            char *latin1_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16LE string into Latin1 string.
   * If the string cannot be represented as Latin1, an error
   * is returned.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion
   * result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of char written if
   * successful.
   */
  simdutf_warn_unused virtual result
  convert_utf16le_to_latin1_with_errors(const char16_t *input, size_t length,
                                        char *latin1_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into Latin1 string.
   * If the string cannot be represented as Latin1, an error
   * is returned.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion
   * result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of char written if
   * successful.
   */
  simdutf_warn_unused virtual result
  convert_utf16be_to_latin1_with_errors(const char16_t *input, size_t length,
                                        char *latin1_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16LE string into Latin1 string.
   *
   * This function assumes that the input string is valid UTF-L16LE and that it
   * can be represented as Latin1. If you violate this assumption, the result is
   * implementation defined and may include system-dependent behavior such as
   * crashes.
   *
   * This function is for expert users only and not part of our public API. Use
   * convert_utf16le_to_latin1 instead.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion
   * result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf16le_to_latin1(const char16_t *input, size_t length,
                                  char *latin1_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16BE string into Latin1 string.
   *
   * This function assumes that the input string is valid UTF16-BE and that it
   * can be represented as Latin1. If you violate this assumption, the result is
   * implementation defined and may include system-dependent behavior such as
   * crashes.
   *
   * This function is for expert users only and not part of our public API. Use
   * convert_utf16be_to_latin1 instead.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion
   * result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf16be_to_latin1(const char16_t *input, size_t length,
                                  char *latin1_buffer) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
  /**
   * Convert possibly broken UTF-16LE string into UTF-8 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-16LE
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_utf16le_to_utf8(const char16_t *input, size_t length,
                          char *utf8_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into UTF-8 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-16BE
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_utf16be_to_utf8(const char16_t *input, size_t length,
                          char *utf8_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16LE string into UTF-8 string and stop on
   * error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of char written if
   * successful.
   */
  simdutf_warn_unused virtual result
  convert_utf16le_to_utf8_with_errors(const char16_t *input, size_t length,
                                      char *utf8_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into UTF-8 string and stop on
   * error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of char written if
   * successful.
   */
  simdutf_warn_unused virtual result
  convert_utf16be_to_utf8_with_errors(const char16_t *input, size_t length,
                                      char *utf8_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16LE string into UTF-8 string, replacing
   * unpaired surrogates with the Unicode replacement character U+FFFD.
   *
   * This function always succeeds: unpaired surrogates are replaced with
   * U+FFFD (3 bytes in UTF-8: 0xEF 0xBF 0xBD).
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units
   */
  simdutf_warn_unused virtual size_t convert_utf16le_to_utf8_with_replacement(
      const char16_t *input, size_t length,
      char *utf8_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into UTF-8 string, replacing
   * unpaired surrogates with the Unicode replacement character U+FFFD.
   *
   * This function always succeeds: unpaired surrogates are replaced with
   * U+FFFD (3 bytes in UTF-8: 0xEF 0xBF 0xBD).
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units
   */
  simdutf_warn_unused virtual size_t convert_utf16be_to_utf8_with_replacement(
      const char16_t *input, size_t length,
      char *utf8_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16LE string into UTF-8 string.
   *
   * This function assumes that the input string is valid UTF-16LE.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf8_buffer   the pointer to a buffer that can hold the conversion
   * result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf16le_to_utf8(const char16_t *input, size_t length,
                                char *utf8_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16BE string into UTF-8 string.
   *
   * This function assumes that the input string is valid UTF-16BE.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf8_buffer   the pointer to a buffer that can hold the conversion
   * result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf16be_to_utf8(const char16_t *input, size_t length,
                                char *utf8_buffer) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
  /**
   * Convert possibly broken UTF-16LE string into UTF-32 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf32_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-16LE
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_utf16le_to_utf32(const char16_t *input, size_t length,
                           char32_t *utf32_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into UTF-32 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf32_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-16BE
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_utf16be_to_utf32(const char16_t *input, size_t length,
                           char32_t *utf32_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16LE string into UTF-32 string and stop on
   * error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf32_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of char32_t written if
   * successful.
   */
  simdutf_warn_unused virtual result convert_utf16le_to_utf32_with_errors(
      const char16_t *input, size_t length,
      char32_t *utf32_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-16BE string into UTF-32 string and stop on
   * error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf32_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of char32_t written if
   * successful.
   */
  simdutf_warn_unused virtual result convert_utf16be_to_utf32_with_errors(
      const char16_t *input, size_t length,
      char32_t *utf32_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16LE string into UTF-32 string.
   *
   * This function assumes that the input string is valid UTF-16LE.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf32_buffer   the pointer to a buffer that can hold the conversion
   * result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf16le_to_utf32(const char16_t *input, size_t length,
                                 char32_t *utf32_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-16LE string into UTF-32BE string.
   *
   * This function assumes that the input string is valid UTF-16BE.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param utf32_buffer   the pointer to a buffer that can hold the conversion
   * result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf16be_to_utf32(const char16_t *input, size_t length,
                                 char32_t *utf32_buffer) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
  /**
   * Compute the number of bytes that this UTF-16LE string would require in
   * UTF-8 format.
   *
   * This function does not validate the input. It is acceptable to pass invalid
   * UTF-16 strings but in such cases the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @return the number of bytes required to encode the UTF-16LE string as UTF-8
   */
  simdutf_warn_unused virtual size_t
  utf8_length_from_utf16le(const char16_t *input,
                           size_t length) const noexcept = 0;

  /**
   * Compute the number of bytes that this UTF-16BE string would require in
   * UTF-8 format.
   *
   * This function does not validate the input. It is acceptable to pass invalid
   * UTF-16 strings but in such cases the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @return the number of bytes required to encode the UTF-16BE string as UTF-8
   */
  simdutf_warn_unused virtual size_t
  utf8_length_from_utf16be(const char16_t *input,
                           size_t length) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1
  /**
   * Convert possibly broken UTF-32 string into Latin1 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion
   * result
   * @return number of written code units; 0 if input is not a valid UTF-32
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_utf32_to_latin1(const char32_t *input, size_t length,
                          char *latin1_buffer) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1
  /**
   * Convert possibly broken UTF-32 string into Latin1 string and stop on error.
   * If the string cannot be represented as Latin1, an error is returned.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param latin1_buffer   the pointer to buffer that can hold conversion
   * result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of char written if
   * successful.
   */
  simdutf_warn_unused virtual result
  convert_utf32_to_latin1_with_errors(const char32_t *input, size_t length,
                                      char *latin1_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-32 string into Latin1 string.
   *
   * This function assumes that the input string is valid UTF-32 and can be
   * represented as Latin1. If you violate this assumption, the result is
   * implementation defined and may include system-dependent behavior such as
   * crashes.
   *
   * This function is for expert users only and not part of our public API. Use
   * convert_utf32_to_latin1 instead.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param latin1_buffer   the pointer to a buffer that can hold the conversion
   * result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf32_to_latin1(const char32_t *input, size_t length,
                                char *latin1_buffer) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
  /**
   * Convert possibly broken UTF-32 string into UTF-8 string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-32
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_utf32_to_utf8(const char32_t *input, size_t length,
                        char *utf8_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into UTF-8 string and stop on error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param utf8_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of char written if
   * successful.
   */
  simdutf_warn_unused virtual result
  convert_utf32_to_utf8_with_errors(const char32_t *input, size_t length,
                                    char *utf8_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-32 string into UTF-8 string.
   *
   * This function assumes that the input string is valid UTF-32.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param utf8_buffer   the pointer to a buffer that can hold the conversion
   * result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf32_to_utf8(const char32_t *input, size_t length,
                              char *utf8_buffer) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1
  /**
   * Return the number of bytes that this UTF-16 string would require in Latin1
   * format.
   *
   *
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @return the number of bytes required to encode the UTF-16 string as Latin1
   */
  simdutf_warn_unused virtual size_t
  utf16_length_from_latin1(size_t length) const noexcept {
    return length;
  }
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
  /**
   * Convert possibly broken UTF-32 string into UTF-16LE string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param utf16_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-32
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_utf32_to_utf16le(const char32_t *input, size_t length,
                           char16_t *utf16_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into UTF-16BE string.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param utf16_buffer   the pointer to buffer that can hold conversion result
   * @return number of written code units; 0 if input is not a valid UTF-32
   * string
   */
  simdutf_warn_unused virtual size_t
  convert_utf32_to_utf16be(const char32_t *input, size_t length,
                           char16_t *utf16_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into UTF-16LE string and stop on
   * error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param utf16_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of char16_t written if
   * successful.
   */
  simdutf_warn_unused virtual result convert_utf32_to_utf16le_with_errors(
      const char32_t *input, size_t length,
      char16_t *utf16_buffer) const noexcept = 0;

  /**
   * Convert possibly broken UTF-32 string into UTF-16BE string and stop on
   * error.
   *
   * During the conversion also validation of the input string is done.
   * This function is suitable to work with inputs from untrusted sources.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param utf16_buffer   the pointer to buffer that can hold conversion result
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in code units) if any, or the number of char16_t written if
   * successful.
   */
  simdutf_warn_unused virtual result convert_utf32_to_utf16be_with_errors(
      const char32_t *input, size_t length,
      char16_t *utf16_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-32 string into UTF-16LE string.
   *
   * This function assumes that the input string is valid UTF-32.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param utf16_buffer   the pointer to a buffer that can hold the conversion
   * result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf32_to_utf16le(const char32_t *input, size_t length,
                                 char16_t *utf16_buffer) const noexcept = 0;

  /**
   * Convert valid UTF-32 string into UTF-16BE string.
   *
   * This function assumes that the input string is valid UTF-32.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @param utf16_buffer   the pointer to a buffer that can hold the conversion
   * result
   * @return number of written code units; 0 if conversion is not possible
   */
  simdutf_warn_unused virtual size_t
  convert_valid_utf32_to_utf16be(const char32_t *input, size_t length,
                                 char16_t *utf16_buffer) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16
  /**
   * Change the endianness of the input. Can be used to go from UTF-16LE to
   * UTF-16BE or from UTF-16BE to UTF-16LE.
   *
   * This function does not validate the input.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16 string to process
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @param output        the pointer to a buffer that can hold the conversion
   * result
   */
  virtual void change_endianness_utf16(const char16_t *input, size_t length,
                                       char16_t *output) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
  /**
   * Return the number of bytes that this Latin1 string would require in UTF-8
   * format.
   *
   * @param input         the Latin1 string to convert
   * @param length        the length of the string bytes
   * @return the number of bytes required to encode the Latin1 string as UTF-8
   */
  simdutf_warn_unused virtual size_t
  utf8_length_from_latin1(const char *input, size_t length) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
  /**
   * Compute the number of bytes that this UTF-32 string would require in UTF-8
   * format.
   *
   * This function does not validate the input. It is acceptable to pass invalid
   * UTF-32 strings but in such cases the result is implementation defined.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @return the number of bytes required to encode the UTF-32 string as UTF-8
   */
  simdutf_warn_unused virtual size_t
  utf8_length_from_utf32(const char32_t *input,
                         size_t length) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1
  /**
   * Compute the number of bytes that this UTF-32 string would require in Latin1
   * format.
   *
   * This function does not validate the input. It is acceptable to pass invalid
   * UTF-32 strings but in such cases the result is implementation defined.
   *
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @return the number of bytes required to encode the UTF-32 string as Latin1
   */
  simdutf_warn_unused virtual size_t
  latin1_length_from_utf32(size_t length) const noexcept {
    return length;
  }
#endif // SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
  /**
   * Compute the number of bytes that this UTF-8 string would require in Latin1
   * format.
   *
   * This function does not validate the input. It is acceptable to pass invalid
   * UTF-8 strings but in such cases the result is implementation defined.
   *
   * @param input         the UTF-8 string to convert
   * @param length        the length of the string in byte
   * @return the number of bytes required to encode the UTF-8 string as Latin1
   */
  simdutf_warn_unused virtual size_t
  latin1_length_from_utf8(const char *input, size_t length) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1
  /**
   * Compute the number of bytes that this UTF-16LE/BE string would require in
   * Latin1 format.
   *
   * This function does not validate the input. It is acceptable to pass invalid
   * UTF-16 strings but in such cases the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @return the number of bytes required to encode the UTF-16LE string as
   * Latin1
   */
  simdutf_warn_unused virtual size_t
  latin1_length_from_utf16(size_t length) const noexcept {
    return length;
  }
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
  /**
   * Compute the number of two-byte code units that this UTF-32 string would
   * require in UTF-16 format.
   *
   * This function does not validate the input. It is acceptable to pass invalid
   * UTF-32 strings but in such cases the result is implementation defined.
   *
   * @param input         the UTF-32 string to convert
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @return the number of bytes required to encode the UTF-32 string as UTF-16
   */
  simdutf_warn_unused virtual size_t
  utf16_length_from_utf32(const char32_t *input,
                          size_t length) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1
  /**
   * Return the number of bytes that this UTF-32 string would require in Latin1
   * format.
   *
   * @param length        the length of the string in 4-byte code units
   * (char32_t)
   * @return the number of bytes required to encode the UTF-32 string as Latin1
   */
  simdutf_warn_unused virtual size_t
  utf32_length_from_latin1(size_t length) const noexcept {
    return length;
  }
#endif // SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
  /**
   * Compute the number of bytes that this UTF-16LE string would require in
   * UTF-32 format.
   *
   * This function is equivalent to count_utf16le.
   *
   * This function does not validate the input. It is acceptable to pass invalid
   * UTF-16 strings but in such cases the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @return the number of bytes required to encode the UTF-16LE string as
   * UTF-32
   */
  simdutf_warn_unused virtual size_t
  utf32_length_from_utf16le(const char16_t *input,
                            size_t length) const noexcept = 0;

  /**
   * Compute the number of bytes that this UTF-16BE string would require in
   * UTF-32 format.
   *
   * This function is equivalent to count_utf16be.
   *
   * This function does not validate the input. It is acceptable to pass invalid
   * UTF-16 strings but in such cases the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to convert
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @return the number of bytes required to encode the UTF-16BE string as
   * UTF-32
   */
  simdutf_warn_unused virtual size_t
  utf32_length_from_utf16be(const char16_t *input,
                            size_t length) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16
  /**
   * Count the number of code points (characters) in the string assuming that
   * it is valid.
   *
   * This function assumes that the input string is valid UTF-16LE.
   * It is acceptable to pass invalid UTF-16 strings but in such cases
   * the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16LE string to process
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @return number of code points
   */
  simdutf_warn_unused virtual size_t
  count_utf16le(const char16_t *input, size_t length) const noexcept = 0;

  /**
   * Count the number of code points (characters) in the string assuming that
   * it is valid.
   *
   * This function assumes that the input string is valid UTF-16BE.
   * It is acceptable to pass invalid UTF-16 strings but in such cases
   * the result is implementation defined.
   *
   * This function is not BOM-aware.
   *
   * @param input         the UTF-16BE string to process
   * @param length        the length of the string in 2-byte code units
   * (char16_t)
   * @return number of code points
   */
  simdutf_warn_unused virtual size_t
  count_utf16be(const char16_t *input, size_t length) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8
  /**
   * Count the number of code points (characters) in the string assuming that
   * it is valid.
   *
   * This function assumes that the input string is valid UTF-8.
   * It is acceptable to pass invalid UTF-8 strings but in such cases
   * the result is implementation defined.
   *
   * @param input         the UTF-8 string to process
   * @param length        the length of the string in bytes
   * @return number of code points
   */
  simdutf_warn_unused virtual size_t
  count_utf8(const char *input, size_t length) const noexcept = 0;
#endif // SIMDUTF_FEATURE_UTF8

#if SIMDUTF_FEATURE_BASE64
  /**
   * Provide the maximal binary length in bytes given the base64 input.
   * As long as the input does not contain ignorable characters (e.g., ASCII
   * spaces or linefeed characters), the result is exact. In particular, the
   * function checks for padding characters.
   *
   * The function is fast (constant time). It checks up to two characters at
   * the end of the string. The input is not otherwise validated or read..
   *
   * @param input         the base64 input to process
   * @param length        the length of the base64 input in bytes
   * @return maximal number of binary bytes
   */
  simdutf_warn_unused size_t maximal_binary_length_from_base64(
      const char *input, size_t length) const noexcept;

  /**
   * Provide the maximal binary length in bytes given the base64 input.
   * As long as the input does not contain ignorable characters (e.g., ASCII
   * spaces or linefeed characters), the result is exact. In particular, the
   * function checks for padding characters.
   *
   * The function is fast (constant time). It checks up to two characters at
   * the end of the string. The input is not otherwise validated or read.
   *
   * @param input         the base64 input to process, in ASCII stored as 16-bit
   * units
   * @param length        the length of the base64 input in 16-bit units
   * @return maximal number of binary bytes
   */
  simdutf_warn_unused size_t maximal_binary_length_from_base64(
      const char16_t *input, size_t length) const noexcept;

  /**
   * Compute the binary length from a base64 input with ASCII spaces.
   * This function is useful for well-formed base64 inputs that may contain
   * ASCII spaces (such as line breaks). For such inputs, the result is exact.
   *
   * The function counts non-whitespace characters (ASCII value > 0x20) and
   * subtracts padding characters ('=') found at the end.
   *
   * @param input         the base64 input to process
   * @param length        the length of the base64 input in bytes
   * @return number of binary bytes
   */
  simdutf_warn_unused virtual size_t
  binary_length_from_base64(const char *input, size_t length) const noexcept;

  /**
   * Compute the binary length from a base64 input with ASCII spaces.
   * This function is useful for well-formed base64 inputs that may contain
   * ASCII spaces (such as line breaks). For such inputs, the result is exact.
   *
   * The function counts non-whitespace characters (ASCII value > 0x20) and
   * subtracts padding characters ('=') found at the end.
   *
   * @param input         the base64 input to process, in ASCII stored as 16-bit
   * units
   * @param length        the length of the base64 input in 16-bit units
   * @return number of binary bytes
   */
  simdutf_warn_unused virtual size_t
  binary_length_from_base64(const char16_t *input,
                            size_t length) const noexcept;

  /**
   * Convert a base64 input to a binary output.
   *
   * This function follows the WHATWG forgiving-base64 format, which means that
   * it will ignore any ASCII spaces in the input. You may provide a padded
   * input (with one or two equal signs at the end) or an unpadded input
   * (without any equal signs at the end).
   *
   * See https://infra.spec.whatwg.org/#forgiving-base64-decode
   *
   * This function will fail in case of invalid input. When last_chunk_options =
   * loose, there are two possible reasons for failure: the input contains a
   * number of base64 characters that when divided by 4, leaves a single
   * remainder character (BASE64_INPUT_REMAINDER), or the input contains a
   * character that is not a valid base64 character (INVALID_BASE64_CHARACTER).
   *
   * You should call this function with a buffer that is at least
   * maximal_binary_length_from_base64(input, length) bytes long. If you fail to
   * provide that much space, the function may cause a buffer overflow.
   *
   * @param input         the base64 string to process
   * @param length        the length of the string in bytes
   * @param output        the pointer to a buffer that can hold the conversion
   * result (should be at least maximal_binary_length_from_base64(input, length)
   * bytes long).
   * @param options       the base64 options to use, can be base64_default or
   * base64_url, is base64_default by default.
   * @param last_chunk_options the handling of the last chunk (default: loose)
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and either position of the error
   * (in the input in bytes) if any, or the number of bytes written if
   * successful.
   */
  simdutf_warn_unused virtual result
  base64_to_binary(const char *input, size_t length, char *output,
                   base64_options options = base64_default,
                   last_chunk_handling_options last_chunk_options =
                       last_chunk_handling_options::loose) const noexcept = 0;

  /**
   * Convert a base64 input to a binary output while returning more details
   * than base64_to_binary.
   *
   * This function follows the WHATWG forgiving-base64 format, which means that
   * it will ignore any ASCII spaces in the input. You may provide a padded
   * input (with one or two equal signs at the end) or an unpadded input
   * (without any equal signs at the end).
   *
   * See https://infra.spec.whatwg.org/#forgiving-base64-decode
   *
   * This function will fail in case of invalid input. When last_chunk_options =
   * loose, there are two possible reasons for failure: the input contains a
   * number of base64 characters that when divided by 4, leaves a single
   * remainder character (BASE64_INPUT_REMAINDER), or the input contains a
   * character that is not a valid base64 character (INVALID_BASE64_CHARACTER).
   *
   * You should call this function with a buffer that is at least
   * maximal_binary_length_from_base64(input, length) bytes long. If you fail to
   * provide that much space, the function may cause a buffer overflow.
   *
   * @param input         the base64 string to process
   * @param length        the length of the string in bytes
   * @param output        the pointer to a buffer that can hold the conversion
   * result (should be at least maximal_binary_length_from_base64(input, length)
   * bytes long).
   * @param options       the base64 options to use, can be base64_default or
   * base64_url, is base64_default by default.
   * @param last_chunk_options the handling of the last chunk (default: loose)
   * @return a full_result pair struct (of type simdutf::result containing the
   * three fields error, input_count and output_count).
   */
  simdutf_warn_unused virtual full_result base64_to_binary_details(
      const char *input, size_t length, char *output,
      base64_options options = base64_default,
      last_chunk_handling_options last_chunk_options =
          last_chunk_handling_options::loose) const noexcept = 0;

  /**
   * Convert a base64 input to a binary output.
   *
   * This function follows the WHATWG forgiving-base64 format, which means that
   * it will ignore any ASCII spaces in the input. You may provide a padded
   * input (with one or two equal signs at the end) or an unpadded input
   * (without any equal signs at the end).
   *
   * See https://infra.spec.whatwg.org/#forgiving-base64-decode
   *
   * This function will fail in case of invalid input. When last_chunk_options =
   * loose, there are two possible reasons for failure: the input contains a
   * number of base64 characters that when divided by 4, leaves a single
   * remainder character (BASE64_INPUT_REMAINDER), or the input contains a
   * character that is not a valid base64 character (INVALID_BASE64_CHARACTER).
   *
   * You should call this function with a buffer that is at least
   * maximal_binary_length_from_base64(input, length) bytes long. If you
   * fail to provide that much space, the function may cause a buffer overflow.
   *
   * @param input         the base64 string to process, in ASCII stored as
   * 16-bit units
   * @param length        the length of the string in 16-bit units
   * @param output        the pointer to a buffer that can hold the conversion
   * result (should be at least maximal_binary_length_from_base64(input, length)
   * bytes long).
   * @param options       the base64 options to use, can be base64_default or
   * base64_url, is base64_default by default.
   * @param last_chunk_options the handling of the last chunk (default: loose)
   * @return a result pair struct (of type simdutf::result containing the two
   * fields error and count) with an error code and position of the
   * INVALID_BASE64_CHARACTER error (in the input in units) if any, or the
   * number of bytes written if successful.
   */
  simdutf_warn_unused virtual result
  base64_to_binary(const char16_t *input, size_t length, char *output,
                   base64_options options = base64_default,
                   last_chunk_handling_options last_chunk_options =
                       last_chunk_handling_options::loose) const noexcept = 0;

  /**
   * Convert a base64 input to a binary output while returning more details
   * than base64_to_binary.
   *
   * This function follows the WHATWG forgiving-base64 format, which means that
   * it will ignore any ASCII spaces in the input. You may provide a padded
   * input (with one or two equal signs at the end) or an unpadded input
   * (without any equal signs at the end).
   *
   * See https://infra.spec.whatwg.org/#forgiving-base64-decode
   *
   * This function will fail in case of invalid input. When last_chunk_options =
   * loose, there are two possible reasons for failure: the input contains a
   * number of base64 characters that when divided by 4, leaves a single
   * remainder character (BASE64_INPUT_REMAINDER), or the input contains a
   * character that is not a valid base64 character (INVALID_BASE64_CHARACTER).
   *
   * You should call this function with a buffer that is at least
   * maximal_binary_length_from_base64(input, length) bytes long. If you fail to
   * provide that much space, the function may cause a buffer overflow.
   *
   * @param input         the base64 string to process
   * @param length        the length of the string in bytes
   * @param output        the pointer to a buffer that can hold the conversion
   * result (should be at least maximal_binary_length_from_base64(input, length)
   * bytes long).
   * @param options       the base64 options to use, can be base64_default or
   * base64_url, is base64_default by default.
   * @param last_chunk_options the handling of the last chunk (default: loose)
   * @return a full_result pair struct (of type simdutf::result containing the
   * three fields error, input_count and output_count).
   */
  simdutf_warn_unused virtual full_result base64_to_binary_details(
      const char16_t *input, size_t length, char *output,
      base64_options options = base64_default,
      last_chunk_handling_options last_chunk_options =
          last_chunk_handling_options::loose) const noexcept = 0;

  /**
   * Provide the base64 length in bytes given the length of a binary input.
   *
   * @param length        the length of the input in bytes
   * @param options       the base64 options to use, can be base64_default or
   * base64_url, is base64_default by default.
   * @return number of base64 bytes
   */
  simdutf_warn_unused size_t base64_length_from_binary(
      size_t length, base64_options options = base64_default) const noexcept;

  /**
   * Convert a binary input to a base64 output.
   *
   * The default option (simdutf::base64_default) uses the characters `+` and
   * `/` as part of its alphabet. Further, it adds padding (`=`) at the end of
   * the output to ensure that the output length is a multiple of four.
   *
   * The URL option (simdutf::base64_url) uses the characters `-` and `_` as
   * part of its alphabet. No padding is added at the end of the output.
   *
   * This function always succeeds.
   *
   * @param input         the binary to process
   * @param length        the length of the input in bytes
   * @param output        the pointer to a buffer that can hold the conversion
   * result (should be at least base64_length_from_binary(length) bytes long)
   * @param options       the base64 options to use, can be base64_default or
   * base64_url, is base64_default by default.
   * @return number of written bytes, will be equal to
   * base64_length_from_binary(length, options)
   */
  virtual size_t
  binary_to_base64(const char *input, size_t length, char *output,
                   base64_options options = base64_default) const noexcept = 0;

  /**
   * Convert a binary input to a base64 output with lines of given length.
   * Lines are separated by a single linefeed character.
   *
   * The default option (simdutf::base64_default) uses the characters `+` and
   * `/` as part of its alphabet. Further, it adds padding (`=`) at the end of
   * the output to ensure that the output length is a multiple of four.
   *
   * The URL option (simdutf::base64_url) uses the characters `-` and `_` as
   * part of its alphabet. No padding is added at the end of the output.
   *
   * This function always succeeds.
   *
   * @param input         the binary to process
   * @param length        the length of the input in bytes
   * @param output        the pointer to a buffer that can hold the conversion
   * result (should be at least base64_length_from_binary_with_lines(length,
   * options, line_length) bytes long)
   * @param line_length   the length of each line, values smaller than 4 are
   * interpreted as 4
   * @param options       the base64 options to use, can be base64_default or
   * base64_url, is base64_default by default.
   * @return number of written bytes, will be equal to
   * base64_length_from_binary_with_lines(length, options, line_length)
   */
  virtual size_t binary_to_base64_with_lines(
      const char *input, size_t length, char *output,
      size_t line_length = simdutf::default_line_length,
      base64_options options = base64_default) const noexcept = 0;

  /**
   * Find the first occurrence of a character in a string. If the character is
   * not found, return a pointer to the end of the string.
   * @param start        the start of the string
   * @param end          the end of the string
   * @param character    the character to find
   * @return a pointer to the first occurrence of the character in the string,
   * or a pointer to the end of the string if the character is not found.
   *
   */
  virtual const char *find(const char *start, const char *end,
                           char character) const noexcept = 0;
  virtual const char16_t *find(const char16_t *start, const char16_t *end,
                               char16_t character) const noexcept = 0;
#endif // SIMDUTF_FEATURE_BASE64

#ifdef SIMDUTF_INTERNAL_TESTS
  // This method is exported only in developer mode, its purpose
  // is to expose some internal test procedures from the given
  // implementation and then use them through our standard test
  // framework.
  //
  // Regular users should not use it, the tests of the public
  // API are enough.

  struct TestProcedure {
    // display name
    std::string_view name;

    // procedure should return whether given test pass or not
    void (*procedure)(const implementation &);
  };

  virtual std::vector<TestProcedure> internal_tests() const;
#endif

protected:
  /** @private Construct an implementation with the given name and description.
   * For subclasses.
   * @param name the name of this implementation
   * @param description a description of this implementation
   * @param required_instruction_sets the instruction sets this implementation
   * requires
   */
  simdutf_really_inline implementation(const char *name,
                                       const char *description,
                                       uint32_t required_instruction_sets)
      : _name(name), _description(description),
        _required_instruction_sets(required_instruction_sets) {}

protected:
  ~implementation() = default;

private:
  /**
   * The name of this implementation.
   */
  const char *_name;

  /**
   * The description of this implementation.
   */
  const char *_description;

  /**
   * Instruction sets required for this implementation.
   */
  const uint32_t _required_instruction_sets;
};

/** @private */
namespace internal {

/**
 * The list of available implementations compiled into simdutf.
 */
class available_implementation_list {
public:
  /** Get the list of available implementations compiled into simdutf */
  simdutf_really_inline available_implementation_list() {}
  /** Number of implementations */
  size_t size() const noexcept;
  /** STL const begin() iterator */
  const implementation *const *begin() const noexcept;
  /** STL const end() iterator */
  const implementation *const *end() const noexcept;

  /**
   * Get the implementation with the given name.
   *
   * Case sensitive.
   *
   *     const implementation *impl =
   * simdutf::available_implementations["westmere"]; if (!impl) { exit(1); } if
   * (!imp->supported_by_runtime_system()) { exit(1); }
   *     simdutf::active_implementation = impl;
   *
   * @param name the implementation to find, e.g. "westmere", "haswell", "arm64"
   * @return the implementation, or nullptr if the parse failed.
   */
  const implementation *operator[](std::string_view name) const noexcept {
    for (const implementation *impl : *this) {
      if (impl->name() == name) {
        return impl;
      }
    }
    return nullptr;
  }

  /**
   * Detect the most advanced implementation supported by the current host.
   *
   * This is used to initialize the implementation on startup.
   *
   *     const implementation *impl =
   * simdutf::available_implementation::detect_best_supported();
   *     simdutf::active_implementation = impl;
   *
   * @return the most advanced supported implementation for the current host, or
   * an implementation that returns UNSUPPORTED_ARCHITECTURE if there is no
   * supported implementation. Will never return nullptr.
   */
  const implementation *detect_best_supported() const noexcept;
};

template <typename T> class atomic_ptr {
public:
  atomic_ptr(T *_ptr) : ptr{_ptr} {}

#if defined(SIMDUTF_NO_THREADS)
  operator const T *() const { return ptr; }
  const T &operator*() const { return *ptr; }
  const T *operator->() const { return ptr; }

  operator T *() { return ptr; }
  T &operator*() { return *ptr; }
  T *operator->() { return ptr; }
  atomic_ptr &operator=(T *_ptr) {
    ptr = _ptr;
    return *this;
  }

#else
  operator const T *() const { return ptr.load(); }
  const T &operator*() const { return *ptr; }
  const T *operator->() const { return ptr.load(); }

  operator T *() { return ptr.load(); }
  T &operator*() { return *ptr; }
  T *operator->() { return ptr.load(); }
  atomic_ptr &operator=(T *_ptr) {
    ptr = _ptr;
    return *this;
  }

#endif

private:
#if defined(SIMDUTF_NO_THREADS)
  T *ptr;
#else
  std::atomic<T *> ptr;
#endif
};

class detect_best_supported_implementation_on_first_use;

} // namespace internal

/**
 * The list of available implementations compiled into simdutf.
 */
extern SIMDUTF_DLLIMPORTEXPORT const internal::available_implementation_list &
get_available_implementations();

/**
 * The active implementation.
 *
 * Automatically initialized on first use to the most advanced implementation
 * supported by this hardware.
 */
extern SIMDUTF_DLLIMPORTEXPORT internal::atomic_ptr<const implementation> &
get_active_implementation();

} // namespace simdutf

#if SIMDUTF_FEATURE_BASE64
  // this header is not part of the public api
/* begin file include/simdutf/base64_implementation.h */
#ifndef SIMDUTF_BASE64_IMPLEMENTATION_H
#define SIMDUTF_BASE64_IMPLEMENTATION_H

// this is not part of the public api

#include <type_traits> // for is_same

namespace simdutf {

template <typename chartype>
simdutf_warn_unused simdutf_constexpr23 result slow_base64_to_binary_safe_impl(
    const chartype *input, size_t length, char *output, size_t &outlen,
    base64_options options,
    last_chunk_handling_options last_chunk_options) noexcept {
  const bool ignore_garbage = (options & base64_default_accept_garbage) != 0;
  auto ri = simdutf::scalar::base64::find_end(input, length, options);
  size_t equallocation = ri.equallocation;
  size_t equalsigns = ri.equalsigns;
  length = ri.srclen;
  size_t full_input_length = ri.full_input_length;
  (void)full_input_length;
  if (length == 0) {
    outlen = 0;
    if (!ignore_garbage && equalsigns > 0) {
      return {INVALID_BASE64_CHARACTER, equallocation};
    }
    return {SUCCESS, 0};
  }

  // The parameters of base64_tail_decode_safe are:
  // - dst: the output buffer
  // - outlen: the size of the output buffer
  // - srcr: the input buffer
  // - length: the size of the input buffer
  // - padded_characters: the number of padding characters
  // - options: the options for the base64 decoder
  // - last_chunk_options: the options for the last chunk
  // The function will return the number of bytes written to the output buffer
  // and the number of bytes read from the input buffer.
  // The function will also return an error code if the input buffer is not
  // valid base64.
  full_result r = scalar::base64::base64_tail_decode_safe(
      output, outlen, input, length, equalsigns, options, last_chunk_options);
  r = scalar::base64::patch_tail_result(r, 0, 0, equallocation,
                                        full_input_length, last_chunk_options);
  outlen = r.output_count;
  if (!is_partial(last_chunk_options) && r.error == error_code::SUCCESS &&
      equalsigns > 0) {
    // additional checks
    if ((outlen % 3 == 0) || ((outlen % 3) + 1 + equalsigns != 4)) {
      r.error = error_code::INVALID_BASE64_CHARACTER;
    }
  }
  return {r.error, r.input_count}; // we cannot return r itself because it gets
                                   // converted to error/output_count
}

template <typename chartype>
simdutf_warn_unused simdutf_constexpr23 result base64_to_binary_safe_impl(
    const chartype *input, size_t length, char *output, size_t &outlen,
    base64_options options,
    last_chunk_handling_options last_chunk_handling_options,
    bool decode_up_to_bad_char) noexcept {
  static_assert(std::is_same<chartype, char>::value ||
                    std::is_same<chartype, char16_t>::value,
                "Only char and char16_t are supported.");
  size_t remaining_input_length = length;
  size_t remaining_output_length = outlen;
  size_t input_position = 0;
  size_t output_position = 0;

  // We also do a first pass using the fast path to decode as much as possible
  size_t safe_input = detail::min(
      remaining_input_length,
      base64_length_from_binary(remaining_output_length / 3 * 3, options));
  bool done_with_partial = (safe_input == remaining_input_length);
  simdutf::full_result r;

#if SIMDUTF_CPLUSPLUS23
  if consteval {
    r = scalar::base64::base64_to_binary_details_impl(
        input + input_position, safe_input, output + output_position, options,
        done_with_partial
            ? last_chunk_handling_options
            : simdutf::last_chunk_handling_options::only_full_chunks);
  } else
#endif
  {
    r = get_active_implementation()->base64_to_binary_details(
        input + input_position, safe_input, output + output_position, options,
        done_with_partial
            ? last_chunk_handling_options
            : simdutf::last_chunk_handling_options::only_full_chunks);
  }
  simdutf_log_assert(r.input_count <= safe_input,
                     "You should not read more than safe_input");
  simdutf_log_assert(r.output_count <= remaining_output_length,
                     "You should not write more than remaining_output_length");
  // Technically redundant, but we want to be explicit about it.
  input_position += r.input_count;
  output_position += r.output_count;
  remaining_input_length -= r.input_count;
  remaining_output_length -= r.output_count;
  if (r.error != simdutf::error_code::SUCCESS) {
    // There is an error. We return.
    if (decode_up_to_bad_char &&
        r.error == error_code::INVALID_BASE64_CHARACTER) {
      return slow_base64_to_binary_safe_impl(
          input, length, output, outlen, options, last_chunk_handling_options);
    }
    outlen = output_position;
    return {r.error, input_position};
  }

  if (done_with_partial) {
    // We are done. We have decoded everything.
    outlen = output_position;
    return {simdutf::error_code::SUCCESS, input_position};
  }
  // We have decoded some data, but we still have some data to decode.
  // We need to decode the rest of the input buffer.
  r = simdutf::scalar::base64::base64_to_binary_details_safe_impl(
      input + input_position, remaining_input_length, output + output_position,
      remaining_output_length, options, last_chunk_handling_options);
  input_position += r.input_count;
  output_position += r.output_count;
  remaining_input_length -= r.input_count;
  remaining_output_length -= r.output_count;

  if (r.error != simdutf::error_code::SUCCESS) {
    // There is an error. We return.
    if (decode_up_to_bad_char &&
        r.error == error_code::INVALID_BASE64_CHARACTER) {
      return slow_base64_to_binary_safe_impl(
          input, length, output, outlen, options, last_chunk_handling_options);
    }
    outlen = output_position;
    return {r.error, input_position};
  }
  if (input_position < length) {
    // We cannot process the entire input in one go, so we need to
    // process it in two steps: first the fast path, then the slow path.
    // In some cases, the processing might 'eat up' trailing ignorable
    // characters in the fast path, but that can be a problem.
    // suppose we have just white space followed by a single base64 character.
    // If we first process the white space with the fast path, it will
    // eat all of it. But, by the JavaScript standard, we should consume
    // no character. See
    // https://tc39.es/proposal-arraybuffer-base64/spec/#sec-frombase64
    while (input_position > 0 &&
           base64_ignorable(input[input_position - 1], options)) {
      input_position--;
    }
  }
  outlen = output_position;
  return {simdutf::error_code::SUCCESS, input_position};
}

} // namespace simdutf
#endif // SIMDUTF_BASE64_IMPLEMENTATION_H
/* end file include/simdutf/base64_implementation.h */

namespace simdutf {
  #if SIMDUTF_SPAN
/**
 * @brief span overload
 * @return a tuple of result and outlen
 */
simdutf_really_inline
    simdutf_constexpr23 simdutf_warn_unused std::tuple<result, std::size_t>
    base64_to_binary_safe(
        const detail::input_span_of_byte_like auto &input,
        detail::output_span_of_byte_like auto &&binary_output,
        base64_options options = base64_default,
        last_chunk_handling_options last_chunk_options = loose,
        bool decode_up_to_bad_char = false) noexcept {
  size_t outlen = binary_output.size();
    #if SIMDUTF_CPLUSPLUS23
  if consteval {
    using CInput = std::decay_t<decltype(*input.data())>;
    static_assert(std::is_same_v<CInput, char>,
                  "sorry, the constexpr implementation is for now limited to "
                  "input of type char");
    using COutput = std::decay_t<decltype(*binary_output.data())>;
    static_assert(std::is_same_v<COutput, char>,
                  "sorry, the constexpr implementation is for now limited to "
                  "output of type char");
    auto r = base64_to_binary_safe_impl(
        input.data(), input.size(), binary_output.data(), outlen, options,
        last_chunk_options, decode_up_to_bad_char);
    return {r, outlen};
  } else
    #endif
  {
    auto r = base64_to_binary_safe_impl<char>(
        reinterpret_cast<const char *>(input.data()), input.size(),
        reinterpret_cast<char *>(binary_output.data()), outlen, options,
        last_chunk_options, decode_up_to_bad_char);
    return {r, outlen};
  }
}

    #if SIMDUTF_SPAN
/**
 * @brief span overload
 * @return a tuple of result and outlen
 */
simdutf_really_inline
    simdutf_warn_unused simdutf_constexpr23 std::tuple<result, std::size_t>
    base64_to_binary_safe(
        std::span<const char16_t> input,
        detail::output_span_of_byte_like auto &&binary_output,
        base64_options options = base64_default,
        last_chunk_handling_options last_chunk_options = loose,
        bool decode_up_to_bad_char = false) noexcept {
  size_t outlen = binary_output.size();
      #if SIMDUTF_CPLUSPLUS23
  if consteval {
    auto r = base64_to_binary_safe_impl(
        input.data(), input.size(), binary_output.data(), outlen, options,
        last_chunk_options, decode_up_to_bad_char);
    return {r, outlen};
  } else
      #endif
  {
    auto r = base64_to_binary_safe(
        input.data(), input.size(),
        reinterpret_cast<char *>(binary_output.data()), outlen, options,
        last_chunk_options, decode_up_to_bad_char);
    return {r, outlen};
  }
}
    #endif // SIMDUTF_SPAN

  #endif // SIMDUTF_SPAN
} // namespace simdutf

#endif // SIMDUTF_FEATURE_BASE64

#if SIMDUTF_CPLUSPLUS23 && SIMDUTF_FEATURE_BASE64

namespace simdutf {
namespace literals {

namespace detail {

// the detail namespace is not part of the public api

template <std::size_t N> struct base64_literal_helper {
  std::array<char, N - 1> storage{};
  static constexpr std::size_t size() noexcept { return N - 1; }
  consteval base64_literal_helper(const char (&str)[N]) {
    for (std::size_t i = 0; i < size(); i++) {
      storage[i] = str[i];
    }
  }
};

template <std::size_t InputLen> struct base64_decode_result {
  static constexpr std::size_t max_out = (InputLen + 3) / 4 * 3;
  std::array<char, max_out> buffer{};
  std::size_t output_count{};
};

template <std::size_t InputLen>
consteval auto base64_decode_literal(const char *str) {
  base64_decode_result<InputLen> result{};
  auto r = scalar::base64::base64_to_binary_details_impl(
      str, InputLen, result.buffer.data(), base64_default, loose);
  if (r.error != error_code::SUCCESS) {
  #if __cpp_lib_unreachable >= 202202L
    std::unreachable(); // invalid base64 input in _base64 literal
  #else
    // workaround for older stdlib
    throw "invalid base64 input in _base64 literal";
  #endif
  }
  result.output_count = r.output_count;
  return result;
}

template <base64_literal_helper a> consteval auto base64_make_array() {
  constexpr auto decoded = base64_decode_literal<a.size()>(a.storage.data());
  std::array<char, decoded.output_count> ret{};
  for (std::size_t i = 0; i < decoded.output_count; i++) {
    ret[i] = decoded.buffer[i];
  }
  return ret;
}

} // namespace detail

/**
 * User-defined literal for compile-time base64 decoding.
 *
 * Usage:
 *   using namespace simdutf::literals;
 *   constexpr auto decoded = "SGVsbG8gV29ybGQh"_base64;
 *   // decoded is a std::array<char, 12> containing "Hello World!"
 *
 * The input must be valid base64. Whitepace is allowed and ignored.
 * A compilation error occurs if the input is invalid.
 */
template <detail::base64_literal_helper a> consteval auto operator""_base64() {
  return detail::base64_make_array<a>();
}

} // namespace literals
} // namespace simdutf

#endif // SIMDUTF_CPLUSPLUS23 && SIMDUTF_FEATURE_BASE64

#endif // SIMDUTF_IMPLEMENTATION_H
/* end file include/simdutf/implementation.h */

// Implementation-internal files (must be included before the implementations
// themselves, to keep amalgamation working--otherwise, the first time a file is
// included, it might be put inside the #ifdef
// SIMDUTF_IMPLEMENTATION_ARM64/FALLBACK/etc., which means the other
// implementations can't compile unless that implementation is turned on).

SIMDUTF_POP_DISABLE_WARNINGS

#endif // SIMDUTF_H
/* end file include/simdutf.h */
