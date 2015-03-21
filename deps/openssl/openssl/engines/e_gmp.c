/* crypto/engine/e_gmp.c */
/*
 * Written by Geoff Thorpe (geoff@geoffthorpe.net) for the OpenSSL project
 * 2003.
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

/*
 * This engine is not (currently) compiled in by default. Do enable it,
 * reconfigure OpenSSL with "enable-gmp -lgmp". The GMP libraries and headers
 * must reside in one of the paths searched by the compiler/linker, otherwise
 * paths must be specified - eg. try configuring with "enable-gmp
 * -I<includepath> -L<libpath> -lgmp". YMMV.
 */

/*-
 * As for what this does - it's a largely unoptimised implementation of an
 * ENGINE that uses the GMP library to perform RSA private key operations. To
 * obtain more information about what "unoptimised" means, see my original mail
 * on the subject (though ignore the build instructions which have since
 * changed);
 *
 *    http://www.mail-archive.com/openssl-dev@openssl.org/msg12227.html
 *
 * On my athlon system at least, it appears the builtin OpenSSL code is now
 * slightly faster, which is to say that the RSA-related MPI performance
 * between OpenSSL's BIGNUM and GMP's mpz implementations is probably pretty
 * balanced for this chip, and so the performance degradation in this ENGINE by
 * having to convert to/from GMP formats (and not being able to cache
 * montgomery forms) is probably the difference. However, if some unconfirmed
 * reports from users is anything to go by, the situation on some other
 * chipsets might be a good deal more favourable to the GMP version (eg. PPC).
 * Feedback welcome. */

#include <stdio.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/buffer.h>
#include <openssl/engine.h>
#ifndef OPENSSL_NO_RSA
# include <openssl/rsa.h>
#endif
#include <openssl/bn.h>

#ifndef OPENSSL_NO_HW
# ifndef OPENSSL_NO_GMP

#  include <gmp.h>

#  define E_GMP_LIB_NAME "gmp engine"
#  include "e_gmp_err.c"

static int e_gmp_destroy(ENGINE *e);
static int e_gmp_init(ENGINE *e);
static int e_gmp_finish(ENGINE *e);
static int e_gmp_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f) (void));

#  ifndef OPENSSL_NO_RSA
/* RSA stuff */
static int e_gmp_rsa_mod_exp(BIGNUM *r, const BIGNUM *I, RSA *rsa,
                             BN_CTX *ctx);
static int e_gmp_rsa_finish(RSA *r);
#  endif

/* The definitions for control commands specific to this engine */
/* #define E_GMP_CMD_SO_PATH            ENGINE_CMD_BASE */
static const ENGINE_CMD_DEFN e_gmp_cmd_defns[] = {
#  if 0
    {E_GMP_CMD_SO_PATH,
     "SO_PATH",
     "Specifies the path to the 'e_gmp' shared library",
     ENGINE_CMD_FLAG_STRING},
#  endif
    {0, NULL, NULL, 0}
};

#  ifndef OPENSSL_NO_RSA
/* Our internal RSA_METHOD that we provide pointers to */
static RSA_METHOD e_gmp_rsa = {
    "GMP RSA method",
    NULL,
    NULL,
    NULL,
    NULL,
    e_gmp_rsa_mod_exp,
    NULL,
    NULL,
    e_gmp_rsa_finish,
    /*
     * These flags initialise montgomery crud that GMP ignores, however it
     * makes sure the public key ops (which are done in openssl) don't seem
     * *slower* than usual :-)
     */
    RSA_FLAG_CACHE_PUBLIC | RSA_FLAG_CACHE_PRIVATE,
    NULL,
    NULL,
    NULL
};
#  endif

/* Constants used when creating the ENGINE */
static const char *engine_e_gmp_id = "gmp";
static const char *engine_e_gmp_name = "GMP engine support";

/*
 * This internal function is used by ENGINE_gmp() and possibly by the
 * "dynamic" ENGINE support too
 */
static int bind_helper(ENGINE *e)
{
#  ifndef OPENSSL_NO_RSA
    const RSA_METHOD *meth1;
#  endif
    if (!ENGINE_set_id(e, engine_e_gmp_id) ||
        !ENGINE_set_name(e, engine_e_gmp_name) ||
#  ifndef OPENSSL_NO_RSA
        !ENGINE_set_RSA(e, &e_gmp_rsa) ||
#  endif
        !ENGINE_set_destroy_function(e, e_gmp_destroy) ||
        !ENGINE_set_init_function(e, e_gmp_init) ||
        !ENGINE_set_finish_function(e, e_gmp_finish) ||
        !ENGINE_set_ctrl_function(e, e_gmp_ctrl) ||
        !ENGINE_set_cmd_defns(e, e_gmp_cmd_defns))
        return 0;

#  ifndef OPENSSL_NO_RSA
    meth1 = RSA_PKCS1_SSLeay();
    e_gmp_rsa.rsa_pub_enc = meth1->rsa_pub_enc;
    e_gmp_rsa.rsa_pub_dec = meth1->rsa_pub_dec;
    e_gmp_rsa.rsa_priv_enc = meth1->rsa_priv_enc;
    e_gmp_rsa.rsa_priv_dec = meth1->rsa_priv_dec;
    e_gmp_rsa.bn_mod_exp = meth1->bn_mod_exp;
#  endif

    /* Ensure the e_gmp error handling is set up */
    ERR_load_GMP_strings();
    return 1;
}

static ENGINE *engine_gmp(void)
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

void ENGINE_load_gmp(void)
{
    /* Copied from eng_[openssl|dyn].c */
    ENGINE *toadd = engine_gmp();
    if (!toadd)
        return;
    ENGINE_add(toadd);
    ENGINE_free(toadd);
    ERR_clear_error();
}

#  ifndef OPENSSL_NO_RSA
/* Used to attach our own key-data to an RSA structure */
static int hndidx_rsa = -1;
#  endif

static int e_gmp_destroy(ENGINE *e)
{
    ERR_unload_GMP_strings();
    return 1;
}

/* (de)initialisation functions. */
static int e_gmp_init(ENGINE *e)
{
#  ifndef OPENSSL_NO_RSA
    if (hndidx_rsa == -1)
        hndidx_rsa = RSA_get_ex_new_index(0,
                                          "GMP-based RSA key handle",
                                          NULL, NULL, NULL);
#  endif
    if (hndidx_rsa == -1)
        return 0;
    return 1;
}

static int e_gmp_finish(ENGINE *e)
{
    return 1;
}

static int e_gmp_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f) (void))
{
    int to_return = 1;

    switch (cmd) {
#  if 0
    case E_GMP_CMD_SO_PATH:
        /* ... */
#  endif
        /* The command isn't understood by this engine */
    default:
        GMPerr(GMP_F_E_GMP_CTRL, GMP_R_CTRL_COMMAND_NOT_IMPLEMENTED);
        to_return = 0;
        break;
    }

    return to_return;
}

/*
 * Most often limb sizes will be the same. If not, we use hex conversion
 * which is neat, but extremely inefficient.
 */
static int bn2gmp(const BIGNUM *bn, mpz_t g)
{
    bn_check_top(bn);
    if (((sizeof(bn->d[0]) * 8) == GMP_NUMB_BITS) &&
        (BN_BITS2 == GMP_NUMB_BITS)) {
        /* The common case */
        if (!_mpz_realloc(g, bn->top))
            return 0;
        memcpy(&g->_mp_d[0], &bn->d[0], bn->top * sizeof(bn->d[0]));
        g->_mp_size = bn->top;
        if (bn->neg)
            g->_mp_size = -g->_mp_size;
        return 1;
    } else {
        int toret;
        char *tmpchar = BN_bn2hex(bn);
        if (!tmpchar)
            return 0;
        toret = (mpz_set_str(g, tmpchar, 16) == 0 ? 1 : 0);
        OPENSSL_free(tmpchar);
        return toret;
    }
}

static int gmp2bn(mpz_t g, BIGNUM *bn)
{
    if (((sizeof(bn->d[0]) * 8) == GMP_NUMB_BITS) &&
        (BN_BITS2 == GMP_NUMB_BITS)) {
        /* The common case */
        int s = (g->_mp_size >= 0) ? g->_mp_size : -g->_mp_size;
        BN_zero(bn);
        if (bn_expand2(bn, s) == NULL)
            return 0;
        bn->top = s;
        memcpy(&bn->d[0], &g->_mp_d[0], s * sizeof(bn->d[0]));
        bn_correct_top(bn);
        bn->neg = g->_mp_size >= 0 ? 0 : 1;
        return 1;
    } else {
        int toret;
        char *tmpchar = OPENSSL_malloc(mpz_sizeinbase(g, 16) + 10);
        if (!tmpchar)
            return 0;
        mpz_get_str(tmpchar, 16, g);
        toret = BN_hex2bn(&bn, tmpchar);
        OPENSSL_free(tmpchar);
        return toret;
    }
}

#  ifndef OPENSSL_NO_RSA
typedef struct st_e_gmp_rsa_ctx {
    int public_only;
    mpz_t n;
    mpz_t d;
    mpz_t e;
    mpz_t p;
    mpz_t q;
    mpz_t dmp1;
    mpz_t dmq1;
    mpz_t iqmp;
    mpz_t r0, r1, I0, m1;
} E_GMP_RSA_CTX;

static E_GMP_RSA_CTX *e_gmp_get_rsa(RSA *rsa)
{
    E_GMP_RSA_CTX *hptr = RSA_get_ex_data(rsa, hndidx_rsa);
    if (hptr)
        return hptr;
    hptr = OPENSSL_malloc(sizeof(E_GMP_RSA_CTX));
    if (!hptr)
        return NULL;
    /*
     * These inits could probably be replaced by more intelligent mpz_init2()
     * versions, to reduce malloc-thrashing.
     */
    mpz_init(hptr->n);
    mpz_init(hptr->d);
    mpz_init(hptr->e);
    mpz_init(hptr->p);
    mpz_init(hptr->q);
    mpz_init(hptr->dmp1);
    mpz_init(hptr->dmq1);
    mpz_init(hptr->iqmp);
    mpz_init(hptr->r0);
    mpz_init(hptr->r1);
    mpz_init(hptr->I0);
    mpz_init(hptr->m1);
    if (!bn2gmp(rsa->n, hptr->n) || !bn2gmp(rsa->e, hptr->e))
        goto err;
    if (!rsa->p || !rsa->q || !rsa->d || !rsa->dmp1 || !rsa->dmq1
        || !rsa->iqmp) {
        hptr->public_only = 1;
        return hptr;
    }
    if (!bn2gmp(rsa->d, hptr->d) || !bn2gmp(rsa->p, hptr->p) ||
        !bn2gmp(rsa->q, hptr->q) || !bn2gmp(rsa->dmp1, hptr->dmp1) ||
        !bn2gmp(rsa->dmq1, hptr->dmq1) || !bn2gmp(rsa->iqmp, hptr->iqmp))
        goto err;
    hptr->public_only = 0;
    RSA_set_ex_data(rsa, hndidx_rsa, hptr);
    return hptr;
 err:
    mpz_clear(hptr->n);
    mpz_clear(hptr->d);
    mpz_clear(hptr->e);
    mpz_clear(hptr->p);
    mpz_clear(hptr->q);
    mpz_clear(hptr->dmp1);
    mpz_clear(hptr->dmq1);
    mpz_clear(hptr->iqmp);
    mpz_clear(hptr->r0);
    mpz_clear(hptr->r1);
    mpz_clear(hptr->I0);
    mpz_clear(hptr->m1);
    OPENSSL_free(hptr);
    return NULL;
}

static int e_gmp_rsa_finish(RSA *rsa)
{
    E_GMP_RSA_CTX *hptr = RSA_get_ex_data(rsa, hndidx_rsa);
    if (!hptr)
        return 0;
    mpz_clear(hptr->n);
    mpz_clear(hptr->d);
    mpz_clear(hptr->e);
    mpz_clear(hptr->p);
    mpz_clear(hptr->q);
    mpz_clear(hptr->dmp1);
    mpz_clear(hptr->dmq1);
    mpz_clear(hptr->iqmp);
    mpz_clear(hptr->r0);
    mpz_clear(hptr->r1);
    mpz_clear(hptr->I0);
    mpz_clear(hptr->m1);
    OPENSSL_free(hptr);
    RSA_set_ex_data(rsa, hndidx_rsa, NULL);
    return 1;
}

static int e_gmp_rsa_mod_exp(BIGNUM *r, const BIGNUM *I, RSA *rsa,
                             BN_CTX *ctx)
{
    E_GMP_RSA_CTX *hptr;
    int to_return = 0;

    hptr = e_gmp_get_rsa(rsa);
    if (!hptr) {
        GMPerr(GMP_F_E_GMP_RSA_MOD_EXP, GMP_R_KEY_CONTEXT_ERROR);
        return 0;
    }
    if (hptr->public_only) {
        GMPerr(GMP_F_E_GMP_RSA_MOD_EXP, GMP_R_MISSING_KEY_COMPONENTS);
        return 0;
    }

    /* ugh!!! */
    if (!bn2gmp(I, hptr->I0))
        return 0;

    /*
     * This is basically the CRT logic in crypto/rsa/rsa_eay.c reworded into
     * GMP-speak. It may be that GMP's API facilitates cleaner formulations
     * of this stuff, eg. better handling of negatives, or functions that
     * combine operations.
     */

    mpz_mod(hptr->r1, hptr->I0, hptr->q);
    mpz_powm(hptr->m1, hptr->r1, hptr->dmq1, hptr->q);

    mpz_mod(hptr->r1, hptr->I0, hptr->p);
    mpz_powm(hptr->r0, hptr->r1, hptr->dmp1, hptr->p);

    mpz_sub(hptr->r0, hptr->r0, hptr->m1);

    if (mpz_sgn(hptr->r0) < 0)
        mpz_add(hptr->r0, hptr->r0, hptr->p);
    mpz_mul(hptr->r1, hptr->r0, hptr->iqmp);
    mpz_mod(hptr->r0, hptr->r1, hptr->p);

    if (mpz_sgn(hptr->r0) < 0)
        mpz_add(hptr->r0, hptr->r0, hptr->p);
    mpz_mul(hptr->r1, hptr->r0, hptr->q);
    mpz_add(hptr->r0, hptr->r1, hptr->m1);

    /* ugh!!! */
    if (gmp2bn(hptr->r0, r))
        to_return = 1;

    return 1;
}
#  endif

# endif                         /* !OPENSSL_NO_GMP */

/*
 * This stuff is needed if this ENGINE is being compiled into a
 * self-contained shared-library.
 */
# ifndef OPENSSL_NO_DYNAMIC_ENGINE
IMPLEMENT_DYNAMIC_CHECK_FN()
#  ifndef OPENSSL_NO_GMP
static int bind_fn(ENGINE *e, const char *id)
{
    if (id && (strcmp(id, engine_e_gmp_id) != 0))
        return 0;
    if (!bind_helper(e))
        return 0;
    return 1;
}

IMPLEMENT_DYNAMIC_BIND_FN(bind_fn)
#  else
OPENSSL_EXPORT
    int bind_engine(ENGINE *e, const char *id, const dynamic_fns *fns);
OPENSSL_EXPORT
    int bind_engine(ENGINE *e, const char *id, const dynamic_fns *fns)
{
    return 0;
}
#  endif
# endif                         /* !OPENSSL_NO_DYNAMIC_ENGINE */

#endif                          /* !OPENSSL_NO_HW */
