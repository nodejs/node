static inline __m128i
enc_translate (const __m128i in)
{
	// A lookup table containing the absolute offsets for all ranges:
	const __m128i lut = _mm_setr_epi8(
		 65,  71, -4, -4,
		 -4,  -4, -4, -4,
		 -4,  -4, -4, -4,
		-19, -16,  0,  0
	);

	// Translate values 0..63 to the Base64 alphabet. There are five sets:
	// #  From      To         Abs    Index  Characters
	// 0  [0..25]   [65..90]   +65        0  ABCDEFGHIJKLMNOPQRSTUVWXYZ
	// 1  [26..51]  [97..122]  +71        1  abcdefghijklmnopqrstuvwxyz
	// 2  [52..61]  [48..57]    -4  [2..11]  0123456789
	// 3  [62]      [43]       -19       12  +
	// 4  [63]      [47]       -16       13  /

	// Create LUT indices from the input. The index for range #0 is right,
	// others are 1 less than expected:
	__m128i indices = _mm_subs_epu8(in, _mm_set1_epi8(51));

	// mask is 0xFF (-1) for range #[1..4] and 0x00 for range #0:
	__m128i mask = _mm_cmpgt_epi8(in, _mm_set1_epi8(25));

	// Subtract -1, so add 1 to indices for range #[1..4]. All indices are
	// now correct:
	indices = _mm_sub_epi8(indices, mask);

	// Add offsets to input values:
	return _mm_add_epi8(in, _mm_shuffle_epi8(lut, indices));
}
