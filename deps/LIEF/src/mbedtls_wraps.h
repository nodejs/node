/* Copyright 2021 - 2026 R. Thomas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_MBEDTLS_WRAP_H
#define LIEF_MBEDTLS_WRAP_H
#include <cassert>

// NOLINTBEGIN
// clang-format off

// NOTE(romain) about this file: between version 3.x and version 4.x of MbedTLS,
// some API have been internalize and are no longer accessible from the public
// headers. If `LIEF_OPT_MBEDTLS_EXTERNAL` is not set, we could ship this
// private headers into the zip file given in this repo. Since LIEF also
// aims to provide control to users on how they want to integrate and link
// third-party libraries, this solution is not viable.
//
// Therefore, I choose to backport private elements in this file to ensure that
// this works with both modes: LIEF_OPT_MBEDTLS_EXTERNAL=ON/OFF
// This is not ideal and it introduces an additional cost when upgrading
// MbedTLS.
//
// To ensure that we correctly review this file during a MbedTLS upgrade, I
// static_asserted the version from which the structures, enums have been taken.

#ifndef MBEDTLS_X509_USE_C
#define MBEDTLS_X509_USE_C
#endif

#ifndef MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
#endif


#if defined(__cplusplus)
extern "C" {
#endif

#include <mbedtls/build_info.h>
#include <mbedtls/pk.h>
#include <mbedtls/oid.h>

#if defined(MBEDTLS_THREADING_C)
#include "mbedtls/threading.h"
#endif

static_assert(MBEDTLS_VERSION_NUMBER == 0x04000000, "Expecting mbedtls 4.0.0");

// From: tf-psa-crypto/include/mbedtls/private/pk_private.h
// <pk_private.h>
typedef enum {
    MBEDTLS_PK_NONE=0,
    MBEDTLS_PK_RSA,
    MBEDTLS_PK_ECKEY,
    MBEDTLS_PK_ECKEY_DH,
    MBEDTLS_PK_ECDSA,
    MBEDTLS_PK_RSA_ALT,
    MBEDTLS_PK_RSASSA_PSS,
    MBEDTLS_PK_OPAQUE,
} mbedtls_pk_type_t;

mbedtls_pk_type_t mbedtls_pk_get_type(const mbedtls_pk_context *ctx);
// </pk_private.h>

// From: tf-psa-crypto/drivers/builtin/include/mbedtls/private/rsa.h
// <rsa.h>
typedef struct mbedtls_rsa_context {
    int MBEDTLS_PRIVATE(ver);                    /*!<  Reserved for internal purposes.
                                                  *    Do not set this field in application
                                                  *    code. Its meaning might change without
                                                  *    notice. */
    size_t MBEDTLS_PRIVATE(len);                 /*!<  The size of \p N in Bytes. */

    mbedtls_mpi MBEDTLS_PRIVATE(N);              /*!<  The public modulus. */
    mbedtls_mpi MBEDTLS_PRIVATE(E);              /*!<  The public exponent. */

    mbedtls_mpi MBEDTLS_PRIVATE(D);              /*!<  The private exponent. */
    mbedtls_mpi MBEDTLS_PRIVATE(P);              /*!<  The first prime factor. */
    mbedtls_mpi MBEDTLS_PRIVATE(Q);              /*!<  The second prime factor. */

    mbedtls_mpi MBEDTLS_PRIVATE(DP);             /*!<  <code>D % (P - 1)</code>. */
    mbedtls_mpi MBEDTLS_PRIVATE(DQ);             /*!<  <code>D % (Q - 1)</code>. */
    mbedtls_mpi MBEDTLS_PRIVATE(QP);             /*!<  <code>1 / (Q % P)</code>. */

    mbedtls_mpi MBEDTLS_PRIVATE(RN);             /*!<  cached <code>R^2 mod N</code>. */

    mbedtls_mpi MBEDTLS_PRIVATE(RP);             /*!<  cached <code>R^2 mod P</code>. */
    mbedtls_mpi MBEDTLS_PRIVATE(RQ);             /*!<  cached <code>R^2 mod Q</code>. */

    mbedtls_mpi MBEDTLS_PRIVATE(Vi);             /*!<  The cached blinding value. */
    mbedtls_mpi MBEDTLS_PRIVATE(Vf);             /*!<  The cached un-blinding value. */

    int MBEDTLS_PRIVATE(padding);                /*!< Selects padding mode:
                                                  #MBEDTLS_RSA_PKCS_V15 for 1.5 padding and
                                                  #MBEDTLS_RSA_PKCS_V21 for OAEP or PSS. */
    int MBEDTLS_PRIVATE(hash_id);                /*!< Hash identifier of mbedtls_md_type_t type,
                                                    as specified in md.h for use in the MGF
                                                    mask generating function used in the
                                                    EME-OAEP and EMSA-PSS encodings. */
#if defined(MBEDTLS_THREADING_C)
    /* Invariant: the mutex is initialized iff ver != 0. */
    mbedtls_threading_mutex_t MBEDTLS_PRIVATE(mutex);    /*!<  Thread-safety mutex. */
#endif
} mbedtls_rsa_context;

static inline mbedtls_rsa_context *mbedtls_pk_rsa(const mbedtls_pk_context pk)
{
    switch (mbedtls_pk_get_type(&pk)) {
        case MBEDTLS_PK_RSA:
            return (mbedtls_rsa_context *) (pk).MBEDTLS_PRIVATE(pk_ctx);
        default:
            return NULL;
    }
}

int mbedtls_rsa_parse_pubkey(mbedtls_rsa_context *rsa, const unsigned char *key, size_t keylen);
void mbedtls_rsa_init(mbedtls_rsa_context *ctx);
void mbedtls_rsa_free(mbedtls_rsa_context *ctx);

int mbedtls_rsa_public(mbedtls_rsa_context *ctx,
                       const unsigned char *input,
                       unsigned char *output);

// </rsa.h>

int mbedtls_x509_oid_get_attr_short_name(const mbedtls_asn1_buf *oid, const char **short_name);

inline int rsa_from_pk(mbedtls_pk_context* pk, mbedtls_rsa_context* ctx) {
  assert(ctx != nullptr);
  assert(pk != nullptr);

  mbedtls_rsa_init(ctx);
  return mbedtls_rsa_parse_pubkey(ctx, pk->private_pub_raw,
                                  pk->private_pub_raw_len);
}

#if defined(__cplusplus)
}
#endif

// NOLINTEND
// clang-format on

#endif
