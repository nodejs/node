/*
 * Copyright 2018-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "../testutil.h"
#include <openssl/provider.h>
#include <string.h>

int test_get_libctx(OSSL_LIB_CTX **libctx, OSSL_PROVIDER **default_null_prov,
                    const char *config_file,
                    OSSL_PROVIDER **provider, const char *module_name)
{
    if ((*libctx = OSSL_LIB_CTX_new()) == NULL) {
        opt_printf_stderr("Failed to create libctx\n");
        goto err;
    }

    if (default_null_prov != NULL
        && (*default_null_prov = OSSL_PROVIDER_load(NULL, "null")) == NULL) {
        opt_printf_stderr("Failed to load null provider into default libctx\n");
        goto err;
    }

    if (config_file != NULL
            && !OSSL_LIB_CTX_load_config(*libctx, config_file)) {
        opt_printf_stderr("Error loading config from file %s\n", config_file);
        goto err;
    }

    if (module_name != NULL
            && (*provider = OSSL_PROVIDER_load(*libctx, module_name)) == NULL) {
        opt_printf_stderr("Failed to load provider %s\n", module_name);
        goto err;
    }
    return 1;

 err:
    ERR_print_errors_fp(stderr);
    return 0;
}

int test_arg_libctx(OSSL_LIB_CTX **libctx, OSSL_PROVIDER **default_null_prov,
                    OSSL_PROVIDER **provider, int argn, const char *usage)
{
    const char *module_name;

    if (!TEST_ptr(module_name = test_get_argument(argn))) {
        TEST_error("usage: <prog> %s", usage);
        return 0;
    }
    if (strcmp(module_name, "none") == 0)
        return 1;
    return test_get_libctx(libctx, default_null_prov,
                           test_get_argument(argn + 1), provider, module_name);
}
