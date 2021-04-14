/*
 * Copyright 2019-2021 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2019, Oracle and/or its affiliates.  All rights reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Implementation of the FIPS 140-2 section 4.9.2 Conditional Tests.
 */

#include <string.h>
#include <openssl/evp.h>
#include <openssl/core_dispatch.h>
#include <openssl/params.h>
#include <openssl/self_test.h>
#include "prov/providercommon.h"
#include "prov/provider_ctx.h"
#include "internal/cryptlib.h"
#include "crypto/rand_pool.h"
#include "drbg_local.h"
#include "prov/seeding.h"

typedef struct crng_test_global_st {
    unsigned char crngt_prev[EVP_MAX_MD_SIZE];
    EVP_MD *md;
    int preloaded;
    CRYPTO_RWLOCK *lock;
} CRNG_TEST_GLOBAL;

static int crngt_get_entropy(PROV_CTX *provctx, const EVP_MD *digest,
                             unsigned char *buf, unsigned char *md,
                             unsigned int *md_size)
{
    int r;
    size_t n;
    unsigned char *p;

    n = ossl_prov_get_entropy(provctx, &p, 0, CRNGT_BUFSIZ, CRNGT_BUFSIZ);
    if (n == CRNGT_BUFSIZ) {
        r = EVP_Digest(p, CRNGT_BUFSIZ, md, md_size, digest, NULL);
        if (r != 0)
            memcpy(buf, p, CRNGT_BUFSIZ);
        ossl_prov_cleanup_entropy(provctx, p, n);
        return r != 0;
    }
    if (n != 0)
        ossl_prov_cleanup_entropy(provctx, p, n);
    return 0;
}

static void rand_crng_ossl_ctx_free(void *vcrngt_glob)
{
    CRNG_TEST_GLOBAL *crngt_glob = vcrngt_glob;

    CRYPTO_THREAD_lock_free(crngt_glob->lock);
    EVP_MD_free(crngt_glob->md);
    OPENSSL_free(crngt_glob);
}

static void *rand_crng_ossl_ctx_new(OSSL_LIB_CTX *ctx)
{
    CRNG_TEST_GLOBAL *crngt_glob = OPENSSL_zalloc(sizeof(*crngt_glob));

    if (crngt_glob == NULL)
        return NULL;

    if ((crngt_glob->md = EVP_MD_fetch(ctx, "SHA256", "")) == NULL) {
        OPENSSL_free(crngt_glob);
        return NULL;
    }

    if ((crngt_glob->lock = CRYPTO_THREAD_lock_new()) == NULL) {
        EVP_MD_free(crngt_glob->md);
        OPENSSL_free(crngt_glob);
        return NULL;
    }

    return crngt_glob;
}

static const OSSL_LIB_CTX_METHOD rand_crng_ossl_ctx_method = {
    OSSL_LIB_CTX_METHOD_DEFAULT_PRIORITY,
    rand_crng_ossl_ctx_new,
    rand_crng_ossl_ctx_free,
};

static int prov_crngt_compare_previous(const unsigned char *prev,
                                       const unsigned char *cur,
                                       size_t sz)
{
    const int res = memcmp(prev, cur, sz) != 0;

    if (!res)
        ossl_set_error_state(OSSL_SELF_TEST_TYPE_CRNG);
    return res;
}

size_t ossl_crngt_get_entropy(PROV_DRBG *drbg,
                              unsigned char **pout,
                              int entropy, size_t min_len, size_t max_len,
                              int prediction_resistance)
{
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned char buf[CRNGT_BUFSIZ];
    unsigned char *ent, *entp, *entbuf;
    unsigned int sz;
    size_t bytes_needed;
    size_t r = 0, s, t;
    int crng_test_pass = 1;
    OSSL_LIB_CTX *libctx = ossl_prov_ctx_get0_libctx(drbg->provctx);
    CRNG_TEST_GLOBAL *crngt_glob
        = ossl_lib_ctx_get_data(libctx, OSSL_LIB_CTX_RAND_CRNGT_INDEX,
                                &rand_crng_ossl_ctx_method);
    OSSL_CALLBACK *stcb = NULL;
    void *stcbarg = NULL;
    OSSL_SELF_TEST *st = NULL;

    if (crngt_glob == NULL)
        return 0;

    if (!CRYPTO_THREAD_write_lock(crngt_glob->lock))
        return 0;

    if (!crngt_glob->preloaded) {
        if (!crngt_get_entropy(drbg->provctx, crngt_glob->md, buf,
                               crngt_glob->crngt_prev, NULL)) {
            OPENSSL_cleanse(buf, sizeof(buf));
            goto unlock_return;
        }
        crngt_glob->preloaded = 1;
    }

    /*
     * Calculate how many bytes of seed material we require, rounded up
     * to the nearest byte.  If the entropy is of less than full quality,
     * the amount required should be scaled up appropriately here.
     */
    bytes_needed = (entropy + 7) / 8;
    if (bytes_needed < min_len)
        bytes_needed = min_len;
    if (bytes_needed > max_len)
        goto unlock_return;
    entp = ent = OPENSSL_secure_malloc(bytes_needed);
    if (ent == NULL)
        goto unlock_return;

    OSSL_SELF_TEST_get_callback(libctx, &stcb, &stcbarg);
    if (stcb != NULL) {
        st = OSSL_SELF_TEST_new(stcb, stcbarg);
        if (st == NULL)
            goto err;
        OSSL_SELF_TEST_onbegin(st, OSSL_SELF_TEST_TYPE_CRNG,
                               OSSL_SELF_TEST_DESC_RNG);
    }

    for (t = bytes_needed; t > 0;) {
        /* Care needs to be taken to avoid overrunning the buffer */
        s = t >= CRNGT_BUFSIZ ? CRNGT_BUFSIZ : t;
        entbuf = t >= CRNGT_BUFSIZ ? entp : buf;
        if (!crngt_get_entropy(drbg->provctx, crngt_glob->md, entbuf, md, &sz))
            goto err;
        if (t < CRNGT_BUFSIZ)
            memcpy(entp, buf, t);
        /* Force a failure here if the callback returns 1 */
        if (OSSL_SELF_TEST_oncorrupt_byte(st, md))
            memcpy(md, crngt_glob->crngt_prev, sz);
        if (!prov_crngt_compare_previous(crngt_glob->crngt_prev, md, sz)) {
            crng_test_pass = 0;
            goto err;
        }
        /* Update for next block */
        memcpy(crngt_glob->crngt_prev, md, sz);
        entp += s;
        t -= s;
    }
    r = bytes_needed;
    *pout = ent;
    ent = NULL;

 err:
    OSSL_SELF_TEST_onend(st, crng_test_pass);
    OSSL_SELF_TEST_free(st);
    OPENSSL_secure_clear_free(ent, bytes_needed);

 unlock_return:
    CRYPTO_THREAD_unlock(crngt_glob->lock);
    return r;
}

void ossl_crngt_cleanup_entropy(ossl_unused PROV_DRBG *drbg,
                                unsigned char *out, size_t outlen)
{
    OPENSSL_secure_clear_free(out, outlen);
}
