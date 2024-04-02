static inline void
enc_loop_ssse3_inner (const uint8_t **s, uint8_t **o)
{
	// Load input:
	__m128i str = _mm_loadu_si128((__m128i *) *s);

	// Reshuffle:
	str = enc_reshuffle(str);

	// Translate reshuffled bytes to the Base64 alphabet:
	str = enc_translate(str);

	// Store:
	_mm_storeu_si128((__m128i *) *o, str);

	*s += 12;
	*o += 16;
}

static inline void
enc_loop_ssse3 (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	if (*slen < 16) {
		return;
	}

	// Process blocks of 12 bytes at a time. Because blocks are loaded 16
	// bytes at a time, ensure that there will be at least 4 remaining
	// bytes after the last round, so that the final read will not pass
	// beyond the bounds of the input buffer:
	size_t rounds = (*slen - 4) / 12;

	*slen -= rounds * 12;	// 12 bytes consumed per round
	*olen += rounds * 16;	// 16 bytes produced per round

	do {
		if (rounds >= 8) {
			enc_loop_ssse3_inner(s, o);
			enc_loop_ssse3_inner(s, o);
			enc_loop_ssse3_inner(s, o);
			enc_loop_ssse3_inner(s, o);
			enc_loop_ssse3_inner(s, o);
			enc_loop_ssse3_inner(s, o);
			enc_loop_ssse3_inner(s, o);
			enc_loop_ssse3_inner(s, o);
			rounds -= 8;
			continue;
		}
		if (rounds >= 4) {
			enc_loop_ssse3_inner(s, o);
			enc_loop_ssse3_inner(s, o);
			enc_loop_ssse3_inner(s, o);
			enc_loop_ssse3_inner(s, o);
			rounds -= 4;
			continue;
		}
		if (rounds >= 2) {
			enc_loop_ssse3_inner(s, o);
			enc_loop_ssse3_inner(s, o);
			rounds -= 2;
			continue;
		}
		enc_loop_ssse3_inner(s, o);
		break;

	} while (rounds > 0);
}
