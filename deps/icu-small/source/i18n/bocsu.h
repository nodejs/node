/*
*******************************************************************************
*   Copyright (C) 2001-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  bocsu.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   Author: Markus W. Scherer
*
*   Modification history:
*   05/18/2001  weiv    Made into separate module
*/

#ifndef BOCSU_H
#define BOCSU_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

U_NAMESPACE_BEGIN

class ByteSink;

U_NAMESPACE_END

/*
 * "BOCSU"
 * Binary Ordered Compression Scheme for Unicode
 *
 * Specific application:
 *
 * Encode a Unicode string for the identical level of a sort key.
 * Restrictions:
 * - byte stream (unsigned 8-bit bytes)
 * - lexical order of the identical-level run must be
 *   the same as code point order for the string
 * - avoid byte values 0, 1, 2
 *
 * Method: Slope Detection
 * Remember the previous code point (initial 0).
 * For each cp in the string, encode the difference to the previous one.
 *
 * With a compact encoding of differences, this yields good results for
 * small scripts and UTF-like results otherwise.
 *
 * Encoding of differences:
 * - Similar to a UTF, encoding the length of the byte sequence in the lead bytes.
 * - Does not need to be friendly for decoding or random access
 *   (trail byte values may overlap with lead/single byte values).
 * - The signedness must be encoded as the most significant part.
 *
 * We encode differences with few bytes if their absolute values are small.
 * For correct ordering, we must treat the entire value range -10ffff..+10ffff
 * in ascending order, which forbids encoding the sign and the absolute value separately.
 * Instead, we split the lead byte range in the middle and encode non-negative values
 * going up and negative values going down.
 *
 * For very small absolute values, the difference is added to a middle byte value
 * for single-byte encoded differences.
 * For somewhat larger absolute values, the difference is divided by the number
 * of byte values available, the modulo is used for one trail byte, and the remainder
 * is added to a lead byte avoiding the single-byte range.
 * For large absolute values, the difference is similarly encoded in three bytes.
 *
 * This encoding does not use byte values 0, 1, 2, but uses all other byte values
 * for lead/single bytes so that the middle range of single bytes is as large
 * as possible.
 * Note that the lead byte ranges overlap some, but that the sequences as a whole
 * are well ordered. I.e., even if the lead byte is the same for sequences of different
 * lengths, the trail bytes establish correct order.
 * It would be possible to encode slightly larger ranges for each length (>1) by
 * subtracting the lower bound of the range. However, that would also slow down the
 * calculation.
 *
 * For the actual string encoding, an optimization moves the previous code point value
 * to the middle of its Unicode script block to minimize the differences in
 * same-script text runs.
 */

/* Do not use byte values 0, 1, 2 because they are separators in sort keys. */
#define SLOPE_MIN           3
#define SLOPE_MAX           0xff
#define SLOPE_MIDDLE        0x81

#define SLOPE_TAIL_COUNT    (SLOPE_MAX-SLOPE_MIN+1)

#define SLOPE_MAX_BYTES     4

/*
 * Number of lead bytes:
 * 1        middle byte for 0
 * 2*80=160 single bytes for !=0
 * 2*42=84  for double-byte values
 * 2*3=6    for 3-byte values
 * 2*1=2    for 4-byte values
 *
 * The sum must be <=SLOPE_TAIL_COUNT.
 *
 * Why these numbers?
 * - There should be >=128 single-byte values to cover 128-blocks
 *   with small scripts.
 * - There should be >=20902 single/double-byte values to cover Unihan.
 * - It helps CJK Extension B some if there are 3-byte values that cover
 *   the distance between them and Unihan.
 *   This also helps to jump among distant places in the BMP.
 * - Four-byte values are necessary to cover the rest of Unicode.
 *
 * Symmetrical lead byte counts are for convenience.
 * With an equal distribution of even and odd differences there is also
 * no advantage to asymmetrical lead byte counts.
 */
#define SLOPE_SINGLE        80
#define SLOPE_LEAD_2        42
#define SLOPE_LEAD_3        3
#define SLOPE_LEAD_4        1

/* The difference value range for single-byters. */
#define SLOPE_REACH_POS_1   SLOPE_SINGLE
#define SLOPE_REACH_NEG_1   (-SLOPE_SINGLE)

/* The difference value range for double-byters. */
#define SLOPE_REACH_POS_2   (SLOPE_LEAD_2*SLOPE_TAIL_COUNT+(SLOPE_LEAD_2-1))
#define SLOPE_REACH_NEG_2   (-SLOPE_REACH_POS_2-1)

/* The difference value range for 3-byters. */
#define SLOPE_REACH_POS_3   (SLOPE_LEAD_3*SLOPE_TAIL_COUNT*SLOPE_TAIL_COUNT+(SLOPE_LEAD_3-1)*SLOPE_TAIL_COUNT+(SLOPE_TAIL_COUNT-1))
#define SLOPE_REACH_NEG_3   (-SLOPE_REACH_POS_3-1)

/* The lead byte start values. */
#define SLOPE_START_POS_2   (SLOPE_MIDDLE+SLOPE_SINGLE+1)
#define SLOPE_START_POS_3   (SLOPE_START_POS_2+SLOPE_LEAD_2)

#define SLOPE_START_NEG_2   (SLOPE_MIDDLE+SLOPE_REACH_NEG_1)
#define SLOPE_START_NEG_3   (SLOPE_START_NEG_2-SLOPE_LEAD_2)

/*
 * Integer division and modulo with negative numerators
 * yields negative modulo results and quotients that are one more than
 * what we need here.
 */
#define NEGDIVMOD(n, d, m) { \
    (m)=(n)%(d); \
    (n)/=(d); \
    if((m)<0) { \
        --(n); \
        (m)+=(d); \
    } \
}

U_CFUNC UChar32
u_writeIdenticalLevelRun(UChar32 prev, const UChar *s, int32_t length, icu::ByteSink &sink);

#endif /* #if !UCONFIG_NO_COLLATION */

#endif
