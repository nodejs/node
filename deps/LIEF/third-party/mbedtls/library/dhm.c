/*
 *  Diffie-Hellman-Merkle key exchange
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 *  The following sources were referenced in the design of this implementation
 *  of the Diffie-Hellman-Merkle algorithm:
 *
 *  [1] Handbook of Applied Cryptography - 1997, Chapter 12
 *      Menezes, van Oorschot and Vanstone
 *
 */

#include "common.h"

#if defined(MBEDTLS_DHM_C)

#include "mbedtls/dhm.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#include <string.h>

#if defined(MBEDTLS_PEM_PARSE_C)
#include "mbedtls/pem.h"
#endif

#if defined(MBEDTLS_ASN1_PARSE_C)
#include "mbedtls/asn1.h"
#endif

#include "mbedtls/platform.h"

#if !defined(MBEDTLS_DHM_ALT)

/*
 * helper to validate the mbedtls_mpi size and import it
 */
static int dhm_read_bignum(mbedtls_mpi *X,
                           unsigned char **p,
                           const unsigned char *end)
{
    int ret, n;

    if (end - *p < 2) {
        return MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    }

    n = MBEDTLS_GET_UINT16_BE(*p, 0);
    (*p) += 2;

    if ((size_t) (end - *p) < (size_t) n) {
        return MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    }

    if ((ret = mbedtls_mpi_read_binary(X, *p, n)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_DHM_READ_PARAMS_FAILED, ret);
    }

    (*p) += n;

    return 0;
}

/*
 * Verify sanity of parameter with regards to P
 *
 * Parameter should be: 2 <= public_param <= P - 2
 *
 * This means that we need to return an error if
 *              public_param < 2 or public_param > P-2
 *
 * For more information on the attack, see:
 *  http://www.cl.cam.ac.uk/~rja14/Papers/psandqs.pdf
 *  http://web.nvd.nist.gov/view/vuln/detail?vulnId=CVE-2005-2643
 */
static int dhm_check_range(const mbedtls_mpi *param, const mbedtls_mpi *P)
{
    mbedtls_mpi U;
    int ret = 0;

    mbedtls_mpi_init(&U);

    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&U, P, 2));

    if (mbedtls_mpi_cmp_int(param, 2) < 0 ||
        mbedtls_mpi_cmp_mpi(param, &U) > 0) {
        ret = MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    }

cleanup:
    mbedtls_mpi_free(&U);
    return ret;
}

void mbedtls_dhm_init(mbedtls_dhm_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_dhm_context));
}

size_t mbedtls_dhm_get_bitlen(const mbedtls_dhm_context *ctx)
{
    return mbedtls_mpi_bitlen(&ctx->P);
}

size_t mbedtls_dhm_get_len(const mbedtls_dhm_context *ctx)
{
    return mbedtls_mpi_size(&ctx->P);
}

int mbedtls_dhm_get_value(const mbedtls_dhm_context *ctx,
                          mbedtls_dhm_parameter param,
                          mbedtls_mpi *dest)
{
    const mbedtls_mpi *src = NULL;
    switch (param) {
        case MBEDTLS_DHM_PARAM_P:
            src = &ctx->P;
            break;
        case MBEDTLS_DHM_PARAM_G:
            src = &ctx->G;
            break;
        case MBEDTLS_DHM_PARAM_X:
            src = &ctx->X;
            break;
        case MBEDTLS_DHM_PARAM_GX:
            src = &ctx->GX;
            break;
        case MBEDTLS_DHM_PARAM_GY:
            src = &ctx->GY;
            break;
        case MBEDTLS_DHM_PARAM_K:
            src = &ctx->K;
            break;
        default:
            return MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    }
    return mbedtls_mpi_copy(dest, src);
}

/*
 * Parse the ServerKeyExchange parameters
 */
int mbedtls_dhm_read_params(mbedtls_dhm_context *ctx,
                            unsigned char **p,
                            const unsigned char *end)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if ((ret = dhm_read_bignum(&ctx->P,  p, end)) != 0 ||
        (ret = dhm_read_bignum(&ctx->G,  p, end)) != 0 ||
        (ret = dhm_read_bignum(&ctx->GY, p, end)) != 0) {
        return ret;
    }

    if ((ret = dhm_check_range(&ctx->GY, &ctx->P)) != 0) {
        return ret;
    }

    return 0;
}

/*
 * Pick a random R in the range [2, M-2] for blinding or key generation.
 */
static int dhm_random_below(mbedtls_mpi *R, const mbedtls_mpi *M,
                            int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret;

    MBEDTLS_MPI_CHK(mbedtls_mpi_random(R, 3, M, f_rng, p_rng));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(R, R, 1));

cleanup:
    return ret;
}

static int dhm_make_common(mbedtls_dhm_context *ctx, int x_size,
                           int (*f_rng)(void *, unsigned char *, size_t),
                           void *p_rng)
{
    int ret = 0;

    if (mbedtls_mpi_cmp_int(&ctx->P, 0) == 0) {
        return MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    }
    if (x_size < 0) {
        return MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    }

    if ((unsigned) x_size < mbedtls_mpi_size(&ctx->P)) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_fill_random(&ctx->X, x_size, f_rng, p_rng));
    } else {
        /* Generate X as large as possible ( <= P - 2 ) */
        ret = dhm_random_below(&ctx->X, &ctx->P, f_rng, p_rng);
        if (ret == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) {
            return MBEDTLS_ERR_DHM_MAKE_PARAMS_FAILED;
        }
        if (ret != 0) {
            return ret;
        }
    }

    /*
     * Calculate GX = G^X mod P
     */
    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&ctx->GX, &ctx->G, &ctx->X,
                                        &ctx->P, &ctx->RP));

    if ((ret = dhm_check_range(&ctx->GX, &ctx->P)) != 0) {
        return ret;
    }

cleanup:
    return ret;
}

/*
 * Setup and write the ServerKeyExchange parameters
 */
int mbedtls_dhm_make_params(mbedtls_dhm_context *ctx, int x_size,
                            unsigned char *output, size_t *olen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret;
    size_t n1, n2, n3;
    unsigned char *p;

    ret = dhm_make_common(ctx, x_size, f_rng, p_rng);
    if (ret != 0) {
        goto cleanup;
    }

    /*
     * Export P, G, GX. RFC 5246 §4.4 states that "leading zero octets are
     * not required". We omit leading zeros for compactness.
     */
#define DHM_MPI_EXPORT(X, n)                                          \
    do {                                                                \
        MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary((X),               \
                                                 p + 2,               \
                                                 (n)));           \
        *p++ = MBEDTLS_BYTE_1(n);                                     \
        *p++ = MBEDTLS_BYTE_0(n);                                     \
        p += (n);                                                     \
    } while (0)

    n1 = mbedtls_mpi_size(&ctx->P);
    n2 = mbedtls_mpi_size(&ctx->G);
    n3 = mbedtls_mpi_size(&ctx->GX);

    p = output;
    DHM_MPI_EXPORT(&ctx->P, n1);
    DHM_MPI_EXPORT(&ctx->G, n2);
    DHM_MPI_EXPORT(&ctx->GX, n3);

    *olen = (size_t) (p - output);

cleanup:
    if (ret != 0 && ret > -128) {
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_DHM_MAKE_PARAMS_FAILED, ret);
    }
    return ret;
}

/*
 * Set prime modulus and generator
 */
int mbedtls_dhm_set_group(mbedtls_dhm_context *ctx,
                          const mbedtls_mpi *P,
                          const mbedtls_mpi *G)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if ((ret = mbedtls_mpi_copy(&ctx->P, P)) != 0 ||
        (ret = mbedtls_mpi_copy(&ctx->G, G)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_DHM_SET_GROUP_FAILED, ret);
    }

    return 0;
}

/*
 * Import the peer's public value G^Y
 */
int mbedtls_dhm_read_public(mbedtls_dhm_context *ctx,
                            const unsigned char *input, size_t ilen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (ilen < 1 || ilen > mbedtls_dhm_get_len(ctx)) {
        return MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    }

    if ((ret = mbedtls_mpi_read_binary(&ctx->GY, input, ilen)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_DHM_READ_PUBLIC_FAILED, ret);
    }

    return 0;
}

/*
 * Create own private value X and export G^X
 */
int mbedtls_dhm_make_public(mbedtls_dhm_context *ctx, int x_size,
                            unsigned char *output, size_t olen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret;

    if (olen < 1 || olen > mbedtls_dhm_get_len(ctx)) {
        return MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    }

    ret = dhm_make_common(ctx, x_size, f_rng, p_rng);
    if (ret == MBEDTLS_ERR_DHM_MAKE_PARAMS_FAILED) {
        return MBEDTLS_ERR_DHM_MAKE_PUBLIC_FAILED;
    }
    if (ret != 0) {
        goto cleanup;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&ctx->GX, output, olen));

cleanup:
    if (ret != 0 && ret > -128) {
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_DHM_MAKE_PUBLIC_FAILED, ret);
    }
    return ret;
}


/*
 * Use the blinding method and optimisation suggested in section 10 of:
 *  KOCHER, Paul C. Timing attacks on implementations of Diffie-Hellman, RSA,
 *  DSS, and other systems. In : Advances in Cryptology-CRYPTO'96. Springer
 *  Berlin Heidelberg, 1996. p. 104-113.
 */
static int dhm_update_blinding(mbedtls_dhm_context *ctx,
                               int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    int ret;
    mbedtls_mpi R;

    mbedtls_mpi_init(&R);

    /*
     * Don't use any blinding the first time a particular X is used,
     * but remember it to use blinding next time.
     */
    if (mbedtls_mpi_cmp_mpi(&ctx->X, &ctx->pX) != 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&ctx->pX, &ctx->X));
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&ctx->Vi, 1));
        MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&ctx->Vf, 1));

        return 0;
    }

    /*
     * Ok, we need blinding. Can we re-use existing values?
     * If yes, just update them by squaring them.
     */
    if (mbedtls_mpi_cmp_int(&ctx->Vi, 1) != 0) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&ctx->Vi, &ctx->Vi, &ctx->Vi));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&ctx->Vi, &ctx->Vi, &ctx->P));

        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&ctx->Vf, &ctx->Vf, &ctx->Vf));
        MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&ctx->Vf, &ctx->Vf, &ctx->P));

        return 0;
    }

    /*
     * We need to generate blinding values from scratch
     */

    /* Vi = random( 2, P-2 ) */
    MBEDTLS_MPI_CHK(dhm_random_below(&ctx->Vi, &ctx->P, f_rng, p_rng));

    /* Vf = Vi^-X mod P
     * First compute Vi^-1 = R * (R Vi)^-1, (avoiding leaks from inv_mod),
     * then elevate to the Xth power. */
    MBEDTLS_MPI_CHK(dhm_random_below(&R, &ctx->P, f_rng, p_rng));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&ctx->Vf, &ctx->Vi, &R));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&ctx->Vf, &ctx->Vf, &ctx->P));
    MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod(&ctx->Vf, &ctx->Vf, &ctx->P));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&ctx->Vf, &ctx->Vf, &R));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&ctx->Vf, &ctx->Vf, &ctx->P));

    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&ctx->Vf, &ctx->Vf, &ctx->X, &ctx->P, &ctx->RP));

cleanup:
    mbedtls_mpi_free(&R);

    return ret;
}

/*
 * Derive and export the shared secret (G^Y)^X mod P
 */
int mbedtls_dhm_calc_secret(mbedtls_dhm_context *ctx,
                            unsigned char *output, size_t output_size, size_t *olen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi GYb;

    if (f_rng == NULL) {
        return MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    }

    if (output_size < mbedtls_dhm_get_len(ctx)) {
        return MBEDTLS_ERR_DHM_BAD_INPUT_DATA;
    }

    if ((ret = dhm_check_range(&ctx->GY, &ctx->P)) != 0) {
        return ret;
    }

    mbedtls_mpi_init(&GYb);

    /* Blind peer's value */
    MBEDTLS_MPI_CHK(dhm_update_blinding(ctx, f_rng, p_rng));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&GYb, &ctx->GY, &ctx->Vi));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&GYb, &GYb, &ctx->P));

    /* Do modular exponentiation */
    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&ctx->K, &GYb, &ctx->X,
                                        &ctx->P, &ctx->RP));

    /* Unblind secret value */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&ctx->K, &ctx->K, &ctx->Vf));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&ctx->K, &ctx->K, &ctx->P));

    /* Output the secret without any leading zero byte. This is mandatory
     * for TLS per RFC 5246 §8.1.2. */
    *olen = mbedtls_mpi_size(&ctx->K);
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&ctx->K, output, *olen));

cleanup:
    mbedtls_mpi_free(&GYb);

    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_DHM_CALC_SECRET_FAILED, ret);
    }

    return 0;
}

/*
 * Free the components of a DHM key
 */
void mbedtls_dhm_free(mbedtls_dhm_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mbedtls_mpi_free(&ctx->pX);
    mbedtls_mpi_free(&ctx->Vf);
    mbedtls_mpi_free(&ctx->Vi);
    mbedtls_mpi_free(&ctx->RP);
    mbedtls_mpi_free(&ctx->K);
    mbedtls_mpi_free(&ctx->GY);
    mbedtls_mpi_free(&ctx->GX);
    mbedtls_mpi_free(&ctx->X);
    mbedtls_mpi_free(&ctx->G);
    mbedtls_mpi_free(&ctx->P);

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_dhm_context));
}

#if defined(MBEDTLS_ASN1_PARSE_C)
/*
 * Parse DHM parameters
 */
int mbedtls_dhm_parse_dhm(mbedtls_dhm_context *dhm, const unsigned char *dhmin,
                          size_t dhminlen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;
    unsigned char *p, *end;
#if defined(MBEDTLS_PEM_PARSE_C)
    mbedtls_pem_context pem;
#endif /* MBEDTLS_PEM_PARSE_C */

#if defined(MBEDTLS_PEM_PARSE_C)
    mbedtls_pem_init(&pem);

    /* Avoid calling mbedtls_pem_read_buffer() on non-null-terminated string */
    if (dhminlen == 0 || dhmin[dhminlen - 1] != '\0') {
        ret = MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT;
    } else {
        ret = mbedtls_pem_read_buffer(&pem,
                                      "-----BEGIN DH PARAMETERS-----",
                                      "-----END DH PARAMETERS-----",
                                      dhmin, NULL, 0, &dhminlen);
    }

    if (ret == 0) {
        /*
         * Was PEM encoded
         */
        dhminlen = pem.buflen;
    } else if (ret != MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT) {
        goto exit;
    }

    p = (ret == 0) ? pem.buf : (unsigned char *) dhmin;
#else
    p = (unsigned char *) dhmin;
#endif /* MBEDTLS_PEM_PARSE_C */
    end = p + dhminlen;

    /*
     *  DHParams ::= SEQUENCE {
     *      prime              INTEGER,  -- P
     *      generator          INTEGER,  -- g
     *      privateValueLength INTEGER OPTIONAL
     *  }
     */
    if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
                                    MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_DHM_INVALID_FORMAT, ret);
        goto exit;
    }

    end = p + len;

    if ((ret = mbedtls_asn1_get_mpi(&p, end, &dhm->P)) != 0 ||
        (ret = mbedtls_asn1_get_mpi(&p, end, &dhm->G)) != 0) {
        ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_DHM_INVALID_FORMAT, ret);
        goto exit;
    }

    if (p != end) {
        /* This might be the optional privateValueLength.
         * If so, we can cleanly discard it */
        mbedtls_mpi rec;
        mbedtls_mpi_init(&rec);
        ret = mbedtls_asn1_get_mpi(&p, end, &rec);
        mbedtls_mpi_free(&rec);
        if (ret != 0) {
            ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_DHM_INVALID_FORMAT, ret);
            goto exit;
        }
        if (p != end) {
            ret = MBEDTLS_ERROR_ADD(MBEDTLS_ERR_DHM_INVALID_FORMAT,
                                    MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
            goto exit;
        }
    }

    ret = 0;

exit:
#if defined(MBEDTLS_PEM_PARSE_C)
    mbedtls_pem_free(&pem);
#endif
    if (ret != 0) {
        mbedtls_dhm_free(dhm);
    }

    return ret;
}

#if defined(MBEDTLS_FS_IO)
/*
 * Load all data from a file into a given buffer.
 *
 * The file is expected to contain either PEM or DER encoded data.
 * A terminating null byte is always appended. It is included in the announced
 * length only if the data looks like it is PEM encoded.
 */
static int load_file(const char *path, unsigned char **buf, size_t *n)
{
    FILE *f;
    long size;

    if ((f = fopen(path, "rb")) == NULL) {
        return MBEDTLS_ERR_DHM_FILE_IO_ERROR;
    }
    /* The data loaded here is public, so don't bother disabling buffering. */

    fseek(f, 0, SEEK_END);
    if ((size = ftell(f)) == -1) {
        fclose(f);
        return MBEDTLS_ERR_DHM_FILE_IO_ERROR;
    }
    fseek(f, 0, SEEK_SET);

    *n = (size_t) size;

    if (*n + 1 == 0 ||
        (*buf = mbedtls_calloc(1, *n + 1)) == NULL) {
        fclose(f);
        return MBEDTLS_ERR_DHM_ALLOC_FAILED;
    }

    if (fread(*buf, 1, *n, f) != *n) {
        fclose(f);

        mbedtls_zeroize_and_free(*buf, *n + 1);

        return MBEDTLS_ERR_DHM_FILE_IO_ERROR;
    }

    fclose(f);

    (*buf)[*n] = '\0';

    if (strstr((const char *) *buf, "-----BEGIN ") != NULL) {
        ++*n;
    }

    return 0;
}

/*
 * Load and parse DHM parameters
 */
int mbedtls_dhm_parse_dhmfile(mbedtls_dhm_context *dhm, const char *path)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n;
    unsigned char *buf;

    if ((ret = load_file(path, &buf, &n)) != 0) {
        return ret;
    }

    ret = mbedtls_dhm_parse_dhm(dhm, buf, n);

    mbedtls_zeroize_and_free(buf, n);

    return ret;
}
#endif /* MBEDTLS_FS_IO */
#endif /* MBEDTLS_ASN1_PARSE_C */
#endif /* MBEDTLS_DHM_ALT */

#if defined(MBEDTLS_SELF_TEST)

#if defined(MBEDTLS_PEM_PARSE_C)
static const char mbedtls_test_dhm_params[] =
    "-----BEGIN DH PARAMETERS-----\r\n"
    "MIGHAoGBAJ419DBEOgmQTzo5qXl5fQcN9TN455wkOL7052HzxxRVMyhYmwQcgJvh\r\n"
    "1sa18fyfR9OiVEMYglOpkqVoGLN7qd5aQNNi5W7/C+VBdHTBJcGZJyyP5B3qcz32\r\n"
    "9mLJKudlVudV0Qxk5qUJaPZ/xupz0NyoVpviuiBOI1gNi8ovSXWzAgEC\r\n"
    "-----END DH PARAMETERS-----\r\n";
#else /* MBEDTLS_PEM_PARSE_C */
static const char mbedtls_test_dhm_params[] = {
    0x30, 0x81, 0x87, 0x02, 0x81, 0x81, 0x00, 0x9e, 0x35, 0xf4, 0x30, 0x44,
    0x3a, 0x09, 0x90, 0x4f, 0x3a, 0x39, 0xa9, 0x79, 0x79, 0x7d, 0x07, 0x0d,
    0xf5, 0x33, 0x78, 0xe7, 0x9c, 0x24, 0x38, 0xbe, 0xf4, 0xe7, 0x61, 0xf3,
    0xc7, 0x14, 0x55, 0x33, 0x28, 0x58, 0x9b, 0x04, 0x1c, 0x80, 0x9b, 0xe1,
    0xd6, 0xc6, 0xb5, 0xf1, 0xfc, 0x9f, 0x47, 0xd3, 0xa2, 0x54, 0x43, 0x18,
    0x82, 0x53, 0xa9, 0x92, 0xa5, 0x68, 0x18, 0xb3, 0x7b, 0xa9, 0xde, 0x5a,
    0x40, 0xd3, 0x62, 0xe5, 0x6e, 0xff, 0x0b, 0xe5, 0x41, 0x74, 0x74, 0xc1,
    0x25, 0xc1, 0x99, 0x27, 0x2c, 0x8f, 0xe4, 0x1d, 0xea, 0x73, 0x3d, 0xf6,
    0xf6, 0x62, 0xc9, 0x2a, 0xe7, 0x65, 0x56, 0xe7, 0x55, 0xd1, 0x0c, 0x64,
    0xe6, 0xa5, 0x09, 0x68, 0xf6, 0x7f, 0xc6, 0xea, 0x73, 0xd0, 0xdc, 0xa8,
    0x56, 0x9b, 0xe2, 0xba, 0x20, 0x4e, 0x23, 0x58, 0x0d, 0x8b, 0xca, 0x2f,
    0x49, 0x75, 0xb3, 0x02, 0x01, 0x02
};
#endif /* MBEDTLS_PEM_PARSE_C */

static const size_t mbedtls_test_dhm_params_len = sizeof(mbedtls_test_dhm_params);

/*
 * Checkup routine
 */
int mbedtls_dhm_self_test(int verbose)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_dhm_context dhm;

    mbedtls_dhm_init(&dhm);

    if (verbose != 0) {
        mbedtls_printf("  DHM parameter load: ");
    }

    if ((ret = mbedtls_dhm_parse_dhm(&dhm,
                                     (const unsigned char *) mbedtls_test_dhm_params,
                                     mbedtls_test_dhm_params_len)) != 0) {
        if (verbose != 0) {
            mbedtls_printf("failed\n");
        }

        ret = 1;
        goto exit;
    }

    if (verbose != 0) {
        mbedtls_printf("passed\n\n");
    }

exit:
    mbedtls_dhm_free(&dhm);

    return ret;
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_DHM_C */
