/*
 * Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * DSA low level APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"
#include <openssl/obj_mac.h>
#include <openssl/evp.h>
#include "internal/packet.h"
#include "prov/der_ml_dsa.h"
#include "prov/der_pq_dsa.h"
#include "prov/der_digests.h"

#define SET_OID(oid, oidlen, oidname)  \
    (oid) = ossl_der_oid_id_##oidname; \
    (oidlen) = sizeof(ossl_der_oid_id_##oidname)

#define SET_DIGEST_OID(oidname, digestsz) \
    SET_OID(*oid, *oidlen, oidname);      \
    *sz = digestsz

int ossl_DER_w_algorithmIdentifier_ML_DSA(WPACKET *pkt, int tag, ML_DSA_KEY *key)
{
    const uint8_t *oid;
    size_t oidlen;
    const char *name = ossl_ml_dsa_key_get_name(key);

    if (OPENSSL_strcasecmp(name, "ML-DSA-44") == 0) {
        SET_OID(oid, oidlen, ml_dsa_44);
    } else if (OPENSSL_strcasecmp(name, "ML-DSA-65") == 0) {
        SET_OID(oid, oidlen, ml_dsa_65);
    } else if (OPENSSL_strcasecmp(name, "ML-DSA-87") == 0) {
        SET_OID(oid, oidlen, ml_dsa_87);
    } else {
        return 0;
    }
    return ossl_DER_w_begin_sequence(pkt, tag)
        /* No parameters */
        && ossl_DER_w_precompiled(pkt, -1, oid, oidlen)
        && ossl_DER_w_end_sequence(pkt, tag);
}

int ossl_der_oid_pq_dsa_prehash_digest(const char *oid_digest_name,
    const uint8_t **oid, size_t *oidlen, size_t *sz)
{
    if (OPENSSL_strcasecmp(oid_digest_name, "SHAKE-256") == 0) {
        SET_DIGEST_OID(shake256, 64);
    } else if (OPENSSL_strcasecmp(oid_digest_name, "SHAKE-128") == 0) {
        SET_DIGEST_OID(shake128, 32);
    } else if (OPENSSL_strcasecmp(oid_digest_name, "SHA-224") == 0) {
        SET_DIGEST_OID(sha224, 28);
    } else if (OPENSSL_strcasecmp(oid_digest_name, "SHA-256") == 0) {
        SET_DIGEST_OID(sha256, 32);
    } else if (OPENSSL_strcasecmp(oid_digest_name, "SHA-384") == 0) {
        SET_DIGEST_OID(sha384, 48);
    } else if (OPENSSL_strcasecmp(oid_digest_name, "SHA-512") == 0) {
        SET_DIGEST_OID(sha512, 64);
    } else if (OPENSSL_strcasecmp(oid_digest_name, "SHA3-224") == 0) {
        SET_DIGEST_OID(sha3_224, 28);
    } else if (OPENSSL_strcasecmp(oid_digest_name, "SHA3-256") == 0) {
        SET_DIGEST_OID(sha3_256, 32);
    } else if (OPENSSL_strcasecmp(oid_digest_name, "SHA3-384") == 0) {
        SET_DIGEST_OID(sha3_384, 48);
    } else if (OPENSSL_strcasecmp(oid_digest_name, "SHA3-512") == 0) {
        SET_DIGEST_OID(sha3_512, 64);
    } else {
        return 0;
    }
    return 1;
}
