/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/conf.h>
#include <openssl/ssl.h>
#include "ssl_locl.h"

/* SSL library configuration module. */

struct ssl_conf_name {
    /* Name of this set of commands */
    char *name;
    /* List of commands */
    struct ssl_conf_cmd *cmds;
    /* Number of commands */
    size_t cmd_count;
};

struct ssl_conf_cmd {
    /* Command */
    char *cmd;
    /* Argument */
    char *arg;
};

static struct ssl_conf_name *ssl_names;
static size_t ssl_names_count;

static void ssl_module_free(CONF_IMODULE *md)
{
    size_t i, j;
    if (ssl_names == NULL)
        return;
    for (i = 0; i < ssl_names_count; i++) {
        struct ssl_conf_name *tname = ssl_names + i;
        OPENSSL_free(tname->name);
        for (j = 0; j < tname->cmd_count; j++) {
            OPENSSL_free(tname->cmds[j].cmd);
            OPENSSL_free(tname->cmds[j].arg);
        }
        OPENSSL_free(tname->cmds);
    }
    OPENSSL_free(ssl_names);
    ssl_names = NULL;
    ssl_names_count = 0;
}

static int ssl_module_init(CONF_IMODULE *md, const CONF *cnf)
{
    size_t i, j, cnt;
    int rv = 0;
    const char *ssl_conf_section;
    STACK_OF(CONF_VALUE) *cmd_lists;
    ssl_conf_section = CONF_imodule_get_value(md);
    cmd_lists = NCONF_get_section(cnf, ssl_conf_section);
    if (sk_CONF_VALUE_num(cmd_lists) <= 0) {
        if (cmd_lists == NULL)
            SSLerr(SSL_F_SSL_MODULE_INIT, SSL_R_SSL_SECTION_NOT_FOUND);
        else
            SSLerr(SSL_F_SSL_MODULE_INIT, SSL_R_SSL_SECTION_EMPTY);
        ERR_add_error_data(2, "section=", ssl_conf_section);
        goto err;
    }
    cnt = sk_CONF_VALUE_num(cmd_lists);
    ssl_names = OPENSSL_zalloc(sizeof(*ssl_names) * cnt);
    ssl_names_count = cnt;
    for (i = 0; i < ssl_names_count; i++) {
        struct ssl_conf_name *ssl_name = ssl_names + i;
        CONF_VALUE *sect = sk_CONF_VALUE_value(cmd_lists, i);
        STACK_OF(CONF_VALUE) *cmds = NCONF_get_section(cnf, sect->value);
        if (sk_CONF_VALUE_num(cmds) <= 0) {
            if (cmds == NULL)
                SSLerr(SSL_F_SSL_MODULE_INIT,
                       SSL_R_SSL_COMMAND_SECTION_NOT_FOUND);
            else
                SSLerr(SSL_F_SSL_MODULE_INIT, SSL_R_SSL_COMMAND_SECTION_EMPTY);
            ERR_add_error_data(4, "name=", sect->name, ", value=", sect->value);
            goto err;
        }
        ssl_name->name = BUF_strdup(sect->name);
        if (ssl_name->name == NULL)
            goto err;
        cnt = sk_CONF_VALUE_num(cmds);
        ssl_name->cmds = OPENSSL_zalloc(cnt * sizeof(struct ssl_conf_cmd));
        if (ssl_name->cmds == NULL)
            goto err;
        ssl_name->cmd_count = cnt;
        for (j = 0; j < cnt; j++) {
            const char *name;
            CONF_VALUE *cmd_conf = sk_CONF_VALUE_value(cmds, j);
            struct ssl_conf_cmd *cmd = ssl_name->cmds + j;
            /* Skip any initial dot in name */
            name = strchr(cmd_conf->name, '.');
            if (name != NULL)
                name++;
            else
                name = cmd_conf->name;
            cmd->cmd = BUF_strdup(name);
            cmd->arg = BUF_strdup(cmd_conf->value);
            if (cmd->cmd == NULL || cmd->arg == NULL)
                goto err;
        }

    }
    rv = 1;
 err:
    if (rv == 0)
        ssl_module_free(md);
    return rv;
}

void SSL_add_ssl_module(void)
{
    CONF_module_add("ssl_conf", ssl_module_init, ssl_module_free);
}

static const struct ssl_conf_name *ssl_name_find(const char *name)
{
    size_t i;
    const struct ssl_conf_name *nm;
    if (name == NULL)
        return NULL;
    for (i = 0, nm = ssl_names; i < ssl_names_count; i++, nm++) {
        if (strcmp(nm->name, name) == 0)
            return nm;
    }
    return NULL;
}

static int ssl_do_config(SSL *s, SSL_CTX *ctx, const char *name)
{
    SSL_CONF_CTX *cctx = NULL;
    size_t i;
    int rv = 0;
    unsigned int flags;
    const SSL_METHOD *meth;
    const struct ssl_conf_name *nm;
    struct ssl_conf_cmd *cmd;
    if (s == NULL && ctx == NULL) {
        SSLerr(SSL_F_SSL_DO_CONFIG, ERR_R_PASSED_NULL_PARAMETER);
        goto err;
    }
    nm = ssl_name_find(name);
    if (nm == NULL) {
        SSLerr(SSL_F_SSL_DO_CONFIG, SSL_R_INVALID_CONFIGURATION_NAME);
        ERR_add_error_data(2, "name=", name);
        goto err;
    }
    cctx = SSL_CONF_CTX_new();
    if (cctx == NULL)
        goto err;
    flags = SSL_CONF_FLAG_FILE;
    flags |= SSL_CONF_FLAG_CERTIFICATE | SSL_CONF_FLAG_REQUIRE_PRIVATE;
    if (s != NULL) {
        meth = s->method;
        SSL_CONF_CTX_set_ssl(cctx, s);
    } else {
        meth = ctx->method;
        SSL_CONF_CTX_set_ssl_ctx(cctx, ctx);
    }
    if (meth->ssl_accept != ssl_undefined_function)
        flags |= SSL_CONF_FLAG_SERVER;
    if (meth->ssl_connect != ssl_undefined_function)
        flags |= SSL_CONF_FLAG_CLIENT;
    SSL_CONF_CTX_set_flags(cctx, flags);
    for (i = 0, cmd = nm->cmds; i < nm->cmd_count; i++, cmd++) {
        rv = SSL_CONF_cmd(cctx, cmd->cmd, cmd->arg);
        if (rv <= 0) {
            if (rv == -2)
                SSLerr(SSL_F_SSL_DO_CONFIG, SSL_R_UNKNOWN_COMMAND);
            else
                SSLerr(SSL_F_SSL_DO_CONFIG, SSL_R_BAD_VALUE);
            ERR_add_error_data(6, "section=", name, ", cmd=", cmd->cmd,
                               ", arg=", cmd->arg);
            goto err;
        }
    }
    rv = SSL_CONF_CTX_finish(cctx);
 err:
    SSL_CONF_CTX_free(cctx);
    return rv <= 0 ? 0 : 1;
}

int SSL_config(SSL *s, const char *name)
{
    return ssl_do_config(s, NULL, name);
}

int SSL_CTX_config(SSL_CTX *ctx, const char *name)
{
    return ssl_do_config(NULL, ctx, name);
}
