/*
 * Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/pkcs12.h>

/* Simple PKCS#12 file reader */

int main(int argc, char **argv)
{
    FILE *fp;
    EVP_PKEY *pkey;
    X509 *cert;
    STACK_OF(X509) *ca = NULL;
    PKCS12 *p12;
    int i;
    if (argc != 4) {
        fprintf(stderr, "Usage: pkread p12file password opfile\n");
        exit(1);
    }
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    if ((fp = fopen(argv[1], "rb")) == NULL) {
        fprintf(stderr, "Error opening file %s\n", argv[1]);
        exit(1);
    }
    p12 = d2i_PKCS12_fp(fp, NULL);
    fclose(fp);
    if (!p12) {
        fprintf(stderr, "Error reading PKCS#12 file\n");
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    if (!PKCS12_parse(p12, argv[2], &pkey, &cert, &ca)) {
        fprintf(stderr, "Error parsing PKCS#12 file\n");
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    PKCS12_free(p12);
    if ((fp = fopen(argv[3], "w")) == NULL) {
        fprintf(stderr, "Error opening file %s\n", argv[1]);
        exit(1);
    }
    if (pkey) {
        fprintf(fp, "***Private Key***\n");
        PEM_write_PrivateKey(fp, pkey, NULL, NULL, 0, NULL, NULL);
    }
    if (cert) {
        fprintf(fp, "***User Certificate***\n");
        PEM_write_X509_AUX(fp, cert);
    }
    if (ca && sk_X509_num(ca)) {
        fprintf(fp, "***Other Certificates***\n");
        for (i = 0; i < sk_X509_num(ca); i++)
            PEM_write_X509_AUX(fp, sk_X509_value(ca, i));
    }
    fclose(fp);
    return 0;
}
