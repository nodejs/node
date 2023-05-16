/*
 * Copyright 2020-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "apps.h"
#include <string.h>
#include <openssl/err.h>
#include <openssl/provider.h>
#include <openssl/safestack.h>

/* Non-zero if any of the provider options have been seen */
static int provider_option_given = 0;

DEFINE_STACK_OF(OSSL_PROVIDER)

/*
 * See comments in opt_verify for explanation of this.
 */
enum prov_range { OPT_PROV_ENUM };

static STACK_OF(OSSL_PROVIDER) *app_providers = NULL;

static void provider_free(OSSL_PROVIDER *prov)
{
    OSSL_PROVIDER_unload(prov);
}

int app_provider_load(OSSL_LIB_CTX *libctx, const char *provider_name)
{
    OSSL_PROVIDER *prov;

    prov = OSSL_PROVIDER_load(libctx, provider_name);
    if (prov == NULL) {
        opt_printf_stderr("%s: unable to load provider %s\n"
                          "Hint: use -provider-path option or OPENSSL_MODULES environment variable.\n",
                          opt_getprog(), provider_name);
        ERR_print_errors(bio_err);
        return 0;
    }
    if (app_providers == NULL)
        app_providers = sk_OSSL_PROVIDER_new_null();
    if (app_providers == NULL
        || !sk_OSSL_PROVIDER_push(app_providers, prov)) {
        app_providers_cleanup();
        return 0;
    }
    return 1;
}

void app_providers_cleanup(void)
{
    sk_OSSL_PROVIDER_pop_free(app_providers, provider_free);
    app_providers = NULL;
}

static int opt_provider_path(const char *path)
{
    if (path != NULL && *path == '\0')
        path = NULL;
    return OSSL_PROVIDER_set_default_search_path(app_get0_libctx(), path);
}

int opt_provider(int opt)
{
    const int given = provider_option_given;

    provider_option_given = 1;
    switch ((enum prov_range)opt) {
    case OPT_PROV__FIRST:
    case OPT_PROV__LAST:
        return 1;
    case OPT_PROV_PROVIDER:
        return app_provider_load(app_get0_libctx(), opt_arg());
    case OPT_PROV_PROVIDER_PATH:
        return opt_provider_path(opt_arg());
    case OPT_PROV_PROPQUERY:
        return app_set_propq(opt_arg());
    }
    /* Should never get here but if we do, undo what we did earlier */
    provider_option_given = given;
    return 0;
}

int opt_provider_option_given(void)
{
    return provider_option_given;
}
