/*
 *  Debugging routines
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "ssl_misc.h"

#if defined(MBEDTLS_DEBUG_C)

#include "mbedtls/platform.h"

#include "debug_internal.h"
#include "mbedtls/error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* DEBUG_BUF_SIZE must be at least 2 */
#define DEBUG_BUF_SIZE      512

static int debug_threshold = 0;

void mbedtls_debug_set_threshold(int threshold)
{
    debug_threshold = threshold;
}

/*
 * All calls to f_dbg must be made via this function
 */
static inline void debug_send_line(const mbedtls_ssl_context *ssl, int level,
                                   const char *file, int line,
                                   const char *str)
{
    /*
     * If in a threaded environment, we need a thread identifier.
     * Since there is no portable way to get one, use the address of the ssl
     * context instead, as it shouldn't be shared between threads.
     */
#if defined(MBEDTLS_THREADING_C)
    char idstr[20 + DEBUG_BUF_SIZE]; /* 0x + 16 nibbles + ': ' */
    mbedtls_snprintf(idstr, sizeof(idstr), "%p: %s", (void *) ssl, str);
    ssl->conf->f_dbg(ssl->conf->p_dbg, level, file, line, idstr);
#else
    ssl->conf->f_dbg(ssl->conf->p_dbg, level, file, line, str);
#endif
}

MBEDTLS_PRINTF_ATTRIBUTE(5, 6)
void mbedtls_debug_print_msg(const mbedtls_ssl_context *ssl, int level,
                             const char *file, int line,
                             const char *format, ...)
{
    va_list argp;
    char str[DEBUG_BUF_SIZE];
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_STATIC_ASSERT(DEBUG_BUF_SIZE >= 2, "DEBUG_BUF_SIZE too small");

    if (NULL == ssl              ||
        NULL == ssl->conf        ||
        NULL == ssl->conf->f_dbg ||
        level > debug_threshold) {
        return;
    }

    va_start(argp, format);
    ret = mbedtls_vsnprintf(str, DEBUG_BUF_SIZE, format, argp);
    va_end(argp);

    if (ret < 0) {
        ret = 0;
    } else {
        if (ret >= DEBUG_BUF_SIZE - 1) {
            ret = DEBUG_BUF_SIZE - 2;
        }
    }
    str[ret]     = '\n';
    str[ret + 1] = '\0';

    debug_send_line(ssl, level, file, line, str);
}

void mbedtls_debug_print_ret(const mbedtls_ssl_context *ssl, int level,
                             const char *file, int line,
                             const char *text, int ret)
{
    char str[DEBUG_BUF_SIZE];

    if (NULL == ssl              ||
        NULL == ssl->conf        ||
        NULL == ssl->conf->f_dbg ||
        level > debug_threshold) {
        return;
    }

    /*
     * With non-blocking I/O and examples that just retry immediately,
     * the logs would be quickly flooded with WANT_READ, so ignore that.
     * Don't ignore WANT_WRITE however, since it is usually rare.
     */
    if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
        return;
    }

    mbedtls_snprintf(str, sizeof(str), "%s() returned %d (-0x%04x)\n",
                     text, ret, (unsigned int) -ret);

    debug_send_line(ssl, level, file, line, str);
}

void mbedtls_debug_print_buf(const mbedtls_ssl_context *ssl, int level,
                             const char *file, int line, const char *text,
                             const unsigned char *buf, size_t len)
{
    char str[DEBUG_BUF_SIZE];
    char txt[17];
    size_t i, idx = 0;

    if (NULL == ssl              ||
        NULL == ssl->conf        ||
        NULL == ssl->conf->f_dbg ||
        level > debug_threshold) {
        return;
    }

    mbedtls_snprintf(str + idx, sizeof(str) - idx, "dumping '%s' (%u bytes)\n",
                     text, (unsigned int) len);

    debug_send_line(ssl, level, file, line, str);

    memset(txt, 0, sizeof(txt));
    for (i = 0; i < len; i++) {
        if (i >= 4096) {
            break;
        }

        if (i % 16 == 0) {
            if (i > 0) {
                mbedtls_snprintf(str + idx, sizeof(str) - idx, "  %s\n", txt);
                debug_send_line(ssl, level, file, line, str);

                idx = 0;
                memset(txt, 0, sizeof(txt));
            }

            idx += mbedtls_snprintf(str + idx, sizeof(str) - idx, "%04x: ",
                                    (unsigned int) i);

        }

        idx += mbedtls_snprintf(str + idx, sizeof(str) - idx, " %02x",
                                (unsigned int) buf[i]);
        txt[i % 16] = (buf[i] > 31 && buf[i] < 127) ? buf[i] : '.';
    }

    if (len > 0) {
        for (/* i = i */; i % 16 != 0; i++) {
            idx += mbedtls_snprintf(str + idx, sizeof(str) - idx, "   ");
        }

        mbedtls_snprintf(str + idx, sizeof(str) - idx, "  %s\n", txt);
        debug_send_line(ssl, level, file, line, str);
    }
}

#if defined(MBEDTLS_BIGNUM_C)
void mbedtls_debug_print_mpi(const mbedtls_ssl_context *ssl, int level,
                             const char *file, int line,
                             const char *text, const mbedtls_mpi *X)
{
    char str[DEBUG_BUF_SIZE];
    size_t bitlen;
    size_t idx = 0;

    if (NULL == ssl              ||
        NULL == ssl->conf        ||
        NULL == ssl->conf->f_dbg ||
        NULL == X                ||
        level > debug_threshold) {
        return;
    }

    bitlen = mbedtls_mpi_bitlen(X);

    mbedtls_snprintf(str, sizeof(str), "value of '%s' (%u bits) is:\n",
                     text, (unsigned) bitlen);
    debug_send_line(ssl, level, file, line, str);

    if (bitlen == 0) {
        str[0] = ' '; str[1] = '0'; str[2] = '0';
        idx = 3;
    } else {
        int n;
        for (n = (int) ((bitlen - 1) / 8); n >= 0; n--) {
            size_t limb_offset = n / sizeof(mbedtls_mpi_uint);
            size_t offset_in_limb = n % sizeof(mbedtls_mpi_uint);
            unsigned char octet =
                (X->p[limb_offset] >> (offset_in_limb * 8)) & 0xff;
            mbedtls_snprintf(str + idx, sizeof(str) - idx, " %02x", octet);
            idx += 3;
            /* Wrap lines after 16 octets that each take 3 columns */
            if (idx >= 3 * 16) {
                mbedtls_snprintf(str + idx, sizeof(str) - idx, "\n");
                debug_send_line(ssl, level, file, line, str);
                idx = 0;
            }
        }
    }

    if (idx != 0) {
        mbedtls_snprintf(str + idx, sizeof(str) - idx, "\n");
        debug_send_line(ssl, level, file, line, str);
    }
}
#endif /* MBEDTLS_BIGNUM_C */

#if defined(MBEDTLS_X509_CRT_PARSE_C) && !defined(MBEDTLS_X509_REMOVE_INFO)

/* no-check-names will be removed in mbedtls#10229. */
#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY) || defined(MBEDTLS_PK_USE_PSA_RSA_DATA) //no-check-names
static void mbedtls_debug_print_integer(const mbedtls_ssl_context *ssl, int level,
                                        const char *file, int line, const char *text,
                                        const unsigned char *buf, size_t bitlen)
{
    char str[DEBUG_BUF_SIZE];
    size_t i, len_bytes = PSA_BITS_TO_BYTES(bitlen), idx = 0;

    mbedtls_snprintf(str + idx, sizeof(str) - idx, "value of '%s' (%u bits) is:\n",
                     text, (unsigned int) bitlen);

    debug_send_line(ssl, level, file, line, str);

    for (i = 0; i < len_bytes; i++) {
        if (i >= 4096) {
            break;
        }

        if (i % 16 == 0) {
            if (i > 0) {
                mbedtls_snprintf(str + idx, sizeof(str) - idx, "\n");
                debug_send_line(ssl, level, file, line, str);

                idx = 0;
            }
        }

        idx += mbedtls_snprintf(str + idx, sizeof(str) - idx, " %02x",
                                (unsigned int) buf[i]);
    }

    if (len_bytes > 0) {
        mbedtls_snprintf(str + idx, sizeof(str) - idx, "\n");
        debug_send_line(ssl, level, file, line, str);
    }
}
/* no-check-names will be removed in mbedtls#10229. */
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY || MBEDTLS_PK_USE_PSA_RSA_DATA */ //no-check-names

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
static void mbedtls_debug_print_psa_ec(const mbedtls_ssl_context *ssl, int level,
                                       const char *file, int line,
                                       const char *text, const mbedtls_pk_context *pk)
{
    char str[DEBUG_BUF_SIZE];
    const uint8_t *coord_start;
    size_t coord_len;

    if (NULL == ssl              ||
        NULL == ssl->conf        ||
        NULL == ssl->conf->f_dbg ||
        level > debug_threshold) {
        return;
    }

    /* For the description of pk->pk_raw content please refer to the description
     * psa_export_public_key() function. */
    coord_len = (pk->pub_raw_len - 1)/2;

    /* X coordinate */
    coord_start = pk->pub_raw + 1;
    mbedtls_snprintf(str, sizeof(str), "%s(X)", text);
    mbedtls_debug_print_integer(ssl, level, file, line, str, coord_start, coord_len * 8);

    /* Y coordinate */
    coord_start = coord_start + coord_len;
    mbedtls_snprintf(str, sizeof(str), "%s(Y)", text);
    mbedtls_debug_print_integer(ssl, level, file, line, str, coord_start, coord_len * 8);
}
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */

/* no-check-names will be removed in mbedtls#10229. */
#if defined(MBEDTLS_PK_USE_PSA_RSA_DATA) //no-check-names
static size_t debug_count_valid_bits(unsigned char **buf, size_t len)
{
    size_t i, bits;

    /* Ignore initial null bytes (if any). */
    while ((len > 0) && (**buf == 0x00)) {
        (*buf)++;
        len--;
    }

    if (len == 0) {
        return 0;
    }

    bits = len * 8;

    /* Ignore initial null bits (if any). */
    for (i = 7; i > 0; i--) {
        if ((**buf & (0x1 << i)) != 0) {
            break;
        }
        bits--;
    }

    return bits;
}

static void mbedtls_debug_print_psa_rsa(const mbedtls_ssl_context *ssl, int level,
                                        const char *file, int line,
                                        const char *text, const mbedtls_pk_context *pk)
{
    char str[DEBUG_BUF_SIZE];
    /* no-check-names will be removed in mbedtls#10229. */
    unsigned char key_der[MBEDTLS_PK_MAX_RSA_PUBKEY_RAW_LEN]; //no-check-names
    unsigned char *start_cur;
    unsigned char *end_cur;
    size_t len, bits;
    int ret;

    if (NULL == ssl              ||
        NULL == ssl->conf        ||
        NULL == ssl->conf->f_dbg ||
        level > debug_threshold) {
        return;
    }

    if (pk->pub_raw_len > sizeof(key_der)) {
        snprintf(str, sizeof(str),
                 "RSA public key too large: %" MBEDTLS_PRINTF_SIZET " > %" MBEDTLS_PRINTF_SIZET,
                 pk->pub_raw_len, sizeof(key_der));
        debug_send_line(ssl, level, file, line, str);
        return;
    }

    memcpy(key_der, pk->pub_raw, pk->pub_raw_len);
    start_cur = key_der;
    end_cur = key_der + pk->pub_raw_len;

    /* This integer parsing solution should be replaced with mbedtls_asn1_get_integer().
     * See #10238. */
    ret = mbedtls_asn1_get_tag(&start_cur, end_cur, &len,
                               MBEDTLS_ASN1_SEQUENCE | MBEDTLS_ASN1_CONSTRUCTED);
    if (ret != 0) {
        return;
    }

    ret = mbedtls_asn1_get_tag(&start_cur, end_cur, &len, MBEDTLS_ASN1_INTEGER);
    if (ret != 0) {
        return;
    }

    bits = debug_count_valid_bits(&start_cur, len);
    if (bits == 0) {
        return;
    }
    len = PSA_BITS_TO_BYTES(bits);

    mbedtls_snprintf(str, sizeof(str), "%s.N", text);
    mbedtls_debug_print_integer(ssl, level, file, line, str, start_cur, bits);

    start_cur += len;

    ret = mbedtls_asn1_get_tag(&start_cur, end_cur, &len, MBEDTLS_ASN1_INTEGER);
    if (ret != 0) {
        return;
    }

    bits = debug_count_valid_bits(&start_cur, len);
    if (bits == 0) {
        return;
    }

    mbedtls_snprintf(str, sizeof(str), "%s.E", text);
    mbedtls_debug_print_integer(ssl, level, file, line, str, start_cur, bits);
}
/* no-check-names will be removed in mbedtls#10229. */
#endif /* MBEDTLS_PK_USE_PSA_RSA_DATA */ //no-check-names

static void debug_print_pk(const mbedtls_ssl_context *ssl, int level,
                           const char *file, int line,
                           const char *text, const mbedtls_pk_context *pk)
{
    size_t i;
    mbedtls_pk_debug_item items[MBEDTLS_PK_DEBUG_MAX_ITEMS];
    char name[16];

    memset(items, 0, sizeof(items));

    if (mbedtls_pk_debug(pk, items) != 0) {
        debug_send_line(ssl, level, file, line,
                        "invalid PK context\n");
        return;
    }

    for (i = 0; i < MBEDTLS_PK_DEBUG_MAX_ITEMS; i++) {
        if (items[i].type == MBEDTLS_PK_DEBUG_NONE) {
            return;
        }

        mbedtls_snprintf(name, sizeof(name), "%s%s", text, items[i].name);
        name[sizeof(name) - 1] = '\0';

#if defined(MBEDTLS_RSA_C)
        if (items[i].type == MBEDTLS_PK_DEBUG_MPI) {
            mbedtls_debug_print_mpi(ssl, level, file, line, name, items[i].value);
        } else
#endif /* MBEDTLS_RSA_C */
/* no-check-names will be removed in mbedtls#10229. */
#if defined(MBEDTLS_PK_USE_PSA_RSA_DATA) //no-check-names
        if (items[i].type == MBEDTLS_PK_DEBUG_PSA_RSA) { //no-check-names
            mbedtls_debug_print_psa_rsa(ssl, level, file, line, name, items[i].value);
        } else
#endif /* MBEDTLS_PK_USE_PSA_RSA_DATA */ //no-check-names
#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
        if (items[i].type == MBEDTLS_PK_DEBUG_PSA_EC) {
            mbedtls_debug_print_psa_ec(ssl, level, file, line, name, items[i].value);
        } else
#endif /* PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY */
        { debug_send_line(ssl, level, file, line,
                          "should not happen\n"); }
    }
}

static void debug_print_line_by_line(const mbedtls_ssl_context *ssl, int level,
                                     const char *file, int line, const char *text)
{
    char str[DEBUG_BUF_SIZE];
    const char *start, *cur;

    start = text;
    for (cur = text; *cur != '\0'; cur++) {
        if (*cur == '\n') {
            size_t len = (size_t) (cur - start) + 1;
            if (len > DEBUG_BUF_SIZE - 1) {
                len = DEBUG_BUF_SIZE - 1;
            }

            memcpy(str, start, len);
            str[len] = '\0';

            debug_send_line(ssl, level, file, line, str);

            start = cur + 1;
        }
    }
}

void mbedtls_debug_print_crt(const mbedtls_ssl_context *ssl, int level,
                             const char *file, int line,
                             const char *text, const mbedtls_x509_crt *crt)
{
    char str[DEBUG_BUF_SIZE];
    int i = 0;

    if (NULL == ssl              ||
        NULL == ssl->conf        ||
        NULL == ssl->conf->f_dbg ||
        NULL == crt              ||
        level > debug_threshold) {
        return;
    }

    while (crt != NULL) {
        char buf[1024];

        mbedtls_snprintf(str, sizeof(str), "%s #%d:\n", text, ++i);
        debug_send_line(ssl, level, file, line, str);

        mbedtls_x509_crt_info(buf, sizeof(buf) - 1, "", crt);
        debug_print_line_by_line(ssl, level, file, line, buf);

        debug_print_pk(ssl, level, file, line, "crt->", &crt->pk);

        crt = crt->next;
    }
}
#endif /* MBEDTLS_X509_CRT_PARSE_C && MBEDTLS_X509_REMOVE_INFO */

#endif /* MBEDTLS_DEBUG_C */
