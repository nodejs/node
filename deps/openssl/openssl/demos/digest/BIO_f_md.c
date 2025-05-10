/*-
 * Copyright 2019-2024 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*-
 * Example of using EVP_MD_fetch and EVP_Digest* methods to calculate
 * a digest of static buffers
 * You can find SHA3 test vectors from NIST here:
 * https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/documents/sha3/sha-3bytetestvectors.zip
 * For example, contains these lines:
    Len = 80
    Msg = 1ca984dcc913344370cf
    MD = 6915ea0eeffb99b9b246a0e34daf3947852684c3d618260119a22835659e4f23d4eb66a15d0affb8e93771578f5e8f25b7a5f2a55f511fb8b96325ba2cd14816
 * use xxd convert the hex message string to binary input for BIO_f_md:
 * echo "1ca984dcc913344370cf" | xxd -r -p | ./BIO_f_md
 * and then verify the output matches MD above.
 */

#include <string.h>
#include <stdio.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

/*-
 * This demonstration will show how to digest data using
 * a BIO configured with a message digest
 * A message digest name may be passed as an argument.
 * The default digest is SHA3-512
 */

int main(int argc, char *argv[])
{
    int ret = EXIT_FAILURE;
    OSSL_LIB_CTX *library_context = NULL;
    BIO *input = NULL;
    BIO *bio_digest = NULL, *reading = NULL;
    EVP_MD *md = NULL;
    unsigned char buffer[512];
    int digest_size;
    char *digest_value = NULL;
    int j;

    input = BIO_new_fd(fileno(stdin), 1);
    if (input == NULL) {
        fprintf(stderr, "BIO_new_fd() for stdin returned NULL\n");
        goto cleanup;
    }
    library_context = OSSL_LIB_CTX_new();
    if (library_context == NULL) {
        fprintf(stderr, "OSSL_LIB_CTX_new() returned NULL\n");
        goto cleanup;
    }

    /*
     * Fetch a message digest by name
     * The algorithm name is case insensitive.
     * See providers(7) for details about algorithm fetching
     */
    md = EVP_MD_fetch(library_context, "SHA3-512", NULL);
    if (md == NULL) {
        fprintf(stderr, "EVP_MD_fetch did not find SHA3-512.\n");
        goto cleanup;
    }
    digest_size = EVP_MD_get_size(md);
    if (digest_size <= 0) {
        fprintf(stderr, "EVP_MD_get_size returned invalid size.\n");
        goto cleanup;
    }

    digest_value = OPENSSL_malloc(digest_size);
    if (digest_value == NULL) {
        fprintf(stderr, "Can't allocate %lu bytes for the digest value.\n", (unsigned long)digest_size);
        goto cleanup;
    }
    /* Make a bio that uses the digest */
    bio_digest = BIO_new(BIO_f_md());
    if (bio_digest == NULL) {
        fprintf(stderr, "BIO_new(BIO_f_md()) returned NULL\n");
        goto cleanup;
    }
    /* set our bio_digest BIO to digest data */
    if (BIO_set_md(bio_digest, md) != 1) {
           fprintf(stderr, "BIO_set_md failed.\n");
           goto cleanup;
    }
    /*-
     * We will use BIO chaining so that as we read, the digest gets updated
     * See the man page for BIO_push
     */
    reading = BIO_push(bio_digest, input);

    while (BIO_read(reading, buffer, sizeof(buffer)) > 0)
        ;

    /*-
     * BIO_gets must be used to calculate the final
     * digest value and then copy it to digest_value.
     */
    if (BIO_gets(bio_digest, digest_value, digest_size) != digest_size) {
        fprintf(stderr, "BIO_gets(bio_digest) failed\n");
        goto cleanup;
    }
    for (j = 0; j < digest_size; j++) {
        fprintf(stdout, "%02x", (unsigned char)digest_value[j]);
    }
    fprintf(stdout, "\n");
    ret = EXIT_SUCCESS;

cleanup:
    if (ret != EXIT_SUCCESS)
        ERR_print_errors_fp(stderr);

    OPENSSL_free(digest_value);
    BIO_free(input);
    BIO_free(bio_digest);
    EVP_MD_free(md);
    OSSL_LIB_CTX_free(library_context);

    return ret;
}
