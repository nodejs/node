/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * SHA low level APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include <openssl/byteorder.h>
#include <openssl/crypto.h>
#include <openssl/core_dispatch.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/params.h>
#include <openssl/proverr.h>
#include <openssl/core_names.h>
#include "prov/digestcommon.h"
#include "prov/implementations.h"
#include "crypto/sha.h"
#include "internal/common.h"
#include "providers/implementations/digests/sha2_prov.inc"

#define SHA2_FLAGS PROV_DIGEST_FLAG_ALGID_ABSENT

extern int SHA1_Update_thunk(void *ctx, const unsigned char *data, size_t sz);
extern int SHA256_Update_thunk(void *ctx, const unsigned char *data, size_t sz);
extern int SHA512_Update_thunk(void *ctx, const unsigned char *data, size_t sz);

/* Special set_params method for SSL3 */
static int sha1_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
    struct sha1_set_ctx_params_st p;
    SHA_CTX *ctx = (SHA_CTX *)vctx;

    if (ossl_unlikely(ctx == NULL || !sha1_set_ctx_params_decoder(params, &p)))
        return 0;

    if (p.ssl3_ms != NULL)
        return ossl_sha1_ctrl(ctx, EVP_CTRL_SSL3_MASTER_SECRET,
            (int)p.ssl3_ms->data_size, p.ssl3_ms->data);

    return 1;
}

static const OSSL_PARAM *sha1_settable_ctx_params(ossl_unused void *ctx,
    ossl_unused void *provctx)
{
    return sha1_set_ctx_params_list;
}

static const unsigned char sha256magic[] = "SHA256v1";
#define SHA256MAGIC_LEN (sizeof(sha256magic) - 1)
#define SHA256_SERIALIZATION_LEN                      \
    (                                                 \
        SHA256MAGIC_LEN /* magic */                   \
        + sizeof(uint32_t) /* c->md_len */            \
        + sizeof(uint32_t) /* c->num */               \
        + sizeof(uint32_t) * 8 /* c->h */             \
        + sizeof(uint32_t) * 2 /* c->Nl + c->Nh */    \
        + sizeof(uint32_t) * SHA_LBLOCK /* c->data */ \
    )

static int SHA256_Serialize(SHA256_CTX *c, unsigned char *out,
    size_t *outlen)
{
    unsigned char *p;
    unsigned long i;

    if (out == NULL) {
        if (outlen == NULL)
            return 0;

        *outlen = SHA256_SERIALIZATION_LEN;
        return 1;
    }

    if (outlen != NULL && *outlen < SHA256_SERIALIZATION_LEN)
        return 0;

    p = out;

    /* Magic code */
    memcpy(p, sha256magic, SHA256MAGIC_LEN);
    p += SHA256MAGIC_LEN;

    /* md_len */
    p = OPENSSL_store_u32_le(p, c->md_len);

    /* num */
    p = OPENSSL_store_u32_le(p, c->num);

    /* h */
    for (i = 0; i < sizeof(c->h) / sizeof(SHA_LONG); i++)
        p = OPENSSL_store_u32_le(p, c->h[i]);

    /* Nl, Nh */
    p = OPENSSL_store_u32_le(p, c->Nl);
    p = OPENSSL_store_u32_le(p, c->Nh);

    /* data */
    for (i = 0; i < SHA_LBLOCK; i++)
        p = OPENSSL_store_u32_le(p, c->data[i]);

    if (outlen != NULL)
        *outlen = SHA256_SERIALIZATION_LEN;

    return 1;
}

/*
 * This function only performs basic input sanity checks and is not
 * built to handle malicious input data. Only trusted input should be
 * fed to this function
 */
static int SHA256_Deserialize(SHA256_CTX *c, const unsigned char *in,
    size_t inlen)
{
    const unsigned char *p;
    uint32_t val;
    unsigned long i;

    if (c == NULL || in == NULL || inlen != SHA256_SERIALIZATION_LEN)
        return 0;

    /* Magic code check */
    if (memcmp(in, sha256magic, SHA256MAGIC_LEN) != 0)
        return 0;

    p = in + SHA256MAGIC_LEN;

    /* md_len check */
    p = OPENSSL_load_u32_le(&val, p);
    if ((unsigned int)val != c->md_len) {
        return 0;
    }

    /* num check */
    p = OPENSSL_load_u32_le(&val, p);
    if (val >= sizeof(c->data))
        return 0;
    c->num = (unsigned int)val;

    /* h */
    for (i = 0; i < (sizeof(c->h) / sizeof(SHA_LONG)); i++) {
        p = OPENSSL_load_u32_le(&val, p);
        c->h[i] = (SHA_LONG)val;
    }

    /* Nl, Nh */
    p = OPENSSL_load_u32_le(&val, p);
    c->Nl = (SHA_LONG)val;
    p = OPENSSL_load_u32_le(&val, p);
    c->Nh = (SHA_LONG)val;

    /* data */
    for (i = 0; i < SHA_LBLOCK; i++) {
        p = OPENSSL_load_u32_le(&val, p);
        c->data[i] = (SHA_LONG)val;
    }

    return 1;
}

static const unsigned char sha512magic[] = "SHA512v1";
#define SHA512MAGIC_LEN (sizeof(sha512magic) - 1)
#define SHA512_SERIALIZATION_LEN                   \
    (                                              \
        SHA512MAGIC_LEN /* magic */                \
        + sizeof(uint32_t) /* c->md_len */         \
        + sizeof(uint32_t) /* c->num */            \
        + sizeof(uint64_t) * 8 /* c->h */          \
        + sizeof(uint64_t) * 2 /* c->Nl + c->Nh */ \
        + SHA512_CBLOCK /* c->u.d/c->u.p */        \
    )

static int SHA512_Serialize(SHA512_CTX *c, unsigned char *out,
    size_t *outlen)
{
    unsigned char *p;
    unsigned long i;

    if (out == NULL) {
        if (outlen == NULL)
            return 0;

        *outlen = SHA512_SERIALIZATION_LEN;
        return 1;
    }

    if (outlen != NULL && *outlen < SHA512_SERIALIZATION_LEN)
        return 0;

    p = out;

    /* Magic code */
    memcpy(p, sha512magic, SHA512MAGIC_LEN);
    p += SHA512MAGIC_LEN;

    /* md_len */
    p = OPENSSL_store_u32_le(p, c->md_len);

    /* num */
    p = OPENSSL_store_u32_le(p, c->num);

    /* h */
    for (i = 0; i < sizeof(c->h) / sizeof(SHA_LONG64); i++)
        p = OPENSSL_store_u64_le(p, c->h[i]);

    /* Nl, Nh */
    p = OPENSSL_store_u64_le(p, c->Nl);
    p = OPENSSL_store_u64_le(p, c->Nh);

    /* data */
    memcpy(p, c->u.p, SHA512_CBLOCK);
    p += SHA512_CBLOCK;

    if (outlen != NULL)
        *outlen = SHA512_SERIALIZATION_LEN;

    return 1;
}

/*
 * This function only performs basic input sanity checks and is not
 * built to handle malicious input data. Only trusted input should be
 * fed to this function
 */
static int SHA512_Deserialize(SHA512_CTX *c, const unsigned char *in,
    size_t inlen)
{
    const unsigned char *p;
    uint32_t val32;
    uint64_t val;
    unsigned long i;

    if (c == NULL || in == NULL || inlen != SHA512_SERIALIZATION_LEN)
        return 0;

    /* Magic code */
    if (memcmp(in, sha512magic, SHA512MAGIC_LEN) != 0)
        return 0;

    p = in + SHA512MAGIC_LEN;

    /* md_len check */
    p = OPENSSL_load_u32_le(&val32, p);
    if ((unsigned int)val32 != c->md_len)
        return 0;

    /* num check */
    p = OPENSSL_load_u32_le(&val32, p);
    if (val32 >= sizeof(c->u.d))
        return 0;
    c->num = (unsigned int)val32;

    /* h */
    for (i = 0; i < (sizeof(c->h) / sizeof(SHA_LONG64)); i++) {
        p = OPENSSL_load_u64_le(&val, p);
        c->h[i] = (SHA_LONG64)val;
    }

    /* Nl, Nh */
    p = OPENSSL_load_u64_le(&val, p);
    c->Nl = (SHA_LONG64)val;
    p = OPENSSL_load_u64_le(&val, p);
    c->Nh = (SHA_LONG64)val;

    /* data */
    memcpy(c->u.p, p, SHA512_CBLOCK);
    p += SHA512_CBLOCK;

    return 1;
}

/* ossl_sha1_functions */
IMPLEMENT_digest_functions_with_settable_ctx(
    sha1, SHA_CTX, SHA_CBLOCK, SHA_DIGEST_LENGTH, SHA2_FLAGS,
    SHA1_Init, SHA1_Update_thunk, SHA1_Final,
    sha1_settable_ctx_params, sha1_set_ctx_params)

/* ossl_sha224_functions */
IMPLEMENT_digest_functions_with_serialize(sha224, SHA256_CTX,
    SHA256_CBLOCK, SHA224_DIGEST_LENGTH,
    SHA2_FLAGS, SHA224_Init,
    SHA256_Update_thunk, SHA224_Final,
    SHA256_Serialize, SHA256_Deserialize)

/* ossl_sha256_functions */
IMPLEMENT_digest_functions_with_serialize(sha256, SHA256_CTX,
    SHA256_CBLOCK, SHA256_DIGEST_LENGTH,
    SHA2_FLAGS, SHA256_Init,
    SHA256_Update_thunk, SHA256_Final,
    SHA256_Serialize, SHA256_Deserialize)
/* ossl_sha256_192_internal_functions */
IMPLEMENT_digest_functions_with_serialize(sha256_192_internal, SHA256_CTX,
    SHA256_CBLOCK, SHA256_192_DIGEST_LENGTH,
    SHA2_FLAGS, ossl_sha256_192_init,
    SHA256_Update_thunk, SHA256_Final,
    SHA256_Serialize, SHA256_Deserialize)
/* ossl_sha384_functions */
IMPLEMENT_digest_functions_with_serialize(sha384, SHA512_CTX,
    SHA512_CBLOCK, SHA384_DIGEST_LENGTH,
    SHA2_FLAGS, SHA384_Init,
    SHA512_Update_thunk, SHA384_Final,
    SHA512_Serialize, SHA512_Deserialize)

/* ossl_sha512_functions */
IMPLEMENT_digest_functions_with_serialize(sha512, SHA512_CTX,
    SHA512_CBLOCK, SHA512_DIGEST_LENGTH,
    SHA2_FLAGS, SHA512_Init,
    SHA512_Update_thunk, SHA512_Final,
    SHA512_Serialize, SHA512_Deserialize)

/* ossl_sha512_224_functions */
IMPLEMENT_digest_functions_with_serialize(sha512_224, SHA512_CTX,
    SHA512_CBLOCK, SHA224_DIGEST_LENGTH,
    SHA2_FLAGS, sha512_224_init,
    SHA512_Update_thunk, SHA512_Final,
    SHA512_Serialize, SHA512_Deserialize)

/* ossl_sha512_256_functions */
IMPLEMENT_digest_functions_with_serialize(sha512_256, SHA512_CTX,
    SHA512_CBLOCK, SHA256_DIGEST_LENGTH,
    SHA2_FLAGS, sha512_256_init,
    SHA512_Update_thunk, SHA512_Final,
    SHA512_Serialize, SHA512_Deserialize)
