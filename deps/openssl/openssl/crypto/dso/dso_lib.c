/* dso_lib.c */
/*
 * Written by Geoff Thorpe (geoff@geoffthorpe.net) for the OpenSSL project
 * 2000.
 */
/* ====================================================================
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <stdio.h>
#include <openssl/crypto.h>
#include "cryptlib.h"
#include <openssl/dso.h>

static DSO_METHOD *default_DSO_meth = NULL;

DSO *DSO_new(void)
{
    return (DSO_new_method(NULL));
}

void DSO_set_default_method(DSO_METHOD *meth)
{
    default_DSO_meth = meth;
}

DSO_METHOD *DSO_get_default_method(void)
{
    return (default_DSO_meth);
}

DSO_METHOD *DSO_get_method(DSO *dso)
{
    return (dso->meth);
}

DSO_METHOD *DSO_set_method(DSO *dso, DSO_METHOD *meth)
{
    DSO_METHOD *mtmp;
    mtmp = dso->meth;
    dso->meth = meth;
    return (mtmp);
}

DSO *DSO_new_method(DSO_METHOD *meth)
{
    DSO *ret;

    if (default_DSO_meth == NULL)
        /*
         * We default to DSO_METH_openssl() which in turn defaults to
         * stealing the "best available" method. Will fallback to
         * DSO_METH_null() in the worst case.
         */
        default_DSO_meth = DSO_METHOD_openssl();
    ret = (DSO *)OPENSSL_malloc(sizeof(DSO));
    if (ret == NULL) {
        DSOerr(DSO_F_DSO_NEW_METHOD, ERR_R_MALLOC_FAILURE);
        return (NULL);
    }
    memset(ret, 0, sizeof(DSO));
    ret->meth_data = sk_void_new_null();
    if (ret->meth_data == NULL) {
        /* sk_new doesn't generate any errors so we do */
        DSOerr(DSO_F_DSO_NEW_METHOD, ERR_R_MALLOC_FAILURE);
        OPENSSL_free(ret);
        return (NULL);
    }
    if (meth == NULL)
        ret->meth = default_DSO_meth;
    else
        ret->meth = meth;
    ret->references = 1;
    if ((ret->meth->init != NULL) && !ret->meth->init(ret)) {
        sk_void_free(ret->meth_data);
        OPENSSL_free(ret);
        ret = NULL;
    }
    return (ret);
}

int DSO_free(DSO *dso)
{
    int i;

    if (dso == NULL) {
        DSOerr(DSO_F_DSO_FREE, ERR_R_PASSED_NULL_PARAMETER);
        return (0);
    }

    i = CRYPTO_add(&dso->references, -1, CRYPTO_LOCK_DSO);
#ifdef REF_PRINT
    REF_PRINT("DSO", dso);
#endif
    if (i > 0)
        return (1);
#ifdef REF_CHECK
    if (i < 0) {
        fprintf(stderr, "DSO_free, bad reference count\n");
        abort();
    }
#endif

    if ((dso->meth->dso_unload != NULL) && !dso->meth->dso_unload(dso)) {
        DSOerr(DSO_F_DSO_FREE, DSO_R_UNLOAD_FAILED);
        return (0);
    }

    if ((dso->meth->finish != NULL) && !dso->meth->finish(dso)) {
        DSOerr(DSO_F_DSO_FREE, DSO_R_FINISH_FAILED);
        return (0);
    }

    sk_void_free(dso->meth_data);
    if (dso->filename != NULL)
        OPENSSL_free(dso->filename);
    if (dso->loaded_filename != NULL)
        OPENSSL_free(dso->loaded_filename);

    OPENSSL_free(dso);
    return (1);
}

int DSO_flags(DSO *dso)
{
    return ((dso == NULL) ? 0 : dso->flags);
}

int DSO_up_ref(DSO *dso)
{
    if (dso == NULL) {
        DSOerr(DSO_F_DSO_UP_REF, ERR_R_PASSED_NULL_PARAMETER);
        return (0);
    }

    CRYPTO_add(&dso->references, 1, CRYPTO_LOCK_DSO);
    return (1);
}

DSO *DSO_load(DSO *dso, const char *filename, DSO_METHOD *meth, int flags)
{
    DSO *ret;
    int allocated = 0;

    if (dso == NULL) {
        ret = DSO_new_method(meth);
        if (ret == NULL) {
            DSOerr(DSO_F_DSO_LOAD, ERR_R_MALLOC_FAILURE);
            goto err;
        }
        allocated = 1;
        /* Pass the provided flags to the new DSO object */
        if (DSO_ctrl(ret, DSO_CTRL_SET_FLAGS, flags, NULL) < 0) {
            DSOerr(DSO_F_DSO_LOAD, DSO_R_CTRL_FAILED);
            goto err;
        }
    } else
        ret = dso;
    /* Don't load if we're currently already loaded */
    if (ret->filename != NULL) {
        DSOerr(DSO_F_DSO_LOAD, DSO_R_DSO_ALREADY_LOADED);
        goto err;
    }
    /*
     * filename can only be NULL if we were passed a dso that already has one
     * set.
     */
    if (filename != NULL)
        if (!DSO_set_filename(ret, filename)) {
            DSOerr(DSO_F_DSO_LOAD, DSO_R_SET_FILENAME_FAILED);
            goto err;
        }
    filename = ret->filename;
    if (filename == NULL) {
        DSOerr(DSO_F_DSO_LOAD, DSO_R_NO_FILENAME);
        goto err;
    }
    if (ret->meth->dso_load == NULL) {
        DSOerr(DSO_F_DSO_LOAD, DSO_R_UNSUPPORTED);
        goto err;
    }
    if (!ret->meth->dso_load(ret)) {
        DSOerr(DSO_F_DSO_LOAD, DSO_R_LOAD_FAILED);
        goto err;
    }
    /* Load succeeded */
    return (ret);
 err:
    if (allocated)
        DSO_free(ret);
    return (NULL);
}

void *DSO_bind_var(DSO *dso, const char *symname)
{
    void *ret = NULL;

    if ((dso == NULL) || (symname == NULL)) {
        DSOerr(DSO_F_DSO_BIND_VAR, ERR_R_PASSED_NULL_PARAMETER);
        return (NULL);
    }
    if (dso->meth->dso_bind_var == NULL) {
        DSOerr(DSO_F_DSO_BIND_VAR, DSO_R_UNSUPPORTED);
        return (NULL);
    }
    if ((ret = dso->meth->dso_bind_var(dso, symname)) == NULL) {
        DSOerr(DSO_F_DSO_BIND_VAR, DSO_R_SYM_FAILURE);
        return (NULL);
    }
    /* Success */
    return (ret);
}

DSO_FUNC_TYPE DSO_bind_func(DSO *dso, const char *symname)
{
    DSO_FUNC_TYPE ret = NULL;

    if ((dso == NULL) || (symname == NULL)) {
        DSOerr(DSO_F_DSO_BIND_FUNC, ERR_R_PASSED_NULL_PARAMETER);
        return (NULL);
    }
    if (dso->meth->dso_bind_func == NULL) {
        DSOerr(DSO_F_DSO_BIND_FUNC, DSO_R_UNSUPPORTED);
        return (NULL);
    }
    if ((ret = dso->meth->dso_bind_func(dso, symname)) == NULL) {
        DSOerr(DSO_F_DSO_BIND_FUNC, DSO_R_SYM_FAILURE);
        return (NULL);
    }
    /* Success */
    return (ret);
}

/*
 * I don't really like these *_ctrl functions very much to be perfectly
 * honest. For one thing, I think I have to return a negative value for any
 * error because possible DSO_ctrl() commands may return values such as
 * "size"s that can legitimately be zero (making the standard
 * "if (DSO_cmd(...))" form that works almost everywhere else fail at odd
 * times. I'd prefer "output" values to be passed by reference and the return
 * value as success/failure like usual ... but we conform when we must... :-)
 */
long DSO_ctrl(DSO *dso, int cmd, long larg, void *parg)
{
    if (dso == NULL) {
        DSOerr(DSO_F_DSO_CTRL, ERR_R_PASSED_NULL_PARAMETER);
        return (-1);
    }
    /*
     * We should intercept certain generic commands and only pass control to
     * the method-specific ctrl() function if it's something we don't handle.
     */
    switch (cmd) {
    case DSO_CTRL_GET_FLAGS:
        return dso->flags;
    case DSO_CTRL_SET_FLAGS:
        dso->flags = (int)larg;
        return (0);
    case DSO_CTRL_OR_FLAGS:
        dso->flags |= (int)larg;
        return (0);
    default:
        break;
    }
    if ((dso->meth == NULL) || (dso->meth->dso_ctrl == NULL)) {
        DSOerr(DSO_F_DSO_CTRL, DSO_R_UNSUPPORTED);
        return (-1);
    }
    return (dso->meth->dso_ctrl(dso, cmd, larg, parg));
}

int DSO_set_name_converter(DSO *dso, DSO_NAME_CONVERTER_FUNC cb,
                           DSO_NAME_CONVERTER_FUNC *oldcb)
{
    if (dso == NULL) {
        DSOerr(DSO_F_DSO_SET_NAME_CONVERTER, ERR_R_PASSED_NULL_PARAMETER);
        return (0);
    }
    if (oldcb)
        *oldcb = dso->name_converter;
    dso->name_converter = cb;
    return (1);
}

const char *DSO_get_filename(DSO *dso)
{
    if (dso == NULL) {
        DSOerr(DSO_F_DSO_GET_FILENAME, ERR_R_PASSED_NULL_PARAMETER);
        return (NULL);
    }
    return (dso->filename);
}

int DSO_set_filename(DSO *dso, const char *filename)
{
    char *copied;

    if ((dso == NULL) || (filename == NULL)) {
        DSOerr(DSO_F_DSO_SET_FILENAME, ERR_R_PASSED_NULL_PARAMETER);
        return (0);
    }
    if (dso->loaded_filename) {
        DSOerr(DSO_F_DSO_SET_FILENAME, DSO_R_DSO_ALREADY_LOADED);
        return (0);
    }
    /* We'll duplicate filename */
    copied = OPENSSL_malloc(strlen(filename) + 1);
    if (copied == NULL) {
        DSOerr(DSO_F_DSO_SET_FILENAME, ERR_R_MALLOC_FAILURE);
        return (0);
    }
    BUF_strlcpy(copied, filename, strlen(filename) + 1);
    if (dso->filename)
        OPENSSL_free(dso->filename);
    dso->filename = copied;
    return (1);
}

char *DSO_merge(DSO *dso, const char *filespec1, const char *filespec2)
{
    char *result = NULL;

    if (dso == NULL || filespec1 == NULL) {
        DSOerr(DSO_F_DSO_MERGE, ERR_R_PASSED_NULL_PARAMETER);
        return (NULL);
    }
    if ((dso->flags & DSO_FLAG_NO_NAME_TRANSLATION) == 0) {
        if (dso->merger != NULL)
            result = dso->merger(dso, filespec1, filespec2);
        else if (dso->meth->dso_merger != NULL)
            result = dso->meth->dso_merger(dso, filespec1, filespec2);
    }
    return (result);
}

char *DSO_convert_filename(DSO *dso, const char *filename)
{
    char *result = NULL;

    if (dso == NULL) {
        DSOerr(DSO_F_DSO_CONVERT_FILENAME, ERR_R_PASSED_NULL_PARAMETER);
        return (NULL);
    }
    if (filename == NULL)
        filename = dso->filename;
    if (filename == NULL) {
        DSOerr(DSO_F_DSO_CONVERT_FILENAME, DSO_R_NO_FILENAME);
        return (NULL);
    }
    if ((dso->flags & DSO_FLAG_NO_NAME_TRANSLATION) == 0) {
        if (dso->name_converter != NULL)
            result = dso->name_converter(dso, filename);
        else if (dso->meth->dso_name_converter != NULL)
            result = dso->meth->dso_name_converter(dso, filename);
    }
    if (result == NULL) {
        result = OPENSSL_malloc(strlen(filename) + 1);
        if (result == NULL) {
            DSOerr(DSO_F_DSO_CONVERT_FILENAME, ERR_R_MALLOC_FAILURE);
            return (NULL);
        }
        BUF_strlcpy(result, filename, strlen(filename) + 1);
    }
    return (result);
}

const char *DSO_get_loaded_filename(DSO *dso)
{
    if (dso == NULL) {
        DSOerr(DSO_F_DSO_GET_LOADED_FILENAME, ERR_R_PASSED_NULL_PARAMETER);
        return (NULL);
    }
    return (dso->loaded_filename);
}

int DSO_pathbyaddr(void *addr, char *path, int sz)
{
    DSO_METHOD *meth = default_DSO_meth;
    if (meth == NULL)
        meth = DSO_METHOD_openssl();
    if (meth->pathbyaddr == NULL) {
        DSOerr(DSO_F_DSO_PATHBYADDR, DSO_R_UNSUPPORTED);
        return -1;
    }
    return (*meth->pathbyaddr) (addr, path, sz);
}

void *DSO_global_lookup(const char *name)
{
    DSO_METHOD *meth = default_DSO_meth;
    if (meth == NULL)
        meth = DSO_METHOD_openssl();
    if (meth->globallookup == NULL) {
        DSOerr(DSO_F_DSO_GLOBAL_LOOKUP, DSO_R_UNSUPPORTED);
        return NULL;
    }
    return (*meth->globallookup) (name);
}
