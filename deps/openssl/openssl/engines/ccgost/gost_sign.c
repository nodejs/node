/**********************************************************************
 *                          gost_sign.c                               *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *         This file is distributed under the same license as OpenSSL *
 *                                                                    *
 *       Implementation of GOST R 34.10-94 signature algorithm        *
 *       for OpenSSL                                                  *
 *          Requires OpenSSL 0.9.9 for compilation                    *
 **********************************************************************/
#include <string.h>
#include <openssl/rand.h>
#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/evp.h>

#include "gost_params.h"
#include "gost_lcl.h"
#include "e_gost_err.h"

#ifdef DEBUG_SIGN
void dump_signature(const char *message, const unsigned char *buffer,
                    size_t len)
{
    size_t i;
    fprintf(stderr, "signature %s Length=%d", message, len);
    for (i = 0; i < len; i++) {
        if (i % 16 == 0)
            fputc('\n', stderr);
        fprintf(stderr, " %02x", buffer[i]);
    }
    fprintf(stderr, "\nEnd of signature\n");
}

void dump_dsa_sig(const char *message, DSA_SIG *sig)
{
    fprintf(stderr, "%s\nR=", message);
    BN_print_fp(stderr, sig->r);
    fprintf(stderr, "\nS=");
    BN_print_fp(stderr, sig->s);
    fprintf(stderr, "\n");
}

#else

# define dump_signature(a,b,c)
# define dump_dsa_sig(a,b)
#endif

/*
 * Computes signature and returns it as DSA_SIG structure
 */
DSA_SIG *gost_do_sign(const unsigned char *dgst, int dlen, DSA *dsa)
{
    BIGNUM *k = NULL, *tmp = NULL, *tmp2 = NULL;
    DSA_SIG *newsig = DSA_SIG_new();
    BIGNUM *md = hashsum2bn(dgst);
    /* check if H(M) mod q is zero */
    BN_CTX *ctx = BN_CTX_new();
    BN_CTX_start(ctx);
    if (!newsig) {
        GOSTerr(GOST_F_GOST_DO_SIGN, GOST_R_NO_MEMORY);
        goto err;
    }
    tmp = BN_CTX_get(ctx);
    k = BN_CTX_get(ctx);
    tmp2 = BN_CTX_get(ctx);
    BN_mod(tmp, md, dsa->q, ctx);
    if (BN_is_zero(tmp)) {
        BN_one(md);
    }
    do {
        do {
            /*
             * Generate random number k less than q
             */
            BN_rand_range(k, dsa->q);
            /* generate r = (a^x mod p) mod q */
            BN_mod_exp(tmp, dsa->g, k, dsa->p, ctx);
            if (!(newsig->r))
                newsig->r = BN_new();
            BN_mod(newsig->r, tmp, dsa->q, ctx);
        }
        while (BN_is_zero(newsig->r));
        /* generate s = (xr + k(Hm)) mod q */
        BN_mod_mul(tmp, dsa->priv_key, newsig->r, dsa->q, ctx);
        BN_mod_mul(tmp2, k, md, dsa->q, ctx);
        if (!newsig->s)
            newsig->s = BN_new();
        BN_mod_add(newsig->s, tmp, tmp2, dsa->q, ctx);
    }
    while (BN_is_zero(newsig->s));
 err:
    BN_free(md);
    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    return newsig;
}

/*
 * Packs signature according to Cryptocom rules
 * and frees up DSA_SIG structure
 */
/*-
int pack_sign_cc(DSA_SIG *s,int order,unsigned char *sig, size_t *siglen)
        {
        *siglen = 2*order;
        memset(sig,0,*siglen);
        store_bignum(s->r, sig,order);
        store_bignum(s->s, sig + order,order);
        dump_signature("serialized",sig,*siglen);
        DSA_SIG_free(s);
        return 1;
        }
*/
/*
 * Packs signature according to Cryptopro rules
 * and frees up DSA_SIG structure
 */
int pack_sign_cp(DSA_SIG *s, int order, unsigned char *sig, size_t *siglen)
{
    *siglen = 2 * order;
    memset(sig, 0, *siglen);
    store_bignum(s->s, sig, order);
    store_bignum(s->r, sig + order, order);
    dump_signature("serialized", sig, *siglen);
    DSA_SIG_free(s);
    return 1;
}

/*
 * Verifies signature passed as DSA_SIG structure
 *
 */

int gost_do_verify(const unsigned char *dgst, int dgst_len,
                   DSA_SIG *sig, DSA *dsa)
{
    BIGNUM *md, *tmp = NULL;
    BIGNUM *q2 = NULL;
    BIGNUM *u = NULL, *v = NULL, *z1 = NULL, *z2 = NULL;
    BIGNUM *tmp2 = NULL, *tmp3 = NULL;
    int ok;
    BN_CTX *ctx = BN_CTX_new();

    BN_CTX_start(ctx);
    if (BN_cmp(sig->s, dsa->q) >= 1 || BN_cmp(sig->r, dsa->q) >= 1) {
        GOSTerr(GOST_F_GOST_DO_VERIFY, GOST_R_SIGNATURE_PARTS_GREATER_THAN_Q);
        return 0;
    }
    md = hashsum2bn(dgst);

    tmp = BN_CTX_get(ctx);
    v = BN_CTX_get(ctx);
    q2 = BN_CTX_get(ctx);
    z1 = BN_CTX_get(ctx);
    z2 = BN_CTX_get(ctx);
    tmp2 = BN_CTX_get(ctx);
    tmp3 = BN_CTX_get(ctx);
    u = BN_CTX_get(ctx);

    BN_mod(tmp, md, dsa->q, ctx);
    if (BN_is_zero(tmp)) {
        BN_one(md);
    }
    BN_copy(q2, dsa->q);
    BN_sub_word(q2, 2);
    BN_mod_exp(v, md, q2, dsa->q, ctx);
    BN_mod_mul(z1, sig->s, v, dsa->q, ctx);
    BN_sub(tmp, dsa->q, sig->r);
    BN_mod_mul(z2, tmp, v, dsa->p, ctx);
    BN_mod_exp(tmp, dsa->g, z1, dsa->p, ctx);
    BN_mod_exp(tmp2, dsa->pub_key, z2, dsa->p, ctx);
    BN_mod_mul(tmp3, tmp, tmp2, dsa->p, ctx);
    BN_mod(u, tmp3, dsa->q, ctx);
    ok = BN_cmp(u, sig->r);

    BN_free(md);
    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    if (ok != 0) {
        GOSTerr(GOST_F_GOST_DO_VERIFY, GOST_R_SIGNATURE_MISMATCH);
    }
    return (ok == 0);
}

/*
 * Computes public keys for GOST R 34.10-94 algorithm
 *
 */
int gost94_compute_public(DSA *dsa)
{
    /* Now fill algorithm parameters with correct values */
    BN_CTX *ctx = BN_CTX_new();
    if (!dsa->g) {
        GOSTerr(GOST_F_GOST94_COMPUTE_PUBLIC, GOST_R_KEY_IS_NOT_INITALIZED);
        return 0;
    }
    /* Compute public key  y = a^x mod p */
    dsa->pub_key = BN_new();
    BN_mod_exp(dsa->pub_key, dsa->g, dsa->priv_key, dsa->p, ctx);
    BN_CTX_free(ctx);
    return 1;
}

/*
 * Fill GOST 94 params, searching them in R3410_paramset array
 * by nid of paramset
 *
 */
int fill_GOST94_params(DSA *dsa, int nid)
{
    R3410_params *params = R3410_paramset;
    while (params->nid != NID_undef && params->nid != nid)
        params++;
    if (params->nid == NID_undef) {
        GOSTerr(GOST_F_FILL_GOST94_PARAMS, GOST_R_UNSUPPORTED_PARAMETER_SET);
        return 0;
    }
#define dump_signature(a,b,c)
    if (dsa->p) {
        BN_free(dsa->p);
    }
    dsa->p = NULL;
    BN_dec2bn(&(dsa->p), params->p);
    if (dsa->q) {
        BN_free(dsa->q);
    }
    dsa->q = NULL;
    BN_dec2bn(&(dsa->q), params->q);
    if (dsa->g) {
        BN_free(dsa->g);
    }
    dsa->g = NULL;
    BN_dec2bn(&(dsa->g), params->a);
    return 1;
}

/*
 *  Generate GOST R 34.10-94 keypair
 *
 *
 */
int gost_sign_keygen(DSA *dsa)
{
    dsa->priv_key = BN_new();
    BN_rand_range(dsa->priv_key, dsa->q);
    return gost94_compute_public(dsa);
}

/* Unpack signature according to cryptocom rules  */
/*-
DSA_SIG *unpack_cc_signature(const unsigned char *sig,size_t siglen)
        {
        DSA_SIG *s;
        s = DSA_SIG_new();
        if (s == NULL)
                {
                GOSTerr(GOST_F_UNPACK_CC_SIGNATURE,GOST_R_NO_MEMORY);
                return(NULL);
                }
        s->r = getbnfrombuf(sig, siglen/2);
        s->s = getbnfrombuf(sig + siglen/2, siglen/2);
        return s;
        }
*/
/* Unpack signature according to cryptopro rules  */
DSA_SIG *unpack_cp_signature(const unsigned char *sig, size_t siglen)
{
    DSA_SIG *s;

    s = DSA_SIG_new();
    if (s == NULL) {
        GOSTerr(GOST_F_UNPACK_CP_SIGNATURE, GOST_R_NO_MEMORY);
        return NULL;
    }
    s->s = getbnfrombuf(sig, siglen / 2);
    s->r = getbnfrombuf(sig + siglen / 2, siglen / 2);
    return s;
}

/* Convert little-endian byte array into bignum */
BIGNUM *hashsum2bn(const unsigned char *dgst)
{
    unsigned char buf[32];
    int i;
    for (i = 0; i < 32; i++) {
        buf[31 - i] = dgst[i];
    }
    return getbnfrombuf(buf, 32);
}

/* Convert byte buffer to bignum, skipping leading zeros*/
BIGNUM *getbnfrombuf(const unsigned char *buf, size_t len)
{
    while (*buf == 0 && len > 0) {
        buf++;
        len--;
    }
    if (len) {
        return BN_bin2bn(buf, len, NULL);
    } else {
        BIGNUM *b = BN_new();
        BN_zero(b);
        return b;
    }
}

/*
 * Pack bignum into byte buffer of given size, filling all leading bytes by
 * zeros
 */
int store_bignum(BIGNUM *bn, unsigned char *buf, int len)
{
    int bytes = BN_num_bytes(bn);
    if (bytes > len)
        return 0;
    memset(buf, 0, len);
    BN_bn2bin(bn, buf + len - bytes);
    return 1;
}
