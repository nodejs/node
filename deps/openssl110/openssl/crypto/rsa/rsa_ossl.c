/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/cryptlib.h"
#include "internal/bn_int.h"
#include "rsa_locl.h"

#ifndef RSA_NULL

static int rsa_ossl_public_encrypt(int flen, const unsigned char *from,
                                  unsigned char *to, RSA *rsa, int padding);
static int rsa_ossl_private_encrypt(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding);
static int rsa_ossl_public_decrypt(int flen, const unsigned char *from,
                                  unsigned char *to, RSA *rsa, int padding);
static int rsa_ossl_private_decrypt(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding);
static int rsa_ossl_mod_exp(BIGNUM *r0, const BIGNUM *i, RSA *rsa,
                           BN_CTX *ctx);
static int rsa_ossl_init(RSA *rsa);
static int rsa_ossl_finish(RSA *rsa);
static RSA_METHOD rsa_pkcs1_ossl_meth = {
    "OpenSSL PKCS#1 RSA (from Eric Young)",
    rsa_ossl_public_encrypt,
    rsa_ossl_public_decrypt,     /* signature verification */
    rsa_ossl_private_encrypt,    /* signing */
    rsa_ossl_private_decrypt,
    rsa_ossl_mod_exp,
    BN_mod_exp_mont,            /* XXX probably we should not use Montgomery
                                 * if e == 3 */
    rsa_ossl_init,
    rsa_ossl_finish,
    RSA_FLAG_FIPS_METHOD,       /* flags */
    NULL,
    0,                          /* rsa_sign */
    0,                          /* rsa_verify */
    NULL                        /* rsa_keygen */
};

const RSA_METHOD *RSA_PKCS1_OpenSSL(void)
{
    return &rsa_pkcs1_ossl_meth;
}

static int rsa_ossl_public_encrypt(int flen, const unsigned char *from,
                                  unsigned char *to, RSA *rsa, int padding)
{
    BIGNUM *f, *ret;
    int i, j, k, num = 0, r = -1;
    unsigned char *buf = NULL;
    BN_CTX *ctx = NULL;

    if (BN_num_bits(rsa->n) > OPENSSL_RSA_MAX_MODULUS_BITS) {
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_ENCRYPT, RSA_R_MODULUS_TOO_LARGE);
        return -1;
    }

    if (BN_ucmp(rsa->n, rsa->e) <= 0) {
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_ENCRYPT, RSA_R_BAD_E_VALUE);
        return -1;
    }

    /* for large moduli, enforce exponent limit */
    if (BN_num_bits(rsa->n) > OPENSSL_RSA_SMALL_MODULUS_BITS) {
        if (BN_num_bits(rsa->e) > OPENSSL_RSA_MAX_PUBEXP_BITS) {
            RSAerr(RSA_F_RSA_OSSL_PUBLIC_ENCRYPT, RSA_R_BAD_E_VALUE);
            return -1;
        }
    }

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;
    BN_CTX_start(ctx);
    f = BN_CTX_get(ctx);
    ret = BN_CTX_get(ctx);
    num = BN_num_bytes(rsa->n);
    buf = OPENSSL_malloc(num);
    if (f == NULL || ret == NULL || buf == NULL) {
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_ENCRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    switch (padding) {
    case RSA_PKCS1_PADDING:
        i = RSA_padding_add_PKCS1_type_2(buf, num, from, flen);
        break;
    case RSA_PKCS1_OAEP_PADDING:
        i = RSA_padding_add_PKCS1_OAEP(buf, num, from, flen, NULL, 0);
        break;
    case RSA_SSLV23_PADDING:
        i = RSA_padding_add_SSLv23(buf, num, from, flen);
        break;
    case RSA_NO_PADDING:
        i = RSA_padding_add_none(buf, num, from, flen);
        break;
    default:
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_ENCRYPT, RSA_R_UNKNOWN_PADDING_TYPE);
        goto err;
    }
    if (i <= 0)
        goto err;

    if (BN_bin2bn(buf, num, f) == NULL)
        goto err;

    if (BN_ucmp(f, rsa->n) >= 0) {
        /* usually the padding functions would catch this */
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_ENCRYPT,
               RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
        goto err;
    }

    if (rsa->flags & RSA_FLAG_CACHE_PUBLIC)
        if (!BN_MONT_CTX_set_locked
            (&rsa->_method_mod_n, rsa->lock, rsa->n, ctx))
            goto err;

    if (!rsa->meth->bn_mod_exp(ret, f, rsa->e, rsa->n, ctx,
                               rsa->_method_mod_n))
        goto err;

    /*
     * put in leading 0 bytes if the number is less than the length of the
     * modulus
     */
    j = BN_num_bytes(ret);
    i = BN_bn2bin(ret, &(to[num - j]));
    for (k = 0; k < (num - i); k++)
        to[k] = 0;

    r = num;
 err:
    if (ctx != NULL)
        BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    OPENSSL_clear_free(buf, num);
    return (r);
}

static BN_BLINDING *rsa_get_blinding(RSA *rsa, int *local, BN_CTX *ctx)
{
    BN_BLINDING *ret;

    CRYPTO_THREAD_write_lock(rsa->lock);

    if (rsa->blinding == NULL) {
        rsa->blinding = RSA_setup_blinding(rsa, ctx);
    }

    ret = rsa->blinding;
    if (ret == NULL)
        goto err;

    if (BN_BLINDING_is_current_thread(ret)) {
        /* rsa->blinding is ours! */

        *local = 1;
    } else {
        /* resort to rsa->mt_blinding instead */

        /*
         * instructs rsa_blinding_convert(), rsa_blinding_invert() that the
         * BN_BLINDING is shared, meaning that accesses require locks, and
         * that the blinding factor must be stored outside the BN_BLINDING
         */
        *local = 0;

        if (rsa->mt_blinding == NULL) {
            rsa->mt_blinding = RSA_setup_blinding(rsa, ctx);
        }
        ret = rsa->mt_blinding;
    }

 err:
    CRYPTO_THREAD_unlock(rsa->lock);
    return ret;
}

static int rsa_blinding_convert(BN_BLINDING *b, BIGNUM *f, BIGNUM *unblind,
                                BN_CTX *ctx)
{
    if (unblind == NULL)
        /*
         * Local blinding: store the unblinding factor in BN_BLINDING.
         */
        return BN_BLINDING_convert_ex(f, NULL, b, ctx);
    else {
        /*
         * Shared blinding: store the unblinding factor outside BN_BLINDING.
         */
        int ret;

        BN_BLINDING_lock(b);
        ret = BN_BLINDING_convert_ex(f, unblind, b, ctx);
        BN_BLINDING_unlock(b);

        return ret;
    }
}

static int rsa_blinding_invert(BN_BLINDING *b, BIGNUM *f, BIGNUM *unblind,
                               BN_CTX *ctx)
{
    /*
     * For local blinding, unblind is set to NULL, and BN_BLINDING_invert_ex
     * will use the unblinding factor stored in BN_BLINDING. If BN_BLINDING
     * is shared between threads, unblind must be non-null:
     * BN_BLINDING_invert_ex will then use the local unblinding factor, and
     * will only read the modulus from BN_BLINDING. In both cases it's safe
     * to access the blinding without a lock.
     */
    return BN_BLINDING_invert_ex(f, unblind, b, ctx);
}

/* signing */
static int rsa_ossl_private_encrypt(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding)
{
    BIGNUM *f, *ret, *res;
    int i, j, k, num = 0, r = -1;
    unsigned char *buf = NULL;
    BN_CTX *ctx = NULL;
    int local_blinding = 0;
    /*
     * Used only if the blinding structure is shared. A non-NULL unblind
     * instructs rsa_blinding_convert() and rsa_blinding_invert() to store
     * the unblinding factor outside the blinding structure.
     */
    BIGNUM *unblind = NULL;
    BN_BLINDING *blinding = NULL;

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;
    BN_CTX_start(ctx);
    f = BN_CTX_get(ctx);
    ret = BN_CTX_get(ctx);
    num = BN_num_bytes(rsa->n);
    buf = OPENSSL_malloc(num);
    if (f == NULL || ret == NULL || buf == NULL) {
        RSAerr(RSA_F_RSA_OSSL_PRIVATE_ENCRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    switch (padding) {
    case RSA_PKCS1_PADDING:
        i = RSA_padding_add_PKCS1_type_1(buf, num, from, flen);
        break;
    case RSA_X931_PADDING:
        i = RSA_padding_add_X931(buf, num, from, flen);
        break;
    case RSA_NO_PADDING:
        i = RSA_padding_add_none(buf, num, from, flen);
        break;
    case RSA_SSLV23_PADDING:
    default:
        RSAerr(RSA_F_RSA_OSSL_PRIVATE_ENCRYPT, RSA_R_UNKNOWN_PADDING_TYPE);
        goto err;
    }
    if (i <= 0)
        goto err;

    if (BN_bin2bn(buf, num, f) == NULL)
        goto err;

    if (BN_ucmp(f, rsa->n) >= 0) {
        /* usually the padding functions would catch this */
        RSAerr(RSA_F_RSA_OSSL_PRIVATE_ENCRYPT,
               RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
        goto err;
    }

    if (!(rsa->flags & RSA_FLAG_NO_BLINDING)) {
        blinding = rsa_get_blinding(rsa, &local_blinding, ctx);
        if (blinding == NULL) {
            RSAerr(RSA_F_RSA_OSSL_PRIVATE_ENCRYPT, ERR_R_INTERNAL_ERROR);
            goto err;
        }
    }

    if (blinding != NULL) {
        if (!local_blinding && ((unblind = BN_CTX_get(ctx)) == NULL)) {
            RSAerr(RSA_F_RSA_OSSL_PRIVATE_ENCRYPT, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        if (!rsa_blinding_convert(blinding, f, unblind, ctx))
            goto err;
    }

    if ((rsa->flags & RSA_FLAG_EXT_PKEY) ||
        ((rsa->p != NULL) &&
         (rsa->q != NULL) &&
         (rsa->dmp1 != NULL) && (rsa->dmq1 != NULL) && (rsa->iqmp != NULL))) {
        if (!rsa->meth->rsa_mod_exp(ret, f, rsa, ctx))
            goto err;
    } else {
        BIGNUM *d = BN_new();
        if (d == NULL) {
            RSAerr(RSA_F_RSA_OSSL_PRIVATE_ENCRYPT, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        BN_with_flags(d, rsa->d, BN_FLG_CONSTTIME);

        if (rsa->flags & RSA_FLAG_CACHE_PUBLIC)
            if (!BN_MONT_CTX_set_locked
                (&rsa->_method_mod_n, rsa->lock, rsa->n, ctx)) {
                BN_free(d);
                goto err;
            }

        if (!rsa->meth->bn_mod_exp(ret, f, d, rsa->n, ctx,
                                   rsa->_method_mod_n)) {
            BN_free(d);
            goto err;
        }
        /* We MUST free d before any further use of rsa->d */
        BN_free(d);
    }

    if (blinding)
        if (!rsa_blinding_invert(blinding, ret, unblind, ctx))
            goto err;

    if (padding == RSA_X931_PADDING) {
        BN_sub(f, rsa->n, ret);
        if (BN_cmp(ret, f) > 0)
            res = f;
        else
            res = ret;
    } else
        res = ret;

    /*
     * put in leading 0 bytes if the number is less than the length of the
     * modulus
     */
    j = BN_num_bytes(res);
    i = BN_bn2bin(res, &(to[num - j]));
    for (k = 0; k < (num - i); k++)
        to[k] = 0;

    r = num;
 err:
    if (ctx != NULL)
        BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    OPENSSL_clear_free(buf, num);
    return (r);
}

static int rsa_ossl_private_decrypt(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding)
{
    BIGNUM *f, *ret;
    int j, num = 0, r = -1;
    unsigned char *p;
    unsigned char *buf = NULL;
    BN_CTX *ctx = NULL;
    int local_blinding = 0;
    /*
     * Used only if the blinding structure is shared. A non-NULL unblind
     * instructs rsa_blinding_convert() and rsa_blinding_invert() to store
     * the unblinding factor outside the blinding structure.
     */
    BIGNUM *unblind = NULL;
    BN_BLINDING *blinding = NULL;

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;
    BN_CTX_start(ctx);
    f = BN_CTX_get(ctx);
    ret = BN_CTX_get(ctx);
    num = BN_num_bytes(rsa->n);
    buf = OPENSSL_malloc(num);
    if (f == NULL || ret == NULL || buf == NULL) {
        RSAerr(RSA_F_RSA_OSSL_PRIVATE_DECRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    /*
     * This check was for equality but PGP does evil things and chops off the
     * top '0' bytes
     */
    if (flen > num) {
        RSAerr(RSA_F_RSA_OSSL_PRIVATE_DECRYPT,
               RSA_R_DATA_GREATER_THAN_MOD_LEN);
        goto err;
    }

    /* make data into a big number */
    if (BN_bin2bn(from, (int)flen, f) == NULL)
        goto err;

    if (BN_ucmp(f, rsa->n) >= 0) {
        RSAerr(RSA_F_RSA_OSSL_PRIVATE_DECRYPT,
               RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
        goto err;
    }

    if (!(rsa->flags & RSA_FLAG_NO_BLINDING)) {
        blinding = rsa_get_blinding(rsa, &local_blinding, ctx);
        if (blinding == NULL) {
            RSAerr(RSA_F_RSA_OSSL_PRIVATE_DECRYPT, ERR_R_INTERNAL_ERROR);
            goto err;
        }
    }

    if (blinding != NULL) {
        if (!local_blinding && ((unblind = BN_CTX_get(ctx)) == NULL)) {
            RSAerr(RSA_F_RSA_OSSL_PRIVATE_DECRYPT, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        if (!rsa_blinding_convert(blinding, f, unblind, ctx))
            goto err;
    }

    /* do the decrypt */
    if ((rsa->flags & RSA_FLAG_EXT_PKEY) ||
        ((rsa->p != NULL) &&
         (rsa->q != NULL) &&
         (rsa->dmp1 != NULL) && (rsa->dmq1 != NULL) && (rsa->iqmp != NULL))) {
        if (!rsa->meth->rsa_mod_exp(ret, f, rsa, ctx))
            goto err;
    } else {
        BIGNUM *d = BN_new();
        if (d == NULL) {
            RSAerr(RSA_F_RSA_OSSL_PRIVATE_DECRYPT, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        BN_with_flags(d, rsa->d, BN_FLG_CONSTTIME);

        if (rsa->flags & RSA_FLAG_CACHE_PUBLIC)
            if (!BN_MONT_CTX_set_locked
                (&rsa->_method_mod_n, rsa->lock, rsa->n, ctx)) {
                BN_free(d);
                goto err;
            }
        if (!rsa->meth->bn_mod_exp(ret, f, d, rsa->n, ctx,
                                   rsa->_method_mod_n)) {
            BN_free(d);
            goto err;
        }
        /* We MUST free d before any further use of rsa->d */
        BN_free(d);
    }

    if (blinding)
        if (!rsa_blinding_invert(blinding, ret, unblind, ctx))
            goto err;

    p = buf;
    j = BN_bn2bin(ret, p);      /* j is only used with no-padding mode */

    switch (padding) {
    case RSA_PKCS1_PADDING:
        r = RSA_padding_check_PKCS1_type_2(to, num, buf, j, num);
        break;
    case RSA_PKCS1_OAEP_PADDING:
        r = RSA_padding_check_PKCS1_OAEP(to, num, buf, j, num, NULL, 0);
        break;
    case RSA_SSLV23_PADDING:
        r = RSA_padding_check_SSLv23(to, num, buf, j, num);
        break;
    case RSA_NO_PADDING:
        r = RSA_padding_check_none(to, num, buf, j, num);
        break;
    default:
        RSAerr(RSA_F_RSA_OSSL_PRIVATE_DECRYPT, RSA_R_UNKNOWN_PADDING_TYPE);
        goto err;
    }
    if (r < 0)
        RSAerr(RSA_F_RSA_OSSL_PRIVATE_DECRYPT, RSA_R_PADDING_CHECK_FAILED);

 err:
    if (ctx != NULL)
        BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    OPENSSL_clear_free(buf, num);
    return (r);
}

/* signature verification */
static int rsa_ossl_public_decrypt(int flen, const unsigned char *from,
                                  unsigned char *to, RSA *rsa, int padding)
{
    BIGNUM *f, *ret;
    int i, num = 0, r = -1;
    unsigned char *p;
    unsigned char *buf = NULL;
    BN_CTX *ctx = NULL;

    if (BN_num_bits(rsa->n) > OPENSSL_RSA_MAX_MODULUS_BITS) {
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_DECRYPT, RSA_R_MODULUS_TOO_LARGE);
        return -1;
    }

    if (BN_ucmp(rsa->n, rsa->e) <= 0) {
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_DECRYPT, RSA_R_BAD_E_VALUE);
        return -1;
    }

    /* for large moduli, enforce exponent limit */
    if (BN_num_bits(rsa->n) > OPENSSL_RSA_SMALL_MODULUS_BITS) {
        if (BN_num_bits(rsa->e) > OPENSSL_RSA_MAX_PUBEXP_BITS) {
            RSAerr(RSA_F_RSA_OSSL_PUBLIC_DECRYPT, RSA_R_BAD_E_VALUE);
            return -1;
        }
    }

    if ((ctx = BN_CTX_new()) == NULL)
        goto err;
    BN_CTX_start(ctx);
    f = BN_CTX_get(ctx);
    ret = BN_CTX_get(ctx);
    num = BN_num_bytes(rsa->n);
    buf = OPENSSL_malloc(num);
    if (f == NULL || ret == NULL || buf == NULL) {
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_DECRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    /*
     * This check was for equality but PGP does evil things and chops off the
     * top '0' bytes
     */
    if (flen > num) {
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_DECRYPT, RSA_R_DATA_GREATER_THAN_MOD_LEN);
        goto err;
    }

    if (BN_bin2bn(from, flen, f) == NULL)
        goto err;

    if (BN_ucmp(f, rsa->n) >= 0) {
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_DECRYPT,
               RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
        goto err;
    }

    if (rsa->flags & RSA_FLAG_CACHE_PUBLIC)
        if (!BN_MONT_CTX_set_locked
            (&rsa->_method_mod_n, rsa->lock, rsa->n, ctx))
            goto err;

    if (!rsa->meth->bn_mod_exp(ret, f, rsa->e, rsa->n, ctx,
                               rsa->_method_mod_n))
        goto err;

    if ((padding == RSA_X931_PADDING) && ((bn_get_words(ret)[0] & 0xf) != 12))
        if (!BN_sub(ret, rsa->n, ret))
            goto err;

    p = buf;
    i = BN_bn2bin(ret, p);

    switch (padding) {
    case RSA_PKCS1_PADDING:
        r = RSA_padding_check_PKCS1_type_1(to, num, buf, i, num);
        break;
    case RSA_X931_PADDING:
        r = RSA_padding_check_X931(to, num, buf, i, num);
        break;
    case RSA_NO_PADDING:
        r = RSA_padding_check_none(to, num, buf, i, num);
        break;
    default:
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_DECRYPT, RSA_R_UNKNOWN_PADDING_TYPE);
        goto err;
    }
    if (r < 0)
        RSAerr(RSA_F_RSA_OSSL_PUBLIC_DECRYPT, RSA_R_PADDING_CHECK_FAILED);

 err:
    if (ctx != NULL)
        BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    OPENSSL_clear_free(buf, num);
    return (r);
}

static int rsa_ossl_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx)
{
    BIGNUM *r1, *m1, *vrfy;
    int ret = 0;

    BN_CTX_start(ctx);

    r1 = BN_CTX_get(ctx);
    m1 = BN_CTX_get(ctx);
    vrfy = BN_CTX_get(ctx);

    {
        BIGNUM *p = BN_new(), *q = BN_new();

        /*
         * Make sure BN_mod_inverse in Montgomery initialization uses the
         * BN_FLG_CONSTTIME flag
         */
        if (p == NULL || q == NULL) {
            BN_free(p);
            BN_free(q);
            goto err;
        }
        BN_with_flags(p, rsa->p, BN_FLG_CONSTTIME);
        BN_with_flags(q, rsa->q, BN_FLG_CONSTTIME);

        if (rsa->flags & RSA_FLAG_CACHE_PRIVATE) {
            if (!BN_MONT_CTX_set_locked
                (&rsa->_method_mod_p, rsa->lock, p, ctx)
                || !BN_MONT_CTX_set_locked(&rsa->_method_mod_q,
                                           rsa->lock, q, ctx)) {
                BN_free(p);
                BN_free(q);
                goto err;
            }
        }
        /*
         * We MUST free p and q before any further use of rsa->p and rsa->q
         */
        BN_free(p);
        BN_free(q);
    }

    if (rsa->flags & RSA_FLAG_CACHE_PUBLIC)
        if (!BN_MONT_CTX_set_locked
            (&rsa->_method_mod_n, rsa->lock, rsa->n, ctx))
            goto err;

    /* compute I mod q */
    {
        BIGNUM *c = BN_new();
        if (c == NULL)
            goto err;
        BN_with_flags(c, I, BN_FLG_CONSTTIME);

        if (!BN_mod(r1, c, rsa->q, ctx)) {
            BN_free(c);
            goto err;
        }

        {
            BIGNUM *dmq1 = BN_new();
            if (dmq1 == NULL) {
                BN_free(c);
                goto err;
            }
            BN_with_flags(dmq1, rsa->dmq1, BN_FLG_CONSTTIME);

            /* compute r1^dmq1 mod q */
            if (!rsa->meth->bn_mod_exp(m1, r1, dmq1, rsa->q, ctx,
                rsa->_method_mod_q)) {
                BN_free(c);
                BN_free(dmq1);
                goto err;
            }
            /* We MUST free dmq1 before any further use of rsa->dmq1 */
            BN_free(dmq1);
        }

        /* compute I mod p */
        if (!BN_mod(r1, c, rsa->p, ctx)) {
            BN_free(c);
            goto err;
        }
        /* We MUST free c before any further use of I */
        BN_free(c);
    }

    {
        BIGNUM *dmp1 = BN_new();
        if (dmp1 == NULL)
            goto err;
        BN_with_flags(dmp1, rsa->dmp1, BN_FLG_CONSTTIME);

        /* compute r1^dmp1 mod p */
        if (!rsa->meth->bn_mod_exp(r0, r1, dmp1, rsa->p, ctx,
                                   rsa->_method_mod_p)) {
            BN_free(dmp1);
            goto err;
        }
        /* We MUST free dmp1 before any further use of rsa->dmp1 */
        BN_free(dmp1);
    }

    if (!BN_sub(r0, r0, m1))
        goto err;
    /*
     * This will help stop the size of r0 increasing, which does affect the
     * multiply if it optimised for a power of 2 size
     */
    if (BN_is_negative(r0))
        if (!BN_add(r0, r0, rsa->p))
            goto err;

    if (!BN_mul(r1, r0, rsa->iqmp, ctx))
        goto err;

    {
        BIGNUM *pr1 = BN_new();
        if (pr1 == NULL)
            goto err;
        BN_with_flags(pr1, r1, BN_FLG_CONSTTIME);

        if (!BN_mod(r0, pr1, rsa->p, ctx)) {
            BN_free(pr1);
            goto err;
        }
        /* We MUST free pr1 before any further use of r1 */
        BN_free(pr1);
    }

    /*
     * If p < q it is occasionally possible for the correction of adding 'p'
     * if r0 is negative above to leave the result still negative. This can
     * break the private key operations: the following second correction
     * should *always* correct this rare occurrence. This will *never* happen
     * with OpenSSL generated keys because they ensure p > q [steve]
     */
    if (BN_is_negative(r0))
        if (!BN_add(r0, r0, rsa->p))
            goto err;
    if (!BN_mul(r1, r0, rsa->q, ctx))
        goto err;
    if (!BN_add(r0, r1, m1))
        goto err;

    if (rsa->e && rsa->n) {
        if (!rsa->meth->bn_mod_exp(vrfy, r0, rsa->e, rsa->n, ctx,
                                   rsa->_method_mod_n))
            goto err;
        /*
         * If 'I' was greater than (or equal to) rsa->n, the operation will
         * be equivalent to using 'I mod n'. However, the result of the
         * verify will *always* be less than 'n' so we don't check for
         * absolute equality, just congruency.
         */
        if (!BN_sub(vrfy, vrfy, I))
            goto err;
        if (!BN_mod(vrfy, vrfy, rsa->n, ctx))
            goto err;
        if (BN_is_negative(vrfy))
            if (!BN_add(vrfy, vrfy, rsa->n))
                goto err;
        if (!BN_is_zero(vrfy)) {
            /*
             * 'I' and 'vrfy' aren't congruent mod n. Don't leak
             * miscalculated CRT output, just do a raw (slower) mod_exp and
             * return that instead.
             */

            BIGNUM *d = BN_new();
            if (d == NULL)
                goto err;
            BN_with_flags(d, rsa->d, BN_FLG_CONSTTIME);

            if (!rsa->meth->bn_mod_exp(r0, I, d, rsa->n, ctx,
                                       rsa->_method_mod_n)) {
                BN_free(d);
                goto err;
            }
            /* We MUST free d before any further use of rsa->d */
            BN_free(d);
        }
    }
    ret = 1;
 err:
    BN_CTX_end(ctx);
    return (ret);
}

static int rsa_ossl_init(RSA *rsa)
{
    rsa->flags |= RSA_FLAG_CACHE_PUBLIC | RSA_FLAG_CACHE_PRIVATE;
    return (1);
}

static int rsa_ossl_finish(RSA *rsa)
{
    BN_MONT_CTX_free(rsa->_method_mod_n);
    BN_MONT_CTX_free(rsa->_method_mod_p);
    BN_MONT_CTX_free(rsa->_method_mod_q);
    return (1);
}

#endif
