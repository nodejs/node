/* rrdef.c
 *
 * access functions to rr definitions list.
 * a Net::DNS like library for C
 * LibDNS Team @ NLnet Labs
 *
 * (c) NLnet Labs, 2004-2006
 * See the file LICENSE for the license
 */
/**
 * \file
 *
 * Defines resource record types and constants.
 */
#include "config.h"
#include "gldns/rrdef.h"
#include "gldns/parseutil.h"

#include <stdlib.h>

/* classes  */
static gldns_lookup_table gldns_rr_classes_data[] = {
        { GLDNS_RR_CLASS_IN, "IN" },
        { GLDNS_RR_CLASS_CH, "CH" },
        { GLDNS_RR_CLASS_HS, "HS" },
        { GLDNS_RR_CLASS_NONE, "NONE" },
        { GLDNS_RR_CLASS_ANY, "ANY" },
        { 0, NULL }
};
gldns_lookup_table* gldns_rr_classes = gldns_rr_classes_data;

/* types */
static const gldns_rdf_type type_0_wireformat[] = { GLDNS_RDF_TYPE_UNKNOWN };
static const gldns_rdf_type type_a_wireformat[] = { GLDNS_RDF_TYPE_A };
static const gldns_rdf_type type_ns_wireformat[] = { GLDNS_RDF_TYPE_DNAME };
static const gldns_rdf_type type_md_wireformat[] = { GLDNS_RDF_TYPE_DNAME };
static const gldns_rdf_type type_mf_wireformat[] = { GLDNS_RDF_TYPE_DNAME };
static const gldns_rdf_type type_cname_wireformat[] = { GLDNS_RDF_TYPE_DNAME };
static const gldns_rdf_type type_soa_wireformat[] = {
	GLDNS_RDF_TYPE_DNAME, GLDNS_RDF_TYPE_DNAME, GLDNS_RDF_TYPE_INT32, 
	GLDNS_RDF_TYPE_PERIOD, GLDNS_RDF_TYPE_PERIOD, GLDNS_RDF_TYPE_PERIOD,
	GLDNS_RDF_TYPE_PERIOD
};
static const gldns_rdf_type type_mb_wireformat[] = { GLDNS_RDF_TYPE_DNAME };
static const gldns_rdf_type type_mg_wireformat[] = { GLDNS_RDF_TYPE_DNAME };
static const gldns_rdf_type type_mr_wireformat[] = { GLDNS_RDF_TYPE_DNAME };
static const gldns_rdf_type type_wks_wireformat[] = {
	GLDNS_RDF_TYPE_A, GLDNS_RDF_TYPE_WKS
};
static const gldns_rdf_type type_ptr_wireformat[] = { GLDNS_RDF_TYPE_DNAME };
static const gldns_rdf_type type_hinfo_wireformat[] = {
	GLDNS_RDF_TYPE_STR, GLDNS_RDF_TYPE_STR
};
static const gldns_rdf_type type_minfo_wireformat[] = {
	GLDNS_RDF_TYPE_DNAME, GLDNS_RDF_TYPE_DNAME
};
static const gldns_rdf_type type_mx_wireformat[] = {
	GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_DNAME
};
static const gldns_rdf_type type_rp_wireformat[] = {
	GLDNS_RDF_TYPE_DNAME, GLDNS_RDF_TYPE_DNAME
};
static const gldns_rdf_type type_afsdb_wireformat[] = {
	GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_DNAME
};
static const gldns_rdf_type type_x25_wireformat[] = { GLDNS_RDF_TYPE_STR };
static const gldns_rdf_type type_isdn_wireformat[] = {
	GLDNS_RDF_TYPE_STR, GLDNS_RDF_TYPE_STR
};
static const gldns_rdf_type type_rt_wireformat[] = {
	GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_DNAME
};
static const gldns_rdf_type type_nsap_wireformat[] = {
	GLDNS_RDF_TYPE_NSAP
};
static const gldns_rdf_type type_nsap_ptr_wireformat[] = {
	GLDNS_RDF_TYPE_STR
};
static const gldns_rdf_type type_sig_wireformat[] = {
	GLDNS_RDF_TYPE_TYPE, GLDNS_RDF_TYPE_ALG, GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_INT32,
	GLDNS_RDF_TYPE_TIME, GLDNS_RDF_TYPE_TIME, GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_DNAME, GLDNS_RDF_TYPE_B64
};
static const gldns_rdf_type type_key_wireformat[] = {
	GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_B64
};
static const gldns_rdf_type type_px_wireformat[] = {
	GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_DNAME, GLDNS_RDF_TYPE_DNAME
};
static const gldns_rdf_type type_gpos_wireformat[] = {
	GLDNS_RDF_TYPE_STR, GLDNS_RDF_TYPE_STR, GLDNS_RDF_TYPE_STR
};
static const gldns_rdf_type type_aaaa_wireformat[] = { GLDNS_RDF_TYPE_AAAA };
static const gldns_rdf_type type_loc_wireformat[] = { GLDNS_RDF_TYPE_LOC };
static const gldns_rdf_type type_nxt_wireformat[] = {
	GLDNS_RDF_TYPE_DNAME, GLDNS_RDF_TYPE_UNKNOWN
};
static const gldns_rdf_type type_eid_wireformat[] = {
	GLDNS_RDF_TYPE_HEX
};
static const gldns_rdf_type type_nimloc_wireformat[] = {
	GLDNS_RDF_TYPE_HEX
};
static const gldns_rdf_type type_srv_wireformat[] = {
	GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_DNAME
};
static const gldns_rdf_type type_atma_wireformat[] = {
	GLDNS_RDF_TYPE_ATMA
};
static const gldns_rdf_type type_naptr_wireformat[] = {
	GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_STR, GLDNS_RDF_TYPE_STR, GLDNS_RDF_TYPE_STR, GLDNS_RDF_TYPE_DNAME
};
static const gldns_rdf_type type_kx_wireformat[] = {
	GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_DNAME
};
static const gldns_rdf_type type_cert_wireformat[] = {
	 GLDNS_RDF_TYPE_CERT_ALG, GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_ALG, GLDNS_RDF_TYPE_B64
};
static const gldns_rdf_type type_a6_wireformat[] = { GLDNS_RDF_TYPE_UNKNOWN };
static const gldns_rdf_type type_dname_wireformat[] = { GLDNS_RDF_TYPE_DNAME };
static const gldns_rdf_type type_sink_wireformat[] = { GLDNS_RDF_TYPE_INT8,
	GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_B64
};
static const gldns_rdf_type type_apl_wireformat[] = {
	GLDNS_RDF_TYPE_APL
};
static const gldns_rdf_type type_ds_wireformat[] = {
	GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_ALG, GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_HEX
};
static const gldns_rdf_type type_sshfp_wireformat[] = {
	GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_HEX
};
static const gldns_rdf_type type_ipseckey_wireformat[] = {
	GLDNS_RDF_TYPE_IPSECKEY
};
static const gldns_rdf_type type_rrsig_wireformat[] = {
	GLDNS_RDF_TYPE_TYPE, GLDNS_RDF_TYPE_ALG, GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_INT32,
	GLDNS_RDF_TYPE_TIME, GLDNS_RDF_TYPE_TIME, GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_DNAME, GLDNS_RDF_TYPE_B64
};
static const gldns_rdf_type type_nsec_wireformat[] = {
	GLDNS_RDF_TYPE_DNAME, GLDNS_RDF_TYPE_NSEC
};
static const gldns_rdf_type type_dhcid_wireformat[] = {
	GLDNS_RDF_TYPE_B64
};
static const gldns_rdf_type type_talink_wireformat[] = {
	GLDNS_RDF_TYPE_DNAME, GLDNS_RDF_TYPE_DNAME
};
static const gldns_rdf_type type_openpgpkey_wireformat[] = {
	GLDNS_RDF_TYPE_B64
};
static const gldns_rdf_type type_csync_wireformat[] = {
	GLDNS_RDF_TYPE_INT32, GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_NSEC
};
static const gldns_rdf_type type_zonemd_wireformat[] = {
	GLDNS_RDF_TYPE_INT32, GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_HEX
};
/* nsec3 is some vars, followed by same type of data of nsec */
static const gldns_rdf_type type_nsec3_wireformat[] = {
/*	GLDNS_RDF_TYPE_NSEC3_VARS, GLDNS_RDF_TYPE_NSEC3_NEXT_OWNER, GLDNS_RDF_TYPE_NSEC*/
	GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_INT8, GLDNS_RDF_TYPE_INT16, GLDNS_RDF_TYPE_NSEC3_SALT, GLDNS_RDF_TYPE_NSEC3_NEXT_OWNER, GLDNS_RDF_TYPE_NSEC
};

static const gldns_rdf_type type_nsec3param_wireformat[] = {
/*	GLDNS_RDF_TYPE_NSEC3_PARAMS_VARS*/
	GLDNS_RDF_TYPE_INT8,
	GLDNS_RDF_TYPE_INT8,
	GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_NSEC3_SALT
};

static const gldns_rdf_type type_dnskey_wireformat[] = {
	GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_INT8,
	GLDNS_RDF_TYPE_ALG,
	GLDNS_RDF_TYPE_B64
};
static const gldns_rdf_type type_tkey_wireformat[] = {
	GLDNS_RDF_TYPE_DNAME,
	GLDNS_RDF_TYPE_TIME,
	GLDNS_RDF_TYPE_TIME,
	GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_TSIGERROR,
	GLDNS_RDF_TYPE_INT16_DATA,
	GLDNS_RDF_TYPE_INT16_DATA,
};
static const gldns_rdf_type type_tsig_wireformat[] = {
	GLDNS_RDF_TYPE_DNAME,
	GLDNS_RDF_TYPE_TSIGTIME,
	GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_INT16_DATA,
	GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_TSIGERROR,
	GLDNS_RDF_TYPE_INT16_DATA
};
static const gldns_rdf_type type_tlsa_wireformat[] = {
	GLDNS_RDF_TYPE_INT8,
	GLDNS_RDF_TYPE_INT8,
	GLDNS_RDF_TYPE_INT8,
	GLDNS_RDF_TYPE_HEX
};
static const gldns_rdf_type type_hip_wireformat[] = {
	GLDNS_RDF_TYPE_HIP
};
static const gldns_rdf_type type_nid_wireformat[] = {
	GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_ILNP64
};
static const gldns_rdf_type type_l32_wireformat[] = {
	GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_A
};
static const gldns_rdf_type type_l64_wireformat[] = {
	GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_ILNP64
};
static const gldns_rdf_type type_lp_wireformat[] = {
	GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_DNAME
};
static const gldns_rdf_type type_eui48_wireformat[] = {
	GLDNS_RDF_TYPE_EUI48
};
static const gldns_rdf_type type_eui64_wireformat[] = {
	GLDNS_RDF_TYPE_EUI64
};
static const gldns_rdf_type type_uri_wireformat[] = {
	GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_INT16,
	GLDNS_RDF_TYPE_LONG_STR
};
static const gldns_rdf_type type_caa_wireformat[] = {
	GLDNS_RDF_TYPE_INT8,
	GLDNS_RDF_TYPE_TAG,
	GLDNS_RDF_TYPE_LONG_STR
};
#ifdef DRAFT_RRTYPES
static const gldns_rdf_type type_doa_wireformat[] = {
	GLDNS_RDF_TYPE_INT32, GLDNS_RDF_TYPE_INT32, GLDNS_RDF_TYPE_INT8,
	GLDNS_RDF_TYPE_STR, GLDNS_RDF_TYPE_B64
};
static const gldns_rdf_type type_amtrelay_wireformat[] = {
	GLDNS_RDF_TYPE_AMTRELAY
};
#endif

/* All RR's defined in 1035 are well known and can thus
 * be compressed. See RFC3597. These RR's are:
 * CNAME HINFO MB MD MF MG MINFO MR MX NULL NS PTR SOA TXT
 */
static gldns_rr_descriptor rdata_field_descriptors[] = {
	/* 0 */
	{(enum gldns_enum_rr_type)0, NULL, 0, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 1 */
	{GLDNS_RR_TYPE_A, "A", 1, 1, type_a_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 2 */
	{GLDNS_RR_TYPE_NS, "NS", 1, 1, type_ns_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_COMPRESS, 1 },
	/* 3 */
	{GLDNS_RR_TYPE_MD, "MD", 1, 1, type_md_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_COMPRESS, 1 },
	/* 4 */
	{GLDNS_RR_TYPE_MF, "MF", 1, 1, type_mf_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_COMPRESS, 1 },
	/* 5 */
	{GLDNS_RR_TYPE_CNAME, "CNAME", 1, 1, type_cname_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_COMPRESS, 1 },
	/* 6 */
	{GLDNS_RR_TYPE_SOA, "SOA", 7, 7, type_soa_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_COMPRESS, 2 },
	/* 7 */
	{GLDNS_RR_TYPE_MB, "MB", 1, 1, type_mb_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_COMPRESS, 1 },
	/* 8 */
	{GLDNS_RR_TYPE_MG, "MG", 1, 1, type_mg_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_COMPRESS, 1 },
	/* 9 */
	{GLDNS_RR_TYPE_MR, "MR", 1, 1, type_mr_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_COMPRESS, 1 },
	/* 10 */
	{GLDNS_RR_TYPE_NULL, "NULL", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 11 */
	{GLDNS_RR_TYPE_WKS, "WKS", 2, 2, type_wks_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 12 */
	{GLDNS_RR_TYPE_PTR, "PTR", 1, 1, type_ptr_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_COMPRESS, 1 },
	/* 13 */
	{GLDNS_RR_TYPE_HINFO, "HINFO", 2, 2, type_hinfo_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 14 */
	{GLDNS_RR_TYPE_MINFO, "MINFO", 2, 2, type_minfo_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_COMPRESS, 2 },
	/* 15 */
	{GLDNS_RR_TYPE_MX, "MX", 2, 2, type_mx_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_COMPRESS, 1 },
	/* 16 */
	{GLDNS_RR_TYPE_TXT, "TXT", 1, 0, NULL, GLDNS_RDF_TYPE_STR, GLDNS_RR_NO_COMPRESS, 0 },
	/* 17 */
	{GLDNS_RR_TYPE_RP, "RP", 2, 2, type_rp_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 2 },
	/* 18 */
	{GLDNS_RR_TYPE_AFSDB, "AFSDB", 2, 2, type_afsdb_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },
	/* 19 */
	{GLDNS_RR_TYPE_X25, "X25", 1, 1, type_x25_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 20 */
	{GLDNS_RR_TYPE_ISDN, "ISDN", 1, 2, type_isdn_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 21 */
	{GLDNS_RR_TYPE_RT, "RT", 2, 2, type_rt_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },
	/* 22 */
	{GLDNS_RR_TYPE_NSAP, "NSAP", 1, 1, type_nsap_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 23 */
	{GLDNS_RR_TYPE_NSAP_PTR, "NSAP-PTR", 1, 1, type_nsap_ptr_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 24 */
	{GLDNS_RR_TYPE_SIG, "SIG", 9, 9, type_sig_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },
	/* 25 */
	{GLDNS_RR_TYPE_KEY, "KEY", 4, 4, type_key_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 26 */
	{GLDNS_RR_TYPE_PX, "PX", 3, 3, type_px_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 2 },
	/* 27 */
	{GLDNS_RR_TYPE_GPOS, "GPOS", 3, 3, type_gpos_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 28 */
	{GLDNS_RR_TYPE_AAAA, "AAAA", 1, 1, type_aaaa_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 29 */
	{GLDNS_RR_TYPE_LOC, "LOC", 1, 1, type_loc_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 30 */
	{GLDNS_RR_TYPE_NXT, "NXT", 2, 2, type_nxt_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },
	/* 31 */
	{GLDNS_RR_TYPE_EID, "EID", 1, 1, type_eid_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 32 */
	{GLDNS_RR_TYPE_NIMLOC, "NIMLOC", 1, 1, type_nimloc_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 33 */
	{GLDNS_RR_TYPE_SRV, "SRV", 4, 4, type_srv_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },
	/* 34 */
	{GLDNS_RR_TYPE_ATMA, "ATMA", 1, 1, type_atma_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 35 */
	{GLDNS_RR_TYPE_NAPTR, "NAPTR", 6, 6, type_naptr_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },
	/* 36 */
	{GLDNS_RR_TYPE_KX, "KX", 2, 2, type_kx_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },
	/* 37 */
	{GLDNS_RR_TYPE_CERT, "CERT", 4, 4, type_cert_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 38 */
	{GLDNS_RR_TYPE_A6, "A6", 1, 1, type_a6_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 39 */
	{GLDNS_RR_TYPE_DNAME, "DNAME", 1, 1, type_dname_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },
	/* 40 */
	{GLDNS_RR_TYPE_SINK, "SINK", 1, 1, type_sink_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 41 */
	{GLDNS_RR_TYPE_OPT, "OPT", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 42 */
	{GLDNS_RR_TYPE_APL, "APL", 0, 0, type_apl_wireformat, GLDNS_RDF_TYPE_APL, GLDNS_RR_NO_COMPRESS, 0 },
	/* 43 */
	{GLDNS_RR_TYPE_DS, "DS", 4, 4, type_ds_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 44 */
	{GLDNS_RR_TYPE_SSHFP, "SSHFP", 3, 3, type_sshfp_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 45 */
	{GLDNS_RR_TYPE_IPSECKEY, "IPSECKEY", 1, 1, type_ipseckey_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 46 */
	{GLDNS_RR_TYPE_RRSIG, "RRSIG", 9, 9, type_rrsig_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },
	/* 47 */
	{GLDNS_RR_TYPE_NSEC, "NSEC", 1, 2, type_nsec_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },
	/* 48 */
	{GLDNS_RR_TYPE_DNSKEY, "DNSKEY", 4, 4, type_dnskey_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 49 */
	{GLDNS_RR_TYPE_DHCID, "DHCID", 1, 1, type_dhcid_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 50 */
	{GLDNS_RR_TYPE_NSEC3, "NSEC3", 5, 6, type_nsec3_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 51 */
	{GLDNS_RR_TYPE_NSEC3PARAM, "NSEC3PARAM", 4, 4, type_nsec3param_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 52 */
	{GLDNS_RR_TYPE_TLSA, "TLSA", 4, 4, type_tlsa_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 53 */
	{GLDNS_RR_TYPE_SMIMEA, "SMIMEA", 4, 4, type_tlsa_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 54 */
{(enum gldns_enum_rr_type)0, "TYPE54", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
        /* 55
	 * Hip ends with 0 or more Rendezvous Servers represented as dname's.
	 * Hence the GLDNS_RDF_TYPE_DNAME _variable field and the _maximum field
	 * set to 0.
	 */
	{GLDNS_RR_TYPE_HIP, "HIP", 1, 1, type_hip_wireformat, GLDNS_RDF_TYPE_DNAME, GLDNS_RR_NO_COMPRESS, 0 },

#ifdef DRAFT_RRTYPES
	/* 56 */
	{GLDNS_RR_TYPE_NINFO, "NINFO", 1, 0, NULL, GLDNS_RDF_TYPE_STR, GLDNS_RR_NO_COMPRESS, 0 },
	/* 57 */
	{GLDNS_RR_TYPE_RKEY, "RKEY", 4, 4, type_key_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
#else
{(enum gldns_enum_rr_type)0, "TYPE56", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE57", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
#endif
	/* 58 */
	{GLDNS_RR_TYPE_TALINK, "TALINK", 2, 2, type_talink_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 2 },

	/* 59 */
	{GLDNS_RR_TYPE_CDS, "CDS", 4, 4, type_ds_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 60 */
	{GLDNS_RR_TYPE_CDNSKEY, "CDNSKEY", 4, 4, type_dnskey_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 61 */
{GLDNS_RR_TYPE_OPENPGPKEY, "OPENPGPKEY", 1, 1, type_openpgpkey_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 62 */
	{GLDNS_RR_TYPE_CSYNC, "CSYNC", 3, 3, type_csync_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 63 */
	{GLDNS_RR_TYPE_ZONEMD, "ZONEMD", 4, 4, type_zonemd_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE64", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE65", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE66", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE67", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE68", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE69", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE70", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE71", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE72", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE73", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE74", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE75", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE76", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE77", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE78", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE79", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE80", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE81", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE82", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE83", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE84", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE85", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE86", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE87", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE88", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE89", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE90", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE91", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE92", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE93", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE94", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE95", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE96", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE97", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE98", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },

	/* 99 */
	{GLDNS_RR_TYPE_SPF,  "SPF", 1, 0, NULL, GLDNS_RDF_TYPE_STR, GLDNS_RR_NO_COMPRESS, 0 },

	/* UINFO  [IANA-Reserved] */
{(enum gldns_enum_rr_type)0, "TYPE100", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* UID    [IANA-Reserved] */
{(enum gldns_enum_rr_type)0, "TYPE101", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* GID    [IANA-Reserved] */
{(enum gldns_enum_rr_type)0, "TYPE102", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* UNSPEC [IANA-Reserved] */
{(enum gldns_enum_rr_type)0, "TYPE103", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },

	/* 104 */
	{GLDNS_RR_TYPE_NID, "NID", 2, 2, type_nid_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 105 */
	{GLDNS_RR_TYPE_L32, "L32", 2, 2, type_l32_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 106 */
	{GLDNS_RR_TYPE_L64, "L64", 2, 2, type_l64_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 107 */
	{GLDNS_RR_TYPE_LP, "LP", 2, 2, type_lp_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },

	/* 108 */
	{GLDNS_RR_TYPE_EUI48, "EUI48", 1, 1, type_eui48_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 109 */
	{GLDNS_RR_TYPE_EUI64, "EUI64", 1, 1, type_eui64_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },

{(enum gldns_enum_rr_type)0, "TYPE110", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE111", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE112", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE113", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE114", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE115", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE116", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE117", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE118", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE119", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE120", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE121", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE122", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE123", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE124", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE125", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE126", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE127", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE128", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE129", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE130", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE131", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE132", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE133", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE134", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE135", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE136", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE137", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE138", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE139", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE140", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE141", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE142", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE143", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE144", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE145", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE146", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE147", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE148", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE149", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE150", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE151", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE152", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE153", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE154", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE155", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE156", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE157", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE158", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE159", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE160", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE161", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE162", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE163", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE164", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE165", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE166", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE167", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE168", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE169", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE170", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE171", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE172", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE173", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE174", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE175", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE176", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE177", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE178", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE179", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE180", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE181", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE182", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE183", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE184", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE185", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE186", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE187", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE188", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE189", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE190", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE191", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE192", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE193", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE194", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE195", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE196", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE197", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE198", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE199", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE200", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE201", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE202", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE203", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE204", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE205", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE206", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE207", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE208", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE209", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE210", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE211", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE212", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE213", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE214", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE215", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE216", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE217", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE218", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE219", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE220", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE221", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE222", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE223", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE224", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE225", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE226", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE227", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE228", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE229", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE230", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE231", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE232", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE233", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE234", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE235", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE236", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE237", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE238", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE239", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE240", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE241", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE242", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE243", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE244", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE245", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE246", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE247", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE248", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },

	/* GLDNS_RDF_TYPE_INT16_DATA takes two fields (length and data) as one.
	 * So, unlike RFC 2930 spec, we have 7 min/max rdf's i.s.o. 8/9.
	 */
	/* 249 */
	{GLDNS_RR_TYPE_TKEY, "TKEY", 7, 7, type_tkey_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },
	/* GLDNS_RDF_TYPE_INT16_DATA takes two fields (length and data) as one.
	 * So, unlike RFC 2930 spec, we have 7 min/max rdf's i.s.o. 8/9.
	 */
	/* 250 */
	{GLDNS_RR_TYPE_TSIG, "TSIG", 7, 7, type_tsig_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 1 },

	/* IXFR: A request for a transfer of an incremental zone transfer */
{GLDNS_RR_TYPE_IXFR, "IXFR", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* AXFR: A request for a transfer of an entire zone */
{GLDNS_RR_TYPE_AXFR, "AXFR", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* MAILB: A request for mailbox-related records (MB, MG or MR) */
{GLDNS_RR_TYPE_MAILB, "MAILB", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* MAILA: A request for mail agent RRs (Obsolete - see MX) */
{GLDNS_RR_TYPE_MAILA, "MAILA", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* ANY: A request for all (available) records */
{GLDNS_RR_TYPE_ANY, "ANY", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },

	/* 256 */
	{GLDNS_RR_TYPE_URI, "URI", 3, 3, type_uri_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 257 */
	{GLDNS_RR_TYPE_CAA, "CAA", 3, 3, type_caa_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
#ifdef DRAFT_RRTYPES
	/* 258 */
	{GLDNS_RR_TYPE_AVC, "AVC", 1, 0, NULL, GLDNS_RDF_TYPE_STR, GLDNS_RR_NO_COMPRESS, 0 },
	/* 259 */
	{GLDNS_RR_TYPE_DOA, "DOA", 1, 0, type_doa_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
	/* 260 */
	{GLDNS_RR_TYPE_AMTRELAY, "AMTRELAY", 1, 0, type_amtrelay_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
#else
{(enum gldns_enum_rr_type)0, "TYPE258", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE259", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
{(enum gldns_enum_rr_type)0, "TYPE260", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
#endif

/* split in array, no longer contiguous */

#ifdef DRAFT_RRTYPES
	/* 32768 */
	{GLDNS_RR_TYPE_TA, "TA", 4, 4, type_ds_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
#else
{(enum gldns_enum_rr_type)0, "TYPE32768", 1, 1, type_0_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 },
#endif
	/* 32769 */
	{GLDNS_RR_TYPE_DLV, "DLV", 4, 4, type_ds_wireformat, GLDNS_RDF_TYPE_NONE, GLDNS_RR_NO_COMPRESS, 0 }
};

/**
 * \def GLDNS_RDATA_FIELD_DESCRIPTORS_COUNT
 * computes the number of rdata fields
 */
#define GLDNS_RDATA_FIELD_DESCRIPTORS_COUNT \
	(sizeof(rdata_field_descriptors)/sizeof(rdata_field_descriptors[0]))

const gldns_rr_descriptor *
gldns_rr_descript(uint16_t type)
{
	size_t i;
	if (type < GLDNS_RDATA_FIELD_DESCRIPTORS_COMMON) {
		return &rdata_field_descriptors[type];
	} else {
		/* because not all array index equals type code */
		for (i = GLDNS_RDATA_FIELD_DESCRIPTORS_COMMON;
		     i < GLDNS_RDATA_FIELD_DESCRIPTORS_COUNT;
		     i++) {
		        if (rdata_field_descriptors[i]._type == type) {
		     		return &rdata_field_descriptors[i];
			}
		}
                return &rdata_field_descriptors[0];
	}
}

size_t
gldns_rr_descriptor_minimum(const gldns_rr_descriptor *descriptor)
{
	if (descriptor) {
		return descriptor->_minimum;
	} else {
		return 0;
	}
}

size_t
gldns_rr_descriptor_maximum(const gldns_rr_descriptor *descriptor)
{
	if (descriptor) {
		if (descriptor->_variable != GLDNS_RDF_TYPE_NONE) {
			return 65535; /* cannot be more than 64k */
		} else {
			return descriptor->_maximum;
		}
	} else {
		return 0;
	}
}

gldns_rdf_type
gldns_rr_descriptor_field_type(const gldns_rr_descriptor *descriptor,
                              size_t index)
{
	assert(descriptor != NULL);
	assert(index < descriptor->_maximum
	       || descriptor->_variable != GLDNS_RDF_TYPE_NONE);
	if (index < descriptor->_maximum) {
		return descriptor->_wireformat[index];
	} else {
		return descriptor->_variable;
	}
}

gldns_rr_type
gldns_get_rr_type_by_name(const char *name)
{
	unsigned int i;
	const char *desc_name;
	const gldns_rr_descriptor *desc;

	/* TYPEXX representation */
	if (strlen(name) > 4 && strncasecmp(name, "TYPE", 4) == 0) {
		return atoi(name + 4);
	}

	/* Normal types */
	for (i = 0; i < (unsigned int) GLDNS_RDATA_FIELD_DESCRIPTORS_COUNT; i++) {
		desc = &rdata_field_descriptors[i];
		desc_name = desc->_name;
		if(desc_name &&
		   strlen(name) == strlen(desc_name) &&
		   strncasecmp(name, desc_name, strlen(desc_name)) == 0) {
			/* because not all array index equals type code */
			return desc->_type;
		}
	}

	/* special cases for query types */
	if (strlen(name) == 4 && strncasecmp(name, "IXFR", 4) == 0) {
		return GLDNS_RR_TYPE_IXFR;
	} else if (strlen(name) == 4 && strncasecmp(name, "AXFR", 4) == 0) {
		return GLDNS_RR_TYPE_AXFR;
	} else if (strlen(name) == 5 && strncasecmp(name, "MAILB", 5) == 0) {
		return GLDNS_RR_TYPE_MAILB;
	} else if (strlen(name) == 5 && strncasecmp(name, "MAILA", 5) == 0) {
		return GLDNS_RR_TYPE_MAILA;
	} else if (strlen(name) == 3 && strncasecmp(name, "ANY", 3) == 0) {
		return GLDNS_RR_TYPE_ANY;
	}

	return (enum gldns_enum_rr_type)0;
}

gldns_rr_class
gldns_get_rr_class_by_name(const char *name)
{
	gldns_lookup_table *lt;

	/* CLASSXX representation */
	if (strlen(name) > 5 && strncasecmp(name, "CLASS", 5) == 0) {
		return atoi(name + 5);
	}

	/* Normal types */
	lt = gldns_lookup_by_name(gldns_rr_classes, name);
	if (lt) {
		return lt->id;
	}
	return 0;
}
