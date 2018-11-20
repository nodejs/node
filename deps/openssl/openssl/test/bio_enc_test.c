/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/rand.h>

int main()
{
    BIO *b;
    static const unsigned char key[16] = { 0 };
    static unsigned char inp[1024] = { 0 };
    unsigned char out[1024], ref[1024];
    int i, lref, len;

    /* Fill buffer with non-zero data so that over steps can be detected */
    if (RAND_bytes(inp, sizeof(inp)) <= 0)
        return -1;

    /*
     * Exercise CBC cipher
     */

    /* reference output for single-chunk operation */
    b = BIO_new(BIO_f_cipher());
    if (!BIO_set_cipher(b, EVP_aes_128_cbc(), key, NULL, 0))
        return -1;
    BIO_push(b, BIO_new_mem_buf(inp, sizeof(inp)));
    lref = BIO_read(b, ref, sizeof(ref));
    BIO_free_all(b);

    /* perform split operations and compare to reference */
    for (i = 1; i < lref; i++) {
        b = BIO_new(BIO_f_cipher());
        if (!BIO_set_cipher(b, EVP_aes_128_cbc(), key, NULL, 0))
             return -1;
        BIO_push(b, BIO_new_mem_buf(inp, sizeof(inp)));
        memset(out, 0, sizeof(out));
        out[i] = ~ref[i];
        len = BIO_read(b, out, i);
        /* check for overstep */
        if (out[i] != (unsigned char)~ref[i]) {
            fprintf(stderr, "CBC output overstep@%d\n", i);
            return 1;
        }
        len += BIO_read(b, out + len, sizeof(out) - len);
        BIO_free_all(b);

        if (len != lref || memcmp(out, ref, len)) {
            fprintf(stderr, "CBC output mismatch@%d\n", i);
            return 2;
        }
    }

    /* perform small-chunk operations and compare to reference */
    for (i = 1; i < lref / 2; i++) {
        int delta;

        b = BIO_new(BIO_f_cipher());
        if (!BIO_set_cipher(b, EVP_aes_128_cbc(), key, NULL, 0))
             return -1;
        BIO_push(b, BIO_new_mem_buf(inp, sizeof(inp)));
        memset(out, 0, sizeof(out));
        for (len = 0; (delta = BIO_read(b, out + len, i)); ) {
            len += delta;
        }
        BIO_free_all(b);

        if (len != lref || memcmp(out, ref, len)) {
            fprintf(stderr, "CBC output mismatch@%d\n", i);
            return 3;
        }
    }

    /*
     * Exercise CTR cipher
     */

    /* reference output for single-chunk operation */
    b = BIO_new(BIO_f_cipher());
    if (!BIO_set_cipher(b, EVP_aes_128_ctr(), key, NULL, 0))
         return -1;
    BIO_push(b, BIO_new_mem_buf(inp, sizeof(inp)));
    lref = BIO_read(b, ref, sizeof(ref));
    BIO_free_all(b);

    /* perform split operations and compare to reference */
    for (i = 1; i < lref; i++) {
        b = BIO_new(BIO_f_cipher());
        if (!BIO_set_cipher(b, EVP_aes_128_ctr(), key, NULL, 0))
             return -1;
        BIO_push(b, BIO_new_mem_buf(inp, sizeof(inp)));
        memset(out, 0, sizeof(out));
        out[i] = ~ref[i];
        len = BIO_read(b, out, i);
        /* check for overstep */
        if (out[i] != (unsigned char)~ref[i]) {
            fprintf(stderr, "CTR output overstep@%d\n", i);
            return 4;
        }
        len += BIO_read(b, out + len, sizeof(out) - len);
        BIO_free_all(b);

        if (len != lref || memcmp(out, ref, len)) {
            fprintf(stderr, "CTR output mismatch@%d\n", i);
            return 5;
        }
    }

    /* perform small-chunk operations and compare to reference */
    for (i = 1; i < lref / 2; i++) {
        int delta;

        b = BIO_new(BIO_f_cipher());
        if (!BIO_set_cipher(b, EVP_aes_128_ctr(), key, NULL, 0))
             return -1;
        BIO_push(b, BIO_new_mem_buf(inp, sizeof(inp)));
        memset(out, 0, sizeof(out));
        for (len = 0; (delta = BIO_read(b, out + len, i)); ) {
            len += delta;
        }
        BIO_free_all(b);

        if (len != lref || memcmp(out, ref, len)) {
            fprintf(stderr, "CTR output mismatch@%d\n", i);
            return 6;
        }
    }

    return 0;
}
