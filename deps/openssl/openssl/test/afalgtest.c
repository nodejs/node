/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/opensslconf.h>

#ifndef OPENSSL_NO_AFALGENG
# include <linux/version.h>
# define K_MAJ   4
# define K_MIN1  1
# define K_MIN2  0
# if LINUX_VERSION_CODE < KERNEL_VERSION(K_MAJ, K_MIN1, K_MIN2)
/*
 * If we get here then it looks like there is a mismatch between the linux
 * headers and the actual kernel version, so we have tried to compile with
 * afalg support, but then skipped it in e_afalg.c. As far as this test is
 * concerned we behave as if we had been configured without support
 */
#  define OPENSSL_NO_AFALGENG
# endif
#endif

#ifndef OPENSSL_NO_AFALGENG
#include <string.h>
#include <openssl/engine.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

/* Use a buffer size which is not aligned to block size */
#define BUFFER_SIZE     (8 * 1024) - 13

static int test_afalg_aes_128_cbc(ENGINE *e)
{
    EVP_CIPHER_CTX *ctx;
    const EVP_CIPHER *cipher = EVP_aes_128_cbc();
    unsigned char key[] = "\x5F\x4D\xCC\x3B\x5A\xA7\x65\xD6\
                           \x1D\x83\x27\xDE\xB8\x82\xCF\x99";
    unsigned char iv[] = "\x2B\x95\x99\x0A\x91\x51\x37\x4A\
                          \xBD\x8F\xF8\xC5\xA7\xA0\xFE\x08";

    unsigned char in[BUFFER_SIZE];
    unsigned char ebuf[BUFFER_SIZE + 32];
    unsigned char dbuf[BUFFER_SIZE + 32];
    int encl, encf, decl, decf;
    unsigned int status = 0;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        fprintf(stderr, "%s() failed to allocate ctx\n", __func__);
        return 0;
    }
    RAND_bytes(in, BUFFER_SIZE);

    if (       !EVP_CipherInit_ex(ctx, cipher, e, key, iv, 1)
            || !EVP_CipherUpdate(ctx, ebuf, &encl, in, BUFFER_SIZE)
            || !EVP_CipherFinal_ex(ctx, ebuf+encl, &encf)) {
        fprintf(stderr, "%s() failed encryption\n", __func__);
        goto end;
    }
    encl += encf;

    if (       !EVP_CIPHER_CTX_reset(ctx)
            || !EVP_CipherInit_ex(ctx, cipher, e, key, iv, 0)
            || !EVP_CipherUpdate(ctx, dbuf, &decl, ebuf, encl)
            || !EVP_CipherFinal_ex(ctx, dbuf+decl, &decf)) {
        fprintf(stderr, "%s() failed decryption\n", __func__);
        goto end;
    }
    decl += decf;

    if (       decl != BUFFER_SIZE
            || memcmp(dbuf, in, BUFFER_SIZE)) {
        fprintf(stderr, "%s() failed Dec(Enc(P)) != P\n", __func__);
        goto end;
    }

    status = 1;

 end:
    EVP_CIPHER_CTX_free(ctx);
    return status;
}

int main(int argc, char **argv)
{
    ENGINE *e;

    CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    ENGINE_load_builtin_engines();

# ifndef OPENSSL_NO_STATIC_ENGINE
    OPENSSL_init_crypto(OPENSSL_INIT_ENGINE_AFALG, NULL);
# endif

    e = ENGINE_by_id("afalg");
    if (e == NULL) {
        /*
         * A failure to load is probably a platform environment problem so we
         * don't treat this as an OpenSSL test failure, i.e. we return 0
         */
        fprintf(stderr,
                "AFALG Test: Failed to load AFALG Engine - skipping test\n");
        return 0;
    }

    if (test_afalg_aes_128_cbc(e) == 0) {
        ENGINE_free(e);
        return 1;
    }

    ENGINE_free(e);
    printf("PASS\n");
    return 0;
}

#else  /* OPENSSL_NO_AFALGENG */

int main(int argc, char **argv)
{
    fprintf(stderr, "AFALG not supported - skipping AFALG tests\n");
    printf("PASS\n");
    return 0;
}

#endif
