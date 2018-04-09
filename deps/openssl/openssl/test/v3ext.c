/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/err.h>

int main(int ac, char **av)
{
    X509 *x = NULL;
    BIO *b = NULL;
    long pathlen;
    int ret = 1;

    if (ac != 2) {
        fprintf(stderr, "Usage error\n");
        goto end;
    }
    b = BIO_new_file(av[1], "r");
    if (b == NULL)
        goto end;
    x = PEM_read_bio_X509(b, NULL, NULL, NULL);
    if (x == NULL)
        goto end;
    pathlen = X509_get_pathlen(x);
    if (pathlen == 6)
        ret = 0;

end:
    ERR_print_errors_fp(stderr);
    BIO_free(b);
    X509_free(x);
    return ret;
}
