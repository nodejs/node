/*
 *  RFC 1521 base64 encoding/decoding
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include <limits.h>

#include "common.h"

#if defined(MBEDTLS_BASE64_C)

#include "mbedtls/base64.h"
#include "base64_internal.h"
#include "constant_time_internal.h"
#include "mbedtls/error.h"

#include <stdint.h>

#if defined(MBEDTLS_SELF_TEST)
#include <string.h>
#include "mbedtls/platform.h"
#endif /* MBEDTLS_SELF_TEST */

MBEDTLS_STATIC_TESTABLE
unsigned char mbedtls_ct_base64_enc_char(unsigned char value)
{
    unsigned char digit = 0;
    /* For each range of values, if value is in that range, mask digit with
     * the corresponding value. Since value can only be in a single range,
     * only at most one masking will change digit. */
    digit |= mbedtls_ct_uchar_in_range_if(0, 25, value, 'A' + value);
    digit |= mbedtls_ct_uchar_in_range_if(26, 51, value, 'a' + value - 26);
    digit |= mbedtls_ct_uchar_in_range_if(52, 61, value, '0' + value - 52);
    digit |= mbedtls_ct_uchar_in_range_if(62, 62, value, '+');
    digit |= mbedtls_ct_uchar_in_range_if(63, 63, value, '/');
    return digit;
}

MBEDTLS_STATIC_TESTABLE
signed char mbedtls_ct_base64_dec_value(unsigned char c)
{
    unsigned char val = 0;
    /* For each range of digits, if c is in that range, mask val with
     * the corresponding value. Since c can only be in a single range,
     * only at most one masking will change val. Set val to one plus
     * the desired value so that it stays 0 if c is in none of the ranges. */
    val |= mbedtls_ct_uchar_in_range_if('A', 'Z', c, c - 'A' +  0 + 1);
    val |= mbedtls_ct_uchar_in_range_if('a', 'z', c, c - 'a' + 26 + 1);
    val |= mbedtls_ct_uchar_in_range_if('0', '9', c, c - '0' + 52 + 1);
    val |= mbedtls_ct_uchar_in_range_if('+', '+', c, c - '+' + 62 + 1);
    val |= mbedtls_ct_uchar_in_range_if('/', '/', c, c - '/' + 63 + 1);
    /* At this point, val is 0 if c is an invalid digit and v+1 if c is
     * a digit with the value v. */
    return val - 1;
}

/*
 * Encode a buffer into base64 format
 */
int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen)
{
    size_t i, n;
    int C1, C2, C3;
    unsigned char *p;

    if (slen == 0) {
        *olen = 0;
        return 0;
    }

    n = slen / 3 + (slen % 3 != 0);

    if (n > (SIZE_MAX - 1) / 4) {
        *olen = SIZE_MAX;
        return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
    }

    n *= 4;

    if ((dlen < n + 1) || (NULL == dst)) {
        *olen = n + 1;
        return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
    }

    n = (slen / 3) * 3;

    for (i = 0, p = dst; i < n; i += 3) {
        C1 = *src++;
        C2 = *src++;
        C3 = *src++;

        *p++ = mbedtls_ct_base64_enc_char((C1 >> 2) & 0x3F);
        *p++ = mbedtls_ct_base64_enc_char((((C1 &  3) << 4) + (C2 >> 4))
                                          & 0x3F);
        *p++ = mbedtls_ct_base64_enc_char((((C2 & 15) << 2) + (C3 >> 6))
                                          & 0x3F);
        *p++ = mbedtls_ct_base64_enc_char(C3 & 0x3F);
    }

    if (i < slen) {
        C1 = *src++;
        C2 = ((i + 1) < slen) ? *src++ : 0;

        *p++ = mbedtls_ct_base64_enc_char((C1 >> 2) & 0x3F);
        *p++ = mbedtls_ct_base64_enc_char((((C1 & 3) << 4) + (C2 >> 4))
                                          & 0x3F);

        if ((i + 1) < slen) {
            *p++ = mbedtls_ct_base64_enc_char(((C2 & 15) << 2) & 0x3F);
        } else {
            *p++ = '=';
        }

        *p++ = '=';
    }

    *olen = (size_t) (p - dst);
    *p = 0;

    return 0;
}

/*
 * Decode a base64-formatted buffer
 */
int mbedtls_base64_decode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen)
{
    size_t i; /* index in source */
    size_t n; /* number of digits or trailing = in source */
    uint32_t x; /* value accumulator */
    unsigned accumulated_digits = 0;
    unsigned equals = 0;
    int spaces_present = 0;
    unsigned char *p;

    /* First pass: check for validity and get output length */
    for (i = n = 0; i < slen; i++) {
        /* Skip spaces before checking for EOL */
        spaces_present = 0;
        while (i < slen && src[i] == ' ') {
            ++i;
            spaces_present = 1;
        }

        /* Spaces at end of buffer are OK */
        if (i == slen) {
            break;
        }

        if ((slen - i) >= 2 &&
            src[i] == '\r' && src[i + 1] == '\n') {
            continue;
        }

        if (src[i] == '\n') {
            continue;
        }

        /* Space inside a line is an error */
        if (spaces_present) {
            return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
        }

        if (src[i] > 127) {
            return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
        }

        if (src[i] == '=') {
            if (++equals > 2) {
                return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
            }
        } else {
            if (equals != 0) {
                return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
            }
            if (mbedtls_ct_base64_dec_value(src[i]) < 0) {
                return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
            }
        }
        n++;
    }

    /* In valid base64, the number of digits (n-equals) is always of the form
     * 4*k, 4*k+2 or *4k+3. Also, the number n of digits plus the number of
     * equal signs at the end is always a multiple of 4. */
    if ((n - equals) % 4 == 1) {
        return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
    }
    if (n % 4 != 0) {
        return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
    }

    /* We've determined that the input is valid, and that it contains
     * exactly k blocks of digits-or-equals, with n = 4 * k,
     * and equals only present at the end of the last block if at all.
     * Now we can calculate the length of the output.
     *
     * Each block of 4 digits in the input map to 3 bytes of output.
     * For the last block:
     * - abcd (where abcd are digits) is a full 3-byte block;
     * - abc= means 1 byte less than a full 3-byte block of output;
     * - ab== means 2 bytes less than a full 3-byte block of output;
     * - a==== and ==== is rejected above.
     */
    *olen = (n / 4) * 3 - equals;

    /* If the output buffer is too small, signal this and stop here.
     * Also, as documented, stop here if `dst` is null, independently of
     * `dlen`.
     *
     * There is an edge case when the output is empty: in this case,
     * `dlen == 0` with `dst == NULL` is valid (on some platforms,
     * `malloc(0)` returns `NULL`). Since the call is valid, we return
     * 0 in this case.
     */
    if ((*olen != 0 && dst == NULL) || dlen < *olen) {
        return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
    }

    for (x = 0, p = dst; i > 0; i--, src++) {
        if (*src == '\r' || *src == '\n' || *src == ' ') {
            continue;
        }
        if (*src == '=') {
            /* We already know from the first loop that equal signs are
             * only at the end. */
            break;
        }
        x = x << 6;
        x |= mbedtls_ct_base64_dec_value(*src);

        if (++accumulated_digits == 4) {
            accumulated_digits = 0;
            *p++ = MBEDTLS_BYTE_2(x);
            *p++ = MBEDTLS_BYTE_1(x);
            *p++ = MBEDTLS_BYTE_0(x);
        }
    }
    if (accumulated_digits == 3) {
        *p++ = MBEDTLS_BYTE_2(x << 6);
        *p++ = MBEDTLS_BYTE_1(x << 6);
    } else if (accumulated_digits == 2) {
        *p++ = MBEDTLS_BYTE_2(x << 12);
    }

    if (*olen != (size_t) (p - dst)) {
        return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    }

    return 0;
}

#if defined(MBEDTLS_SELF_TEST)

static const unsigned char base64_test_dec[64] =
{
    0x24, 0x48, 0x6E, 0x56, 0x87, 0x62, 0x5A, 0xBD,
    0xBF, 0x17, 0xD9, 0xA2, 0xC4, 0x17, 0x1A, 0x01,
    0x94, 0xED, 0x8F, 0x1E, 0x11, 0xB3, 0xD7, 0x09,
    0x0C, 0xB6, 0xE9, 0x10, 0x6F, 0x22, 0xEE, 0x13,
    0xCA, 0xB3, 0x07, 0x05, 0x76, 0xC9, 0xFA, 0x31,
    0x6C, 0x08, 0x34, 0xFF, 0x8D, 0xC2, 0x6C, 0x38,
    0x00, 0x43, 0xE9, 0x54, 0x97, 0xAF, 0x50, 0x4B,
    0xD1, 0x41, 0xBA, 0x95, 0x31, 0x5A, 0x0B, 0x97
};

static const unsigned char base64_test_enc[] =
    "JEhuVodiWr2/F9mixBcaAZTtjx4Rs9cJDLbpEG8i7hPK"
    "swcFdsn6MWwINP+Nwmw4AEPpVJevUEvRQbqVMVoLlw==";

/*
 * Checkup routine
 */
int mbedtls_base64_self_test(int verbose)
{
    size_t len;
    const unsigned char *src;
    unsigned char buffer[128];

    if (verbose != 0) {
        mbedtls_printf("  Base64 encoding test: ");
    }

    src = base64_test_dec;

    if (mbedtls_base64_encode(buffer, sizeof(buffer), &len, src, 64) != 0 ||
        memcmp(base64_test_enc, buffer, 88) != 0) {
        if (verbose != 0) {
            mbedtls_printf("failed\n");
        }

        return 1;
    }

    if (verbose != 0) {
        mbedtls_printf("passed\n  Base64 decoding test: ");
    }

    src = base64_test_enc;

    if (mbedtls_base64_decode(buffer, sizeof(buffer), &len, src, 88) != 0 ||
        memcmp(base64_test_dec, buffer, 64) != 0) {
        if (verbose != 0) {
            mbedtls_printf("failed\n");
        }

        return 1;
    }

    if (verbose != 0) {
        mbedtls_printf("passed\n\n");
    }

    return 0;
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_BASE64_C */
