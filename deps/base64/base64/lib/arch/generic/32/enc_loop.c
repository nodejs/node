static inline void
enc_loop_generic_32_inner (const uint8_t **s, uint8_t **o)
{
	uint32_t src;

	// Load input:
	memcpy(&src, *s, sizeof (src));

	// Reorder to 32-bit big-endian, if not already in that format. The
	// workset must be in big-endian, otherwise the shifted bits do not
	// carry over properly among adjacent bytes:
	src = BASE64_HTOBE32(src);

	// Two indices for the 12-bit lookup table:
	const size_t index0 = (src >> 20) & 0xFFFU;
	const size_t index1 = (src >>  8) & 0xFFFU;

	// Table lookup and store:
	memcpy(*o + 0, base64_table_enc_12bit + index0, 2);
	memcpy(*o + 2, base64_table_enc_12bit + index1, 2);

	*s += 3;
	*o += 4;
}

static inline void
enc_loop_generic_32 (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	if (*slen < 4) {
		return;
	}

	// Process blocks of 3 bytes at a time. Because blocks are loaded 4
	// bytes at a time, ensure that there will be at least one remaining
	// byte after the last round, so that the final read will not pass
	// beyond the bounds of the input buffer:
	size_t rounds = (*slen - 1) / 3;

	*slen -= rounds * 3;	// 3 bytes consumed per round
	*olen += rounds * 4;	// 4 bytes produced per round

	do {
		if (rounds >= 8) {
			enc_loop_generic_32_inner(s, o);
			enc_loop_generic_32_inner(s, o);
			enc_loop_generic_32_inner(s, o);
			enc_loop_generic_32_inner(s, o);
			enc_loop_generic_32_inner(s, o);
			enc_loop_generic_32_inner(s, o);
			enc_loop_generic_32_inner(s, o);
			enc_loop_generic_32_inner(s, o);
			rounds -= 8;
			continue;
		}
		if (rounds >= 4) {
			enc_loop_generic_32_inner(s, o);
			enc_loop_generic_32_inner(s, o);
			enc_loop_generic_32_inner(s, o);
			enc_loop_generic_32_inner(s, o);
			rounds -= 4;
			continue;
		}
		if (rounds >= 2) {
			enc_loop_generic_32_inner(s, o);
			enc_loop_generic_32_inner(s, o);
			rounds -= 2;
			continue;
		}
		enc_loop_generic_32_inner(s, o);
		break;

	} while (rounds > 0);
}
