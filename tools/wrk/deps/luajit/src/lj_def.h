/*
** LuaJIT common internal definitions.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_DEF_H
#define _LJ_DEF_H

#include "lua.h"

#if defined(_MSC_VER)
/* MSVC is stuck in the last century and doesn't have C99's stdint.h. */
typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#ifdef _WIN64
typedef __int64 intptr_t;
typedef unsigned __int64 uintptr_t;
#else
typedef __int32 intptr_t;
typedef unsigned __int32 uintptr_t;
#endif
#elif defined(__symbian__)
/* Cough. */
typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef int intptr_t;
typedef unsigned int uintptr_t;
#else
#include <stdint.h>
#endif

/* Needed everywhere. */
#include <string.h>
#include <stdlib.h>

/* Various VM limits. */
#define LJ_MAX_MEM	0x7fffff00	/* Max. total memory allocation. */
#define LJ_MAX_ALLOC	LJ_MAX_MEM	/* Max. individual allocation length. */
#define LJ_MAX_STR	LJ_MAX_MEM	/* Max. string length. */
#define LJ_MAX_UDATA	LJ_MAX_MEM	/* Max. userdata length. */

#define LJ_MAX_STRTAB	(1<<26)		/* Max. string table size. */
#define LJ_MAX_HBITS	26		/* Max. hash bits. */
#define LJ_MAX_ABITS	28		/* Max. bits of array key. */
#define LJ_MAX_ASIZE	((1<<(LJ_MAX_ABITS-1))+1)  /* Max. array part size. */
#define LJ_MAX_COLOSIZE	16		/* Max. elems for colocated array. */

#define LJ_MAX_LINE	LJ_MAX_MEM	/* Max. source code line number. */
#define LJ_MAX_XLEVEL	200		/* Max. syntactic nesting level. */
#define LJ_MAX_BCINS	(1<<26)		/* Max. # of bytecode instructions. */
#define LJ_MAX_SLOTS	250		/* Max. # of slots in a Lua func. */
#define LJ_MAX_LOCVAR	200		/* Max. # of local variables. */
#define LJ_MAX_UPVAL	60		/* Max. # of upvalues. */

#define LJ_MAX_IDXCHAIN	100		/* __index/__newindex chain limit. */
#define LJ_STACK_EXTRA	5		/* Extra stack space (metamethods). */

#define LJ_NUM_CBPAGE	1		/* Number of FFI callback pages. */

/* Minimum table/buffer sizes. */
#define LJ_MIN_GLOBAL	6		/* Min. global table size (hbits). */
#define LJ_MIN_REGISTRY	2		/* Min. registry size (hbits). */
#define LJ_MIN_STRTAB	256		/* Min. string table size (pow2). */
#define LJ_MIN_SBUF	32		/* Min. string buffer length. */
#define LJ_MIN_VECSZ	8		/* Min. size for growable vectors. */
#define LJ_MIN_IRSZ	32		/* Min. size for growable IR. */
#define LJ_MIN_K64SZ	16		/* Min. size for chained K64Array. */

/* JIT compiler limits. */
#define LJ_MAX_JSLOTS	250		/* Max. # of stack slots for a trace. */
#define LJ_MAX_PHI	64		/* Max. # of PHIs for a loop. */
#define LJ_MAX_EXITSTUBGR	16	/* Max. # of exit stub groups. */

/* Various macros. */
#ifndef UNUSED
#define UNUSED(x)	((void)(x))	/* to avoid warnings */
#endif

#define U64x(hi, lo)	(((uint64_t)0x##hi << 32) + (uint64_t)0x##lo)
#define i32ptr(p)	((int32_t)(intptr_t)(void *)(p))
#define u32ptr(p)	((uint32_t)(intptr_t)(void *)(p))

#define checki8(x)	((x) == (int32_t)(int8_t)(x))
#define checku8(x)	((x) == (int32_t)(uint8_t)(x))
#define checki16(x)	((x) == (int32_t)(int16_t)(x))
#define checku16(x)	((x) == (int32_t)(uint16_t)(x))
#define checki32(x)	((x) == (int32_t)(x))
#define checku32(x)	((x) == (uint32_t)(x))
#define checkptr32(x)	((uintptr_t)(x) == (uint32_t)(uintptr_t)(x))

/* Every half-decent C compiler transforms this into a rotate instruction. */
#define lj_rol(x, n)	(((x)<<(n)) | ((x)>>(-(int)(n)&(8*sizeof(x)-1))))
#define lj_ror(x, n)	(((x)<<(-(int)(n)&(8*sizeof(x)-1))) | ((x)>>(n)))

/* A really naive Bloom filter. But sufficient for our needs. */
typedef uintptr_t BloomFilter;
#define BLOOM_MASK	(8*sizeof(BloomFilter) - 1)
#define bloombit(x)	((uintptr_t)1 << ((x) & BLOOM_MASK))
#define bloomset(b, x)	((b) |= bloombit((x)))
#define bloomtest(b, x)	((b) & bloombit((x)))

#if defined(__GNUC__)

#define LJ_NORET	__attribute__((noreturn))
#define LJ_ALIGN(n)	__attribute__((aligned(n)))
#define LJ_INLINE	inline
#define LJ_AINLINE	inline __attribute__((always_inline))
#define LJ_NOINLINE	__attribute__((noinline))

#if defined(__ELF__) || defined(__MACH__)
#if !((defined(__sun__) && defined(__svr4__)) || defined(__CELLOS_LV2__))
#define LJ_NOAPI	extern __attribute__((visibility("hidden")))
#endif
#endif

/* Note: it's only beneficial to use fastcall on x86 and then only for up to
** two non-FP args. The amalgamated compile covers all LJ_FUNC cases. Only
** indirect calls and related tail-called C functions are marked as fastcall.
*/
#if defined(__i386__)
#define LJ_FASTCALL	__attribute__((fastcall))
#endif

#define LJ_LIKELY(x)	__builtin_expect(!!(x), 1)
#define LJ_UNLIKELY(x)	__builtin_expect(!!(x), 0)

#define lj_ffs(x)	((uint32_t)__builtin_ctz(x))
/* Don't ask ... */
#if defined(__INTEL_COMPILER) && (defined(__i386__) || defined(__x86_64__))
static LJ_AINLINE uint32_t lj_fls(uint32_t x)
{
  uint32_t r; __asm__("bsrl %1, %0" : "=r" (r) : "rm" (x) : "cc"); return r;
}
#else
#define lj_fls(x)	((uint32_t)(__builtin_clz(x)^31))
#endif

#if defined(__arm__)
static LJ_AINLINE uint32_t lj_bswap(uint32_t x)
{
  uint32_t r;
#if __ARM_ARCH_6__ || __ARM_ARCH_6J__ || __ARM_ARCH_6T2__ || __ARM_ARCH_6Z__ ||\
    __ARM_ARCH_6ZK__ || __ARM_ARCH_7__ || __ARM_ARCH_7A__ || __ARM_ARCH_7R__
  __asm__("rev %0, %1" : "=r" (r) : "r" (x));
  return r;
#else
#ifdef __thumb__
  r = x ^ lj_ror(x, 16);
#else
  __asm__("eor %0, %1, %1, ror #16" : "=r" (r) : "r" (x));
#endif
  return ((r & 0xff00ffffu) >> 8) ^ lj_ror(x, 8);
#endif
}

static LJ_AINLINE uint64_t lj_bswap64(uint64_t x)
{
  return ((uint64_t)lj_bswap((uint32_t)x)<<32) | lj_bswap((uint32_t)(x>>32));
}
#elif (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)
static LJ_AINLINE uint32_t lj_bswap(uint32_t x)
{
  return (uint32_t)__builtin_bswap32((int32_t)x);
}

static LJ_AINLINE uint64_t lj_bswap64(uint64_t x)
{
  return (uint64_t)__builtin_bswap64((int64_t)x);
}
#elif defined(__i386__) || defined(__x86_64__)
static LJ_AINLINE uint32_t lj_bswap(uint32_t x)
{
  uint32_t r; __asm__("bswap %0" : "=r" (r) : "0" (x)); return r;
}

#if defined(__i386__)
static LJ_AINLINE uint64_t lj_bswap64(uint64_t x)
{
  return ((uint64_t)lj_bswap((uint32_t)x)<<32) | lj_bswap((uint32_t)(x>>32));
}
#else
static LJ_AINLINE uint64_t lj_bswap64(uint64_t x)
{
  uint64_t r; __asm__("bswap %0" : "=r" (r) : "0" (x)); return r;
}
#endif
#else
static LJ_AINLINE uint32_t lj_bswap(uint32_t x)
{
  return (x << 24) | ((x & 0xff00) << 8) | ((x >> 8) & 0xff00) | (x >> 24);
}

static LJ_AINLINE uint64_t lj_bswap64(uint64_t x)
{
  return (uint64_t)lj_bswap((uint32_t)(x >> 32)) |
	 ((uint64_t)lj_bswap((uint32_t)x) << 32);
}
#endif

typedef union __attribute__((packed)) Unaligned16 {
  uint16_t u;
  uint8_t b[2];
} Unaligned16;

typedef union __attribute__((packed)) Unaligned32 {
  uint32_t u;
  uint8_t b[4];
} Unaligned32;

/* Unaligned load of uint16_t. */
static LJ_AINLINE uint16_t lj_getu16(const void *p)
{
  return ((const Unaligned16 *)p)->u;
}

/* Unaligned load of uint32_t. */
static LJ_AINLINE uint32_t lj_getu32(const void *p)
{
  return ((const Unaligned32 *)p)->u;
}

#elif defined(_MSC_VER)

#define LJ_NORET	__declspec(noreturn)
#define LJ_ALIGN(n)	__declspec(align(n))
#define LJ_INLINE	__inline
#define LJ_AINLINE	__forceinline
#define LJ_NOINLINE	__declspec(noinline)
#if defined(_M_IX86)
#define LJ_FASTCALL	__fastcall
#endif

#ifdef _M_PPC
unsigned int _CountLeadingZeros(long);
#pragma intrinsic(_CountLeadingZeros)
static LJ_AINLINE uint32_t lj_fls(uint32_t x)
{
  return _CountLeadingZeros(x) ^ 31;
}
#else
unsigned char _BitScanForward(uint32_t *, unsigned long);
unsigned char _BitScanReverse(uint32_t *, unsigned long);
#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_BitScanReverse)

static LJ_AINLINE uint32_t lj_ffs(uint32_t x)
{
  uint32_t r; _BitScanForward(&r, x); return r;
}

static LJ_AINLINE uint32_t lj_fls(uint32_t x)
{
  uint32_t r; _BitScanReverse(&r, x); return r;
}
#endif

unsigned long _byteswap_ulong(unsigned long);
uint64_t _byteswap_uint64(uint64_t);
#define lj_bswap(x)	(_byteswap_ulong((x)))
#define lj_bswap64(x)	(_byteswap_uint64((x)))

#if defined(_M_PPC) && defined(LUAJIT_NO_UNALIGNED)
/*
** Replacement for unaligned loads on Xbox 360. Disabled by default since it's
** usually more costly than the occasional stall when crossing a cache-line.
*/
static LJ_AINLINE uint16_t lj_getu16(const void *v)
{
  const uint8_t *p = (const uint8_t *)v;
  return (uint16_t)((p[0]<<8) | p[1]);
}
static LJ_AINLINE uint32_t lj_getu32(const void *v)
{
  const uint8_t *p = (const uint8_t *)v;
  return (uint32_t)((p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3]);
}
#else
/* Unaligned loads are generally ok on x86/x64. */
#define lj_getu16(p)	(*(uint16_t *)(p))
#define lj_getu32(p)	(*(uint32_t *)(p))
#endif

#else
#error "missing defines for your compiler"
#endif

/* Optional defines. */
#ifndef LJ_FASTCALL
#define LJ_FASTCALL
#endif
#ifndef LJ_NORET
#define LJ_NORET
#endif
#ifndef LJ_NOAPI
#define LJ_NOAPI	extern
#endif
#ifndef LJ_LIKELY
#define LJ_LIKELY(x)	(x)
#define LJ_UNLIKELY(x)	(x)
#endif

/* Attributes for internal functions. */
#define LJ_DATA		LJ_NOAPI
#define LJ_DATADEF
#define LJ_ASMF		LJ_NOAPI
#define LJ_FUNCA	LJ_NOAPI
#if defined(ljamalg_c)
#define LJ_FUNC		static
#else
#define LJ_FUNC		LJ_NOAPI
#endif
#define LJ_FUNC_NORET	LJ_FUNC LJ_NORET
#define LJ_FUNCA_NORET	LJ_FUNCA LJ_NORET
#define LJ_ASMF_NORET	LJ_ASMF LJ_NORET

/* Runtime assertions. */
#ifdef lua_assert
#define check_exp(c, e)		(lua_assert(c), (e))
#define api_check(l, e)		lua_assert(e)
#else
#define lua_assert(c)		((void)0)
#define check_exp(c, e)		(e)
#define api_check		luai_apicheck
#endif

/* Static assertions. */
#define LJ_ASSERT_NAME2(name, line)	name ## line
#define LJ_ASSERT_NAME(line)		LJ_ASSERT_NAME2(lj_assert_, line)
#ifdef __COUNTER__
#define LJ_STATIC_ASSERT(cond) \
  extern void LJ_ASSERT_NAME(__COUNTER__)(int STATIC_ASSERTION_FAILED[(cond)?1:-1])
#else
#define LJ_STATIC_ASSERT(cond) \
  extern void LJ_ASSERT_NAME(__LINE__)(int STATIC_ASSERTION_FAILED[(cond)?1:-1])
#endif

#endif
