/* crypto/engine/hw_ibmca.c */
/* Written by Geoff Thorpe (geoff@geoffthorpe.net) for the OpenSSL
 * project 2000.
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

/* (C) COPYRIGHT International Business Machines Corp. 2001 */

#include <stdio.h>
#include <openssl/crypto.h>
#include <openssl/dso.h>
#include <openssl/engine.h>

#ifndef OPENSSL_NO_HW
#ifndef OPENSSL_NO_HW_IBMCA

#ifdef FLAT_INC
#include "ica_openssl_api.h"
#else
#include "vendor_defns/ica_openssl_api.h"
#endif

#define IBMCA_LIB_NAME "ibmca engine"
#include "hw_ibmca_err.c"

static int ibmca_destroy(ENGINE *e);
static int ibmca_init(ENGINE *e);
static int ibmca_finish(ENGINE *e);
static int ibmca_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)());

static const char *IBMCA_F1 = "icaOpenAdapter";
static const char *IBMCA_F2 = "icaCloseAdapter";
static const char *IBMCA_F3 = "icaRsaModExpo";
static const char *IBMCA_F4 = "icaRandomNumberGenerate";
static const char *IBMCA_F5 = "icaRsaCrt";

ICA_ADAPTER_HANDLE handle=0;

/* BIGNUM stuff */
static int ibmca_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
        const BIGNUM *m, BN_CTX *ctx);

static int ibmca_mod_exp_crt(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
        const BIGNUM *q, const BIGNUM *dmp1, const BIGNUM *dmq1,
        const BIGNUM *iqmp, BN_CTX *ctx);

#ifndef OPENSSL_NO_RSA  
/* RSA stuff */
static int ibmca_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa);
#endif

/* This function is aliased to mod_exp (with the mont stuff dropped). */
static int ibmca_mod_exp_mont(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
        const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);

#ifndef OPENSSL_NO_DSA 
/* DSA stuff */
static int ibmca_dsa_mod_exp(DSA *dsa, BIGNUM *rr, BIGNUM *a1,
        BIGNUM *p1, BIGNUM *a2, BIGNUM *p2, BIGNUM *m,
        BN_CTX *ctx, BN_MONT_CTX *in_mont);
static int ibmca_mod_exp_dsa(DSA *dsa, BIGNUM *r, BIGNUM *a,
        const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
        BN_MONT_CTX *m_ctx);
#endif

#ifndef OPENSSL_NO_DH 
/* DH stuff */
/* This function is alised to mod_exp (with the DH and mont dropped). */
static int ibmca_mod_exp_dh(const DH *dh, BIGNUM *r, 
	const BIGNUM *a, const BIGNUM *p,
	const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);
#endif

/* RAND stuff */
static int ibmca_rand_bytes(unsigned char *buf, int num);
static int ibmca_rand_status(void);


/* WJH - check for more commands, like in nuron */

/* The definitions for control commands specific to this engine */
#define IBMCA_CMD_SO_PATH		ENGINE_CMD_BASE
static const ENGINE_CMD_DEFN ibmca_cmd_defns[] = {
	{IBMCA_CMD_SO_PATH,
		"SO_PATH",
		"Specifies the path to the 'atasi' shared library",
		ENGINE_CMD_FLAG_STRING},
	{0, NULL, NULL, 0}
	};

#ifndef OPENSSL_NO_RSA  
/* Our internal RSA_METHOD that we provide pointers to */
static RSA_METHOD ibmca_rsa =
        {
        "Ibmca RSA method",
        NULL,
        NULL,
        NULL,
        NULL,
        ibmca_rsa_mod_exp,
        ibmca_mod_exp_mont,
        NULL,
        NULL,
        0,
        NULL,
        NULL,
        NULL
        };
#endif

#ifndef OPENSSL_NO_DSA
/* Our internal DSA_METHOD that we provide pointers to */
static DSA_METHOD ibmca_dsa =
        {
        "Ibmca DSA method",
        NULL, /* dsa_do_sign */
        NULL, /* dsa_sign_setup */
        NULL, /* dsa_do_verify */
        ibmca_dsa_mod_exp, /* dsa_mod_exp */
        ibmca_mod_exp_dsa, /* bn_mod_exp */
        NULL, /* init */
        NULL, /* finish */
        0, /* flags */
        NULL /* app_data */
        };
#endif

#ifndef OPENSSL_NO_DH
/* Our internal DH_METHOD that we provide pointers to */
static DH_METHOD ibmca_dh =
        {
        "Ibmca DH method",
        NULL,
        NULL,
        ibmca_mod_exp_dh,
        NULL,
        NULL,
        0,
        NULL
        };
#endif

static RAND_METHOD ibmca_rand =
        {
        /* "IBMCA RAND method", */
        NULL,
        ibmca_rand_bytes,
        NULL,
        NULL,
        ibmca_rand_bytes,
        ibmca_rand_status,
        };

/* Constants used when creating the ENGINE */
static const char *engine_ibmca_id = "ibmca";
static const char *engine_ibmca_name = "Ibmca hardware engine support";

/* This internal function is used by ENGINE_ibmca() and possibly by the
 * "dynamic" ENGINE support too */
static int bind_helper(ENGINE *e)
	{
#ifndef OPENSSL_NO_RSA
	const RSA_METHOD *meth1;
#endif
#ifndef OPENSSL_NO_DSA
	const DSA_METHOD *meth2;
#endif
#ifndef OPENSSL_NO_DH
	const DH_METHOD *meth3;
#endif
	if(!ENGINE_set_id(e, engine_ibmca_id) ||
		!ENGINE_set_name(e, engine_ibmca_name) ||
#ifndef OPENSSL_NO_RSA
		!ENGINE_set_RSA(e, &ibmca_rsa) ||
#endif
#ifndef OPENSSL_NO_DSA
		!ENGINE_set_DSA(e, &ibmca_dsa) ||
#endif
#ifndef OPENSSL_NO_DH
		!ENGINE_set_DH(e, &ibmca_dh) ||
#endif
		!ENGINE_set_RAND(e, &ibmca_rand) ||
		!ENGINE_set_destroy_function(e, ibmca_destroy) ||
		!ENGINE_set_init_function(e, ibmca_init) ||
		!ENGINE_set_finish_function(e, ibmca_finish) ||
		!ENGINE_set_ctrl_function(e, ibmca_ctrl) ||
		!ENGINE_set_cmd_defns(e, ibmca_cmd_defns))
		return 0;

#ifndef OPENSSL_NO_RSA
	/* We know that the "PKCS1_SSLeay()" functions hook properly
	 * to the ibmca-specific mod_exp and mod_exp_crt so we use
	 * those functions. NB: We don't use ENGINE_openssl() or
	 * anything "more generic" because something like the RSAref
	 * code may not hook properly, and if you own one of these
	 * cards then you have the right to do RSA operations on it
	 * anyway! */ 
	meth1 = RSA_PKCS1_SSLeay();
	ibmca_rsa.rsa_pub_enc = meth1->rsa_pub_enc;
	ibmca_rsa.rsa_pub_dec = meth1->rsa_pub_dec;
	ibmca_rsa.rsa_priv_enc = meth1->rsa_priv_enc;
	ibmca_rsa.rsa_priv_dec = meth1->rsa_priv_dec;
#endif

#ifndef OPENSSL_NO_DSA
	/* Use the DSA_OpenSSL() method and just hook the mod_exp-ish
	 * bits. */
	meth2 = DSA_OpenSSL();
	ibmca_dsa.dsa_do_sign = meth2->dsa_do_sign;
	ibmca_dsa.dsa_sign_setup = meth2->dsa_sign_setup;
	ibmca_dsa.dsa_do_verify = meth2->dsa_do_verify;
#endif

#ifndef OPENSSL_NO_DH
	/* Much the same for Diffie-Hellman */
	meth3 = DH_OpenSSL();
	ibmca_dh.generate_key = meth3->generate_key;
	ibmca_dh.compute_key = meth3->compute_key;
#endif

	/* Ensure the ibmca error handling is set up */
	ERR_load_IBMCA_strings(); 
	return 1;
	}

static ENGINE *engine_ibmca(void)
	{
	ENGINE *ret = ENGINE_new();
	if(!ret)
		return NULL;
	if(!bind_helper(ret))
		{
		ENGINE_free(ret);
		return NULL;
		}
	return ret;
	}

#ifdef ENGINE_DYNAMIC_SUPPORT
static
#endif
void ENGINE_load_ibmca(void)
	{
	/* Copied from eng_[openssl|dyn].c */
	ENGINE *toadd = engine_ibmca();
	if(!toadd) return;
	ENGINE_add(toadd);
	ENGINE_free(toadd);
	ERR_clear_error();
	}

/* Destructor (complements the "ENGINE_ibmca()" constructor) */
static int ibmca_destroy(ENGINE *e)
	{
	/* Unload the ibmca error strings so any error state including our
	 * functs or reasons won't lead to a segfault (they simply get displayed
	 * without corresponding string data because none will be found). */
        ERR_unload_IBMCA_strings(); 
	return 1;
	}


/* This is a process-global DSO handle used for loading and unloading
 * the Ibmca library. NB: This is only set (or unset) during an
 * init() or finish() call (reference counts permitting) and they're
 * operating with global locks, so this should be thread-safe
 * implicitly. */

static DSO *ibmca_dso = NULL;

/* These are the function pointers that are (un)set when the library has
 * successfully (un)loaded. */

static unsigned int    (ICA_CALL *p_icaOpenAdapter)();
static unsigned int    (ICA_CALL *p_icaCloseAdapter)();
static unsigned int    (ICA_CALL *p_icaRsaModExpo)();
static unsigned int    (ICA_CALL *p_icaRandomNumberGenerate)();
static unsigned int    (ICA_CALL *p_icaRsaCrt)();

/* utility function to obtain a context */
static int get_context(ICA_ADAPTER_HANDLE *p_handle)
        {
        unsigned int status=0;

        status = p_icaOpenAdapter(0, p_handle);
        if(status != 0)
                return 0;
        return 1;
        }

/* similarly to release one. */
static void release_context(ICA_ADAPTER_HANDLE handle)
        {
        p_icaCloseAdapter(handle);
        }

/* (de)initialisation functions. */
static int ibmca_init(ENGINE *e)
        {

        void          (*p1)();
        void          (*p2)();
        void          (*p3)();
        void          (*p4)();
        void          (*p5)();

        if(ibmca_dso != NULL)
                {
                IBMCAerr(IBMCA_F_IBMCA_INIT,IBMCA_R_ALREADY_LOADED);
                goto err;
                }
        /* Attempt to load libatasi.so/atasi.dll/whatever. Needs to be
         * changed unfortunately because the Ibmca drivers don't have
         * standard library names that can be platform-translated well. */
        /* TODO: Work out how to actually map to the names the Ibmca
         * drivers really use - for now a symbollic link needs to be
         * created on the host system from libatasi.so to atasi.so on
         * unix variants. */

	/* WJH XXX check name translation */

        ibmca_dso = DSO_load(NULL, IBMCA_LIBNAME, NULL,
			     /* DSO_FLAG_NAME_TRANSLATION */ 0);
        if(ibmca_dso == NULL)
                {
                IBMCAerr(IBMCA_F_IBMCA_INIT,IBMCA_R_DSO_FAILURE);
                goto err;
                }

        if(!(p1 = DSO_bind_func(
                ibmca_dso, IBMCA_F1)) ||
                !(p2 = DSO_bind_func(
                        ibmca_dso, IBMCA_F2)) ||
                !(p3 = DSO_bind_func(
                        ibmca_dso, IBMCA_F3)) ||
                !(p4 = DSO_bind_func(
                        ibmca_dso, IBMCA_F4)) ||
                !(p5 = DSO_bind_func(
                        ibmca_dso, IBMCA_F5)))
                {
                IBMCAerr(IBMCA_F_IBMCA_INIT,IBMCA_R_DSO_FAILURE);
                goto err;
                }

        /* Copy the pointers */

	p_icaOpenAdapter =           (unsigned int (ICA_CALL *)())p1;
	p_icaCloseAdapter =          (unsigned int (ICA_CALL *)())p2;
	p_icaRsaModExpo =            (unsigned int (ICA_CALL *)())p3;
	p_icaRandomNumberGenerate =  (unsigned int (ICA_CALL *)())p4;
	p_icaRsaCrt =                (unsigned int (ICA_CALL *)())p5;

        if(!get_context(&handle))
                {
                IBMCAerr(IBMCA_F_IBMCA_INIT,IBMCA_R_UNIT_FAILURE);
                goto err;
                }

        return 1;
 err:
        if(ibmca_dso)
                DSO_free(ibmca_dso);

        p_icaOpenAdapter = NULL;
        p_icaCloseAdapter = NULL;
        p_icaRsaModExpo = NULL;
        p_icaRandomNumberGenerate = NULL;

        return 0;
        }

static int ibmca_finish(ENGINE *e)
        {
        if(ibmca_dso == NULL)
                {
                IBMCAerr(IBMCA_F_IBMCA_FINISH,IBMCA_R_NOT_LOADED);
                return 0;
                }
        release_context(handle);
        if(!DSO_free(ibmca_dso))
                {
                IBMCAerr(IBMCA_F_IBMCA_FINISH,IBMCA_R_DSO_FAILURE);
                return 0;
                }
        ibmca_dso = NULL;

        return 1;
        }

static int ibmca_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)())
	{
	int initialised = ((ibmca_dso == NULL) ? 0 : 1);
	switch(cmd)
		{
	case IBMCA_CMD_SO_PATH:
		if(p == NULL)
			{
			IBMCAerr(IBMCA_F_IBMCA_CTRL,ERR_R_PASSED_NULL_PARAMETER);
			return 0;
			}
		if(initialised)
			{
			IBMCAerr(IBMCA_F_IBMCA_CTRL,IBMCA_R_ALREADY_LOADED);
			return 0;
			}
		IBMCA_LIBNAME = (const char *)p;
		return 1;
	default:
		break;
		}
	IBMCAerr(IBMCA_F_IBMCA_CTRL,IBMCA_R_CTRL_COMMAND_NOT_IMPLEMENTED);
	return 0;
	}


static int ibmca_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
        const BIGNUM *m, BN_CTX *ctx)
        {
        /* I need somewhere to store temporary serialised values for
         * use with the Ibmca API calls. A neat cheat - I'll use
         * BIGNUMs from the BN_CTX but access their arrays directly as
         * byte arrays <grin>. This way I don't have to clean anything
         * up. */

        BIGNUM *argument=NULL;
        BIGNUM *result=NULL;
        BIGNUM *key=NULL;
        int to_return;
	int inLen, outLen, tmpLen;


        ICA_KEY_RSA_MODEXPO *publKey=NULL;
        unsigned int rc;

        to_return = 0; /* expect failure */

        if(!ibmca_dso)
                {
                IBMCAerr(IBMCA_F_IBMCA_MOD_EXP,IBMCA_R_NOT_LOADED);
                goto err;
                }
        /* Prepare the params */
	BN_CTX_start(ctx);
        argument = BN_CTX_get(ctx);
        result = BN_CTX_get(ctx);
        key = BN_CTX_get(ctx);

        if( !argument || !result || !key)
                {
                IBMCAerr(IBMCA_F_IBMCA_MOD_EXP,IBMCA_R_BN_CTX_FULL);
                goto err;
                }


	if(!bn_wexpand(argument, m->top) || !bn_wexpand(result, m->top) ||
                !bn_wexpand(key, sizeof(*publKey)/BN_BYTES))

                {
                IBMCAerr(IBMCA_F_IBMCA_MOD_EXP,IBMCA_R_BN_EXPAND_FAIL);
                goto err;
                }

        publKey = (ICA_KEY_RSA_MODEXPO *)key->d;

        if (publKey == NULL)
                {
                goto err;
                }
        memset(publKey, 0, sizeof(ICA_KEY_RSA_MODEXPO));

        publKey->keyType   =  CORRECT_ENDIANNESS(ME_KEY_TYPE);
        publKey->keyLength =  CORRECT_ENDIANNESS(sizeof(ICA_KEY_RSA_MODEXPO));
        publKey->expOffset =  (char *) publKey->keyRecord - (char *) publKey;

        /* A quirk of the card: the exponent length has to be the same
     as the modulus (key) length */

	outLen = BN_num_bytes(m);

/* check for modulus length SAB*/
	if (outLen > 256 ) {
		IBMCAerr(IBMCA_F_IBMCA_MOD_EXP,IBMCA_R_MEXP_LENGTH_TO_LARGE);
		goto err;
	}
/* check for modulus length SAB*/


	publKey->expLength = publKey->nLength = outLen;
/* SAB Check for underflow condition
    the size of the exponent is less than the size of the parameter
    then we have a big problem and will underflow the keyRecord
   buffer.  Bad stuff could happen then
*/
if (outLen < BN_num_bytes(p)){
	IBMCAerr(IBMCA_F_IBMCA_MOD_EXP,IBMCA_R_UNDERFLOW_KEYRECORD);
	goto err;
}
/* SAB End check for underflow */


        BN_bn2bin(p, &publKey->keyRecord[publKey->expLength -
                BN_num_bytes(p)]);
        BN_bn2bin(m, &publKey->keyRecord[publKey->expLength]);



        publKey->modulusBitLength = CORRECT_ENDIANNESS(publKey->nLength * 8);
        publKey->nOffset   = CORRECT_ENDIANNESS(publKey->expOffset + 
						publKey->expLength);

        publKey->expOffset = CORRECT_ENDIANNESS((char *) publKey->keyRecord - 
						(char *) publKey);

	tmpLen = outLen;
	publKey->expLength = publKey->nLength = CORRECT_ENDIANNESS(tmpLen);

  /* Prepare the argument */

	memset(argument->d, 0, outLen);
	BN_bn2bin(a, (unsigned char *)argument->d + outLen -
                 BN_num_bytes(a));

	inLen = outLen;

  /* Perform the operation */

          if( (rc = p_icaRsaModExpo(handle, inLen,(unsigned char *)argument->d,
                publKey, &outLen, (unsigned char *)result->d))
                !=0 )

                {
                printf("rc = %d\n", rc);
                IBMCAerr(IBMCA_F_IBMCA_MOD_EXP,IBMCA_R_REQUEST_FAILED);
                goto err;
                }


        /* Convert the response */
        BN_bin2bn((unsigned char *)result->d, outLen, r);
        to_return = 1;
 err:
	BN_CTX_end(ctx);
        return to_return;
        }

#ifndef OPENSSL_NO_RSA 
static int ibmca_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa)
        {
        BN_CTX *ctx;
        int to_return = 0;

        if((ctx = BN_CTX_new()) == NULL)
                goto err;
        if(!rsa->p || !rsa->q || !rsa->dmp1 || !rsa->dmq1 || !rsa->iqmp)
                {
                if(!rsa->d || !rsa->n)
                        {
                        IBMCAerr(IBMCA_F_IBMCA_RSA_MOD_EXP,
                                IBMCA_R_MISSING_KEY_COMPONENTS);
                        goto err;
                        }
                to_return = ibmca_mod_exp(r0, I, rsa->d, rsa->n, ctx);
                }
        else
                {
                to_return = ibmca_mod_exp_crt(r0, I, rsa->p, rsa->q, rsa->dmp1,
                        rsa->dmq1, rsa->iqmp, ctx);
                }
 err:
        if(ctx)
                BN_CTX_free(ctx);
        return to_return;
        }
#endif

/* Ein kleines chinesisches "Restessen"  */
static int ibmca_mod_exp_crt(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
        const BIGNUM *q, const BIGNUM *dmp1,
        const BIGNUM *dmq1, const BIGNUM *iqmp, BN_CTX *ctx)
        {

        BIGNUM *argument = NULL;
        BIGNUM *result = NULL;
        BIGNUM *key = NULL;

        int to_return = 0; /* expect failure */

        char                *pkey=NULL;
        ICA_KEY_RSA_CRT     *privKey=NULL;
        int inLen, outLen;

        int rc;
        unsigned int        offset, pSize, qSize;
/* SAB New variables */
	unsigned int keyRecordSize;
	unsigned int pbytes = BN_num_bytes(p);
	unsigned int qbytes = BN_num_bytes(q);
	unsigned int dmp1bytes = BN_num_bytes(dmp1);
	unsigned int dmq1bytes = BN_num_bytes(dmq1);
	unsigned int iqmpbytes = BN_num_bytes(iqmp);

        /* Prepare the params */

	BN_CTX_start(ctx);
        argument = BN_CTX_get(ctx);
        result = BN_CTX_get(ctx);
        key = BN_CTX_get(ctx);

        if(!argument || !result || !key)
                {
                IBMCAerr(IBMCA_F_IBMCA_MOD_EXP_CRT,IBMCA_R_BN_CTX_FULL);
                goto err;
                }

	if(!bn_wexpand(argument, p->top + q->top) ||
                !bn_wexpand(result, p->top + q->top) ||
                !bn_wexpand(key, sizeof(*privKey)/BN_BYTES ))
                {
                IBMCAerr(IBMCA_F_IBMCA_MOD_EXP_CRT,IBMCA_R_BN_EXPAND_FAIL);
                goto err;
                }


        privKey = (ICA_KEY_RSA_CRT *)key->d;
/* SAB Add check for total size in bytes of the parms does not exceed
   the buffer space we have
   do this first
*/
      keyRecordSize = pbytes+qbytes+dmp1bytes+dmq1bytes+iqmpbytes;
     if (  keyRecordSize > sizeof(privKey->keyRecord )) {
	 IBMCAerr(IBMCA_F_IBMCA_MOD_EXP_CRT,IBMCA_R_OPERANDS_TO_LARGE);
         goto err;
     }

     if ( (qbytes + dmq1bytes)  > 256 ){
	 IBMCAerr(IBMCA_F_IBMCA_MOD_EXP_CRT,IBMCA_R_OPERANDS_TO_LARGE);
         goto err;
     }

     if ( pbytes + dmp1bytes > 256 ) {
	 IBMCAerr(IBMCA_F_IBMCA_MOD_EXP_CRT,IBMCA_R_OPERANDS_TO_LARGE);
         goto err;
     }

/* end SAB additions */
  
        memset(privKey, 0, sizeof(ICA_KEY_RSA_CRT));
        privKey->keyType =  CORRECT_ENDIANNESS(CRT_KEY_TYPE);
        privKey->keyLength = CORRECT_ENDIANNESS(sizeof(ICA_KEY_RSA_CRT));
        privKey->modulusBitLength = 
	  CORRECT_ENDIANNESS(BN_num_bytes(q) * 2 * 8);

        /*
         * p,dp & qInv are 1 QWORD Larger
         */
        privKey->pLength = CORRECT_ENDIANNESS(BN_num_bytes(p)+8);
        privKey->qLength = CORRECT_ENDIANNESS(BN_num_bytes(q));
        privKey->dpLength = CORRECT_ENDIANNESS(BN_num_bytes(dmp1)+8);
        privKey->dqLength = CORRECT_ENDIANNESS(BN_num_bytes(dmq1));
        privKey->qInvLength = CORRECT_ENDIANNESS(BN_num_bytes(iqmp)+8);

        offset = (char *) privKey->keyRecord
                  - (char *) privKey;

        qSize = BN_num_bytes(q);
        pSize = qSize + 8;   /*  1 QWORD larger */


/* SAB  probably aittle redundant, but we'll verify that each of the
   components which make up a key record sent ot the card does not exceed
   the space that is allocated for it.  this handles the case where even if
   the total length does not exceed keyrecord zied, if the operands are funny sized
they could cause potential side affects on either the card or the result */

     if ( (pbytes > pSize) || (dmp1bytes > pSize) ||
          (iqmpbytes > pSize) || ( qbytes >qSize) ||
          (dmq1bytes > qSize) ) {
		IBMCAerr(IBMCA_F_IBMCA_MOD_EXP_CRT, IBMCA_R_OPERANDS_TO_LARGE);
		goto err;

	}
     

        privKey->dpOffset = CORRECT_ENDIANNESS(offset);

	offset += pSize;
	privKey->dqOffset = CORRECT_ENDIANNESS(offset);

	offset += qSize;
	privKey->pOffset = CORRECT_ENDIANNESS(offset);

	offset += pSize;
	privKey->qOffset = CORRECT_ENDIANNESS(offset);

	offset += qSize;
	privKey->qInvOffset = CORRECT_ENDIANNESS(offset);

        pkey = (char *) privKey->keyRecord;


/* SAB first check that we don;t under flow the buffer */
	if ( pSize < pbytes ) {
		IBMCAerr(IBMCA_F_IBMCA_MOD_EXP_CRT, IBMCA_R_UNDERFLOW_CONDITION);
		goto err;
	}

        /* pkey += pSize - BN_num_bytes(p); WROING this should be dmp1) */
        pkey += pSize - BN_num_bytes(dmp1);
        BN_bn2bin(dmp1, pkey);   
        pkey += BN_num_bytes(dmp1);  /* move the pointer */

        BN_bn2bin(dmq1, pkey);  /* Copy over dmq1 */

        pkey += qSize;     /* move pointer */
	pkey += pSize - BN_num_bytes(p);  /* set up for zero padding of next field */

        BN_bn2bin(p, pkey);
        pkey += BN_num_bytes(p);  /* increment pointer by number of bytes moved  */

        BN_bn2bin(q, pkey);
        pkey += qSize ;  /* move the pointer */
	pkey +=  pSize - BN_num_bytes(iqmp); /* Adjust for padding */
        BN_bn2bin(iqmp, pkey);

        /* Prepare the argument and response */

	outLen = CORRECT_ENDIANNESS(privKey->qLength) * 2;  /* Correct endianess is used 
						because the fields were converted above */

        if (outLen > 256) {
		IBMCAerr(IBMCA_F_IBMCA_MOD_EXP_CRT,IBMCA_R_OUTLEN_TO_LARGE);
		goto err;
	}

	/* SAB check for underflow here on the argeument */
	if ( outLen < BN_num_bytes(a)) {
		IBMCAerr(IBMCA_F_IBMCA_MOD_EXP_CRT,IBMCA_R_UNDERFLOW_CONDITION);
		goto err;
	}

        BN_bn2bin(a, (unsigned char *)argument->d + outLen -
                          BN_num_bytes(a));
        inLen = outLen;

        memset(result->d, 0, outLen);

        /* Perform the operation */

        if ( (rc = p_icaRsaCrt(handle, inLen, (unsigned char *)argument->d,
                privKey, &outLen, (unsigned char *)result->d)) != 0)
                {
                printf("rc = %d\n", rc);
                IBMCAerr(IBMCA_F_IBMCA_MOD_EXP_CRT,IBMCA_R_REQUEST_FAILED);
                goto err;
                }

        /* Convert the response */

        BN_bin2bn((unsigned char *)result->d, outLen, r);
        to_return = 1;

 err:
	BN_CTX_end(ctx);
        return to_return;

        }

#ifndef OPENSSL_NO_DSA
/* This code was liberated and adapted from the commented-out code in
 * dsa_ossl.c. Because of the unoptimised form of the Ibmca acceleration
 * (it doesn't have a CRT form for RSA), this function means that an
 * Ibmca system running with a DSA server certificate can handshake
 * around 5 or 6 times faster/more than an equivalent system running with
 * RSA. Just check out the "signs" statistics from the RSA and DSA parts
 * of "openssl speed -engine ibmca dsa1024 rsa1024". */
static int ibmca_dsa_mod_exp(DSA *dsa, BIGNUM *rr, BIGNUM *a1,
        BIGNUM *p1, BIGNUM *a2, BIGNUM *p2, BIGNUM *m,
        BN_CTX *ctx, BN_MONT_CTX *in_mont)
        {
        BIGNUM t;
        int to_return = 0;

        BN_init(&t);
        /* let rr = a1 ^ p1 mod m */
        if (!ibmca_mod_exp(rr,a1,p1,m,ctx)) goto end;
        /* let t = a2 ^ p2 mod m */
        if (!ibmca_mod_exp(&t,a2,p2,m,ctx)) goto end;
        /* let rr = rr * t mod m */
        if (!BN_mod_mul(rr,rr,&t,m,ctx)) goto end;
        to_return = 1;
 end:
        BN_free(&t);
        return to_return;
        }


static int ibmca_mod_exp_dsa(DSA *dsa, BIGNUM *r, BIGNUM *a,
        const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx,
        BN_MONT_CTX *m_ctx)
        {
        return ibmca_mod_exp(r, a, p, m, ctx);
        }
#endif

/* This function is aliased to mod_exp (with the mont stuff dropped). */
static int ibmca_mod_exp_mont(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
        const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
        {
        return ibmca_mod_exp(r, a, p, m, ctx);
        }

#ifndef OPENSSL_NO_DH 
/* This function is aliased to mod_exp (with the dh and mont dropped). */
static int ibmca_mod_exp_dh(DH const *dh, BIGNUM *r, 
	const BIGNUM *a, const BIGNUM *p, 
	const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx)
        {
        return ibmca_mod_exp(r, a, p, m, ctx);
        }
#endif

/* Random bytes are good */
static int ibmca_rand_bytes(unsigned char *buf, int num)
        {
        int to_return = 0; /* assume failure */
        unsigned int ret;


        if(handle == 0)
                {
                IBMCAerr(IBMCA_F_IBMCA_RAND_BYTES,IBMCA_R_NOT_INITIALISED);
                goto err;
                }

        ret = p_icaRandomNumberGenerate(handle, num, buf);
        if (ret < 0)
                {
                IBMCAerr(IBMCA_F_IBMCA_RAND_BYTES,IBMCA_R_REQUEST_FAILED);
                goto err;
                }
        to_return = 1;
 err:
        return to_return;
        }

static int ibmca_rand_status(void)
        {
        return 1;
        }

/* This stuff is needed if this ENGINE is being compiled into a self-contained
 * shared-library. */
#ifdef ENGINE_DYNAMIC_SUPPORT
static int bind_fn(ENGINE *e, const char *id)
	{
	if(id && (strcmp(id, engine_ibmca_id) != 0))  /* WJH XXX */
		return 0;
	if(!bind_helper(e))
		return 0;
	return 1;
	}
IMPLEMENT_DYNAMIC_CHECK_FN()
IMPLEMENT_DYNAMIC_BIND_FN(bind_fn)
#endif /* ENGINE_DYNAMIC_SUPPORT */


#endif /* !OPENSSL_NO_HW_IBMCA */
#endif /* !OPENSSL_NO_HW */
