/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_COMPILER_H
#define ZSTD_COMPILER_H

#include <stddef.h>

#include "portability_macros.h"

/*-*******************************************************
*  Compiler specifics
*********************************************************/
/* force inlining */

#if !defined(ZSTD_NO_INLINE)
#if (defined(__GNUC__) && !defined(__STRICT_ANSI__)) || defined(__cplusplus) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* C99 */
#  define INLINE_KEYWORD inline
#else
#  define INLINE_KEYWORD
#endif

#if defined(__GNUC__) || defined(__ICCARM__)
#  define FORCE_INLINE_ATTR __attribute__((always_inline))
#elif defined(_MSC_VER)
#  define FORCE_INLINE_ATTR __forceinline
#else
#  define FORCE_INLINE_ATTR
#endif

#else

#define INLINE_KEYWORD
#define FORCE_INLINE_ATTR

#endif

/**
  On MSVC qsort requires that functions passed into it use the __cdecl calling conversion(CC).
  This explicitly marks such functions as __cdecl so that the code will still compile
  if a CC other than __cdecl has been made the default.
*/
#if  defined(_MSC_VER)
#  define WIN_CDECL __cdecl
#else
#  define WIN_CDECL
#endif

/* UNUSED_ATTR tells the compiler it is okay if the function is unused. */
#if defined(__GNUC__)
#  define UNUSED_ATTR __attribute__((unused))
#else
#  define UNUSED_ATTR
#endif

/**
 * FORCE_INLINE_TEMPLATE is used to define C "templates", which take constant
 * parameters. They must be inlined for the compiler to eliminate the constant
 * branches.
 */
#define FORCE_INLINE_TEMPLATE static INLINE_KEYWORD FORCE_INLINE_ATTR UNUSED_ATTR
/**
 * HINT_INLINE is used to help the compiler generate better code. It is *not*
 * used for "templates", so it can be tweaked based on the compilers
 * performance.
 *
 * gcc-4.8 and gcc-4.9 have been shown to benefit from leaving off the
 * always_inline attribute.
 *
 * clang up to 5.0.0 (trunk) benefit tremendously from the always_inline
 * attribute.
 */
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 8 && __GNUC__ < 5
#  define HINT_INLINE static INLINE_KEYWORD
#else
#  define HINT_INLINE FORCE_INLINE_TEMPLATE
#endif

/* "soft" inline :
 * The compiler is free to select if it's a good idea to inline or not.
 * The main objective is to silence compiler warnings
 * when a defined function in included but not used.
 *
 * Note : this macro is prefixed `MEM_` because it used to be provided by `mem.h` unit.
 * Updating the prefix is probably preferable, but requires a fairly large codemod,
 * since this name is used everywhere.
 */
#ifndef MEM_STATIC  /* already defined in Linux Kernel mem.h */
#if defined(__GNUC__)
#  define MEM_STATIC static __inline UNUSED_ATTR
#elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
#  define MEM_STATIC static inline
#elif defined(_MSC_VER)
#  define MEM_STATIC static __inline
#else
#  define MEM_STATIC static  /* this version may generate warnings for unused static functions; disable the relevant warning */
#endif
#endif

/* force no inlining */
#ifdef _MSC_VER
#  define FORCE_NOINLINE static __declspec(noinline)
#else
#  if defined(__GNUC__) || defined(__ICCARM__)
#    define FORCE_NOINLINE static __attribute__((__noinline__))
#  else
#    define FORCE_NOINLINE static
#  endif
#endif


/* target attribute */
#if defined(__GNUC__) || defined(__ICCARM__)
#  define TARGET_ATTRIBUTE(target) __attribute__((__target__(target)))
#else
#  define TARGET_ATTRIBUTE(target)
#endif

/* Target attribute for BMI2 dynamic dispatch.
 * Enable lzcnt, bmi, and bmi2.
 * We test for bmi1 & bmi2. lzcnt is included in bmi1.
 */
#define BMI2_TARGET_ATTRIBUTE TARGET_ATTRIBUTE("lzcnt,bmi,bmi2")

/* prefetch
 * can be disabled, by declaring NO_PREFETCH build macro */
#if defined(NO_PREFETCH)
#  define PREFETCH_L1(ptr)  do { (void)(ptr); } while (0)  /* disabled */
#  define PREFETCH_L2(ptr)  do { (void)(ptr); } while (0)  /* disabled */
#else
#  if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_I86)) && !defined(_M_ARM64EC)  /* _mm_prefetch() is not defined outside of x86/x64 */
#    include <mmintrin.h>   /* https://msdn.microsoft.com/fr-fr/library/84szxsww(v=vs.90).aspx */
#    define PREFETCH_L1(ptr)  _mm_prefetch((const char*)(ptr), _MM_HINT_T0)
#    define PREFETCH_L2(ptr)  _mm_prefetch((const char*)(ptr), _MM_HINT_T1)
#  elif defined(__GNUC__) && ( (__GNUC__ >= 4) || ( (__GNUC__ == 3) && (__GNUC_MINOR__ >= 1) ) )
#    define PREFETCH_L1(ptr)  __builtin_prefetch((ptr), 0 /* rw==read */, 3 /* locality */)
#    define PREFETCH_L2(ptr)  __builtin_prefetch((ptr), 0 /* rw==read */, 2 /* locality */)
#  elif defined(__aarch64__)
#    define PREFETCH_L1(ptr)  do { __asm__ __volatile__("prfm pldl1keep, %0" ::"Q"(*(ptr))); } while (0)
#    define PREFETCH_L2(ptr)  do { __asm__ __volatile__("prfm pldl2keep, %0" ::"Q"(*(ptr))); } while (0)
#  else
#    define PREFETCH_L1(ptr) do { (void)(ptr); } while (0)  /* disabled */
#    define PREFETCH_L2(ptr) do { (void)(ptr); } while (0)  /* disabled */
#  endif
#endif  /* NO_PREFETCH */

#define CACHELINE_SIZE 64

#define PREFETCH_AREA(p, s)                              \
    do {                                                 \
        const char* const _ptr = (const char*)(p);       \
        size_t const _size = (size_t)(s);                \
        size_t _pos;                                     \
        for (_pos=0; _pos<_size; _pos+=CACHELINE_SIZE) { \
            PREFETCH_L2(_ptr + _pos);                    \
        }                                                \
    } while (0)

/* vectorization
 * older GCC (pre gcc-4.3 picked as the cutoff) uses a different syntax,
 * and some compilers, like Intel ICC and MCST LCC, do not support it at all. */
#if !defined(__INTEL_COMPILER) && !defined(__clang__) && defined(__GNUC__) && !defined(__LCC__)
#  if (__GNUC__ == 4 && __GNUC_MINOR__ > 3) || (__GNUC__ >= 5)
#    define DONT_VECTORIZE __attribute__((optimize("no-tree-vectorize")))
#  else
#    define DONT_VECTORIZE _Pragma("GCC optimize(\"no-tree-vectorize\")")
#  endif
#else
#  define DONT_VECTORIZE
#endif

/* Tell the compiler that a branch is likely or unlikely.
 * Only use these macros if it causes the compiler to generate better code.
 * If you can remove a LIKELY/UNLIKELY annotation without speed changes in gcc
 * and clang, please do.
 */
#if defined(__GNUC__)
#define LIKELY(x) (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#if __has_builtin(__builtin_unreachable) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)))
#  define ZSTD_UNREACHABLE do { assert(0), __builtin_unreachable(); } while (0)
#else
#  define ZSTD_UNREACHABLE do { assert(0); } while (0)
#endif

/* disable warnings */
#ifdef _MSC_VER    /* Visual Studio */
#  include <intrin.h>                    /* For Visual 2005 */
#  pragma warning(disable : 4100)        /* disable: C4100: unreferenced formal parameter */
#  pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#  pragma warning(disable : 4204)        /* disable: C4204: non-constant aggregate initializer */
#  pragma warning(disable : 4214)        /* disable: C4214: non-int bitfields */
#  pragma warning(disable : 4324)        /* disable: C4324: padded structure */
#endif

/*Like DYNAMIC_BMI2 but for compile time determination of BMI2 support*/
#ifndef STATIC_BMI2
#  if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_I86))
#    ifdef __AVX2__  //MSVC does not have a BMI2 specific flag, but every CPU that supports AVX2 also supports BMI2
#       define STATIC_BMI2 1
#    endif
#  elif defined(__BMI2__) && defined(__x86_64__) && defined(__GNUC__)
#    define STATIC_BMI2 1
#  endif
#endif

#ifndef STATIC_BMI2
    #define STATIC_BMI2 0
#endif

/* compile time determination of SIMD support */
#if !defined(ZSTD_NO_INTRINSICS)
#  if defined(__SSE2__) || defined(_M_AMD64) || (defined (_M_IX86) && defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#    define ZSTD_ARCH_X86_SSE2
#  endif
#  if defined(__ARM_NEON) || defined(_M_ARM64)
#    define ZSTD_ARCH_ARM_NEON
#  endif
#
#  if defined(ZSTD_ARCH_X86_SSE2)
#    include <emmintrin.h>
#  elif defined(ZSTD_ARCH_ARM_NEON)
#    include <arm_neon.h>
#  endif
#endif

/* C-language Attributes are added in C23. */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ > 201710L) && defined(__has_c_attribute)
# define ZSTD_HAS_C_ATTRIBUTE(x) __has_c_attribute(x)
#else
# define ZSTD_HAS_C_ATTRIBUTE(x) 0
#endif

/* Only use C++ attributes in C++. Some compilers report support for C++
 * attributes when compiling with C.
 */
#if defined(__cplusplus) && defined(__has_cpp_attribute)
# define ZSTD_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
# define ZSTD_HAS_CPP_ATTRIBUTE(x) 0
#endif

/* Define ZSTD_FALLTHROUGH macro for annotating switch case with the 'fallthrough' attribute.
 * - C23: https://en.cppreference.com/w/c/language/attributes/fallthrough
 * - CPP17: https://en.cppreference.com/w/cpp/language/attributes/fallthrough
 * - Else: __attribute__((__fallthrough__))
 */
#ifndef ZSTD_FALLTHROUGH
# if ZSTD_HAS_C_ATTRIBUTE(fallthrough)
#  define ZSTD_FALLTHROUGH [[fallthrough]]
# elif ZSTD_HAS_CPP_ATTRIBUTE(fallthrough)
#  define ZSTD_FALLTHROUGH [[fallthrough]]
# elif __has_attribute(__fallthrough__)
/* Leading semicolon is to satisfy gcc-11 with -pedantic. Without the semicolon
 * gcc complains about: a label can only be part of a statement and a declaration is not a statement.
 */
#  define ZSTD_FALLTHROUGH ; __attribute__((__fallthrough__))
# else
#  define ZSTD_FALLTHROUGH
# endif
#endif

/*-**************************************************************
*  Alignment check
*****************************************************************/

/* this test was initially positioned in mem.h,
 * but this file is removed (or replaced) for linux kernel
 * so it's now hosted in compiler.h,
 * which remains valid for both user & kernel spaces.
 */

#ifndef ZSTD_ALIGNOF
# if defined(__GNUC__) || defined(_MSC_VER)
/* covers gcc, clang & MSVC */
/* note : this section must come first, before C11,
 * due to a limitation in the kernel source generator */
#  define ZSTD_ALIGNOF(T) __alignof(T)

# elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
/* C11 support */
#  include <stdalign.h>
#  define ZSTD_ALIGNOF(T) alignof(T)

# else
/* No known support for alignof() - imperfect backup */
#  define ZSTD_ALIGNOF(T) (sizeof(void*) < sizeof(T) ? sizeof(void*) : sizeof(T))

# endif
#endif /* ZSTD_ALIGNOF */

/*-**************************************************************
*  Sanitizer
*****************************************************************/

/**
 * Zstd relies on pointer overflow in its decompressor.
 * We add this attribute to functions that rely on pointer overflow.
 */
#ifndef ZSTD_ALLOW_POINTER_OVERFLOW_ATTR
#  if __has_attribute(no_sanitize)
#    if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 8
       /* gcc < 8 only has signed-integer-overlow which triggers on pointer overflow */
#      define ZSTD_ALLOW_POINTER_OVERFLOW_ATTR __attribute__((no_sanitize("signed-integer-overflow")))
#    else
       /* older versions of clang [3.7, 5.0) will warn that pointer-overflow is ignored. */
#      define ZSTD_ALLOW_POINTER_OVERFLOW_ATTR __attribute__((no_sanitize("pointer-overflow")))
#    endif
#  else
#    define ZSTD_ALLOW_POINTER_OVERFLOW_ATTR
#  endif
#endif

/**
 * Helper function to perform a wrapped pointer difference without trigging
 * UBSAN.
 *
 * @returns lhs - rhs with wrapping
 */
MEM_STATIC
ZSTD_ALLOW_POINTER_OVERFLOW_ATTR
ptrdiff_t ZSTD_wrappedPtrDiff(unsigned char const* lhs, unsigned char const* rhs)
{
    return lhs - rhs;
}

/**
 * Helper function to perform a wrapped pointer add without triggering UBSAN.
 *
 * @return ptr + add with wrapping
 */
MEM_STATIC
ZSTD_ALLOW_POINTER_OVERFLOW_ATTR
unsigned char const* ZSTD_wrappedPtrAdd(unsigned char const* ptr, ptrdiff_t add)
{
    return ptr + add;
}

/**
 * Helper function to perform a wrapped pointer subtraction without triggering
 * UBSAN.
 *
 * @return ptr - sub with wrapping
 */
MEM_STATIC
ZSTD_ALLOW_POINTER_OVERFLOW_ATTR
unsigned char const* ZSTD_wrappedPtrSub(unsigned char const* ptr, ptrdiff_t sub)
{
    return ptr - sub;
}

/**
 * Helper function to add to a pointer that works around C's undefined behavior
 * of adding 0 to NULL.
 *
 * @returns `ptr + add` except it defines `NULL + 0 == NULL`.
 */
MEM_STATIC
unsigned char* ZSTD_maybeNullPtrAdd(unsigned char* ptr, ptrdiff_t add)
{
    return add > 0 ? ptr + add : ptr;
}

/* Issue #3240 reports an ASAN failure on an llvm-mingw build. Out of an
 * abundance of caution, disable our custom poisoning on mingw. */
#ifdef __MINGW32__
#ifndef ZSTD_ASAN_DONT_POISON_WORKSPACE
#define ZSTD_ASAN_DONT_POISON_WORKSPACE 1
#endif
#ifndef ZSTD_MSAN_DONT_POISON_WORKSPACE
#define ZSTD_MSAN_DONT_POISON_WORKSPACE 1
#endif
#endif

#if ZSTD_MEMORY_SANITIZER && !defined(ZSTD_MSAN_DONT_POISON_WORKSPACE)
/* Not all platforms that support msan provide sanitizers/msan_interface.h.
 * We therefore declare the functions we need ourselves, rather than trying to
 * include the header file... */
#include <stddef.h>  /* size_t */
#define ZSTD_DEPS_NEED_STDINT
#include "zstd_deps.h"  /* intptr_t */

/* Make memory region fully initialized (without changing its contents). */
void __msan_unpoison(const volatile void *a, size_t size);

/* Make memory region fully uninitialized (without changing its contents).
   This is a legacy interface that does not update origin information. Use
   __msan_allocated_memory() instead. */
void __msan_poison(const volatile void *a, size_t size);

/* Returns the offset of the first (at least partially) poisoned byte in the
   memory range, or -1 if the whole range is good. */
intptr_t __msan_test_shadow(const volatile void *x, size_t size);

/* Print shadow and origin for the memory range to stderr in a human-readable
   format. */
void __msan_print_shadow(const volatile void *x, size_t size);
#endif

#if ZSTD_ADDRESS_SANITIZER && !defined(ZSTD_ASAN_DONT_POISON_WORKSPACE)
/* Not all platforms that support asan provide sanitizers/asan_interface.h.
 * We therefore declare the functions we need ourselves, rather than trying to
 * include the header file... */
#include <stddef.h>  /* size_t */

/**
 * Marks a memory region (<c>[addr, addr+size)</c>) as unaddressable.
 *
 * This memory must be previously allocated by your program. Instrumented
 * code is forbidden from accessing addresses in this region until it is
 * unpoisoned. This function is not guaranteed to poison the entire region -
 * it could poison only a subregion of <c>[addr, addr+size)</c> due to ASan
 * alignment restrictions.
 *
 * \note This function is not thread-safe because no two threads can poison or
 * unpoison memory in the same memory region simultaneously.
 *
 * \param addr Start of memory region.
 * \param size Size of memory region. */
void __asan_poison_memory_region(void const volatile *addr, size_t size);

/**
 * Marks a memory region (<c>[addr, addr+size)</c>) as addressable.
 *
 * This memory must be previously allocated by your program. Accessing
 * addresses in this region is allowed until this region is poisoned again.
 * This function could unpoison a super-region of <c>[addr, addr+size)</c> due
 * to ASan alignment restrictions.
 *
 * \note This function is not thread-safe because no two threads can
 * poison or unpoison memory in the same memory region simultaneously.
 *
 * \param addr Start of memory region.
 * \param size Size of memory region. */
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
#endif

#endif /* ZSTD_COMPILER_H */
