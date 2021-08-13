#ifdef BASE64_NEON64_USE_ASM
static inline void
enc_loop_neon64_inner_asm (const uint8_t **s, uint8_t **o, const uint8x16x4_t tbl_enc)
{
	// This function duplicates the functionality of enc_loop_neon64_inner,
	// but entirely with inline assembly. This gives a significant speedup
	// over using NEON intrinsics, which do not always generate very good
	// code. The logic of the assembly is directly lifted from the
	// intrinsics version, so it can be used as a guide to this code.

	// Temporary registers, used as scratch space.
	uint8x16_t tmp0, tmp1, tmp2, tmp3;

	// Numeric constant.
	const uint8x16_t n63 = vdupq_n_u8(63);

	__asm__ (

		// Load 48 bytes and deinterleave. The bytes are loaded to
		// hard-coded registers v12, v13 and v14, to ensure that they
		// are contiguous. Increment the source pointer.
		"ld3 {v12.16b, v13.16b, v14.16b}, [%[src]], #48 \n\t"

		// Reshuffle the bytes using temporaries.
		"ushr %[t0].16b, v12.16b,   #2         \n\t"
		"ushr %[t1].16b, v13.16b,   #4         \n\t"
		"ushr %[t2].16b, v14.16b,   #6         \n\t"
		"sli  %[t1].16b, v12.16b,   #4         \n\t"
		"sli  %[t2].16b, v13.16b,   #2         \n\t"
		"and  %[t1].16b, %[t1].16b, %[n63].16b \n\t"
		"and  %[t2].16b, %[t2].16b, %[n63].16b \n\t"
		"and  %[t3].16b, v14.16b,   %[n63].16b \n\t"

		// Translate the values to the Base64 alphabet.
		"tbl v12.16b, {%[l0].16b, %[l1].16b, %[l2].16b, %[l3].16b}, %[t0].16b \n\t"
		"tbl v13.16b, {%[l0].16b, %[l1].16b, %[l2].16b, %[l3].16b}, %[t1].16b \n\t"
		"tbl v14.16b, {%[l0].16b, %[l1].16b, %[l2].16b, %[l3].16b}, %[t2].16b \n\t"
		"tbl v15.16b, {%[l0].16b, %[l1].16b, %[l2].16b, %[l3].16b}, %[t3].16b \n\t"

		// Store 64 bytes and interleave. Increment the dest pointer.
		"st4 {v12.16b, v13.16b, v14.16b, v15.16b}, [%[dst]], #64 \n\t"

		// Outputs (modified).
		: [src] "+r"  (*s),
		  [dst] "+r"  (*o),
		  [t0]  "=&w" (tmp0),
		  [t1]  "=&w" (tmp1),
		  [t2]  "=&w" (tmp2),
		  [t3]  "=&w" (tmp3)

		// Inputs (not modified).
		: [n63] "w" (n63),
		  [l0]  "w" (tbl_enc.val[0]),
		  [l1]  "w" (tbl_enc.val[1]),
		  [l2]  "w" (tbl_enc.val[2]),
		  [l3]  "w" (tbl_enc.val[3])

		// Clobbers.
		: "v12", "v13", "v14", "v15"
	);
}
#endif

static inline void
enc_loop_neon64_inner (const uint8_t **s, uint8_t **o, const uint8x16x4_t tbl_enc)
{
#ifdef BASE64_NEON64_USE_ASM
	enc_loop_neon64_inner_asm(s, o, tbl_enc);
#else
	// Load 48 bytes and deinterleave:
	uint8x16x3_t src = vld3q_u8(*s);

	// Divide bits of three input bytes over four output bytes:
	uint8x16x4_t out = enc_reshuffle(src);

	// The bits have now been shifted to the right locations;
	// translate their values 0..63 to the Base64 alphabet.
	// Use a 64-byte table lookup:
	out.val[0] = vqtbl4q_u8(tbl_enc, out.val[0]);
	out.val[1] = vqtbl4q_u8(tbl_enc, out.val[1]);
	out.val[2] = vqtbl4q_u8(tbl_enc, out.val[2]);
	out.val[3] = vqtbl4q_u8(tbl_enc, out.val[3]);

	// Interleave and store output:
	vst4q_u8(*o, out);

	*s += 48;
	*o += 64;
#endif
}

static inline void
enc_loop_neon64 (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	size_t rounds = *slen / 48;

	*slen -= rounds * 48;	// 48 bytes consumed per round
	*olen += rounds * 64;	// 64 bytes produced per round

	// Load the encoding table:
	const uint8x16x4_t tbl_enc = load_64byte_table(base64_table_enc_6bit);

	while (rounds > 0) {
		if (rounds >= 8) {
			enc_loop_neon64_inner(s, o, tbl_enc);
			enc_loop_neon64_inner(s, o, tbl_enc);
			enc_loop_neon64_inner(s, o, tbl_enc);
			enc_loop_neon64_inner(s, o, tbl_enc);
			enc_loop_neon64_inner(s, o, tbl_enc);
			enc_loop_neon64_inner(s, o, tbl_enc);
			enc_loop_neon64_inner(s, o, tbl_enc);
			enc_loop_neon64_inner(s, o, tbl_enc);
			rounds -= 8;
			continue;
		}
		if (rounds >= 4) {
			enc_loop_neon64_inner(s, o, tbl_enc);
			enc_loop_neon64_inner(s, o, tbl_enc);
			enc_loop_neon64_inner(s, o, tbl_enc);
			enc_loop_neon64_inner(s, o, tbl_enc);
			rounds -= 4;
			continue;
		}
		if (rounds >= 2) {
			enc_loop_neon64_inner(s, o, tbl_enc);
			enc_loop_neon64_inner(s, o, tbl_enc);
			rounds -= 2;
			continue;
		}
		enc_loop_neon64_inner(s, o, tbl_enc);
		break;
	}
}
