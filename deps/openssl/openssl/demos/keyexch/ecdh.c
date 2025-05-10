/*
 * Copyright 2023-2024 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/err.h>

/*
 * This is a demonstration of key exchange using ECDH.
 *
 * EC key exchange requires 2 parties (peers) to first agree on shared group
 * parameters (the EC curve name). Each peer then generates a public/private
 * key pair using the shared curve name. Each peer then gives their public key
 * to the other peer. A peer can then derive the same shared secret using their
 * private key and the other peers public key.
 */

/* Object used to store information for a single Peer */
typedef struct peer_data_st {
    const char *name;               /* name of peer */
    const char *curvename;          /* The shared curve name */
    EVP_PKEY *priv;                 /* private keypair */
    EVP_PKEY *pub;                  /* public key to send to other peer */
    unsigned char *secret;          /* allocated shared secret buffer */
    size_t secretlen;
} PEER_DATA;

/*
 * The public key needs to be given to the other peer
 * The following code extracts the public key data from the private key
 * and then builds an EVP_KEY public key.
 */
static int get_peer_public_key(PEER_DATA *peer, OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    EVP_PKEY_CTX *ctx;
    OSSL_PARAM params[3];
    unsigned char pubkeydata[256];
    size_t pubkeylen;

    /* Get the EC encoded public key data from the peers private key */
    if (!EVP_PKEY_get_octet_string_param(peer->priv, OSSL_PKEY_PARAM_PUB_KEY,
                                         pubkeydata, sizeof(pubkeydata),
                                         &pubkeylen))
        return 0;

    /* Create a EC public key from the public key data */
    ctx = EVP_PKEY_CTX_new_from_name(libctx, "EC", NULL);
    if (ctx == NULL)
        return 0;
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                 (char *)peer->curvename, 0);
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                                  pubkeydata, pubkeylen);
    params[2] = OSSL_PARAM_construct_end();
    ret = EVP_PKEY_fromdata_init(ctx) > 0
          && (EVP_PKEY_fromdata(ctx, &peer->pub, EVP_PKEY_PUBLIC_KEY,
                                params) > 0);
    EVP_PKEY_CTX_free(ctx);
    return ret;
}

static int create_peer(PEER_DATA *peer, OSSL_LIB_CTX *libctx)
{
    int ret = 0;
    EVP_PKEY_CTX *ctx = NULL;
    OSSL_PARAM params[2];

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                                 (char *)peer->curvename, 0);
    params[1] = OSSL_PARAM_construct_end();

    ctx = EVP_PKEY_CTX_new_from_name(libctx, "EC", NULL);
    if (ctx == NULL)
        return 0;

    if (EVP_PKEY_keygen_init(ctx) <= 0
            || !EVP_PKEY_CTX_set_params(ctx, params)
            || EVP_PKEY_generate(ctx, &peer->priv) <= 0
            || !get_peer_public_key(peer, libctx)) {
        EVP_PKEY_free(peer->priv);
        peer->priv = NULL;
        goto err;
    }
    ret = 1;
err:
    EVP_PKEY_CTX_free(ctx);
    return ret;
}

static void destroy_peer(PEER_DATA *peer)
{
    EVP_PKEY_free(peer->priv);
    EVP_PKEY_free(peer->pub);
}

static int generate_secret(PEER_DATA *peerA, EVP_PKEY *peerBpub,
                           OSSL_LIB_CTX *libctx)
{
    unsigned char *secret = NULL;
    size_t secretlen = 0;
    EVP_PKEY_CTX *derivectx;

    /* Create an EVP_PKEY_CTX that contains peerA's private key */
    derivectx = EVP_PKEY_CTX_new_from_pkey(libctx, peerA->priv, NULL);
    if (derivectx == NULL)
        return 0;

    if (EVP_PKEY_derive_init(derivectx) <= 0)
        goto cleanup;
    /* Set up peerB's public key */
    if (EVP_PKEY_derive_set_peer(derivectx, peerBpub) <= 0)
        goto cleanup;

    /*
     * For backwards compatibility purposes the OpenSSL ECDH provider supports
     * optionally using a X963KDF to expand the secret data. This can be done
     * with code similar to the following.
     *
     *   OSSL_PARAM params[5];
     *   size_t outlen = 128;
     *   unsigned char ukm[] = { 1, 2, 3, 4 };
     *   params[0] = OSSL_PARAM_construct_utf8_string(OSSL_EXCHANGE_PARAM_KDF_TYPE,
     *                                                "X963KDF", 0);
     *   params[1] = OSSL_PARAM_construct_utf8_string(OSSL_EXCHANGE_PARAM_KDF_DIGEST,
     *                                                "SHA256", 0);
     *   params[2] = OSSL_PARAM_construct_size_t(OSSL_EXCHANGE_PARAM_KDF_OUTLEN,
     *                                           &outlen);
     *   params[3] = OSSL_PARAM_construct_octet_string(OSSL_EXCHANGE_PARAM_KDF_UKM,
     *                                                 ukm, sizeof(ukm));
     *   params[4] = OSSL_PARAM_construct_end();
     *   if (!EVP_PKEY_CTX_set_params(derivectx, params))
     *       goto cleanup;
     *
     * Note: After the secret is generated below, the peer could alternatively
     * pass the secret to a KDF to derive additional key data from the secret.
     * See demos/kdf/hkdf.c for an example (where ikm is the secret key)
     */

    /* Calculate the size of the secret and allocate space */
    if (EVP_PKEY_derive(derivectx, NULL, &secretlen) <= 0)
        goto cleanup;
    secret = (unsigned char *)OPENSSL_malloc(secretlen);
    if (secret == NULL)
        goto cleanup;

    /*
     * Derive the shared secret. In this example 32 bytes are generated.
     * For EC curves the secret size is related to the degree of the curve
     * which is 256 bits for P-256.
     */
    if (EVP_PKEY_derive(derivectx, secret, &secretlen) <= 0)
        goto cleanup;
    peerA->secret = secret;
    peerA->secretlen = secretlen;

    printf("Shared secret (%s):\n", peerA->name);
    BIO_dump_indent_fp(stdout, peerA->secret, peerA->secretlen, 2);
    putchar('\n');

    return 1;
cleanup:
    OPENSSL_free(secret);
    EVP_PKEY_CTX_free(derivectx);
    return 0;
}

int main(void)
{
    int ret = EXIT_FAILURE;
    /* Initialise the 2 peers that will share a secret */
    PEER_DATA peer1 = {"peer 1", "P-256"};
    PEER_DATA peer2 = {"peer 2", "P-256"};
    /*
     * Setting libctx to NULL uses the default library context
     * Use OSSL_LIB_CTX_new() to create a non default library context
     */
    OSSL_LIB_CTX *libctx = NULL;

    /* Each peer creates a (Ephemeral) keypair */
    if (!create_peer(&peer1, libctx)
            || !create_peer(&peer2, libctx)) {
        fprintf(stderr, "Create peer failed\n");
        goto cleanup;
    }

    /*
     * Each peer uses its private key and the other peers public key to
     * derive a shared secret
     */
    if (!generate_secret(&peer1, peer2.pub, libctx)
            || !generate_secret(&peer2, peer1.pub, libctx)) {
        fprintf(stderr, "Generate secrets failed\n");
        goto cleanup;
    }

    /* For illustrative purposes demonstrate that the derived secrets are equal */
    if (peer1.secretlen != peer2.secretlen
            || CRYPTO_memcmp(peer1.secret, peer2.secret, peer1.secretlen) != 0) {
        fprintf(stderr, "Derived secrets do not match\n");
        goto cleanup;
    } else {
        fprintf(stdout, "Derived secrets match\n");
    }

    ret = EXIT_SUCCESS;
cleanup:
    if (ret != EXIT_SUCCESS)
        ERR_print_errors_fp(stderr);
    destroy_peer(&peer2);
    destroy_peer(&peer1);
    return ret;
}
