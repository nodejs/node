/*-
 * Copyright 2022-2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * Example showing how to generate an RSA key pair.
 *
 * When generating an RSA key, you must specify the number of bits in the key. A
 * reasonable value would be 4096. Avoid using values below 2048. These values
 * are reasonable as of 2022.
 */

#include <string.h>
#include <stdio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/core_names.h>
#include <openssl/pem.h>

/* A property query used for selecting algorithm implementations. */
static const char *propq = NULL;

/*
 * Generates an RSA public-private key pair and returns it.
 * The number of bits is specified by the bits argument.
 *
 * This uses the long way of generating an RSA key.
 */
static EVP_PKEY *generate_rsa_key_long(OSSL_LIB_CTX *libctx, unsigned int bits)
{
    EVP_PKEY_CTX *genctx = NULL;
    EVP_PKEY *pkey = NULL;
    unsigned int primes = 2;

    /* Create context using RSA algorithm. "RSA-PSS" could also be used here. */
    genctx = EVP_PKEY_CTX_new_from_name(libctx, "RSA", propq);
    if (genctx == NULL) {
        fprintf(stderr, "EVP_PKEY_CTX_new_from_name() failed\n");
        goto cleanup;
    }

    /* Initialize context for key generation purposes. */
    if (EVP_PKEY_keygen_init(genctx) <= 0) {
        fprintf(stderr, "EVP_PKEY_keygen_init() failed\n");
        goto cleanup;
    }

    /*
     * Here we set the number of bits to use in the RSA key.
     * See comment at top of file for information on appropriate values.
     */
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(genctx, bits) <= 0) {
        fprintf(stderr, "EVP_PKEY_CTX_set_rsa_keygen_bits() failed\n");
        goto cleanup;
    }

    /*
     * It is possible to create an RSA key using more than two primes.
     * Do not do this unless you know why you need this.
     * You ordinarily do not need to specify this, as the default is two.
     *
     * Both of these parameters can also be set via EVP_PKEY_CTX_set_params, but
     * these functions provide a more concise way to do so.
     */
    if (EVP_PKEY_CTX_set_rsa_keygen_primes(genctx, primes) <= 0) {
        fprintf(stderr, "EVP_PKEY_CTX_set_rsa_keygen_primes() failed\n");
        goto cleanup;
    }

    /*
     * Generating an RSA key with a number of bits large enough to be secure for
     * modern applications can take a fairly substantial amount of time (e.g.
     * one second). If you require fast key generation, consider using an EC key
     * instead.
     *
     * If you require progress information during the key generation process,
     * you can set a progress callback using EVP_PKEY_set_cb; see the example in
     * EVP_PKEY_generate(3).
     */
    fprintf(stdout, "Generating RSA key, this may take some time...\n");
    if (EVP_PKEY_generate(genctx, &pkey) <= 0) {
        fprintf(stderr, "EVP_PKEY_generate() failed\n");
        goto cleanup;
    }

    /* pkey is now set to an object representing the generated key pair. */

cleanup:
    EVP_PKEY_CTX_free(genctx);
    return pkey;
}

/*
 * Generates an RSA public-private key pair and returns it.
 * The number of bits is specified by the bits argument.
 *
 * This uses a more concise way of generating an RSA key, which is suitable for
 * simple cases. It is used if -s is passed on the command line, otherwise the
 * long method above is used. The ability to choose between these two methods is
 * shown here only for demonstration; the results are equivalent.
 */
static EVP_PKEY *generate_rsa_key_short(OSSL_LIB_CTX *libctx, unsigned int bits)
{
    EVP_PKEY *pkey = NULL;

    fprintf(stdout, "Generating RSA key, this may take some time...\n");
    pkey = EVP_PKEY_Q_keygen(libctx, propq, "RSA", (size_t)bits);

    if (pkey == NULL)
        fprintf(stderr, "EVP_PKEY_Q_keygen() failed\n");

    return pkey;
}

/*
 * Prints information on an EVP_PKEY object representing an RSA key pair.
 */
static int dump_key(const EVP_PKEY *pkey)
{
    int ret = 0;
    int bits = 0;
    BIGNUM *n = NULL, *e = NULL, *d = NULL, *p = NULL, *q = NULL;

    /*
     * Retrieve value of n. This value is not secret and forms part of the
     * public key.
     *
     * Calling EVP_PKEY_get_bn_param with a NULL BIGNUM pointer causes
     * a new BIGNUM to be allocated, so these must be freed subsequently.
     */
    if (EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_N, &n) == 0) {
        fprintf(stderr, "Failed to retrieve n\n");
        goto cleanup;
    }

    /*
     * Retrieve value of e. This value is not secret and forms part of the
     * public key. It is typically 65537 and need not be changed.
     */
    if (EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_E, &e) == 0) {
        fprintf(stderr, "Failed to retrieve e\n");
        goto cleanup;
    }

    /*
     * Retrieve value of d. This value is secret and forms part of the private
     * key. It must not be published.
     */
    if (EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_D, &d) == 0) {
        fprintf(stderr, "Failed to retrieve d\n");
        goto cleanup;
    }

    /*
     * Retrieve value of the first prime factor, commonly known as p. This value
     * is secret and forms part of the private key. It must not be published.
     */
    if (EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_FACTOR1, &p) == 0) {
        fprintf(stderr, "Failed to retrieve p\n");
        goto cleanup;
    }

    /*
     * Retrieve value of the second prime factor, commonly known as q. This value
     * is secret and forms part of the private key. It must not be published.
     *
     * If you are creating an RSA key with more than two primes for special
     * applications, you can retrieve these primes with
     * OSSL_PKEY_PARAM_RSA_FACTOR3, etc.
     */
    if (EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_FACTOR2, &q) == 0) {
        fprintf(stderr, "Failed to retrieve q\n");
        goto cleanup;
    }

    /*
     * We can also retrieve the key size in bits for informational purposes.
     */
    if (EVP_PKEY_get_int_param(pkey, OSSL_PKEY_PARAM_BITS, &bits) == 0) {
        fprintf(stderr, "Failed to retrieve bits\n");
        goto cleanup;
    }

    /* Output hexadecimal representations of the BIGNUM objects. */
    fprintf(stdout, "\nNumber of bits: %d\n\n", bits);
    fprintf(stdout, "Public values:\n");
    fprintf(stdout, "  n = 0x");
    BN_print_fp(stdout, n);
    fprintf(stdout, "\n");

    fprintf(stdout, "  e = 0x");
    BN_print_fp(stdout, e);
    fprintf(stdout, "\n\n");

    fprintf(stdout, "Private values:\n");
    fprintf(stdout, "  d = 0x");
    BN_print_fp(stdout, d);
    fprintf(stdout, "\n");

    fprintf(stdout, "  p = 0x");
    BN_print_fp(stdout, p);
    fprintf(stdout, "\n");

    fprintf(stdout, "  q = 0x");
    BN_print_fp(stdout, q);
    fprintf(stdout, "\n\n");

    /* Output a PEM encoding of the public key. */
    if (PEM_write_PUBKEY(stdout, pkey) == 0) {
        fprintf(stderr, "Failed to output PEM-encoded public key\n");
        goto cleanup;
    }

    /*
     * Output a PEM encoding of the private key. Please note that this output is
     * not encrypted. You may wish to use the arguments to specify encryption of
     * the key if you are storing it on disk. See PEM_write_PrivateKey(3).
     */
    if (PEM_write_PrivateKey(stdout, pkey, NULL, NULL, 0, NULL, NULL) == 0) {
        fprintf(stderr, "Failed to output PEM-encoded private key\n");
        goto cleanup;
    }

    ret = 1;
cleanup:
    BN_free(n); /* not secret */
    BN_free(e); /* not secret */
    BN_clear_free(d); /* secret - scrub before freeing */
    BN_clear_free(p); /* secret - scrub before freeing */
    BN_clear_free(q); /* secret - scrub before freeing */
    return ret;
}

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    OSSL_LIB_CTX *libctx = NULL;
    EVP_PKEY *pkey = NULL;
    unsigned int bits = 4096;
    int bits_i, use_short = 0;

    /* usage: [-s] [<bits>] */
    if (argc > 1 && strcmp(argv[1], "-s") == 0) {
        --argc;
        ++argv;
        use_short = 1;
    }

    if (argc > 1) {
        bits_i = atoi(argv[1]);
        if (bits < 512) {
            fprintf(stderr, "Invalid RSA key size\n");
            return EXIT_FAILURE;
        }

        bits = (unsigned int)bits_i;
    }

    /* Avoid using key sizes less than 2048 bits; see comment at top of file. */
    if (bits < 2048)
        fprintf(stderr, "Warning: very weak key size\n\n");

    /* Generate RSA key. */
    if (use_short)
        pkey = generate_rsa_key_short(libctx, bits);
    else
        pkey = generate_rsa_key_long(libctx, bits);

    if (pkey == NULL)
        goto cleanup;

    /* Dump the integers comprising the key. */
    if (dump_key(pkey) == 0) {
        fprintf(stderr, "Failed to dump key\n");
        goto cleanup;
    }

    ret = EXIT_SUCCESS;
cleanup:
    EVP_PKEY_free(pkey);
    OSSL_LIB_CTX_free(libctx);
    return ret;
}
