/*
 * ASN.1 buffer writing functionality
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_ASN1_WRITE_C) || defined(MBEDTLS_X509_USE_C) || \
    defined(MBEDTLS_PSA_UTIL_HAVE_ECDSA)

#include "mbedtls/asn1write.h"
#include "mbedtls/error.h"

#include <string.h>

#include "mbedtls/platform.h"

#if defined(MBEDTLS_ASN1_PARSE_C)
#include "mbedtls/asn1.h"
#endif

int mbedtls_asn1_write_len(unsigned char **p, const unsigned char *start, size_t len)
{
#if SIZE_MAX > 0xFFFFFFFF
    if (len > 0xFFFFFFFF) {
        return MBEDTLS_ERR_ASN1_INVALID_LENGTH;
    }
#endif

    int required = 1;

    if (len >= 0x80) {
        for (size_t l = len; l != 0; l >>= 8) {
            required++;
        }
    }

    if (required > (*p - start)) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    do {
        *--(*p) = MBEDTLS_BYTE_0(len);
        len >>= 8;
    } while (len);

    if (required > 1) {
        *--(*p) = (unsigned char) (0x80 + required - 1);
    }

    return required;
}

int mbedtls_asn1_write_tag(unsigned char **p, const unsigned char *start, unsigned char tag)
{
    if (*p - start < 1) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    *--(*p) = tag;

    return 1;
}
#endif /* MBEDTLS_ASN1_WRITE_C || MBEDTLS_X509_USE_C || MBEDTLS_PSA_UTIL_HAVE_ECDSA */

#if defined(MBEDTLS_ASN1_WRITE_C)
static int mbedtls_asn1_write_len_and_tag(unsigned char **p,
                                          const unsigned char *start,
                                          size_t len,
                                          unsigned char tag)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_len(p, start, len));
    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_tag(p, start, tag));

    return (int) len;
}

int mbedtls_asn1_write_raw_buffer(unsigned char **p, const unsigned char *start,
                                  const unsigned char *buf, size_t size)
{
    size_t len = 0;

    if (*p < start || (size_t) (*p - start) < size) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    len = size;
    (*p) -= len;
    if (len != 0) {
        memcpy(*p, buf, len);
    }

    return (int) len;
}

#if defined(MBEDTLS_BIGNUM_C)
int mbedtls_asn1_write_mpi(unsigned char **p, const unsigned char *start, const mbedtls_mpi *X)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;

    // Write the MPI
    //
    len = mbedtls_mpi_size(X);

    /* DER represents 0 with a sign bit (0=nonnegative) and 7 value bits, not
     * as 0 digits. We need to end up with 020100, not with 0200. */
    if (len == 0) {
        len = 1;
    }

    if (*p < start || (size_t) (*p - start) < len) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    (*p) -= len;
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(X, *p, len));

    // DER format assumes 2s complement for numbers, so the leftmost bit
    // should be 0 for positive numbers and 1 for negative numbers.
    //
    if (X->s == 1 && **p & 0x80) {
        if (*p - start < 1) {
            return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
        }

        *--(*p) = 0x00;
        len += 1;
    }

    ret = mbedtls_asn1_write_len_and_tag(p, start, len, MBEDTLS_ASN1_INTEGER);

cleanup:
    return ret;
}
#endif /* MBEDTLS_BIGNUM_C */

int mbedtls_asn1_write_null(unsigned char **p, const unsigned char *start)
{
    // Write NULL
    //
    return mbedtls_asn1_write_len_and_tag(p, start, 0, MBEDTLS_ASN1_NULL);
}

int mbedtls_asn1_write_oid(unsigned char **p, const unsigned char *start,
                           const char *oid, size_t oid_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_raw_buffer(p, start,
                                                            (const unsigned char *) oid, oid_len));
    return mbedtls_asn1_write_len_and_tag(p, start, len, MBEDTLS_ASN1_OID);
}

int mbedtls_asn1_write_algorithm_identifier(unsigned char **p, const unsigned char *start,
                                            const char *oid, size_t oid_len,
                                            size_t par_len)
{
    return mbedtls_asn1_write_algorithm_identifier_ext(p, start, oid, oid_len, par_len, 1);
}

int mbedtls_asn1_write_algorithm_identifier_ext(unsigned char **p, const unsigned char *start,
                                                const char *oid, size_t oid_len,
                                                size_t par_len, int has_par)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;

    if (has_par) {
        if (par_len == 0) {
            MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_null(p, start));
        } else {
            len += par_len;
        }
    }

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_oid(p, start, oid, oid_len));

    return mbedtls_asn1_write_len_and_tag(p, start, len,
                                          MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
}

int mbedtls_asn1_write_bool(unsigned char **p, const unsigned char *start, int boolean)
{
    size_t len = 0;

    if (*p - start < 1) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    *--(*p) = (boolean) ? 255 : 0;
    len++;

    return mbedtls_asn1_write_len_and_tag(p, start, len, MBEDTLS_ASN1_BOOLEAN);
}

static int asn1_write_tagged_int(unsigned char **p, const unsigned char *start, int val, int tag)
{
    size_t len = 0;

    do {
        if (*p - start < 1) {
            return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
        }
        len += 1;
        *--(*p) = val & 0xff;
        val >>= 8;
    } while (val > 0);

    if (**p & 0x80) {
        if (*p - start < 1) {
            return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
        }
        *--(*p) = 0x00;
        len += 1;
    }

    return mbedtls_asn1_write_len_and_tag(p, start, len, tag);
}

int mbedtls_asn1_write_int(unsigned char **p, const unsigned char *start, int val)
{
    return asn1_write_tagged_int(p, start, val, MBEDTLS_ASN1_INTEGER);
}

int mbedtls_asn1_write_enum(unsigned char **p, const unsigned char *start, int val)
{
    return asn1_write_tagged_int(p, start, val, MBEDTLS_ASN1_ENUMERATED);
}

int mbedtls_asn1_write_tagged_string(unsigned char **p, const unsigned char *start, int tag,
                                     const char *text, size_t text_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_raw_buffer(p, start,
                                                            (const unsigned char *) text,
                                                            text_len));

    return mbedtls_asn1_write_len_and_tag(p, start, len, tag);
}

int mbedtls_asn1_write_utf8_string(unsigned char **p, const unsigned char *start,
                                   const char *text, size_t text_len)
{
    return mbedtls_asn1_write_tagged_string(p, start, MBEDTLS_ASN1_UTF8_STRING, text, text_len);
}

int mbedtls_asn1_write_printable_string(unsigned char **p, const unsigned char *start,
                                        const char *text, size_t text_len)
{
    return mbedtls_asn1_write_tagged_string(p, start, MBEDTLS_ASN1_PRINTABLE_STRING, text,
                                            text_len);
}

int mbedtls_asn1_write_ia5_string(unsigned char **p, const unsigned char *start,
                                  const char *text, size_t text_len)
{
    return mbedtls_asn1_write_tagged_string(p, start, MBEDTLS_ASN1_IA5_STRING, text, text_len);
}

int mbedtls_asn1_write_named_bitstring(unsigned char **p,
                                       const unsigned char *start,
                                       const unsigned char *buf,
                                       size_t bits)
{
    size_t unused_bits, byte_len;
    const unsigned char *cur_byte;
    unsigned char cur_byte_shifted;
    unsigned char bit;

    byte_len = (bits + 7) / 8;
    unused_bits = (byte_len * 8) - bits;

    /*
     * Named bitstrings require that trailing 0s are excluded in the encoding
     * of the bitstring. Trailing 0s are considered part of the 'unused' bits
     * when encoding this value in the first content octet
     */
    if (bits != 0) {
        cur_byte = buf + byte_len - 1;
        cur_byte_shifted = *cur_byte >> unused_bits;

        for (;;) {
            bit = cur_byte_shifted & 0x1;
            cur_byte_shifted >>= 1;

            if (bit != 0) {
                break;
            }

            bits--;
            if (bits == 0) {
                break;
            }

            if (bits % 8 == 0) {
                cur_byte_shifted = *--cur_byte;
            }
        }
    }

    return mbedtls_asn1_write_bitstring(p, start, buf, bits);
}

int mbedtls_asn1_write_bitstring(unsigned char **p, const unsigned char *start,
                                 const unsigned char *buf, size_t bits)
{
    size_t len = 0;
    size_t unused_bits, byte_len;

    byte_len = (bits + 7) / 8;
    unused_bits = (byte_len * 8) - bits;

    if (*p < start || (size_t) (*p - start) < byte_len + 1) {
        return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
    }

    len = byte_len + 1;

    /* Write the bitstring. Ensure the unused bits are zeroed */
    if (byte_len > 0) {
        byte_len--;
        *--(*p) = buf[byte_len] & ~((0x1 << unused_bits) - 1);
        (*p) -= byte_len;
        memcpy(*p, buf, byte_len);
    }

    /* Write unused bits */
    *--(*p) = (unsigned char) unused_bits;

    return mbedtls_asn1_write_len_and_tag(p, start, len, MBEDTLS_ASN1_BIT_STRING);
}

int mbedtls_asn1_write_octet_string(unsigned char **p, const unsigned char *start,
                                    const unsigned char *buf, size_t size)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;

    MBEDTLS_ASN1_CHK_ADD(len, mbedtls_asn1_write_raw_buffer(p, start, buf, size));

    return mbedtls_asn1_write_len_and_tag(p, start, len, MBEDTLS_ASN1_OCTET_STRING);
}


#if !defined(MBEDTLS_ASN1_PARSE_C)
/* This is a copy of the ASN.1 parsing function mbedtls_asn1_find_named_data(),
 * which is replicated to avoid a dependency ASN1_WRITE_C on ASN1_PARSE_C. */
static mbedtls_asn1_named_data *asn1_find_named_data(
    mbedtls_asn1_named_data *list,
    const char *oid, size_t len)
{
    while (list != NULL) {
        if (list->oid.len == len &&
            memcmp(list->oid.p, oid, len) == 0) {
            break;
        }

        list = list->next;
    }

    return list;
}
#else
#define asn1_find_named_data(list, oid, len) \
    ((mbedtls_asn1_named_data *) mbedtls_asn1_find_named_data(list, oid, len))
#endif

mbedtls_asn1_named_data *mbedtls_asn1_store_named_data(
    mbedtls_asn1_named_data **head,
    const char *oid, size_t oid_len,
    const unsigned char *val,
    size_t val_len)
{
    mbedtls_asn1_named_data *cur;

    if ((cur = asn1_find_named_data(*head, oid, oid_len)) == NULL) {
        // Add new entry if not present yet based on OID
        //
        cur = (mbedtls_asn1_named_data *) mbedtls_calloc(1,
                                                         sizeof(mbedtls_asn1_named_data));
        if (cur == NULL) {
            return NULL;
        }

        cur->oid.len = oid_len;
        cur->oid.p = mbedtls_calloc(1, oid_len);
        if (cur->oid.p == NULL) {
            mbedtls_free(cur);
            return NULL;
        }

        memcpy(cur->oid.p, oid, oid_len);

        cur->val.len = val_len;
        if (val_len != 0) {
            cur->val.p = mbedtls_calloc(1, val_len);
            if (cur->val.p == NULL) {
                mbedtls_free(cur->oid.p);
                mbedtls_free(cur);
                return NULL;
            }
        }

        cur->next = *head;
        *head = cur;
    } else if (val_len == 0) {
        mbedtls_free(cur->val.p);
        cur->val.p = NULL;
        cur->val.len = 0;
    } else if (cur->val.len != val_len) {
        /*
         * Enlarge existing value buffer if needed
         * Preserve old data until the allocation succeeded, to leave list in
         * a consistent state in case allocation fails.
         */
        void *p = mbedtls_calloc(1, val_len);
        if (p == NULL) {
            return NULL;
        }

        mbedtls_free(cur->val.p);
        cur->val.p = p;
        cur->val.len = val_len;
    }

    if (val != NULL && val_len != 0) {
        memcpy(cur->val.p, val, val_len);
    }

    return cur;
}
#endif /* MBEDTLS_ASN1_WRITE_C */
