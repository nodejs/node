#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "../../../include/libbase64.h"
#include "../../tables/tables.h"
#include "../../codecs.h"
#include "config.h"
#include "../../env.h"

#if HAVE_AVX
#include <immintrin.h>

// Only enable inline assembly on supported compilers and on 64-bit CPUs.
#ifndef BASE64_AVX_USE_ASM
# if (defined(__GNUC__) || defined(__clang__)) && BASE64_WORDSIZE == 64
#  define BASE64_AVX_USE_ASM 1
# else
#  define BASE64_AVX_USE_ASM 0
# endif
#endif

#include "../ssse3/dec_reshuffle.c"
#include "../ssse3/dec_loop.c"

#if BASE64_AVX_USE_ASM
# include "enc_loop_asm.c"
#else
# include "../ssse3/enc_translate.c"
# include "../ssse3/enc_reshuffle.c"
# include "../ssse3/enc_loop.c"
#endif

#endif	// HAVE_AVX

BASE64_ENC_FUNCTION(avx)
{
#if HAVE_AVX
	#include "../generic/enc_head.c"

	// For supported compilers, use a hand-optimized inline assembly
	// encoder. Otherwise fall back on the SSSE3 encoder, but compiled with
	// AVX flags to generate better optimized AVX code.

#if BASE64_AVX_USE_ASM
	enc_loop_avx(&s, &slen, &o, &olen);
#else
	enc_loop_ssse3(&s, &slen, &o, &olen);
#endif

	#include "../generic/enc_tail.c"
#else
	BASE64_ENC_STUB
#endif
}

BASE64_DEC_FUNCTION(avx)
{
#if HAVE_AVX
	#include "../generic/dec_head.c"
	dec_loop_ssse3(&s, &slen, &o, &olen);
	#include "../generic/dec_tail.c"
#else
	BASE64_DEC_STUB
#endif
}
