/* Author: Maurice Gittens <maurice@gittens.nl>                       */
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

#include <stdio.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/dso.h>
#include <openssl/x509.h>
#include <openssl/objects.h>
#include <openssl/engine.h>
#include <openssl/rand.h>
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#endif
#include <openssl/bn.h>

#ifndef OPENSSL_NO_HW
#ifndef OPENSSL_NO_HW_4758_CCA

#ifdef FLAT_INC
#include "hw_4758_cca.h"
#else
#include "vendor_defns/hw_4758_cca.h"
#endif

#include "e_4758cca_err.c"

static int ibm_4758_cca_destroy(ENGINE *e);
static int ibm_4758_cca_init(ENGINE *e);
static int ibm_4758_cca_finish(ENGINE *e);
static int ibm_4758_cca_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)(void));

/* rsa functions */
/*---------------*/
#ifndef OPENSSL_NO_RSA
static int cca_rsa_pub_enc(int flen, const unsigned char *from,
		unsigned char *to, RSA *rsa,int padding);
static int cca_rsa_priv_dec(int flen, const unsigned char *from,
		unsigned char *to, RSA *rsa,int padding);
static int cca_rsa_sign(int type, const unsigned char *m, unsigned int m_len,
		unsigned char *sigret, unsigned int *siglen, const RSA *rsa);
static int cca_rsa_verify(int dtype, const unsigned char *m, unsigned int m_len,
	const unsigned char *sigbuf, unsigned int siglen, const RSA *rsa);

/* utility functions */
/*-----------------------*/
static EVP_PKEY *ibm_4758_load_privkey(ENGINE*, const char*,
		UI_METHOD *ui_method, void *callback_data);
static EVP_PKEY *ibm_4758_load_pubkey(ENGINE*, const char*,
		UI_METHOD *ui_method, void *callback_data);

static int getModulusAndExponent(const unsigned char *token, long *exponentLength,
		unsigned char *exponent, long *modulusLength,
		long *modulusFieldLength, unsigned char *modulus);
#endif

/* RAND number functions */
/*-----------------------*/
static int cca_get_random_bytes(unsigned char*, int);
static int cca_random_status(void);

#ifndef OPENSSL_NO_RSA
static void cca_ex_free(void *obj, void *item, CRYPTO_EX_DATA *ad,
		int idx,long argl, void *argp);
#endif

/* Function pointers for CCA verbs */
/*---------------------------------*/
#ifndef OPENSSL_NO_RSA
static F_KEYRECORDREAD keyRecordRead;
static F_DIGITALSIGNATUREGENERATE digitalSignatureGenerate;
static F_DIGITALSIGNATUREVERIFY digitalSignatureVerify;
static F_PUBLICKEYEXTRACT publicKeyExtract;
static F_PKAENCRYPT pkaEncrypt;
static F_PKADECRYPT pkaDecrypt;
#endif
static F_RANDOMNUMBERGENERATE randomNumberGenerate;

/* static variables */
/*------------------*/
static const char *CCA4758_LIB_NAME = NULL;
static const char *get_CCA4758_LIB_NAME(void)
	{
	if(CCA4758_LIB_NAME)
		return CCA4758_LIB_NAME;
	return CCA_LIB_NAME;
	}
static void free_CCA4758_LIB_NAME(void)
	{
	if(CCA4758_LIB_NAME)
		OPENSSL_free((void*)CCA4758_LIB_NAME);
	CCA4758_LIB_NAME = NULL;
	}
static long set_CCA4758_LIB_NAME(const char *name)
	{
	free_CCA4758_LIB_NAME();
	return (((CCA4758_LIB_NAME = BUF_strdup(name)) != NULL) ? 1 : 0);
	}
#ifndef OPENSSL_NO_RSA
static const char* n_keyRecordRead = CSNDKRR;
static const char* n_digitalSignatureGenerate = CSNDDSG;
static const char* n_digitalSignatureVerify = CSNDDSV;
static const char* n_publicKeyExtract = CSNDPKX;
static const char* n_pkaEncrypt = CSNDPKE;
static const char* n_pkaDecrypt = CSNDPKD;
#endif
static const char* n_randomNumberGenerate = CSNBRNG;

#ifndef OPENSSL_NO_RSA
static int hndidx = -1;
#endif
static DSO *dso = NULL;

/* openssl engine initialization structures */
/*------------------------------------------*/

#define CCA4758_CMD_SO_PATH		ENGINE_CMD_BASE
static const ENGINE_CMD_DEFN	cca4758_cmd_defns[] = {
	{CCA4758_CMD_SO_PATH,
		"SO_PATH",
		"Specifies the path to the '4758cca' shared library",
		ENGINE_CMD_FLAG_STRING},
	{0, NULL, NULL, 0}
	};

#ifndef OPENSSL_NO_RSA
static RSA_METHOD ibm_4758_cca_rsa =
	{
	"IBM 4758 CCA RSA method",
	cca_rsa_pub_enc,
	NULL,
	NULL,
	cca_rsa_priv_dec,
	NULL, /*rsa_mod_exp,*/
	NULL, /*mod_exp_mont,*/
	NULL, /* init */
	NULL, /* finish */
	RSA_FLAG_SIGN_VER,	  /* flags */
	NULL, /* app_data */
	cca_rsa_sign, /* rsa_sign */
	cca_rsa_verify, /* rsa_verify */
	NULL /* rsa_keygen */
	};
#endif

static RAND_METHOD ibm_4758_cca_rand =
	{
	/* "IBM 4758 RAND method", */
	NULL, /* seed */
	cca_get_random_bytes, /* get random bytes from the card */
	NULL, /* cleanup */
	NULL, /* add */
	cca_get_random_bytes, /* pseudo rand */
	cca_random_status, /* status */
	};

static const char *engine_4758_cca_id = "4758cca";
static const char *engine_4758_cca_name = "IBM 4758 CCA hardware engine support";
#ifndef OPENSSL_NO_DYNAMIC_ENGINE 
/* Compatibility hack, the dynamic library uses this form in the path */
static const char *engine_4758_cca_id_alt = "4758_cca";
#endif

/* engine implementation */
/*-----------------------*/
static int bind_helper(ENGINE *e)
	{
	if(!ENGINE_set_id(e, engine_4758_cca_id) ||
			!ENGINE_set_name(e, engine_4758_cca_name) ||
#ifndef OPENSSL_NO_RSA
			!ENGINE_set_RSA(e, &ibm_4758_cca_rsa) ||
#endif
			!ENGINE_set_RAND(e, &ibm_4758_cca_rand) ||
			!ENGINE_set_destroy_function(e, ibm_4758_cca_destroy) ||
			!ENGINE_set_init_function(e, ibm_4758_cca_init) ||
			!ENGINE_set_finish_function(e, ibm_4758_cca_finish) ||
			!ENGINE_set_ctrl_function(e, ibm_4758_cca_ctrl) ||
#ifndef OPENSSL_NO_RSA
			!ENGINE_set_load_privkey_function(e, ibm_4758_load_privkey) ||
			!ENGINE_set_load_pubkey_function(e, ibm_4758_load_pubkey) ||
#endif
			!ENGINE_set_cmd_defns(e, cca4758_cmd_defns))
		return 0;
	/* Ensure the error handling is set up */
	ERR_load_CCA4758_strings();
	return 1;
	}

#ifdef OPENSSL_NO_DYNAMIC_ENGINE
static ENGINE *engine_4758_cca(void)
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

void ENGINE_load_4758cca(void)
	{
	ENGINE *e_4758 = engine_4758_cca();
	if (!e_4758) return;
	ENGINE_add(e_4758);
	ENGINE_free(e_4758);
	ERR_clear_error();   
	}
#endif

static int ibm_4758_cca_destroy(ENGINE *e)
	{
	ERR_unload_CCA4758_strings();
	free_CCA4758_LIB_NAME();
	return 1;
	}

static int ibm_4758_cca_init(ENGINE *e)
	{
	if(dso)
		{
		CCA4758err(CCA4758_F_IBM_4758_CCA_INIT,CCA4758_R_ALREADY_LOADED);
		goto err;
		}

	dso = DSO_load(NULL, get_CCA4758_LIB_NAME(), NULL, 0);
	if(!dso)
		{
		CCA4758err(CCA4758_F_IBM_4758_CCA_INIT,CCA4758_R_DSO_FAILURE);
		goto err;
		}

#ifndef OPENSSL_NO_RSA
	if(!(keyRecordRead = (F_KEYRECORDREAD)
				DSO_bind_func(dso, n_keyRecordRead)) ||
			!(randomNumberGenerate = (F_RANDOMNUMBERGENERATE)
				DSO_bind_func(dso, n_randomNumberGenerate)) ||
			!(digitalSignatureGenerate = (F_DIGITALSIGNATUREGENERATE)
				DSO_bind_func(dso, n_digitalSignatureGenerate)) ||
			!(digitalSignatureVerify = (F_DIGITALSIGNATUREVERIFY)
				DSO_bind_func(dso, n_digitalSignatureVerify)) ||
			!(publicKeyExtract = (F_PUBLICKEYEXTRACT)
				DSO_bind_func(dso, n_publicKeyExtract)) ||
			!(pkaEncrypt = (F_PKAENCRYPT)
				DSO_bind_func(dso, n_pkaEncrypt)) ||
			!(pkaDecrypt = (F_PKADECRYPT)
				DSO_bind_func(dso, n_pkaDecrypt)))
		{
		CCA4758err(CCA4758_F_IBM_4758_CCA_INIT,CCA4758_R_DSO_FAILURE);
		goto err;
		}
#else
	if(!(randomNumberGenerate = (F_RANDOMNUMBERGENERATE)
				DSO_bind_func(dso, n_randomNumberGenerate)))
		{
		CCA4758err(CCA4758_F_IBM_4758_CCA_INIT,CCA4758_R_DSO_FAILURE);
		goto err;
		}
#endif

#ifndef OPENSSL_NO_RSA
	hndidx = RSA_get_ex_new_index(0, "IBM 4758 CCA RSA key handle",
		NULL, NULL, cca_ex_free);
#endif

	return 1;
err:
	if(dso)
		DSO_free(dso);
	dso = NULL;

#ifndef OPENSSL_NO_RSA
	keyRecordRead = (F_KEYRECORDREAD)0;
	digitalSignatureGenerate = (F_DIGITALSIGNATUREGENERATE)0;
	digitalSignatureVerify = (F_DIGITALSIGNATUREVERIFY)0;
	publicKeyExtract = (F_PUBLICKEYEXTRACT)0;
	pkaEncrypt = (F_PKAENCRYPT)0;
	pkaDecrypt = (F_PKADECRYPT)0;
#endif
	randomNumberGenerate = (F_RANDOMNUMBERGENERATE)0;
	return 0;
	}

static int ibm_4758_cca_finish(ENGINE *e)
	{
	free_CCA4758_LIB_NAME();
	if(!dso)
		{
		CCA4758err(CCA4758_F_IBM_4758_CCA_FINISH,
				CCA4758_R_NOT_LOADED);
		return 0;
		}
	if(!DSO_free(dso))
		{
		CCA4758err(CCA4758_F_IBM_4758_CCA_FINISH,
				CCA4758_R_UNIT_FAILURE);
		return 0;
		}
	dso = NULL;
#ifndef OPENSSL_NO_RSA
	keyRecordRead = (F_KEYRECORDREAD)0;
	randomNumberGenerate = (F_RANDOMNUMBERGENERATE)0;
	digitalSignatureGenerate = (F_DIGITALSIGNATUREGENERATE)0;
	digitalSignatureVerify = (F_DIGITALSIGNATUREVERIFY)0;
	publicKeyExtract = (F_PUBLICKEYEXTRACT)0;
	pkaEncrypt = (F_PKAENCRYPT)0;
	pkaDecrypt = (F_PKADECRYPT)0;
#endif
	randomNumberGenerate = (F_RANDOMNUMBERGENERATE)0;
	return 1;
	}

static int ibm_4758_cca_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f)(void))
	{
	int initialised = ((dso == NULL) ? 0 : 1);
	switch(cmd)
		{
	case CCA4758_CMD_SO_PATH:
		if(p == NULL)
			{
			CCA4758err(CCA4758_F_IBM_4758_CCA_CTRL,
					ERR_R_PASSED_NULL_PARAMETER);
			return 0;
			}
		if(initialised)
			{
			CCA4758err(CCA4758_F_IBM_4758_CCA_CTRL,
					CCA4758_R_ALREADY_LOADED);
			return 0;
			}
		return set_CCA4758_LIB_NAME((const char *)p);
	default:
		break;
		}
	CCA4758err(CCA4758_F_IBM_4758_CCA_CTRL,
			CCA4758_R_COMMAND_NOT_IMPLEMENTED);
	return 0;
	}

#ifndef OPENSSL_NO_RSA

#define MAX_CCA_PKA_TOKEN_SIZE 2500

static EVP_PKEY *ibm_4758_load_privkey(ENGINE* e, const char* key_id,
			UI_METHOD *ui_method, void *callback_data)
	{
	RSA *rtmp = NULL;
	EVP_PKEY *res = NULL;
	unsigned char* keyToken = NULL;
	unsigned char pubKeyToken[MAX_CCA_PKA_TOKEN_SIZE];
	long pubKeyTokenLength = MAX_CCA_PKA_TOKEN_SIZE;
	long keyTokenLength = MAX_CCA_PKA_TOKEN_SIZE;
	long returnCode;
	long reasonCode;
	long exitDataLength = 0;
	long ruleArrayLength = 0;
	unsigned char exitData[8];
	unsigned char ruleArray[8];
	unsigned char keyLabel[64];
	unsigned long keyLabelLength = strlen(key_id);
	unsigned char modulus[256];
	long modulusFieldLength = sizeof(modulus);
	long modulusLength = 0;
	unsigned char exponent[256];
	long exponentLength = sizeof(exponent);

	if (keyLabelLength > sizeof(keyLabel))
		{
		CCA4758err(CCA4758_F_IBM_4758_LOAD_PRIVKEY,
		CCA4758_R_SIZE_TOO_LARGE_OR_TOO_SMALL);
		return NULL;
		}

	memset(keyLabel,' ', sizeof(keyLabel));
	memcpy(keyLabel, key_id, keyLabelLength);

	keyToken = OPENSSL_malloc(MAX_CCA_PKA_TOKEN_SIZE + sizeof(long));
	if (!keyToken)
		{
		CCA4758err(CCA4758_F_IBM_4758_LOAD_PRIVKEY,
				ERR_R_MALLOC_FAILURE);
		goto err;
		}

	keyRecordRead(&returnCode, &reasonCode, &exitDataLength,
		exitData, &ruleArrayLength, ruleArray, keyLabel,
		&keyTokenLength, keyToken+sizeof(long));

	if (returnCode)
		{
		CCA4758err(CCA4758_F_IBM_4758_LOAD_PRIVKEY,
			CCA4758_R_FAILED_LOADING_PRIVATE_KEY);
		goto err;
		}

	publicKeyExtract(&returnCode, &reasonCode, &exitDataLength,
		exitData, &ruleArrayLength, ruleArray, &keyTokenLength,
		keyToken+sizeof(long), &pubKeyTokenLength, pubKeyToken);

	if (returnCode)
		{
		CCA4758err(CCA4758_F_IBM_4758_LOAD_PRIVKEY,
			CCA4758_R_FAILED_LOADING_PRIVATE_KEY);
		goto err;
		}

	if (!getModulusAndExponent(pubKeyToken, &exponentLength,
			exponent, &modulusLength, &modulusFieldLength,
			modulus))
		{
		CCA4758err(CCA4758_F_IBM_4758_LOAD_PRIVKEY,
			CCA4758_R_FAILED_LOADING_PRIVATE_KEY);
		goto err;
		}

	(*(long*)keyToken) = keyTokenLength;
	rtmp = RSA_new_method(e);
	RSA_set_ex_data(rtmp, hndidx, (char *)keyToken);

	rtmp->e = BN_bin2bn(exponent, exponentLength, NULL);
	rtmp->n = BN_bin2bn(modulus, modulusFieldLength, NULL);
	rtmp->flags |= RSA_FLAG_EXT_PKEY;

	res = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(res, rtmp);

	return res;
err:
	if (keyToken)
		OPENSSL_free(keyToken);
	return NULL;
	}

static EVP_PKEY *ibm_4758_load_pubkey(ENGINE* e, const char* key_id,
			UI_METHOD *ui_method, void *callback_data)
	{
	RSA *rtmp = NULL;
	EVP_PKEY *res = NULL;
	unsigned char* keyToken = NULL;
	long keyTokenLength = MAX_CCA_PKA_TOKEN_SIZE;
	long returnCode;
	long reasonCode;
	long exitDataLength = 0;
	long ruleArrayLength = 0;
	unsigned char exitData[8];
	unsigned char ruleArray[8];
	unsigned char keyLabel[64];
	unsigned long keyLabelLength = strlen(key_id);
	unsigned char modulus[512];
	long modulusFieldLength = sizeof(modulus);
	long modulusLength = 0;
	unsigned char exponent[512];
	long exponentLength = sizeof(exponent);

	if (keyLabelLength > sizeof(keyLabel))
		{
		CCA4758err(CCA4758_F_IBM_4758_LOAD_PUBKEY,
			CCA4758_R_SIZE_TOO_LARGE_OR_TOO_SMALL);
		return NULL;
		}

	memset(keyLabel,' ', sizeof(keyLabel));
	memcpy(keyLabel, key_id, keyLabelLength);

	keyToken = OPENSSL_malloc(MAX_CCA_PKA_TOKEN_SIZE + sizeof(long));
	if (!keyToken)
		{
		CCA4758err(CCA4758_F_IBM_4758_LOAD_PUBKEY,
				ERR_R_MALLOC_FAILURE);
		goto err;
		}

	keyRecordRead(&returnCode, &reasonCode, &exitDataLength, exitData,
		&ruleArrayLength, ruleArray, keyLabel, &keyTokenLength,
		keyToken+sizeof(long));

	if (returnCode)
		{
		CCA4758err(CCA4758_F_IBM_4758_LOAD_PUBKEY,
				ERR_R_MALLOC_FAILURE);
		goto err;
		}

	if (!getModulusAndExponent(keyToken+sizeof(long), &exponentLength,
			exponent, &modulusLength, &modulusFieldLength, modulus))
		{
		CCA4758err(CCA4758_F_IBM_4758_LOAD_PUBKEY,
			CCA4758_R_FAILED_LOADING_PUBLIC_KEY);
		goto err;
		}

	(*(long*)keyToken) = keyTokenLength;
	rtmp = RSA_new_method(e);
	RSA_set_ex_data(rtmp, hndidx, (char *)keyToken);
	rtmp->e = BN_bin2bn(exponent, exponentLength, NULL);
	rtmp->n = BN_bin2bn(modulus, modulusFieldLength, NULL);
	rtmp->flags |= RSA_FLAG_EXT_PKEY;
	res = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(res, rtmp);

	return res;
err:
	if (keyToken)
		OPENSSL_free(keyToken);
	return NULL;
	}

static int cca_rsa_pub_enc(int flen, const unsigned char *from,
			unsigned char *to, RSA *rsa,int padding)
	{
	long returnCode;
	long reasonCode;
	long lflen = flen;
	long exitDataLength = 0;
	unsigned char exitData[8];
	long ruleArrayLength = 1;
	unsigned char ruleArray[8] = "PKCS-1.2";
	long dataStructureLength = 0;
	unsigned char dataStructure[8];
	long outputLength = RSA_size(rsa);
	long keyTokenLength;
	unsigned char* keyToken = (unsigned char*)RSA_get_ex_data(rsa, hndidx);

	keyTokenLength = *(long*)keyToken;
	keyToken+=sizeof(long);

	pkaEncrypt(&returnCode, &reasonCode, &exitDataLength, exitData,
		&ruleArrayLength, ruleArray, &lflen, (unsigned char*)from,
		&dataStructureLength, dataStructure, &keyTokenLength,
		keyToken, &outputLength, to);

	if (returnCode || reasonCode)
		return -(returnCode << 16 | reasonCode);
	return outputLength;
	}

static int cca_rsa_priv_dec(int flen, const unsigned char *from,
			unsigned char *to, RSA *rsa,int padding)
	{
	long returnCode;
	long reasonCode;
	long lflen = flen;
	long exitDataLength = 0;
	unsigned char exitData[8];
	long ruleArrayLength = 1;
	unsigned char ruleArray[8] = "PKCS-1.2";
	long dataStructureLength = 0;
	unsigned char dataStructure[8];
	long outputLength = RSA_size(rsa);
	long keyTokenLength;
	unsigned char* keyToken = (unsigned char*)RSA_get_ex_data(rsa, hndidx);

	keyTokenLength = *(long*)keyToken;
	keyToken+=sizeof(long);

	pkaDecrypt(&returnCode, &reasonCode, &exitDataLength, exitData,
		&ruleArrayLength, ruleArray, &lflen, (unsigned char*)from,
		&dataStructureLength, dataStructure, &keyTokenLength,
		keyToken, &outputLength, to);

	return (returnCode | reasonCode) ? 0 : 1;
	}

#define SSL_SIG_LEN 36

static int cca_rsa_verify(int type, const unsigned char *m, unsigned int m_len,
	const unsigned char *sigbuf, unsigned int siglen, const RSA *rsa)
	{
	long returnCode;
	long reasonCode;
	long lsiglen = siglen;
	long exitDataLength = 0;
	unsigned char exitData[8];
	long ruleArrayLength = 1;
	unsigned char ruleArray[8] = "PKCS-1.1";
	long keyTokenLength;
	unsigned char* keyToken = (unsigned char*)RSA_get_ex_data(rsa, hndidx);
	long length = SSL_SIG_LEN;
	long keyLength ;
	unsigned char *hashBuffer = NULL;
	X509_SIG sig;
	ASN1_TYPE parameter;
	X509_ALGOR algorithm;
	ASN1_OCTET_STRING digest;

	keyTokenLength = *(long*)keyToken;
	keyToken+=sizeof(long);

	if (type == NID_md5 || type == NID_sha1)
		{
		sig.algor = &algorithm;
		algorithm.algorithm = OBJ_nid2obj(type);

		if (!algorithm.algorithm)
			{
			CCA4758err(CCA4758_F_CCA_RSA_VERIFY,
				CCA4758_R_UNKNOWN_ALGORITHM_TYPE);
			return 0;
			}

		if (!algorithm.algorithm->length)
			{
			CCA4758err(CCA4758_F_CCA_RSA_VERIFY,
				CCA4758_R_ASN1_OID_UNKNOWN_FOR_MD);
			return 0;
			}

		parameter.type = V_ASN1_NULL;
		parameter.value.ptr = NULL;
		algorithm.parameter = &parameter;

		sig.digest = &digest;
		sig.digest->data = (unsigned char*)m;
		sig.digest->length = m_len;

		length = i2d_X509_SIG(&sig, NULL);
		}

	keyLength = RSA_size(rsa);

	if (length - RSA_PKCS1_PADDING > keyLength)
		{
		CCA4758err(CCA4758_F_CCA_RSA_VERIFY,
			CCA4758_R_SIZE_TOO_LARGE_OR_TOO_SMALL);
		return 0;
		}

	switch (type)
		{
		case NID_md5_sha1 :
			if (m_len != SSL_SIG_LEN)
				{
				CCA4758err(CCA4758_F_CCA_RSA_VERIFY,
				CCA4758_R_SIZE_TOO_LARGE_OR_TOO_SMALL);
				return 0;
				}

			hashBuffer = (unsigned char *)m;
			length = m_len;
			break;
		case NID_md5 :
			{
			unsigned char *ptr;
			ptr = hashBuffer = OPENSSL_malloc(
					(unsigned int)keyLength+1);
			if (!hashBuffer)
				{
				CCA4758err(CCA4758_F_CCA_RSA_VERIFY,
						ERR_R_MALLOC_FAILURE);
				return 0;
				}

			i2d_X509_SIG(&sig, &ptr);
			}
			break;
		case NID_sha1 :
			{
			unsigned char *ptr;
			ptr = hashBuffer = OPENSSL_malloc(
					(unsigned int)keyLength+1);
			if (!hashBuffer)
				{
				CCA4758err(CCA4758_F_CCA_RSA_VERIFY,
						ERR_R_MALLOC_FAILURE);
				return 0;
				}
			i2d_X509_SIG(&sig, &ptr);
			}
			break;
		default:
			return 0;
		}

	digitalSignatureVerify(&returnCode, &reasonCode, &exitDataLength,
		exitData, &ruleArrayLength, ruleArray, &keyTokenLength,
		keyToken, &length, hashBuffer, &lsiglen,
						(unsigned char *)sigbuf);

	if (type == NID_sha1 || type == NID_md5)
		{
		OPENSSL_cleanse(hashBuffer, keyLength+1);
		OPENSSL_free(hashBuffer);
		}

	return ((returnCode || reasonCode) ? 0 : 1);
	}

#define SSL_SIG_LEN 36

static int cca_rsa_sign(int type, const unsigned char *m, unsigned int m_len,
		unsigned char *sigret, unsigned int *siglen, const RSA *rsa)
	{
	long returnCode;
	long reasonCode;
	long exitDataLength = 0;
	unsigned char exitData[8];
	long ruleArrayLength = 1;
	unsigned char ruleArray[8] = "PKCS-1.1";
	long outputLength=256;
	long outputBitLength;
	long keyTokenLength;
	unsigned char *hashBuffer = NULL;
	unsigned char* keyToken = (unsigned char*)RSA_get_ex_data(rsa, hndidx);
	long length = SSL_SIG_LEN;
	long keyLength ;
	X509_SIG sig;
	ASN1_TYPE parameter;
	X509_ALGOR algorithm;
	ASN1_OCTET_STRING digest;

	keyTokenLength = *(long*)keyToken;
	keyToken+=sizeof(long);

	if (type == NID_md5 || type == NID_sha1)
		{
		sig.algor = &algorithm;
		algorithm.algorithm = OBJ_nid2obj(type);

		if (!algorithm.algorithm)
			{
			CCA4758err(CCA4758_F_CCA_RSA_SIGN,
				CCA4758_R_UNKNOWN_ALGORITHM_TYPE);
			return 0;
			}

		if (!algorithm.algorithm->length)
			{
			CCA4758err(CCA4758_F_CCA_RSA_SIGN,
				CCA4758_R_ASN1_OID_UNKNOWN_FOR_MD);
			return 0;
			}

		parameter.type = V_ASN1_NULL;
		parameter.value.ptr = NULL;
		algorithm.parameter = &parameter;

		sig.digest = &digest;
		sig.digest->data = (unsigned char*)m;
		sig.digest->length = m_len;

		length = i2d_X509_SIG(&sig, NULL);
		}

	keyLength = RSA_size(rsa);

	if (length - RSA_PKCS1_PADDING > keyLength)
		{
		CCA4758err(CCA4758_F_CCA_RSA_SIGN,
			CCA4758_R_SIZE_TOO_LARGE_OR_TOO_SMALL);
		return 0;
		}

	switch (type)
		{
		case NID_md5_sha1 :
			if (m_len != SSL_SIG_LEN)
				{
				CCA4758err(CCA4758_F_CCA_RSA_SIGN,
				CCA4758_R_SIZE_TOO_LARGE_OR_TOO_SMALL);
				return 0;
				}
			hashBuffer = (unsigned char*)m;
			length = m_len;
			break;
		case NID_md5 :
			{
			unsigned char *ptr;
			ptr = hashBuffer = OPENSSL_malloc(
					(unsigned int)keyLength+1);
			if (!hashBuffer)
				{
				CCA4758err(CCA4758_F_CCA_RSA_SIGN,
						ERR_R_MALLOC_FAILURE);
				return 0;
				}
			i2d_X509_SIG(&sig, &ptr);
			}
			break;
		case NID_sha1 :
			{
			unsigned char *ptr;
			ptr = hashBuffer = OPENSSL_malloc(
					(unsigned int)keyLength+1);
			if (!hashBuffer)
				{
				CCA4758err(CCA4758_F_CCA_RSA_SIGN,
						ERR_R_MALLOC_FAILURE);
				return 0;
				}
			i2d_X509_SIG(&sig, &ptr);
			}
			break;
		default:
			return 0;
		}

	digitalSignatureGenerate(&returnCode, &reasonCode, &exitDataLength,
		exitData, &ruleArrayLength, ruleArray, &keyTokenLength,
		keyToken, &length, hashBuffer, &outputLength, &outputBitLength,
		sigret);

	if (type == NID_sha1 || type == NID_md5)
		{
		OPENSSL_cleanse(hashBuffer, keyLength+1);
		OPENSSL_free(hashBuffer);
		}

	*siglen = outputLength;

	return ((returnCode || reasonCode) ? 0 : 1);
	}

static int getModulusAndExponent(const unsigned char*token, long *exponentLength,
		unsigned char *exponent, long *modulusLength, long *modulusFieldLength,
		unsigned char *modulus)
	{
	unsigned long len;

	if (*token++ != (char)0x1E) /* internal PKA token? */
		return 0;

	if (*token++) /* token version must be zero */
		return 0;

	len = *token++;
	len = len << 8;
	len |= (unsigned char)*token++;

	token += 4; /* skip reserved bytes */

	if (*token++ == (char)0x04)
		{
		if (*token++) /* token version must be zero */
			return 0;

		len = *token++;
		len = len << 8;
		len |= (unsigned char)*token++;

		token+=2; /* skip reserved section */

		len = *token++;
		len = len << 8;
		len |= (unsigned char)*token++;

		*exponentLength = len;

		len = *token++;
		len = len << 8;
		len |= (unsigned char)*token++;

		*modulusLength = len;

		len = *token++;
		len = len << 8;
		len |= (unsigned char)*token++;

		*modulusFieldLength = len;

		memcpy(exponent, token, *exponentLength);
		token+= *exponentLength;

		memcpy(modulus, token, *modulusFieldLength);
		return 1;
		}
	return 0;
	}

#endif /* OPENSSL_NO_RSA */

static int cca_random_status(void)
	{
	return 1;
	}

static int cca_get_random_bytes(unsigned char* buf, int num)
	{
	long ret_code;
	long reason_code;
	long exit_data_length;
	unsigned char exit_data[4];
	unsigned char form[] = "RANDOM  ";
	unsigned char rand_buf[8];

	while(num >= (int)sizeof(rand_buf))
		{
		randomNumberGenerate(&ret_code, &reason_code, &exit_data_length,
			exit_data, form, rand_buf);
		if (ret_code)
			return 0;
		num -= sizeof(rand_buf);
		memcpy(buf, rand_buf, sizeof(rand_buf));
		buf += sizeof(rand_buf);
		}

	if (num)
		{
		randomNumberGenerate(&ret_code, &reason_code, NULL, NULL,
			form, rand_buf);
		if (ret_code)
			return 0;
		memcpy(buf, rand_buf, num);
		}

	return 1;
	}

#ifndef OPENSSL_NO_RSA
static void cca_ex_free(void *obj, void *item, CRYPTO_EX_DATA *ad, int idx,
		long argl, void *argp)
	{
	if (item)
		OPENSSL_free(item);
	}
#endif

/* Goo to handle building as a dynamic engine */
#ifndef OPENSSL_NO_DYNAMIC_ENGINE 
static int bind_fn(ENGINE *e, const char *id)
	{
	if(id && (strcmp(id, engine_4758_cca_id) != 0) &&
			(strcmp(id, engine_4758_cca_id_alt) != 0))
		return 0;
	if(!bind_helper(e))
		return 0;
	return 1;
	}       
IMPLEMENT_DYNAMIC_CHECK_FN()
IMPLEMENT_DYNAMIC_BIND_FN(bind_fn)
#endif /* OPENSSL_NO_DYNAMIC_ENGINE */

#endif /* !OPENSSL_NO_HW_4758_CCA */
#endif /* !OPENSSL_NO_HW */
