/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/opensslv.h>

/* The test is only currently implemented for DSO_DLFCN and DSO_WIN32 */
#if defined(DSO_DLFCN) || defined(DSO_WIN32)

#define SSL_CTX_NEW "SSL_CTX_new"
#define SSL_CTX_FREE "SSL_CTX_free"
#define TLS_METHOD "TLS_method"

#define ERR_GET_ERROR "ERR_get_error"
#define OPENSSL_VERSION_NUM_FUNC "OpenSSL_version_num"

typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_method_st SSL_METHOD;
typedef const SSL_METHOD * (*TLS_method_t)(void);
typedef SSL_CTX * (*SSL_CTX_new_t)(const SSL_METHOD *meth);
typedef void (*SSL_CTX_free_t)(SSL_CTX *);

typedef unsigned long (*ERR_get_error_t)(void);
typedef unsigned long (*OpenSSL_version_num_t)(void);

static TLS_method_t TLS_method;
static SSL_CTX_new_t SSL_CTX_new;
static SSL_CTX_free_t SSL_CTX_free;

static ERR_get_error_t ERR_get_error;
static OpenSSL_version_num_t OpenSSL_version_num;

#ifdef DSO_DLFCN

# include <dlfcn.h>

typedef void * SHLIB;
typedef void * SHLIB_SYM;
# define SHLIB_INIT NULL

static int shlib_load(const char *filename, SHLIB *lib)
{
    *lib = dlopen(filename, RTLD_GLOBAL | RTLD_LAZY);

    if (*lib == NULL)
        return 0;

    return 1;
}

static int shlib_sym(SHLIB lib, const char *symname, SHLIB_SYM *sym)
{
    *sym = dlsym(lib, symname);

    return *sym != NULL;
}

static int shlib_close(SHLIB lib)
{
    if (dlclose(lib) != 0)
        return 0;

    return 1;
}

#elif defined(DSO_WIN32)

# include <windows.h>

typedef HINSTANCE SHLIB;
typedef void * SHLIB_SYM;
# define SHLIB_INIT 0

static int shlib_load(const char *filename, SHLIB *lib)
{
    *lib = LoadLibraryA(filename);
    if (*lib == NULL)
        return 0;

    return 1;
}

static int shlib_sym(SHLIB lib, const char *symname, SHLIB_SYM *sym)
{
    *sym = (SHLIB_SYM)GetProcAddress(lib, symname);

    return *sym != NULL;
}

static int shlib_close(SHLIB lib)
{
    if (FreeLibrary(lib) == 0)
        return 0;

    return 1;
}

#endif

# define CRYPTO_FIRST_OPT    "-crypto_first"
# define SSL_FIRST_OPT       "-ssl_first"
# define JUST_CRYPTO_OPT     "-just_crypto"

enum test_types_en {
    CRYPTO_FIRST,
    SSL_FIRST,
    JUST_CRYPTO
};

int main(int argc, char **argv)
{
    SHLIB ssllib = SHLIB_INIT, cryptolib = SHLIB_INIT;
    SSL_CTX *ctx;
    union {
        void (*func) (void);
        SHLIB_SYM sym;
    } tls_method_sym, ssl_ctx_new_sym, ssl_ctx_free_sym, err_get_error_sym,
    openssl_version_num_sym;
    enum test_types_en test_type;
    int i;

    if (argc != 4) {
        printf("Unexpected number of arguments\n");
        return 1;
    }

    if (strcmp(argv[1], CRYPTO_FIRST_OPT) == 0) {
        test_type = CRYPTO_FIRST;
    } else if (strcmp(argv[1], SSL_FIRST_OPT) == 0) {
            test_type = SSL_FIRST;
    } else if (strcmp(argv[1], JUST_CRYPTO_OPT) == 0) {
            test_type = JUST_CRYPTO;
    } else {
        printf("Unrecognised argument\n");
        return 1;
    }

    for (i = 0; i < 2; i++) {
        if ((i == 0 && (test_type == CRYPTO_FIRST
                       || test_type == JUST_CRYPTO))
               || (i == 1 && test_type == SSL_FIRST)) {
            if (!shlib_load(argv[2], &cryptolib)) {
                printf("Unable to load libcrypto\n");
                return 1;
            }
        }
        if ((i == 0 && test_type == SSL_FIRST)
                || (i == 1 && test_type == CRYPTO_FIRST)) {
            if (!shlib_load(argv[3], &ssllib)) {
                printf("Unable to load libssl\n");
                return 1;
            }
        }
    }

    if (test_type != JUST_CRYPTO) {
        if (!shlib_sym(ssllib, TLS_METHOD, &tls_method_sym.sym)
                || !shlib_sym(ssllib, SSL_CTX_NEW, &ssl_ctx_new_sym.sym)
                || !shlib_sym(ssllib, SSL_CTX_FREE, &ssl_ctx_free_sym.sym)) {
            printf("Unable to load ssl symbols\n");
            return 1;
        }

        TLS_method = (TLS_method_t)tls_method_sym.func;
        SSL_CTX_new = (SSL_CTX_new_t)ssl_ctx_new_sym.func;
        SSL_CTX_free = (SSL_CTX_free_t)ssl_ctx_free_sym.func;

        ctx = SSL_CTX_new(TLS_method());
        if (ctx == NULL) {
            printf("Unable to create SSL_CTX\n");
            return 1;
        }
        SSL_CTX_free(ctx);
    }

    if (!shlib_sym(cryptolib, ERR_GET_ERROR, &err_get_error_sym.sym)
            || !shlib_sym(cryptolib, OPENSSL_VERSION_NUM_FUNC,
                          &openssl_version_num_sym.sym)) {
        printf("Unable to load crypto symbols\n");
        return 1;
    }

    ERR_get_error = (ERR_get_error_t)err_get_error_sym.func;
    OpenSSL_version_num = (OpenSSL_version_num_t)openssl_version_num_sym.func;

    if (ERR_get_error() != 0) {
        printf("Unexpected error in error queue\n");
        return 1;
    }

    if (OpenSSL_version_num() != OPENSSL_VERSION_NUMBER) {
        printf("Unexpected library version loaded\n");
        return 1;
    }

    for (i = 0; i < 2; i++) {
        if ((i == 0 && test_type == CRYPTO_FIRST)
                || (i == 1 && test_type == SSL_FIRST)) {
            if (!shlib_close(ssllib)) {
                printf("Unable to close libssl\n");
                return 1;
            }
        }
        if ((i == 0 && (test_type == SSL_FIRST
                       || test_type == JUST_CRYPTO))
                || (i == 1 && test_type == CRYPTO_FIRST)) {
            if (!shlib_close(cryptolib)) {
                printf("Unable to close libcrypto\n");
                return 1;
            }
        }
    }

    printf("Success\n");
    return 0;
}
#else
int main(void)
{
    printf("Test not implemented on this platform\n");
    return 0;
}
#endif
