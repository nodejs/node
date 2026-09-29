/*
 *  Platform-specific and custom entropy polling functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "tf_psa_crypto_common.h"

#include <string.h>

#if defined(MBEDTLS_ENTROPY_C)

#include "mbedtls/platform.h"
#include "mbedtls/private/entropy.h"
#include "entropy_poll.h"
#include "mbedtls/private/error_common.h"
#include "mbedtls/private/error_common.h"
#include <psa/crypto_driver_random.h>

/* In principle, we could support both a built-in source and a custom
 * source. However, it isn't a common need. So for now the two
 * callback functions have the same name and there can only be one. */
#if defined(MBEDTLS_PSA_BUILTIN_GET_ENTROPY) || defined(MBEDTLS_PSA_DRIVER_GET_ENTROPY)

/* We currently only support a single "true" entropy source (other than the
 * "fake" source which is the NV seed). It can be either the built-in one
 * or a user-provided callback. */
#if defined(MBEDTLS_PSA_DRIVER_GET_ENTROPY) && defined(MBEDTLS_PSA_BUILTIN_GET_ENTROPY)
#error "MBEDTLS_PSA_DRIVER_GET_ENTROPY and MBEDTLS_PSA_BUILTIN_GET_ENTROPY " \
    "are currently incompatible."
#endif

int mbedtls_entropy_poll_platform(void *data, unsigned char *output, size_t len, size_t *olen)
{
    int ret;
    size_t estimate_bits = 0;
    (void) data;

    /* Historically, in PolarSSL and Mbed TLS, the entropy callback provided
     * full-entropy output, and reported how many bytes in the output buffer
     * had useful data. Reporting the length was not very useful because
     * we get the same amount of entropy by processing the whole output bufer,
     * processing the whole buffer barely costs more CPU time since the buffer
     * is small, and most entropy sources just fill the whole buffer anyway.
     * So since TF-PSA-Crypto 1.0, we process the whole buffer.
     */
    *olen = len;

    ret = mbedtls_platform_get_entropy(PSA_DRIVER_GET_ENTROPY_FLAGS_NONE,
                                       &estimate_bits, output, len);
    if (ret != 0) {
        return ret;
    }

    if (estimate_bits < (8 * len)) {
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }

    return 0;
}
#endif /* MBEDTLS_PSA_BUILTIN_GET_ENTROPY || MBEDTLS_PSA_DRIVER_GET_ENTROPY */

#if defined(MBEDTLS_ENTROPY_NV_SEED)
int mbedtls_nv_seed_poll(void *data,
                         unsigned char *output, size_t len, size_t *olen)
{
    unsigned char buf[MBEDTLS_ENTROPY_BLOCK_SIZE];
    size_t use_len = MBEDTLS_ENTROPY_BLOCK_SIZE;
    ((void) data);

    memset(buf, 0, MBEDTLS_ENTROPY_BLOCK_SIZE);

    if (mbedtls_nv_seed_read(buf, MBEDTLS_ENTROPY_BLOCK_SIZE) < 0) {
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }

    if (len < use_len) {
        use_len = len;
    }

    memcpy(output, buf, use_len);
    *olen = use_len;

    return 0;
}
#endif /* MBEDTLS_ENTROPY_NV_SEED */

#endif /* MBEDTLS_ENTROPY_C */
