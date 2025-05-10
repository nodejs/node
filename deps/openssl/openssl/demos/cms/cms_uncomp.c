/*
 * Copyright 2008-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Simple S/MIME uncompression example */
#include <openssl/pem.h>
#include <openssl/cms.h>
#include <openssl/err.h>

int main(int argc, char **argv)
{
    BIO *in = NULL, *out = NULL;
    CMS_ContentInfo *cms = NULL;
    int ret = EXIT_FAILURE;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    /* Open compressed content */

    in = BIO_new_file("smcomp.txt", "r");

    if (!in)
        goto err;

    /* Sign content */
    cms = SMIME_read_CMS(in, NULL);

    if (!cms)
        goto err;

    out = BIO_new_file("smuncomp.txt", "w");
    if (!out)
        goto err;

    /* Uncompress S/MIME message */
    if (!CMS_uncompress(cms, out, NULL, 0))
        goto err;

    ret = EXIT_SUCCESS;
 err:
    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "Error Uncompressing Data\n");
        ERR_print_errors_fp(stderr);
    }

    CMS_ContentInfo_free(cms);
    BIO_free(in);
    BIO_free(out);
    return ret;
}
