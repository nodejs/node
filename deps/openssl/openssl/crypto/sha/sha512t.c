/* crypto/sha/sha512t.c */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
 * ====================================================================
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>

#if defined(OPENSSL_NO_SHA) || defined(OPENSSL_NO_SHA512)
int main(int argc, char *argv[])
{
    printf("No SHA512 support\n");
    return (0);
}
#else

unsigned char app_c1[SHA512_DIGEST_LENGTH] = {
    0xdd, 0xaf, 0x35, 0xa1, 0x93, 0x61, 0x7a, 0xba,
    0xcc, 0x41, 0x73, 0x49, 0xae, 0x20, 0x41, 0x31,
    0x12, 0xe6, 0xfa, 0x4e, 0x89, 0xa9, 0x7e, 0xa2,
    0x0a, 0x9e, 0xee, 0xe6, 0x4b, 0x55, 0xd3, 0x9a,
    0x21, 0x92, 0x99, 0x2a, 0x27, 0x4f, 0xc1, 0xa8,
    0x36, 0xba, 0x3c, 0x23, 0xa3, 0xfe, 0xeb, 0xbd,
    0x45, 0x4d, 0x44, 0x23, 0x64, 0x3c, 0xe8, 0x0e,
    0x2a, 0x9a, 0xc9, 0x4f, 0xa5, 0x4c, 0xa4, 0x9f
};

unsigned char app_c2[SHA512_DIGEST_LENGTH] = {
    0x8e, 0x95, 0x9b, 0x75, 0xda, 0xe3, 0x13, 0xda,
    0x8c, 0xf4, 0xf7, 0x28, 0x14, 0xfc, 0x14, 0x3f,
    0x8f, 0x77, 0x79, 0xc6, 0xeb, 0x9f, 0x7f, 0xa1,
    0x72, 0x99, 0xae, 0xad, 0xb6, 0x88, 0x90, 0x18,
    0x50, 0x1d, 0x28, 0x9e, 0x49, 0x00, 0xf7, 0xe4,
    0x33, 0x1b, 0x99, 0xde, 0xc4, 0xb5, 0x43, 0x3a,
    0xc7, 0xd3, 0x29, 0xee, 0xb6, 0xdd, 0x26, 0x54,
    0x5e, 0x96, 0xe5, 0x5b, 0x87, 0x4b, 0xe9, 0x09
};

unsigned char app_c3[SHA512_DIGEST_LENGTH] = {
    0xe7, 0x18, 0x48, 0x3d, 0x0c, 0xe7, 0x69, 0x64,
    0x4e, 0x2e, 0x42, 0xc7, 0xbc, 0x15, 0xb4, 0x63,
    0x8e, 0x1f, 0x98, 0xb1, 0x3b, 0x20, 0x44, 0x28,
    0x56, 0x32, 0xa8, 0x03, 0xaf, 0xa9, 0x73, 0xeb,
    0xde, 0x0f, 0xf2, 0x44, 0x87, 0x7e, 0xa6, 0x0a,
    0x4c, 0xb0, 0x43, 0x2c, 0xe5, 0x77, 0xc3, 0x1b,
    0xeb, 0x00, 0x9c, 0x5c, 0x2c, 0x49, 0xaa, 0x2e,
    0x4e, 0xad, 0xb2, 0x17, 0xad, 0x8c, 0xc0, 0x9b
};

unsigned char app_d1[SHA384_DIGEST_LENGTH] = {
    0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b,
    0xb5, 0xa0, 0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07,
    0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63,
    0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed,
    0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23,
    0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7
};

unsigned char app_d2[SHA384_DIGEST_LENGTH] = {
    0x09, 0x33, 0x0c, 0x33, 0xf7, 0x11, 0x47, 0xe8,
    0x3d, 0x19, 0x2f, 0xc7, 0x82, 0xcd, 0x1b, 0x47,
    0x53, 0x11, 0x1b, 0x17, 0x3b, 0x3b, 0x05, 0xd2,
    0x2f, 0xa0, 0x80, 0x86, 0xe3, 0xb0, 0xf7, 0x12,
    0xfc, 0xc7, 0xc7, 0x1a, 0x55, 0x7e, 0x2d, 0xb9,
    0x66, 0xc3, 0xe9, 0xfa, 0x91, 0x74, 0x60, 0x39
};

unsigned char app_d3[SHA384_DIGEST_LENGTH] = {
    0x9d, 0x0e, 0x18, 0x09, 0x71, 0x64, 0x74, 0xcb,
    0x08, 0x6e, 0x83, 0x4e, 0x31, 0x0a, 0x4a, 0x1c,
    0xed, 0x14, 0x9e, 0x9c, 0x00, 0xf2, 0x48, 0x52,
    0x79, 0x72, 0xce, 0xc5, 0x70, 0x4c, 0x2a, 0x5b,
    0x07, 0xb8, 0xb3, 0xdc, 0x38, 0xec, 0xc4, 0xeb,
    0xae, 0x97, 0xdd, 0xd8, 0x7f, 0x3d, 0x89, 0x85
};

int main(int argc, char **argv)
{
    unsigned char md[SHA512_DIGEST_LENGTH];
    int i;
    EVP_MD_CTX evp;

# ifdef OPENSSL_IA32_SSE2
    /*
     * Alternative to this is to call OpenSSL_add_all_algorithms... The below
     * code is retained exclusively for debugging purposes.
     */
    {
        char *env;

        if ((env = getenv("OPENSSL_ia32cap")))
            OPENSSL_ia32cap = strtoul(env, NULL, 0);
    }
# endif

    fprintf(stdout, "Testing SHA-512 ");

    EVP_Digest("abc", 3, md, NULL, EVP_sha512(), NULL);
    if (memcmp(md, app_c1, sizeof(app_c1))) {
        fflush(stdout);
        fprintf(stderr, "\nTEST 1 of 3 failed.\n");
        return 1;
    } else
        fprintf(stdout, ".");
    fflush(stdout);

    EVP_Digest("abcdefgh" "bcdefghi" "cdefghij" "defghijk"
               "efghijkl" "fghijklm" "ghijklmn" "hijklmno"
               "ijklmnop" "jklmnopq" "klmnopqr" "lmnopqrs"
               "mnopqrst" "nopqrstu", 112, md, NULL, EVP_sha512(), NULL);
    if (memcmp(md, app_c2, sizeof(app_c2))) {
        fflush(stdout);
        fprintf(stderr, "\nTEST 2 of 3 failed.\n");
        return 1;
    } else
        fprintf(stdout, ".");
    fflush(stdout);

    EVP_MD_CTX_init(&evp);
    EVP_DigestInit_ex(&evp, EVP_sha512(), NULL);
    for (i = 0; i < 1000000; i += 288)
        EVP_DigestUpdate(&evp, "aaaaaaaa" "aaaaaaaa" "aaaaaaaa" "aaaaaaaa"
                         "aaaaaaaa" "aaaaaaaa" "aaaaaaaa" "aaaaaaaa"
                         "aaaaaaaa" "aaaaaaaa" "aaaaaaaa" "aaaaaaaa"
                         "aaaaaaaa" "aaaaaaaa" "aaaaaaaa" "aaaaaaaa"
                         "aaaaaaaa" "aaaaaaaa" "aaaaaaaa" "aaaaaaaa"
                         "aaaaaaaa" "aaaaaaaa" "aaaaaaaa" "aaaaaaaa"
                         "aaaaaaaa" "aaaaaaaa" "aaaaaaaa" "aaaaaaaa"
                         "aaaaaaaa" "aaaaaaaa" "aaaaaaaa" "aaaaaaaa"
                         "aaaaaaaa" "aaaaaaaa" "aaaaaaaa" "aaaaaaaa",
                         (1000000 - i) < 288 ? 1000000 - i : 288);
    EVP_DigestFinal_ex(&evp, md, NULL);
    EVP_MD_CTX_cleanup(&evp);

    if (memcmp(md, app_c3, sizeof(app_c3))) {
        fflush(stdout);
        fprintf(stderr, "\nTEST 3 of 3 failed.\n");
        return 1;
    } else
        fprintf(stdout, ".");
    fflush(stdout);

    fprintf(stdout, " passed.\n");
    fflush(stdout);

    fprintf(stdout, "Testing SHA-384 ");

    EVP_Digest("abc", 3, md, NULL, EVP_sha384(), NULL);
    if (memcmp(md, app_d1, sizeof(app_d1))) {
        fflush(stdout);
        fprintf(stderr, "\nTEST 1 of 3 failed.\n");
        return 1;
    } else
        fprintf(stdout, ".");
    fflush(stdout);

    EVP_Digest("abcdefgh" "bcdefghi" "cdefghij" "defghijk"
               "efghijkl" "fghijklm" "ghijklmn" "hijklmno"
               "ijklmnop" "jklmnopq" "klmnopqr" "lmnopqrs"
               "mnopqrst" "nopqrstu", 112, md, NULL, EVP_sha384(), NULL);
    if (memcmp(md, app_d2, sizeof(app_d2))) {
        fflush(stdout);
        fprintf(stderr, "\nTEST 2 of 3 failed.\n");
        return 1;
    } else
        fprintf(stdout, ".");
    fflush(stdout);

    EVP_MD_CTX_init(&evp);
    EVP_DigestInit_ex(&evp, EVP_sha384(), NULL);
    for (i = 0; i < 1000000; i += 64)
        EVP_DigestUpdate(&evp, "aaaaaaaa" "aaaaaaaa" "aaaaaaaa" "aaaaaaaa"
                         "aaaaaaaa" "aaaaaaaa" "aaaaaaaa" "aaaaaaaa",
                         (1000000 - i) < 64 ? 1000000 - i : 64);
    EVP_DigestFinal_ex(&evp, md, NULL);
    EVP_MD_CTX_cleanup(&evp);

    if (memcmp(md, app_d3, sizeof(app_d3))) {
        fflush(stdout);
        fprintf(stderr, "\nTEST 3 of 3 failed.\n");
        return 1;
    } else
        fprintf(stdout, ".");
    fflush(stdout);

    fprintf(stdout, " passed.\n");
    fflush(stdout);

    return 0;
}
#endif
