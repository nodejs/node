/*
 * Copyright 2015-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include "conf_local.h"
#include "internal/sslconf.h"
#include "internal/core.h"
#include "internal/cryptlib.h"

typedef struct ssl_module_st SSL_MODULE;

struct ssl_module_st {
    struct ssl_conf_name_st *names;
    size_t names_count;
};

static void ssl_module_free(CONF_IMODULE *md)
{
    SSL_MODULE *ssl = CONF_imodule_get_usr_data(md);

    if (ssl == NULL)
        return;

    CONF_imodule_set_usr_data(md, NULL);
    ossl_lib_ctx_detach_ssl_conf_imodule(NULL, md);

    for (size_t i = 0; i < ssl->names_count; i++) {
        struct ssl_conf_name_st *tname = ssl->names + i;

        OPENSSL_free(tname->name);
        for (size_t j = 0; j < tname->cmd_count; j++) {
            OPENSSL_free(tname->cmds[j].cmd);
            OPENSSL_free(tname->cmds[j].arg);
        }
        OPENSSL_free(tname->cmds);
    }

    OPENSSL_free(ssl->names);
    OPENSSL_free(ssl);
}

static int ssl_module_init(CONF_IMODULE *md, const CONF *cnf)
{
    size_t i, j, cnt;
    int rv = 0;
    const char *ssl_conf_section;
    STACK_OF(CONF_VALUE) *cmd_lists;
    OSSL_LIB_CTX *libctx;
    SSL_MODULE *ssl = NULL;

    ssl_conf_section = CONF_imodule_get_value(md);
    cmd_lists = NCONF_get_section(cnf, ssl_conf_section);
    if (sk_CONF_VALUE_num(cmd_lists) <= 0) {
        int rcode = cmd_lists == NULL
            ? CONF_R_SSL_SECTION_NOT_FOUND
            : CONF_R_SSL_SECTION_EMPTY;

        ERR_raise_data(ERR_LIB_CONF, rcode, "section=%s", ssl_conf_section);
        goto err;
    }
    cnt = sk_CONF_VALUE_num(cmd_lists);
    ssl_module_free(md);

    ssl = OPENSSL_zalloc(sizeof(*ssl));
    if (ssl == NULL)
        goto err;
    CONF_imodule_set_usr_data(md, ssl);

    ssl->names = OPENSSL_calloc(cnt, sizeof(*ssl->names));
    libctx = ossl_lib_ctx_get_concrete(cnf->libctx);
    if (libctx == NULL || ssl->names == NULL)
        goto err;

    ossl_lib_ctx_attach_ssl_conf_imodule(libctx, md);
    ssl->names_count = cnt;
    for (i = 0; i < ssl->names_count; i++) {
        struct ssl_conf_name_st *ssl_name = ssl->names + i;
        CONF_VALUE *sect = sk_CONF_VALUE_value(cmd_lists, (int)i);
        STACK_OF(CONF_VALUE) *cmds = NCONF_get_section(cnf, sect->value);

        if (sk_CONF_VALUE_num(cmds) <= 0) {
            int rcode = cmds == NULL
                ? CONF_R_SSL_COMMAND_SECTION_NOT_FOUND
                : CONF_R_SSL_COMMAND_SECTION_EMPTY;

            ERR_raise_data(ERR_LIB_CONF, rcode,
                "name=%s, value=%s", sect->name, sect->value);
            goto err;
        }
        ssl_name->name = OPENSSL_strdup(sect->name);
        if (ssl_name->name == NULL)
            goto err;
        cnt = sk_CONF_VALUE_num(cmds);
        ssl_name->cmds = OPENSSL_calloc(cnt, sizeof(struct ssl_conf_cmd_st));
        if (ssl_name->cmds == NULL)
            goto err;
        ssl_name->cmd_count = cnt;
        for (j = 0; j < cnt; j++) {
            const char *name;
            CONF_VALUE *cmd_conf = sk_CONF_VALUE_value(cmds, (int)j);
            struct ssl_conf_cmd_st *cmd = ssl_name->cmds + j;

            /* Skip any initial dot in name */
            name = strchr(cmd_conf->name, '.');
            if (name != NULL)
                name++;
            else
                name = cmd_conf->name;
            cmd->cmd = OPENSSL_strdup(name);
            cmd->arg = OPENSSL_strdup(cmd_conf->value);
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

/*
 * Returns the set of commands with index |idx| previously searched for via
 * conf_ssl_name_find. Also stores the name of the set of commands in |*name|
 * and the number of commands in the set in |*cnt|.
 */
const SSL_CONF_CMD *conf_ssl_get(CONF_IMODULE *md, size_t idx, const char **name, size_t *cnt)
{
    SSL_MODULE *ssl = md != NULL ? CONF_imodule_get_usr_data(md) : NULL;

    if (ssl == NULL || ssl->names == NULL)
        return NULL;

    *name = ssl->names[idx].name;
    *cnt = ssl->names[idx].cmd_count;
    return ssl->names[idx].cmds;
}

/*
 * Search for the named set of commands given in |name|. On success return the
 * index for the command set in |*idx|.
 * Returns 1 on success or 0 on failure.
 */
int conf_ssl_name_find(CONF_IMODULE *md, const char *name, size_t *idx)
{
    SSL_MODULE *ssl = md != NULL ? CONF_imodule_get_usr_data(md) : NULL;
    const struct ssl_conf_name_st *nm;
    size_t i;

    if (ssl == NULL || ssl->names == NULL)
        return 0;

    for (i = 0, nm = ssl->names; i < ssl->names_count; i++, nm++) {
        if (strcmp(nm->name, name) == 0) {
            *idx = i;
            return 1;
        }
    }
    return 0;
}

/*
 * Given a command set |cmd|, return details on the command at index |idx| which
 * must be less than the number of commands in the set (as returned by
 * conf_ssl_get). The name of the command will be returned in |*cmdstr| and the
 * argument is returned in |*arg|.
 */
void conf_ssl_get_cmd(const SSL_CONF_CMD *cmd, size_t idx, char **cmdstr,
    char **arg)
{
    *cmdstr = cmd[idx].cmd;
    *arg = cmd[idx].arg;
}

void ossl_config_add_ssl_module(void)
{
    CONF_module_add("ssl_conf", ssl_module_init, ssl_module_free);
}
