/* test/igetest.c */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 */

#include <openssl/aes.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TEST_SIZE       128
#define BIG_TEST_SIZE 10240

static void hexdump(FILE *f, const char *title, const unsigned char *s, int l)
{
    int n = 0;

    fprintf(f, "%s", title);
    for (; n < l; ++n) {
        if ((n % 16) == 0)
            fprintf(f, "\n%04x", n);
        fprintf(f, " %02x", s[n]);
    }
    fprintf(f, "\n");
}

#define MAX_VECTOR_SIZE 64

struct ige_test {
    const unsigned char key[16];
    const unsigned char iv[32];
    const unsigned char in[MAX_VECTOR_SIZE];
    const unsigned char out[MAX_VECTOR_SIZE];
    const size_t length;
    const int encrypt;
};

static struct ige_test const ige_test_vectors[] = {
    {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}, /* key */
     {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f}, /* iv */
     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* in */
     {0x1a, 0x85, 0x19, 0xa6, 0x55, 0x7b, 0xe6, 0x52,
      0xe9, 0xda, 0x8e, 0x43, 0xda, 0x4e, 0xf4, 0x45,
      0x3c, 0xf4, 0x56, 0xb4, 0xca, 0x48, 0x8a, 0xa3,
      0x83, 0xc7, 0x9c, 0x98, 0xb3, 0x47, 0x97, 0xcb}, /* out */
     32, AES_ENCRYPT},          /* test vector 0 */

    {{0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20,
      0x61, 0x6e, 0x20, 0x69, 0x6d, 0x70, 0x6c, 0x65}, /* key */
     {0x6d, 0x65, 0x6e, 0x74, 0x61, 0x74, 0x69, 0x6f,
      0x6e, 0x20, 0x6f, 0x66, 0x20, 0x49, 0x47, 0x45,
      0x20, 0x6d, 0x6f, 0x64, 0x65, 0x20, 0x66, 0x6f,
      0x72, 0x20, 0x4f, 0x70, 0x65, 0x6e, 0x53, 0x53}, /* iv */
     {0x4c, 0x2e, 0x20, 0x4c, 0x65, 0x74, 0x27, 0x73,
      0x20, 0x68, 0x6f, 0x70, 0x65, 0x20, 0x42, 0x65,
      0x6e, 0x20, 0x67, 0x6f, 0x74, 0x20, 0x69, 0x74,
      0x20, 0x72, 0x69, 0x67, 0x68, 0x74, 0x21, 0x0a}, /* in */
     {0x99, 0x70, 0x64, 0x87, 0xa1, 0xcd, 0xe6, 0x13,
      0xbc, 0x6d, 0xe0, 0xb6, 0xf2, 0x4b, 0x1c, 0x7a,
      0xa4, 0x48, 0xc8, 0xb9, 0xc3, 0x40, 0x3e, 0x34,
      0x67, 0xa8, 0xca, 0xd8, 0x93, 0x40, 0xf5, 0x3b}, /* out */
     32, AES_DECRYPT},          /* test vector 1 */
};

struct bi_ige_test {
    const unsigned char key1[32];
    const unsigned char key2[32];
    const unsigned char iv[64];
    const unsigned char in[MAX_VECTOR_SIZE];
    const unsigned char out[MAX_VECTOR_SIZE];
    const size_t keysize;
    const size_t length;
    const int encrypt;
};

static struct bi_ige_test const bi_ige_test_vectors[] = {
    {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}, /* key1 */
     {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f}, /* key2 */
     {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
      0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
      0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
      0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f}, /* iv */
     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* in */
     {0x14, 0x40, 0x6f, 0xae, 0xa2, 0x79, 0xf2, 0x56,
      0x1f, 0x86, 0xeb, 0x3b, 0x7d, 0xff, 0x53, 0xdc,
      0x4e, 0x27, 0x0c, 0x03, 0xde, 0x7c, 0xe5, 0x16,
      0x6a, 0x9c, 0x20, 0x33, 0x9d, 0x33, 0xfe, 0x12}, /* out */
     16, 32, AES_ENCRYPT},      /* test vector 0 */
    {{0x58, 0x0a, 0x06, 0xe9, 0x97, 0x07, 0x59, 0x5c,
      0x9e, 0x19, 0xd2, 0xa7, 0xbb, 0x40, 0x2b, 0x7a,
      0xc7, 0xd8, 0x11, 0x9e, 0x4c, 0x51, 0x35, 0x75,
      0x64, 0x28, 0x0f, 0x23, 0xad, 0x74, 0xac, 0x37}, /* key1 */
     {0xd1, 0x80, 0xa0, 0x31, 0x47, 0xa3, 0x11, 0x13,
      0x86, 0x26, 0x9e, 0x6d, 0xff, 0xaf, 0x72, 0x74,
      0x5b, 0xa2, 0x35, 0x81, 0xd2, 0xa6, 0x3d, 0x21,
      0x67, 0x7b, 0x58, 0xa8, 0x18, 0xf9, 0x72, 0xe4}, /* key2 */
     {0x80, 0x3d, 0xbd, 0x4c, 0xe6, 0x7b, 0x06, 0xa9,
      0x53, 0x35, 0xd5, 0x7e, 0x71, 0xc1, 0x70, 0x70,
      0x74, 0x9a, 0x00, 0x28, 0x0c, 0xbf, 0x6c, 0x42,
      0x9b, 0xa4, 0xdd, 0x65, 0x11, 0x77, 0x7c, 0x67,
      0xfe, 0x76, 0x0a, 0xf0, 0xd5, 0xc6, 0x6e, 0x6a,
      0xe7, 0x5e, 0x4c, 0xf2, 0x7e, 0x9e, 0xf9, 0x20,
      0x0e, 0x54, 0x6f, 0x2d, 0x8a, 0x8d, 0x7e, 0xbd,
      0x48, 0x79, 0x37, 0x99, 0xff, 0x27, 0x93, 0xa3}, /* iv */
     {0xf1, 0x54, 0x3d, 0xca, 0xfe, 0xb5, 0xef, 0x1c,
      0x4f, 0xa6, 0x43, 0xf6, 0xe6, 0x48, 0x57, 0xf0,
      0xee, 0x15, 0x7f, 0xe3, 0xe7, 0x2f, 0xd0, 0x2f,
      0x11, 0x95, 0x7a, 0x17, 0x00, 0xab, 0xa7, 0x0b,
      0xbe, 0x44, 0x09, 0x9c, 0xcd, 0xac, 0xa8, 0x52,
      0xa1, 0x8e, 0x7b, 0x75, 0xbc, 0xa4, 0x92, 0x5a,
      0xab, 0x46, 0xd3, 0x3a, 0xa0, 0xd5, 0x35, 0x1c,
      0x55, 0xa4, 0xb3, 0xa8, 0x40, 0x81, 0xa5, 0x0b}, /* in */
     {0x42, 0xe5, 0x28, 0x30, 0x31, 0xc2, 0xa0, 0x23,
      0x68, 0x49, 0x4e, 0xb3, 0x24, 0x59, 0x92, 0x79,
      0xc1, 0xa5, 0xcc, 0xe6, 0x76, 0x53, 0xb1, 0xcf,
      0x20, 0x86, 0x23, 0xe8, 0x72, 0x55, 0x99, 0x92,
      0x0d, 0x16, 0x1c, 0x5a, 0x2f, 0xce, 0xcb, 0x51,
      0xe2, 0x67, 0xfa, 0x10, 0xec, 0xcd, 0x3d, 0x67,
      0xa5, 0xe6, 0xf7, 0x31, 0x26, 0xb0, 0x0d, 0x76,
      0x5e, 0x28, 0xdc, 0x7f, 0x01, 0xc5, 0xa5, 0x4c}, /* out */
     32, 64, AES_ENCRYPT},      /* test vector 1 */

};

static int run_test_vectors(void)
{
    unsigned int n;
    int errs = 0;

    for (n = 0; n < sizeof(ige_test_vectors) / sizeof(ige_test_vectors[0]);
         ++n) {
        const struct ige_test *const v = &ige_test_vectors[n];
        AES_KEY key;
        unsigned char buf[MAX_VECTOR_SIZE];
        unsigned char iv[AES_BLOCK_SIZE * 2];

        assert(v->length <= MAX_VECTOR_SIZE);

        if (v->encrypt == AES_ENCRYPT)
            AES_set_encrypt_key(v->key, 8 * sizeof(v->key), &key);
        else
            AES_set_decrypt_key(v->key, 8 * sizeof(v->key), &key);
        memcpy(iv, v->iv, sizeof(iv));
        AES_ige_encrypt(v->in, buf, v->length, &key, iv, v->encrypt);

        if (memcmp(v->out, buf, v->length)) {
            printf("IGE test vector %d failed\n", n);
            hexdump(stdout, "key", v->key, sizeof(v->key));
            hexdump(stdout, "iv", v->iv, sizeof(v->iv));
            hexdump(stdout, "in", v->in, v->length);
            hexdump(stdout, "expected", v->out, v->length);
            hexdump(stdout, "got", buf, v->length);

            ++errs;
        }

        /* try with in == out */
        memcpy(iv, v->iv, sizeof(iv));
        memcpy(buf, v->in, v->length);
        AES_ige_encrypt(buf, buf, v->length, &key, iv, v->encrypt);

        if (memcmp(v->out, buf, v->length)) {
            printf("IGE test vector %d failed (with in == out)\n", n);
            hexdump(stdout, "key", v->key, sizeof(v->key));
            hexdump(stdout, "iv", v->iv, sizeof(v->iv));
            hexdump(stdout, "in", v->in, v->length);
            hexdump(stdout, "expected", v->out, v->length);
            hexdump(stdout, "got", buf, v->length);

            ++errs;
        }
    }

    for (n = 0;
         n < sizeof(bi_ige_test_vectors) / sizeof(bi_ige_test_vectors[0]);
         ++n) {
        const struct bi_ige_test *const v = &bi_ige_test_vectors[n];
        AES_KEY key1;
        AES_KEY key2;
        unsigned char buf[MAX_VECTOR_SIZE];

        assert(v->length <= MAX_VECTOR_SIZE);

        if (v->encrypt == AES_ENCRYPT) {
            AES_set_encrypt_key(v->key1, 8 * v->keysize, &key1);
            AES_set_encrypt_key(v->key2, 8 * v->keysize, &key2);
        } else {
            AES_set_decrypt_key(v->key1, 8 * v->keysize, &key1);
            AES_set_decrypt_key(v->key2, 8 * v->keysize, &key2);
        }

        AES_bi_ige_encrypt(v->in, buf, v->length, &key1, &key2, v->iv,
                           v->encrypt);

        if (memcmp(v->out, buf, v->length)) {
            printf("Bidirectional IGE test vector %d failed\n", n);
            hexdump(stdout, "key 1", v->key1, sizeof(v->key1));
            hexdump(stdout, "key 2", v->key2, sizeof(v->key2));
            hexdump(stdout, "iv", v->iv, sizeof(v->iv));
            hexdump(stdout, "in", v->in, v->length);
            hexdump(stdout, "expected", v->out, v->length);
            hexdump(stdout, "got", buf, v->length);

            ++errs;
        }
    }

    return errs;
}

int main(int argc, char **argv)
{
    unsigned char rkey[16];
    unsigned char rkey2[16];
    AES_KEY key;
    AES_KEY key2;
    unsigned char plaintext[BIG_TEST_SIZE];
    unsigned char ciphertext[BIG_TEST_SIZE];
    unsigned char checktext[BIG_TEST_SIZE];
    unsigned char iv[AES_BLOCK_SIZE * 4];
    unsigned char saved_iv[AES_BLOCK_SIZE * 4];
    int err = 0;
    unsigned int n;
    unsigned matches;

    assert(BIG_TEST_SIZE >= TEST_SIZE);

    RAND_pseudo_bytes(rkey, sizeof(rkey));
    RAND_pseudo_bytes(plaintext, sizeof(plaintext));
    RAND_pseudo_bytes(iv, sizeof(iv));
    memcpy(saved_iv, iv, sizeof(saved_iv));

    /* Forward IGE only... */

    /* Straight encrypt/decrypt */
    AES_set_encrypt_key(rkey, 8 * sizeof(rkey), &key);
    AES_ige_encrypt(plaintext, ciphertext, TEST_SIZE, &key, iv, AES_ENCRYPT);

    AES_set_decrypt_key(rkey, 8 * sizeof(rkey), &key);
    memcpy(iv, saved_iv, sizeof(iv));
    AES_ige_encrypt(ciphertext, checktext, TEST_SIZE, &key, iv, AES_DECRYPT);

    if (memcmp(checktext, plaintext, TEST_SIZE)) {
        printf("Encrypt+decrypt doesn't match\n");
        hexdump(stdout, "Plaintext", plaintext, TEST_SIZE);
        hexdump(stdout, "Checktext", checktext, TEST_SIZE);
        ++err;
    }

    /* Now check encrypt chaining works */
    AES_set_encrypt_key(rkey, 8 * sizeof(rkey), &key);
    memcpy(iv, saved_iv, sizeof(iv));
    AES_ige_encrypt(plaintext, ciphertext, TEST_SIZE / 2, &key, iv,
                    AES_ENCRYPT);
    AES_ige_encrypt(plaintext + TEST_SIZE / 2,
                    ciphertext + TEST_SIZE / 2, TEST_SIZE / 2,
                    &key, iv, AES_ENCRYPT);

    AES_set_decrypt_key(rkey, 8 * sizeof(rkey), &key);
    memcpy(iv, saved_iv, sizeof(iv));
    AES_ige_encrypt(ciphertext, checktext, TEST_SIZE, &key, iv, AES_DECRYPT);

    if (memcmp(checktext, plaintext, TEST_SIZE)) {
        printf("Chained encrypt+decrypt doesn't match\n");
        hexdump(stdout, "Plaintext", plaintext, TEST_SIZE);
        hexdump(stdout, "Checktext", checktext, TEST_SIZE);
        ++err;
    }

    /* And check decrypt chaining */
    AES_set_encrypt_key(rkey, 8 * sizeof(rkey), &key);
    memcpy(iv, saved_iv, sizeof(iv));
    AES_ige_encrypt(plaintext, ciphertext, TEST_SIZE / 2, &key, iv,
                    AES_ENCRYPT);
    AES_ige_encrypt(plaintext + TEST_SIZE / 2,
                    ciphertext + TEST_SIZE / 2, TEST_SIZE / 2,
                    &key, iv, AES_ENCRYPT);

    AES_set_decrypt_key(rkey, 8 * sizeof(rkey), &key);
    memcpy(iv, saved_iv, sizeof(iv));
    AES_ige_encrypt(ciphertext, checktext, TEST_SIZE / 2, &key, iv,
                    AES_DECRYPT);
    AES_ige_encrypt(ciphertext + TEST_SIZE / 2,
                    checktext + TEST_SIZE / 2, TEST_SIZE / 2, &key, iv,
                    AES_DECRYPT);

    if (memcmp(checktext, plaintext, TEST_SIZE)) {
        printf("Chained encrypt+chained decrypt doesn't match\n");
        hexdump(stdout, "Plaintext", plaintext, TEST_SIZE);
        hexdump(stdout, "Checktext", checktext, TEST_SIZE);
        ++err;
    }

    /* make sure garble extends forwards only */
    AES_set_encrypt_key(rkey, 8 * sizeof(rkey), &key);
    memcpy(iv, saved_iv, sizeof(iv));
    AES_ige_encrypt(plaintext, ciphertext, sizeof(plaintext), &key, iv,
                    AES_ENCRYPT);

    /* corrupt halfway through */
    ++ciphertext[sizeof(ciphertext) / 2];
    AES_set_decrypt_key(rkey, 8 * sizeof(rkey), &key);
    memcpy(iv, saved_iv, sizeof(iv));
    AES_ige_encrypt(ciphertext, checktext, sizeof(checktext), &key, iv,
                    AES_DECRYPT);

    matches = 0;
    for (n = 0; n < sizeof(checktext); ++n)
        if (checktext[n] == plaintext[n])
            ++matches;

    if (matches > sizeof(checktext) / 2 + sizeof(checktext) / 100) {
        printf("More than 51%% matches after garbling\n");
        ++err;
    }

    if (matches < sizeof(checktext) / 2) {
        printf("Garble extends backwards!\n");
        ++err;
    }

    /* Bi-directional IGE */

    /*
     * Note that we don't have to recover the IV, because chaining isn't
     */
    /* possible with biIGE, so the IV is not updated. */

    RAND_pseudo_bytes(rkey2, sizeof(rkey2));

    /* Straight encrypt/decrypt */
    AES_set_encrypt_key(rkey, 8 * sizeof(rkey), &key);
    AES_set_encrypt_key(rkey2, 8 * sizeof(rkey2), &key2);
    AES_bi_ige_encrypt(plaintext, ciphertext, TEST_SIZE, &key, &key2, iv,
                       AES_ENCRYPT);

    AES_set_decrypt_key(rkey, 8 * sizeof(rkey), &key);
    AES_set_decrypt_key(rkey2, 8 * sizeof(rkey2), &key2);
    AES_bi_ige_encrypt(ciphertext, checktext, TEST_SIZE, &key, &key2, iv,
                       AES_DECRYPT);

    if (memcmp(checktext, plaintext, TEST_SIZE)) {
        printf("Encrypt+decrypt doesn't match\n");
        hexdump(stdout, "Plaintext", plaintext, TEST_SIZE);
        hexdump(stdout, "Checktext", checktext, TEST_SIZE);
        ++err;
    }

    /* make sure garble extends both ways */
    AES_set_encrypt_key(rkey, 8 * sizeof(rkey), &key);
    AES_set_encrypt_key(rkey2, 8 * sizeof(rkey2), &key2);
    AES_ige_encrypt(plaintext, ciphertext, sizeof(plaintext), &key, iv,
                    AES_ENCRYPT);

    /* corrupt halfway through */
    ++ciphertext[sizeof(ciphertext) / 2];
    AES_set_decrypt_key(rkey, 8 * sizeof(rkey), &key);
    AES_set_decrypt_key(rkey2, 8 * sizeof(rkey2), &key2);
    AES_ige_encrypt(ciphertext, checktext, sizeof(checktext), &key, iv,
                    AES_DECRYPT);

    matches = 0;
    for (n = 0; n < sizeof(checktext); ++n)
        if (checktext[n] == plaintext[n])
            ++matches;

    if (matches > sizeof(checktext) / 100) {
        printf("More than 1%% matches after bidirectional garbling\n");
        ++err;
    }

    /* make sure garble extends both ways (2) */
    AES_set_encrypt_key(rkey, 8 * sizeof(rkey), &key);
    AES_set_encrypt_key(rkey2, 8 * sizeof(rkey2), &key2);
    AES_ige_encrypt(plaintext, ciphertext, sizeof(plaintext), &key, iv,
                    AES_ENCRYPT);

    /* corrupt right at the end */
    ++ciphertext[sizeof(ciphertext) - 1];
    AES_set_decrypt_key(rkey, 8 * sizeof(rkey), &key);
    AES_set_decrypt_key(rkey2, 8 * sizeof(rkey2), &key2);
    AES_ige_encrypt(ciphertext, checktext, sizeof(checktext), &key, iv,
                    AES_DECRYPT);

    matches = 0;
    for (n = 0; n < sizeof(checktext); ++n)
        if (checktext[n] == plaintext[n])
            ++matches;

    if (matches > sizeof(checktext) / 100) {
        printf("More than 1%% matches after bidirectional garbling (2)\n");
        ++err;
    }

    /* make sure garble extends both ways (3) */
    AES_set_encrypt_key(rkey, 8 * sizeof(rkey), &key);
    AES_set_encrypt_key(rkey2, 8 * sizeof(rkey2), &key2);
    AES_ige_encrypt(plaintext, ciphertext, sizeof(plaintext), &key, iv,
                    AES_ENCRYPT);

    /* corrupt right at the start */
    ++ciphertext[0];
    AES_set_decrypt_key(rkey, 8 * sizeof(rkey), &key);
    AES_set_decrypt_key(rkey2, 8 * sizeof(rkey2), &key2);
    AES_ige_encrypt(ciphertext, checktext, sizeof(checktext), &key, iv,
                    AES_DECRYPT);

    matches = 0;
    for (n = 0; n < sizeof(checktext); ++n)
        if (checktext[n] == plaintext[n])
            ++matches;

    if (matches > sizeof(checktext) / 100) {
        printf("More than 1%% matches after bidirectional garbling (3)\n");
        ++err;
    }

    err += run_test_vectors();

    return err;
}
