static inline void
enc_loop_generic_64_inner (const uint8_t **s, uint8_t **o)
{
	uint64_t src;

	// Load input:
	memcpy(&src, *s, sizeof (src));

	// Reorder to 64-bit big-endian, if not already in that format. The
	// workset must be in big-endian, otherwise the shifted bits do not
	// carry over properly among adjacent bytes:
	src = BASE64_HTOBE64(src);

	// Four indices for the 12-bit lookup table:
	const size_t index0 = (src >> 52) & 0xFFFU;
	const size_t index1 = (src >> 40) & 0xFFFU;
	const size_t index2 = (src >> 28) & 0xFFFU;
	const size_t index3 = (src >> 16) & 0xFFFU;

	// Table lookup and store:
	memcpy(*o + 0, base64_table_enc_12bit + index0, 2);
	memcpy(*o + 2, base64_table_enc_12bit + index1, 2);
	memcpy(*o + 4, base64_table_enc_12bit + index2, 2);
	memcpy(*o + 6, base64_table_enc_12bit + index3, 2);

	*s += 6;
	*o += 8;
}

static inline void
enc_loop_generic_64 (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	if (*slen < 8) {
		return;
	}

	// Process blocks of 6 bytes at a time. Because blocks are loaded 8
	// bytes at a time, ensure that there will be at least 2 remaining
	// bytes after the last round, so that the final read will not pass
	// beyond the bounds of the input buffer:
	size_t rounds = (*slen - 2) / 6;

	*slen -= rounds * 6;	// 6 bytes consumed per round
	*olen += rounds * 8;	// 8 bytes produced per round

	do {
		if (rounds >= 8) {
			enc_loop_generic_64_inner(s, o);
			enc_loop_generic_64_inner(s, o);
			enc_loop_generic_64_inner(s, o);
			enc_loop_generic_64_inner(s, o);
			enc_loop_generic_64_inner(s, o);
			enc_loop_generic_64_inner(s, o);
			enc_loop_generic_64_inner(s, o);
			enc_loop_generic_64_inner(s, o);
			rounds -= 8;
			continue;
		}
		if (rounds >= 4) {
			enc_loop_generic_64_inner(s, o);
			enc_loop_generic_64_inner(s, o);
			enc_loop_generic_64_inner(s, o);
			enc_loop_generic_64_inner(s, o);
			rounds -= 4;
			continue;
		}
		if (rounds >= 2) {
			enc_loop_generic_64_inner(s, o);
			enc_loop_generic_64_inner(s, o);
			rounds -= 2;
			continue;
		}
		enc_loop_generic_64_inner(s, o);
		break;

	} while (rounds > 0);
}
