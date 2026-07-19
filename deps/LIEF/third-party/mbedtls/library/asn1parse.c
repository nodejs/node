/*
 *  Generic ASN.1 parsing
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_ASN1_PARSE_C) || defined(MBEDTLS_X509_CREATE_C) || \
    defined(MBEDTLS_PSA_UTIL_HAVE_ECDSA)

#include "mbedtls/asn1.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#include <string.h>

#if defined(MBEDTLS_BIGNUM_C)
#include "mbedtls/bignum.h"
#endif

#include "mbedtls/platform.h"

/*
 * ASN.1 DER decoding routines
 */
int mbedtls_asn1_get_len(unsigned char **p,
                         const unsigned char *end,
                         size_t *len)
{
    if ((end - *p) < 1) {
        return MBEDTLS_ERR_ASN1_OUT_OF_DATA;
    }

    if ((**p & 0x80) == 0) {
        *len = *(*p)++;
    } else {
        int n = (**p) & 0x7F;
        if (n == 0 || n > 4) {
            return MBEDTLS_ERR_ASN1_INVALID_LENGTH;
        }
        if ((end - *p) <= n) {
            return MBEDTLS_ERR_ASN1_OUT_OF_DATA;
        }
        *len = 0;
        (*p)++;
        while (n--) {
            *len = (*len << 8) | **p;
            (*p)++;
        }
    }

    if (*len > (size_t) (end - *p)) {
        return MBEDTLS_ERR_ASN1_OUT_OF_DATA;
    }

    return 0;
}

int mbedtls_asn1_get_tag(unsigned char **p,
                         const unsigned char *end,
                         size_t *len, int tag)
{
    if ((end - *p) < 1) {
        return MBEDTLS_ERR_ASN1_OUT_OF_DATA;
    }

    if (**p != tag) {
        return MBEDTLS_ERR_ASN1_UNEXPECTED_TAG;
    }

    (*p)++;

    return mbedtls_asn1_get_len(p, end, len);
}
#endif /* MBEDTLS_ASN1_PARSE_C || MBEDTLS_X509_CREATE_C || MBEDTLS_PSA_UTIL_HAVE_ECDSA */

#if defined(MBEDTLS_ASN1_PARSE_C)
int mbedtls_asn1_get_bool(unsigned char **p,
                          const unsigned char *end,
                          int *val)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;

    if ((ret = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_BOOLEAN)) != 0) {
        return ret;
    }

    if (len != 1) {
        return MBEDTLS_ERR_ASN1_INVALID_LENGTH;
    }

    *val = (**p != 0) ? 1 : 0;
    (*p)++;

    return 0;
}

static int asn1_get_tagged_int(unsigned char **p,
                               const unsigned char *end,
                               int tag, int *val)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;

    if ((ret = mbedtls_asn1_get_tag(p, end, &len, tag)) != 0) {
        return ret;
    }

    /*
     * len==0 is malformed (0 must be represented as 020100 for INTEGER,
     * or 0A0100 for ENUMERATED tags
     */
    if (len == 0) {
        return MBEDTLS_ERR_ASN1_INVALID_LENGTH;
    }
    /* This is a cryptography library. Reject negative integers. */
    if ((**p & 0x80) != 0) {
        return MBEDTLS_ERR_ASN1_INVALID_LENGTH;
    }

    /* Skip leading zeros. */
    while (len > 0 && **p == 0) {
        ++(*p);
        --len;
    }

    /* Reject integers that don't fit in an int. This code assumes that
     * the int type has no padding bit. */
    if (len > sizeof(int)) {
        return MBEDTLS_ERR_ASN1_INVALID_LENGTH;
    }
    if (len == sizeof(int) && (**p & 0x80) != 0) {
        return MBEDTLS_ERR_ASN1_INVALID_LENGTH;
    }

    *val = 0;
    while (len-- > 0) {
        *val = (*val << 8) | **p;
        (*p)++;
    }

    return 0;
}

int mbedtls_asn1_get_int(unsigned char **p,
                         const unsigned char *end,
                         int *val)
{
    return asn1_get_tagged_int(p, end, MBEDTLS_ASN1_INTEGER, val);
}

int mbedtls_asn1_get_enum(unsigned char **p,
                          const unsigned char *end,
                          int *val)
{
    return asn1_get_tagged_int(p, end, MBEDTLS_ASN1_ENUMERATED, val);
}

#if defined(MBEDTLS_BIGNUM_C)
int mbedtls_asn1_get_mpi(unsigned char **p,
                         const unsigned char *end,
                         mbedtls_mpi *X)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;

    if ((ret = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_INTEGER)) != 0) {
        return ret;
    }

    ret = mbedtls_mpi_read_binary(X, *p, len);

    *p += len;

    return ret;
}
#endif /* MBEDTLS_BIGNUM_C */

int mbedtls_asn1_get_bitstring(unsigned char **p, const unsigned char *end,
                               mbedtls_asn1_bitstring *bs)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    /* Certificate type is a single byte bitstring */
    if ((ret = mbedtls_asn1_get_tag(p, end, &bs->len, MBEDTLS_ASN1_BIT_STRING)) != 0) {
        return ret;
    }

    /* Check length, subtract one for actual bit string length */
    if (bs->len < 1) {
        return MBEDTLS_ERR_ASN1_OUT_OF_DATA;
    }
    bs->len -= 1;

    /* Get number of unused bits, ensure unused bits <= 7 */
    bs->unused_bits = **p;
    if (bs->unused_bits > 7) {
        return MBEDTLS_ERR_ASN1_INVALID_LENGTH;
    }
    (*p)++;

    /* Get actual bitstring */
    bs->p = *p;
    *p += bs->len;

    if (*p != end) {
        return MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;
    }

    return 0;
}

/*
 * Traverse an ASN.1 "SEQUENCE OF <tag>"
 * and call a callback for each entry found.
 */
int mbedtls_asn1_traverse_sequence_of(
    unsigned char **p,
    const unsigned char *end,
    unsigned char tag_must_mask, unsigned char tag_must_val,
    unsigned char tag_may_mask, unsigned char tag_may_val,
    int (*cb)(void *ctx, int tag,
              unsigned char *start, size_t len),
    void *ctx)
{
    int ret;
    size_t len;

    /* Get main sequence tag */
    if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return ret;
    }

    if (*p + len != end) {
        return MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;
    }

    while (*p < end) {
        unsigned char const tag = *(*p)++;

        if ((tag & tag_must_mask) != tag_must_val) {
            return MBEDTLS_ERR_ASN1_UNEXPECTED_TAG;
        }

        if ((ret = mbedtls_asn1_get_len(p, end, &len)) != 0) {
            return ret;
        }

        if ((tag & tag_may_mask) == tag_may_val) {
            if (cb != NULL) {
                ret = cb(ctx, tag, *p, len);
                if (ret != 0) {
                    return ret;
                }
            }
        }

        *p += len;
    }

    return 0;
}

/*
 * Get a bit string without unused bits
 */
int mbedtls_asn1_get_bitstring_null(unsigned char **p, const unsigned char *end,
                                    size_t *len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if ((ret = mbedtls_asn1_get_tag(p, end, len, MBEDTLS_ASN1_BIT_STRING)) != 0) {
        return ret;
    }

    if (*len == 0) {
        return MBEDTLS_ERR_ASN1_INVALID_DATA;
    }
    --(*len);

    if (**p != 0) {
        return MBEDTLS_ERR_ASN1_INVALID_DATA;
    }
    ++(*p);

    return 0;
}

void mbedtls_asn1_sequence_free(mbedtls_asn1_sequence *seq)
{
    while (seq != NULL) {
        mbedtls_asn1_sequence *next = seq->next;
        mbedtls_free(seq);
        seq = next;
    }
}

typedef struct {
    int tag;
    mbedtls_asn1_sequence *cur;
} asn1_get_sequence_of_cb_ctx_t;

static int asn1_get_sequence_of_cb(void *ctx,
                                   int tag,
                                   unsigned char *start,
                                   size_t len)
{
    asn1_get_sequence_of_cb_ctx_t *cb_ctx =
        (asn1_get_sequence_of_cb_ctx_t *) ctx;
    mbedtls_asn1_sequence *cur =
        cb_ctx->cur;

    if (cur->buf.p != NULL) {
        cur->next =
            mbedtls_calloc(1, sizeof(mbedtls_asn1_sequence));

        if (cur->next == NULL) {
            return MBEDTLS_ERR_ASN1_ALLOC_FAILED;
        }

        cur = cur->next;
    }

    cur->buf.p = start;
    cur->buf.len = len;
    cur->buf.tag = tag;

    cb_ctx->cur = cur;
    return 0;
}

/*
 *  Parses and splits an ASN.1 "SEQUENCE OF <tag>"
 */
int mbedtls_asn1_get_sequence_of(unsigned char **p,
                                 const unsigned char *end,
                                 mbedtls_asn1_sequence *cur,
                                 int tag)
{
    asn1_get_sequence_of_cb_ctx_t cb_ctx = { tag, cur };
    memset(cur, 0, sizeof(mbedtls_asn1_sequence));
    return mbedtls_asn1_traverse_sequence_of(
        p, end, 0xFF, tag, 0, 0,
        asn1_get_sequence_of_cb, &cb_ctx);
}

int mbedtls_asn1_get_alg(unsigned char **p,
                         const unsigned char *end,
                         mbedtls_asn1_buf *alg, mbedtls_asn1_buf *params)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;

    if ((ret = mbedtls_asn1_get_tag(p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return ret;
    }

    if ((end - *p) < 1) {
        return MBEDTLS_ERR_ASN1_OUT_OF_DATA;
    }

    alg->tag = **p;
    end = *p + len;

    if ((ret = mbedtls_asn1_get_tag(p, end, &alg->len, MBEDTLS_ASN1_OID)) != 0) {
        return ret;
    }

    alg->p = *p;
    *p += alg->len;

    if (*p == end) {
        mbedtls_platform_zeroize(params, sizeof(mbedtls_asn1_buf));
        return 0;
    }

    params->tag = **p;
    (*p)++;

    if ((ret = mbedtls_asn1_get_len(p, end, &params->len)) != 0) {
        return ret;
    }

    params->p = *p;
    *p += params->len;

    if (*p != end) {
        return MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;
    }

    return 0;
}

int mbedtls_asn1_get_alg_null(unsigned char **p,
                              const unsigned char *end,
                              mbedtls_asn1_buf *alg)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_asn1_buf params;

    memset(&params, 0, sizeof(mbedtls_asn1_buf));

    if ((ret = mbedtls_asn1_get_alg(p, end, alg, &params)) != 0) {
        return ret;
    }

    if ((params.tag != MBEDTLS_ASN1_NULL && params.tag != 0) || params.len != 0) {
        return MBEDTLS_ERR_ASN1_INVALID_DATA;
    }

    return 0;
}

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
void mbedtls_asn1_free_named_data(mbedtls_asn1_named_data *cur)
{
    if (cur == NULL) {
        return;
    }

    mbedtls_free(cur->oid.p);
    mbedtls_free(cur->val.p);

    mbedtls_platform_zeroize(cur, sizeof(mbedtls_asn1_named_data));
}
#endif /* MBEDTLS_DEPRECATED_REMOVED */

void mbedtls_asn1_free_named_data_list(mbedtls_asn1_named_data **head)
{
    mbedtls_asn1_named_data *cur;

    while ((cur = *head) != NULL) {
        *head = cur->next;
        mbedtls_free(cur->oid.p);
        mbedtls_free(cur->val.p);
        mbedtls_free(cur);
    }
}

void mbedtls_asn1_free_named_data_list_shallow(mbedtls_asn1_named_data *name)
{
    for (mbedtls_asn1_named_data *next; name != NULL; name = next) {
        next = name->next;
        mbedtls_free(name);
    }
}

const mbedtls_asn1_named_data *mbedtls_asn1_find_named_data(const mbedtls_asn1_named_data *list,
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

#endif /* MBEDTLS_ASN1_PARSE_C */
