#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include "../../../include/libbase64.h"
#include "../../tables/tables.h"
#include "../../codecs.h"
#include "config.h"
#include "../../env.h"

#if HAVE_SSSE3
#include <tmmintrin.h>

#include "dec_reshuffle.c"
#include "dec_loop.c"
#include "enc_reshuffle.c"
#include "enc_translate.c"
#include "enc_loop.c"

#endif	// HAVE_SSSE3

BASE64_ENC_FUNCTION(ssse3)
{
#if HAVE_SSSE3
	#include "../generic/enc_head.c"
	enc_loop_ssse3(&s, &slen, &o, &olen);
	#include "../generic/enc_tail.c"
#else
	BASE64_ENC_STUB
#endif
}

BASE64_DEC_FUNCTION(ssse3)
{
#if HAVE_SSSE3
	#include "../generic/dec_head.c"
	dec_loop_ssse3(&s, &slen, &o, &olen);
	#include "../generic/dec_tail.c"
#else
	BASE64_DEC_STUB
#endif
}
