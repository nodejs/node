/*
 * Copyright 2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>

#include <openssl/opensslconf.h>
#include <openssl/crypto.h>
#include <openssl/ocsp.h>
#include <openssl/x509.h>
#include <openssl/asn1.h>
#include <openssl/pem.h>

#include "testutil.h"

static const char *certstr;
static const char *privkeystr;

#ifndef OPENSSL_NO_OCSP
static int get_cert_and_key(X509 **cert_out, EVP_PKEY **key_out)
{
    BIO *certbio, *keybio;
    X509 *cert = NULL;
    EVP_PKEY *key = NULL;

    if ((certbio = BIO_new_file(certstr, "r")) == NULL)
        return 0;
    cert = PEM_read_bio_X509(certbio, NULL, NULL, NULL);
    BIO_free(certbio);
    if ((keybio = BIO_new_file(privkeystr, "r")) == NULL)
        goto end;
    key = PEM_read_bio_PrivateKey(keybio, NULL, NULL, NULL);
    BIO_free(keybio);
    if (cert == NULL || key == NULL)
        goto end;
    *cert_out = cert;
    *key_out = key;
    return 1;
 end:
    X509_free(cert);
    EVP_PKEY_free(key);
    return 0;
}

static OCSP_BASICRESP *make_dummy_resp(void)
{
    const unsigned char namestr[] = "openssl.example.com";
    unsigned char keybytes[128] = {7};
    OCSP_BASICRESP *bs = OCSP_BASICRESP_new();
    OCSP_BASICRESP *bs_out = NULL;
    OCSP_CERTID *cid = NULL;
    ASN1_TIME *thisupd = ASN1_TIME_set(NULL, time(NULL));
    ASN1_TIME *nextupd = ASN1_TIME_set(NULL, time(NULL) + 200);
    X509_NAME *name = X509_NAME_new();
    ASN1_BIT_STRING *key = ASN1_BIT_STRING_new();
    ASN1_INTEGER *serial = ASN1_INTEGER_new();

    if (!X509_NAME_add_entry_by_NID(name, NID_commonName, MBSTRING_ASC,
                                   namestr, -1, -1, 1)
        || !ASN1_BIT_STRING_set(key, keybytes, sizeof(keybytes))
        || !ASN1_INTEGER_set_uint64(serial, (uint64_t)1))
        goto err;
    cid = OCSP_cert_id_new(EVP_sha256(), name, key, serial);
    if (bs == NULL
        || thisupd == NULL
        || nextupd == NULL
        || cid == NULL
        || !OCSP_basic_add1_status(bs, cid,
                                   V_OCSP_CERTSTATUS_UNKNOWN,
                                   0, NULL, thisupd, nextupd))
        goto err;
    bs_out = bs;
    bs = NULL;
 err:
    ASN1_TIME_free(thisupd);
    ASN1_TIME_free(nextupd);
    ASN1_BIT_STRING_free(key);
    ASN1_INTEGER_free(serial);
    OCSP_CERTID_free(cid);
    OCSP_BASICRESP_free(bs);
    X509_NAME_free(name);
    return bs_out;
}

static int test_resp_signer(void)
{
    OCSP_BASICRESP *bs = NULL;
    X509 *signer = NULL, *tmp;
    EVP_PKEY *key = NULL;
    STACK_OF(X509) *extra_certs = NULL;
    int ret = 0;

    /*
     * Test a response with no certs at all; get the signer from the
     * extra certs given to OCSP_resp_get0_signer().
     */
    bs = make_dummy_resp();
    extra_certs = sk_X509_new_null();
    if (bs == NULL
        || extra_certs == NULL
        || !get_cert_and_key(&signer, &key)
        || !sk_X509_push(extra_certs, signer)
        || !OCSP_basic_sign(bs, signer, key, EVP_sha1(),
                                      NULL, OCSP_NOCERTS))
        goto err;
    if (!OCSP_resp_get0_signer(bs, &tmp, extra_certs)
        || X509_cmp(tmp, signer) != 0)
        goto err;
    OCSP_BASICRESP_free(bs);

    /* Do it again but include the signer cert */
    bs = make_dummy_resp();
    tmp = NULL;
    if (bs == NULL
        || !OCSP_basic_sign(bs, signer, key, EVP_sha1(),
                            NULL, 0))
        goto err;
    if (!OCSP_resp_get0_signer(bs, &tmp, NULL)
        || X509_cmp(tmp, signer) != 0)
        goto err;
    ret = 1;
 err:
    OCSP_BASICRESP_free(bs);
    sk_X509_free(extra_certs);
    X509_free(signer);
    EVP_PKEY_free(key);
    return ret;
}
#endif

int main(int argc, char *argv[])
{
    int testresult = 1;
    BIO *err = NULL;

    if (argc != 3) {
        printf("Invalid argument count\n");
        return 1;
    }
    if ((certstr = argv[1]) == NULL
        || (privkeystr = argv[2]) == NULL)
        return 1;
    err = BIO_new_fp(stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

#ifndef OPENSSL_NO_OCSP
    ADD_TEST(test_resp_signer);
#endif
    testresult = run_tests(argv[0]);

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks(err) <= 0)
        testresult = 1;
#endif
    BIO_free(err);

    if (!testresult)
        printf("PASS\n");

    return testresult;
}
