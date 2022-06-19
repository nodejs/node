/*
 * Copyright 1995-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_INTERNAL_CRYPTLIB_H
# define OSSL_INTERNAL_CRYPTLIB_H
# pragma once

# include <stdlib.h>
# include <string.h>

# ifdef OPENSSL_USE_APPLINK
#  define BIO_FLAGS_UPLINK_INTERNAL 0x8000
#  include "ms/uplink.h"
# else
#  define BIO_FLAGS_UPLINK_INTERNAL 0
# endif

# include <openssl/crypto.h>
# include <openssl/buffer.h>
# include <openssl/bio.h>
# include <openssl/asn1.h>
# include <openssl/err.h>
# include "internal/nelem.h"

#ifdef NDEBUG
# define ossl_assert(x) ((x) != 0)
#else
__owur static ossl_inline int ossl_assert_int(int expr, const char *exprstr,
                                              const char *file, int line)
{
    if (!expr)
        OPENSSL_die(exprstr, file, line);

    return expr;
}

# define ossl_assert(x) ossl_assert_int((x) != 0, "Assertion failed: "#x, \
                                         __FILE__, __LINE__)

#endif

/*
 * Use this inside a union with the field that needs to be aligned to a
 * reasonable boundary for the platform.  The most pessimistic alignment
 * of the listed types will be used by the compiler.
 */
# define OSSL_UNION_ALIGN       \
    double align;               \
    ossl_uintmax_t align_int;   \
    void *align_ptr

typedef struct ex_callback_st EX_CALLBACK;
DEFINE_STACK_OF(EX_CALLBACK)

typedef struct mem_st MEM;
DEFINE_LHASH_OF(MEM);

# define OPENSSL_CONF             "openssl.cnf"

# ifndef OPENSSL_SYS_VMS
#  define X509_CERT_AREA          OPENSSLDIR
#  define X509_CERT_DIR           OPENSSLDIR "/certs"
#  define X509_CERT_FILE          OPENSSLDIR "/cert.pem"
#  define X509_PRIVATE_DIR        OPENSSLDIR "/private"
#  define CTLOG_FILE              OPENSSLDIR "/ct_log_list.cnf"
# else
#  define X509_CERT_AREA          "OSSL$DATAROOT:[000000]"
#  define X509_CERT_DIR           "OSSL$DATAROOT:[CERTS]"
#  define X509_CERT_FILE          "OSSL$DATAROOT:[000000]cert.pem"
#  define X509_PRIVATE_DIR        "OSSL$DATAROOT:[PRIVATE]"
#  define CTLOG_FILE              "OSSL$DATAROOT:[000000]ct_log_list.cnf"
# endif

# define X509_CERT_DIR_EVP        "SSL_CERT_DIR"
# define X509_CERT_FILE_EVP       "SSL_CERT_FILE"
# define CTLOG_FILE_EVP           "CTLOG_FILE"

/* size of string representations */
# define DECIMAL_SIZE(type)      ((sizeof(type)*8+2)/3+1)
# define HEX_SIZE(type)          (sizeof(type)*2)

void OPENSSL_cpuid_setup(void);
#if defined(__i386)   || defined(__i386__)   || defined(_M_IX86) || \
    defined(__x86_64) || defined(__x86_64__) || \
    defined(_M_AMD64) || defined(_M_X64)
extern unsigned int OPENSSL_ia32cap_P[];
#endif
void OPENSSL_showfatal(const char *fmta, ...);
int ossl_do_ex_data_init(OSSL_LIB_CTX *ctx);
void ossl_crypto_cleanup_all_ex_data_int(OSSL_LIB_CTX *ctx);
int openssl_init_fork_handlers(void);
int openssl_get_fork_id(void);

char *ossl_safe_getenv(const char *name);

extern CRYPTO_RWLOCK *memdbg_lock;
int openssl_strerror_r(int errnum, char *buf, size_t buflen);
# if !defined(OPENSSL_NO_STDIO)
FILE *openssl_fopen(const char *filename, const char *mode);
# else
void *openssl_fopen(const char *filename, const char *mode);
# endif

uint32_t OPENSSL_rdtsc(void);
size_t OPENSSL_instrument_bus(unsigned int *, size_t);
size_t OPENSSL_instrument_bus2(unsigned int *, size_t, size_t);

/* ex_data structures */

/*
 * Each structure type (sometimes called a class), that supports
 * exdata has a stack of callbacks for each instance.
 */
struct ex_callback_st {
    long argl;                  /* Arbitrary long */
    void *argp;                 /* Arbitrary void * */
    int priority;               /* Priority ordering for freeing */
    CRYPTO_EX_new *new_func;
    CRYPTO_EX_free *free_func;
    CRYPTO_EX_dup *dup_func;
};

/*
 * The state for each class.  This could just be a typedef, but
 * a structure allows future changes.
 */
typedef struct ex_callbacks_st {
    STACK_OF(EX_CALLBACK) *meth;
} EX_CALLBACKS;

typedef struct ossl_ex_data_global_st {
    CRYPTO_RWLOCK *ex_data_lock;
    EX_CALLBACKS ex_data[CRYPTO_EX_INDEX__COUNT];
} OSSL_EX_DATA_GLOBAL;


/* OSSL_LIB_CTX */

# define OSSL_LIB_CTX_PROVIDER_STORE_RUN_ONCE_INDEX          0
# define OSSL_LIB_CTX_DEFAULT_METHOD_STORE_RUN_ONCE_INDEX    1
# define OSSL_LIB_CTX_METHOD_STORE_RUN_ONCE_INDEX            2
# define OSSL_LIB_CTX_MAX_RUN_ONCE                           3

# define OSSL_LIB_CTX_EVP_METHOD_STORE_INDEX         0
# define OSSL_LIB_CTX_PROVIDER_STORE_INDEX           1
# define OSSL_LIB_CTX_PROPERTY_DEFN_INDEX            2
# define OSSL_LIB_CTX_PROPERTY_STRING_INDEX          3
# define OSSL_LIB_CTX_NAMEMAP_INDEX                  4
# define OSSL_LIB_CTX_DRBG_INDEX                     5
# define OSSL_LIB_CTX_DRBG_NONCE_INDEX               6
# define OSSL_LIB_CTX_RAND_CRNGT_INDEX               7
# ifdef FIPS_MODULE
#  define OSSL_LIB_CTX_THREAD_EVENT_HANDLER_INDEX    8
# endif
# define OSSL_LIB_CTX_FIPS_PROV_INDEX                9
# define OSSL_LIB_CTX_ENCODER_STORE_INDEX           10
# define OSSL_LIB_CTX_DECODER_STORE_INDEX           11
# define OSSL_LIB_CTX_SELF_TEST_CB_INDEX            12
# define OSSL_LIB_CTX_BIO_PROV_INDEX                13
# define OSSL_LIB_CTX_GLOBAL_PROPERTIES             14
# define OSSL_LIB_CTX_STORE_LOADER_STORE_INDEX      15
# define OSSL_LIB_CTX_PROVIDER_CONF_INDEX           16
# define OSSL_LIB_CTX_BIO_CORE_INDEX                17
# define OSSL_LIB_CTX_CHILD_PROVIDER_INDEX          18
# define OSSL_LIB_CTX_MAX_INDEXES                   19

# define OSSL_LIB_CTX_METHOD_LOW_PRIORITY          -1
# define OSSL_LIB_CTX_METHOD_DEFAULT_PRIORITY       0
# define OSSL_LIB_CTX_METHOD_PRIORITY_1             1
# define OSSL_LIB_CTX_METHOD_PRIORITY_2             2

typedef struct ossl_lib_ctx_method {
    int priority;
    void *(*new_func)(OSSL_LIB_CTX *ctx);
    void (*free_func)(void *);
} OSSL_LIB_CTX_METHOD;

OSSL_LIB_CTX *ossl_lib_ctx_get_concrete(OSSL_LIB_CTX *ctx);
int ossl_lib_ctx_is_default(OSSL_LIB_CTX *ctx);
int ossl_lib_ctx_is_global_default(OSSL_LIB_CTX *ctx);

/* Functions to retrieve pointers to data by index */
void *ossl_lib_ctx_get_data(OSSL_LIB_CTX *, int /* index */,
                            const OSSL_LIB_CTX_METHOD * ctx);

void ossl_lib_ctx_default_deinit(void);
OSSL_EX_DATA_GLOBAL *ossl_lib_ctx_get_ex_data_global(OSSL_LIB_CTX *ctx);
typedef int (ossl_lib_ctx_run_once_fn)(OSSL_LIB_CTX *ctx);
typedef void (ossl_lib_ctx_onfree_fn)(OSSL_LIB_CTX *ctx);

int ossl_lib_ctx_run_once(OSSL_LIB_CTX *ctx, unsigned int idx,
                          ossl_lib_ctx_run_once_fn run_once_fn);
int ossl_lib_ctx_onfree(OSSL_LIB_CTX *ctx, ossl_lib_ctx_onfree_fn onfreefn);
const char *ossl_lib_ctx_get_descriptor(OSSL_LIB_CTX *libctx);

OSSL_LIB_CTX *ossl_crypto_ex_data_get_ossl_lib_ctx(const CRYPTO_EX_DATA *ad);
int ossl_crypto_new_ex_data_ex(OSSL_LIB_CTX *ctx, int class_index, void *obj,
                               CRYPTO_EX_DATA *ad);
int ossl_crypto_get_ex_new_index_ex(OSSL_LIB_CTX *ctx, int class_index,
                                    long argl, void *argp,
                                    CRYPTO_EX_new *new_func,
                                    CRYPTO_EX_dup *dup_func,
                                    CRYPTO_EX_free *free_func,
                                    int priority);
int ossl_crypto_free_ex_index_ex(OSSL_LIB_CTX *ctx, int class_index, int idx);

/* Function for simple binary search */

/* Flags */
# define OSSL_BSEARCH_VALUE_ON_NOMATCH            0x01
# define OSSL_BSEARCH_FIRST_VALUE_ON_MATCH        0x02

const void *ossl_bsearch(const void *key, const void *base, int num,
                         int size, int (*cmp) (const void *, const void *),
                         int flags);

char *ossl_sk_ASN1_UTF8STRING2text(STACK_OF(ASN1_UTF8STRING) *text,
                                   const char *sep, size_t max_len);
char *ossl_ipaddr_to_asc(unsigned char *p, int len);

char *ossl_buf2hexstr_sep(const unsigned char *buf, long buflen, char sep);
unsigned char *ossl_hexstr2buf_sep(const char *str, long *buflen,
                                   const char sep);

static ossl_inline int ossl_ends_with_dirsep(const char *path)
{
    if (*path != '\0')
        path += strlen(path) - 1;
# if defined __VMS
    if (*path == ']' || *path == '>' || *path == ':')
        return 1;
# elif defined _WIN32
    if (*path == '\\')
        return 1;
# endif
    return *path == '/';
}

static ossl_inline int ossl_is_absolute_path(const char *path)
{
# if defined __VMS
    if (strchr(path, ':') != NULL
        || ((path[0] == '[' || path[0] == '<')
            && path[1] != '.' && path[1] != '-'
            && path[1] != ']' && path[1] != '>'))
        return 1;
# elif defined _WIN32
    if (path[0] == '\\'
        || (path[0] != '\0' && path[1] == ':'))
        return 1;
# endif
    return path[0] == '/';
}

#endif
