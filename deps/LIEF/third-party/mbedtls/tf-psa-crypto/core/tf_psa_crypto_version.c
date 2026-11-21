/*
 *  Version information
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "tf_psa_crypto_common.h"

#if defined(TF_PSA_CRYPTO_VERSION)
#include "tf-psa-crypto/version.h"

unsigned int tf_psa_crypto_version_get_number(void)
{
    return TF_PSA_CRYPTO_VERSION_NUMBER;
}

const char *tf_psa_crypto_version_get_string(void)
{
    return TF_PSA_CRYPTO_VERSION_STRING;
}

const char *tf_psa_crypto_version_get_string_full(void)
{
    return TF_PSA_CRYPTO_VERSION_STRING_FULL;
}

#endif /* TF_PSA_CRYPTO_VERSION */
