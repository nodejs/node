static inline __m128i
enc_reshuffle (__m128i in)
{
	// Input, bytes MSB to LSB:
	// 0 0 0 0 l k j i h g f e d c b a

	in = _mm_shuffle_epi8(in, _mm_set_epi8(
		10, 11,  9, 10,
		 7,  8,  6,  7,
		 4,  5,  3,  4,
		 1,  2,  0,  1));
	// in, bytes MSB to LSB:
	// k l j k
	// h i g h
	// e f d e
	// b c a b

	const __m128i t0 = _mm_and_si128(in, _mm_set1_epi32(0x0FC0FC00));
	// bits, upper case are most significant bits, lower case are least significant bits
	// 0000kkkk LL000000 JJJJJJ00 00000000
	// 0000hhhh II000000 GGGGGG00 00000000
	// 0000eeee FF000000 DDDDDD00 00000000
	// 0000bbbb CC000000 AAAAAA00 00000000

	const __m128i t1 = _mm_mulhi_epu16(t0, _mm_set1_epi32(0x04000040));
	// 00000000 00kkkkLL 00000000 00JJJJJJ
	// 00000000 00hhhhII 00000000 00GGGGGG
	// 00000000 00eeeeFF 00000000 00DDDDDD
	// 00000000 00bbbbCC 00000000 00AAAAAA

	const __m128i t2 = _mm_and_si128(in, _mm_set1_epi32(0x003F03F0));
	// 00000000 00llllll 000000jj KKKK0000
	// 00000000 00iiiiii 000000gg HHHH0000
	// 00000000 00ffffff 000000dd EEEE0000
	// 00000000 00cccccc 000000aa BBBB0000

	const __m128i t3 = _mm_mullo_epi16(t2, _mm_set1_epi32(0x01000010));
	// 00llllll 00000000 00jjKKKK 00000000
	// 00iiiiii 00000000 00ggHHHH 00000000
	// 00ffffff 00000000 00ddEEEE 00000000
	// 00cccccc 00000000 00aaBBBB 00000000

	return _mm_or_si128(t1, t3);
	// 00llllll 00kkkkLL 00jjKKKK 00JJJJJJ
	// 00iiiiii 00hhhhII 00ggHHHH 00GGGGGG
	// 00ffffff 00eeeeFF 00ddEEEE 00DDDDDD
	// 00cccccc 00bbbbCC 00aaBBBB 00AAAAAA
}
