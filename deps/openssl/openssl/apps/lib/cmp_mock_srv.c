/*
 * Copyright 2018-2023 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright Siemens AG 2018-2020
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or atf
 * https://www.openssl.org/source/license.html
 */

#include "apps.h"
#include "cmp_mock_srv.h"

#include <openssl/cmp.h>
#include <openssl/err.h>
#include <openssl/cmperr.h>

/* the context for the CMP mock server */
typedef struct
{
    X509 *certOut;             /* certificate to be returned in cp/ip/kup msg */
    STACK_OF(X509) *chainOut;  /* chain of certOut to add to extraCerts field */
    STACK_OF(X509) *caPubsOut; /* certs to return in caPubs field of ip msg */
    OSSL_CMP_PKISI *statusOut; /* status for ip/cp/kup/rp msg unless polling */
    int sendError;             /* send error response on given request type */
    OSSL_CMP_MSG *certReq;     /* ir/cr/p10cr/kur remembered while polling */
    int pollCount;             /* number of polls before actual cert response */
    int curr_pollCount;        /* number of polls so far for current request */
    int checkAfterTime;        /* time the client should wait between polling */
} mock_srv_ctx;


static void mock_srv_ctx_free(mock_srv_ctx *ctx)
{
    if (ctx == NULL)
        return;

    OSSL_CMP_PKISI_free(ctx->statusOut);
    X509_free(ctx->certOut);
    sk_X509_pop_free(ctx->chainOut, X509_free);
    sk_X509_pop_free(ctx->caPubsOut, X509_free);
    OSSL_CMP_MSG_free(ctx->certReq);
    OPENSSL_free(ctx);
}

static mock_srv_ctx *mock_srv_ctx_new(void)
{
    mock_srv_ctx *ctx = OPENSSL_zalloc(sizeof(mock_srv_ctx));

    if (ctx == NULL)
        goto err;

    if ((ctx->statusOut = OSSL_CMP_PKISI_new()) == NULL)
        goto err;

    ctx->sendError = -1;

    /* all other elements are initialized to 0 or NULL, respectively */
    return ctx;
 err:
    mock_srv_ctx_free(ctx);
    return NULL;
}

int ossl_cmp_mock_srv_set1_certOut(OSSL_CMP_SRV_CTX *srv_ctx, X509 *cert)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return 0;
    }
    if (cert == NULL || X509_up_ref(cert)) {
        X509_free(ctx->certOut);
        ctx->certOut = cert;
        return 1;
    }
    return 0;
}

int ossl_cmp_mock_srv_set1_chainOut(OSSL_CMP_SRV_CTX *srv_ctx,
                                    STACK_OF(X509) *chain)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);
    STACK_OF(X509) *chain_copy = NULL;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return 0;
    }
    if (chain != NULL && (chain_copy = X509_chain_up_ref(chain)) == NULL)
        return 0;
    sk_X509_pop_free(ctx->chainOut, X509_free);
    ctx->chainOut = chain_copy;
    return 1;
}

int ossl_cmp_mock_srv_set1_caPubsOut(OSSL_CMP_SRV_CTX *srv_ctx,
                                     STACK_OF(X509) *caPubs)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);
    STACK_OF(X509) *caPubs_copy = NULL;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return 0;
    }
    if (caPubs != NULL && (caPubs_copy = X509_chain_up_ref(caPubs)) == NULL)
        return 0;
    sk_X509_pop_free(ctx->caPubsOut, X509_free);
    ctx->caPubsOut = caPubs_copy;
    return 1;
}

int ossl_cmp_mock_srv_set_statusInfo(OSSL_CMP_SRV_CTX *srv_ctx, int status,
                                     int fail_info, const char *text)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);
    OSSL_CMP_PKISI *si;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return 0;
    }
    if ((si = OSSL_CMP_STATUSINFO_new(status, fail_info, text)) == NULL)
        return 0;
    OSSL_CMP_PKISI_free(ctx->statusOut);
    ctx->statusOut = si;
    return 1;
}

int ossl_cmp_mock_srv_set_sendError(OSSL_CMP_SRV_CTX *srv_ctx, int bodytype)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return 0;
    }
    /* might check bodytype, but this would require exporting all body types */
    ctx->sendError = bodytype;
    return 1;
}

int ossl_cmp_mock_srv_set_pollCount(OSSL_CMP_SRV_CTX *srv_ctx, int count)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return 0;
    }
    if (count < 0) {
        ERR_raise(ERR_LIB_CMP, CMP_R_INVALID_ARGS);
        return 0;
    }
    ctx->pollCount = count;
    return 1;
}

int ossl_cmp_mock_srv_set_checkAfterTime(OSSL_CMP_SRV_CTX *srv_ctx, int sec)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return 0;
    }
    ctx->checkAfterTime = sec;
    return 1;
}

static OSSL_CMP_PKISI *process_cert_request(OSSL_CMP_SRV_CTX *srv_ctx,
                                            const OSSL_CMP_MSG *cert_req,
                                            ossl_unused int certReqId,
                                            const OSSL_CRMF_MSG *crm,
                                            const X509_REQ *p10cr,
                                            X509 **certOut,
                                            STACK_OF(X509) **chainOut,
                                            STACK_OF(X509) **caPubs)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);
    OSSL_CMP_PKISI *si = NULL;

    if (ctx == NULL || cert_req == NULL
            || certOut == NULL || chainOut == NULL || caPubs == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return NULL;
    }
    if (ctx->sendError == 1
            || ctx->sendError == OSSL_CMP_MSG_get_bodytype(cert_req)) {
        ERR_raise(ERR_LIB_CMP, CMP_R_ERROR_PROCESSING_MESSAGE);
        return NULL;
    }

    *certOut = NULL;
    *chainOut = NULL;
    *caPubs = NULL;

    if (ctx->pollCount > 0 && ctx->curr_pollCount == 0) {
        /* start polling */
        if (ctx->certReq != NULL) {
            /* already in polling mode */
            ERR_raise(ERR_LIB_CMP, CMP_R_UNEXPECTED_PKIBODY);
            return NULL;
        }
        if ((ctx->certReq = OSSL_CMP_MSG_dup(cert_req)) == NULL)
            return NULL;
        return OSSL_CMP_STATUSINFO_new(OSSL_CMP_PKISTATUS_waiting, 0, NULL);
    }
    if (ctx->curr_pollCount >= ctx->pollCount)
        /* give final response after polling */
        ctx->curr_pollCount = 0;

    if (OSSL_CMP_MSG_get_bodytype(cert_req) == OSSL_CMP_KUR
            && crm != NULL && ctx->certOut != NULL) {
        const OSSL_CRMF_CERTID *cid = OSSL_CRMF_MSG_get0_regCtrl_oldCertID(crm);
        const X509_NAME *issuer = X509_get_issuer_name(ctx->certOut);
        const ASN1_INTEGER *serial = X509_get0_serialNumber(ctx->certOut);

        if (cid == NULL) {
            ERR_raise(ERR_LIB_CMP, CMP_R_MISSING_CERTID);
            return NULL;
        }
        if (issuer != NULL
            && X509_NAME_cmp(issuer, OSSL_CRMF_CERTID_get0_issuer(cid)) != 0) {
            ERR_raise(ERR_LIB_CMP, CMP_R_WRONG_CERTID);
            return NULL;
        }
        if (serial != NULL
            && ASN1_INTEGER_cmp(serial,
                                OSSL_CRMF_CERTID_get0_serialNumber(cid)) != 0) {
            ERR_raise(ERR_LIB_CMP, CMP_R_WRONG_CERTID);
            return NULL;
        }
    }

    if (ctx->certOut != NULL
            && (*certOut = X509_dup(ctx->certOut)) == NULL)
        goto err;
    if (ctx->chainOut != NULL
            && (*chainOut = X509_chain_up_ref(ctx->chainOut)) == NULL)
        goto err;
    if (ctx->caPubsOut != NULL
            && (*caPubs = X509_chain_up_ref(ctx->caPubsOut)) == NULL)
        goto err;
    if (ctx->statusOut != NULL
            && (si = OSSL_CMP_PKISI_dup(ctx->statusOut)) == NULL)
        goto err;
    return si;

 err:
    X509_free(*certOut);
    *certOut = NULL;
    sk_X509_pop_free(*chainOut, X509_free);
    *chainOut = NULL;
    sk_X509_pop_free(*caPubs, X509_free);
    *caPubs = NULL;
    return NULL;
}

static OSSL_CMP_PKISI *process_rr(OSSL_CMP_SRV_CTX *srv_ctx,
                                  const OSSL_CMP_MSG *rr,
                                  const X509_NAME *issuer,
                                  const ASN1_INTEGER *serial)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);

    if (ctx == NULL || rr == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return NULL;
    }
    if (ctx->certOut == NULL || ctx->sendError == 1
            || ctx->sendError == OSSL_CMP_MSG_get_bodytype(rr)) {
        ERR_raise(ERR_LIB_CMP, CMP_R_ERROR_PROCESSING_MESSAGE);
        return NULL;
    }

    /* Allow any RR derived from CSR, which may include subject and serial */
    if (issuer == NULL || serial == NULL)
        return OSSL_CMP_PKISI_dup(ctx->statusOut);

    /* accept revocation only for the certificate we sent in ir/cr/kur */
    if (X509_NAME_cmp(issuer, X509_get_issuer_name(ctx->certOut)) != 0
            || ASN1_INTEGER_cmp(serial,
                                X509_get0_serialNumber(ctx->certOut)) != 0) {
        ERR_raise_data(ERR_LIB_CMP, CMP_R_REQUEST_NOT_ACCEPTED,
                       "wrong certificate to revoke");
        return NULL;
    }
    return OSSL_CMP_PKISI_dup(ctx->statusOut);
}

static int process_genm(OSSL_CMP_SRV_CTX *srv_ctx,
                        const OSSL_CMP_MSG *genm,
                        const STACK_OF(OSSL_CMP_ITAV) *in,
                        STACK_OF(OSSL_CMP_ITAV) **out)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);

    if (ctx == NULL || genm == NULL || in == NULL || out == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return 0;
    }
    if (ctx->sendError == 1
            || ctx->sendError == OSSL_CMP_MSG_get_bodytype(genm)
            || sk_OSSL_CMP_ITAV_num(in) > 1) {
        ERR_raise(ERR_LIB_CMP, CMP_R_ERROR_PROCESSING_MESSAGE);
        return 0;
    }

    *out = sk_OSSL_CMP_ITAV_deep_copy(in, OSSL_CMP_ITAV_dup,
                                      OSSL_CMP_ITAV_free);
    return *out != NULL;
}

static void process_error(OSSL_CMP_SRV_CTX *srv_ctx, const OSSL_CMP_MSG *error,
                          const OSSL_CMP_PKISI *statusInfo,
                          const ASN1_INTEGER *errorCode,
                          const OSSL_CMP_PKIFREETEXT *errorDetails)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);
    char buf[OSSL_CMP_PKISI_BUFLEN];
    char *sibuf;
    int i;

    if (ctx == NULL || error == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return;
    }

    BIO_printf(bio_err, "mock server received error:\n");

    if (statusInfo == NULL) {
        BIO_printf(bio_err, "pkiStatusInfo absent\n");
    } else {
        sibuf = OSSL_CMP_snprint_PKIStatusInfo(statusInfo, buf, sizeof(buf));
        BIO_printf(bio_err, "pkiStatusInfo: %s\n",
                   sibuf != NULL ? sibuf: "<invalid>");
    }

    if (errorCode == NULL)
        BIO_printf(bio_err, "errorCode absent\n");
    else
        BIO_printf(bio_err, "errorCode: %ld\n", ASN1_INTEGER_get(errorCode));

    if (sk_ASN1_UTF8STRING_num(errorDetails) <= 0) {
        BIO_printf(bio_err, "errorDetails absent\n");
    } else {
        BIO_printf(bio_err, "errorDetails: ");
        for (i = 0; i < sk_ASN1_UTF8STRING_num(errorDetails); i++) {
            if (i > 0)
                BIO_printf(bio_err, ", ");
            BIO_printf(bio_err, "\"");
            ASN1_STRING_print(bio_err,
                              sk_ASN1_UTF8STRING_value(errorDetails, i));
            BIO_printf(bio_err, "\"");
        }
        BIO_printf(bio_err, "\n");
    }
}

static int process_certConf(OSSL_CMP_SRV_CTX *srv_ctx,
                            const OSSL_CMP_MSG *certConf,
                            ossl_unused int certReqId,
                            const ASN1_OCTET_STRING *certHash,
                            const OSSL_CMP_PKISI *si)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);
    ASN1_OCTET_STRING *digest;

    if (ctx == NULL || certConf == NULL || certHash == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return 0;
    }
    if (ctx->sendError == 1
            || ctx->sendError == OSSL_CMP_MSG_get_bodytype(certConf)
            || ctx->certOut == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_ERROR_PROCESSING_MESSAGE);
        return 0;
    }

    if ((digest = X509_digest_sig(ctx->certOut, NULL, NULL)) == NULL)
        return 0;
    if (ASN1_OCTET_STRING_cmp(certHash, digest) != 0) {
        ASN1_OCTET_STRING_free(digest);
        ERR_raise(ERR_LIB_CMP, CMP_R_CERTHASH_UNMATCHED);
        return 0;
    }
    ASN1_OCTET_STRING_free(digest);
    return 1;
}

static int process_pollReq(OSSL_CMP_SRV_CTX *srv_ctx,
                           const OSSL_CMP_MSG *pollReq,
                           ossl_unused int certReqId,
                           OSSL_CMP_MSG **certReq, int64_t *check_after)
{
    mock_srv_ctx *ctx = OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx);

    if (ctx == NULL || pollReq == NULL
            || certReq == NULL || check_after == NULL) {
        ERR_raise(ERR_LIB_CMP, CMP_R_NULL_ARGUMENT);
        return 0;
    }
    if (ctx->sendError == 1
            || ctx->sendError == OSSL_CMP_MSG_get_bodytype(pollReq)) {
        *certReq = NULL;
        ERR_raise(ERR_LIB_CMP, CMP_R_ERROR_PROCESSING_MESSAGE);
        return 0;
    }
    if (ctx->certReq == NULL) {
        /* not currently in polling mode */
        *certReq = NULL;
        ERR_raise(ERR_LIB_CMP, CMP_R_UNEXPECTED_PKIBODY);
        return 0;
    }

    if (++ctx->curr_pollCount >= ctx->pollCount) {
        /* end polling */
        *certReq = ctx->certReq;
        ctx->certReq = NULL;
        *check_after = 0;
    } else {
        *certReq = NULL;
        *check_after = ctx->checkAfterTime;
    }
    return 1;
}

OSSL_CMP_SRV_CTX *ossl_cmp_mock_srv_new(OSSL_LIB_CTX *libctx, const char *propq)
{
    OSSL_CMP_SRV_CTX *srv_ctx = OSSL_CMP_SRV_CTX_new(libctx, propq);
    mock_srv_ctx *ctx = mock_srv_ctx_new();

    if (srv_ctx != NULL && ctx != NULL
            && OSSL_CMP_SRV_CTX_init(srv_ctx, ctx, process_cert_request,
                                     process_rr, process_genm, process_error,
                                     process_certConf, process_pollReq))
        return srv_ctx;

    mock_srv_ctx_free(ctx);
    OSSL_CMP_SRV_CTX_free(srv_ctx);
    return NULL;
}

void ossl_cmp_mock_srv_free(OSSL_CMP_SRV_CTX *srv_ctx)
{
    if (srv_ctx != NULL)
        mock_srv_ctx_free(OSSL_CMP_SRV_CTX_get0_custom_ctx(srv_ctx));
    OSSL_CMP_SRV_CTX_free(srv_ctx);
}
