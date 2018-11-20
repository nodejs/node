/*
 * Copyright 2013-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>

#include "../e_os.h"

#include <openssl/opensslconf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/conf.h>

typedef struct {
    const char *pass;
    int passlen;
    const char *salt;
    int saltlen;
    int iter;
} testdata;

static testdata test_cases[] = {
    {"password", 8, "salt", 4, 1},
    {"password", 8, "salt", 4, 2},
    {"password", 8, "salt", 4, 4096},
    {"passwordPASSWORDpassword", 24,
     "saltSALTsaltSALTsaltSALTsaltSALTsalt", 36, 4096},
    {"pass\0word", 9, "sa\0lt", 5, 4096},
    {NULL},
};

static const char *sha1_results[] = {
    "0c60c80f961f0e71f3a9b524af6012062fe037a6",
    "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957",
    "4b007901b765489abead49d926f721d065a429c1",
    "3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038",
    "56fa6aa75548099dcc37d7f03425e0c3",
};

static const char *sha256_results[] = {
    "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b",
    "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43",
    "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a",
    "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c63551"
        "8c7dac47e9",
    "89b69d0516f829893c696226650a8687",
};

static const char *sha512_results[] = {
    "867f70cf1ade02cff3752599a3a53dc4af34c7a669815ae5d513554e1c8cf252c02d47"
        "0a285a0501bad999bfe943c08f050235d7d68b1da55e63f73b60a57fce",
    "e1d9c16aa681708a45f5c7c4e215ceb66e011a2e9f0040713f18aefdb866d53cf76cab"
        "2868a39b9f7840edce4fef5a82be67335c77a6068e04112754f27ccf4e",
    "d197b1b33db0143e018b12f3d1d1479e6cdebdcc97c5c0f87f6902e072f457b5143f30"
        "602641b3d55cd335988cb36b84376060ecd532e039b742a239434af2d5",
    "8c0511f4c6e597c6ac6315d8f0362e225f3c501495ba23b868c005174dc4ee71115b59"
        "f9e60cd9532fa33e0f75aefe30225c583a186cd82bd4daea9724a3d3b8",
    "9d9e9c4cd21fe4be24d5b8244c759665",
};

static void hexdump(FILE *f, const char *title, const unsigned char *s, int l)
{
    int i;
    fprintf(f, "%s", title);
    for (i = 0; i < l; i++) {
        fprintf(f, "%02x", s[i]);
    }
    fprintf(f, "\n");
}

static void convert(unsigned char *dst, const unsigned char *src, int len)
{
    int i;
    for (i = 0; i < len; i++, dst++, src += 2) {
        unsigned int n;
        sscanf((char *)src, "%2x", &n);
        *dst = (unsigned char)n;
    }
    *dst = 0;
}

static void
test_p5_pbkdf2(int i, char *digestname, testdata *test, const char *hex)
{
    const EVP_MD *digest;
    unsigned char *out;
    unsigned char *expected;
    int keylen, r;

    digest = EVP_get_digestbyname(digestname);
    if (digest == NULL) {
        fprintf(stderr, "unknown digest %s\n", digestname);
        EXIT(5);
    }

    if ((strlen(hex) % 2) != 0) {
        fprintf(stderr, "odd hex digest %s %i\n", digestname, i);
        EXIT(5);
    }
    keylen = strlen(hex) / 2;
    expected = OPENSSL_malloc(keylen + 1);
    out = OPENSSL_malloc(keylen + 1);
    if ((expected == NULL) || (out == NULL)) {
        fprintf(stderr, "malloc() failed\n");
        EXIT(5);
    }
    convert(expected, (const unsigned char *)hex, keylen);

    r = PKCS5_PBKDF2_HMAC(test->pass, test->passlen,
                          (const unsigned char *)test->salt, test->saltlen,
                          test->iter, digest, keylen, out);

    if (r == 0) {
        fprintf(stderr, "PKCS5_PBKDF2_HMAC(%s) failure test %i\n",
                digestname, i);
        EXIT(3);
    }
    if (memcmp(expected, out, keylen) != 0) {
        fprintf(stderr, "Wrong result for PKCS5_PBKDF2_HMAC(%s) test %i\n",
                digestname, i);
        hexdump(stderr, "expected: ", expected, keylen);
        hexdump(stderr, "result:   ", out, keylen);
        EXIT(2);
    }
    OPENSSL_free(expected);
    OPENSSL_free(out);
}

int main(int argc, char **argv)
{
    int i;
    testdata *test = test_cases;

    CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    OPENSSL_init_crypto(OPENSSL_INIT_ENGINE_ALL_BUILTIN, NULL);

    printf("PKCS5_PBKDF2_HMAC() tests ");
    for (i = 0; test->pass != NULL; i++, test++) {
        test_p5_pbkdf2(i, "sha1", test, sha1_results[i]);
        test_p5_pbkdf2(i, "sha256", test, sha256_results[i]);
        test_p5_pbkdf2(i, "sha512", test, sha512_results[i]);
        printf(".");
    }
    printf(" done\n");

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks_fp(stderr) <= 0)
        return 1;
# endif
    return 0;
}
