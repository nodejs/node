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

static char *find_friendly_name(PKCS12 *p12)
{
    STACK_OF(PKCS7) *safes = PKCS12_unpack_authsafes(p12);
    int n, m;
    char *name = NULL;
    PKCS7 *safe;
    STACK_OF(PKCS12_SAFEBAG) *bags;
    PKCS12_SAFEBAG *bag;

    if ((safes = PKCS12_unpack_authsafes(p12)) == NULL)
        return NULL;

    for (n = 0; n < sk_PKCS7_num(safes) && name == NULL; n++) {
        safe = sk_PKCS7_value(safes, n);
        if (OBJ_obj2nid(safe->type) != NID_pkcs7_data
                || (bags = PKCS12_unpack_p7data(safe)) == NULL)
            continue;

        for (m = 0; m < sk_PKCS12_SAFEBAG_num(bags) && name == NULL; m++) {
            bag = sk_PKCS12_SAFEBAG_value(bags, m);
            name = PKCS12_get_friendlyname(bag);
        }
        sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
    }

    sk_PKCS7_pop_free(safes, PKCS7_free);

    return name;
}

int main(int argc, char **argv)
{
    FILE *fp;
    EVP_PKEY *pkey;
    X509 *cert;
    STACK_OF(X509) *ca = NULL;
    PKCS12 *p12;
    const char *name;
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
    name = find_friendly_name(p12);
    PKCS12_free(p12);
    if ((fp = fopen(argv[3], "w")) == NULL) {
        fprintf(stderr, "Error opening file %s\n", argv[1]);
        exit(1);
    }
    if (name)
        fprintf(fp, "***Friendly Name***\n%s\n", name);
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
