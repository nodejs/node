/*
 * keyraw.c - raw key operations and conversions - OpenSSL version
 *
 * (c) NLnet Labs, 2004-2008
 *
 * See the file LICENSE for the license
 */
/**
 * \file
 * Implementation of raw DNSKEY functions (work on wire rdata).
 */

#include "config.h"
#include "gldns/keyraw.h"
#include "gldns/rrdef.h"

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/md5.h>
#ifdef HAVE_OPENSSL_CONF_H
# include <openssl/conf.h>
#endif
#ifdef HAVE_OPENSSL_ENGINE_H
#  include <openssl/engine.h>
#endif
#ifdef HAVE_OPENSSL_BN_H
#include <openssl/bn.h>
#endif
#ifdef HAVE_OPENSSL_RSA_H
#include <openssl/rsa.h>
#endif
#ifdef HAVE_OPENSSL_DSA_H
#include <openssl/dsa.h>
#endif
#endif /* HAVE_SSL */

#ifdef HAVE_SSL
#ifdef USE_GOST

/** store GOST engine reference loaded into OpenSSL library */
#ifdef OPENSSL_NO_ENGINE
int
gldns_key_EVP_load_gost_id(void)
{
	return 0;
}
void gldns_key_EVP_unload_gost(void)
{
}
#else
ENGINE* gldns_gost_engine = NULL;

int
gldns_key_EVP_load_gost_id(void)
{
	static int gost_id = 0;
	const EVP_PKEY_ASN1_METHOD* meth;
	ENGINE* e;

	if(gost_id) return gost_id;

	/* see if configuration loaded gost implementation from other engine*/
	meth = EVP_PKEY_asn1_find_str(NULL, "gost2001", -1);
	if(meth) {
		EVP_PKEY_asn1_get0_info(&gost_id, NULL, NULL, NULL, NULL, meth);
		return gost_id;
	}

	/* see if engine can be loaded already */
	e = ENGINE_by_id("gost");
	if(!e) {
		/* load it ourself, in case statically linked */
		ENGINE_load_builtin_engines();
		ENGINE_load_dynamic();
		e = ENGINE_by_id("gost");
	}
	if(!e) {
		/* no gost engine in openssl */
		return 0;
	}
	if(!ENGINE_set_default(e, ENGINE_METHOD_ALL)) {
		ENGINE_finish(e);
		ENGINE_free(e);
		return 0;
	}

	meth = EVP_PKEY_asn1_find_str(&e, "gost2001", -1);
	if(!meth) {
		/* algo not found */
		ENGINE_finish(e);
		ENGINE_free(e);
		return 0;
	}
        /* Note: do not ENGINE_finish and ENGINE_free the acquired engine
         * on some platforms this frees up the meth and unloads gost stuff */
        gldns_gost_engine = e;
	
	EVP_PKEY_asn1_get0_info(&gost_id, NULL, NULL, NULL, NULL, meth);
	return gost_id;
} 

void gldns_key_EVP_unload_gost(void)
{
        if(gldns_gost_engine) {
                ENGINE_finish(gldns_gost_engine);
                ENGINE_free(gldns_gost_engine);
                gldns_gost_engine = NULL;
        }
}
#endif /* ifndef OPENSSL_NO_ENGINE */
#endif /* USE_GOST */

DSA *
gldns_key_buf2dsa_raw(unsigned char* key, size_t len)
{
	uint8_t T;
	uint16_t length;
	uint16_t offset;
	DSA *dsa;
	BIGNUM *Q; BIGNUM *P;
	BIGNUM *G; BIGNUM *Y;

	if(len == 0)
		return NULL;
	T = (uint8_t)key[0];
	length = (64 + T * 8);
	offset = 1;

	if (T > 8) {
		return NULL;
	}
	if(len < (size_t)1 + SHA_DIGEST_LENGTH + 3*length)
		return NULL;

	Q = BN_bin2bn(key+offset, SHA_DIGEST_LENGTH, NULL);
	offset += SHA_DIGEST_LENGTH;

	P = BN_bin2bn(key+offset, (int)length, NULL);
	offset += length;

	G = BN_bin2bn(key+offset, (int)length, NULL);
	offset += length;

	Y = BN_bin2bn(key+offset, (int)length, NULL);

	/* create the key and set its properties */
	if(!Q || !P || !G || !Y || !(dsa = DSA_new())) {
		BN_free(Q);
		BN_free(P);
		BN_free(G);
		BN_free(Y);
		return NULL;
	}

#if defined(HAVE_DSA_SET0_PQG) && defined(HAVE_DSA_SET0_KEY)
	if (!DSA_set0_pqg(dsa, P, Q, G)) {
		/* QPG not yet attached, need to free */
		BN_free(Q);
		BN_free(P);
		BN_free(G);

		DSA_free(dsa);
		BN_free(Y);
		return NULL;
	}
	if (!DSA_set0_key(dsa, Y, NULL)) {
		/* QPG attached, cleaned up by DSA_fre() */
		DSA_free(dsa);
		BN_free(Y);
		return NULL;
	}
#else
#  ifndef S_SPLINT_S
	dsa->p = P;
	dsa->q = Q;
	dsa->g = G;
	dsa->pub_key = Y;
#  endif /* splint */
#endif

	return dsa;
}

RSA *
gldns_key_buf2rsa_raw(unsigned char* key, size_t len)
{
	uint16_t offset;
	uint16_t exp;
	uint16_t int16;
	RSA *rsa;
	BIGNUM *modulus;
	BIGNUM *exponent;

	if (len == 0)
		return NULL;
	if (key[0] == 0) {
		if(len < 3)
			return NULL;
		memmove(&int16, key+1, 2);
		exp = ntohs(int16);
		offset = 3;
	} else {
		exp = key[0];
		offset = 1;
	}

	/* key length at least one */
	if(len < (size_t)offset + exp + 1)
		return NULL;

	/* Exponent */
	exponent = BN_new();
	if(!exponent) return NULL;
	(void) BN_bin2bn(key+offset, (int)exp, exponent);
	offset += exp;

	/* Modulus */
	modulus = BN_new();
	if(!modulus) {
		BN_free(exponent);
		return NULL;
	}
	/* length of the buffer must match the key length! */
	(void) BN_bin2bn(key+offset, (int)(len - offset), modulus);

	rsa = RSA_new();
	if(!rsa) {
		BN_free(exponent);
		BN_free(modulus);
		return NULL;
	}

#if defined(HAVE_RSA_SET0_KEY)	
	if (!RSA_set0_key(rsa, modulus, exponent, NULL)) {
		BN_free(exponent);
		BN_free(modulus);
		RSA_free(rsa);
		return NULL;
	}
#else	
#  ifndef S_SPLINT_S
	rsa->n = modulus;
	rsa->e = exponent;
#  endif /* splint */
#endif

	return rsa;
}

#ifdef USE_GOST
EVP_PKEY*
gldns_gost2pkey_raw(unsigned char* key, size_t keylen)
{
	/* prefix header for X509 encoding */
	uint8_t asn[37] = { 0x30, 0x63, 0x30, 0x1c, 0x06, 0x06, 0x2a, 0x85, 
		0x03, 0x02, 0x02, 0x13, 0x30, 0x12, 0x06, 0x07, 0x2a, 0x85, 
		0x03, 0x02, 0x02, 0x23, 0x01, 0x06, 0x07, 0x2a, 0x85, 0x03, 
		0x02, 0x02, 0x1e, 0x01, 0x03, 0x43, 0x00, 0x04, 0x40};
	unsigned char encoded[37+64];
	const unsigned char* pp;
	if(keylen != 64) {
		/* key wrong size */
		return NULL;
	}

	/* create evp_key */
	memmove(encoded, asn, 37);
	memmove(encoded+37, key, 64);
	pp = (unsigned char*)&encoded[0];

	return d2i_PUBKEY(NULL, &pp, (int)sizeof(encoded));
}
#endif /* USE_GOST */

#ifdef USE_ECDSA
EVP_PKEY*
gldns_ecdsa2pkey_raw(unsigned char* key, size_t keylen, uint8_t algo)
{
	unsigned char buf[256+2]; /* sufficient for 2*384/8+1 */
        const unsigned char* pp = buf;
        EVP_PKEY *evp_key;
        EC_KEY *ec;
	/* check length, which uncompressed must be 2 bignums */
        if(algo == GLDNS_ECDSAP256SHA256) {
		if(keylen != 2*256/8) return NULL;
                ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
        } else if(algo == GLDNS_ECDSAP384SHA384) {
		if(keylen != 2*384/8) return NULL;
                ec = EC_KEY_new_by_curve_name(NID_secp384r1);
        } else    ec = NULL;
        if(!ec) return NULL;
	if(keylen+1 > sizeof(buf)) { /* sanity check */
                EC_KEY_free(ec);
		return NULL;
	}
	/* prepend the 0x02 (from docs) (or actually 0x04 from implementation
	 * of openssl) for uncompressed data */
	buf[0] = POINT_CONVERSION_UNCOMPRESSED;
	memmove(buf+1, key, keylen);
        if(!o2i_ECPublicKey(&ec, &pp, (int)keylen+1)) {
                EC_KEY_free(ec);
                return NULL;
        }
        evp_key = EVP_PKEY_new();
        if(!evp_key) {
                EC_KEY_free(ec);
                return NULL;
        }
        if (!EVP_PKEY_assign_EC_KEY(evp_key, ec)) {
		EVP_PKEY_free(evp_key);
		EC_KEY_free(ec);
		return NULL;
	}
        return evp_key;
}
#endif /* USE_ECDSA */

#ifdef USE_ED25519
EVP_PKEY*
gldns_ed255192pkey_raw(const unsigned char* key, size_t keylen)
{
	/* ASN1 for ED25519 is 302a300506032b6570032100 <32byteskey> */
	uint8_t pre[] = {0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65,
		0x70, 0x03, 0x21, 0x00};
	int pre_len = 12;
	uint8_t buf[256];
	EVP_PKEY *evp_key;
	/* pp gets modified by d2i() */
	const unsigned char* pp = (unsigned char*)buf;
	if(keylen != 32 || keylen + pre_len > sizeof(buf))
		return NULL; /* wrong length */
	memmove(buf, pre, pre_len);
	memmove(buf+pre_len, key, keylen);
	evp_key = d2i_PUBKEY(NULL, &pp, (int)(pre_len+keylen));
	return evp_key;
}
#endif /* USE_ED25519 */

#ifdef USE_ED448
EVP_PKEY*
gldns_ed4482pkey_raw(const unsigned char* key, size_t keylen)
{
	/* ASN1 for ED448 is 3043300506032b6571033a00 <57byteskey> */
	uint8_t pre[] = {0x30, 0x43, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65,
		0x71, 0x03, 0x3a, 0x00};
        int pre_len = 12;
	uint8_t buf[256];
        EVP_PKEY *evp_key;
	/* pp gets modified by d2i() */
        const unsigned char* pp = (unsigned char*)buf;
	if(keylen != 57 || keylen + pre_len > sizeof(buf))
		return NULL; /* wrong length */
	memmove(buf, pre, pre_len);
	memmove(buf+pre_len, key, keylen);
	evp_key = d2i_PUBKEY(NULL, &pp, (int)(pre_len+keylen));
        return evp_key;
}
#endif /* USE_ED448 */

int
gldns_digest_evp(unsigned char* data, unsigned int len, unsigned char* dest,
	const EVP_MD* md)
{
	EVP_MD_CTX* ctx;
	ctx = EVP_MD_CTX_create();
	if(!ctx)
		return 0;
	if(!EVP_DigestInit_ex(ctx, md, NULL) ||
		!EVP_DigestUpdate(ctx, data, len) ||
		!EVP_DigestFinal_ex(ctx, dest, NULL)) {
		EVP_MD_CTX_destroy(ctx);
		return 0;
	}
	EVP_MD_CTX_destroy(ctx);
	return 1;
}
#endif /* HAVE_SSL */
