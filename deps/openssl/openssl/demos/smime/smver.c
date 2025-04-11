/*
 * Copyright 2007-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Simple S/MIME verification example */
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/err.h>

int main(int argc, char **argv)
{
    BIO *in = NULL, *out = NULL, *tbio = NULL, *cont = NULL;
    X509_STORE *st = NULL;
    X509 *cacert = NULL;
    PKCS7 *p7 = NULL;
    int ret = EXIT_FAILURE;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    /* Set up trusted CA certificate store */

    st = X509_STORE_new();
    if (st == NULL)
        goto err;

    /* Read in signer certificate and private key */
    tbio = BIO_new_file("cacert.pem", "r");

    if (tbio == NULL)
        goto err;

    cacert = PEM_read_bio_X509(tbio, NULL, 0, NULL);

    if (cacert == NULL)
        goto err;

    if (!X509_STORE_add_cert(st, cacert))
        goto err;

    /* Open content being signed */

    in = BIO_new_file("smout.txt", "r");

    if (in == NULL)
        goto err;

    /* Sign content */
    p7 = SMIME_read_PKCS7(in, &cont);

    if (p7 == NULL)
        goto err;

    /* File to output verified content to */
    out = BIO_new_file("smver.txt", "w");
    if (out == NULL)
        goto err;

    if (!PKCS7_verify(p7, NULL, st, cont, out, 0)) {
        fprintf(stderr, "Verification Failure\n");
        goto err;
    }

    printf("Verification Successful\n");

    ret = EXIT_SUCCESS;
 err:
    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "Error Verifying Data\n");
        ERR_print_errors_fp(stderr);
    }

    X509_STORE_free(st);
    PKCS7_free(p7);
    X509_free(cacert);
    BIO_free(in);
    BIO_free(out);
    BIO_free(tbio);
    return ret;
}
