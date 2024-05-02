/*
 * Copyright 1999-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

int ossl_v3_name_cmp(const char *name, const char *cmp);

extern const X509V3_EXT_METHOD ossl_v3_bcons, ossl_v3_nscert, ossl_v3_key_usage, ossl_v3_ext_ku;
extern const X509V3_EXT_METHOD ossl_v3_pkey_usage_period, ossl_v3_sxnet, ossl_v3_info, ossl_v3_sinfo;
extern const X509V3_EXT_METHOD ossl_v3_ns_ia5_list[8], ossl_v3_alt[3], ossl_v3_skey_id, ossl_v3_akey_id;
extern const X509V3_EXT_METHOD ossl_v3_crl_num, ossl_v3_crl_reason, ossl_v3_crl_invdate;
extern const X509V3_EXT_METHOD ossl_v3_delta_crl, ossl_v3_cpols, ossl_v3_crld, ossl_v3_freshest_crl;
extern const X509V3_EXT_METHOD ossl_v3_ocsp_nonce, ossl_v3_ocsp_accresp, ossl_v3_ocsp_acutoff;
extern const X509V3_EXT_METHOD ossl_v3_ocsp_crlid, ossl_v3_ocsp_nocheck, ossl_v3_ocsp_serviceloc;
extern const X509V3_EXT_METHOD ossl_v3_crl_hold, ossl_v3_pci;
extern const X509V3_EXT_METHOD ossl_v3_policy_mappings, ossl_v3_policy_constraints;
extern const X509V3_EXT_METHOD ossl_v3_name_constraints, ossl_v3_inhibit_anyp, ossl_v3_idp;
extern const X509V3_EXT_METHOD ossl_v3_addr, ossl_v3_asid;
extern const X509V3_EXT_METHOD ossl_v3_ct_scts[3];
extern const X509V3_EXT_METHOD ossl_v3_tls_feature;
extern const X509V3_EXT_METHOD ossl_v3_ext_admission;
extern const X509V3_EXT_METHOD ossl_v3_utf8_list[1];
extern const X509V3_EXT_METHOD ossl_v3_issuer_sign_tool;
