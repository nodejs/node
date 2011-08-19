/* tasn_prn.c */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
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


#include <stddef.h>
#include <openssl/asn1.h>
#include <openssl/objects.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/nasn.h>

/* Print routines. Print out a whole structure from a template.
 */

static int asn1_item_print_nm(BIO *out, void *fld, int indent, const ASN1_ITEM *it, const char *name);

int ASN1_item_print(BIO *out, void *fld, int indent, const ASN1_ITEM *it)
{
	return asn1_item_print_nm(out, fld, indent, it, it->sname);
}

static int asn1_item_print_nm(BIO *out, void *fld, int indent, const ASN1_ITEM *it, const char *name)
{
	ASN1_STRING *str;
	const ASN1_TEMPLATE *tt;
	void *tmpfld;
	int i;
	if(!fld) {
		BIO_printf(out, "%*s%s ABSENT\n", indent, "", name);
		return 1;
	}
	switch(it->itype) {

		case ASN1_ITYPE_PRIMITIVE:
		if(it->templates)
			return ASN1_template_print(out, fld, indent, it->templates);
		return asn1_primitive_print(out, fld, it->utype, indent, name);
		break;

		case ASN1_ITYPE_MSTRING:
		str = fld;
		return asn1_primitive_print(out, fld, str->type, indent, name);

		case ASN1_ITYPE_EXTERN:
		BIO_printf(out, "%*s%s:EXTERNAL TYPE %s %s\n", indent, "", name, it->sname, fld ? "" : "ABSENT");
		return 1;
		case ASN1_ITYPE_COMPAT:
		BIO_printf(out, "%*s%s:COMPATIBLE TYPE %s %s\n", indent, "", name, it->sname, fld ? "" : "ABSENT");
		return 1;


		case ASN1_ITYPE_CHOICE:
		/* CHOICE type, get selector */
		i = asn1_get_choice_selector(fld, it);
		/* This should never happen... */
		if((i < 0) || (i >= it->tcount)) {
			BIO_printf(out, "%s selector [%d] out of range\n", it->sname, i);
			return 1;
		}
		tt = it->templates + i;
		tmpfld = asn1_get_field(fld, tt);
		return ASN1_template_print(out, tmpfld, indent, tt);

		case ASN1_ITYPE_SEQUENCE:
		BIO_printf(out, "%*s%s {\n", indent, "", name);
		/* Get each field entry */
		for(i = 0, tt = it->templates; i < it->tcount; i++, tt++) {
			tmpfld = asn1_get_field(fld, tt);
			ASN1_template_print(out, tmpfld, indent + 2, tt);
		}
		BIO_printf(out, "%*s}\n", indent, "");
		return 1;

		default:
		return 0;
	}
}

int ASN1_template_print(BIO *out, void *fld, int indent, const ASN1_TEMPLATE *tt)
{
	int i, flags;
#if 0
	if(!fld) return 0; 
#endif
	flags = tt->flags;
	if(flags & ASN1_TFLG_SK_MASK) {
		char *tname;
		void *skitem;
		/* SET OF, SEQUENCE OF */
		if(flags & ASN1_TFLG_SET_OF) tname = "SET";
		else tname = "SEQUENCE";
		if(fld) {
			BIO_printf(out, "%*s%s OF %s {\n", indent, "", tname, tt->field_name);
			for(i = 0; i < sk_num(fld); i++) {
				skitem = sk_value(fld, i);
				asn1_item_print_nm(out, skitem, indent + 2, tt->item, "");
			}
			BIO_printf(out, "%*s}\n", indent, "");
		} else 
			BIO_printf(out, "%*s%s OF %s ABSENT\n", indent, "", tname, tt->field_name);
		return 1;
	}
	return asn1_item_print_nm(out, fld, indent, tt->item, tt->field_name);
}

static int asn1_primitive_print(BIO *out, void *fld, long utype, int indent, const char *name)
{
	ASN1_STRING *str = fld;
	if(fld) {
		if(utype == V_ASN1_BOOLEAN) {
			int *bool = fld;
if(*bool == -1) printf("BOOL MISSING\n");
			BIO_printf(out, "%*s%s:%s", indent, "", "BOOLEAN", *bool ? "TRUE" : "FALSE");
		} else if((utype == V_ASN1_INTEGER) 
			  || (utype == V_ASN1_ENUMERATED)) {
			char *s, *nm;
			s = i2s_ASN1_INTEGER(NULL, fld);
			if(utype == V_ASN1_INTEGER) nm = "INTEGER";
			else nm = "ENUMERATED";
			BIO_printf(out, "%*s%s:%s", indent, "", nm, s);
			OPENSSL_free(s);
		} else if(utype == V_ASN1_NULL) {
			BIO_printf(out, "%*s%s", indent, "", "NULL");
		} else if(utype == V_ASN1_UTCTIME) {
			BIO_printf(out, "%*s%s:%s:", indent, "", name, "UTCTIME");
			ASN1_UTCTIME_print(out, str);
		} else if(utype == V_ASN1_GENERALIZEDTIME) {
			BIO_printf(out, "%*s%s:%s:", indent, "", name, "GENERALIZEDTIME");
			ASN1_GENERALIZEDTIME_print(out, str);
		} else if(utype == V_ASN1_OBJECT) {
			char objbuf[80], *ln;
			ln = OBJ_nid2ln(OBJ_obj2nid(fld));
			if(!ln) ln = "";
			OBJ_obj2txt(objbuf, sizeof objbuf, fld, 1);
			BIO_printf(out, "%*s%s:%s (%s)", indent, "", "OBJECT", ln, objbuf);
		} else {
			BIO_printf(out, "%*s%s:", indent, "", name);
			ASN1_STRING_print_ex(out, str, ASN1_STRFLGS_DUMP_UNKNOWN|ASN1_STRFLGS_SHOW_TYPE);
		}
		BIO_printf(out, "\n");
	} else BIO_printf(out, "%*s%s [ABSENT]\n", indent, "", name);
	return 1;
}
