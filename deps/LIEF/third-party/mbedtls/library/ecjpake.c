/*
 *  Elliptic curve J-PAKE
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * References in the code are to the Thread v1.0 Specification,
 * available to members of the Thread Group http://threadgroup.org/
 */

#include "common.h"

#if defined(MBEDTLS_ECJPAKE_C)

#include "mbedtls/ecjpake.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#include <string.h>

#if !defined(MBEDTLS_ECJPAKE_ALT)

/*
 * Convert a mbedtls_ecjpake_role to identifier string
 */
static const char * const ecjpake_id[] = {
    "client",
    "server"
};

#define ID_MINE     (ecjpake_id[ctx->role])
#define ID_PEER     (ecjpake_id[1 - ctx->role])

/**
 * Helper to Compute a hash from md_type
 */
static int mbedtls_ecjpake_compute_hash(mbedtls_md_type_t md_type,
                                        const unsigned char *input, size_t ilen,
                                        unsigned char *output)
{
    return mbedtls_md(mbedtls_md_info_from_type(md_type),
                      input, ilen, output);
}

/*
 * Initialize context
 */
void mbedtls_ecjpake_init(mbedtls_ecjpake_context *ctx)
{
    ctx->md_type = MBEDTLS_MD_NONE;
    mbedtls_ecp_group_init(&ctx->grp);
    ctx->point_format = MBEDTLS_ECP_PF_UNCOMPRESSED;

    mbedtls_ecp_point_init(&ctx->Xm1);
    mbedtls_ecp_point_init(&ctx->Xm2);
    mbedtls_ecp_point_init(&ctx->Xp1);
    mbedtls_ecp_point_init(&ctx->Xp2);
    mbedtls_ecp_point_init(&ctx->Xp);

    mbedtls_mpi_init(&ctx->xm1);
    mbedtls_mpi_init(&ctx->xm2);
    mbedtls_mpi_init(&ctx->s);
}

/*
 * Free context
 */
void mbedtls_ecjpake_free(mbedtls_ecjpake_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->md_type = MBEDTLS_MD_NONE;
    mbedtls_ecp_group_free(&ctx->grp);

    mbedtls_ecp_point_free(&ctx->Xm1);
    mbedtls_ecp_point_free(&ctx->Xm2);
    mbedtls_ecp_point_free(&ctx->Xp1);
    mbedtls_ecp_point_free(&ctx->Xp2);
    mbedtls_ecp_point_free(&ctx->Xp);

    mbedtls_mpi_free(&ctx->xm1);
    mbedtls_mpi_free(&ctx->xm2);
    mbedtls_mpi_free(&ctx->s);
}

/*
 * Setup context
 */
int mbedtls_ecjpake_setup(mbedtls_ecjpake_context *ctx,
                          mbedtls_ecjpake_role role,
                          mbedtls_md_type_t hash,
                          mbedtls_ecp_group_id curve,
                          const unsigned char *secret,
                          size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (role != MBEDTLS_ECJPAKE_CLIENT && role != MBEDTLS_ECJPAKE_SERVER) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    ctx->role = role;

    if ((mbedtls_md_info_from_type(hash)) == NULL) {
        return MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE;
    }

    ctx->md_type = hash;

    MBEDTLS_MPI_CHK(mbedtls_ecp_group_load(&ctx->grp, curve));

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&ctx->s, secret, len));

cleanup:
    if (ret != 0) {
        mbedtls_ecjpake_free(ctx);
    }

    return ret;
}

int mbedtls_ecjpake_set_point_format(mbedtls_ecjpake_context *ctx,
                                     int point_format)
{
    switch (point_format) {
        case MBEDTLS_ECP_PF_UNCOMPRESSED:
        case MBEDTLS_ECP_PF_COMPRESSED:
            ctx->point_format = point_format;
            return 0;
        default:
            return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }
}

/*
 * Check if context is ready for use
 */
int mbedtls_ecjpake_check(const mbedtls_ecjpake_context *ctx)
{
    if (ctx->md_type == MBEDTLS_MD_NONE ||
        ctx->grp.id == MBEDTLS_ECP_DP_NONE ||
        ctx->s.p == NULL) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    return 0;
}

/*
 * Write a point plus its length to a buffer
 */
static int ecjpake_write_len_point(unsigned char **p,
                                   const unsigned char *end,
                                   const mbedtls_ecp_group *grp,
                                   const int pf,
                                   const mbedtls_ecp_point *P)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;

    /* Need at least 4 for length plus 1 for point */
    if (end < *p || end - *p < 5) {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    ret = mbedtls_ecp_point_write_binary(grp, P, pf,
                                         &len, *p + 4, (size_t) (end - (*p + 4)));
    if (ret != 0) {
        return ret;
    }

    MBEDTLS_PUT_UINT32_BE(len, *p, 0);

    *p += 4 + len;

    return 0;
}

/*
 * Size of the temporary buffer for ecjpake_hash:
 * 3 EC points plus their length, plus ID and its length (4 + 6 bytes)
 */
#define ECJPAKE_HASH_BUF_LEN    (3 * (4 + MBEDTLS_ECP_MAX_PT_LEN) + 4 + 6)

/*
 * Compute hash for ZKP (7.4.2.2.2.1)
 */
static int ecjpake_hash(const mbedtls_md_type_t md_type,
                        const mbedtls_ecp_group *grp,
                        const int pf,
                        const mbedtls_ecp_point *G,
                        const mbedtls_ecp_point *V,
                        const mbedtls_ecp_point *X,
                        const char *id,
                        mbedtls_mpi *h)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char buf[ECJPAKE_HASH_BUF_LEN];
    unsigned char *p = buf;
    const unsigned char *end = buf + sizeof(buf);
    const size_t id_len = strlen(id);
    unsigned char hash[MBEDTLS_MD_MAX_SIZE];

    /* Write things to temporary buffer */
    MBEDTLS_MPI_CHK(ecjpake_write_len_point(&p, end, grp, pf, G));
    MBEDTLS_MPI_CHK(ecjpake_write_len_point(&p, end, grp, pf, V));
    MBEDTLS_MPI_CHK(ecjpake_write_len_point(&p, end, grp, pf, X));

    if (end - p < 4) {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    MBEDTLS_PUT_UINT32_BE(id_len, p, 0);
    p += 4;

    if (end < p || (size_t) (end - p) < id_len) {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    memcpy(p, id, id_len);
    p += id_len;

    /* Compute hash */
    MBEDTLS_MPI_CHK(mbedtls_ecjpake_compute_hash(md_type,
                                                 buf, (size_t) (p - buf), hash));

    /* Turn it into an integer mod n */
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(h, hash,
                                            mbedtls_md_get_size_from_type(md_type)));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(h, h, &grp->N));

cleanup:
    return ret;
}

/*
 * Parse a ECShnorrZKP (7.4.2.2.2) and verify it (7.4.2.3.3)
 */
static int ecjpake_zkp_read(const mbedtls_md_type_t md_type,
                            const mbedtls_ecp_group *grp,
                            const int pf,
                            const mbedtls_ecp_point *G,
                            const mbedtls_ecp_point *X,
                            const char *id,
                            const unsigned char **p,
                            const unsigned char *end)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_point V, VV;
    mbedtls_mpi r, h;
    size_t r_len;

    mbedtls_ecp_point_init(&V);
    mbedtls_ecp_point_init(&VV);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&h);

    /*
     * struct {
     *     ECPoint V;
     *     opaque r<1..2^8-1>;
     * } ECSchnorrZKP;
     */
    if (end < *p) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    MBEDTLS_MPI_CHK(mbedtls_ecp_tls_read_point(grp, &V, p, (size_t) (end - *p)));

    if (end < *p || (size_t) (end - *p) < 1) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

    r_len = *(*p)++;

    if (end < *p || (size_t) (end - *p) < r_len || r_len == 0) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&r, *p, r_len));
    *p += r_len;

    /*
     * Verification
     */
    MBEDTLS_MPI_CHK(ecjpake_hash(md_type, grp, pf, G, &V, X, id, &h));
    MBEDTLS_MPI_CHK(mbedtls_ecp_muladd((mbedtls_ecp_group *) grp,
                                       &VV, &h, X, &r, G));

    if (mbedtls_ecp_point_cmp(&VV, &V) != 0) {
        ret = MBEDTLS_ERR_ECP_VERIFY_FAILED;
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_point_free(&V);
    mbedtls_ecp_point_free(&VV);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&h);

    return ret;
}

/*
 * Generate ZKP (7.4.2.3.2) and write it as ECSchnorrZKP (7.4.2.2.2)
 */
static int ecjpake_zkp_write(const mbedtls_md_type_t md_type,
                             const mbedtls_ecp_group *grp,
                             const int pf,
                             const mbedtls_ecp_point *G,
                             const mbedtls_mpi *x,
                             const mbedtls_ecp_point *X,
                             const char *id,
                             unsigned char **p,
                             const unsigned char *end,
                             int (*f_rng)(void *, unsigned char *, size_t),
                             void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_point V;
    mbedtls_mpi v;
    mbedtls_mpi h; /* later recycled to hold r */
    size_t len;

    if (end < *p) {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    mbedtls_ecp_point_init(&V);
    mbedtls_mpi_init(&v);
    mbedtls_mpi_init(&h);

    /* Compute signature */
    MBEDTLS_MPI_CHK(mbedtls_ecp_gen_keypair_base((mbedtls_ecp_group *) grp,
                                                 G, &v, &V, f_rng, p_rng));
    MBEDTLS_MPI_CHK(ecjpake_hash(md_type, grp, pf, G, &V, X, id, &h));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&h, &h, x));     /* x*h */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&h, &v, &h));     /* v - x*h */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&h, &h, &grp->N));     /* r */

    /* Write it out */
    MBEDTLS_MPI_CHK(mbedtls_ecp_tls_write_point(grp, &V,
                                                pf, &len, *p, (size_t) (end - *p)));
    *p += len;

    len = mbedtls_mpi_size(&h);   /* actually r */
    if (end < *p || (size_t) (end - *p) < 1 + len || len > 255) {
        ret = MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    *(*p)++ = MBEDTLS_BYTE_0(len);
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&h, *p, len));     /* r */
    *p += len;

cleanup:
    mbedtls_ecp_point_free(&V);
    mbedtls_mpi_free(&v);
    mbedtls_mpi_free(&h);

    return ret;
}

/*
 * Parse a ECJPAKEKeyKP (7.4.2.2.1) and check proof
 * Output: verified public key X
 */
static int ecjpake_kkp_read(const mbedtls_md_type_t md_type,
                            const mbedtls_ecp_group *grp,
                            const int pf,
                            const mbedtls_ecp_point *G,
                            mbedtls_ecp_point *X,
                            const char *id,
                            const unsigned char **p,
                            const unsigned char *end)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    if (end < *p) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    /*
     * struct {
     *     ECPoint X;
     *     ECSchnorrZKP zkp;
     * } ECJPAKEKeyKP;
     */
    MBEDTLS_MPI_CHK(mbedtls_ecp_tls_read_point(grp, X, p, (size_t) (end - *p)));
    if (mbedtls_ecp_is_zero(X)) {
        ret = MBEDTLS_ERR_ECP_INVALID_KEY;
        goto cleanup;
    }

    MBEDTLS_MPI_CHK(ecjpake_zkp_read(md_type, grp, pf, G, X, id, p, end));

cleanup:
    return ret;
}

/*
 * Generate an ECJPAKEKeyKP
 * Output: the serialized structure, plus private/public key pair
 */
static int ecjpake_kkp_write(const mbedtls_md_type_t md_type,
                             const mbedtls_ecp_group *grp,
                             const int pf,
                             const mbedtls_ecp_point *G,
                             mbedtls_mpi *x,
                             mbedtls_ecp_point *X,
                             const char *id,
                             unsigned char **p,
                             const unsigned char *end,
                             int (*f_rng)(void *, unsigned char *, size_t),
                             void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t len;

    if (end < *p) {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    /* Generate key (7.4.2.3.1) and write it out */
    MBEDTLS_MPI_CHK(mbedtls_ecp_gen_keypair_base((mbedtls_ecp_group *) grp, G, x, X,
                                                 f_rng, p_rng));
    MBEDTLS_MPI_CHK(mbedtls_ecp_tls_write_point(grp, X,
                                                pf, &len, *p, (size_t) (end - *p)));
    *p += len;

    /* Generate and write proof */
    MBEDTLS_MPI_CHK(ecjpake_zkp_write(md_type, grp, pf, G, x, X, id,
                                      p, end, f_rng, p_rng));

cleanup:
    return ret;
}

/*
 * Read a ECJPAKEKeyKPPairList (7.4.2.3) and check proofs
 * Outputs: verified peer public keys Xa, Xb
 */
static int ecjpake_kkpp_read(const mbedtls_md_type_t md_type,
                             const mbedtls_ecp_group *grp,
                             const int pf,
                             const mbedtls_ecp_point *G,
                             mbedtls_ecp_point *Xa,
                             mbedtls_ecp_point *Xb,
                             const char *id,
                             const unsigned char *buf,
                             size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const unsigned char *p = buf;
    const unsigned char *end = buf + len;

    /*
     * struct {
     *     ECJPAKEKeyKP ecjpake_key_kp_pair_list[2];
     * } ECJPAKEKeyKPPairList;
     */
    MBEDTLS_MPI_CHK(ecjpake_kkp_read(md_type, grp, pf, G, Xa, id, &p, end));
    MBEDTLS_MPI_CHK(ecjpake_kkp_read(md_type, grp, pf, G, Xb, id, &p, end));

    if (p != end) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

cleanup:
    return ret;
}

/*
 * Generate a ECJPAKEKeyKPPairList
 * Outputs: the serialized structure, plus two private/public key pairs
 */
static int ecjpake_kkpp_write(const mbedtls_md_type_t md_type,
                              const mbedtls_ecp_group *grp,
                              const int pf,
                              const mbedtls_ecp_point *G,
                              mbedtls_mpi *xm1,
                              mbedtls_ecp_point *Xa,
                              mbedtls_mpi *xm2,
                              mbedtls_ecp_point *Xb,
                              const char *id,
                              unsigned char *buf,
                              size_t len,
                              size_t *olen,
                              int (*f_rng)(void *, unsigned char *, size_t),
                              void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *p = buf;
    const unsigned char *end = buf + len;

    MBEDTLS_MPI_CHK(ecjpake_kkp_write(md_type, grp, pf, G, xm1, Xa, id,
                                      &p, end, f_rng, p_rng));
    MBEDTLS_MPI_CHK(ecjpake_kkp_write(md_type, grp, pf, G, xm2, Xb, id,
                                      &p, end, f_rng, p_rng));

    *olen = (size_t) (p - buf);

cleanup:
    return ret;
}

/*
 * Read and process the first round message
 */
int mbedtls_ecjpake_read_round_one(mbedtls_ecjpake_context *ctx,
                                   const unsigned char *buf,
                                   size_t len)
{
    return ecjpake_kkpp_read(ctx->md_type, &ctx->grp, ctx->point_format,
                             &ctx->grp.G,
                             &ctx->Xp1, &ctx->Xp2, ID_PEER,
                             buf, len);
}

/*
 * Generate and write the first round message
 */
int mbedtls_ecjpake_write_round_one(mbedtls_ecjpake_context *ctx,
                                    unsigned char *buf, size_t len, size_t *olen,
                                    int (*f_rng)(void *, unsigned char *, size_t),
                                    void *p_rng)
{
    return ecjpake_kkpp_write(ctx->md_type, &ctx->grp, ctx->point_format,
                              &ctx->grp.G,
                              &ctx->xm1, &ctx->Xm1, &ctx->xm2, &ctx->Xm2,
                              ID_MINE, buf, len, olen, f_rng, p_rng);
}

/*
 * Compute the sum of three points R = A + B + C
 */
static int ecjpake_ecp_add3(mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                            const mbedtls_ecp_point *A,
                            const mbedtls_ecp_point *B,
                            const mbedtls_ecp_point *C)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi one;

    mbedtls_mpi_init(&one);

    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&one, 1));
    MBEDTLS_MPI_CHK(mbedtls_ecp_muladd(grp, R, &one, A, &one, B));
    MBEDTLS_MPI_CHK(mbedtls_ecp_muladd(grp, R, &one, R, &one, C));

cleanup:
    mbedtls_mpi_free(&one);

    return ret;
}

/*
 * Read and process second round message (C: 7.4.2.5, S: 7.4.2.6)
 */
int mbedtls_ecjpake_read_round_two(mbedtls_ecjpake_context *ctx,
                                   const unsigned char *buf,
                                   size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    const unsigned char *p = buf;
    const unsigned char *end = buf + len;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point G;    /* C: GB, S: GA */

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&G);

    /*
     * Server: GA = X3  + X4  + X1      (7.4.2.6.1)
     * Client: GB = X1  + X2  + X3      (7.4.2.5.1)
     * Unified: G = Xm1 + Xm2 + Xp1
     * We need that before parsing in order to check Xp as we read it
     */
    MBEDTLS_MPI_CHK(ecjpake_ecp_add3(&ctx->grp, &G,
                                     &ctx->Xm1, &ctx->Xm2, &ctx->Xp1));

    /*
     * struct {
     *     ECParameters curve_params;   // only client reading server msg
     *     ECJPAKEKeyKP ecjpake_key_kp;
     * } Client/ServerECJPAKEParams;
     */
    if (ctx->role == MBEDTLS_ECJPAKE_CLIENT) {
        MBEDTLS_MPI_CHK(mbedtls_ecp_tls_read_group(&grp, &p, len));
        if (grp.id != ctx->grp.id) {
            ret = MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
            goto cleanup;
        }
    }

    MBEDTLS_MPI_CHK(ecjpake_kkp_read(ctx->md_type, &ctx->grp,
                                     ctx->point_format,
                                     &G, &ctx->Xp, ID_PEER, &p, end));

    if (p != end) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&G);

    return ret;
}

/*
 * Compute R = +/- X * S mod N, taking care not to leak S
 */
static int ecjpake_mul_secret(mbedtls_mpi *R, int sign,
                              const mbedtls_mpi *X,
                              const mbedtls_mpi *S,
                              const mbedtls_mpi *N,
                              int (*f_rng)(void *, unsigned char *, size_t),
                              void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi b; /* Blinding value, then s + N * blinding */

    mbedtls_mpi_init(&b);

    /* b = s + rnd-128-bit * N */
    MBEDTLS_MPI_CHK(mbedtls_mpi_fill_random(&b, 16, f_rng, p_rng));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&b, &b, N));
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&b, &b, S));

    /* R = sign * X * b mod N */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(R, X, &b));
    R->s *= sign;
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(R, R, N));

cleanup:
    mbedtls_mpi_free(&b);

    return ret;
}

/*
 * Generate and write the second round message (S: 7.4.2.5, C: 7.4.2.6)
 */
int mbedtls_ecjpake_write_round_two(mbedtls_ecjpake_context *ctx,
                                    unsigned char *buf, size_t len, size_t *olen,
                                    int (*f_rng)(void *, unsigned char *, size_t),
                                    void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_point G;    /* C: GA, S: GB */
    mbedtls_ecp_point Xm;   /* C: Xc, S: Xs */
    mbedtls_mpi xm;         /* C: xc, S: xs */
    unsigned char *p = buf;
    const unsigned char *end = buf + len;
    size_t ec_len;

    mbedtls_ecp_point_init(&G);
    mbedtls_ecp_point_init(&Xm);
    mbedtls_mpi_init(&xm);

    /*
     * First generate private/public key pair (S: 7.4.2.5.1, C: 7.4.2.6.1)
     *
     * Client:  GA = X1  + X3  + X4  | xs = x2  * s | Xc = xc * GA
     * Server:  GB = X3  + X1  + X2  | xs = x4  * s | Xs = xs * GB
     * Unified: G  = Xm1 + Xp1 + Xp2 | xm = xm2 * s | Xm = xm * G
     */
    MBEDTLS_MPI_CHK(ecjpake_ecp_add3(&ctx->grp, &G,
                                     &ctx->Xp1, &ctx->Xp2, &ctx->Xm1));
    MBEDTLS_MPI_CHK(ecjpake_mul_secret(&xm, 1, &ctx->xm2, &ctx->s,
                                       &ctx->grp.N, f_rng, p_rng));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&ctx->grp, &Xm, &xm, &G, f_rng, p_rng));

    /*
     * Now write things out
     *
     * struct {
     *     ECParameters curve_params;   // only server writing its message
     *     ECJPAKEKeyKP ecjpake_key_kp;
     * } Client/ServerECJPAKEParams;
     */
    if (ctx->role == MBEDTLS_ECJPAKE_SERVER) {
        if (end < p) {
            ret = MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
            goto cleanup;
        }
        MBEDTLS_MPI_CHK(mbedtls_ecp_tls_write_group(&ctx->grp, &ec_len,
                                                    p, (size_t) (end - p)));
        p += ec_len;
    }

    if (end < p) {
        ret = MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    MBEDTLS_MPI_CHK(mbedtls_ecp_tls_write_point(&ctx->grp, &Xm,
                                                ctx->point_format, &ec_len, p, (size_t) (end - p)));
    p += ec_len;

    MBEDTLS_MPI_CHK(ecjpake_zkp_write(ctx->md_type, &ctx->grp,
                                      ctx->point_format,
                                      &G, &xm, &Xm, ID_MINE,
                                      &p, end, f_rng, p_rng));

    *olen = (size_t) (p - buf);

cleanup:
    mbedtls_ecp_point_free(&G);
    mbedtls_ecp_point_free(&Xm);
    mbedtls_mpi_free(&xm);

    return ret;
}

/*
 * Derive PMS (7.4.2.7 / 7.4.2.8)
 */
static int mbedtls_ecjpake_derive_k(mbedtls_ecjpake_context *ctx,
                                    mbedtls_ecp_point *K,
                                    int (*f_rng)(void *, unsigned char *, size_t),
                                    void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_mpi m_xm2_s, one;

    mbedtls_mpi_init(&m_xm2_s);
    mbedtls_mpi_init(&one);

    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&one, 1));

    /*
     * Client:  K = ( Xs - X4  * x2  * s ) * x2
     * Server:  K = ( Xc - X2  * x4  * s ) * x4
     * Unified: K = ( Xp - Xp2 * xm2 * s ) * xm2
     */
    MBEDTLS_MPI_CHK(ecjpake_mul_secret(&m_xm2_s, -1, &ctx->xm2, &ctx->s,
                                       &ctx->grp.N, f_rng, p_rng));
    MBEDTLS_MPI_CHK(mbedtls_ecp_muladd(&ctx->grp, K,
                                       &one, &ctx->Xp,
                                       &m_xm2_s, &ctx->Xp2));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&ctx->grp, K, &ctx->xm2, K,
                                    f_rng, p_rng));

cleanup:
    mbedtls_mpi_free(&m_xm2_s);
    mbedtls_mpi_free(&one);

    return ret;
}

int mbedtls_ecjpake_derive_secret(mbedtls_ecjpake_context *ctx,
                                  unsigned char *buf, size_t len, size_t *olen,
                                  int (*f_rng)(void *, unsigned char *, size_t),
                                  void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_point K;
    unsigned char kx[MBEDTLS_ECP_MAX_BYTES];
    size_t x_bytes;

    *olen = mbedtls_md_get_size_from_type(ctx->md_type);
    if (len < *olen) {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    mbedtls_ecp_point_init(&K);

    ret = mbedtls_ecjpake_derive_k(ctx, &K, f_rng, p_rng);
    if (ret) {
        goto cleanup;
    }

    /* PMS = SHA-256( K.X ) */
    x_bytes = (ctx->grp.pbits + 7) / 8;
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&K.X, kx, x_bytes));
    MBEDTLS_MPI_CHK(mbedtls_ecjpake_compute_hash(ctx->md_type,
                                                 kx, x_bytes, buf));

cleanup:
    mbedtls_ecp_point_free(&K);

    return ret;
}

int mbedtls_ecjpake_write_shared_key(mbedtls_ecjpake_context *ctx,
                                     unsigned char *buf, size_t len, size_t *olen,
                                     int (*f_rng)(void *, unsigned char *, size_t),
                                     void *p_rng)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecp_point K;

    mbedtls_ecp_point_init(&K);

    ret = mbedtls_ecjpake_derive_k(ctx, &K, f_rng, p_rng);
    if (ret) {
        goto cleanup;
    }

    ret = mbedtls_ecp_point_write_binary(&ctx->grp, &K, ctx->point_format,
                                         olen, buf, len);
    if (ret != 0) {
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_point_free(&K);

    return ret;
}

#undef ID_MINE
#undef ID_PEER

#endif /* ! MBEDTLS_ECJPAKE_ALT */

#if defined(MBEDTLS_SELF_TEST)

#include "mbedtls/platform.h"

#if !defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED) || \
    !defined(MBEDTLS_MD_CAN_SHA256)
int mbedtls_ecjpake_self_test(int verbose)
{
    (void) verbose;
    return 0;
}
#else

static const unsigned char ecjpake_test_password[] = {
    0x74, 0x68, 0x72, 0x65, 0x61, 0x64, 0x6a, 0x70, 0x61, 0x6b, 0x65, 0x74,
    0x65, 0x73, 0x74
};

#if !defined(MBEDTLS_ECJPAKE_ALT)

static const unsigned char ecjpake_test_x1[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
    0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x21
};

static const unsigned char ecjpake_test_x2[] = {
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c,
    0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x81
};

static const unsigned char ecjpake_test_x3[] = {
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c,
    0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x81
};

static const unsigned char ecjpake_test_x4[] = {
    0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc,
    0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
    0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe1
};

static const unsigned char ecjpake_test_cli_one[] = {
    0x41, 0x04, 0xac, 0xcf, 0x01, 0x06, 0xef, 0x85, 0x8f, 0xa2, 0xd9, 0x19,
    0x33, 0x13, 0x46, 0x80, 0x5a, 0x78, 0xb5, 0x8b, 0xba, 0xd0, 0xb8, 0x44,
    0xe5, 0xc7, 0x89, 0x28, 0x79, 0x14, 0x61, 0x87, 0xdd, 0x26, 0x66, 0xad,
    0xa7, 0x81, 0xbb, 0x7f, 0x11, 0x13, 0x72, 0x25, 0x1a, 0x89, 0x10, 0x62,
    0x1f, 0x63, 0x4d, 0xf1, 0x28, 0xac, 0x48, 0xe3, 0x81, 0xfd, 0x6e, 0xf9,
    0x06, 0x07, 0x31, 0xf6, 0x94, 0xa4, 0x41, 0x04, 0x1d, 0xd0, 0xbd, 0x5d,
    0x45, 0x66, 0xc9, 0xbe, 0xd9, 0xce, 0x7d, 0xe7, 0x01, 0xb5, 0xe8, 0x2e,
    0x08, 0xe8, 0x4b, 0x73, 0x04, 0x66, 0x01, 0x8a, 0xb9, 0x03, 0xc7, 0x9e,
    0xb9, 0x82, 0x17, 0x22, 0x36, 0xc0, 0xc1, 0x72, 0x8a, 0xe4, 0xbf, 0x73,
    0x61, 0x0d, 0x34, 0xde, 0x44, 0x24, 0x6e, 0xf3, 0xd9, 0xc0, 0x5a, 0x22,
    0x36, 0xfb, 0x66, 0xa6, 0x58, 0x3d, 0x74, 0x49, 0x30, 0x8b, 0xab, 0xce,
    0x20, 0x72, 0xfe, 0x16, 0x66, 0x29, 0x92, 0xe9, 0x23, 0x5c, 0x25, 0x00,
    0x2f, 0x11, 0xb1, 0x50, 0x87, 0xb8, 0x27, 0x38, 0xe0, 0x3c, 0x94, 0x5b,
    0xf7, 0xa2, 0x99, 0x5d, 0xda, 0x1e, 0x98, 0x34, 0x58, 0x41, 0x04, 0x7e,
    0xa6, 0xe3, 0xa4, 0x48, 0x70, 0x37, 0xa9, 0xe0, 0xdb, 0xd7, 0x92, 0x62,
    0xb2, 0xcc, 0x27, 0x3e, 0x77, 0x99, 0x30, 0xfc, 0x18, 0x40, 0x9a, 0xc5,
    0x36, 0x1c, 0x5f, 0xe6, 0x69, 0xd7, 0x02, 0xe1, 0x47, 0x79, 0x0a, 0xeb,
    0x4c, 0xe7, 0xfd, 0x65, 0x75, 0xab, 0x0f, 0x6c, 0x7f, 0xd1, 0xc3, 0x35,
    0x93, 0x9a, 0xa8, 0x63, 0xba, 0x37, 0xec, 0x91, 0xb7, 0xe3, 0x2b, 0xb0,
    0x13, 0xbb, 0x2b, 0x41, 0x04, 0xa4, 0x95, 0x58, 0xd3, 0x2e, 0xd1, 0xeb,
    0xfc, 0x18, 0x16, 0xaf, 0x4f, 0xf0, 0x9b, 0x55, 0xfc, 0xb4, 0xca, 0x47,
    0xb2, 0xa0, 0x2d, 0x1e, 0x7c, 0xaf, 0x11, 0x79, 0xea, 0x3f, 0xe1, 0x39,
    0x5b, 0x22, 0xb8, 0x61, 0x96, 0x40, 0x16, 0xfa, 0xba, 0xf7, 0x2c, 0x97,
    0x56, 0x95, 0xd9, 0x3d, 0x4d, 0xf0, 0xe5, 0x19, 0x7f, 0xe9, 0xf0, 0x40,
    0x63, 0x4e, 0xd5, 0x97, 0x64, 0x93, 0x77, 0x87, 0xbe, 0x20, 0xbc, 0x4d,
    0xee, 0xbb, 0xf9, 0xb8, 0xd6, 0x0a, 0x33, 0x5f, 0x04, 0x6c, 0xa3, 0xaa,
    0x94, 0x1e, 0x45, 0x86, 0x4c, 0x7c, 0xad, 0xef, 0x9c, 0xf7, 0x5b, 0x3d,
    0x8b, 0x01, 0x0e, 0x44, 0x3e, 0xf0
};

static const unsigned char ecjpake_test_srv_one[] = {
    0x41, 0x04, 0x7e, 0xa6, 0xe3, 0xa4, 0x48, 0x70, 0x37, 0xa9, 0xe0, 0xdb,
    0xd7, 0x92, 0x62, 0xb2, 0xcc, 0x27, 0x3e, 0x77, 0x99, 0x30, 0xfc, 0x18,
    0x40, 0x9a, 0xc5, 0x36, 0x1c, 0x5f, 0xe6, 0x69, 0xd7, 0x02, 0xe1, 0x47,
    0x79, 0x0a, 0xeb, 0x4c, 0xe7, 0xfd, 0x65, 0x75, 0xab, 0x0f, 0x6c, 0x7f,
    0xd1, 0xc3, 0x35, 0x93, 0x9a, 0xa8, 0x63, 0xba, 0x37, 0xec, 0x91, 0xb7,
    0xe3, 0x2b, 0xb0, 0x13, 0xbb, 0x2b, 0x41, 0x04, 0x09, 0xf8, 0x5b, 0x3d,
    0x20, 0xeb, 0xd7, 0x88, 0x5c, 0xe4, 0x64, 0xc0, 0x8d, 0x05, 0x6d, 0x64,
    0x28, 0xfe, 0x4d, 0xd9, 0x28, 0x7a, 0xa3, 0x65, 0xf1, 0x31, 0xf4, 0x36,
    0x0f, 0xf3, 0x86, 0xd8, 0x46, 0x89, 0x8b, 0xc4, 0xb4, 0x15, 0x83, 0xc2,
    0xa5, 0x19, 0x7f, 0x65, 0xd7, 0x87, 0x42, 0x74, 0x6c, 0x12, 0xa5, 0xec,
    0x0a, 0x4f, 0xfe, 0x2f, 0x27, 0x0a, 0x75, 0x0a, 0x1d, 0x8f, 0xb5, 0x16,
    0x20, 0x93, 0x4d, 0x74, 0xeb, 0x43, 0xe5, 0x4d, 0xf4, 0x24, 0xfd, 0x96,
    0x30, 0x6c, 0x01, 0x17, 0xbf, 0x13, 0x1a, 0xfa, 0xbf, 0x90, 0xa9, 0xd3,
    0x3d, 0x11, 0x98, 0xd9, 0x05, 0x19, 0x37, 0x35, 0x14, 0x41, 0x04, 0x19,
    0x0a, 0x07, 0x70, 0x0f, 0xfa, 0x4b, 0xe6, 0xae, 0x1d, 0x79, 0xee, 0x0f,
    0x06, 0xae, 0xb5, 0x44, 0xcd, 0x5a, 0xdd, 0xaa, 0xbe, 0xdf, 0x70, 0xf8,
    0x62, 0x33, 0x21, 0x33, 0x2c, 0x54, 0xf3, 0x55, 0xf0, 0xfb, 0xfe, 0xc7,
    0x83, 0xed, 0x35, 0x9e, 0x5d, 0x0b, 0xf7, 0x37, 0x7a, 0x0f, 0xc4, 0xea,
    0x7a, 0xce, 0x47, 0x3c, 0x9c, 0x11, 0x2b, 0x41, 0xcc, 0xd4, 0x1a, 0xc5,
    0x6a, 0x56, 0x12, 0x41, 0x04, 0x36, 0x0a, 0x1c, 0xea, 0x33, 0xfc, 0xe6,
    0x41, 0x15, 0x64, 0x58, 0xe0, 0xa4, 0xea, 0xc2, 0x19, 0xe9, 0x68, 0x31,
    0xe6, 0xae, 0xbc, 0x88, 0xb3, 0xf3, 0x75, 0x2f, 0x93, 0xa0, 0x28, 0x1d,
    0x1b, 0xf1, 0xfb, 0x10, 0x60, 0x51, 0xdb, 0x96, 0x94, 0xa8, 0xd6, 0xe8,
    0x62, 0xa5, 0xef, 0x13, 0x24, 0xa3, 0xd9, 0xe2, 0x78, 0x94, 0xf1, 0xee,
    0x4f, 0x7c, 0x59, 0x19, 0x99, 0x65, 0xa8, 0xdd, 0x4a, 0x20, 0x91, 0x84,
    0x7d, 0x2d, 0x22, 0xdf, 0x3e, 0xe5, 0x5f, 0xaa, 0x2a, 0x3f, 0xb3, 0x3f,
    0xd2, 0xd1, 0xe0, 0x55, 0xa0, 0x7a, 0x7c, 0x61, 0xec, 0xfb, 0x8d, 0x80,
    0xec, 0x00, 0xc2, 0xc9, 0xeb, 0x12
};

static const unsigned char ecjpake_test_srv_two[] = {
    0x03, 0x00, 0x17, 0x41, 0x04, 0x0f, 0xb2, 0x2b, 0x1d, 0x5d, 0x11, 0x23,
    0xe0, 0xef, 0x9f, 0xeb, 0x9d, 0x8a, 0x2e, 0x59, 0x0a, 0x1f, 0x4d, 0x7c,
    0xed, 0x2c, 0x2b, 0x06, 0x58, 0x6e, 0x8f, 0x2a, 0x16, 0xd4, 0xeb, 0x2f,
    0xda, 0x43, 0x28, 0xa2, 0x0b, 0x07, 0xd8, 0xfd, 0x66, 0x76, 0x54, 0xca,
    0x18, 0xc5, 0x4e, 0x32, 0xa3, 0x33, 0xa0, 0x84, 0x54, 0x51, 0xe9, 0x26,
    0xee, 0x88, 0x04, 0xfd, 0x7a, 0xf0, 0xaa, 0xa7, 0xa6, 0x41, 0x04, 0x55,
    0x16, 0xea, 0x3e, 0x54, 0xa0, 0xd5, 0xd8, 0xb2, 0xce, 0x78, 0x6b, 0x38,
    0xd3, 0x83, 0x37, 0x00, 0x29, 0xa5, 0xdb, 0xe4, 0x45, 0x9c, 0x9d, 0xd6,
    0x01, 0xb4, 0x08, 0xa2, 0x4a, 0xe6, 0x46, 0x5c, 0x8a, 0xc9, 0x05, 0xb9,
    0xeb, 0x03, 0xb5, 0xd3, 0x69, 0x1c, 0x13, 0x9e, 0xf8, 0x3f, 0x1c, 0xd4,
    0x20, 0x0f, 0x6c, 0x9c, 0xd4, 0xec, 0x39, 0x22, 0x18, 0xa5, 0x9e, 0xd2,
    0x43, 0xd3, 0xc8, 0x20, 0xff, 0x72, 0x4a, 0x9a, 0x70, 0xb8, 0x8c, 0xb8,
    0x6f, 0x20, 0xb4, 0x34, 0xc6, 0x86, 0x5a, 0xa1, 0xcd, 0x79, 0x06, 0xdd,
    0x7c, 0x9b, 0xce, 0x35, 0x25, 0xf5, 0x08, 0x27, 0x6f, 0x26, 0x83, 0x6c
};

static const unsigned char ecjpake_test_cli_two[] = {
    0x41, 0x04, 0x69, 0xd5, 0x4e, 0xe8, 0x5e, 0x90, 0xce, 0x3f, 0x12, 0x46,
    0x74, 0x2d, 0xe5, 0x07, 0xe9, 0x39, 0xe8, 0x1d, 0x1d, 0xc1, 0xc5, 0xcb,
    0x98, 0x8b, 0x58, 0xc3, 0x10, 0xc9, 0xfd, 0xd9, 0x52, 0x4d, 0x93, 0x72,
    0x0b, 0x45, 0x54, 0x1c, 0x83, 0xee, 0x88, 0x41, 0x19, 0x1d, 0xa7, 0xce,
    0xd8, 0x6e, 0x33, 0x12, 0xd4, 0x36, 0x23, 0xc1, 0xd6, 0x3e, 0x74, 0x98,
    0x9a, 0xba, 0x4a, 0xff, 0xd1, 0xee, 0x41, 0x04, 0x07, 0x7e, 0x8c, 0x31,
    0xe2, 0x0e, 0x6b, 0xed, 0xb7, 0x60, 0xc1, 0x35, 0x93, 0xe6, 0x9f, 0x15,
    0xbe, 0x85, 0xc2, 0x7d, 0x68, 0xcd, 0x09, 0xcc, 0xb8, 0xc4, 0x18, 0x36,
    0x08, 0x91, 0x7c, 0x5c, 0x3d, 0x40, 0x9f, 0xac, 0x39, 0xfe, 0xfe, 0xe8,
    0x2f, 0x72, 0x92, 0xd3, 0x6f, 0x0d, 0x23, 0xe0, 0x55, 0x91, 0x3f, 0x45,
    0xa5, 0x2b, 0x85, 0xdd, 0x8a, 0x20, 0x52, 0xe9, 0xe1, 0x29, 0xbb, 0x4d,
    0x20, 0x0f, 0x01, 0x1f, 0x19, 0x48, 0x35, 0x35, 0xa6, 0xe8, 0x9a, 0x58,
    0x0c, 0x9b, 0x00, 0x03, 0xba, 0xf2, 0x14, 0x62, 0xec, 0xe9, 0x1a, 0x82,
    0xcc, 0x38, 0xdb, 0xdc, 0xae, 0x60, 0xd9, 0xc5, 0x4c
};

static const unsigned char ecjpake_test_shared_key[] = {
    0x04, 0x01, 0xab, 0xe9, 0xf2, 0xc7, 0x3a, 0x99, 0x14, 0xcb, 0x1f, 0x80,
    0xfb, 0x9d, 0xdb, 0x7e, 0x00, 0x12, 0xa8, 0x9c, 0x2f, 0x39, 0x27, 0x79,
    0xf9, 0x64, 0x40, 0x14, 0x75, 0xea, 0xc1, 0x31, 0x28, 0x43, 0x8f, 0xe1,
    0x12, 0x41, 0xd6, 0xc1, 0xe5, 0x5f, 0x7b, 0x80, 0x88, 0x94, 0xc9, 0xc0,
    0x27, 0xa3, 0x34, 0x41, 0xf5, 0xcb, 0xa1, 0xfe, 0x6c, 0xc7, 0xe6, 0x12,
    0x17, 0xc3, 0xde, 0x27, 0xb4,
};

static const unsigned char ecjpake_test_pms[] = {
    0xf3, 0xd4, 0x7f, 0x59, 0x98, 0x44, 0xdb, 0x92, 0xa5, 0x69, 0xbb, 0xe7,
    0x98, 0x1e, 0x39, 0xd9, 0x31, 0xfd, 0x74, 0x3b, 0xf2, 0x2e, 0x98, 0xf9,
    0xb4, 0x38, 0xf7, 0x19, 0xd3, 0xc4, 0xf3, 0x51
};

/*
 * PRNG for test - !!!INSECURE NEVER USE IN PRODUCTION!!!
 *
 * This is the linear congruential generator from numerical recipes,
 * except we only use the low byte as the output. See
 * https://en.wikipedia.org/wiki/Linear_congruential_generator#Parameters_in_common_use
 */
static int self_test_rng(void *ctx, unsigned char *out, size_t len)
{
    static uint32_t state = 42;

    (void) ctx;

    for (size_t i = 0; i < len; i++) {
        state = state * 1664525u + 1013904223u;
        out[i] = (unsigned char) state;
    }

    return 0;
}

/* Load my private keys and generate the corresponding public keys */
static int ecjpake_test_load(mbedtls_ecjpake_context *ctx,
                             const unsigned char *xm1, size_t len1,
                             const unsigned char *xm2, size_t len2)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&ctx->xm1, xm1, len1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&ctx->xm2, xm2, len2));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&ctx->grp, &ctx->Xm1, &ctx->xm1,
                                    &ctx->grp.G, self_test_rng, NULL));
    MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&ctx->grp, &ctx->Xm2, &ctx->xm2,
                                    &ctx->grp.G, self_test_rng, NULL));

cleanup:
    return ret;
}

#endif /* ! MBEDTLS_ECJPAKE_ALT */

/* For tests we don't need a secure RNG;
 * use the LGC from Numerical Recipes for simplicity */
static int ecjpake_lgc(void *p, unsigned char *out, size_t len)
{
    static uint32_t x = 42;
    (void) p;

    while (len > 0) {
        size_t use_len = len > 4 ? 4 : len;
        x = 1664525 * x + 1013904223;
        memcpy(out, &x, use_len);
        out += use_len;
        len -= use_len;
    }

    return 0;
}

#define TEST_ASSERT(x)    \
    do {                    \
        if (x)             \
        ret = 0;        \
        else                \
        {                   \
            ret = 1;        \
            goto cleanup;   \
        }                   \
    } while (0)

/*
 * Checkup routine
 */
int mbedtls_ecjpake_self_test(int verbose)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    mbedtls_ecjpake_context cli;
    mbedtls_ecjpake_context srv;
    unsigned char buf[512], pms[32];
    size_t len, pmslen;

    mbedtls_ecjpake_init(&cli);
    mbedtls_ecjpake_init(&srv);

    if (verbose != 0) {
        mbedtls_printf("  ECJPAKE test #0 (setup): ");
    }

    TEST_ASSERT(mbedtls_ecjpake_setup(&cli, MBEDTLS_ECJPAKE_CLIENT,
                                      MBEDTLS_MD_SHA256, MBEDTLS_ECP_DP_SECP256R1,
                                      ecjpake_test_password,
                                      sizeof(ecjpake_test_password)) == 0);

    TEST_ASSERT(mbedtls_ecjpake_setup(&srv, MBEDTLS_ECJPAKE_SERVER,
                                      MBEDTLS_MD_SHA256, MBEDTLS_ECP_DP_SECP256R1,
                                      ecjpake_test_password,
                                      sizeof(ecjpake_test_password)) == 0);

    if (verbose != 0) {
        mbedtls_printf("passed\n");
    }

    if (verbose != 0) {
        mbedtls_printf("  ECJPAKE test #1 (random handshake): ");
    }

    TEST_ASSERT(mbedtls_ecjpake_write_round_one(&cli,
                                                buf, sizeof(buf), &len, ecjpake_lgc, NULL) == 0);

    TEST_ASSERT(mbedtls_ecjpake_read_round_one(&srv, buf, len) == 0);

    TEST_ASSERT(mbedtls_ecjpake_write_round_one(&srv,
                                                buf, sizeof(buf), &len, ecjpake_lgc, NULL) == 0);

    TEST_ASSERT(mbedtls_ecjpake_read_round_one(&cli, buf, len) == 0);

    TEST_ASSERT(mbedtls_ecjpake_write_round_two(&srv,
                                                buf, sizeof(buf), &len, ecjpake_lgc, NULL) == 0);

    TEST_ASSERT(mbedtls_ecjpake_read_round_two(&cli, buf, len) == 0);

    TEST_ASSERT(mbedtls_ecjpake_derive_secret(&cli,
                                              pms, sizeof(pms), &pmslen, ecjpake_lgc, NULL) == 0);

    TEST_ASSERT(mbedtls_ecjpake_write_round_two(&cli,
                                                buf, sizeof(buf), &len, ecjpake_lgc, NULL) == 0);

    TEST_ASSERT(mbedtls_ecjpake_read_round_two(&srv, buf, len) == 0);

    TEST_ASSERT(mbedtls_ecjpake_derive_secret(&srv,
                                              buf, sizeof(buf), &len, ecjpake_lgc, NULL) == 0);

    TEST_ASSERT(len == pmslen);
    TEST_ASSERT(memcmp(buf, pms, len) == 0);

    if (verbose != 0) {
        mbedtls_printf("passed\n");
    }

#if !defined(MBEDTLS_ECJPAKE_ALT)
    /* 'reference handshake' tests can only be run against implementations
     * for which we have 100% control over how the random ephemeral keys
     * are generated. This is only the case for the internal Mbed TLS
     * implementation, so these tests are skipped in case the internal
     * implementation is swapped out for an alternative one. */
    if (verbose != 0) {
        mbedtls_printf("  ECJPAKE test #2 (reference handshake): ");
    }

    /* Simulate generation of round one */
    MBEDTLS_MPI_CHK(ecjpake_test_load(&cli,
                                      ecjpake_test_x1, sizeof(ecjpake_test_x1),
                                      ecjpake_test_x2, sizeof(ecjpake_test_x2)));

    MBEDTLS_MPI_CHK(ecjpake_test_load(&srv,
                                      ecjpake_test_x3, sizeof(ecjpake_test_x3),
                                      ecjpake_test_x4, sizeof(ecjpake_test_x4)));

    /* Read round one */
    TEST_ASSERT(mbedtls_ecjpake_read_round_one(&srv,
                                               ecjpake_test_cli_one,
                                               sizeof(ecjpake_test_cli_one)) == 0);

    TEST_ASSERT(mbedtls_ecjpake_read_round_one(&cli,
                                               ecjpake_test_srv_one,
                                               sizeof(ecjpake_test_srv_one)) == 0);

    /* Skip generation of round two, read round two */
    TEST_ASSERT(mbedtls_ecjpake_read_round_two(&cli,
                                               ecjpake_test_srv_two,
                                               sizeof(ecjpake_test_srv_two)) == 0);

    TEST_ASSERT(mbedtls_ecjpake_read_round_two(&srv,
                                               ecjpake_test_cli_two,
                                               sizeof(ecjpake_test_cli_two)) == 0);

    /* Server derives PMS */
    TEST_ASSERT(mbedtls_ecjpake_derive_secret(&srv,
                                              buf, sizeof(buf), &len, ecjpake_lgc, NULL) == 0);

    TEST_ASSERT(len == sizeof(ecjpake_test_pms));
    TEST_ASSERT(memcmp(buf, ecjpake_test_pms, len) == 0);

    /* Server derives K as unsigned binary data */
    TEST_ASSERT(mbedtls_ecjpake_write_shared_key(&srv,
                                                 buf, sizeof(buf), &len, ecjpake_lgc, NULL) == 0);

    TEST_ASSERT(len == sizeof(ecjpake_test_shared_key));
    TEST_ASSERT(memcmp(buf, ecjpake_test_shared_key, len) == 0);

    memset(buf, 0, len);   /* Avoid interferences with next step */

    /* Client derives PMS */
    TEST_ASSERT(mbedtls_ecjpake_derive_secret(&cli,
                                              buf, sizeof(buf), &len, ecjpake_lgc, NULL) == 0);

    TEST_ASSERT(len == sizeof(ecjpake_test_pms));
    TEST_ASSERT(memcmp(buf, ecjpake_test_pms, len) == 0);

    /* Client derives K as unsigned binary data */
    TEST_ASSERT(mbedtls_ecjpake_write_shared_key(&cli,
                                                 buf, sizeof(buf), &len, ecjpake_lgc, NULL) == 0);

    TEST_ASSERT(len == sizeof(ecjpake_test_shared_key));
    TEST_ASSERT(memcmp(buf, ecjpake_test_shared_key, len) == 0);

    if (verbose != 0) {
        mbedtls_printf("passed\n");
    }
#endif /* ! MBEDTLS_ECJPAKE_ALT */

cleanup:
    mbedtls_ecjpake_free(&cli);
    mbedtls_ecjpake_free(&srv);

    if (ret != 0) {
        if (verbose != 0) {
            mbedtls_printf("failed\n");
        }

        ret = 1;
    }

    if (verbose != 0) {
        mbedtls_printf("\n");
    }

    return ret;
}

#undef TEST_ASSERT

#endif /* MBEDTLS_ECP_DP_SECP256R1_ENABLED && MBEDTLS_MD_CAN_SHA256 */

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_ECJPAKE_C */
