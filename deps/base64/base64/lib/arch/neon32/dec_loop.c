static inline int
is_nonzero (const uint8x16_t v)
{
	uint64_t u64;
	const uint64x2_t v64 = vreinterpretq_u64_u8(v);
	const uint32x2_t v32 = vqmovn_u64(v64);

	vst1_u64(&u64, vreinterpret_u64_u32(v32));
	return u64 != 0;
}

static inline uint8x16_t
delta_lookup (const uint8x16_t v)
{
	const uint8x8_t lut = {
		0, 16, 19, 4, (uint8_t) -65, (uint8_t) -65, (uint8_t) -71, (uint8_t) -71,
	};

	return vcombine_u8(
		vtbl1_u8(lut, vget_low_u8(v)),
		vtbl1_u8(lut, vget_high_u8(v)));
}

static inline uint8x16_t
dec_loop_neon32_lane (uint8x16_t *lane)
{
	// See the SSSE3 decoder for an explanation of the algorithm.
	const uint8x16_t lut_lo = {
		0x15, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x13, 0x1A, 0x1B, 0x1B, 0x1B, 0x1A
	};

	const uint8x16_t lut_hi = {
		0x10, 0x10, 0x01, 0x02, 0x04, 0x08, 0x04, 0x08,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10
	};

	const uint8x16_t mask_0F = vdupq_n_u8(0x0F);
	const uint8x16_t mask_2F = vdupq_n_u8(0x2F);

	const uint8x16_t hi_nibbles = vshrq_n_u8(*lane, 4);
	const uint8x16_t lo_nibbles = vandq_u8(*lane, mask_0F);
	const uint8x16_t eq_2F      = vceqq_u8(*lane, mask_2F);

	const uint8x16_t hi = vqtbl1q_u8(lut_hi, hi_nibbles);
	const uint8x16_t lo = vqtbl1q_u8(lut_lo, lo_nibbles);

	// Now simply add the delta values to the input:
	*lane = vaddq_u8(*lane, delta_lookup(vaddq_u8(eq_2F, hi_nibbles)));

	// Return the validity mask:
	return vandq_u8(lo, hi);
}

static inline void
dec_loop_neon32 (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	if (*slen < 64) {
		return;
	}

	// Process blocks of 64 bytes per round. Unlike the SSE codecs, no
	// extra trailing zero bytes are written, so it is not necessary to
	// reserve extra input bytes:
	size_t rounds = *slen / 64;

	*slen -= rounds * 64;	// 64 bytes consumed per round
	*olen += rounds * 48;	// 48 bytes produced per round

	do {
		uint8x16x3_t dec;

		// Load 64 bytes and deinterleave:
		uint8x16x4_t str = vld4q_u8(*s);

		// Decode each lane, collect a mask of invalid inputs:
		const uint8x16_t classified
			= dec_loop_neon32_lane(&str.val[0])
			| dec_loop_neon32_lane(&str.val[1])
			| dec_loop_neon32_lane(&str.val[2])
			| dec_loop_neon32_lane(&str.val[3]);

		// Check for invalid input: if any of the delta values are
		// zero, fall back on bytewise code to do error checking and
		// reporting:
		if (is_nonzero(classified)) {
			break;
		}

		// Compress four bytes into three:
		dec.val[0] = vorrq_u8(vshlq_n_u8(str.val[0], 2), vshrq_n_u8(str.val[1], 4));
		dec.val[1] = vorrq_u8(vshlq_n_u8(str.val[1], 4), vshrq_n_u8(str.val[2], 2));
		dec.val[2] = vorrq_u8(vshlq_n_u8(str.val[2], 6), str.val[3]);

		// Interleave and store decoded result:
		vst3q_u8(*o, dec);

		*s += 64;
		*o += 48;

	} while (--rounds > 0);

	// Adjust for any rounds that were skipped:
	*slen += rounds * 64;
	*olen -= rounds * 48;
}
