/* crypto/engine/hw_cluster_labs.c */
/*
 * Written by Jan Tschirschwitz (jan.tschirschwitz@cluster-labs.com for the
 * OpenSSL project 2000.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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

#define MSC_VER                 /* only used cryptic.h */

#include <stdio.h>
#include <openssl/crypto.h>
#include <openssl/dso.h>
#include <openssl/des.h>
#include <openssl/engine.h>

#ifndef NO_HW
# ifndef NO_HW_CLUSTER_LABS

#  ifdef FLAT_INC
#   include "cluster_labs.h"
#  else
#   include "vendor_defns/cluster_labs.h"
#  endif

#  define CL_LIB_NAME "cluster_labs engine"
#  include "hw_cluster_labs_err.c"

static int cluster_labs_destroy(ENGINE *e);
static int cluster_labs_init(ENGINE *e);
static int cluster_labs_finish(ENGINE *e);
static int cluster_labs_ctrl(ENGINE *e, int cmd, long i, void *p,
                             void (*f) ());

/* BIGNUM stuff */
/* This function is aliased to mod_exp (with the mont stuff dropped). */
static int cluster_labs_mod_exp_mont(BIGNUM *r, const BIGNUM *a,
                                     const BIGNUM *p, const BIGNUM *m,
                                     BN_CTX *ctx, BN_MONT_CTX *m_ctx);

/* RSA stuff */
#  ifndef OPENSSL_NO_RSA
static int cluster_labs_rsa_pub_enc(int flen, const unsigned char *from,
                                    unsigned char *to, RSA *rsa, int padding);
static int cluster_labs_rsa_pub_dec(int flen, const unsigned char *from,
                                    unsigned char *to, RSA *rsa, int padding);
static int cluster_labs_rsa_priv_enc(int flen, const unsigned char *from,
                                     unsigned char *to, RSA *rsa,
                                     int padding);
static int cluster_labs_rsa_priv_dec(int flen, const unsigned char *from,
                                     unsigned char *to, RSA *rsa,
                                     int padding);
static int cluster_labs_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa);
#  endif

/* DSA stuff */
#  ifndef OPENSSL_NO_DSA
static DSA_SIG *cluster_labs_dsa_sign(const unsigned char *dgst, int dlen,
                                      DSA *dsa);
static int cluster_labs_dsa_verify(const unsigned char *dgst, int dgst_len,
                                   DSA_SIG *sig, DSA *dsa);
static int cluster_labs_dsa_mod_exp(DSA *dsa, BIGNUM *rr, BIGNUM *a1,
                                    BIGNUM *p1, BIGNUM *a2, BIGNUM *p2,
                                    BIGNUM *m, BN_CTX *ctx,
                                    BN_MONT_CTX *in_mont);
static int cluster_labs_mod_exp_dsa(DSA *dsa, BIGNUM *r, BIGNUM *a,
                                    const BIGNUM *p, const BIGNUM *m,
                                    BN_CTX *ctx, BN_MONT_CTX *m_ctx);
#  endif

/* DH stuff */
#  ifndef OPENSSL_NO_DH
/* This function is alised to mod_exp (with the DH and mont dropped). */
static int cluster_labs_mod_exp_dh(const DH *dh, BIGNUM *r, const BIGNUM *a,
                                   const BIGNUM *p, const BIGNUM *m,
                                   BN_CTX *ctx, BN_MONT_CTX *m_ctx);
#  endif

/* RANDOM stuff */
static int cluster_labs_rand_bytes(unsigned char *buf, int num);

/* The definitions for control commands specific to this engine */
#  define CLUSTER_LABS_CMD_SO_PATH                ENGINE_CMD_BASE
static const ENGINE_CMD_DEFN cluster_labs_cmd_defns[] = {
    {CLUSTER_LABS_CMD_SO_PATH,
     "SO_PATH",
     "Specifies the path to the 'cluster labs' shared library",
     ENGINE_CMD_FLAG_STRING},
    {0, NULL, NULL, 0}
};

/* Our internal RSA_METHOD that we provide pointers to */
#  ifndef OPENSSL_NO_RSA
static RSA_METHOD cluster_labs_rsa = {
    "Cluster Labs RSA method",
    cluster_labs_rsa_pub_enc,   /* rsa_pub_enc */
    cluster_labs_rsa_pub_dec,   /* rsa_pub_dec */
    cluster_labs_rsa_priv_enc,  /* rsa_priv_enc */
    cluster_labs_rsa_priv_dec,  /* rsa_priv_dec */
    cluster_labs_rsa_mod_exp,   /* rsa_mod_exp */
    cluster_labs_mod_exp_mont,  /* bn_mod_exp */
    NULL,                       /* init */
    NULL,                       /* finish */
    0,                          /* flags */
    NULL,                       /* apps_data */
    NULL,                       /* rsa_sign */
    NULL                        /* rsa_verify */
};
#  endif

/* Our internal DSA_METHOD that we provide pointers to */
#  ifndef OPENSSL_NO_DSA
static DSA_METHOD cluster_labs_dsa = {
    "Cluster Labs DSA method",
    cluster_labs_dsa_sign,      /* dsa_do_sign */
    NULL,                       /* dsa_sign_setup */
    cluster_labs_dsa_verify,    /* dsa_do_verify */
    cluster_labs_dsa_mod_exp,   /* dsa_mod_exp */
    cluster_labs_mod_exp_dsa,   /* bn_mod_exp */
    NULL,                       /* init */
    NULL,                       /* finish */
    0,                          /* flags */
    NULL                        /* app_data */
};
#  endif

/* Our internal DH_METHOD that we provide pointers to */
#  ifndef OPENSSL_NO_DH
static DH_METHOD cluster_labs_dh = {
    "Cluster Labs DH method",
    NULL,                       /* generate key */
    NULL,                       /* compute key */
    cluster_labs_mod_exp_dh,    /* bn_mod_exp */
    NULL,                       /* init */
    NULL,                       /* finish */
    0,                          /* flags */
    NULL                        /* app_data */
};
#  endif

static RAND_METHOD cluster_labs_rand = {
    /* "Cluster Labs RAND method", */
    NULL,                       /* seed */
    cluster_labs_rand_bytes,    /* bytes */
    NULL,                       /* cleanup */
    NULL,                       /* add */
    cluster_labs_rand_bytes,    /* pseudorand */
    NULL,                       /* status */
};

static const char *engine_cluster_labs_id = "cluster_labs";
static const char *engine_cluster_labs_name =
    "Cluster Labs hardware engine support";

/* engine implementation */
/* ---------------------*/
static int bind_helper(ENGINE *e)
{

    if (!ENGINE_set_id(e, engine_cluster_labs_id) ||
        !ENGINE_set_name(e, engine_cluster_labs_name) ||
#  ifndef OPENSSL_NO_RSA
        !ENGINE_set_RSA(e, &cluster_labs_rsa) ||
#  endif
#  ifndef OPENSSL_NO_DSA
        !ENGINE_set_DSA(e, &cluster_labs_dsa) ||
#  endif
#  ifndef OPENSSL_NO_DH
        !ENGINE_set_DH(e, &cluster_labs_dh) ||
#  endif
        !ENGINE_set_RAND(e, &cluster_labs_rand) ||
        !ENGINE_set_destroy_function(e, cluster_labs_destroy) ||
        !ENGINE_set_init_function(e, cluster_labs_init) ||
        !ENGINE_set_finish_function(e, cluster_labs_finish) ||
        !ENGINE_set_ctrl_function(e, cluster_labs_ctrl) ||
        !ENGINE_set_cmd_defns(e, cluster_labs_cmd_defns))
        return 0;
    /* Ensure the error handling is set up */
    ERR_load_CL_strings();
    return 1;
}

#  ifndef ENGINE_DYNAMIC_SUPPORT
static ENGINE *engine_cluster_labs(void)
{
    ENGINE *ret = ENGINE_new();

    if (!ret)
        return NULL;
    if (!bind_helper(ret)) {
        ENGINE_free(ret);
        return NULL;
    }
    return ret;
}

#   ifdef ENGINE_DYNAMIC_SUPPORT
static
#   endif
void ENGINE_load_cluster_labs(void)
{

    ENGINE *cluster_labs = engine_cluster_labs();

    if (!cluster_labs)
        return;
    ENGINE_add(cluster_labs);
    ENGINE_free(cluster_labs);
    ERR_clear_error();
}
#  endif                        /* !ENGINE_DYNAMIC_SUPPORT */

static int cluster_labs_destroy(ENGINE *e)
{

    ERR_unload_CL_strings();
    return 1;
}

/*
 * This is a process-global DSO handle used for loading and unloading the
 * Cluster Labs library. NB: This is only set (or unset) during an init() or
 * finish() call (reference counts permitting) and they're operating with
 * global locks, so this should be thread-safe implicitly.
 */
static DSO *cluster_labs_dso = NULL;

/*
 * These are the function pointers that are (un)set when the library has
 * successfully (un)loaded.
 */
static cl_engine_init *p_cl_engine_init = NULL;
static cl_mod_exp *p_cl_mod_exp = NULL;
static cl_mod_exp_crt *p_cl_mod_exp_crt = NULL;
static cl_rsa_mod_exp *p_cl_rsa_mod_exp = NULL;
static cl_rsa_priv_enc *p_cl_rsa_priv_enc = NULL;
static cl_rsa_priv_dec *p_cl_rsa_priv_dec = NULL;
static cl_rsa_pub_enc *p_cl_rsa_pub_enc = NULL;
static cl_rsa_pub_dec *p_cl_rsa_pub_dec = NULL;
static cl_rand_bytes *p_cl_rand_bytes = NULL;
static cl_dsa_sign *p_cl_dsa_sign = NULL;
static cl_dsa_verify *p_cl_dsa_verify = NULL;

int cluster_labs_init(ENGINE *e)
{

    cl_engine_init *p1;
    cl_mod_exp *p2;
    cl_mod_exp_crt *p3;
    cl_rsa_mod_exp *p4;
    cl_rsa_priv_enc *p5;
    cl_rsa_priv_dec *p6;
    cl_rsa_pub_enc *p7;
    cl_rsa_pub_dec *p8;
    cl_rand_bytes *p20;
    cl_dsa_sign *p30;
    cl_dsa_verify *p31;

    /* engine already loaded */
    if (cluster_labs_dso != NULL) {
        CLerr(CL_F_CLUSTER_LABS_INIT, CL_R_ALREADY_LOADED);
        goto err;
    }
    /* try to load engine    */
    cluster_labs_dso = DSO_load(NULL, CLUSTER_LABS_LIB_NAME, NULL, 0);
    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_INIT, CL_R_DSO_FAILURE);
        goto err;
    }
    /* bind functions */
    if (!
        (p1 =
         (cl_engine_init *) DSO_bind_func(cluster_labs_dso, CLUSTER_LABS_F1))
|| !(p2 = (cl_mod_exp *) DSO_bind_func(cluster_labs_dso, CLUSTER_LABS_F2))
|| !(p3 = (cl_mod_exp_crt *) DSO_bind_func(cluster_labs_dso, CLUSTER_LABS_F3))
|| !(p4 = (cl_rsa_mod_exp *) DSO_bind_func(cluster_labs_dso, CLUSTER_LABS_F4))
|| !(p5 =
     (cl_rsa_priv_enc *) DSO_bind_func(cluster_labs_dso, CLUSTER_LABS_F5))
|| !(p6 =
     (cl_rsa_priv_dec *) DSO_bind_func(cluster_labs_dso, CLUSTER_LABS_F6))
|| !(p7 = (cl_rsa_pub_enc *) DSO_bind_func(cluster_labs_dso, CLUSTER_LABS_F7))
|| !(p8 = (cl_rsa_pub_dec *) DSO_bind_func(cluster_labs_dso, CLUSTER_LABS_F8))
|| !(p20 =
     (cl_rand_bytes *) DSO_bind_func(cluster_labs_dso, CLUSTER_LABS_F20))
|| !(p30 = (cl_dsa_sign *) DSO_bind_func(cluster_labs_dso, CLUSTER_LABS_F30))
|| !(p31 =
     (cl_dsa_verify *) DSO_bind_func(cluster_labs_dso, CLUSTER_LABS_F31))) {
        CLerr(CL_F_CLUSTER_LABS_INIT, CL_R_DSO_FAILURE);
        goto err;
    }

    /* copy function pointers */
    p_cl_engine_init = p1;
    p_cl_mod_exp = p2;
    p_cl_mod_exp_crt = p3;
    p_cl_rsa_mod_exp = p4;
    p_cl_rsa_priv_enc = p5;
    p_cl_rsa_priv_dec = p6;
    p_cl_rsa_pub_enc = p7;
    p_cl_rsa_pub_dec = p8;
    p_cl_rand_bytes = p20;
    p_cl_dsa_sign = p30;
    p_cl_dsa_verify = p31;

    /* cluster labs engine init */
    if (p_cl_engine_init() == 0) {
        CLerr(CL_F_CLUSTER_LABS_INIT, CL_R_INIT_FAILED);
        goto err;
    }

    return (1);

 err:
    /* reset all pointers */
    if (cluster_labs_dso)
        DSO_free(cluster_labs_dso);

    cluster_labs_dso = NULL;
    p_cl_engine_init = NULL;
    p_cl_mod_exp = NULL;
    p_cl_mod_exp_crt = NULL;
    p_cl_rsa_mod_exp = NULL;
    p_cl_rsa_priv_enc = NULL;
    p_cl_rsa_priv_dec = NULL;
    p_cl_rsa_pub_enc = NULL;
    p_cl_rsa_pub_dec = NULL;
    p_cl_rand_bytes = NULL;
    p_cl_dsa_sign = NULL;
    p_cl_dsa_verify = NULL;

    return (0);
}

static int cluster_labs_finish(ENGINE *e)
{

    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_FINISH, CL_R_NOT_LOADED);
        return 0;
    }
    if (!DSO_free(cluster_labs_dso)) {
        CLerr(CL_F_CLUSTER_LABS_FINISH, CL_R_DSO_FAILURE);
        return 0;
    }

    cluster_labs_dso = NULL;
    p_cl_engine_init = NULL;
    p_cl_mod_exp = NULL;
    p_cl_rsa_mod_exp = NULL;
    p_cl_mod_exp_crt = NULL;
    p_cl_rsa_priv_enc = NULL;
    p_cl_rsa_priv_dec = NULL;
    p_cl_rsa_pub_enc = NULL;
    p_cl_rsa_pub_dec = NULL;
    p_cl_rand_bytes = NULL;
    p_cl_dsa_sign = NULL;
    p_cl_dsa_verify = NULL;

    return (1);

}

static int cluster_labs_ctrl(ENGINE *e, int cmd, long i, void *p,
                             void (*f) ())
{
    int initialised = ((cluster_labs_dso == NULL) ? 0 : 1);

    switch (cmd) {
    case CLUSTER_LABS_CMD_SO_PATH:
        if (p == NULL) {
            CLerr(CL_F_CLUSTER_LABS_CTRL, ERR_R_PASSED_NULL_PARAMETER);
            return 0;
        }
        if (initialised) {
            CLerr(CL_F_CLUSTER_LABS_CTRL, CL_R_ALREADY_LOADED);
            return 0;
        }
        CLUSTER_LABS_LIB_NAME = (const char *)p;
        return 1;
    default:
        break;
    }
    CLerr(CL_F_CLUSTER_LABS_CTRL, CL_R_COMMAND_NOT_IMPLEMENTED);
    return 0;
}

static int cluster_labs_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                                const BIGNUM *m, BN_CTX *ctx)
{

    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_MOD_EXP, CL_R_NOT_LOADED);
        return 0;
    }
    if (p_cl_mod_exp == NULL) {
        CLerr(CL_F_CLUSTER_LABS_MOD_EXP, CL_R_FUNCTION_NOT_BINDED);
        return 0;
    }

    return p_cl_mod_exp(r, a, p, m, ctx);

}

static int cluster_labs_mod_exp_crt(BIGNUM *r, BIGNUM *a, const BIGNUM *p,
                                    const BIGNUM *q, const BIGNUM *dmp1,
                                    const BIGNUM *dmq1, const BIGNUM *iqmp,
                                    BN_CTX *ctx)
{

    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_MOD_EXP_CRT, CL_R_NOT_LOADED);
        return 0;
    }
    if (p_cl_mod_exp_crt == NULL) {
        CLerr(CL_F_CLUSTER_LABS_MOD_EXP_CRT, CL_R_FUNCTION_NOT_BINDED);
        return 0;
    }

    return p_cl_mod_exp_crt(r, a, p, q, dmp1, dmq1, iqmp, ctx);

}

static int cluster_labs_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa)
{

    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RSA_MOD_EXP, CL_R_NOT_LOADED);
        return 0;
    }
    if (p_cl_rsa_mod_exp == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RSA_MOD_EXP, CL_R_FUNCTION_NOT_BINDED);
        return 0;
    }

    return p_cl_rsa_mod_exp(r0, I, rsa);

}

static DSA_SIG *cluster_labs_dsa_sign(const unsigned char *dgst, int dlen,
                                      DSA *dsa)
{

    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_DSA_SIGN, CL_R_NOT_LOADED);
        return 0;
    }
    if (p_cl_dsa_sign == NULL) {
        CLerr(CL_F_CLUSTER_LABS_DSA_SIGN, CL_R_FUNCTION_NOT_BINDED);
        return 0;
    }

    return p_cl_dsa_sign(dgst, dlen, dsa);

}

static int cluster_labs_dsa_verify(const unsigned char *dgst, int dgst_len,
                                   DSA_SIG *sig, DSA *dsa)
{

    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_DSA_VERIFY, CL_R_NOT_LOADED);
        return 0;
    }

    if (p_cl_dsa_verify == NULL) {
        CLerr(CL_F_CLUSTER_LABS_DSA_VERIFY, CL_R_FUNCTION_NOT_BINDED);
        return 0;
    }

    return p_cl_dsa_verify(dgst, dgst_len, sig, dsa);

}

static int cluster_labs_dsa_mod_exp(DSA *dsa, BIGNUM *rr, BIGNUM *a1,
                                    BIGNUM *p1, BIGNUM *a2, BIGNUM *p2,
                                    BIGNUM *m, BN_CTX *ctx,
                                    BN_MONT_CTX *in_mont)
{
    BIGNUM t;
    int status = 0;

    BN_init(&t);
    /* let rr = a1 ^ p1 mod m */
    if (!cluster_labs_mod_exp(rr, a1, p1, m, ctx))
        goto end;
    /* let t = a2 ^ p2 mod m */
    if (!cluster_labs_mod_exp(&t, a2, p2, m, ctx))
        goto end;
    /* let rr = rr * t mod m */
    if (!BN_mod_mul(rr, rr, &t, m, ctx))
        goto end;
    status = 1;
 end:
    BN_free(&t);

    return (1);

}

static int cluster_labs_mod_exp_dsa(DSA *dsa, BIGNUM *r, BIGNUM *a,
                                    const BIGNUM *p, const BIGNUM *m,
                                    BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    return cluster_labs_mod_exp(r, a, p, m, ctx);
}

/* This function is aliased to mod_exp (with the mont stuff dropped). */
static int cluster_labs_mod_exp_mont(BIGNUM *r, const BIGNUM *a,
                                     const BIGNUM *p, const BIGNUM *m,
                                     BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    return cluster_labs_mod_exp(r, a, p, m, ctx);
}

/* This function is aliased to mod_exp (with the dh and mont dropped). */
static int cluster_labs_mod_exp_dh(const DH *dh, BIGNUM *r, const BIGNUM *a,
                                   const BIGNUM *p, const BIGNUM *m,
                                   BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    return cluster_labs_mod_exp(r, a, p, m, ctx);
}

static int cluster_labs_rsa_pub_enc(int flen, const unsigned char *from,
                                    unsigned char *to, RSA *rsa, int padding)
{

    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RSA_PUB_ENC, CL_R_NOT_LOADED);
        return 0;
    }
    if (p_cl_rsa_priv_enc == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RSA_PUB_ENC, CL_R_FUNCTION_NOT_BINDED);
        return 0;
    }

    return p_cl_rsa_pub_enc(flen, from, to, rsa, padding);

}

static int cluster_labs_rsa_pub_dec(int flen, const unsigned char *from,
                                    unsigned char *to, RSA *rsa, int padding)
{

    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RSA_PUB_DEC, CL_R_NOT_LOADED);
        return 0;
    }
    if (p_cl_rsa_priv_enc == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RSA_PUB_DEC, CL_R_FUNCTION_NOT_BINDED);
        return 0;
    }

    return p_cl_rsa_pub_dec(flen, from, to, rsa, padding);

}

static int cluster_labs_rsa_priv_enc(int flen, const unsigned char *from,
                                     unsigned char *to, RSA *rsa, int padding)
{

    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RSA_PRIV_ENC, CL_R_NOT_LOADED);
        return 0;
    }

    if (p_cl_rsa_priv_enc == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RSA_PRIV_ENC, CL_R_FUNCTION_NOT_BINDED);
        return 0;
    }

    return p_cl_rsa_priv_enc(flen, from, to, rsa, padding);

}

static int cluster_labs_rsa_priv_dec(int flen, const unsigned char *from,
                                     unsigned char *to, RSA *rsa, int padding)
{

    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RSA_PRIV_DEC, CL_R_NOT_LOADED);
        return 0;
    }
    if (p_cl_rsa_priv_dec == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RSA_PRIV_DEC, CL_R_FUNCTION_NOT_BINDED);
        return 0;
    }

    return p_cl_rsa_priv_dec(flen, from, to, rsa, padding);

}

/************************************************************************************
* Symmetric algorithms
************************************************************************************/
/* this will be come soon! */

/************************************************************************************
* Random generator
************************************************************************************/

static int cluster_labs_rand_bytes(unsigned char *buf, int num)
{

    if (cluster_labs_dso == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RAND_BYTES, CL_R_NOT_LOADED);
        return 0;
    }
    if (p_cl_mod_exp_crt == NULL) {
        CLerr(CL_F_CLUSTER_LABS_RAND_BYTES, CL_R_FUNCTION_NOT_BINDED);
        return 0;
    }

    return p_cl_rand_bytes(buf, num);

}

/*
 * This stuff is needed if this ENGINE is being compiled into a
 * self-contained shared-library.
 */
#  ifdef ENGINE_DYNAMIC_SUPPORT
static int bind_fn(ENGINE *e, const char *id)
{
    fprintf(stderr, "bind_fn CLUSTER_LABS\n");
    if (id && (strcmp(id, engine_cluster_labs_id) != 0)) {
        fprintf(stderr, "bind_fn return(0) first\n");
        return 0;
    }
    if (!bind_helper(e)) {
        fprintf(stderr, "bind_fn return(1) first\n");
        return 0;
    }
    fprintf(stderr, "bind_fn return(1)\n");
    return 1;
}

IMPLEMENT_DYNAMIC_CHECK_FN()
    IMPLEMENT_DYNAMIC_BIND_FN(bind_fn)
#  endif                        /* ENGINE_DYNAMIC_SUPPORT */
# endif                         /* !NO_HW_CLUSTER_LABS */
#endif                          /* !NO_HW */
