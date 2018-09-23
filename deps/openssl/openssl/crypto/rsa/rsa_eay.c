/* crypto/rsa/rsa_eay.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>

#ifndef RSA_NULL

static int RSA_eay_public_encrypt(int flen, const unsigned char *from,
                                  unsigned char *to, RSA *rsa, int padding);
static int RSA_eay_private_encrypt(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding);
static int RSA_eay_public_decrypt(int flen, const unsigned char *from,
                                  unsigned char *to, RSA *rsa, int padding);
static int RSA_eay_private_decrypt(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding);
static int RSA_eay_mod_exp(BIGNUM *r0, const BIGNUM *i, RSA *rsa,
                           BN_CTX *ctx);
static int RSA_eay_init(RSA *rsa);
static int RSA_eay_finish(RSA *rsa);
static RSA_METHOD rsa_pkcs1_eay_meth = {
    "Eric Young's PKCS#1 RSA",
    RSA_eay_public_encrypt,
    RSA_eay_public_decrypt,     /* signature verification */
    RSA_eay_private_encrypt,    /* signing */
    RSA_eay_private_decrypt,
    RSA_eay_mod_exp,
    BN_mod_exp_mont,            /* XXX probably we should not use Montgomery
                                 * if e == 3 */
    RSA_eay_init,
    RSA_eay_finish,
    0,                          /* flags */
    NULL,
    0,                          /* rsa_sign */
    0,                          /* rsa_verify */
    NULL                        /* rsa_keygen */
};

const RSA_METHOD *RSA_PKCS1_SSLeay(void)
{
    return (&rsa_pkcs1_eay_meth);
}

static int RSA_eay_public_encrypt(int flen, const unsigned char *from,
                                  unsigned char *to, RSA *rsa, int padding)
{
    BIGNUM *f, *ret;
    int i, j, k, num = 0, r = -1;
    unsigned char *buf = NULL;
    BN_CTX *ctx = NULL;

    if (BN_num_bits(rsa->n) > OPENSSL_RSA_MAX_MODULUS_BITS) {
        RSAerr(RSA_F_RSA_EAY_PUBLIC_ENCRYPT, RSA_R_MODULUS_TOO_LARGE);
        return -1;
    }

    if (BN_ucmp(rsa->n, rsa->e) <= 0) {
        RSAerr(RSA_F_RSA_EAY_PUBLIC_ENCRYPT, RSA_R_BAD_E_VALUE);
        return -1;
    }

    /* for large moduli, enforce exponent limit */
    if (BN_num_bits(rsa->n) > OPENSSL_RSA_SMALL_MODULUS_BITS) {
        if (BN_num_bits(rsa->e) > OPENSSL_RSA_MAX_PUBEXP_BITS) {
            RSAerr(RSA_F_RSA_EAY_PUBLIC_ENCRYPT, RSA_R_BAD_E_VALUE);
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
    if (!f || !ret || !buf) {
        RSAerr(RSA_F_RSA_EAY_PUBLIC_ENCRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    switch (padding) {
    case RSA_PKCS1_PADDING:
        i = RSA_padding_add_PKCS1_type_2(buf, num, from, flen);
        break;
# ifndef OPENSSL_NO_SHA
    case RSA_PKCS1_OAEP_PADDING:
        i = RSA_padding_add_PKCS1_OAEP(buf, num, from, flen, NULL, 0);
        break;
# endif
    case RSA_SSLV23_PADDING:
        i = RSA_padding_add_SSLv23(buf, num, from, flen);
        break;
    case RSA_NO_PADDING:
        i = RSA_padding_add_none(buf, num, from, flen);
        break;
    default:
        RSAerr(RSA_F_RSA_EAY_PUBLIC_ENCRYPT, RSA_R_UNKNOWN_PADDING_TYPE);
        goto err;
    }
    if (i <= 0)
        goto err;

    if (BN_bin2bn(buf, num, f) == NULL)
        goto err;

    if (BN_ucmp(f, rsa->n) >= 0) {
        /* usually the padding functions would catch this */
        RSAerr(RSA_F_RSA_EAY_PUBLIC_ENCRYPT,
               RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
        goto err;
    }

    if (rsa->flags & RSA_FLAG_CACHE_PUBLIC)
        if (!BN_MONT_CTX_set_locked
            (&rsa->_method_mod_n, CRYPTO_LOCK_RSA, rsa->n, ctx))
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
    if (ctx != NULL) {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
    if (buf != NULL) {
        OPENSSL_cleanse(buf, num);
        OPENSSL_free(buf);
    }
    return (r);
}

static BN_BLINDING *rsa_get_blinding(RSA *rsa, int *local, BN_CTX *ctx)
{
    BN_BLINDING *ret;
    int got_write_lock = 0;
    CRYPTO_THREADID cur;

    CRYPTO_r_lock(CRYPTO_LOCK_RSA);

    if (rsa->blinding == NULL) {
        CRYPTO_r_unlock(CRYPTO_LOCK_RSA);
        CRYPTO_w_lock(CRYPTO_LOCK_RSA);
        got_write_lock = 1;

        if (rsa->blinding == NULL)
            rsa->blinding = RSA_setup_blinding(rsa, ctx);
    }

    ret = rsa->blinding;
    if (ret == NULL)
        goto err;

    CRYPTO_THREADID_current(&cur);
    if (!CRYPTO_THREADID_cmp(&cur, BN_BLINDING_thread_id(ret))) {
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
            if (!got_write_lock) {
                CRYPTO_r_unlock(CRYPTO_LOCK_RSA);
                CRYPTO_w_lock(CRYPTO_LOCK_RSA);
                got_write_lock = 1;
            }

            if (rsa->mt_blinding == NULL)
                rsa->mt_blinding = RSA_setup_blinding(rsa, ctx);
        }
        ret = rsa->mt_blinding;
    }

 err:
    if (got_write_lock)
        CRYPTO_w_unlock(CRYPTO_LOCK_RSA);
    else
        CRYPTO_r_unlock(CRYPTO_LOCK_RSA);
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
        CRYPTO_w_lock(CRYPTO_LOCK_RSA_BLINDING);
        ret = BN_BLINDING_convert_ex(f, unblind, b, ctx);
        CRYPTO_w_unlock(CRYPTO_LOCK_RSA_BLINDING);
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
static int RSA_eay_private_encrypt(int flen, const unsigned char *from,
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
    if (!f || !ret || !buf) {
        RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT, ERR_R_MALLOC_FAILURE);
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
        RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT, RSA_R_UNKNOWN_PADDING_TYPE);
        goto err;
    }
    if (i <= 0)
        goto err;

    if (BN_bin2bn(buf, num, f) == NULL)
        goto err;

    if (BN_ucmp(f, rsa->n) >= 0) {
        /* usually the padding functions would catch this */
        RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT,
               RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
        goto err;
    }

    if (!(rsa->flags & RSA_FLAG_NO_BLINDING)) {
        blinding = rsa_get_blinding(rsa, &local_blinding, ctx);
        if (blinding == NULL) {
            RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT, ERR_R_INTERNAL_ERROR);
            goto err;
        }
    }

    if (blinding != NULL) {
        if (!local_blinding && ((unblind = BN_CTX_get(ctx)) == NULL)) {
            RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT, ERR_R_MALLOC_FAILURE);
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
        BIGNUM local_d;
        BIGNUM *d = NULL;

        if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
            BN_init(&local_d);
            d = &local_d;
            BN_with_flags(d, rsa->d, BN_FLG_CONSTTIME);
        } else
            d = rsa->d;

        if (rsa->flags & RSA_FLAG_CACHE_PUBLIC)
            if (!BN_MONT_CTX_set_locked
                (&rsa->_method_mod_n, CRYPTO_LOCK_RSA, rsa->n, ctx))
                goto err;

        if (!rsa->meth->bn_mod_exp(ret, f, d, rsa->n, ctx,
                                   rsa->_method_mod_n))
            goto err;
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
    if (ctx != NULL) {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
    if (buf != NULL) {
        OPENSSL_cleanse(buf, num);
        OPENSSL_free(buf);
    }
    return (r);
}

static int RSA_eay_private_decrypt(int flen, const unsigned char *from,
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
    if (!f || !ret || !buf) {
        RSAerr(RSA_F_RSA_EAY_PRIVATE_DECRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    /*
     * This check was for equality but PGP does evil things and chops off the
     * top '0' bytes
     */
    if (flen > num) {
        RSAerr(RSA_F_RSA_EAY_PRIVATE_DECRYPT,
               RSA_R_DATA_GREATER_THAN_MOD_LEN);
        goto err;
    }

    /* make data into a big number */
    if (BN_bin2bn(from, (int)flen, f) == NULL)
        goto err;

    if (BN_ucmp(f, rsa->n) >= 0) {
        RSAerr(RSA_F_RSA_EAY_PRIVATE_DECRYPT,
               RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
        goto err;
    }

    if (!(rsa->flags & RSA_FLAG_NO_BLINDING)) {
        blinding = rsa_get_blinding(rsa, &local_blinding, ctx);
        if (blinding == NULL) {
            RSAerr(RSA_F_RSA_EAY_PRIVATE_DECRYPT, ERR_R_INTERNAL_ERROR);
            goto err;
        }
    }

    if (blinding != NULL) {
        if (!local_blinding && ((unblind = BN_CTX_get(ctx)) == NULL)) {
            RSAerr(RSA_F_RSA_EAY_PRIVATE_DECRYPT, ERR_R_MALLOC_FAILURE);
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
        BIGNUM local_d;
        BIGNUM *d = NULL;

        if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
            d = &local_d;
            BN_with_flags(d, rsa->d, BN_FLG_CONSTTIME);
        } else
            d = rsa->d;

        if (rsa->flags & RSA_FLAG_CACHE_PUBLIC)
            if (!BN_MONT_CTX_set_locked
                (&rsa->_method_mod_n, CRYPTO_LOCK_RSA, rsa->n, ctx))
                goto err;
        if (!rsa->meth->bn_mod_exp(ret, f, d, rsa->n, ctx,
                                   rsa->_method_mod_n))
            goto err;
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
# ifndef OPENSSL_NO_SHA
    case RSA_PKCS1_OAEP_PADDING:
        r = RSA_padding_check_PKCS1_OAEP(to, num, buf, j, num, NULL, 0);
        break;
# endif
    case RSA_SSLV23_PADDING:
        r = RSA_padding_check_SSLv23(to, num, buf, j, num);
        break;
    case RSA_NO_PADDING:
        r = RSA_padding_check_none(to, num, buf, j, num);
        break;
    default:
        RSAerr(RSA_F_RSA_EAY_PRIVATE_DECRYPT, RSA_R_UNKNOWN_PADDING_TYPE);
        goto err;
    }
    if (r < 0)
        RSAerr(RSA_F_RSA_EAY_PRIVATE_DECRYPT, RSA_R_PADDING_CHECK_FAILED);

 err:
    if (ctx != NULL) {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
    if (buf != NULL) {
        OPENSSL_cleanse(buf, num);
        OPENSSL_free(buf);
    }
    return (r);
}

/* signature verification */
static int RSA_eay_public_decrypt(int flen, const unsigned char *from,
                                  unsigned char *to, RSA *rsa, int padding)
{
    BIGNUM *f, *ret;
    int i, num = 0, r = -1;
    unsigned char *p;
    unsigned char *buf = NULL;
    BN_CTX *ctx = NULL;

    if (BN_num_bits(rsa->n) > OPENSSL_RSA_MAX_MODULUS_BITS) {
        RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT, RSA_R_MODULUS_TOO_LARGE);
        return -1;
    }

    if (BN_ucmp(rsa->n, rsa->e) <= 0) {
        RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT, RSA_R_BAD_E_VALUE);
        return -1;
    }

    /* for large moduli, enforce exponent limit */
    if (BN_num_bits(rsa->n) > OPENSSL_RSA_SMALL_MODULUS_BITS) {
        if (BN_num_bits(rsa->e) > OPENSSL_RSA_MAX_PUBEXP_BITS) {
            RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT, RSA_R_BAD_E_VALUE);
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
    if (!f || !ret || !buf) {
        RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    /*
     * This check was for equality but PGP does evil things and chops off the
     * top '0' bytes
     */
    if (flen > num) {
        RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT, RSA_R_DATA_GREATER_THAN_MOD_LEN);
        goto err;
    }

    if (BN_bin2bn(from, flen, f) == NULL)
        goto err;

    if (BN_ucmp(f, rsa->n) >= 0) {
        RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT,
               RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
        goto err;
    }

    if (rsa->flags & RSA_FLAG_CACHE_PUBLIC)
        if (!BN_MONT_CTX_set_locked
            (&rsa->_method_mod_n, CRYPTO_LOCK_RSA, rsa->n, ctx))
            goto err;

    if (!rsa->meth->bn_mod_exp(ret, f, rsa->e, rsa->n, ctx,
                               rsa->_method_mod_n))
        goto err;

    if ((padding == RSA_X931_PADDING) && ((ret->d[0] & 0xf) != 12))
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
        RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT, RSA_R_UNKNOWN_PADDING_TYPE);
        goto err;
    }
    if (r < 0)
        RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT, RSA_R_PADDING_CHECK_FAILED);

 err:
    if (ctx != NULL) {
        BN_CTX_end(ctx);
        BN_CTX_free(ctx);
    }
    if (buf != NULL) {
        OPENSSL_cleanse(buf, num);
        OPENSSL_free(buf);
    }
    return (r);
}

static int RSA_eay_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx)
{
    BIGNUM *r1, *m1, *vrfy;
    BIGNUM local_dmp1, local_dmq1, local_c, local_r1;
    BIGNUM *dmp1, *dmq1, *c, *pr1;
    int ret = 0;

    BN_CTX_start(ctx);
    r1 = BN_CTX_get(ctx);
    m1 = BN_CTX_get(ctx);
    vrfy = BN_CTX_get(ctx);

    {
        BIGNUM local_p, local_q;
        BIGNUM *p = NULL, *q = NULL;

        /*
         * Make sure BN_mod_inverse in Montgomery intialization uses the
         * BN_FLG_CONSTTIME flag (unless RSA_FLAG_NO_CONSTTIME is set)
         */
        if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
            BN_init(&local_p);
            p = &local_p;
            BN_with_flags(p, rsa->p, BN_FLG_CONSTTIME);

            BN_init(&local_q);
            q = &local_q;
            BN_with_flags(q, rsa->q, BN_FLG_CONSTTIME);
        } else {
            p = rsa->p;
            q = rsa->q;
        }

        if (rsa->flags & RSA_FLAG_CACHE_PRIVATE) {
            if (!BN_MONT_CTX_set_locked
                (&rsa->_method_mod_p, CRYPTO_LOCK_RSA, p, ctx))
                goto err;
            if (!BN_MONT_CTX_set_locked
                (&rsa->_method_mod_q, CRYPTO_LOCK_RSA, q, ctx))
                goto err;
        }
    }

    if (rsa->flags & RSA_FLAG_CACHE_PUBLIC)
        if (!BN_MONT_CTX_set_locked
            (&rsa->_method_mod_n, CRYPTO_LOCK_RSA, rsa->n, ctx))
            goto err;

    /* compute I mod q */
    if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
        c = &local_c;
        BN_with_flags(c, I, BN_FLG_CONSTTIME);
        if (!BN_mod(r1, c, rsa->q, ctx))
            goto err;
    } else {
        if (!BN_mod(r1, I, rsa->q, ctx))
            goto err;
    }

    /* compute r1^dmq1 mod q */
    if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
        dmq1 = &local_dmq1;
        BN_with_flags(dmq1, rsa->dmq1, BN_FLG_CONSTTIME);
    } else
        dmq1 = rsa->dmq1;
    if (!rsa->meth->bn_mod_exp(m1, r1, dmq1, rsa->q, ctx, rsa->_method_mod_q))
        goto err;

    /* compute I mod p */
    if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
        c = &local_c;
        BN_with_flags(c, I, BN_FLG_CONSTTIME);
        if (!BN_mod(r1, c, rsa->p, ctx))
            goto err;
    } else {
        if (!BN_mod(r1, I, rsa->p, ctx))
            goto err;
    }

    /* compute r1^dmp1 mod p */
    if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
        dmp1 = &local_dmp1;
        BN_with_flags(dmp1, rsa->dmp1, BN_FLG_CONSTTIME);
    } else
        dmp1 = rsa->dmp1;
    if (!rsa->meth->bn_mod_exp(r0, r1, dmp1, rsa->p, ctx, rsa->_method_mod_p))
        goto err;

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

    /* Turn BN_FLG_CONSTTIME flag on before division operation */
    if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
        pr1 = &local_r1;
        BN_with_flags(pr1, r1, BN_FLG_CONSTTIME);
    } else
        pr1 = r1;
    if (!BN_mod(r0, pr1, rsa->p, ctx))
        goto err;

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

            BIGNUM local_d;
            BIGNUM *d = NULL;

            if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
                d = &local_d;
                BN_with_flags(d, rsa->d, BN_FLG_CONSTTIME);
            } else
                d = rsa->d;
            if (!rsa->meth->bn_mod_exp(r0, I, d, rsa->n, ctx,
                                       rsa->_method_mod_n))
                goto err;
        }
    }
    ret = 1;
 err:
    BN_CTX_end(ctx);
    return (ret);
}

static int RSA_eay_init(RSA *rsa)
{
    rsa->flags |= RSA_FLAG_CACHE_PUBLIC | RSA_FLAG_CACHE_PRIVATE;
    return (1);
}

static int RSA_eay_finish(RSA *rsa)
{
    if (rsa->_method_mod_n != NULL)
        BN_MONT_CTX_free(rsa->_method_mod_n);
    if (rsa->_method_mod_p != NULL)
        BN_MONT_CTX_free(rsa->_method_mod_p);
    if (rsa->_method_mod_q != NULL)
        BN_MONT_CTX_free(rsa->_method_mod_q);
    return (1);
}

#endif
