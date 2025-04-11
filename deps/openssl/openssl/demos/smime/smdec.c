/*
 * Copyright 2007-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Simple S/MIME signing example */
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/err.h>

int main(int argc, char **argv)
{
    BIO *in = NULL, *out = NULL, *tbio = NULL;
    X509 *rcert = NULL;
    EVP_PKEY *rkey = NULL;
    PKCS7 *p7 = NULL;
    int ret = EXIT_FAILURE;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    /* Read in recipient certificate and private key */
    tbio = BIO_new_file("signer.pem", "r");

    if (!tbio)
        goto err;

    rcert = PEM_read_bio_X509(tbio, NULL, 0, NULL);

    if (BIO_reset(tbio) < 0)
        goto err;

    rkey = PEM_read_bio_PrivateKey(tbio, NULL, 0, NULL);

    if (!rcert || !rkey)
        goto err;

    /* Open content being signed */

    in = BIO_new_file("smencr.txt", "r");

    if (!in)
        goto err;

    /* Sign content */
    p7 = SMIME_read_PKCS7(in, NULL);

    if (!p7)
        goto err;

    out = BIO_new_file("encrout.txt", "w");
    if (!out)
        goto err;

    /* Decrypt S/MIME message */
    if (!PKCS7_decrypt(p7, rkey, rcert, out, 0))
        goto err;

    printf("Success\n");

    ret = EXIT_SUCCESS;
 err:
    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "Error Signing Data\n");
        ERR_print_errors_fp(stderr);
    }
    PKCS7_free(p7);
    X509_free(rcert);
    EVP_PKEY_free(rkey);
    BIO_free(in);
    BIO_free(out);
    BIO_free(tbio);

    return ret;
}
