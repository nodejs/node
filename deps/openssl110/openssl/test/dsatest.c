/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "../e_os.h"

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/bn.h>

#ifdef OPENSSL_NO_DSA
int main(int argc, char *argv[])
{
    printf("No DSA support\n");
    return (0);
}
#else
# include <openssl/dsa.h>

static int dsa_cb(int p, int n, BN_GENCB *arg);

/*
 * seed, out_p, out_q, out_g are taken from the updated Appendix 5 to FIPS
 * PUB 186 and also appear in Appendix 5 to FIPS PIB 186-1
 */
static unsigned char seed[20] = {
    0xd5, 0x01, 0x4e, 0x4b, 0x60, 0xef, 0x2b, 0xa8, 0xb6, 0x21, 0x1b, 0x40,
    0x62, 0xba, 0x32, 0x24, 0xe0, 0x42, 0x7d, 0xd3,
};

static unsigned char out_p[] = {
    0x8d, 0xf2, 0xa4, 0x94, 0x49, 0x22, 0x76, 0xaa,
    0x3d, 0x25, 0x75, 0x9b, 0xb0, 0x68, 0x69, 0xcb,
    0xea, 0xc0, 0xd8, 0x3a, 0xfb, 0x8d, 0x0c, 0xf7,
    0xcb, 0xb8, 0x32, 0x4f, 0x0d, 0x78, 0x82, 0xe5,
    0xd0, 0x76, 0x2f, 0xc5, 0xb7, 0x21, 0x0e, 0xaf,
    0xc2, 0xe9, 0xad, 0xac, 0x32, 0xab, 0x7a, 0xac,
    0x49, 0x69, 0x3d, 0xfb, 0xf8, 0x37, 0x24, 0xc2,
    0xec, 0x07, 0x36, 0xee, 0x31, 0xc8, 0x02, 0x91,
};

static unsigned char out_q[] = {
    0xc7, 0x73, 0x21, 0x8c, 0x73, 0x7e, 0xc8, 0xee,
    0x99, 0x3b, 0x4f, 0x2d, 0xed, 0x30, 0xf4, 0x8e,
    0xda, 0xce, 0x91, 0x5f,
};

static unsigned char out_g[] = {
    0x62, 0x6d, 0x02, 0x78, 0x39, 0xea, 0x0a, 0x13,
    0x41, 0x31, 0x63, 0xa5, 0x5b, 0x4c, 0xb5, 0x00,
    0x29, 0x9d, 0x55, 0x22, 0x95, 0x6c, 0xef, 0xcb,
    0x3b, 0xff, 0x10, 0xf3, 0x99, 0xce, 0x2c, 0x2e,
    0x71, 0xcb, 0x9d, 0xe5, 0xfa, 0x24, 0xba, 0xbf,
    0x58, 0xe5, 0xb7, 0x95, 0x21, 0x92, 0x5c, 0x9c,
    0xc4, 0x2e, 0x9f, 0x6f, 0x46, 0x4b, 0x08, 0x8c,
    0xc5, 0x72, 0xaf, 0x53, 0xe6, 0xd7, 0x88, 0x02,
};

static const unsigned char str1[] = "12345678901234567890";

static const char rnd_seed[] =
    "string to make the random number generator think it has entropy";

static BIO *bio_err = NULL;

int main(int argc, char **argv)
{
    BN_GENCB *cb;
    DSA *dsa = NULL;
    int counter, ret = 0, i, j;
    unsigned char buf[256];
    unsigned long h;
    unsigned char sig[256];
    unsigned int siglen;
    const BIGNUM *p = NULL, *q = NULL, *g = NULL;

    if (bio_err == NULL)
        bio_err = BIO_new_fp(stderr, BIO_NOCLOSE | BIO_FP_TEXT);

    CRYPTO_set_mem_debug(1);
    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    RAND_seed(rnd_seed, sizeof rnd_seed);

    BIO_printf(bio_err, "test generation of DSA parameters\n");

    cb = BN_GENCB_new();
    if (!cb)
        goto end;

    BN_GENCB_set(cb, dsa_cb, bio_err);
    if (((dsa = DSA_new()) == NULL) || !DSA_generate_parameters_ex(dsa, 512,
                                                                   seed, 20,
                                                                   &counter,
                                                                   &h, cb))
        goto end;

    BIO_printf(bio_err, "seed\n");
    for (i = 0; i < 20; i += 4) {
        BIO_printf(bio_err, "%02X%02X%02X%02X ",
                   seed[i], seed[i + 1], seed[i + 2], seed[i + 3]);
    }
    BIO_printf(bio_err, "\ncounter=%d h=%ld\n", counter, h);

    DSA_print(bio_err, dsa, 0);
    if (counter != 105) {
        BIO_printf(bio_err, "counter should be 105\n");
        goto end;
    }
    if (h != 2) {
        BIO_printf(bio_err, "h should be 2\n");
        goto end;
    }

    DSA_get0_pqg(dsa, &p, &q, &g);
    i = BN_bn2bin(q, buf);
    j = sizeof(out_q);
    if ((i != j) || (memcmp(buf, out_q, i) != 0)) {
        BIO_printf(bio_err, "q value is wrong\n");
        goto end;
    }

    i = BN_bn2bin(p, buf);
    j = sizeof(out_p);
    if ((i != j) || (memcmp(buf, out_p, i) != 0)) {
        BIO_printf(bio_err, "p value is wrong\n");
        goto end;
    }

    i = BN_bn2bin(g, buf);
    j = sizeof(out_g);
    if ((i != j) || (memcmp(buf, out_g, i) != 0)) {
        BIO_printf(bio_err, "g value is wrong\n");
        goto end;
    }

    DSA_generate_key(dsa);
    DSA_sign(0, str1, 20, sig, &siglen, dsa);
    if (DSA_verify(0, str1, 20, sig, siglen, dsa) == 1)
        ret = 1;

 end:
    if (!ret)
        ERR_print_errors(bio_err);
    DSA_free(dsa);
    BN_GENCB_free(cb);

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks(bio_err) <= 0)
        ret = 0;
#endif
    BIO_free(bio_err);
    bio_err = NULL;
    EXIT(!ret);
}

static int dsa_cb(int p, int n, BN_GENCB *arg)
{
    char c = '*';
    static int ok = 0, num = 0;

    if (p == 0) {
        c = '.';
        num++;
    };
    if (p == 1)
        c = '+';
    if (p == 2) {
        c = '*';
        ok++;
    }
    if (p == 3)
        c = '\n';
    BIO_write(BN_GENCB_get_arg(arg), &c, 1);
    (void)BIO_flush(BN_GENCB_get_arg(arg));

    if (!ok && (p == 0) && (num > 1)) {
        BIO_printf(BN_GENCB_get_arg(arg), "error in dsatest\n");
        return 0;
    }
    return 1;
}
#endif
