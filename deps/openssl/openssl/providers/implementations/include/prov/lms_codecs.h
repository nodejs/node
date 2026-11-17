/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef PROV_LMS_CODECS_H
#define PROV_LMS_CODECS_H
#pragma once

#ifndef OPENSSL_NO_LMS
#include <openssl/e_os2.h>
#include "crypto/lms.h"
#include "prov/ciphercommon.h"
#include "prov/provider_ctx.h"

__owur LMS_KEY *
ossl_lms_d2i_PUBKEY(const uint8_t *pubenc, int publen, PROV_CTX *provctx);
__owur int ossl_lms_i2d_pubkey(const LMS_KEY *key, unsigned char **out);
__owur int ossl_lms_key_to_text(BIO *out, const LMS_KEY *key, int selection);

#endif /* OPENSSL_NO_LMS */
#endif /* PROV_LMS_CODECS_H */
