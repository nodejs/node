static inline uint8x16x4_t
enc_reshuffle (uint8x16x3_t in)
{
	uint8x16x4_t out;

	// Input:
	// in[0]  = a7 a6 a5 a4 a3 a2 a1 a0
	// in[1]  = b7 b6 b5 b4 b3 b2 b1 b0
	// in[2]  = c7 c6 c5 c4 c3 c2 c1 c0

	// Output:
	// out[0] = 00 00 a7 a6 a5 a4 a3 a2
	// out[1] = 00 00 a1 a0 b7 b6 b5 b4
	// out[2] = 00 00 b3 b2 b1 b0 c7 c6
	// out[3] = 00 00 c5 c4 c3 c2 c1 c0

	// Move the input bits to where they need to be in the outputs. Except
	// for the first output, the high two bits are not cleared.
	out.val[0] = vshrq_n_u8(in.val[0], 2);
	out.val[1] = vshrq_n_u8(in.val[1], 4);
	out.val[2] = vshrq_n_u8(in.val[2], 6);
	out.val[1] = vsliq_n_u8(out.val[1], in.val[0], 4);
	out.val[2] = vsliq_n_u8(out.val[2], in.val[1], 2);

	// Clear the high two bits in the second, third and fourth output.
	out.val[1] = vandq_u8(out.val[1], vdupq_n_u8(0x3F));
	out.val[2] = vandq_u8(out.val[2], vdupq_n_u8(0x3F));
	out.val[3] = vandq_u8(in.val[2],  vdupq_n_u8(0x3F));

	return out;
}
