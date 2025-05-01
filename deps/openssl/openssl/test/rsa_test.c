/*
 * Copyright 1999-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* test vectors from p1ovect1.txt */

/*
 * RSA low level APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include <stdio.h>
#include <string.h>

#include "internal/nelem.h"

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/bn.h>

#include "testutil.h"

#include <openssl/rsa.h>

#define SetKey \
    RSA_set0_key(key,                                           \
                 BN_bin2bn(n, sizeof(n)-1, NULL),               \
                 BN_bin2bn(e, sizeof(e)-1, NULL),               \
                 BN_bin2bn(d, sizeof(d)-1, NULL));              \
    RSA_set0_factors(key,                                       \
                     BN_bin2bn(p, sizeof(p)-1, NULL),           \
                     BN_bin2bn(q, sizeof(q)-1, NULL));          \
    RSA_set0_crt_params(key,                                    \
                        BN_bin2bn(dmp1, sizeof(dmp1)-1, NULL),  \
                        BN_bin2bn(dmq1, sizeof(dmq1)-1, NULL),  \
                        BN_bin2bn(iqmp, sizeof(iqmp)-1, NULL)); \
    if (c != NULL)                                              \
        memcpy(c, ctext_ex, sizeof(ctext_ex) - 1);              \
    return sizeof(ctext_ex) - 1;

static int key1(RSA *key, unsigned char *c)
{
    static unsigned char n[] =
        "\x00\xAA\x36\xAB\xCE\x88\xAC\xFD\xFF\x55\x52\x3C\x7F\xC4\x52\x3F"
        "\x90\xEF\xA0\x0D\xF3\x77\x4A\x25\x9F\x2E\x62\xB4\xC5\xD9\x9C\xB5"
        "\xAD\xB3\x00\xA0\x28\x5E\x53\x01\x93\x0E\x0C\x70\xFB\x68\x76\x93"
        "\x9C\xE6\x16\xCE\x62\x4A\x11\xE0\x08\x6D\x34\x1E\xBC\xAC\xA0\xA1"
        "\xF5";

    static unsigned char e[] = "\x11";

    static unsigned char d[] =
        "\x0A\x03\x37\x48\x62\x64\x87\x69\x5F\x5F\x30\xBC\x38\xB9\x8B\x44"
        "\xC2\xCD\x2D\xFF\x43\x40\x98\xCD\x20\xD8\xA1\x38\xD0\x90\xBF\x64"
        "\x79\x7C\x3F\xA7\xA2\xCD\xCB\x3C\xD1\xE0\xBD\xBA\x26\x54\xB4\xF9"
        "\xDF\x8E\x8A\xE5\x9D\x73\x3D\x9F\x33\xB3\x01\x62\x4A\xFD\x1D\x51";

    static unsigned char p[] =
        "\x00\xD8\x40\xB4\x16\x66\xB4\x2E\x92\xEA\x0D\xA3\xB4\x32\x04\xB5"
        "\xCF\xCE\x33\x52\x52\x4D\x04\x16\xA5\xA4\x41\xE7\x00\xAF\x46\x12"
        "\x0D";

    static unsigned char q[] =
        "\x00\xC9\x7F\xB1\xF0\x27\xF4\x53\xF6\x34\x12\x33\xEA\xAA\xD1\xD9"
        "\x35\x3F\x6C\x42\xD0\x88\x66\xB1\xD0\x5A\x0F\x20\x35\x02\x8B\x9D"
        "\x89";

    static unsigned char dmp1[] =
        "\x59\x0B\x95\x72\xA2\xC2\xA9\xC4\x06\x05\x9D\xC2\xAB\x2F\x1D\xAF"
        "\xEB\x7E\x8B\x4F\x10\xA7\x54\x9E\x8E\xED\xF5\xB4\xFC\xE0\x9E\x05";

    static unsigned char dmq1[] =
        "\x00\x8E\x3C\x05\x21\xFE\x15\xE0\xEA\x06\xA3\x6F\xF0\xF1\x0C\x99"
        "\x52\xC3\x5B\x7A\x75\x14\xFD\x32\x38\xB8\x0A\xAD\x52\x98\x62\x8D"
        "\x51";

    static unsigned char iqmp[] =
        "\x36\x3F\xF7\x18\x9D\xA8\xE9\x0B\x1D\x34\x1F\x71\xD0\x9B\x76\xA8"
        "\xA9\x43\xE1\x1D\x10\xB2\x4D\x24\x9F\x2D\xEA\xFE\xF8\x0C\x18\x26";

    static unsigned char ctext_ex[] =
        "\x1b\x8f\x05\xf9\xca\x1a\x79\x52\x6e\x53\xf3\xcc\x51\x4f\xdb\x89"
        "\x2b\xfb\x91\x93\x23\x1e\x78\xb9\x92\xe6\x8d\x50\xa4\x80\xcb\x52"
        "\x33\x89\x5c\x74\x95\x8d\x5d\x02\xab\x8c\x0f\xd0\x40\xeb\x58\x44"
        "\xb0\x05\xc3\x9e\xd8\x27\x4a\x9d\xbf\xa8\x06\x71\x40\x94\x39\xd2";

    SetKey;
}

static int key2(RSA *key, unsigned char *c)
{
    static unsigned char n[] =
        "\x00\xA3\x07\x9A\x90\xDF\x0D\xFD\x72\xAC\x09\x0C\xCC\x2A\x78\xB8"
        "\x74\x13\x13\x3E\x40\x75\x9C\x98\xFA\xF8\x20\x4F\x35\x8A\x0B\x26"
        "\x3C\x67\x70\xE7\x83\xA9\x3B\x69\x71\xB7\x37\x79\xD2\x71\x7B\xE8"
        "\x34\x77\xCF";

    static unsigned char e[] = "\x3";

    static unsigned char d[] =
        "\x6C\xAF\xBC\x60\x94\xB3\xFE\x4C\x72\xB0\xB3\x32\xC6\xFB\x25\xA2"
        "\xB7\x62\x29\x80\x4E\x68\x65\xFC\xA4\x5A\x74\xDF\x0F\x8F\xB8\x41"
        "\x3B\x52\xC0\xD0\xE5\x3D\x9B\x59\x0F\xF1\x9B\xE7\x9F\x49\xDD\x21"
        "\xE5\xEB";

    static unsigned char p[] =
        "\x00\xCF\x20\x35\x02\x8B\x9D\x86\x98\x40\xB4\x16\x66\xB4\x2E\x92"
        "\xEA\x0D\xA3\xB4\x32\x04\xB5\xCF\xCE\x91";

    static unsigned char q[] =
        "\x00\xC9\x7F\xB1\xF0\x27\xF4\x53\xF6\x34\x12\x33\xEA\xAA\xD1\xD9"
        "\x35\x3F\x6C\x42\xD0\x88\x66\xB1\xD0\x5F";

    static unsigned char dmp1[] =
        "\x00\x8A\x15\x78\xAC\x5D\x13\xAF\x10\x2B\x22\xB9\x99\xCD\x74\x61"
        "\xF1\x5E\x6D\x22\xCC\x03\x23\xDF\xDF\x0B";

    static unsigned char dmq1[] =
        "\x00\x86\x55\x21\x4A\xC5\x4D\x8D\x4E\xCD\x61\x77\xF1\xC7\x36\x90"
        "\xCE\x2A\x48\x2C\x8B\x05\x99\xCB\xE0\x3F";

    static unsigned char iqmp[] =
        "\x00\x83\xEF\xEF\xB8\xA9\xA4\x0D\x1D\xB6\xED\x98\xAD\x84\xED\x13"
        "\x35\xDC\xC1\x08\xF3\x22\xD0\x57\xCF\x8D";

    static unsigned char ctext_ex[] =
        "\x14\xbd\xdd\x28\xc9\x83\x35\x19\x23\x80\xe8\xe5\x49\xb1\x58\x2a"
        "\x8b\x40\xb4\x48\x6d\x03\xa6\xa5\x31\x1f\x1f\xd5\xf0\xa1\x80\xe4"
        "\x17\x53\x03\x29\xa9\x34\x90\x74\xb1\x52\x13\x54\x29\x08\x24\x52"
        "\x62\x51";

    SetKey;
}

static int key3(RSA *key, unsigned char *c)
{
    static unsigned char n[] =
        "\x00\xBB\xF8\x2F\x09\x06\x82\xCE\x9C\x23\x38\xAC\x2B\x9D\xA8\x71"
        "\xF7\x36\x8D\x07\xEE\xD4\x10\x43\xA4\x40\xD6\xB6\xF0\x74\x54\xF5"
        "\x1F\xB8\xDF\xBA\xAF\x03\x5C\x02\xAB\x61\xEA\x48\xCE\xEB\x6F\xCD"
        "\x48\x76\xED\x52\x0D\x60\xE1\xEC\x46\x19\x71\x9D\x8A\x5B\x8B\x80"
        "\x7F\xAF\xB8\xE0\xA3\xDF\xC7\x37\x72\x3E\xE6\xB4\xB7\xD9\x3A\x25"
        "\x84\xEE\x6A\x64\x9D\x06\x09\x53\x74\x88\x34\xB2\x45\x45\x98\x39"
        "\x4E\xE0\xAA\xB1\x2D\x7B\x61\xA5\x1F\x52\x7A\x9A\x41\xF6\xC1\x68"
        "\x7F\xE2\x53\x72\x98\xCA\x2A\x8F\x59\x46\xF8\xE5\xFD\x09\x1D\xBD"
        "\xCB";

    static unsigned char e[] = "\x11";

    static unsigned char d[] =
        "\x00\xA5\xDA\xFC\x53\x41\xFA\xF2\x89\xC4\xB9\x88\xDB\x30\xC1\xCD"
        "\xF8\x3F\x31\x25\x1E\x06\x68\xB4\x27\x84\x81\x38\x01\x57\x96\x41"
        "\xB2\x94\x10\xB3\xC7\x99\x8D\x6B\xC4\x65\x74\x5E\x5C\x39\x26\x69"
        "\xD6\x87\x0D\xA2\xC0\x82\xA9\x39\xE3\x7F\xDC\xB8\x2E\xC9\x3E\xDA"
        "\xC9\x7F\xF3\xAD\x59\x50\xAC\xCF\xBC\x11\x1C\x76\xF1\xA9\x52\x94"
        "\x44\xE5\x6A\xAF\x68\xC5\x6C\x09\x2C\xD3\x8D\xC3\xBE\xF5\xD2\x0A"
        "\x93\x99\x26\xED\x4F\x74\xA1\x3E\xDD\xFB\xE1\xA1\xCE\xCC\x48\x94"
        "\xAF\x94\x28\xC2\xB7\xB8\x88\x3F\xE4\x46\x3A\x4B\xC8\x5B\x1C\xB3"
        "\xC1";

    static unsigned char p[] =
        "\x00\xEE\xCF\xAE\x81\xB1\xB9\xB3\xC9\x08\x81\x0B\x10\xA1\xB5\x60"
        "\x01\x99\xEB\x9F\x44\xAE\xF4\xFD\xA4\x93\xB8\x1A\x9E\x3D\x84\xF6"
        "\x32\x12\x4E\xF0\x23\x6E\x5D\x1E\x3B\x7E\x28\xFA\xE7\xAA\x04\x0A"
        "\x2D\x5B\x25\x21\x76\x45\x9D\x1F\x39\x75\x41\xBA\x2A\x58\xFB\x65"
        "\x99";

    static unsigned char q[] =
        "\x00\xC9\x7F\xB1\xF0\x27\xF4\x53\xF6\x34\x12\x33\xEA\xAA\xD1\xD9"
        "\x35\x3F\x6C\x42\xD0\x88\x66\xB1\xD0\x5A\x0F\x20\x35\x02\x8B\x9D"
        "\x86\x98\x40\xB4\x16\x66\xB4\x2E\x92\xEA\x0D\xA3\xB4\x32\x04\xB5"
        "\xCF\xCE\x33\x52\x52\x4D\x04\x16\xA5\xA4\x41\xE7\x00\xAF\x46\x15"
        "\x03";

    static unsigned char dmp1[] =
        "\x54\x49\x4C\xA6\x3E\xBA\x03\x37\xE4\xE2\x40\x23\xFC\xD6\x9A\x5A"
        "\xEB\x07\xDD\xDC\x01\x83\xA4\xD0\xAC\x9B\x54\xB0\x51\xF2\xB1\x3E"
        "\xD9\x49\x09\x75\xEA\xB7\x74\x14\xFF\x59\xC1\xF7\x69\x2E\x9A\x2E"
        "\x20\x2B\x38\xFC\x91\x0A\x47\x41\x74\xAD\xC9\x3C\x1F\x67\xC9\x81";

    static unsigned char dmq1[] =
        "\x47\x1E\x02\x90\xFF\x0A\xF0\x75\x03\x51\xB7\xF8\x78\x86\x4C\xA9"
        "\x61\xAD\xBD\x3A\x8A\x7E\x99\x1C\x5C\x05\x56\xA9\x4C\x31\x46\xA7"
        "\xF9\x80\x3F\x8F\x6F\x8A\xE3\x42\xE9\x31\xFD\x8A\xE4\x7A\x22\x0D"
        "\x1B\x99\xA4\x95\x84\x98\x07\xFE\x39\xF9\x24\x5A\x98\x36\xDA\x3D";

    static unsigned char iqmp[] =
        "\x00\xB0\x6C\x4F\xDA\xBB\x63\x01\x19\x8D\x26\x5B\xDB\xAE\x94\x23"
        "\xB3\x80\xF2\x71\xF7\x34\x53\x88\x50\x93\x07\x7F\xCD\x39\xE2\x11"
        "\x9F\xC9\x86\x32\x15\x4F\x58\x83\xB1\x67\xA9\x67\xBF\x40\x2B\x4E"
        "\x9E\x2E\x0F\x96\x56\xE6\x98\xEA\x36\x66\xED\xFB\x25\x79\x80\x39"
        "\xF7";

    static unsigned char ctext_ex[] =
        "\xb8\x24\x6b\x56\xa6\xed\x58\x81\xae\xb5\x85\xd9\xa2\x5b\x2a\xd7"
        "\x90\xc4\x17\xe0\x80\x68\x1b\xf1\xac\x2b\xc3\xde\xb6\x9d\x8b\xce"
        "\xf0\xc4\x36\x6f\xec\x40\x0a\xf0\x52\xa7\x2e\x9b\x0e\xff\xb5\xb3"
        "\xf2\xf1\x92\xdb\xea\xca\x03\xc1\x27\x40\x05\x71\x13\xbf\x1f\x06"
        "\x69\xac\x22\xe9\xf3\xa7\x85\x2e\x3c\x15\xd9\x13\xca\xb0\xb8\x86"
        "\x3a\x95\xc9\x92\x94\xce\x86\x74\x21\x49\x54\x61\x03\x46\xf4\xd4"
        "\x74\xb2\x6f\x7c\x48\xb4\x2e\xe6\x8e\x1f\x57\x2a\x1f\xc4\x02\x6a"
        "\xc4\x56\xb4\xf5\x9f\x7b\x62\x1e\xa1\xb9\xd8\x8f\x64\x20\x2f\xb1";

    SetKey;
}

static int rsa_setkey(RSA** key, unsigned char *ctext, int idx)
{
    int clen = 0;

    *key = RSA_new();
    if (*key != NULL)
        switch (idx) {
        case 0:
            clen = key1(*key, ctext);
            break;
        case 1:
            clen = key2(*key, ctext);
            break;
        case 2:
            clen = key3(*key, ctext);
            break;
        }
    return clen;
}

static int test_rsa_simple(int idx, int en_pad_type, int de_pad_type,
                           int success, unsigned char *ctext_ex, int *clen,
                           RSA **retkey)
{
    int ret = 0;
    RSA *key;
    unsigned char ptext[256];
    unsigned char ctext[256];
    static unsigned char ptext_ex[] = "\x54\x85\x9b\x34\x2c\x49\xea\x2a";
    int plen;
    int clentmp = 0;
    int num;

    plen = sizeof(ptext_ex) - 1;
    clentmp = rsa_setkey(&key, ctext_ex, idx);
    if (clen != NULL)
        *clen = clentmp;

    num = RSA_public_encrypt(plen, ptext_ex, ctext, key, en_pad_type);
    if (!TEST_int_eq(num, clentmp))
        goto err;

    num = RSA_private_decrypt(num, ctext, ptext, key, de_pad_type);
    if (success) {
        if (!TEST_int_gt(num, 0) || !TEST_mem_eq(ptext, num, ptext_ex, plen))
            goto err;
    } else {
        if (!TEST_int_lt(num, 0))
            goto err;
    }

    ret = 1;
    if (retkey != NULL) {
        *retkey = key;
        key = NULL;
    }
err:
    RSA_free(key);
    return ret;
}

static int test_rsa_pkcs1(int idx)
{
    return test_rsa_simple(idx, RSA_PKCS1_PADDING, RSA_PKCS1_PADDING, 1, NULL,
                           NULL, NULL);
}

static int test_rsa_oaep(int idx)
{
    int ret = 0;
    RSA *key = NULL;
    unsigned char ptext[256];
    static unsigned char ptext_ex[] = "\x54\x85\x9b\x34\x2c\x49\xea\x2a";
    unsigned char ctext_ex[256];
    int plen;
    int clen = 0;
    int num;
    int n;

    if (!test_rsa_simple(idx, RSA_PKCS1_OAEP_PADDING, RSA_PKCS1_OAEP_PADDING, 1,
                         ctext_ex, &clen, &key))
        goto err;

    plen = sizeof(ptext_ex) - 1;

    /* Different ciphertexts. Try decrypting ctext_ex */
    num = RSA_private_decrypt(clen, ctext_ex, ptext, key,
                              RSA_PKCS1_OAEP_PADDING);
    if (num <= 0 || !TEST_mem_eq(ptext, num, ptext_ex, plen))
        goto err;

    /* Try decrypting corrupted ciphertexts. */
    for (n = 0; n < clen; ++n) {
        ctext_ex[n] ^= 1;
        num = RSA_private_decrypt(clen, ctext_ex, ptext, key,
                                      RSA_PKCS1_OAEP_PADDING);
        if (!TEST_int_le(num, 0))
            goto err;
        ctext_ex[n] ^= 1;
    }

    /* Test truncated ciphertexts, as well as negative length. */
    for (n = -1; n < clen; ++n) {
        num = RSA_private_decrypt(n, ctext_ex, ptext, key,
                                  RSA_PKCS1_OAEP_PADDING);
        if (!TEST_int_le(num, 0))
            goto err;
    }

    ret = 1;
err:
    RSA_free(key);
    return ret;
}

static const struct {
    int bits;
    unsigned int r;
} rsa_security_bits_cases[] = {
    /* NIST SP 800-56B rev 2 (draft) Appendix D Table 5 */
    { 2048,     112 },
    { 3072,     128 },
    { 4096,     152 },
    { 6144,     176 },
    { 8192,     200 },
    /* NIST FIPS 140-2 IG 7.5 */
    { 7680,     192 },
    { 15360,    256 },
    /* Older values */
    { 256,      40  },
    { 512,      56  },
    { 1024,     80  },
    /* Some other values */
    { 8888,     208 },
    { 2468,     120 },
    { 13456,    248 },
    /* Edge points */
    { 15359,    256 },
    { 15361,    264 },
    { 7679,     192 },
    { 7681,     200 },
};

static int test_rsa_security_bit(int n)
{
    static const unsigned char vals[8] = {
        0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40
    };
    RSA *key = RSA_new();
    const int bits = rsa_security_bits_cases[n].bits;
    const int result = rsa_security_bits_cases[n].r;
    const int bytes = (bits + 7) / 8;
    int r = 0;
    unsigned char num[2000];

    if (!TEST_ptr(key) || !TEST_int_le(bytes, (int)sizeof(num)))
        goto err;

    /*
     * It is necessary to set the RSA key in order to ask for the strength.
     * A BN of an appropriate size is created, in general it won't have the
     * properties necessary for RSA to function.  This is okay here since
     * the RSA key is never used.
     */
    memset(num, vals[bits % 8], bytes);

    /*
     * The 'e' parameter is set to the same value as 'n'.  This saves having
     * an extra BN to hold a sensible value for 'e'.  This is safe since the
     * RSA key is not used.  The 'd' parameter can be NULL safely.
     */
    if (TEST_true(RSA_set0_key(key, BN_bin2bn(num, bytes, NULL),
                               BN_bin2bn(num, bytes, NULL), NULL))
            && TEST_uint_eq(RSA_security_bits(key), result))
        r = 1;
err:
    RSA_free(key);
    return r;
}

static int test_EVP_rsa_legacy_key(void)
{
    int ret;
    size_t buflen = 384;
    size_t msglen = 64;
    unsigned char sigbuf[384];
    unsigned char msgbuf[64];
    BIGNUM *p;
    BIGNUM *q;
    BIGNUM *n;
    BIGNUM *d;
    BIGNUM *e;
    RSA *rsa;
    const EVP_MD *md;
    EVP_MD_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;

    unsigned char n_data[] = {
    0x00, 0xc7, 0x28, 0x7a, 0x28, 0x91, 0x51, 0xa5, 0xe8, 0x3c, 0x45, 0xcf,
    0x1d, 0xa9, 0x69, 0x7a, 0x0d, 0xdb, 0xdd, 0x8f, 0xe2, 0xde, 0x85, 0xdd,
    0x85, 0x6d, 0x8f, 0x78, 0x20, 0xd6, 0xe, 0xe5, 0x06, 0xcb, 0x9c, 0xd6,
    0xd3, 0xca, 0xef, 0x1d, 0x80, 0xd3, 0x18, 0x23, 0x91, 0x5c, 0xe5, 0xc8,
    0x44, 0x37, 0x56, 0x1b, 0x68, 0x7f, 0x08, 0xa3, 0x1c, 0xf6, 0xe8, 0x11,
    0x38, 0x0f, 0x2e, 0xad, 0xb1, 0x89, 0x8b, 0x08, 0xe8, 0x35, 0xaf, 0x3b,
    0xfe, 0x37, 0x8d, 0x21, 0xd5, 0x3f, 0x1f, 0x4b, 0x01, 0x30, 0xd8, 0xd0,
    0x24, 0xf7, 0xab, 0x57, 0xad, 0xac, 0xbc, 0x53, 0x6d, 0x84, 0x8e, 0xa1,
    0xb2, 0x5b, 0x8e, 0xe7, 0xb3, 0xac, 0xfc, 0x60, 0x22, 0x10, 0x1e, 0x99,
    0xfa, 0xa0, 0x60, 0x00, 0x69, 0x5f, 0x8e, 0xca, 0x6d, 0x9c, 0xee, 0x5e,
    0x84, 0x4e, 0x53, 0x83, 0x42, 0x76, 0x4d, 0xb8, 0xc1, 0xeb, 0x4e, 0x3d,
    0xc3, 0xce, 0xac, 0x79, 0xbb, 0x29, 0x5d, 0x92, 0x33, 0x6e, 0xcf, 0x8f,
    0x5a, 0xf0, 0xb3, 0xb5, 0xdc, 0xd5, 0xa3, 0xaf, 0x40, 0x4b, 0x0f, 0x05,
    0xac, 0x46, 0x53, 0x2d, 0x5f, 0x20, 0x96, 0x42, 0xa8, 0x47, 0x61, 0x54,
    0x05, 0x2c, 0x8a, 0x26, 0x5d, 0x92, 0x1d, 0x01, 0x2a, 0x27, 0x8a, 0xfc,
    0x64, 0x24, 0x5c, 0x34, 0xde, 0x92, 0xc6, 0x82, 0xea, 0x4d, 0xe2, 0x52,
    0xe5, 0xad, 0x62, 0x00, 0xc6, 0xc8, 0xe9, 0x0c, 0x22, 0xf0, 0x9e, 0xbe,
    0xdc, 0x51, 0x58, 0xad, 0x3b, 0xba, 0x2e, 0x45, 0x65, 0xcc, 0x5b, 0x55,
    0x46, 0x67, 0x18, 0x4a, 0x80, 0x67, 0x5b, 0x84, 0x7f, 0x13, 0x37, 0x45,
    0xd8, 0x03, 0xc6, 0x22, 0xc3, 0x4a, 0x46, 0x6b, 0xde, 0x50, 0xbf, 0x16,
    0x0a, 0x23, 0x0b, 0xaa, 0x50, 0x54, 0xf6, 0x20, 0x83, 0x74, 0x33, 0x97,
    0x2e, 0xf2, 0x8e, 0x7e, 0x13 };

    unsigned char e_data[]  = { 0x01, 0x00, 0x01 };

    unsigned char d_data[] = {
    0x09, 0x2d, 0xcb, 0xe7, 0x87, 0xbf, 0x10, 0x1a, 0xf2, 0x80, 0x33, 0x2a,
    0x06, 0x4f, 0x56, 0xb1, 0x41, 0xd3, 0x65, 0xd8, 0xca, 0x71, 0xb8, 0x02,
    0x78, 0xc8, 0xb6, 0x7c, 0x28, 0xf4, 0x6c, 0xe8, 0xd1, 0xc4, 0x92, 0x40,
    0x23, 0xa7, 0xbe, 0x9f, 0xdb, 0xda, 0xce, 0x74, 0xda, 0x27, 0xbb, 0x01,
    0xad, 0xdd, 0x39, 0x99, 0x28, 0xd5, 0xb0, 0x92, 0xda, 0xac, 0x5a, 0x72,
    0xcf, 0x7c, 0x52, 0xc4, 0x0e, 0x77, 0x4a, 0x7b, 0x4d, 0x52, 0x1c, 0xbd,
    0x3c, 0x39, 0x34, 0x78, 0x7c, 0x16, 0xc8, 0xa1, 0xae, 0xeb, 0x27, 0x38,
    0xb4, 0xf3, 0x80, 0x30, 0x80, 0x78, 0x13, 0x8e, 0x46, 0x20, 0x3e, 0xc2,
    0x96, 0x26, 0xb1, 0x76, 0x1e, 0x00, 0x69, 0xbb, 0xd8, 0x2b, 0x58, 0xe4,
    0x6c, 0xb4, 0xd0, 0x00, 0x0b, 0x47, 0xec, 0xfb, 0x7d, 0x52, 0x9d, 0x27,
    0x92, 0xe6, 0x95, 0x73, 0xa0, 0x39, 0x37, 0xcd, 0x1f, 0x60, 0x13, 0x1c,
    0x87, 0x9d, 0xa7, 0x91, 0x90, 0xf9, 0x36, 0xc5, 0xfa, 0x3f, 0xf9, 0x7f,
    0x50, 0xf8, 0xb3, 0x54, 0x65, 0xff, 0x6f, 0xa6, 0x22, 0xcc, 0x4a, 0x1e,
    0x49, 0x3f, 0x07, 0xc6, 0xf2, 0x65, 0x73, 0x13, 0x1b, 0x2d, 0xb6, 0x15,
    0xff, 0xcd, 0x9a, 0x1c, 0xea, 0xef, 0x58, 0x56, 0x91, 0x2d, 0x47, 0x81,
    0x56, 0x0d, 0xc3, 0xb0, 0x47, 0x58, 0x8d, 0x05, 0x7d, 0x5b, 0xc0, 0x22,
    0xa4, 0xf0, 0x2e, 0x70, 0x36, 0x01, 0x89, 0xa1, 0x71, 0xed, 0x76, 0xe9,
    0x8d, 0xf5, 0x49, 0xaf, 0x11, 0xbe, 0xe4, 0xd4, 0x48, 0x92, 0xb6, 0x5b,
    0xc2, 0x04, 0xd4, 0x0c, 0x5c, 0x8b, 0xe3, 0xfa, 0x29, 0x63, 0x86, 0xb4,
    0x10, 0xad, 0x32, 0x07, 0x85, 0xe2, 0x43, 0x76, 0x16, 0x90, 0xab, 0xdf,
    0xb3, 0x36, 0x0a, 0xc4, 0x49, 0x7b, 0x95, 0x48, 0x50, 0x72, 0x8f, 0x7d,
    0xf4, 0xfa, 0x60, 0xc1 };

    unsigned char p_data[] = {
    0x00, 0xed, 0xf7, 0xa7, 0x00, 0x5a, 0xbb, 0xd1, 0x52, 0x65, 0x9b, 0xec,
    0xfe, 0x27, 0x8b, 0xe2, 0xbe, 0x40, 0x8c, 0x2f, 0x6f, 0xb4, 0x26, 0xb2,
    0xbe, 0x45, 0x4b, 0x3b, 0x5a, 0xaa, 0xc6, 0xaa, 0xfa, 0xc1, 0x3a, 0xa9,
    0xa1, 0xba, 0xb7, 0x86, 0x1a, 0x98, 0x15, 0x5f, 0x5c, 0x1c, 0x57, 0x78,
    0x78, 0x6a, 0x13, 0xc2, 0x40, 0x7d, 0x07, 0x87, 0x47, 0xc6, 0x96, 0xd5,
    0x92, 0xc9, 0x65, 0x2c, 0xfe, 0xbb, 0xe0, 0xd6, 0x76, 0x25, 0x5a, 0xa3,
    0xdf, 0x97, 0x4b, 0x64, 0xfd, 0x3b, 0x2b, 0xbc, 0xfb, 0x80, 0xad, 0x3b,
    0x7d, 0x1f, 0x48, 0x56, 0x27, 0xf7, 0x2f, 0x8e, 0x92, 0x07, 0xa8, 0x9f,
    0xbc, 0x5a, 0xce, 0xfa, 0xd5, 0x67, 0xad, 0xf4, 0xbf, 0xe0, 0xc9, 0x3e,
    0x8e, 0xb5, 0x90, 0x58, 0x54, 0x92, 0x9f, 0xda, 0x36, 0xc0, 0x0d, 0x57,
    0xfe, 0x6c, 0x23, 0x63, 0x8b, 0xd1, 0x1e, 0x4f, 0xd3 };

    unsigned char q_data[] = {
    0x00, 0xd6, 0x3f, 0xf5, 0xee, 0xff, 0x4d, 0x7d, 0x8c, 0x1a, 0x85, 0x5d,
    0x3c, 0x4f, 0x9d, 0xdf, 0xc7, 0x68, 0x27, 0x7f, 0xe4, 0x4f, 0x4f, 0xd7,
    0xa2, 0x3b, 0xcd, 0x4a, 0x34, 0xd8, 0x55, 0x4a, 0x3e, 0x8e, 0xb3, 0xa8,
    0xe9, 0x8a, 0xc5, 0x94, 0xd1, 0x09, 0x32, 0x4b, 0x79, 0x8d, 0x7b, 0x03,
    0x0b, 0x5d, 0xca, 0x91, 0x41, 0xbc, 0x82, 0xc3, 0x89, 0x67, 0x4d, 0x03,
    0x68, 0x03, 0x2d, 0x0e, 0x4e, 0x97, 0x6c, 0xf6, 0x3e, 0x1f, 0xf4, 0x50,
    0x06, 0x5d, 0x05, 0x22, 0xf2, 0xf8, 0xf2, 0xde, 0xad, 0x2e, 0x9d, 0xc3,
    0x97, 0x1b, 0xc3, 0x75, 0xe7, 0x86, 0xde, 0xc5, 0x11, 0x89, 0xed, 0x6a,
    0x13, 0x14, 0x23, 0x4b, 0x98, 0x81, 0xf7, 0xd4, 0x1c, 0xee, 0x30, 0x92,
    0x85, 0x20, 0x4f, 0x35, 0x02, 0xfa, 0xda, 0x14, 0x77, 0xfa, 0x08, 0x34,
    0x60, 0xc7, 0x93, 0x72, 0xdc, 0xc4, 0x18, 0x70, 0xc1 };

    memset(msgbuf, 0xef, 64);

    ret = (TEST_ptr((p = BN_bin2bn(p_data, sizeof(p_data), NULL)))
           && TEST_ptr((q = BN_bin2bn(q_data, sizeof(q_data), NULL)))
           && TEST_ptr((n = BN_bin2bn(n_data, sizeof(n_data), NULL)))
           && TEST_ptr((d = BN_bin2bn(d_data, sizeof(d_data), NULL)))
           && TEST_ptr((e = BN_bin2bn(e_data, sizeof(e_data), NULL)))
           && TEST_ptr((rsa = RSA_new()))
           && TEST_ptr((md = EVP_sha256()))
           && TEST_ptr((ctx = EVP_MD_CTX_new()))
           && TEST_ptr((pkey = EVP_PKEY_new()))
           && TEST_true(RSA_set0_factors(rsa, p, q))
           && TEST_true(RSA_set0_key(rsa, n, e, d))
           && TEST_true(EVP_PKEY_assign_RSA(pkey, rsa))
           && TEST_true(EVP_DigestSignInit(ctx, NULL, md, NULL, pkey))
           && TEST_true(EVP_DigestSign(ctx, sigbuf, &buflen, msgbuf, msglen)));

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ret;
}

int setup_tests(void)
{
    ADD_ALL_TESTS(test_rsa_pkcs1, 3);
    ADD_ALL_TESTS(test_rsa_oaep, 3);
    ADD_ALL_TESTS(test_rsa_security_bit, OSSL_NELEM(rsa_security_bits_cases));
    ADD_TEST(test_EVP_rsa_legacy_key);
    return 1;
}
