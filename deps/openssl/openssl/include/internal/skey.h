/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_CRYPTO_SKEY_H
#define OSSL_CRYPTO_SKEY_H

#include <stddef.h>

#include <openssl/types.h>

/* Known symmetric key type definitions */
#define SKEY_TYPE_GENERIC 1 /* generic bytes container unknown key types */
#define SKEY_TYPE_AES 2 /* AES keys */

struct prov_skey_st {
    /*
     * Internal skey implementation,
     * A symmetric key is basically just a buffer of bytes of
     * defined length, and a type, that defines, what
     * cryptosystem the key is meant for (AES, HMAC, etc...)
     */
    OSSL_LIB_CTX *libctx;

    int type;

    unsigned char *data;
    size_t length;

    /* Metadata — set during import, returned during export / get_key_id */
    char *alias;
    unsigned char *local_keyid;
    size_t local_keyid_len;
    unsigned char *algorithm_oid;
    size_t algorithm_oid_len;
    unsigned char *algorithm_params;
    size_t algorithm_params_len;
};

#endif /* OSSL_CRYPTO_SKEY_H */
