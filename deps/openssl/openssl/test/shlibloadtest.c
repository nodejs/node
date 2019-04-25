/*
 * Copyright 2016-2019 The OpenSSL Project Authors. All Rights Reserved.
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
#include <openssl/ssl.h>
#include <openssl/ossl_typ.h>
#include "internal/dso_conf.h"

typedef void DSO;

typedef const SSL_METHOD * (*TLS_method_t)(void);
typedef SSL_CTX * (*SSL_CTX_new_t)(const SSL_METHOD *meth);
typedef void (*SSL_CTX_free_t)(SSL_CTX *);
typedef int (*OPENSSL_init_crypto_t)(uint64_t, void *);
typedef int (*OPENSSL_atexit_t)(void (*handler)(void));
typedef unsigned long (*ERR_get_error_t)(void);
typedef unsigned long (*OpenSSL_version_num_t)(void);
typedef DSO * (*DSO_dsobyaddr_t)(void (*addr)(void), int flags);
typedef int (*DSO_free_t)(DSO *dso);

typedef enum test_types_en {
    CRYPTO_FIRST,
    SSL_FIRST,
    JUST_CRYPTO,
    DSO_REFTEST,
    NO_ATEXIT
} TEST_TYPE;

static TEST_TYPE test_type;
static const char *path_crypto;
static const char *path_ssl;
static const char *path_atexit;

#ifdef DSO_DLFCN

# include <dlfcn.h>

# define SHLIB_INIT NULL

typedef void *SHLIB;
typedef void *SHLIB_SYM;

static int shlib_load(const char *filename, SHLIB *lib)
{
    int dl_flags = (RTLD_GLOBAL|RTLD_LAZY);
#ifdef _AIX
    if (filename[strlen(filename) - 1] == ')')
        dl_flags |= RTLD_MEMBER;
#endif
    *lib = dlopen(filename, dl_flags);
    return *lib == NULL ? 0 : 1;
}

static int shlib_sym(SHLIB lib, const char *symname, SHLIB_SYM *sym)
{
    *sym = dlsym(lib, symname);
    return *sym != NULL;
}

static int shlib_close(SHLIB lib)
{
    return dlclose(lib) != 0 ? 0 : 1;
}
#endif

#ifdef DSO_WIN32

# include <windows.h>

# define SHLIB_INIT 0

typedef HINSTANCE SHLIB;
typedef void *SHLIB_SYM;

static int shlib_load(const char *filename, SHLIB *lib)
{
    *lib = LoadLibraryA(filename);
    return *lib == NULL ? 0 : 1;
}

static int shlib_sym(SHLIB lib, const char *symname, SHLIB_SYM *sym)
{
    *sym = (SHLIB_SYM)GetProcAddress(lib, symname);
    return *sym != NULL;
}

static int shlib_close(SHLIB lib)
{
    return FreeLibrary(lib) == 0 ? 0 : 1;
}
#endif


#if defined(DSO_DLFCN) || defined(DSO_WIN32)

static int atexit_handler_done = 0;

static void atexit_handler(void)
{
    FILE *atexit_file = fopen(path_atexit, "w");

    if (atexit_file == NULL)
        return;

    fprintf(atexit_file, "atexit() run\n");
    fclose(atexit_file);
    atexit_handler_done++;
}

static int test_lib(void)
{
    SHLIB ssllib = SHLIB_INIT;
    SHLIB cryptolib = SHLIB_INIT;
    SSL_CTX *ctx;
    union {
        void (*func)(void);
        SHLIB_SYM sym;
    } symbols[3];
    TLS_method_t myTLS_method;
    SSL_CTX_new_t mySSL_CTX_new;
    SSL_CTX_free_t mySSL_CTX_free;
    ERR_get_error_t myERR_get_error;
    OpenSSL_version_num_t myOpenSSL_version_num;
    OPENSSL_atexit_t myOPENSSL_atexit;
    int result = 0;

    switch (test_type) {
    case JUST_CRYPTO:
    case DSO_REFTEST:
    case NO_ATEXIT:
    case CRYPTO_FIRST:
        if (!shlib_load(path_crypto, &cryptolib)) {
            fprintf(stderr, "Failed to load libcrypto\n");
            goto end;
        }
        if (test_type != CRYPTO_FIRST)
            break;
        /* Fall through */

    case SSL_FIRST:
        if (!shlib_load(path_ssl, &ssllib)) {
            fprintf(stderr, "Failed to load libssl\n");
            goto end;
        }
        if (test_type != SSL_FIRST)
            break;
        if (!shlib_load(path_crypto, &cryptolib)) {
            fprintf(stderr, "Failed to load libcrypto\n");
            goto end;
        }
        break;
    }

    if (test_type == NO_ATEXIT) {
        OPENSSL_init_crypto_t myOPENSSL_init_crypto;

        if (!shlib_sym(cryptolib, "OPENSSL_init_crypto", &symbols[0].sym)) {
            fprintf(stderr, "Failed to load OPENSSL_init_crypto symbol\n");
            goto end;
        }
        myOPENSSL_init_crypto = (OPENSSL_init_crypto_t)symbols[0].func;
        if (!myOPENSSL_init_crypto(OPENSSL_INIT_NO_ATEXIT, NULL)) {
            fprintf(stderr, "Failed to initialise libcrypto\n");
            goto end;
        }
    }

    if (test_type != JUST_CRYPTO
            && test_type != DSO_REFTEST
            && test_type != NO_ATEXIT) {
        if (!shlib_sym(ssllib, "TLS_method", &symbols[0].sym)
                || !shlib_sym(ssllib, "SSL_CTX_new", &symbols[1].sym)
                || !shlib_sym(ssllib, "SSL_CTX_free", &symbols[2].sym)) {
            fprintf(stderr, "Failed to load libssl symbols\n");
            goto end;
        }
        myTLS_method = (TLS_method_t)symbols[0].func;
        mySSL_CTX_new = (SSL_CTX_new_t)symbols[1].func;
        mySSL_CTX_free = (SSL_CTX_free_t)symbols[2].func;
        ctx = mySSL_CTX_new(myTLS_method());
        if (ctx == NULL) {
            fprintf(stderr, "Failed to create SSL_CTX\n");
            goto end;
        }
        mySSL_CTX_free(ctx);
    }

    if (!shlib_sym(cryptolib, "ERR_get_error", &symbols[0].sym)
           || !shlib_sym(cryptolib, "OpenSSL_version_num", &symbols[1].sym)
           || !shlib_sym(cryptolib, "OPENSSL_atexit", &symbols[2].sym)) {
        fprintf(stderr, "Failed to load libcrypto symbols\n");
        goto end;
    }
    myERR_get_error = (ERR_get_error_t)symbols[0].func;
    if (myERR_get_error() != 0) {
        fprintf(stderr, "Unexpected ERR_get_error() response\n");
        goto end;
    }

    myOpenSSL_version_num = (OpenSSL_version_num_t)symbols[1].func;
    if (myOpenSSL_version_num()  != OPENSSL_VERSION_NUMBER) {
        fprintf(stderr, "Invalid library version number\n");
        goto end;
    }

    myOPENSSL_atexit = (OPENSSL_atexit_t)symbols[2].func;
    if (!myOPENSSL_atexit(atexit_handler)) {
        fprintf(stderr, "Failed to register atexit handler\n");
        goto end;
    }

    if (test_type == DSO_REFTEST) {
# ifdef DSO_DLFCN
        DSO_dsobyaddr_t myDSO_dsobyaddr;
        DSO_free_t myDSO_free;

        /*
         * This is resembling the code used in ossl_init_base() and
         * OPENSSL_atexit() to block unloading the library after dlclose().
         * We are not testing this on Windows, because it is done there in a
         * completely different way. Especially as a call to DSO_dsobyaddr()
         * will always return an error, because DSO_pathbyaddr() is not
         * implemented there.
         */
        if (!shlib_sym(cryptolib, "DSO_dsobyaddr", &symbols[0].sym)
                || !shlib_sym(cryptolib, "DSO_free", &symbols[1].sym)) {
            fprintf(stderr, "Unable to load DSO symbols\n");
            goto end;
        }

        myDSO_dsobyaddr = (DSO_dsobyaddr_t)symbols[0].func;
        myDSO_free = (DSO_free_t)symbols[1].func;

        {
            DSO *hndl;
            /* use known symbol from crypto module */
            hndl = myDSO_dsobyaddr((void (*)(void))myERR_get_error, 0);
            if (hndl == NULL) {
                fprintf(stderr, "DSO_dsobyaddr() failed\n");
                goto end;
            }
            myDSO_free(hndl);
        }
# endif /* DSO_DLFCN */
    }

    if (!shlib_close(cryptolib)) {
        fprintf(stderr, "Failed to close libcrypto\n");
        goto end;
    }

    if (test_type == CRYPTO_FIRST || test_type == SSL_FIRST) {
        if (!shlib_close(ssllib)) {
            fprintf(stderr, "Failed to close libssl\n");
            goto end;
        }
    }

# if defined(OPENSSL_NO_PINSHARED) \
    && defined(__GLIBC__) \
    && defined(__GLIBC_PREREQ) \
    && defined(OPENSSL_SYS_LINUX)
#  if __GLIBC_PREREQ(2, 3)
    /*
     * If we didn't pin the so then we are hopefully on a platform that supports
     * running atexit() on so unload. If not we might crash. We know this is
     * true on linux since glibc 2.2.3
     */
    if (test_type != NO_ATEXIT && atexit_handler_done != 1) {
        fprintf(stderr, "atexit() handler did not run\n");
        goto end;
    }
#  endif
# endif

    result = 1;
end:
    return result;
}
#endif


/*
 * shlibloadtest should not use the normal test framework because we don't want
 * it to link against libcrypto (which the framework uses). The point of the
 * test is to check dynamic loading and unloading of libcrypto/libssl.
 */
int main(int argc, char *argv[])
{
    const char *p;

    if (argc != 5) {
        fprintf(stderr, "Incorrect number of arguments\n");
        return 1;
    }

    p = argv[1];

    if (strcmp(p, "-crypto_first") == 0) {
        test_type = CRYPTO_FIRST;
    } else if (strcmp(p, "-ssl_first") == 0) {
        test_type = SSL_FIRST;
    } else if (strcmp(p, "-just_crypto") == 0) {
        test_type = JUST_CRYPTO;
    } else if (strcmp(p, "-dso_ref") == 0) {
        test_type = DSO_REFTEST;
    } else if (strcmp(p, "-no_atexit") == 0) {
        test_type = NO_ATEXIT;
    } else {
        fprintf(stderr, "Unrecognised argument\n");
        return 1;
    }
    path_crypto = argv[2];
    path_ssl = argv[3];
    path_atexit = argv[4];
    if (path_crypto == NULL || path_ssl == NULL) {
        fprintf(stderr, "Invalid libcrypto/libssl path\n");
        return 1;
    }

#if defined(DSO_DLFCN) || defined(DSO_WIN32)
    if (!test_lib())
        return 1;
#endif
    return 0;
}
