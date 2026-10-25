/*
 *  Interface to code from Project Everest
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
 *  This file is part of Mbed TLS (https://tls.mbed.org).
 */

#include "tf_psa_crypto_common.h"

#include <string.h>

#include "mbedtls/private/ecdh.h"

#include "x25519.h"
#include "everest.h"

#include "mbedtls/platform.h"

#if defined(MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED)

int mbedtls_everest_setup( mbedtls_ecdh_context_everest *ctx, int grp_id )
{
    if( grp_id != MBEDTLS_ECP_DP_CURVE25519 )
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    mbedtls_x25519_init( &ctx->ctx );
    return 0;
}

void mbedtls_everest_free( mbedtls_ecdh_context_everest *ctx )
{
    mbedtls_x25519_free( &ctx->ctx );
}

int mbedtls_everest_make_params( mbedtls_ecdh_context_everest *ctx, size_t *olen,
                                 unsigned char *buf, size_t blen,
                                 int( *f_rng )( void *, unsigned char *, size_t ),
                                 void *p_rng )
{
    mbedtls_x25519_context *x25519_ctx = &ctx->ctx;
    return mbedtls_x25519_make_params( x25519_ctx, olen, buf, blen, f_rng, p_rng );
}

int mbedtls_everest_read_params( mbedtls_ecdh_context_everest *ctx,
                                 const unsigned char **buf,
                                 const unsigned char *end )
{
    mbedtls_x25519_context *x25519_ctx = &ctx->ctx;
    return mbedtls_x25519_read_params( x25519_ctx, buf, end );
}

int mbedtls_everest_get_params( mbedtls_ecdh_context_everest *ctx,
                                const mbedtls_ecp_keypair *key,
                                mbedtls_everest_ecdh_side side )
{
    mbedtls_x25519_context *x25519_ctx = &ctx->ctx;
    mbedtls_x25519_ecdh_side s = side == MBEDTLS_EVEREST_ECDH_OURS ?
                                            MBEDTLS_X25519_ECDH_OURS :
                                            MBEDTLS_X25519_ECDH_THEIRS;
    return mbedtls_x25519_get_params( x25519_ctx, key, s );
}

int mbedtls_everest_make_public( mbedtls_ecdh_context_everest *ctx, size_t *olen,
                                 unsigned char *buf, size_t blen,
                                 int( *f_rng )( void *, unsigned char *, size_t ),
                                 void *p_rng )
{
    mbedtls_x25519_context *x25519_ctx = &ctx->ctx;
    return mbedtls_x25519_make_public( x25519_ctx, olen, buf, blen, f_rng, p_rng );
}

int mbedtls_everest_read_public( mbedtls_ecdh_context_everest *ctx,
                                 const unsigned char *buf, size_t blen )
{
    mbedtls_x25519_context *x25519_ctx = &ctx->ctx;
    return mbedtls_x25519_read_public ( x25519_ctx, buf, blen );
}

int mbedtls_everest_calc_secret( mbedtls_ecdh_context_everest *ctx, size_t *olen,
                                 unsigned char *buf, size_t blen,
                                 int( *f_rng )( void *, unsigned char *, size_t ),
                                 void *p_rng )
{
    mbedtls_x25519_context *x25519_ctx = &ctx->ctx;
    return mbedtls_x25519_calc_secret( x25519_ctx, olen, buf, blen, f_rng, p_rng );
}

#endif /* MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED */
