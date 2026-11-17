/*
 * Copyright 2015-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_CRYPTO_EVP_H
#define OSSL_CRYPTO_EVP_H
#pragma once

#include <openssl/evp.h>
#include <openssl/core_dispatch.h>
#include "internal/refcount.h"
#include "crypto/ecx.h"

/*
 * Default PKCS5 PBE KDF salt lengths
 * In RFC 8018, PBE1 uses 8 bytes (64 bits) for its salt length.
 * It also specifies to use at least 8 bytes for PBES2.
 * The NIST requirement for PBKDF2 is 128 bits so we use this as the
 * default for PBE2 (scrypt and HKDF2)
 */
#define PKCS5_DEFAULT_PBE1_SALT_LEN PKCS5_SALT_LEN
#define PKCS5_DEFAULT_PBE2_SALT_LEN 16
/*
 * Don't free up md_ctx->pctx in EVP_MD_CTX_reset, use the reserved flag
 * values in evp.h
 */
#define EVP_MD_CTX_FLAG_KEEP_PKEY_CTX 0x0400
#define EVP_MD_CTX_FLAG_FINALISED 0x0800

#define evp_pkey_ctx_is_legacy(ctx) \
    ((ctx)->keymgmt == NULL)
#define evp_pkey_ctx_is_provided(ctx) \
    (!evp_pkey_ctx_is_legacy(ctx))

struct evp_pkey_ctx_st {
    /* Actual operation */
    int operation;

    /*
     * Library context, property query, keytype and keymgmt associated with
     * this context
     */
    OSSL_LIB_CTX *libctx;
    char *propquery;
    const char *keytype;
    /* If |pkey| below is set, this field is always a reference to its keymgmt */
    EVP_KEYMGMT *keymgmt;

    union {
        struct {
            void *genctx;
        } keymgmt;

        struct {
            EVP_KEYEXCH *exchange;
            /*
             * Opaque ctx returned from a providers exchange algorithm
             * implementation OSSL_FUNC_keyexch_newctx()
             */
            void *algctx;
        } kex;

        struct {
            EVP_SIGNATURE *signature;
            /*
             * Opaque ctx returned from a providers signature algorithm
             * implementation OSSL_FUNC_signature_newctx()
             */
            void *algctx;
        } sig;

        struct {
            EVP_ASYM_CIPHER *cipher;
            /*
             * Opaque ctx returned from a providers asymmetric cipher algorithm
             * implementation OSSL_FUNC_asym_cipher_newctx()
             */
            void *algctx;
        } ciph;
        struct {
            EVP_KEM *kem;
            /*
             * Opaque ctx returned from a providers KEM algorithm
             * implementation OSSL_FUNC_kem_newctx()
             */
            void *algctx;
        } encap;
    } op;

    /*
     * Cached parameters.  Inits of operations that depend on these should
     * call evp_pkey_ctx_use_delayed_data() when the operation has been set
     * up properly.
     */
    struct {
        /* Distinguishing Identifier, ISO/IEC 15946-3, FIPS 196 */
        char *dist_id_name; /* The name used with EVP_PKEY_CTX_ctrl_str() */
        void *dist_id; /* The distinguishing ID itself */
        size_t dist_id_len; /* The length of the distinguishing ID */

        /* Indicators of what has been set.  Keep them together! */
        unsigned int dist_id_set : 1;
    } cached_parameters;

    /* Application specific data, usually used by the callback */
    void *app_data;
    /* Keygen callback */
    EVP_PKEY_gen_cb *pkey_gencb;
    /* implementation specific keygen data */
    int *keygen_info;
    int keygen_info_count;

    /* Legacy fields below */

    /* EVP_PKEY identity */
    int legacy_keytype;
    /* Key: may be NULL */
    EVP_PKEY *pkey;
    /* Peer key for key agreement, may be NULL */
    EVP_PKEY *peerkey;
    /* Algorithm specific data */
    void *data;
    /*
     * Used to support taking custody of memory in the case of a provider being
     * used with the deprecated EVP_PKEY_CTX_set_rsa_keygen_pubexp() API. This
     * member should NOT be used for any other purpose and should be removed
     * when said deprecated API is excised completely.
     */
    BIGNUM *rsa_pubexp;
} /* EVP_PKEY_CTX */;

#define EVP_PKEY_FLAG_DYNAMIC 1

void evp_pkey_set_cb_translate(BN_GENCB *cb, EVP_PKEY_CTX *ctx);

struct evp_mac_st {
    OSSL_PROVIDER *prov;
    int name_id;
    int no_store;
    char *type_name;
    const char *description;

    CRYPTO_REF_COUNT refcnt;

    OSSL_FUNC_mac_newctx_fn *newctx;
    OSSL_FUNC_mac_dupctx_fn *dupctx;
    OSSL_FUNC_mac_freectx_fn *freectx;
    OSSL_FUNC_mac_init_fn *init;
    OSSL_FUNC_mac_update_fn *update;
    OSSL_FUNC_mac_final_fn *final;
    OSSL_FUNC_mac_gettable_params_fn *gettable_params;
    OSSL_FUNC_mac_gettable_ctx_params_fn *gettable_ctx_params;
    OSSL_FUNC_mac_settable_ctx_params_fn *settable_ctx_params;
    OSSL_FUNC_mac_get_params_fn *get_params;
    OSSL_FUNC_mac_get_ctx_params_fn *get_ctx_params;
    OSSL_FUNC_mac_set_ctx_params_fn *set_ctx_params;
    OSSL_FUNC_mac_init_skey_fn *init_skey;
};

struct evp_kdf_st {
    OSSL_PROVIDER *prov;
    int name_id;
    int no_store;
    char *type_name;
    const char *description;
    CRYPTO_REF_COUNT refcnt;

    OSSL_FUNC_kdf_newctx_fn *newctx;
    OSSL_FUNC_kdf_dupctx_fn *dupctx;
    OSSL_FUNC_kdf_freectx_fn *freectx;
    OSSL_FUNC_kdf_reset_fn *reset;
    OSSL_FUNC_kdf_derive_fn *derive;
    OSSL_FUNC_kdf_gettable_params_fn *gettable_params;
    OSSL_FUNC_kdf_gettable_ctx_params_fn *gettable_ctx_params;
    OSSL_FUNC_kdf_settable_ctx_params_fn *settable_ctx_params;
    OSSL_FUNC_kdf_get_params_fn *get_params;
    OSSL_FUNC_kdf_get_ctx_params_fn *get_ctx_params;
    OSSL_FUNC_kdf_set_ctx_params_fn *set_ctx_params;
    OSSL_FUNC_kdf_set_skey_fn *set_skey;
    OSSL_FUNC_kdf_derive_skey_fn *derive_skey;
};

#define EVP_ORIG_DYNAMIC 0
#define EVP_ORIG_GLOBAL 1

struct evp_md_st {
    /* nid */
    int type;

    int pkey_type;
    int md_size;
    unsigned long flags;
    int origin;
    int block_size;

    int name_id;
    char *type_name;
    const char *description;
    OSSL_PROVIDER *prov;
    CRYPTO_REF_COUNT refcnt;
    OSSL_FUNC_digest_newctx_fn *newctx;
    OSSL_FUNC_digest_init_fn *dinit;
    OSSL_FUNC_digest_update_fn *dupdate;
    OSSL_FUNC_digest_final_fn *dfinal;
    OSSL_FUNC_digest_squeeze_fn *dsqueeze;
    OSSL_FUNC_digest_digest_fn *digest;
    OSSL_FUNC_digest_freectx_fn *freectx;
    OSSL_FUNC_digest_copyctx_fn *copyctx;
    OSSL_FUNC_digest_dupctx_fn *dupctx;
    OSSL_FUNC_digest_get_params_fn *get_params;
    OSSL_FUNC_digest_set_ctx_params_fn *set_ctx_params;
    OSSL_FUNC_digest_get_ctx_params_fn *get_ctx_params;
    OSSL_FUNC_digest_gettable_params_fn *gettable_params;
    OSSL_FUNC_digest_settable_ctx_params_fn *settable_ctx_params;
    OSSL_FUNC_digest_gettable_ctx_params_fn *gettable_ctx_params;
    OSSL_FUNC_digest_serialize_fn *serialize;
    OSSL_FUNC_digest_deserialize_fn *deserialize;
} /* EVP_MD */;

struct evp_cipher_st {
    int nid;

    int block_size;
    /* Default value for variable length ciphers */
    int key_len;
    int iv_len;

    /* Various flags */
    unsigned long flags;
    /* How the EVP_CIPHER was created. */
    int origin;

    int name_id;
    char *type_name;
    const char *description;
    OSSL_PROVIDER *prov;
    CRYPTO_REF_COUNT refcnt;
    OSSL_FUNC_cipher_newctx_fn *newctx;
    OSSL_FUNC_cipher_encrypt_init_fn *einit;
    OSSL_FUNC_cipher_decrypt_init_fn *dinit;
    OSSL_FUNC_cipher_update_fn *cupdate;
    OSSL_FUNC_cipher_final_fn *cfinal;
    OSSL_FUNC_cipher_cipher_fn *ccipher;
    OSSL_FUNC_cipher_pipeline_encrypt_init_fn *p_einit;
    OSSL_FUNC_cipher_pipeline_decrypt_init_fn *p_dinit;
    OSSL_FUNC_cipher_pipeline_update_fn *p_cupdate;
    OSSL_FUNC_cipher_pipeline_final_fn *p_cfinal;
    OSSL_FUNC_cipher_freectx_fn *freectx;
    OSSL_FUNC_cipher_dupctx_fn *dupctx;
    OSSL_FUNC_cipher_get_params_fn *get_params;
    OSSL_FUNC_cipher_get_ctx_params_fn *get_ctx_params;
    OSSL_FUNC_cipher_set_ctx_params_fn *set_ctx_params;
    OSSL_FUNC_cipher_gettable_params_fn *gettable_params;
    OSSL_FUNC_cipher_gettable_ctx_params_fn *gettable_ctx_params;
    OSSL_FUNC_cipher_settable_ctx_params_fn *settable_ctx_params;
    OSSL_FUNC_cipher_encrypt_skey_init_fn *einit_skey;
    OSSL_FUNC_cipher_decrypt_skey_init_fn *dinit_skey;
} /* EVP_CIPHER */;

#define EVP_MAXCHUNK ((size_t)1 << 30)

#define BLOCK_CIPHER_def1(cname, nmode, mode, MODE, kstruct, nid, block_size, \
    key_len, iv_len, flags)                                                   \
    static const EVP_CIPHER cname##_##mode = {                                \
        nid##_##nmode, block_size, key_len, iv_len,                           \
        flags | EVP_CIPH_##MODE##_MODE,                                       \
        EVP_ORIG_GLOBAL                                                       \
    };                                                                        \
    const EVP_CIPHER *EVP_##cname##_##mode(void) { return &cname##_##mode; }

#define BLOCK_CIPHER_def_cbc(cname, kstruct, nid, block_size, key_len,         \
    iv_len, flags)                                                             \
    BLOCK_CIPHER_def1(cname, cbc, cbc, CBC, kstruct, nid, block_size, key_len, \
        iv_len, flags)

#define BLOCK_CIPHER_def_cfb(cname, kstruct, nid, key_len,                 \
    iv_len, cbits, flags)                                                  \
    BLOCK_CIPHER_def1(cname, cfb##cbits, cfb##cbits, CFB, kstruct, nid, 1, \
        key_len, iv_len, flags)

#define BLOCK_CIPHER_def_ofb(cname, kstruct, nid, key_len,          \
    iv_len, cbits, flags)                                           \
    BLOCK_CIPHER_def1(cname, ofb##cbits, ofb, OFB, kstruct, nid, 1, \
        key_len, iv_len, flags)

#define BLOCK_CIPHER_def_ecb(cname, kstruct, nid, block_size, key_len,         \
    flags)                                                                     \
    BLOCK_CIPHER_def1(cname, ecb, ecb, ECB, kstruct, nid, block_size, key_len, \
        0, flags)

#define BLOCK_CIPHER_defs(cname, kstruct,                                         \
    nid, block_size, key_len, iv_len, cbits, flags)                               \
    BLOCK_CIPHER_def_cbc(cname, kstruct, nid, block_size, key_len, iv_len, flags) \
    BLOCK_CIPHER_def_cfb(cname, kstruct, nid, key_len, iv_len, cbits,             \
        flags)                                                                    \
    BLOCK_CIPHER_def_ofb(cname, kstruct, nid, key_len, iv_len, cbits,             \
        flags)                                                                    \
    BLOCK_CIPHER_def_ecb(cname, kstruct, nid, block_size, key_len, flags)

#define IMPLEMENT_BLOCK_CIPHER(cname, ksched, cprefix, kstruct, nid,    \
    block_size, key_len, iv_len, cbits,                                 \
    flags)                                                              \
    BLOCK_CIPHER_defs(cname, kstruct, nid, block_size, key_len, iv_len, \
        cbits, flags)

#define IMPLEMENT_CFBR(cipher, cprefix, kstruct, ksched, keysize, cbits, iv_len, fl) \
    BLOCK_CIPHER_def_cfb(cipher##_##keysize, kstruct,                                \
        NID_##cipher##_##keysize, keysize / 8, iv_len, cbits,                        \
        (fl) | EVP_CIPH_FLAG_DEFAULT_ASN1)

typedef struct {
    unsigned char iv[EVP_MAX_IV_LENGTH];
    unsigned int iv_len;
    unsigned int tag_len;
} evp_cipher_aead_asn1_params;

int evp_cipher_param_to_asn1_ex(EVP_CIPHER_CTX *c, ASN1_TYPE *type,
    evp_cipher_aead_asn1_params *params);

int evp_cipher_asn1_to_param_ex(EVP_CIPHER_CTX *c, ASN1_TYPE *type,
    evp_cipher_aead_asn1_params *params);

/*
 * To support transparent execution of operation in backends other
 * than the "origin" key, we support transparent export/import to
 * those providers, and maintain a cache of the imported keydata,
 * so we don't need to redo the export/import every time we perform
 * the same operation in that same provider.
 * This requires that the "origin" backend (whether it's a legacy or a
 * provider "origin") implements exports, and that the target provider
 * has an EVP_KEYMGMT that implements import.
 */
typedef struct {
    EVP_KEYMGMT *keymgmt;
    void *keydata;
    int selection;
} OP_CACHE_ELEM;

DEFINE_STACK_OF(OP_CACHE_ELEM)

/*
 * An EVP_PKEY can have the following states:
 *
 * untyped & empty:
 *
 *     type == EVP_PKEY_NONE && keymgmt == NULL
 *
 * typed & empty:
 *
 *     (type != EVP_PKEY_NONE && pkey.ptr == NULL)      ## legacy (libcrypto only)
 *     || (keymgmt != NULL && keydata == NULL)          ## provider side
 *
 * fully assigned:
 *
 *     (type != EVP_PKEY_NONE && pkey.ptr != NULL)      ## legacy (libcrypto only)
 *     || (keymgmt != NULL && keydata != NULL)          ## provider side
 *
 * The easiest way to detect a legacy key is:
 *
 *     keymgmt == NULL && type != EVP_PKEY_NONE
 *
 * The easiest way to detect a provider side key is:
 *
 *     keymgmt != NULL
 */
#define evp_pkey_is_blank(pk) \
    ((pk)->type == EVP_PKEY_NONE && (pk)->keymgmt == NULL)
#define evp_pkey_is_typed(pk) \
    ((pk)->type != EVP_PKEY_NONE || (pk)->keymgmt != NULL)
#ifndef FIPS_MODULE
#define evp_pkey_is_assigned(pk) \
    ((pk)->pkey.ptr != NULL || (pk)->keydata != NULL)
#else
#define evp_pkey_is_assigned(pk) \
    ((pk)->keydata != NULL)
#endif
#define evp_pkey_is_legacy(pk) \
    ((pk)->type != EVP_PKEY_NONE && (pk)->keymgmt == NULL)
#define evp_pkey_is_provided(pk) \
    ((pk)->keymgmt != NULL)

union legacy_pkey_st {
    void *ptr;
    struct rsa_st *rsa; /* RSA */
#ifndef OPENSSL_NO_DSA
    struct dsa_st *dsa; /* DSA */
#endif
#ifndef OPENSSL_NO_DH
    struct dh_st *dh; /* DH */
#endif
#ifndef OPENSSL_NO_EC
    struct ec_key_st *ec; /* ECC */
#ifndef OPENSSL_NO_ECX
    ECX_KEY *ecx; /* X25519, X448, Ed25519, Ed448 */
#endif
#endif
};

struct evp_pkey_st {
    /* == Legacy attributes == */
    int type;
    int save_type;

#ifndef FIPS_MODULE
    /*
     * Legacy key "origin" is composed of a pointer to an EVP_PKEY_ASN1_METHOD,
     * a pointer to a low level key and possibly a pointer to an engine.
     */
    const EVP_PKEY_ASN1_METHOD *ameth;

    /* Union to store the reference to an origin legacy key */
    union legacy_pkey_st pkey;

    /* Union to store the reference to a non-origin legacy key */
    union legacy_pkey_st legacy_cache_pkey;
#endif

    /* == Common attributes == */
    CRYPTO_REF_COUNT references;
    CRYPTO_RWLOCK *lock;
#ifndef FIPS_MODULE
    STACK_OF(X509_ATTRIBUTE) *attributes; /* [ 0 ] */
    int save_parameters;
    unsigned int foreign : 1; /* the low-level key is using an engine or an app-method */
    CRYPTO_EX_DATA ex_data;
#endif

    /* == Provider attributes == */

    /*
     * Provider keydata "origin" is composed of a pointer to an EVP_KEYMGMT
     * and a pointer to the provider side key data.  This is never used at
     * the same time as the legacy key data above.
     */
    EVP_KEYMGMT *keymgmt;
    void *keydata;
    /*
     * If any libcrypto code does anything that may modify the keydata
     * contents, this dirty counter must be incremented.
     */
    size_t dirty_cnt;

    /*
     * To support transparent execution of operation in backends other
     * than the "origin" key, we support transparent export/import to
     * those providers, and maintain a cache of the imported keydata,
     * so we don't need to redo the export/import every time we perform
     * the same operation in that same provider.
     */
    STACK_OF(OP_CACHE_ELEM) *operation_cache;

    /*
     * We keep a copy of that "origin"'s dirty count, so we know if the
     * operation cache needs flushing.
     */
    size_t dirty_cnt_copy;

    /* Cache of key object information */
    struct {
        int bits;
        int security_bits;
        int security_category;
        int size;
    } cache;
}; /* EVP_PKEY */

/* The EVP_PKEY_OP_TYPE_ macros are found in include/openssl/evp.h */

#define EVP_PKEY_CTX_IS_SIGNATURE_OP(ctx) \
    (((ctx)->operation & EVP_PKEY_OP_TYPE_SIG) != 0)

#define EVP_PKEY_CTX_IS_DERIVE_OP(ctx) \
    (((ctx)->operation & EVP_PKEY_OP_TYPE_DERIVE) != 0)

#define EVP_PKEY_CTX_IS_ASYM_CIPHER_OP(ctx) \
    (((ctx)->operation & EVP_PKEY_OP_TYPE_CRYPT) != 0)

#define EVP_PKEY_CTX_IS_GEN_OP(ctx) \
    (((ctx)->operation & EVP_PKEY_OP_TYPE_GEN) != 0)

#define EVP_PKEY_CTX_IS_FROMDATA_OP(ctx) \
    (((ctx)->operation & EVP_PKEY_OP_TYPE_DATA) != 0)

#define EVP_PKEY_CTX_IS_KEM_OP(ctx) \
    (((ctx)->operation & EVP_PKEY_OP_TYPE_KEM) != 0)

struct evp_skey_st {
    /* == Common attributes == */
    CRYPTO_REF_COUNT references;
    CRYPTO_RWLOCK *lock;

    void *keydata; /* Alg-specific key data */
    EVP_SKEYMGMT *skeymgmt; /* Import, export, manage */
}; /* EVP_SKEY */

void openssl_add_all_ciphers_int(void);
void openssl_add_all_digests_int(void);
void evp_cleanup_int(void);
void *evp_pkey_export_to_provider(EVP_PKEY *pk, OSSL_LIB_CTX *libctx,
    EVP_KEYMGMT **keymgmt,
    const char *propquery);
#ifndef FIPS_MODULE
int evp_pkey_copy_downgraded(EVP_PKEY **dest, const EVP_PKEY *src);
void *evp_pkey_get_legacy(EVP_PKEY *pk);
void evp_pkey_free_legacy(EVP_PKEY *x);
EVP_PKEY *evp_pkcs82pkey_legacy(const PKCS8_PRIV_KEY_INFO *p8inf,
    OSSL_LIB_CTX *libctx, const char *propq);
#endif

/*
 * KEYMGMT utility functions
 */

/*
 * Key import structure and helper function, to be used as an export callback
 */
struct evp_keymgmt_util_try_import_data_st {
    EVP_KEYMGMT *keymgmt;
    void *keydata;

    int selection;
};
int evp_keymgmt_util_try_import(const OSSL_PARAM params[], void *arg);
int evp_keymgmt_util_assign_pkey(EVP_PKEY *pkey, EVP_KEYMGMT *keymgmt,
    void *keydata);
EVP_PKEY *evp_keymgmt_util_make_pkey(EVP_KEYMGMT *keymgmt, void *keydata);

int evp_keymgmt_util_export(const EVP_PKEY *pk, int selection,
    OSSL_CALLBACK *export_cb, void *export_cbarg);
void *evp_keymgmt_util_export_to_provider(EVP_PKEY *pk, EVP_KEYMGMT *keymgmt,
    int selection);
OP_CACHE_ELEM *evp_keymgmt_util_find_operation_cache(EVP_PKEY *pk,
    EVP_KEYMGMT *keymgmt,
    int selection);
void evp_keymgmt_util_clear_operation_cache(EVP_PKEY *pk);
int evp_keymgmt_util_cache_keydata(EVP_PKEY *pk, EVP_KEYMGMT *keymgmt,
    void *keydata, int selection);
void evp_keymgmt_util_cache_keyinfo(EVP_PKEY *pk);
void *evp_keymgmt_util_fromdata(EVP_PKEY *target, EVP_KEYMGMT *keymgmt,
    int selection, const OSSL_PARAM params[]);
int evp_keymgmt_util_has(EVP_PKEY *pk, int selection);
int evp_keymgmt_util_match(EVP_PKEY *pk1, EVP_PKEY *pk2, int selection);
int evp_keymgmt_util_copy(EVP_PKEY *to, EVP_PKEY *from, int selection);
void *evp_keymgmt_util_gen(EVP_PKEY *target, EVP_KEYMGMT *keymgmt,
    void *genctx, OSSL_CALLBACK *cb, void *cbarg);
int evp_keymgmt_util_get_deflt_digest_name(EVP_KEYMGMT *keymgmt,
    void *keydata,
    char *mdname, size_t mdname_sz);
const char *evp_keymgmt_util_query_operation_name(EVP_KEYMGMT *keymgmt,
    int op_id);

/*
 * KEYMGMT provider interface functions
 */
void *evp_keymgmt_newdata(const EVP_KEYMGMT *keymgmt, const OSSL_PARAM params[]);
void evp_keymgmt_freedata(const EVP_KEYMGMT *keymgmt, void *keyddata);
int evp_keymgmt_get_params(const EVP_KEYMGMT *keymgmt,
    void *keydata, OSSL_PARAM params[]);
int evp_keymgmt_set_params(const EVP_KEYMGMT *keymgmt,
    void *keydata, const OSSL_PARAM params[]);
void *evp_keymgmt_gen_init(const EVP_KEYMGMT *keymgmt, int selection,
    const OSSL_PARAM params[]);
int evp_keymgmt_gen_set_template(const EVP_KEYMGMT *keymgmt, void *genctx,
    void *templ);
int evp_keymgmt_gen_set_params(const EVP_KEYMGMT *keymgmt, void *genctx,
    const OSSL_PARAM params[]);
int evp_keymgmt_gen_get_params(const EVP_KEYMGMT *keymgmt,
    void *genctx, OSSL_PARAM params[]);
void *evp_keymgmt_gen(const EVP_KEYMGMT *keymgmt, void *genctx,
    OSSL_CALLBACK *cb, void *cbarg);
void evp_keymgmt_gen_cleanup(const EVP_KEYMGMT *keymgmt, void *genctx);

int evp_keymgmt_has_load(const EVP_KEYMGMT *keymgmt);
void *evp_keymgmt_load(const EVP_KEYMGMT *keymgmt,
    const void *objref, size_t objref_sz);

int evp_keymgmt_has(const EVP_KEYMGMT *keymgmt, void *keyddata, int selection);
int evp_keymgmt_validate(const EVP_KEYMGMT *keymgmt, void *keydata,
    int selection, int checktype);
int evp_keymgmt_match(const EVP_KEYMGMT *keymgmt,
    const void *keydata1, const void *keydata2,
    int selection);

int evp_keymgmt_import(const EVP_KEYMGMT *keymgmt, void *keydata,
    int selection, const OSSL_PARAM params[]);
const OSSL_PARAM *evp_keymgmt_import_types(const EVP_KEYMGMT *keymgmt,
    int selection);
int evp_keymgmt_export(const EVP_KEYMGMT *keymgmt, void *keydata,
    int selection, OSSL_CALLBACK *param_cb, void *cbarg);
const OSSL_PARAM *evp_keymgmt_export_types(const EVP_KEYMGMT *keymgmt,
    int selection);
void *evp_keymgmt_dup(const EVP_KEYMGMT *keymgmt,
    const void *keydata_from, int selection);
EVP_KEYMGMT *evp_keymgmt_fetch_from_prov(OSSL_PROVIDER *prov,
    const char *name,
    const char *properties);

/*
 * SKEYMGMT provider interface functions
 */
EVP_SKEY *evp_skey_alloc(EVP_SKEYMGMT *skeymgmt);
void evp_skeymgmt_freedata(const EVP_SKEYMGMT *keymgmt, void *keyddata);
void *evp_skeymgmt_import(const EVP_SKEYMGMT *skeymgmt, int selection, const OSSL_PARAM params[]);
int evp_skeymgmt_export(const EVP_SKEYMGMT *skeymgmt, void *keydata,
    int selection, OSSL_CALLBACK *param_cb, void *cbarg);
void *evp_skeymgmt_generate(const EVP_SKEYMGMT *skeymgmt, const OSSL_PARAM params[]);
EVP_SKEYMGMT *evp_skeymgmt_fetch_from_prov(OSSL_PROVIDER *prov,
    const char *name,
    const char *properties);

/* Pulling defines out of C source files */

#define EVP_RC4_KEY_SIZE 16
#ifndef TLS1_1_VERSION
#define TLS1_1_VERSION 0x0302
#endif

void evp_encode_ctx_set_flags(EVP_ENCODE_CTX *ctx, unsigned int flags);

/* EVP_ENCODE_CTX flags */
/* Don't generate new lines when encoding */
#define EVP_ENCODE_CTX_NO_NEWLINES 1
/* Use the SRP base64 alphabet instead of the standard one */
#define EVP_ENCODE_CTX_USE_SRP_ALPHABET 2

const EVP_CIPHER *evp_get_cipherbyname_ex(OSSL_LIB_CTX *libctx,
    const char *name);
const EVP_MD *evp_get_digestbyname_ex(OSSL_LIB_CTX *libctx,
    const char *name);

int ossl_pkcs5_pbkdf2_hmac_ex(const char *pass, int passlen,
    const unsigned char *salt, int saltlen, int iter,
    const EVP_MD *digest, int keylen,
    unsigned char *out,
    OSSL_LIB_CTX *libctx, const char *propq);

#ifndef FIPS_MODULE
/*
 * Internal helpers for stricter EVP_PKEY_CTX_{set,get}_params().
 *
 * Return 1 on success, 0 or negative for errors.
 *
 * In particular they return -2 if any of the params is not supported.
 *
 * They are not available in FIPS_MODULE as they depend on
 *      - EVP_PKEY_CTX_{get,set}_params()
 *      - EVP_PKEY_CTX_{gettable,settable}_params()
 *
 */
int evp_pkey_ctx_set_params_strict(EVP_PKEY_CTX *ctx, OSSL_PARAM *params);
int evp_pkey_ctx_get_params_strict(EVP_PKEY_CTX *ctx, OSSL_PARAM *params);

EVP_MD_CTX *evp_md_ctx_new_ex(EVP_PKEY *pkey, const ASN1_OCTET_STRING *id,
    OSSL_LIB_CTX *libctx, const char *propq);
int evp_pkey_name2type(const char *name);
const char *evp_pkey_type2name(int type);

int evp_pkey_ctx_use_cached_data(EVP_PKEY_CTX *ctx);
#endif /* !defined(FIPS_MODULE) */

int evp_method_store_cache_flush(OSSL_LIB_CTX *libctx);
int evp_method_store_remove_all_provided(const OSSL_PROVIDER *prov);

int evp_default_properties_enable_fips_int(OSSL_LIB_CTX *libctx, int enable,
    int loadconfig);
int evp_set_default_properties_int(OSSL_LIB_CTX *libctx, const char *propq,
    int loadconfig, int mirrored);
char *evp_get_global_properties_str(OSSL_LIB_CTX *libctx, int loadconfig);

void evp_md_ctx_clear_digest(EVP_MD_CTX *ctx, int force, int keep_digest);
/* just free the algctx if set, returns 0 on inconsistent state of ctx */
int evp_md_ctx_free_algctx(EVP_MD_CTX *ctx);

/* Three possible states: */
#define EVP_PKEY_STATE_UNKNOWN 0
#define EVP_PKEY_STATE_LEGACY 1
#define EVP_PKEY_STATE_PROVIDER 2
int evp_pkey_ctx_state(const EVP_PKEY_CTX *ctx);

/* These two must ONLY be called for provider side operations */
int evp_pkey_ctx_ctrl_to_param(EVP_PKEY_CTX *ctx,
    int keytype, int optype,
    int cmd, int p1, void *p2);
int evp_pkey_ctx_ctrl_str_to_param(EVP_PKEY_CTX *ctx,
    const char *name, const char *value);

/* These two must ONLY be called for legacy operations */
int evp_pkey_ctx_set_params_to_ctrl(EVP_PKEY_CTX *ctx, const OSSL_PARAM *params);
int evp_pkey_ctx_get_params_to_ctrl(EVP_PKEY_CTX *ctx, OSSL_PARAM *params);

/* This must ONLY be called for legacy EVP_PKEYs */
int evp_pkey_get_params_to_ctrl(const EVP_PKEY *pkey, OSSL_PARAM *params);

/* Same as the public get0 functions but are not const */
#ifndef OPENSSL_NO_DEPRECATED_3_0
DH *evp_pkey_get0_DH_int(const EVP_PKEY *pkey);
EC_KEY *evp_pkey_get0_EC_KEY_int(const EVP_PKEY *pkey);
RSA *evp_pkey_get0_RSA_int(const EVP_PKEY *pkey);
#endif

/* Get internal identification number routines */
int evp_asym_cipher_get_number(const EVP_ASYM_CIPHER *cipher);
int evp_cipher_get_number(const EVP_CIPHER *cipher);
int evp_kdf_get_number(const EVP_KDF *kdf);
int evp_kem_get_number(const EVP_KEM *wrap);
int evp_keyexch_get_number(const EVP_KEYEXCH *keyexch);
int evp_keymgmt_get_number(const EVP_KEYMGMT *keymgmt);
int evp_keymgmt_get_legacy_alg(const EVP_KEYMGMT *keymgmt);
int evp_mac_get_number(const EVP_MAC *mac);
int evp_md_get_number(const EVP_MD *md);
int evp_rand_get_number(const EVP_RAND *rand);
int evp_rand_can_seed(EVP_RAND_CTX *ctx);
size_t evp_rand_get_seed(EVP_RAND_CTX *ctx,
    unsigned char **buffer,
    int entropy, size_t min_len, size_t max_len,
    int prediction_resistance,
    const unsigned char *adin, size_t adin_len);
void evp_rand_clear_seed(EVP_RAND_CTX *ctx,
    unsigned char *buffer, size_t b_len);
int evp_signature_get_number(const EVP_SIGNATURE *signature);

int evp_pkey_decrypt_alloc(EVP_PKEY_CTX *ctx, unsigned char **outp,
    size_t *outlenp, size_t expected_outlen,
    const unsigned char *in, size_t inlen);

int ossl_md2hmacnid(int mdnid);
int ossl_hmac2mdnid(int hmac_nid);

const EVP_PKEY_ASN1_METHOD *evp_pkey_asn1_find(int type);
const EVP_PKEY_ASN1_METHOD *evp_pkey_asn1_find_str(const char *str, int len);
int evp_pkey_asn1_get_count(void);
const EVP_PKEY_ASN1_METHOD *evp_pkey_asn1_get0(int idx);
int evp_pkey_asn1_get0_info(int *ppkey_id, int *ppkey_base_id,
    int *ppkey_flags, const char **pinfo,
    const char **ppem_str,
    const EVP_PKEY_ASN1_METHOD *ameth);
const EVP_PKEY_ASN1_METHOD *evp_pkey_get0_asn1(const EVP_PKEY *pkey);

#endif /* OSSL_CRYPTO_EVP_H */
