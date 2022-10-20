// AVX512 algorithm is based on permutevar and multishift. The code is based on
// https://github.com/WojciechMula/base64simd which is under BSD-2 license.

static inline __m512i
enc_reshuffle_translate (const __m512i input)
{
	// 32-bit input
	// [ 0  0  0  0  0  0  0  0|c1 c0 d5 d4 d3 d2 d1 d0|
	//  b3 b2 b1 b0 c5 c4 c3 c2|a5 a4 a3 a2 a1 a0 b5 b4]
	// output order  [1, 2, 0, 1]
	// [b3 b2 b1 b0 c5 c4 c3 c2|c1 c0 d5 d4 d3 d2 d1 d0|
	//  a5 a4 a3 a2 a1 a0 b5 b4|b3 b2 b1 b0 c3 c2 c1 c0]

	const __m512i shuffle_input = _mm512_setr_epi32(0x01020001,
	                                                0x04050304,
	                                                0x07080607,
	                                                0x0a0b090a,
	                                                0x0d0e0c0d,
	                                                0x10110f10,
	                                                0x13141213,
	                                                0x16171516,
	                                                0x191a1819,
	                                                0x1c1d1b1c,
	                                                0x1f201e1f,
	                                                0x22232122,
	                                                0x25262425,
	                                                0x28292728,
	                                                0x2b2c2a2b,
	                                                0x2e2f2d2e);

	// Reorder bytes
	// [b3 b2 b1 b0 c5 c4 c3 c2|c1 c0 d5 d4 d3 d2 d1 d0|
	//  a5 a4 a3 a2 a1 a0 b5 b4|b3 b2 b1 b0 c3 c2 c1 c0]
	const __m512i in = _mm512_permutexvar_epi8(shuffle_input, input);

	// After multishift a single 32-bit lane has following layout
	// [c1 c0 d5 d4 d3 d2 d1 d0|b1 b0 c5 c4 c3 c2 c1 c0|
	//  a1 a0 b5 b4 b3 b2 b1 b0|d1 d0 a5 a4 a3 a2 a1 a0]
	// (a = [10:17], b = [4:11], c = [22:27], d = [16:21])

	// 48, 54, 36, 42, 16, 22, 4, 10
	const __m512i shifts = _mm512_set1_epi64(0x3036242a1016040alu);
	__m512i shuffled_in = _mm512_multishift_epi64_epi8(shifts, in);

	// Translate immediatedly after reshuffled.
	const __m512i lookup = _mm512_loadu_si512(base64_table_enc_6bit);

	// Translation 6-bit values to ASCII.
	return _mm512_permutexvar_epi8(shuffled_in, lookup);
}
