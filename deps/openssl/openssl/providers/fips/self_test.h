/*
 * Copyright 2019-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#if !defined(OSSL_PROVIDERS_FIPS_SELF_TEST_H)
#define OSSL_PROVIDERS_FIPS_SELF_TEST_H

#include <openssl/core_dispatch.h>
#include <openssl/types.h>
#include <openssl/self_test.h>
#include "internal/fips.h"

typedef struct self_test_post_params_st {
    /* FIPS module integrity check parameters */
    const char *module_filename; /* Module file to perform MAC on */
    const char *module_checksum_data; /* Expected module MAC integrity */

    /* Used to decide whether to defer tests or not */
    unsigned char defer_tests;

    /* BIO callbacks supplied to the FIPS provider */
    OSSL_FUNC_BIO_new_file_fn *bio_new_file_cb;
    OSSL_FUNC_BIO_new_membuf_fn *bio_new_buffer_cb;
    OSSL_FUNC_BIO_read_ex_fn *bio_read_ex_cb;
    OSSL_FUNC_BIO_free_fn *bio_free_cb;
    OSSL_CALLBACK *cb;
    void *cb_arg;
    OSSL_LIB_CTX *libctx;

} SELF_TEST_POST_PARAMS;

int SELF_TEST_post(SELF_TEST_POST_PARAMS *st, void *fips_global,
    int on_demand_test);
int SELF_TEST_kats_execute(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx,
    self_test_id_t id, int switch_rand);
int SELF_TEST_kats(OSSL_SELF_TEST *st, OSSL_LIB_CTX *libctx);
int SELF_TEST_lock_deferred(void *fips_global);
void SELF_TEST_unlock_deferred(void *fips_global);

void SELF_TEST_disable_conditional_error_state(void);

/* KAT tests categories */
enum st_test_category {
    SELF_TEST_INTEGRITY = 0, /* currently unused */
    SELF_TEST_KAT_DIGEST,
    SELF_TEST_KAT_CIPHER,
    SELF_TEST_KAT_SIGNATURE,
    SELF_TEST_KAT_KDF,
    SELF_TEST_DRBG,
    SELF_TEST_KAT_KAS,
    SELF_TEST_KAT_ASYM_KEYGEN,
    SELF_TEST_KAT_KEM,
    SELF_TEST_KAT_ASYM_CIPHER,
    SELF_TEST_KAT_MAC,
};

enum st_test_state {
    SELF_TEST_STATE_INIT = 0, /* Test has not been execute yet */
    SELF_TEST_STATE_IN_PROGRESS, /* Test is currently being executed */
    SELF_TEST_STATE_PASSED, /* Test is marked as passed */
    SELF_TEST_STATE_FAILED, /* Test failed */
    SELF_TEST_STATE_IMPLICIT, /* Marks test as implicitly handled */
    SELF_TEST_STATE_DEFER, /* Like INIT, but mark test as deferred */
};

/* used to store raw parameters for keys and algorithms */
typedef struct st_kat_param_st {
    const char *name; /* an OSSL_PARAM name */
    size_t type; /* the type associated with the data */
    const void *data; /* unsigned char [], or char [] depending on the type */
    size_t data_len; /* the length of the data */
} ST_KAT_PARAM;

typedef struct st_const_buffer_st {
    const unsigned char *buf;
    size_t len;
} ST_BUFFER;

#define CIPHER_MODE_ENCRYPT 1
#define CIPHER_MODE_DECRYPT 2
#define CIPHER_MODE_ALL (CIPHER_MODE_ENCRYPT | CIPHER_MODE_DECRYPT)

typedef struct st_kat_cipher_st {
    int mode;
    ST_BUFFER key;
    ST_BUFFER iv;
    ST_BUFFER aad;
    ST_BUFFER tag;
} ST_KAT_CIPHER;

typedef struct st_kat_asym_cipher_st {
    int encrypt;
    const ST_KAT_PARAM *key;
    const ST_KAT_PARAM *postinit;
} ST_KAT_ASYM_CIPHER;

typedef struct st_kat_keygen_st {
    const ST_KAT_PARAM *keygen_params;
    const ST_KAT_PARAM *expected_params;
} ST_KAT_ASYM_KEYGEN;

typedef struct st_kat_kem_st {
    const ST_KAT_PARAM *key;
    ST_BUFFER cipher_text;
    ST_BUFFER entropy;
    ST_BUFFER secret;
    ST_BUFFER reject_secret;
} ST_KAT_KEM;

/* FIPS 140-3 only allows DSA verification for legacy purposes */
#define SIGNATURE_MODE_VERIFY_ONLY 1
#define SIGNATURE_MODE_SIGN_ONLY 2
#define SIGNATURE_MODE_DIGESTED 4
#define SIGNATURE_MODE_SIG_DIGESTED 8

typedef struct st_kat_sign_st {
    const char *keytype;
    int mode;
    const ST_KAT_PARAM *key;
    ST_BUFFER entropy;
    ST_BUFFER nonce;
    ST_BUFFER persstr;
    const ST_KAT_PARAM *init;
    const ST_KAT_PARAM *verify;
} ST_KAT_SIGN;

typedef struct st_kat_kdf_st {
    const ST_KAT_PARAM *params;
} ST_KAT_KDF;

typedef struct st_kat_kas_st {
    const ST_KAT_PARAM *key_group;
    const ST_KAT_PARAM *key_host_data;
    const ST_KAT_PARAM *key_peer_data;
} ST_KAT_KAS;

typedef struct st_kat_drbg_st {
    const char *param_name;
    const char *param_value;
    ST_BUFFER entropyin;
    ST_BUFFER nonce;
    ST_BUFFER persstr;
    ST_BUFFER entropyinpr1;
    ST_BUFFER entropyinpr2;
    ST_BUFFER entropyaddin1;
    ST_BUFFER entropyaddin2;
} ST_KAT_DRBG;

typedef struct st_kat_mac_st {
    const ST_KAT_PARAM *params;
} ST_KAT_MAC;

typedef struct self_test_st {
    self_test_id_t id;
    const char *algorithm;
    const char *desc;
    enum st_test_category category;
    enum st_test_state state;
    ST_BUFFER pt;
    ST_BUFFER expected; /* Set to NULL if this value changes */
    union {
        ST_KAT_CIPHER cipher;
        ST_KAT_ASYM_CIPHER ac;
        ST_KAT_ASYM_KEYGEN akgen;
        ST_KAT_KEM kem;
        ST_KAT_SIGN sig;
        ST_KAT_KDF kdf;
        ST_KAT_KAS kas;
        ST_KAT_DRBG drbg;
        ST_KAT_MAC mac;
    } u;
    const self_test_id_t *depends_on;
} ST_DEFINITION;

extern ST_DEFINITION st_all_tests[ST_ID_MAX];
int ossl_get_self_test_state(self_test_id_t id, enum st_test_state *state);
int ossl_set_self_test_state(self_test_id_t id, enum st_test_state state);

#endif /* !defined(OSSL_PROVIDERS_FIPS_SELF_TEST_H) */
