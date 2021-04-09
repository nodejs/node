/**
 *
 * /brief functions for DNSSEC trust anchor management
 */
/*
 * Copyright (c) 2017, NLnet Labs, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the names of the copyright holders nor the
 *   names of its contributors may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Verisign, Inc. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "debug.h"
#include "anchor.h"
#include <fcntl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <time.h>
#include "types-internal.h"
#include "context.h"
#include "dnssec.h"
#include "yxml/yxml.h"
#include "gldns/parseutil.h"
#include "gldns/gbuffer.h"
#include "gldns/str2wire.h"
#include "gldns/wire2str.h"
#include "gldns/pkthdr.h"
#include "gldns/keyraw.h"
#include "general.h"
#include "util-internal.h"
#include "platform.h"

/* get key usage out of its extension, returns 0 if no key_usage extension */
static unsigned long
_getdns_get_usage_of_ex(X509* cert)
{
	unsigned long val = 0;
	ASN1_BIT_STRING* s;

	if((s=X509_get_ext_d2i(cert, NID_key_usage, NULL, NULL))) {
		if(s->length > 0) {
			val = s->data[0];
			if(s->length > 1)
				val |= s->data[1] << 8;
		}
		ASN1_BIT_STRING_free(s);
	}
	return val;
}

/** get valid signers from the list of signers in the signature */
static STACK_OF(X509)*
_getdns_get_valid_signers(PKCS7* p7, const char* p7signer)
{
	int i;
	STACK_OF(X509)* validsigners = sk_X509_new_null();
	STACK_OF(X509)* signers = PKCS7_get0_signers(p7, NULL, 0);
	unsigned long usage = 0;
	if(!validsigners) {
		DEBUG_ANCHOR("ERROR %s(): Failed to allocated validsigners\n"
		            , __FUNC__);
		sk_X509_free(signers);
		return NULL;
	}
	if(!signers) {
		DEBUG_ANCHOR("ERROR %s(): Failed to allocated signers\n"
		            , __FUNC__);
		sk_X509_free(validsigners);
		return NULL;
	}
	for(i=0; i<sk_X509_num(signers); i++) {
		char buf[1024];
		X509_NAME* nm = X509_get_subject_name(
			sk_X509_value(signers, i));
		if(!nm) {
			DEBUG_ANCHOR("%s(): cert %d has no subject name\n"
				    , __FUNC__, i);
			continue;
		}
		if(!p7signer || strcmp(p7signer, "")==0) {
			/* there is no name to check, return all records */
			DEBUG_ANCHOR("%s(): did not check commonName of signer\n"
				    , __FUNC__);
		} else {
			if(!X509_NAME_get_text_by_NID(nm,
				NID_pkcs9_emailAddress,
				buf, (int)sizeof(buf))) {
				DEBUG_ANCHOR("%s(): removed cert with no name\n"
					    , __FUNC__);
				continue; /* no name, no use */
			}
			if(strcmp(buf, p7signer) != 0) {
				DEBUG_ANCHOR("%s(): removed cert with wrong name\n"
					    , __FUNC__);
				continue; /* wrong name, skip it */
			}
		}

		/* check that the key usage allows digital signatures
		 * (the p7s) */
		usage = _getdns_get_usage_of_ex(sk_X509_value(signers, i));
		if(!(usage & KU_DIGITAL_SIGNATURE)) {
			DEBUG_ANCHOR("%s(): removed cert with no key usage "
			             "Digital Signature allowed\n"
				    , __FUNC__);
			continue;
		}

		/* we like this cert, add it to our list of valid
		 * signers certificates */
		sk_X509_push(validsigners, sk_X509_value(signers, i));
	}
	sk_X509_free(signers);
	return validsigners;
}

static int
_getdns_verify_p7sig(BIO* data, BIO* p7s, X509_STORE *store, const char* p7signer)
{
	PKCS7* p7;
	STACK_OF(X509)* validsigners;
	int secure = 0;
#ifdef X509_V_FLAG_CHECK_SS_SIGNATURE
	X509_VERIFY_PARAM* param = X509_VERIFY_PARAM_new();
	if(!param) {
		DEBUG_ANCHOR("ERROR %s(): Failed to allocated param\n"
		            , __FUNC__);
		return 0;
	}
	/* do the selfcheck on the root certificate; it checks that the
	 * input is valid */
	X509_VERIFY_PARAM_set_flags(param, X509_V_FLAG_CHECK_SS_SIGNATURE);
	X509_STORE_set1_param(store, param);
	X509_VERIFY_PARAM_free(param);
#endif
	(void)BIO_reset(p7s);
	(void)BIO_reset(data);

	/* convert p7s to p7 (the signature) */
	p7 = d2i_PKCS7_bio(p7s, NULL);
	if(!p7) {
		DEBUG_ANCHOR("ERROR %s(): could not parse p7s signature file\n"
		            , __FUNC__);
		return 0;
	}
	/* check what is in the Subject name of the certificates,
	 * and build a stack that contains only the right certificates */
	validsigners = _getdns_get_valid_signers(p7, p7signer);
	if(!validsigners) {
		PKCS7_free(p7);
		return 0;
	}
	if(PKCS7_verify(p7, validsigners, store, data, NULL, PKCS7_NOINTERN) == 1) {
		secure = 1;
	}
#if defined(ANCHOR_DEBUG) && ANCHOR_DEBUG
	else {
		DEBUG_ANCHOR("ERROR %s(): the PKCS7 signature did not verify\n"
		            , __FUNC__);
		ERR_print_errors_cb(_getdns_ERR_print_errors_cb_f, NULL);
	}
#endif
	sk_X509_free(validsigners);
	PKCS7_free(p7);
	return secure;
}

uint8_t *_getdns_tas_validate(struct mem_funcs *mf,
    const getdns_bindata *xml_bd, const getdns_bindata *p7s_bd,
    const getdns_bindata *crt_bd, const char *p7signer,
    uint64_t *now_ms, uint8_t *tas, size_t *tas_len)
{
	BIO *xml = NULL, *p7s = NULL, *crt = NULL;
	X509 *x = NULL;
	X509_STORE *store = NULL;
	uint8_t *success = NULL;

	if (!(xml = BIO_new_mem_buf(xml_bd->data, xml_bd->size)))
		DEBUG_ANCHOR("ERROR %s(): Failed allocating xml BIO\n"
		            , __FUNC__);

	else if (!(p7s = BIO_new_mem_buf(p7s_bd->data, p7s_bd->size)))
		DEBUG_ANCHOR("ERROR %s(): Failed allocating p7s BIO\n"
		            , __FUNC__);
	
	else if (!(crt = BIO_new_mem_buf(crt_bd->data, crt_bd->size)))
		DEBUG_ANCHOR("ERROR %s(): Failed allocating crt BIO\n"
		            , __FUNC__);

	else if (!(x = PEM_read_bio_X509(crt, NULL, 0, NULL)))
		DEBUG_ANCHOR("ERROR %s(): Parsing builtin certificate\n"
		            , __FUNC__);

	else if (!(store = X509_STORE_new()))
		DEBUG_ANCHOR("ERROR %s(): Failed allocating store\n"
		            , __FUNC__);

	else if (!X509_STORE_add_cert(store, x))
		DEBUG_ANCHOR("ERROR %s(): Adding certificate to store\n"
		            , __FUNC__);

	else if (_getdns_verify_p7sig(xml, p7s, store, p7signer)) {
		gldns_buffer gbuf;

		gldns_buffer_init_vfixed_frm_data(&gbuf, tas, *tas_len);

		if (!_getdns_parse_xml_trust_anchors_buf(&gbuf, now_ms,
		    (char *)xml_bd->data, xml_bd->size))
			DEBUG_ANCHOR("Failed to parse trust anchor XML data");

		else if (gldns_buffer_position(&gbuf) > *tas_len) {
			*tas_len = gldns_buffer_position(&gbuf);
			if ((success = GETDNS_XMALLOC(*mf, uint8_t, *tas_len))) {
				gldns_buffer_init_frm_data(&gbuf, success, *tas_len);
				if (!_getdns_parse_xml_trust_anchors_buf(&gbuf,
				    now_ms, (char *)xml_bd->data, xml_bd->size)) {

					DEBUG_ANCHOR("Failed to re-parse trust"
					             " anchor XML data\n");
					GETDNS_FREE(*mf, success);
					success = NULL;
				}
			} else
				DEBUG_ANCHOR("Could not allocate space for "
				             "trust anchors\n");
		} else {
			success = tas;
			*tas_len = gldns_buffer_position(&gbuf);
		}
	} else {
		DEBUG_ANCHOR("Verifying trust-anchors failed!\n");
	}
	if (store)	X509_STORE_free(store);
	if (x)		X509_free(x);
	if (crt)	BIO_free(crt);
	if (xml)	BIO_free(xml);
	if (p7s)	BIO_free(p7s);
	return success;
}

void _getdns_context_equip_with_anchor(
    getdns_context *context, uint64_t *now_ms)
{
	uint8_t xml_spc[4096], *xml_data = NULL;
	uint8_t p7s_spc[4096], *p7s_data = NULL;
	size_t xml_len, p7s_len;
	const char *verify_email = NULL;
	const char *verify_CA = NULL;
	getdns_return_t r;

	BIO *xml = NULL, *p7s = NULL, *crt = NULL;
	X509 *x = NULL;
	X509_STORE *store = NULL;

	if ((r = getdns_context_get_trust_anchors_verify_CA(
	    context, &verify_CA)))
		DEBUG_ANCHOR("ERROR %s(): Getting trust anchor verify"
			     " CA: \"%s\"\n", __FUNC__
			    , getdns_get_errorstr_by_id(r));

	else if (!verify_CA || !*verify_CA)
		DEBUG_ANCHOR("NOTICE: Trust anchor verification explicitely "
		             "disabled by empty verify CA\n");

	else if ((r = getdns_context_get_trust_anchors_verify_email(
	    context, &verify_email)))
		DEBUG_ANCHOR("ERROR %s(): Getting trust anchor verify email "
		             "address: \"%s\"\n", __FUNC__
		            , getdns_get_errorstr_by_id(r));

	else if (!verify_email || !*verify_email)
		DEBUG_ANCHOR("NOTICE: Trust anchor verification explicitely "
		             "disabled by empty verify email\n");

	else if (!(xml_data = _getdns_context_get_priv_file(context,
	    "root-anchors.xml", xml_spc, sizeof(xml_spc), &xml_len)))
		DEBUG_ANCHOR("DEBUG %s(): root-anchors.xml not present\n"
		            , __FUNC__);

	else if (!(p7s_data = _getdns_context_get_priv_file(context,
	    "root-anchors.p7s", p7s_spc, sizeof(p7s_spc), &p7s_len)))
		DEBUG_ANCHOR("DEBUG %s(): root-anchors.p7s not present\n"
		            , __FUNC__);

	else if (!(xml = BIO_new_mem_buf(xml_data, xml_len)))
		DEBUG_ANCHOR("ERROR %s(): Failed allocating xml BIO\n"
		            , __FUNC__);

	else if (!(p7s = BIO_new_mem_buf(p7s_data, p7s_len)))
		DEBUG_ANCHOR("ERROR %s(): Failed allocating p7s BIO\n"
		            , __FUNC__);

	else if (!(crt = BIO_new_mem_buf((void *)verify_CA, -1)))
		DEBUG_ANCHOR("ERROR %s(): Failed allocating crt BIO\n"
		            , __FUNC__);

	else if (!(x = PEM_read_bio_X509(crt, NULL, 0, NULL)))
		DEBUG_ANCHOR("ERROR %s(): Parsing builtin certificate\n"
		            , __FUNC__);

	else if (!(store = X509_STORE_new()))
		DEBUG_ANCHOR("ERROR %s(): Failed allocating store\n"
		            , __FUNC__);

	else if (!X509_STORE_add_cert(store, x))
		DEBUG_ANCHOR("ERROR %s(): Adding certificate to store\n"
		            , __FUNC__);

	else if (_getdns_verify_p7sig(xml, p7s, store, verify_email)) {
		uint8_t ta_spc[sizeof(context->trust_anchors_spc)];
		size_t ta_len;
		uint8_t *ta = NULL;
		gldns_buffer gbuf;

		gldns_buffer_init_vfixed_frm_data(
		    &gbuf, ta_spc, sizeof(ta_spc));

		if (!_getdns_parse_xml_trust_anchors_buf(&gbuf, now_ms,
		    (char *)xml_data, xml_len))
			DEBUG_ANCHOR("Failed to parse trust anchor XML data");
		else if ((ta_len = gldns_buffer_position(&gbuf)) > sizeof(ta_spc)) {
			if ((ta = GETDNS_XMALLOC(context->mf, uint8_t, ta_len))) {
				gldns_buffer_init_frm_data(&gbuf, ta,
				    gldns_buffer_position(&gbuf));
				if (!_getdns_parse_xml_trust_anchors_buf(
				    &gbuf, now_ms, (char *)xml_data, xml_len)) {
					DEBUG_ANCHOR("Failed to re-parse trust"
					             " anchor XML data");
					GETDNS_FREE(context->mf, ta);
				} else {
					context->trust_anchors = ta;
					context->trust_anchors_len = ta_len;
					context->trust_anchors_source = GETDNS_TASRC_XML;
					_getdns_ta_notify_dnsreqs(context);
				}
			} else
				DEBUG_ANCHOR("Could not allocate space for XML file");
		} else {
			(void)memcpy(context->trust_anchors_spc, ta_spc, ta_len);
			context->trust_anchors = context->trust_anchors_spc;
			context->trust_anchors_len = ta_len;
			context->trust_anchors_source = GETDNS_TASRC_XML;
			_getdns_ta_notify_dnsreqs(context);
		}
		DEBUG_ANCHOR("ta: %p, ta_len: %d\n",
		    (void *)context->trust_anchors, (int)context->trust_anchors_len);
		
	} else {
		DEBUG_ANCHOR("Verifying trust-anchors failed!\n");
	}
	if (store)	X509_STORE_free(store);
	if (x)		X509_free(x);
	if (crt)	BIO_free(crt);
	if (xml)	BIO_free(xml);
	if (p7s)	BIO_free(p7s);
	if (xml_data && xml_data != xml_spc)
		GETDNS_FREE(context->mf, xml_data);
	if (p7s_data && p7s_data != p7s_spc)
		GETDNS_FREE(context->mf, p7s_data);
}
