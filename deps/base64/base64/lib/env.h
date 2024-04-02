#ifndef BASE64_ENV_H
#define BASE64_ENV_H

#include <stdint.h>

// This header file contains macro definitions that describe certain aspects of
// the compile-time environment. Compatibility and portability macros go here.

// Define machine endianness. This is for GCC:
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#  define BASE64_LITTLE_ENDIAN 1
#else
#  define BASE64_LITTLE_ENDIAN 0
#endif

// This is for Clang:
#ifdef __LITTLE_ENDIAN__
#  define BASE64_LITTLE_ENDIAN 1
#endif

#ifdef __BIG_ENDIAN__
#  define BASE64_LITTLE_ENDIAN 0
#endif

// MSVC++ needs intrin.h for _byteswap_uint64 (issue #68):
#if BASE64_LITTLE_ENDIAN && defined(_MSC_VER)
#  include <intrin.h>
#endif

// Endian conversion functions:
#if BASE64_LITTLE_ENDIAN
#  ifdef _MSC_VER
//   Microsoft Visual C++:
#    define BASE64_HTOBE32(x)	_byteswap_ulong(x)
#    define BASE64_HTOBE64(x)	_byteswap_uint64(x)
#  else
//   GCC and Clang:
#    define BASE64_HTOBE32(x)	__builtin_bswap32(x)
#    define BASE64_HTOBE64(x)	__builtin_bswap64(x)
#  endif
#else
// No conversion needed:
#  define BASE64_HTOBE32(x)	(x)
#  define BASE64_HTOBE64(x)	(x)
#endif

// Detect word size:
#if defined (__x86_64__)
// This also works for the x32 ABI, which has a 64-bit word size.
#  define BASE64_WORDSIZE 64
#elif SIZE_MAX == UINT32_MAX
#  define BASE64_WORDSIZE 32
#elif SIZE_MAX == UINT64_MAX
#  define BASE64_WORDSIZE 64
#else
#  error BASE64_WORDSIZE_NOT_DEFINED
#endif

// End-of-file definitions.
// Almost end-of-file when waiting for the last '=' character:
#define BASE64_AEOF 1
// End-of-file when stream end has been reached or invalid input provided:
#define BASE64_EOF 2

// GCC 7 defaults to issuing a warning for fallthrough in switch statements,
// unless the fallthrough cases are marked with an attribute. As we use
// fallthrough deliberately, define an alias for the attribute:
#if __GNUC__ >= 7
#  define BASE64_FALLTHROUGH  __attribute__((fallthrough));
#else
#  define BASE64_FALLTHROUGH
#endif

#endif	// BASE64_ENV_H
