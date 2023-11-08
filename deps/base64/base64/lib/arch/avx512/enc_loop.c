static inline void
enc_loop_avx512_inner (const uint8_t **s, uint8_t **o)
{
	// Load input.
	__m512i src = _mm512_loadu_si512((__m512i *) *s);

	// Reshuffle, translate, store.
	src = enc_reshuffle_translate(src);
	_mm512_storeu_si512((__m512i *) *o, src);

	*s += 48;
	*o += 64;
}

static inline void
enc_loop_avx512 (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	if (*slen < 64) {
		return;
	}

	// Process blocks of 48 bytes at a time. Because blocks are loaded 64
	// bytes at a time, ensure that there will be at least 24 remaining
	// bytes after the last round, so that the final read will not pass
	// beyond the bounds of the input buffer.
	size_t rounds = (*slen - 24) / 48;

	*slen -= rounds * 48;   // 48 bytes consumed per round
	*olen += rounds * 64;   // 64 bytes produced per round

	while (rounds > 0) {
		if (rounds >= 8) {
			enc_loop_avx512_inner(s, o);
			enc_loop_avx512_inner(s, o);
			enc_loop_avx512_inner(s, o);
			enc_loop_avx512_inner(s, o);
			enc_loop_avx512_inner(s, o);
			enc_loop_avx512_inner(s, o);
			enc_loop_avx512_inner(s, o);
			enc_loop_avx512_inner(s, o);
			rounds -= 8;
			continue;
		}
		if (rounds >= 4) {
			enc_loop_avx512_inner(s, o);
			enc_loop_avx512_inner(s, o);
			enc_loop_avx512_inner(s, o);
			enc_loop_avx512_inner(s, o);
			rounds -= 4;
			continue;
		}
		if (rounds >= 2) {
			enc_loop_avx512_inner(s, o);
			enc_loop_avx512_inner(s, o);
			rounds -= 2;
			continue;
		}
		enc_loop_avx512_inner(s, o);
		break;
	}
}
