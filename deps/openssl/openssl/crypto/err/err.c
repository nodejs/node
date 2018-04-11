/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <internal/cryptlib_int.h>
#include <internal/err.h>
#include <internal/err_int.h>
#include <openssl/lhash.h>
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/bio.h>
#include <openssl/opensslconf.h>
#include <internal/thread_once.h>

static void err_load_strings(int lib, ERR_STRING_DATA *str);

static void ERR_STATE_free(ERR_STATE *s);
#ifndef OPENSSL_NO_ERR
static ERR_STRING_DATA ERR_str_libraries[] = {
    {ERR_PACK(ERR_LIB_NONE, 0, 0), "unknown library"},
    {ERR_PACK(ERR_LIB_SYS, 0, 0), "system library"},
    {ERR_PACK(ERR_LIB_BN, 0, 0), "bignum routines"},
    {ERR_PACK(ERR_LIB_RSA, 0, 0), "rsa routines"},
    {ERR_PACK(ERR_LIB_DH, 0, 0), "Diffie-Hellman routines"},
    {ERR_PACK(ERR_LIB_EVP, 0, 0), "digital envelope routines"},
    {ERR_PACK(ERR_LIB_BUF, 0, 0), "memory buffer routines"},
    {ERR_PACK(ERR_LIB_OBJ, 0, 0), "object identifier routines"},
    {ERR_PACK(ERR_LIB_PEM, 0, 0), "PEM routines"},
    {ERR_PACK(ERR_LIB_DSA, 0, 0), "dsa routines"},
    {ERR_PACK(ERR_LIB_X509, 0, 0), "x509 certificate routines"},
    {ERR_PACK(ERR_LIB_ASN1, 0, 0), "asn1 encoding routines"},
    {ERR_PACK(ERR_LIB_CONF, 0, 0), "configuration file routines"},
    {ERR_PACK(ERR_LIB_CRYPTO, 0, 0), "common libcrypto routines"},
    {ERR_PACK(ERR_LIB_EC, 0, 0), "elliptic curve routines"},
    {ERR_PACK(ERR_LIB_ECDSA, 0, 0), "ECDSA routines"},
    {ERR_PACK(ERR_LIB_ECDH, 0, 0), "ECDH routines"},
    {ERR_PACK(ERR_LIB_SSL, 0, 0), "SSL routines"},
    {ERR_PACK(ERR_LIB_BIO, 0, 0), "BIO routines"},
    {ERR_PACK(ERR_LIB_PKCS7, 0, 0), "PKCS7 routines"},
    {ERR_PACK(ERR_LIB_X509V3, 0, 0), "X509 V3 routines"},
    {ERR_PACK(ERR_LIB_PKCS12, 0, 0), "PKCS12 routines"},
    {ERR_PACK(ERR_LIB_RAND, 0, 0), "random number generator"},
    {ERR_PACK(ERR_LIB_DSO, 0, 0), "DSO support routines"},
    {ERR_PACK(ERR_LIB_TS, 0, 0), "time stamp routines"},
    {ERR_PACK(ERR_LIB_ENGINE, 0, 0), "engine routines"},
    {ERR_PACK(ERR_LIB_OCSP, 0, 0), "OCSP routines"},
    {ERR_PACK(ERR_LIB_UI, 0, 0), "UI routines"},
    {ERR_PACK(ERR_LIB_FIPS, 0, 0), "FIPS routines"},
    {ERR_PACK(ERR_LIB_CMS, 0, 0), "CMS routines"},
    {ERR_PACK(ERR_LIB_HMAC, 0, 0), "HMAC routines"},
    {ERR_PACK(ERR_LIB_CT, 0, 0), "CT routines"},
    {ERR_PACK(ERR_LIB_ASYNC, 0, 0), "ASYNC routines"},
    {ERR_PACK(ERR_LIB_KDF, 0, 0), "KDF routines"},
    {0, NULL},
};

static ERR_STRING_DATA ERR_str_functs[] = {
    {ERR_PACK(0, SYS_F_FOPEN, 0), "fopen"},
    {ERR_PACK(0, SYS_F_CONNECT, 0), "connect"},
    {ERR_PACK(0, SYS_F_GETSERVBYNAME, 0), "getservbyname"},
    {ERR_PACK(0, SYS_F_SOCKET, 0), "socket"},
    {ERR_PACK(0, SYS_F_IOCTLSOCKET, 0), "ioctlsocket"},
    {ERR_PACK(0, SYS_F_BIND, 0), "bind"},
    {ERR_PACK(0, SYS_F_LISTEN, 0), "listen"},
    {ERR_PACK(0, SYS_F_ACCEPT, 0), "accept"},
# ifdef OPENSSL_SYS_WINDOWS
    {ERR_PACK(0, SYS_F_WSASTARTUP, 0), "WSAstartup"},
# endif
    {ERR_PACK(0, SYS_F_OPENDIR, 0), "opendir"},
    {ERR_PACK(0, SYS_F_FREAD, 0), "fread"},
    {ERR_PACK(0, SYS_F_GETADDRINFO, 0), "getaddrinfo"},
    {ERR_PACK(0, SYS_F_GETNAMEINFO, 0), "getnameinfo"},
    {ERR_PACK(0, SYS_F_SETSOCKOPT, 0), "setsockopt"},
    {ERR_PACK(0, SYS_F_GETSOCKOPT, 0), "getsockopt"},
    {ERR_PACK(0, SYS_F_GETSOCKNAME, 0), "getsockname"},
    {ERR_PACK(0, SYS_F_GETHOSTBYNAME, 0), "gethostbyname"},
    {ERR_PACK(0, SYS_F_FFLUSH, 0), "fflush"},
    {0, NULL},
};

static ERR_STRING_DATA ERR_str_reasons[] = {
    {ERR_R_SYS_LIB, "system lib"},
    {ERR_R_BN_LIB, "BN lib"},
    {ERR_R_RSA_LIB, "RSA lib"},
    {ERR_R_DH_LIB, "DH lib"},
    {ERR_R_EVP_LIB, "EVP lib"},
    {ERR_R_BUF_LIB, "BUF lib"},
    {ERR_R_OBJ_LIB, "OBJ lib"},
    {ERR_R_PEM_LIB, "PEM lib"},
    {ERR_R_DSA_LIB, "DSA lib"},
    {ERR_R_X509_LIB, "X509 lib"},
    {ERR_R_ASN1_LIB, "ASN1 lib"},
    {ERR_R_EC_LIB, "EC lib"},
    {ERR_R_BIO_LIB, "BIO lib"},
    {ERR_R_PKCS7_LIB, "PKCS7 lib"},
    {ERR_R_X509V3_LIB, "X509V3 lib"},
    {ERR_R_ENGINE_LIB, "ENGINE lib"},
    {ERR_R_ECDSA_LIB, "ECDSA lib"},

    {ERR_R_NESTED_ASN1_ERROR, "nested asn1 error"},
    {ERR_R_MISSING_ASN1_EOS, "missing asn1 eos"},

    {ERR_R_FATAL, "fatal"},
    {ERR_R_MALLOC_FAILURE, "malloc failure"},
    {ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED,
     "called a function you should not call"},
    {ERR_R_PASSED_NULL_PARAMETER, "passed a null parameter"},
    {ERR_R_INTERNAL_ERROR, "internal error"},
    {ERR_R_DISABLED, "called a function that was disabled at compile-time"},
    {ERR_R_INIT_FAIL, "init fail"},

    {0, NULL},
};
#endif

static CRYPTO_ONCE err_init = CRYPTO_ONCE_STATIC_INIT;
static int set_err_thread_local;
static CRYPTO_THREAD_LOCAL err_thread_local;

static CRYPTO_ONCE err_string_init = CRYPTO_ONCE_STATIC_INIT;
static CRYPTO_RWLOCK *err_string_lock;

static ERR_STRING_DATA *int_err_get_item(const ERR_STRING_DATA *);

/*
 * The internal state
 */

static LHASH_OF(ERR_STRING_DATA) *int_error_hash = NULL;
static int int_err_library_number = ERR_LIB_USER;

static unsigned long get_error_values(int inc, int top, const char **file,
                                      int *line, const char **data,
                                      int *flags);

static unsigned long err_string_data_hash(const ERR_STRING_DATA *a)
{
    unsigned long ret, l;

    l = a->error;
    ret = l ^ ERR_GET_LIB(l) ^ ERR_GET_FUNC(l);
    return (ret ^ ret % 19 * 13);
}

static int err_string_data_cmp(const ERR_STRING_DATA *a,
                               const ERR_STRING_DATA *b)
{
    return (int)(a->error - b->error);
}

static ERR_STRING_DATA *int_err_get_item(const ERR_STRING_DATA *d)
{
    ERR_STRING_DATA *p = NULL;

    CRYPTO_THREAD_read_lock(err_string_lock);
    if (int_error_hash != NULL)
        p = lh_ERR_STRING_DATA_retrieve(int_error_hash, d);
    CRYPTO_THREAD_unlock(err_string_lock);

    return p;
}

#ifndef OPENSSL_NO_ERR
# define NUM_SYS_STR_REASONS 127
# define LEN_SYS_STR_REASON 32

static ERR_STRING_DATA SYS_str_reasons[NUM_SYS_STR_REASONS + 1];
/*
 * SYS_str_reasons is filled with copies of strerror() results at
 * initialization. 'errno' values up to 127 should cover all usual errors,
 * others will be displayed numerically by ERR_error_string. It is crucial
 * that we have something for each reason code that occurs in
 * ERR_str_reasons, or bogus reason strings will be returned for SYSerr(),
 * which always gets an errno value and never one of those 'standard' reason
 * codes.
 */

static void build_SYS_str_reasons(void)
{
    /* OPENSSL_malloc cannot be used here, use static storage instead */
    static char strerror_tab[NUM_SYS_STR_REASONS][LEN_SYS_STR_REASON];
    static int init = 1;
    int i;

    CRYPTO_THREAD_write_lock(err_string_lock);
    if (!init) {
        CRYPTO_THREAD_unlock(err_string_lock);
        return;
    }

    for (i = 1; i <= NUM_SYS_STR_REASONS; i++) {
        ERR_STRING_DATA *str = &SYS_str_reasons[i - 1];

        str->error = (unsigned long)i;
        if (str->string == NULL) {
            char (*dest)[LEN_SYS_STR_REASON] = &(strerror_tab[i - 1]);
            if (openssl_strerror_r(i, *dest, sizeof(*dest)))
                str->string = *dest;
        }
        if (str->string == NULL)
            str->string = "unknown";
    }

    /*
     * Now we still have SYS_str_reasons[NUM_SYS_STR_REASONS] = {0, NULL}, as
     * required by ERR_load_strings.
     */

    init = 0;

    CRYPTO_THREAD_unlock(err_string_lock);
}
#endif

#define err_clear_data(p,i) \
        do { \
        if ((p)->err_data_flags[i] & ERR_TXT_MALLOCED) \
                {  \
                OPENSSL_free((p)->err_data[i]); \
                (p)->err_data[i]=NULL; \
                } \
        (p)->err_data_flags[i]=0; \
        } while(0)

#define err_clear(p,i) \
        do { \
        (p)->err_flags[i]=0; \
        (p)->err_buffer[i]=0; \
        err_clear_data(p,i); \
        (p)->err_file[i]=NULL; \
        (p)->err_line[i]= -1; \
        } while(0)

static void ERR_STATE_free(ERR_STATE *s)
{
    int i;

    if (s == NULL)
        return;

    for (i = 0; i < ERR_NUM_ERRORS; i++) {
        err_clear_data(s, i);
    }
    OPENSSL_free(s);
}

DEFINE_RUN_ONCE_STATIC(do_err_strings_init)
{
    OPENSSL_init_crypto(0, NULL);
    err_string_lock = CRYPTO_THREAD_lock_new();
    return err_string_lock != NULL;
}

void err_cleanup(void)
{
    if (set_err_thread_local != 0)
        CRYPTO_THREAD_cleanup_local(&err_thread_local);
    CRYPTO_THREAD_lock_free(err_string_lock);
    err_string_lock = NULL;
}

int ERR_load_ERR_strings(void)
{
#ifndef OPENSSL_NO_ERR
    if (!RUN_ONCE(&err_string_init, do_err_strings_init))
        return 0;

    err_load_strings(0, ERR_str_libraries);
    err_load_strings(0, ERR_str_reasons);
    err_load_strings(ERR_LIB_SYS, ERR_str_functs);
    build_SYS_str_reasons();
    err_load_strings(ERR_LIB_SYS, SYS_str_reasons);
#endif
    return 1;
}

static void err_load_strings(int lib, ERR_STRING_DATA *str)
{
    CRYPTO_THREAD_write_lock(err_string_lock);
    if (int_error_hash == NULL)
        int_error_hash = lh_ERR_STRING_DATA_new(err_string_data_hash,
                                                err_string_data_cmp);
    if (int_error_hash != NULL) {
        for (; str->error; str++) {
            if (lib)
                str->error |= ERR_PACK(lib, 0, 0);
            (void)lh_ERR_STRING_DATA_insert(int_error_hash, str);
        }
    }
    CRYPTO_THREAD_unlock(err_string_lock);
}

int ERR_load_strings(int lib, ERR_STRING_DATA *str)
{
    if (ERR_load_ERR_strings() == 0)
        return 0;
    err_load_strings(lib, str);
    return 1;
}

int ERR_unload_strings(int lib, ERR_STRING_DATA *str)
{
    if (!RUN_ONCE(&err_string_init, do_err_strings_init))
        return 0;

    CRYPTO_THREAD_write_lock(err_string_lock);
    if (int_error_hash != NULL) {
        for (; str->error; str++) {
            if (lib)
                str->error |= ERR_PACK(lib, 0, 0);
            (void)lh_ERR_STRING_DATA_delete(int_error_hash, str);
        }
    }
    CRYPTO_THREAD_unlock(err_string_lock);

    return 1;
}

void err_free_strings_int(void)
{
    if (!RUN_ONCE(&err_string_init, do_err_strings_init))
        return;

    CRYPTO_THREAD_write_lock(err_string_lock);
    lh_ERR_STRING_DATA_free(int_error_hash);
    int_error_hash = NULL;
    CRYPTO_THREAD_unlock(err_string_lock);
}

/********************************************************/

void ERR_put_error(int lib, int func, int reason, const char *file, int line)
{
    ERR_STATE *es;

#ifdef _OSD_POSIX
    /*
     * In the BS2000-OSD POSIX subsystem, the compiler generates path names
     * in the form "*POSIX(/etc/passwd)". This dirty hack strips them to
     * something sensible. @@@ We shouldn't modify a const string, though.
     */
    if (strncmp(file, "*POSIX(", sizeof("*POSIX(") - 1) == 0) {
        char *end;

        /* Skip the "*POSIX(" prefix */
        file += sizeof("*POSIX(") - 1;
        end = &file[strlen(file) - 1];
        if (*end == ')')
            *end = '\0';
        /* Optional: use the basename of the path only. */
        if ((end = strrchr(file, '/')) != NULL)
            file = &end[1];
    }
#endif
    es = ERR_get_state();
    if (es == NULL)
        return;

    es->top = (es->top + 1) % ERR_NUM_ERRORS;
    if (es->top == es->bottom)
        es->bottom = (es->bottom + 1) % ERR_NUM_ERRORS;
    es->err_flags[es->top] = 0;
    es->err_buffer[es->top] = ERR_PACK(lib, func, reason);
    es->err_file[es->top] = file;
    es->err_line[es->top] = line;
    err_clear_data(es, es->top);
}

void ERR_clear_error(void)
{
    int i;
    ERR_STATE *es;

    es = ERR_get_state();
    if (es == NULL)
        return;

    for (i = 0; i < ERR_NUM_ERRORS; i++) {
        err_clear(es, i);
    }
    es->top = es->bottom = 0;
}

unsigned long ERR_get_error(void)
{
    return (get_error_values(1, 0, NULL, NULL, NULL, NULL));
}

unsigned long ERR_get_error_line(const char **file, int *line)
{
    return (get_error_values(1, 0, file, line, NULL, NULL));
}

unsigned long ERR_get_error_line_data(const char **file, int *line,
                                      const char **data, int *flags)
{
    return (get_error_values(1, 0, file, line, data, flags));
}

unsigned long ERR_peek_error(void)
{
    return (get_error_values(0, 0, NULL, NULL, NULL, NULL));
}

unsigned long ERR_peek_error_line(const char **file, int *line)
{
    return (get_error_values(0, 0, file, line, NULL, NULL));
}

unsigned long ERR_peek_error_line_data(const char **file, int *line,
                                       const char **data, int *flags)
{
    return (get_error_values(0, 0, file, line, data, flags));
}

unsigned long ERR_peek_last_error(void)
{
    return (get_error_values(0, 1, NULL, NULL, NULL, NULL));
}

unsigned long ERR_peek_last_error_line(const char **file, int *line)
{
    return (get_error_values(0, 1, file, line, NULL, NULL));
}

unsigned long ERR_peek_last_error_line_data(const char **file, int *line,
                                            const char **data, int *flags)
{
    return (get_error_values(0, 1, file, line, data, flags));
}

static unsigned long get_error_values(int inc, int top, const char **file,
                                      int *line, const char **data,
                                      int *flags)
{
    int i = 0;
    ERR_STATE *es;
    unsigned long ret;

    es = ERR_get_state();
    if (es == NULL)
        return 0;

    if (inc && top) {
        if (file)
            *file = "";
        if (line)
            *line = 0;
        if (data)
            *data = "";
        if (flags)
            *flags = 0;

        return ERR_R_INTERNAL_ERROR;
    }

    if (es->bottom == es->top)
        return 0;
    if (top)
        i = es->top;            /* last error */
    else
        i = (es->bottom + 1) % ERR_NUM_ERRORS; /* first error */

    ret = es->err_buffer[i];
    if (inc) {
        es->bottom = i;
        es->err_buffer[i] = 0;
    }

    if ((file != NULL) && (line != NULL)) {
        if (es->err_file[i] == NULL) {
            *file = "NA";
            if (line != NULL)
                *line = 0;
        } else {
            *file = es->err_file[i];
            if (line != NULL)
                *line = es->err_line[i];
        }
    }

    if (data == NULL) {
        if (inc) {
            err_clear_data(es, i);
        }
    } else {
        if (es->err_data[i] == NULL) {
            *data = "";
            if (flags != NULL)
                *flags = 0;
        } else {
            *data = es->err_data[i];
            if (flags != NULL)
                *flags = es->err_data_flags[i];
        }
    }
    return ret;
}

void ERR_error_string_n(unsigned long e, char *buf, size_t len)
{
    char lsbuf[64], fsbuf[64], rsbuf[64];
    const char *ls, *fs, *rs;
    unsigned long l, f, r;

    if (len == 0)
        return;

    l = ERR_GET_LIB(e);
    f = ERR_GET_FUNC(e);
    r = ERR_GET_REASON(e);

    ls = ERR_lib_error_string(e);
    fs = ERR_func_error_string(e);
    rs = ERR_reason_error_string(e);

    if (ls == NULL)
        BIO_snprintf(lsbuf, sizeof(lsbuf), "lib(%lu)", l);
    if (fs == NULL)
        BIO_snprintf(fsbuf, sizeof(fsbuf), "func(%lu)", f);
    if (rs == NULL)
        BIO_snprintf(rsbuf, sizeof(rsbuf), "reason(%lu)", r);

    BIO_snprintf(buf, len, "error:%08lX:%s:%s:%s", e, ls ? ls : lsbuf,
                 fs ? fs : fsbuf, rs ? rs : rsbuf);
    if (strlen(buf) == len - 1) {
        /*
         * output may be truncated; make sure we always have 5
         * colon-separated fields, i.e. 4 colons ...
         */
#define NUM_COLONS 4
        if (len > NUM_COLONS) { /* ... if possible */
            int i;
            char *s = buf;

            for (i = 0; i < NUM_COLONS; i++) {
                char *colon = strchr(s, ':');
                if (colon == NULL || colon > &buf[len - 1] - NUM_COLONS + i) {
                    /*
                     * set colon no. i at last possible position (buf[len-1]
                     * is the terminating 0)
                     */
                    colon = &buf[len - 1] - NUM_COLONS + i;
                    *colon = ':';
                }
                s = colon + 1;
            }
        }
    }
}

/*
 * ERR_error_string_n should be used instead for ret != NULL as
 * ERR_error_string cannot know how large the buffer is
 */
char *ERR_error_string(unsigned long e, char *ret)
{
    static char buf[256];

    if (ret == NULL)
        ret = buf;
    ERR_error_string_n(e, ret, 256);

    return ret;
}

const char *ERR_lib_error_string(unsigned long e)
{
    ERR_STRING_DATA d, *p;
    unsigned long l;

    if (!RUN_ONCE(&err_string_init, do_err_strings_init)) {
        return NULL;
    }

    l = ERR_GET_LIB(e);
    d.error = ERR_PACK(l, 0, 0);
    p = int_err_get_item(&d);
    return ((p == NULL) ? NULL : p->string);
}

const char *ERR_func_error_string(unsigned long e)
{
    ERR_STRING_DATA d, *p;
    unsigned long l, f;

    if (!RUN_ONCE(&err_string_init, do_err_strings_init)) {
        return NULL;
    }

    l = ERR_GET_LIB(e);
    f = ERR_GET_FUNC(e);
    d.error = ERR_PACK(l, f, 0);
    p = int_err_get_item(&d);
    return ((p == NULL) ? NULL : p->string);
}

const char *ERR_reason_error_string(unsigned long e)
{
    ERR_STRING_DATA d, *p = NULL;
    unsigned long l, r;

    if (!RUN_ONCE(&err_string_init, do_err_strings_init)) {
        return NULL;
    }

    l = ERR_GET_LIB(e);
    r = ERR_GET_REASON(e);
    d.error = ERR_PACK(l, 0, r);
    p = int_err_get_item(&d);
    if (!p) {
        d.error = ERR_PACK(0, 0, r);
        p = int_err_get_item(&d);
    }
    return ((p == NULL) ? NULL : p->string);
}

void err_delete_thread_state(void)
{
    ERR_STATE *state = CRYPTO_THREAD_get_local(&err_thread_local);
    if (state == NULL)
        return;

    CRYPTO_THREAD_set_local(&err_thread_local, NULL);
    ERR_STATE_free(state);
}

#if OPENSSL_API_COMPAT < 0x10100000L
void ERR_remove_thread_state(void *dummy)
{
}
#endif

#if OPENSSL_API_COMPAT < 0x10000000L
void ERR_remove_state(unsigned long pid)
{
}
#endif

DEFINE_RUN_ONCE_STATIC(err_do_init)
{
    set_err_thread_local = 1;
    return CRYPTO_THREAD_init_local(&err_thread_local, NULL);
}

ERR_STATE *ERR_get_state(void)
{
    ERR_STATE *state = NULL;

    if (!RUN_ONCE(&err_init, err_do_init))
        return NULL;

    /*
     * If base OPENSSL_init_crypto() hasn't been called yet, be sure to call
     * it now to avoid state to be doubly allocated and thereby leak memory.
     * Needed on any platform that doesn't define OPENSSL_USE_NODELETE.
     */
    if (!OPENSSL_init_crypto(0, NULL))
        return NULL;

    state = CRYPTO_THREAD_get_local(&err_thread_local);

    if (state == NULL) {
        state = OPENSSL_zalloc(sizeof(*state));
        if (state == NULL)
            return NULL;

        if (!ossl_init_thread_start(OPENSSL_INIT_THREAD_ERR_STATE)
            || !CRYPTO_THREAD_set_local(&err_thread_local, state)) {
            ERR_STATE_free(state);
            return NULL;
        }

        /* Ignore failures from these */
        OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
    }

    return state;
}

int ERR_get_next_error_library(void)
{
    int ret;

    if (!RUN_ONCE(&err_string_init, do_err_strings_init)) {
        return 0;
    }

    CRYPTO_THREAD_write_lock(err_string_lock);
    ret = int_err_library_number++;
    CRYPTO_THREAD_unlock(err_string_lock);
    return ret;
}

void ERR_set_error_data(char *data, int flags)
{
    ERR_STATE *es;
    int i;

    es = ERR_get_state();
    if (es == NULL)
        return;

    i = es->top;

    err_clear_data(es, i);
    es->err_data[i] = data;
    es->err_data_flags[i] = flags;
}

void ERR_add_error_data(int num, ...)
{
    va_list args;
    va_start(args, num);
    ERR_add_error_vdata(num, args);
    va_end(args);
}

void ERR_add_error_vdata(int num, va_list args)
{
    int i, n, s;
    char *str, *p, *a;

    s = 80;
    str = OPENSSL_malloc(s + 1);
    if (str == NULL)
        return;
    str[0] = '\0';

    n = 0;
    for (i = 0; i < num; i++) {
        a = va_arg(args, char *);
        /* ignore NULLs, thanks to Bob Beck <beck@obtuse.com> */
        if (a != NULL) {
            n += strlen(a);
            if (n > s) {
                s = n + 20;
                p = OPENSSL_realloc(str, s + 1);
                if (p == NULL) {
                    OPENSSL_free(str);
                    return;
                }
                str = p;
            }
            OPENSSL_strlcat(str, a, (size_t)s + 1);
        }
    }
    ERR_set_error_data(str, ERR_TXT_MALLOCED | ERR_TXT_STRING);
}

int ERR_set_mark(void)
{
    ERR_STATE *es;

    es = ERR_get_state();
    if (es == NULL)
        return 0;

    if (es->bottom == es->top)
        return 0;
    es->err_flags[es->top] |= ERR_FLAG_MARK;
    return 1;
}

int ERR_pop_to_mark(void)
{
    ERR_STATE *es;

    es = ERR_get_state();
    if (es == NULL)
        return 0;

    while (es->bottom != es->top
           && (es->err_flags[es->top] & ERR_FLAG_MARK) == 0) {
        err_clear(es, es->top);
        es->top -= 1;
        if (es->top == -1)
            es->top = ERR_NUM_ERRORS - 1;
    }

    if (es->bottom == es->top)
        return 0;
    es->err_flags[es->top] &= ~ERR_FLAG_MARK;
    return 1;
}
