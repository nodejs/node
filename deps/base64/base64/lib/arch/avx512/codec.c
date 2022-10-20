#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "../../../include/libbase64.h"
#include "../../tables/tables.h"
#include "../../codecs.h"
#include "config.h"
#include "../../env.h"

#if HAVE_AVX512
#include <immintrin.h>

#include "../avx2/dec_reshuffle.c"
#include "../avx2/dec_loop.c"
#include "enc_reshuffle_translate.c"
#include "enc_loop.c"

#endif	// HAVE_AVX512

BASE64_ENC_FUNCTION(avx512)
{
#if HAVE_AVX512
	#include "../generic/enc_head.c"
	enc_loop_avx512(&s, &slen, &o, &olen);
	#include "../generic/enc_tail.c"
#else
	BASE64_ENC_STUB
#endif
}

// Reuse AVX2 decoding. Not supporting AVX512 at present
BASE64_DEC_FUNCTION(avx512)
{
#if HAVE_AVX512
	#include "../generic/dec_head.c"
	dec_loop_avx2(&s, &slen, &o, &olen);
	#include "../generic/dec_tail.c"
#else
	BASE64_DEC_STUB
#endif
}
