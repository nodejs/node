/*
 *  Generic SSL/TLS messaging layer functions
 *  (record layer + retransmission state machine)
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 *  http://www.ietf.org/rfc/rfc2246.txt
 *  http://www.ietf.org/rfc/rfc4346.txt
 */

#include "common.h"

#if defined(MBEDTLS_SSL_TLS_C)

#include "mbedtls/platform.h"

#include "mbedtls/ssl.h"
#include "ssl_misc.h"
#include "debug_internal.h"
#include "mbedtls/error.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/version.h"
#include "constant_time_internal.h"
#include "mbedtls/constant_time.h"

#include <limits.h>
#include <string.h>

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include "psa_util_internal.h"
#include "psa/crypto.h"
#endif

#if defined(MBEDTLS_X509_CRT_PARSE_C)
#include "mbedtls/oid.h"
#endif

#if defined(MBEDTLS_USE_PSA_CRYPTO)
/* Define a local translating function to save code size by not using too many
 * arguments in each translating place. */
static int local_err_translation(psa_status_t status)
{
    return psa_status_to_mbedtls(status, psa_to_ssl_errors,
                                 ARRAY_LENGTH(psa_to_ssl_errors),
                                 psa_generic_status_to_mbedtls);
}
#define PSA_TO_MBEDTLS_ERR(status) local_err_translation(status)
#endif

#if defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)

#if defined(MBEDTLS_USE_PSA_CRYPTO)

#if defined(PSA_WANT_ALG_SHA_384)
#define MAX_HASH_BLOCK_LENGTH PSA_HASH_BLOCK_LENGTH(PSA_ALG_SHA_384)
#elif defined(PSA_WANT_ALG_SHA_256)
#define MAX_HASH_BLOCK_LENGTH PSA_HASH_BLOCK_LENGTH(PSA_ALG_SHA_256)
#else /* See check_config.h */
#define MAX_HASH_BLOCK_LENGTH PSA_HASH_BLOCK_LENGTH(PSA_ALG_SHA_1)
#endif

MBEDTLS_STATIC_TESTABLE
int mbedtls_ct_hmac(mbedtls_svc_key_id_t key,
                    psa_algorithm_t mac_alg,
                    const unsigned char *add_data,
                    size_t add_data_len,
                    const unsigned char *data,
                    size_t data_len_secret,
                    size_t min_data_len,
                    size_t max_data_len,
                    unsigned char *output)
{
    /*
     * This function breaks the HMAC abstraction and uses psa_hash_clone()
     * extension in order to get constant-flow behaviour.
     *
     * HMAC(msg) is defined as HASH(okey + HASH(ikey + msg)) where + means
     * concatenation, and okey/ikey are the XOR of the key with some fixed bit
     * patterns (see RFC 2104, sec. 2).
     *
     * We'll first compute ikey/okey, then inner_hash = HASH(ikey + msg) by
     * hashing up to minlen, then cloning the context, and for each byte up
     * to maxlen finishing up the hash computation, keeping only the
     * correct result.
     *
     * Then we only need to compute HASH(okey + inner_hash) and we're done.
     */
    psa_algorithm_t hash_alg = PSA_ALG_HMAC_GET_HASH(mac_alg);
    const size_t block_size = PSA_HASH_BLOCK_LENGTH(hash_alg);
    unsigned char key_buf[MAX_HASH_BLOCK_LENGTH];
    const size_t hash_size = PSA_HASH_LENGTH(hash_alg);
    psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
    size_t hash_length;

    unsigned char aux_out[PSA_HASH_MAX_SIZE];
    psa_hash_operation_t aux_operation = PSA_HASH_OPERATION_INIT;
    size_t offset;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    size_t mac_key_length;
    size_t i;

#define PSA_CHK(func_call)        \
    do {                            \
        status = (func_call);       \
        if (status != PSA_SUCCESS) \
        goto cleanup;           \
    } while (0)

    /* Export MAC key
     * We assume key length is always exactly the output size
     * which is never more than the block size, thus we use block_size
     * as the key buffer size.
     */
    PSA_CHK(psa_export_key(key, key_buf, block_size, &mac_key_length));

    /* Calculate ikey */
    for (i = 0; i < mac_key_length; i++) {
        key_buf[i] = (unsigned char) (key_buf[i] ^ 0x36);
    }
    for (; i < block_size; ++i) {
        key_buf[i] = 0x36;
    }

    PSA_CHK(psa_hash_setup(&operation, hash_alg));

    /* Now compute inner_hash = HASH(ikey + msg) */
    PSA_CHK(psa_hash_update(&operation, key_buf, block_size));
    PSA_CHK(psa_hash_update(&operation, add_data, add_data_len));
    PSA_CHK(psa_hash_update(&operation, data, min_data_len));

    /* Fill the hash buffer in advance with something that is
     * not a valid hash (barring an attack on the hash and
     * deliberately-crafted input), in case the caller doesn't
     * check the return status properly. */
    memset(output, '!', hash_size);

    /* For each possible length, compute the hash up to that point */
    for (offset = min_data_len; offset <= max_data_len; offset++) {
        PSA_CHK(psa_hash_clone(&operation, &aux_operation));
        PSA_CHK(psa_hash_finish(&aux_operation, aux_out,
                                PSA_HASH_MAX_SIZE, &hash_length));
        /* Keep only the correct inner_hash in the output buffer */
        mbedtls_ct_memcpy_if(mbedtls_ct_uint_eq(offset, data_len_secret),
                             output, aux_out, NULL, hash_size);

        if (offset < max_data_len) {
            PSA_CHK(psa_hash_update(&operation, data + offset, 1));
        }
    }

    /* Abort current operation to prepare for final operation */
    PSA_CHK(psa_hash_abort(&operation));

    /* Calculate okey */
    for (i = 0; i < mac_key_length; i++) {
        key_buf[i] = (unsigned char) ((key_buf[i] ^ 0x36) ^ 0x5C);
    }
    for (; i < block_size; ++i) {
        key_buf[i] = 0x5C;
    }

    /* Now compute HASH(okey + inner_hash) */
    PSA_CHK(psa_hash_setup(&operation, hash_alg));
    PSA_CHK(psa_hash_update(&operation, key_buf, block_size));
    PSA_CHK(psa_hash_update(&operation, output, hash_size));
    PSA_CHK(psa_hash_finish(&operation, output, hash_size, &hash_length));

#undef PSA_CHK

cleanup:
    mbedtls_platform_zeroize(key_buf, MAX_HASH_BLOCK_LENGTH);
    mbedtls_platform_zeroize(aux_out, PSA_HASH_MAX_SIZE);

    psa_hash_abort(&operation);
    psa_hash_abort(&aux_operation);
    return PSA_TO_MBEDTLS_ERR(status);
}

#undef MAX_HASH_BLOCK_LENGTH

#else
MBEDTLS_STATIC_TESTABLE
int mbedtls_ct_hmac(mbedtls_md_context_t *ctx,
                    const unsigned char *add_data,
                    size_t add_data_len,
                    const unsigned char *data,
                    size_t data_len_secret,
                    size_t min_data_len,
                    size_t max_data_len,
                    unsigned char *output)
{
    /*
     * This function breaks the HMAC abstraction and uses the md_clone()
     * extension to the MD API in order to get constant-flow behaviour.
     *
     * HMAC(msg) is defined as HASH(okey + HASH(ikey + msg)) where + means
     * concatenation, and okey/ikey are the XOR of the key with some fixed bit
     * patterns (see RFC 2104, sec. 2), which are stored in ctx->hmac_ctx.
     *
     * We'll first compute inner_hash = HASH(ikey + msg) by hashing up to
     * minlen, then cloning the context, and for each byte up to maxlen
     * finishing up the hash computation, keeping only the correct result.
     *
     * Then we only need to compute HASH(okey + inner_hash) and we're done.
     */
    const mbedtls_md_type_t md_alg = mbedtls_md_get_type(ctx->md_info);
    /* TLS 1.2 only supports SHA-384, SHA-256, SHA-1, MD-5,
     * all of which have the same block size except SHA-384. */
    const size_t block_size = md_alg == MBEDTLS_MD_SHA384 ? 128 : 64;
    const unsigned char * const ikey = ctx->hmac_ctx;
    const unsigned char * const okey = ikey + block_size;
    const size_t hash_size = mbedtls_md_get_size(ctx->md_info);

    unsigned char aux_out[MBEDTLS_MD_MAX_SIZE];
    mbedtls_md_context_t aux;
    size_t offset;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    mbedtls_md_init(&aux);

#define MD_CHK(func_call) \
    do {                    \
        ret = (func_call);  \
        if (ret != 0)      \
        goto cleanup;   \
    } while (0)

    MD_CHK(mbedtls_md_setup(&aux, ctx->md_info, 0));

    /* After hmac_start() of hmac_reset(), ikey has already been hashed,
     * so we can start directly with the message */
    MD_CHK(mbedtls_md_update(ctx, add_data, add_data_len));
    MD_CHK(mbedtls_md_update(ctx, data, min_data_len));

    /* Fill the hash buffer in advance with something that is
     * not a valid hash (barring an attack on the hash and
     * deliberately-crafted input), in case the caller doesn't
     * check the return status properly. */
    memset(output, '!', hash_size);

    /* For each possible length, compute the hash up to that point */
    for (offset = min_data_len; offset <= max_data_len; offset++) {
        MD_CHK(mbedtls_md_clone(&aux, ctx));
        MD_CHK(mbedtls_md_finish(&aux, aux_out));
        /* Keep only the correct inner_hash in the output buffer */
        mbedtls_ct_memcpy_if(mbedtls_ct_uint_eq(offset, data_len_secret),
                             output, aux_out, NULL, hash_size);

        if (offset < max_data_len) {
            MD_CHK(mbedtls_md_update(ctx, data + offset, 1));
        }
    }

    /* The context needs to finish() before it starts() again */
    MD_CHK(mbedtls_md_finish(ctx, aux_out));

    /* Now compute HASH(okey + inner_hash) */
    MD_CHK(mbedtls_md_starts(ctx));
    MD_CHK(mbedtls_md_update(ctx, okey, block_size));
    MD_CHK(mbedtls_md_update(ctx, output, hash_size));
    MD_CHK(mbedtls_md_finish(ctx, output));

    /* Done, get ready for next time */
    MD_CHK(mbedtls_md_hmac_reset(ctx));

#undef MD_CHK

cleanup:
    mbedtls_md_free(&aux);
    return ret;
}

#endif /* MBEDTLS_USE_PSA_CRYPTO */

#endif /* MBEDTLS_SSL_SOME_SUITES_USE_MAC */

static uint32_t ssl_get_hs_total_len(mbedtls_ssl_context const *ssl);

/*
 * Start a timer.
 * Passing millisecs = 0 cancels a running timer.
 */
void mbedtls_ssl_set_timer(mbedtls_ssl_context *ssl, uint32_t millisecs)
{
    if (ssl->f_set_timer == NULL) {
        return;
    }

    MBEDTLS_SSL_DEBUG_MSG(3, ("set_timer to %d ms", (int) millisecs));
    ssl->f_set_timer(ssl->p_timer, millisecs / 4, millisecs);
}

/*
 * Return -1 is timer is expired, 0 if it isn't.
 */
int mbedtls_ssl_check_timer(mbedtls_ssl_context *ssl)
{
    if (ssl->f_get_timer == NULL) {
        return 0;
    }

    if (ssl->f_get_timer(ssl->p_timer) == 2) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("timer expired"));
        return -1;
    }

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_record_header(mbedtls_ssl_context const *ssl,
                                   unsigned char *buf,
                                   size_t len,
                                   mbedtls_record *rec);

int mbedtls_ssl_check_record(mbedtls_ssl_context const *ssl,
                             unsigned char *buf,
                             size_t buflen)
{
    int ret = 0;
    MBEDTLS_SSL_DEBUG_MSG(1, ("=> mbedtls_ssl_check_record"));
    MBEDTLS_SSL_DEBUG_BUF(3, "record buffer", buf, buflen);

    /* We don't support record checking in TLS because
     * there doesn't seem to be a usecase for it.
     */
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_STREAM) {
        ret = MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE;
        goto exit;
    }
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    else {
        mbedtls_record rec;

        ret = ssl_parse_record_header(ssl, buf, buflen, &rec);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(3, "ssl_parse_record_header", ret);
            goto exit;
        }

        if (ssl->transform_in != NULL) {
            ret = mbedtls_ssl_decrypt_buf(ssl, ssl->transform_in, &rec);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(3, "mbedtls_ssl_decrypt_buf", ret);
                goto exit;
            }
        }
    }
#endif /* MBEDTLS_SSL_PROTO_DTLS */

exit:
    /* On success, we have decrypted the buffer in-place, so make
     * sure we don't leak any plaintext data. */
    mbedtls_platform_zeroize(buf, buflen);

    /* For the purpose of this API, treat messages with unexpected CID
     * as well as such from future epochs as unexpected. */
    if (ret == MBEDTLS_ERR_SSL_UNEXPECTED_CID ||
        ret == MBEDTLS_ERR_SSL_EARLY_MESSAGE) {
        ret = MBEDTLS_ERR_SSL_UNEXPECTED_RECORD;
    }

    MBEDTLS_SSL_DEBUG_MSG(1, ("<= mbedtls_ssl_check_record"));
    return ret;
}

#define SSL_DONT_FORCE_FLUSH 0
#define SSL_FORCE_FLUSH      1

#if defined(MBEDTLS_SSL_PROTO_DTLS)

/* Forward declarations for functions related to message buffering. */
static void ssl_buffering_free_slot(mbedtls_ssl_context *ssl,
                                    uint8_t slot);
static void ssl_free_buffered_record(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_load_buffered_message(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_load_buffered_record(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_buffer_message(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_buffer_future_record(mbedtls_ssl_context *ssl,
                                    mbedtls_record const *rec);
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_next_record_is_in_datagram(mbedtls_ssl_context *ssl);

static size_t ssl_get_maximum_datagram_size(mbedtls_ssl_context const *ssl)
{
    size_t mtu = mbedtls_ssl_get_current_mtu(ssl);
#if defined(MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
    size_t out_buf_len = ssl->out_buf_len;
#else
    size_t out_buf_len = MBEDTLS_SSL_OUT_BUFFER_LEN;
#endif

    if (mtu != 0 && mtu < out_buf_len) {
        return mtu;
    }

    return out_buf_len;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_get_remaining_space_in_datagram(mbedtls_ssl_context const *ssl)
{
    size_t const bytes_written = ssl->out_left;
    size_t const mtu           = ssl_get_maximum_datagram_size(ssl);

    /* Double-check that the write-index hasn't gone
     * past what we can transmit in a single datagram. */
    if (bytes_written > mtu) {
        /* Should never happen... */
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    return (int) (mtu - bytes_written);
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_get_remaining_payload_in_datagram(mbedtls_ssl_context const *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t remaining, expansion;
    size_t max_len = MBEDTLS_SSL_OUT_CONTENT_LEN;

#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH)
    const size_t mfl = mbedtls_ssl_get_output_max_frag_len(ssl);

    if (max_len > mfl) {
        max_len = mfl;
    }

    /* By the standard (RFC 6066 Sect. 4), the MFL extension
     * only limits the maximum record payload size, so in theory
     * we would be allowed to pack multiple records of payload size
     * MFL into a single datagram. However, this would mean that there's
     * no way to explicitly communicate MTU restrictions to the peer.
     *
     * The following reduction of max_len makes sure that we never
     * write datagrams larger than MFL + Record Expansion Overhead.
     */
    if (max_len <= ssl->out_left) {
        return 0;
    }

    max_len -= ssl->out_left;
#endif

    ret = ssl_get_remaining_space_in_datagram(ssl);
    if (ret < 0) {
        return ret;
    }
    remaining = (size_t) ret;

    ret = mbedtls_ssl_get_record_expansion(ssl);
    if (ret < 0) {
        return ret;
    }
    expansion = (size_t) ret;

    if (remaining <= expansion) {
        return 0;
    }

    remaining -= expansion;
    if (remaining >= max_len) {
        remaining = max_len;
    }

    return (int) remaining;
}

/*
 * Double the retransmit timeout value, within the allowed range,
 * returning -1 if the maximum value has already been reached.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_double_retransmit_timeout(mbedtls_ssl_context *ssl)
{
    uint32_t new_timeout;

    if (ssl->handshake->retransmit_timeout >= ssl->conf->hs_timeout_max) {
        return -1;
    }

    /* Implement the final paragraph of RFC 6347 section 4.1.1.1
     * in the following way: after the initial transmission and a first
     * retransmission, back off to a temporary estimated MTU of 508 bytes.
     * This value is guaranteed to be deliverable (if not guaranteed to be
     * delivered) of any compliant IPv4 (and IPv6) network, and should work
     * on most non-IP stacks too. */
    if (ssl->handshake->retransmit_timeout != ssl->conf->hs_timeout_min) {
        ssl->handshake->mtu = 508;
        MBEDTLS_SSL_DEBUG_MSG(2, ("mtu autoreduction to %d bytes", ssl->handshake->mtu));
    }

    new_timeout = 2 * ssl->handshake->retransmit_timeout;

    /* Avoid arithmetic overflow and range overflow */
    if (new_timeout < ssl->handshake->retransmit_timeout ||
        new_timeout > ssl->conf->hs_timeout_max) {
        new_timeout = ssl->conf->hs_timeout_max;
    }

    ssl->handshake->retransmit_timeout = new_timeout;
    MBEDTLS_SSL_DEBUG_MSG(3, ("update timeout value to %lu millisecs",
                              (unsigned long) ssl->handshake->retransmit_timeout));

    return 0;
}

static void ssl_reset_retransmit_timeout(mbedtls_ssl_context *ssl)
{
    ssl->handshake->retransmit_timeout = ssl->conf->hs_timeout_min;
    MBEDTLS_SSL_DEBUG_MSG(3, ("update timeout value to %lu millisecs",
                              (unsigned long) ssl->handshake->retransmit_timeout));
}
#endif /* MBEDTLS_SSL_PROTO_DTLS */

/*
 * Encryption/decryption functions
 */

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID) || defined(MBEDTLS_SSL_PROTO_TLS1_3)

static size_t ssl_compute_padding_length(size_t len,
                                         size_t granularity)
{
    return (granularity - (len + 1) % granularity) % granularity;
}

/* This functions transforms a (D)TLS plaintext fragment and a record content
 * type into an instance of the (D)TLSInnerPlaintext structure. This is used
 * in DTLS 1.2 + CID and within TLS 1.3 to allow flexible padding and to protect
 * a record's content type.
 *
 *        struct {
 *            opaque content[DTLSPlaintext.length];
 *            ContentType real_type;
 *            uint8 zeros[length_of_padding];
 *        } (D)TLSInnerPlaintext;
 *
 *  Input:
 *  - `content`: The beginning of the buffer holding the
 *               plaintext to be wrapped.
 *  - `*content_size`: The length of the plaintext in Bytes.
 *  - `max_len`: The number of Bytes available starting from
 *               `content`. This must be `>= *content_size`.
 *  - `rec_type`: The desired record content type.
 *
 *  Output:
 *  - `content`: The beginning of the resulting (D)TLSInnerPlaintext structure.
 *  - `*content_size`: The length of the resulting (D)TLSInnerPlaintext structure.
 *
 *  Returns:
 *  - `0` on success.
 *  - A negative error code if `max_len` didn't offer enough space
 *    for the expansion.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_build_inner_plaintext(unsigned char *content,
                                     size_t *content_size,
                                     size_t remaining,
                                     uint8_t rec_type,
                                     size_t pad)
{
    size_t len = *content_size;

    /* Write real content type */
    if (remaining == 0) {
        return -1;
    }
    content[len] = rec_type;
    len++;
    remaining--;

    if (remaining < pad) {
        return -1;
    }
    memset(content + len, 0, pad);
    len += pad;
    remaining -= pad;

    *content_size = len;
    return 0;
}

/* This function parses a (D)TLSInnerPlaintext structure.
 * See ssl_build_inner_plaintext() for details. */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_inner_plaintext(unsigned char const *content,
                                     size_t *content_size,
                                     uint8_t *rec_type)
{
    size_t remaining = *content_size;

    /* Determine length of padding by skipping zeroes from the back. */
    do {
        if (remaining == 0) {
            return -1;
        }
        remaining--;
    } while (content[remaining] == 0);

    *content_size = remaining;
    *rec_type = content[remaining];

    return 0;
}
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID || MBEDTLS_SSL_PROTO_TLS1_3 */

/* The size of the `add_data` structure depends on various
 * factors, namely
 *
 * 1) CID functionality disabled
 *
 * additional_data =
 *    8:                    seq_num +
 *    1:                       type +
 *    2:                    version +
 *    2:  length of inner plaintext +
 *
 * size = 13 bytes
 *
 * 2) CID functionality based on RFC 9146 enabled
 *
 * size = 8 + 1 + 1 + 1 + 2 + 2 + 6 + 2 + CID-length
 *      = 23 + CID-length
 *
 * 3) CID functionality based on legacy CID version
    according to draft-ietf-tls-dtls-connection-id-05
 *  https://tools.ietf.org/html/draft-ietf-tls-dtls-connection-id-05
 *
 * size = 13 + 1 + CID-length
 *
 * More information about the CID usage:
 *
 * Per Section 5.3 of draft-ietf-tls-dtls-connection-id-05 the
 * size of the additional data structure is calculated as:
 *
 * additional_data =
 *    8:                    seq_num +
 *    1:                  tls12_cid +
 *    2:     DTLSCipherText.version +
 *    n:                        cid +
 *    1:                 cid_length +
 *    2: length_of_DTLSInnerPlaintext
 *
 * Per RFC 9146 the size of the add_data structure is calculated as:
 *
 * additional_data =
 *    8:        seq_num_placeholder +
 *    1:                  tls12_cid +
 *    1:                 cid_length +
 *    1:                  tls12_cid +
 *    2:     DTLSCiphertext.version +
 *    2:                      epoch +
 *    6:            sequence_number +
 *    n:                        cid +
 *    2: length_of_DTLSInnerPlaintext
 *
 */
static void ssl_extract_add_data_from_record(unsigned char *add_data,
                                             size_t *add_data_len,
                                             mbedtls_record *rec,
                                             mbedtls_ssl_protocol_version
                                             tls_version,
                                             size_t taglen)
{
    /* Several types of ciphers have been defined for use with TLS and DTLS,
     * and the MAC calculations for those ciphers differ slightly. Further
     * variants were added when the CID functionality was added with RFC 9146.
     * This implementations also considers the use of a legacy version of the
     * CID specification published in draft-ietf-tls-dtls-connection-id-05,
     * which is used in deployments.
     *
     * We will distinguish between the non-CID and the CID cases below.
     *
     * --- Non-CID cases ---
     *
     * Quoting RFC 5246 (TLS 1.2):
     *
     *    additional_data = seq_num + TLSCompressed.type +
     *                      TLSCompressed.version + TLSCompressed.length;
     *
     * For TLS 1.3, the record sequence number is dropped from the AAD
     * and encoded within the nonce of the AEAD operation instead.
     * Moreover, the additional data involves the length of the TLS
     * ciphertext, not the TLS plaintext as in earlier versions.
     * Quoting RFC 8446 (TLS 1.3):
     *
     *      additional_data = TLSCiphertext.opaque_type ||
     *                        TLSCiphertext.legacy_record_version ||
     *                        TLSCiphertext.length
     *
     * We pass the tag length to this function in order to compute the
     * ciphertext length from the inner plaintext length rec->data_len via
     *
     *     TLSCiphertext.length = TLSInnerPlaintext.length + taglen.
     *
     * --- CID cases ---
     *
     * RFC 9146 uses a common pattern when constructing the data
     * passed into a MAC / AEAD cipher.
     *
     * Data concatenation for MACs used with block ciphers with
     * Encrypt-then-MAC Processing (with CID):
     *
     *  data = seq_num_placeholder +
     *         tls12_cid +
     *         cid_length +
     *         tls12_cid +
     *         DTLSCiphertext.version +
     *         epoch +
     *         sequence_number +
     *         cid +
     *         DTLSCiphertext.length +
     *         IV +
     *         ENC(content + padding + padding_length)
     *
     * Data concatenation for MACs used with block ciphers (with CID):
     *
     *  data =  seq_num_placeholder +
     *          tls12_cid +
     *          cid_length +
     *          tls12_cid +
     *          DTLSCiphertext.version +
     *          epoch +
     *          sequence_number +
     *          cid +
     *          length_of_DTLSInnerPlaintext +
     *          DTLSInnerPlaintext.content +
     *          DTLSInnerPlaintext.real_type +
     *          DTLSInnerPlaintext.zeros
     *
     * AEAD ciphers use the following additional data calculation (with CIDs):
     *
     *     additional_data = seq_num_placeholder +
     *                tls12_cid +
     *                cid_length +
     *                tls12_cid +
     *                DTLSCiphertext.version +
     *                epoch +
     *                sequence_number +
     *                cid +
     *                length_of_DTLSInnerPlaintext
     *
     * Section 5.3 of draft-ietf-tls-dtls-connection-id-05 (for legacy CID use)
     * defines the additional data calculation as follows:
     *
     *     additional_data = seq_num +
     *                tls12_cid +
     *                DTLSCipherText.version +
     *                cid +
     *                cid_length +
     *                length_of_DTLSInnerPlaintext
     */

    unsigned char *cur = add_data;
    size_t ad_len_field = rec->data_len;

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID) && \
    MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT == 0
    const unsigned char seq_num_placeholder[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    if (tls_version == MBEDTLS_SSL_VERSION_TLS1_3) {
        /* In TLS 1.3, the AAD contains the length of the TLSCiphertext,
         * which differs from the length of the TLSInnerPlaintext
         * by the length of the authentication tag. */
        ad_len_field += taglen;
    } else
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */
    {
        ((void) tls_version);
        ((void) taglen);

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID) && \
        MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT == 0
        if (rec->cid_len != 0) {
            // seq_num_placeholder
            memcpy(cur, seq_num_placeholder, sizeof(seq_num_placeholder));
            cur += sizeof(seq_num_placeholder);

            // tls12_cid type
            *cur = rec->type;
            cur++;

            // cid_length
            *cur = rec->cid_len;
            cur++;
        } else
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
        {
            // epoch + sequence number
            memcpy(cur, rec->ctr, sizeof(rec->ctr));
            cur += sizeof(rec->ctr);
        }
    }

    // type
    *cur = rec->type;
    cur++;

    // version
    memcpy(cur, rec->ver, sizeof(rec->ver));
    cur += sizeof(rec->ver);

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID) && \
    MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT == 1

    if (rec->cid_len != 0) {
        // CID
        memcpy(cur, rec->cid, rec->cid_len);
        cur += rec->cid_len;

        // cid_length
        *cur = rec->cid_len;
        cur++;

        // length of inner plaintext
        MBEDTLS_PUT_UINT16_BE(ad_len_field, cur, 0);
        cur += 2;
    } else
#elif defined(MBEDTLS_SSL_DTLS_CONNECTION_ID) && \
    MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT == 0

    if (rec->cid_len != 0) {
        // epoch + sequence number
        memcpy(cur, rec->ctr, sizeof(rec->ctr));
        cur += sizeof(rec->ctr);

        // CID
        memcpy(cur, rec->cid, rec->cid_len);
        cur += rec->cid_len;

        // length of inner plaintext
        MBEDTLS_PUT_UINT16_BE(ad_len_field, cur, 0);
        cur += 2;
    } else
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
    {
        MBEDTLS_PUT_UINT16_BE(ad_len_field, cur, 0);
        cur += 2;
    }

    *add_data_len = (size_t) (cur - add_data);
}

#if defined(MBEDTLS_SSL_HAVE_AEAD)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_transform_aead_dynamic_iv_is_explicit(
    mbedtls_ssl_transform const *transform)
{
    return transform->ivlen != transform->fixed_ivlen;
}

/* Compute IV := ( fixed_iv || 0 ) XOR ( 0 || dynamic_IV )
 *
 * Concretely, this occurs in two variants:
 *
 * a) Fixed and dynamic IV lengths add up to total IV length, giving
 *       IV = fixed_iv || dynamic_iv
 *
 *    This variant is used in TLS 1.2 when used with GCM or CCM.
 *
 * b) Fixed IV lengths matches total IV length, giving
 *       IV = fixed_iv XOR ( 0 || dynamic_iv )
 *
 *    This variant occurs in TLS 1.3 and for TLS 1.2 when using ChaChaPoly.
 *
 * See also the documentation of mbedtls_ssl_transform.
 *
 * This function has the precondition that
 *
 *     dst_iv_len >= max( fixed_iv_len, dynamic_iv_len )
 *
 * which has to be ensured by the caller. If this precondition
 * violated, the behavior of this function is undefined.
 */
static void ssl_build_record_nonce(unsigned char *dst_iv,
                                   size_t dst_iv_len,
                                   unsigned char const *fixed_iv,
                                   size_t fixed_iv_len,
                                   unsigned char const *dynamic_iv,
                                   size_t dynamic_iv_len)
{
    /* Start with Fixed IV || 0 */
    memset(dst_iv, 0, dst_iv_len);
    memcpy(dst_iv, fixed_iv, fixed_iv_len);

    dst_iv += dst_iv_len - dynamic_iv_len;
    mbedtls_xor(dst_iv, dst_iv, dynamic_iv, dynamic_iv_len);
}
#endif /* MBEDTLS_SSL_HAVE_AEAD */

int mbedtls_ssl_encrypt_buf(mbedtls_ssl_context *ssl,
                            mbedtls_ssl_transform *transform,
                            mbedtls_record *rec,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    mbedtls_ssl_mode_t ssl_mode;
    int auth_done = 0;
    unsigned char *data;
    /* For an explanation of the additional data length see
     * the description of ssl_extract_add_data_from_record().
     */
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    unsigned char add_data[23 + MBEDTLS_SSL_CID_OUT_LEN_MAX];
#else
    unsigned char add_data[13];
#endif
    size_t add_data_len;
    size_t post_avail;

    /* The SSL context is only used for debugging purposes! */
#if !defined(MBEDTLS_DEBUG_C)
    ssl = NULL; /* make sure we don't use it except for debug */
    ((void) ssl);
#endif

    /* The PRNG is used for dynamic IV generation that's used
     * for CBC transformations in TLS 1.2. */
#if !(defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC) && \
    defined(MBEDTLS_SSL_PROTO_TLS1_2))
    ((void) f_rng);
    ((void) p_rng);
#endif

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> encrypt buf"));

    if (transform == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("no transform provided to encrypt_buf"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
    if (rec == NULL
        || rec->buf == NULL
        || rec->buf_len < rec->data_offset
        || rec->buf_len - rec->data_offset < rec->data_len
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
        || rec->cid_len != 0
#endif
        ) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad record structure provided to encrypt_buf"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    ssl_mode = mbedtls_ssl_get_mode_from_transform(transform);

    data = rec->buf + rec->data_offset;
    post_avail = rec->buf_len - (rec->data_len + rec->data_offset);
    MBEDTLS_SSL_DEBUG_BUF(4, "before encrypt: output payload",
                          data, rec->data_len);

    if (rec->data_len > MBEDTLS_SSL_OUT_CONTENT_LEN) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("Record content %" MBEDTLS_PRINTF_SIZET
                                  " too large, maximum %" MBEDTLS_PRINTF_SIZET,
                                  rec->data_len,
                                  (size_t) MBEDTLS_SSL_OUT_CONTENT_LEN));
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    /* The following two code paths implement the (D)TLSInnerPlaintext
     * structure present in TLS 1.3 and DTLS 1.2 + CID.
     *
     * See ssl_build_inner_plaintext() for more information.
     *
     * Note that this changes `rec->data_len`, and hence
     * `post_avail` needs to be recalculated afterwards.
     *
     * Note also that the two code paths cannot occur simultaneously
     * since they apply to different versions of the protocol. There
     * is hence no risk of double-addition of the inner plaintext.
     */
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    if (transform->tls_version == MBEDTLS_SSL_VERSION_TLS1_3) {
        size_t padding =
            ssl_compute_padding_length(rec->data_len,
                                       MBEDTLS_SSL_CID_TLS1_3_PADDING_GRANULARITY);
        if (ssl_build_inner_plaintext(data,
                                      &rec->data_len,
                                      post_avail,
                                      rec->type,
                                      padding) != 0) {
            return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
        }

        rec->type = MBEDTLS_SSL_MSG_APPLICATION_DATA;
    }
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    /*
     * Add CID information
     */
    rec->cid_len = transform->out_cid_len;
    memcpy(rec->cid, transform->out_cid, transform->out_cid_len);
    MBEDTLS_SSL_DEBUG_BUF(3, "CID", rec->cid, rec->cid_len);

    if (rec->cid_len != 0) {
        size_t padding =
            ssl_compute_padding_length(rec->data_len,
                                       MBEDTLS_SSL_CID_TLS1_3_PADDING_GRANULARITY);
        /*
         * Wrap plaintext into DTLSInnerPlaintext structure.
         * See ssl_build_inner_plaintext() for more information.
         *
         * Note that this changes `rec->data_len`, and hence
         * `post_avail` needs to be recalculated afterwards.
         */
        if (ssl_build_inner_plaintext(data,
                                      &rec->data_len,
                                      post_avail,
                                      rec->type,
                                      padding) != 0) {
            return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
        }

        rec->type = MBEDTLS_SSL_MSG_CID;
    }
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

    post_avail = rec->buf_len - (rec->data_len + rec->data_offset);

    /*
     * Add MAC before if needed
     */
#if defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)
    if (ssl_mode == MBEDTLS_SSL_MODE_STREAM ||
        ssl_mode == MBEDTLS_SSL_MODE_CBC) {
        if (post_avail < transform->maclen) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("Buffer provided for encrypted record not large enough"));
            return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
        }
#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
        unsigned char mac[MBEDTLS_SSL_MAC_ADD];
        int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
#if defined(MBEDTLS_USE_PSA_CRYPTO)
        psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;
        psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
        size_t sign_mac_length = 0;
#endif /* MBEDTLS_USE_PSA_CRYPTO */

        ssl_extract_add_data_from_record(add_data, &add_data_len, rec,
                                         transform->tls_version,
                                         transform->taglen);

#if defined(MBEDTLS_USE_PSA_CRYPTO)
        status = psa_mac_sign_setup(&operation, transform->psa_mac_enc,
                                    transform->psa_mac_alg);
        if (status != PSA_SUCCESS) {
            goto hmac_failed_etm_disabled;
        }

        status = psa_mac_update(&operation, add_data, add_data_len);
        if (status != PSA_SUCCESS) {
            goto hmac_failed_etm_disabled;
        }

        status = psa_mac_update(&operation, data, rec->data_len);
        if (status != PSA_SUCCESS) {
            goto hmac_failed_etm_disabled;
        }

        status = psa_mac_sign_finish(&operation, mac, MBEDTLS_SSL_MAC_ADD,
                                     &sign_mac_length);
        if (status != PSA_SUCCESS) {
            goto hmac_failed_etm_disabled;
        }
#else
        ret = mbedtls_md_hmac_update(&transform->md_ctx_enc, add_data,
                                     add_data_len);
        if (ret != 0) {
            goto hmac_failed_etm_disabled;
        }
        ret = mbedtls_md_hmac_update(&transform->md_ctx_enc, data, rec->data_len);
        if (ret != 0) {
            goto hmac_failed_etm_disabled;
        }
        ret = mbedtls_md_hmac_finish(&transform->md_ctx_enc, mac);
        if (ret != 0) {
            goto hmac_failed_etm_disabled;
        }
        ret = mbedtls_md_hmac_reset(&transform->md_ctx_enc);
        if (ret != 0) {
            goto hmac_failed_etm_disabled;
        }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

        memcpy(data + rec->data_len, mac, transform->maclen);
#endif

        MBEDTLS_SSL_DEBUG_BUF(4, "computed mac", data + rec->data_len,
                              transform->maclen);

        rec->data_len += transform->maclen;
        post_avail -= transform->maclen;
        auth_done++;

hmac_failed_etm_disabled:
        mbedtls_platform_zeroize(mac, transform->maclen);
#if defined(MBEDTLS_USE_PSA_CRYPTO)
        ret = PSA_TO_MBEDTLS_ERR(status);
        status = psa_mac_abort(&operation);
        if (ret == 0 && status != PSA_SUCCESS) {
            ret = PSA_TO_MBEDTLS_ERR(status);
        }
#endif /* MBEDTLS_USE_PSA_CRYPTO */
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_md_hmac_xxx", ret);
            return ret;
        }
    }
#endif /* MBEDTLS_SSL_SOME_SUITES_USE_MAC */

    /*
     * Encrypt
     */
#if defined(MBEDTLS_SSL_SOME_SUITES_USE_STREAM)
    if (ssl_mode == MBEDTLS_SSL_MODE_STREAM) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("before encrypt: msglen = %" MBEDTLS_PRINTF_SIZET ", "
                                                                                    "including %d bytes of padding",
                                  rec->data_len, 0));

        /* The only supported stream cipher is "NULL",
         * so there's nothing to do here.*/
    } else
#endif /* MBEDTLS_SSL_SOME_SUITES_USE_STREAM */

#if defined(MBEDTLS_SSL_HAVE_AEAD)
    if (ssl_mode == MBEDTLS_SSL_MODE_AEAD) {
        unsigned char iv[12];
        unsigned char *dynamic_iv;
        size_t dynamic_iv_len;
        int dynamic_iv_is_explicit =
            ssl_transform_aead_dynamic_iv_is_explicit(transform);
#if defined(MBEDTLS_USE_PSA_CRYPTO)
        psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
#endif /* MBEDTLS_USE_PSA_CRYPTO */
        int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

        /* Check that there's space for the authentication tag. */
        if (post_avail < transform->taglen) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("Buffer provided for encrypted record not large enough"));
            return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
        }

        /*
         * Build nonce for AEAD encryption.
         *
         * Note: In the case of CCM and GCM in TLS 1.2, the dynamic
         *       part of the IV is prepended to the ciphertext and
         *       can be chosen freely - in particular, it need not
         *       agree with the record sequence number.
         *       However, since ChaChaPoly as well as all AEAD modes
         *       in TLS 1.3 use the record sequence number as the
         *       dynamic part of the nonce, we uniformly use the
         *       record sequence number here in all cases.
         */
        dynamic_iv     = rec->ctr;
        dynamic_iv_len = sizeof(rec->ctr);

        ssl_build_record_nonce(iv, sizeof(iv),
                               transform->iv_enc,
                               transform->fixed_ivlen,
                               dynamic_iv,
                               dynamic_iv_len);

        /*
         * Build additional data for AEAD encryption.
         * This depends on the TLS version.
         */
        ssl_extract_add_data_from_record(add_data, &add_data_len, rec,
                                         transform->tls_version,
                                         transform->taglen);

        MBEDTLS_SSL_DEBUG_BUF(4, "IV used (internal)",
                              iv, transform->ivlen);
        MBEDTLS_SSL_DEBUG_BUF(4, "IV used (transmitted)",
                              dynamic_iv,
                              dynamic_iv_is_explicit ? dynamic_iv_len : 0);
        MBEDTLS_SSL_DEBUG_BUF(4, "additional data used for AEAD",
                              add_data, add_data_len);
        MBEDTLS_SSL_DEBUG_MSG(3, ("before encrypt: msglen = %" MBEDTLS_PRINTF_SIZET ", "
                                                                                    "including 0 bytes of padding",
                                  rec->data_len));

        /*
         * Encrypt and authenticate
         */
#if defined(MBEDTLS_USE_PSA_CRYPTO)
        status = psa_aead_encrypt(transform->psa_key_enc,
                                  transform->psa_alg,
                                  iv, transform->ivlen,
                                  add_data, add_data_len,
                                  data, rec->data_len,
                                  data, rec->buf_len - (data - rec->buf),
                                  &rec->data_len);

        if (status != PSA_SUCCESS) {
            ret = PSA_TO_MBEDTLS_ERR(status);
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_encrypt_buf", ret);
            return ret;
        }
#else
        if ((ret = mbedtls_cipher_auth_encrypt_ext(&transform->cipher_ctx_enc,
                                                   iv, transform->ivlen,
                                                   add_data, add_data_len,
                                                   data, rec->data_len, /* src */
                                                   data, rec->buf_len - (size_t) (data - rec->buf), /* dst */
                                                   &rec->data_len,
                                                   transform->taglen)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_cipher_auth_encrypt_ext", ret);
            return ret;
        }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

        MBEDTLS_SSL_DEBUG_BUF(4, "after encrypt: tag",
                              data + rec->data_len - transform->taglen,
                              transform->taglen);
        /* Account for authentication tag. */
        post_avail -= transform->taglen;

        /*
         * Prefix record content with dynamic IV in case it is explicit.
         */
        if (dynamic_iv_is_explicit != 0) {
            if (rec->data_offset < dynamic_iv_len) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("Buffer provided for encrypted record not large enough"));
                return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
            }

            memcpy(data - dynamic_iv_len, dynamic_iv, dynamic_iv_len);
            rec->data_offset -= dynamic_iv_len;
            rec->data_len    += dynamic_iv_len;
        }

        auth_done++;
    } else
#endif /* MBEDTLS_SSL_HAVE_AEAD */
#if defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC)
    if (ssl_mode == MBEDTLS_SSL_MODE_CBC ||
        ssl_mode == MBEDTLS_SSL_MODE_CBC_ETM) {
        int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
        size_t padlen, i;
        size_t olen;
#if defined(MBEDTLS_USE_PSA_CRYPTO)
        psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
        size_t part_len;
        psa_cipher_operation_t cipher_op = PSA_CIPHER_OPERATION_INIT;
#endif /* MBEDTLS_USE_PSA_CRYPTO */

        /* Currently we're always using minimal padding
         * (up to 255 bytes would be allowed). */
        padlen = transform->ivlen - (rec->data_len + 1) % transform->ivlen;
        if (padlen == transform->ivlen) {
            padlen = 0;
        }

        /* Check there's enough space in the buffer for the padding. */
        if (post_avail < padlen + 1) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("Buffer provided for encrypted record not large enough"));
            return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
        }

        for (i = 0; i <= padlen; i++) {
            data[rec->data_len + i] = (unsigned char) padlen;
        }

        rec->data_len += padlen + 1;
        post_avail -= padlen + 1;

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
        /*
         * Prepend per-record IV for block cipher in TLS v1.2 as per
         * Method 1 (6.2.3.2. in RFC4346 and RFC5246)
         */
        if (f_rng == NULL) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("No PRNG provided to encrypt_record routine"));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        if (rec->data_offset < transform->ivlen) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("Buffer provided for encrypted record not large enough"));
            return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
        }

        /*
         * Generate IV
         */
        ret = f_rng(p_rng, transform->iv_enc, transform->ivlen);
        if (ret != 0) {
            return ret;
        }

        memcpy(data - transform->ivlen, transform->iv_enc, transform->ivlen);
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

        MBEDTLS_SSL_DEBUG_MSG(3, ("before encrypt: msglen = %" MBEDTLS_PRINTF_SIZET ", "
                                                                                    "including %"
                                  MBEDTLS_PRINTF_SIZET
                                  " bytes of IV and %" MBEDTLS_PRINTF_SIZET " bytes of padding",
                                  rec->data_len, transform->ivlen,
                                  padlen + 1));

#if defined(MBEDTLS_USE_PSA_CRYPTO)
        status = psa_cipher_encrypt_setup(&cipher_op,
                                          transform->psa_key_enc, transform->psa_alg);

        if (status != PSA_SUCCESS) {
            ret = PSA_TO_MBEDTLS_ERR(status);
            MBEDTLS_SSL_DEBUG_RET(1, "psa_cipher_encrypt_setup", ret);
            return ret;
        }

        status = psa_cipher_set_iv(&cipher_op, transform->iv_enc, transform->ivlen);

        if (status != PSA_SUCCESS) {
            ret = PSA_TO_MBEDTLS_ERR(status);
            MBEDTLS_SSL_DEBUG_RET(1, "psa_cipher_set_iv", ret);
            return ret;

        }

        status = psa_cipher_update(&cipher_op,
                                   data, rec->data_len,
                                   data, rec->data_len, &olen);

        if (status != PSA_SUCCESS) {
            ret = PSA_TO_MBEDTLS_ERR(status);
            MBEDTLS_SSL_DEBUG_RET(1, "psa_cipher_update", ret);
            return ret;

        }

        status = psa_cipher_finish(&cipher_op,
                                   data + olen, rec->data_len - olen,
                                   &part_len);

        if (status != PSA_SUCCESS) {
            ret = PSA_TO_MBEDTLS_ERR(status);
            MBEDTLS_SSL_DEBUG_RET(1, "psa_cipher_finish", ret);
            return ret;

        }

        olen += part_len;
#else
        if ((ret = mbedtls_cipher_crypt(&transform->cipher_ctx_enc,
                                        transform->iv_enc,
                                        transform->ivlen,
                                        data, rec->data_len,
                                        data, &olen)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_cipher_crypt", ret);
            return ret;
        }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

        if (rec->data_len != olen) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        data             -= transform->ivlen;
        rec->data_offset -= transform->ivlen;
        rec->data_len    += transform->ivlen;

#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
        if (auth_done == 0) {
            unsigned char mac[MBEDTLS_SSL_MAC_ADD];
#if defined(MBEDTLS_USE_PSA_CRYPTO)
            psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;
            size_t sign_mac_length = 0;
#endif /* MBEDTLS_USE_PSA_CRYPTO */

            /* MAC(MAC_write_key, add_data, IV, ENC(content + padding + padding_length))
             */

            if (post_avail < transform->maclen) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("Buffer provided for encrypted record not large enough"));
                return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
            }

            ssl_extract_add_data_from_record(add_data, &add_data_len,
                                             rec, transform->tls_version,
                                             transform->taglen);

            MBEDTLS_SSL_DEBUG_MSG(3, ("using encrypt then mac"));
            MBEDTLS_SSL_DEBUG_BUF(4, "MAC'd meta-data", add_data,
                                  add_data_len);
#if defined(MBEDTLS_USE_PSA_CRYPTO)
            status = psa_mac_sign_setup(&operation, transform->psa_mac_enc,
                                        transform->psa_mac_alg);
            if (status != PSA_SUCCESS) {
                goto hmac_failed_etm_enabled;
            }

            status = psa_mac_update(&operation, add_data, add_data_len);
            if (status != PSA_SUCCESS) {
                goto hmac_failed_etm_enabled;
            }

            status = psa_mac_update(&operation, data, rec->data_len);
            if (status != PSA_SUCCESS) {
                goto hmac_failed_etm_enabled;
            }

            status = psa_mac_sign_finish(&operation, mac, MBEDTLS_SSL_MAC_ADD,
                                         &sign_mac_length);
            if (status != PSA_SUCCESS) {
                goto hmac_failed_etm_enabled;
            }
#else

            ret = mbedtls_md_hmac_update(&transform->md_ctx_enc, add_data,
                                         add_data_len);
            if (ret != 0) {
                goto hmac_failed_etm_enabled;
            }
            ret = mbedtls_md_hmac_update(&transform->md_ctx_enc,
                                         data, rec->data_len);
            if (ret != 0) {
                goto hmac_failed_etm_enabled;
            }
            ret = mbedtls_md_hmac_finish(&transform->md_ctx_enc, mac);
            if (ret != 0) {
                goto hmac_failed_etm_enabled;
            }
            ret = mbedtls_md_hmac_reset(&transform->md_ctx_enc);
            if (ret != 0) {
                goto hmac_failed_etm_enabled;
            }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

            memcpy(data + rec->data_len, mac, transform->maclen);

            rec->data_len += transform->maclen;
            post_avail -= transform->maclen;
            auth_done++;

hmac_failed_etm_enabled:
            mbedtls_platform_zeroize(mac, transform->maclen);
#if defined(MBEDTLS_USE_PSA_CRYPTO)
            ret = PSA_TO_MBEDTLS_ERR(status);
            status = psa_mac_abort(&operation);
            if (ret == 0 && status != PSA_SUCCESS) {
                ret = PSA_TO_MBEDTLS_ERR(status);
            }
#endif /* MBEDTLS_USE_PSA_CRYPTO */
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "HMAC calculation failed", ret);
                return ret;
            }
        }
#endif /* MBEDTLS_SSL_ENCRYPT_THEN_MAC */
    } else
#endif /* MBEDTLS_SSL_SOME_SUITES_USE_CBC) */
    {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    /* Make extra sure authentication was performed, exactly once */
    if (auth_done != 1) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= encrypt buf"));

    return 0;
}

int mbedtls_ssl_decrypt_buf(mbedtls_ssl_context const *ssl,
                            mbedtls_ssl_transform *transform,
                            mbedtls_record *rec)
{
#if defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC) || defined(MBEDTLS_SSL_HAVE_AEAD)
    size_t olen;
#endif /* MBEDTLS_SSL_SOME_SUITES_USE_CBC || MBEDTLS_SSL_HAVE_AEAD */
    mbedtls_ssl_mode_t ssl_mode;
    int ret;

    int auth_done = 0;
#if defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)
    size_t padlen = 0;
    mbedtls_ct_condition_t correct = MBEDTLS_CT_TRUE;
#endif
    unsigned char *data;
    /* For an explanation of the additional data length see
     * the description of ssl_extract_add_data_from_record().
     */
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    unsigned char add_data[23 + MBEDTLS_SSL_CID_IN_LEN_MAX];
#else
    unsigned char add_data[13];
#endif
    size_t add_data_len;

#if !defined(MBEDTLS_DEBUG_C)
    ssl = NULL; /* make sure we don't use it except for debug */
    ((void) ssl);
#endif

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> decrypt buf"));
    if (rec == NULL                     ||
        rec->buf == NULL                ||
        rec->buf_len < rec->data_offset ||
        rec->buf_len - rec->data_offset < rec->data_len) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad record structure provided to decrypt_buf"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    data = rec->buf + rec->data_offset;
    ssl_mode = mbedtls_ssl_get_mode_from_transform(transform);

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    /*
     * Match record's CID with incoming CID.
     */
    if (rec->cid_len != transform->in_cid_len ||
        memcmp(rec->cid, transform->in_cid, rec->cid_len) != 0) {
        return MBEDTLS_ERR_SSL_UNEXPECTED_CID;
    }
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

#if defined(MBEDTLS_SSL_SOME_SUITES_USE_STREAM)
    if (ssl_mode == MBEDTLS_SSL_MODE_STREAM) {
        if (rec->data_len < transform->maclen) {
            MBEDTLS_SSL_DEBUG_MSG(1,
                                  ("Record too short for MAC:"
                                   " %" MBEDTLS_PRINTF_SIZET " < %" MBEDTLS_PRINTF_SIZET,
                                   rec->data_len, transform->maclen));
            return MBEDTLS_ERR_SSL_INVALID_MAC;
        }

        /* The only supported stream cipher is "NULL",
         * so there's no encryption to do here.*/
    } else
#endif /* MBEDTLS_SSL_SOME_SUITES_USE_STREAM */
#if defined(MBEDTLS_SSL_HAVE_AEAD)
    if (ssl_mode == MBEDTLS_SSL_MODE_AEAD) {
        unsigned char iv[12];
        unsigned char *dynamic_iv;
        size_t dynamic_iv_len;
#if defined(MBEDTLS_USE_PSA_CRYPTO)
        psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
#endif /* MBEDTLS_USE_PSA_CRYPTO */

        /*
         * Extract dynamic part of nonce for AEAD decryption.
         *
         * Note: In the case of CCM and GCM in TLS 1.2, the dynamic
         *       part of the IV is prepended to the ciphertext and
         *       can be chosen freely - in particular, it need not
         *       agree with the record sequence number.
         */
        dynamic_iv_len = sizeof(rec->ctr);
        if (ssl_transform_aead_dynamic_iv_is_explicit(transform) == 1) {
            if (rec->data_len < dynamic_iv_len) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("msglen (%" MBEDTLS_PRINTF_SIZET
                                          " ) < explicit_iv_len (%" MBEDTLS_PRINTF_SIZET ") ",
                                          rec->data_len,
                                          dynamic_iv_len));
                return MBEDTLS_ERR_SSL_INVALID_MAC;
            }
            dynamic_iv = data;

            data += dynamic_iv_len;
            rec->data_offset += dynamic_iv_len;
            rec->data_len    -= dynamic_iv_len;
        } else {
            dynamic_iv = rec->ctr;
        }

        /* Check that there's space for the authentication tag. */
        if (rec->data_len < transform->taglen) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("msglen (%" MBEDTLS_PRINTF_SIZET
                                      ") < taglen (%" MBEDTLS_PRINTF_SIZET ") ",
                                      rec->data_len,
                                      transform->taglen));
            return MBEDTLS_ERR_SSL_INVALID_MAC;
        }
        rec->data_len -= transform->taglen;

        /*
         * Prepare nonce from dynamic and static parts.
         */
        ssl_build_record_nonce(iv, sizeof(iv),
                               transform->iv_dec,
                               transform->fixed_ivlen,
                               dynamic_iv,
                               dynamic_iv_len);

        /*
         * Build additional data for AEAD encryption.
         * This depends on the TLS version.
         */
        ssl_extract_add_data_from_record(add_data, &add_data_len, rec,
                                         transform->tls_version,
                                         transform->taglen);
        MBEDTLS_SSL_DEBUG_BUF(4, "additional data used for AEAD",
                              add_data, add_data_len);

        /* Because of the check above, we know that there are
         * explicit_iv_len Bytes preceding data, and taglen
         * bytes following data + data_len. This justifies
         * the debug message and the invocation of
         * mbedtls_cipher_auth_decrypt_ext() below. */

        MBEDTLS_SSL_DEBUG_BUF(4, "IV used", iv, transform->ivlen);
        MBEDTLS_SSL_DEBUG_BUF(4, "TAG used", data + rec->data_len,
                              transform->taglen);

        /*
         * Decrypt and authenticate
         */
#if defined(MBEDTLS_USE_PSA_CRYPTO)
        status = psa_aead_decrypt(transform->psa_key_dec,
                                  transform->psa_alg,
                                  iv, transform->ivlen,
                                  add_data, add_data_len,
                                  data, rec->data_len + transform->taglen,
                                  data, rec->buf_len - (data - rec->buf),
                                  &olen);

        if (status != PSA_SUCCESS) {
            ret = PSA_TO_MBEDTLS_ERR(status);
            MBEDTLS_SSL_DEBUG_RET(1, "psa_aead_decrypt", ret);
            return ret;
        }
#else
        if ((ret = mbedtls_cipher_auth_decrypt_ext
                       (&transform->cipher_ctx_dec,
                       iv, transform->ivlen,
                       add_data, add_data_len,
                       data, rec->data_len + transform->taglen, /* src */
                       data, rec->buf_len - (size_t) (data - rec->buf), &olen, /* dst */
                       transform->taglen)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_cipher_auth_decrypt_ext", ret);

            if (ret == MBEDTLS_ERR_CIPHER_AUTH_FAILED) {
                return MBEDTLS_ERR_SSL_INVALID_MAC;
            }

            return ret;
        }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

        auth_done++;

        /* Double-check that AEAD decryption doesn't change content length. */
        if (olen != rec->data_len) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }
    } else
#endif /* MBEDTLS_SSL_HAVE_AEAD */
#if defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC)
    if (ssl_mode == MBEDTLS_SSL_MODE_CBC ||
        ssl_mode == MBEDTLS_SSL_MODE_CBC_ETM) {
        size_t minlen = 0;
#if defined(MBEDTLS_USE_PSA_CRYPTO)
        psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
        size_t part_len;
        psa_cipher_operation_t cipher_op = PSA_CIPHER_OPERATION_INIT;
#endif /* MBEDTLS_USE_PSA_CRYPTO */

        /*
         * Check immediate ciphertext sanity
         */
#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
        /* The ciphertext is prefixed with the CBC IV. */
        minlen += transform->ivlen;
#endif

        /* Size considerations:
         *
         * - The CBC cipher text must not be empty and hence
         *   at least of size transform->ivlen.
         *
         * Together with the potential IV-prefix, this explains
         * the first of the two checks below.
         *
         * - The record must contain a MAC, either in plain or
         *   encrypted, depending on whether Encrypt-then-MAC
         *   is used or not.
         *   - If it is, the message contains the IV-prefix,
         *     the CBC ciphertext, and the MAC.
         *   - If it is not, the padded plaintext, and hence
         *     the CBC ciphertext, has at least length maclen + 1
         *     because there is at least the padding length byte.
         *
         * As the CBC ciphertext is not empty, both cases give the
         * lower bound minlen + maclen + 1 on the record size, which
         * we test for in the second check below.
         */
        if (rec->data_len < minlen + transform->ivlen ||
            rec->data_len < minlen + transform->maclen + 1) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("msglen (%" MBEDTLS_PRINTF_SIZET
                                      ") < max( ivlen(%" MBEDTLS_PRINTF_SIZET
                                      "), maclen (%" MBEDTLS_PRINTF_SIZET ") "
                                                                          "+ 1 ) ( + expl IV )",
                                      rec->data_len,
                                      transform->ivlen,
                                      transform->maclen));
            return MBEDTLS_ERR_SSL_INVALID_MAC;
        }

        /*
         * Authenticate before decrypt if enabled
         */
#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
        if (ssl_mode == MBEDTLS_SSL_MODE_CBC_ETM) {
#if defined(MBEDTLS_USE_PSA_CRYPTO)
            psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;
#else
            unsigned char mac_expect[MBEDTLS_SSL_MAC_ADD];
#endif /* MBEDTLS_USE_PSA_CRYPTO */

            MBEDTLS_SSL_DEBUG_MSG(3, ("using encrypt then mac"));

            /* Update data_len in tandem with add_data.
             *
             * The subtraction is safe because of the previous check
             * data_len >= minlen + maclen + 1.
             *
             * Afterwards, we know that data + data_len is followed by at
             * least maclen Bytes, which justifies the call to
             * mbedtls_ct_memcmp() below.
             *
             * Further, we still know that data_len > minlen */
            rec->data_len -= transform->maclen;
            ssl_extract_add_data_from_record(add_data, &add_data_len, rec,
                                             transform->tls_version,
                                             transform->taglen);

            /* Calculate expected MAC. */
            MBEDTLS_SSL_DEBUG_BUF(4, "MAC'd meta-data", add_data,
                                  add_data_len);
#if defined(MBEDTLS_USE_PSA_CRYPTO)
            status = psa_mac_verify_setup(&operation, transform->psa_mac_dec,
                                          transform->psa_mac_alg);
            if (status != PSA_SUCCESS) {
                goto hmac_failed_etm_enabled;
            }

            status = psa_mac_update(&operation, add_data, add_data_len);
            if (status != PSA_SUCCESS) {
                goto hmac_failed_etm_enabled;
            }

            status = psa_mac_update(&operation, data, rec->data_len);
            if (status != PSA_SUCCESS) {
                goto hmac_failed_etm_enabled;
            }

            /* Compare expected MAC with MAC at the end of the record. */
            status = psa_mac_verify_finish(&operation, data + rec->data_len,
                                           transform->maclen);
            if (status != PSA_SUCCESS) {
                goto hmac_failed_etm_enabled;
            }
#else
            ret = mbedtls_md_hmac_update(&transform->md_ctx_dec, add_data,
                                         add_data_len);
            if (ret != 0) {
                goto hmac_failed_etm_enabled;
            }
            ret = mbedtls_md_hmac_update(&transform->md_ctx_dec,
                                         data, rec->data_len);
            if (ret != 0) {
                goto hmac_failed_etm_enabled;
            }
            ret = mbedtls_md_hmac_finish(&transform->md_ctx_dec, mac_expect);
            if (ret != 0) {
                goto hmac_failed_etm_enabled;
            }
            ret = mbedtls_md_hmac_reset(&transform->md_ctx_dec);
            if (ret != 0) {
                goto hmac_failed_etm_enabled;
            }

            MBEDTLS_SSL_DEBUG_BUF(4, "message  mac", data + rec->data_len,
                                  transform->maclen);
            MBEDTLS_SSL_DEBUG_BUF(4, "expected mac", mac_expect,
                                  transform->maclen);

            /* Compare expected MAC with MAC at the end of the record. */
            if (mbedtls_ct_memcmp(data + rec->data_len, mac_expect,
                                  transform->maclen) != 0) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("message mac does not match"));
                ret = MBEDTLS_ERR_SSL_INVALID_MAC;
                goto hmac_failed_etm_enabled;
            }
#endif /* MBEDTLS_USE_PSA_CRYPTO */
            auth_done++;

hmac_failed_etm_enabled:
#if defined(MBEDTLS_USE_PSA_CRYPTO)
            ret = PSA_TO_MBEDTLS_ERR(status);
            status = psa_mac_abort(&operation);
            if (ret == 0 && status != PSA_SUCCESS) {
                ret = PSA_TO_MBEDTLS_ERR(status);
            }
#else
            mbedtls_platform_zeroize(mac_expect, transform->maclen);
#endif /* MBEDTLS_USE_PSA_CRYPTO */
            if (ret != 0) {
                if (ret != MBEDTLS_ERR_SSL_INVALID_MAC) {
                    MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_hmac_xxx", ret);
                }
                return ret;
            }
        }
#endif /* MBEDTLS_SSL_ENCRYPT_THEN_MAC */

        /*
         * Check length sanity
         */

        /* We know from above that data_len > minlen >= 0,
         * so the following check in particular implies that
         * data_len >= minlen + ivlen ( = minlen or 2 * minlen ). */
        if (rec->data_len % transform->ivlen != 0) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("msglen (%" MBEDTLS_PRINTF_SIZET
                                      ") %% ivlen (%" MBEDTLS_PRINTF_SIZET ") != 0",
                                      rec->data_len, transform->ivlen));
            return MBEDTLS_ERR_SSL_INVALID_MAC;
        }

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
        /*
         * Initialize for prepended IV for block cipher in TLS v1.2
         */
        /* Safe because data_len >= minlen + ivlen = 2 * ivlen. */
        memcpy(transform->iv_dec, data, transform->ivlen);

        data += transform->ivlen;
        rec->data_offset += transform->ivlen;
        rec->data_len -= transform->ivlen;
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

        /* We still have data_len % ivlen == 0 and data_len >= ivlen here. */

#if defined(MBEDTLS_USE_PSA_CRYPTO)
        status = psa_cipher_decrypt_setup(&cipher_op,
                                          transform->psa_key_dec, transform->psa_alg);

        if (status != PSA_SUCCESS) {
            ret = PSA_TO_MBEDTLS_ERR(status);
            MBEDTLS_SSL_DEBUG_RET(1, "psa_cipher_decrypt_setup", ret);
            return ret;
        }

        status = psa_cipher_set_iv(&cipher_op, transform->iv_dec, transform->ivlen);

        if (status != PSA_SUCCESS) {
            ret = PSA_TO_MBEDTLS_ERR(status);
            MBEDTLS_SSL_DEBUG_RET(1, "psa_cipher_set_iv", ret);
            return ret;
        }

        status = psa_cipher_update(&cipher_op,
                                   data, rec->data_len,
                                   data, rec->data_len, &olen);

        if (status != PSA_SUCCESS) {
            ret = PSA_TO_MBEDTLS_ERR(status);
            MBEDTLS_SSL_DEBUG_RET(1, "psa_cipher_update", ret);
            return ret;
        }

        status = psa_cipher_finish(&cipher_op,
                                   data + olen, rec->data_len - olen,
                                   &part_len);

        if (status != PSA_SUCCESS) {
            ret = PSA_TO_MBEDTLS_ERR(status);
            MBEDTLS_SSL_DEBUG_RET(1, "psa_cipher_finish", ret);
            return ret;
        }

        olen += part_len;
#else

        if ((ret = mbedtls_cipher_crypt(&transform->cipher_ctx_dec,
                                        transform->iv_dec, transform->ivlen,
                                        data, rec->data_len, data, &olen)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_cipher_crypt", ret);
            return ret;
        }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

        /* Double-check that length hasn't changed during decryption. */
        if (rec->data_len != olen) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        /* Safe since data_len >= minlen + maclen + 1, so after having
         * subtracted at most minlen and maclen up to this point,
         * data_len > 0 (because of data_len % ivlen == 0, it's actually
         * >= ivlen ). */
        padlen = data[rec->data_len - 1];

        if (auth_done == 1) {
            const mbedtls_ct_condition_t ge = mbedtls_ct_uint_ge(
                rec->data_len,
                padlen + 1);
            correct = mbedtls_ct_bool_and(ge, correct);
            padlen  = mbedtls_ct_size_if_else_0(ge, padlen);
        } else {
#if defined(MBEDTLS_SSL_DEBUG_ALL)
            if (rec->data_len < transform->maclen + padlen + 1) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("msglen (%" MBEDTLS_PRINTF_SIZET
                                          ") < maclen (%" MBEDTLS_PRINTF_SIZET
                                          ") + padlen (%" MBEDTLS_PRINTF_SIZET ")",
                                          rec->data_len,
                                          transform->maclen,
                                          padlen + 1));
            }
#endif
            const mbedtls_ct_condition_t ge = mbedtls_ct_uint_ge(
                rec->data_len,
                transform->maclen + padlen + 1);
            correct = mbedtls_ct_bool_and(ge, correct);
            padlen  = mbedtls_ct_size_if_else_0(ge, padlen);
        }

        padlen++;

        /* Regardless of the validity of the padding,
         * we have data_len >= padlen here. */

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
        /* The padding check involves a series of up to 256
         * consecutive memory reads at the end of the record
         * plaintext buffer. In order to hide the length and
         * validity of the padding, always perform exactly
         * `min(256,plaintext_len)` reads (but take into account
         * only the last `padlen` bytes for the padding check). */
        size_t pad_count = 0;
        volatile unsigned char * const check = data;

        /* Index of first padding byte; it has been ensured above
         * that the subtraction is safe. */
        size_t const padding_idx = rec->data_len - padlen;
        size_t const num_checks = rec->data_len <= 256 ? rec->data_len : 256;
        size_t const start_idx = rec->data_len - num_checks;
        size_t idx;

        for (idx = start_idx; idx < rec->data_len; idx++) {
            /* pad_count += (idx >= padding_idx) &&
             *              (check[idx] == padlen - 1);
             */
            const mbedtls_ct_condition_t a = mbedtls_ct_uint_ge(idx, padding_idx);
            size_t increment = mbedtls_ct_size_if_else_0(a, 1);
            const mbedtls_ct_condition_t b = mbedtls_ct_uint_eq(check[idx], padlen - 1);
            increment = mbedtls_ct_size_if_else_0(b, increment);
            pad_count += increment;
        }
        correct = mbedtls_ct_bool_and(mbedtls_ct_uint_eq(pad_count, padlen), correct);

#if defined(MBEDTLS_SSL_DEBUG_ALL)
        if (padlen > 0 && correct == MBEDTLS_CT_FALSE) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad padding byte detected"));
        }
#endif
        padlen = mbedtls_ct_size_if_else_0(correct, padlen);

#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

        /* If the padding was found to be invalid, padlen == 0
         * and the subtraction is safe. If the padding was found valid,
         * padlen hasn't been changed and the previous assertion
         * data_len >= padlen still holds. */
        rec->data_len -= padlen;
    } else
#endif /* MBEDTLS_SSL_SOME_SUITES_USE_CBC */
    {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

#if defined(MBEDTLS_SSL_DEBUG_ALL)
    MBEDTLS_SSL_DEBUG_BUF(4, "raw buffer after decryption",
                          data, rec->data_len);
#endif

    /*
     * Authenticate if not done yet.
     * Compute the MAC regardless of the padding result (RFC4346, CBCTIME).
     */
#if defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)
    if (auth_done == 0) {
        unsigned char mac_expect[MBEDTLS_SSL_MAC_ADD] = { 0 };
        unsigned char mac_peer[MBEDTLS_SSL_MAC_ADD] = { 0 };

        /* For CBC+MAC, If the initial value of padlen was such that
         * data_len < maclen + padlen + 1, then padlen
         * got reset to 1, and the initial check
         * data_len >= minlen + maclen + 1
         * guarantees that at this point we still
         * have at least data_len >= maclen.
         *
         * If the initial value of padlen was such that
         * data_len >= maclen + padlen + 1, then we have
         * subtracted either padlen + 1 (if the padding was correct)
         * or 0 (if the padding was incorrect) since then,
         * hence data_len >= maclen in any case.
         *
         * For stream ciphers, we checked above that
         * data_len >= maclen.
         */
        rec->data_len -= transform->maclen;
        ssl_extract_add_data_from_record(add_data, &add_data_len, rec,
                                         transform->tls_version,
                                         transform->taglen);

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
        /*
         * The next two sizes are the minimum and maximum values of
         * data_len over all padlen values.
         *
         * They're independent of padlen, since we previously did
         * data_len -= padlen.
         *
         * Note that max_len + maclen is never more than the buffer
         * length, as we previously did in_msglen -= maclen too.
         */
        const size_t max_len = rec->data_len + padlen;
        const size_t min_len = (max_len > 256) ? max_len - 256 : 0;

#if defined(MBEDTLS_USE_PSA_CRYPTO)
        ret = mbedtls_ct_hmac(transform->psa_mac_dec,
                              transform->psa_mac_alg,
                              add_data, add_data_len,
                              data, rec->data_len, min_len, max_len,
                              mac_expect);
#else
        ret = mbedtls_ct_hmac(&transform->md_ctx_dec,
                              add_data, add_data_len,
                              data, rec->data_len, min_len, max_len,
                              mac_expect);
#endif /* MBEDTLS_USE_PSA_CRYPTO */
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ct_hmac", ret);
            goto hmac_failed_etm_disabled;
        }

        mbedtls_ct_memcpy_offset(mac_peer, data,
                                 rec->data_len,
                                 min_len, max_len,
                                 transform->maclen);
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

#if defined(MBEDTLS_SSL_DEBUG_ALL)
        MBEDTLS_SSL_DEBUG_BUF(4, "expected mac", mac_expect, transform->maclen);
        MBEDTLS_SSL_DEBUG_BUF(4, "message  mac", mac_peer, transform->maclen);
#endif

        if (mbedtls_ct_memcmp(mac_peer, mac_expect,
                              transform->maclen) != 0) {
#if defined(MBEDTLS_SSL_DEBUG_ALL)
            MBEDTLS_SSL_DEBUG_MSG(1, ("message mac does not match"));
#endif
            correct = MBEDTLS_CT_FALSE;
        }
        auth_done++;

hmac_failed_etm_disabled:
        mbedtls_platform_zeroize(mac_peer, transform->maclen);
        mbedtls_platform_zeroize(mac_expect, transform->maclen);
        if (ret != 0) {
            return ret;
        }
    }

    /*
     * Finally check the correct flag
     */
    if (correct == MBEDTLS_CT_FALSE) {
        return MBEDTLS_ERR_SSL_INVALID_MAC;
    }
#endif /* MBEDTLS_SSL_SOME_SUITES_USE_MAC */

    /* Make extra sure authentication was performed, exactly once */
    if (auth_done != 1) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    if (transform->tls_version == MBEDTLS_SSL_VERSION_TLS1_3) {
        /* Remove inner padding and infer true content type. */
        ret = ssl_parse_inner_plaintext(data, &rec->data_len,
                                        &rec->type);

        if (ret != 0) {
            return MBEDTLS_ERR_SSL_INVALID_RECORD;
        }
    }
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    if (rec->cid_len != 0) {
        ret = ssl_parse_inner_plaintext(data, &rec->data_len,
                                        &rec->type);
        if (ret != 0) {
            return MBEDTLS_ERR_SSL_INVALID_RECORD;
        }
    }
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= decrypt buf"));

    return 0;
}

#undef MAC_NONE
#undef MAC_PLAINTEXT
#undef MAC_CIPHERTEXT

/*
 * Fill the input message buffer by appending data to it.
 * The amount of data already fetched is in ssl->in_left.
 *
 * If we return 0, is it guaranteed that (at least) nb_want bytes are
 * available (from this read and/or a previous one). Otherwise, an error code
 * is returned (possibly EOF or WANT_READ).
 *
 * With stream transport (TLS) on success ssl->in_left == nb_want, but
 * with datagram transport (DTLS) on success ssl->in_left >= nb_want,
 * since we always read a whole datagram at once.
 *
 * For DTLS, it is up to the caller to set ssl->next_record_offset when
 * they're done reading a record.
 */
int mbedtls_ssl_fetch_input(mbedtls_ssl_context *ssl, size_t nb_want)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;
#if defined(MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
    size_t in_buf_len = ssl->in_buf_len;
#else
    size_t in_buf_len = MBEDTLS_SSL_IN_BUFFER_LEN;
#endif

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> fetch input"));

    if (ssl->f_recv == NULL && ssl->f_recv_timeout == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("Bad usage of mbedtls_ssl_set_bio() "));
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    if (nb_want > in_buf_len - (size_t) (ssl->in_hdr - ssl->in_buf)) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("requesting more data than fits"));
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        uint32_t timeout;

        /*
         * The point is, we need to always read a full datagram at once, so we
         * sometimes read more then requested, and handle the additional data.
         * It could be the rest of the current record (while fetching the
         * header) and/or some other records in the same datagram.
         */

        /*
         * Move to the next record in the already read datagram if applicable
         */
        if (ssl->next_record_offset != 0) {
            if (ssl->in_left < ssl->next_record_offset) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
                return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
            }

            ssl->in_left -= ssl->next_record_offset;

            if (ssl->in_left != 0) {
                MBEDTLS_SSL_DEBUG_MSG(2, ("next record in same datagram, offset: %"
                                          MBEDTLS_PRINTF_SIZET,
                                          ssl->next_record_offset));
                memmove(ssl->in_hdr,
                        ssl->in_hdr + ssl->next_record_offset,
                        ssl->in_left);
            }

            ssl->next_record_offset = 0;
        }

        MBEDTLS_SSL_DEBUG_MSG(2, ("in_left: %" MBEDTLS_PRINTF_SIZET
                                  ", nb_want: %" MBEDTLS_PRINTF_SIZET,
                                  ssl->in_left, nb_want));

        /*
         * Done if we already have enough data.
         */
        if (nb_want <= ssl->in_left) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("<= fetch input"));
            return 0;
        }

        /*
         * A record can't be split across datagrams. If we need to read but
         * are not at the beginning of a new record, the caller did something
         * wrong.
         */
        if (ssl->in_left != 0) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        /*
         * Don't even try to read if time's out already.
         * This avoids by-passing the timer when repeatedly receiving messages
         * that will end up being dropped.
         */
        if (mbedtls_ssl_check_timer(ssl) != 0) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("timer has expired"));
            ret = MBEDTLS_ERR_SSL_TIMEOUT;
        } else {
            len = in_buf_len - (size_t) (ssl->in_hdr - ssl->in_buf);

            if (mbedtls_ssl_is_handshake_over(ssl) == 0) {
                timeout = ssl->handshake->retransmit_timeout;
            } else {
                timeout = ssl->conf->read_timeout;
            }

            MBEDTLS_SSL_DEBUG_MSG(3, ("f_recv_timeout: %lu ms", (unsigned long) timeout));

            if (ssl->f_recv_timeout != NULL) {
                ret = ssl->f_recv_timeout(ssl->p_bio, ssl->in_hdr, len,
                                          timeout);
            } else {
                ret = ssl->f_recv(ssl->p_bio, ssl->in_hdr, len);
            }

            MBEDTLS_SSL_DEBUG_RET(2, "ssl->f_recv(_timeout)", ret);

            if (ret == 0) {
                return MBEDTLS_ERR_SSL_CONN_EOF;
            }
        }

        if (ret == MBEDTLS_ERR_SSL_TIMEOUT) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("timeout"));
            mbedtls_ssl_set_timer(ssl, 0);

            if (ssl->state != MBEDTLS_SSL_HANDSHAKE_OVER) {
                if (ssl_double_retransmit_timeout(ssl) != 0) {
                    MBEDTLS_SSL_DEBUG_MSG(1, ("handshake timeout"));
                    return MBEDTLS_ERR_SSL_TIMEOUT;
                }

                if ((ret = mbedtls_ssl_resend(ssl)) != 0) {
                    MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_resend", ret);
                    return ret;
                }

                return MBEDTLS_ERR_SSL_WANT_READ;
            }
#if defined(MBEDTLS_SSL_SRV_C) && defined(MBEDTLS_SSL_RENEGOTIATION)
            else if (ssl->conf->endpoint == MBEDTLS_SSL_IS_SERVER &&
                     ssl->renego_status == MBEDTLS_SSL_RENEGOTIATION_PENDING) {
                if ((ret = mbedtls_ssl_resend_hello_request(ssl)) != 0) {
                    MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_resend_hello_request",
                                          ret);
                    return ret;
                }

                return MBEDTLS_ERR_SSL_WANT_READ;
            }
#endif /* MBEDTLS_SSL_SRV_C && MBEDTLS_SSL_RENEGOTIATION */
        }

        if (ret < 0) {
            return ret;
        }

        ssl->in_left = ret;
    } else
#endif
    {
        MBEDTLS_SSL_DEBUG_MSG(2, ("in_left: %" MBEDTLS_PRINTF_SIZET
                                  ", nb_want: %" MBEDTLS_PRINTF_SIZET,
                                  ssl->in_left, nb_want));

        while (ssl->in_left < nb_want) {
            len = nb_want - ssl->in_left;

            if (mbedtls_ssl_check_timer(ssl) != 0) {
                ret = MBEDTLS_ERR_SSL_TIMEOUT;
            } else {
                if (ssl->f_recv_timeout != NULL) {
                    ret = ssl->f_recv_timeout(ssl->p_bio,
                                              ssl->in_hdr + ssl->in_left, len,
                                              ssl->conf->read_timeout);
                } else {
                    ret = ssl->f_recv(ssl->p_bio,
                                      ssl->in_hdr + ssl->in_left, len);
                }
            }

            MBEDTLS_SSL_DEBUG_MSG(2, ("in_left: %" MBEDTLS_PRINTF_SIZET
                                      ", nb_want: %" MBEDTLS_PRINTF_SIZET,
                                      ssl->in_left, nb_want));
            MBEDTLS_SSL_DEBUG_RET(2, "ssl->f_recv(_timeout)", ret);

            if (ret == 0) {
                return MBEDTLS_ERR_SSL_CONN_EOF;
            }

            if (ret < 0) {
                return ret;
            }

            if ((size_t) ret > len) {
                MBEDTLS_SSL_DEBUG_MSG(1,
                                      ("f_recv returned %d bytes but only %" MBEDTLS_PRINTF_SIZET
                                       " were requested",
                                       ret, len));
                return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
            }

            ssl->in_left += ret;
        }
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= fetch input"));

    return 0;
}

/*
 * Flush any data not yet written
 */
int mbedtls_ssl_flush_output(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *buf;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> flush output"));

    if (ssl->f_send == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("Bad usage of mbedtls_ssl_set_bio() "));
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    /* Avoid incrementing counter if data is flushed */
    if (ssl->out_left == 0) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("<= flush output"));
        return 0;
    }

    while (ssl->out_left > 0) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("message length: %" MBEDTLS_PRINTF_SIZET
                                  ", out_left: %" MBEDTLS_PRINTF_SIZET,
                                  mbedtls_ssl_out_hdr_len(ssl) + ssl->out_msglen, ssl->out_left));

        buf = ssl->out_hdr - ssl->out_left;
        ret = ssl->f_send(ssl->p_bio, buf, ssl->out_left);

        MBEDTLS_SSL_DEBUG_RET(2, "ssl->f_send", ret);

        if (ret <= 0) {
            return ret;
        }

        if ((size_t) ret > ssl->out_left) {
            MBEDTLS_SSL_DEBUG_MSG(1,
                                  ("f_send returned %d bytes but only %" MBEDTLS_PRINTF_SIZET
                                   " bytes were sent",
                                   ret, ssl->out_left));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        ssl->out_left -= ret;
    }

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        ssl->out_hdr = ssl->out_buf;
    } else
#endif
    {
        ssl->out_hdr = ssl->out_buf + 8;
    }
    mbedtls_ssl_update_out_pointers(ssl, ssl->transform_out);

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= flush output"));

    return 0;
}

/*
 * Functions to handle the DTLS retransmission state machine
 */
#if defined(MBEDTLS_SSL_PROTO_DTLS)
/*
 * Append current handshake message to current outgoing flight
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_flight_append(mbedtls_ssl_context *ssl)
{
    mbedtls_ssl_flight_item *msg;
    MBEDTLS_SSL_DEBUG_MSG(2, ("=> ssl_flight_append"));
    MBEDTLS_SSL_DEBUG_BUF(4, "message appended to flight",
                          ssl->out_msg, ssl->out_msglen);

    /* Allocate space for current message */
    if ((msg = mbedtls_calloc(1, sizeof(mbedtls_ssl_flight_item))) == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("alloc %" MBEDTLS_PRINTF_SIZET " bytes failed",
                                  sizeof(mbedtls_ssl_flight_item)));
        return MBEDTLS_ERR_SSL_ALLOC_FAILED;
    }

    if ((msg->p = mbedtls_calloc(1, ssl->out_msglen)) == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("alloc %" MBEDTLS_PRINTF_SIZET " bytes failed",
                                  ssl->out_msglen));
        mbedtls_free(msg);
        return MBEDTLS_ERR_SSL_ALLOC_FAILED;
    }

    /* Copy current handshake message with headers */
    memcpy(msg->p, ssl->out_msg, ssl->out_msglen);
    msg->len = ssl->out_msglen;
    msg->type = ssl->out_msgtype;
    msg->next = NULL;

    /* Append to the current flight */
    if (ssl->handshake->flight == NULL) {
        ssl->handshake->flight = msg;
    } else {
        mbedtls_ssl_flight_item *cur = ssl->handshake->flight;
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = msg;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= ssl_flight_append"));
    return 0;
}

/*
 * Free the current flight of handshake messages
 */
void mbedtls_ssl_flight_free(mbedtls_ssl_flight_item *flight)
{
    mbedtls_ssl_flight_item *cur = flight;
    mbedtls_ssl_flight_item *next;

    while (cur != NULL) {
        next = cur->next;

        mbedtls_free(cur->p);
        mbedtls_free(cur);

        cur = next;
    }
}

/*
 * Swap transform_out and out_ctr with the alternative ones
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_swap_epochs(mbedtls_ssl_context *ssl)
{
    mbedtls_ssl_transform *tmp_transform;
    unsigned char tmp_out_ctr[MBEDTLS_SSL_SEQUENCE_NUMBER_LEN];

    if (ssl->transform_out == ssl->handshake->alt_transform_out) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("skip swap epochs"));
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(3, ("swap epochs"));

    /* Swap transforms */
    tmp_transform                     = ssl->transform_out;
    ssl->transform_out                = ssl->handshake->alt_transform_out;
    ssl->handshake->alt_transform_out = tmp_transform;

    /* Swap epoch + sequence_number */
    memcpy(tmp_out_ctr, ssl->cur_out_ctr, sizeof(tmp_out_ctr));
    memcpy(ssl->cur_out_ctr, ssl->handshake->alt_out_ctr,
           sizeof(ssl->cur_out_ctr));
    memcpy(ssl->handshake->alt_out_ctr, tmp_out_ctr,
           sizeof(ssl->handshake->alt_out_ctr));

    /* Adjust to the newly activated transform */
    mbedtls_ssl_update_out_pointers(ssl, ssl->transform_out);

    return 0;
}

/*
 * Retransmit the current flight of messages.
 */
int mbedtls_ssl_resend(mbedtls_ssl_context *ssl)
{
    int ret = 0;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> mbedtls_ssl_resend"));

    ret = mbedtls_ssl_flight_transmit(ssl);

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= mbedtls_ssl_resend"));

    return ret;
}

/*
 * Transmit or retransmit the current flight of messages.
 *
 * Need to remember the current message in case flush_output returns
 * WANT_WRITE, causing us to exit this function and come back later.
 * This function must be called until state is no longer SENDING.
 */
int mbedtls_ssl_flight_transmit(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    MBEDTLS_SSL_DEBUG_MSG(2, ("=> mbedtls_ssl_flight_transmit"));

    if (ssl->handshake->retransmit_state != MBEDTLS_SSL_RETRANS_SENDING) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("initialise flight transmission"));

        ssl->handshake->cur_msg = ssl->handshake->flight;
        ssl->handshake->cur_msg_p = ssl->handshake->flight->p + 12;
        ret = ssl_swap_epochs(ssl);
        if (ret != 0) {
            return ret;
        }

        ssl->handshake->retransmit_state = MBEDTLS_SSL_RETRANS_SENDING;
    }

    while (ssl->handshake->cur_msg != NULL) {
        size_t max_frag_len;
        const mbedtls_ssl_flight_item * const cur = ssl->handshake->cur_msg;

        int const is_finished =
            (cur->type == MBEDTLS_SSL_MSG_HANDSHAKE &&
             cur->p[0] == MBEDTLS_SSL_HS_FINISHED);

        int const force_flush = ssl->disable_datagram_packing == 1 ?
                                SSL_FORCE_FLUSH : SSL_DONT_FORCE_FLUSH;

        /* Swap epochs before sending Finished: we can't do it after
         * sending ChangeCipherSpec, in case write returns WANT_READ.
         * Must be done before copying, may change out_msg pointer */
        if (is_finished && ssl->handshake->cur_msg_p == (cur->p + 12)) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("swap epochs to send finished message"));
            ret = ssl_swap_epochs(ssl);
            if (ret != 0) {
                return ret;
            }
        }

        ret = ssl_get_remaining_payload_in_datagram(ssl);
        if (ret < 0) {
            return ret;
        }
        max_frag_len = (size_t) ret;

        /* CCS is copied as is, while HS messages may need fragmentation */
        if (cur->type == MBEDTLS_SSL_MSG_CHANGE_CIPHER_SPEC) {
            if (max_frag_len == 0) {
                if ((ret = mbedtls_ssl_flush_output(ssl)) != 0) {
                    return ret;
                }

                continue;
            }

            memcpy(ssl->out_msg, cur->p, cur->len);
            ssl->out_msglen  = cur->len;
            ssl->out_msgtype = cur->type;

            /* Update position inside current message */
            ssl->handshake->cur_msg_p += cur->len;
        } else {
            const unsigned char * const p = ssl->handshake->cur_msg_p;
            const size_t hs_len = cur->len - 12;
            const size_t frag_off = (size_t) (p - (cur->p + 12));
            const size_t rem_len = hs_len - frag_off;
            size_t cur_hs_frag_len, max_hs_frag_len;

            if ((max_frag_len < 12) || (max_frag_len == 12 && hs_len != 0)) {
                if (is_finished) {
                    ret = ssl_swap_epochs(ssl);
                    if (ret != 0) {
                        return ret;
                    }
                }

                if ((ret = mbedtls_ssl_flush_output(ssl)) != 0) {
                    return ret;
                }

                continue;
            }
            max_hs_frag_len = max_frag_len - 12;

            cur_hs_frag_len = rem_len > max_hs_frag_len ?
                              max_hs_frag_len : rem_len;

            if (frag_off == 0 && cur_hs_frag_len != hs_len) {
                MBEDTLS_SSL_DEBUG_MSG(2, ("fragmenting handshake message (%u > %u)",
                                          (unsigned) cur_hs_frag_len,
                                          (unsigned) max_hs_frag_len));
            }

            /* Messages are stored with handshake headers as if not fragmented,
             * copy beginning of headers then fill fragmentation fields.
             * Handshake headers: type(1) len(3) seq(2) f_off(3) f_len(3) */
            memcpy(ssl->out_msg, cur->p, 6);

            ssl->out_msg[6] = MBEDTLS_BYTE_2(frag_off);
            ssl->out_msg[7] = MBEDTLS_BYTE_1(frag_off);
            ssl->out_msg[8] = MBEDTLS_BYTE_0(frag_off);

            ssl->out_msg[9] = MBEDTLS_BYTE_2(cur_hs_frag_len);
            ssl->out_msg[10] = MBEDTLS_BYTE_1(cur_hs_frag_len);
            ssl->out_msg[11] = MBEDTLS_BYTE_0(cur_hs_frag_len);

            MBEDTLS_SSL_DEBUG_BUF(3, "handshake header", ssl->out_msg, 12);

            /* Copy the handshake message content and set records fields */
            memcpy(ssl->out_msg + 12, p, cur_hs_frag_len);
            ssl->out_msglen = cur_hs_frag_len + 12;
            ssl->out_msgtype = cur->type;

            /* Update position inside current message */
            ssl->handshake->cur_msg_p += cur_hs_frag_len;
        }

        /* If done with the current message move to the next one if any */
        if (ssl->handshake->cur_msg_p >= cur->p + cur->len) {
            if (cur->next != NULL) {
                ssl->handshake->cur_msg = cur->next;
                ssl->handshake->cur_msg_p = cur->next->p + 12;
            } else {
                ssl->handshake->cur_msg = NULL;
                ssl->handshake->cur_msg_p = NULL;
            }
        }

        /* Actually send the message out */
        if ((ret = mbedtls_ssl_write_record(ssl, force_flush)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_write_record", ret);
            return ret;
        }
    }

    if ((ret = mbedtls_ssl_flush_output(ssl)) != 0) {
        return ret;
    }

    /* Update state and set timer */
    if (mbedtls_ssl_is_handshake_over(ssl) == 1) {
        ssl->handshake->retransmit_state = MBEDTLS_SSL_RETRANS_FINISHED;
    } else {
        ssl->handshake->retransmit_state = MBEDTLS_SSL_RETRANS_WAITING;
        mbedtls_ssl_set_timer(ssl, ssl->handshake->retransmit_timeout);
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= mbedtls_ssl_flight_transmit"));

    return 0;
}

/*
 * To be called when the last message of an incoming flight is received.
 */
void mbedtls_ssl_recv_flight_completed(mbedtls_ssl_context *ssl)
{
    /* We won't need to resend that one any more */
    mbedtls_ssl_flight_free(ssl->handshake->flight);
    ssl->handshake->flight = NULL;
    ssl->handshake->cur_msg = NULL;

    /* The next incoming flight will start with this msg_seq */
    ssl->handshake->in_flight_start_seq = ssl->handshake->in_msg_seq;

    /* We don't want to remember CCS's across flight boundaries. */
    ssl->handshake->buffering.seen_ccs = 0;

    /* Clear future message buffering structure. */
    mbedtls_ssl_buffering_free(ssl);

    /* Cancel timer */
    mbedtls_ssl_set_timer(ssl, 0);

    if (ssl->in_msgtype == MBEDTLS_SSL_MSG_HANDSHAKE &&
        ssl->in_msg[0] == MBEDTLS_SSL_HS_FINISHED) {
        ssl->handshake->retransmit_state = MBEDTLS_SSL_RETRANS_FINISHED;
    } else {
        ssl->handshake->retransmit_state = MBEDTLS_SSL_RETRANS_PREPARING;
    }
}

/*
 * To be called when the last message of an outgoing flight is send.
 */
void mbedtls_ssl_send_flight_completed(mbedtls_ssl_context *ssl)
{
    ssl_reset_retransmit_timeout(ssl);
    mbedtls_ssl_set_timer(ssl, ssl->handshake->retransmit_timeout);

    if (ssl->in_msgtype == MBEDTLS_SSL_MSG_HANDSHAKE &&
        ssl->in_msg[0] == MBEDTLS_SSL_HS_FINISHED) {
        ssl->handshake->retransmit_state = MBEDTLS_SSL_RETRANS_FINISHED;
    } else {
        ssl->handshake->retransmit_state = MBEDTLS_SSL_RETRANS_WAITING;
    }
}
#endif /* MBEDTLS_SSL_PROTO_DTLS */

/*
 * Handshake layer functions
 */
int mbedtls_ssl_start_handshake_msg(mbedtls_ssl_context *ssl, unsigned char hs_type,
                                    unsigned char **buf, size_t *buf_len)
{
    /*
     * Reserve 4 bytes for handshake header. ( Section 4,RFC 8446 )
     *    ...
     *    HandshakeType msg_type;
     *    uint24 length;
     *    ...
     */
    *buf = ssl->out_msg + 4;
    *buf_len = MBEDTLS_SSL_OUT_CONTENT_LEN - 4;

    ssl->out_msgtype = MBEDTLS_SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = hs_type;

    return 0;
}

/*
 * Write (DTLS: or queue) current handshake (including CCS) message.
 *
 *  - fill in handshake headers
 *  - update handshake checksum
 *  - DTLS: save message for resending
 *  - then pass to the record layer
 *
 * DTLS: except for HelloRequest, messages are only queued, and will only be
 * actually sent when calling flight_transmit() or resend().
 *
 * Inputs:
 *  - ssl->out_msglen: 4 + actual handshake message len
 *      (4 is the size of handshake headers for TLS)
 *  - ssl->out_msg[0]: the handshake type (ClientHello, ServerHello, etc)
 *  - ssl->out_msg + 4: the handshake message body
 *
 * Outputs, ie state before passing to flight_append() or write_record():
 *   - ssl->out_msglen: the length of the record contents
 *      (including handshake headers but excluding record headers)
 *   - ssl->out_msg: the record contents (handshake headers + content)
 */
int mbedtls_ssl_write_handshake_msg_ext(mbedtls_ssl_context *ssl,
                                        int update_checksum,
                                        int force_flush)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const size_t hs_len = ssl->out_msglen - 4;
    const unsigned char hs_type = ssl->out_msg[0];

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write handshake message"));

    /*
     * Sanity checks
     */
    if (ssl->out_msgtype != MBEDTLS_SSL_MSG_HANDSHAKE          &&
        ssl->out_msgtype != MBEDTLS_SSL_MSG_CHANGE_CIPHER_SPEC) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    /* Whenever we send anything different from a
     * HelloRequest we should be in a handshake - double check. */
    if (!(ssl->out_msgtype == MBEDTLS_SSL_MSG_HANDSHAKE &&
          hs_type          == MBEDTLS_SSL_HS_HELLO_REQUEST) &&
        ssl->handshake == NULL) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM &&
        ssl->handshake != NULL &&
        ssl->handshake->retransmit_state == MBEDTLS_SSL_RETRANS_SENDING) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
#endif

    /* Double-check that we did not exceed the bounds
     * of the outgoing record buffer.
     * This should never fail as the various message
     * writing functions must obey the bounds of the
     * outgoing record buffer, but better be safe.
     *
     * Note: We deliberately do not check for the MTU or MFL here.
     */
    if (ssl->out_msglen > MBEDTLS_SSL_OUT_CONTENT_LEN) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("Record too large: "
                                  "size %" MBEDTLS_PRINTF_SIZET
                                  ", maximum %" MBEDTLS_PRINTF_SIZET,
                                  ssl->out_msglen,
                                  (size_t) MBEDTLS_SSL_OUT_CONTENT_LEN));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    /*
     * Fill handshake headers
     */
    if (ssl->out_msgtype == MBEDTLS_SSL_MSG_HANDSHAKE) {
        ssl->out_msg[1] = MBEDTLS_BYTE_2(hs_len);
        ssl->out_msg[2] = MBEDTLS_BYTE_1(hs_len);
        ssl->out_msg[3] = MBEDTLS_BYTE_0(hs_len);

        /*
         * DTLS has additional fields in the Handshake layer,
         * between the length field and the actual payload:
         *      uint16 message_seq;
         *      uint24 fragment_offset;
         *      uint24 fragment_length;
         */
#if defined(MBEDTLS_SSL_PROTO_DTLS)
        if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
            /* Make room for the additional DTLS fields */
            if (MBEDTLS_SSL_OUT_CONTENT_LEN - ssl->out_msglen < 8) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("DTLS handshake message too large: "
                                          "size %" MBEDTLS_PRINTF_SIZET ", maximum %"
                                          MBEDTLS_PRINTF_SIZET,
                                          hs_len,
                                          (size_t) (MBEDTLS_SSL_OUT_CONTENT_LEN - 12)));
                return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
            }

            memmove(ssl->out_msg + 12, ssl->out_msg + 4, hs_len);
            ssl->out_msglen += 8;

            /* Write message_seq and update it, except for HelloRequest */
            if (hs_type != MBEDTLS_SSL_HS_HELLO_REQUEST) {
                MBEDTLS_PUT_UINT16_BE(ssl->handshake->out_msg_seq, ssl->out_msg, 4);
                ++(ssl->handshake->out_msg_seq);
            } else {
                ssl->out_msg[4] = 0;
                ssl->out_msg[5] = 0;
            }

            /* Handshake hashes are computed without fragmentation,
             * so set frag_offset = 0 and frag_len = hs_len for now */
            memset(ssl->out_msg + 6, 0x00, 3);
            memcpy(ssl->out_msg + 9, ssl->out_msg + 1, 3);
        }
#endif /* MBEDTLS_SSL_PROTO_DTLS */

        /* Update running hashes of handshake messages seen */
        if (hs_type != MBEDTLS_SSL_HS_HELLO_REQUEST && update_checksum != 0) {
            ret = ssl->handshake->update_checksum(ssl, ssl->out_msg,
                                                  ssl->out_msglen);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "update_checksum", ret);
                return ret;
            }
        }
    }

    /* Either send now, or just save to be sent (and resent) later */
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM &&
        !(ssl->out_msgtype == MBEDTLS_SSL_MSG_HANDSHAKE &&
          hs_type          == MBEDTLS_SSL_HS_HELLO_REQUEST)) {
        if ((ret = ssl_flight_append(ssl)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "ssl_flight_append", ret);
            return ret;
        }
    } else
#endif
    {
        if ((ret = mbedtls_ssl_write_record(ssl, force_flush)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "ssl_write_record", ret);
            return ret;
        }
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write handshake message"));

    return 0;
}

int mbedtls_ssl_finish_handshake_msg(mbedtls_ssl_context *ssl,
                                     size_t buf_len, size_t msg_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t msg_with_header_len;
    ((void) buf_len);

    /* Add reserved 4 bytes for handshake header */
    msg_with_header_len = msg_len + 4;
    ssl->out_msglen = msg_with_header_len;
    MBEDTLS_SSL_PROC_CHK(mbedtls_ssl_write_handshake_msg_ext(ssl, 0, 0));

cleanup:
    return ret;
}

/*
 * Record layer functions
 */

/*
 * Write current record.
 *
 * Uses:
 *  - ssl->out_msgtype: type of the message (AppData, Handshake, Alert, CCS)
 *  - ssl->out_msglen: length of the record content (excl headers)
 *  - ssl->out_msg: record content
 */
int mbedtls_ssl_write_record(mbedtls_ssl_context *ssl, int force_flush)
{
    int ret, done = 0;
    size_t len = ssl->out_msglen;
    int flush = force_flush;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write record"));

    if (!done) {
        unsigned i;
        size_t protected_record_size;
#if defined(MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
        size_t out_buf_len = ssl->out_buf_len;
#else
        size_t out_buf_len = MBEDTLS_SSL_OUT_BUFFER_LEN;
#endif
        /* Skip writing the record content type to after the encryption,
         * as it may change when using the CID extension. */
        mbedtls_ssl_protocol_version tls_ver = ssl->tls_version;
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
        /* TLS 1.3 still uses the TLS 1.2 version identifier
         * for backwards compatibility. */
        if (tls_ver == MBEDTLS_SSL_VERSION_TLS1_3) {
            tls_ver = MBEDTLS_SSL_VERSION_TLS1_2;
        }
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */
        mbedtls_ssl_write_version(ssl->out_hdr + 1, ssl->conf->transport,
                                  tls_ver);

        memcpy(ssl->out_ctr, ssl->cur_out_ctr, MBEDTLS_SSL_SEQUENCE_NUMBER_LEN);
        MBEDTLS_PUT_UINT16_BE(len, ssl->out_len, 0);

        if (ssl->transform_out != NULL) {
            mbedtls_record rec;

            rec.buf         = ssl->out_iv;
            rec.buf_len     = out_buf_len - (size_t) (ssl->out_iv - ssl->out_buf);
            rec.data_len    = ssl->out_msglen;
            rec.data_offset = (size_t) (ssl->out_msg - rec.buf);

            memcpy(&rec.ctr[0], ssl->out_ctr, sizeof(rec.ctr));
            mbedtls_ssl_write_version(rec.ver, ssl->conf->transport, tls_ver);
            rec.type = ssl->out_msgtype;

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
            /* The CID is set by mbedtls_ssl_encrypt_buf(). */
            rec.cid_len = 0;
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

            if ((ret = mbedtls_ssl_encrypt_buf(ssl, ssl->transform_out, &rec,
                                               ssl->conf->f_rng, ssl->conf->p_rng)) != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "ssl_encrypt_buf", ret);
                return ret;
            }

            if (rec.data_offset != 0) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
                return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
            }

            /* Update the record content type and CID. */
            ssl->out_msgtype = rec.type;
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
            memcpy(ssl->out_cid, rec.cid, rec.cid_len);
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
            ssl->out_msglen = len = rec.data_len;
            MBEDTLS_PUT_UINT16_BE(rec.data_len, ssl->out_len, 0);
        }

        protected_record_size = len + mbedtls_ssl_out_hdr_len(ssl);

#if defined(MBEDTLS_SSL_PROTO_DTLS)
        /* In case of DTLS, double-check that we don't exceed
         * the remaining space in the datagram. */
        if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
            ret = ssl_get_remaining_space_in_datagram(ssl);
            if (ret < 0) {
                return ret;
            }

            if (protected_record_size > (size_t) ret) {
                /* Should never happen */
                return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
            }
        }
#endif /* MBEDTLS_SSL_PROTO_DTLS */

        /* Now write the potentially updated record content type. */
        ssl->out_hdr[0] = (unsigned char) ssl->out_msgtype;

        MBEDTLS_SSL_DEBUG_MSG(3, ("output record: msgtype = %u, "
                                  "version = [%u:%u], msglen = %" MBEDTLS_PRINTF_SIZET,
                                  ssl->out_hdr[0], ssl->out_hdr[1],
                                  ssl->out_hdr[2], len));

        MBEDTLS_SSL_DEBUG_BUF(4, "output record sent to network",
                              ssl->out_hdr, protected_record_size);

        ssl->out_left += protected_record_size;
        ssl->out_hdr  += protected_record_size;
        mbedtls_ssl_update_out_pointers(ssl, ssl->transform_out);

        for (i = 8; i > mbedtls_ssl_ep_len(ssl); i--) {
            if (++ssl->cur_out_ctr[i - 1] != 0) {
                break;
            }
        }

        /* The loop goes to its end if the counter is wrapping */
        if (i == mbedtls_ssl_ep_len(ssl)) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("outgoing message counter would wrap"));
            return MBEDTLS_ERR_SSL_COUNTER_WRAPPING;
        }
    }

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM &&
        flush == SSL_DONT_FORCE_FLUSH) {
        size_t remaining;
        ret = ssl_get_remaining_payload_in_datagram(ssl);
        if (ret < 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "ssl_get_remaining_payload_in_datagram",
                                  ret);
            return ret;
        }

        remaining = (size_t) ret;
        if (remaining == 0) {
            flush = SSL_FORCE_FLUSH;
        } else {
            MBEDTLS_SSL_DEBUG_MSG(2,
                                  ("Still %u bytes available in current datagram",
                                   (unsigned) remaining));
        }
    }
#endif /* MBEDTLS_SSL_PROTO_DTLS */

    if ((flush == SSL_FORCE_FLUSH) &&
        (ret = mbedtls_ssl_flush_output(ssl)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_flush_output", ret);
        return ret;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write record"));

    return 0;
}

#if defined(MBEDTLS_SSL_PROTO_DTLS)

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_hs_is_proper_fragment(mbedtls_ssl_context *ssl)
{
    if (ssl->in_msglen < ssl->in_hslen ||
        memcmp(ssl->in_msg + 6, "\0\0\0",        3) != 0 ||
        memcmp(ssl->in_msg + 9, ssl->in_msg + 1, 3) != 0) {
        return 1;
    }
    return 0;
}

static uint32_t ssl_get_hs_frag_len(mbedtls_ssl_context const *ssl)
{
    return MBEDTLS_GET_UINT24_BE(ssl->in_msg, 9);
}

static uint32_t ssl_get_hs_frag_off(mbedtls_ssl_context const *ssl)
{
    return MBEDTLS_GET_UINT24_BE(ssl->in_msg, 6);
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_check_hs_header(mbedtls_ssl_context const *ssl)
{
    uint32_t msg_len, frag_off, frag_len;

    msg_len  = ssl_get_hs_total_len(ssl);
    frag_off = ssl_get_hs_frag_off(ssl);
    frag_len = ssl_get_hs_frag_len(ssl);

    if (frag_off > msg_len) {
        return -1;
    }

    if (frag_len > msg_len - frag_off) {
        return -1;
    }

    if (frag_len + 12 > ssl->in_msglen) {
        return -1;
    }

    return 0;
}

/*
 * Mark bits in bitmask (used for DTLS HS reassembly)
 */
static void ssl_bitmask_set(unsigned char *mask, size_t offset, size_t len)
{
    unsigned int start_bits, end_bits;

    start_bits = 8 - (offset % 8);
    if (start_bits != 8) {
        size_t first_byte_idx = offset / 8;

        /* Special case */
        if (len <= start_bits) {
            for (; len != 0; len--) {
                mask[first_byte_idx] |= 1 << (start_bits - len);
            }

            /* Avoid potential issues with offset or len becoming invalid */
            return;
        }

        offset += start_bits; /* Now offset % 8 == 0 */
        len -= start_bits;

        for (; start_bits != 0; start_bits--) {
            mask[first_byte_idx] |= 1 << (start_bits - 1);
        }
    }

    end_bits = len % 8;
    if (end_bits != 0) {
        size_t last_byte_idx = (offset + len) / 8;

        len -= end_bits; /* Now len % 8 == 0 */

        for (; end_bits != 0; end_bits--) {
            mask[last_byte_idx] |= 1 << (8 - end_bits);
        }
    }

    memset(mask + offset / 8, 0xFF, len / 8);
}

/*
 * Check that bitmask is full
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_bitmask_check(unsigned char *mask, size_t len)
{
    size_t i;

    for (i = 0; i < len / 8; i++) {
        if (mask[i] != 0xFF) {
            return -1;
        }
    }

    for (i = 0; i < len % 8; i++) {
        if ((mask[len / 8] & (1 << (7 - i))) == 0) {
            return -1;
        }
    }

    return 0;
}

/* msg_len does not include the handshake header */
static size_t ssl_get_reassembly_buffer_size(size_t msg_len,
                                             unsigned add_bitmap)
{
    size_t alloc_len;

    alloc_len  = 12;                                 /* Handshake header */
    alloc_len += msg_len;                            /* Content buffer   */

    if (add_bitmap) {
        alloc_len += msg_len / 8 + (msg_len % 8 != 0);   /* Bitmap       */

    }
    return alloc_len;
}

#endif /* MBEDTLS_SSL_PROTO_DTLS */

static uint32_t ssl_get_hs_total_len(mbedtls_ssl_context const *ssl)
{
    return MBEDTLS_GET_UINT24_BE(ssl->in_msg, 1);
}

int mbedtls_ssl_prepare_handshake_record(mbedtls_ssl_context *ssl)
{
    if (ssl->badmac_seen_or_in_hsfraglen == 0) {
        /* The handshake message must at least include the header.
         * We may not have the full message yet in case of fragmentation.
         * To simplify the code, we insist on having the header (and in
         * particular the handshake message length) in the first
         * fragment. */
        if (ssl->in_msglen < mbedtls_ssl_hs_hdr_len(ssl)) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("handshake message too short: %" MBEDTLS_PRINTF_SIZET,
                                      ssl->in_msglen));
            return MBEDTLS_ERR_SSL_INVALID_RECORD;
        }

        ssl->in_hslen = mbedtls_ssl_hs_hdr_len(ssl) + ssl_get_hs_total_len(ssl);
    }

    MBEDTLS_SSL_DEBUG_MSG(3, ("handshake message: msglen ="
                              " %" MBEDTLS_PRINTF_SIZET ", type = %u, hslen = %"
                              MBEDTLS_PRINTF_SIZET,
                              ssl->in_msglen, ssl->in_msg[0], ssl->in_hslen));

    if (ssl->transform_in != NULL) {
        MBEDTLS_SSL_DEBUG_MSG(4, ("decrypted handshake message:"
                                  " iv-buf=%d hdr-buf=%d hdr-buf=%d",
                                  (int) (ssl->in_iv - ssl->in_buf),
                                  (int) (ssl->in_hdr - ssl->in_buf),
                                  (int) (ssl->in_msg - ssl->in_buf)));
    }

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
        unsigned int recv_msg_seq = MBEDTLS_GET_UINT16_BE(ssl->in_msg, 4);

        if (ssl_check_hs_header(ssl) != 0) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("invalid handshake header"));
            return MBEDTLS_ERR_SSL_INVALID_RECORD;
        }

        if (ssl->handshake != NULL &&
            ((mbedtls_ssl_is_handshake_over(ssl) == 0 &&
              recv_msg_seq != ssl->handshake->in_msg_seq) ||
             (mbedtls_ssl_is_handshake_over(ssl) == 1 &&
              ssl->in_msg[0] != MBEDTLS_SSL_HS_CLIENT_HELLO))) {
            if (recv_msg_seq > ssl->handshake->in_msg_seq) {
                MBEDTLS_SSL_DEBUG_MSG(2,
                                      (
                                          "received future handshake message of sequence number %u (next %u)",
                                          recv_msg_seq,
                                          ssl->handshake->in_msg_seq));
                return MBEDTLS_ERR_SSL_EARLY_MESSAGE;
            }

            /* Retransmit only on last message from previous flight, to avoid
             * too many retransmissions.
             * Besides, No sane server ever retransmits HelloVerifyRequest */
            if (recv_msg_seq == ssl->handshake->in_flight_start_seq - 1 &&
                ssl->in_msg[0] != MBEDTLS_SSL_HS_HELLO_VERIFY_REQUEST) {
                MBEDTLS_SSL_DEBUG_MSG(2, ("received message from last flight, "
                                          "message_seq = %u, start_of_flight = %u",
                                          recv_msg_seq,
                                          ssl->handshake->in_flight_start_seq));

                if ((ret = mbedtls_ssl_resend(ssl)) != 0) {
                    MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_resend", ret);
                    return ret;
                }
            } else {
                MBEDTLS_SSL_DEBUG_MSG(2, ("dropping out-of-sequence message: "
                                          "message_seq = %u, expected = %u",
                                          recv_msg_seq,
                                          ssl->handshake->in_msg_seq));
            }

            return MBEDTLS_ERR_SSL_CONTINUE_PROCESSING;
        }
        /* Wait until message completion to increment in_msg_seq */

        /* Message reassembly is handled alongside buffering of future
         * messages; the commonality is that both handshake fragments and
         * future messages cannot be forwarded immediately to the
         * handshake logic layer. */
        if (ssl_hs_is_proper_fragment(ssl) == 1) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("found fragmented DTLS handshake message"));
            return MBEDTLS_ERR_SSL_EARLY_MESSAGE;
        }
    } else
#endif /* MBEDTLS_SSL_PROTO_DTLS */
    {
        unsigned char *const reassembled_record_start =
            ssl->in_buf + MBEDTLS_SSL_SEQUENCE_NUMBER_LEN;
        unsigned char *const payload_start =
            reassembled_record_start + mbedtls_ssl_in_hdr_len(ssl);
        unsigned char *payload_end = payload_start + ssl->badmac_seen_or_in_hsfraglen;
        /* How many more bytes we want to have a complete handshake message. */
        const size_t hs_remain = ssl->in_hslen - ssl->badmac_seen_or_in_hsfraglen;
        /* How many bytes of the current record are part of the first
         * handshake message. There may be more handshake messages (possibly
         * incomplete) in the same record; if so, we leave them after the
         * current record, and ssl_consume_current_message() will take
         * care of consuming the next handshake message. */
        const size_t hs_this_fragment_len =
            ssl->in_msglen > hs_remain ? hs_remain : ssl->in_msglen;
        (void) hs_this_fragment_len;

        MBEDTLS_SSL_DEBUG_MSG(3,
                              ("%s handshake fragment: %" MBEDTLS_PRINTF_SIZET
                               ", %u..%u of %" MBEDTLS_PRINTF_SIZET,
                               (ssl->badmac_seen_or_in_hsfraglen != 0 ?
                                "subsequent" :
                                hs_this_fragment_len == ssl->in_hslen ?
                                "sole" :
                                "initial"),
                               ssl->in_msglen,
                               ssl->badmac_seen_or_in_hsfraglen,
                               ssl->badmac_seen_or_in_hsfraglen +
                               (unsigned) hs_this_fragment_len,
                               ssl->in_hslen));

        /* Move the received handshake fragment to have the whole message
         * (at least the part received so far) in a single segment at a
         * known offset in the input buffer.
         * - When receiving a non-initial handshake fragment, append it to
         *   the initial segment.
         * - Even the initial handshake fragment is moved, if it was
         *   encrypted with an explicit IV: decryption leaves the payload
         *   after the explicit IV, but here we move it to start where the
         *   IV was.
         */
#if defined(MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
        size_t const in_buf_len = ssl->in_buf_len;
#else
        size_t const in_buf_len = MBEDTLS_SSL_IN_BUFFER_LEN;
#endif
        if (payload_end + ssl->in_msglen > ssl->in_buf + in_buf_len) {
            MBEDTLS_SSL_DEBUG_MSG(1,
                                  ("Shouldn't happen: no room to move handshake fragment %"
                                   MBEDTLS_PRINTF_SIZET " from %p to %p (buf=%p len=%"
                                   MBEDTLS_PRINTF_SIZET ")",
                                   ssl->in_msglen,
                                   (void *) ssl->in_msg, (void *) payload_end,
                                   (void *) ssl->in_buf, in_buf_len));
            return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
        }
        memmove(payload_end, ssl->in_msg, ssl->in_msglen);

        ssl->badmac_seen_or_in_hsfraglen += (unsigned) ssl->in_msglen;
        payload_end += ssl->in_msglen;

        if (ssl->badmac_seen_or_in_hsfraglen < ssl->in_hslen) {
            MBEDTLS_SSL_DEBUG_MSG(3, ("Prepare: waiting for more handshake fragments "
                                      "%u/%" MBEDTLS_PRINTF_SIZET,
                                      ssl->badmac_seen_or_in_hsfraglen, ssl->in_hslen));
            ssl->in_hdr = payload_end;
            ssl->in_msglen = 0;
            mbedtls_ssl_update_in_pointers(ssl);
            return MBEDTLS_ERR_SSL_CONTINUE_PROCESSING;
        } else {
            ssl->in_msglen = ssl->badmac_seen_or_in_hsfraglen;
            ssl->badmac_seen_or_in_hsfraglen = 0;
            ssl->in_hdr = reassembled_record_start;
            mbedtls_ssl_update_in_pointers(ssl);

            /* Update the record length in the fully reassembled record */
            if (ssl->in_msglen > 0xffff) {
                MBEDTLS_SSL_DEBUG_MSG(1,
                                      ("Shouldn't happen: in_msglen=%"
                                       MBEDTLS_PRINTF_SIZET " > 0xffff",
                                       ssl->in_msglen));
                return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
            }
            MBEDTLS_PUT_UINT16_BE(ssl->in_msglen, ssl->in_len, 0);

            size_t record_len = mbedtls_ssl_in_hdr_len(ssl) + ssl->in_msglen;
            (void) record_len;
            MBEDTLS_SSL_DEBUG_BUF(4, "reassembled record",
                                  ssl->in_hdr, record_len);
            if (ssl->in_hslen < ssl->in_msglen) {
                MBEDTLS_SSL_DEBUG_MSG(3,
                                      ("More handshake messages in the record: "
                                       "%" MBEDTLS_PRINTF_SIZET " + %" MBEDTLS_PRINTF_SIZET,
                                       ssl->in_hslen,
                                       ssl->in_msglen - ssl->in_hslen));
            }
        }
    }

    return 0;
}

int mbedtls_ssl_update_handshake_status(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ssl_handshake_params * const hs = ssl->handshake;

    if (mbedtls_ssl_is_handshake_over(ssl) == 0 && hs != NULL) {
        ret = ssl->handshake->update_checksum(ssl, ssl->in_msg, ssl->in_hslen);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "update_checksum", ret);
            return ret;
        }
    }

    /* Handshake message is complete, increment counter */
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM &&
        ssl->handshake != NULL) {
        unsigned offset;
        mbedtls_ssl_hs_buffer *hs_buf;

        /* Increment handshake sequence number */
        hs->in_msg_seq++;

        /*
         * Clear up handshake buffering and reassembly structure.
         */

        /* Free first entry */
        ssl_buffering_free_slot(ssl, 0);

        /* Shift all other entries */
        for (offset = 0, hs_buf = &hs->buffering.hs[0];
             offset + 1 < MBEDTLS_SSL_MAX_BUFFERED_HS;
             offset++, hs_buf++) {
            *hs_buf = *(hs_buf + 1);
        }

        /* Create a fresh last entry */
        memset(hs_buf, 0, sizeof(mbedtls_ssl_hs_buffer));
    }
#endif
    return 0;
}

/*
 * DTLS anti-replay: RFC 6347 4.1.2.6
 *
 * in_window is a field of bits numbered from 0 (lsb) to 63 (msb).
 * Bit n is set iff record number in_window_top - n has been seen.
 *
 * Usually, in_window_top is the last record number seen and the lsb of
 * in_window is set. The only exception is the initial state (record number 0
 * not seen yet).
 */
#if defined(MBEDTLS_SSL_DTLS_ANTI_REPLAY)
void mbedtls_ssl_dtls_replay_reset(mbedtls_ssl_context *ssl)
{
    ssl->in_window_top = 0;
    ssl->in_window = 0;
}

static inline uint64_t ssl_load_six_bytes(unsigned char *buf)
{
    return ((uint64_t) buf[0] << 40) |
           ((uint64_t) buf[1] << 32) |
           ((uint64_t) buf[2] << 24) |
           ((uint64_t) buf[3] << 16) |
           ((uint64_t) buf[4] <<  8) |
           ((uint64_t) buf[5]);
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int mbedtls_ssl_dtls_record_replay_check(mbedtls_ssl_context *ssl, uint8_t *record_in_ctr)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *original_in_ctr;

    // save original in_ctr
    original_in_ctr = ssl->in_ctr;

    // use counter from record
    ssl->in_ctr = record_in_ctr;

    ret = mbedtls_ssl_dtls_replay_check((mbedtls_ssl_context const *) ssl);

    // restore the counter
    ssl->in_ctr = original_in_ctr;

    return ret;
}

/*
 * Return 0 if sequence number is acceptable, -1 otherwise
 */
int mbedtls_ssl_dtls_replay_check(mbedtls_ssl_context const *ssl)
{
    uint64_t rec_seqnum = ssl_load_six_bytes(ssl->in_ctr + 2);
    uint64_t bit;

    if (ssl->conf->anti_replay == MBEDTLS_SSL_ANTI_REPLAY_DISABLED) {
        return 0;
    }

    if (rec_seqnum > ssl->in_window_top) {
        return 0;
    }

    bit = ssl->in_window_top - rec_seqnum;

    if (bit >= 64) {
        return -1;
    }

    if ((ssl->in_window & ((uint64_t) 1 << bit)) != 0) {
        return -1;
    }

    return 0;
}

/*
 * Update replay window on new validated record
 */
void mbedtls_ssl_dtls_replay_update(mbedtls_ssl_context *ssl)
{
    uint64_t rec_seqnum = ssl_load_six_bytes(ssl->in_ctr + 2);

    if (ssl->conf->anti_replay == MBEDTLS_SSL_ANTI_REPLAY_DISABLED) {
        return;
    }

    if (rec_seqnum > ssl->in_window_top) {
        /* Update window_top and the contents of the window */
        uint64_t shift = rec_seqnum - ssl->in_window_top;

        if (shift >= 64) {
            ssl->in_window = 1;
        } else {
            ssl->in_window <<= shift;
            ssl->in_window |= 1;
        }

        ssl->in_window_top = rec_seqnum;
    } else {
        /* Mark that number as seen in the current window */
        uint64_t bit = ssl->in_window_top - rec_seqnum;

        if (bit < 64) { /* Always true, but be extra sure */
            ssl->in_window |= (uint64_t) 1 << bit;
        }
    }
}
#endif /* MBEDTLS_SSL_DTLS_ANTI_REPLAY */

#if defined(MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE) && defined(MBEDTLS_SSL_SRV_C)
/*
 * Check if a datagram looks like a ClientHello with a valid cookie,
 * and if it doesn't, generate a HelloVerifyRequest message.
 * Both input and output include full DTLS headers.
 *
 * - if cookie is valid, return 0
 * - if ClientHello looks superficially valid but cookie is not,
 *   fill obuf and set olen, then
 *   return MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED
 * - otherwise return a specific error code
 */
MBEDTLS_CHECK_RETURN_CRITICAL
MBEDTLS_STATIC_TESTABLE
int mbedtls_ssl_check_dtls_clihlo_cookie(
    mbedtls_ssl_context *ssl,
    const unsigned char *cli_id, size_t cli_id_len,
    const unsigned char *in, size_t in_len,
    unsigned char *obuf, size_t buf_len, size_t *olen)
{
    size_t sid_len, cookie_len, epoch, fragment_offset;
    unsigned char *p;

    /*
     * Structure of ClientHello with record and handshake headers,
     * and expected values. We don't need to check a lot, more checks will be
     * done when actually parsing the ClientHello - skipping those checks
     * avoids code duplication and does not make cookie forging any easier.
     *
     *  0-0  ContentType type;                  copied, must be handshake
     *  1-2  ProtocolVersion version;           copied
     *  3-4  uint16 epoch;                      copied, must be 0
     *  5-10 uint48 sequence_number;            copied
     * 11-12 uint16 length;                     (ignored)
     *
     * 13-13 HandshakeType msg_type;            (ignored)
     * 14-16 uint24 length;                     (ignored)
     * 17-18 uint16 message_seq;                copied
     * 19-21 uint24 fragment_offset;            copied, must be 0
     * 22-24 uint24 fragment_length;            (ignored)
     *
     * 25-26 ProtocolVersion client_version;    (ignored)
     * 27-58 Random random;                     (ignored)
     * 59-xx SessionID session_id;              1 byte len + sid_len content
     * 60+   opaque cookie<0..2^8-1>;           1 byte len + content
     *       ...
     *
     * Minimum length is 61 bytes.
     */
    MBEDTLS_SSL_DEBUG_MSG(4, ("check cookie: in_len=%u",
                              (unsigned) in_len));
    MBEDTLS_SSL_DEBUG_BUF(4, "cli_id", cli_id, cli_id_len);
    if (in_len < 61) {
        MBEDTLS_SSL_DEBUG_MSG(4, ("check cookie: record too short"));
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    epoch = MBEDTLS_GET_UINT16_BE(in, 3);
    fragment_offset = MBEDTLS_GET_UINT24_BE(in, 19);

    if (in[0] != MBEDTLS_SSL_MSG_HANDSHAKE || epoch != 0 ||
        fragment_offset != 0) {
        MBEDTLS_SSL_DEBUG_MSG(4, ("check cookie: not a good ClientHello"));
        MBEDTLS_SSL_DEBUG_MSG(4, ("    type=%u epoch=%u fragment_offset=%u",
                                  in[0], (unsigned) epoch,
                                  (unsigned) fragment_offset));
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    sid_len = in[59];
    if (59 + 1 + sid_len + 1 > in_len) {
        MBEDTLS_SSL_DEBUG_MSG(4, ("check cookie: sid_len=%u > %u",
                                  (unsigned) sid_len,
                                  (unsigned) in_len - 61));
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }
    MBEDTLS_SSL_DEBUG_BUF(4, "sid received from network",
                          in + 60, sid_len);

    cookie_len = in[60 + sid_len];
    if (59 + 1 + sid_len + 1 + cookie_len > in_len) {
        MBEDTLS_SSL_DEBUG_MSG(4, ("check cookie: cookie_len=%u > %u",
                                  (unsigned) cookie_len,
                                  (unsigned) (in_len - sid_len - 61)));
        return MBEDTLS_ERR_SSL_DECODE_ERROR;
    }

    MBEDTLS_SSL_DEBUG_BUF(4, "cookie received from network",
                          in + sid_len + 61, cookie_len);
    if (ssl->conf->f_cookie_check(ssl->conf->p_cookie,
                                  in + sid_len + 61, cookie_len,
                                  cli_id, cli_id_len) == 0) {
        MBEDTLS_SSL_DEBUG_MSG(4, ("check cookie: valid"));
        return 0;
    }

    /*
     * If we get here, we've got an invalid cookie, let's prepare HVR.
     *
     *  0-0  ContentType type;                  copied
     *  1-2  ProtocolVersion version;           copied
     *  3-4  uint16 epoch;                      copied
     *  5-10 uint48 sequence_number;            copied
     * 11-12 uint16 length;                     olen - 13
     *
     * 13-13 HandshakeType msg_type;            hello_verify_request
     * 14-16 uint24 length;                     olen - 25
     * 17-18 uint16 message_seq;                copied
     * 19-21 uint24 fragment_offset;            copied
     * 22-24 uint24 fragment_length;            olen - 25
     *
     * 25-26 ProtocolVersion server_version;    0xfe 0xff
     * 27-27 opaque cookie<0..2^8-1>;           cookie_len = olen - 27, cookie
     *
     * Minimum length is 28.
     */
    if (buf_len < 28) {
        return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
    }

    /* Copy most fields and adapt others */
    memcpy(obuf, in, 25);
    obuf[13] = MBEDTLS_SSL_HS_HELLO_VERIFY_REQUEST;
    obuf[25] = 0xfe;
    obuf[26] = 0xff;

    /* Generate and write actual cookie */
    p = obuf + 28;
    if (ssl->conf->f_cookie_write(ssl->conf->p_cookie,
                                  &p, obuf + buf_len,
                                  cli_id, cli_id_len) != 0) {
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    *olen = (size_t) (p - obuf);

    /* Go back and fill length fields */
    obuf[27] = (unsigned char) (*olen - 28);

    obuf[14] = obuf[22] = MBEDTLS_BYTE_2(*olen - 25);
    obuf[15] = obuf[23] = MBEDTLS_BYTE_1(*olen - 25);
    obuf[16] = obuf[24] = MBEDTLS_BYTE_0(*olen - 25);

    MBEDTLS_PUT_UINT16_BE(*olen - 13, obuf, 11);

    return MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED;
}

/*
 * Handle possible client reconnect with the same UDP quadruplet
 * (RFC 6347 Section 4.2.8).
 *
 * Called by ssl_parse_record_header() in case we receive an epoch 0 record
 * that looks like a ClientHello.
 *
 * - if the input looks like a ClientHello without cookies,
 *   send back HelloVerifyRequest, then return 0
 * - if the input looks like a ClientHello with a valid cookie,
 *   reset the session of the current context, and
 *   return MBEDTLS_ERR_SSL_CLIENT_RECONNECT
 * - if anything goes wrong, return a specific error code
 *
 * This function is called (through ssl_check_client_reconnect()) when an
 * unexpected record is found in ssl_get_next_record(), which will discard the
 * record if we return 0, and bubble up the return value otherwise (this
 * includes the case of MBEDTLS_ERR_SSL_CLIENT_RECONNECT and of unexpected
 * errors, and is the right thing to do in both cases).
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_handle_possible_reconnect(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len = 0;

    if (ssl->conf->f_cookie_write == NULL ||
        ssl->conf->f_cookie_check == NULL) {
        /* If we can't use cookies to verify reachability of the peer,
         * drop the record. */
        MBEDTLS_SSL_DEBUG_MSG(1, ("no cookie callbacks, "
                                  "can't check reconnect validity"));
        return 0;
    }

    ret = mbedtls_ssl_check_dtls_clihlo_cookie(
        ssl,
        ssl->cli_id, ssl->cli_id_len,
        ssl->in_buf, ssl->in_left,
        ssl->out_buf, MBEDTLS_SSL_OUT_CONTENT_LEN, &len);

    MBEDTLS_SSL_DEBUG_RET(2, "mbedtls_ssl_check_dtls_clihlo_cookie", ret);

    if (ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
        int send_ret;
        MBEDTLS_SSL_DEBUG_MSG(1, ("sending HelloVerifyRequest"));
        MBEDTLS_SSL_DEBUG_BUF(4, "output record sent to network",
                              ssl->out_buf, len);
        /* Don't check write errors as we can't do anything here.
         * If the error is permanent we'll catch it later,
         * if it's not, then hopefully it'll work next time. */
        send_ret = ssl->f_send(ssl->p_bio, ssl->out_buf, len);
        MBEDTLS_SSL_DEBUG_RET(2, "ssl->f_send", send_ret);
        (void) send_ret;

        return 0;
    }

    if (ret == 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("cookie is valid, resetting context"));
        if ((ret = mbedtls_ssl_session_reset_int(ssl, 1)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "reset", ret);
            return ret;
        }

        return MBEDTLS_ERR_SSL_CLIENT_RECONNECT;
    }

    return ret;
}
#endif /* MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE && MBEDTLS_SSL_SRV_C */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_check_record_type(uint8_t record_type)
{
    if (record_type != MBEDTLS_SSL_MSG_HANDSHAKE &&
        record_type != MBEDTLS_SSL_MSG_ALERT &&
        record_type != MBEDTLS_SSL_MSG_CHANGE_CIPHER_SPEC &&
        record_type != MBEDTLS_SSL_MSG_APPLICATION_DATA) {
        return MBEDTLS_ERR_SSL_INVALID_RECORD;
    }

    return 0;
}

/*
 * ContentType type;
 * ProtocolVersion version;
 * uint16 epoch;            // DTLS only
 * uint48 sequence_number;  // DTLS only
 * uint16 length;
 *
 * Return 0 if header looks sane (and, for DTLS, the record is expected)
 * MBEDTLS_ERR_SSL_INVALID_RECORD if the header looks bad,
 * MBEDTLS_ERR_SSL_UNEXPECTED_RECORD (DTLS only) if sane but unexpected.
 *
 * With DTLS, mbedtls_ssl_read_record() will:
 * 1. proceed with the record if this function returns 0
 * 2. drop only the current record if this function returns UNEXPECTED_RECORD
 * 3. return CLIENT_RECONNECT if this function return that value
 * 4. drop the whole datagram if this function returns anything else.
 * Point 2 is needed when the peer is resending, and we have already received
 * the first record from a datagram but are still waiting for the others.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_parse_record_header(mbedtls_ssl_context const *ssl,
                                   unsigned char *buf,
                                   size_t len,
                                   mbedtls_record *rec)
{
    mbedtls_ssl_protocol_version tls_version;

    size_t const rec_hdr_type_offset    = 0;
    size_t const rec_hdr_type_len       = 1;

    size_t const rec_hdr_version_offset = rec_hdr_type_offset +
                                          rec_hdr_type_len;
    size_t const rec_hdr_version_len    = 2;

    size_t const rec_hdr_ctr_len        = 8;
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    uint32_t     rec_epoch;
    size_t const rec_hdr_ctr_offset     = rec_hdr_version_offset +
                                          rec_hdr_version_len;

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    size_t const rec_hdr_cid_offset     = rec_hdr_ctr_offset +
                                          rec_hdr_ctr_len;
    size_t       rec_hdr_cid_len        = 0;
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
#endif /* MBEDTLS_SSL_PROTO_DTLS */

    size_t       rec_hdr_len_offset; /* To be determined */
    size_t const rec_hdr_len_len    = 2;

    /*
     * Check minimum lengths for record header.
     */

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        rec_hdr_len_offset = rec_hdr_ctr_offset + rec_hdr_ctr_len;
    } else
#endif /* MBEDTLS_SSL_PROTO_DTLS */
    {
        rec_hdr_len_offset = rec_hdr_version_offset + rec_hdr_version_len;
    }

    if (len < rec_hdr_len_offset + rec_hdr_len_len) {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              (
                                  "datagram of length %u too small to hold DTLS record header of length %u",
                                  (unsigned) len,
                                  (unsigned) (rec_hdr_len_len + rec_hdr_len_len)));
        return MBEDTLS_ERR_SSL_INVALID_RECORD;
    }

    /*
     * Parse and validate record content type
     */

    rec->type = buf[rec_hdr_type_offset];

    /* Check record content type */
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    rec->cid_len = 0;

    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM &&
        ssl->conf->cid_len != 0                                &&
        rec->type == MBEDTLS_SSL_MSG_CID) {
        /* Shift pointers to account for record header including CID
         * struct {
         *   ContentType outer_type = tls12_cid;
         *   ProtocolVersion version;
         *   uint16 epoch;
         *   uint48 sequence_number;
         *   opaque cid[cid_length]; // Additional field compared to
         *                           // default DTLS record format
         *   uint16 length;
         *   opaque enc_content[DTLSCiphertext.length];
         * } DTLSCiphertext;
         */

        /* So far, we only support static CID lengths
         * fixed in the configuration. */
        rec_hdr_cid_len = ssl->conf->cid_len;
        rec_hdr_len_offset += rec_hdr_cid_len;

        if (len < rec_hdr_len_offset + rec_hdr_len_len) {
            MBEDTLS_SSL_DEBUG_MSG(1,
                                  (
                                      "datagram of length %u too small to hold DTLS record header including CID, length %u",
                                      (unsigned) len,
                                      (unsigned) (rec_hdr_len_offset + rec_hdr_len_len)));
            return MBEDTLS_ERR_SSL_INVALID_RECORD;
        }

        /* configured CID len is guaranteed at most 255, see
         * MBEDTLS_SSL_CID_OUT_LEN_MAX in check_config.h */
        rec->cid_len = (uint8_t) rec_hdr_cid_len;
        memcpy(rec->cid, buf + rec_hdr_cid_offset, rec_hdr_cid_len);
    } else
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
    {
        if (ssl_check_record_type(rec->type)) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("unknown record type %u",
                                      (unsigned) rec->type));
            return MBEDTLS_ERR_SSL_INVALID_RECORD;
        }
    }

    /*
     * Parse and validate record version
     */
    rec->ver[0] = buf[rec_hdr_version_offset + 0];
    rec->ver[1] = buf[rec_hdr_version_offset + 1];
    tls_version = (mbedtls_ssl_protocol_version) mbedtls_ssl_read_version(
        buf + rec_hdr_version_offset,
        ssl->conf->transport);

    if (tls_version > ssl->conf->max_tls_version) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("TLS version mismatch: got %u, expected max %u",
                                  (unsigned) tls_version,
                                  (unsigned) ssl->conf->max_tls_version));

        return MBEDTLS_ERR_SSL_INVALID_RECORD;
    }
    /*
     * Parse/Copy record sequence number.
     */

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        /* Copy explicit record sequence number from input buffer. */
        memcpy(&rec->ctr[0], buf + rec_hdr_ctr_offset,
               rec_hdr_ctr_len);
    } else
#endif /* MBEDTLS_SSL_PROTO_DTLS */
    {
        /* Copy implicit record sequence number from SSL context structure. */
        memcpy(&rec->ctr[0], ssl->in_ctr, rec_hdr_ctr_len);
    }

    /*
     * Parse record length.
     */

    rec->data_offset = rec_hdr_len_offset + rec_hdr_len_len;
    rec->data_len    = MBEDTLS_GET_UINT16_BE(buf, rec_hdr_len_offset);
    MBEDTLS_SSL_DEBUG_BUF(4, "input record header", buf, rec->data_offset);

    MBEDTLS_SSL_DEBUG_MSG(3, ("input record: msgtype = %u, "
                              "version = [0x%x], msglen = %" MBEDTLS_PRINTF_SIZET,
                              rec->type, (unsigned) tls_version, rec->data_len));

    rec->buf     = buf;
    rec->buf_len = rec->data_offset + rec->data_len;

    if (rec->data_len == 0) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("rejecting empty record"));
        return MBEDTLS_ERR_SSL_INVALID_RECORD;
    }

    /*
     * DTLS-related tests.
     * Check epoch before checking length constraint because
     * the latter varies with the epoch. E.g., if a ChangeCipherSpec
     * message gets duplicated before the corresponding Finished message,
     * the second ChangeCipherSpec should be discarded because it belongs
     * to an old epoch, but not because its length is shorter than
     * the minimum record length for packets using the new record transform.
     * Note that these two kinds of failures are handled differently,
     * as an unexpected record is silently skipped but an invalid
     * record leads to the entire datagram being dropped.
     */
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        rec_epoch = MBEDTLS_GET_UINT16_BE(rec->ctr, 0);

        /* Check that the datagram is large enough to contain a record
         * of the advertised length. */
        if (len < rec->data_offset + rec->data_len) {
            MBEDTLS_SSL_DEBUG_MSG(1,
                                  (
                                      "Datagram of length %u too small to contain record of advertised length %u.",
                                      (unsigned) len,
                                      (unsigned) (rec->data_offset + rec->data_len)));
            return MBEDTLS_ERR_SSL_INVALID_RECORD;
        }

        /* Records from other, non-matching epochs are silently discarded.
         * (The case of same-port Client reconnects must be considered in
         *  the caller). */
        if (rec_epoch != ssl->in_epoch) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("record from another epoch: "
                                      "expected %u, received %lu",
                                      ssl->in_epoch, (unsigned long) rec_epoch));

            /* Records from the next epoch are considered for buffering
             * (concretely: early Finished messages). */
            if (rec_epoch == (unsigned) ssl->in_epoch + 1) {
                MBEDTLS_SSL_DEBUG_MSG(2, ("Consider record for buffering"));
                return MBEDTLS_ERR_SSL_EARLY_MESSAGE;
            }

            return MBEDTLS_ERR_SSL_UNEXPECTED_RECORD;
        }
#if defined(MBEDTLS_SSL_DTLS_ANTI_REPLAY)
        /* For records from the correct epoch, check whether their
         * sequence number has been seen before. */
        else if (mbedtls_ssl_dtls_record_replay_check((mbedtls_ssl_context *) ssl,
                                                      &rec->ctr[0]) != 0) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("replayed record"));
            return MBEDTLS_ERR_SSL_UNEXPECTED_RECORD;
        }
#endif
    }
#endif /* MBEDTLS_SSL_PROTO_DTLS */

    return 0;
}


#if defined(MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE) && defined(MBEDTLS_SSL_SRV_C)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_check_client_reconnect(mbedtls_ssl_context *ssl)
{
    unsigned int rec_epoch = MBEDTLS_GET_UINT16_BE(ssl->in_ctr, 0);

    /*
     * Check for an epoch 0 ClientHello. We can't use in_msg here to
     * access the first byte of record content (handshake type), as we
     * have an active transform (possibly iv_len != 0), so use the
     * fact that the record header len is 13 instead.
     */
    if (rec_epoch == 0 &&
        ssl->conf->endpoint == MBEDTLS_SSL_IS_SERVER &&
        mbedtls_ssl_is_handshake_over(ssl) == 1 &&
        ssl->in_msgtype == MBEDTLS_SSL_MSG_HANDSHAKE &&
        ssl->in_left > 13 &&
        ssl->in_buf[13] == MBEDTLS_SSL_HS_CLIENT_HELLO) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("possible client reconnect "
                                  "from the same port"));
        return ssl_handle_possible_reconnect(ssl);
    }

    return 0;
}
#endif /* MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE && MBEDTLS_SSL_SRV_C */

/*
 * If applicable, decrypt record content
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_prepare_record_content(mbedtls_ssl_context *ssl,
                                      mbedtls_record *rec)
{
    int ret, done = 0;

    MBEDTLS_SSL_DEBUG_BUF(4, "input record from network",
                          rec->buf, rec->buf_len);

    /*
     * In TLS 1.3, always treat ChangeCipherSpec records
     * as unencrypted. The only thing we do with them is
     * check the length and content and ignore them.
     */
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    if (ssl->transform_in != NULL &&
        ssl->transform_in->tls_version == MBEDTLS_SSL_VERSION_TLS1_3) {
        if (rec->type == MBEDTLS_SSL_MSG_CHANGE_CIPHER_SPEC) {
            done = 1;
        }
    }
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */

    if (!done && ssl->transform_in != NULL) {
        unsigned char const old_msg_type = rec->type;

        if ((ret = mbedtls_ssl_decrypt_buf(ssl, ssl->transform_in,
                                           rec)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "ssl_decrypt_buf", ret);

#if defined(MBEDTLS_SSL_EARLY_DATA) && defined(MBEDTLS_SSL_SRV_C)
            /*
             * Although the server rejected early data, it might receive early
             * data as long as it has not received the client Finished message.
             * It is encrypted with early keys and should be ignored as stated
             * in section 4.2.10 of RFC 8446:
             *
             * "Ignore the extension and return a regular 1-RTT response. The
             * server then skips past early data by attempting to deprotect
             * received records using the handshake traffic key, discarding
             * records which fail deprotection (up to the configured
             * max_early_data_size). Once a record is deprotected successfully,
             * it is treated as the start of the client's second flight and the
             * server proceeds as with an ordinary 1-RTT handshake."
             */
            if ((old_msg_type == MBEDTLS_SSL_MSG_APPLICATION_DATA) &&
                (ssl->discard_early_data_record ==
                 MBEDTLS_SSL_EARLY_DATA_TRY_TO_DEPROTECT_AND_DISCARD)) {
                MBEDTLS_SSL_DEBUG_MSG(
                    3, ("EarlyData: deprotect and discard app data records."));

                ret = mbedtls_ssl_tls13_check_early_data_len(ssl, rec->data_len);
                if (ret != 0) {
                    return ret;
                }
                ret = MBEDTLS_ERR_SSL_CONTINUE_PROCESSING;
            }
#endif /* MBEDTLS_SSL_EARLY_DATA && MBEDTLS_SSL_SRV_C */

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
            if (ret == MBEDTLS_ERR_SSL_UNEXPECTED_CID &&
                ssl->conf->ignore_unexpected_cid
                == MBEDTLS_SSL_UNEXPECTED_CID_IGNORE) {
                MBEDTLS_SSL_DEBUG_MSG(3, ("ignoring unexpected CID"));
                ret = MBEDTLS_ERR_SSL_CONTINUE_PROCESSING;
            }
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

            /*
             * The decryption of the record failed, no reason to ignore it,
             * return in error with the decryption error code.
             */
            return ret;
        }

#if defined(MBEDTLS_SSL_EARLY_DATA) && defined(MBEDTLS_SSL_SRV_C)
        /*
         * If the server were discarding protected records that it fails to
         * deprotect because it has rejected early data, as we have just
         * deprotected successfully a record, the server has to resume normal
         * operation and fail the connection if the deprotection of a record
         * fails.
         */
        if (ssl->discard_early_data_record ==
            MBEDTLS_SSL_EARLY_DATA_TRY_TO_DEPROTECT_AND_DISCARD) {
            ssl->discard_early_data_record = MBEDTLS_SSL_EARLY_DATA_NO_DISCARD;
        }
#endif /* MBEDTLS_SSL_EARLY_DATA && MBEDTLS_SSL_SRV_C */

        if (old_msg_type != rec->type) {
            MBEDTLS_SSL_DEBUG_MSG(4, ("record type after decrypt (before %d): %d",
                                      old_msg_type, rec->type));
        }

        MBEDTLS_SSL_DEBUG_BUF(4, "input payload after decrypt",
                              rec->buf + rec->data_offset, rec->data_len);

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
        /* We have already checked the record content type
         * in ssl_parse_record_header(), failing or silently
         * dropping the record in the case of an unknown type.
         *
         * Since with the use of CIDs, the record content type
         * might change during decryption, re-check the record
         * content type, but treat a failure as fatal this time. */
        if (ssl_check_record_type(rec->type)) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("unknown record type"));
            return MBEDTLS_ERR_SSL_INVALID_RECORD;
        }
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

        if (rec->data_len == 0) {
#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
            if (ssl->tls_version == MBEDTLS_SSL_VERSION_TLS1_2
                && rec->type != MBEDTLS_SSL_MSG_APPLICATION_DATA) {
                /* TLS v1.2 explicitly disallows zero-length messages which are not application data */
                MBEDTLS_SSL_DEBUG_MSG(1, ("invalid zero-length message type: %d", ssl->in_msgtype));
                return MBEDTLS_ERR_SSL_INVALID_RECORD;
            }
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

            ssl->nb_zero++;

            /*
             * Three or more empty messages may be a DoS attack
             * (excessive CPU consumption).
             */
            if (ssl->nb_zero > 3) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("received four consecutive empty "
                                          "messages, possible DoS attack"));
                /* Treat the records as if they were not properly authenticated,
                 * thereby failing the connection if we see more than allowed
                 * by the configured bad MAC threshold. */
                return MBEDTLS_ERR_SSL_INVALID_MAC;
            }
        } else {
            ssl->nb_zero = 0;
        }

#if defined(MBEDTLS_SSL_PROTO_DTLS)
        if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
            ; /* in_ctr read from peer, not maintained internally */
        } else
#endif
        {
            unsigned i;
            for (i = MBEDTLS_SSL_SEQUENCE_NUMBER_LEN;
                 i > mbedtls_ssl_ep_len(ssl); i--) {
                if (++ssl->in_ctr[i - 1] != 0) {
                    break;
                }
            }

            /* The loop goes to its end iff the counter is wrapping */
            if (i == mbedtls_ssl_ep_len(ssl)) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("incoming message counter would wrap"));
                return MBEDTLS_ERR_SSL_COUNTER_WRAPPING;
            }
        }

    }

#if defined(MBEDTLS_SSL_EARLY_DATA) && defined(MBEDTLS_SSL_SRV_C)
    /*
     * Although the server rejected early data because it needed to send an
     * HelloRetryRequest message, it might receive early data as long as it has
     * not received the client Finished message.
     * The early data is encrypted with early keys and should be ignored as
     * stated in section 4.2.10 of RFC 8446 (second case):
     *
     * "The server then ignores early data by skipping all records with an
     * external content type of "application_data" (indicating that they are
     * encrypted), up to the configured max_early_data_size. Ignore application
     * data message before 2nd ClientHello when early_data was received in 1st
     * ClientHello."
     */
    if (ssl->discard_early_data_record == MBEDTLS_SSL_EARLY_DATA_DISCARD) {
        if (rec->type == MBEDTLS_SSL_MSG_APPLICATION_DATA) {

            ret = mbedtls_ssl_tls13_check_early_data_len(ssl, rec->data_len);
            if (ret != 0) {
                return ret;
            }

            MBEDTLS_SSL_DEBUG_MSG(
                3, ("EarlyData: Ignore application message before 2nd ClientHello"));

            return MBEDTLS_ERR_SSL_CONTINUE_PROCESSING;
        } else if (rec->type == MBEDTLS_SSL_MSG_HANDSHAKE) {
            ssl->discard_early_data_record = MBEDTLS_SSL_EARLY_DATA_NO_DISCARD;
        }
    }
#endif /* MBEDTLS_SSL_EARLY_DATA && MBEDTLS_SSL_SRV_C */

#if defined(MBEDTLS_SSL_DTLS_ANTI_REPLAY)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        mbedtls_ssl_dtls_replay_update(ssl);
    }
#endif

    /* Check actual (decrypted) record content length against
     * configured maximum. */
    if (rec->data_len > MBEDTLS_SSL_IN_CONTENT_LEN) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad message length"));
        return MBEDTLS_ERR_SSL_INVALID_RECORD;
    }

    return 0;
}

/*
 * Read a record.
 *
 * Silently ignore non-fatal alert (and for DTLS, invalid records as well,
 * RFC 6347 4.1.2.7) and continue reading until a valid record is found.
 *
 */

/* Helper functions for mbedtls_ssl_read_record(). */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_consume_current_message(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_get_next_record(mbedtls_ssl_context *ssl);
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_record_is_in_progress(mbedtls_ssl_context *ssl);

int mbedtls_ssl_read_record(mbedtls_ssl_context *ssl,
                            unsigned update_hs_digest)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> read record"));

    if (ssl->keep_current_message == 0) {
        do {

            ret = ssl_consume_current_message(ssl);
            if (ret != 0) {
                return ret;
            }

            if (ssl_record_is_in_progress(ssl) == 0) {
                int dtls_have_buffered = 0;
#if defined(MBEDTLS_SSL_PROTO_DTLS)

                /* We only check for buffered messages if the
                 * current datagram is fully consumed. */
                if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM &&
                    ssl_next_record_is_in_datagram(ssl) == 0) {
                    if (ssl_load_buffered_message(ssl) == 0) {
                        dtls_have_buffered = 1;
                    }
                }

#endif /* MBEDTLS_SSL_PROTO_DTLS */
                if (dtls_have_buffered == 0) {
                    ret = ssl_get_next_record(ssl);
                    if (ret == MBEDTLS_ERR_SSL_CONTINUE_PROCESSING) {
                        continue;
                    }

                    if (ret != 0) {
                        MBEDTLS_SSL_DEBUG_RET(1, ("ssl_get_next_record"), ret);
                        return ret;
                    }
                }
            }

            ret = mbedtls_ssl_handle_message_type(ssl);

#if defined(MBEDTLS_SSL_PROTO_DTLS)
            if (ret == MBEDTLS_ERR_SSL_EARLY_MESSAGE) {
                /* Buffer future message */
                ret = ssl_buffer_message(ssl);
                if (ret != 0) {
                    return ret;
                }

                ret = MBEDTLS_ERR_SSL_CONTINUE_PROCESSING;
            }
#endif /* MBEDTLS_SSL_PROTO_DTLS */

        } while (MBEDTLS_ERR_SSL_NON_FATAL           == ret  ||
                 MBEDTLS_ERR_SSL_CONTINUE_PROCESSING == ret);

        if (0 != ret) {
            MBEDTLS_SSL_DEBUG_RET(1, ("mbedtls_ssl_handle_message_type"), ret);
            return ret;
        }

        if (ssl->in_msgtype == MBEDTLS_SSL_MSG_HANDSHAKE &&
            update_hs_digest == 1) {
            ret = mbedtls_ssl_update_handshake_status(ssl);
            if (0 != ret) {
                MBEDTLS_SSL_DEBUG_RET(1, ("mbedtls_ssl_update_handshake_status"), ret);
                return ret;
            }
        }
    } else {
        MBEDTLS_SSL_DEBUG_MSG(2, ("reuse previously read message"));
        ssl->keep_current_message = 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= read record"));

    return 0;
}

#if defined(MBEDTLS_SSL_PROTO_DTLS)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_next_record_is_in_datagram(mbedtls_ssl_context *ssl)
{
    if (ssl->in_left > ssl->next_record_offset) {
        return 1;
    }

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_load_buffered_message(mbedtls_ssl_context *ssl)
{
    mbedtls_ssl_handshake_params * const hs = ssl->handshake;
    mbedtls_ssl_hs_buffer *hs_buf;
    int ret = 0;

    if (hs == NULL) {
        return -1;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> ssl_load_buffered_message"));

    if (ssl->state == MBEDTLS_SSL_CLIENT_CHANGE_CIPHER_SPEC ||
        ssl->state == MBEDTLS_SSL_SERVER_CHANGE_CIPHER_SPEC) {
        /* Check if we have seen a ChangeCipherSpec before.
         * If yes, synthesize a CCS record. */
        if (!hs->buffering.seen_ccs) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("CCS not seen in the current flight"));
            ret = -1;
            goto exit;
        }

        MBEDTLS_SSL_DEBUG_MSG(2, ("Injecting buffered CCS message"));
        ssl->in_msgtype = MBEDTLS_SSL_MSG_CHANGE_CIPHER_SPEC;
        ssl->in_msglen = 1;
        ssl->in_msg[0] = 1;

        /* As long as they are equal, the exact value doesn't matter. */
        ssl->in_left            = 0;
        ssl->next_record_offset = 0;

        hs->buffering.seen_ccs = 0;
        goto exit;
    }

#if defined(MBEDTLS_DEBUG_C)
    /* Debug only */
    {
        unsigned offset;
        for (offset = 1; offset < MBEDTLS_SSL_MAX_BUFFERED_HS; offset++) {
            hs_buf = &hs->buffering.hs[offset];
            if (hs_buf->is_valid == 1) {
                MBEDTLS_SSL_DEBUG_MSG(2, ("Future message with sequence number %u %s buffered.",
                                          hs->in_msg_seq + offset,
                                          hs_buf->is_complete ? "fully" : "partially"));
            }
        }
    }
#endif /* MBEDTLS_DEBUG_C */

    /* Check if we have buffered and/or fully reassembled the
     * next handshake message. */
    hs_buf = &hs->buffering.hs[0];
    if ((hs_buf->is_valid == 1) && (hs_buf->is_complete == 1)) {
        /* Synthesize a record containing the buffered HS message. */
        size_t msg_len = MBEDTLS_GET_UINT24_BE(hs_buf->data, 1);

        /* Double-check that we haven't accidentally buffered
         * a message that doesn't fit into the input buffer. */
        if (msg_len + 12 > MBEDTLS_SSL_IN_CONTENT_LEN) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        MBEDTLS_SSL_DEBUG_MSG(2, ("Next handshake message has been buffered - load"));
        MBEDTLS_SSL_DEBUG_BUF(3, "Buffered handshake message (incl. header)",
                              hs_buf->data, msg_len + 12);

        ssl->in_msgtype = MBEDTLS_SSL_MSG_HANDSHAKE;
        ssl->in_hslen   = msg_len + 12;
        ssl->in_msglen  = msg_len + 12;
        memcpy(ssl->in_msg, hs_buf->data, ssl->in_hslen);

        ret = 0;
        goto exit;
    } else {
        MBEDTLS_SSL_DEBUG_MSG(2, ("Next handshake message %u not or only partially bufffered",
                                  hs->in_msg_seq));
    }

    ret = -1;

exit:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= ssl_load_buffered_message"));
    return ret;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_buffer_make_space(mbedtls_ssl_context *ssl,
                                 size_t desired)
{
    int offset;
    mbedtls_ssl_handshake_params * const hs = ssl->handshake;
    MBEDTLS_SSL_DEBUG_MSG(2, ("Attempt to free buffered messages to have %u bytes available",
                              (unsigned) desired));

    /* Get rid of future records epoch first, if such exist. */
    ssl_free_buffered_record(ssl);

    /* Check if we have enough space available now. */
    if (desired <= (MBEDTLS_SSL_DTLS_MAX_BUFFERING -
                    hs->buffering.total_bytes_buffered)) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("Enough space available after freeing future epoch record"));
        return 0;
    }

    /* We don't have enough space to buffer the next expected handshake
     * message. Remove buffers used for future messages to gain space,
     * starting with the most distant one. */
    for (offset = MBEDTLS_SSL_MAX_BUFFERED_HS - 1;
         offset >= 0; offset--) {
        MBEDTLS_SSL_DEBUG_MSG(2,
                              (
                                  "Free buffering slot %d to make space for reassembly of next handshake message",
                                  offset));

        ssl_buffering_free_slot(ssl, (uint8_t) offset);

        /* Check if we have enough space available now. */
        if (desired <= (MBEDTLS_SSL_DTLS_MAX_BUFFERING -
                        hs->buffering.total_bytes_buffered)) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("Enough space available after freeing buffered HS messages"));
            return 0;
        }
    }

    return -1;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_buffer_message(mbedtls_ssl_context *ssl)
{
    int ret = 0;
    mbedtls_ssl_handshake_params * const hs = ssl->handshake;

    if (hs == NULL) {
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> ssl_buffer_message"));

    switch (ssl->in_msgtype) {
        case MBEDTLS_SSL_MSG_CHANGE_CIPHER_SPEC:
            MBEDTLS_SSL_DEBUG_MSG(2, ("Remember CCS message"));

            hs->buffering.seen_ccs = 1;
            break;

        case MBEDTLS_SSL_MSG_HANDSHAKE:
        {
            unsigned recv_msg_seq_offset;
            unsigned recv_msg_seq = MBEDTLS_GET_UINT16_BE(ssl->in_msg, 4);
            mbedtls_ssl_hs_buffer *hs_buf;
            size_t msg_len = ssl->in_hslen - 12;

            /* We should never receive an old handshake
             * message - double-check nonetheless. */
            if (recv_msg_seq < ssl->handshake->in_msg_seq) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
                return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
            }

            recv_msg_seq_offset = recv_msg_seq - ssl->handshake->in_msg_seq;
            if (recv_msg_seq_offset >= MBEDTLS_SSL_MAX_BUFFERED_HS) {
                /* Silently ignore -- message too far in the future */
                MBEDTLS_SSL_DEBUG_MSG(2,
                                      ("Ignore future HS message with sequence number %u, "
                                       "buffering window %u - %u",
                                       recv_msg_seq, ssl->handshake->in_msg_seq,
                                       ssl->handshake->in_msg_seq + MBEDTLS_SSL_MAX_BUFFERED_HS -
                                       1));

                goto exit;
            }

            MBEDTLS_SSL_DEBUG_MSG(2, ("Buffering HS message with sequence number %u, offset %u ",
                                      recv_msg_seq, recv_msg_seq_offset));

            hs_buf = &hs->buffering.hs[recv_msg_seq_offset];

            /* Check if the buffering for this seq nr has already commenced. */
            if (!hs_buf->is_valid) {
                size_t reassembly_buf_sz;

                hs_buf->is_fragmented =
                    (ssl_hs_is_proper_fragment(ssl) == 1);

                /* We copy the message back into the input buffer
                 * after reassembly, so check that it's not too large.
                 * This is an implementation-specific limitation
                 * and not one from the standard, hence it is not
                 * checked in ssl_check_hs_header(). */
                if (msg_len + 12 > MBEDTLS_SSL_IN_CONTENT_LEN) {
                    /* Ignore message */
                    goto exit;
                }

                /* Check if we have enough space to buffer the message. */
                if (hs->buffering.total_bytes_buffered >
                    MBEDTLS_SSL_DTLS_MAX_BUFFERING) {
                    MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
                    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
                }

                reassembly_buf_sz = ssl_get_reassembly_buffer_size(msg_len,
                                                                   hs_buf->is_fragmented);

                if (reassembly_buf_sz > (MBEDTLS_SSL_DTLS_MAX_BUFFERING -
                                         hs->buffering.total_bytes_buffered)) {
                    if (recv_msg_seq_offset > 0) {
                        /* If we can't buffer a future message because
                         * of space limitations -- ignore. */
                        MBEDTLS_SSL_DEBUG_MSG(2,
                                              ("Buffering of future message of size %"
                                               MBEDTLS_PRINTF_SIZET
                                               " would exceed the compile-time limit %"
                                               MBEDTLS_PRINTF_SIZET
                                               " (already %" MBEDTLS_PRINTF_SIZET
                                               " bytes buffered) -- ignore\n",
                                               msg_len, (size_t) MBEDTLS_SSL_DTLS_MAX_BUFFERING,
                                               hs->buffering.total_bytes_buffered));
                        goto exit;
                    } else {
                        MBEDTLS_SSL_DEBUG_MSG(2,
                                              ("Buffering of future message of size %"
                                               MBEDTLS_PRINTF_SIZET
                                               " would exceed the compile-time limit %"
                                               MBEDTLS_PRINTF_SIZET
                                               " (already %" MBEDTLS_PRINTF_SIZET
                                               " bytes buffered) -- attempt to make space by freeing buffered future messages\n",
                                               msg_len, (size_t) MBEDTLS_SSL_DTLS_MAX_BUFFERING,
                                               hs->buffering.total_bytes_buffered));
                    }

                    if (ssl_buffer_make_space(ssl, reassembly_buf_sz) != 0) {
                        MBEDTLS_SSL_DEBUG_MSG(2,
                                              ("Reassembly of next message of size %"
                                               MBEDTLS_PRINTF_SIZET
                                               " (%" MBEDTLS_PRINTF_SIZET
                                               " with bitmap) would exceed"
                                               " the compile-time limit %"
                                               MBEDTLS_PRINTF_SIZET
                                               " (already %" MBEDTLS_PRINTF_SIZET
                                               " bytes buffered) -- fail\n",
                                               msg_len,
                                               reassembly_buf_sz,
                                               (size_t) MBEDTLS_SSL_DTLS_MAX_BUFFERING,
                                               hs->buffering.total_bytes_buffered));
                        ret = MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
                        goto exit;
                    }
                }

                MBEDTLS_SSL_DEBUG_MSG(2,
                                      ("initialize reassembly, total length = %"
                                       MBEDTLS_PRINTF_SIZET,
                                       msg_len));

                hs_buf->data = mbedtls_calloc(1, reassembly_buf_sz);
                if (hs_buf->data == NULL) {
                    ret = MBEDTLS_ERR_SSL_ALLOC_FAILED;
                    goto exit;
                }
                hs_buf->data_len = reassembly_buf_sz;

                /* Prepare final header: copy msg_type, length and message_seq,
                 * then add standardised fragment_offset and fragment_length */
                memcpy(hs_buf->data, ssl->in_msg, 6);
                memset(hs_buf->data + 6, 0, 3);
                memcpy(hs_buf->data + 9, hs_buf->data + 1, 3);

                hs_buf->is_valid = 1;

                hs->buffering.total_bytes_buffered += reassembly_buf_sz;
            } else {
                /* Make sure msg_type and length are consistent */
                if (memcmp(hs_buf->data, ssl->in_msg, 4) != 0) {
                    MBEDTLS_SSL_DEBUG_MSG(1, ("Fragment header mismatch - ignore"));
                    /* Ignore */
                    goto exit;
                }
            }

            if (!hs_buf->is_complete) {
                size_t frag_len, frag_off;
                unsigned char * const msg = hs_buf->data + 12;

                /*
                 * Check and copy current fragment
                 */

                /* Validation of header fields already done in
                 * mbedtls_ssl_prepare_handshake_record(). */
                frag_off = ssl_get_hs_frag_off(ssl);
                frag_len = ssl_get_hs_frag_len(ssl);

                MBEDTLS_SSL_DEBUG_MSG(2, ("adding fragment, offset = %" MBEDTLS_PRINTF_SIZET
                                          ", length = %" MBEDTLS_PRINTF_SIZET,
                                          frag_off, frag_len));
                memcpy(msg + frag_off, ssl->in_msg + 12, frag_len);

                if (hs_buf->is_fragmented) {
                    unsigned char * const bitmask = msg + msg_len;
                    ssl_bitmask_set(bitmask, frag_off, frag_len);
                    hs_buf->is_complete = (ssl_bitmask_check(bitmask,
                                                             msg_len) == 0);
                } else {
                    hs_buf->is_complete = 1;
                }

                MBEDTLS_SSL_DEBUG_MSG(2, ("message %scomplete",
                                          hs_buf->is_complete ? "" : "not yet "));
            }

            break;
        }

        default:
            /* We don't buffer other types of messages. */
            break;
    }

exit:

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= ssl_buffer_message"));
    return ret;
}
#endif /* MBEDTLS_SSL_PROTO_DTLS */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_consume_current_message(mbedtls_ssl_context *ssl)
{
    /*
     * Consume last content-layer message and potentially
     * update in_msglen which keeps track of the contents'
     * consumption state.
     *
     * (1) Handshake messages:
     *     Remove last handshake message, move content
     *     and adapt in_msglen.
     *
     * (2) Alert messages:
     *     Consume whole record content, in_msglen = 0.
     *
     * (3) Change cipher spec:
     *     Consume whole record content, in_msglen = 0.
     *
     * (4) Application data:
     *     Don't do anything - the record layer provides
     *     the application data as a stream transport
     *     and consumes through mbedtls_ssl_read only.
     *
     */

    /* Case (1): Handshake messages */
    if (ssl->in_hslen != 0) {
        /* Hard assertion to be sure that no application data
         * is in flight, as corrupting ssl->in_msglen during
         * ssl->in_offt != NULL is fatal. */
        if (ssl->in_offt != NULL) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }

        if (ssl->badmac_seen_or_in_hsfraglen != 0) {
            /* Not all handshake fragments have arrived, do not consume. */
            MBEDTLS_SSL_DEBUG_MSG(3, ("Consume: waiting for more handshake fragments "
                                      "%u/%" MBEDTLS_PRINTF_SIZET,
                                      ssl->badmac_seen_or_in_hsfraglen, ssl->in_hslen));
            return 0;
        }

        /*
         * Get next Handshake message in the current record
         */

        /* Notes:
         * (1) in_hslen is not necessarily the size of the
         *     current handshake content: If DTLS handshake
         *     fragmentation is used, that's the fragment
         *     size instead. Using the total handshake message
         *     size here is faulty and should be changed at
         *     some point.
         * (2) While it doesn't seem to cause problems, one
         *     has to be very careful not to assume that in_hslen
         *     is always <= in_msglen in a sensible communication.
         *     Again, it's wrong for DTLS handshake fragmentation.
         *     The following check is therefore mandatory, and
         *     should not be treated as a silently corrected assertion.
         *     Additionally, ssl->in_hslen might be arbitrarily out of
         *     bounds after handling a DTLS message with an unexpected
         *     sequence number, see mbedtls_ssl_prepare_handshake_record.
         */
        if (ssl->in_hslen < ssl->in_msglen) {
            ssl->in_msglen -= ssl->in_hslen;
            memmove(ssl->in_msg, ssl->in_msg + ssl->in_hslen,
                    ssl->in_msglen);
            MBEDTLS_PUT_UINT16_BE(ssl->in_msglen, ssl->in_len, 0);

            MBEDTLS_SSL_DEBUG_BUF(4, "remaining content in record",
                                  ssl->in_msg, ssl->in_msglen);
        } else {
            ssl->in_msglen = 0;
        }

        ssl->in_hslen   = 0;
    }
    /* Case (4): Application data */
    else if (ssl->in_offt != NULL) {
        return 0;
    }
    /* Everything else (CCS & Alerts) */
    else {
        ssl->in_msglen = 0;
    }

    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_record_is_in_progress(mbedtls_ssl_context *ssl)
{
    if (ssl->in_msglen > 0) {
        return 1;
    }

    return 0;
}

#if defined(MBEDTLS_SSL_PROTO_DTLS)

static void ssl_free_buffered_record(mbedtls_ssl_context *ssl)
{
    mbedtls_ssl_handshake_params * const hs = ssl->handshake;
    if (hs == NULL) {
        return;
    }

    if (hs->buffering.future_record.data != NULL) {
        hs->buffering.total_bytes_buffered -=
            hs->buffering.future_record.len;

        mbedtls_free(hs->buffering.future_record.data);
        hs->buffering.future_record.data = NULL;
    }
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_load_buffered_record(mbedtls_ssl_context *ssl)
{
    mbedtls_ssl_handshake_params * const hs = ssl->handshake;
    unsigned char *rec;
    size_t rec_len;
    unsigned rec_epoch;
#if defined(MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH)
    size_t in_buf_len = ssl->in_buf_len;
#else
    size_t in_buf_len = MBEDTLS_SSL_IN_BUFFER_LEN;
#endif
    if (ssl->conf->transport != MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        return 0;
    }

    if (hs == NULL) {
        return 0;
    }

    rec       = hs->buffering.future_record.data;
    rec_len   = hs->buffering.future_record.len;
    rec_epoch = hs->buffering.future_record.epoch;

    if (rec == NULL) {
        return 0;
    }

    /* Only consider loading future records if the
     * input buffer is empty. */
    if (ssl_next_record_is_in_datagram(ssl) == 1) {
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> ssl_load_buffered_record"));

    if (rec_epoch != ssl->in_epoch) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("Buffered record not from current epoch."));
        goto exit;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("Found buffered record from current epoch - load"));

    /* Double-check that the record is not too large */
    if (rec_len > in_buf_len - (size_t) (ssl->in_hdr - ssl->in_buf)) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }

    memcpy(ssl->in_hdr, rec, rec_len);
    ssl->in_left = rec_len;
    ssl->next_record_offset = 0;

    ssl_free_buffered_record(ssl);

exit:
    MBEDTLS_SSL_DEBUG_MSG(2, ("<= ssl_load_buffered_record"));
    return 0;
}

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_buffer_future_record(mbedtls_ssl_context *ssl,
                                    mbedtls_record const *rec)
{
    mbedtls_ssl_handshake_params * const hs = ssl->handshake;

    /* Don't buffer future records outside handshakes. */
    if (hs == NULL) {
        return 0;
    }

    /* Only buffer handshake records (we are only interested
     * in Finished messages). */
    if (rec->type != MBEDTLS_SSL_MSG_HANDSHAKE) {
        return 0;
    }

    /* Don't buffer more than one future epoch record. */
    if (hs->buffering.future_record.data != NULL) {
        return 0;
    }

    /* Don't buffer record if there's not enough buffering space remaining. */
    if (rec->buf_len > (MBEDTLS_SSL_DTLS_MAX_BUFFERING -
                        hs->buffering.total_bytes_buffered)) {
        MBEDTLS_SSL_DEBUG_MSG(2, ("Buffering of future epoch record of size %" MBEDTLS_PRINTF_SIZET
                                  " would exceed the compile-time limit %" MBEDTLS_PRINTF_SIZET
                                  " (already %" MBEDTLS_PRINTF_SIZET
                                  " bytes buffered) -- ignore\n",
                                  rec->buf_len, (size_t) MBEDTLS_SSL_DTLS_MAX_BUFFERING,
                                  hs->buffering.total_bytes_buffered));
        return 0;
    }

    /* Buffer record */
    MBEDTLS_SSL_DEBUG_MSG(2, ("Buffer record from epoch %u",
                              ssl->in_epoch + 1U));
    MBEDTLS_SSL_DEBUG_BUF(3, "Buffered record", rec->buf, rec->buf_len);

    /* ssl_parse_record_header() only considers records
     * of the next epoch as candidates for buffering. */
    hs->buffering.future_record.epoch = ssl->in_epoch + 1;
    hs->buffering.future_record.len   = rec->buf_len;

    hs->buffering.future_record.data =
        mbedtls_calloc(1, hs->buffering.future_record.len);
    if (hs->buffering.future_record.data == NULL) {
        /* If we run out of RAM trying to buffer a
         * record from the next epoch, just ignore. */
        return 0;
    }

    memcpy(hs->buffering.future_record.data, rec->buf, rec->buf_len);

    hs->buffering.total_bytes_buffered += rec->buf_len;
    return 0;
}

#endif /* MBEDTLS_SSL_PROTO_DTLS */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_get_next_record(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_record rec;

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    /* We might have buffered a future record; if so,
     * and if the epoch matches now, load it.
     * On success, this call will set ssl->in_left to
     * the length of the buffered record, so that
     * the calls to ssl_fetch_input() below will
     * essentially be no-ops. */
    ret = ssl_load_buffered_record(ssl);
    if (ret != 0) {
        return ret;
    }
#endif /* MBEDTLS_SSL_PROTO_DTLS */

    /* Ensure that we have enough space available for the default form
     * of TLS / DTLS record headers (5 Bytes for TLS, 13 Bytes for DTLS,
     * with no space for CIDs counted in). */
    ret = mbedtls_ssl_fetch_input(ssl, mbedtls_ssl_in_hdr_len(ssl));
    if (ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_fetch_input", ret);
        return ret;
    }

    ret = ssl_parse_record_header(ssl, ssl->in_hdr, ssl->in_left, &rec);
    if (ret != 0) {
#if defined(MBEDTLS_SSL_PROTO_DTLS)
        if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
            if (ret == MBEDTLS_ERR_SSL_EARLY_MESSAGE) {
                ret = ssl_buffer_future_record(ssl, &rec);
                if (ret != 0) {
                    return ret;
                }

                /* Fall through to handling of unexpected records */
                ret = MBEDTLS_ERR_SSL_UNEXPECTED_RECORD;
            }

            if (ret == MBEDTLS_ERR_SSL_UNEXPECTED_RECORD) {
#if defined(MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE) && defined(MBEDTLS_SSL_SRV_C)
                /* Reset in pointers to default state for TLS/DTLS records,
                 * assuming no CID and no offset between record content and
                 * record plaintext. */
                mbedtls_ssl_update_in_pointers(ssl);

                /* Setup internal message pointers from record structure. */
                ssl->in_msgtype = rec.type;
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
                ssl->in_len = ssl->in_cid + rec.cid_len;
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
                ssl->in_iv  = ssl->in_msg = ssl->in_len + 2;
                ssl->in_msglen = rec.data_len;

                ret = ssl_check_client_reconnect(ssl);
                MBEDTLS_SSL_DEBUG_RET(2, "ssl_check_client_reconnect", ret);
                if (ret != 0) {
                    return ret;
                }
#endif

                /* Skip unexpected record (but not whole datagram) */
                ssl->next_record_offset = rec.buf_len;

                MBEDTLS_SSL_DEBUG_MSG(1, ("discarding unexpected record "
                                          "(header)"));
            } else {
                /* Skip invalid record and the rest of the datagram */
                ssl->next_record_offset = 0;
                ssl->in_left = 0;

                MBEDTLS_SSL_DEBUG_MSG(1, ("discarding invalid record "
                                          "(header)"));
            }

            /* Get next record */
            return MBEDTLS_ERR_SSL_CONTINUE_PROCESSING;
        } else
#endif
        {
            return ret;
        }
    }

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        /* Remember offset of next record within datagram. */
        ssl->next_record_offset = rec.buf_len;
        if (ssl->next_record_offset < ssl->in_left) {
            MBEDTLS_SSL_DEBUG_MSG(3, ("more than one record within datagram"));
        }
    } else
#endif
    {
        /*
         * Fetch record contents from underlying transport.
         */
        ret = mbedtls_ssl_fetch_input(ssl, rec.buf_len);
        if (ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_fetch_input", ret);
            return ret;
        }

        ssl->in_left = 0;
    }

    /*
     * Decrypt record contents.
     */

    if ((ret = ssl_prepare_record_content(ssl, &rec)) != 0) {
#if defined(MBEDTLS_SSL_PROTO_DTLS)
        if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
            /* Silently discard invalid records */
            if (ret == MBEDTLS_ERR_SSL_INVALID_MAC) {
                /* Except when waiting for Finished as a bad mac here
                 * probably means something went wrong in the handshake
                 * (eg wrong psk used, mitm downgrade attempt, etc.) */
                if (ssl->state == MBEDTLS_SSL_CLIENT_FINISHED ||
                    ssl->state == MBEDTLS_SSL_SERVER_FINISHED) {
#if defined(MBEDTLS_SSL_ALL_ALERT_MESSAGES)
                    if (ret == MBEDTLS_ERR_SSL_INVALID_MAC) {
                        mbedtls_ssl_send_alert_message(ssl,
                                                       MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                                       MBEDTLS_SSL_ALERT_MSG_BAD_RECORD_MAC);
                    }
#endif
                    return ret;
                }

                if (ssl->conf->badmac_limit != 0) {
                    ++ssl->badmac_seen_or_in_hsfraglen;
                    if (ssl->badmac_seen_or_in_hsfraglen >= ssl->conf->badmac_limit) {
                        MBEDTLS_SSL_DEBUG_MSG(1, ("too many records with bad MAC"));
                        return MBEDTLS_ERR_SSL_INVALID_MAC;
                    }
                }

                /* As above, invalid records cause
                 * dismissal of the whole datagram. */

                ssl->next_record_offset = 0;
                ssl->in_left = 0;

                MBEDTLS_SSL_DEBUG_MSG(1, ("discarding invalid record (mac)"));
                return MBEDTLS_ERR_SSL_CONTINUE_PROCESSING;
            }

            return ret;
        } else
#endif
        {
            /* Error out (and send alert) on invalid records */
#if defined(MBEDTLS_SSL_ALL_ALERT_MESSAGES)
            if (ret == MBEDTLS_ERR_SSL_INVALID_MAC) {
                mbedtls_ssl_send_alert_message(ssl,
                                               MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                               MBEDTLS_SSL_ALERT_MSG_BAD_RECORD_MAC);
            }
#endif
            return ret;
        }
    }


    /* Reset in pointers to default state for TLS/DTLS records,
     * assuming no CID and no offset between record content and
     * record plaintext. */
    mbedtls_ssl_update_in_pointers(ssl);
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    ssl->in_len = ssl->in_cid + rec.cid_len;
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
    ssl->in_iv  = ssl->in_len + 2;

    /* The record content type may change during decryption,
     * so re-read it. */
    ssl->in_msgtype = rec.type;
    /* Also update the input buffer, because unfortunately
     * the server-side ssl_parse_client_hello() reparses the
     * record header when receiving a ClientHello initiating
     * a renegotiation. */
    ssl->in_hdr[0] = rec.type;
    ssl->in_msg    = rec.buf + rec.data_offset;
    ssl->in_msglen = rec.data_len;
    MBEDTLS_PUT_UINT16_BE(rec.data_len, ssl->in_len, 0);

    return 0;
}

int mbedtls_ssl_handle_message_type(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    /* If we're in the middle of a fragmented TLS handshake message,
     * we don't accept any other message type. For TLS 1.3, the spec forbids
     * interleaving other message types between handshake fragments. For TLS
     * 1.2, the spec does not forbid it but we do. */
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_STREAM &&
        ssl->badmac_seen_or_in_hsfraglen != 0 &&
        ssl->in_msgtype != MBEDTLS_SSL_MSG_HANDSHAKE) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("non-handshake message in the middle"
                                  " of a fragmented handshake message"));
        return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
    }

    /*
     * Handle particular types of records
     */
    if (ssl->in_msgtype == MBEDTLS_SSL_MSG_HANDSHAKE) {
        if ((ret = mbedtls_ssl_prepare_handshake_record(ssl)) != 0) {
            return ret;
        }
    }

    if (ssl->in_msgtype == MBEDTLS_SSL_MSG_CHANGE_CIPHER_SPEC) {
        if (ssl->in_msglen != 1) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("invalid CCS message, len: %" MBEDTLS_PRINTF_SIZET,
                                      ssl->in_msglen));
            return MBEDTLS_ERR_SSL_INVALID_RECORD;
        }

        if (ssl->in_msg[0] != 1) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("invalid CCS message, content: %02x",
                                      ssl->in_msg[0]));
            return MBEDTLS_ERR_SSL_INVALID_RECORD;
        }

#if defined(MBEDTLS_SSL_PROTO_DTLS)
        if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM &&
            ssl->state != MBEDTLS_SSL_CLIENT_CHANGE_CIPHER_SPEC    &&
            ssl->state != MBEDTLS_SSL_SERVER_CHANGE_CIPHER_SPEC) {
            if (ssl->handshake == NULL) {
                MBEDTLS_SSL_DEBUG_MSG(1, ("dropping ChangeCipherSpec outside handshake"));
                return MBEDTLS_ERR_SSL_UNEXPECTED_RECORD;
            }

            MBEDTLS_SSL_DEBUG_MSG(1, ("received out-of-order ChangeCipherSpec - remember"));
            return MBEDTLS_ERR_SSL_EARLY_MESSAGE;
        }
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
        if (ssl->tls_version == MBEDTLS_SSL_VERSION_TLS1_3) {
            MBEDTLS_SSL_DEBUG_MSG(2,
                                  ("Ignore ChangeCipherSpec in TLS 1.3 compatibility mode"));
            return MBEDTLS_ERR_SSL_CONTINUE_PROCESSING;
        }
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */
    }

    if (ssl->in_msgtype == MBEDTLS_SSL_MSG_ALERT) {
        if (ssl->in_msglen != 2) {
            /* Note: Standard allows for more than one 2 byte alert
               to be packed in a single message, but Mbed TLS doesn't
               currently support this. */
            MBEDTLS_SSL_DEBUG_MSG(1, ("invalid alert message, len: %" MBEDTLS_PRINTF_SIZET,
                                      ssl->in_msglen));
            return MBEDTLS_ERR_SSL_INVALID_RECORD;
        }

        MBEDTLS_SSL_DEBUG_MSG(2, ("got an alert message, type: [%u:%u]",
                                  ssl->in_msg[0], ssl->in_msg[1]));

        /*
         * Ignore non-fatal alerts, except close_notify and no_renegotiation
         */
        if (ssl->in_msg[0] == MBEDTLS_SSL_ALERT_LEVEL_FATAL) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("is a fatal alert message (msg %d)",
                                      ssl->in_msg[1]));
            return MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE;
        }

        if (ssl->in_msg[0] == MBEDTLS_SSL_ALERT_LEVEL_WARNING &&
            ssl->in_msg[1] == MBEDTLS_SSL_ALERT_MSG_CLOSE_NOTIFY) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("is a close notify message"));
            return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
        }

#if defined(MBEDTLS_SSL_RENEGOTIATION_ENABLED)
        if (ssl->in_msg[0] == MBEDTLS_SSL_ALERT_LEVEL_WARNING &&
            ssl->in_msg[1] == MBEDTLS_SSL_ALERT_MSG_NO_RENEGOTIATION) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("is a no renegotiation alert"));
            /* Will be handled when trying to parse ServerHello */
            return 0;
        }
#endif
        /* Silently ignore: fetch new message */
        return MBEDTLS_ERR_SSL_NON_FATAL;
    }

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        /* Drop unexpected ApplicationData records,
         * except at the beginning of renegotiations */
        if (ssl->in_msgtype == MBEDTLS_SSL_MSG_APPLICATION_DATA &&
            mbedtls_ssl_is_handshake_over(ssl) == 0
#if defined(MBEDTLS_SSL_RENEGOTIATION)
            && !(ssl->renego_status == MBEDTLS_SSL_RENEGOTIATION_IN_PROGRESS &&
                 ssl->state == MBEDTLS_SSL_SERVER_HELLO)
#endif
            ) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("dropping unexpected ApplicationData"));
            return MBEDTLS_ERR_SSL_NON_FATAL;
        }

        if (ssl->handshake != NULL &&
            mbedtls_ssl_is_handshake_over(ssl) == 1) {
            mbedtls_ssl_handshake_wrapup_free_hs_transform(ssl);
        }
    }
#endif /* MBEDTLS_SSL_PROTO_DTLS */

    return 0;
}

int mbedtls_ssl_send_fatal_handshake_failure(mbedtls_ssl_context *ssl)
{
    return mbedtls_ssl_send_alert_message(ssl,
                                          MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                          MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE);
}

int mbedtls_ssl_send_alert_message(mbedtls_ssl_context *ssl,
                                   unsigned char level,
                                   unsigned char message)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (ssl == NULL || ssl->conf == NULL) {
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    if (ssl->out_left != 0) {
        return mbedtls_ssl_flush_output(ssl);
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> send alert message"));
    MBEDTLS_SSL_DEBUG_MSG(3, ("send alert level=%u message=%u", level, message));

    ssl->out_msgtype = MBEDTLS_SSL_MSG_ALERT;
    ssl->out_msglen = 2;
    ssl->out_msg[0] = level;
    ssl->out_msg[1] = message;

    if ((ret = mbedtls_ssl_write_record(ssl, SSL_FORCE_FLUSH)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_write_record", ret);
        return ret;
    }
    MBEDTLS_SSL_DEBUG_MSG(2, ("<= send alert message"));

    return 0;
}

int mbedtls_ssl_write_change_cipher_spec(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write change cipher spec"));

    ssl->out_msgtype = MBEDTLS_SSL_MSG_CHANGE_CIPHER_SPEC;
    ssl->out_msglen  = 1;
    ssl->out_msg[0]  = 1;

    mbedtls_ssl_handshake_increment_state(ssl);

    if ((ret = mbedtls_ssl_write_handshake_msg(ssl)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_write_handshake_msg", ret);
        return ret;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write change cipher spec"));

    return 0;
}

int mbedtls_ssl_parse_change_cipher_spec(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> parse change cipher spec"));

    if ((ret = mbedtls_ssl_read_record(ssl, 1)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_read_record", ret);
        return ret;
    }

    if (ssl->in_msgtype != MBEDTLS_SSL_MSG_CHANGE_CIPHER_SPEC) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("bad change cipher spec message"));
        mbedtls_ssl_send_alert_message(ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_UNEXPECTED_MESSAGE);
        return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
    }

    /* CCS records are only accepted if they have length 1 and content '1',
     * so we don't need to check this here. */

    /*
     * Switch to our negotiated transform and session parameters for inbound
     * data.
     */
    MBEDTLS_SSL_DEBUG_MSG(3, ("switching to new transform spec for inbound data"));
#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
    ssl->transform_in = ssl->transform_negotiate;
#endif
    ssl->session_in = ssl->session_negotiate;

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
#if defined(MBEDTLS_SSL_DTLS_ANTI_REPLAY)
        mbedtls_ssl_dtls_replay_reset(ssl);
#endif

        /* Increment epoch */
        if (++ssl->in_epoch == 0) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("DTLS epoch would wrap"));
            /* This is highly unlikely to happen for legitimate reasons, so
               treat it as an attack and don't send an alert. */
            return MBEDTLS_ERR_SSL_COUNTER_WRAPPING;
        }
    } else
#endif /* MBEDTLS_SSL_PROTO_DTLS */
    memset(ssl->in_ctr, 0, MBEDTLS_SSL_SEQUENCE_NUMBER_LEN);

    mbedtls_ssl_update_in_pointers(ssl);

    mbedtls_ssl_handshake_increment_state(ssl);

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= parse change cipher spec"));

    return 0;
}

/* Once ssl->out_hdr as the address of the beginning of the
 * next outgoing record is set, deduce the other pointers.
 *
 * Note: For TLS, we save the implicit record sequence number
 *       (entering MAC computation) in the 8 bytes before ssl->out_hdr,
 *       and the caller has to make sure there's space for this.
 */

static size_t ssl_transform_get_explicit_iv_len(
    mbedtls_ssl_transform const *transform)
{
    return transform->ivlen - transform->fixed_ivlen;
}

void mbedtls_ssl_update_out_pointers(mbedtls_ssl_context *ssl,
                                     mbedtls_ssl_transform *transform)
{
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        ssl->out_ctr = ssl->out_hdr +  3;
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
        ssl->out_cid = ssl->out_ctr + MBEDTLS_SSL_SEQUENCE_NUMBER_LEN;
        ssl->out_len = ssl->out_cid;
        if (transform != NULL) {
            ssl->out_len += transform->out_cid_len;
        }
#else /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
        ssl->out_len = ssl->out_ctr + MBEDTLS_SSL_SEQUENCE_NUMBER_LEN;
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
        ssl->out_iv  = ssl->out_len + 2;
    } else
#endif
    {
        ssl->out_len = ssl->out_hdr + 3;
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
        ssl->out_cid = ssl->out_len;
#endif
        ssl->out_iv  = ssl->out_hdr + 5;
    }

    ssl->out_msg = ssl->out_iv;
    /* Adjust out_msg to make space for explicit IV, if used. */
    if (transform != NULL) {
        ssl->out_msg += ssl_transform_get_explicit_iv_len(transform);
    }
}

/* Once ssl->in_hdr as the address of the beginning of the
 * next incoming record is set, deduce the other pointers.
 *
 * Note: For TLS, we save the implicit record sequence number
 *       (entering MAC computation) in the 8 bytes before ssl->in_hdr,
 *       and the caller has to make sure there's space for this.
 */

void mbedtls_ssl_update_in_pointers(mbedtls_ssl_context *ssl)
{
    /* This function sets the pointers to match the case
     * of unprotected TLS/DTLS records, with both  ssl->in_iv
     * and ssl->in_msg pointing to the beginning of the record
     * content.
     *
     * When decrypting a protected record, ssl->in_msg
     * will be shifted to point to the beginning of the
     * record plaintext.
     */

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        /* This sets the header pointers to match records
         * without CID. When we receive a record containing
         * a CID, the fields are shifted accordingly in
         * ssl_parse_record_header(). */
        ssl->in_ctr = ssl->in_hdr +  3;
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
        ssl->in_cid = ssl->in_ctr + MBEDTLS_SSL_SEQUENCE_NUMBER_LEN;
        ssl->in_len = ssl->in_cid; /* Default: no CID */
#else /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
        ssl->in_len = ssl->in_ctr + MBEDTLS_SSL_SEQUENCE_NUMBER_LEN;
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */
        ssl->in_iv  = ssl->in_len + 2;
    } else
#endif
    {
        ssl->in_ctr = ssl->in_buf;
        ssl->in_len = ssl->in_hdr + 3;
#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
        ssl->in_cid = ssl->in_len;
#endif
        ssl->in_iv  = ssl->in_hdr + 5;
    }

    /* This will be adjusted at record decryption time. */
    ssl->in_msg = ssl->in_iv;
}

/*
 * Setup an SSL context
 */

void mbedtls_ssl_reset_in_pointers(mbedtls_ssl_context *ssl)
{
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        ssl->in_hdr = ssl->in_buf;
    } else
#endif  /* MBEDTLS_SSL_PROTO_DTLS */
    {
        ssl->in_hdr = ssl->in_buf + MBEDTLS_SSL_SEQUENCE_NUMBER_LEN;
    }

    /* Derive other internal pointers. */
    mbedtls_ssl_update_in_pointers(ssl);
}

void mbedtls_ssl_reset_out_pointers(mbedtls_ssl_context *ssl)
{
    /* Set the incoming and outgoing record pointers. */
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        ssl->out_hdr = ssl->out_buf;
    } else
#endif /* MBEDTLS_SSL_PROTO_DTLS */
    {
        ssl->out_ctr = ssl->out_buf;
        ssl->out_hdr = ssl->out_buf + MBEDTLS_SSL_SEQUENCE_NUMBER_LEN;
    }
    /* Derive other internal pointers. */
    mbedtls_ssl_update_out_pointers(ssl, NULL /* no transform enabled */);
}

/*
 * SSL get accessors
 */
size_t mbedtls_ssl_get_bytes_avail(const mbedtls_ssl_context *ssl)
{
    return ssl->in_offt == NULL ? 0 : ssl->in_msglen;
}

int mbedtls_ssl_check_pending(const mbedtls_ssl_context *ssl)
{
    /*
     * Case A: We're currently holding back
     * a message for further processing.
     */

    if (ssl->keep_current_message == 1) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("ssl_check_pending: record held back for processing"));
        return 1;
    }

    /*
     * Case B: Further records are pending in the current datagram.
     */

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM &&
        ssl->in_left > ssl->next_record_offset) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("ssl_check_pending: more records within current datagram"));
        return 1;
    }
#endif /* MBEDTLS_SSL_PROTO_DTLS */

    /*
     * Case C: A handshake message is being processed.
     */

    if (ssl->in_hslen > 0 && ssl->in_hslen < ssl->in_msglen) {
        MBEDTLS_SSL_DEBUG_MSG(3,
                              ("ssl_check_pending: more handshake messages within current record"));
        return 1;
    }

    /*
     * Case D: An application data message is being processed
     */
    if (ssl->in_offt != NULL) {
        MBEDTLS_SSL_DEBUG_MSG(3, ("ssl_check_pending: application data record is being processed"));
        return 1;
    }

    /*
     * In all other cases, the rest of the message can be dropped.
     * As in ssl_get_next_record, this needs to be adapted if
     * we implement support for multiple alerts in single records.
     */

    MBEDTLS_SSL_DEBUG_MSG(3, ("ssl_check_pending: nothing pending"));
    return 0;
}


int mbedtls_ssl_get_record_expansion(const mbedtls_ssl_context *ssl)
{
    size_t transform_expansion = 0;
    const mbedtls_ssl_transform *transform = ssl->transform_out;
    unsigned block_size;
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_type_t key_type;
#endif /* MBEDTLS_USE_PSA_CRYPTO */

    size_t out_hdr_len = mbedtls_ssl_out_hdr_len(ssl);

    if (transform == NULL) {
        return (int) out_hdr_len;
    }


#if defined(MBEDTLS_USE_PSA_CRYPTO)
    if (transform->psa_alg == PSA_ALG_GCM ||
        transform->psa_alg == PSA_ALG_CCM ||
        transform->psa_alg == PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, 8) ||
        transform->psa_alg == PSA_ALG_CHACHA20_POLY1305 ||
        transform->psa_alg == MBEDTLS_SSL_NULL_CIPHER) {
        transform_expansion = transform->minlen;
    } else if (transform->psa_alg == PSA_ALG_CBC_NO_PADDING) {
        (void) psa_get_key_attributes(transform->psa_key_enc, &attr);
        key_type = psa_get_key_type(&attr);

        block_size = PSA_BLOCK_CIPHER_BLOCK_LENGTH(key_type);

        /* Expansion due to the addition of the MAC. */
        transform_expansion += transform->maclen;

        /* Expansion due to the addition of CBC padding;
         * Theoretically up to 256 bytes, but we never use
         * more than the block size of the underlying cipher. */
        transform_expansion += block_size;

        /* For TLS 1.2 or higher, an explicit IV is added
         * after the record header. */
#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
        transform_expansion += block_size;
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */
    } else {
        MBEDTLS_SSL_DEBUG_MSG(1,
                              ("Unsupported psa_alg spotted in mbedtls_ssl_get_record_expansion()"));
        return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
#else
    switch (mbedtls_cipher_get_cipher_mode(&transform->cipher_ctx_enc)) {
        case MBEDTLS_MODE_GCM:
        case MBEDTLS_MODE_CCM:
        case MBEDTLS_MODE_CHACHAPOLY:
        case MBEDTLS_MODE_STREAM:
            transform_expansion = transform->minlen;
            break;

        case MBEDTLS_MODE_CBC:

            block_size = mbedtls_cipher_get_block_size(
                &transform->cipher_ctx_enc);

            /* Expansion due to the addition of the MAC. */
            transform_expansion += transform->maclen;

            /* Expansion due to the addition of CBC padding;
             * Theoretically up to 256 bytes, but we never use
             * more than the block size of the underlying cipher. */
            transform_expansion += block_size;

            /* For TLS 1.2 or higher, an explicit IV is added
             * after the record header. */
#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
            transform_expansion += block_size;
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

            break;

        default:
            MBEDTLS_SSL_DEBUG_MSG(1, ("should never happen"));
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    if (transform->out_cid_len != 0) {
        transform_expansion += MBEDTLS_SSL_MAX_CID_EXPANSION;
    }
#endif /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

    return (int) (out_hdr_len + transform_expansion);
}

#if defined(MBEDTLS_SSL_RENEGOTIATION)
/*
 * Check record counters and renegotiate if they're above the limit.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_check_ctr_renegotiate(mbedtls_ssl_context *ssl)
{
    size_t ep_len = mbedtls_ssl_ep_len(ssl);
    int in_ctr_cmp;
    int out_ctr_cmp;

    if (mbedtls_ssl_is_handshake_over(ssl) == 0 ||
        ssl->renego_status == MBEDTLS_SSL_RENEGOTIATION_PENDING ||
        ssl->conf->disable_renegotiation == MBEDTLS_SSL_RENEGOTIATION_DISABLED) {
        return 0;
    }

    in_ctr_cmp = memcmp(ssl->in_ctr + ep_len,
                        &ssl->conf->renego_period[ep_len],
                        MBEDTLS_SSL_SEQUENCE_NUMBER_LEN - ep_len);
    out_ctr_cmp = memcmp(&ssl->cur_out_ctr[ep_len],
                         &ssl->conf->renego_period[ep_len],
                         sizeof(ssl->cur_out_ctr) - ep_len);

    if (in_ctr_cmp <= 0 && out_ctr_cmp <= 0) {
        return 0;
    }

    MBEDTLS_SSL_DEBUG_MSG(1, ("record counter limit reached: renegotiate"));
    return mbedtls_ssl_renegotiate(ssl);
}
#endif /* MBEDTLS_SSL_RENEGOTIATION */

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)

#if defined(MBEDTLS_SSL_CLI_C)
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_is_new_session_ticket(mbedtls_ssl_context *ssl)
{

    if ((ssl->in_hslen == mbedtls_ssl_hs_hdr_len(ssl)) ||
        (ssl->in_msg[0] != MBEDTLS_SSL_HS_NEW_SESSION_TICKET)) {
        return 0;
    }

    return 1;
}
#endif /* MBEDTLS_SSL_CLI_C */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls13_handle_hs_message_post_handshake(mbedtls_ssl_context *ssl)
{

    MBEDTLS_SSL_DEBUG_MSG(3, ("received post-handshake message"));

#if defined(MBEDTLS_SSL_CLI_C)
    if (ssl->conf->endpoint == MBEDTLS_SSL_IS_CLIENT) {
        if (ssl_tls13_is_new_session_ticket(ssl)) {
#if defined(MBEDTLS_SSL_SESSION_TICKETS)
            MBEDTLS_SSL_DEBUG_MSG(3, ("NewSessionTicket received"));
            if (mbedtls_ssl_conf_is_signal_new_session_tickets_enabled(ssl->conf) ==
                MBEDTLS_SSL_TLS1_3_SIGNAL_NEW_SESSION_TICKETS_ENABLED) {
                ssl->keep_current_message = 1;

                mbedtls_ssl_handshake_set_state(ssl,
                                                MBEDTLS_SSL_TLS1_3_NEW_SESSION_TICKET);
                return MBEDTLS_ERR_SSL_WANT_READ;
            } else {
                MBEDTLS_SSL_DEBUG_MSG(3, ("Ignoring NewSessionTicket, handling disabled."));
                return 0;
            }
#else
            MBEDTLS_SSL_DEBUG_MSG(3, ("Ignoring NewSessionTicket, not supported."));
            return 0;
#endif
        }
    }
#endif /* MBEDTLS_SSL_CLI_C */

    /* Fail in all other cases. */
    return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
}
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
/* This function is called from mbedtls_ssl_read() when a handshake message is
 * received after the initial handshake. In this context, handshake messages
 * may only be sent for the purpose of initiating renegotiations.
 *
 * This function is introduced as a separate helper since the handling
 * of post-handshake handshake messages changes significantly in TLS 1.3,
 * and having a helper function allows to distinguish between TLS <= 1.2 and
 * TLS 1.3 in the future without bloating the logic of mbedtls_ssl_read().
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_tls12_handle_hs_message_post_handshake(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    /*
     * - For client-side, expect SERVER_HELLO_REQUEST.
     * - For server-side, expect CLIENT_HELLO.
     * - Fail (TLS) or silently drop record (DTLS) in other cases.
     */

#if defined(MBEDTLS_SSL_CLI_C)
    if (ssl->conf->endpoint == MBEDTLS_SSL_IS_CLIENT &&
        (ssl->in_msg[0] != MBEDTLS_SSL_HS_HELLO_REQUEST ||
         ssl->in_hslen  != mbedtls_ssl_hs_hdr_len(ssl))) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("handshake received (not HelloRequest)"));

        /* With DTLS, drop the packet (probably from last handshake) */
#if defined(MBEDTLS_SSL_PROTO_DTLS)
        if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
            return 0;
        }
#endif
        return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
    }
#endif /* MBEDTLS_SSL_CLI_C */

#if defined(MBEDTLS_SSL_SRV_C)
    if (ssl->conf->endpoint == MBEDTLS_SSL_IS_SERVER &&
        ssl->in_msg[0] != MBEDTLS_SSL_HS_CLIENT_HELLO) {
        MBEDTLS_SSL_DEBUG_MSG(1, ("handshake received (not ClientHello)"));

        /* With DTLS, drop the packet (probably from last handshake) */
#if defined(MBEDTLS_SSL_PROTO_DTLS)
        if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
            return 0;
        }
#endif
        return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
    }
#endif /* MBEDTLS_SSL_SRV_C */

#if defined(MBEDTLS_SSL_RENEGOTIATION)
    /* Determine whether renegotiation attempt should be accepted */
    if (!(ssl->conf->disable_renegotiation == MBEDTLS_SSL_RENEGOTIATION_DISABLED ||
          (ssl->secure_renegotiation == MBEDTLS_SSL_LEGACY_RENEGOTIATION &&
           ssl->conf->allow_legacy_renegotiation ==
           MBEDTLS_SSL_LEGACY_NO_RENEGOTIATION))) {
        /*
         * Accept renegotiation request
         */

        /* DTLS clients need to know renego is server-initiated */
#if defined(MBEDTLS_SSL_PROTO_DTLS)
        if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM &&
            ssl->conf->endpoint == MBEDTLS_SSL_IS_CLIENT) {
            ssl->renego_status = MBEDTLS_SSL_RENEGOTIATION_PENDING;
        }
#endif
        ret = mbedtls_ssl_start_renegotiation(ssl);
        if (ret != MBEDTLS_ERR_SSL_WAITING_SERVER_HELLO_RENEGO &&
            ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_start_renegotiation",
                                  ret);
            return ret;
        }
    } else
#endif /* MBEDTLS_SSL_RENEGOTIATION */
    {
        /*
         * Refuse renegotiation
         */

        MBEDTLS_SSL_DEBUG_MSG(3, ("refusing renegotiation, sending alert"));

        if ((ret = mbedtls_ssl_send_alert_message(ssl,
                                                  MBEDTLS_SSL_ALERT_LEVEL_WARNING,
                                                  MBEDTLS_SSL_ALERT_MSG_NO_RENEGOTIATION)) != 0) {
            return ret;
        }
    }

    return 0;
}
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_handle_hs_message_post_handshake(mbedtls_ssl_context *ssl)
{
    /* Check protocol version and dispatch accordingly. */
#if defined(MBEDTLS_SSL_PROTO_TLS1_3)
    if (ssl->tls_version == MBEDTLS_SSL_VERSION_TLS1_3) {
        return ssl_tls13_handle_hs_message_post_handshake(ssl);
    }
#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */

#if defined(MBEDTLS_SSL_PROTO_TLS1_2)
    if (ssl->tls_version <= MBEDTLS_SSL_VERSION_TLS1_2) {
        return ssl_tls12_handle_hs_message_post_handshake(ssl);
    }
#endif /* MBEDTLS_SSL_PROTO_TLS1_2 */

    /* Should never happen */
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

/*
 * brief          Read at most 'len' application data bytes from the input
 *                buffer.
 *
 * param ssl      SSL context:
 *                - First byte of application data not read yet in the input
 *                  buffer located at address `in_offt`.
 *                - The number of bytes of data not read yet is `in_msglen`.
 * param buf      buffer that will hold the data
 * param len      maximum number of bytes to read
 *
 * note           The function updates the fields `in_offt` and `in_msglen`
 *                according to the number of bytes read.
 *
 * return         The number of bytes read.
 */
static int ssl_read_application_data(
    mbedtls_ssl_context *ssl, unsigned char *buf, size_t len)
{
    size_t n = (len < ssl->in_msglen) ? len : ssl->in_msglen;

    if (len != 0) {
        memcpy(buf, ssl->in_offt, n);
        ssl->in_msglen -= n;
    }

    /* Zeroising the plaintext buffer to erase unused application data
       from the memory. */
    mbedtls_platform_zeroize(ssl->in_offt, n);

    if (ssl->in_msglen == 0) {
        /* all bytes consumed */
        ssl->in_offt = NULL;
        ssl->keep_current_message = 0;
    } else {
        /* more data available */
        ssl->in_offt += n;
    }

    return (int) n;
}

/*
 * Receive application data decrypted from the SSL layer
 */
int mbedtls_ssl_read(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (ssl == NULL || ssl->conf == NULL) {
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> read"));

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        if ((ret = mbedtls_ssl_flush_output(ssl)) != 0) {
            return ret;
        }

        if (ssl->handshake != NULL &&
            ssl->handshake->retransmit_state == MBEDTLS_SSL_RETRANS_SENDING) {
            if ((ret = mbedtls_ssl_flight_transmit(ssl)) != 0) {
                return ret;
            }
        }
    }
#endif

    /*
     * Check if renegotiation is necessary and/or handshake is
     * in process. If yes, perform/continue, and fall through
     * if an unexpected packet is received while the client
     * is waiting for the ServerHello.
     *
     * (There is no equivalent to the last condition on
     *  the server-side as it is not treated as within
     *  a handshake while waiting for the ClientHello
     *  after a renegotiation request.)
     */

#if defined(MBEDTLS_SSL_RENEGOTIATION)
    ret = ssl_check_ctr_renegotiate(ssl);
    if (ret != MBEDTLS_ERR_SSL_WAITING_SERVER_HELLO_RENEGO &&
        ret != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "ssl_check_ctr_renegotiate", ret);
        return ret;
    }
#endif

    if (ssl->state != MBEDTLS_SSL_HANDSHAKE_OVER) {
        ret = mbedtls_ssl_handshake(ssl);
        if (ret != MBEDTLS_ERR_SSL_WAITING_SERVER_HELLO_RENEGO &&
            ret != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_handshake", ret);
            return ret;
        }
    }

    /* Loop as long as no application data record is available */
    while (ssl->in_offt == NULL) {
        /* Start timer if not already running */
        if (ssl->f_get_timer != NULL &&
            ssl->f_get_timer(ssl->p_timer) == -1) {
            mbedtls_ssl_set_timer(ssl, ssl->conf->read_timeout);
        }

        if ((ret = mbedtls_ssl_read_record(ssl, 1)) != 0) {
            if (ret == MBEDTLS_ERR_SSL_CONN_EOF) {
                return 0;
            }

            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_read_record", ret);
            return ret;
        }

        if (ssl->in_msglen  == 0 &&
            ssl->in_msgtype == MBEDTLS_SSL_MSG_APPLICATION_DATA) {
            /*
             * OpenSSL sends empty messages to randomize the IV
             */
            if ((ret = mbedtls_ssl_read_record(ssl, 1)) != 0) {
                if (ret == MBEDTLS_ERR_SSL_CONN_EOF) {
                    return 0;
                }

                MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_read_record", ret);
                return ret;
            }
        }

        if (ssl->in_msgtype == MBEDTLS_SSL_MSG_HANDSHAKE) {
            ret = ssl_handle_hs_message_post_handshake(ssl);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "ssl_handle_hs_message_post_handshake",
                                      ret);
                return ret;
            }

            /* At this point, we don't know whether the renegotiation triggered
             * by the post-handshake message has been completed or not. The cases
             * to consider are the following:
             * 1) The renegotiation is complete. In this case, no new record
             *    has been read yet.
             * 2) The renegotiation is incomplete because the client received
             *    an application data record while awaiting the ServerHello.
             * 3) The renegotiation is incomplete because the client received
             *    a non-handshake, non-application data message while awaiting
             *    the ServerHello.
             *
             * In each of these cases, looping will be the proper action:
             * - For 1), the next iteration will read a new record and check
             *   if it's application data.
             * - For 2), the loop condition isn't satisfied as application data
             *   is present, hence continue is the same as break
             * - For 3), the loop condition is satisfied and read_record
             *   will re-deliver the message that was held back by the client
             *   when expecting the ServerHello.
             */

            continue;
        }
#if defined(MBEDTLS_SSL_RENEGOTIATION)
        else if (ssl->renego_status == MBEDTLS_SSL_RENEGOTIATION_PENDING) {
            if (ssl->conf->renego_max_records >= 0) {
                if (++ssl->renego_records_seen > ssl->conf->renego_max_records) {
                    MBEDTLS_SSL_DEBUG_MSG(1, ("renegotiation requested, "
                                              "but not honored by client"));
                    return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
                }
            }
        }
#endif /* MBEDTLS_SSL_RENEGOTIATION */

        /* Fatal and closure alerts handled by mbedtls_ssl_read_record() */
        if (ssl->in_msgtype == MBEDTLS_SSL_MSG_ALERT) {
            MBEDTLS_SSL_DEBUG_MSG(2, ("ignoring non-fatal non-closure alert"));
            return MBEDTLS_ERR_SSL_WANT_READ;
        }

        if (ssl->in_msgtype != MBEDTLS_SSL_MSG_APPLICATION_DATA) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("bad application data message"));
            return MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE;
        }

        ssl->in_offt = ssl->in_msg;

        /* We're going to return something now, cancel timer,
         * except if handshake (renegotiation) is in progress */
        if (mbedtls_ssl_is_handshake_over(ssl) == 1) {
            mbedtls_ssl_set_timer(ssl, 0);
        }

#if defined(MBEDTLS_SSL_PROTO_DTLS)
        /* If we requested renego but received AppData, resend HelloRequest.
         * Do it now, after setting in_offt, to avoid taking this branch
         * again if ssl_write_hello_request() returns WANT_WRITE */
#if defined(MBEDTLS_SSL_SRV_C) && defined(MBEDTLS_SSL_RENEGOTIATION)
        if (ssl->conf->endpoint == MBEDTLS_SSL_IS_SERVER &&
            ssl->renego_status == MBEDTLS_SSL_RENEGOTIATION_PENDING) {
            if ((ret = mbedtls_ssl_resend_hello_request(ssl)) != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_resend_hello_request",
                                      ret);
                return ret;
            }
        }
#endif /* MBEDTLS_SSL_SRV_C && MBEDTLS_SSL_RENEGOTIATION */
#endif /* MBEDTLS_SSL_PROTO_DTLS */
    }

    ret = ssl_read_application_data(ssl, buf, len);

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= read"));

    return ret;
}

#if defined(MBEDTLS_SSL_SRV_C) && defined(MBEDTLS_SSL_EARLY_DATA)
int mbedtls_ssl_read_early_data(mbedtls_ssl_context *ssl,
                                unsigned char *buf, size_t len)
{
    if (ssl == NULL || (ssl->conf == NULL)) {
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    /*
     * The server may receive early data only while waiting for the End of
     * Early Data handshake message.
     */
    if ((ssl->state != MBEDTLS_SSL_END_OF_EARLY_DATA) ||
        (ssl->in_offt == NULL)) {
        return MBEDTLS_ERR_SSL_CANNOT_READ_EARLY_DATA;
    }

    return ssl_read_application_data(ssl, buf, len);
}
#endif /* MBEDTLS_SSL_SRV_C && MBEDTLS_SSL_EARLY_DATA */

/*
 * Send application data to be encrypted by the SSL layer, taking care of max
 * fragment length and buffer size.
 *
 * According to RFC 5246 Section 6.2.1:
 *
 *      Zero-length fragments of Application data MAY be sent as they are
 *      potentially useful as a traffic analysis countermeasure.
 *
 * Therefore, it is possible that the input message length is 0 and the
 * corresponding return code is 0 on success.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
static int ssl_write_real(mbedtls_ssl_context *ssl,
                          const unsigned char *buf, size_t len)
{
    int ret = mbedtls_ssl_get_max_out_record_payload(ssl);
    const size_t max_len = (size_t) ret;

    if (ret < 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_get_max_out_record_payload", ret);
        return ret;
    }

    if (len > max_len) {
#if defined(MBEDTLS_SSL_PROTO_DTLS)
        if (ssl->conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
            MBEDTLS_SSL_DEBUG_MSG(1, ("fragment larger than the (negotiated) "
                                      "maximum fragment length: %" MBEDTLS_PRINTF_SIZET
                                      " > %" MBEDTLS_PRINTF_SIZET,
                                      len, max_len));
            return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
        } else
#endif
        len = max_len;
    }

    if (ssl->out_left != 0) {
        /*
         * The user has previously tried to send the data and
         * MBEDTLS_ERR_SSL_WANT_WRITE or the message was only partially
         * written. In this case, we expect the high-level write function
         * (e.g. mbedtls_ssl_write()) to be called with the same parameters
         */
        if ((ret = mbedtls_ssl_flush_output(ssl)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_flush_output", ret);
            return ret;
        }
    } else {
        /*
         * The user is trying to send a message the first time, so we need to
         * copy the data into the internal buffers and setup the data structure
         * to keep track of partial writes
         */
        ssl->out_msglen  = len;
        ssl->out_msgtype = MBEDTLS_SSL_MSG_APPLICATION_DATA;
        if (len > 0) {
            memcpy(ssl->out_msg, buf, len);
        }

        if ((ret = mbedtls_ssl_write_record(ssl, SSL_FORCE_FLUSH)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_write_record", ret);
            return ret;
        }
    }

    return (int) len;
}

/*
 * Write application data (public-facing wrapper)
 */
int mbedtls_ssl_write(mbedtls_ssl_context *ssl, const unsigned char *buf, size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write"));

    if (ssl == NULL || ssl->conf == NULL) {
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

#if defined(MBEDTLS_SSL_RENEGOTIATION)
    if ((ret = ssl_check_ctr_renegotiate(ssl)) != 0) {
        MBEDTLS_SSL_DEBUG_RET(1, "ssl_check_ctr_renegotiate", ret);
        return ret;
    }
#endif

    if (ssl->state != MBEDTLS_SSL_HANDSHAKE_OVER) {
        if ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_handshake", ret);
            return ret;
        }
    }

    ret = ssl_write_real(ssl, buf, len);

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write"));

    return ret;
}

#if defined(MBEDTLS_SSL_EARLY_DATA) && defined(MBEDTLS_SSL_CLI_C)
int mbedtls_ssl_write_early_data(mbedtls_ssl_context *ssl,
                                 const unsigned char *buf, size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const struct mbedtls_ssl_config *conf;
    uint32_t remaining;

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write early_data"));

    if (ssl == NULL || (conf = ssl->conf) == NULL) {
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    if (conf->endpoint != MBEDTLS_SSL_IS_CLIENT) {
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    if ((!mbedtls_ssl_conf_is_tls13_enabled(conf)) ||
        (conf->transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) ||
        (conf->early_data_enabled != MBEDTLS_SSL_EARLY_DATA_ENABLED)) {
        return MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA;
    }

    if (ssl->tls_version != MBEDTLS_SSL_VERSION_TLS1_3) {
        return MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA;
    }

    /*
     * If we are at the beginning of the handshake, the early data state being
     * equal to MBEDTLS_SSL_EARLY_DATA_STATE_IDLE or
     * MBEDTLS_SSL_EARLY_DATA_STATE_IND_SENT advance the handshake just
     * enough to be able to send early data if possible. That way, we can
     * guarantee that when starting the handshake with this function we will
     * send at least one record of early data. Note that when the state is
     * MBEDTLS_SSL_EARLY_DATA_STATE_IND_SENT and not yet
     * MBEDTLS_SSL_EARLY_DATA_STATE_CAN_WRITE, we cannot send early data
     * as the early data outbound transform has not been set as we may have to
     * first send a dummy CCS in clear.
     */
    if ((ssl->early_data_state == MBEDTLS_SSL_EARLY_DATA_STATE_IDLE) ||
        (ssl->early_data_state == MBEDTLS_SSL_EARLY_DATA_STATE_IND_SENT)) {
        while ((ssl->early_data_state == MBEDTLS_SSL_EARLY_DATA_STATE_IDLE) ||
               (ssl->early_data_state == MBEDTLS_SSL_EARLY_DATA_STATE_IND_SENT)) {
            ret = mbedtls_ssl_handshake_step(ssl);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_handshake_step", ret);
                return ret;
            }

            ret = mbedtls_ssl_flush_output(ssl);
            if (ret != 0) {
                MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_flush_output", ret);
                return ret;
            }
        }
        remaining = ssl->session_negotiate->max_early_data_size;
    } else {
        /*
         * If we are past the point where we can send early data or we have
         * already reached the maximum early data size, return immediatly.
         * Otherwise, progress the handshake as much as possible to not delay
         * it too much. If we reach a point where we can still send early data,
         * then we will send some.
         */
        if ((ssl->early_data_state != MBEDTLS_SSL_EARLY_DATA_STATE_CAN_WRITE) &&
            (ssl->early_data_state != MBEDTLS_SSL_EARLY_DATA_STATE_ACCEPTED)) {
            return MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA;
        }

        remaining = ssl->session_negotiate->max_early_data_size -
                    ssl->total_early_data_size;

        if (remaining == 0) {
            return MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA;
        }

        ret = mbedtls_ssl_handshake(ssl);
        if ((ret != 0) && (ret != MBEDTLS_ERR_SSL_WANT_READ)) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_handshake", ret);
            return ret;
        }
    }

    if (((ssl->early_data_state != MBEDTLS_SSL_EARLY_DATA_STATE_CAN_WRITE) &&
         (ssl->early_data_state != MBEDTLS_SSL_EARLY_DATA_STATE_ACCEPTED))
        || (remaining == 0)) {
        return MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA;
    }

    if (len > remaining) {
        len = remaining;
    }

    ret = ssl_write_real(ssl, buf, len);
    if (ret >= 0) {
        ssl->total_early_data_size += ret;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write early_data, ret=%d", ret));

    return ret;
}
#endif /* MBEDTLS_SSL_EARLY_DATA && MBEDTLS_SSL_CLI_C */

/*
 * Notify the peer that the connection is being closed
 */
int mbedtls_ssl_close_notify(mbedtls_ssl_context *ssl)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (ssl == NULL || ssl->conf == NULL) {
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("=> write close notify"));

    if (mbedtls_ssl_is_handshake_over(ssl) == 1) {
        if ((ret = mbedtls_ssl_send_alert_message(ssl,
                                                  MBEDTLS_SSL_ALERT_LEVEL_WARNING,
                                                  MBEDTLS_SSL_ALERT_MSG_CLOSE_NOTIFY)) != 0) {
            MBEDTLS_SSL_DEBUG_RET(1, "mbedtls_ssl_send_alert_message", ret);
            return ret;
        }
    }

    MBEDTLS_SSL_DEBUG_MSG(2, ("<= write close notify"));

    return 0;
}

void mbedtls_ssl_transform_free(mbedtls_ssl_transform *transform)
{
    if (transform == NULL) {
        return;
    }

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_destroy_key(transform->psa_key_enc);
    psa_destroy_key(transform->psa_key_dec);
#else
    mbedtls_cipher_free(&transform->cipher_ctx_enc);
    mbedtls_cipher_free(&transform->cipher_ctx_dec);
#endif /* MBEDTLS_USE_PSA_CRYPTO */

#if defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)
#if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_destroy_key(transform->psa_mac_enc);
    psa_destroy_key(transform->psa_mac_dec);
#else
    mbedtls_md_free(&transform->md_ctx_enc);
    mbedtls_md_free(&transform->md_ctx_dec);
#endif /* MBEDTLS_USE_PSA_CRYPTO */
#endif

    mbedtls_platform_zeroize(transform, sizeof(mbedtls_ssl_transform));
}

void mbedtls_ssl_set_inbound_transform(mbedtls_ssl_context *ssl,
                                       mbedtls_ssl_transform *transform)
{
    ssl->transform_in = transform;
    memset(ssl->in_ctr, 0, MBEDTLS_SSL_SEQUENCE_NUMBER_LEN);
}

void mbedtls_ssl_set_outbound_transform(mbedtls_ssl_context *ssl,
                                        mbedtls_ssl_transform *transform)
{
    ssl->transform_out = transform;
    memset(ssl->cur_out_ctr, 0, sizeof(ssl->cur_out_ctr));
}

#if defined(MBEDTLS_SSL_PROTO_DTLS)

void mbedtls_ssl_buffering_free(mbedtls_ssl_context *ssl)
{
    unsigned offset;
    mbedtls_ssl_handshake_params * const hs = ssl->handshake;

    if (hs == NULL) {
        return;
    }

    ssl_free_buffered_record(ssl);

    for (offset = 0; offset < MBEDTLS_SSL_MAX_BUFFERED_HS; offset++) {
        ssl_buffering_free_slot(ssl, offset);
    }
}

static void ssl_buffering_free_slot(mbedtls_ssl_context *ssl,
                                    uint8_t slot)
{
    mbedtls_ssl_handshake_params * const hs = ssl->handshake;
    mbedtls_ssl_hs_buffer * const hs_buf = &hs->buffering.hs[slot];

    if (slot >= MBEDTLS_SSL_MAX_BUFFERED_HS) {
        return;
    }

    if (hs_buf->is_valid == 1) {
        hs->buffering.total_bytes_buffered -= hs_buf->data_len;
        mbedtls_zeroize_and_free(hs_buf->data, hs_buf->data_len);
        memset(hs_buf, 0, sizeof(mbedtls_ssl_hs_buffer));
    }
}

#endif /* MBEDTLS_SSL_PROTO_DTLS */

/*
 * Convert version numbers to/from wire format
 * and, for DTLS, to/from TLS equivalent.
 *
 * For TLS this is the identity.
 * For DTLS, map as follows, then use 1's complement (v -> ~v):
 * 1.x <-> 3.x+1    for x != 0 (DTLS 1.2 based on TLS 1.2)
 *                  DTLS 1.0 is stored as TLS 1.1 internally
 */
void mbedtls_ssl_write_version(unsigned char version[2], int transport,
                               mbedtls_ssl_protocol_version tls_version)
{
    uint16_t tls_version_formatted;
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        tls_version_formatted =
            ~(tls_version - (tls_version == 0x0302 ? 0x0202 : 0x0201));
    } else
#else
    ((void) transport);
#endif
    {
        tls_version_formatted = (uint16_t) tls_version;
    }
    MBEDTLS_PUT_UINT16_BE(tls_version_formatted, version, 0);
}

uint16_t mbedtls_ssl_read_version(const unsigned char version[2],
                                  int transport)
{
    uint16_t tls_version = MBEDTLS_GET_UINT16_BE(version, 0);
#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (transport == MBEDTLS_SSL_TRANSPORT_DATAGRAM) {
        tls_version =
            ~(tls_version - (tls_version == 0xfeff ? 0x0202 : 0x0201));
    }
#else
    ((void) transport);
#endif
    return tls_version;
}

/*
 * Send pending fatal alert.
 * 0,   No alert message.
 * !0,  if mbedtls_ssl_send_alert_message() returned in error, the error code it
 *      returned, ssl->alert_reason otherwise.
 */
int mbedtls_ssl_handle_pending_alert(mbedtls_ssl_context *ssl)
{
    int ret;

    /* No pending alert, return success*/
    if (ssl->send_alert == 0) {
        return 0;
    }

    ret = mbedtls_ssl_send_alert_message(ssl,
                                         MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                         ssl->alert_type);

    /* If mbedtls_ssl_send_alert_message() returned with MBEDTLS_ERR_SSL_WANT_WRITE,
     * do not clear the alert to be able to send it later.
     */
    if (ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
        ssl->send_alert = 0;
    }

    if (ret != 0) {
        return ret;
    }

    return ssl->alert_reason;
}

/*
 * Set pending fatal alert flag.
 */
void mbedtls_ssl_pend_fatal_alert(mbedtls_ssl_context *ssl,
                                  unsigned char alert_type,
                                  int alert_reason)
{
    ssl->send_alert = 1;
    ssl->alert_type = alert_type;
    ssl->alert_reason = alert_reason;
}

#endif /* MBEDTLS_SSL_TLS_C */
