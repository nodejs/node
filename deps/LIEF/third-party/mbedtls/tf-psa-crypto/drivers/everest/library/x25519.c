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

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_ECDH_C) && defined(MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED)

#include <mbedtls/private/ecdh.h>

#if !(defined(__SIZEOF_INT128__) && (__SIZEOF_INT128__ == 16))
#define KRML_VERIFIED_UINT128
#endif

#include <Hacl_Curve25519.h>
#include <mbedtls/platform_util.h>

#include "x25519.h"

#include <string.h>

/*
 * Initialize context
 */
void mbedtls_x25519_init( mbedtls_x25519_context *ctx )
{
    mbedtls_platform_zeroize( ctx, sizeof( mbedtls_x25519_context ) );
}

/*
 * Free context
 */
void mbedtls_x25519_free( mbedtls_x25519_context *ctx )
{
    if( ctx == NULL )
        return;

    mbedtls_platform_zeroize( ctx->our_secret, MBEDTLS_X25519_KEY_SIZE_BYTES );
    mbedtls_platform_zeroize( ctx->peer_point, MBEDTLS_X25519_KEY_SIZE_BYTES );
}

int mbedtls_x25519_make_params( mbedtls_x25519_context *ctx, size_t *olen,
                        unsigned char *buf, size_t blen,
                        int( *f_rng )(void *, unsigned char *, size_t),
                        void *p_rng )
{
    int ret = 0;

    uint8_t base[MBEDTLS_X25519_KEY_SIZE_BYTES] = {0};

    if( ( ret = f_rng( p_rng, ctx->our_secret, MBEDTLS_X25519_KEY_SIZE_BYTES ) ) != 0 )
        return ret;

    *olen = MBEDTLS_X25519_KEY_SIZE_BYTES + 4;
    if( blen < *olen )
        return( MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL );

    *buf++ = MBEDTLS_ECP_TLS_NAMED_CURVE;
    *buf++ = MBEDTLS_ECP_TLS_CURVE25519 >> 8;
    *buf++ = MBEDTLS_ECP_TLS_CURVE25519 & 0xFF;
    *buf++ = MBEDTLS_X25519_KEY_SIZE_BYTES;

    base[0] = 9;
    Hacl_Curve25519_crypto_scalarmult( buf, ctx->our_secret, base );

    base[0] = 0;
    if( memcmp( buf, base, MBEDTLS_X25519_KEY_SIZE_BYTES) == 0 )
        return MBEDTLS_ERR_ECP_RANDOM_FAILED;

    return( 0 );
}

int mbedtls_x25519_read_params( mbedtls_x25519_context *ctx,
                        const unsigned char **buf, const unsigned char *end )
{
    if( end - *buf < MBEDTLS_X25519_KEY_SIZE_BYTES + 1 )
        return( MBEDTLS_ERR_ECP_BAD_INPUT_DATA );

    if( ( *(*buf)++ != MBEDTLS_X25519_KEY_SIZE_BYTES ) )
        return( MBEDTLS_ERR_ECP_BAD_INPUT_DATA );

    memcpy( ctx->peer_point, *buf, MBEDTLS_X25519_KEY_SIZE_BYTES );
    *buf += MBEDTLS_X25519_KEY_SIZE_BYTES;
    return( 0 );
}

int mbedtls_x25519_get_params( mbedtls_x25519_context *ctx, const mbedtls_ecp_keypair *key,
                               mbedtls_x25519_ecdh_side side )
{
    size_t olen = 0;

    switch( side ) {
    case MBEDTLS_X25519_ECDH_THEIRS:
        return mbedtls_ecp_point_write_binary( &key->grp, &key->Q, MBEDTLS_ECP_PF_COMPRESSED, &olen, ctx->peer_point, MBEDTLS_X25519_KEY_SIZE_BYTES );
    case MBEDTLS_X25519_ECDH_OURS:
        return mbedtls_mpi_write_binary_le( &key->d, ctx->our_secret, MBEDTLS_X25519_KEY_SIZE_BYTES );
    default:
        return( MBEDTLS_ERR_ECP_BAD_INPUT_DATA );
    }
}

int mbedtls_x25519_calc_secret( mbedtls_x25519_context *ctx, size_t *olen,
                        unsigned char *buf, size_t blen,
                        int( *f_rng )(void *, unsigned char *, size_t),
                        void *p_rng )
{
    /* f_rng and p_rng are not used here because this implementation does not
       need blinding since it has constant trace. */
    (( void )f_rng);
    (( void )p_rng);

    *olen = MBEDTLS_X25519_KEY_SIZE_BYTES;

    if( blen < *olen )
        return( MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL );

    Hacl_Curve25519_crypto_scalarmult( buf, ctx->our_secret, ctx->peer_point);

    /* Wipe the DH secret and don't let the peer chose a small subgroup point */
    mbedtls_platform_zeroize( ctx->our_secret, MBEDTLS_X25519_KEY_SIZE_BYTES );

    if( memcmp( buf, ctx->our_secret, MBEDTLS_X25519_KEY_SIZE_BYTES) == 0 )
        return MBEDTLS_ERR_ECP_RANDOM_FAILED;

    return( 0 );
}

int mbedtls_x25519_make_public( mbedtls_x25519_context *ctx, size_t *olen,
                        unsigned char *buf, size_t blen,
                        int( *f_rng )(void *, unsigned char *, size_t),
                        void *p_rng )
{
    int ret = 0;
    unsigned char base[MBEDTLS_X25519_KEY_SIZE_BYTES] = { 0 };

    if( ctx == NULL )
        return( MBEDTLS_ERR_ECP_BAD_INPUT_DATA );

    if( ( ret = f_rng( p_rng, ctx->our_secret, MBEDTLS_X25519_KEY_SIZE_BYTES ) ) != 0 )
        return ret;

    *olen = MBEDTLS_X25519_KEY_SIZE_BYTES + 1;
    if( blen < *olen )
        return(MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL);
    *buf++ = MBEDTLS_X25519_KEY_SIZE_BYTES;

    base[0] = 9;
    Hacl_Curve25519_crypto_scalarmult( buf, ctx->our_secret, base );

    base[0] = 0;
    if( memcmp( buf, base, MBEDTLS_X25519_KEY_SIZE_BYTES ) == 0 )
        return MBEDTLS_ERR_ECP_RANDOM_FAILED;

    return( ret );
}

int mbedtls_x25519_read_public( mbedtls_x25519_context *ctx,
                        const unsigned char *buf, size_t blen )
{
    if( blen < MBEDTLS_X25519_KEY_SIZE_BYTES + 1 )
        return(MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL);
    if( (*buf++ != MBEDTLS_X25519_KEY_SIZE_BYTES) )
        return(MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
    memcpy( ctx->peer_point, buf, MBEDTLS_X25519_KEY_SIZE_BYTES );
    return( 0 );
}


#endif /* MBEDTLS_ECDH_C && MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED */
