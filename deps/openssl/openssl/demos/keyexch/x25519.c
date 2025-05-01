/*
 * Copyright 2022-2023 The OpenSSL Project Authors. All Rights Reserved.
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

/*
 * This is a demonstration of key exchange using X25519.
 *
 * The variables beginning `peer1_` / `peer2_` are data which would normally be
 * accessible to that peer.
 *
 * Ordinarily you would use random keys, which are demonstrated
 * below when use_kat=0. A known answer test is demonstrated
 * when use_kat=1.
 */

/* A property query used for selecting the X25519 implementation. */
static const char *propq = NULL;

static const unsigned char peer1_privk_data[32] = {
    0x80, 0x5b, 0x30, 0x20, 0x25, 0x4a, 0x70, 0x2c,
    0xad, 0xa9, 0x8d, 0x7d, 0x47, 0xf8, 0x1b, 0x20,
    0x89, 0xd2, 0xf9, 0x14, 0xac, 0x92, 0x27, 0xf2,
    0x10, 0x7e, 0xdb, 0x21, 0xbd, 0x73, 0x73, 0x5d
};

static const unsigned char peer2_privk_data[32] = {
    0xf8, 0x84, 0x19, 0x69, 0x79, 0x13, 0x0d, 0xbd,
    0xb1, 0x76, 0xd7, 0x0e, 0x7e, 0x0f, 0xb6, 0xf4,
    0x8c, 0x4a, 0x8c, 0x5f, 0xd8, 0x15, 0x09, 0x0a,
    0x71, 0x78, 0x74, 0x92, 0x0f, 0x85, 0xc8, 0x43
};

static const unsigned char expected_result[32] = {
    0x19, 0x71, 0x26, 0x12, 0x74, 0xb5, 0xb1, 0xce,
    0x77, 0xd0, 0x79, 0x24, 0xb6, 0x0a, 0x5c, 0x72,
    0x0c, 0xa6, 0x56, 0xc0, 0x11, 0xeb, 0x43, 0x11,
    0x94, 0x3b, 0x01, 0x45, 0xca, 0x19, 0xfe, 0x09
};

typedef struct peer_data_st {
    const char *name;               /* name of peer */
    EVP_PKEY *privk;                /* privk generated for peer */
    unsigned char pubk_data[32];    /* generated pubk to send to other peer */

    unsigned char *secret;          /* allocated shared secret buffer */
    size_t secret_len;
} PEER_DATA;

/*
 * Prepare for X25519 key exchange. The public key to be sent to the remote peer
 * is put in pubk_data, which should be a 32-byte buffer. Returns 1 on success.
 */
static int keyexch_x25519_before(
    OSSL_LIB_CTX *libctx,
    const unsigned char *kat_privk_data,
    PEER_DATA *local_peer)
{
    int ret = 0;
    size_t pubk_data_len = 0;

    /* Generate or load X25519 key for the peer */
    if (kat_privk_data != NULL)
        local_peer->privk =
            EVP_PKEY_new_raw_private_key_ex(libctx, "X25519", propq,
                                            kat_privk_data,
                                            sizeof(peer1_privk_data));
    else
        local_peer->privk = EVP_PKEY_Q_keygen(libctx, propq, "X25519");

    if (local_peer->privk == NULL) {
        fprintf(stderr, "Could not load or generate private key\n");
        goto end;
    }

    /* Get public key corresponding to the private key */
    if (EVP_PKEY_get_octet_string_param(local_peer->privk,
                                        OSSL_PKEY_PARAM_PUB_KEY,
                                        local_peer->pubk_data,
                                        sizeof(local_peer->pubk_data),
                                        &pubk_data_len) == 0) {
        fprintf(stderr, "EVP_PKEY_get_octet_string_param() failed\n");
        goto end;
    }

    /* X25519 public keys are always 32 bytes */
    if (pubk_data_len != 32) {
        fprintf(stderr, "EVP_PKEY_get_octet_string_param() "
                "yielded wrong length\n");
        goto end;
    }

    ret = 1;
end:
    if (ret == 0) {
        EVP_PKEY_free(local_peer->privk);
        local_peer->privk = NULL;
    }

    return ret;
}

/*
 * Complete X25519 key exchange. remote_peer_pubk_data should be the 32 byte
 * public key value received from the remote peer. On success, returns 1 and the
 * secret is pointed to by *secret. The caller must free it.
 */
static int keyexch_x25519_after(
    OSSL_LIB_CTX *libctx,
    int use_kat,
    PEER_DATA *local_peer,
    const unsigned char *remote_peer_pubk_data)
{
    int ret = 0;
    EVP_PKEY *remote_peer_pubk = NULL;
    EVP_PKEY_CTX *ctx = NULL;

    local_peer->secret = NULL;

    /* Load public key for remote peer. */
    remote_peer_pubk =
        EVP_PKEY_new_raw_public_key_ex(libctx, "X25519", propq,
                                       remote_peer_pubk_data, 32);
    if (remote_peer_pubk == NULL) {
        fprintf(stderr, "EVP_PKEY_new_raw_public_key_ex() failed\n");
        goto end;
    }

    /* Create key exchange context. */
    ctx = EVP_PKEY_CTX_new_from_pkey(libctx, local_peer->privk, propq);
    if (ctx == NULL) {
        fprintf(stderr, "EVP_PKEY_CTX_new_from_pkey() failed\n");
        goto end;
    }

    /* Initialize derivation process. */
    if (EVP_PKEY_derive_init(ctx) == 0) {
        fprintf(stderr, "EVP_PKEY_derive_init() failed\n");
        goto end;
    }

    /* Configure each peer with the other peer's public key. */
    if (EVP_PKEY_derive_set_peer(ctx, remote_peer_pubk) == 0) {
        fprintf(stderr, "EVP_PKEY_derive_set_peer() failed\n");
        goto end;
    }

    /* Determine the secret length. */
    if (EVP_PKEY_derive(ctx, NULL, &local_peer->secret_len) == 0) {
        fprintf(stderr, "EVP_PKEY_derive() failed\n");
        goto end;
    }

    /*
     * We are using X25519, so the secret generated will always be 32 bytes.
     * However for exposition, the code below demonstrates a generic
     * implementation for arbitrary lengths.
     */
    if (local_peer->secret_len != 32) { /* unreachable */
        fprintf(stderr, "Secret is always 32 bytes for X25519\n");
        goto end;
    }

    /* Allocate memory for shared secrets. */
    local_peer->secret = OPENSSL_malloc(local_peer->secret_len);
    if (local_peer->secret == NULL) {
        fprintf(stderr, "Could not allocate memory for secret\n");
        goto end;
    }

    /* Derive the shared secret. */
    if (EVP_PKEY_derive(ctx, local_peer->secret,
                        &local_peer->secret_len) == 0) {
        fprintf(stderr, "EVP_PKEY_derive() failed\n");
        goto end;
    }

    printf("Shared secret (%s):\n", local_peer->name);
    BIO_dump_indent_fp(stdout, local_peer->secret, local_peer->secret_len, 2);
    putchar('\n');

    ret = 1;
end:
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(remote_peer_pubk);
    if (ret == 0) {
        OPENSSL_clear_free(local_peer->secret, local_peer->secret_len);
        local_peer->secret = NULL;
    }

    return ret;
}

static int keyexch_x25519(int use_kat)
{
    int ret = 0;
    OSSL_LIB_CTX *libctx = NULL;
    PEER_DATA peer1 = {"peer 1"}, peer2 = {"peer 2"};

    /*
     * Each peer generates its private key and sends its public key
     * to the other peer. The private key is stored locally for
     * later use.
     */
    if (keyexch_x25519_before(libctx, use_kat ? peer1_privk_data : NULL,
                              &peer1) == 0)
        return 0;

    if (keyexch_x25519_before(libctx, use_kat ? peer2_privk_data : NULL,
                              &peer2) == 0)
        return 0;

    /*
     * Each peer uses the other peer's public key to perform key exchange.
     * After this succeeds, each peer has the same secret in its
     * PEER_DATA.
     */
    if (keyexch_x25519_after(libctx, use_kat, &peer1, peer2.pubk_data) == 0)
        return 0;

    if (keyexch_x25519_after(libctx, use_kat, &peer2, peer1.pubk_data) == 0)
        return 0;

    /*
     * Here we demonstrate the secrets are equal for exposition purposes.
     *
     * Although in practice you will generally not need to compare secrets
     * produced through key exchange, if you do compare cryptographic secrets,
     * always do so using a constant-time function such as CRYPTO_memcmp, never
     * using memcmp(3).
     */
    if (CRYPTO_memcmp(peer1.secret, peer2.secret, peer1.secret_len) != 0) {
        fprintf(stderr, "Negotiated secrets do not match\n");
        goto end;
    }

    /* If we are doing the KAT, the secret should equal our reference result. */
    if (use_kat && CRYPTO_memcmp(peer1.secret, expected_result,
                                 peer1.secret_len) != 0) {
        fprintf(stderr, "Did not get expected result\n");
        goto end;
    }

    ret = 1;
end:
    /* The secrets are sensitive, so ensure they are erased before freeing. */
    OPENSSL_clear_free(peer1.secret, peer1.secret_len);
    OPENSSL_clear_free(peer2.secret, peer2.secret_len);

    EVP_PKEY_free(peer1.privk);
    EVP_PKEY_free(peer2.privk);
    OSSL_LIB_CTX_free(libctx);
    return ret;
}

int main(int argc, char **argv)
{
    /* Test X25519 key exchange with known result. */
    printf("Key exchange using known answer (deterministic):\n");
    if (keyexch_x25519(1) == 0)
        return EXIT_FAILURE;

    /* Test X25519 key exchange with random keys. */
    printf("Key exchange using random keys:\n");
    if (keyexch_x25519(0) == 0)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
