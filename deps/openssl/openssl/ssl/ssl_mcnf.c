/*
 * Copyright 2015-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/conf.h>
#include <openssl/ssl.h>
#include <openssl/trace.h>
#include "ssl_local.h"
#include "internal/sslconf.h"
#include "internal/cryptlib.h"

/* SSL library configuration module. */

void SSL_add_ssl_module(void)
{
    /* Do nothing. This will be added automatically by libcrypto */
}

static CONF_IMODULE *ssl_do_lookup_module(OSSL_LIB_CTX *libctx)
{
    CONF_IMODULE *m = OSSL_LIB_CTX_get_data(libctx, OSSL_LIB_CTX_SSL_CONF_IMODULE);

    if (m != NULL)
        return m;

    libctx = OSSL_LIB_CTX_get0_global_default();
    return OSSL_LIB_CTX_get_data(libctx, OSSL_LIB_CTX_SSL_CONF_IMODULE);
}

static int ssl_do_config(SSL *s, SSL_CTX *ctx, const char *name, int system)
{
    SSL_CONF_CTX *cctx = NULL;
    size_t i, idx, cmd_count;
    int err = 1;
    unsigned int flags;
    unsigned int conf_diagnostics = 0;
    const SSL_METHOD *meth;
    const SSL_CONF_CMD *cmds;
    OSSL_LIB_CTX *libctx = NULL, *prev_libctx = NULL;
    CONF_IMODULE *imod = NULL;

    ERR_set_mark();

    if (s == NULL && ctx == NULL) {
        ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
        goto err;
    }
    if (name == NULL && system)
        name = "system_default";

    if (name == NULL) {
        ERR_raise_data(ERR_LIB_SSL, SSL_R_INVALID_CONFIGURATION_NAME,
            "name not specified (name == NULL)");
        goto err;
    }

    libctx = s != NULL ? s->ctx->libctx : ctx->libctx;
    imod = ssl_do_lookup_module(libctx);
    if (!conf_ssl_name_find(imod, name, &idx)) {
        if (!system)
            ERR_raise_data(ERR_LIB_SSL, SSL_R_INVALID_CONFIGURATION_NAME,
                "name=%s", name);
        goto err;
    }

    cmds = conf_ssl_get(imod, idx, &name, &cmd_count);
    flags = SSL_CONF_FLAG_FILE;
    if (!system)
        flags |= SSL_CONF_FLAG_CERTIFICATE | SSL_CONF_FLAG_REQUIRE_PRIVATE;

    cctx = SSL_CONF_CTX_new();
    if (cctx == NULL) {
        /* this is a fatal error, always report */
        system = 0;
        goto err;
    }

    if (s != NULL) {
        meth = s->method;
        SSL_CONF_CTX_set_ssl(cctx, s);
        libctx = s->ctx->libctx;
    } else {
        meth = ctx->method;
        SSL_CONF_CTX_set_ssl_ctx(cctx, ctx);
        libctx = ctx->libctx;
    }

    conf_diagnostics = OSSL_LIB_CTX_get_conf_diagnostics(libctx);
    if (conf_diagnostics)
        flags |= SSL_CONF_FLAG_SHOW_ERRORS;
    if (meth->ssl_accept != ssl_undefined_function)
        flags |= SSL_CONF_FLAG_SERVER;
    if (meth->ssl_connect != ssl_undefined_function)
        flags |= SSL_CONF_FLAG_CLIENT;
    SSL_CONF_CTX_set_flags(cctx, flags);
    prev_libctx = OSSL_LIB_CTX_set0_default(libctx);
    err = 0;
    for (i = 0; i < cmd_count; i++) {
        char *cmdstr, *arg;
        int rv;

        conf_ssl_get_cmd(cmds, i, &cmdstr, &arg);
        rv = SSL_CONF_cmd(cctx, cmdstr, arg);
        if (rv <= 0)
            ++err;
    }
    if (!SSL_CONF_CTX_finish(cctx))
        ++err;
err:
    OSSL_LIB_CTX_set0_default(prev_libctx);
    SSL_CONF_CTX_free(cctx);
    if (err == 0) {
        ERR_pop_to_mark();
        return 1;
    }
    if (system && !conf_diagnostics) {
        /*
         * Discard errors so that SSL_CTX_new does not return
         * success with stale errors on the error stack.
         */
        ERR_pop_to_mark();
        return 1;
    }
    ERR_clear_last_mark();
    return 0;
}

int SSL_config(SSL *s, const char *name)
{
    return ssl_do_config(s, NULL, name, 0);
}

int SSL_CTX_config(SSL_CTX *ctx, const char *name)
{
    return ssl_do_config(NULL, ctx, name, 0);
}

int ssl_ctx_system_config(SSL_CTX *ctx)
{
    return ssl_do_config(NULL, ctx, NULL, 1);
}
