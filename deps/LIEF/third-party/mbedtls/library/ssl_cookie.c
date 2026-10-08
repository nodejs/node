/*
 *  DTLS cookie callbacks implementation
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 * These session callbacks use a simple chained list
 * to store and retrieve the session information.
 */

#include "ssl_misc.h"

#if defined(MBEDTLS_SSL_COOKIE_C)

#include "mbedtls/platform.h"

#include "mbedtls/ssl_cookie.h"
#include "mbedtls/error.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/constant_time.h"

#include <string.h>

#include "mbedtls/psa_util.h"
/* Define a local translating function to save code size by not using too many
 * arguments in each translating place. */
static int local_err_translation(psa_status_t status)
{
    return psa_status_to_mbedtls(status, psa_to_ssl_errors,
                                 ARRAY_LENGTH(psa_to_ssl_errors),
                                 psa_generic_status_to_mbedtls);
}
#define PSA_TO_MBEDTLS_ERR(status) local_err_translation(status)

/*
 * If DTLS is in use, then at least one of SHA-256 or SHA-384 is
 * available. Try SHA-256 first as 384 wastes resources
 */
#if defined(PSA_WANT_ALG_SHA_256)
#define COOKIE_MD           MBEDTLS_MD_SHA256
#define COOKIE_MD_OUTLEN    32
#define COOKIE_HMAC_LEN     28
#elif defined(PSA_WANT_ALG_SHA_384)
#define COOKIE_MD           MBEDTLS_MD_SHA384
#define COOKIE_MD_OUTLEN    48
#define COOKIE_HMAC_LEN     28
#else
#error "DTLS hello verify needs SHA-256 or SHA-384"
#endif

/*
 * Cookies are formed of a 4-bytes timestamp (or serial number) and
 * an HMAC of timestamp and client ID.
 */
#define COOKIE_LEN      (4 + COOKIE_HMAC_LEN)

void mbedtls_ssl_cookie_init(mbedtls_ssl_cookie_ctx *ctx)
{
    ctx->psa_hmac_key = MBEDTLS_SVC_KEY_ID_INIT;
#if !defined(MBEDTLS_HAVE_TIME)
    ctx->serial = 0;
#endif
    ctx->timeout = MBEDTLS_SSL_COOKIE_TIMEOUT;

}

void mbedtls_ssl_cookie_set_timeout(mbedtls_ssl_cookie_ctx *ctx, unsigned long delay)
{
    ctx->timeout = delay;
}

void mbedtls_ssl_cookie_free(mbedtls_ssl_cookie_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }

    psa_destroy_key(ctx->psa_hmac_key);

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_ssl_cookie_ctx));
}

int mbedtls_ssl_cookie_setup(mbedtls_ssl_cookie_ctx *ctx)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_algorithm_t alg;


    alg = mbedtls_md_psa_alg_from_type(COOKIE_MD);
    if (alg == 0) {
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    ctx->psa_hmac_alg = PSA_ALG_TRUNCATED_MAC(PSA_ALG_HMAC(alg),
                                              COOKIE_HMAC_LEN);

    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_MESSAGE |
                            PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, ctx->psa_hmac_alg);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, PSA_BYTES_TO_BITS(COOKIE_MD_OUTLEN));

    if ((status = psa_generate_key(&attributes,
                                   &ctx->psa_hmac_key)) != PSA_SUCCESS) {
        return PSA_TO_MBEDTLS_ERR(status);
    }

    return 0;
}


/*
 * Generate cookie for DTLS ClientHello verification
 */
int mbedtls_ssl_cookie_write(void *p_ctx,
                             unsigned char **p, unsigned char *end,
                             const unsigned char *cli_id, size_t cli_id_len)
{
    psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t sign_mac_length = 0;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ssl_cookie_ctx *ctx = (mbedtls_ssl_cookie_ctx *) p_ctx;
    unsigned long t;

    if (ctx == NULL || cli_id == NULL) {
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    MBEDTLS_SSL_CHK_BUF_PTR(*p, end, COOKIE_LEN);

#if defined(MBEDTLS_HAVE_TIME)
    t = (unsigned long) mbedtls_time(NULL);
#else
    t = ctx->serial++;
#endif

    MBEDTLS_PUT_UINT32_BE(t, *p, 0);
    *p += 4;

    status = psa_mac_sign_setup(&operation, ctx->psa_hmac_key,
                                ctx->psa_hmac_alg);
    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
        goto exit;
    }

    status = psa_mac_update(&operation, *p - 4, 4);
    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
        goto exit;
    }

    status = psa_mac_update(&operation, cli_id, cli_id_len);
    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
        goto exit;
    }

    status = psa_mac_sign_finish(&operation, *p, COOKIE_MD_OUTLEN,
                                 &sign_mac_length);
    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
        goto exit;
    }

    *p += COOKIE_HMAC_LEN;

    ret = 0;

exit:
    status = psa_mac_abort(&operation);
    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
    }
    return ret;
}

/*
 * Check a cookie
 */
int mbedtls_ssl_cookie_check(void *p_ctx,
                             const unsigned char *cookie, size_t cookie_len,
                             const unsigned char *cli_id, size_t cli_id_len)
{
    psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    int ret = 0;
    mbedtls_ssl_cookie_ctx *ctx = (mbedtls_ssl_cookie_ctx *) p_ctx;
    unsigned long cur_time, cookie_time;

    if (ctx == NULL || cli_id == NULL) {
        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    if (cookie_len != COOKIE_LEN) {
        return -1;
    }

    status = psa_mac_verify_setup(&operation, ctx->psa_hmac_key,
                                  ctx->psa_hmac_alg);
    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
        goto exit;
    }

    status = psa_mac_update(&operation, cookie, 4);
    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
        goto exit;
    }

    status = psa_mac_update(&operation, cli_id,
                            cli_id_len);
    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
        goto exit;
    }

    status = psa_mac_verify_finish(&operation, cookie + 4,
                                   COOKIE_HMAC_LEN);
    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
        goto exit;
    }

    ret = 0;

#if defined(MBEDTLS_HAVE_TIME)
    cur_time = (unsigned long) mbedtls_time(NULL);
#else
    cur_time = ctx->serial;
#endif

    cookie_time = (unsigned long) MBEDTLS_GET_UINT32_BE(cookie, 0);

    if (ctx->timeout != 0 && cur_time - cookie_time > ctx->timeout) {
        ret = -1;
        goto exit;
    }

exit:
    status = psa_mac_abort(&operation);
    if (status != PSA_SUCCESS) {
        ret = PSA_TO_MBEDTLS_ERR(status);
    }
    return ret;
}
#endif /* MBEDTLS_SSL_COOKIE_C */
