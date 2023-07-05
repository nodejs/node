static inline int
dec_loop_generic_32_inner (const uint8_t **s, uint8_t **o, size_t *rounds)
{
	const uint32_t str
		= base64_table_dec_32bit_d0[(*s)[0]]
		| base64_table_dec_32bit_d1[(*s)[1]]
		| base64_table_dec_32bit_d2[(*s)[2]]
		| base64_table_dec_32bit_d3[(*s)[3]];

#if BASE64_LITTLE_ENDIAN

	// LUTs for little-endian set MSB in case of invalid character:
	if (str & UINT32_C(0x80000000)) {
		return 0;
	}
#else
	// LUTs for big-endian set LSB in case of invalid character:
	if (str & UINT32_C(1)) {
		return 0;
	}
#endif
	// Store the output:
	memcpy(*o, &str, sizeof (str));

	*s += 4;
	*o += 3;
	*rounds -= 1;

	return 1;
}

static inline void
dec_loop_generic_32 (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	if (*slen < 8) {
		return;
	}

	// Process blocks of 4 bytes per round. Because one extra zero byte is
	// written after the output, ensure that there will be at least 4 bytes
	// of input data left to cover the gap. (Two data bytes and up to two
	// end-of-string markers.)
	size_t rounds = (*slen - 4) / 4;

	*slen -= rounds * 4;	// 4 bytes consumed per round
	*olen += rounds * 3;	// 3 bytes produced per round

	do {
		if (rounds >= 8) {
			if (dec_loop_generic_32_inner(s, o, &rounds) &&
			    dec_loop_generic_32_inner(s, o, &rounds) &&
			    dec_loop_generic_32_inner(s, o, &rounds) &&
			    dec_loop_generic_32_inner(s, o, &rounds) &&
			    dec_loop_generic_32_inner(s, o, &rounds) &&
			    dec_loop_generic_32_inner(s, o, &rounds) &&
			    dec_loop_generic_32_inner(s, o, &rounds) &&
			    dec_loop_generic_32_inner(s, o, &rounds)) {
				continue;
			}
			break;
		}
		if (rounds >= 4) {
			if (dec_loop_generic_32_inner(s, o, &rounds) &&
			    dec_loop_generic_32_inner(s, o, &rounds) &&
			    dec_loop_generic_32_inner(s, o, &rounds) &&
			    dec_loop_generic_32_inner(s, o, &rounds)) {
				continue;
			}
			break;
		}
		if (rounds >= 2) {
			if (dec_loop_generic_32_inner(s, o, &rounds) &&
			    dec_loop_generic_32_inner(s, o, &rounds)) {
				continue;
			}
			break;
		}
		dec_loop_generic_32_inner(s, o, &rounds);
		break;

	} while (rounds > 0);

	// Adjust for any rounds that were skipped:
	*slen += rounds * 4;
	*olen -= rounds * 3;
}
