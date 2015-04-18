/* ext_dat.h */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2004 The OpenSSL Project.  All rights reserved.
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
/* This file contains a table of "standard" extensions */

extern X509V3_EXT_METHOD v3_bcons, v3_nscert, v3_key_usage, v3_ext_ku;
extern X509V3_EXT_METHOD v3_pkey_usage_period, v3_sxnet, v3_info, v3_sinfo;
extern X509V3_EXT_METHOD v3_ns_ia5_list[], v3_alt[], v3_skey_id, v3_akey_id;
extern X509V3_EXT_METHOD v3_crl_num, v3_crl_reason, v3_crl_invdate;
extern X509V3_EXT_METHOD v3_delta_crl, v3_cpols, v3_crld, v3_freshest_crl;
extern X509V3_EXT_METHOD v3_ocsp_nonce, v3_ocsp_accresp, v3_ocsp_acutoff;
extern X509V3_EXT_METHOD v3_ocsp_crlid, v3_ocsp_nocheck, v3_ocsp_serviceloc;
extern X509V3_EXT_METHOD v3_crl_hold, v3_pci;
extern X509V3_EXT_METHOD v3_policy_mappings, v3_policy_constraints;
extern X509V3_EXT_METHOD v3_name_constraints, v3_inhibit_anyp, v3_idp;
extern X509V3_EXT_METHOD v3_addr, v3_asid;

/*
 * This table will be searched using OBJ_bsearch so it *must* kept in order
 * of the ext_nid values.
 */

static const X509V3_EXT_METHOD *standard_exts[] = {
    &v3_nscert,
    &v3_ns_ia5_list[0],
    &v3_ns_ia5_list[1],
    &v3_ns_ia5_list[2],
    &v3_ns_ia5_list[3],
    &v3_ns_ia5_list[4],
    &v3_ns_ia5_list[5],
    &v3_ns_ia5_list[6],
    &v3_skey_id,
    &v3_key_usage,
    &v3_pkey_usage_period,
    &v3_alt[0],
    &v3_alt[1],
    &v3_bcons,
    &v3_crl_num,
    &v3_cpols,
    &v3_akey_id,
    &v3_crld,
    &v3_ext_ku,
    &v3_delta_crl,
    &v3_crl_reason,
#ifndef OPENSSL_NO_OCSP
    &v3_crl_invdate,
#endif
    &v3_sxnet,
    &v3_info,
#ifndef OPENSSL_NO_RFC3779
    &v3_addr,
    &v3_asid,
#endif
#ifndef OPENSSL_NO_OCSP
    &v3_ocsp_nonce,
    &v3_ocsp_crlid,
    &v3_ocsp_accresp,
    &v3_ocsp_nocheck,
    &v3_ocsp_acutoff,
    &v3_ocsp_serviceloc,
#endif
    &v3_sinfo,
    &v3_policy_constraints,
#ifndef OPENSSL_NO_OCSP
    &v3_crl_hold,
#endif
    &v3_pci,
    &v3_name_constraints,
    &v3_policy_mappings,
    &v3_inhibit_anyp,
    &v3_idp,
    &v3_alt[2],
    &v3_freshest_crl,
};

/* Number of standard extensions */

#define STANDARD_EXTENSION_COUNT (sizeof(standard_exts)/sizeof(X509V3_EXT_METHOD *))
