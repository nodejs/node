#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "../../../include/libbase64.h"
#include "../../tables/tables.h"
#include "../../codecs.h"
#include "config.h"
#include "../../env.h"

#ifdef __aarch64__
#  if (defined(__ARM_NEON__) || defined(__ARM_NEON)) && HAVE_NEON64
#    define BASE64_USE_NEON64
#  endif
#endif

#ifdef BASE64_USE_NEON64
#include <arm_neon.h>

// Only enable inline assembly on supported compilers.
#if defined(__GNUC__) || defined(__clang__)
#define BASE64_NEON64_USE_ASM
#endif

static inline uint8x16x4_t
load_64byte_table (const uint8_t *p)
{
#ifdef BASE64_NEON64_USE_ASM

	// Force the table to be loaded into contiguous registers. GCC will not
	// normally allocate contiguous registers for a `uint8x16x4_t'. These
	// registers are chosen to not conflict with the ones in the enc loop.
	register uint8x16_t t0 __asm__ ("v8");
	register uint8x16_t t1 __asm__ ("v9");
	register uint8x16_t t2 __asm__ ("v10");
	register uint8x16_t t3 __asm__ ("v11");

	__asm__ (
		"ld1 {%[t0].16b, %[t1].16b, %[t2].16b, %[t3].16b}, [%[src]], #64 \n\t"
		: [src] "+r" (p),
		  [t0]  "=w" (t0),
		  [t1]  "=w" (t1),
		  [t2]  "=w" (t2),
		  [t3]  "=w" (t3)
	);

	return (uint8x16x4_t) {
		.val[0] = t0,
		.val[1] = t1,
		.val[2] = t2,
		.val[3] = t3,
	};
#else
	return vld1q_u8_x4(p);
#endif
}

#include "../generic/32/dec_loop.c"
#include "../generic/64/enc_loop.c"
#include "dec_loop.c"

#ifdef BASE64_NEON64_USE_ASM
# include "enc_loop_asm.c"
#else
# include "enc_reshuffle.c"
# include "enc_loop.c"
#endif

#endif	// BASE64_USE_NEON64

// Stride size is so large on these NEON 64-bit functions
// (48 bytes encode, 64 bytes decode) that we inline the
// uint64 codec to stay performant on smaller inputs.

BASE64_ENC_FUNCTION(neon64)
{
#ifdef BASE64_USE_NEON64
	#include "../generic/enc_head.c"
	enc_loop_neon64(&s, &slen, &o, &olen);
	enc_loop_generic_64(&s, &slen, &o, &olen);
	#include "../generic/enc_tail.c"
#else
	BASE64_ENC_STUB
#endif
}

BASE64_DEC_FUNCTION(neon64)
{
#ifdef BASE64_USE_NEON64
	#include "../generic/dec_head.c"
	dec_loop_neon64(&s, &slen, &o, &olen);
	dec_loop_generic_32(&s, &slen, &o, &olen);
	#include "../generic/dec_tail.c"
#else
	BASE64_DEC_STUB
#endif
}
