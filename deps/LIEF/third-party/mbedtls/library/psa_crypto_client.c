/*
 *  PSA crypto client code
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"
#include "psa/crypto.h"

#if defined(MBEDTLS_PSA_CRYPTO_CLIENT)

#include <string.h>
#include "mbedtls/platform.h"

void psa_reset_key_attributes(psa_key_attributes_t *attributes)
{
    memset(attributes, 0, sizeof(*attributes));
}

#endif /* MBEDTLS_PSA_CRYPTO_CLIENT */
