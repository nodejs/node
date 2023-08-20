// The input consists of five valid character sets in the Base64 alphabet,
// which we need to map back to the 6-bit values they represent.
// There are three ranges, two singles, and then there's the rest.
//
//   #  From       To        LUT  Characters
//   1  [0..42]    [255]      #1  invalid input
//   2  [43]       [62]       #1  +
//   3  [44..46]   [255]      #1  invalid input
//   4  [47]       [63]       #1  /
//   5  [48..57]   [52..61]   #1  0..9
//   6  [58..63]   [255]      #1  invalid input
//   7  [64]       [255]      #2  invalid input
//   8  [65..90]   [0..25]    #2  A..Z
//   9  [91..96]   [255]      #2  invalid input
//  10  [97..122]  [26..51]   #2  a..z
//  11  [123..126] [255]      #2  invalid input
// (12) Everything else => invalid input

// The first LUT will use the VTBL instruction (out of range indices are set to
// 0 in destination).
static const uint8_t dec_lut1[] = {
	255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,
	255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,
	255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U,  62U, 255U, 255U, 255U,  63U,
	 52U,  53U,  54U,  55U,  56U,  57U,  58U,  59U,  60U,  61U, 255U, 255U, 255U, 255U, 255U, 255U,
};

// The second LUT will use the VTBX instruction (out of range indices will be
// unchanged in destination). Input [64..126] will be mapped to index [1..63]
// in this LUT. Index 0 means that value comes from LUT #1.
static const uint8_t dec_lut2[] = {
	  0U, 255U,   0U,   1U,   2U,   3U,   4U,   5U,   6U,   7U,   8U,   9U,  10U,  11U,  12U,  13U,
	 14U,  15U,  16U,  17U,  18U,  19U,  20U,  21U,  22U,  23U,  24U,  25U, 255U, 255U, 255U, 255U,
	255U, 255U,  26U,  27U,  28U,  29U,  30U,  31U,  32U,  33U,  34U,  35U,  36U,  37U,  38U,  39U,
	 40U,  41U,  42U,  43U,  44U,  45U,  46U,  47U,  48U,  49U,  50U,  51U, 255U, 255U, 255U, 255U,
};

// All input values in range for the first look-up will be 0U in the second
// look-up result. All input values out of range for the first look-up will be
// 0U in the first look-up result. Thus, the two results can be ORed without
// conflicts.
//
// Invalid characters that are in the valid range for either look-up will be
// set to 255U in the combined result. Other invalid characters will just be
// passed through with the second look-up result (using the VTBX instruction).
// Since the second LUT is 64 bytes, those passed-through values are guaranteed
// to have a value greater than 63U. Therefore, valid characters will be mapped
// to the valid [0..63] range and all invalid characters will be mapped to
// values greater than 63.

static inline void
dec_loop_neon64 (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	if (*slen < 64) {
		return;
	}

	// Process blocks of 64 bytes per round. Unlike the SSE codecs, no
	// extra trailing zero bytes are written, so it is not necessary to
	// reserve extra input bytes:
	size_t rounds = *slen / 64;

	*slen -= rounds * 64;	// 64 bytes consumed per round
	*olen += rounds * 48;	// 48 bytes produced per round

	const uint8x16x4_t tbl_dec1 = load_64byte_table(dec_lut1);
	const uint8x16x4_t tbl_dec2 = load_64byte_table(dec_lut2);

	do {
		const uint8x16_t offset = vdupq_n_u8(63U);
		uint8x16x4_t dec1, dec2;
		uint8x16x3_t dec;

		// Load 64 bytes and deinterleave:
		uint8x16x4_t str = vld4q_u8((uint8_t *) *s);

		// Get indices for second LUT:
		dec2.val[0] = vqsubq_u8(str.val[0], offset);
		dec2.val[1] = vqsubq_u8(str.val[1], offset);
		dec2.val[2] = vqsubq_u8(str.val[2], offset);
		dec2.val[3] = vqsubq_u8(str.val[3], offset);

		// Get values from first LUT:
		dec1.val[0] = vqtbl4q_u8(tbl_dec1, str.val[0]);
		dec1.val[1] = vqtbl4q_u8(tbl_dec1, str.val[1]);
		dec1.val[2] = vqtbl4q_u8(tbl_dec1, str.val[2]);
		dec1.val[3] = vqtbl4q_u8(tbl_dec1, str.val[3]);

		// Get values from second LUT:
		dec2.val[0] = vqtbx4q_u8(dec2.val[0], tbl_dec2, dec2.val[0]);
		dec2.val[1] = vqtbx4q_u8(dec2.val[1], tbl_dec2, dec2.val[1]);
		dec2.val[2] = vqtbx4q_u8(dec2.val[2], tbl_dec2, dec2.val[2]);
		dec2.val[3] = vqtbx4q_u8(dec2.val[3], tbl_dec2, dec2.val[3]);

		// Get final values:
		str.val[0] = vorrq_u8(dec1.val[0], dec2.val[0]);
		str.val[1] = vorrq_u8(dec1.val[1], dec2.val[1]);
		str.val[2] = vorrq_u8(dec1.val[2], dec2.val[2]);
		str.val[3] = vorrq_u8(dec1.val[3], dec2.val[3]);

		// Check for invalid input, any value larger than 63:
		const uint8x16_t classified
			= vcgtq_u8(str.val[0], vdupq_n_u8(63))
			| vcgtq_u8(str.val[1], vdupq_n_u8(63))
			| vcgtq_u8(str.val[2], vdupq_n_u8(63))
			| vcgtq_u8(str.val[3], vdupq_n_u8(63));

		// Check that all bits are zero:
		if (vmaxvq_u8(classified) != 0U) {
			break;
		}

		// Compress four bytes into three:
		dec.val[0] = vshlq_n_u8(str.val[0], 2) | vshrq_n_u8(str.val[1], 4);
		dec.val[1] = vshlq_n_u8(str.val[1], 4) | vshrq_n_u8(str.val[2], 2);
		dec.val[2] = vshlq_n_u8(str.val[2], 6) | str.val[3];

		// Interleave and store decoded result:
		vst3q_u8((uint8_t *) *o, dec);

		*s += 64;
		*o += 48;

	} while (--rounds > 0);

	// Adjust for any rounds that were skipped:
	*slen += rounds * 64;
	*olen -= rounds * 48;
}
