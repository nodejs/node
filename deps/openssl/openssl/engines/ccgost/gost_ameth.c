/**********************************************************************
 *                          gost_ameth.c                              *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *         This file is distributed under the same license as OpenSSL *
 *                                                                    *
 *       Implementation of RFC 4490/4491 ASN1 method                  *
 *       for OpenSSL                                                  *
 *          Requires OpenSSL 0.9.9 for compilation                    *
 **********************************************************************/
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/evp.h>
#include <openssl/asn1.h>
#include "gost_params.h"
#include "gost_lcl.h"
#include "e_gost_err.h"

int gost94_nid_by_params(DSA *p) 
	{
	R3410_params *gost_params;
	BIGNUM *q=BN_new();
	for (gost_params = R3410_paramset;gost_params->q!=NULL; gost_params++) 
		{
		BN_dec2bn(&q,gost_params->q);
		if (!BN_cmp(q,p->q)) 
			{
			BN_free(q);
			return gost_params->nid;
			}
		}	
	BN_free(q);
	return NID_undef;
	}

static ASN1_STRING  *encode_gost_algor_params(const EVP_PKEY *key)
	{
	ASN1_STRING *params = ASN1_STRING_new();
	GOST_KEY_PARAMS *gkp = GOST_KEY_PARAMS_new();
	int pkey_param_nid = NID_undef;

	if (!params || !gkp) 
		{
		GOSTerr(GOST_F_ENCODE_GOST_ALGOR_PARAMS,
			ERR_R_MALLOC_FAILURE);
		ASN1_STRING_free(params);
		params = NULL;
		goto err;
		}	
	switch (EVP_PKEY_base_id(key)) 
		{
		case NID_id_GostR3410_2001:
			pkey_param_nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(EVP_PKEY_get0((EVP_PKEY *)key)));
			break;
		case NID_id_GostR3410_94:
			pkey_param_nid = (int) gost94_nid_by_params(EVP_PKEY_get0((EVP_PKEY *)key));
			if (pkey_param_nid == NID_undef) 
				{
				GOSTerr(GOST_F_ENCODE_GOST_ALGOR_PARAMS,
					GOST_R_INVALID_GOST94_PARMSET);
				ASN1_STRING_free(params);
				params=NULL;
				goto err;
				}	
			break;
		}	
	gkp->key_params = OBJ_nid2obj(pkey_param_nid);
	gkp->hash_params = OBJ_nid2obj(NID_id_GostR3411_94_CryptoProParamSet);
	/*gkp->cipher_params = OBJ_nid2obj(cipher_param_nid);*/
	params->length = i2d_GOST_KEY_PARAMS(gkp, &params->data);
	if (params->length <=0 ) 
		{
		GOSTerr(GOST_F_ENCODE_GOST_ALGOR_PARAMS,
			ERR_R_MALLOC_FAILURE);
		ASN1_STRING_free(params);
		params = NULL;
		goto err;
		}
	params ->type = V_ASN1_SEQUENCE;
	err:
	GOST_KEY_PARAMS_free(gkp);
	return params;
	}

/* Parses GOST algorithm parameters from X509_ALGOR and
 * modifies pkey setting NID and parameters
 */
static int decode_gost_algor_params(EVP_PKEY *pkey, X509_ALGOR *palg) 
	{
	ASN1_OBJECT *palg_obj =NULL;
	int ptype = V_ASN1_UNDEF;
	int pkey_nid = NID_undef,param_nid = NID_undef;
        void *_pval;
	ASN1_STRING *pval = NULL;
	const unsigned char  *p;
	GOST_KEY_PARAMS *gkp = NULL;

	X509_ALGOR_get0(&palg_obj, &ptype, &_pval, palg);
        pval = _pval;
	if (ptype != V_ASN1_SEQUENCE) 
		{
		GOSTerr(GOST_F_DECODE_GOST_ALGOR_PARAMS,
			GOST_R_BAD_KEY_PARAMETERS_FORMAT);
		return 0;
		}	
	p=pval->data;
	pkey_nid = OBJ_obj2nid(palg_obj);

	gkp = d2i_GOST_KEY_PARAMS(NULL,&p,pval->length);
	if (!gkp) 
		{
		GOSTerr(GOST_F_DECODE_GOST_ALGOR_PARAMS,
			GOST_R_BAD_PKEY_PARAMETERS_FORMAT);
		return 0;
		}	
	param_nid = OBJ_obj2nid(gkp->key_params);
	GOST_KEY_PARAMS_free(gkp);
	EVP_PKEY_set_type(pkey,pkey_nid);
	switch (pkey_nid) 
		{
		case NID_id_GostR3410_94:
		{
		DSA *dsa= EVP_PKEY_get0(pkey);
		if (!dsa) 
			{
			dsa = DSA_new();
			if (!EVP_PKEY_assign(pkey,pkey_nid,dsa)) return 0;
			}
		if (!fill_GOST94_params(dsa,param_nid)) return 0;
		break;
		}
		case NID_id_GostR3410_2001:
		{
		EC_KEY *ec = EVP_PKEY_get0(pkey);
		if (!ec) 
			{
			ec = EC_KEY_new();
			if (!EVP_PKEY_assign(pkey,pkey_nid,ec)) return 0;
			}
		if (!fill_GOST2001_params(ec,param_nid)) return 0;
		}
		}

	return 1;
	}

static int gost_set_priv_key(EVP_PKEY *pkey,BIGNUM *priv) 
	{
	switch (EVP_PKEY_base_id(pkey)) 
		{
		case NID_id_GostR3410_94:
		{
		DSA *dsa = EVP_PKEY_get0(pkey);
		if (!dsa) 
			{
			dsa = DSA_new();
			EVP_PKEY_assign(pkey,EVP_PKEY_base_id(pkey),dsa);
			}	
		dsa->priv_key = BN_dup(priv);
		if (!EVP_PKEY_missing_parameters(pkey)) 
			gost94_compute_public(dsa);
		break;
		}	
		case NID_id_GostR3410_2001:
		{
		EC_KEY *ec = EVP_PKEY_get0(pkey);
		if (!ec) 
			{
			ec = EC_KEY_new();
			EVP_PKEY_assign(pkey,EVP_PKEY_base_id(pkey),ec);
			}	
		if (!EC_KEY_set_private_key(ec,priv)) return 0;
		if (!EVP_PKEY_missing_parameters(pkey)) 
			gost2001_compute_public(ec);
		break;
		}
		}
	return 1;		
	}
BIGNUM* gost_get0_priv_key(const EVP_PKEY *pkey) 
	{
	switch (EVP_PKEY_base_id(pkey)) 
		{
		case NID_id_GostR3410_94:
		{
		DSA *dsa = EVP_PKEY_get0((EVP_PKEY *)pkey);
		if (!dsa) 
			{
			return NULL;
			}	
		if (!dsa->priv_key) return NULL;
		return dsa->priv_key;
		break;
		}	
		case NID_id_GostR3410_2001:
		{
		EC_KEY *ec = EVP_PKEY_get0((EVP_PKEY *)pkey);
		const BIGNUM* priv;
		if (!ec) 
			{
			return NULL;
			}	
		if (!(priv=EC_KEY_get0_private_key(ec))) return NULL;
		return (BIGNUM *)priv;
		break;
		}
		}
	return NULL;		
	}

static int pkey_ctrl_gost(EVP_PKEY *pkey, int op,
	long arg1, void *arg2)
	{
	switch (op)
		{
		case ASN1_PKEY_CTRL_PKCS7_SIGN:
			if (arg1 == 0) 
				{
				X509_ALGOR *alg1 = NULL, *alg2 = NULL;
				int nid = EVP_PKEY_base_id(pkey);
				PKCS7_SIGNER_INFO_get0_algs((PKCS7_SIGNER_INFO*)arg2, 
					NULL, &alg1, &alg2);
				X509_ALGOR_set0(alg1, OBJ_nid2obj(NID_id_GostR3411_94),
					V_ASN1_NULL, 0);
				if (nid == NID_undef) 
					{
					return (-1);
					}
				X509_ALGOR_set0(alg2, OBJ_nid2obj(nid), V_ASN1_NULL, 0);
				}
			return 1;
		case ASN1_PKEY_CTRL_PKCS7_ENCRYPT:
			if (arg1 == 0)
				{
				X509_ALGOR *alg;
				ASN1_STRING * params = encode_gost_algor_params(pkey);
				if (!params) 
					{
					return -1;
					}
				PKCS7_RECIP_INFO_get0_alg((PKCS7_RECIP_INFO*)arg2, &alg);
				X509_ALGOR_set0(alg, OBJ_nid2obj(pkey->type),
					V_ASN1_SEQUENCE, params);
				}
			return 1;
		case ASN1_PKEY_CTRL_DEFAULT_MD_NID:
			*(int *)arg2 = NID_id_GostR3411_94;
			return 2;
		}
	
	return -2;
	}
/*----------------------- free functions * ------------------------------*/
static void pkey_free_gost94(EVP_PKEY *key) 
	{
	if (key->pkey.dsa) 
		{
		DSA_free(key->pkey.dsa);
		}
	}

static void pkey_free_gost01(EVP_PKEY *key) 
	{
	if (key->pkey.ec) 
		{
		EC_KEY_free(key->pkey.ec);
		}
	}	

/* ------------------ private key functions  -----------------------------*/
static int priv_decode_gost( EVP_PKEY *pk, PKCS8_PRIV_KEY_INFO *p8inf) 
	{
	const unsigned char *pkey_buf = NULL,*p=NULL;
	int priv_len = 0;
	BIGNUM *pk_num=NULL;
	int ret =0;
	X509_ALGOR *palg =NULL;
	ASN1_OBJECT *palg_obj = NULL;
	ASN1_INTEGER *priv_key=NULL;

	if (!PKCS8_pkey_get0(&palg_obj,&pkey_buf,&priv_len,&palg,p8inf)) 
		return 0;
	p = pkey_buf;
	if (!decode_gost_algor_params(pk,palg)) 
		{
		return 0;
		}
	if (V_ASN1_OCTET_STRING == *p) 
		{
		/* New format - Little endian octet string */
		unsigned char rev_buf[32];
		int i;
		ASN1_OCTET_STRING *s = d2i_ASN1_OCTET_STRING(NULL,&p,priv_len);
		if (!s||s->length !=32) 
			{
			GOSTerr(GOST_F_PRIV_DECODE_GOST,
				EVP_R_DECODE_ERROR);
			return 0;	
			}
		for (i=0;i<32;i++)
			{
			rev_buf[31-i]=s->data[i];
			}
		ASN1_STRING_free(s);
		pk_num = getbnfrombuf(rev_buf,32);
		} 
	else
		{
		priv_key=d2i_ASN1_INTEGER(NULL,&p,priv_len);
		if (!priv_key) return 0;
		ret= ((pk_num =  ASN1_INTEGER_to_BN(priv_key, NULL))!=NULL) ;
		ASN1_INTEGER_free(priv_key);
		if (!ret)
			{
			GOSTerr(GOST_F_PRIV_DECODE_GOST,
				EVP_R_DECODE_ERROR);
			return 0;	
			}
		}

	ret= gost_set_priv_key(pk,pk_num);
	BN_free(pk_num);
	return ret;
	}

/* ----------------------------------------------------------------------*/
static int priv_encode_gost(PKCS8_PRIV_KEY_INFO *p8, const EVP_PKEY *pk)
	{
	ASN1_OBJECT *algobj = OBJ_nid2obj(EVP_PKEY_base_id(pk));
	ASN1_STRING *params = encode_gost_algor_params(pk);
	unsigned char *priv_buf = NULL;
	int priv_len;

	ASN1_INTEGER *asn1key=NULL;
	if (!params) 
		{
		return 0;
		}
	asn1key = BN_to_ASN1_INTEGER(gost_get0_priv_key(pk),NULL);
	priv_len = i2d_ASN1_INTEGER(asn1key,&priv_buf);
	ASN1_INTEGER_free(asn1key);
	return PKCS8_pkey_set0(p8,algobj,0,V_ASN1_SEQUENCE,params,
		priv_buf,priv_len);
	}
/* --------- printing keys --------------------------------*/
static int print_gost_94(BIO *out, const EVP_PKEY *pkey, int indent,
	ASN1_PCTX *pctx, int type) 
	{
	int param_nid = NID_undef;

	if (type == 2) 
		{
		BIGNUM *key;

		if (!BIO_indent(out,indent,128)) return 0;
		BIO_printf(out,"Private key: ");
		key = gost_get0_priv_key(pkey);
		if (!key) 
			BIO_printf(out,"<undefined>");
		else 
			BN_print(out,key);
		BIO_printf(out,"\n");
		}
	if (type >= 1)
		{
		BIGNUM *pubkey;
		
		pubkey = ((DSA *)EVP_PKEY_get0((EVP_PKEY *)pkey))->pub_key;
		BIO_indent(out,indent,128);
		BIO_printf(out,"Public key: ");
		BN_print(out,pubkey);
		BIO_printf(out,"\n");
	}	

	param_nid = gost94_nid_by_params(EVP_PKEY_get0((EVP_PKEY *)pkey));
	BIO_indent(out,indent,128);
	BIO_printf(out, "Parameter set: %s\n",OBJ_nid2ln(param_nid));
	return 1;
}

static int param_print_gost94(BIO *out, const EVP_PKEY *pkey, int indent,
	ASN1_PCTX *pctx) 
	{
	return print_gost_94(out, pkey, indent, pctx,0);
	}

static int pub_print_gost94(BIO *out, const EVP_PKEY *pkey, int indent,
	ASN1_PCTX *pctx)
	{
	return print_gost_94(out,pkey, indent, pctx,1);
	}
static int priv_print_gost94(BIO *out,const EVP_PKEY *pkey, int indent,
	ASN1_PCTX *pctx) 
	{
	return print_gost_94(out,pkey,indent,pctx,2);
	}

static int print_gost_01(BIO *out, const EVP_PKEY *pkey, int indent,
	ASN1_PCTX *pctx, int type)
	{
	int param_nid = NID_undef;
	if (type == 2) 
		{
		BIGNUM *key;

		if (!BIO_indent(out,indent,128)) return 0;
		BIO_printf(out,"Private key: ");
		key = gost_get0_priv_key(pkey);
		if (!key) 
			BIO_printf(out,"<undefined)");
		else 
			BN_print(out,key);
		BIO_printf(out,"\n");
		}
	if (type >= 1) 
		{
		BN_CTX *ctx = BN_CTX_new();
		BIGNUM *X,*Y;
		const EC_POINT *pubkey;
		const EC_GROUP *group;

		if (!ctx) 
			{
			GOSTerr(GOST_F_PRINT_GOST_01,ERR_R_MALLOC_FAILURE);
			return 0;
			}
		BN_CTX_start(ctx);
		X = BN_CTX_get(ctx);
		Y = BN_CTX_get(ctx);
		pubkey = EC_KEY_get0_public_key((EC_KEY *)EVP_PKEY_get0((EVP_PKEY *)pkey));
		group = EC_KEY_get0_group((EC_KEY *)EVP_PKEY_get0((EVP_PKEY *)pkey));
		if (!EC_POINT_get_affine_coordinates_GFp(group,pubkey,X,Y,ctx)) 
			{
			GOSTerr(GOST_F_PRINT_GOST_01,ERR_R_EC_LIB);
			BN_CTX_free(ctx);
			return 0;
			}
		if (!BIO_indent(out,indent,128)) return 0;
		BIO_printf(out,"Public key:\n");
		if (!BIO_indent(out,indent+3,128)) return 0;
		BIO_printf(out,"X:");
		BN_print(out,X);
		BIO_printf(out,"\n");
		BIO_indent(out,indent+3,128);
		BIO_printf(out,"Y:");
		BN_print(out,Y);
		BIO_printf(out,"\n");
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
		}

	param_nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(EVP_PKEY_get0((EVP_PKEY *)pkey)));
	if (!BIO_indent(out,indent,128)) return 0;
	BIO_printf(out,"Parameter set: %s\n",OBJ_nid2ln(param_nid));
	return 1;
}
static int param_print_gost01(BIO *out, const EVP_PKEY *pkey, int indent,
	ASN1_PCTX *pctx) 
	{	
	return print_gost_01(out,pkey,indent,pctx,0);
	}
static int pub_print_gost01(BIO *out, const EVP_PKEY *pkey, int indent,
	ASN1_PCTX *pctx)
	{
	return print_gost_01(out,pkey, indent, pctx,1);
	}
static int priv_print_gost01(BIO *out,const EVP_PKEY *pkey, int indent,
	ASN1_PCTX *pctx) 
	{
	return print_gost_01(out,pkey,indent,pctx,2);
	}
/* ---------------------------------------------------------------------*/
static int param_missing_gost94(const EVP_PKEY *pk) 
	{
	const DSA *dsa = EVP_PKEY_get0((EVP_PKEY *)pk);
	if (!dsa) return 1;
	if (!dsa->q) return 1;
	return 0;
	}

static int param_missing_gost01(const EVP_PKEY *pk) 
	{
	const EC_KEY *ec = EVP_PKEY_get0((EVP_PKEY *)pk);
	if (!ec) return 1;
	if (!EC_KEY_get0_group(ec)) return 1;
	return 0;
	}

static int param_copy_gost94(EVP_PKEY *to, const EVP_PKEY *from) 
	{
	const DSA *dfrom = EVP_PKEY_get0((EVP_PKEY *)from);
	DSA *dto = EVP_PKEY_get0(to);
	if (EVP_PKEY_base_id(from) != EVP_PKEY_base_id(to)) 
		{
		GOSTerr(GOST_F_PARAM_COPY_GOST94,
			GOST_R_INCOMPATIBLE_ALGORITHMS);
		return 0;
		}	
	if (!dfrom) 
		{
		GOSTerr(GOST_F_PARAM_COPY_GOST94,
			GOST_R_KEY_PARAMETERS_MISSING);
		return 0;
		}	
	if (!dto) 
		{
		dto = DSA_new();
		EVP_PKEY_assign(to,EVP_PKEY_base_id(from),dto);
		}	
#define COPYBIGNUM(a,b,x) if (a->x) BN_free(a->x); a->x=BN_dup(b->x);	
	COPYBIGNUM(dto,dfrom,p)
		COPYBIGNUM(dto,dfrom,q)
		COPYBIGNUM(dto,dfrom,g)

		if (dto->priv_key) 
			gost94_compute_public(dto);
	return 1;	
	}
static int param_copy_gost01(EVP_PKEY *to, const EVP_PKEY *from) 
	{
	EC_KEY *eto = EVP_PKEY_get0(to);
	const EC_KEY *efrom = EVP_PKEY_get0((EVP_PKEY *)from);
	if (EVP_PKEY_base_id(from) != EVP_PKEY_base_id(to)) 
		{
		GOSTerr(GOST_F_PARAM_COPY_GOST01,
			GOST_R_INCOMPATIBLE_ALGORITHMS);
		return 0;
		}	
	if (!efrom) 
		{
		GOSTerr(GOST_F_PARAM_COPY_GOST01,
			GOST_R_KEY_PARAMETERS_MISSING);
		return 0;
		}	
	if (!eto) 
		{
		eto = EC_KEY_new();
		EVP_PKEY_assign(to,EVP_PKEY_base_id(from),eto);
		}	
	EC_KEY_set_group(eto,EC_KEY_get0_group(efrom));
	if (EC_KEY_get0_private_key(eto)) 
		{
		gost2001_compute_public(eto);
		}
	return 1;
	}

static int param_cmp_gost94(const EVP_PKEY *a, const EVP_PKEY *b) 
	{
	const DSA *da = EVP_PKEY_get0((EVP_PKEY *)a);
	const DSA *db = EVP_PKEY_get0((EVP_PKEY *)b);
	if (!BN_cmp(da->q,db->q)) return 1;
	return 0;
	}

static int param_cmp_gost01(const EVP_PKEY *a, const EVP_PKEY *b) 
	{
	if (EC_GROUP_get_curve_name(EC_KEY_get0_group(EVP_PKEY_get0((EVP_PKEY *)a)))==
		EC_GROUP_get_curve_name(EC_KEY_get0_group(EVP_PKEY_get0((EVP_PKEY *)b)))) 
		{
		return 1;
		}
	return 0;

	}

/* ---------- Public key functions * --------------------------------------*/
static int pub_decode_gost94(EVP_PKEY *pk, X509_PUBKEY *pub)
	{
	X509_ALGOR *palg = NULL;
	const unsigned char *pubkey_buf = NULL;
	unsigned char *databuf;
	ASN1_OBJECT *palgobj = NULL;
	int pub_len,i,j;
	DSA *dsa;
	ASN1_OCTET_STRING *octet= NULL;

	if (!X509_PUBKEY_get0_param(&palgobj,&pubkey_buf,&pub_len,
			&palg, pub)) return 0;
	EVP_PKEY_assign(pk,OBJ_obj2nid(palgobj),NULL);	
	if (!decode_gost_algor_params(pk,palg)) return 0;
	octet = d2i_ASN1_OCTET_STRING(NULL,&pubkey_buf,pub_len);
	if (!octet) 
		{
		GOSTerr(GOST_F_PUB_DECODE_GOST94,ERR_R_MALLOC_FAILURE);
		return 0;
		}	
	databuf = OPENSSL_malloc(octet->length);
	for (i=0,j=octet->length-1;i<octet->length;i++,j--)
		{
		databuf[j]=octet->data[i];
		}	
	dsa = EVP_PKEY_get0(pk);
	dsa->pub_key=BN_bin2bn(databuf,octet->length,NULL);
	ASN1_OCTET_STRING_free(octet);
	OPENSSL_free(databuf);
	return 1;

	}

static int pub_encode_gost94(X509_PUBKEY *pub,const EVP_PKEY *pk)
	{
	ASN1_OBJECT *algobj = NULL;
	ASN1_OCTET_STRING *octet = NULL;
	void *pval = NULL;
	unsigned char *buf=NULL,*databuf,*sptr;
	int i,j,data_len,ret=0;

	int ptype = V_ASN1_UNDEF;
	DSA *dsa = EVP_PKEY_get0((EVP_PKEY *)pk);
	algobj = OBJ_nid2obj(EVP_PKEY_base_id(pk));
	if (pk->save_parameters) 
		{
		ASN1_STRING *params = encode_gost_algor_params(pk);
		pval = params;
		ptype = V_ASN1_SEQUENCE;
		}	
	data_len = BN_num_bytes(dsa->pub_key);
	databuf = OPENSSL_malloc(data_len);
	BN_bn2bin(dsa->pub_key,databuf);
	octet = ASN1_OCTET_STRING_new();
	ASN1_STRING_set(octet,NULL,data_len);
	sptr = ASN1_STRING_data(octet);
	for (i=0,j=data_len-1; i< data_len;i++,j--)
		{
		sptr[i]=databuf[j];
		}
	OPENSSL_free(databuf);
	ret = i2d_ASN1_OCTET_STRING(octet,&buf);
	ASN1_BIT_STRING_free(octet);
	if (ret <0)  return 0;
	return X509_PUBKEY_set0_param(pub,algobj,ptype,pval,buf,ret);
	}

static int pub_decode_gost01(EVP_PKEY *pk,X509_PUBKEY *pub)
	{
	X509_ALGOR *palg = NULL;
	const unsigned char *pubkey_buf = NULL;
	unsigned char *databuf;
	ASN1_OBJECT *palgobj = NULL;
	int pub_len,i,j;
	EC_POINT *pub_key;
	BIGNUM *X,*Y;
	ASN1_OCTET_STRING *octet= NULL;
	int len;
	const EC_GROUP *group;

	if (!X509_PUBKEY_get0_param(&palgobj,&pubkey_buf,&pub_len,
			&palg, pub)) return 0;
	EVP_PKEY_assign(pk,OBJ_obj2nid(palgobj),NULL);	
	if (!decode_gost_algor_params(pk,palg)) return 0;
	group = EC_KEY_get0_group(EVP_PKEY_get0(pk));
	octet = d2i_ASN1_OCTET_STRING(NULL,&pubkey_buf,pub_len);
	if (!octet) 
		{
		GOSTerr(GOST_F_PUB_DECODE_GOST01,ERR_R_MALLOC_FAILURE);
		return 0;
		}	
	databuf = OPENSSL_malloc(octet->length);
	for (i=0,j=octet->length-1;i<octet->length;i++,j--)
		{
		databuf[j]=octet->data[i];
		}
	len=octet->length/2;
	ASN1_OCTET_STRING_free(octet);	
	
	Y= getbnfrombuf(databuf,len);
	X= getbnfrombuf(databuf+len,len);
	OPENSSL_free(databuf);
	pub_key = EC_POINT_new(group);
	if (!EC_POINT_set_affine_coordinates_GFp(group
			,pub_key,X,Y,NULL))
		{
		GOSTerr(GOST_F_PUB_DECODE_GOST01,
			ERR_R_EC_LIB);
		EC_POINT_free(pub_key);
		BN_free(X);
		BN_free(Y);
		return 0;
		}	
	BN_free(X);
	BN_free(Y);
	if (!EC_KEY_set_public_key(EVP_PKEY_get0(pk),pub_key))
		{
		GOSTerr(GOST_F_PUB_DECODE_GOST01,
			ERR_R_EC_LIB);
		EC_POINT_free(pub_key);
		return 0;
		}	
	EC_POINT_free(pub_key);
	return 1;

	}

static int pub_encode_gost01(X509_PUBKEY *pub,const EVP_PKEY *pk)
	{
	ASN1_OBJECT *algobj = NULL;
	ASN1_OCTET_STRING *octet = NULL;
	void *pval = NULL;
	unsigned char *buf=NULL,*databuf,*sptr;
	int i,j,data_len,ret=0;
	const EC_POINT *pub_key;
	BIGNUM *X,*Y,*order;
	const EC_KEY *ec = EVP_PKEY_get0((EVP_PKEY *)pk);
	int ptype = V_ASN1_UNDEF;

	algobj = OBJ_nid2obj(EVP_PKEY_base_id(pk));
	if (pk->save_parameters) 
		{
		ASN1_STRING *params = encode_gost_algor_params(pk);
		pval = params;
		ptype = V_ASN1_SEQUENCE;
		}
	order = BN_new();
	EC_GROUP_get_order(EC_KEY_get0_group(ec),order,NULL);
	pub_key=EC_KEY_get0_public_key(ec);
	if (!pub_key) 
		{
		GOSTerr(GOST_F_PUB_ENCODE_GOST01,
			GOST_R_PUBLIC_KEY_UNDEFINED);
		return 0;
		}	
	X=BN_new();
	Y=BN_new();
	EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(ec),
		pub_key,X,Y,NULL);
	data_len = 2*BN_num_bytes(order);
	BN_free(order);
	databuf = OPENSSL_malloc(data_len);
	memset(databuf,0,data_len);
	
	store_bignum(X,databuf+data_len/2,data_len/2);
	store_bignum(Y,databuf,data_len/2);

	BN_free(X);
	BN_free(Y);
	octet = ASN1_OCTET_STRING_new();
	ASN1_STRING_set(octet,NULL,data_len);
	sptr=ASN1_STRING_data(octet);
    for (i=0,j=data_len-1;i<data_len;i++,j--) 
		{
        sptr[i]=databuf[j];
		}
    OPENSSL_free(databuf);
	ret = i2d_ASN1_OCTET_STRING(octet,&buf);
	ASN1_BIT_STRING_free(octet);
	if (ret <0)  return 0;
	return X509_PUBKEY_set0_param(pub,algobj,ptype,pval,buf,ret);
	}

static int pub_cmp_gost94(const EVP_PKEY *a, const EVP_PKEY *b)
	{
	const DSA *da = EVP_PKEY_get0((EVP_PKEY *)a);
	const DSA *db = EVP_PKEY_get0((EVP_PKEY *)b);
	if (da && db && da->pub_key && db->pub_key
		&& !BN_cmp(da->pub_key,db->pub_key)) 
		{
		return 1;
		}		
	return 0;
	}

static int pub_cmp_gost01(const EVP_PKEY *a,const EVP_PKEY *b)
	{
	const EC_KEY *ea = EVP_PKEY_get0((EVP_PKEY *)a);
	const EC_KEY *eb = EVP_PKEY_get0((EVP_PKEY *)b);
	const EC_POINT *ka,*kb;
	int ret=0;
	if (!ea || !eb) return 0;
	ka = EC_KEY_get0_public_key(ea);
	kb = EC_KEY_get0_public_key(eb);
	if (!ka || !kb) return 0;
	ret = (0==EC_POINT_cmp(EC_KEY_get0_group(ea),ka,kb,NULL)) ;
	return ret;
	}




static int pkey_size_gost(const EVP_PKEY *pk)
	{
	return 64;
	}

static int pkey_bits_gost(const EVP_PKEY *pk)
	{
	return 256;
	}
/*------------------------ ASN1 METHOD for GOST MAC  -------------------*/
static void  mackey_free_gost(EVP_PKEY *pk)
	{
		if (pk->pkey.ptr) {
			OPENSSL_free(pk->pkey.ptr);
		}	
	}
static int mac_ctrl_gost(EVP_PKEY *pkey, int op, long arg1, void *arg2)
{
	switch (op)
		{
		case ASN1_PKEY_CTRL_DEFAULT_MD_NID:
			*(int *)arg2 = NID_id_Gost28147_89_MAC;
			return 2;
		}
	return -2;
}	

static int gost94_param_encode(const EVP_PKEY *pkey, unsigned char **pder) 
{
   int nid=gost94_nid_by_params(EVP_PKEY_get0((EVP_PKEY *)pkey));
   return i2d_ASN1_OBJECT(OBJ_nid2obj(nid),pder);
}
static int gost2001_param_encode(const EVP_PKEY *pkey, unsigned char **pder) 
{
   int nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(EVP_PKEY_get0((EVP_PKEY *)pkey)));
   return i2d_ASN1_OBJECT(OBJ_nid2obj(nid),pder);
}

static int gost94_param_decode(EVP_PKEY *pkey, const unsigned char **pder, int derlen)
{
	ASN1_OBJECT *obj=NULL;
	DSA *dsa = EVP_PKEY_get0(pkey);
	int nid;
	if (d2i_ASN1_OBJECT(&obj,pder,derlen)==NULL) {
		return 0;
	}
	nid = OBJ_obj2nid(obj);
	ASN1_OBJECT_free(obj);
	if (!dsa) 
		{
		dsa=DSA_new();
		if (!EVP_PKEY_assign(pkey,NID_id_GostR3410_94,dsa)) return 0;
		}
	if (!fill_GOST94_params(dsa,nid)) return 0;
	return 1;
}	

static int gost2001_param_decode(EVP_PKEY *pkey, const unsigned char **pder, int derlen) {
	ASN1_OBJECT *obj=NULL;
	int nid;
	EC_KEY *ec = EVP_PKEY_get0(pkey);
	if (d2i_ASN1_OBJECT(&obj,pder,derlen)==NULL) {
		return 0;
	}
	nid = OBJ_obj2nid(obj);
	ASN1_OBJECT_free(obj);
	if (!ec) 
		{
		ec = EC_KEY_new();
		if (!EVP_PKEY_assign(pkey,NID_id_GostR3410_2001,ec)) return 0;
		}	
	if (!fill_GOST2001_params(ec, nid)) return 0;
	return 1;
}	





/* ----------------------------------------------------------------------*/
int register_ameth_gost (int nid, EVP_PKEY_ASN1_METHOD **ameth, const char* pemstr, const char* info) 
	{
	*ameth =	EVP_PKEY_asn1_new(nid, 
		ASN1_PKEY_SIGPARAM_NULL, pemstr, info); 
	if (!*ameth) return 0;
	switch (nid) 
		{
		case NID_id_GostR3410_94:
			EVP_PKEY_asn1_set_free (*ameth, pkey_free_gost94);
			EVP_PKEY_asn1_set_private (*ameth, 
				priv_decode_gost, priv_encode_gost, 
				priv_print_gost94);

			EVP_PKEY_asn1_set_param (*ameth, 
				gost94_param_decode, gost94_param_encode,
				param_missing_gost94, param_copy_gost94, 
				param_cmp_gost94,param_print_gost94 );
			EVP_PKEY_asn1_set_public (*ameth,
				pub_decode_gost94, pub_encode_gost94,
				pub_cmp_gost94, pub_print_gost94,
				pkey_size_gost, pkey_bits_gost);
	
			EVP_PKEY_asn1_set_ctrl (*ameth, pkey_ctrl_gost);
			break;
		case NID_id_GostR3410_2001:
			EVP_PKEY_asn1_set_free (*ameth, pkey_free_gost01);
			EVP_PKEY_asn1_set_private (*ameth, 
				priv_decode_gost, priv_encode_gost, 
				priv_print_gost01);

			EVP_PKEY_asn1_set_param (*ameth, 
				gost2001_param_decode, gost2001_param_encode,
				param_missing_gost01, param_copy_gost01, 
				param_cmp_gost01, param_print_gost01);
			EVP_PKEY_asn1_set_public (*ameth,
				pub_decode_gost01, pub_encode_gost01,
				pub_cmp_gost01, pub_print_gost01,
				pkey_size_gost, pkey_bits_gost);
	
			EVP_PKEY_asn1_set_ctrl (*ameth, pkey_ctrl_gost);
			break;
		case NID_id_Gost28147_89_MAC:
			EVP_PKEY_asn1_set_free(*ameth, mackey_free_gost);
			EVP_PKEY_asn1_set_ctrl(*ameth,mac_ctrl_gost);	
			break;
		}		
	return 1;
	}
