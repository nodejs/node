// The input consists of six character sets in the Base64 alphabet, which we
// need to map back to the 6-bit values they represent. There are three ranges,
// two singles, and then there's the rest.
//
//  #  From       To        Add  Characters
//  1  [43]       [62]      +19  +
//  2  [47]       [63]      +16  /
//  3  [48..57]   [52..61]   +4  0..9
//  4  [65..90]   [0..25]   -65  A..Z
//  5  [97..122]  [26..51]  -71  a..z
// (6) Everything else => invalid input
//
// We will use lookup tables for character validation and offset computation.
// Remember that 0x2X and 0x0X are the same index for _mm_shuffle_epi8, this
// allows to mask with 0x2F instead of 0x0F and thus save one constant
// declaration (register and/or memory access).
//
// For offsets:
// Perfect hash for lut = ((src >> 4) & 0x2F) + ((src == 0x2F) ? 0xFF : 0x00)
// 0000 = garbage
// 0001 = /
// 0010 = +
// 0011 = 0-9
// 0100 = A-Z
// 0101 = A-Z
// 0110 = a-z
// 0111 = a-z
// 1000 >= garbage
//
// For validation, here's the table.
// A character is valid if and only if the AND of the 2 lookups equals 0:
//
// hi \ lo              0000 0001 0010 0011 0100 0101 0110 0111 1000 1001 1010 1011 1100 1101 1110 1111
//      LUT             0x15 0x11 0x11 0x11 0x11 0x11 0x11 0x11 0x11 0x11 0x13 0x1A 0x1B 0x1B 0x1B 0x1A
//
// 0000 0x10 char        NUL  SOH  STX  ETX  EOT  ENQ  ACK  BEL   BS   HT   LF   VT   FF   CR   SO   SI
//           andlut     0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10
//
// 0001 0x10 char        DLE  DC1  DC2  DC3  DC4  NAK  SYN  ETB  CAN   EM  SUB  ESC   FS   GS   RS   US
//           andlut     0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10
//
// 0010 0x01 char               !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /
//           andlut     0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x01 0x00 0x01 0x01 0x01 0x00
//
// 0011 0x02 char          0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?
//           andlut     0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x02 0x02 0x02 0x02 0x02 0x02
//
// 0100 0x04 char          @    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O
//           andlut     0x04 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
//
// 0101 0x08 char          P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _
//           andlut     0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x08 0x08 0x08 0x08 0x08
//
// 0110 0x04 char          `    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o
//           andlut     0x04 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
// 0111 0x08 char          p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~
//           andlut     0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x08 0x08 0x08 0x08 0x08
//
// 1000 0x10 andlut     0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10
// 1001 0x10 andlut     0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10
// 1010 0x10 andlut     0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10
// 1011 0x10 andlut     0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10
// 1100 0x10 andlut     0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10
// 1101 0x10 andlut     0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10
// 1110 0x10 andlut     0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10
// 1111 0x10 andlut     0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10 0x10

static inline int
dec_loop_ssse3_inner (const uint8_t **s, uint8_t **o, size_t *rounds)
{
	const __m128i lut_lo = _mm_setr_epi8(
		0x15, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x13, 0x1A, 0x1B, 0x1B, 0x1B, 0x1A);

	const __m128i lut_hi = _mm_setr_epi8(
		0x10, 0x10, 0x01, 0x02, 0x04, 0x08, 0x04, 0x08,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10);

	const __m128i lut_roll = _mm_setr_epi8(
		0,  16,  19,   4, -65, -65, -71, -71,
		0,   0,   0,   0,   0,   0,   0,   0);

	const __m128i mask_2F = _mm_set1_epi8(0x2F);

	// Load input:
	__m128i str = _mm_loadu_si128((__m128i *) *s);

	// Table lookups:
	const __m128i hi_nibbles = _mm_and_si128(_mm_srli_epi32(str, 4), mask_2F);
	const __m128i lo_nibbles = _mm_and_si128(str, mask_2F);
	const __m128i hi         = _mm_shuffle_epi8(lut_hi, hi_nibbles);
	const __m128i lo         = _mm_shuffle_epi8(lut_lo, lo_nibbles);

	// Check for invalid input: if any "and" values from lo and hi are not
	// zero, fall back on bytewise code to do error checking and reporting:
	if (_mm_movemask_epi8(_mm_cmpgt_epi8(_mm_and_si128(lo, hi), _mm_setzero_si128())) != 0) {
		return 0;
	}

	const __m128i eq_2F = _mm_cmpeq_epi8(str, mask_2F);
	const __m128i roll  = _mm_shuffle_epi8(lut_roll, _mm_add_epi8(eq_2F, hi_nibbles));

	// Now simply add the delta values to the input:
	str = _mm_add_epi8(str, roll);

	// Reshuffle the input to packed 12-byte output format:
	str = dec_reshuffle(str);

	// Store the output:
	_mm_storeu_si128((__m128i *) *o, str);

	*s += 16;
	*o += 12;
	*rounds -= 1;

	return 1;
}

static inline void
dec_loop_ssse3 (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	if (*slen < 24) {
		return;
	}

	// Process blocks of 16 bytes per round. Because 4 extra zero bytes are
	// written after the output, ensure that there will be at least 8 bytes
	// of input data left to cover the gap. (6 data bytes and up to two
	// end-of-string markers.)
	size_t rounds = (*slen - 8) / 16;

	*slen -= rounds * 16;	// 16 bytes consumed per round
	*olen += rounds * 12;	// 12 bytes produced per round

	do {
		if (rounds >= 8) {
			if (dec_loop_ssse3_inner(s, o, &rounds) &&
			    dec_loop_ssse3_inner(s, o, &rounds) &&
			    dec_loop_ssse3_inner(s, o, &rounds) &&
			    dec_loop_ssse3_inner(s, o, &rounds) &&
			    dec_loop_ssse3_inner(s, o, &rounds) &&
			    dec_loop_ssse3_inner(s, o, &rounds) &&
			    dec_loop_ssse3_inner(s, o, &rounds) &&
			    dec_loop_ssse3_inner(s, o, &rounds)) {
				continue;
			}
			break;
		}
		if (rounds >= 4) {
			if (dec_loop_ssse3_inner(s, o, &rounds) &&
			    dec_loop_ssse3_inner(s, o, &rounds) &&
			    dec_loop_ssse3_inner(s, o, &rounds) &&
			    dec_loop_ssse3_inner(s, o, &rounds)) {
				continue;
			}
			break;
		}
		if (rounds >= 2) {
			if (dec_loop_ssse3_inner(s, o, &rounds) &&
			    dec_loop_ssse3_inner(s, o, &rounds)) {
				continue;
			}
			break;
		}
		dec_loop_ssse3_inner(s, o, &rounds);
		break;

	} while (rounds > 0);

	// Adjust for any rounds that were skipped:
	*slen += rounds * 16;
	*olen -= rounds * 12;
}
