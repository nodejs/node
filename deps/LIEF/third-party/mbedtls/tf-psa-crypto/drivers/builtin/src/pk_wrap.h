/**
 * \file pk_wrap.h
 *
 * \brief Public Key abstraction layer: wrapper functions
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_PK_WRAP_H
#define MBEDTLS_PK_WRAP_H

#include "tf-psa-crypto/build_info.h"

#include "mbedtls/pk.h"
#if defined(MBEDTLS_PK_HAVE_PRIVATE_HEADER)
#include <mbedtls/private/pk_private.h>
#endif /* MBEDTLS_PK_HAVE_PRIVATE_HEADER */

#include "psa/crypto.h"

typedef enum {
    MBEDTLS_PK_RS_OP_VERIFY,
    MBEDTLS_PK_RS_OP_SIGN,
} mbedtls_pk_rs_op_t;

typedef struct {
    mbedtls_pk_rs_op_t op_type;
    void *op;
    mbedtls_svc_key_id_t pub_id;
} mbedtls_pk_psa_restartable_ctx_t;

struct mbedtls_pk_info_t {
    /** Public key type */
    mbedtls_pk_type_t type;

    /** Type name */
    const char *name;

    /** Get key size in bits */
    size_t (*get_bitlen)(mbedtls_pk_context *pk);

    /** Tell if the context implements this type (e.g. ECKEY can do ECDSA) */
    int (*can_do)(mbedtls_pk_type_t type);

    /** Verify signature */
    int (*verify_func)(mbedtls_pk_context *pk, mbedtls_md_type_t md_alg,
                       const unsigned char *hash, size_t hash_len,
                       const unsigned char *sig, size_t sig_len);

    /** Make signature */
    int (*sign_func)(mbedtls_pk_context *pk, mbedtls_md_type_t md_alg,
                     const unsigned char *hash, size_t hash_len,
                     unsigned char *sig, size_t sig_size, size_t *sig_len);

#if defined(MBEDTLS_ECP_RESTARTABLE)
    /** Verify signature (restartable) */
    int (*verify_rs_func)(mbedtls_pk_context *pk, mbedtls_md_type_t md_alg,
                          const unsigned char *hash, size_t hash_len,
                          const unsigned char *sig, size_t sig_len,
                          void *rs_ctx);

    /** Make signature (restartable) */
    int (*sign_rs_func)(mbedtls_pk_context *pk, mbedtls_md_type_t md_alg,
                        const unsigned char *hash, size_t hash_len,
                        unsigned char *sig, size_t sig_size, size_t *sig_len,
                        void *rs_ctx);
#endif /* MBEDTLS_ECP_RESTARTABLE */

    /** Check public-private key pair */
    int (*check_pair_func)(mbedtls_pk_context *pub, mbedtls_pk_context *prv);

    /** Allocate a new context */
    void * (*ctx_alloc_func)(void);

    /** Free the given context */
    void (*ctx_free_func)(void *ctx);

#if defined(MBEDTLS_ECP_RESTARTABLE)
    /** Allocate the restart context */
    void *(*rs_alloc_func)(mbedtls_pk_rs_op_t op_type);

    /** Free the restart context */
    void (*rs_free_func)(void *rs_ctx);
#endif /* MBEDTLS_ECP_RESTARTABLE */

    /** Interface with the debug module */
    void (*debug_func)(mbedtls_pk_context *pk, mbedtls_pk_debug_item *items);

};
#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
extern const mbedtls_pk_info_t mbedtls_rsa_info;
#endif

#if defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
extern const mbedtls_pk_info_t mbedtls_eckey_info;
extern const mbedtls_pk_info_t mbedtls_eckeydh_info;
#endif

#if defined(PSA_HAVE_ALG_SOME_ECDSA)
extern const mbedtls_pk_info_t mbedtls_ecdsa_info;
#endif

extern const mbedtls_pk_info_t mbedtls_ecdsa_opaque_info;
extern const mbedtls_pk_info_t mbedtls_rsa_opaque_info;

#if defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY)
int mbedtls_pk_psa_rsa_sign_ext(psa_algorithm_t psa_alg_md,
                                mbedtls_pk_context *pk,
                                const unsigned char *hash, size_t hash_len,
                                unsigned char *sig, size_t sig_size,
                                size_t *sig_len);
#endif /* PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY */

#endif /* MBEDTLS_PK_WRAP_H */
