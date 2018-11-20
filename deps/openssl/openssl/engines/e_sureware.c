/*-
*  Written by Corinne Dive-Reclus(cdive@baltimore.com)
*
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
* Written by Corinne Dive-Reclus(cdive@baltimore.com)
*
* Copyright@2001 Baltimore Technologies Ltd.
* All right Reserved.
*                                                                                      *
*        THIS FILE IS PROVIDED BY BALTIMORE TECHNOLOGIES ``AS IS'' AND                 *
*        ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE         *
*        IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE    *
*        ARE DISCLAIMED.  IN NO EVENT SHALL BALTIMORE TECHNOLOGIES BE LIABLE           *
*        FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL    *
*        DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS       *
*        OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)         *
*        HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT    *
*        LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY     *
*        OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF        *
*        SUCH DAMAGE.                                                                  *
====================================================================*/

#include <stdio.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/dso.h>
#include <openssl/engine.h>
#include <openssl/rand.h>
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
# ifndef OPENSSL_NO_HW_SUREWARE

#  ifdef FLAT_INC
#   include "sureware.h"
#  else
#   include "vendor_defns/sureware.h"
#  endif

#  define SUREWARE_LIB_NAME "sureware engine"
#  include "e_sureware_err.c"

static int surewarehk_ctrl(ENGINE *e, int cmd, long i, void *p,
                           void (*f) (void));
static int surewarehk_destroy(ENGINE *e);
static int surewarehk_init(ENGINE *e);
static int surewarehk_finish(ENGINE *e);
static int surewarehk_modexp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                             const BIGNUM *m, BN_CTX *ctx);

/* RSA stuff */
#  ifndef OPENSSL_NO_RSA
static int surewarehk_rsa_priv_dec(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding);
static int surewarehk_rsa_sign(int flen, const unsigned char *from,
                               unsigned char *to, RSA *rsa, int padding);
#  endif

/* RAND stuff */
static int surewarehk_rand_bytes(unsigned char *buf, int num);
static void surewarehk_rand_seed(const void *buf, int num);
static void surewarehk_rand_add(const void *buf, int num, double entropy);

/* KM stuff */
static EVP_PKEY *surewarehk_load_privkey(ENGINE *e, const char *key_id,
                                         UI_METHOD *ui_method,
                                         void *callback_data);
static EVP_PKEY *surewarehk_load_pubkey(ENGINE *e, const char *key_id,
                                        UI_METHOD *ui_method,
                                        void *callback_data);
static void surewarehk_ex_free(void *obj, void *item, CRYPTO_EX_DATA *ad,
                               int idx, long argl, void *argp);
#  if 0
static void surewarehk_dh_ex_free(void *obj, void *item, CRYPTO_EX_DATA *ad,
                                  int idx, long argl, void *argp);
#  endif

#  ifndef OPENSSL_NO_RSA
/* This function is aliased to mod_exp (with the mont stuff dropped). */
static int surewarehk_mod_exp_mont(BIGNUM *r, const BIGNUM *a,
                                   const BIGNUM *p, const BIGNUM *m,
                                   BN_CTX *ctx, BN_MONT_CTX *m_ctx)
{
    return surewarehk_modexp(r, a, p, m, ctx);
}

/* Our internal RSA_METHOD that we provide pointers to */
static RSA_METHOD surewarehk_rsa = {
    "SureWare RSA method",
    NULL,                       /* pub_enc */
    NULL,                       /* pub_dec */
    surewarehk_rsa_sign,        /* our rsa_sign is OpenSSL priv_enc */
    surewarehk_rsa_priv_dec,    /* priv_dec */
    NULL,                       /* mod_exp */
    surewarehk_mod_exp_mont,    /* mod_exp_mongomery */
    NULL,                       /* init */
    NULL,                       /* finish */
    0,                          /* RSA flag */
    NULL,
    NULL,                       /* OpenSSL sign */
    NULL,                       /* OpenSSL verify */
    NULL                        /* keygen */
};
#  endif

#  ifndef OPENSSL_NO_DH
/* Our internal DH_METHOD that we provide pointers to */
/* This function is aliased to mod_exp (with the dh and mont dropped). */
static int surewarehk_modexp_dh(const DH *dh, BIGNUM *r, const BIGNUM *a,
                                const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
                                BN_MONT_CTX *m_ctx)
{
    return surewarehk_modexp(r, a, p, m, ctx);
}

static DH_METHOD surewarehk_dh = {
    "SureWare DH method",
    NULL,                       /* gen_key */
    NULL,                       /* agree, */
    surewarehk_modexp_dh,       /* dh mod exp */
    NULL,                       /* init */
    NULL,                       /* finish */
    0,                          /* flags */
    NULL,
    NULL
};
#  endif

static RAND_METHOD surewarehk_rand = {
    /* "SureWare RAND method", */
    surewarehk_rand_seed,
    surewarehk_rand_bytes,
    NULL,                       /* cleanup */
    surewarehk_rand_add,
    surewarehk_rand_bytes,
    NULL,                       /* rand_status */
};

#  ifndef OPENSSL_NO_DSA
/* DSA stuff */
static DSA_SIG *surewarehk_dsa_do_sign(const unsigned char *dgst, int dlen,
                                       DSA *dsa);
static int surewarehk_dsa_mod_exp(DSA *dsa, BIGNUM *rr, BIGNUM *a1,
                                  BIGNUM *p1, BIGNUM *a2, BIGNUM *p2,
                                  BIGNUM *m, BN_CTX *ctx,
                                  BN_MONT_CTX *in_mont)
{
    BIGNUM t;
    int to_return = 0;
    BN_init(&t);
    /* let rr = a1 ^ p1 mod m */
    if (!surewarehk_modexp(rr, a1, p1, m, ctx))
        goto end;
    /* let t = a2 ^ p2 mod m */
    if (!surewarehk_modexp(&t, a2, p2, m, ctx))
        goto end;
    /* let rr = rr * t mod m */
    if (!BN_mod_mul(rr, rr, &t, m, ctx))
        goto end;
    to_return = 1;
 end:
    BN_free(&t);
    return to_return;
}

static DSA_METHOD surewarehk_dsa = {
    "SureWare DSA method",
    surewarehk_dsa_do_sign,
    NULL,                       /* sign setup */
    NULL,                       /* verify, */
    surewarehk_dsa_mod_exp,     /* mod exp */
    NULL,                       /* bn mod exp */
    NULL,                       /* init */
    NULL,                       /* finish */
    0,
    NULL,
    NULL,
    NULL
};
#  endif

static const char *engine_sureware_id = "sureware";
static const char *engine_sureware_name = "SureWare hardware engine support";

/* Now, to our own code */

/*
 * As this is only ever called once, there's no need for locking (indeed -
 * the lock will already be held by our caller!!!)
 */
static int bind_sureware(ENGINE *e)
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

    if (!ENGINE_set_id(e, engine_sureware_id) ||
        !ENGINE_set_name(e, engine_sureware_name) ||
#  ifndef OPENSSL_NO_RSA
        !ENGINE_set_RSA(e, &surewarehk_rsa) ||
#  endif
#  ifndef OPENSSL_NO_DSA
        !ENGINE_set_DSA(e, &surewarehk_dsa) ||
#  endif
#  ifndef OPENSSL_NO_DH
        !ENGINE_set_DH(e, &surewarehk_dh) ||
#  endif
        !ENGINE_set_RAND(e, &surewarehk_rand) ||
        !ENGINE_set_destroy_function(e, surewarehk_destroy) ||
        !ENGINE_set_init_function(e, surewarehk_init) ||
        !ENGINE_set_finish_function(e, surewarehk_finish) ||
        !ENGINE_set_ctrl_function(e, surewarehk_ctrl) ||
        !ENGINE_set_load_privkey_function(e, surewarehk_load_privkey) ||
        !ENGINE_set_load_pubkey_function(e, surewarehk_load_pubkey))
        return 0;

#  ifndef OPENSSL_NO_RSA
    /*
     * We know that the "PKCS1_SSLeay()" functions hook properly to the
     * cswift-specific mod_exp and mod_exp_crt so we use those functions. NB:
     * We don't use ENGINE_openssl() or anything "more generic" because
     * something like the RSAref code may not hook properly, and if you own
     * one of these cards then you have the right to do RSA operations on it
     * anyway!
     */
    meth1 = RSA_PKCS1_SSLeay();
    if (meth1) {
        surewarehk_rsa.rsa_pub_enc = meth1->rsa_pub_enc;
        surewarehk_rsa.rsa_pub_dec = meth1->rsa_pub_dec;
    }
#  endif

#  ifndef OPENSSL_NO_DSA
    /*
     * Use the DSA_OpenSSL() method and just hook the mod_exp-ish bits.
     */
    meth2 = DSA_OpenSSL();
    if (meth2) {
        surewarehk_dsa.dsa_do_verify = meth2->dsa_do_verify;
    }
#  endif

#  ifndef OPENSSL_NO_DH
    /* Much the same for Diffie-Hellman */
    meth3 = DH_OpenSSL();
    if (meth3) {
        surewarehk_dh.generate_key = meth3->generate_key;
        surewarehk_dh.compute_key = meth3->compute_key;
    }
#  endif

    /* Ensure the sureware error handling is set up */
    ERR_load_SUREWARE_strings();
    return 1;
}

#  ifndef OPENSSL_NO_DYNAMIC_ENGINE
static int bind_helper(ENGINE *e, const char *id)
{
    if (id && (strcmp(id, engine_sureware_id) != 0))
        return 0;
    if (!bind_sureware(e))
        return 0;
    return 1;
}

IMPLEMENT_DYNAMIC_CHECK_FN()
    IMPLEMENT_DYNAMIC_BIND_FN(bind_helper)
#  else
static ENGINE *engine_sureware(void)
{
    ENGINE *ret = ENGINE_new();
    if (!ret)
        return NULL;
    if (!bind_sureware(ret)) {
        ENGINE_free(ret);
        return NULL;
    }
    return ret;
}

void ENGINE_load_sureware(void)
{
    /* Copied from eng_[openssl|dyn].c */
    ENGINE *toadd = engine_sureware();
    if (!toadd)
        return;
    ENGINE_add(toadd);
    ENGINE_free(toadd);
    ERR_clear_error();
}
#  endif

/*
 * This is a process-global DSO handle used for loading and unloading the
 * SureWareHook library. NB: This is only set (or unset) during an init() or
 * finish() call (reference counts permitting) and they're operating with
 * global locks, so this should be thread-safe implicitly.
 */
static DSO *surewarehk_dso = NULL;
#  ifndef OPENSSL_NO_RSA
/* Index for KM handle.  Not really used yet. */
static int rsaHndidx = -1;
#  endif
#  ifndef OPENSSL_NO_DSA
/* Index for KM handle.  Not really used yet. */
static int dsaHndidx = -1;
#  endif

/*
 * These are the function pointers that are (un)set when the library has
 * successfully (un)loaded.
 */
static SureWareHook_Init_t *p_surewarehk_Init = NULL;
static SureWareHook_Finish_t *p_surewarehk_Finish = NULL;
static SureWareHook_Rand_Bytes_t *p_surewarehk_Rand_Bytes = NULL;
static SureWareHook_Rand_Seed_t *p_surewarehk_Rand_Seed = NULL;
static SureWareHook_Load_Privkey_t *p_surewarehk_Load_Privkey = NULL;
static SureWareHook_Info_Pubkey_t *p_surewarehk_Info_Pubkey = NULL;
static SureWareHook_Load_Rsa_Pubkey_t *p_surewarehk_Load_Rsa_Pubkey = NULL;
static SureWareHook_Load_Dsa_Pubkey_t *p_surewarehk_Load_Dsa_Pubkey = NULL;
static SureWareHook_Free_t *p_surewarehk_Free = NULL;
static SureWareHook_Rsa_Priv_Dec_t *p_surewarehk_Rsa_Priv_Dec = NULL;
static SureWareHook_Rsa_Sign_t *p_surewarehk_Rsa_Sign = NULL;
static SureWareHook_Dsa_Sign_t *p_surewarehk_Dsa_Sign = NULL;
static SureWareHook_Mod_Exp_t *p_surewarehk_Mod_Exp = NULL;

/* Used in the DSO operations. */
static const char *surewarehk_LIBNAME = "SureWareHook";
static const char *n_surewarehk_Init = "SureWareHook_Init";
static const char *n_surewarehk_Finish = "SureWareHook_Finish";
static const char *n_surewarehk_Rand_Bytes = "SureWareHook_Rand_Bytes";
static const char *n_surewarehk_Rand_Seed = "SureWareHook_Rand_Seed";
static const char *n_surewarehk_Load_Privkey = "SureWareHook_Load_Privkey";
static const char *n_surewarehk_Info_Pubkey = "SureWareHook_Info_Pubkey";
static const char *n_surewarehk_Load_Rsa_Pubkey =
    "SureWareHook_Load_Rsa_Pubkey";
static const char *n_surewarehk_Load_Dsa_Pubkey =
    "SureWareHook_Load_Dsa_Pubkey";
static const char *n_surewarehk_Free = "SureWareHook_Free";
static const char *n_surewarehk_Rsa_Priv_Dec = "SureWareHook_Rsa_Priv_Dec";
static const char *n_surewarehk_Rsa_Sign = "SureWareHook_Rsa_Sign";
static const char *n_surewarehk_Dsa_Sign = "SureWareHook_Dsa_Sign";
static const char *n_surewarehk_Mod_Exp = "SureWareHook_Mod_Exp";
static BIO *logstream = NULL;

/*
 * SureWareHook library functions and mechanics - these are used by the
 * higher-level functions further down. NB: As and where there's no error
 * checking, take a look lower down where these functions are called, the
 * checking and error handling is probably down there.
 */
static int threadsafe = 1;
static int surewarehk_ctrl(ENGINE *e, int cmd, long i, void *p,
                           void (*f) (void))
{
    int to_return = 1;

    switch (cmd) {
    case ENGINE_CTRL_SET_LOGSTREAM:
        {
            BIO *bio = (BIO *)p;
            CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
            if (logstream) {
                BIO_free(logstream);
                logstream = NULL;
            }
            if (CRYPTO_add(&bio->references, 1, CRYPTO_LOCK_BIO) > 1)
                logstream = bio;
            else
                SUREWAREerr(SUREWARE_F_SUREWAREHK_CTRL,
                            SUREWARE_R_BIO_WAS_FREED);
        }
        CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
        break;
        /*
         * This will prevent the initialisation function from "installing"
         * the mutex-handling callbacks, even if they are available from
         * within the library (or were provided to the library from the
         * calling application). This is to remove any baggage for
         * applications not using multithreading.
         */
    case ENGINE_CTRL_CHIL_NO_LOCKING:
        CRYPTO_w_lock(CRYPTO_LOCK_ENGINE);
        threadsafe = 0;
        CRYPTO_w_unlock(CRYPTO_LOCK_ENGINE);
        break;

        /* The command isn't understood by this engine */
    default:
        SUREWAREerr(SUREWARE_F_SUREWAREHK_CTRL,
                    ENGINE_R_CTRL_COMMAND_NOT_IMPLEMENTED);
        to_return = 0;
        break;
    }

    return to_return;
}

/* Destructor (complements the "ENGINE_surewarehk()" constructor) */
static int surewarehk_destroy(ENGINE *e)
{
    ERR_unload_SUREWARE_strings();
    return 1;
}

/* (de)initialisation functions. */
static int surewarehk_init(ENGINE *e)
{
    char msg[64] = "ENGINE_init";
    SureWareHook_Init_t *p1 = NULL;
    SureWareHook_Finish_t *p2 = NULL;
    SureWareHook_Rand_Bytes_t *p3 = NULL;
    SureWareHook_Rand_Seed_t *p4 = NULL;
    SureWareHook_Load_Privkey_t *p5 = NULL;
    SureWareHook_Load_Rsa_Pubkey_t *p6 = NULL;
    SureWareHook_Free_t *p7 = NULL;
    SureWareHook_Rsa_Priv_Dec_t *p8 = NULL;
    SureWareHook_Rsa_Sign_t *p9 = NULL;
    SureWareHook_Dsa_Sign_t *p12 = NULL;
    SureWareHook_Info_Pubkey_t *p13 = NULL;
    SureWareHook_Load_Dsa_Pubkey_t *p14 = NULL;
    SureWareHook_Mod_Exp_t *p15 = NULL;

    if (surewarehk_dso != NULL) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_INIT, ENGINE_R_ALREADY_LOADED);
        goto err;
    }
    /* Attempt to load libsurewarehk.so/surewarehk.dll/whatever. */
    surewarehk_dso = DSO_load(NULL, surewarehk_LIBNAME, NULL, 0);
    if (surewarehk_dso == NULL) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_INIT, ENGINE_R_DSO_FAILURE);
        goto err;
    }
    if (!
        (p1 =
         (SureWareHook_Init_t *) DSO_bind_func(surewarehk_dso,
                                               n_surewarehk_Init))
|| !(p2 =
     (SureWareHook_Finish_t *) DSO_bind_func(surewarehk_dso,
                                             n_surewarehk_Finish))
|| !(p3 =
     (SureWareHook_Rand_Bytes_t *) DSO_bind_func(surewarehk_dso,
                                                 n_surewarehk_Rand_Bytes))
|| !(p4 =
     (SureWareHook_Rand_Seed_t *) DSO_bind_func(surewarehk_dso,
                                                n_surewarehk_Rand_Seed))
|| !(p5 =
     (SureWareHook_Load_Privkey_t *) DSO_bind_func(surewarehk_dso,
                                                   n_surewarehk_Load_Privkey))
|| !(p6 =
     (SureWareHook_Load_Rsa_Pubkey_t *) DSO_bind_func(surewarehk_dso,
                                                      n_surewarehk_Load_Rsa_Pubkey))
|| !(p7 =
     (SureWareHook_Free_t *) DSO_bind_func(surewarehk_dso, n_surewarehk_Free))
|| !(p8 =
     (SureWareHook_Rsa_Priv_Dec_t *) DSO_bind_func(surewarehk_dso,
                                                   n_surewarehk_Rsa_Priv_Dec))
|| !(p9 =
     (SureWareHook_Rsa_Sign_t *) DSO_bind_func(surewarehk_dso,
                                               n_surewarehk_Rsa_Sign))
|| !(p12 =
     (SureWareHook_Dsa_Sign_t *) DSO_bind_func(surewarehk_dso,
                                               n_surewarehk_Dsa_Sign))
|| !(p13 =
     (SureWareHook_Info_Pubkey_t *) DSO_bind_func(surewarehk_dso,
                                                  n_surewarehk_Info_Pubkey))
|| !(p14 =
     (SureWareHook_Load_Dsa_Pubkey_t *) DSO_bind_func(surewarehk_dso,
                                                      n_surewarehk_Load_Dsa_Pubkey))
|| !(p15 =
     (SureWareHook_Mod_Exp_t *) DSO_bind_func(surewarehk_dso,
                                              n_surewarehk_Mod_Exp))) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_INIT, ENGINE_R_DSO_FAILURE);
        goto err;
    }
    /* Copy the pointers */
    p_surewarehk_Init = p1;
    p_surewarehk_Finish = p2;
    p_surewarehk_Rand_Bytes = p3;
    p_surewarehk_Rand_Seed = p4;
    p_surewarehk_Load_Privkey = p5;
    p_surewarehk_Load_Rsa_Pubkey = p6;
    p_surewarehk_Free = p7;
    p_surewarehk_Rsa_Priv_Dec = p8;
    p_surewarehk_Rsa_Sign = p9;
    p_surewarehk_Dsa_Sign = p12;
    p_surewarehk_Info_Pubkey = p13;
    p_surewarehk_Load_Dsa_Pubkey = p14;
    p_surewarehk_Mod_Exp = p15;
    /* Contact the hardware and initialises it. */
    if (p_surewarehk_Init(msg, threadsafe) == SUREWAREHOOK_ERROR_UNIT_FAILURE) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_INIT, SUREWARE_R_UNIT_FAILURE);
        goto err;
    }
    if (p_surewarehk_Init(msg, threadsafe) == SUREWAREHOOK_ERROR_UNIT_FAILURE) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_INIT, SUREWARE_R_UNIT_FAILURE);
        goto err;
    }
    /*
     * try to load the default private key, if failed does not return a
     * failure but wait for an explicit ENGINE_load_privakey
     */
    surewarehk_load_privkey(e, NULL, NULL, NULL);

    /* Everything's fine. */
#  ifndef OPENSSL_NO_RSA
    if (rsaHndidx == -1)
        rsaHndidx = RSA_get_ex_new_index(0,
                                         "SureWareHook RSA key handle",
                                         NULL, NULL, surewarehk_ex_free);
#  endif
#  ifndef OPENSSL_NO_DSA
    if (dsaHndidx == -1)
        dsaHndidx = DSA_get_ex_new_index(0,
                                         "SureWareHook DSA key handle",
                                         NULL, NULL, surewarehk_ex_free);
#  endif

    return 1;
 err:
    if (surewarehk_dso)
        DSO_free(surewarehk_dso);
    surewarehk_dso = NULL;
    p_surewarehk_Init = NULL;
    p_surewarehk_Finish = NULL;
    p_surewarehk_Rand_Bytes = NULL;
    p_surewarehk_Rand_Seed = NULL;
    p_surewarehk_Load_Privkey = NULL;
    p_surewarehk_Load_Rsa_Pubkey = NULL;
    p_surewarehk_Free = NULL;
    p_surewarehk_Rsa_Priv_Dec = NULL;
    p_surewarehk_Rsa_Sign = NULL;
    p_surewarehk_Dsa_Sign = NULL;
    p_surewarehk_Info_Pubkey = NULL;
    p_surewarehk_Load_Dsa_Pubkey = NULL;
    p_surewarehk_Mod_Exp = NULL;
    return 0;
}

static int surewarehk_finish(ENGINE *e)
{
    int to_return = 1;
    if (surewarehk_dso == NULL) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_FINISH, ENGINE_R_NOT_LOADED);
        to_return = 0;
        goto err;
    }
    p_surewarehk_Finish();
    if (!DSO_free(surewarehk_dso)) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_FINISH, ENGINE_R_DSO_FAILURE);
        to_return = 0;
        goto err;
    }
 err:
    if (logstream)
        BIO_free(logstream);
    surewarehk_dso = NULL;
    p_surewarehk_Init = NULL;
    p_surewarehk_Finish = NULL;
    p_surewarehk_Rand_Bytes = NULL;
    p_surewarehk_Rand_Seed = NULL;
    p_surewarehk_Load_Privkey = NULL;
    p_surewarehk_Load_Rsa_Pubkey = NULL;
    p_surewarehk_Free = NULL;
    p_surewarehk_Rsa_Priv_Dec = NULL;
    p_surewarehk_Rsa_Sign = NULL;
    p_surewarehk_Dsa_Sign = NULL;
    p_surewarehk_Info_Pubkey = NULL;
    p_surewarehk_Load_Dsa_Pubkey = NULL;
    p_surewarehk_Mod_Exp = NULL;
    return to_return;
}

static void surewarehk_error_handling(char *const msg, int func, int ret)
{
    switch (ret) {
    case SUREWAREHOOK_ERROR_UNIT_FAILURE:
        ENGINEerr(func, SUREWARE_R_UNIT_FAILURE);
        break;
    case SUREWAREHOOK_ERROR_FALLBACK:
        ENGINEerr(func, SUREWARE_R_REQUEST_FALLBACK);
        break;
    case SUREWAREHOOK_ERROR_DATA_SIZE:
        ENGINEerr(func, SUREWARE_R_SIZE_TOO_LARGE_OR_TOO_SMALL);
        break;
    case SUREWAREHOOK_ERROR_INVALID_PAD:
        ENGINEerr(func, SUREWARE_R_PADDING_CHECK_FAILED);
        break;
    default:
        ENGINEerr(func, SUREWARE_R_REQUEST_FAILED);
        break;
    case 1:                    /* nothing */
        msg[0] = '\0';
    }
    if (*msg) {
        ERR_add_error_data(1, msg);
        if (logstream) {
            CRYPTO_w_lock(CRYPTO_LOCK_BIO);
            BIO_write(logstream, msg, strlen(msg));
            CRYPTO_w_unlock(CRYPTO_LOCK_BIO);
        }
    }
}

static int surewarehk_rand_bytes(unsigned char *buf, int num)
{
    int ret = 0;
    char msg[64] = "ENGINE_rand_bytes";
    if (!p_surewarehk_Rand_Bytes) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_RAND_BYTES,
                    ENGINE_R_NOT_INITIALISED);
    } else {
        ret = p_surewarehk_Rand_Bytes(msg, buf, num);
        surewarehk_error_handling(msg, SUREWARE_F_SUREWAREHK_RAND_BYTES, ret);
    }
    return ret == 1 ? 1 : 0;
}

static void surewarehk_rand_seed(const void *buf, int num)
{
    int ret = 0;
    char msg[64] = "ENGINE_rand_seed";
    if (!p_surewarehk_Rand_Seed) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_RAND_SEED,
                    ENGINE_R_NOT_INITIALISED);
    } else {
        ret = p_surewarehk_Rand_Seed(msg, buf, num);
        surewarehk_error_handling(msg, SUREWARE_F_SUREWAREHK_RAND_SEED, ret);
    }
}

static void surewarehk_rand_add(const void *buf, int num, double entropy)
{
    surewarehk_rand_seed(buf, num);
}

static EVP_PKEY *sureware_load_public(ENGINE *e, const char *key_id,
                                      char *hptr, unsigned long el,
                                      char keytype)
{
    EVP_PKEY *res = NULL;
#  ifndef OPENSSL_NO_RSA
    RSA *rsatmp = NULL;
#  endif
#  ifndef OPENSSL_NO_DSA
    DSA *dsatmp = NULL;
#  endif
    char msg[64] = "sureware_load_public";
    int ret = 0;
    if (!p_surewarehk_Load_Rsa_Pubkey || !p_surewarehk_Load_Dsa_Pubkey) {
        SUREWAREerr(SUREWARE_F_SUREWARE_LOAD_PUBLIC,
                    ENGINE_R_NOT_INITIALISED);
        goto err;
    }
    switch (keytype) {
#  ifndef OPENSSL_NO_RSA
    case 1:
         /*RSA*/
            /* set private external reference */
            rsatmp = RSA_new_method(e);
        RSA_set_ex_data(rsatmp, rsaHndidx, hptr);
        rsatmp->flags |= RSA_FLAG_EXT_PKEY;

        /* set public big nums */
        rsatmp->e = BN_new();
        rsatmp->n = BN_new();
        bn_expand2(rsatmp->e, el / sizeof(BN_ULONG));
        bn_expand2(rsatmp->n, el / sizeof(BN_ULONG));
        if (!rsatmp->e || rsatmp->e->dmax != (int)(el / sizeof(BN_ULONG)) ||
            !rsatmp->n || rsatmp->n->dmax != (int)(el / sizeof(BN_ULONG)))
            goto err;
        ret = p_surewarehk_Load_Rsa_Pubkey(msg, key_id, el,
                                           (unsigned long *)rsatmp->n->d,
                                           (unsigned long *)rsatmp->e->d);
        surewarehk_error_handling(msg, SUREWARE_F_SUREWARE_LOAD_PUBLIC, ret);
        if (ret != 1) {
            SUREWAREerr(SUREWARE_F_SUREWARE_LOAD_PUBLIC,
                        ENGINE_R_FAILED_LOADING_PUBLIC_KEY);
            goto err;
        }
        /* normalise pub e and pub n */
        rsatmp->e->top = el / sizeof(BN_ULONG);
        bn_fix_top(rsatmp->e);
        rsatmp->n->top = el / sizeof(BN_ULONG);
        bn_fix_top(rsatmp->n);
        /* create an EVP object: engine + rsa key */
        res = EVP_PKEY_new();
        EVP_PKEY_assign_RSA(res, rsatmp);
        break;
#  endif

#  ifndef OPENSSL_NO_DSA
    case 2:
         /*DSA*/
            /* set private/public external reference */
            dsatmp = DSA_new_method(e);
        DSA_set_ex_data(dsatmp, dsaHndidx, hptr);
        /*
         * dsatmp->flags |= DSA_FLAG_EXT_PKEY;
         */

        /* set public key */
        dsatmp->pub_key = BN_new();
        dsatmp->p = BN_new();
        dsatmp->q = BN_new();
        dsatmp->g = BN_new();
        bn_expand2(dsatmp->pub_key, el / sizeof(BN_ULONG));
        bn_expand2(dsatmp->p, el / sizeof(BN_ULONG));
        bn_expand2(dsatmp->q, 20 / sizeof(BN_ULONG));
        bn_expand2(dsatmp->g, el / sizeof(BN_ULONG));
        if (!dsatmp->pub_key
            || dsatmp->pub_key->dmax != (int)(el / sizeof(BN_ULONG))
            || !dsatmp->p || dsatmp->p->dmax != (int)(el / sizeof(BN_ULONG))
            || !dsatmp->q || dsatmp->q->dmax != 20 / sizeof(BN_ULONG)
            || !dsatmp->g || dsatmp->g->dmax != (int)(el / sizeof(BN_ULONG)))
            goto err;

        ret = p_surewarehk_Load_Dsa_Pubkey(msg, key_id, el,
                                           (unsigned long *)dsatmp->
                                           pub_key->d,
                                           (unsigned long *)dsatmp->p->d,
                                           (unsigned long *)dsatmp->q->d,
                                           (unsigned long *)dsatmp->g->d);
        surewarehk_error_handling(msg, SUREWARE_F_SUREWARE_LOAD_PUBLIC, ret);
        if (ret != 1) {
            SUREWAREerr(SUREWARE_F_SUREWARE_LOAD_PUBLIC,
                        ENGINE_R_FAILED_LOADING_PUBLIC_KEY);
            goto err;
        }
        /* set parameters */
        /* normalise pubkey and parameters in case of */
        dsatmp->pub_key->top = el / sizeof(BN_ULONG);
        bn_fix_top(dsatmp->pub_key);
        dsatmp->p->top = el / sizeof(BN_ULONG);
        bn_fix_top(dsatmp->p);
        dsatmp->q->top = 20 / sizeof(BN_ULONG);
        bn_fix_top(dsatmp->q);
        dsatmp->g->top = el / sizeof(BN_ULONG);
        bn_fix_top(dsatmp->g);

        /* create an EVP object: engine + rsa key */
        res = EVP_PKEY_new();
        EVP_PKEY_assign_DSA(res, dsatmp);
        break;
#  endif

    default:
        SUREWAREerr(SUREWARE_F_SUREWARE_LOAD_PUBLIC,
                    ENGINE_R_FAILED_LOADING_PRIVATE_KEY);
        goto err;
    }
    return res;
 err:
#  ifndef OPENSSL_NO_RSA
    if (rsatmp)
        RSA_free(rsatmp);
#  endif
#  ifndef OPENSSL_NO_DSA
    if (dsatmp)
        DSA_free(dsatmp);
#  endif
    return NULL;
}

static EVP_PKEY *surewarehk_load_privkey(ENGINE *e, const char *key_id,
                                         UI_METHOD *ui_method,
                                         void *callback_data)
{
    EVP_PKEY *res = NULL;
    int ret = 0;
    unsigned long el = 0;
    char *hptr = NULL;
    char keytype = 0;
    char msg[64] = "ENGINE_load_privkey";

    if (!p_surewarehk_Load_Privkey) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_LOAD_PRIVKEY,
                    ENGINE_R_NOT_INITIALISED);
    } else {
        ret = p_surewarehk_Load_Privkey(msg, key_id, &hptr, &el, &keytype);
        if (ret != 1) {
            SUREWAREerr(SUREWARE_F_SUREWAREHK_LOAD_PRIVKEY,
                        ENGINE_R_FAILED_LOADING_PRIVATE_KEY);
            ERR_add_error_data(1, msg);
        } else
            res = sureware_load_public(e, key_id, hptr, el, keytype);
    }
    return res;
}

static EVP_PKEY *surewarehk_load_pubkey(ENGINE *e, const char *key_id,
                                        UI_METHOD *ui_method,
                                        void *callback_data)
{
    EVP_PKEY *res = NULL;
    int ret = 0;
    unsigned long el = 0;
    char *hptr = NULL;
    char keytype = 0;
    char msg[64] = "ENGINE_load_pubkey";

    if (!p_surewarehk_Info_Pubkey) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_LOAD_PUBKEY,
                    ENGINE_R_NOT_INITIALISED);
    } else {
        /* call once to identify if DSA or RSA */
        ret = p_surewarehk_Info_Pubkey(msg, key_id, &el, &keytype);
        if (ret != 1) {
            SUREWAREerr(SUREWARE_F_SUREWAREHK_LOAD_PUBKEY,
                        ENGINE_R_FAILED_LOADING_PUBLIC_KEY);
            ERR_add_error_data(1, msg);
        } else
            res = sureware_load_public(e, key_id, hptr, el, keytype);
    }
    return res;
}

/*
 * This cleans up an RSA/DSA KM key(do not destroy the key into the hardware)
 * , called when ex_data is freed
 */
static void surewarehk_ex_free(void *obj, void *item, CRYPTO_EX_DATA *ad,
                               int idx, long argl, void *argp)
{
    if (!p_surewarehk_Free) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_EX_FREE, ENGINE_R_NOT_INITIALISED);
    } else
        p_surewarehk_Free((char *)item, 0);
}

#  if 0
/* not currently used (bug?) */
/*
 * This cleans up an DH KM key (destroys the key into hardware), called when
 * ex_data is freed
 */
static void surewarehk_dh_ex_free(void *obj, void *item, CRYPTO_EX_DATA *ad,
                                  int idx, long argl, void *argp)
{
    if (!p_surewarehk_Free) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_DH_EX_FREE,
                    ENGINE_R_NOT_INITIALISED);
    } else
        p_surewarehk_Free((char *)item, 1);
}
#  endif

/*
 * return number of decrypted bytes
 */
#  ifndef OPENSSL_NO_RSA
static int surewarehk_rsa_priv_dec(int flen, const unsigned char *from,
                                   unsigned char *to, RSA *rsa, int padding)
{
    int ret = 0, tlen;
    char *buf = NULL, *hptr = NULL;
    char msg[64] = "ENGINE_rsa_priv_dec";
    if (!p_surewarehk_Rsa_Priv_Dec) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_RSA_PRIV_DEC,
                    ENGINE_R_NOT_INITIALISED);
    }
    /* extract ref to private key */
    else if (!(hptr = RSA_get_ex_data(rsa, rsaHndidx))) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_RSA_PRIV_DEC,
                    SUREWARE_R_MISSING_KEY_COMPONENTS);
        goto err;
    }
    /* analyse what padding we can do into the hardware */
    if (padding == RSA_PKCS1_PADDING) {
        /* do it one shot */
        ret =
            p_surewarehk_Rsa_Priv_Dec(msg, flen, (unsigned char *)from, &tlen,
                                      to, hptr, SUREWARE_PKCS1_PAD);
        surewarehk_error_handling(msg, SUREWARE_F_SUREWAREHK_RSA_PRIV_DEC,
                                  ret);
        if (ret != 1)
            goto err;
        ret = tlen;
    } else {                    /* do with no padding into hardware */

        ret =
            p_surewarehk_Rsa_Priv_Dec(msg, flen, (unsigned char *)from, &tlen,
                                      to, hptr, SUREWARE_NO_PAD);
        surewarehk_error_handling(msg, SUREWARE_F_SUREWAREHK_RSA_PRIV_DEC,
                                  ret);
        if (ret != 1)
            goto err;
        /* intermediate buffer for padding */
        if ((buf = OPENSSL_malloc(tlen)) == NULL) {
            SUREWAREerr(SUREWARE_F_SUREWAREHK_RSA_PRIV_DEC,
                        ERR_R_MALLOC_FAILURE);
            goto err;
        }
        memcpy(buf, to, tlen);  /* transfert to into buf */
        switch (padding) {      /* check padding in software */
#   ifndef OPENSSL_NO_SHA
        case RSA_PKCS1_OAEP_PADDING:
            ret =
                RSA_padding_check_PKCS1_OAEP(to, tlen, (unsigned char *)buf,
                                             tlen, tlen, NULL, 0);
            break;
#   endif
        case RSA_SSLV23_PADDING:
            ret =
                RSA_padding_check_SSLv23(to, tlen, (unsigned char *)buf, flen,
                                         tlen);
            break;
        case RSA_NO_PADDING:
            ret =
                RSA_padding_check_none(to, tlen, (unsigned char *)buf, flen,
                                       tlen);
            break;
        default:
            SUREWAREerr(SUREWARE_F_SUREWAREHK_RSA_PRIV_DEC,
                        SUREWARE_R_UNKNOWN_PADDING_TYPE);
            goto err;
        }
        if (ret < 0)
            SUREWAREerr(SUREWARE_F_SUREWAREHK_RSA_PRIV_DEC,
                        SUREWARE_R_PADDING_CHECK_FAILED);
    }
 err:
    if (buf) {
        OPENSSL_cleanse(buf, tlen);
        OPENSSL_free(buf);
    }
    return ret;
}

/*
 * Does what OpenSSL rsa_priv_enc does.
 */
static int surewarehk_rsa_sign(int flen, const unsigned char *from,
                               unsigned char *to, RSA *rsa, int padding)
{
    int ret = 0, tlen;
    char *hptr = NULL;
    char msg[64] = "ENGINE_rsa_sign";
    if (!p_surewarehk_Rsa_Sign) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_RSA_SIGN, ENGINE_R_NOT_INITIALISED);
    }
    /* extract ref to private key */
    else if (!(hptr = RSA_get_ex_data(rsa, rsaHndidx))) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_RSA_SIGN,
                    SUREWARE_R_MISSING_KEY_COMPONENTS);
    } else {
        switch (padding) {
        case RSA_PKCS1_PADDING: /* do it in one shot */
            ret =
                p_surewarehk_Rsa_Sign(msg, flen, (unsigned char *)from, &tlen,
                                      to, hptr, SUREWARE_PKCS1_PAD);
            surewarehk_error_handling(msg, SUREWARE_F_SUREWAREHK_RSA_SIGN,
                                      ret);
            break;
        case RSA_NO_PADDING:
        default:
            SUREWAREerr(SUREWARE_F_SUREWAREHK_RSA_SIGN,
                        SUREWARE_R_UNKNOWN_PADDING_TYPE);
        }
    }
    return ret == 1 ? tlen : ret;
}

#  endif

#  ifndef OPENSSL_NO_DSA
/* DSA sign and verify */
static DSA_SIG *surewarehk_dsa_do_sign(const unsigned char *from, int flen,
                                       DSA *dsa)
{
    int ret = 0;
    char *hptr = NULL;
    DSA_SIG *psign = NULL;
    char msg[64] = "ENGINE_dsa_do_sign";
    if (!p_surewarehk_Dsa_Sign) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_DSA_DO_SIGN,
                    ENGINE_R_NOT_INITIALISED);
        goto err;
    }
    /* extract ref to private key */
    else if (!(hptr = DSA_get_ex_data(dsa, dsaHndidx))) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_DSA_DO_SIGN,
                    SUREWARE_R_MISSING_KEY_COMPONENTS);
        goto err;
    } else {
        if ((psign = DSA_SIG_new()) == NULL) {
            SUREWAREerr(SUREWARE_F_SUREWAREHK_DSA_DO_SIGN,
                        ERR_R_MALLOC_FAILURE);
            goto err;
        }
        psign->r = BN_new();
        psign->s = BN_new();
        bn_expand2(psign->r, 20 / sizeof(BN_ULONG));
        bn_expand2(psign->s, 20 / sizeof(BN_ULONG));
        if (!psign->r || psign->r->dmax != 20 / sizeof(BN_ULONG) ||
            !psign->s || psign->s->dmax != 20 / sizeof(BN_ULONG))
            goto err;
        ret = p_surewarehk_Dsa_Sign(msg, flen, from,
                                    (unsigned long *)psign->r->d,
                                    (unsigned long *)psign->s->d, hptr);
        surewarehk_error_handling(msg, SUREWARE_F_SUREWAREHK_DSA_DO_SIGN,
                                  ret);
    }
    psign->r->top = 20 / sizeof(BN_ULONG);
    bn_fix_top(psign->r);
    psign->s->top = 20 / sizeof(BN_ULONG);
    bn_fix_top(psign->s);

 err:
    if (psign) {
        DSA_SIG_free(psign);
        psign = NULL;
    }
    return psign;
}
#  endif

static int surewarehk_modexp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                             const BIGNUM *m, BN_CTX *ctx)
{
    int ret = 0;
    char msg[64] = "ENGINE_modexp";
    if (!p_surewarehk_Mod_Exp) {
        SUREWAREerr(SUREWARE_F_SUREWAREHK_MODEXP, ENGINE_R_NOT_INITIALISED);
    } else {
        bn_expand2(r, m->top);
        if (r && r->dmax == m->top) {
            /* do it */
            ret = p_surewarehk_Mod_Exp(msg,
                                       m->top * sizeof(BN_ULONG),
                                       (unsigned long *)m->d,
                                       p->top * sizeof(BN_ULONG),
                                       (unsigned long *)p->d,
                                       a->top * sizeof(BN_ULONG),
                                       (unsigned long *)a->d,
                                       (unsigned long *)r->d);
            surewarehk_error_handling(msg, SUREWARE_F_SUREWAREHK_MODEXP, ret);
            if (ret == 1) {
                /* normalise result */
                r->top = m->top;
                bn_fix_top(r);
            }
        }
    }
    return ret;
}
# endif                         /* !OPENSSL_NO_HW_SureWare */
#endif                          /* !OPENSSL_NO_HW */
