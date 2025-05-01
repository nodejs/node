/*
 * Copyright 2012-2024 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Simple ARIA CBC encryption demonstration program.
 */

#include <stdio.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/core_names.h>

/* ARIA key */
static const unsigned char cbc_key[] = {
    0xee, 0xbc, 0x1f, 0x57, 0x48, 0x7f, 0x51, 0x92, 0x1c, 0x04, 0x65, 0x66,
    0x5f, 0x8a, 0xe6, 0xd1, 0x65, 0x8b, 0xb2, 0x6d, 0xe6, 0xf8, 0xa0, 0x69,
    0xa3, 0x52, 0x02, 0x93, 0xa5, 0x72, 0x07, 0x8f
};

/* Unique initialisation vector */
static const unsigned char cbc_iv[] = {
    0x99, 0xaa, 0x3e, 0x68, 0xed, 0x81, 0x73, 0xa0, 0xee, 0xd0, 0x66, 0x84,
    0x99, 0xaa, 0x3e, 0x68,
};

/* Example plaintext to encrypt */
static const unsigned char cbc_pt[] = {
    0xf5, 0x6e, 0x87, 0x05, 0x5b, 0xc3, 0x2d, 0x0e, 0xeb, 0x31, 0xb2, 0xea,
    0xcc, 0x2b, 0xf2, 0xa5
};

/* Expected ciphertext value */
static const unsigned char cbc_ct[] = {
    0x9a, 0x44, 0xe6, 0x85, 0x94, 0x26, 0xff, 0x30, 0x03, 0xd3, 0x7e, 0xc6,
    0xb5, 0x4a, 0x09, 0x66, 0x39, 0x28, 0xf3, 0x67, 0x14, 0xbc, 0xe8, 0xe2,
    0xcf, 0x31, 0xb8, 0x60, 0x42, 0x72, 0x6d, 0xc8
};

/*
 * A library context and property query can be used to select & filter
 * algorithm implementations. If they are NULL then the default library
 * context and properties are used.
 */
static OSSL_LIB_CTX *libctx = NULL;
static const char *propq = NULL;

static int aria_cbc_encrypt(void)
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx;
    EVP_CIPHER *cipher = NULL;
    int outlen, tmplen;
    unsigned char outbuf[1024];

    printf("ARIA CBC Encrypt:\n");
    printf("Plaintext:\n");
    BIO_dump_fp(stdout, cbc_pt, sizeof(cbc_pt));

    /* Create a context for the encrypt operation */
    if ((ctx = EVP_CIPHER_CTX_new()) == NULL)
        goto err;

    /* Fetch the cipher implementation */
    if ((cipher = EVP_CIPHER_fetch(libctx, "ARIA-256-CBC", propq)) == NULL)
        goto err;

    /*
     * Initialise an encrypt operation with the cipher/mode, key and IV.
     * We are not setting any custom params so let params be just NULL.
     */
    if (!EVP_EncryptInit_ex2(ctx, cipher, cbc_key, cbc_iv, /* params */ NULL))
        goto err;

    /* Encrypt plaintext */
    if (!EVP_EncryptUpdate(ctx, outbuf, &outlen, cbc_pt, sizeof(cbc_pt)))
        goto err;

    /* Finalise: there can be some additional output from padding */
    if (!EVP_EncryptFinal_ex(ctx, outbuf + outlen, &tmplen))
        goto err;
    outlen += tmplen;

    /* Output encrypted block */
    printf("Ciphertext (outlen:%d):\n", outlen);
    BIO_dump_fp(stdout, outbuf, outlen);

    if (sizeof(cbc_ct) == outlen && !CRYPTO_memcmp(outbuf, cbc_ct, outlen))
        printf("Final ciphertext matches expected ciphertext\n");
    else
        printf("Final ciphertext differs from expected ciphertext\n");

    ret = 1;
err:
    if (!ret)
        ERR_print_errors_fp(stderr);

    EVP_CIPHER_free(cipher);
    EVP_CIPHER_CTX_free(ctx);

    return ret;
}

static int aria_cbc_decrypt(void)
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx;
    EVP_CIPHER *cipher = NULL;
    int outlen, tmplen;
    unsigned char outbuf[1024];

    printf("ARIA CBC Decrypt:\n");
    printf("Ciphertext:\n");
    BIO_dump_fp(stdout, cbc_ct, sizeof(cbc_ct));

    if ((ctx = EVP_CIPHER_CTX_new()) == NULL)
        goto err;

    /* Fetch the cipher implementation */
    if ((cipher = EVP_CIPHER_fetch(libctx, "ARIA-256-CBC", propq)) == NULL)
        goto err;

    /*
     * Initialise an encrypt operation with the cipher/mode, key and IV.
     * We are not setting any custom params so let params be just NULL.
     */
    if (!EVP_DecryptInit_ex2(ctx, cipher, cbc_key, cbc_iv, /* params */ NULL))
        goto err;

    /* Decrypt plaintext */
    if (!EVP_DecryptUpdate(ctx, outbuf, &outlen, cbc_ct, sizeof(cbc_ct)))
        goto err;

    /* Finalise: there can be some additional output from padding */
    if (!EVP_DecryptFinal_ex(ctx, outbuf + outlen, &tmplen))
        goto err;
    outlen += tmplen;

    /* Output decrypted block */
    printf("Plaintext (outlen:%d):\n", outlen);
    BIO_dump_fp(stdout, outbuf, outlen);

    if (sizeof(cbc_pt) == outlen && !CRYPTO_memcmp(outbuf, cbc_pt, outlen))
        printf("Final plaintext matches original plaintext\n");
    else
        printf("Final plaintext differs from original plaintext\n");

    ret = 1;
err:
    if (!ret)
        ERR_print_errors_fp(stderr);

    EVP_CIPHER_free(cipher);
    EVP_CIPHER_CTX_free(ctx);

    return ret;
}

int main(int argc, char **argv)
{
    if (!aria_cbc_encrypt())
        return EXIT_FAILURE;

    if (!aria_cbc_decrypt())
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
