#include "tunala.h"

#ifndef NO_OPENSSL

/* For callbacks generating output, here are their file-descriptors. */
static FILE *fp_cb_ssl_info = NULL;
static FILE *fp_cb_ssl_verify = NULL;
/*-
 * Output level:
 *     0 = nothing,
 *     1 = minimal, just errors,
 *     2 = minimal, all steps,
 *     3 = detail, all steps */
static unsigned int cb_ssl_verify_level = 1;

/* Other static rubbish (to mirror s_cb.c where required) */
static int int_verify_depth = 10;

/*
 * This function is largely borrowed from the one used in OpenSSL's
 * "s_client" and "s_server" utilities.
 */
void cb_ssl_info(const SSL *s, int where, int ret)
{
    const char *str1, *str2;
    int w;

    if (!fp_cb_ssl_info)
        return;

    w = where & ~SSL_ST_MASK;
    str1 = (w & SSL_ST_CONNECT ? "SSL_connect" : (w & SSL_ST_ACCEPT ?
                                                  "SSL_accept" :
                                                  "undefined")), str2 =
        SSL_state_string_long(s);

    if (where & SSL_CB_LOOP)
        fprintf(fp_cb_ssl_info, "(%s) %s\n", str1, str2);
    else if (where & SSL_CB_EXIT) {
        if (ret == 0)
            fprintf(fp_cb_ssl_info, "(%s) failed in %s\n", str1, str2);
        /*
         * In a non-blocking model, we get a few of these "error"s simply
         * because we're calling "reads" and "writes" on the state-machine
         * that are virtual NOPs simply to avoid wasting the time seeing if
         * we *should* call them. Removing this case makes the "-out_state"
         * output a lot easier on the eye.
         */
# if 0
        else if (ret < 0)
            fprintf(fp_cb_ssl_info, "%s:error in %s\n", str1, str2);
# endif
    }
}

void cb_ssl_info_set_output(FILE *fp)
{
    fp_cb_ssl_info = fp;
}

static const char *int_reason_no_issuer =
    "X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT";
static const char *int_reason_not_yet = "X509_V_ERR_CERT_NOT_YET_VALID";
static const char *int_reason_before =
    "X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD";
static const char *int_reason_expired = "X509_V_ERR_CERT_HAS_EXPIRED";
static const char *int_reason_after =
    "X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD";

/* Stolen wholesale from apps/s_cb.c :-) And since then, mutilated ... */
int cb_ssl_verify(int ok, X509_STORE_CTX *ctx)
{
    char buf1[256];             /* Used for the subject name */
    char buf2[256];             /* Used for the issuer name */
    const char *reason = NULL;  /* Error reason (if any) */
    X509 *err_cert;
    int err, depth;

    if (!fp_cb_ssl_verify || (cb_ssl_verify_level == 0))
        return ok;
    err_cert = X509_STORE_CTX_get_current_cert(ctx);
    err = X509_STORE_CTX_get_error(ctx);
    depth = X509_STORE_CTX_get_error_depth(ctx);

    buf1[0] = buf2[0] = '\0';
    /* Fill buf1 */
    X509_NAME_oneline(X509_get_subject_name(err_cert), buf1, 256);
    /* Fill buf2 */
    X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), buf2, 256);
    switch (ctx->error) {
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
        reason = int_reason_no_issuer;
        break;
    case X509_V_ERR_CERT_NOT_YET_VALID:
        reason = int_reason_not_yet;
        break;
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
        reason = int_reason_before;
        break;
    case X509_V_ERR_CERT_HAS_EXPIRED:
        reason = int_reason_expired;
        break;
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
        reason = int_reason_after;
        break;
    }

    if ((cb_ssl_verify_level == 1) && ok)
        return ok;
    fprintf(fp_cb_ssl_verify, "chain-depth=%d, ", depth);
    if (reason)
        fprintf(fp_cb_ssl_verify, "error=%s\n", reason);
    else
        fprintf(fp_cb_ssl_verify, "error=%d\n", err);
    if (cb_ssl_verify_level < 3)
        return ok;
    fprintf(fp_cb_ssl_verify, "--> subject = %s\n", buf1);
    fprintf(fp_cb_ssl_verify, "--> issuer  = %s\n", buf2);
    if (!ok)
        fprintf(fp_cb_ssl_verify, "--> verify error:num=%d:%s\n", err,
                X509_verify_cert_error_string(err));
    fprintf(fp_cb_ssl_verify, "--> verify return:%d\n", ok);
    return ok;
}

void cb_ssl_verify_set_output(FILE *fp)
{
    fp_cb_ssl_verify = fp;
}

void cb_ssl_verify_set_depth(unsigned int verify_depth)
{
    int_verify_depth = verify_depth;
}

void cb_ssl_verify_set_level(unsigned int level)
{
    if (level < 4)
        cb_ssl_verify_level = level;
}

RSA *cb_generate_tmp_rsa(SSL *s, int is_export, int keylength)
{
    /*
     * TODO: Perhaps make it so our global key can be generated on-the-fly
     * after certain intervals?
     */
    static RSA *rsa_tmp = NULL;
    BIGNUM *bn = NULL;
    int ok = 1;
    if (!rsa_tmp) {
        ok = 0;
        if (!(bn = BN_new()))
            goto end;
        if (!BN_set_word(bn, RSA_F4))
            goto end;
        if (!(rsa_tmp = RSA_new()))
            goto end;
        if (!RSA_generate_key_ex(rsa_tmp, keylength, bn, NULL))
            goto end;
        ok = 1;
    }
 end:
    if (bn)
        BN_free(bn);
    if (!ok) {
        RSA_free(rsa_tmp);
        rsa_tmp = NULL;
    }
    return rsa_tmp;
}

#endif                          /* !defined(NO_OPENSSL) */
