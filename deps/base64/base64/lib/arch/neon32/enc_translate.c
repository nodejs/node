static inline uint8x16x4_t
enc_translate (const uint8x16x4_t in)
{
	// A lookup table containing the absolute offsets for all ranges:
	const uint8x16_t lut = {
		 65U,  71U, 252U, 252U,
		252U, 252U, 252U, 252U,
		252U, 252U, 252U, 252U,
		237U, 240U,   0U,   0U
	};

	const uint8x16_t offset = vdupq_n_u8(51);

	uint8x16x4_t indices, mask, delta, out;

	// Translate values 0..63 to the Base64 alphabet. There are five sets:
	// #  From      To         Abs    Index  Characters
	// 0  [0..25]   [65..90]   +65        0  ABCDEFGHIJKLMNOPQRSTUVWXYZ
	// 1  [26..51]  [97..122]  +71        1  abcdefghijklmnopqrstuvwxyz
	// 2  [52..61]  [48..57]    -4  [2..11]  0123456789
	// 3  [62]      [43]       -19       12  +
	// 4  [63]      [47]       -16       13  /

	// Create LUT indices from input:
	// the index for range #0 is right, others are 1 less than expected:
	indices.val[0] = vqsubq_u8(in.val[0], offset);
	indices.val[1] = vqsubq_u8(in.val[1], offset);
	indices.val[2] = vqsubq_u8(in.val[2], offset);
	indices.val[3] = vqsubq_u8(in.val[3], offset);

	// mask is 0xFF (-1) for range #[1..4] and 0x00 for range #0:
	mask.val[0] = vcgtq_u8(in.val[0], vdupq_n_u8(25));
	mask.val[1] = vcgtq_u8(in.val[1], vdupq_n_u8(25));
	mask.val[2] = vcgtq_u8(in.val[2], vdupq_n_u8(25));
	mask.val[3] = vcgtq_u8(in.val[3], vdupq_n_u8(25));

	// Subtract -1, so add 1 to indices for range #[1..4], All indices are
	// now correct:
	indices.val[0] = vsubq_u8(indices.val[0], mask.val[0]);
	indices.val[1] = vsubq_u8(indices.val[1], mask.val[1]);
	indices.val[2] = vsubq_u8(indices.val[2], mask.val[2]);
	indices.val[3] = vsubq_u8(indices.val[3], mask.val[3]);

	// Lookup delta values:
	delta.val[0] = vqtbl1q_u8(lut, indices.val[0]);
	delta.val[1] = vqtbl1q_u8(lut, indices.val[1]);
	delta.val[2] = vqtbl1q_u8(lut, indices.val[2]);
	delta.val[3] = vqtbl1q_u8(lut, indices.val[3]);

	// Add delta values:
	out.val[0] = vaddq_u8(in.val[0], delta.val[0]);
	out.val[1] = vaddq_u8(in.val[1], delta.val[1]);
	out.val[2] = vaddq_u8(in.val[2], delta.val[2]);
	out.val[3] = vaddq_u8(in.val[3], delta.val[3]);

	return out;
}
