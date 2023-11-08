#ifdef BASE64_NEON32_USE_ASM
static inline void
enc_loop_neon32_inner_asm (const uint8_t **s, uint8_t **o)
{
	// This function duplicates the functionality of enc_loop_neon32_inner,
	// but entirely with inline assembly. This gives a significant speedup
	// over using NEON intrinsics, which do not always generate very good
	// code. The logic of the assembly is directly lifted from the
	// intrinsics version, so it can be used as a guide to this code.

	// Temporary registers, used as scratch space.
	uint8x16_t tmp0, tmp1, tmp2, tmp3;
	uint8x16_t mask0, mask1, mask2, mask3;

	// A lookup table containing the absolute offsets for all ranges.
	const uint8x16_t lut = {
		  65U,  71U, 252U, 252U,
		 252U, 252U, 252U, 252U,
		 252U, 252U, 252U, 252U,
		 237U, 240U,   0U,   0U
	};

	// Numeric constants.
	const uint8x16_t n51 = vdupq_n_u8(51);
	const uint8x16_t n25 = vdupq_n_u8(25);
	const uint8x16_t n63 = vdupq_n_u8(63);

	__asm__ (

		// Load 48 bytes and deinterleave. The bytes are loaded to
		// hard-coded registers q12, q13 and q14, to ensure that they
		// are contiguous. Increment the source pointer.
		"vld3.8 {d24, d26, d28}, [%[src]]! \n\t"
		"vld3.8 {d25, d27, d29}, [%[src]]! \n\t"

		// Reshuffle the bytes using temporaries.
		"vshr.u8 %q[t0], q12,    #2      \n\t"
		"vshr.u8 %q[t1], q13,    #4      \n\t"
		"vshr.u8 %q[t2], q14,    #6      \n\t"
		"vsli.8  %q[t1], q12,    #4      \n\t"
		"vsli.8  %q[t2], q13,    #2      \n\t"
		"vand.u8 %q[t1], %q[t1], %q[n63] \n\t"
		"vand.u8 %q[t2], %q[t2], %q[n63] \n\t"
		"vand.u8 %q[t3], q14,    %q[n63] \n\t"

		// t0..t3 are the reshuffled inputs. Create LUT indices.
		"vqsub.u8 q12, %q[t0], %q[n51] \n\t"
		"vqsub.u8 q13, %q[t1], %q[n51] \n\t"
		"vqsub.u8 q14, %q[t2], %q[n51] \n\t"
		"vqsub.u8 q15, %q[t3], %q[n51] \n\t"

		// Create the mask for range #0.
		"vcgt.u8 %q[m0], %q[t0], %q[n25] \n\t"
		"vcgt.u8 %q[m1], %q[t1], %q[n25] \n\t"
		"vcgt.u8 %q[m2], %q[t2], %q[n25] \n\t"
		"vcgt.u8 %q[m3], %q[t3], %q[n25] \n\t"

		// Subtract -1 to correct the LUT indices.
		"vsub.u8 q12, %q[m0] \n\t"
		"vsub.u8 q13, %q[m1] \n\t"
		"vsub.u8 q14, %q[m2] \n\t"
		"vsub.u8 q15, %q[m3] \n\t"

		// Lookup the delta values.
		"vtbl.u8 d24, {%q[lut]}, d24 \n\t"
		"vtbl.u8 d25, {%q[lut]}, d25 \n\t"
		"vtbl.u8 d26, {%q[lut]}, d26 \n\t"
		"vtbl.u8 d27, {%q[lut]}, d27 \n\t"
		"vtbl.u8 d28, {%q[lut]}, d28 \n\t"
		"vtbl.u8 d29, {%q[lut]}, d29 \n\t"
		"vtbl.u8 d30, {%q[lut]}, d30 \n\t"
		"vtbl.u8 d31, {%q[lut]}, d31 \n\t"

		// Add the delta values.
		"vadd.u8 q12, %q[t0] \n\t"
		"vadd.u8 q13, %q[t1] \n\t"
		"vadd.u8 q14, %q[t2] \n\t"
		"vadd.u8 q15, %q[t3] \n\t"

		// Store 64 bytes and interleave. Increment the dest pointer.
		"vst4.8 {d24, d26, d28, d30}, [%[dst]]! \n\t"
		"vst4.8 {d25, d27, d29, d31}, [%[dst]]! \n\t"

		// Outputs (modified).
		: [src] "+r"  (*s),
		  [dst] "+r"  (*o),
		  [t0]  "=&w" (tmp0),
		  [t1]  "=&w" (tmp1),
		  [t2]  "=&w" (tmp2),
		  [t3]  "=&w" (tmp3),
		  [m0]  "=&w" (mask0),
		  [m1]  "=&w" (mask1),
		  [m2]  "=&w" (mask2),
		  [m3]  "=&w" (mask3)

		// Inputs (not modified).
		: [lut] "w" (lut),
		  [n25] "w" (n25),
		  [n51] "w" (n51),
		  [n63] "w" (n63)

		// Clobbers.
		: "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31",
		  "cc", "memory"
	);
}
#endif

static inline void
enc_loop_neon32_inner (const uint8_t **s, uint8_t **o)
{
#ifdef BASE64_NEON32_USE_ASM
	enc_loop_neon32_inner_asm(s, o);
#else
	// Load 48 bytes and deinterleave:
	uint8x16x3_t src = vld3q_u8(*s);

	// Reshuffle:
	uint8x16x4_t out = enc_reshuffle(src);

	// Translate reshuffled bytes to the Base64 alphabet:
	out = enc_translate(out);

	// Interleave and store output:
	vst4q_u8(*o, out);

	*s += 48;
	*o += 64;
#endif
}

static inline void
enc_loop_neon32 (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	size_t rounds = *slen / 48;

	*slen -= rounds * 48;	// 48 bytes consumed per round
	*olen += rounds * 64;	// 64 bytes produced per round

	while (rounds > 0) {
		if (rounds >= 8) {
			enc_loop_neon32_inner(s, o);
			enc_loop_neon32_inner(s, o);
			enc_loop_neon32_inner(s, o);
			enc_loop_neon32_inner(s, o);
			enc_loop_neon32_inner(s, o);
			enc_loop_neon32_inner(s, o);
			enc_loop_neon32_inner(s, o);
			enc_loop_neon32_inner(s, o);
			rounds -= 8;
			continue;
		}
		if (rounds >= 4) {
			enc_loop_neon32_inner(s, o);
			enc_loop_neon32_inner(s, o);
			enc_loop_neon32_inner(s, o);
			enc_loop_neon32_inner(s, o);
			rounds -= 4;
			continue;
		}
		if (rounds >= 2) {
			enc_loop_neon32_inner(s, o);
			enc_loop_neon32_inner(s, o);
			rounds -= 2;
			continue;
		}
		enc_loop_neon32_inner(s, o);
		break;
	}
}
