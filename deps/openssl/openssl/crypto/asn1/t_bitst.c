/* t_bitst.c */
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
#include <openssl/conf.h>
#include <openssl/x509v3.h>

int ASN1_BIT_STRING_name_print(BIO *out, ASN1_BIT_STRING *bs,
				BIT_STRING_BITNAME *tbl, int indent)
{
	BIT_STRING_BITNAME *bnam;
	char first = 1;
	BIO_printf(out, "%*s", indent, "");
	for(bnam = tbl; bnam->lname; bnam++) {
		if(ASN1_BIT_STRING_get_bit(bs, bnam->bitnum)) {
			if(!first) BIO_puts(out, ", ");
			BIO_puts(out, bnam->lname);
			first = 0;
		}
	}
	BIO_puts(out, "\n");
	return 1;
}

int ASN1_BIT_STRING_set_asc(ASN1_BIT_STRING *bs, char *name, int value,
				BIT_STRING_BITNAME *tbl)
{
	int bitnum;
	bitnum = ASN1_BIT_STRING_num_asc(name, tbl);
	if(bitnum < 0) return 0;
	if(bs) {
		if(!ASN1_BIT_STRING_set_bit(bs, bitnum, value))
			return 0;
	}
	return 1;
}

int ASN1_BIT_STRING_num_asc(char *name, BIT_STRING_BITNAME *tbl)
{
	BIT_STRING_BITNAME *bnam;
	for(bnam = tbl; bnam->lname; bnam++) {
		if(!strcmp(bnam->sname, name) ||
			!strcmp(bnam->lname, name) ) return bnam->bitnum;
	}
	return -1;
}
