/*
 * Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
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

/* Remove spaces from beginning and end of a string */

static void remove_space(char **pval)
{
    unsigned char *p = (unsigned char *)*pval;

    while (isspace(*p))
        p++;

    *pval = (char *)p;

    p = p + strlen(*pval) - 1;

    /* Remove trailing space */
    while (isspace(*p))
        *p-- = 0;
}

/*
 * Given a line of the form:
 *      name = value # comment
 * extract name and value. NB: modifies passed buffer.
 */

static int parse_line(char **pkw, char **pval, char *linebuf)
{
    char *p;

    p = linebuf + strlen(linebuf) - 1;

    if (*p != '\n') {
        fprintf(stderr, "FATAL: missing EOL\n");
        exit(1);
    }

    /* Look for # */

    p = strchr(linebuf, '#');

    if (p)
        *p = '\0';

    /* Look for = sign */
    p = strchr(linebuf, '=');

    /* If no '=' exit */
    if (!p)
        return 0;

    *p++ = '\0';

    *pkw = linebuf;
    *pval = p;

    /* Remove spaces from keyword and value */
    remove_space(pkw);
    remove_space(pval);

    return 1;
}

/*
 * Unescape some escape sequences in string literals.
 * Return the result in a newly allocated buffer.
 * Currently only supports '\n'.
 * If the input length is 0, returns a valid 1-byte buffer, but sets
 * the length to 0.
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
    ret = p = OPENSSL_malloc(input_len);
    if (ret == NULL)
        return NULL;

    for (i = 0; i < input_len; i++) {
        if (input[i] == '\\') {
            if (i == input_len - 1 || input[i+1] != 'n')
                goto err;
            *p++ = '\n';
            i++;
        } else {
            *p++ = input[i];
        }
    }

    *out_len = p - ret;
    return ret;

 err:
    OPENSSL_free(ret);
    return NULL;
}

/* For a hex string "value" convert to a binary allocated buffer */
static int test_bin(const char *value, unsigned char **buf, size_t *buflen)
{
    long len;

    *buflen = 0;

    /* Check for empty value */
    if (!*value) {
        /*
         * Don't return NULL for zero length buffer.
         * This is needed for some tests with empty keys: HMAC_Init_ex() expects
         * a non-NULL key buffer even if the key length is 0, in order to detect
         * key reset.
         */
        *buf = OPENSSL_malloc(1);
        if (!*buf)
            return 0;
        **buf = 0;
        *buflen = 0;
        return 1;
    }

    /* Check for NULL literal */
    if (strcmp(value, "NULL") == 0) {
        *buf = NULL;
        *buflen = 0;
        return 1;
    }

    /* Check for string literal */
    if (value[0] == '"') {
        size_t vlen;
        value++;
        vlen = strlen(value);
        if (value[vlen - 1] != '"')
            return 0;
        vlen--;
        *buf = unescape(value, vlen, buflen);
        if (*buf == NULL)
            return 0;
        return 1;
    }

    /* Otherwise assume as hex literal and convert it to binary buffer */
    *buf = OPENSSL_hexstr2buf(value, &len);
    if (!*buf) {
        fprintf(stderr, "Value=%s\n", value);
        ERR_print_errors_fp(stderr);
        return -1;
    }
    /* Size of input buffer means we'll never overflow */
    *buflen = len;
    return 1;
}
#ifndef OPENSSL_NO_SCRYPT
/* Currently only used by scrypt tests */
/* Parse unsigned decimal 64 bit integer value */
static int test_uint64(const char *value, uint64_t *pr)
{
    const char *p = value;
    if (!*p) {
        fprintf(stderr, "Invalid empty integer value\n");
        return -1;
    }
    *pr = 0;
    while (*p) {
        if (*pr > UINT64_MAX/10) {
            fprintf(stderr, "Integer string overflow value=%s\n", value);
            return -1;
        }
        *pr *= 10;
        if (*p < '0' || *p > '9') {
            fprintf(stderr, "Invalid integer string value=%s\n", value);
            return -1;
        }
        *pr += *p - '0';
        p++;
    }
    return 1;
}
#endif

/* Structure holding test information */
struct evp_test {
    /* file being read */
    BIO *in;
    /* temp memory BIO for reading in keys */
    BIO *key;
    /* List of public and private keys */
    struct key_list *private;
    struct key_list *public;
    /* method for this test */
    const struct evp_test_method *meth;
    /* current line being processed */
    unsigned int line;
    /* start line of current test */
    unsigned int start_line;
    /* Error string for test */
    const char *err, *aux_err;
    /* Expected error value of test */
    char *expected_err;
    /* Expected error function string */
    char *func;
    /* Expected error reason string */
    char *reason;
    /* Number of tests */
    int ntests;
    /* Error count */
    int errors;
    /* Number of tests skipped */
    int nskip;
    /* If output mismatch expected and got value */
    unsigned char *out_received;
    size_t out_received_len;
    unsigned char *out_expected;
    size_t out_expected_len;
    /* test specific data */
    void *data;
    /* Current test should be skipped */
    int skip;
};

struct key_list {
    char *name;
    EVP_PKEY *key;
    struct key_list *next;
};

/* Test method structure */
struct evp_test_method {
    /* Name of test as it appears in file */
    const char *name;
    /* Initialise test for "alg" */
    int (*init) (struct evp_test * t, const char *alg);
    /* Clean up method */
    void (*cleanup) (struct evp_test * t);
    /* Test specific name value pair processing */
    int (*parse) (struct evp_test * t, const char *name, const char *value);
    /* Run the test itself */
    int (*run_test) (struct evp_test * t);
};

static const struct evp_test_method digest_test_method, cipher_test_method;
static const struct evp_test_method mac_test_method;
static const struct evp_test_method psign_test_method, pverify_test_method;
static const struct evp_test_method pdecrypt_test_method;
static const struct evp_test_method pverify_recover_test_method;
static const struct evp_test_method pderive_test_method;
static const struct evp_test_method pbe_test_method;
static const struct evp_test_method encode_test_method;
static const struct evp_test_method kdf_test_method;
static const struct evp_test_method keypair_test_method;

static const struct evp_test_method *evp_test_list[] = {
    &digest_test_method,
    &cipher_test_method,
    &mac_test_method,
    &psign_test_method,
    &pverify_test_method,
    &pdecrypt_test_method,
    &pverify_recover_test_method,
    &pderive_test_method,
    &pbe_test_method,
    &encode_test_method,
    &kdf_test_method,
    &keypair_test_method,
    NULL
};

static const struct evp_test_method *evp_find_test(const char *name)
{
    const struct evp_test_method **tt;

    for (tt = evp_test_list; *tt; tt++) {
        if (strcmp(name, (*tt)->name) == 0)
            return *tt;
    }
    return NULL;
}

static void hex_print(const char *name, const unsigned char *buf, size_t len)
{
    size_t i;
    fprintf(stderr, "%s ", name);
    for (i = 0; i < len; i++)
        fprintf(stderr, "%02X", buf[i]);
    fputs("\n", stderr);
}

static void free_expected(struct evp_test *t)
{
    OPENSSL_free(t->expected_err);
    t->expected_err = NULL;
    OPENSSL_free(t->func);
    t->func = NULL;
    OPENSSL_free(t->reason);
    t->reason = NULL;
    OPENSSL_free(t->out_expected);
    OPENSSL_free(t->out_received);
    t->out_expected = NULL;
    t->out_received = NULL;
    t->out_expected_len = 0;
    t->out_received_len = 0;
    /* Literals. */
    t->err = NULL;
}

static void print_expected(struct evp_test *t)
{
    if (t->out_expected == NULL && t->out_received == NULL)
        return;
    hex_print("Expected:", t->out_expected, t->out_expected_len);
    hex_print("Got:     ", t->out_received, t->out_received_len);
    free_expected(t);
}

static int check_test_error(struct evp_test *t)
{
    unsigned long err;
    const char *func;
    const char *reason;
    if (!t->err && !t->expected_err)
        return 1;
    if (t->err && !t->expected_err) {
        if (t->aux_err != NULL) {
            fprintf(stderr, "Test line %d(%s): unexpected error %s\n",
                    t->start_line, t->aux_err, t->err);
        } else {
            fprintf(stderr, "Test line %d: unexpected error %s\n",
                    t->start_line, t->err);
        }
        print_expected(t);
        return 0;
    }
    if (!t->err && t->expected_err) {
        fprintf(stderr, "Test line %d: succeeded expecting %s\n",
                t->start_line, t->expected_err);
        return 0;
    }

    if (strcmp(t->err, t->expected_err) != 0) {
        fprintf(stderr, "Test line %d: expecting %s got %s\n",
                t->start_line, t->expected_err, t->err);
        return 0;
    }

    if (t->func == NULL && t->reason == NULL)
        return 1;

    if (t->func == NULL || t->reason == NULL) {
        fprintf(stderr, "Test line %d: missing function or reason code\n",
                t->start_line);
        return 0;
    }

    err = ERR_peek_error();
    if (err == 0) {
        fprintf(stderr, "Test line %d, expected error \"%s:%s\" not set\n",
                t->start_line, t->func, t->reason);
        return 0;
    }

    func = ERR_func_error_string(err);
    reason = ERR_reason_error_string(err);

    if (func == NULL && reason == NULL) {
        fprintf(stderr, "Test line %d: expected error \"%s:%s\", no strings available.  Skipping...\n",
                t->start_line, t->func, t->reason);
        return 1;
    }

    if (strcmp(func, t->func) == 0 && strcmp(reason, t->reason) == 0)
        return 1;

    fprintf(stderr, "Test line %d: expected error \"%s:%s\", got \"%s:%s\"\n",
            t->start_line, t->func, t->reason, func, reason);

    return 0;
}

/* Setup a new test, run any existing test */

static int setup_test(struct evp_test *t, const struct evp_test_method *tmeth)
{
    /* If we already have a test set up run it */
    if (t->meth) {
        t->ntests++;
        if (t->skip) {
            t->nskip++;
        } else {
            /* run the test */
            if (t->err == NULL && t->meth->run_test(t) != 1) {
                fprintf(stderr, "%s test error line %d\n",
                        t->meth->name, t->start_line);
                return 0;
            }
            if (!check_test_error(t)) {
                if (t->err)
                    ERR_print_errors_fp(stderr);
                t->errors++;
            }
        }
        /* clean it up */
        ERR_clear_error();
        if (t->data != NULL) {
            t->meth->cleanup(t);
            OPENSSL_free(t->data);
            t->data = NULL;
        }
        OPENSSL_free(t->expected_err);
        t->expected_err = NULL;
        free_expected(t);
    }
    t->meth = tmeth;
    return 1;
}

static int find_key(EVP_PKEY **ppk, const char *name, struct key_list *lst)
{
    for (; lst; lst = lst->next) {
        if (strcmp(lst->name, name) == 0) {
            if (ppk)
                *ppk = lst->key;
            return 1;
        }
    }
    return 0;
}

static void free_key_list(struct key_list *lst)
{
    while (lst != NULL) {
        struct key_list *ltmp;
        EVP_PKEY_free(lst->key);
        OPENSSL_free(lst->name);
        ltmp = lst->next;
        OPENSSL_free(lst);
        lst = ltmp;
    }
}

static int check_unsupported()
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


static int read_key(struct evp_test *t)
{
    char tmpbuf[80];
    if (t->key == NULL)
        t->key = BIO_new(BIO_s_mem());
    else if (BIO_reset(t->key) <= 0)
        return 0;
    if (t->key == NULL) {
        fprintf(stderr, "Error allocating key memory BIO\n");
        return 0;
    }
    /* Read to PEM end line and place content in memory BIO */
    while (BIO_gets(t->in, tmpbuf, sizeof(tmpbuf))) {
        t->line++;
        if (BIO_puts(t->key, tmpbuf) <= 0) {
            fprintf(stderr, "Error writing to key memory BIO\n");
            return 0;
        }
        if (strncmp(tmpbuf, "-----END", 8) == 0)
            return 1;
    }
    fprintf(stderr, "Can't find key end\n");
    return 0;
}

static int process_test(struct evp_test *t, char *buf, int verbose)
{
    char *keyword = NULL, *value = NULL;
    int rv = 0, add_key = 0;
    struct key_list **lst = NULL, *key = NULL;
    EVP_PKEY *pk = NULL;
    const struct evp_test_method *tmeth = NULL;
    if (verbose)
        fputs(buf, stdout);
    if (!parse_line(&keyword, &value, buf))
        return 1;
    if (strcmp(keyword, "PrivateKey") == 0) {
        if (!read_key(t))
            return 0;
        pk = PEM_read_bio_PrivateKey(t->key, NULL, 0, NULL);
        if (pk == NULL && !check_unsupported()) {
            fprintf(stderr, "Error reading private key %s\n", value);
            ERR_print_errors_fp(stderr);
            return 0;
        }
        lst = &t->private;
        add_key = 1;
    }
    if (strcmp(keyword, "PublicKey") == 0) {
        if (!read_key(t))
            return 0;
        pk = PEM_read_bio_PUBKEY(t->key, NULL, 0, NULL);
        if (pk == NULL && !check_unsupported()) {
            fprintf(stderr, "Error reading public key %s\n", value);
            ERR_print_errors_fp(stderr);
            return 0;
        }
        lst = &t->public;
        add_key = 1;
    }
    /* If we have a key add to list */
    if (add_key) {
        if (find_key(NULL, value, *lst)) {
            fprintf(stderr, "Duplicate key %s\n", value);
            return 0;
        }
        key = OPENSSL_malloc(sizeof(*key));
        if (!key)
            return 0;
        key->name = OPENSSL_strdup(value);
        key->key = pk;
        key->next = *lst;
        *lst = key;
        return 1;
    }

    /* See if keyword corresponds to a test start */
    tmeth = evp_find_test(keyword);
    if (tmeth) {
        if (!setup_test(t, tmeth))
            return 0;
        t->start_line = t->line;
        t->skip = 0;
        if (!tmeth->init(t, value)) {
            fprintf(stderr, "Unknown %s: %s\n", keyword, value);
            return 0;
        }
        return 1;
    } else if (t->skip) {
        return 1;
    } else if (strcmp(keyword, "Result") == 0) {
        if (t->expected_err) {
            fprintf(stderr, "Line %d: multiple result lines\n", t->line);
            return 0;
        }
        t->expected_err = OPENSSL_strdup(value);
        if (t->expected_err == NULL)
            return 0;
    } else if (strcmp(keyword, "Function") == 0) {
        if (t->func != NULL) {
            fprintf(stderr, "Line %d: multiple function lines\n", t->line);
            return 0;
        }
        t->func = OPENSSL_strdup(value);
        if (t->func == NULL)
            return 0;
    } else if (strcmp(keyword, "Reason") == 0) {
        if (t->reason != NULL) {
            fprintf(stderr, "Line %d: multiple reason lines\n", t->line);
            return 0;
        }
        t->reason = OPENSSL_strdup(value);
        if (t->reason == NULL)
            return 0;
    } else {
        /* Must be test specific line: try to parse it */
        if (t->meth)
            rv = t->meth->parse(t, keyword, value);

        if (rv == 0)
            fprintf(stderr, "line %d: unexpected keyword %s\n",
                    t->line, keyword);

        if (rv < 0)
            fprintf(stderr, "line %d: error processing keyword %s\n",
                    t->line, keyword);
        if (rv <= 0)
            return 0;
    }
    return 1;
}

static int check_var_length_output(struct evp_test *t,
                                   const unsigned char *expected,
                                   size_t expected_len,
                                   const unsigned char *received,
                                   size_t received_len)
{
    if (expected_len == received_len &&
        memcmp(expected, received, expected_len) == 0) {
        return 0;
    }

    /* The result printing code expects a non-NULL buffer. */
    t->out_expected = OPENSSL_memdup(expected, expected_len ? expected_len : 1);
    t->out_expected_len = expected_len;
    t->out_received = OPENSSL_memdup(received, received_len ? received_len : 1);
    t->out_received_len = received_len;
    if (t->out_expected == NULL || t->out_received == NULL) {
        fprintf(stderr, "Memory allocation error!\n");
        exit(1);
    }
    return 1;
}

static int check_output(struct evp_test *t,
                        const unsigned char *expected,
                        const unsigned char *received,
                        size_t len)
{
    return check_var_length_output(t, expected, len, received, len);
}

int main(int argc, char **argv)
{
    BIO *in = NULL;
    char buf[10240];
    struct evp_test t;

    if (argc != 2) {
        fprintf(stderr, "usage: evp_test testfile.txt\n");
        return 1;
    }

    CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);

    memset(&t, 0, sizeof(t));
    t.start_line = -1;
    in = BIO_new_file(argv[1], "rb");
    if (in == NULL) {
        fprintf(stderr, "Can't open %s for reading\n", argv[1]);
        return 1;
    }
    t.in = in;
    t.err = NULL;
    while (BIO_gets(in, buf, sizeof(buf))) {
        t.line++;
        if (!process_test(&t, buf, 0))
            exit(1);
    }
    /* Run any final test we have */
    if (!setup_test(&t, NULL))
        exit(1);
    fprintf(stderr, "%d tests completed with %d errors, %d skipped\n",
            t.ntests, t.errors, t.nskip);
    free_key_list(t.public);
    free_key_list(t.private);
    BIO_free(t.key);
    BIO_free(in);

#ifndef OPENSSL_NO_CRYPTO_MDEBUG
    if (CRYPTO_mem_leaks_fp(stderr) <= 0)
        return 1;
#endif
    if (t.errors)
        return 1;
    return 0;
}

static void test_free(void *d)
{
    OPENSSL_free(d);
}

/* Message digest tests */

struct digest_data {
    /* Digest this test is for */
    const EVP_MD *digest;
    /* Input to digest */
    unsigned char *input;
    size_t input_len;
    /* Repeat count for input */
    size_t nrpt;
    /* Expected output */
    unsigned char *output;
    size_t output_len;
};

static int digest_test_init(struct evp_test *t, const char *alg)
{
    const EVP_MD *digest;
    struct digest_data *mdat;
    digest = EVP_get_digestbyname(alg);
    if (!digest) {
        /* If alg has an OID assume disabled algorithm */
        if (OBJ_sn2nid(alg) != NID_undef || OBJ_ln2nid(alg) != NID_undef) {
            t->skip = 1;
            return 1;
        }
        return 0;
    }
    mdat = OPENSSL_malloc(sizeof(*mdat));
    mdat->digest = digest;
    mdat->input = NULL;
    mdat->output = NULL;
    mdat->nrpt = 1;
    t->data = mdat;
    return 1;
}

static void digest_test_cleanup(struct evp_test *t)
{
    struct digest_data *mdat = t->data;
    test_free(mdat->input);
    test_free(mdat->output);
}

static int digest_test_parse(struct evp_test *t,
                             const char *keyword, const char *value)
{
    struct digest_data *mdata = t->data;
    if (strcmp(keyword, "Input") == 0)
        return test_bin(value, &mdata->input, &mdata->input_len);
    if (strcmp(keyword, "Output") == 0)
        return test_bin(value, &mdata->output, &mdata->output_len);
    if (strcmp(keyword, "Count") == 0) {
        long nrpt = atoi(value);
        if (nrpt <= 0)
            return 0;
        mdata->nrpt = (size_t)nrpt;
        return 1;
    }
    return 0;
}

static int digest_test_run(struct evp_test *t)
{
    struct digest_data *mdata = t->data;
    size_t i;
    const char *err = "INTERNAL_ERROR";
    EVP_MD_CTX *mctx;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    mctx = EVP_MD_CTX_new();
    if (!mctx)
        goto err;
    err = "DIGESTINIT_ERROR";
    if (!EVP_DigestInit_ex(mctx, mdata->digest, NULL))
        goto err;
    err = "DIGESTUPDATE_ERROR";
    for (i = 0; i < mdata->nrpt; i++) {
        if (!EVP_DigestUpdate(mctx, mdata->input, mdata->input_len))
            goto err;
    }
    err = "DIGESTFINAL_ERROR";
    if (!EVP_DigestFinal(mctx, md, &md_len))
        goto err;
    err = "DIGEST_LENGTH_MISMATCH";
    if (md_len != mdata->output_len)
        goto err;
    err = "DIGEST_MISMATCH";
    if (check_output(t, mdata->output, md, md_len))
        goto err;
    err = NULL;
 err:
    EVP_MD_CTX_free(mctx);
    t->err = err;
    return 1;
}

static const struct evp_test_method digest_test_method = {
    "Digest",
    digest_test_init,
    digest_test_cleanup,
    digest_test_parse,
    digest_test_run
};

/* Cipher tests */
struct cipher_data {
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
    /* GCM, CCM only */
    unsigned char *aad;
    size_t aad_len;
    unsigned char *tag;
    size_t tag_len;
};

static int cipher_test_init(struct evp_test *t, const char *alg)
{
    const EVP_CIPHER *cipher;
    struct cipher_data *cdat = t->data;
    cipher = EVP_get_cipherbyname(alg);
    if (!cipher) {
        /* If alg has an OID assume disabled algorithm */
        if (OBJ_sn2nid(alg) != NID_undef || OBJ_ln2nid(alg) != NID_undef) {
            t->skip = 1;
            return 1;
        }
        return 0;
    }
    cdat = OPENSSL_malloc(sizeof(*cdat));
    cdat->cipher = cipher;
    cdat->enc = -1;
    cdat->key = NULL;
    cdat->iv = NULL;
    cdat->ciphertext = NULL;
    cdat->plaintext = NULL;
    cdat->aad = NULL;
    cdat->tag = NULL;
    t->data = cdat;
    if (EVP_CIPHER_mode(cipher) == EVP_CIPH_GCM_MODE
        || EVP_CIPHER_mode(cipher) == EVP_CIPH_OCB_MODE
        || EVP_CIPHER_mode(cipher) == EVP_CIPH_CCM_MODE)
        cdat->aead = EVP_CIPHER_mode(cipher);
    else if (EVP_CIPHER_flags(cipher) & EVP_CIPH_FLAG_AEAD_CIPHER)
        cdat->aead = -1;
    else
        cdat->aead = 0;

    return 1;
}

static void cipher_test_cleanup(struct evp_test *t)
{
    struct cipher_data *cdat = t->data;
    test_free(cdat->key);
    test_free(cdat->iv);
    test_free(cdat->ciphertext);
    test_free(cdat->plaintext);
    test_free(cdat->aad);
    test_free(cdat->tag);
}

static int cipher_test_parse(struct evp_test *t, const char *keyword,
                             const char *value)
{
    struct cipher_data *cdat = t->data;
    if (strcmp(keyword, "Key") == 0)
        return test_bin(value, &cdat->key, &cdat->key_len);
    if (strcmp(keyword, "IV") == 0)
        return test_bin(value, &cdat->iv, &cdat->iv_len);
    if (strcmp(keyword, "Plaintext") == 0)
        return test_bin(value, &cdat->plaintext, &cdat->plaintext_len);
    if (strcmp(keyword, "Ciphertext") == 0)
        return test_bin(value, &cdat->ciphertext, &cdat->ciphertext_len);
    if (cdat->aead) {
        if (strcmp(keyword, "AAD") == 0)
            return test_bin(value, &cdat->aad, &cdat->aad_len);
        if (strcmp(keyword, "Tag") == 0)
            return test_bin(value, &cdat->tag, &cdat->tag_len);
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

static int cipher_test_enc(struct evp_test *t, int enc,
                           size_t out_misalign, size_t inp_misalign, int frag)
{
    struct cipher_data *cdat = t->data;
    unsigned char *in, *out, *tmp = NULL;
    size_t in_len, out_len, donelen = 0;
    int tmplen, chunklen, tmpflen;
    EVP_CIPHER_CTX *ctx = NULL;
    const char *err;
    err = "INTERNAL_ERROR";
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        goto err;
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
    if (enc) {
        in = cdat->plaintext;
        in_len = cdat->plaintext_len;
        out = cdat->ciphertext;
        out_len = cdat->ciphertext_len;
    } else {
        in = cdat->ciphertext;
        in_len = cdat->ciphertext_len;
        out = cdat->plaintext;
        out_len = cdat->plaintext_len;
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
    err = "CIPHERINIT_ERROR";
    if (!EVP_CipherInit_ex(ctx, cdat->cipher, NULL, NULL, NULL, enc))
        goto err;
    err = "INVALID_IV_LENGTH";
    if (cdat->iv) {
        if (cdat->aead) {
            if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN,
                                     cdat->iv_len, 0))
                goto err;
        } else if (cdat->iv_len != (size_t)EVP_CIPHER_CTX_iv_length(ctx))
            goto err;
    }
    if (cdat->aead) {
        unsigned char *tag;
        /*
         * If encrypting or OCB just set tag length initially, otherwise
         * set tag length and value.
         */
        if (enc || cdat->aead == EVP_CIPH_OCB_MODE) {
            err = "TAG_LENGTH_SET_ERROR";
            tag = NULL;
        } else {
            err = "TAG_SET_ERROR";
            tag = cdat->tag;
        }
        if (tag || cdat->aead != EVP_CIPH_GCM_MODE) {
            if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
                                     cdat->tag_len, tag))
                goto err;
        }
    }

    err = "INVALID_KEY_LENGTH";
    if (!EVP_CIPHER_CTX_set_key_length(ctx, cdat->key_len))
        goto err;
    err = "KEY_SET_ERROR";
    if (!EVP_CipherInit_ex(ctx, NULL, NULL, cdat->key, cdat->iv, -1))
        goto err;

    if (!enc && cdat->aead == EVP_CIPH_OCB_MODE) {
        if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
                                 cdat->tag_len, cdat->tag)) {
            err = "TAG_SET_ERROR";
            goto err;
        }
    }

    if (cdat->aead == EVP_CIPH_CCM_MODE) {
        if (!EVP_CipherUpdate(ctx, NULL, &tmplen, NULL, out_len)) {
            err = "CCM_PLAINTEXT_LENGTH_SET_ERROR";
            goto err;
        }
    }
    if (cdat->aad) {
        err = "AAD_SET_ERROR";
        if (!frag) {
            if (!EVP_CipherUpdate(ctx, NULL, &chunklen, cdat->aad,
                                  cdat->aad_len))
                goto err;
        } else {
            /*
             * Supply the AAD in chunks less than the block size where possible
             */
            if (cdat->aad_len > 0) {
                if (!EVP_CipherUpdate(ctx, NULL, &chunklen, cdat->aad, 1))
                    goto err;
                donelen++;
            }
            if (cdat->aad_len > 2) {
                if (!EVP_CipherUpdate(ctx, NULL, &chunklen, cdat->aad + donelen,
                                      cdat->aad_len - 2))
                    goto err;
                donelen += cdat->aad_len - 2;
            }
            if (cdat->aad_len > 1
                    && !EVP_CipherUpdate(ctx, NULL, &chunklen,
                                         cdat->aad + donelen, 1))
                goto err;
        }
    }
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    err = "CIPHERUPDATE_ERROR";
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
    if (cdat->aead == EVP_CIPH_CCM_MODE)
        tmpflen = 0;
    else {
        err = "CIPHERFINAL_ERROR";
        if (!EVP_CipherFinal_ex(ctx, tmp + out_misalign + tmplen, &tmpflen))
            goto err;
    }
    err = "LENGTH_MISMATCH";
    if (out_len != (size_t)(tmplen + tmpflen))
        goto err;
    err = "VALUE_MISMATCH";
    if (check_output(t, out, tmp + out_misalign, out_len))
        goto err;
    if (enc && cdat->aead) {
        unsigned char rtag[16];
        if (cdat->tag_len > sizeof(rtag)) {
            err = "TAG_LENGTH_INTERNAL_ERROR";
            goto err;
        }
        if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG,
                                 cdat->tag_len, rtag)) {
            err = "TAG_RETRIEVE_ERROR";
            goto err;
        }
        if (check_output(t, cdat->tag, rtag, cdat->tag_len)) {
            err = "TAG_VALUE_MISMATCH";
            goto err;
        }
    }
    err = NULL;
 err:
    OPENSSL_free(tmp);
    EVP_CIPHER_CTX_free(ctx);
    t->err = err;
    return err ? 0 : 1;
}

static int cipher_test_run(struct evp_test *t)
{
    struct cipher_data *cdat = t->data;
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

static const struct evp_test_method cipher_test_method = {
    "Cipher",
    cipher_test_init,
    cipher_test_cleanup,
    cipher_test_parse,
    cipher_test_run
};

struct mac_data {
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
};

static int mac_test_init(struct evp_test *t, const char *alg)
{
    int type;
    struct mac_data *mdat;
    if (strcmp(alg, "HMAC") == 0) {
        type = EVP_PKEY_HMAC;
    } else if (strcmp(alg, "CMAC") == 0) {
#ifndef OPENSSL_NO_CMAC
        type = EVP_PKEY_CMAC;
#else
        t->skip = 1;
        return 1;
#endif
    } else
        return 0;

    mdat = OPENSSL_malloc(sizeof(*mdat));
    mdat->type = type;
    mdat->alg = NULL;
    mdat->key = NULL;
    mdat->input = NULL;
    mdat->output = NULL;
    t->data = mdat;
    return 1;
}

static void mac_test_cleanup(struct evp_test *t)
{
    struct mac_data *mdat = t->data;
    test_free(mdat->alg);
    test_free(mdat->key);
    test_free(mdat->input);
    test_free(mdat->output);
}

static int mac_test_parse(struct evp_test *t,
                          const char *keyword, const char *value)
{
    struct mac_data *mdata = t->data;
    if (strcmp(keyword, "Key") == 0)
        return test_bin(value, &mdata->key, &mdata->key_len);
    if (strcmp(keyword, "Algorithm") == 0) {
        mdata->alg = OPENSSL_strdup(value);
        if (!mdata->alg)
            return 0;
        return 1;
    }
    if (strcmp(keyword, "Input") == 0)
        return test_bin(value, &mdata->input, &mdata->input_len);
    if (strcmp(keyword, "Output") == 0)
        return test_bin(value, &mdata->output, &mdata->output_len);
    return 0;
}

static int mac_test_run(struct evp_test *t)
{
    struct mac_data *mdata = t->data;
    const char *err = "INTERNAL_ERROR";
    EVP_MD_CTX *mctx = NULL;
    EVP_PKEY_CTX *pctx = NULL, *genctx = NULL;
    EVP_PKEY *key = NULL;
    const EVP_MD *md = NULL;
    unsigned char *mac = NULL;
    size_t mac_len;

#ifdef OPENSSL_NO_DES
    if (mdata->alg != NULL && strstr(mdata->alg, "DES") != NULL) {
        /* Skip DES */
        err = NULL;
        goto err;
    }
#endif

    err = "MAC_PKEY_CTX_ERROR";
    genctx = EVP_PKEY_CTX_new_id(mdata->type, NULL);
    if (!genctx)
        goto err;

    err = "MAC_KEYGEN_INIT_ERROR";
    if (EVP_PKEY_keygen_init(genctx) <= 0)
        goto err;
    if (mdata->type == EVP_PKEY_CMAC) {
        err = "MAC_ALGORITHM_SET_ERROR";
        if (EVP_PKEY_CTX_ctrl_str(genctx, "cipher", mdata->alg) <= 0)
            goto err;
    }

    err = "MAC_KEY_SET_ERROR";
    if (EVP_PKEY_CTX_set_mac_key(genctx, mdata->key, mdata->key_len) <= 0)
        goto err;

    err = "MAC_KEY_GENERATE_ERROR";
    if (EVP_PKEY_keygen(genctx, &key) <= 0)
        goto err;
    if (mdata->type == EVP_PKEY_HMAC) {
        err = "MAC_ALGORITHM_SET_ERROR";
        md = EVP_get_digestbyname(mdata->alg);
        if (!md)
            goto err;
    }
    mctx = EVP_MD_CTX_new();
    if (!mctx)
        goto err;
    err = "DIGESTSIGNINIT_ERROR";
    if (!EVP_DigestSignInit(mctx, &pctx, md, NULL, key))
        goto err;

    err = "DIGESTSIGNUPDATE_ERROR";
    if (!EVP_DigestSignUpdate(mctx, mdata->input, mdata->input_len))
        goto err;
    err = "DIGESTSIGNFINAL_LENGTH_ERROR";
    if (!EVP_DigestSignFinal(mctx, NULL, &mac_len))
        goto err;
    mac = OPENSSL_malloc(mac_len);
    if (!mac) {
        fprintf(stderr, "Error allocating mac buffer!\n");
        exit(1);
    }
    if (!EVP_DigestSignFinal(mctx, mac, &mac_len))
        goto err;
    err = "MAC_LENGTH_MISMATCH";
    if (mac_len != mdata->output_len)
        goto err;
    err = "MAC_MISMATCH";
    if (check_output(t, mdata->output, mac, mac_len))
        goto err;
    err = NULL;
 err:
    EVP_MD_CTX_free(mctx);
    OPENSSL_free(mac);
    EVP_PKEY_CTX_free(genctx);
    EVP_PKEY_free(key);
    t->err = err;
    return 1;
}

static const struct evp_test_method mac_test_method = {
    "MAC",
    mac_test_init,
    mac_test_cleanup,
    mac_test_parse,
    mac_test_run
};

/*
 * Public key operations. These are all very similar and can share
 * a lot of common code.
 */

struct pkey_data {
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
};

/*
 * Perform public key operation setup: lookup key, allocated ctx and call
 * the appropriate initialisation function
 */
static int pkey_test_init(struct evp_test *t, const char *name,
                          int use_public,
                          int (*keyopinit) (EVP_PKEY_CTX *ctx),
                          int (*keyop) (EVP_PKEY_CTX *ctx,
                                        unsigned char *sig, size_t *siglen,
                                        const unsigned char *tbs,
                                        size_t tbslen)
    )
{
    struct pkey_data *kdata;
    EVP_PKEY *pkey = NULL;
    int rv = 0;
    if (use_public)
        rv = find_key(&pkey, name, t->public);
    if (!rv)
        rv = find_key(&pkey, name, t->private);
    if (!rv || pkey == NULL) {
        t->skip = 1;
        return 1;
    }

    kdata = OPENSSL_malloc(sizeof(*kdata));
    if (!kdata) {
        EVP_PKEY_free(pkey);
        return 0;
    }
    kdata->ctx = NULL;
    kdata->input = NULL;
    kdata->output = NULL;
    kdata->keyop = keyop;
    t->data = kdata;
    kdata->ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!kdata->ctx)
        return 0;
    if (keyopinit(kdata->ctx) <= 0)
        t->err = "KEYOP_INIT_ERROR";
    return 1;
}

static void pkey_test_cleanup(struct evp_test *t)
{
    struct pkey_data *kdata = t->data;

    OPENSSL_free(kdata->input);
    OPENSSL_free(kdata->output);
    EVP_PKEY_CTX_free(kdata->ctx);
}

static int pkey_test_ctrl(struct evp_test *t, EVP_PKEY_CTX *pctx,
                          const char *value)
{
    int rv;
    char *p, *tmpval;

    tmpval = OPENSSL_strdup(value);
    if (tmpval == NULL)
        return 0;
    p = strchr(tmpval, ':');
    if (p != NULL)
        *p++ = 0;
    rv = EVP_PKEY_CTX_ctrl_str(pctx, tmpval, p);
    if (rv == -2) {
        t->err = "PKEY_CTRL_INVALID";
        rv = 1;
    } else if (p != NULL && rv <= 0) {
        /* If p has an OID and lookup fails assume disabled algorithm */
        int nid = OBJ_sn2nid(p);
        if (nid == NID_undef)
             nid = OBJ_ln2nid(p);
        if ((nid != NID_undef) && EVP_get_digestbynid(nid) == NULL &&
            EVP_get_cipherbynid(nid) == NULL) {
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

static int pkey_test_parse(struct evp_test *t,
                           const char *keyword, const char *value)
{
    struct pkey_data *kdata = t->data;
    if (strcmp(keyword, "Input") == 0)
        return test_bin(value, &kdata->input, &kdata->input_len);
    if (strcmp(keyword, "Output") == 0)
        return test_bin(value, &kdata->output, &kdata->output_len);
    if (strcmp(keyword, "Ctrl") == 0)
        return pkey_test_ctrl(t, kdata->ctx, value);
    return 0;
}

static int pkey_test_run(struct evp_test *t)
{
    struct pkey_data *kdata = t->data;
    unsigned char *out = NULL;
    size_t out_len;
    const char *err = "KEYOP_LENGTH_ERROR";
    if (kdata->keyop(kdata->ctx, NULL, &out_len, kdata->input,
                     kdata->input_len) <= 0)
        goto err;
    out = OPENSSL_malloc(out_len);
    if (!out) {
        fprintf(stderr, "Error allocating output buffer!\n");
        exit(1);
    }
    err = "KEYOP_ERROR";
    if (kdata->keyop
        (kdata->ctx, out, &out_len, kdata->input, kdata->input_len) <= 0)
        goto err;
    err = "KEYOP_LENGTH_MISMATCH";
    if (out_len != kdata->output_len)
        goto err;
    err = "KEYOP_MISMATCH";
    if (check_output(t, kdata->output, out, out_len))
        goto err;
    err = NULL;
 err:
    OPENSSL_free(out);
    t->err = err;
    return 1;
}

static int sign_test_init(struct evp_test *t, const char *name)
{
    return pkey_test_init(t, name, 0, EVP_PKEY_sign_init, EVP_PKEY_sign);
}

static const struct evp_test_method psign_test_method = {
    "Sign",
    sign_test_init,
    pkey_test_cleanup,
    pkey_test_parse,
    pkey_test_run
};

static int verify_recover_test_init(struct evp_test *t, const char *name)
{
    return pkey_test_init(t, name, 1, EVP_PKEY_verify_recover_init,
                          EVP_PKEY_verify_recover);
}

static const struct evp_test_method pverify_recover_test_method = {
    "VerifyRecover",
    verify_recover_test_init,
    pkey_test_cleanup,
    pkey_test_parse,
    pkey_test_run
};

static int decrypt_test_init(struct evp_test *t, const char *name)
{
    return pkey_test_init(t, name, 0, EVP_PKEY_decrypt_init,
                          EVP_PKEY_decrypt);
}

static const struct evp_test_method pdecrypt_test_method = {
    "Decrypt",
    decrypt_test_init,
    pkey_test_cleanup,
    pkey_test_parse,
    pkey_test_run
};

static int verify_test_init(struct evp_test *t, const char *name)
{
    return pkey_test_init(t, name, 1, EVP_PKEY_verify_init, 0);
}

static int verify_test_run(struct evp_test *t)
{
    struct pkey_data *kdata = t->data;
    if (EVP_PKEY_verify(kdata->ctx, kdata->output, kdata->output_len,
                        kdata->input, kdata->input_len) <= 0)
        t->err = "VERIFY_ERROR";
    return 1;
}

static const struct evp_test_method pverify_test_method = {
    "Verify",
    verify_test_init,
    pkey_test_cleanup,
    pkey_test_parse,
    verify_test_run
};


static int pderive_test_init(struct evp_test *t, const char *name)
{
    return pkey_test_init(t, name, 0, EVP_PKEY_derive_init, 0);
}

static int pderive_test_parse(struct evp_test *t,
                              const char *keyword, const char *value)
{
    struct pkey_data *kdata = t->data;

    if (strcmp(keyword, "PeerKey") == 0) {
        EVP_PKEY *peer;
        if (find_key(&peer, value, t->public) == 0)
            return 0;
        if (EVP_PKEY_derive_set_peer(kdata->ctx, peer) <= 0)
            return 0;
        return 1;
    }
    if (strcmp(keyword, "SharedSecret") == 0)
        return test_bin(value, &kdata->output, &kdata->output_len);
    if (strcmp(keyword, "Ctrl") == 0)
        return pkey_test_ctrl(t, kdata->ctx, value);
    return 0;
}

static int pderive_test_run(struct evp_test *t)
{
    struct pkey_data *kdata = t->data;
    unsigned char *out = NULL;
    size_t out_len;
    const char *err = "DERIVE_ERROR";

    if (EVP_PKEY_derive(kdata->ctx, NULL, &out_len) <= 0)
        goto err;
    out = OPENSSL_malloc(out_len);
    if (!out) {
        fprintf(stderr, "Error allocating output buffer!\n");
        exit(1);
    }
    if (EVP_PKEY_derive(kdata->ctx, out, &out_len) <= 0)
        goto err;
    err = "SHARED_SECRET_LENGTH_MISMATCH";
    if (kdata->output == NULL || out_len != kdata->output_len)
        goto err;
    err = "SHARED_SECRET_MISMATCH";
    if (check_output(t, kdata->output, out, out_len))
        goto err;
    err = NULL;
 err:
    OPENSSL_free(out);
    t->err = err;
    return 1;
}

static const struct evp_test_method pderive_test_method = {
    "Derive",
    pderive_test_init,
    pkey_test_cleanup,
    pderive_test_parse,
    pderive_test_run
};

/* PBE tests */

#define PBE_TYPE_SCRYPT 1
#define PBE_TYPE_PBKDF2 2
#define PBE_TYPE_PKCS12 3

struct pbe_data {

    int pbe_type;

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
};

#ifndef OPENSSL_NO_SCRYPT
static int scrypt_test_parse(struct evp_test *t,
                             const char *keyword, const char *value)
{
    struct pbe_data *pdata = t->data;

    if (strcmp(keyword, "N") == 0)
        return test_uint64(value, &pdata->N);
    if (strcmp(keyword, "p") == 0)
        return test_uint64(value, &pdata->p);
    if (strcmp(keyword, "r") == 0)
        return test_uint64(value, &pdata->r);
    if (strcmp(keyword, "maxmem") == 0)
        return test_uint64(value, &pdata->maxmem);
    return 0;
}
#endif

static int pbkdf2_test_parse(struct evp_test *t,
                             const char *keyword, const char *value)
{
    struct pbe_data *pdata = t->data;

    if (strcmp(keyword, "iter") == 0) {
        pdata->iter = atoi(value);
        if (pdata->iter <= 0)
            return 0;
        return 1;
    }
    if (strcmp(keyword, "MD") == 0) {
        pdata->md = EVP_get_digestbyname(value);
        if (pdata->md == NULL)
            return 0;
        return 1;
    }
    return 0;
}

static int pkcs12_test_parse(struct evp_test *t,
                             const char *keyword, const char *value)
{
    struct pbe_data *pdata = t->data;

    if (strcmp(keyword, "id") == 0) {
        pdata->id = atoi(value);
        if (pdata->id <= 0)
            return 0;
        return 1;
    }
    return pbkdf2_test_parse(t, keyword, value);
}

static int pbe_test_init(struct evp_test *t, const char *alg)
{
    struct pbe_data *pdat;
    int pbe_type = 0;

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
        fprintf(stderr, "Unknown pbe algorithm %s\n", alg);
    }
    pdat = OPENSSL_malloc(sizeof(*pdat));
    pdat->pbe_type = pbe_type;
    pdat->pass = NULL;
    pdat->salt = NULL;
    pdat->N = 0;
    pdat->r = 0;
    pdat->p = 0;
    pdat->maxmem = 0;
    pdat->id = 0;
    pdat->iter = 0;
    pdat->md = NULL;
    t->data = pdat;
    return 1;
}

static void pbe_test_cleanup(struct evp_test *t)
{
    struct pbe_data *pdat = t->data;
    test_free(pdat->pass);
    test_free(pdat->salt);
    test_free(pdat->key);
}

static int pbe_test_parse(struct evp_test *t,
                             const char *keyword, const char *value)
{
    struct pbe_data *pdata = t->data;

    if (strcmp(keyword, "Password") == 0)
        return test_bin(value, &pdata->pass, &pdata->pass_len);
    if (strcmp(keyword, "Salt") == 0)
        return test_bin(value, &pdata->salt, &pdata->salt_len);
    if (strcmp(keyword, "Key") == 0)
        return test_bin(value, &pdata->key, &pdata->key_len);
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

static int pbe_test_run(struct evp_test *t)
{
    struct pbe_data *pdata = t->data;
    const char *err = "INTERNAL_ERROR";
    unsigned char *key;

    key = OPENSSL_malloc(pdata->key_len);
    if (!key)
        goto err;
    if (pdata->pbe_type == PBE_TYPE_PBKDF2) {
        err = "PBKDF2_ERROR";
        if (PKCS5_PBKDF2_HMAC((char *)pdata->pass, pdata->pass_len,
                              pdata->salt, pdata->salt_len,
                              pdata->iter, pdata->md,
                              pdata->key_len, key) == 0)
            goto err;
#ifndef OPENSSL_NO_SCRYPT
    } else if (pdata->pbe_type == PBE_TYPE_SCRYPT) {
        err = "SCRYPT_ERROR";
        if (EVP_PBE_scrypt((const char *)pdata->pass, pdata->pass_len,
                           pdata->salt, pdata->salt_len,
                           pdata->N, pdata->r, pdata->p, pdata->maxmem,
                           key, pdata->key_len) == 0)
            goto err;
#endif
    } else if (pdata->pbe_type == PBE_TYPE_PKCS12) {
        err = "PKCS12_ERROR";
        if (PKCS12_key_gen_uni(pdata->pass, pdata->pass_len,
                               pdata->salt, pdata->salt_len,
                               pdata->id, pdata->iter, pdata->key_len,
                               key, pdata->md) == 0)
            goto err;
    }
    err = "KEY_MISMATCH";
    if (check_output(t, pdata->key, key, pdata->key_len))
        goto err;
    err = NULL;
    err:
    OPENSSL_free(key);
    t->err = err;
    return 1;
}

static const struct evp_test_method pbe_test_method = {
    "PBE",
    pbe_test_init,
    pbe_test_cleanup,
    pbe_test_parse,
    pbe_test_run
};

/* Base64 tests */

typedef enum {
    BASE64_CANONICAL_ENCODING = 0,
    BASE64_VALID_ENCODING = 1,
    BASE64_INVALID_ENCODING = 2
} base64_encoding_type;

struct encode_data {
    /* Input to encoding */
    unsigned char *input;
    size_t input_len;
    /* Expected output */
    unsigned char *output;
    size_t output_len;
    base64_encoding_type encoding;
};

static int encode_test_init(struct evp_test *t, const char *encoding)
{
    struct encode_data *edata = OPENSSL_zalloc(sizeof(*edata));

    if (strcmp(encoding, "canonical") == 0) {
        edata->encoding = BASE64_CANONICAL_ENCODING;
    } else if (strcmp(encoding, "valid") == 0) {
        edata->encoding = BASE64_VALID_ENCODING;
    } else if (strcmp(encoding, "invalid") == 0) {
        edata->encoding = BASE64_INVALID_ENCODING;
        t->expected_err = OPENSSL_strdup("DECODE_ERROR");
        if (t->expected_err == NULL)
            return 0;
    } else {
        fprintf(stderr, "Bad encoding: %s. Should be one of "
                "{canonical, valid, invalid}\n", encoding);
        return 0;
    }
    t->data = edata;
    return 1;
}

static void encode_test_cleanup(struct evp_test *t)
{
    struct encode_data *edata = t->data;
    test_free(edata->input);
    test_free(edata->output);
    memset(edata, 0, sizeof(*edata));
}

static int encode_test_parse(struct evp_test *t,
                             const char *keyword, const char *value)
{
    struct encode_data *edata = t->data;
    if (strcmp(keyword, "Input") == 0)
        return test_bin(value, &edata->input, &edata->input_len);
    if (strcmp(keyword, "Output") == 0)
        return test_bin(value, &edata->output, &edata->output_len);
    return 0;
}

static int encode_test_run(struct evp_test *t)
{
    struct encode_data *edata = t->data;
    unsigned char *encode_out = NULL, *decode_out = NULL;
    int output_len, chunk_len;
    const char *err = "INTERNAL_ERROR";
    EVP_ENCODE_CTX *decode_ctx = EVP_ENCODE_CTX_new();

    if (decode_ctx == NULL)
        goto err;

    if (edata->encoding == BASE64_CANONICAL_ENCODING) {
        EVP_ENCODE_CTX *encode_ctx = EVP_ENCODE_CTX_new();
        if (encode_ctx == NULL)
            goto err;
        encode_out = OPENSSL_malloc(EVP_ENCODE_LENGTH(edata->input_len));
        if (encode_out == NULL)
            goto err;

        EVP_EncodeInit(encode_ctx);
        EVP_EncodeUpdate(encode_ctx, encode_out, &chunk_len,
                         edata->input, edata->input_len);
        output_len = chunk_len;

        EVP_EncodeFinal(encode_ctx, encode_out + chunk_len, &chunk_len);
        output_len += chunk_len;

        EVP_ENCODE_CTX_free(encode_ctx);

        if (check_var_length_output(t, edata->output, edata->output_len,
                                    encode_out, output_len)) {
            err = "BAD_ENCODING";
            goto err;
        }
    }

    decode_out = OPENSSL_malloc(EVP_DECODE_LENGTH(edata->output_len));
    if (decode_out == NULL)
        goto err;

    EVP_DecodeInit(decode_ctx);
    if (EVP_DecodeUpdate(decode_ctx, decode_out, &chunk_len, edata->output,
                         edata->output_len) < 0) {
        err = "DECODE_ERROR";
        goto err;
    }
    output_len = chunk_len;

    if (EVP_DecodeFinal(decode_ctx, decode_out + chunk_len, &chunk_len) != 1) {
        err = "DECODE_ERROR";
        goto err;
    }
    output_len += chunk_len;

    if (edata->encoding != BASE64_INVALID_ENCODING &&
        check_var_length_output(t, edata->input, edata->input_len,
                                decode_out, output_len)) {
        err = "BAD_DECODING";
        goto err;
    }

    err = NULL;
 err:
    t->err = err;
    OPENSSL_free(encode_out);
    OPENSSL_free(decode_out);
    EVP_ENCODE_CTX_free(decode_ctx);
    return 1;
}

static const struct evp_test_method encode_test_method = {
    "Encoding",
    encode_test_init,
    encode_test_cleanup,
    encode_test_parse,
    encode_test_run,
};

/* KDF operations */

struct kdf_data {
    /* Context for this operation */
    EVP_PKEY_CTX *ctx;
    /* Expected output */
    unsigned char *output;
    size_t output_len;
};

/*
 * Perform public key operation setup: lookup key, allocated ctx and call
 * the appropriate initialisation function
 */
static int kdf_test_init(struct evp_test *t, const char *name)
{
    struct kdf_data *kdata;

    kdata = OPENSSL_malloc(sizeof(*kdata));
    if (kdata == NULL)
        return 0;
    kdata->ctx = NULL;
    kdata->output = NULL;
    t->data = kdata;
    kdata->ctx = EVP_PKEY_CTX_new_id(OBJ_sn2nid(name), NULL);
    if (kdata->ctx == NULL)
        return 0;
    if (EVP_PKEY_derive_init(kdata->ctx) <= 0)
        return 0;
    return 1;
}

static void kdf_test_cleanup(struct evp_test *t)
{
    struct kdf_data *kdata = t->data;
    OPENSSL_free(kdata->output);
    EVP_PKEY_CTX_free(kdata->ctx);
}

static int kdf_test_parse(struct evp_test *t,
                          const char *keyword, const char *value)
{
    struct kdf_data *kdata = t->data;
    if (strcmp(keyword, "Output") == 0)
        return test_bin(value, &kdata->output, &kdata->output_len);
    if (strncmp(keyword, "Ctrl", 4) == 0)
        return pkey_test_ctrl(t, kdata->ctx, value);
    return 0;
}

static int kdf_test_run(struct evp_test *t)
{
    struct kdf_data *kdata = t->data;
    unsigned char *out = NULL;
    size_t out_len = kdata->output_len;
    const char *err = "INTERNAL_ERROR";
    out = OPENSSL_malloc(out_len);
    if (!out) {
        fprintf(stderr, "Error allocating output buffer!\n");
        exit(1);
    }
    err = "KDF_DERIVE_ERROR";
    if (EVP_PKEY_derive(kdata->ctx, out, &out_len) <= 0)
        goto err;
    err = "KDF_LENGTH_MISMATCH";
    if (out_len != kdata->output_len)
        goto err;
    err = "KDF_MISMATCH";
    if (check_output(t, kdata->output, out, out_len))
        goto err;
    err = NULL;
 err:
    OPENSSL_free(out);
    t->err = err;
    return 1;
}

static const struct evp_test_method kdf_test_method = {
    "KDF",
    kdf_test_init,
    kdf_test_cleanup,
    kdf_test_parse,
    kdf_test_run
};

struct keypair_test_data {
    EVP_PKEY *privk;
    EVP_PKEY *pubk;
};

static int keypair_test_init(struct evp_test *t, const char *pair)
{
    int rv = 0;
    EVP_PKEY *pk = NULL, *pubk = NULL;
    char *pub, *priv = NULL;
    const char *err = "INTERNAL_ERROR";
    struct keypair_test_data *data;

    priv = OPENSSL_strdup(pair);
    if (priv == NULL)
        return 0;
    pub = strchr(priv, ':');
    if ( pub == NULL ) {
        fprintf(stderr, "Wrong syntax \"%s\"\n", pair);
        goto end;
    }
    *pub++ = 0; /* split priv and pub strings */

    if (find_key(&pk, priv, t->private) == 0) {
        fprintf(stderr, "Cannot find private key: %s\n", priv);
        err = "MISSING_PRIVATE_KEY";
        goto end;
    }
    if (find_key(&pubk, pub, t->public) == 0) {
        fprintf(stderr, "Cannot find public key: %s\n", pub);
        err = "MISSING_PUBLIC_KEY";
        goto end;
    }

    if (pk == NULL && pubk == NULL) {
        /* Both keys are listed but unsupported: skip this test */
        t->skip = 1;
        rv = 1;
        goto end;
    }

    data = OPENSSL_malloc(sizeof(*data));
    if (data == NULL )
        goto end;

    data->privk = pk;
    data->pubk = pubk;
    t->data = data;

    rv = 1;
    err = NULL;

end:
    if (priv)
        OPENSSL_free(priv);
    t->err = err;
    return rv;
}

static void keypair_test_cleanup(struct evp_test *t)
{
    struct keypair_test_data *data = t->data;
    t->data = NULL;
    if (data)
        test_free(data);
    return;
}

/* For test that do not accept any custom keyword:
 *      return 0 if called
 */
static int void_test_parse(struct evp_test *t, const char *keyword, const char *value)
{
    return 0;
}

static int keypair_test_run(struct evp_test *t)
{
    int rv = 0;
    const struct keypair_test_data *pair = t->data;
    const char *err = "INTERNAL_ERROR";

    if (pair == NULL)
        goto end;

    if (pair->privk == NULL || pair->pubk == NULL) {
        /* this can only happen if only one of the keys is not set
         * which means that one of them was unsupported while the
         * other isn't: hence a key type mismatch.
         */
        err = "KEYPAIR_TYPE_MISMATCH";
        rv = 1;
        goto end;
    }

    if ((rv = EVP_PKEY_cmp(pair->privk, pair->pubk)) != 1 ) {
        if ( 0 == rv ) {
            err = "KEYPAIR_MISMATCH";
        } else if ( -1 == rv ) {
            err = "KEYPAIR_TYPE_MISMATCH";
        } else if ( -2 == rv ) {
            err = "UNSUPPORTED_KEY_COMPARISON";
        } else {
            fprintf(stderr, "Unexpected error in key comparison\n");
            rv = 0;
            goto end;
        }
        rv = 1;
        goto end;
    }

    rv = 1;
    err = NULL;

end:
    t->err = err;
    return rv;
}

static const struct evp_test_method keypair_test_method = {
    "PrivPubKeyPair",
    keypair_test_init,
    keypair_test_cleanup,
    void_test_parse,
    keypair_test_run
};

