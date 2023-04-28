/*
 * Copyright 2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Simple aes wrap encryption demonstration program.
 */

#include <stdio.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/core_names.h>

/* aes key */
static const unsigned char wrap_key[] = {
    0xee, 0xbc, 0x1f, 0x57, 0x48, 0x7f, 0x51, 0x92, 0x1c, 0x04, 0x65, 0x66,
    0x5f, 0x8a, 0xe6, 0xd1, 0x65, 0x8b, 0xb2, 0x6d, 0xe6, 0xf8, 0xa0, 0x69,
    0xa3, 0x52, 0x02, 0x93, 0xa5, 0x72, 0x07, 0x8f
};

/* Unique initialisation vector */
static const unsigned char wrap_iv[] = {
    0x99, 0xaa, 0x3e, 0x68, 0xed, 0x81, 0x73, 0xa0, 0xee, 0xd0, 0x66, 0x84,
    0x99, 0xaa, 0x3e, 0x68,
};

/* Example plaintext to encrypt */
static const unsigned char wrap_pt[] = {
    0xad, 0x4f, 0xc9, 0xfc, 0x77, 0x69, 0xc9, 0xea, 0xfc, 0xdf, 0x00, 0xac,
    0x34, 0xec, 0x40, 0xbc, 0x28, 0x3f, 0xa4, 0x5e, 0xd8, 0x99, 0xe4, 0x5d,
    0x5e, 0x7a, 0xc4, 0xe6, 0xca, 0x7b, 0xa5, 0xb7,
};

/* Expected ciphertext value */
static const unsigned char wrap_ct[] = {
    0x97, 0x99, 0x55, 0xca, 0xf6, 0x3e, 0x95, 0x54, 0x39, 0xd6, 0xaf, 0x63, 0xff, 0x2c, 0xe3, 0x96,
    0xf7, 0x0d, 0x2c, 0x9c, 0xc7, 0x43, 0xc0, 0xb6, 0x31, 0x43, 0xb9, 0x20, 0xac, 0x6b, 0xd3, 0x67,
    0xad, 0x01, 0xaf, 0xa7, 0x32, 0x74, 0x26, 0x92,
};

/*
 * A library context and property query can be used to select & filter
 * algorithm implementations. If they are NULL then the default library
 * context and properties are used.
 */
OSSL_LIB_CTX *libctx = NULL;
const char *propq = NULL;

int aes_wrap_encrypt(void)
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx;
    EVP_CIPHER *cipher = NULL;
    int outlen, tmplen;
    unsigned char outbuf[1024];

    printf("aes wrap Encrypt:\n");
    printf("Plaintext:\n");
    BIO_dump_fp(stdout, wrap_pt, sizeof(wrap_pt));

    /* Create a context for the encrypt operation */
    if ((ctx = EVP_CIPHER_CTX_new()) == NULL)
        goto err;

    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

    /* Fetch the cipher implementation */
    if ((cipher = EVP_CIPHER_fetch(libctx, "AES-256-WRAP", propq)) == NULL)
        goto err;

    /*
     * Initialise an encrypt operation with the cipher/mode, key and IV.
     * We are not setting any custom params so let params be just NULL.
     */
    if (!EVP_EncryptInit_ex2(ctx, cipher, wrap_key, wrap_iv, /* params */ NULL))
        goto err;

    /* Encrypt plaintext */
    if (!EVP_EncryptUpdate(ctx, outbuf, &outlen, wrap_pt, sizeof(wrap_pt)))
        goto err;

    /* Finalise: there can be some additional output from padding */
    if (!EVP_EncryptFinal_ex(ctx, outbuf + outlen, &tmplen))
        goto err;
    outlen += tmplen;

    /* Output encrypted block */
    printf("Ciphertext (outlen:%d):\n", outlen);
    BIO_dump_fp(stdout, outbuf, outlen);

    if (sizeof(wrap_ct) == outlen && !CRYPTO_memcmp(outbuf, wrap_ct, outlen))
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

int aes_wrap_decrypt(void)
{
    int ret = 0;
    EVP_CIPHER_CTX *ctx;
    EVP_CIPHER *cipher = NULL;
    int outlen, tmplen;
    unsigned char outbuf[1024];

    printf("aes wrap Decrypt:\n");
    printf("Ciphertext:\n");
    BIO_dump_fp(stdout, wrap_ct, sizeof(wrap_ct));

    if ((ctx = EVP_CIPHER_CTX_new()) == NULL)
        goto err;

    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

    /* Fetch the cipher implementation */
    if ((cipher = EVP_CIPHER_fetch(libctx, "aes-256-wrap", propq)) == NULL)
        goto err;

    /*
     * Initialise an encrypt operation with the cipher/mode, key and IV.
     * We are not setting any custom params so let params be just NULL.
     */
    if (!EVP_DecryptInit_ex2(ctx, cipher, wrap_key, wrap_iv, /* params */ NULL))
        goto err;

    /* Decrypt plaintext */
    if (!EVP_DecryptUpdate(ctx, outbuf, &outlen, wrap_ct, sizeof(wrap_ct)))
        goto err;

    /* Finalise: there can be some additional output from padding */
    if (!EVP_DecryptFinal_ex(ctx, outbuf + outlen, &tmplen))
        goto err;
    outlen += tmplen;

    /* Output decrypted block */
    printf("Plaintext (outlen:%d):\n", outlen);
    BIO_dump_fp(stdout, outbuf, outlen);

    if (sizeof(wrap_pt) == outlen && !CRYPTO_memcmp(outbuf, wrap_pt, outlen))
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
    if (!aes_wrap_encrypt())
       return 1;

    if (!aes_wrap_decrypt())
        return 1;

    return 0;
}

