#ifndef LIBBUFFERSWAP_H
#define LIBBUFFERSWAP_H

#include <stddef.h>	/* size_t */


#if defined(_WIN32) || defined(__CYGWIN__)
#define BUFFERSWAP_SYMBOL_IMPORT __declspec(dllimport)
#define BUFFERSWAP_SYMBOL_EXPORT __declspec(dllexport)
#define BUFFERSWAP_SYMBOL_PRIVATE

#elif __GNUC__ >= 4
#define BUFFERSWAP_SYMBOL_IMPORT   __attribute__ ((visibility ("default")))
#define BUFFERSWAP_SYMBOL_EXPORT   __attribute__ ((visibility ("default")))
#define BUFFERSWAP_SYMBOL_PRIVATE  __attribute__ ((visibility ("hidden")))

#else
#define BUFFERSWAP_SYMBOL_IMPORT
#define BUFFERSWAP_SYMBOL_EXPORT
#define BUFFERSWAP_SYMBOL_PRIVATE
#endif

#if defined(BUFFERSWAP_STATIC_DEFINE)
#define BUFFERSWAP_EXPORT
#define BUFFERSWAP_NO_EXPORT

#else
#if defined(BUFFERSWAP_EXPORTS) // defined if we are building the shared library
#define BUFFERSWAP_EXPORT BUFFERSWAP_SYMBOL_EXPORT

#else
#define BUFFERSWAP_EXPORT BUFFERSWAP_SYMBOL_IMPORT
#endif

#define BUFFERSWAP_NO_EXPORT BUFFERSWAP_SYMBOL_PRIVATE
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* Wrapper function to swap n bits buffer `src` with length of `srclen`. The
 * swap occurs in place*/
void BUFFERSWAP_EXPORT swap_simd
	( char		*src
	, size_t	srclen
	, size_t    n
	) ;

#ifdef __cplusplus
}
#endif

#endif /* LIBBUFFERSWAP_H */
