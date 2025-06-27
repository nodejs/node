/*
 * Copyright 2018-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_INTERNAL_SSLCONF_H
# define OSSL_INTERNAL_SSLCONF_H
# pragma once

typedef struct ssl_conf_cmd_st SSL_CONF_CMD;

const SSL_CONF_CMD *conf_ssl_get(size_t idx, const char **name, size_t *cnt);
int conf_ssl_name_find(const char *name, size_t *idx);
void conf_ssl_get_cmd(const SSL_CONF_CMD *cmd, size_t idx, char **cmdstr,
                      char **arg);

#endif
