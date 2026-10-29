/*
 * Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "crypto/lms_sig.h"

/**
 * @brief Create a new LMS_SIG object
 */
LMS_SIG *ossl_lms_sig_new(void)
{
    return OPENSSL_zalloc(sizeof(LMS_SIG));
}

/**
 * @brief Destroy an existing LMS_SIG object
 */
void ossl_lms_sig_free(LMS_SIG *sig)
{
    OPENSSL_free(sig);
}
