/*
 * Copyright 2008-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Simple S/MIME compress example */
#include <openssl/pem.h>
#include <openssl/cms.h>
#include <openssl/err.h>

int main(int argc, char **argv)
{
    BIO *in = NULL, *out = NULL;
    CMS_ContentInfo *cms = NULL;
    int ret = EXIT_FAILURE;

    /*
     * On OpenSSL 1.0.0+ only:
     * for streaming set CMS_STREAM
     */
    int flags = CMS_STREAM;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    /* Open content being compressed */

    in = BIO_new_file("comp.txt", "r");

    if (!in)
        goto err;

    /* compress content */
    cms = CMS_compress(in, NID_zlib_compression, flags);

    if (!cms)
        goto err;

    out = BIO_new_file("smcomp.txt", "w");
    if (!out)
        goto err;

    /* Write out S/MIME message */
    if (!SMIME_write_CMS(out, cms, in, flags))
        goto err;

    ret = EXIT_SUCCESS;

 err:

    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "Error Compressing Data\n");
        ERR_print_errors_fp(stderr);
    }

    CMS_ContentInfo_free(cms);
    BIO_free(in);
    BIO_free(out);
    return ret;
}
