/*
 * Copyright 2018-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_INTERNAL_SSLCONF_H
#define OSSL_INTERNAL_SSLCONF_H
#pragma once

#include <stddef.h>

#include <openssl/conf.h>

typedef struct ssl_conf_cmd_st SSL_CONF_CMD;

/*
 * SSL library configuration module placeholder. We load it here but defer
 * all decisions about its contents to libssl.
 */

struct ssl_conf_name_st {
    /* Name of this set of commands */
    char *name;
    /* List of commands */
    SSL_CONF_CMD *cmds;
    /* Number of commands */
    size_t cmd_count;
};

struct ssl_conf_cmd_st {
    /* Command */
    char *cmd;
    /* Argument */
    char *arg;
};

const SSL_CONF_CMD *conf_ssl_get(CONF_IMODULE *m, size_t idx, const char **name,
    size_t *cnt);
int conf_ssl_name_find(CONF_IMODULE *m, const char *name, size_t *idx);
void conf_ssl_get_cmd(const SSL_CONF_CMD *c, size_t idx, char **cmd, char **arg);

#endif
