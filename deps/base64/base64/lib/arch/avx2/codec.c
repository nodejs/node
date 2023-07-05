#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "../../../include/libbase64.h"
#include "../../tables/tables.h"
#include "../../codecs.h"
#include "config.h"
#include "../../env.h"

#if HAVE_AVX2
#include <immintrin.h>

#include "dec_reshuffle.c"
#include "dec_loop.c"
#include "enc_translate.c"
#include "enc_reshuffle.c"
#include "enc_loop.c"

#endif	// HAVE_AVX2

BASE64_ENC_FUNCTION(avx2)
{
#if HAVE_AVX2
	#include "../generic/enc_head.c"
	enc_loop_avx2(&s, &slen, &o, &olen);
	#include "../generic/enc_tail.c"
#else
	BASE64_ENC_STUB
#endif
}

BASE64_DEC_FUNCTION(avx2)
{
#if HAVE_AVX2
	#include "../generic/dec_head.c"
	dec_loop_avx2(&s, &slen, &o, &olen);
	#include "../generic/dec_tail.c"
#else
	BASE64_DEC_STUB
#endif
}
