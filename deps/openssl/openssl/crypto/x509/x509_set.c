/* crypto/x509/x509_set.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/asn1.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

int X509_set_version(X509 *x, long version)
	{
	if (x == NULL) return(0);
	if (x->cert_info->version == NULL)
		{
		if ((x->cert_info->version=M_ASN1_INTEGER_new()) == NULL)
			return(0);
		}
	return(ASN1_INTEGER_set(x->cert_info->version,version));
	}

int X509_set_serialNumber(X509 *x, ASN1_INTEGER *serial)
	{
	ASN1_INTEGER *in;

	if (x == NULL) return(0);
	in=x->cert_info->serialNumber;
	if (in != serial)
		{
		in=M_ASN1_INTEGER_dup(serial);
		if (in != NULL)
			{
			M_ASN1_INTEGER_free(x->cert_info->serialNumber);
			x->cert_info->serialNumber=in;
			}
		}
	return(in != NULL);
	}

int X509_set_issuer_name(X509 *x, X509_NAME *name)
	{
	if ((x == NULL) || (x->cert_info == NULL)) return(0);
	return(X509_NAME_set(&x->cert_info->issuer,name));
	}

int X509_set_subject_name(X509 *x, X509_NAME *name)
	{
	if ((x == NULL) || (x->cert_info == NULL)) return(0);
	return(X509_NAME_set(&x->cert_info->subject,name));
	}

int X509_set_notBefore(X509 *x, const ASN1_TIME *tm)
	{
	ASN1_TIME *in;

	if ((x == NULL) || (x->cert_info->validity == NULL)) return(0);
	in=x->cert_info->validity->notBefore;
	if (in != tm)
		{
		in=M_ASN1_TIME_dup(tm);
		if (in != NULL)
			{
			M_ASN1_TIME_free(x->cert_info->validity->notBefore);
			x->cert_info->validity->notBefore=in;
			}
		}
	return(in != NULL);
	}

int X509_set_notAfter(X509 *x, const ASN1_TIME *tm)
	{
	ASN1_TIME *in;

	if ((x == NULL) || (x->cert_info->validity == NULL)) return(0);
	in=x->cert_info->validity->notAfter;
	if (in != tm)
		{
		in=M_ASN1_TIME_dup(tm);
		if (in != NULL)
			{
			M_ASN1_TIME_free(x->cert_info->validity->notAfter);
			x->cert_info->validity->notAfter=in;
			}
		}
	return(in != NULL);
	}

int X509_set_pubkey(X509 *x, EVP_PKEY *pkey)
	{
	if ((x == NULL) || (x->cert_info == NULL)) return(0);
	return(X509_PUBKEY_set(&(x->cert_info->key),pkey));
	}



