/* crypto/engine/hw_atalla.c */
/*
 * Written by Geoff Thorpe (geoff@geoffthorpe.net) for the OpenSSL project
 * 2000.
 */
/* ====================================================================
 * Copyright (c) 1999-2001 The OpenSSL Project.  All rights reserved.
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
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/dso.h>
#include <openssl/engine.h>
#ifndef OPENSSL_NO_RSA
# include <openssl/rsa.h>
#endif
#ifndef OPENSSL_NO_DSA
# include <openssl/dsa.h>
#endif
#ifndef OPENSSL_NO_DH
# include <openssl/dh.h>
#endif
#include <openssl/bn.h>

#ifndef OPENSSL_NO_HW
# ifndef OPENSSL_NO_HW_ATALLA

#  ifdef FLAT_INC
#   include "atalla.h"
#  else
#   include "vendor_defns/atalla.h"
#  endif

#  define ATALLA_LIB_NAME "atalla engine"
#  include "e_atalla_err.c"

static int atalla_destroy(ENGINE *e);
static int atalla_init(ENGINE *e);
static int atalla_finish(ENGINE *e);
static int atalla_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f) (void));

/* BIGNUM stuff */
static int atalla_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                          const BIGNUM *m, BN_CTX *ctx);

#  ifndef OPENSSL_NO_RSA
/* RSA stuff */
static int atalla_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa,
                              BN_CTX *ctx);
/* This function is aliased to mod_exp (with the mont stuff dropped). */
static int atalla_mod_exp_mont(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                               const BIGNUM *m, BN_CTX *ctx,
                               BN_MONT_CTX *m_ctx);
#  endif

#  ifndef OPENSSL_NO_DSA
/* DSA stuff */
static int atalla_dsa_mod_exp(DSA *dsa, BIGNUM *rr, BIGNUM *a1,
                              BIGNUM *p1, BIGNUM *a2, BIGNUM *p2, BIGNUM *m,
                              BN_CTX *ctx, BN_MONT_CTX *in_mont);
static int atalla_mod_exp_dsa(DSA *dsa, BIGNUM *r, BIGNUM *a,
                              const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
                              BN_MONT_CTX *m_ctx);
#  endif

#  ifndef OPENSSL_NO_DH
/* DH stuff */
/* This function is alised to mod_exp (with the DH and mont dropped). */
static int atalla_mod_exp_dh(const DH *dh, BIGNUM *r,
                             const BIGNUM *a, const BIGNUM *p,
                             const BIGNUM *m, BN_CTX *ctx,
                             BN_MONT_CTX *m_ctx);
#  endif

/* The definitions for control commands specific to this engine */
#  define ATALLA_CMD_SO_PATH              ENGINE_CMD_BASE
static const ENGINE_CMD_DEFN atalla_cmd_defns[] = {
    {ATALLA_CMD_SO_PATH,
     "SO_PATH",
     "Specifies the path to the 'atasi' shared library",
     ENGINE_CMD_FLAG_STRING},
    {0, NULL, NULL, 0}
};

#  ifndef OPENSSL_NO_RSA
/* Our internal RSA_METHOD that we provide pointers to */
static RSA_METHOD atalla_rsa = {
    "Atalla RSA method",
    NULL,
    NULL,
    NULL,
    NULL,
    atalla_rsa_mod_exp,
    atalla_mod_exp_mont,
    NULL,
    NULL,
    0,
    NULL,
    NULL,
    NULL,
    NULL
};
#  endif

#  ifndef OPENSSL_NO_DSA
/* Our internal DSA_METHOD that we provide pointers to */
static DSA_METHOD atalla_dsa = {
    "Atalla DSA method",
    NULL,                       /* dsa_do_sign */
    NULL,                       /* dsa_sign_setup */
    NULL,                       /* dsa_do_verify */
    atalla_dsa_mod_exp,         /* dsa_mod_exp */
    atalla_mod_exp_dsa,         /* bn_mod_exp */
    NULL,                       /* init */
    NULL,                       /* finish */
    0,                          /* flags */
    NULL,                       /* app_data */
    NULL,                       /* dsa_paramgen */
    NULL                        /* dsa_keygen */
};
#  endif

#  ifndef OPENSSL_NO_DH
/* Our internal DH_METHOD that we provide pointers to */
static DH_METHOD atalla_dh = {
    "Atalla DH method",
    NULL,
    NULL,
    atalla_mod_exp_dh,
    NULL,
    NULL,
    0,
    NULL,
    NULL
};
#  endif

/* Constants used when creating the ENGINE */
static const char *engine_atalla_id = "atalla";
static const char *engine_atalla_name = "Atalla hardware engine support";

/*
 * This internal function is used by ENGINE_atalla() and possibly by the
 * "dynamic" ENGINE support too
 */
static int bind_helper(ENGINE *e)
{
#  ifndef OPENSSL_NO_RSA
    const RSA_METHOD *meth1;
#  endif
#  ifndef OPENSSL_NO_DSA
    const DSA_METHOD *meth2;
#  endif
#  ifndef OPENSSL_NO_DH
    const DH_METHOD *meth3;
#  endif
    if (!ENGINE_set_id(e, engine_atalla_id) ||
        !ENGINE_set_name(e, engine_atalla_name) ||
#  ifndef OPENSSL_NO_RSA
        !ENGINE_set_RSA(e, &atalla_rsa) ||
#  endif
#  ifndef OPENSSL_NO_DSA
        !ENGINE_set_DSA(e, &atalla_dsa) ||
#  endif
#  ifndef OPENSSL_NO_DH
        !ENGINE_set_DH(e, &atalla_dh) ||
#  endif
        !ENGINE_set_destroy_function(e, atalla_destroy) ||
        !ENGINE_set_init_function(e, atalla_init) ||
        !ENGINE_set_finish_function(e, atalla_finish) ||
        !ENGINE_set_ctrl_function(e, atalla_ctrl) ||
        !ENGINE_set_cmd_defns(e, atalla_cmd_defns))
        return 0;

#  ifndef OPENSSL_NO_RSA
    /*
     * We know that the "PKCS1_SSLeay()" functions hook properly to the
     * atalla-specific mod_exp and mod_exp_crt so we use those functions. NB:
     * We don't use ENGINE_openssl() or anything "more generic" because
     * something like the RSAref code may not hook properly, and if you own
     * one of these cards then you have the right to do RSA operations on it
     * anyway!
     */
    meth1 = RSA_PKCS1_SSLeay();
    atalla_rsa.rsa_pub_enc = meth1->rsa_pub_enc;
    atalla_rsa.rsa_pub_dec = meth1->rsa_pub_dec;
    atalla_rsa.rsa_priv_enc = meth1->rsa_priv_enc;
    atalla_rsa.rsa_priv_dec = meth1->rsa_priv_dec;
#  endif

#  ifndef OPENSSL_NO_DSA
    /*
     * Use the DSA_OpenSSL() method and just hook the mod_exp-ish bits.
     */
    meth2 = DSA_OpenSSL();
    atalla_dsa.dsa_do_sign = meth2->dsa_do_sign;
    atalla_dsa.dsa_sign_setup = meth2->dsa_sign_setup;
    atalla_dsa.dsa_do_verify = meth2->dsa_do_verify;
#  endif

#  ifndef OPENSSL_NO_DH
    /* Much the same for Diffie-Hellman */
    meth3 = DH_OpenSSL();
    atalla_dh.generate_key = meth3->generate_key;
    atalla_dh.compute_key = meth3->compute_key;
#  endif

    /* Ensure the atalla error handling is set up */
    ERR_load_ATALLA_strings();
    return 1;
}

#  ifdef OPENSSL_NO_DYNAMIC_ENGINE
static ENGINE *engine_atalla(void)
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

void ENGINE_load_atalla(void)
{
    /* Copied from eng_[openssl|dyn].c */
    ENGINE *toadd = engine_atalla();
    if (!toadd)
        return;
    ENGINE_add(toadd);
    ENGINE_free(toadd);
    ERR_clear_error();
}
#  endif

/*
 * This is a process-global DSO handle used for loading and unloading the
 * Atalla library. NB: This is only set (or unset) during an init() or
 * finish() call (reference counts permitting) and they're operating with
 * global locks, so this should be thread-safe implicitly.
 */
static DSO *atalla_dso = NULL;

/*
 * These are the function pointers that are (un)set when the library has
 * successfully (un)loaded.
 */
static tfnASI_GetHardwareConfig *p_Atalla_GetHardwareConfig = NULL;
static tfnASI_RSAPrivateKeyOpFn *p_Atalla_RSAPrivateKeyOpFn = NULL;
static tfnASI_GetPerformanceStatistics *p_Atalla_GetPerformanceStatistics =
    NULL;

/*
 * These are the static string constants for the DSO file name and the
 * function symbol names to bind to. Regrettably, the DSO name on *nix
 * appears to be "atasi.so" rather than something more consistent like
 * "libatasi.so". At the time of writing, I'm not sure what the file name on
 * win32 is but clearly native name translation is not possible (eg
 * libatasi.so on *nix, and atasi.dll on win32). For the purposes of testing,
 * I have created a symbollic link called "libatasi.so" so that we can use
 * native name-translation - a better solution will be needed.
 */
static const char *ATALLA_LIBNAME = NULL;
static const char *get_ATALLA_LIBNAME(void)
{
    if (ATALLA_LIBNAME)
        return ATALLA_LIBNAME;
    return "atasi";
}

static void free_ATALLA_LIBNAME(void)
{
    if (ATALLA_LIBNAME)
        OPENSSL_free((void *)ATALLA_LIBNAME);
    ATALLA_LIBNAME = NULL;
}

static long set_ATALLA_LIBNAME(const char *name)
{
    free_ATALLA_LIBNAME();
    return (((ATALLA_LIBNAME = BUF_strdup(name)) != NULL) ? 1 : 0);
}

static const char *ATALLA_F1 = "ASI_GetHardwareConfig";
static const char *ATALLA_F2 = "ASI_RSAPrivateKeyOpFn";
static const char *ATALLA_F3 = "ASI_GetPerformanceStatistics";

/* Destructor (complements the "ENGINE_atalla()" constructor) */
static int atalla_destroy(ENGINE *e)
{
    free_ATALLA_LIBNAME();
    /*
     * Unload the atalla error strings so any error state including our
     * functs or reasons won't lead to a segfault (they simply get displayed
     * without corresponding string data because none will be found).
     */
    ERR_unload_ATALLA_strings();
    return 1;
}

/* (de)initialisation functions. */
static int atalla_init(ENGINE *e)
{
    tfnASI_GetHardwareConfig *p1;
    tfnASI_RSAPrivateKeyOpFn *p2;
    tfnASI_GetPerformanceStatistics *p3;
    /*
     * Not sure of the origin of this magic value, but Ben's code had it and
     * it seemed to have been working for a few people. :-)
     */
    unsigned int config_buf[1024];

    if (atalla_dso != NULL) {
        ATALLAerr(ATALLA_F_ATALLA_INIT, ATALLA_R_ALREADY_LOADED);
        goto err;
    }
    /*
     * Attempt to load libatasi.so/atasi.dll/whatever. Needs to be changed
     * unfortunately because the Atalla drivers don't have standard library
     * names that can be platform-translated well.
     */
    /*
     * TODO: Work out how to actually map to the names the Atalla drivers
     * really use - for now a symbollic link needs to be created on the host
     * system from libatasi.so to atasi.so on unix variants.
     */
    atalla_dso = DSO_load(NULL, get_ATALLA_LIBNAME(), NULL, 0);
    if (atalla_dso == NULL) {
        ATALLAerr(ATALLA_F_ATALLA_INIT, ATALLA_R_NOT_LOADED);
        goto err;
    }
    if (!
        (p1 =
         (tfnASI_GetHardwareConfig *) DSO_bind_func(atalla_dso, ATALLA_F1))
|| !(p2 = (tfnASI_RSAPrivateKeyOpFn *) DSO_bind_func(atalla_dso, ATALLA_F2))
|| !(p3 =
     (tfnASI_GetPerformanceStatistics *) DSO_bind_func(atalla_dso,
                                                       ATALLA_F3))) {
        ATALLAerr(ATALLA_F_ATALLA_INIT, ATALLA_R_NOT_LOADED);
        goto err;
    }
    /* Copy the pointers */
    p_Atalla_GetHardwareConfig = p1;
    p_Atalla_RSAPrivateKeyOpFn = p2;
    p_Atalla_GetPerformanceStatistics = p3;
    /*
     * Perform a basic test to see if there's actually any unit running.
     */
    if (p1(0L, config_buf) != 0) {
        ATALLAerr(ATALLA_F_ATALLA_INIT, ATALLA_R_UNIT_FAILURE);
        goto err;
    }
    /* Everything's fine. */
    return 1;
 err:
    if (atalla_dso)
        DSO_free(atalla_dso);
    atalla_dso = NULL;
    p_Atalla_GetHardwareConfig = NULL;
    p_Atalla_RSAPrivateKeyOpFn = NULL;
    p_Atalla_GetPerformanceStatistics = NULL;
    return 0;
}

static int atalla_finish(ENGINE *e)
{
    free_ATALLA_LIBNAME();
    if (atalla_dso == NULL) {
        ATALLAerr(ATALLA_F_ATALLA_FINISH, ATALLA_R_NOT_LOADED);
        return 0;
    }
    if (!DSO_free(atalla_dso)) {
        ATALLAerr(ATALLA_F_ATALLA_FINISH, ATALLA_R_UNIT_FAILURE);
        return 0;
    }
    atalla_dso = NULL;
    p_Atalla_GetHardwareConfig = NULL;
    p_Atalla_RSAPrivateKeyOpFn = NULL;
    p_Atalla_GetPerformanceStatistics = NULL;
    return 1;
}

static int atalla_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f) (void))
{
    int initialised = ((atalla_dso == NULL) ? 0 : 1);
    switch (cmd) {
    case ATALLA_CMD_SO_PATH:
        if (p == NULL) {
            ATALLAerr(ATALLA_F_ATALLA_CTRL, ERR_R_PASSED_NULL_PARAMETER);
            return 0;
        }
        if (initialised) {
            ATALLAerr(ATALLA_F_ATALLA_CTRL, ATALLA_R_ALREADY_LOADED);
            return 0;
        }
        return set_ATALLA_LIBNAME((const char *)p);
    default:
        break;
    }
    ATALLAerr(ATALLA_F_ATALLA_CTRL, ATALLA_R_CTRL_COMMAND_NOT_IMPLEMENTED);
    return 0;
}

static int atalla_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                          const BIGNUM *m, BN_CTX *ctx)
{
    /*
     * I need somewhere to store temporary serialised values for use with the
     * Atalla API calls. A neat cheat - I'll use BIGNUMs from the BN_CTX but
     * access their arrays directly as byte arrays <grin>. This way I don't
     * have to clean anything up.
     */
    BIGNUM *modulus;
    BIGNUM *exponent;
    BIGNUM *argument;
    BIGNUM *result;
    RSAPrivateKey keydata;
    int to_return, numbytes;

    modulus = exponent = argument = result = NULL;
    to_return = 0;              /* expect failure */

    if (!atalla_dso) {
        ATALLAerr(ATALLA_F_ATALLA_MOD_EXP, ATALLA_R_NOT_LOADED);
        goto err;
    }
    /* Prepare the params */
    BN_CTX_start(ctx);
    modulus = BN_CTX_get(ctx);
    exponent = BN_CTX_get(ctx);
    argument = BN_CTX_get(ctx);
    result = BN_CTX_get(ctx);
    if (!result) {
        ATALLAerr(ATALLA_F_ATALLA_MOD_EXP, ATALLA_R_BN_CTX_FULL);
        goto err;
    }
    if (!bn_wexpand(modulus, m->top) || !bn_wexpand(exponent, m->top) ||
        !bn_wexpand(argument, m->top) || !bn_wexpand(result, m->top)) {
        ATALLAerr(ATALLA_F_ATALLA_MOD_EXP, ATALLA_R_BN_EXPAND_FAIL);
        goto err;
    }
    /* Prepare the key-data */
    memset(&keydata, 0, sizeof(keydata));
    numbytes = BN_num_bytes(m);
    memset(exponent->d, 0, numbytes);
    memset(modulus->d, 0, numbytes);
    BN_bn2bin(p, (unsigned char *)exponent->d + numbytes - BN_num_bytes(p));
    BN_bn2bin(m, (unsigned char *)modulus->d + numbytes - BN_num_bytes(m));
    keydata.privateExponent.data = (unsigned char *)exponent->d;
    keydata.privateExponent.len = numbytes;
    keydata.modulus.data = (unsigned char *)modulus->d;
    keydata.modulus.len = numbytes;
    /* Prepare the argument */
    memset(argument->d, 0, numbytes);
    memset(result->d, 0, numbytes);
    BN_bn2bin(a, (unsigned char *)argument->d + numbytes - BN_num_bytes(a));
    /* Perform the operation */
    if (p_Atalla_RSAPrivateKeyOpFn(&keydata, (unsigned char *)result->d,
                                   (unsigned char *)argument->d,
                                   keydata.modulus.len) != 0) {
        ATALLAerr(ATALLA_F_ATALLA_MOD_EXP, ATALLA_R_REQUEST_FAILED);
        goto err;
    }
    /* Convert the response */
    BN_bin2bn((unsigned char *)result->d, numbytes, r);
    to_return = 1;
 err:
    BN_CTX_end(ctx);
    return to_return;
}

#  ifndef OPENSSL_NO_RSA
static int atalla_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa,
                              BN_CTX *ctx)
{
    int to_return = 0;

    if (!atalla_dso) {
        ATALLAerr(ATALLA_F_ATALLA_RSA_MOD_EXP, ATALLA_R_NOT_LOADED);
        goto err;
    }
    if (!rsa->d || !rsa->n) {
        ATALLAerr(ATALLA_F_ATALLA_RSA_MOD_EXP,
                  ATALLA_R_MISSING_KEY_COMPONENTS);
        goto err;
    }
    to_return = atalla_mod_exp(r0, I, rsa->d, rsa->n, ctx);
 err:
    return to_return;
}
#  endif

#  ifndef OPENSSL_NO_DSA
/*
 * This code was liberated and adapted from the commented-out code in
 * dsa_ossl.c. Because of the unoptimised form of the Atalla acceleration (it
 * doesn't have a CRT form for RSA), this function means that an Atalla
 * system running with a DSA server certificate can handshake around 5 or 6
 * times faster/more than an equivalent system running with RSA. Just check
 * out the "signs" statistics from the RSA and DSA parts of "openssl speed
 * -engine atalla dsa1024 rsa1024".
 */
static int atalla_dsa_mod_exp(DSA *dsa, BIGNUM *rr, BIGNUM *a1,
                              BIGNUM *p1, BIGNUM *a2, BIGNUM *p2, BIGNUM *m,
                              BN_CTX *ctx, BN_MONT_CTX *in_mont)
{
    BIGNUM t;
    int to_return = 0;

    BN_init(&t);
    /* let rr = a1 ^ p1 mod m */
    if (!atalla_mod_exp(rr, a1, p1, m, ctx))
        goto end;
    /* let t = a2 ^ p2 mod m */
    if (!atalla_mod_exp(&t, a2, p2, m, ctx))
        goto end;
    /* let rr = rr * t mod m */
    if (!BN_mod_mul(rr, rr, &t, m, ctx))
        goto end;
    to_return = 1;
 end:
    BN_free(&t);
    return to_return;
}

static int atalla_mod_exp_dsa(DSA *dsa, BIGNUM *r, BIGNUM *a,
                              const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
                              BN_MONT_CTX *m_ctx)
{
    return atalla_mod_exp(r, a, p, m, ctx);
}
#  endif

#  ifndef OPENSSL_NO_RSA
/* This function is aliased to mod_exp (with the mont stuff dropped). */
static int atalla_mod_exp_mont(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                               const BIGNUM *m, BN_CTX *ctx,
                               BN_MONT_CTX *m_ctx)
{
    return atalla_mod_exp(r, a, p, m, ctx);
}
#  endif

#  ifndef OPENSSL_NO_DH
/* This function is aliased to mod_exp (with the dh and mont dropped). */
static int atalla_mod_exp_dh(const DH *dh, BIGNUM *r,
                             const BIGNUM *a, const BIGNUM *p,
                             const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    return atalla_mod_exp(r, a, p, m, ctx);
}
#  endif

/*
 * This stuff is needed if this ENGINE is being compiled into a
 * self-contained shared-library.
 */
#  ifndef OPENSSL_NO_DYNAMIC_ENGINE
static int bind_fn(ENGINE *e, const char *id)
{
    if (id && (strcmp(id, engine_atalla_id) != 0))
        return 0;
    if (!bind_helper(e))
        return 0;
    return 1;
}

IMPLEMENT_DYNAMIC_CHECK_FN()
    IMPLEMENT_DYNAMIC_BIND_FN(bind_fn)
#  endif                        /* OPENSSL_NO_DYNAMIC_ENGINE */
# endif                         /* !OPENSSL_NO_HW_ATALLA */
#endif                          /* !OPENSSL_NO_HW */
