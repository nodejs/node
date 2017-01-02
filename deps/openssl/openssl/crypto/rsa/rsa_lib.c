/* crypto/rsa/rsa_lib.c */
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

#include <stdio.h>
#include <openssl/crypto.h>
#include "cryptlib.h"
#include <openssl/lhash.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#ifndef OPENSSL_NO_ENGINE
# include <openssl/engine.h>
#endif

#ifdef OPENSSL_FIPS
# include <openssl/fips.h>
#endif

const char RSA_version[] = "RSA" OPENSSL_VERSION_PTEXT;

static const RSA_METHOD *default_RSA_meth = NULL;

RSA *RSA_new(void)
{
    RSA *r = RSA_new_method(NULL);

    return r;
}

void RSA_set_default_method(const RSA_METHOD *meth)
{
    default_RSA_meth = meth;
}

const RSA_METHOD *RSA_get_default_method(void)
{
    if (default_RSA_meth == NULL) {
#ifdef OPENSSL_FIPS
        if (FIPS_mode())
            return FIPS_rsa_pkcs1_ssleay();
        else
            return RSA_PKCS1_SSLeay();
#else
# ifdef RSA_NULL
        default_RSA_meth = RSA_null_method();
# else
        default_RSA_meth = RSA_PKCS1_SSLeay();
# endif
#endif
    }

    return default_RSA_meth;
}

const RSA_METHOD *RSA_get_method(const RSA *rsa)
{
    return rsa->meth;
}

int RSA_set_method(RSA *rsa, const RSA_METHOD *meth)
{
    /*
     * NB: The caller is specifically setting a method, so it's not up to us
     * to deal with which ENGINE it comes from.
     */
    const RSA_METHOD *mtmp;
    mtmp = rsa->meth;
    if (mtmp->finish)
        mtmp->finish(rsa);
#ifndef OPENSSL_NO_ENGINE
    if (rsa->engine) {
        ENGINE_finish(rsa->engine);
        rsa->engine = NULL;
    }
#endif
    rsa->meth = meth;
    if (meth->init)
        meth->init(rsa);
    return 1;
}

RSA *RSA_new_method(ENGINE *engine)
{
    RSA *ret;

    ret = (RSA *)OPENSSL_malloc(sizeof(RSA));
    if (ret == NULL) {
        RSAerr(RSA_F_RSA_NEW_METHOD, ERR_R_MALLOC_FAILURE);
        return NULL;
    }
    memset(ret,0,sizeof(RSA));

    ret->meth = RSA_get_default_method();
#ifndef OPENSSL_NO_ENGINE
    if (engine) {
        if (!ENGINE_init(engine)) {
            RSAerr(RSA_F_RSA_NEW_METHOD, ERR_R_ENGINE_LIB);
            OPENSSL_free(ret);
            return NULL;
        }
        ret->engine = engine;
    } else
        ret->engine = ENGINE_get_default_RSA();
    if (ret->engine) {
        ret->meth = ENGINE_get_RSA(ret->engine);
        if (!ret->meth) {
            RSAerr(RSA_F_RSA_NEW_METHOD, ERR_R_ENGINE_LIB);
            ENGINE_finish(ret->engine);
            OPENSSL_free(ret);
            return NULL;
        }
    }
#endif

    ret->pad = 0;
    ret->version = 0;
    ret->n = NULL;
    ret->e = NULL;
    ret->d = NULL;
    ret->p = NULL;
    ret->q = NULL;
    ret->dmp1 = NULL;
    ret->dmq1 = NULL;
    ret->iqmp = NULL;
    ret->references = 1;
    ret->_method_mod_n = NULL;
    ret->_method_mod_p = NULL;
    ret->_method_mod_q = NULL;
    ret->blinding = NULL;
    ret->mt_blinding = NULL;
    ret->bignum_data = NULL;
    ret->flags = ret->meth->flags & ~RSA_FLAG_NON_FIPS_ALLOW;
    if (!CRYPTO_new_ex_data(CRYPTO_EX_INDEX_RSA, ret, &ret->ex_data)) {
#ifndef OPENSSL_NO_ENGINE
        if (ret->engine)
            ENGINE_finish(ret->engine);
#endif
        OPENSSL_free(ret);
        return (NULL);
    }

    if ((ret->meth->init != NULL) && !ret->meth->init(ret)) {
#ifndef OPENSSL_NO_ENGINE
        if (ret->engine)
            ENGINE_finish(ret->engine);
#endif
        CRYPTO_free_ex_data(CRYPTO_EX_INDEX_RSA, ret, &ret->ex_data);
        OPENSSL_free(ret);
        ret = NULL;
    }
    return (ret);
}

void RSA_free(RSA *r)
{
    int i;

    if (r == NULL)
        return;

    i = CRYPTO_add(&r->references, -1, CRYPTO_LOCK_RSA);
#ifdef REF_PRINT
    REF_PRINT("RSA", r);
#endif
    if (i > 0)
        return;
#ifdef REF_CHECK
    if (i < 0) {
        fprintf(stderr, "RSA_free, bad reference count\n");
        abort();
    }
#endif

    if (r->meth->finish)
        r->meth->finish(r);
#ifndef OPENSSL_NO_ENGINE
    if (r->engine)
        ENGINE_finish(r->engine);
#endif

    CRYPTO_free_ex_data(CRYPTO_EX_INDEX_RSA, r, &r->ex_data);

    if (r->n != NULL)
        BN_clear_free(r->n);
    if (r->e != NULL)
        BN_clear_free(r->e);
    if (r->d != NULL)
        BN_clear_free(r->d);
    if (r->p != NULL)
        BN_clear_free(r->p);
    if (r->q != NULL)
        BN_clear_free(r->q);
    if (r->dmp1 != NULL)
        BN_clear_free(r->dmp1);
    if (r->dmq1 != NULL)
        BN_clear_free(r->dmq1);
    if (r->iqmp != NULL)
        BN_clear_free(r->iqmp);
    if (r->blinding != NULL)
        BN_BLINDING_free(r->blinding);
    if (r->mt_blinding != NULL)
        BN_BLINDING_free(r->mt_blinding);
    if (r->bignum_data != NULL)
        OPENSSL_free_locked(r->bignum_data);
    OPENSSL_free(r);
}

int RSA_up_ref(RSA *r)
{
    int i = CRYPTO_add(&r->references, 1, CRYPTO_LOCK_RSA);
#ifdef REF_PRINT
    REF_PRINT("RSA", r);
#endif
#ifdef REF_CHECK
    if (i < 2) {
        fprintf(stderr, "RSA_up_ref, bad reference count\n");
        abort();
    }
#endif
    return ((i > 1) ? 1 : 0);
}

int RSA_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
                         CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func)
{
    return CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_RSA, argl, argp,
                                   new_func, dup_func, free_func);
}

int RSA_set_ex_data(RSA *r, int idx, void *arg)
{
    return (CRYPTO_set_ex_data(&r->ex_data, idx, arg));
}

void *RSA_get_ex_data(const RSA *r, int idx)
{
    return (CRYPTO_get_ex_data(&r->ex_data, idx));
}

int RSA_memory_lock(RSA *r)
{
    int i, j, k, off;
    char *p;
    BIGNUM *bn, **t[6], *b;
    BN_ULONG *ul;

    if (r->d == NULL)
        return (1);
    t[0] = &r->d;
    t[1] = &r->p;
    t[2] = &r->q;
    t[3] = &r->dmp1;
    t[4] = &r->dmq1;
    t[5] = &r->iqmp;
    k = sizeof(BIGNUM) * 6;
    off = k / sizeof(BN_ULONG) + 1;
    j = 1;
    for (i = 0; i < 6; i++)
        j += (*t[i])->top;
    if ((p = OPENSSL_malloc_locked((off + j) * sizeof(BN_ULONG))) == NULL) {
        RSAerr(RSA_F_RSA_MEMORY_LOCK, ERR_R_MALLOC_FAILURE);
        return (0);
    }
    bn = (BIGNUM *)p;
    ul = (BN_ULONG *)&(p[off]);
    for (i = 0; i < 6; i++) {
        b = *(t[i]);
        *(t[i]) = &(bn[i]);
        memcpy((char *)&(bn[i]), (char *)b, sizeof(BIGNUM));
        bn[i].flags = BN_FLG_STATIC_DATA;
        bn[i].d = ul;
        memcpy((char *)ul, b->d, sizeof(BN_ULONG) * b->top);
        ul += b->top;
        BN_clear_free(b);
    }

    /* I should fix this so it can still be done */
    r->flags &= ~(RSA_FLAG_CACHE_PRIVATE | RSA_FLAG_CACHE_PUBLIC);

    r->bignum_data = p;
    return (1);
}
