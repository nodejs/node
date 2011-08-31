/*
 * libecb - http://software.schmorp.de/pkg/libecb
 *
 * Copyright (©) 2009-2011 Marc Alexander Lehmann <libecb@schmorp.de>
 * Copyright (©) 2011 Emanuele Giaquinta
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ECB_H
#define ECB_H

#ifdef _WIN32
  typedef   signed char   int8_t;
  typedef unsigned char  uint8_t;
  typedef   signed short  int16_t;
  typedef unsigned short uint16_t;
  typedef   signed int    int32_t;
  typedef unsigned int   uint32_t;
  #if __GNUC__
    typedef   signed long long int64_t;
    typedef unsigned long long uint64_t;
  #else /* _MSC_VER || __BORLANDC__ */
    typedef   signed __int64   int64_t;
    typedef unsigned __int64   uint64_t;
  #endif
#else
  #include <inttypes.h>
#endif

/* many compilers define _GNUC_ to some versions but then only implement
 * what their idiot authors think are the "more important" extensions,
 * causing enourmous grief in return for some better fake benchmark numbers.
 * or so.
 * we try to detect these and simply assume they are not gcc - if they have
 * an issue with that they should have done it right in the first place.
 */
#ifndef ECB_GCC_VERSION
  #if !defined(__GNUC_MINOR__) || defined(__INTEL_COMPILER) || defined(__SUNPRO_C) || defined(__SUNPRO_CC) || defined(__llvm__) || defined(__clang__)
    #define ECB_GCC_VERSION(major,minor) 0
  #else
    #define ECB_GCC_VERSION(major,minor) (__GNUC__ > (major) || (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor)))
  #endif
#endif

/*****************************************************************************/

#ifndef ECB_MEMORY_FENCE
  #if ECB_GCC_VERSION(2,5)
    #if defined(__x86) || defined(__i386)
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("lock; orb $0, -1(%%esp)" : : : "memory")
      #define ECB_MEMORY_FENCE_ACQUIRE ECB_MEMORY_FENCE /* non-lock xchg might be enough */
      #define ECB_MEMORY_FENCE_RELEASE do { } while (0) /* unlikely to change in future cpus */
    #elif __amd64
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("mfence" : : : "memory")
      #define ECB_MEMORY_FENCE_ACQUIRE __asm__ __volatile__ ("lfence" : : : "memory")
      #define ECB_MEMORY_FENCE_RELEASE __asm__ __volatile__ ("sfence") /* play safe - not needed in any current cpu */
    #endif
  #endif
#endif

#ifndef ECB_MEMORY_FENCE
  #if ECB_GCC_VERSION(4,4)
    #define ECB_MEMORY_FENCE         __sync_synchronize ()
    #define ECB_MEMORY_FENCE_ACQUIRE ({ char dummy = 0; __sync_lock_test_and_set (&dummy, 1); })
    #define ECB_MEMORY_FENCE_RELEASE ({ char dummy = 1; __sync_lock_release      (&dummy   ); })
  #elif _MSC_VER >= 1400 /* VC++ 2005 */
    #pragma intrinsic(_ReadBarrier,_WriteBarrier,_ReadWriteBarrier)
    #define ECB_MEMORY_FENCE         _ReadWriteBarrier ()
    #define ECB_MEMORY_FENCE_ACQUIRE _ReadWriteBarrier () /* according to msdn, _ReadBarrier is not a load fence */
    #define ECB_MEMORY_FENCE_RELEASE _WriteBarrier ()
  #elif defined(_WIN32)
    #include <WinNT.h>
    #define ECB_MEMORY_FENCE         MemoryBarrier () /* actually just xchg on x86... scary */
    #define ECB_MEMORY_FENCE_ACQUIRE ECB_MEMORY_FENCE
    #define ECB_MEMORY_FENCE_RELEASE ECB_MEMORY_FENCE
  #endif
#endif

#ifndef ECB_MEMORY_FENCE
  /*
   * if you get undefined symbol references to pthread_mutex_lock,
   * or failure to find pthread.h, then you should implement
   * the ECB_MEMORY_FENCE operations for your cpu/compiler
   * OR proide pthread.h and link against the posix thread library
   * of your system.
   */
  #include <pthread.h>

  static pthread_mutex_t ecb_mf_lock = PTHREAD_MUTEX_INITIALIZER;
  #define ECB_MEMORY_FENCE do { pthread_mutex_lock (&ecb_mf_lock); pthread_mutex_unlock (&ecb_mf_lock); } while (0)
  #define ECB_MEMORY_FENCE_ACQUIRE ECB_MEMORY_FENCE
  #define ECB_MEMORY_FENCE_RELEASE ECB_MEMORY_FENCE
#endif

/*****************************************************************************/

#define ECB_C99 (__STDC_VERSION__ >= 199901L)

#if __cplusplus
  #define ecb_inline static inline
#elif ECB_GCC_VERSION(2,5)
  #define ecb_inline static __inline__
#elif ECB_C99
  #define ecb_inline static inline
#else
  #define ecb_inline static
#endif

#if ECB_GCC_VERSION(3,3)
  #define ecb_restrict __restrict__
#elif ECB_C99
  #define ecb_restrict restrict
#else
  #define ecb_restrict
#endif

typedef int ecb_bool;

#define ECB_CONCAT_(a, b) a ## b
#define ECB_CONCAT(a, b) ECB_CONCAT_(a, b)
#define ECB_STRINGIFY_(a) # a
#define ECB_STRINGIFY(a) ECB_STRINGIFY_(a)

#define ecb_function_ ecb_inline

#if ECB_GCC_VERSION(3,1)
  #define ecb_attribute(attrlist)        __attribute__(attrlist)
  #define ecb_is_constant(expr)          __builtin_constant_p (expr)
  #define ecb_expect(expr,value)         __builtin_expect ((expr),(value))
  #define ecb_prefetch(addr,rw,locality) __builtin_prefetch (addr, rw, locality)
#else
  #define ecb_attribute(attrlist)
  #define ecb_is_constant(expr)          0
  #define ecb_expect(expr,value)         (expr)
  #define ecb_prefetch(addr,rw,locality)
#endif

/* no emulation for ecb_decltype */
#if ECB_GCC_VERSION(4,5)
  #define ecb_decltype(x) __decltype(x)
#elif ECB_GCC_VERSION(3,0)
  #define ecb_decltype(x) __typeof(x)
#endif

#define ecb_noinline   ecb_attribute ((__noinline__))
#define ecb_noreturn   ecb_attribute ((__noreturn__))
#define ecb_unused     ecb_attribute ((__unused__))
#define ecb_const      ecb_attribute ((__const__))
#define ecb_pure       ecb_attribute ((__pure__))

#if ECB_GCC_VERSION(4,3)
  #define ecb_artificial ecb_attribute ((__artificial__))
  #define ecb_hot        ecb_attribute ((__hot__))
  #define ecb_cold       ecb_attribute ((__cold__))
#else
  #define ecb_artificial
  #define ecb_hot
  #define ecb_cold
#endif

/* put around conditional expressions if you are very sure that the  */
/* expression is mostly true or mostly false. note that these return */
/* booleans, not the expression.                                     */
#define ecb_expect_false(expr) ecb_expect (!!(expr), 0)
#define ecb_expect_true(expr)  ecb_expect (!!(expr), 1)
/* for compatibility to the rest of the world */
#define ecb_likely(expr)   ecb_expect_true  (expr)
#define ecb_unlikely(expr) ecb_expect_false (expr)

/* count trailing zero bits and count # of one bits */
#if ECB_GCC_VERSION(3,4)
  /* we assume int == 32 bit, long == 32 or 64 bit and long long == 64 bit */
  #define ecb_ld32(x)      (__builtin_clz      (x) ^ 31)
  #define ecb_ld64(x)      (__builtin_clzll    (x) ^ 63)
  #define ecb_ctz32(x)      __builtin_ctz      (x)
  #define ecb_ctz64(x)      __builtin_ctzll    (x)
  #define ecb_popcount32(x) __builtin_popcount (x)
  /* no popcountll */
#else
  ecb_function_ int ecb_ctz32 (uint32_t x) ecb_const;
  ecb_function_ int
  ecb_ctz32 (uint32_t x)
  {
    int r = 0;

    x &= ~x + 1; /* this isolates the lowest bit */

#if ECB_branchless_on_i386
    r += !!(x & 0xaaaaaaaa) << 0;
    r += !!(x & 0xcccccccc) << 1;
    r += !!(x & 0xf0f0f0f0) << 2;
    r += !!(x & 0xff00ff00) << 3;
    r += !!(x & 0xffff0000) << 4;
#else
    if (x & 0xaaaaaaaa) r +=  1;
    if (x & 0xcccccccc) r +=  2;
    if (x & 0xf0f0f0f0) r +=  4;
    if (x & 0xff00ff00) r +=  8;
    if (x & 0xffff0000) r += 16;
#endif

    return r;
  }

  ecb_function_ int ecb_ctz64 (uint64_t x) ecb_const;
  ecb_function_ int
  ecb_ctz64 (uint64_t x)
  {
    int shift = x & 0xffffffffU ? 0 : 32;
    return ecb_ctz32 (x >> shift) + shift;
  }

  ecb_function_ int ecb_popcount32 (uint32_t x) ecb_const;
  ecb_function_ int
  ecb_popcount32 (uint32_t x)
  {
    x -=  (x >> 1) & 0x55555555;
    x  = ((x >> 2) & 0x33333333) + (x & 0x33333333);
    x  = ((x >> 4) + x) & 0x0f0f0f0f;
    x *= 0x01010101;

    return x >> 24;
  }

  /* you have the choice beetween something with a table lookup, */
  /* something using lots of bit arithmetic and a simple loop */
  /* we went for the loop */
  ecb_function_ int ecb_ld32 (uint32_t x) ecb_const;
  ecb_function_ int ecb_ld32 (uint32_t x)
  {
    int r = 0;

    if (x >> 16) { x >>= 16; r += 16; }
    if (x >>  8) { x >>=  8; r +=  8; }
    if (x >>  4) { x >>=  4; r +=  4; }
    if (x >>  2) { x >>=  2; r +=  2; }
    if (x >>  1) {           r +=  1; }

    return r;
  }

  ecb_function_ int ecb_ld64 (uint64_t x) ecb_const;
  ecb_function_ int ecb_ld64 (uint64_t x)
  {
    int r = 0;

    if (x >> 32) { x >>= 32; r += 32; }

    return r + ecb_ld32 (x);
  }
#endif

/* popcount64 is only available on 64 bit cpus as gcc builtin */
/* so for this version we are lazy */
ecb_function_ int ecb_popcount64 (uint64_t x) ecb_const;
ecb_function_ int
ecb_popcount64 (uint64_t x)
{
  return ecb_popcount32 (x) + ecb_popcount32 (x >> 32);
}

ecb_inline uint8_t  ecb_rotl8  (uint8_t  x, unsigned int count) ecb_const;
ecb_inline uint8_t  ecb_rotr8  (uint8_t  x, unsigned int count) ecb_const;
ecb_inline uint16_t ecb_rotl16 (uint16_t x, unsigned int count) ecb_const;
ecb_inline uint16_t ecb_rotr16 (uint16_t x, unsigned int count) ecb_const;
ecb_inline uint32_t ecb_rotl32 (uint32_t x, unsigned int count) ecb_const;
ecb_inline uint32_t ecb_rotr32 (uint32_t x, unsigned int count) ecb_const;
ecb_inline uint64_t ecb_rotl64 (uint64_t x, unsigned int count) ecb_const;
ecb_inline uint64_t ecb_rotr64 (uint64_t x, unsigned int count) ecb_const;

ecb_inline uint8_t  ecb_rotl8  (uint8_t  x, unsigned int count) { return (x >> ( 8 - count)) | (x << count); }
ecb_inline uint8_t  ecb_rotr8  (uint8_t  x, unsigned int count) { return (x << ( 8 - count)) | (x >> count); }
ecb_inline uint16_t ecb_rotl16 (uint16_t x, unsigned int count) { return (x >> (16 - count)) | (x << count); }
ecb_inline uint16_t ecb_rotr16 (uint16_t x, unsigned int count) { return (x << (16 - count)) | (x >> count); }
ecb_inline uint32_t ecb_rotl32 (uint32_t x, unsigned int count) { return (x >> (32 - count)) | (x << count); }
ecb_inline uint32_t ecb_rotr32 (uint32_t x, unsigned int count) { return (x << (32 - count)) | (x >> count); }
ecb_inline uint64_t ecb_rotl64 (uint64_t x, unsigned int count) { return (x >> (64 - count)) | (x << count); }
ecb_inline uint64_t ecb_rotr64 (uint64_t x, unsigned int count) { return (x << (64 - count)) | (x >> count); }

#if ECB_GCC_VERSION(4,3)
  #define ecb_bswap16(x) (__builtin_bswap32 (x) >> 16)
  #define ecb_bswap32(x)  __builtin_bswap32 (x)
  #define ecb_bswap64(x)  __builtin_bswap64 (x)
#else
  ecb_function_ uint16_t ecb_bswap16 (uint16_t x) ecb_const;
  ecb_function_ uint16_t
  ecb_bswap16 (uint16_t x)
  {
    return ecb_rotl16 (x, 8);
  }

  ecb_function_ uint32_t ecb_bswap32 (uint32_t x) ecb_const;
  ecb_function_ uint32_t
  ecb_bswap32 (uint32_t x)
  {
    return (((uint32_t)ecb_bswap16 (x)) << 16) | ecb_bswap16 (x >> 16);
  }

  ecb_function_ uint64_t ecb_bswap64 (uint64_t x) ecb_const;
  ecb_function_ uint64_t
  ecb_bswap64 (uint64_t x)
  {
    return (((uint64_t)ecb_bswap32 (x)) << 32) | ecb_bswap32 (x >> 32);
  }
#endif

#if ECB_GCC_VERSION(4,5)
  #define ecb_unreachable() __builtin_unreachable ()
#else
  /* this seems to work fine, but gcc always emits a warning for it :/ */
  ecb_function_ void ecb_unreachable (void) ecb_noreturn;
  ecb_function_ void ecb_unreachable (void) { }
#endif

/* try to tell the compiler that some condition is definitely true */
#define ecb_assume(cond) do { if (!(cond)) ecb_unreachable (); } while (0)

ecb_function_ unsigned char ecb_byteorder_helper (void) ecb_const;
ecb_function_ unsigned char
ecb_byteorder_helper (void)
{
  const uint32_t u = 0x11223344;
  return *(unsigned char *)&u;
}

ecb_function_ ecb_bool ecb_big_endian    (void) ecb_const;
ecb_function_ ecb_bool ecb_big_endian    (void) { return ecb_byteorder_helper () == 0x11; }
ecb_function_ ecb_bool ecb_little_endian (void) ecb_const;
ecb_function_ ecb_bool ecb_little_endian (void) { return ecb_byteorder_helper () == 0x44; }

#if ECB_GCC_VERSION(3,0) || ECB_C99
  #define ecb_mod(m,n) ((m) % (n) + ((m) % (n) < 0 ? (n) : 0))
#else
  #define ecb_mod(m,n) ((m) < 0 ? ((n) - 1 - ((-1 - (m)) % (n))) : ((m) % (n)))
#endif

#if ecb_cplusplus_does_not_suck
  /* does not work for local types (http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2657.htm) */
  template<typename T, int N>
  static inline int ecb_array_length (const T (&arr)[N])
  {
    return N;
  }
#else
  #define ecb_array_length(name) (sizeof (name) / sizeof (name [0]))
#endif

#endif

