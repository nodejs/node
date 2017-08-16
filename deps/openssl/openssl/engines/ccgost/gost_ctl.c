/**********************************************************************
 *                        gost_ctl.c                                  *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *       This file is distributed under the same license as OpenSSL   *
 *                                                                    *
 *        Implementation of control commands for GOST engine          *
 *            OpenSSL 0.9.9 libraries required                        *
 **********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/buffer.h>
#include "gost_lcl.h"

static char *gost_params[GOST_PARAM_MAX + 1] = { NULL };
static const char *gost_envnames[] = { "CRYPT_PARAMS" };

const ENGINE_CMD_DEFN gost_cmds[] = {
/*- { GOST_CTRL_RNG,
    "RNG",
    "Type of random number generator to use",
    ENGINE_CMD_FLAG_STRING
    },
    { GOST_CTRL_RNG_PARAMS,
    "RNG_PARAMS",
    "Parameter for random number generator",
    ENGINE_CMD_FLAG_STRING
    },
*/ {GOST_CTRL_CRYPT_PARAMS,
           "CRYPT_PARAMS",
           "OID of default GOST 28147-89 parameters",
           ENGINE_CMD_FLAG_STRING},
    {0, NULL, NULL, 0}
};

void gost_param_free()
{
    int i;
    for (i = 0; i <= GOST_PARAM_MAX; i++)
        if (gost_params[i] != NULL) {
            OPENSSL_free(gost_params[i]);
            gost_params[i] = NULL;
        }

}

int gost_control_func(ENGINE *e, int cmd, long i, void *p, void (*f) (void))
{
    int param = cmd - ENGINE_CMD_BASE;
    int ret = 0;
    if (param < 0 || param > GOST_PARAM_MAX)
        return -1;
    ret = gost_set_default_param(param, p);
    return ret;
}

const char *get_gost_engine_param(int param)
{
    char *tmp;
    if (param < 0 || param > GOST_PARAM_MAX)
        return NULL;
    if (gost_params[param] != NULL) {
        return gost_params[param];
    }
    tmp = getenv(gost_envnames[param]);
    if (tmp) {
        if (gost_params[param])
            OPENSSL_free(gost_params[param]);
        gost_params[param] = BUF_strdup(tmp);
        return gost_params[param];
    }
    return NULL;
}

int gost_set_default_param(int param, const char *value)
{
    const char *tmp;
    if (param < 0 || param > GOST_PARAM_MAX)
        return 0;
    tmp = getenv(gost_envnames[param]);
    /*
     * if there is value in the environment, use it, else -passed string *
     */
    if (!tmp)
        tmp = value;
    if (gost_params[param])
        OPENSSL_free(gost_params[param]);
    gost_params[param] = BUF_strdup(tmp);

    return 1;
}
