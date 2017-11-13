/* crypto/ecdh/ech_lib.c */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * The Elliptic Curve Public-Key Crypto Library (ECC Code) included
 * herein is developed by SUN MICROSYSTEMS, INC., and is contributed
 * to the OpenSSL project.
 *
 * The ECC Code is licensed pursuant to the OpenSSL open source
 * license provided below.
 *
 * The ECDH software is originally written by Douglas Stebila of
 * Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright (c) 1998-2003 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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

#include "ech_locl.h"
#include <string.h>
#ifndef OPENSSL_NO_ENGINE
# include <openssl/engine.h>
#endif
#include <openssl/err.h>
#ifdef OPENSSL_FIPS
# include <openssl/fips.h>
#endif

const char ECDH_version[] = "ECDH" OPENSSL_VERSION_PTEXT;

static const ECDH_METHOD *default_ECDH_method = NULL;

static void *ecdh_data_new(void);
static void *ecdh_data_dup(void *);
static void ecdh_data_free(void *);

void ECDH_set_default_method(const ECDH_METHOD *meth)
{
    default_ECDH_method = meth;
}

const ECDH_METHOD *ECDH_get_default_method(void)
{
    if (!default_ECDH_method) {
#ifdef OPENSSL_FIPS
        if (FIPS_mode())
            return FIPS_ecdh_openssl();
        else
            return ECDH_OpenSSL();
#else
        default_ECDH_method = ECDH_OpenSSL();
#endif
    }
    return default_ECDH_method;
}

int ECDH_set_method(EC_KEY *eckey, const ECDH_METHOD *meth)
{
    ECDH_DATA *ecdh;

    ecdh = ecdh_check(eckey);

    if (ecdh == NULL)
        return 0;

#if 0
    mtmp = ecdh->meth;
    if (mtmp->finish)
        mtmp->finish(eckey);
#endif
#ifndef OPENSSL_NO_ENGINE
    if (ecdh->engine) {
        ENGINE_finish(ecdh->engine);
        ecdh->engine = NULL;
    }
#endif
    ecdh->meth = meth;
#if 0
    if (meth->init)
        meth->init(eckey);
#endif
    return 1;
}

static ECDH_DATA *ECDH_DATA_new_method(ENGINE *engine)
{
    ECDH_DATA *ret;

    ret = (ECDH_DATA *)OPENSSL_malloc(sizeof(ECDH_DATA));
    if (ret == NULL) {
        ECDHerr(ECDH_F_ECDH_DATA_NEW_METHOD, ERR_R_MALLOC_FAILURE);
        return (NULL);
    }

    ret->init = NULL;

    ret->meth = ECDH_get_default_method();
    ret->engine = engine;
#ifndef OPENSSL_NO_ENGINE
    if (!ret->engine)
        ret->engine = ENGINE_get_default_ECDH();
    if (ret->engine) {
        ret->meth = ENGINE_get_ECDH(ret->engine);
        if (!ret->meth) {
            ECDHerr(ECDH_F_ECDH_DATA_NEW_METHOD, ERR_R_ENGINE_LIB);
            ENGINE_finish(ret->engine);
            OPENSSL_free(ret);
            return NULL;
        }
    }
#endif

    ret->flags = ret->meth->flags;
    CRYPTO_new_ex_data(CRYPTO_EX_INDEX_ECDH, ret, &ret->ex_data);
#if 0
    if ((ret->meth->init != NULL) && !ret->meth->init(ret)) {
        CRYPTO_free_ex_data(CRYPTO_EX_INDEX_ECDH, ret, &ret->ex_data);
        OPENSSL_free(ret);
        ret = NULL;
    }
#endif
    return (ret);
}

static void *ecdh_data_new(void)
{
    return (void *)ECDH_DATA_new_method(NULL);
}

static void *ecdh_data_dup(void *data)
{
    ECDH_DATA *r = (ECDH_DATA *)data;

    /* XXX: dummy operation */
    if (r == NULL)
        return NULL;

    return (void *)ecdh_data_new();
}

void ecdh_data_free(void *data)
{
    ECDH_DATA *r = (ECDH_DATA *)data;

#ifndef OPENSSL_NO_ENGINE
    if (r->engine)
        ENGINE_finish(r->engine);
#endif

    CRYPTO_free_ex_data(CRYPTO_EX_INDEX_ECDH, r, &r->ex_data);

    OPENSSL_cleanse((void *)r, sizeof(ECDH_DATA));

    OPENSSL_free(r);
}

ECDH_DATA *ecdh_check(EC_KEY *key)
{
    ECDH_DATA *ecdh_data;

    void *data = EC_KEY_get_key_method_data(key, ecdh_data_dup,
                                            ecdh_data_free, ecdh_data_free);
    if (data == NULL) {
        ecdh_data = (ECDH_DATA *)ecdh_data_new();
        if (ecdh_data == NULL)
            return NULL;
        data = EC_KEY_insert_key_method_data(key, (void *)ecdh_data,
                                             ecdh_data_dup, ecdh_data_free,
                                             ecdh_data_free);
        if (data != NULL) {
            /*
             * Another thread raced us to install the key_method data and
             * won.
             */
            ecdh_data_free(ecdh_data);
            ecdh_data = (ECDH_DATA *)data;
        } else if (EC_KEY_get_key_method_data(key, ecdh_data_dup,
                                              ecdh_data_free,
                                              ecdh_data_free) != ecdh_data) {
            /* Or an out of memory error in EC_KEY_insert_key_method_data. */
            ecdh_data_free(ecdh_data);
            return NULL;
        }
    } else {
        ecdh_data = (ECDH_DATA *)data;
    }
#ifdef OPENSSL_FIPS
    if (FIPS_mode() && !(ecdh_data->flags & ECDH_FLAG_FIPS_METHOD)
        && !(EC_KEY_get_flags(key) & EC_FLAG_NON_FIPS_ALLOW)) {
        ECDHerr(ECDH_F_ECDH_CHECK, ECDH_R_NON_FIPS_METHOD);
        return NULL;
    }
#endif

    return ecdh_data;
}

int ECDH_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
                          CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func)
{
    return CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_ECDH, argl, argp,
                                   new_func, dup_func, free_func);
}

int ECDH_set_ex_data(EC_KEY *d, int idx, void *arg)
{
    ECDH_DATA *ecdh;
    ecdh = ecdh_check(d);
    if (ecdh == NULL)
        return 0;
    return (CRYPTO_set_ex_data(&ecdh->ex_data, idx, arg));
}

void *ECDH_get_ex_data(EC_KEY *d, int idx)
{
    ECDH_DATA *ecdh;
    ecdh = ecdh_check(d);
    if (ecdh == NULL)
        return NULL;
    return (CRYPTO_get_ex_data(&ecdh->ex_data, idx));
}
