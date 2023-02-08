/*-
 * Copyright 2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include <string.h>
#include <openssl/decoder.h>
#include <openssl/encoder.h>
#include <openssl/evp.h>

/*
 * Example showing the encoding and decoding of RSA public and private keys. A
 * PEM-encoded RSA key is read in from stdin, decoded, and then re-encoded and
 * output for demonstration purposes. Both public and private keys are accepted.
 *
 * This can be used to load RSA keys from a file or save RSA keys to a file.
 */

/* A property query used for selecting algorithm implementations. */
static const char *propq = NULL;

/*
 * Load a PEM-encoded RSA key from a file, optionally decrypting it with a
 * supplied passphrase.
 */
static EVP_PKEY *load_key(OSSL_LIB_CTX *libctx, FILE *f, const char *passphrase)
{
    int rv = 0;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *dctx = NULL;
    int selection = 0;

    /*
     * Create PEM decoder context expecting an RSA key.
     *
     * For raw (non-PEM-encoded) keys, change "PEM" to "DER".
     *
     * The selection argument here specifies whether we are willing to accept a
     * public key, private key, or either. If it is set to zero, either will be
     * accepted. If set to EVP_PKEY_KEYPAIR, a private key will be required, and
     * if set to EVP_PKEY_PUBLIC_KEY, a public key will be required.
     */
    dctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, "PEM", NULL, "RSA",
                                         selection,
                                         libctx, propq);
    if (dctx == NULL) {
        fprintf(stderr, "OSSL_DECODER_CTX_new_for_pkey() failed\n");
        goto cleanup;
    }

    /*
     * Set passphrase if provided; needed to decrypt encrypted PEM files.
     * If the input is not encrypted, any passphrase provided is ignored.
     *
     * Alternative methods for specifying passphrases exist, such as a callback
     * (see OSSL_DECODER_CTX_set_passphrase_cb(3)), which may be more useful for
     * interactive applications which do not know if a passphrase should be
     * prompted for in advance, or for GUI applications.
     */
    if (passphrase != NULL) {
        if (OSSL_DECODER_CTX_set_passphrase(dctx,
                                            (const unsigned char *)passphrase,
                                            strlen(passphrase)) == 0) {
            fprintf(stderr, "OSSL_DECODER_CTX_set_passphrase() failed\n");
            goto cleanup;
        }
    }

    /* Do the decode, reading from file. */
    if (OSSL_DECODER_from_fp(dctx, f) == 0) {
        fprintf(stderr, "OSSL_DECODER_from_fp() failed\n");
        goto cleanup;
    }

    rv = 1;
cleanup:
    OSSL_DECODER_CTX_free(dctx);

    /*
     * pkey is created by OSSL_DECODER_CTX_new_for_pkey, but we
     * might fail subsequently, so ensure it's properly freed
     * in this case.
     */
    if (rv == 0) {
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }

    return pkey;
}

/*
 * Store an RSA public or private key to a file using PEM encoding.
 *
 * If a passphrase is supplied, the file is encrypted, otherwise
 * it is unencrypted.
 */
static int store_key(EVP_PKEY *pkey, FILE *f, const char *passphrase)
{
    int rv = 0;
    int selection;
    OSSL_ENCODER_CTX *ectx = NULL;

    /*
     * Create a PEM encoder context.
     *
     * For raw (non-PEM-encoded) output, change "PEM" to "DER".
     *
     * The selection argument controls whether the private key is exported
     * (EVP_PKEY_KEYPAIR), or only the public key (EVP_PKEY_PUBLIC_KEY). The
     * former will fail if we only have a public key.
     *
     * Note that unlike the decode API, you cannot specify zero here.
     *
     * Purely for the sake of demonstration, here we choose to export the whole
     * key if a passphrase is provided and the public key otherwise.
     */
    selection = (passphrase != NULL)
        ? EVP_PKEY_KEYPAIR
        : EVP_PKEY_PUBLIC_KEY;

    ectx = OSSL_ENCODER_CTX_new_for_pkey(pkey, selection, "PEM", NULL, propq);
    if (ectx == NULL) {
        fprintf(stderr, "OSSL_ENCODER_CTX_new_for_pkey() failed\n");
        goto cleanup;
    }

    /*
     * Set passphrase if provided; the encoded output will then be encrypted
     * using the passphrase.
     *
     * Alternative methods for specifying passphrases exist, such as a callback
     * (see OSSL_ENCODER_CTX_set_passphrase_cb(3), just as for OSSL_DECODER_CTX;
     * however you are less likely to need them as you presumably know whether
     * encryption is desired in advance.
     *
     * Note that specifying a passphrase alone is not enough to cause the
     * key to be encrypted. You must set both a cipher and a passphrase.
     */
    if (passphrase != NULL) {
        /* Set cipher. AES-128-CBC is a reasonable default. */
        if (OSSL_ENCODER_CTX_set_cipher(ectx, "AES-128-CBC", propq) == 0) {
            fprintf(stderr, "OSSL_ENCODER_CTX_set_cipher() failed\n");
            goto cleanup;
        }

        /* Set passphrase. */
        if (OSSL_ENCODER_CTX_set_passphrase(ectx,
                                            (const unsigned char *)passphrase,
                                            strlen(passphrase)) == 0) {
            fprintf(stderr, "OSSL_ENCODER_CTX_set_passphrase() failed\n");
            goto cleanup;
        }
    }

    /* Do the encode, writing to the given file. */
    if (OSSL_ENCODER_to_fp(ectx, f) == 0) {
        fprintf(stderr, "OSSL_ENCODER_to_fp() failed\n");
        goto cleanup;
    }

    rv = 1;
cleanup:
    OSSL_ENCODER_CTX_free(ectx);
    return rv;
}

int main(int argc, char **argv)
{
    int rv = 1;
    OSSL_LIB_CTX *libctx = NULL;
    EVP_PKEY *pkey = NULL;
    const char *passphrase_in = NULL, *passphrase_out = NULL;

    /* usage: rsa_encode <passphrase-in> <passphrase-out> */
    if (argc > 1 && argv[1][0])
        passphrase_in = argv[1];

    if (argc > 2 && argv[2][0])
        passphrase_out = argv[2];

    /* Decode PEM key from stdin and then PEM encode it to stdout. */
    pkey = load_key(libctx, stdin, passphrase_in);
    if (pkey == NULL) {
        fprintf(stderr, "Failed to decode key\n");
        goto cleanup;
    }

    if (store_key(pkey, stdout, passphrase_out) == 0) {
        fprintf(stderr, "Failed to encode key\n");
        goto cleanup;
    }

    rv = 0;
cleanup:
    EVP_PKEY_free(pkey);
    OSSL_LIB_CTX_free(libctx);
    return rv;
}
