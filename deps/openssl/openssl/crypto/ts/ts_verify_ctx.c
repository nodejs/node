/* crypto/ts/ts_verify_ctx.c */
/*
 * Written by Zoltan Glozik (zglozik@stones.com) for the OpenSSL project
 * 2003.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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

#include "cryptlib.h"
#include <openssl/objects.h>
#include <openssl/ts.h>

TS_VERIFY_CTX *TS_VERIFY_CTX_new(void)
{
    TS_VERIFY_CTX *ctx =
        (TS_VERIFY_CTX *)OPENSSL_malloc(sizeof(TS_VERIFY_CTX));
    if (ctx)
        memset(ctx, 0, sizeof(TS_VERIFY_CTX));
    else
        TSerr(TS_F_TS_VERIFY_CTX_NEW, ERR_R_MALLOC_FAILURE);
    return ctx;
}

void TS_VERIFY_CTX_init(TS_VERIFY_CTX *ctx)
{
    OPENSSL_assert(ctx != NULL);
    memset(ctx, 0, sizeof(TS_VERIFY_CTX));
}

void TS_VERIFY_CTX_free(TS_VERIFY_CTX *ctx)
{
    if (!ctx)
        return;

    TS_VERIFY_CTX_cleanup(ctx);
    OPENSSL_free(ctx);
}

void TS_VERIFY_CTX_cleanup(TS_VERIFY_CTX *ctx)
{
    if (!ctx)
        return;

    X509_STORE_free(ctx->store);
    sk_X509_pop_free(ctx->certs, X509_free);

    ASN1_OBJECT_free(ctx->policy);

    X509_ALGOR_free(ctx->md_alg);
    OPENSSL_free(ctx->imprint);

    BIO_free_all(ctx->data);

    ASN1_INTEGER_free(ctx->nonce);

    GENERAL_NAME_free(ctx->tsa_name);

    TS_VERIFY_CTX_init(ctx);
}

TS_VERIFY_CTX *TS_REQ_to_TS_VERIFY_CTX(TS_REQ *req, TS_VERIFY_CTX *ctx)
{
    TS_VERIFY_CTX *ret = ctx;
    ASN1_OBJECT *policy;
    TS_MSG_IMPRINT *imprint;
    X509_ALGOR *md_alg;
    ASN1_OCTET_STRING *msg;
    const ASN1_INTEGER *nonce;

    OPENSSL_assert(req != NULL);
    if (ret)
        TS_VERIFY_CTX_cleanup(ret);
    else if (!(ret = TS_VERIFY_CTX_new()))
        return NULL;

    /* Setting flags. */
    ret->flags = TS_VFY_ALL_IMPRINT & ~(TS_VFY_TSA_NAME | TS_VFY_SIGNATURE);

    /* Setting policy. */
    if ((policy = TS_REQ_get_policy_id(req)) != NULL) {
        if (!(ret->policy = OBJ_dup(policy)))
            goto err;
    } else
        ret->flags &= ~TS_VFY_POLICY;

    /* Setting md_alg, imprint and imprint_len. */
    imprint = TS_REQ_get_msg_imprint(req);
    md_alg = TS_MSG_IMPRINT_get_algo(imprint);
    if (!(ret->md_alg = X509_ALGOR_dup(md_alg)))
        goto err;
    msg = TS_MSG_IMPRINT_get_msg(imprint);
    ret->imprint_len = ASN1_STRING_length(msg);
    if (!(ret->imprint = OPENSSL_malloc(ret->imprint_len)))
        goto err;
    memcpy(ret->imprint, ASN1_STRING_data(msg), ret->imprint_len);

    /* Setting nonce. */
    if ((nonce = TS_REQ_get_nonce(req)) != NULL) {
        if (!(ret->nonce = ASN1_INTEGER_dup(nonce)))
            goto err;
    } else
        ret->flags &= ~TS_VFY_NONCE;

    return ret;
 err:
    if (ctx)
        TS_VERIFY_CTX_cleanup(ctx);
    else
        TS_VERIFY_CTX_free(ret);
    return NULL;
}
