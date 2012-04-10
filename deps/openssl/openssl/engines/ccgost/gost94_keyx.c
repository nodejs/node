/**********************************************************************
 *                             gost94_keyx.c                          *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *         This file is distributed under the same license as OpenSSL *
 *                                                                    *
 *     Implements generation and parsing of GOST_KEY_TRANSPORT for    *
 *     			GOST R 34.10-94 algorithms                            *
 *																	  *
 *          Requires OpenSSL 0.9.9 for compilation                    *
 **********************************************************************/
#include <string.h>
#include <openssl/dh.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/objects.h>

#include "gost89.h"
#include "gosthash.h"
#include "e_gost_err.h"
#include "gost_keywrap.h"
#include "gost_lcl.h"
/* Common functions for both 94 and 2001 key exchange schemes */
/* Implementation of the Diffi-Hellman key agreement scheme based on
 * GOST-94 keys */

/* Computes Diffie-Hellman key and stores it into buffer in
 * little-endian byte order as expected by both versions of GOST 94
 * algorithm
 */
static int compute_pair_key_le(unsigned char *pair_key,BIGNUM *pub_key,DH *dh) 
	{
	unsigned char be_key[128];
	int i,key_size;
	key_size=DH_compute_key(be_key,pub_key,dh);
	if (!key_size) return 0;
	memset(pair_key,0,128);
	for (i=0;i<key_size;i++)
		{
		pair_key[i]=be_key[key_size-1-i];
		}
	return key_size;	
	}	

/*
 * Computes 256 bit Key exchange key as specified in RFC 4357 
 */
static int make_cp_exchange_key(BIGNUM *priv_key,EVP_PKEY *pubk, unsigned char *shared_key)
	{
	unsigned char dh_key [128];
	int ret;
	gost_hash_ctx hash_ctx;
	DH *dh = DH_new();
	
	if (!dh)
		return 0;
	memset(dh_key,0,128);
	dh->g = BN_dup(pubk->pkey.dsa->g);
	dh->p = BN_dup(pubk->pkey.dsa->p);
	dh->priv_key = BN_dup(priv_key);
	ret=compute_pair_key_le(dh_key,((DSA *)(EVP_PKEY_get0(pubk)))->pub_key,dh) ;
	DH_free(dh);
	if (!ret)	return 0;
	init_gost_hash_ctx(&hash_ctx,&GostR3411_94_CryptoProParamSet);
	start_hash(&hash_ctx);
	hash_block(&hash_ctx,dh_key,128);
	finish_hash(&hash_ctx,shared_key);
	done_gost_hash_ctx(&hash_ctx);
	return 1;
	}

/* EVP_PKEY_METHOD callback derive. Implements VKO R 34.10-94 */

int pkey_gost94_derive(EVP_PKEY_CTX *ctx,unsigned char *key,size_t *keylen)
	{
		EVP_PKEY *pubk = EVP_PKEY_CTX_get0_peerkey(ctx);
		EVP_PKEY *mykey = EVP_PKEY_CTX_get0_pkey(ctx);
		*keylen = 32;
		if (key == NULL) return 1;

		return make_cp_exchange_key(gost_get0_priv_key(mykey), pubk, key);
	}

/* EVP_PKEY_METHOD callback encrypt for
 * GOST R 34.10-94 cryptopro modification
 */


int pkey_GOST94cp_encrypt(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen, const unsigned char* key, size_t key_len ) 
	{
	GOST_KEY_TRANSPORT *gkt=NULL;
	unsigned char shared_key[32], ukm[8],crypted_key[44];
	const struct gost_cipher_info *param=get_encryption_params(NULL);
	EVP_PKEY *pubk = EVP_PKEY_CTX_get0_pkey(ctx);
	struct gost_pmeth_data *data = EVP_PKEY_CTX_get_data(ctx);
	gost_ctx cctx;
	int key_is_ephemeral=1;
	EVP_PKEY *mykey = EVP_PKEY_CTX_get0_peerkey(ctx);

	/* Do not use vizir cipher parameters with cryptopro */
	if (!get_gost_engine_param(GOST_PARAM_CRYPT_PARAMS) && param ==  gost_cipher_list)
		{
		param= gost_cipher_list+1;
		}	

	if (mykey) 
		{
		/* If key already set, it is not ephemeral */
		key_is_ephemeral=0;
		if (!gost_get0_priv_key(mykey)) 
			{
			GOSTerr(GOST_F_PKEY_GOST94CP_ENCRYPT,
			GOST_R_NO_PRIVATE_PART_OF_NON_EPHEMERAL_KEYPAIR);
			goto err;
			}	
		} 
	else 
		{
		/* Otherwise generate ephemeral key */
		key_is_ephemeral = 1;
		if (out) 
			{
			mykey = EVP_PKEY_new();
			EVP_PKEY_assign(mykey, EVP_PKEY_base_id(pubk),DSA_new());
			EVP_PKEY_copy_parameters(mykey,pubk);
			if (!gost_sign_keygen(EVP_PKEY_get0(mykey))) 
				{
				goto err;
				}	
			}
		}	
	if (out)
		make_cp_exchange_key(gost_get0_priv_key(mykey),pubk,shared_key);
	if (data->shared_ukm) 
		{
		memcpy(ukm,data->shared_ukm,8);
		}
	else if (out) 
		{	
		if (RAND_bytes(ukm,8)<=0)
			{
			GOSTerr(GOST_F_PKEY_GOST94CP_ENCRYPT,
					GOST_R_RANDOM_GENERATOR_FAILURE);
			goto err;
			}	
		}
		
	if (out) {
		gost_init(&cctx,param->sblock);
		keyWrapCryptoPro(&cctx,shared_key,ukm,key,crypted_key);
	}	
	gkt = GOST_KEY_TRANSPORT_new();
	if (!gkt)
		{
		goto memerr;
		}	
	if(!ASN1_OCTET_STRING_set(gkt->key_agreement_info->eph_iv,
			ukm,8))
		{
		goto memerr;
		}	
	if (!ASN1_OCTET_STRING_set(gkt->key_info->imit,crypted_key+40,4))
		{
		goto memerr;
		}
	if (!ASN1_OCTET_STRING_set(gkt->key_info->encrypted_key,crypted_key+8,32))
		{
		goto memerr;
		}
	if (key_is_ephemeral) {	
	if (!X509_PUBKEY_set(&gkt->key_agreement_info->ephem_key,out?mykey:pubk))
		{
		GOSTerr(GOST_F_PKEY_GOST94CP_ENCRYPT,GOST_R_CANNOT_PACK_EPHEMERAL_KEY);
		goto err;
		}
		if (out) EVP_PKEY_free(mykey);
	}	
	ASN1_OBJECT_free(gkt->key_agreement_info->cipher);
	gkt->key_agreement_info->cipher = OBJ_nid2obj(param->nid);
	*outlen = i2d_GOST_KEY_TRANSPORT(gkt,out?&out:NULL);
	if (*outlen <= 0)
		{
		GOSTerr(GOST_F_PKEY_GOST94CP_ENCRYPT,GOST_R_ERROR_PACKING_KEY_TRANSPORT_INFO);
		goto err;
		}
	if (!key_is_ephemeral)
		{
		/* Set control "public key from client certificate used" */
		if (EVP_PKEY_CTX_ctrl(ctx, -1, -1, EVP_PKEY_CTRL_PEER_KEY, 3, NULL) <= 0)
			{
			GOSTerr(GOST_F_PKEY_GOST94CP_ENCRYPT,
				GOST_R_CTRL_CALL_FAILED);
			goto err;
			}
		}
	GOST_KEY_TRANSPORT_free(gkt);
	return 1;	
	memerr:
		if (key_is_ephemeral) {
			EVP_PKEY_free(mykey);
		}	
	GOSTerr(GOST_F_PKEY_GOST94CP_ENCRYPT,
		GOST_R_MALLOC_FAILURE);
	err:		
	GOST_KEY_TRANSPORT_free(gkt);
	return -1;
	}

	
/* EVP_PLEY_METHOD callback decrypt for
 * GOST R 34.10-94 cryptopro modification
 */
int pkey_GOST94cp_decrypt(EVP_PKEY_CTX *ctx, unsigned char *key, size_t *key_len,const unsigned char *in, size_t in_len) {
	const unsigned char *p = in;
	GOST_KEY_TRANSPORT *gkt = NULL;
	unsigned char wrappedKey[44];
	unsigned char sharedKey[32];
	gost_ctx cctx;
	const struct gost_cipher_info *param=NULL;
	EVP_PKEY *eph_key=NULL, *peerkey=NULL;
	EVP_PKEY *priv= EVP_PKEY_CTX_get0_pkey(ctx); 
	
	if (!key)
		{
		*key_len = 32;
		return 1;
		}	
	
	gkt = d2i_GOST_KEY_TRANSPORT(NULL,(const unsigned char **)&p,
		in_len);
	if (!gkt)
		{
		GOSTerr(GOST_F_PKEY_GOST94CP_DECRYPT,GOST_R_ERROR_PARSING_KEY_TRANSPORT_INFO);
		return 0;
		}	
	eph_key = X509_PUBKEY_get(gkt->key_agreement_info->ephem_key);
	if (eph_key)
		{
		if (EVP_PKEY_derive_set_peer(ctx, eph_key) <= 0)
			{
			GOSTerr(GOST_F_PKEY_GOST94CP_DECRYPT,
				GOST_R_INCOMPATIBLE_PEER_KEY);
			goto err;
			}
		}
	else
		{
		/* Set control "public key from client certificate used" */
		if (EVP_PKEY_CTX_ctrl(ctx, -1, -1, EVP_PKEY_CTRL_PEER_KEY, 3, NULL) <= 0)
			{
			GOSTerr(GOST_F_PKEY_GOST94CP_DECRYPT,
				GOST_R_CTRL_CALL_FAILED);
			goto err;
			}
		}
	peerkey = EVP_PKEY_CTX_get0_peerkey(ctx);
	if (!peerkey)
		{
		GOSTerr(GOST_F_PKEY_GOST94CP_DECRYPT,
			GOST_R_NO_PEER_KEY);
		goto err;
		}

	param = get_encryption_params(gkt->key_agreement_info->cipher);
    if(!param){
        goto err;
    }
	
	gost_init(&cctx,param->sblock);	
	OPENSSL_assert(gkt->key_agreement_info->eph_iv->length==8);
	memcpy(wrappedKey,gkt->key_agreement_info->eph_iv->data,8);
	OPENSSL_assert(gkt->key_info->encrypted_key->length==32);
	memcpy(wrappedKey+8,gkt->key_info->encrypted_key->data,32);
	OPENSSL_assert(gkt->key_info->imit->length==4);
	memcpy(wrappedKey+40,gkt->key_info->imit->data,4);	
	make_cp_exchange_key(gost_get0_priv_key(priv),peerkey,sharedKey);
	if (!keyUnwrapCryptoPro(&cctx,sharedKey,wrappedKey,key))
		{
		GOSTerr(GOST_F_PKEY_GOST94CP_DECRYPT,
			GOST_R_ERROR_COMPUTING_SHARED_KEY);
		goto err;
		}	
				
	EVP_PKEY_free(eph_key);
	GOST_KEY_TRANSPORT_free(gkt);
	return 1;
err:
	EVP_PKEY_free(eph_key);
	GOST_KEY_TRANSPORT_free(gkt);
	return -1;
	}	

