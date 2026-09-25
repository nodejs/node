/**
 * \file psa/crypto_platform.h
 *
 * \brief PSA cryptography module: Mbed TLS platform definitions
 *
 * \note This file may not be included directly. Applications must
 * include psa/crypto.h.
 *
 * This file contains platform-dependent type definitions.
 *
 * In implementations with isolation between the application and the
 * cryptography module, implementers should take care to ensure that
 * the definitions that are exposed to applications match what the
 * module implements.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_PLATFORM_H
#define PSA_CRYPTO_PLATFORM_H
#include "mbedtls/private_access.h"

/*
 * Include the build-time configuration information header. Here, we do not
 * include `"mbedtls/build_info.h"` directly but `"psa/build_info.h"`, which
 * is basically just an alias to it. This is to ease the maintenance of the
 * TF-PSA-Crypto repository which has a different build system and
 * configuration.
 */
#include "psa/build_info.h"

/* PSA requires several types which C99 provides in stdint.h. */
#include <stdint.h>

#if defined(MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER)

/* Building for the PSA Crypto service on a PSA platform, a key owner is a PSA
 * partition identifier.
 *
 * The function psa_its_identifier_of_slot() in psa_crypto_storage.c that
 * translates a key identifier to a key storage file name assumes that
 * mbedtls_key_owner_id_t is a 32-bit integer. This function thus needs
 * reworking if mbedtls_key_owner_id_t is not defined as a 32-bit integer
 * here anymore.
 */
typedef int32_t mbedtls_key_owner_id_t;

/** Compare two key owner identifiers.
 *
 * \param id1 First key owner identifier.
 * \param id2 Second key owner identifier.
 *
 * \return Non-zero if the two key owner identifiers are equal, zero otherwise.
 */
static inline int mbedtls_key_owner_id_equal(mbedtls_key_owner_id_t id1,
                                             mbedtls_key_owner_id_t id2)
{
    return id1 == id2;
}

#endif /* MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER */

/*
 * When MBEDTLS_PSA_CRYPTO_SPM is defined, the code is being built for SPM
 * (Secure Partition Manager) integration which separates the code into two
 * parts: NSPE (Non-Secure Processing Environment) and SPE (Secure Processing
 * Environment). When building for the SPE, an additional header file should be
 * included.
 */
#if defined(MBEDTLS_PSA_CRYPTO_SPM)
#define PSA_CRYPTO_SECURE 1
#include "crypto_spe.h"
#endif // MBEDTLS_PSA_CRYPTO_SPM

#if defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
/** The type of the context passed to mbedtls_psa_external_get_random().
 *
 * Mbed TLS initializes the context to all-bits-zero before calling
 * mbedtls_psa_external_get_random() for the first time.
 *
 * The definition of this type in the Mbed TLS source code is for
 * demonstration purposes. Implementers of mbedtls_psa_external_get_random()
 * are expected to replace it with a custom definition.
 */
typedef struct {
    uintptr_t MBEDTLS_PRIVATE(opaque)[2];
} mbedtls_psa_external_random_context_t;
#endif /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT) && !defined(MBEDTLS_PSA_CRYPTO_C)
/** The type of the client handle used in context structures
 *
 * When a client view of the multipart context structures is required,
 * this handle is used to keep a mapping with the service side of the
 * context which contains the actual data.
 */
typedef uint32_t mbedtls_psa_client_handle_t;
#endif

#endif /* PSA_CRYPTO_PLATFORM_H */
