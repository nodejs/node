/* p12_kiss.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
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

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/pkcs12.h>

/* Simplified PKCS#12 routines */

static int parse_pk12( PKCS12 *p12, const char *pass, int passlen,
		EVP_PKEY **pkey, X509 **cert, STACK_OF(X509) **ca);

static int parse_bags( STACK_OF(PKCS12_SAFEBAG) *bags, const char *pass,
		       int passlen, EVP_PKEY **pkey, X509 **cert,
		       STACK_OF(X509) **ca, ASN1_OCTET_STRING **keyid,
		       char *keymatch);

static int parse_bag( PKCS12_SAFEBAG *bag, const char *pass, int passlen,
			EVP_PKEY **pkey, X509 **cert, STACK_OF(X509) **ca,
			ASN1_OCTET_STRING **keyid, char *keymatch);

/* Parse and decrypt a PKCS#12 structure returning user key, user cert
 * and other (CA) certs. Note either ca should be NULL, *ca should be NULL,
 * or it should point to a valid STACK structure. pkey and cert can be
 * passed unitialised.
 */

int PKCS12_parse(PKCS12 *p12, const char *pass, EVP_PKEY **pkey, X509 **cert,
	     STACK_OF(X509) **ca)
{

	/* Check for NULL PKCS12 structure */

	if(!p12) {
		PKCS12err(PKCS12_F_PKCS12_PARSE,PKCS12_R_INVALID_NULL_PKCS12_POINTER);
		return 0;
	}

	/* Allocate stack for ca certificates if needed */
	if ((ca != NULL) && (*ca == NULL)) {
		if (!(*ca = sk_X509_new_null())) {
			PKCS12err(PKCS12_F_PKCS12_PARSE,ERR_R_MALLOC_FAILURE);
			return 0;
		}
	}

	if(pkey) *pkey = NULL;
	if(cert) *cert = NULL;

	/* Check the mac */

	/* If password is zero length or NULL then try verifying both cases
	 * to determine which password is correct. The reason for this is that
	 * under PKCS#12 password based encryption no password and a zero length
	 * password are two different things...
	 */

	if(!pass || !*pass) {
		if(PKCS12_verify_mac(p12, NULL, 0)) pass = NULL;
		else if(PKCS12_verify_mac(p12, "", 0)) pass = "";
		else {
			PKCS12err(PKCS12_F_PKCS12_PARSE,PKCS12_R_MAC_VERIFY_FAILURE);
			goto err;
		}
	} else if (!PKCS12_verify_mac(p12, pass, -1)) {
		PKCS12err(PKCS12_F_PKCS12_PARSE,PKCS12_R_MAC_VERIFY_FAILURE);
		goto err;
	}

	if (!parse_pk12 (p12, pass, -1, pkey, cert, ca))
		{
		PKCS12err(PKCS12_F_PKCS12_PARSE,PKCS12_R_PARSE_ERROR);
		goto err;
		}

	return 1;

 err:

	if (pkey && *pkey) EVP_PKEY_free(*pkey);
	if (cert && *cert) X509_free(*cert);
	if (ca) sk_X509_pop_free(*ca, X509_free);
	return 0;

}

/* Parse the outer PKCS#12 structure */

static int parse_pk12(PKCS12 *p12, const char *pass, int passlen,
	     EVP_PKEY **pkey, X509 **cert, STACK_OF(X509) **ca)
{
	STACK_OF(PKCS7) *asafes;
	STACK_OF(PKCS12_SAFEBAG) *bags;
	int i, bagnid;
	PKCS7 *p7;
	ASN1_OCTET_STRING *keyid = NULL;

	char keymatch = 0;
	if (!(asafes = PKCS12_unpack_authsafes (p12))) return 0;
	for (i = 0; i < sk_PKCS7_num (asafes); i++) {
		p7 = sk_PKCS7_value (asafes, i);
		bagnid = OBJ_obj2nid (p7->type);
		if (bagnid == NID_pkcs7_data) {
			bags = PKCS12_unpack_p7data(p7);
		} else if (bagnid == NID_pkcs7_encrypted) {
			bags = PKCS12_unpack_p7encdata(p7, pass, passlen);
		} else continue;
		if (!bags) {
			sk_PKCS7_pop_free(asafes, PKCS7_free);
			return 0;
		}
	    	if (!parse_bags(bags, pass, passlen, pkey, cert, ca,
							 &keyid, &keymatch)) {
			sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
			sk_PKCS7_pop_free(asafes, PKCS7_free);
			return 0;
		}
		sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
	}
	sk_PKCS7_pop_free(asafes, PKCS7_free);
	if (keyid) M_ASN1_OCTET_STRING_free(keyid);
	return 1;
}


static int parse_bags(STACK_OF(PKCS12_SAFEBAG) *bags, const char *pass,
		      int passlen, EVP_PKEY **pkey, X509 **cert,
		      STACK_OF(X509) **ca, ASN1_OCTET_STRING **keyid,
		      char *keymatch)
{
	int i;
	for (i = 0; i < sk_PKCS12_SAFEBAG_num(bags); i++) {
		if (!parse_bag(sk_PKCS12_SAFEBAG_value (bags, i),
			 pass, passlen, pkey, cert, ca, keyid,
							 keymatch)) return 0;
	}
	return 1;
}

#define MATCH_KEY  0x1
#define MATCH_CERT 0x2
#define MATCH_ALL  0x3

static int parse_bag(PKCS12_SAFEBAG *bag, const char *pass, int passlen,
		     EVP_PKEY **pkey, X509 **cert, STACK_OF(X509) **ca,
		     ASN1_OCTET_STRING **keyid,
		     char *keymatch)
{
	PKCS8_PRIV_KEY_INFO *p8;
	X509 *x509;
	ASN1_OCTET_STRING *lkey = NULL, *ckid = NULL;
	ASN1_TYPE *attrib;
	ASN1_BMPSTRING *fname = NULL;

	if ((attrib = PKCS12_get_attr (bag, NID_friendlyName)))
		fname = attrib->value.bmpstring;

	if ((attrib = PKCS12_get_attr (bag, NID_localKeyID))) {
		lkey = attrib->value.octet_string;
		ckid = lkey;
	}

	/* Check for any local key id matching (if needed) */
	if (lkey && ((*keymatch & MATCH_ALL) != MATCH_ALL)) {
		if (*keyid) {
			if (M_ASN1_OCTET_STRING_cmp(*keyid, lkey)) lkey = NULL;
		} else {
			if (!(*keyid = M_ASN1_OCTET_STRING_dup(lkey))) {
				PKCS12err(PKCS12_F_PARSE_BAG,ERR_R_MALLOC_FAILURE);
				return 0;
		    }
		}
	}
	
	switch (M_PKCS12_bag_type(bag))
	{
	case NID_keyBag:
		if (!lkey || !pkey) return 1;	
		if (!(*pkey = EVP_PKCS82PKEY(bag->value.keybag))) return 0;
		*keymatch |= MATCH_KEY;
	break;

	case NID_pkcs8ShroudedKeyBag:
		if (!lkey || !pkey) return 1;	
		if (!(p8 = PKCS12_decrypt_skey(bag, pass, passlen)))
				return 0;
		*pkey = EVP_PKCS82PKEY(p8);
		PKCS8_PRIV_KEY_INFO_free(p8);
		if (!(*pkey)) return 0;
		*keymatch |= MATCH_KEY;
	break;

	case NID_certBag:
		if (M_PKCS12_cert_bag_type(bag) != NID_x509Certificate )
								 return 1;
		if (!(x509 = PKCS12_certbag2x509(bag))) return 0;
		if(ckid)
			{
			if (!X509_keyid_set1(x509, ckid->data, ckid->length))
				{
				X509_free(x509);
				return 0;
				}
			}
		if(fname) {
			int len, r;
			unsigned char *data;
			len = ASN1_STRING_to_UTF8(&data, fname);
			if(len > 0) {
				r = X509_alias_set1(x509, data, len);
				OPENSSL_free(data);
				if (!r)
					{
					X509_free(x509);
					return 0;
					}
			}
		}


		if (lkey) {
			*keymatch |= MATCH_CERT;
			if (cert) *cert = x509;
			else X509_free(x509);
		} else {
			if(ca) sk_X509_push (*ca, x509);
			else X509_free(x509);
		}
	break;

	case NID_safeContentsBag:
		return parse_bags(bag->value.safes, pass, passlen,
			 		pkey, cert, ca, keyid, keymatch);
	break;

	default:
		return 1;
	break;
	}
	return 1;
}

