static inline void
enc_loop_neon64_inner (const uint8_t **s, uint8_t **o, const uint8x16x4_t tbl_enc)
{
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
