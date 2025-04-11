/*
 * Copyright 2008-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* Simple S/MIME verification example */
#include <openssl/pem.h>
#include <openssl/cms.h>
#include <openssl/err.h>

/*
 * print any signingTime attributes.
 * signingTime is when each party purportedly signed the message.
 */
static void print_signingTime(CMS_ContentInfo *cms)
{
    STACK_OF(CMS_SignerInfo) *sis;
    CMS_SignerInfo *si;
    X509_ATTRIBUTE *attr;
    ASN1_TYPE *t;
    ASN1_UTCTIME *utctime;
    ASN1_GENERALIZEDTIME *gtime;
    BIO *b;
    int i, loc;

    b = BIO_new_fp(stdout, BIO_NOCLOSE | BIO_FP_TEXT);
    sis = CMS_get0_SignerInfos(cms);
    for (i = 0; i < sk_CMS_SignerInfo_num(sis); i++) {
        si = sk_CMS_SignerInfo_value(sis, i);
        loc = CMS_signed_get_attr_by_NID(si, NID_pkcs9_signingTime, -1);
        attr = CMS_signed_get_attr(si, loc);
        t = X509_ATTRIBUTE_get0_type(attr, 0);
        if (t == NULL)
            continue;
        switch (t->type) {
        case V_ASN1_UTCTIME:
            utctime = t->value.utctime;
            ASN1_UTCTIME_print(b, utctime);
            break;
        case V_ASN1_GENERALIZEDTIME:
            gtime = t->value.generalizedtime;
            ASN1_GENERALIZEDTIME_print(b, gtime);
            break;
        default:
            fprintf(stderr, "unrecognized signingTime type\n");
            break;
        }
        BIO_printf(b, ": signingTime from SignerInfo %i\n", i);
    }
    BIO_free(b);
    return;
}

int main(int argc, char **argv)
{
    BIO *in = NULL, *out = NULL, *tbio = NULL, *cont = NULL;
    X509_STORE *st = NULL;
    X509 *cacert = NULL;
    CMS_ContentInfo *cms = NULL;
    int ret = EXIT_FAILURE;

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    /* Set up trusted CA certificate store */

    st = X509_STORE_new();
    if (st == NULL)
        goto err;

    /* Read in CA certificate */
    tbio = BIO_new_file("cacert.pem", "r");

    if (tbio == NULL)
        goto err;

    cacert = PEM_read_bio_X509(tbio, NULL, 0, NULL);

    if (cacert == NULL)
        goto err;

    if (!X509_STORE_add_cert(st, cacert))
        goto err;

    /* Open message being verified */

    in = BIO_new_file("smout.txt", "r");

    if (in == NULL)
        goto err;

    /* parse message */
    cms = SMIME_read_CMS(in, &cont);

    if (cms == NULL)
        goto err;

    print_signingTime(cms);

    /* File to output verified content to */
    out = BIO_new_file("smver.txt", "w");
    if (out == NULL)
        goto err;

    if (!CMS_verify(cms, NULL, st, cont, out, 0)) {
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
    CMS_ContentInfo_free(cms);
    X509_free(cacert);
    BIO_free(in);
    BIO_free(out);
    BIO_free(tbio);
    return ret;
}
