/*
 * Copyright 2008-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* S/MIME signing example: 2 signers */
#include <openssl/pem.h>
#include <openssl/cms.h>
#include <openssl/err.h>

int main(int argc, char **argv)
{
    BIO *in = NULL, *out = NULL, *tbio = NULL;
    X509 *scert = NULL, *scert2 = NULL;
    EVP_PKEY *skey = NULL, *skey2 = NULL;
    CMS_ContentInfo *cms = NULL;
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

    cms = CMS_sign(NULL, NULL, NULL, in, CMS_STREAM | CMS_PARTIAL);

    if (!cms)
        goto err;

    /* Add each signer in turn */

    if (!CMS_add1_signer(cms, scert, skey, NULL, 0))
        goto err;

    if (!CMS_add1_signer(cms, scert2, skey2, NULL, 0))
        goto err;

    out = BIO_new_file("smout.txt", "w");
    if (!out)
        goto err;

    /* NB: content included and finalized by SMIME_write_CMS */

    if (!SMIME_write_CMS(out, cms, in, CMS_STREAM))
        goto err;

    printf("Signing Successful\n");

    ret = EXIT_SUCCESS;
 err:
    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "Error Signing Data\n");
        ERR_print_errors_fp(stderr);
    }

    CMS_ContentInfo_free(cms);
    X509_free(scert);
    EVP_PKEY_free(skey);
    X509_free(scert2);
    EVP_PKEY_free(skey2);
    BIO_free(in);
    BIO_free(out);
    BIO_free(tbio);
    return ret;
}
