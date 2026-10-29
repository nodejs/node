/*
 * Copyright 2016-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_INTERNAL_CONF_H
#define OSSL_INTERNAL_CONF_H
#pragma once

#include "internal/dso.h"
#include "internal/thread_once.h"

#include <openssl/conf.h>

#define DEFAULT_CONF_MFLAGS \
    (CONF_MFLAGS_DEFAULT_SECTION | CONF_MFLAGS_IGNORE_MISSING_FILE | CONF_MFLAGS_IGNORE_RETURN_CODES)

struct ossl_init_settings_st {
    char *filename;
    char *appname;
    unsigned long flags;
};

/*
 * This structure contains a data about supported modules. entries in this
 * table correspond to either dynamic or static modules.
 */

struct conf_module_st {
    /* DSO of this module or NULL if static */
    DSO *dso;
    /* Name of the module */
    char *name;
    /* Init function */
    conf_init_func *init;
    /* Finish function */
    conf_finish_func *finish;
    /* Number of successfully initialized modules */
    int links;
    void *usr_data;
};

/*
 * This structure contains information about modules that have been
 * successfully initialized. There may be more than one entry for a given
 * module.
 */

struct conf_imodule_st {
    CONF_MODULE *pmod;
    OSSL_LIB_CTX *libctx;
    char *name;
    char *value;
    unsigned long flags;
    void *usr_data;
};

int ossl_config_int(const OPENSSL_INIT_SETTINGS *);
void ossl_no_config_int(void);
void ossl_config_modules_free(void);
int ossl_conf_parse_bool(const char *value, int *result);

#endif
