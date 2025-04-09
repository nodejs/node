/*
 * Copyright 2007-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* S/MIME signing example: 2 signers */
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/err.h>

int main(int argc, char **argv)
{
    BIO *in = NULL, *out = NULL, *tbio = NULL;
    X509 *scert = NULL, *scert2 = NULL;
    EVP_PKEY *skey = NULL, *skey2 = NULL;
    PKCS7 *p7 = NULL;
    int ret = EXIT_FAILURE;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    tbio = BIO_new_file("signer.pem", "r");

    if (!tbio)
        goto err;

    scert = PEM_read_bio_X509(tbio, NULL, 0, NULL);

    if (BIO_reset(tbio) < 0)
        goto err;

    skey = PEM_read_bio_PrivateKey(tbio, NULL, 0, NULL);

    BIO_free(tbio);

    tbio = BIO_new_file("signer2.pem", "r");

    if (!tbio)
        goto err;

    scert2 = PEM_read_bio_X509(tbio, NULL, 0, NULL);

    if (BIO_reset(tbio) < 0)
        goto err;

    skey2 = PEM_read_bio_PrivateKey(tbio, NULL, 0, NULL);

    if (!scert2 || !skey2)
        goto err;

    in = BIO_new_file("sign.txt", "r");

    if (!in)
        goto err;

    p7 = PKCS7_sign(NULL, NULL, NULL, in, PKCS7_STREAM | PKCS7_PARTIAL);

    if (!p7)
        goto err;

    /* Add each signer in turn */

    if (!PKCS7_sign_add_signer(p7, scert, skey, NULL, 0))
        goto err;

    if (!PKCS7_sign_add_signer(p7, scert2, skey2, NULL, 0))
        goto err;

    out = BIO_new_file("smout.txt", "w");
    if (!out)
        goto err;

    /* NB: content included and finalized by SMIME_write_PKCS7 */

    if (!SMIME_write_PKCS7(out, p7, in, PKCS7_STREAM))
        goto err;

    printf("Success\n");

    ret = EXIT_SUCCESS;
 err:
    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "Error Signing Data\n");
        ERR_print_errors_fp(stderr);
    }
    PKCS7_free(p7);
    X509_free(scert);
    EVP_PKEY_free(skey);
    X509_free(scert2);
    EVP_PKEY_free(skey2);
    BIO_free(in);
    BIO_free(out);
    BIO_free(tbio);
    return ret;
}
