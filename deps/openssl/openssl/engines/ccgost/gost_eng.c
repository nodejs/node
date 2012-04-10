/**********************************************************************
 *                          gost_eng.c                                *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *         This file is distributed under the same license as OpenSSL *
 *                                                                    *
 *              Main file of GOST engine                              *
 *       for OpenSSL                                                  *
 *          Requires OpenSSL 0.9.9 for compilation                    *
 **********************************************************************/
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/engine.h>
#include <openssl/obj_mac.h>
#include "e_gost_err.h"
#include "gost_lcl.h"
static const char *engine_gost_id = "gost";
static const char *engine_gost_name = "Reference implementation of GOST engine";

/* Symmetric cipher and digest function registrar */

static int gost_ciphers(ENGINE *e, const EVP_CIPHER **cipher,
	const int **nids, int nid);

static int gost_digests(ENGINE *e, const EVP_MD **digest,
	const int **nids, int ind);

static int gost_pkey_meths (ENGINE *e, EVP_PKEY_METHOD **pmeth,
	const int **nids, int nid);

static int gost_pkey_asn1_meths (ENGINE *e, EVP_PKEY_ASN1_METHOD **ameth,
	const int **nids, int nid);

static int gost_cipher_nids[] =
    {NID_id_Gost28147_89, NID_gost89_cnt,0};

static int gost_digest_nids[] =
	{NID_id_GostR3411_94,NID_id_Gost28147_89_MAC, 0};

static int gost_pkey_meth_nids[] = 
	{NID_id_GostR3410_94,
	 NID_id_GostR3410_2001, NID_id_Gost28147_89_MAC, 0};

static EVP_PKEY_METHOD *pmeth_GostR3410_94 = NULL,
	 *pmeth_GostR3410_2001 = NULL,
	*pmeth_Gost28147_MAC = NULL;

static EVP_PKEY_ASN1_METHOD *ameth_GostR3410_94 = NULL,
	*ameth_GostR3410_2001 = NULL,
	*ameth_Gost28147_MAC = NULL;


static int gost_engine_init(ENGINE *e)
	{ 
	return 1;
	}

static int gost_engine_finish(ENGINE *e)
	{ 
	return 1;
	}

static int gost_engine_destroy(ENGINE *e)
	{ 
	gost_param_free();
	return 1;
	}

static int bind_gost (ENGINE *e,const char *id) 
	{
	int ret = 0;
	if (id && strcmp(id, engine_gost_id)) return 0;

	if (!ENGINE_set_id(e, engine_gost_id)) 
		{
		printf("ENGINE_set_id failed\n"); 
		goto end;
		}	
	if (!ENGINE_set_name(e, engine_gost_name)) 
		{
		printf("ENGINE_set_name failed\n");
		goto end;
		}	
	if (!ENGINE_set_digests(e, gost_digests)) 
		{
		printf("ENGINE_set_digests failed\n");
		goto end;
		}	
	if (! ENGINE_set_ciphers(e, gost_ciphers)) 
		{
		printf("ENGINE_set_ciphers failed\n");
		goto end;
		}	
	if (! ENGINE_set_pkey_meths(e, gost_pkey_meths)) 
		{
		printf("ENGINE_set_pkey_meths failed\n");
		goto end;
		}	
	if (! ENGINE_set_pkey_asn1_meths(e, gost_pkey_asn1_meths)) 
		{
		printf("ENGINE_set_pkey_asn1_meths failed\n");
		goto end;
		}	
	/* Control function and commands */
	if (!ENGINE_set_cmd_defns(e,gost_cmds)) 
		{
		fprintf(stderr,"ENGINE_set_cmd_defns failed\n");
		goto end;
		}	
	if (!ENGINE_set_ctrl_function(e,gost_control_func)) 
		{
		fprintf(stderr,"ENGINE_set_ctrl_func failed\n");
		goto end;
		}	
	if ( ! ENGINE_set_destroy_function(e, gost_engine_destroy)
		|| ! ENGINE_set_init_function(e,gost_engine_init)
		|| ! ENGINE_set_finish_function(e,gost_engine_finish))
		{
		goto end;
		}

	if (!register_ameth_gost(NID_id_GostR3410_94, &ameth_GostR3410_94, "GOST94", "GOST R 34.10-94")) goto end;
	if (!register_ameth_gost(NID_id_GostR3410_2001, &ameth_GostR3410_2001, "GOST2001", "GOST R 34.10-2001")) goto end;
	if (!register_ameth_gost(NID_id_Gost28147_89_MAC, &ameth_Gost28147_MAC,
		"GOST-MAC", "GOST 28147-89 MAC")) goto end;

	if (!register_pmeth_gost(NID_id_GostR3410_94, &pmeth_GostR3410_94, 0)) goto end;
	if (!register_pmeth_gost(NID_id_GostR3410_2001, &pmeth_GostR3410_2001, 0)) goto end;
	if (!register_pmeth_gost(NID_id_Gost28147_89_MAC, &pmeth_Gost28147_MAC, 0))
		goto end;
	if ( ! ENGINE_register_ciphers(e)
		|| ! ENGINE_register_digests(e)
		|| ! ENGINE_register_pkey_meths(e)
		/* These two actually should go in LIST_ADD command */
		|| ! EVP_add_cipher(&cipher_gost)
		|| ! EVP_add_cipher(&cipher_gost_cpacnt)
		|| ! EVP_add_digest(&digest_gost)
		|| ! EVP_add_digest(&imit_gost_cpa)
		)
		{
		goto end;
		}

	ERR_load_GOST_strings();
	ret = 1;
	end:
	return ret;
	}	

#ifndef OPENSSL_NO_DYNAMIC_ENGINE
IMPLEMENT_DYNAMIC_BIND_FN(bind_gost)
IMPLEMENT_DYNAMIC_CHECK_FN()
#endif  /* ndef OPENSSL_NO_DYNAMIC_ENGINE */

static int gost_digests(ENGINE *e, const EVP_MD **digest,
	const int **nids, int nid)
	{ 
	int ok =1 ;
	if (!digest) 
		{
		*nids = gost_digest_nids;
		return 2; 
		}
	/*printf("Digest no %d requested\n",nid);*/
	if(nid == NID_id_GostR3411_94) 
		{
		*digest = &digest_gost;
		}
	else if (nid == NID_id_Gost28147_89_MAC) 
		{
		*digest = &imit_gost_cpa;
		}
	else
		{
		ok =0;
		*digest = NULL;
		}
	return ok;
	}	
	
static int gost_ciphers (ENGINE *e,const EVP_CIPHER **cipher,
	const int **nids, int nid) 
	{
	int ok = 1;
	if (!cipher) 
		{
		*nids = gost_cipher_nids;
		return 2; /* two ciphers are supported */
		}

	if(nid == NID_id_Gost28147_89) 
		{
		*cipher = &cipher_gost;
		}
	else if  (nid == NID_gost89_cnt) 
		{
		*cipher = &cipher_gost_cpacnt;
		}
	else	
		{
		ok = 0;
		*cipher = NULL;
		}
	return ok;
	}	

static int gost_pkey_meths (ENGINE *e, EVP_PKEY_METHOD **pmeth,
	const int **nids, int nid)
	{
	if (!pmeth) 
		{
		*nids = gost_pkey_meth_nids;
		return 3;
		}

	switch (nid) 
		{
		case NID_id_GostR3410_94: *pmeth = pmeth_GostR3410_94; return 1;
		case NID_id_GostR3410_2001: *pmeth = pmeth_GostR3410_2001; return 1;
		case NID_id_Gost28147_89_MAC: *pmeth = pmeth_Gost28147_MAC; return 1;
		default:;
		}
	
	*pmeth = NULL;
	return 0;
	}

static int gost_pkey_asn1_meths (ENGINE *e, EVP_PKEY_ASN1_METHOD **ameth,
	const int **nids, int nid)
	{
	if (!ameth) 
		{
		*nids = gost_pkey_meth_nids;
		return 3;
		}
	switch (nid) 
		{
		case NID_id_GostR3410_94: *ameth = ameth_GostR3410_94; return 1;
		case NID_id_GostR3410_2001: *ameth = ameth_GostR3410_2001; return 1;
		case NID_id_Gost28147_89_MAC: *ameth = ameth_Gost28147_MAC; return 1;
	
		default:;
		}
	
	*ameth = NULL;
	return 0;
	}

#ifdef OPENSSL_NO_DYNAMIC_ENGINE
static ENGINE *engine_gost(void)
	{	
	ENGINE *ret = ENGINE_new();
	if (!ret)
		return NULL;
	if (!bind_gost(ret,engine_gost_id)) 
		{
		ENGINE_free(ret);
		return NULL;
		}
	return ret;
	}
	
void ENGINE_load_gost(void)
	{
	ENGINE *toadd =engine_gost();
	if (!toadd) return;
	ENGINE_add(toadd);
	ENGINE_free(toadd);
	ERR_clear_error();
	}
#endif	

