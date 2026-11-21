/*
 *  ECDH with curve-optimized implementation multiplexing
 *
 *  Copyright 2016-2018 INRIA and Microsoft Corporation
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of Mbed TLS (https://tls.mbed.org)
 */

#ifndef MBEDTLS_X25519_H
#define MBEDTLS_X25519_H

#ifdef __cplusplus
extern "C" {
#endif

#define MBEDTLS_ECP_TLS_CURVE25519 0x1d
#define MBEDTLS_X25519_KEY_SIZE_BYTES 32

/**
 * Defines the source of the imported EC key.
 */
typedef enum
{
    MBEDTLS_X25519_ECDH_OURS,   /**< Our key. */
    MBEDTLS_X25519_ECDH_THEIRS, /**< The key of the peer. */
} mbedtls_x25519_ecdh_side;

/**
 * \brief           The x25519 context structure.
 */
typedef struct
{
  unsigned char our_secret[MBEDTLS_X25519_KEY_SIZE_BYTES];
  unsigned char peer_point[MBEDTLS_X25519_KEY_SIZE_BYTES];
} mbedtls_x25519_context;

#define MBEDTLS_X25519_CONTEXT_INIT {{0}, {0}}

#if defined(MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS)

/**
 * \brief           This function initializes an x25519 context.
 *
 * \param ctx       The x25519 context to initialize.
 */
void mbedtls_x25519_init( mbedtls_x25519_context *ctx );

/**
 * \brief           This function frees a context.
 *
 * \param ctx       The context to free.
 */
void mbedtls_x25519_free( mbedtls_x25519_context *ctx );

/**
 * \brief           This function generates a public key and a TLS
 *                  ServerKeyExchange payload.
 *
 *                  This is the first function used by a TLS server for x25519.
 *
 *
 * \param ctx       The x25519 context.
 * \param olen      The number of characters written.
 * \param buf       The destination buffer.
 * \param blen      The length of the destination buffer.
 * \param f_rng     The RNG function.
 * \param p_rng     The RNG context.
 *
 * \return          \c 0 on success.
 * \return          An \c MBEDTLS_ERR_ECP_XXX error code on failure.
 */
int mbedtls_x25519_make_params( mbedtls_x25519_context *ctx, size_t *olen,
                        unsigned char *buf, size_t blen,
                        int( *f_rng )(void *, unsigned char *, size_t),
                        void *p_rng );

/**
 * \brief           This function parses and processes a TLS ServerKeyExchange
 *                  payload.
 *
 *
 * \param ctx       The x25519 context.
 * \param buf       The pointer to the start of the input buffer.
 * \param end       The address for one Byte past the end of the buffer.
 *
 * \return          \c 0 on success.
 * \return          An \c MBEDTLS_ERR_ECP_XXX error code on failure.
 *
 */
int mbedtls_x25519_read_params( mbedtls_x25519_context *ctx,
                        const unsigned char **buf, const unsigned char *end );

/**
 * \brief           This function sets up an x25519 context from an EC key.
 *
 *                  It is used by clients and servers in place of the
 *                  ServerKeyEchange for static ECDH, and imports ECDH
 *                  parameters from the EC key information of a certificate.
 *
 * \see             ecp.h
 *
 * \param ctx       The x25519 context to set up.
 * \param key       The EC key to use.
 * \param side      Defines the source of the key: 1: Our key, or
 *                  0: The key of the peer.
 *
 * \return          \c 0 on success.
 * \return          An \c MBEDTLS_ERR_ECP_XXX error code on failure.
 *
 */
int mbedtls_x25519_get_params( mbedtls_x25519_context *ctx, const mbedtls_ecp_keypair *key,
                               mbedtls_x25519_ecdh_side side );

/**
 * \brief           This function derives and exports the shared secret.
 *
 *                  This is the last function used by both TLS client
 *                  and servers.
 *
 *
 * \param ctx       The x25519 context.
 * \param olen      The number of Bytes written.
 * \param buf       The destination buffer.
 * \param blen      The length of the destination buffer.
 * \param f_rng     The RNG function.
 * \param p_rng     The RNG context.
 *
 * \return          \c 0 on success.
 * \return          An \c MBEDTLS_ERR_ECP_XXX error code on failure.
 */
int mbedtls_x25519_calc_secret( mbedtls_x25519_context *ctx, size_t *olen,
                        unsigned char *buf, size_t blen,
                        int( *f_rng )(void *, unsigned char *, size_t),
                        void *p_rng );

/**
 * \brief           This function generates a public key and a TLS
 *                  ClientKeyExchange payload.
 *
 *                  This is the second function used by a TLS client for x25519.
 *
 * \see             ecp.h
 *
 * \param ctx       The x25519 context.
 * \param olen      The number of Bytes written.
 * \param buf       The destination buffer.
 * \param blen      The size of the destination buffer.
 * \param f_rng     The RNG function.
 * \param p_rng     The RNG context.
 *
 * \return          \c 0 on success.
 * \return          An \c MBEDTLS_ERR_ECP_XXX error code on failure.
 */
int mbedtls_x25519_make_public( mbedtls_x25519_context *ctx, size_t *olen,
                        unsigned char *buf, size_t blen,
                        int( *f_rng )(void *, unsigned char *, size_t),
                        void *p_rng );

/**
 * \brief       This function parses and processes a TLS ClientKeyExchange
 *              payload.
 *
 *              This is the second function used by a TLS server for x25519.
 *
 * \see         ecp.h
 *
 * \param ctx   The x25519 context.
 * \param buf   The start of the input buffer.
 * \param blen  The length of the input buffer.
 *
 * \return      \c 0 on success.
 * \return      An \c MBEDTLS_ERR_ECP_XXX error code on failure.
 */
int mbedtls_x25519_read_public( mbedtls_x25519_context *ctx,
                        const unsigned char *buf, size_t blen );

#endif /* MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS */

#ifdef __cplusplus
}
#endif

#endif /* x25519.h */
