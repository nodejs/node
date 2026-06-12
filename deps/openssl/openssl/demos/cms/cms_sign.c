/*
 * Copyright 2008-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Simple S/MIME signing example */
#include <openssl/pem.h>
#include <openssl/cms.h>
#include <openssl/err.h>

int main(int argc, char **argv)
{
    BIO *in = NULL, *out = NULL, *tbio = NULL;
    X509 *scert = NULL;
    EVP_PKEY *skey = NULL;
    CMS_ContentInfo *cms = NULL;
    int ret = EXIT_FAILURE;

    /*
     * For simple S/MIME signing use CMS_DETACHED. On OpenSSL 1.0.0 only: for
     * streaming detached set CMS_DETACHED|CMS_STREAM for streaming
     * non-detached set CMS_STREAM
     */
    int flags = CMS_DETACHED | CMS_STREAM;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    /* Read in signer certificate and private key */
    tbio = BIO_new_file("signer.pem", "r");

    if (!tbio)
        goto err;

    scert = PEM_read_bio_X509(tbio, NULL, 0, NULL);

    if (BIO_reset(tbio) < 0)
        goto err;

    skey = PEM_read_bio_PrivateKey(tbio, NULL, 0, NULL);

    if (!scert || !skey)
        goto err;

    /* Open content being signed */

    in = BIO_new_file("sign.txt", "r");

    if (!in)
        goto err;

    /* Sign content */
    cms = CMS_sign(scert, skey, NULL, in, flags);

    if (!cms)
        goto err;

    out = BIO_new_file("smout.txt", "w");
    if (!out)
        goto err;

    if (!(flags & CMS_STREAM)) {
        if (BIO_reset(in) < 0)
            goto err;
    }

    /* Write out S/MIME message */
    if (!SMIME_write_CMS(out, cms, in, flags))
        goto err;

    ret = EXIT_SUCCESS;
 err:
    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "Error Signing Data\n");
        ERR_print_errors_fp(stderr);
    }

    CMS_ContentInfo_free(cms);
    X509_free(scert);
    EVP_PKEY_free(skey);
    BIO_free(in);
    BIO_free(out);
    BIO_free(tbio);
    return ret;
}
