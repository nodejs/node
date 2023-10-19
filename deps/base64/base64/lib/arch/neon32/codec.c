#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "../../../include/libbase64.h"
#include "../../tables/tables.h"
#include "../../codecs.h"
#include "config.h"
#include "../../env.h"

#ifdef __arm__
#  if (defined(__ARM_NEON__) || defined(__ARM_NEON)) && HAVE_NEON32
#    define BASE64_USE_NEON32
#  endif
#endif

#ifdef BASE64_USE_NEON32
#include <arm_neon.h>

// Only enable inline assembly on supported compilers.
#if defined(__GNUC__) || defined(__clang__)
#define BASE64_NEON32_USE_ASM
#endif

static inline uint8x16_t
vqtbl1q_u8 (const uint8x16_t lut, const uint8x16_t indices)
{
	// NEON32 only supports 64-bit wide lookups in 128-bit tables. Emulate
	// the NEON64 `vqtbl1q_u8` intrinsic to do 128-bit wide lookups.
	uint8x8x2_t lut2;
	uint8x8x2_t result;

	lut2.val[0] = vget_low_u8(lut);
	lut2.val[1] = vget_high_u8(lut);

	result.val[0] = vtbl2_u8(lut2, vget_low_u8(indices));
	result.val[1] = vtbl2_u8(lut2, vget_high_u8(indices));

	return vcombine_u8(result.val[0], result.val[1]);
}

#include "../generic/32/dec_loop.c"
#include "../generic/32/enc_loop.c"
#include "dec_loop.c"
#include "enc_reshuffle.c"
#include "enc_translate.c"
#include "enc_loop.c"

#endif	// BASE64_USE_NEON32

// Stride size is so large on these NEON 32-bit functions
// (48 bytes encode, 32 bytes decode) that we inline the
// uint32 codec to stay performant on smaller inputs.

BASE64_ENC_FUNCTION(neon32)
{
#ifdef BASE64_USE_NEON32
	#include "../generic/enc_head.c"
	enc_loop_neon32(&s, &slen, &o, &olen);
	enc_loop_generic_32(&s, &slen, &o, &olen);
	#include "../generic/enc_tail.c"
#else
	BASE64_ENC_STUB
#endif
}

BASE64_DEC_FUNCTION(neon32)
{
#ifdef BASE64_USE_NEON32
	#include "../generic/dec_head.c"
	dec_loop_neon32(&s, &slen, &o, &olen);
	dec_loop_generic_32(&s, &slen, &o, &olen);
	#include "../generic/dec_tail.c"
#else
	BASE64_DEC_STUB
#endif
}
