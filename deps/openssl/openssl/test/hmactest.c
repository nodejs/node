/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../e_os.h"

# include <openssl/hmac.h>
# include <openssl/sha.h>
# ifndef OPENSSL_NO_MD5
#  include <openssl/md5.h>
# endif

# ifdef CHARSET_EBCDIC
#  include <openssl/ebcdic.h>
# endif

# ifndef OPENSSL_NO_MD5
static struct test_st {
    unsigned char key[16];
    int key_len;
    unsigned char data[64];
    int data_len;
    unsigned char *digest;
} test[8] = {
    {
        "", 0, "More text test vectors to stuff up EBCDIC machines :-)", 54,
        (unsigned char *)"e9139d1e6ee064ef8cf514fc7dc83e86",
    },
    {
        {
            0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
            0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
        }, 16, "Hi There", 8,
        (unsigned char *)"9294727a3638bb1c13f48ef8158bfc9d",
    },
    {
        "Jefe", 4, "what do ya want for nothing?", 28,
        (unsigned char *)"750c783e6ab0b503eaa86e310a5db738",
    },
    {
        {
            0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
            0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
        }, 16, {
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
            0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd
        }, 50, (unsigned char *)"56be34521d144c88dbb8c733f0e8b3f6",
    },
    {
        "", 0, "My test data", 12,
        (unsigned char *)"61afdecb95429ef494d61fdee15990cabf0826fc"
    },
    {
        "", 0, "My test data", 12,
        (unsigned char *)"2274b195d90ce8e03406f4b526a47e0787a88a65479938f1a5baa3ce0f079776"
    },
    {
        "123456", 6, "My test data", 12,
        (unsigned char *)"bab53058ae861a7f191abe2d0145cbb123776a6369ee3f9d79ce455667e411dd"
    },
    {
        "12345", 5, "My test data again", 18,
        (unsigned char *)"a12396ceddd2a85f4c656bc1e0aa50c78cffde3e"
    }
};
# endif

static char *pt(unsigned char *md, unsigned int len);

int main(int argc, char *argv[])
{
# ifndef OPENSSL_NO_MD5
    int i;
    char *p;
# endif
    int err = 0;
    HMAC_CTX *ctx = NULL, *ctx2 = NULL;
    unsigned char buf[EVP_MAX_MD_SIZE];
    unsigned int len;

# ifdef OPENSSL_NO_MD5
    printf("test skipped: MD5 disabled\n");
# else

#  ifdef CHARSET_EBCDIC
    ebcdic2ascii(test[0].data, test[0].data, test[0].data_len);
    ebcdic2ascii(test[1].data, test[1].data, test[1].data_len);
    ebcdic2ascii(test[2].key, test[2].key, test[2].key_len);
    ebcdic2ascii(test[2].data, test[2].data, test[2].data_len);
#  endif

    for (i = 0; i < 4; i++) {
        p = pt(HMAC(EVP_md5(),
                    test[i].key, test[i].key_len,
                    test[i].data, test[i].data_len, NULL, NULL),
                    MD5_DIGEST_LENGTH);

        if (strcmp(p, (char *)test[i].digest) != 0) {
            printf("Error calculating HMAC on %d entry'\n", i);
            printf("got %s instead of %s\n", p, test[i].digest);
            err++;
        } else
            printf("test %d ok\n", i);
    }
# endif                         /* OPENSSL_NO_MD5 */

/* test4 */
    ctx = HMAC_CTX_new();
    if (ctx == NULL) {
        printf("HMAC malloc failure (test 4)\n");
        err++;
        goto end;
    }
    if (HMAC_CTX_get_md(ctx) != NULL) {
        printf("Message digest not NULL for HMAC (test 4)\n");
        err++;
        goto test5;
    }
    if (HMAC_Init_ex(ctx, NULL, 0, NULL, NULL)) {
        printf("Should fail to initialise HMAC with empty MD and key (test 4)\n");
        err++;
        goto test5;
    }
    if (HMAC_Update(ctx, test[4].data, test[4].data_len)) {
        printf("Should fail HMAC_Update with ctx not set up (test 4)\n");
        err++;
        goto test5;
    }
    if (HMAC_Init_ex(ctx, NULL, 0, EVP_sha1(), NULL)) {
        printf("Should fail to initialise HMAC with empty key (test 4)\n");
        err++;
        goto test5;
    }
    if (HMAC_Update(ctx, test[4].data, test[4].data_len)) {
        printf("Should fail HMAC_Update with ctx not set up (test 4)\n");
        err++;
        goto test5;
    }
    printf("test 4 ok\n");
test5:
    /* Test 5 has empty key; test that single-shot accepts a NULL key. */
    p = pt(HMAC(EVP_sha1(), NULL, 0, test[4].data, test[4].data_len,
                NULL, NULL), SHA_DIGEST_LENGTH);
    if (strcmp(p, (char *)test[4].digest) != 0) {
        printf("Error calculating HMAC on %d entry'\n", i);
        printf("got %s instead of %s\n", p, test[4].digest);
        err++;
    }

    HMAC_CTX_reset(ctx);
    if (HMAC_CTX_get_md(ctx) != NULL) {
        printf("Message digest not NULL for HMAC (test 5)\n");
        err++;
        goto test6;
    }
    if (HMAC_Init_ex(ctx, test[4].key, test[4].key_len, NULL, NULL)) {
        printf("Should fail to initialise HMAC with empty MD (test 5)\n");
        err++;
        goto test6;
    }
    if (HMAC_Update(ctx, test[4].data, test[4].data_len)) {
        printf("Should fail HMAC_Update with ctx not set up (test 5)\n");
        err++;
        goto test6;
    }
    if (HMAC_Init_ex(ctx, test[4].key, -1, EVP_sha1(), NULL)) {
        printf("Should fail to initialise HMAC with invalid key len(test 5)\n");
        err++;
        goto test6;
    }
    if (!HMAC_Init_ex(ctx, test[4].key, test[4].key_len, EVP_sha1(), NULL)) {
        printf("Failed to initialise HMAC (test 5)\n");
        err++;
        goto test6;
    }
    if (!HMAC_Update(ctx, test[4].data, test[4].data_len)) {
        printf("Error updating HMAC with data (test 5)\n");
        err++;
        goto test6;
    }
    if (!HMAC_Final(ctx, buf, &len)) {
        printf("Error finalising data (test 5)\n");
        err++;
        goto test6;
    }
    p = pt(buf, len);
    if (strcmp(p, (char *)test[4].digest) != 0) {
        printf("Error calculating interim HMAC on test 5\n");
        printf("got %s instead of %s\n", p, test[4].digest);
        err++;
        goto test6;
    }
    if (HMAC_Init_ex(ctx, NULL, 0, EVP_sha256(), NULL)) {
        printf("Should disallow changing MD without a new key (test 5)\n");
        err++;
        goto test6;
    }
    if (!HMAC_Init_ex(ctx, test[5].key, test[5].key_len, EVP_sha256(), NULL)) {
        printf("Failed to reinitialise HMAC (test 5)\n");
        err++;
        goto test6;
    }
    if (HMAC_CTX_get_md(ctx) != EVP_sha256()) {
        printf("Unexpected message digest for HMAC (test 5)\n");
        err++;
        goto test6;
    }
    if (!HMAC_Update(ctx, test[5].data, test[5].data_len)) {
        printf("Error updating HMAC with data (sha256) (test 5)\n");
        err++;
        goto test6;
    }
    if (!HMAC_Final(ctx, buf, &len)) {
        printf("Error finalising data (sha256) (test 5)\n");
        err++;
        goto test6;
    }
    p = pt(buf, len);
    if (strcmp(p, (char *)test[5].digest) != 0) {
        printf("Error calculating 2nd interim HMAC on test 5\n");
        printf("got %s instead of %s\n", p, test[5].digest);
        err++;
        goto test6;
    }
    if (!HMAC_Init_ex(ctx, test[6].key, test[6].key_len, NULL, NULL)) {
        printf("Failed to reinitialise HMAC with key (test 5)\n");
        err++;
        goto test6;
    }
    if (!HMAC_Update(ctx, test[6].data, test[6].data_len)) {
        printf("Error updating HMAC with data (new key) (test 5)\n");
        err++;
        goto test6;
    }
    if (!HMAC_Final(ctx, buf, &len)) {
        printf("Error finalising data (new key) (test 5)\n");
        err++;
        goto test6;
    }
    p = pt(buf, len);
    if (strcmp(p, (char *)test[6].digest) != 0) {
        printf("error calculating HMAC on test 5\n");
        printf("got %s instead of %s\n", p, test[6].digest);
        err++;
    } else {
        printf("test 5 ok\n");
    }
test6:
    HMAC_CTX_reset(ctx);
    ctx2 = HMAC_CTX_new();
    if (ctx2 == NULL) {
        printf("HMAC malloc failure (test 6)\n");
        err++;
        goto end;
    }
    if (!HMAC_Init_ex(ctx, test[7].key, test[7].key_len, EVP_sha1(), NULL)) {
        printf("Failed to initialise HMAC (test 6)\n");
        err++;
        goto end;
    }
    if (!HMAC_Update(ctx, test[7].data, test[7].data_len)) {
        printf("Error updating HMAC with data (test 6)\n");
        err++;
        goto end;
    }
    if (!HMAC_CTX_copy(ctx2, ctx)) {
        printf("Failed to copy HMAC_CTX (test 6)\n");
        err++;
        goto end;
    }
    if (!HMAC_Final(ctx2, buf, &len)) {
        printf("Error finalising data (test 6)\n");
        err++;
        goto end;
    }
    p = pt(buf, len);
    if (strcmp(p, (char *)test[7].digest) != 0) {
        printf("Error calculating HMAC on test 6\n");
        printf("got %s instead of %s\n", p, test[7].digest);
        err++;
    } else {
        printf("test 6 ok\n");
    }
end:
    HMAC_CTX_free(ctx2);
    HMAC_CTX_free(ctx);
    EXIT(err);
}

# ifndef OPENSSL_NO_MD5
static char *pt(unsigned char *md, unsigned int len)
{
    unsigned int i;
    static char buf[80];

    for (i = 0; i < len; i++)
        sprintf(&(buf[i * 2]), "%02x", md[i]);
    return (buf);
}
# endif
