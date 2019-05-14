/*
 * Copyright 2015-2019 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <openssl/pkcs12.h>
#include <openssl/kdf.h>
#include "internal/numbers.h"
#include "testutil.h"
#include "evp_test.h"


typedef struct evp_test_method_st EVP_TEST_METHOD;

/*
 * Structure holding test information
 */
typedef struct evp_test_st {
    STANZA s;                     /* Common test stanza */
    char *name;
    int skip;                     /* Current test should be skipped */
    const EVP_TEST_METHOD *meth;  /* method for this test */
    const char *err, *aux_err;    /* Error string for test */
    char *expected_err;           /* Expected error value of test */
    char *func;                   /* Expected error function string */
    char *reason;                 /* Expected error reason string */
    void *data;                   /* test specific data */
} EVP_TEST;

/*
 * Test method structure
 */
struct evp_test_method_st {
    /* Name of test as it appears in file */
    const char *name;
    /* Initialise test for "alg" */
    int (*init) (EVP_TEST * t, const char *alg);
    /* Clean up method */
    void (*cleanup) (EVP_TEST * t);
    /* Test specific name value pair processing */
    int (*parse) (EVP_TEST * t, const char *name, const char *value);
    /* Run the test itself */
    int (*run_test) (EVP_TEST * t);
};


/*
 * Linked list of named keys.
 */
typedef struct key_list_st {
    char *name;
    EVP_PKEY *key;
    struct key_list_st *next;
} KEY_LIST;

/*
 * List of public and private keys
 */
static KEY_LIST *private_keys;
static KEY_LIST *public_keys;
static int find_key(EVP_PKEY **ppk, const char *name, KEY_LIST *lst);

static int parse_bin(const char *value, unsigned char **buf, size_t *buflen);

/*
 * Compare two memory regions for equality, returning zero if they differ.
 * However, if there is expected to be an error and the actual error
 * matches then the memory is expected to be different so handle this
 * case without producing unnecessary test framework output.
 */
static int memory_err_compare(EVP_TEST *t, const char *err,
                              const void *expected, size_t expected_len,
                              const void *got, size_t got_len)
{
    int r;

    if (t->expected_err != NULL && strcmp(t->expected_err, err) == 0)
        r = !TEST_mem_ne(expected, expected_len, got, got_len);
    else
        r = TEST_mem_eq(expected, expected_len, got, got_len);
    if (!r)
        t->err = err;
    return r;
}

/*
 * Structure used to hold a list of blocks of memory to test
 * calls to "update" like functions.
 */
struct evp_test_buffer_st {
    unsigned char *buf;
    size_t buflen;
    size_t count;
    int count_set;
};

static void evp_test_buffer_free(EVP_TEST_BUFFER *db)
{
    if (db != NULL) {
        OPENSSL_free(db->buf);
        OPENSSL_free(db);
    }
}

/*
 * append buffer to a list
 */
static int evp_test_buffer_append(const char *value,
                                  STACK_OF(EVP_TEST_BUFFER) **sk)
{
    EVP_TEST_BUFFER *db = NULL;

    if (!TEST_ptr(db = OPENSSL_malloc(sizeof(*db))))
        goto err;

    if (!parse_bin(value, &db->buf, &db->buflen))
        goto err;
    db->count = 1;
    db->count_set = 0;

    if (*sk == NULL && !TEST_ptr(*sk = sk_EVP_TEST_BUFFER_new_null()))
        goto err;
    if (!sk_EVP_TEST_BUFFER_push(*sk, db))
        goto err;

    return 1;

err:
    evp_test_buffer_free(db);
    return 0;
}

/*
 * replace last buffer in list with copies of itself
 */
static int evp_test_buffer_ncopy(const char *value,
                                 STACK_OF(EVP_TEST_BUFFER) *sk)
{
    EVP_TEST_BUFFER *db;
    unsigned char *tbuf, *p;
    size_t tbuflen;
    int ncopy = atoi(value);
    int i;

    if (ncopy <= 0)
        return 0;
    if (sk == NULL || sk_EVP_TEST_BUFFER_num(sk) == 0)
        return 0;
    db = sk_EVP_TEST_BUFFER_value(sk, sk_EVP_TEST_BUFFER_num(sk) - 1);

    tbuflen = db->buflen * ncopy;
    if (!TEST_ptr(tbuf = OPENSSL_malloc(tbuflen)))
        return 0;
    for (i = 0, p = tbuf; i < ncopy; i++, p += db->buflen)
        memcpy(p, db->buf, db->buflen);

    OPENSSL_free(db->buf);
    db->buf = tbuf;
    db->buflen = tbuflen;
    return 1;
}

/*
 * set repeat count for last buffer in list
 */
static int evp_test_buffer_set_count(const char *value,
                                     STACK_OF(EVP_TEST_BUFFER) *sk)
{
    EVP_TEST_BUFFER *db;
    int count = atoi(value);

    if (count <= 0)
        return 0;

    if (sk == NULL || sk_EVP_TEST_BUFFER_num(sk) == 0)
        return 0;

    db = sk_EVP_TEST_BUFFER_value(sk, sk_EVP_TEST_BUFFER_num(sk) - 1);
    if (db->count_set != 0)
        return 0;

    db->count = (size_t)count;
    db->count_set = 1;
    return 1;
}

/*
 * call "fn" with each element of the list in turn
 */
static int evp_test_buffer_do(STACK_OF(EVP_TEST_BUFFER) *sk,
                              int (*fn)(void *ctx,
                                        const unsigned char *buf,
                                        size_t buflen),
                              void *ctx)
{
    int i;

    for (i = 0; i < sk_EVP_TEST_BUFFER_num(sk); i++) {
        EVP_TEST_BUFFER *tb = sk_EVP_TEST_BUFFER_value(sk, i);
        size_t j;

        for (j = 0; j < tb->count; j++) {
            if (fn(ctx, tb->buf, tb->buflen) <= 0)
                return 0;
        }
    }
    return 1;
}

/*
 * Unescape some sequences in string literals (only \n for now).
 * Return an allocated buffer, set |out_len|.  If |input_len|
 * is zero, get an empty buffer but set length to zero.
 */
static unsigned char* unescape(const char *input, size_t input_len,
                               size_t *out_len)
{
    unsigned char *ret, *p;
    size_t i;

    if (input_len == 0) {
        *out_len = 0;
        return OPENSSL_zalloc(1);
    }

    /* Escaping is non-expanding; over-allocate original size for simplicity. */
    if (!TEST_ptr(ret = p = OPENSSL_malloc(input_len)))
        return NULL;

    for (i = 0; i < input_len; i++) {
        if (*input == '\\') {
            if (i == input_len - 1 || *++input != 'n') {
                TEST_error("Bad escape sequence in file");
                goto err;
            }
            *p++ = '\n';
            i++;
            input++;
        } else {
            *p++ = *input++;
        }
    }

    *out_len = p - ret;
    return ret;

 err:
    OPENSSL_free(ret);
    return NULL;
}

/*
 * For a hex string "value" convert to a binary allocated buffer.
 * Return 1 on success or 0 on failure.
 */
static int parse_bin(const char *value, unsigned char **buf, size_t *buflen)
{
    long len;

    /* Check for NULL literal */
    if (strcmp(value, "NULL") == 0) {
        *buf = NULL;
        *buflen = 0;
        return 1;
    }

    /* Check for empty value */
    if (*value == '\0') {
        /*
         * Don't return NULL for zero length buffer. This is needed for
         * some tests with empty keys: HMAC_Init_ex() expects a non-NULL key
         * buffer even if the key length is 0, in order to detect key reset.
         */
        *buf = OPENSSL_malloc(1);
        if (*buf == NULL)
            return 0;
        **buf = 0;
        *buflen = 0;
        return 1;
    }

    /* Check for string literal */
    if (value[0] == '"') {
        size_t vlen = strlen(++value);

        if (vlen == 0 || value[vlen - 1] != '"')
            return 0;
        vlen--;
        *buf = unescape(value, vlen, buflen);
        return *buf == NULL ? 0 : 1;
    }

    /* Otherwise assume as hex literal and convert it to binary buffer */
    if (!TEST_ptr(*buf = OPENSSL_hexstr2buf(value, &len))) {
        TEST_info("Can't convert %s", value);
        TEST_openssl_errors();
        return -1;
    }
    /* Size of input buffer means we'll never overflow */
    *buflen = len;
    return 1;
}


/**
***  MESSAGE DIGEST TESTS
**/

typedef struct digest_data_st {
    /* Digest this test is for */
    const EVP_MD *digest;
    /* Input to digest */
    STACK_OF(EVP_TEST_BUFFER) *input;
    /* Expected output */
    unsigned char *output;
    size_t output_len;
} DIGEST_DATA;

static int digest_test_init(EVP_TEST *t, const char *alg)
{
    DIGEST_DATA *mdat;
    const EVP_MD *digest;

    if ((digest = EVP_get_digestbyname(alg)) == NULL) {
        /* If alg has an OID assume disabled algorithm */
        if (OBJ_sn2nid(alg) != NID_undef || OBJ_ln2nid(alg) != NID_undef) {
            t->skip = 1;
            return 1;
        }
        return 0;
    }
    if (!TEST_ptr(mdat = OPENSSL_zalloc(sizeof(*mdat))))
        return 0;
    t->data = mdat;
    mdat->digest = digest;
    return 1;
}

static void digest_test_cleanup(EVP_TEST *t)
{
    DIGEST_DATA *mdat = t->data;

    sk_EVP_TEST_BUFFER_pop_free(mdat->input, evp_test_buffer_free);
    OPENSSL_free(mdat->output);
}

static int digest_test_parse(EVP_TEST *t,
                             const char *keyword, const char *value)
{
    DIGEST_DATA *mdata = t->data;

    if (strcmp(keyword, "Input") == 0)
        return evp_test_buffer_append(value, &mdata->input);
    if (strcmp(keyword, "Output") == 0)
        return parse_bin(value, &mdata->output, &mdata->output_len);
    if (strcmp(keyword, "Count") == 0)
        return evp_test_buffer_set_count(value, mdata->input);
    if (strcmp(keyword, "Ncopy") == 0)
        return evp_test_buffer_ncopy(value, mdata->input);
    return 0;
}

static int digest_update_fn(void *ctx, const unsigned char *buf, size_t buflen)
{
    return EVP_DigestUpdate(ctx, buf, buflen);
}

static int digest_test_run(EVP_TEST *t)
{
    DIGEST_DATA *expected = t->data;
    EVP_MD_CTX *mctx;
    unsigned char *got = NULL;
    unsigned int got_len;

    t->err = "TEST_FAILURE";
    if (!TEST_ptr(mctx = EVP_MD_CTX_new()))
        goto err;

    got = OPENSSL_malloc(expected->output_len > EVP_MAX_MD_SIZE ?
                         expected->output_len : EVP_MAX_MD_SIZE);
    if (!TEST_ptr(got))
        goto err;

    if (!EVP_DigestInit_ex(mctx, expected->digest, NULL)) {
        t->err = "DIGESTINIT_ERROR";
        goto err;
    }
    if (!evp_test_buffer_do(expected->input, digest_update_fn, mctx)) {
        t->err = "DIGESTUPDATE_ERROR";
        goto err;
    }

    if (EVP_MD_flags(expected->digest) & EVP_MD_FLAG_XOF) {
        got_len = expected->output_len;
        if (!EVP_DigestFinalXOF(mctx, got, got_len)) {
            t->err = "DIGESTFINALXOF_ERROR";
            goto err;
        }
    } else {
        if (!EVP_DigestFinal(mctx, got, &got_len)) {
            t->err = "DIGESTFINAL_ERROR";
            goto err;
        }
    }
    if (!TEST_int_eq(expected->output_len, got_len)) {
        t->err = "DIGEST_LENGTH_MISMATCH";
        goto err;
    }
    if (!memory_err_compare(t, "DIGEST_MISMATCH",
                            expected->output, expected->output_len,
                            got, got_len))
        goto err;

    t->err = NULL;

 err:
    OPENSSL_free(got);
    EVP_MD_CTX_free(mctx);
    return 1;
}

static const EVP_TEST_METHOD digest_test_method = {
    "Digest",
    digest_test_init,
    digest_test_cleanup,
    digest_test_parse,
    digest_test_run
};


/**
***  CIPHER TESTS
**/

typedef struct cipher_data_st {
    const EVP_CIPHER *cipher;
    int enc;
    /* EVP_CIPH_GCM_MODE, EVP_CIPH_CCM_MODE or EVP_CIPH_OCB_MODE if AEAD */
    int aead;
    unsigned char *key;
    size_t key_len;
    unsigned char *iv;
    size_t iv_len;
    unsigned char *plaintext;
    size_t plaintext_len;
    unsigned char *ciphertext;
    size_t ciphertext_len;
    /* GCM, CCM and OCB only */
    unsigned char *aad;
    size_t aad_len;
    unsigned char *tag;
    size_t tag_len;
} CIPHER_DATA;

static int cipher_test_init(EVP_TEST *t, const char *alg)
{
    const EVP_CIPHER *cipher;
    CIPHER_DATA *cdat;
    int m;

    if ((cipher = EVP_get_cipherbyname(alg)) == NULL) {
        /* If alg has an OID assume disabled algorithm */
        if (OBJ_sn2nid(alg) != NID_undef || OBJ_ln2nid(alg) != NID_undef) {
            t->skip = 1;
            return 1;
        }
        return 0;
    }
    cdat = OPENSSL_zalloc(sizeof(*cdat));
    cdat->cipher = cipher;
    cdat->enc = -1;
    m = EVP_CIPHER_mode(cipher);
    if (m == EVP_CIPH_GCM_MODE
            || m == EVP_CIPH_OCB_MODE
            || m == EVP_CIPH_CCM_MODE)
        cdat->aead = m;
    else if (EVP_CIPHER_flags(cipher) & EVP_CIPH_FLAG_AEAD_CIPHER)
        cdat->aead = -1;
    else
        cdat->aead = 0;

    t->data = cdat;
    return 1;
}

static void cipher_test_cleanup(EVP_TEST *t)
{
    CIPHER_DATA *cdat = t->data;

    OPENSSL_free(cdat->key);
    OPENSSL_free(cdat->iv);
    OPENSSL_free(cdat->ciphertext);
    OPENSSL_free(cdat->plaintext);
    OPENSSL_free(cdat->aad);
    OPENSSL_free(cdat->tag);
}

static int cipher_test_parse(EVP_TEST *t, const char *keyword,
                             const char *value)
{
    CIPHER_DATA *cdat = t->data;

    if (strcmp(keyword, "Key") == 0)
        return parse_bin(value, &cdat->key, &cdat->key_len);
    if (strcmp(keyword, "IV") == 0)
        return parse_bin(value, &cdat->iv, &cdat->iv_len);
    if (strcmp(keyword, "Plaintext") == 0)
        return parse_bin(value, &cdat->plaintext, &cdat->plaintext_len);
    if (strcmp(keyword, "Ciphertext") == 0)
        return parse_bin(value, &cdat->ciphertext, &cdat->ciphertext_len);
    if (cdat->aead) {
        if (strcmp(keyword, "AAD") == 0)
            return parse_bin(value, &cdat->aad, &cdat->aad_len);
        if (strcmp(keyword, "Tag") == 0)
            return parse_bin(value, &cdat->tag, &cdat->tag_len);
    }

    if (strcmp(keyword, "Operation") == 0) {
        if (strcmp(value, "ENCRYPT") == 0)
            cdat->enc = 1;
        else if (strcmp(value, "DECRYPT") == 0)
            cdat->enc = 0;
        else
            return 0;
        return 1;
    }
    return 0;
}

static int cipher_test_enc(EVP_TEST *t, int enc,
                           size_t out_misalign, size_t inp_misalign, int frag)
{
    CIPHER_DATA *expected = t->data;
    unsigned char *in, *expected_out, *tmp = NULL;
    size_t in_len, out_len, donelen = 0;
    int ok = 0, tmplen, chunklen, tmpflen;
    EVP_CIPHER_CTX *ctx = NULL;

    t->err = "TEST_FAILURE";
    if (!TEST_ptr(ctx = EVP_CIPHER_CTX_new()))
        goto err;
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
    if (enc) {
        in = expected->plaintext;
        in_len = expected->plaintext_len;
        expected_out = expected->ciphertext;
        out_len = expected->ciphertext_len;
    } else {
        in = expected->ciphertext;
        in_len = expected->ciphertext_len;
        expected_out = expected->plaintext;
        out_len = expected->plaintext_len;
    }
    if (inp_misalign == (size_t)-1) {
        /*
         * Exercise in-place encryption
         */
        tmp = OPENSSL_malloc(out_misalign + in_len + 2 * EVP_MAX_BLOCK_LENGTH);
        if (!tmp)
            goto err;
        in = memcpy(tmp + out_misalign, in, in_len);
    } else {
        inp_misalign += 16 - ((out_misalign + in_len) & 15);
        /*
         * 'tmp' will store both output and copy of input. We make the copy
         * of input to specifically aligned part of 'tmp'. So we just
         * figured out how much padding would ensure the required alignment,
         * now we allocate extended buffer and finally copy the input just
         * past inp_misalign in expression below. Output will be written
         * past out_misalign...
         */
        tmp = OPENSSL_malloc(out_misalign + in_len + 2 * EVP_MAX_BLOCK_LENGTH +
                             inp_misalign + in_len);
        if (!tmp)
            goto err;
        in = memcpy(tmp + out_misalign + in_len + 2 * EVP_MAX_BLOCK_LENGTH +
                    inp_misalign, in, in_len);
    }
    if (!EVP_CipherInit_ex(ctx, expected->cipher, NULL, NULL, NULL, enc)) {
        t->err = "CIPHERINIT_ERROR";
        goto err;
    }
    if (expected->iv) {
        if (expected->aead) {
            if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN,
                                     expected->iv_len, 0)) {
                t->err = "INVALID_IV_LENGTH";
                goto err;
            }
        } else if (expected->iv_len != (size_t)EVP_CIPHER_CTX_iv_length(ctx)) {
            t->err = "INVALID_IV_LENGTH";
            goto err;
        }
    }
    if (expected->aead) {
        unsigned char *tag;
        /*
         * If encrypting or OCB just set tag length initially, otherwise
         * set tag length and value.
         */
        if (enc || expected->aead == EVP_CIPH_OCB_MODE) {
            t->err = "TAG_LENGTH_SET_ERROR";
            tag = NULL;
        } else {
            t->err = "TAG_SET_ERROR";
            tag = expected->tag;
        }
        if (tag || expected->aead != EVP_CIPH_GCM_MODE) {
            if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
                                     expected->tag_len, tag))
                goto err;
        }
    }

    if (!EVP_CIPHER_CTX_set_key_length(ctx, expected->key_len)) {
        t->err = "INVALID_KEY_LENGTH";
        goto err;
    }
    if (!EVP_CipherInit_ex(ctx, NULL, NULL, expected->key, expected->iv, -1)) {
        t->err = "KEY_SET_ERROR";
        goto err;
    }

    if (!enc && expected->aead == EVP_CIPH_OCB_MODE) {
        if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
                                 expected->tag_len, expected->tag)) {
            t->err = "TAG_SET_ERROR";
            goto err;
        }
    }

    if (expected->aead == EVP_CIPH_CCM_MODE) {
        if (!EVP_CipherUpdate(ctx, NULL, &tmplen, NULL, out_len)) {
            t->err = "CCM_PLAINTEXT_LENGTH_SET_ERROR";
            goto err;
        }
    }
    if (expected->aad) {
        t->err = "AAD_SET_ERROR";
        if (!frag) {
            if (!EVP_CipherUpdate(ctx, NULL, &chunklen, expected->aad,
                                  expected->aad_len))
                goto err;
        } else {
            /*
             * Supply the AAD in chunks less than the block size where possible
             */
            if (expected->aad_len > 0) {
                if (!EVP_CipherUpdate(ctx, NULL, &chunklen, expected->aad, 1))
                    goto err;
                donelen++;
            }
            if (expected->aad_len > 2) {
                if (!EVP_CipherUpdate(ctx, NULL, &chunklen,
                                      expected->aad + donelen,
                                      expected->aad_len - 2))
                    goto err;
                donelen += expected->aad_len - 2;
            }
            if (expected->aad_len > 1
                    && !EVP_CipherUpdate(ctx, NULL, &chunklen,
                                         expected->aad + donelen, 1))
                goto err;
        }
    }
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    t->err = "CIPHERUPDATE_ERROR";
    tmplen = 0;
    if (!frag) {
        /* We supply the data all in one go */
        if (!EVP_CipherUpdate(ctx, tmp + out_misalign, &tmplen, in, in_len))
            goto err;
    } else {
        /* Supply the data in chunks less than the block size where possible */
        if (in_len > 0) {
            if (!EVP_CipherUpdate(ctx, tmp + out_misalign, &chunklen, in, 1))
                goto err;
            tmplen += chunklen;
            in++;
            in_len--;
        }
        if (in_len > 1) {
            if (!EVP_CipherUpdate(ctx, tmp + out_misalign + tmplen, &chunklen,
                                  in, in_len - 1))
                goto err;
            tmplen += chunklen;
            in += in_len - 1;
            in_len = 1;
        }
        if (in_len > 0 ) {
            if (!EVP_CipherUpdate(ctx, tmp + out_misalign + tmplen, &chunklen,
                                  in, 1))
                goto err;
            tmplen += chunklen;
        }
    }
    if (!EVP_CipherFinal_ex(ctx, tmp + out_misalign + tmplen, &tmpflen)) {
        t->err = "CIPHERFINAL_ERROR";
        goto err;
    }
    if (!memory_err_compare(t, "VALUE_MISMATCH", expected_out, out_len,
                            tmp + out_misalign, tmplen + tmpflen))
        goto err;
    if (enc && expected->aead) {
        unsigned char rtag[16];

        if (!TEST_size_t_le(expected->tag_len, sizeof(rtag))) {
            t->err = "TAG_LENGTH_INTERNAL_ERROR";
            goto err;
        }
        if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG,
                                 expected->tag_len, rtag)) {
            t->err = "TAG_RETRIEVE_ERROR";
            goto err;
        }
        if (!memory_err_compare(t, "TAG_VALUE_MISMATCH",
                                expected->tag, expected->tag_len,
                                rtag, expected->tag_len))
            goto err;
    }
    t->err = NULL;
    ok = 1;
 err:
    OPENSSL_free(tmp);
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

static int cipher_test_run(EVP_TEST *t)
{
    CIPHER_DATA *cdat = t->data;
    int rv, frag = 0;
    size_t out_misalign, inp_misalign;

    if (!cdat->key) {
        t->err = "NO_KEY";
        return 0;
    }
    if (!cdat->iv && EVP_CIPHER_iv_length(cdat->cipher)) {
        /* IV is optional and usually omitted in wrap mode */
        if (EVP_CIPHER_mode(cdat->cipher) != EVP_CIPH_WRAP_MODE) {
            t->err = "NO_IV";
            return 0;
        }
    }
    if (cdat->aead && !cdat->tag) {
        t->err = "NO_TAG";
        return 0;
    }
    for (out_misalign = 0; out_misalign <= 1;) {
        static char aux_err[64];
        t->aux_err = aux_err;
        for (inp_misalign = (size_t)-1; inp_misalign != 2; inp_misalign++) {
            if (inp_misalign == (size_t)-1) {
                /* kludge: inp_misalign == -1 means "exercise in-place" */
                BIO_snprintf(aux_err, sizeof(aux_err),
                             "%s in-place, %sfragmented",
                             out_misalign ? "misaligned" : "aligned",
                             frag ? "" : "not ");
            } else {
                BIO_snprintf(aux_err, sizeof(aux_err),
                             "%s output and %s input, %sfragmented",
                             out_misalign ? "misaligned" : "aligned",
                             inp_misalign ? "misaligned" : "aligned",
                             frag ? "" : "not ");
            }
            if (cdat->enc) {
                rv = cipher_test_enc(t, 1, out_misalign, inp_misalign, frag);
                /* Not fatal errors: return */
                if (rv != 1) {
                    if (rv < 0)
                        return 0;
                    return 1;
                }
            }
            if (cdat->enc != 1) {
                rv = cipher_test_enc(t, 0, out_misalign, inp_misalign, frag);
                /* Not fatal errors: return */
                if (rv != 1) {
                    if (rv < 0)
                        return 0;
                    return 1;
                }
            }
        }

        if (out_misalign == 1 && frag == 0) {
            /*
             * XTS, CCM and Wrap modes have special requirements about input
             * lengths so we don't fragment for those
             */
            if (cdat->aead == EVP_CIPH_CCM_MODE
                    || EVP_CIPHER_mode(cdat->cipher) == EVP_CIPH_XTS_MODE
                    || EVP_CIPHER_mode(cdat->cipher) == EVP_CIPH_WRAP_MODE)
                break;
            out_misalign = 0;
            frag++;
        } else {
            out_misalign++;
        }
    }
    t->aux_err = NULL;

    return 1;
}

static const EVP_TEST_METHOD cipher_test_method = {
    "Cipher",
    cipher_test_init,
    cipher_test_cleanup,
    cipher_test_parse,
    cipher_test_run
};


/**
***  MAC TESTS
**/

typedef struct mac_data_st {
    /* MAC type */
    int type;
    /* Algorithm string for this MAC */
    char *alg;
    /* MAC key */
    unsigned char *key;
    size_t key_len;
    /* Input to MAC */
    unsigned char *input;
    size_t input_len;
    /* Expected output */
    unsigned char *output;
    size_t output_len;
    /* Collection of controls */
    STACK_OF(OPENSSL_STRING) *controls;
} MAC_DATA;

static int mac_test_init(EVP_TEST *t, const char *alg)
{
    int type;
    MAC_DATA *mdat;

    if (strcmp(alg, "HMAC") == 0) {
        type = EVP_PKEY_HMAC;
    } else if (strcmp(alg, "CMAC") == 0) {
#ifndef OPENSSL_NO_CMAC
        type = EVP_PKEY_CMAC;
#else
        t->skip = 1;
        return 1;
#endif
    } else if (strcmp(alg, "Poly1305") == 0) {
#ifndef OPENSSL_NO_POLY1305
        type = EVP_PKEY_POLY1305;
#else
        t->skip = 1;
        return 1;
#endif
    } else if (strcmp(alg, "SipHash") == 0) {
#ifndef OPENSSL_NO_SIPHASH
        type = EVP_PKEY_SIPHASH;
#else
        t->skip = 1;
        return 1;
#endif
    } else
        return 0;

    mdat = OPENSSL_zalloc(sizeof(*mdat));
    mdat->type = type;
    mdat->controls = sk_OPENSSL_STRING_new_null();
    t->data = mdat;
    return 1;
}

/* Because OPENSSL_free is a macro, it can't be passed as a function pointer */
static void openssl_free(char *m)
{
    OPENSSL_free(m);
}

static void mac_test_cleanup(EVP_TEST *t)
{
    MAC_DATA *mdat = t->data;

    sk_OPENSSL_STRING_pop_free(mdat->controls, openssl_free);
    OPENSSL_free(mdat->alg);
    OPENSSL_free(mdat->key);
    OPENSSL_free(mdat->input);
    OPENSSL_free(mdat->output);
}

static int mac_test_parse(EVP_TEST *t,
                          const char *keyword, const char *value)
{
    MAC_DATA *mdata = t->data;

    if (strcmp(keyword, "Key") == 0)
        return parse_bin(value, &mdata->key, &mdata->key_len);
    if (strcmp(keyword, "Algorithm") == 0) {
        mdata->alg = OPENSSL_strdup(value);
        if (!mdata->alg)
            return 0;
        return 1;
    }
    if (strcmp(keyword, "Input") == 0)
        return parse_bin(value, &mdata->input, &mdata->input_len);
    if (strcmp(keyword, "Output") == 0)
        return parse_bin(value, &mdata->output, &mdata->output_len);
    if (strcmp(keyword, "Ctrl") == 0)
        return sk_OPENSSL_STRING_push(mdata->controls,
                                      OPENSSL_strdup(value)) != 0;
    return 0;
}

static int mac_test_ctrl_pkey(EVP_TEST *t, EVP_PKEY_CTX *pctx,
                              const char *value)
{
    int rv;
    char *p, *tmpval;

    if (!TEST_ptr(tmpval = OPENSSL_strdup(value)))
        return 0;
    p = strchr(tmpval, ':');
    if (p != NULL)
        *p++ = '\0';
    rv = EVP_PKEY_CTX_ctrl_str(pctx, tmpval, p);
    if (rv == -2)
        t->err = "PKEY_CTRL_INVALID";
    else if (rv <= 0)
        t->err = "PKEY_CTRL_ERROR";
    else
        rv = 1;
    OPENSSL_free(tmpval);
    return rv > 0;
}

static int mac_test_run(EVP_TEST *t)
{
    MAC_DATA *expected = t->data;
    EVP_MD_CTX *mctx = NULL;
    EVP_PKEY_CTX *pctx = NULL, *genctx = NULL;
    EVP_PKEY *key = NULL;
    const EVP_MD *md = NULL;
    unsigned char *got = NULL;
    size_t got_len;
    int i;

#ifdef OPENSSL_NO_DES
    if (expected->alg != NULL && strstr(expected->alg, "DES") != NULL) {
        /* Skip DES */
        t->err = NULL;
        goto err;
    }
#endif

    if (expected->type == EVP_PKEY_CMAC)
        key = EVP_PKEY_new_CMAC_key(NULL, expected->key, expected->key_len,
                                    EVP_get_cipherbyname(expected->alg));
    else
        key = EVP_PKEY_new_raw_private_key(expected->type, NULL, expected->key,
                                           expected->key_len);
    if (key == NULL) {
        t->err = "MAC_KEY_CREATE_ERROR";
        goto err;
    }

    if (expected->type == EVP_PKEY_HMAC) {
        if (!TEST_ptr(md = EVP_get_digestbyname(expected->alg))) {
            t->err = "MAC_ALGORITHM_SET_ERROR";
            goto err;
        }
    }
    if (!TEST_ptr(mctx = EVP_MD_CTX_new())) {
        t->err = "INTERNAL_ERROR";
        goto err;
    }
    if (!EVP_DigestSignInit(mctx, &pctx, md, NULL, key)) {
        t->err = "DIGESTSIGNINIT_ERROR";
        goto err;
    }
    for (i = 0; i < sk_OPENSSL_STRING_num(expected->controls); i++)
        if (!mac_test_ctrl_pkey(t, pctx,
                                sk_OPENSSL_STRING_value(expected->controls,
                                                        i))) {
            t->err = "EVPPKEYCTXCTRL_ERROR";
            goto err;
        }
    if (!EVP_DigestSignUpdate(mctx, expected->input, expected->input_len)) {
        t->err = "DIGESTSIGNUPDATE_ERROR";
        goto err;
    }
    if (!EVP_DigestSignFinal(mctx, NULL, &got_len)) {
        t->err = "DIGESTSIGNFINAL_LENGTH_ERROR";
        goto err;
    }
    if (!TEST_ptr(got = OPENSSL_malloc(got_len))) {
        t->err = "TEST_FAILURE";
        goto err;
    }
    if (!EVP_DigestSignFinal(mctx, got, &got_len)
            || !memory_err_compare(t, "TEST_MAC_ERR",
                                   expected->output, expected->output_len,
                                   got, got_len)) {
        t->err = "TEST_MAC_ERR";
        goto err;
    }
    t->err = NULL;
 err:
    EVP_MD_CTX_free(mctx);
    OPENSSL_free(got);
    EVP_PKEY_CTX_free(genctx);
    EVP_PKEY_free(key);
    return 1;
}

static const EVP_TEST_METHOD mac_test_method = {
    "MAC",
    mac_test_init,
    mac_test_cleanup,
    mac_test_parse,
    mac_test_run
};


/**
***  PUBLIC KEY TESTS
***  These are all very similar and share much common code.
**/

typedef struct pkey_data_st {
    /* Context for this operation */
    EVP_PKEY_CTX *ctx;
    /* Key operation to perform */
    int (*keyop) (EVP_PKEY_CTX *ctx,
                  unsigned char *sig, size_t *siglen,
                  const unsigned char *tbs, size_t tbslen);
    /* Input to MAC */
    unsigned char *input;
    size_t input_len;
    /* Expected output */
    unsigned char *output;
    size_t output_len;
} PKEY_DATA;

/*
 * Perform public key operation setup: lookup key, allocated ctx and call
 * the appropriate initialisation function
 */
static int pkey_test_init(EVP_TEST *t, const char *name,
                          int use_public,
                          int (*keyopinit) (EVP_PKEY_CTX *ctx),
                          int (*keyop)(EVP_PKEY_CTX *ctx,
                                       unsigned char *sig, size_t *siglen,
                                       const unsigned char *tbs,
                                       size_t tbslen))
{
    PKEY_DATA *kdata;
    EVP_PKEY *pkey = NULL;
    int rv = 0;

    if (use_public)
        rv = find_key(&pkey, name, public_keys);
    if (rv == 0)
        rv = find_key(&pkey, name, private_keys);
    if (rv == 0 || pkey == NULL) {
        t->skip = 1;
        return 1;
    }

    if (!TEST_ptr(kdata = OPENSSL_zalloc(sizeof(*kdata)))) {
        EVP_PKEY_free(pkey);
        return 0;
    }
    kdata->keyop = keyop;
    if (!TEST_ptr(kdata->ctx = EVP_PKEY_CTX_new(pkey, NULL))) {
        EVP_PKEY_free(pkey);
        OPENSSL_free(kdata);
        return 0;
    }
    if (keyopinit(kdata->ctx) <= 0)
        t->err = "KEYOP_INIT_ERROR";
    t->data = kdata;
    return 1;
}

static void pkey_test_cleanup(EVP_TEST *t)
{
    PKEY_DATA *kdata = t->data;

    OPENSSL_free(kdata->input);
    OPENSSL_free(kdata->output);
    EVP_PKEY_CTX_free(kdata->ctx);
}

static int pkey_test_ctrl(EVP_TEST *t, EVP_PKEY_CTX *pctx,
                          const char *value)
{
    int rv;
    char *p, *tmpval;

    if (!TEST_ptr(tmpval = OPENSSL_strdup(value)))
        return 0;
    p = strchr(tmpval, ':');
    if (p != NULL)
        *p++ = '\0';
    rv = EVP_PKEY_CTX_ctrl_str(pctx, tmpval, p);
    if (rv == -2) {
        t->err = "PKEY_CTRL_INVALID";
        rv = 1;
    } else if (p != NULL && rv <= 0) {
        /* If p has an OID and lookup fails assume disabled algorithm */
        int nid = OBJ_sn2nid(p);

        if (nid == NID_undef)
             nid = OBJ_ln2nid(p);
        if (nid != NID_undef
                && EVP_get_digestbynid(nid) == NULL
                && EVP_get_cipherbynid(nid) == NULL) {
            t->skip = 1;
            rv = 1;
        } else {
            t->err = "PKEY_CTRL_ERROR";
            rv = 1;
        }
    }
    OPENSSL_free(tmpval);
    return rv > 0;
}

static int pkey_test_parse(EVP_TEST *t,
                           const char *keyword, const char *value)
{
    PKEY_DATA *kdata = t->data;
    if (strcmp(keyword, "Input") == 0)
        return parse_bin(value, &kdata->input, &kdata->input_len);
    if (strcmp(keyword, "Output") == 0)
        return parse_bin(value, &kdata->output, &kdata->output_len);
    if (strcmp(keyword, "Ctrl") == 0)
        return pkey_test_ctrl(t, kdata->ctx, value);
    return 0;
}

static int pkey_test_run(EVP_TEST *t)
{
    PKEY_DATA *expected = t->data;
    unsigned char *got = NULL;
    size_t got_len;

    if (expected->keyop(expected->ctx, NULL, &got_len,
                        expected->input, expected->input_len) <= 0
            || !TEST_ptr(got = OPENSSL_malloc(got_len))) {
        t->err = "KEYOP_LENGTH_ERROR";
        goto err;
    }
    if (expected->keyop(expected->ctx, got, &got_len,
                        expected->input, expected->input_len) <= 0) {
        t->err = "KEYOP_ERROR";
        goto err;
    }
    if (!memory_err_compare(t, "KEYOP_MISMATCH",
                            expected->output, expected->output_len,
                            got, got_len))
        goto err;

    t->err = NULL;
 err:
    OPENSSL_free(got);
    return 1;
}

static int sign_test_init(EVP_TEST *t, const char *name)
{
    return pkey_test_init(t, name, 0, EVP_PKEY_sign_init, EVP_PKEY_sign);
}

static const EVP_TEST_METHOD psign_test_method = {
    "Sign",
    sign_test_init,
    pkey_test_cleanup,
    pkey_test_parse,
    pkey_test_run
};

static int verify_recover_test_init(EVP_TEST *t, const char *name)
{
    return pkey_test_init(t, name, 1, EVP_PKEY_verify_recover_init,
                          EVP_PKEY_verify_recover);
}

static const EVP_TEST_METHOD pverify_recover_test_method = {
    "VerifyRecover",
    verify_recover_test_init,
    pkey_test_cleanup,
    pkey_test_parse,
    pkey_test_run
};

static int decrypt_test_init(EVP_TEST *t, const char *name)
{
    return pkey_test_init(t, name, 0, EVP_PKEY_decrypt_init,
                          EVP_PKEY_decrypt);
}

static const EVP_TEST_METHOD pdecrypt_test_method = {
    "Decrypt",
    decrypt_test_init,
    pkey_test_cleanup,
    pkey_test_parse,
    pkey_test_run
};

static int verify_test_init(EVP_TEST *t, const char *name)
{
    return pkey_test_init(t, name, 1, EVP_PKEY_verify_init, 0);
}

static int verify_test_run(EVP_TEST *t)
{
    PKEY_DATA *kdata = t->data;

    if (EVP_PKEY_verify(kdata->ctx, kdata->output, kdata->output_len,
                        kdata->input, kdata->input_len) <= 0)
        t->err = "VERIFY_ERROR";
    return 1;
}

static const EVP_TEST_METHOD pverify_test_method = {
    "Verify",
    verify_test_init,
    pkey_test_cleanup,
    pkey_test_parse,
    verify_test_run
};


static int pderive_test_init(EVP_TEST *t, const char *name)
{
    return pkey_test_init(t, name, 0, EVP_PKEY_derive_init, 0);
}

static int pderive_test_parse(EVP_TEST *t,
                              const char *keyword, const char *value)
{
    PKEY_DATA *kdata = t->data;

    if (strcmp(keyword, "PeerKey") == 0) {
        EVP_PKEY *peer;
        if (find_key(&peer, value, public_keys) == 0)
            return 0;
        if (EVP_PKEY_derive_set_peer(kdata->ctx, peer) <= 0)
            return 0;
        return 1;
    }
    if (strcmp(keyword, "SharedSecret") == 0)
        return parse_bin(value, &kdata->output, &kdata->output_len);
    if (strcmp(keyword, "Ctrl") == 0)
        return pkey_test_ctrl(t, kdata->ctx, value);
    return 0;
}

static int pderive_test_run(EVP_TEST *t)
{
    PKEY_DATA *expected = t->data;
    unsigned char *got = NULL;
    size_t got_len;

    if (EVP_PKEY_derive(expected->ctx, NULL, &got_len) <= 0) {
        t->err = "DERIVE_ERROR";
        goto err;
    }
    if (!TEST_ptr(got = OPENSSL_malloc(got_len))) {
        t->err = "DERIVE_ERROR";
        goto err;
    }
    if (EVP_PKEY_derive(expected->ctx, got, &got_len) <= 0) {
        t->err = "DERIVE_ERROR";
        goto err;
    }
    if (!memory_err_compare(t, "SHARED_SECRET_MISMATCH",
                            expected->output, expected->output_len,
                            got, got_len))
        goto err;

    t->err = NULL;
 err:
    OPENSSL_free(got);
    return 1;
}

static const EVP_TEST_METHOD pderive_test_method = {
    "Derive",
    pderive_test_init,
    pkey_test_cleanup,
    pderive_test_parse,
    pderive_test_run
};


/**
***  PBE TESTS
**/

typedef enum pbe_type_enum {
    PBE_TYPE_INVALID = 0,
    PBE_TYPE_SCRYPT, PBE_TYPE_PBKDF2, PBE_TYPE_PKCS12
} PBE_TYPE;

typedef struct pbe_data_st {
    PBE_TYPE pbe_type;
        /* scrypt parameters */
    uint64_t N, r, p, maxmem;
        /* PKCS#12 parameters */
    int id, iter;
    const EVP_MD *md;
        /* password */
    unsigned char *pass;
    size_t pass_len;
        /* salt */
    unsigned char *salt;
    size_t salt_len;
        /* Expected output */
    unsigned char *key;
    size_t key_len;
} PBE_DATA;

#ifndef OPENSSL_NO_SCRYPT
/*
 * Parse unsigned decimal 64 bit integer value
 */
static int parse_uint64(const char *value, uint64_t *pr)
{
    const char *p = value;

    if (!TEST_true(*p)) {
        TEST_info("Invalid empty integer value");
        return -1;
    }
    for (*pr = 0; *p; ) {
        if (*pr > UINT64_MAX / 10) {
            TEST_error("Integer overflow in string %s", value);
            return -1;
        }
        *pr *= 10;
        if (!TEST_true(isdigit((unsigned char)*p))) {
            TEST_error("Invalid character in string %s", value);
            return -1;
        }
        *pr += *p - '0';
        p++;
    }
    return 1;
}

static int scrypt_test_parse(EVP_TEST *t,
                             const char *keyword, const char *value)
{
    PBE_DATA *pdata = t->data;

    if (strcmp(keyword, "N") == 0)
        return parse_uint64(value, &pdata->N);
    if (strcmp(keyword, "p") == 0)
        return parse_uint64(value, &pdata->p);
    if (strcmp(keyword, "r") == 0)
        return parse_uint64(value, &pdata->r);
    if (strcmp(keyword, "maxmem") == 0)
        return parse_uint64(value, &pdata->maxmem);
    return 0;
}
#endif

static int pbkdf2_test_parse(EVP_TEST *t,
                             const char *keyword, const char *value)
{
    PBE_DATA *pdata = t->data;

    if (strcmp(keyword, "iter") == 0) {
        pdata->iter = atoi(value);
        if (pdata->iter <= 0)
            return -1;
        return 1;
    }
    if (strcmp(keyword, "MD") == 0) {
        pdata->md = EVP_get_digestbyname(value);
        if (pdata->md == NULL)
            return -1;
        return 1;
    }
    return 0;
}

static int pkcs12_test_parse(EVP_TEST *t,
                             const char *keyword, const char *value)
{
    PBE_DATA *pdata = t->data;

    if (strcmp(keyword, "id") == 0) {
        pdata->id = atoi(value);
        if (pdata->id <= 0)
            return -1;
        return 1;
    }
    return pbkdf2_test_parse(t, keyword, value);
}

static int pbe_test_init(EVP_TEST *t, const char *alg)
{
    PBE_DATA *pdat;
    PBE_TYPE pbe_type = PBE_TYPE_INVALID;

    if (strcmp(alg, "scrypt") == 0) {
#ifndef OPENSSL_NO_SCRYPT
        pbe_type = PBE_TYPE_SCRYPT;
#else
        t->skip = 1;
        return 1;
#endif
    } else if (strcmp(alg, "pbkdf2") == 0) {
        pbe_type = PBE_TYPE_PBKDF2;
    } else if (strcmp(alg, "pkcs12") == 0) {
        pbe_type = PBE_TYPE_PKCS12;
    } else {
        TEST_error("Unknown pbe algorithm %s", alg);
    }
    pdat = OPENSSL_zalloc(sizeof(*pdat));
    pdat->pbe_type = pbe_type;
    t->data = pdat;
    return 1;
}

static void pbe_test_cleanup(EVP_TEST *t)
{
    PBE_DATA *pdat = t->data;

    OPENSSL_free(pdat->pass);
    OPENSSL_free(pdat->salt);
    OPENSSL_free(pdat->key);
}

static int pbe_test_parse(EVP_TEST *t,
                          const char *keyword, const char *value)
{
    PBE_DATA *pdata = t->data;

    if (strcmp(keyword, "Password") == 0)
        return parse_bin(value, &pdata->pass, &pdata->pass_len);
    if (strcmp(keyword, "Salt") == 0)
        return parse_bin(value, &pdata->salt, &pdata->salt_len);
    if (strcmp(keyword, "Key") == 0)
        return parse_bin(value, &pdata->key, &pdata->key_len);
    if (pdata->pbe_type == PBE_TYPE_PBKDF2)
        return pbkdf2_test_parse(t, keyword, value);
    else if (pdata->pbe_type == PBE_TYPE_PKCS12)
        return pkcs12_test_parse(t, keyword, value);
#ifndef OPENSSL_NO_SCRYPT
    else if (pdata->pbe_type == PBE_TYPE_SCRYPT)
        return scrypt_test_parse(t, keyword, value);
#endif
    return 0;
}

static int pbe_test_run(EVP_TEST *t)
{
    PBE_DATA *expected = t->data;
    unsigned char *key;

    if (!TEST_ptr(key = OPENSSL_malloc(expected->key_len))) {
        t->err = "INTERNAL_ERROR";
        goto err;
    }
    if (expected->pbe_type == PBE_TYPE_PBKDF2) {
        if (PKCS5_PBKDF2_HMAC((char *)expected->pass, expected->pass_len,
                              expected->salt, expected->salt_len,
                              expected->iter, expected->md,
                              expected->key_len, key) == 0) {
            t->err = "PBKDF2_ERROR";
            goto err;
        }
#ifndef OPENSSL_NO_SCRYPT
    } else if (expected->pbe_type == PBE_TYPE_SCRYPT) {
        if (EVP_PBE_scrypt((const char *)expected->pass, expected->pass_len,
                           expected->salt, expected->salt_len, expected->N,
                           expected->r, expected->p, expected->maxmem,
                           key, expected->key_len) == 0) {
            t->err = "SCRYPT_ERROR";
            goto err;
        }
#endif
    } else if (expected->pbe_type == PBE_TYPE_PKCS12) {
        if (PKCS12_key_gen_uni(expected->pass, expected->pass_len,
                               expected->salt, expected->salt_len,
                               expected->id, expected->iter, expected->key_len,
                               key, expected->md) == 0) {
            t->err = "PKCS12_ERROR";
            goto err;
        }
    }
    if (!memory_err_compare(t, "KEY_MISMATCH", expected->key, expected->key_len,
                            key, expected->key_len))
        goto err;

    t->err = NULL;
err:
    OPENSSL_free(key);
    return 1;
}

static const EVP_TEST_METHOD pbe_test_method = {
    "PBE",
    pbe_test_init,
    pbe_test_cleanup,
    pbe_test_parse,
    pbe_test_run
};


/**
***  BASE64 TESTS
**/

typedef enum {
    BASE64_CANONICAL_ENCODING = 0,
    BASE64_VALID_ENCODING = 1,
    BASE64_INVALID_ENCODING = 2
} base64_encoding_type;

typedef struct encode_data_st {
    /* Input to encoding */
    unsigned char *input;
    size_t input_len;
    /* Expected output */
    unsigned char *output;
    size_t output_len;
    base64_encoding_type encoding;
} ENCODE_DATA;

static int encode_test_init(EVP_TEST *t, const char *encoding)
{
    ENCODE_DATA *edata;

    if (!TEST_ptr(edata = OPENSSL_zalloc(sizeof(*edata))))
        return 0;
    if (strcmp(encoding, "canonical") == 0) {
        edata->encoding = BASE64_CANONICAL_ENCODING;
    } else if (strcmp(encoding, "valid") == 0) {
        edata->encoding = BASE64_VALID_ENCODING;
    } else if (strcmp(encoding, "invalid") == 0) {
        edata->encoding = BASE64_INVALID_ENCODING;
        if (!TEST_ptr(t->expected_err = OPENSSL_strdup("DECODE_ERROR")))
            goto err;
    } else {
        TEST_error("Bad encoding: %s."
                   " Should be one of {canonical, valid, invalid}",
                   encoding);
        goto err;
    }
    t->data = edata;
    return 1;
err:
    OPENSSL_free(edata);
    return 0;
}

static void encode_test_cleanup(EVP_TEST *t)
{
    ENCODE_DATA *edata = t->data;

    OPENSSL_free(edata->input);
    OPENSSL_free(edata->output);
    memset(edata, 0, sizeof(*edata));
}

static int encode_test_parse(EVP_TEST *t,
                             const char *keyword, const char *value)
{
    ENCODE_DATA *edata = t->data;

    if (strcmp(keyword, "Input") == 0)
        return parse_bin(value, &edata->input, &edata->input_len);
    if (strcmp(keyword, "Output") == 0)
        return parse_bin(value, &edata->output, &edata->output_len);
    return 0;
}

static int encode_test_run(EVP_TEST *t)
{
    ENCODE_DATA *expected = t->data;
    unsigned char *encode_out = NULL, *decode_out = NULL;
    int output_len, chunk_len;
    EVP_ENCODE_CTX *decode_ctx = NULL, *encode_ctx = NULL;

    if (!TEST_ptr(decode_ctx = EVP_ENCODE_CTX_new())) {
        t->err = "INTERNAL_ERROR";
        goto err;
    }

    if (expected->encoding == BASE64_CANONICAL_ENCODING) {

        if (!TEST_ptr(encode_ctx = EVP_ENCODE_CTX_new())
                || !TEST_ptr(encode_out =
                        OPENSSL_malloc(EVP_ENCODE_LENGTH(expected->input_len))))
            goto err;

        EVP_EncodeInit(encode_ctx);
        if (!TEST_true(EVP_EncodeUpdate(encode_ctx, encode_out, &chunk_len,
                                        expected->input, expected->input_len)))
            goto err;

        output_len = chunk_len;

        EVP_EncodeFinal(encode_ctx, encode_out + chunk_len, &chunk_len);
        output_len += chunk_len;

        if (!memory_err_compare(t, "BAD_ENCODING",
                                expected->output, expected->output_len,
                                encode_out, output_len))
            goto err;
    }

    if (!TEST_ptr(decode_out =
                OPENSSL_malloc(EVP_DECODE_LENGTH(expected->output_len))))
        goto err;

    EVP_DecodeInit(decode_ctx);
    if (EVP_DecodeUpdate(decode_ctx, decode_out, &chunk_len, expected->output,
                         expected->output_len) < 0) {
        t->err = "DECODE_ERROR";
        goto err;
    }
    output_len = chunk_len;

    if (EVP_DecodeFinal(decode_ctx, decode_out + chunk_len, &chunk_len) != 1) {
        t->err = "DECODE_ERROR";
        goto err;
    }
    output_len += chunk_len;

    if (expected->encoding != BASE64_INVALID_ENCODING
            && !memory_err_compare(t, "BAD_DECODING",
                                   expected->input, expected->input_len,
                                   decode_out, output_len)) {
        t->err = "BAD_DECODING";
        goto err;
    }

    t->err = NULL;
 err:
    OPENSSL_free(encode_out);
    OPENSSL_free(decode_out);
    EVP_ENCODE_CTX_free(decode_ctx);
    EVP_ENCODE_CTX_free(encode_ctx);
    return 1;
}

static const EVP_TEST_METHOD encode_test_method = {
    "Encoding",
    encode_test_init,
    encode_test_cleanup,
    encode_test_parse,
    encode_test_run,
};

/**
***  KDF TESTS
**/

typedef struct kdf_data_st {
    /* Context for this operation */
    EVP_PKEY_CTX *ctx;
    /* Expected output */
    unsigned char *output;
    size_t output_len;
} KDF_DATA;

/*
 * Perform public key operation setup: lookup key, allocated ctx and call
 * the appropriate initialisation function
 */
static int kdf_test_init(EVP_TEST *t, const char *name)
{
    KDF_DATA *kdata;
    int kdf_nid = OBJ_sn2nid(name);

#ifdef OPENSSL_NO_SCRYPT
    if (strcmp(name, "scrypt") == 0) {
        t->skip = 1;
        return 1;
    }
#endif

    if (kdf_nid == NID_undef)
        kdf_nid = OBJ_ln2nid(name);

    if (!TEST_ptr(kdata = OPENSSL_zalloc(sizeof(*kdata))))
        return 0;
    kdata->ctx = EVP_PKEY_CTX_new_id(kdf_nid, NULL);
    if (kdata->ctx == NULL) {
        OPENSSL_free(kdata);
        return 0;
    }
    if (EVP_PKEY_derive_init(kdata->ctx) <= 0) {
        EVP_PKEY_CTX_free(kdata->ctx);
        OPENSSL_free(kdata);
        return 0;
    }
    t->data = kdata;
    return 1;
}

static void kdf_test_cleanup(EVP_TEST *t)
{
    KDF_DATA *kdata = t->data;
    OPENSSL_free(kdata->output);
    EVP_PKEY_CTX_free(kdata->ctx);
}

static int kdf_test_parse(EVP_TEST *t,
                          const char *keyword, const char *value)
{
    KDF_DATA *kdata = t->data;

    if (strcmp(keyword, "Output") == 0)
        return parse_bin(value, &kdata->output, &kdata->output_len);
    if (strncmp(keyword, "Ctrl", 4) == 0)
        return pkey_test_ctrl(t, kdata->ctx, value);
    return 0;
}

static int kdf_test_run(EVP_TEST *t)
{
    KDF_DATA *expected = t->data;
    unsigned char *got = NULL;
    size_t got_len = expected->output_len;

    if (!TEST_ptr(got = OPENSSL_malloc(got_len))) {
        t->err = "INTERNAL_ERROR";
        goto err;
    }
    if (EVP_PKEY_derive(expected->ctx, got, &got_len) <= 0) {
        t->err = "KDF_DERIVE_ERROR";
        goto err;
    }
    if (!memory_err_compare(t, "KDF_MISMATCH",
                            expected->output, expected->output_len,
                            got, got_len))
        goto err;

    t->err = NULL;

 err:
    OPENSSL_free(got);
    return 1;
}

static const EVP_TEST_METHOD kdf_test_method = {
    "KDF",
    kdf_test_init,
    kdf_test_cleanup,
    kdf_test_parse,
    kdf_test_run
};


/**
***  KEYPAIR TESTS
**/

typedef struct keypair_test_data_st {
    EVP_PKEY *privk;
    EVP_PKEY *pubk;
} KEYPAIR_TEST_DATA;

static int keypair_test_init(EVP_TEST *t, const char *pair)
{
    KEYPAIR_TEST_DATA *data;
    int rv = 0;
    EVP_PKEY *pk = NULL, *pubk = NULL;
    char *pub, *priv = NULL;

    /* Split private and public names. */
    if (!TEST_ptr(priv = OPENSSL_strdup(pair))
            || !TEST_ptr(pub = strchr(priv, ':'))) {
        t->err = "PARSING_ERROR";
        goto end;
    }
    *pub++ = '\0';

    if (!TEST_true(find_key(&pk, priv, private_keys))) {
        TEST_info("Can't find private key: %s", priv);
        t->err = "MISSING_PRIVATE_KEY";
        goto end;
    }
    if (!TEST_true(find_key(&pubk, pub, public_keys))) {
        TEST_info("Can't find public key: %s", pub);
        t->err = "MISSING_PUBLIC_KEY";
        goto end;
    }

    if (pk == NULL && pubk == NULL) {
        /* Both keys are listed but unsupported: skip this test */
        t->skip = 1;
        rv = 1;
        goto end;
    }

    if (!TEST_ptr(data = OPENSSL_malloc(sizeof(*data))))
        goto end;
    data->privk = pk;
    data->pubk = pubk;
    t->data = data;
    rv = 1;
    t->err = NULL;

end:
    OPENSSL_free(priv);
    return rv;
}

static void keypair_test_cleanup(EVP_TEST *t)
{
    OPENSSL_free(t->data);
    t->data = NULL;
}

/*
 * For tests that do not accept any custom keywords.
 */
static int void_test_parse(EVP_TEST *t, const char *keyword, const char *value)
{
    return 0;
}

static int keypair_test_run(EVP_TEST *t)
{
    int rv = 0;
    const KEYPAIR_TEST_DATA *pair = t->data;

    if (pair->privk == NULL || pair->pubk == NULL) {
        /*
         * this can only happen if only one of the keys is not set
         * which means that one of them was unsupported while the
         * other isn't: hence a key type mismatch.
         */
        t->err = "KEYPAIR_TYPE_MISMATCH";
        rv = 1;
        goto end;
    }

    if ((rv = EVP_PKEY_cmp(pair->privk, pair->pubk)) != 1 ) {
        if ( 0 == rv ) {
            t->err = "KEYPAIR_MISMATCH";
        } else if ( -1 == rv ) {
            t->err = "KEYPAIR_TYPE_MISMATCH";
        } else if ( -2 == rv ) {
            t->err = "UNSUPPORTED_KEY_COMPARISON";
        } else {
            TEST_error("Unexpected error in key comparison");
            rv = 0;
            goto end;
        }
        rv = 1;
        goto end;
    }

    rv = 1;
    t->err = NULL;

end:
    return rv;
}

static const EVP_TEST_METHOD keypair_test_method = {
    "PrivPubKeyPair",
    keypair_test_init,
    keypair_test_cleanup,
    void_test_parse,
    keypair_test_run
};

/**
***  KEYGEN TEST
**/

typedef struct keygen_test_data_st {
    EVP_PKEY_CTX *genctx; /* Keygen context to use */
    char *keyname; /* Key name to store key or NULL */
} KEYGEN_TEST_DATA;

static int keygen_test_init(EVP_TEST *t, const char *alg)
{
    KEYGEN_TEST_DATA *data;
    EVP_PKEY_CTX *genctx;
    int nid = OBJ_sn2nid(alg);

    if (nid == NID_undef) {
        nid = OBJ_ln2nid(alg);
        if (nid == NID_undef)
            return 0;
    }

    if (!TEST_ptr(genctx = EVP_PKEY_CTX_new_id(nid, NULL))) {
        /* assume algorithm disabled */
        t->skip = 1;
        return 1;
    }

    if (EVP_PKEY_keygen_init(genctx) <= 0) {
        t->err = "KEYGEN_INIT_ERROR";
        goto err;
    }

    if (!TEST_ptr(data = OPENSSL_malloc(sizeof(*data))))
        goto err;
    data->genctx = genctx;
    data->keyname = NULL;
    t->data = data;
    t->err = NULL;
    return 1;

err:
    EVP_PKEY_CTX_free(genctx);
    return 0;
}

static void keygen_test_cleanup(EVP_TEST *t)
{
    KEYGEN_TEST_DATA *keygen = t->data;

    EVP_PKEY_CTX_free(keygen->genctx);
    OPENSSL_free(keygen->keyname);
    OPENSSL_free(t->data);
    t->data = NULL;
}

static int keygen_test_parse(EVP_TEST *t,
                             const char *keyword, const char *value)
{
    KEYGEN_TEST_DATA *keygen = t->data;

    if (strcmp(keyword, "KeyName") == 0)
        return TEST_ptr(keygen->keyname = OPENSSL_strdup(value));
    if (strcmp(keyword, "Ctrl") == 0)
        return pkey_test_ctrl(t, keygen->genctx, value);
    return 0;
}

static int keygen_test_run(EVP_TEST *t)
{
    KEYGEN_TEST_DATA *keygen = t->data;
    EVP_PKEY *pkey = NULL;

    t->err = NULL;
    if (EVP_PKEY_keygen(keygen->genctx, &pkey) <= 0) {
        t->err = "KEYGEN_GENERATE_ERROR";
        goto err;
    }

    if (keygen->keyname != NULL) {
        KEY_LIST *key;

        if (find_key(NULL, keygen->keyname, private_keys)) {
            TEST_info("Duplicate key %s", keygen->keyname);
            goto err;
        }

        if (!TEST_ptr(key = OPENSSL_malloc(sizeof(*key))))
            goto err;
        key->name = keygen->keyname;
        keygen->keyname = NULL;
        key->key = pkey;
        key->next = private_keys;
        private_keys = key;
    } else {
        EVP_PKEY_free(pkey);
    }

    return 1;

err:
    EVP_PKEY_free(pkey);
    return 0;
}

static const EVP_TEST_METHOD keygen_test_method = {
    "KeyGen",
    keygen_test_init,
    keygen_test_cleanup,
    keygen_test_parse,
    keygen_test_run,
};

/**
***  DIGEST SIGN+VERIFY TESTS
**/

typedef struct {
    int is_verify; /* Set to 1 if verifying */
    int is_oneshot; /* Set to 1 for one shot operation */
    const EVP_MD *md; /* Digest to use */
    EVP_MD_CTX *ctx; /* Digest context */
    EVP_PKEY_CTX *pctx;
    STACK_OF(EVP_TEST_BUFFER) *input; /* Input data: streaming */
    unsigned char *osin; /* Input data if one shot */
    size_t osin_len; /* Input length data if one shot */
    unsigned char *output; /* Expected output */
    size_t output_len; /* Expected output length */
} DIGESTSIGN_DATA;

static int digestsigver_test_init(EVP_TEST *t, const char *alg, int is_verify,
                                  int is_oneshot)
{
    const EVP_MD *md = NULL;
    DIGESTSIGN_DATA *mdat;

    if (strcmp(alg, "NULL") != 0) {
        if ((md = EVP_get_digestbyname(alg)) == NULL) {
            /* If alg has an OID assume disabled algorithm */
            if (OBJ_sn2nid(alg) != NID_undef || OBJ_ln2nid(alg) != NID_undef) {
                t->skip = 1;
                return 1;
            }
            return 0;
        }
    }
    if (!TEST_ptr(mdat = OPENSSL_zalloc(sizeof(*mdat))))
        return 0;
    mdat->md = md;
    if (!TEST_ptr(mdat->ctx = EVP_MD_CTX_new())) {
        OPENSSL_free(mdat);
        return 0;
    }
    mdat->is_verify = is_verify;
    mdat->is_oneshot = is_oneshot;
    t->data = mdat;
    return 1;
}

static int digestsign_test_init(EVP_TEST *t, const char *alg)
{
    return digestsigver_test_init(t, alg, 0, 0);
}

static void digestsigver_test_cleanup(EVP_TEST *t)
{
    DIGESTSIGN_DATA *mdata = t->data;

    EVP_MD_CTX_free(mdata->ctx);
    sk_EVP_TEST_BUFFER_pop_free(mdata->input, evp_test_buffer_free);
    OPENSSL_free(mdata->osin);
    OPENSSL_free(mdata->output);
    OPENSSL_free(mdata);
    t->data = NULL;
}

static int digestsigver_test_parse(EVP_TEST *t,
                                   const char *keyword, const char *value)
{
    DIGESTSIGN_DATA *mdata = t->data;

    if (strcmp(keyword, "Key") == 0) {
        EVP_PKEY *pkey = NULL;
        int rv = 0;

        if (mdata->is_verify)
            rv = find_key(&pkey, value, public_keys);
        if (rv == 0)
            rv = find_key(&pkey, value, private_keys);
        if (rv == 0 || pkey == NULL) {
            t->skip = 1;
            return 1;
        }
        if (mdata->is_verify) {
            if (!EVP_DigestVerifyInit(mdata->ctx, &mdata->pctx, mdata->md,
                                      NULL, pkey))
                t->err = "DIGESTVERIFYINIT_ERROR";
            return 1;
        }
        if (!EVP_DigestSignInit(mdata->ctx, &mdata->pctx, mdata->md, NULL,
                                pkey))
            t->err = "DIGESTSIGNINIT_ERROR";
        return 1;
    }

    if (strcmp(keyword, "Input") == 0) {
        if (mdata->is_oneshot)
            return parse_bin(value, &mdata->osin, &mdata->osin_len);
        return evp_test_buffer_append(value, &mdata->input);
    }
    if (strcmp(keyword, "Output") == 0)
        return parse_bin(value, &mdata->output, &mdata->output_len);

    if (!mdata->is_oneshot) {
        if (strcmp(keyword, "Count") == 0)
            return evp_test_buffer_set_count(value, mdata->input);
        if (strcmp(keyword, "Ncopy") == 0)
            return evp_test_buffer_ncopy(value, mdata->input);
    }
    if (strcmp(keyword, "Ctrl") == 0) {
        if (mdata->pctx == NULL)
            return 0;
        return pkey_test_ctrl(t, mdata->pctx, value);
    }
    return 0;
}

static int digestsign_update_fn(void *ctx, const unsigned char *buf,
                                size_t buflen)
{
    return EVP_DigestSignUpdate(ctx, buf, buflen);
}

static int digestsign_test_run(EVP_TEST *t)
{
    DIGESTSIGN_DATA *expected = t->data;
    unsigned char *got = NULL;
    size_t got_len;

    if (!evp_test_buffer_do(expected->input, digestsign_update_fn,
                            expected->ctx)) {
        t->err = "DIGESTUPDATE_ERROR";
        goto err;
    }

    if (!EVP_DigestSignFinal(expected->ctx, NULL, &got_len)) {
        t->err = "DIGESTSIGNFINAL_LENGTH_ERROR";
        goto err;
    }
    if (!TEST_ptr(got = OPENSSL_malloc(got_len))) {
        t->err = "MALLOC_FAILURE";
        goto err;
    }
    if (!EVP_DigestSignFinal(expected->ctx, got, &got_len)) {
        t->err = "DIGESTSIGNFINAL_ERROR";
        goto err;
    }
    if (!memory_err_compare(t, "SIGNATURE_MISMATCH",
                            expected->output, expected->output_len,
                            got, got_len))
        goto err;

    t->err = NULL;
 err:
    OPENSSL_free(got);
    return 1;
}

static const EVP_TEST_METHOD digestsign_test_method = {
    "DigestSign",
    digestsign_test_init,
    digestsigver_test_cleanup,
    digestsigver_test_parse,
    digestsign_test_run
};

static int digestverify_test_init(EVP_TEST *t, const char *alg)
{
    return digestsigver_test_init(t, alg, 1, 0);
}

static int digestverify_update_fn(void *ctx, const unsigned char *buf,
                                  size_t buflen)
{
    return EVP_DigestVerifyUpdate(ctx, buf, buflen);
}

static int digestverify_test_run(EVP_TEST *t)
{
    DIGESTSIGN_DATA *mdata = t->data;

    if (!evp_test_buffer_do(mdata->input, digestverify_update_fn, mdata->ctx)) {
        t->err = "DIGESTUPDATE_ERROR";
        return 1;
    }

    if (EVP_DigestVerifyFinal(mdata->ctx, mdata->output,
                              mdata->output_len) <= 0)
        t->err = "VERIFY_ERROR";
    return 1;
}

static const EVP_TEST_METHOD digestverify_test_method = {
    "DigestVerify",
    digestverify_test_init,
    digestsigver_test_cleanup,
    digestsigver_test_parse,
    digestverify_test_run
};

static int oneshot_digestsign_test_init(EVP_TEST *t, const char *alg)
{
    return digestsigver_test_init(t, alg, 0, 1);
}

static int oneshot_digestsign_test_run(EVP_TEST *t)
{
    DIGESTSIGN_DATA *expected = t->data;
    unsigned char *got = NULL;
    size_t got_len;

    if (!EVP_DigestSign(expected->ctx, NULL, &got_len,
                        expected->osin, expected->osin_len)) {
        t->err = "DIGESTSIGN_LENGTH_ERROR";
        goto err;
    }
    if (!TEST_ptr(got = OPENSSL_malloc(got_len))) {
        t->err = "MALLOC_FAILURE";
        goto err;
    }
    if (!EVP_DigestSign(expected->ctx, got, &got_len,
                        expected->osin, expected->osin_len)) {
        t->err = "DIGESTSIGN_ERROR";
        goto err;
    }
    if (!memory_err_compare(t, "SIGNATURE_MISMATCH",
                            expected->output, expected->output_len,
                            got, got_len))
        goto err;

    t->err = NULL;
 err:
    OPENSSL_free(got);
    return 1;
}

static const EVP_TEST_METHOD oneshot_digestsign_test_method = {
    "OneShotDigestSign",
    oneshot_digestsign_test_init,
    digestsigver_test_cleanup,
    digestsigver_test_parse,
    oneshot_digestsign_test_run
};

static int oneshot_digestverify_test_init(EVP_TEST *t, const char *alg)
{
    return digestsigver_test_init(t, alg, 1, 1);
}

static int oneshot_digestverify_test_run(EVP_TEST *t)
{
    DIGESTSIGN_DATA *mdata = t->data;

    if (EVP_DigestVerify(mdata->ctx, mdata->output, mdata->output_len,
                         mdata->osin, mdata->osin_len) <= 0)
        t->err = "VERIFY_ERROR";
    return 1;
}

static const EVP_TEST_METHOD oneshot_digestverify_test_method = {
    "OneShotDigestVerify",
    oneshot_digestverify_test_init,
    digestsigver_test_cleanup,
    digestsigver_test_parse,
    oneshot_digestverify_test_run
};


/**
***  PARSING AND DISPATCH
**/

static const EVP_TEST_METHOD *evp_test_list[] = {
    &cipher_test_method,
    &digest_test_method,
    &digestsign_test_method,
    &digestverify_test_method,
    &encode_test_method,
    &kdf_test_method,
    &keypair_test_method,
    &keygen_test_method,
    &mac_test_method,
    &oneshot_digestsign_test_method,
    &oneshot_digestverify_test_method,
    &pbe_test_method,
    &pdecrypt_test_method,
    &pderive_test_method,
    &psign_test_method,
    &pverify_recover_test_method,
    &pverify_test_method,
    NULL
};

static const EVP_TEST_METHOD *find_test(const char *name)
{
    const EVP_TEST_METHOD **tt;

    for (tt = evp_test_list; *tt; tt++) {
        if (strcmp(name, (*tt)->name) == 0)
            return *tt;
    }
    return NULL;
}

static void clear_test(EVP_TEST *t)
{
    test_clearstanza(&t->s);
    ERR_clear_error();
    if (t->data != NULL) {
        if (t->meth != NULL)
            t->meth->cleanup(t);
        OPENSSL_free(t->data);
        t->data = NULL;
    }
    OPENSSL_free(t->expected_err);
    t->expected_err = NULL;
    OPENSSL_free(t->func);
    t->func = NULL;
    OPENSSL_free(t->reason);
    t->reason = NULL;

    /* Text literal. */
    t->err = NULL;
    t->skip = 0;
    t->meth = NULL;
}

/*
 * Check for errors in the test structure; return 1 if okay, else 0.
 */
static int check_test_error(EVP_TEST *t)
{
    unsigned long err;
    const char *func;
    const char *reason;

    if (t->err == NULL && t->expected_err == NULL)
        return 1;
    if (t->err != NULL && t->expected_err == NULL) {
        if (t->aux_err != NULL) {
            TEST_info("%s:%d: Source of above error (%s); unexpected error %s",
                      t->s.test_file, t->s.start, t->aux_err, t->err);
        } else {
            TEST_info("%s:%d: Source of above error; unexpected error %s",
                      t->s.test_file, t->s.start, t->err);
        }
        return 0;
    }
    if (t->err == NULL && t->expected_err != NULL) {
        TEST_info("%s:%d: Succeeded but was expecting %s",
                  t->s.test_file, t->s.start, t->expected_err);
        return 0;
    }

    if (strcmp(t->err, t->expected_err) != 0) {
        TEST_info("%s:%d: Expected %s got %s",
                  t->s.test_file, t->s.start, t->expected_err, t->err);
        return 0;
    }

    if (t->func == NULL && t->reason == NULL)
        return 1;

    if (t->func == NULL || t->reason == NULL) {
        TEST_info("%s:%d: Test is missing function or reason code",
                  t->s.test_file, t->s.start);
        return 0;
    }

    err = ERR_peek_error();
    if (err == 0) {
        TEST_info("%s:%d: Expected error \"%s:%s\" not set",
                  t->s.test_file, t->s.start, t->func, t->reason);
        return 0;
    }

    func = ERR_func_error_string(err);
    reason = ERR_reason_error_string(err);
    if (func == NULL && reason == NULL) {
        TEST_info("%s:%d: Expected error \"%s:%s\", no strings available."
                  " Assuming ok.",
                  t->s.test_file, t->s.start, t->func, t->reason);
        return 1;
    }

    if (strcmp(func, t->func) == 0 && strcmp(reason, t->reason) == 0)
        return 1;

    TEST_info("%s:%d: Expected error \"%s:%s\", got \"%s:%s\"",
              t->s.test_file, t->s.start, t->func, t->reason, func, reason);

    return 0;
}

/*
 * Run a parsed test. Log a message and return 0 on error.
 */
static int run_test(EVP_TEST *t)
{
    if (t->meth == NULL)
        return 1;
    t->s.numtests++;
    if (t->skip) {
        t->s.numskip++;
    } else {
        /* run the test */
        if (t->err == NULL && t->meth->run_test(t) != 1) {
            TEST_info("%s:%d %s error",
                      t->s.test_file, t->s.start, t->meth->name);
            return 0;
        }
        if (!check_test_error(t)) {
            TEST_openssl_errors();
            t->s.errors++;
        }
    }

    /* clean it up */
    return 1;
}

static int find_key(EVP_PKEY **ppk, const char *name, KEY_LIST *lst)
{
    for (; lst != NULL; lst = lst->next) {
        if (strcmp(lst->name, name) == 0) {
            if (ppk != NULL)
                *ppk = lst->key;
            return 1;
        }
    }
    return 0;
}

static void free_key_list(KEY_LIST *lst)
{
    while (lst != NULL) {
        KEY_LIST *next = lst->next;

        EVP_PKEY_free(lst->key);
        OPENSSL_free(lst->name);
        OPENSSL_free(lst);
        lst = next;
    }
}

/*
 * Is the key type an unsupported algorithm?
 */
static int key_unsupported(void)
{
    long err = ERR_peek_error();

    if (ERR_GET_LIB(err) == ERR_LIB_EVP
            && ERR_GET_REASON(err) == EVP_R_UNSUPPORTED_ALGORITHM) {
        ERR_clear_error();
        return 1;
    }
#ifndef OPENSSL_NO_EC
    /*
     * If EC support is enabled we should catch also EC_R_UNKNOWN_GROUP as an
     * hint to an unsupported algorithm/curve (e.g. if binary EC support is
     * disabled).
     */
    if (ERR_GET_LIB(err) == ERR_LIB_EC
        && ERR_GET_REASON(err) == EC_R_UNKNOWN_GROUP) {
        ERR_clear_error();
        return 1;
    }
#endif /* OPENSSL_NO_EC */
    return 0;
}

/*
 * NULL out the value from |pp| but return it.  This "steals" a pointer.
 */
static char *take_value(PAIR *pp)
{
    char *p = pp->value;

    pp->value = NULL;
    return p;
}

/*
 * Read and parse one test.  Return 0 if failure, 1 if okay.
 */
static int parse(EVP_TEST *t)
{
    KEY_LIST *key, **klist;
    EVP_PKEY *pkey;
    PAIR *pp;
    int i;

top:
    do {
        if (BIO_eof(t->s.fp))
            return EOF;
        clear_test(t);
        if (!test_readstanza(&t->s))
            return 0;
    } while (t->s.numpairs == 0);
    pp = &t->s.pairs[0];

    /* Are we adding a key? */
    klist = NULL;
    pkey = NULL;
    if (strcmp(pp->key, "PrivateKey") == 0) {
        pkey = PEM_read_bio_PrivateKey(t->s.key, NULL, 0, NULL);
        if (pkey == NULL && !key_unsupported()) {
            EVP_PKEY_free(pkey);
            TEST_info("Can't read private key %s", pp->value);
            TEST_openssl_errors();
            return 0;
        }
        klist = &private_keys;
    } else if (strcmp(pp->key, "PublicKey") == 0) {
        pkey = PEM_read_bio_PUBKEY(t->s.key, NULL, 0, NULL);
        if (pkey == NULL && !key_unsupported()) {
            EVP_PKEY_free(pkey);
            TEST_info("Can't read public key %s", pp->value);
            TEST_openssl_errors();
            return 0;
        }
        klist = &public_keys;
    } else if (strcmp(pp->key, "PrivateKeyRaw") == 0
               || strcmp(pp->key, "PublicKeyRaw") == 0 ) {
        char *strnid = NULL, *keydata = NULL;
        unsigned char *keybin;
        size_t keylen;
        int nid;

        if (strcmp(pp->key, "PrivateKeyRaw") == 0)
            klist = &private_keys;
        else
            klist = &public_keys;

        strnid = strchr(pp->value, ':');
        if (strnid != NULL) {
            *strnid++ = '\0';
            keydata = strchr(strnid, ':');
            if (keydata != NULL)
                *keydata++ = '\0';
        }
        if (keydata == NULL) {
            TEST_info("Failed to parse %s value", pp->key);
            return 0;
        }

        nid = OBJ_txt2nid(strnid);
        if (nid == NID_undef) {
            TEST_info("Uncrecognised algorithm NID");
            return 0;
        }
        if (!parse_bin(keydata, &keybin, &keylen)) {
            TEST_info("Failed to create binary key");
            return 0;
        }
        if (klist == &private_keys)
            pkey = EVP_PKEY_new_raw_private_key(nid, NULL, keybin, keylen);
        else
            pkey = EVP_PKEY_new_raw_public_key(nid, NULL, keybin, keylen);
        if (pkey == NULL && !key_unsupported()) {
            TEST_info("Can't read %s data", pp->key);
            OPENSSL_free(keybin);
            TEST_openssl_errors();
            return 0;
        }
        OPENSSL_free(keybin);
    }

    /* If we have a key add to list */
    if (klist != NULL) {
        if (find_key(NULL, pp->value, *klist)) {
            TEST_info("Duplicate key %s", pp->value);
            return 0;
        }
        if (!TEST_ptr(key = OPENSSL_malloc(sizeof(*key))))
            return 0;
        key->name = take_value(pp);

        /* Hack to detect SM2 keys */
        if(pkey != NULL && strstr(key->name, "SM2") != NULL) {
#ifdef OPENSSL_NO_SM2
            EVP_PKEY_free(pkey);
            pkey = NULL;
#else
            EVP_PKEY_set_alias_type(pkey, EVP_PKEY_SM2);
#endif
        }

        key->key = pkey;
        key->next = *klist;
        *klist = key;

        /* Go back and start a new stanza. */
        if (t->s.numpairs != 1)
            TEST_info("Line %d: missing blank line\n", t->s.curr);
        goto top;
    }

    /* Find the test, based on first keyword. */
    if (!TEST_ptr(t->meth = find_test(pp->key)))
        return 0;
    if (!t->meth->init(t, pp->value)) {
        TEST_error("unknown %s: %s\n", pp->key, pp->value);
        return 0;
    }
    if (t->skip == 1) {
        /* TEST_info("skipping %s %s", pp->key, pp->value); */
        return 0;
    }

    for (pp++, i = 1; i < t->s.numpairs; pp++, i++) {
        if (strcmp(pp->key, "Result") == 0) {
            if (t->expected_err != NULL) {
                TEST_info("Line %d: multiple result lines", t->s.curr);
                return 0;
            }
            t->expected_err = take_value(pp);
        } else if (strcmp(pp->key, "Function") == 0) {
            if (t->func != NULL) {
                TEST_info("Line %d: multiple function lines\n", t->s.curr);
                return 0;
            }
            t->func = take_value(pp);
        } else if (strcmp(pp->key, "Reason") == 0) {
            if (t->reason != NULL) {
                TEST_info("Line %d: multiple reason lines", t->s.curr);
                return 0;
            }
            t->reason = take_value(pp);
        } else {
            /* Must be test specific line: try to parse it */
            int rv = t->meth->parse(t, pp->key, pp->value);

            if (rv == 0) {
                TEST_info("Line %d: unknown keyword %s", t->s.curr, pp->key);
                return 0;
            }
            if (rv < 0) {
                TEST_info("Line %d: error processing keyword %s = %s\n",
                          t->s.curr, pp->key, pp->value);
                return 0;
            }
        }
    }

    return 1;
}

static int run_file_tests(int i)
{
    EVP_TEST *t;
    const char *testfile = test_get_argument(i);
    int c;

    if (!TEST_ptr(t = OPENSSL_zalloc(sizeof(*t))))
        return 0;
    if (!test_start_file(&t->s, testfile)) {
        OPENSSL_free(t);
        return 0;
    }

    while (!BIO_eof(t->s.fp)) {
        c = parse(t);
        if (t->skip)
            continue;
        if (c == 0 || !run_test(t)) {
            t->s.errors++;
            break;
        }
    }
    test_end_file(&t->s);
    clear_test(t);

    free_key_list(public_keys);
    free_key_list(private_keys);
    BIO_free(t->s.key);
    c = t->s.errors;
    OPENSSL_free(t);
    return c == 0;
}

int setup_tests(void)
{
    size_t n = test_get_argument_count();

    if (n == 0) {
        TEST_error("Usage: %s file...", test_get_program_name());
        return 0;
    }

    ADD_ALL_TESTS(run_file_tests, n);
    return 1;
}
