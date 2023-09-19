#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "../../../include/libbase64.h"
#include "../../tables/tables.h"
#include "../../codecs.h"
#include "config.h"
#include "../../env.h"

#if HAVE_SSE41
#include <smmintrin.h>

#include "../ssse3/dec_reshuffle.c"
#include "../ssse3/dec_loop.c"
#include "../ssse3/enc_translate.c"
#include "../ssse3/enc_reshuffle.c"
#include "../ssse3/enc_loop.c"

#endif	// HAVE_SSE41

BASE64_ENC_FUNCTION(sse41)
{
#if HAVE_SSE41
	#include "../generic/enc_head.c"
	enc_loop_ssse3(&s, &slen, &o, &olen);
	#include "../generic/enc_tail.c"
#else
	BASE64_ENC_STUB
#endif
}

BASE64_DEC_FUNCTION(sse41)
{
#if HAVE_SSE41
	#include "../generic/dec_head.c"
	dec_loop_ssse3(&s, &slen, &o, &olen);
	#include "../generic/dec_tail.c"
#else
	BASE64_DEC_STUB
#endif
}
