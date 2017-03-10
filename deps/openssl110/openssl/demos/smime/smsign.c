/*
 * Copyright 2007-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
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
    X509 *scert = NULL;
    EVP_PKEY *skey = NULL;
    PKCS7 *p7 = NULL;
    int ret = 1;

    /*
     * For simple S/MIME signing use PKCS7_DETACHED. On OpenSSL 0.9.9 only:
     * for streaming detached set PKCS7_DETACHED|PKCS7_STREAM for streaming
     * non-detached set PKCS7_STREAM
     */
    int flags = PKCS7_DETACHED | PKCS7_STREAM;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    /* Read in signer certificate and private key */
    tbio = BIO_new_file("signer.pem", "r");

    if (!tbio)
        goto err;

    scert = PEM_read_bio_X509(tbio, NULL, 0, NULL);

    BIO_reset(tbio);

    skey = PEM_read_bio_PrivateKey(tbio, NULL, 0, NULL);

    if (!scert || !skey)
        goto err;

    /* Open content being signed */

    in = BIO_new_file("sign.txt", "r");

    if (!in)
        goto err;

    /* Sign content */
    p7 = PKCS7_sign(scert, skey, NULL, in, flags);

    if (!p7)
        goto err;

    out = BIO_new_file("smout.txt", "w");
    if (!out)
        goto err;

    if (!(flags & PKCS7_STREAM))
        BIO_reset(in);

    /* Write out S/MIME message */
    if (!SMIME_write_PKCS7(out, p7, in, flags))
        goto err;

    ret = 0;

 err:
    if (ret) {
        fprintf(stderr, "Error Signing Data\n");
        ERR_print_errors_fp(stderr);
    }
    PKCS7_free(p7);
    X509_free(scert);
    EVP_PKEY_free(skey);
    BIO_free(in);
    BIO_free(out);
    BIO_free(tbio);

    return ret;

}
