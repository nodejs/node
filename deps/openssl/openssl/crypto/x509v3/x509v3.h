/* x509v3.h */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
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
#ifndef HEADER_X509V3_H
#define HEADER_X509V3_H

#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/conf.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward reference */
struct v3_ext_method;
struct v3_ext_ctx;

/* Useful typedefs */

typedef void * (*X509V3_EXT_NEW)(void);
typedef void (*X509V3_EXT_FREE)(void *);
typedef void * (*X509V3_EXT_D2I)(void *, const unsigned char ** , long);
typedef int (*X509V3_EXT_I2D)(void *, unsigned char **);
typedef STACK_OF(CONF_VALUE) * (*X509V3_EXT_I2V)(struct v3_ext_method *method, void *ext, STACK_OF(CONF_VALUE) *extlist);
typedef void * (*X509V3_EXT_V2I)(struct v3_ext_method *method, struct v3_ext_ctx *ctx, STACK_OF(CONF_VALUE) *values);
typedef char * (*X509V3_EXT_I2S)(struct v3_ext_method *method, void *ext);
typedef void * (*X509V3_EXT_S2I)(struct v3_ext_method *method, struct v3_ext_ctx *ctx, const char *str);
typedef int (*X509V3_EXT_I2R)(struct v3_ext_method *method, void *ext, BIO *out, int indent);
typedef void * (*X509V3_EXT_R2I)(struct v3_ext_method *method, struct v3_ext_ctx *ctx, const char *str);

/* V3 extension structure */

struct v3_ext_method {
int ext_nid;
int ext_flags;
/* If this is set the following four fields are ignored */
ASN1_ITEM_EXP *it;
/* Old style ASN1 calls */
X509V3_EXT_NEW ext_new;
X509V3_EXT_FREE ext_free;
X509V3_EXT_D2I d2i;
X509V3_EXT_I2D i2d;

/* The following pair is used for string extensions */
X509V3_EXT_I2S i2s;
X509V3_EXT_S2I s2i;

/* The following pair is used for multi-valued extensions */
X509V3_EXT_I2V i2v;
X509V3_EXT_V2I v2i;

/* The following are used for raw extensions */
X509V3_EXT_I2R i2r;
X509V3_EXT_R2I r2i;

void *usr_data;	/* Any extension specific data */
};

typedef struct X509V3_CONF_METHOD_st {
char * (*get_string)(void *db, char *section, char *value);
STACK_OF(CONF_VALUE) * (*get_section)(void *db, char *section);
void (*free_string)(void *db, char * string);
void (*free_section)(void *db, STACK_OF(CONF_VALUE) *section);
} X509V3_CONF_METHOD;

/* Context specific info */
struct v3_ext_ctx {
#define CTX_TEST 0x1
int flags;
X509 *issuer_cert;
X509 *subject_cert;
X509_REQ *subject_req;
X509_CRL *crl;
X509V3_CONF_METHOD *db_meth;
void *db;
/* Maybe more here */
};

typedef struct v3_ext_method X509V3_EXT_METHOD;

DECLARE_STACK_OF(X509V3_EXT_METHOD)

/* ext_flags values */
#define X509V3_EXT_DYNAMIC	0x1
#define X509V3_EXT_CTX_DEP	0x2
#define X509V3_EXT_MULTILINE	0x4

typedef BIT_STRING_BITNAME ENUMERATED_NAMES;

typedef struct BASIC_CONSTRAINTS_st {
int ca;
ASN1_INTEGER *pathlen;
} BASIC_CONSTRAINTS;


typedef struct PKEY_USAGE_PERIOD_st {
ASN1_GENERALIZEDTIME *notBefore;
ASN1_GENERALIZEDTIME *notAfter;
} PKEY_USAGE_PERIOD;

typedef struct otherName_st {
ASN1_OBJECT *type_id;
ASN1_TYPE *value;
} OTHERNAME;

typedef struct EDIPartyName_st {
	ASN1_STRING *nameAssigner;
	ASN1_STRING *partyName;
} EDIPARTYNAME;

typedef struct GENERAL_NAME_st {

#define GEN_OTHERNAME	0
#define GEN_EMAIL	1
#define GEN_DNS		2
#define GEN_X400	3
#define GEN_DIRNAME	4
#define GEN_EDIPARTY	5
#define GEN_URI		6
#define GEN_IPADD	7
#define GEN_RID		8

int type;
union {
	char *ptr;
	OTHERNAME *otherName; /* otherName */
	ASN1_IA5STRING *rfc822Name;
	ASN1_IA5STRING *dNSName;
	ASN1_TYPE *x400Address;
	X509_NAME *directoryName;
	EDIPARTYNAME *ediPartyName;
	ASN1_IA5STRING *uniformResourceIdentifier;
	ASN1_OCTET_STRING *iPAddress;
	ASN1_OBJECT *registeredID;

	/* Old names */
	ASN1_OCTET_STRING *ip; /* iPAddress */
	X509_NAME *dirn;		/* dirn */
	ASN1_IA5STRING *ia5;/* rfc822Name, dNSName, uniformResourceIdentifier */
	ASN1_OBJECT *rid; /* registeredID */
	ASN1_TYPE *other; /* x400Address */
} d;
} GENERAL_NAME;

typedef STACK_OF(GENERAL_NAME) GENERAL_NAMES;

typedef struct ACCESS_DESCRIPTION_st {
	ASN1_OBJECT *method;
	GENERAL_NAME *location;
} ACCESS_DESCRIPTION;

typedef STACK_OF(ACCESS_DESCRIPTION) AUTHORITY_INFO_ACCESS;

typedef STACK_OF(ASN1_OBJECT) EXTENDED_KEY_USAGE;

DECLARE_STACK_OF(GENERAL_NAME)
DECLARE_ASN1_SET_OF(GENERAL_NAME)

DECLARE_STACK_OF(ACCESS_DESCRIPTION)
DECLARE_ASN1_SET_OF(ACCESS_DESCRIPTION)

typedef struct DIST_POINT_NAME_st {
int type;
union {
	GENERAL_NAMES *fullname;
	STACK_OF(X509_NAME_ENTRY) *relativename;
} name;
} DIST_POINT_NAME;

typedef struct DIST_POINT_st {
DIST_POINT_NAME	*distpoint;
ASN1_BIT_STRING *reasons;
GENERAL_NAMES *CRLissuer;
} DIST_POINT;

typedef STACK_OF(DIST_POINT) CRL_DIST_POINTS;

DECLARE_STACK_OF(DIST_POINT)
DECLARE_ASN1_SET_OF(DIST_POINT)

typedef struct AUTHORITY_KEYID_st {
ASN1_OCTET_STRING *keyid;
GENERAL_NAMES *issuer;
ASN1_INTEGER *serial;
} AUTHORITY_KEYID;

/* Strong extranet structures */

typedef struct SXNET_ID_st {
	ASN1_INTEGER *zone;
	ASN1_OCTET_STRING *user;
} SXNETID;

DECLARE_STACK_OF(SXNETID)
DECLARE_ASN1_SET_OF(SXNETID)

typedef struct SXNET_st {
	ASN1_INTEGER *version;
	STACK_OF(SXNETID) *ids;
} SXNET;

typedef struct NOTICEREF_st {
	ASN1_STRING *organization;
	STACK_OF(ASN1_INTEGER) *noticenos;
} NOTICEREF;

typedef struct USERNOTICE_st {
	NOTICEREF *noticeref;
	ASN1_STRING *exptext;
} USERNOTICE;

typedef struct POLICYQUALINFO_st {
	ASN1_OBJECT *pqualid;
	union {
		ASN1_IA5STRING *cpsuri;
		USERNOTICE *usernotice;
		ASN1_TYPE *other;
	} d;
} POLICYQUALINFO;

DECLARE_STACK_OF(POLICYQUALINFO)
DECLARE_ASN1_SET_OF(POLICYQUALINFO)

typedef struct POLICYINFO_st {
	ASN1_OBJECT *policyid;
	STACK_OF(POLICYQUALINFO) *qualifiers;
} POLICYINFO;

typedef STACK_OF(POLICYINFO) CERTIFICATEPOLICIES;

DECLARE_STACK_OF(POLICYINFO)
DECLARE_ASN1_SET_OF(POLICYINFO)

typedef struct POLICY_MAPPING_st {
	ASN1_OBJECT *issuerDomainPolicy;
	ASN1_OBJECT *subjectDomainPolicy;
} POLICY_MAPPING;

DECLARE_STACK_OF(POLICY_MAPPING)

typedef STACK_OF(POLICY_MAPPING) POLICY_MAPPINGS;

typedef struct GENERAL_SUBTREE_st {
	GENERAL_NAME *base;
	ASN1_INTEGER *minimum;
	ASN1_INTEGER *maximum;
} GENERAL_SUBTREE;

DECLARE_STACK_OF(GENERAL_SUBTREE)

typedef struct NAME_CONSTRAINTS_st {
	STACK_OF(GENERAL_SUBTREE) *permittedSubtrees;
	STACK_OF(GENERAL_SUBTREE) *excludedSubtrees;
} NAME_CONSTRAINTS;

typedef struct POLICY_CONSTRAINTS_st {
	ASN1_INTEGER *requireExplicitPolicy;
	ASN1_INTEGER *inhibitPolicyMapping;
} POLICY_CONSTRAINTS;

/* Proxy certificate structures, see RFC 3820 */
typedef struct PROXY_POLICY_st
	{
	ASN1_OBJECT *policyLanguage;
	ASN1_OCTET_STRING *policy;
	} PROXY_POLICY;

typedef struct PROXY_CERT_INFO_EXTENSION_st
	{
	ASN1_INTEGER *pcPathLengthConstraint;
	PROXY_POLICY *proxyPolicy;
	} PROXY_CERT_INFO_EXTENSION;

DECLARE_ASN1_FUNCTIONS(PROXY_POLICY)
DECLARE_ASN1_FUNCTIONS(PROXY_CERT_INFO_EXTENSION)


#define X509V3_conf_err(val) ERR_add_error_data(6, "section:", val->section, \
",name:", val->name, ",value:", val->value);

#define X509V3_set_ctx_test(ctx) \
			X509V3_set_ctx(ctx, NULL, NULL, NULL, NULL, CTX_TEST)
#define X509V3_set_ctx_nodb(ctx) (ctx)->db = NULL;

#define EXT_BITSTRING(nid, table) { nid, 0, ASN1_ITEM_ref(ASN1_BIT_STRING), \
			0,0,0,0, \
			0,0, \
			(X509V3_EXT_I2V)i2v_ASN1_BIT_STRING, \
			(X509V3_EXT_V2I)v2i_ASN1_BIT_STRING, \
			NULL, NULL, \
			table}

#define EXT_IA5STRING(nid) { nid, 0, ASN1_ITEM_ref(ASN1_IA5STRING), \
			0,0,0,0, \
			(X509V3_EXT_I2S)i2s_ASN1_IA5STRING, \
			(X509V3_EXT_S2I)s2i_ASN1_IA5STRING, \
			0,0,0,0, \
			NULL}

#define EXT_END { -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}


/* X509_PURPOSE stuff */

#define EXFLAG_BCONS		0x1
#define EXFLAG_KUSAGE		0x2
#define EXFLAG_XKUSAGE		0x4
#define EXFLAG_NSCERT		0x8

#define EXFLAG_CA		0x10
/* Really self issued not necessarily self signed */
#define EXFLAG_SI		0x20
#define EXFLAG_SS		0x20
#define EXFLAG_V1		0x40
#define EXFLAG_INVALID		0x80
#define EXFLAG_SET		0x100
#define EXFLAG_CRITICAL		0x200
#define EXFLAG_PROXY		0x400

#define EXFLAG_INVALID_POLICY	0x800

#define KU_DIGITAL_SIGNATURE	0x0080
#define KU_NON_REPUDIATION	0x0040
#define KU_KEY_ENCIPHERMENT	0x0020
#define KU_DATA_ENCIPHERMENT	0x0010
#define KU_KEY_AGREEMENT	0x0008
#define KU_KEY_CERT_SIGN	0x0004
#define KU_CRL_SIGN		0x0002
#define KU_ENCIPHER_ONLY	0x0001
#define KU_DECIPHER_ONLY	0x8000

#define NS_SSL_CLIENT		0x80
#define NS_SSL_SERVER		0x40
#define NS_SMIME		0x20
#define NS_OBJSIGN		0x10
#define NS_SSL_CA		0x04
#define NS_SMIME_CA		0x02
#define NS_OBJSIGN_CA		0x01
#define NS_ANY_CA		(NS_SSL_CA|NS_SMIME_CA|NS_OBJSIGN_CA)

#define XKU_SSL_SERVER		0x1	
#define XKU_SSL_CLIENT		0x2
#define XKU_SMIME		0x4
#define XKU_CODE_SIGN		0x8
#define XKU_SGC			0x10
#define XKU_OCSP_SIGN		0x20
#define XKU_TIMESTAMP		0x40
#define XKU_DVCS		0x80

#define X509_PURPOSE_DYNAMIC	0x1
#define X509_PURPOSE_DYNAMIC_NAME	0x2

typedef struct x509_purpose_st {
	int purpose;
	int trust;		/* Default trust ID */
	int flags;
	int (*check_purpose)(const struct x509_purpose_st *,
				const X509 *, int);
	char *name;
	char *sname;
	void *usr_data;
} X509_PURPOSE;

#define X509_PURPOSE_SSL_CLIENT		1
#define X509_PURPOSE_SSL_SERVER		2
#define X509_PURPOSE_NS_SSL_SERVER	3
#define X509_PURPOSE_SMIME_SIGN		4
#define X509_PURPOSE_SMIME_ENCRYPT	5
#define X509_PURPOSE_CRL_SIGN		6
#define X509_PURPOSE_ANY		7
#define X509_PURPOSE_OCSP_HELPER	8

#define X509_PURPOSE_MIN		1
#define X509_PURPOSE_MAX		8

/* Flags for X509V3_EXT_print() */

#define X509V3_EXT_UNKNOWN_MASK		(0xfL << 16)
/* Return error for unknown extensions */
#define X509V3_EXT_DEFAULT		0
/* Print error for unknown extensions */
#define X509V3_EXT_ERROR_UNKNOWN	(1L << 16)
/* ASN1 parse unknown extensions */
#define X509V3_EXT_PARSE_UNKNOWN	(2L << 16)
/* BIO_dump unknown extensions */
#define X509V3_EXT_DUMP_UNKNOWN		(3L << 16)

/* Flags for X509V3_add1_i2d */

#define X509V3_ADD_OP_MASK		0xfL
#define X509V3_ADD_DEFAULT		0L
#define X509V3_ADD_APPEND		1L
#define X509V3_ADD_REPLACE		2L
#define X509V3_ADD_REPLACE_EXISTING	3L
#define X509V3_ADD_KEEP_EXISTING	4L
#define X509V3_ADD_DELETE		5L
#define X509V3_ADD_SILENT		0x10

DECLARE_STACK_OF(X509_PURPOSE)

DECLARE_ASN1_FUNCTIONS(BASIC_CONSTRAINTS)

DECLARE_ASN1_FUNCTIONS(SXNET)
DECLARE_ASN1_FUNCTIONS(SXNETID)

int SXNET_add_id_asc(SXNET **psx, char *zone, char *user, int userlen); 
int SXNET_add_id_ulong(SXNET **psx, unsigned long lzone, char *user, int userlen); 
int SXNET_add_id_INTEGER(SXNET **psx, ASN1_INTEGER *izone, char *user, int userlen); 

ASN1_OCTET_STRING *SXNET_get_id_asc(SXNET *sx, char *zone);
ASN1_OCTET_STRING *SXNET_get_id_ulong(SXNET *sx, unsigned long lzone);
ASN1_OCTET_STRING *SXNET_get_id_INTEGER(SXNET *sx, ASN1_INTEGER *zone);

DECLARE_ASN1_FUNCTIONS(AUTHORITY_KEYID)

DECLARE_ASN1_FUNCTIONS(PKEY_USAGE_PERIOD)

DECLARE_ASN1_FUNCTIONS(GENERAL_NAME)


ASN1_BIT_STRING *v2i_ASN1_BIT_STRING(X509V3_EXT_METHOD *method,
				X509V3_CTX *ctx, STACK_OF(CONF_VALUE) *nval);
STACK_OF(CONF_VALUE) *i2v_ASN1_BIT_STRING(X509V3_EXT_METHOD *method,
				ASN1_BIT_STRING *bits,
				STACK_OF(CONF_VALUE) *extlist);

STACK_OF(CONF_VALUE) *i2v_GENERAL_NAME(X509V3_EXT_METHOD *method, GENERAL_NAME *gen, STACK_OF(CONF_VALUE) *ret);
int GENERAL_NAME_print(BIO *out, GENERAL_NAME *gen);

DECLARE_ASN1_FUNCTIONS(GENERAL_NAMES)

STACK_OF(CONF_VALUE) *i2v_GENERAL_NAMES(X509V3_EXT_METHOD *method,
		GENERAL_NAMES *gen, STACK_OF(CONF_VALUE) *extlist);
GENERAL_NAMES *v2i_GENERAL_NAMES(X509V3_EXT_METHOD *method,
				X509V3_CTX *ctx, STACK_OF(CONF_VALUE) *nval);

DECLARE_ASN1_FUNCTIONS(OTHERNAME)
DECLARE_ASN1_FUNCTIONS(EDIPARTYNAME)

char *i2s_ASN1_OCTET_STRING(X509V3_EXT_METHOD *method, ASN1_OCTET_STRING *ia5);
ASN1_OCTET_STRING *s2i_ASN1_OCTET_STRING(X509V3_EXT_METHOD *method, X509V3_CTX *ctx, char *str);

DECLARE_ASN1_FUNCTIONS(EXTENDED_KEY_USAGE)
int i2a_ACCESS_DESCRIPTION(BIO *bp, ACCESS_DESCRIPTION* a);

DECLARE_ASN1_FUNCTIONS(CERTIFICATEPOLICIES)
DECLARE_ASN1_FUNCTIONS(POLICYINFO)
DECLARE_ASN1_FUNCTIONS(POLICYQUALINFO)
DECLARE_ASN1_FUNCTIONS(USERNOTICE)
DECLARE_ASN1_FUNCTIONS(NOTICEREF)

DECLARE_ASN1_FUNCTIONS(CRL_DIST_POINTS)
DECLARE_ASN1_FUNCTIONS(DIST_POINT)
DECLARE_ASN1_FUNCTIONS(DIST_POINT_NAME)

DECLARE_ASN1_FUNCTIONS(ACCESS_DESCRIPTION)
DECLARE_ASN1_FUNCTIONS(AUTHORITY_INFO_ACCESS)

DECLARE_ASN1_ITEM(POLICY_MAPPING)
DECLARE_ASN1_ALLOC_FUNCTIONS(POLICY_MAPPING)
DECLARE_ASN1_ITEM(POLICY_MAPPINGS)

DECLARE_ASN1_ITEM(GENERAL_SUBTREE)
DECLARE_ASN1_ALLOC_FUNCTIONS(GENERAL_SUBTREE)

DECLARE_ASN1_ITEM(NAME_CONSTRAINTS)
DECLARE_ASN1_ALLOC_FUNCTIONS(NAME_CONSTRAINTS)

DECLARE_ASN1_ALLOC_FUNCTIONS(POLICY_CONSTRAINTS)
DECLARE_ASN1_ITEM(POLICY_CONSTRAINTS)

#ifdef HEADER_CONF_H
GENERAL_NAME *v2i_GENERAL_NAME(X509V3_EXT_METHOD *method, X509V3_CTX *ctx,
							CONF_VALUE *cnf);
GENERAL_NAME *v2i_GENERAL_NAME_ex(GENERAL_NAME *out, X509V3_EXT_METHOD *method,
				X509V3_CTX *ctx, CONF_VALUE *cnf, int is_nc);
void X509V3_conf_free(CONF_VALUE *val);

X509_EXTENSION *X509V3_EXT_nconf_nid(CONF *conf, X509V3_CTX *ctx, int ext_nid, char *value);
X509_EXTENSION *X509V3_EXT_nconf(CONF *conf, X509V3_CTX *ctx, char *name, char *value);
int X509V3_EXT_add_nconf_sk(CONF *conf, X509V3_CTX *ctx, char *section, STACK_OF(X509_EXTENSION) **sk);
int X509V3_EXT_add_nconf(CONF *conf, X509V3_CTX *ctx, char *section, X509 *cert);
int X509V3_EXT_REQ_add_nconf(CONF *conf, X509V3_CTX *ctx, char *section, X509_REQ *req);
int X509V3_EXT_CRL_add_nconf(CONF *conf, X509V3_CTX *ctx, char *section, X509_CRL *crl);

X509_EXTENSION *X509V3_EXT_conf_nid(LHASH *conf, X509V3_CTX *ctx, int ext_nid, char *value);
X509_EXTENSION *X509V3_EXT_conf(LHASH *conf, X509V3_CTX *ctx, char *name, char *value);
int X509V3_EXT_add_conf(LHASH *conf, X509V3_CTX *ctx, char *section, X509 *cert);
int X509V3_EXT_REQ_add_conf(LHASH *conf, X509V3_CTX *ctx, char *section, X509_REQ *req);
int X509V3_EXT_CRL_add_conf(LHASH *conf, X509V3_CTX *ctx, char *section, X509_CRL *crl);

int X509V3_add_value_bool_nf(char *name, int asn1_bool,
						STACK_OF(CONF_VALUE) **extlist);
int X509V3_get_value_bool(CONF_VALUE *value, int *asn1_bool);
int X509V3_get_value_int(CONF_VALUE *value, ASN1_INTEGER **aint);
void X509V3_set_nconf(X509V3_CTX *ctx, CONF *conf);
void X509V3_set_conf_lhash(X509V3_CTX *ctx, LHASH *lhash);
#endif

char * X509V3_get_string(X509V3_CTX *ctx, char *name, char *section);
STACK_OF(CONF_VALUE) * X509V3_get_section(X509V3_CTX *ctx, char *section);
void X509V3_string_free(X509V3_CTX *ctx, char *str);
void X509V3_section_free( X509V3_CTX *ctx, STACK_OF(CONF_VALUE) *section);
void X509V3_set_ctx(X509V3_CTX *ctx, X509 *issuer, X509 *subject,
				 X509_REQ *req, X509_CRL *crl, int flags);

int X509V3_add_value(const char *name, const char *value,
						STACK_OF(CONF_VALUE) **extlist);
int X509V3_add_value_uchar(const char *name, const unsigned char *value,
						STACK_OF(CONF_VALUE) **extlist);
int X509V3_add_value_bool(const char *name, int asn1_bool,
						STACK_OF(CONF_VALUE) **extlist);
int X509V3_add_value_int(const char *name, ASN1_INTEGER *aint,
						STACK_OF(CONF_VALUE) **extlist);
char * i2s_ASN1_INTEGER(X509V3_EXT_METHOD *meth, ASN1_INTEGER *aint);
ASN1_INTEGER * s2i_ASN1_INTEGER(X509V3_EXT_METHOD *meth, char *value);
char * i2s_ASN1_ENUMERATED(X509V3_EXT_METHOD *meth, ASN1_ENUMERATED *aint);
char * i2s_ASN1_ENUMERATED_TABLE(X509V3_EXT_METHOD *meth, ASN1_ENUMERATED *aint);
int X509V3_EXT_add(X509V3_EXT_METHOD *ext);
int X509V3_EXT_add_list(X509V3_EXT_METHOD *extlist);
int X509V3_EXT_add_alias(int nid_to, int nid_from);
void X509V3_EXT_cleanup(void);

X509V3_EXT_METHOD *X509V3_EXT_get(X509_EXTENSION *ext);
X509V3_EXT_METHOD *X509V3_EXT_get_nid(int nid);
int X509V3_add_standard_extensions(void);
STACK_OF(CONF_VALUE) *X509V3_parse_list(const char *line);
void *X509V3_EXT_d2i(X509_EXTENSION *ext);
void *X509V3_get_d2i(STACK_OF(X509_EXTENSION) *x, int nid, int *crit, int *idx);


X509_EXTENSION *X509V3_EXT_i2d(int ext_nid, int crit, void *ext_struc);
int X509V3_add1_i2d(STACK_OF(X509_EXTENSION) **x, int nid, void *value, int crit, unsigned long flags);

char *hex_to_string(unsigned char *buffer, long len);
unsigned char *string_to_hex(char *str, long *len);
int name_cmp(const char *name, const char *cmp);

void X509V3_EXT_val_prn(BIO *out, STACK_OF(CONF_VALUE) *val, int indent,
								 int ml);
int X509V3_EXT_print(BIO *out, X509_EXTENSION *ext, unsigned long flag, int indent);
int X509V3_EXT_print_fp(FILE *out, X509_EXTENSION *ext, int flag, int indent);

int X509V3_extensions_print(BIO *out, char *title, STACK_OF(X509_EXTENSION) *exts, unsigned long flag, int indent);

int X509_check_ca(X509 *x);
int X509_check_purpose(X509 *x, int id, int ca);
int X509_supported_extension(X509_EXTENSION *ex);
int X509_PURPOSE_set(int *p, int purpose);
int X509_check_issued(X509 *issuer, X509 *subject);
int X509_PURPOSE_get_count(void);
X509_PURPOSE * X509_PURPOSE_get0(int idx);
int X509_PURPOSE_get_by_sname(char *sname);
int X509_PURPOSE_get_by_id(int id);
int X509_PURPOSE_add(int id, int trust, int flags,
			int (*ck)(const X509_PURPOSE *, const X509 *, int),
				char *name, char *sname, void *arg);
char *X509_PURPOSE_get0_name(X509_PURPOSE *xp);
char *X509_PURPOSE_get0_sname(X509_PURPOSE *xp);
int X509_PURPOSE_get_trust(X509_PURPOSE *xp);
void X509_PURPOSE_cleanup(void);
int X509_PURPOSE_get_id(X509_PURPOSE *);

STACK *X509_get1_email(X509 *x);
STACK *X509_REQ_get1_email(X509_REQ *x);
void X509_email_free(STACK *sk);
STACK *X509_get1_ocsp(X509 *x);

ASN1_OCTET_STRING *a2i_IPADDRESS(const char *ipasc);
ASN1_OCTET_STRING *a2i_IPADDRESS_NC(const char *ipasc);
int a2i_ipadd(unsigned char *ipout, const char *ipasc);
int X509V3_NAME_from_section(X509_NAME *nm, STACK_OF(CONF_VALUE)*dn_sk,
						unsigned long chtype);

void X509_POLICY_NODE_print(BIO *out, X509_POLICY_NODE *node, int indent);

#ifndef OPENSSL_NO_RFC3779

typedef struct ASRange_st {
  ASN1_INTEGER *min, *max;
} ASRange;

#define	ASIdOrRange_id		0
#define	ASIdOrRange_range	1

typedef struct ASIdOrRange_st {
  int type;
  union {
    ASN1_INTEGER *id;
    ASRange      *range;
  } u;
} ASIdOrRange;

typedef STACK_OF(ASIdOrRange) ASIdOrRanges;
DECLARE_STACK_OF(ASIdOrRange)

#define	ASIdentifierChoice_inherit		0
#define	ASIdentifierChoice_asIdsOrRanges	1

typedef struct ASIdentifierChoice_st {
  int type;
  union {
    ASN1_NULL    *inherit;
    ASIdOrRanges *asIdsOrRanges;
  } u;
} ASIdentifierChoice;

typedef struct ASIdentifiers_st {
  ASIdentifierChoice *asnum, *rdi;
} ASIdentifiers;

DECLARE_ASN1_FUNCTIONS(ASRange)
DECLARE_ASN1_FUNCTIONS(ASIdOrRange)
DECLARE_ASN1_FUNCTIONS(ASIdentifierChoice)
DECLARE_ASN1_FUNCTIONS(ASIdentifiers)


typedef struct IPAddressRange_st {
  ASN1_BIT_STRING	*min, *max;
} IPAddressRange;

#define	IPAddressOrRange_addressPrefix	0
#define	IPAddressOrRange_addressRange	1

typedef struct IPAddressOrRange_st {
  int type;
  union {
    ASN1_BIT_STRING	*addressPrefix;
    IPAddressRange	*addressRange;
  } u;
} IPAddressOrRange;

typedef STACK_OF(IPAddressOrRange) IPAddressOrRanges;
DECLARE_STACK_OF(IPAddressOrRange)

#define	IPAddressChoice_inherit			0
#define	IPAddressChoice_addressesOrRanges	1

typedef struct IPAddressChoice_st {
  int type;
  union {
    ASN1_NULL		*inherit;
    IPAddressOrRanges	*addressesOrRanges;
  } u;
} IPAddressChoice;

typedef struct IPAddressFamily_st {
  ASN1_OCTET_STRING	*addressFamily;
  IPAddressChoice	*ipAddressChoice;
} IPAddressFamily;

typedef STACK_OF(IPAddressFamily) IPAddrBlocks;
DECLARE_STACK_OF(IPAddressFamily)

DECLARE_ASN1_FUNCTIONS(IPAddressRange)
DECLARE_ASN1_FUNCTIONS(IPAddressOrRange)
DECLARE_ASN1_FUNCTIONS(IPAddressChoice)
DECLARE_ASN1_FUNCTIONS(IPAddressFamily)

/*
 * API tag for elements of the ASIdentifer SEQUENCE.
 */
#define	V3_ASID_ASNUM	0
#define	V3_ASID_RDI	1

/*
 * AFI values, assigned by IANA.  It'd be nice to make the AFI
 * handling code totally generic, but there are too many little things
 * that would need to be defined for other address families for it to
 * be worth the trouble.
 */
#define	IANA_AFI_IPV4	1
#define	IANA_AFI_IPV6	2

/*
 * Utilities to construct and extract values from RFC3779 extensions,
 * since some of the encodings (particularly for IP address prefixes
 * and ranges) are a bit tedious to work with directly.
 */
int v3_asid_add_inherit(ASIdentifiers *asid, int which);
int v3_asid_add_id_or_range(ASIdentifiers *asid, int which,
			    ASN1_INTEGER *min, ASN1_INTEGER *max);
int v3_addr_add_inherit(IPAddrBlocks *addr,
			const unsigned afi, const unsigned *safi);
int v3_addr_add_prefix(IPAddrBlocks *addr,
		       const unsigned afi, const unsigned *safi,
		       unsigned char *a, const int prefixlen);
int v3_addr_add_range(IPAddrBlocks *addr,
		      const unsigned afi, const unsigned *safi,
		      unsigned char *min, unsigned char *max);
unsigned v3_addr_get_afi(const IPAddressFamily *f);
int v3_addr_get_range(IPAddressOrRange *aor, const unsigned afi,
		      unsigned char *min, unsigned char *max,
		      const int length);

/*
 * Canonical forms.
 */
int v3_asid_is_canonical(ASIdentifiers *asid);
int v3_addr_is_canonical(IPAddrBlocks *addr);
int v3_asid_canonize(ASIdentifiers *asid);
int v3_addr_canonize(IPAddrBlocks *addr);

/*
 * Tests for inheritance and containment.
 */
int v3_asid_inherits(ASIdentifiers *asid);
int v3_addr_inherits(IPAddrBlocks *addr);
int v3_asid_subset(ASIdentifiers *a, ASIdentifiers *b);
int v3_addr_subset(IPAddrBlocks *a, IPAddrBlocks *b);

/*
 * Check whether RFC 3779 extensions nest properly in chains.
 */
int v3_asid_validate_path(X509_STORE_CTX *);
int v3_addr_validate_path(X509_STORE_CTX *);
int v3_asid_validate_resource_set(STACK_OF(X509) *chain,
				  ASIdentifiers *ext,
				  int allow_inheritance);
int v3_addr_validate_resource_set(STACK_OF(X509) *chain,
				  IPAddrBlocks *ext,
				  int allow_inheritance);

#endif /* OPENSSL_NO_RFC3779 */

/* BEGIN ERROR CODES */
/* The following lines are auto generated by the script mkerr.pl. Any changes
 * made after this point may be overwritten when the script is next run.
 */
void ERR_load_X509V3_strings(void);

/* Error codes for the X509V3 functions. */

/* Function codes. */
#define X509V3_F_ASIDENTIFIERCHOICE_CANONIZE		 156
#define X509V3_F_ASIDENTIFIERCHOICE_IS_CANONICAL	 157
#define X509V3_F_COPY_EMAIL				 122
#define X509V3_F_COPY_ISSUER				 123
#define X509V3_F_DO_DIRNAME				 144
#define X509V3_F_DO_EXT_CONF				 124
#define X509V3_F_DO_EXT_I2D				 135
#define X509V3_F_DO_EXT_NCONF				 151
#define X509V3_F_DO_I2V_NAME_CONSTRAINTS		 148
#define X509V3_F_HEX_TO_STRING				 111
#define X509V3_F_I2S_ASN1_ENUMERATED			 121
#define X509V3_F_I2S_ASN1_IA5STRING			 149
#define X509V3_F_I2S_ASN1_INTEGER			 120
#define X509V3_F_I2V_AUTHORITY_INFO_ACCESS		 138
#define X509V3_F_NOTICE_SECTION				 132
#define X509V3_F_NREF_NOS				 133
#define X509V3_F_POLICY_SECTION				 131
#define X509V3_F_PROCESS_PCI_VALUE			 150
#define X509V3_F_R2I_CERTPOL				 130
#define X509V3_F_R2I_PCI				 155
#define X509V3_F_S2I_ASN1_IA5STRING			 100
#define X509V3_F_S2I_ASN1_INTEGER			 108
#define X509V3_F_S2I_ASN1_OCTET_STRING			 112
#define X509V3_F_S2I_ASN1_SKEY_ID			 114
#define X509V3_F_S2I_SKEY_ID				 115
#define X509V3_F_STRING_TO_HEX				 113
#define X509V3_F_SXNET_ADD_ID_ASC			 125
#define X509V3_F_SXNET_ADD_ID_INTEGER			 126
#define X509V3_F_SXNET_ADD_ID_ULONG			 127
#define X509V3_F_SXNET_GET_ID_ASC			 128
#define X509V3_F_SXNET_GET_ID_ULONG			 129
#define X509V3_F_V2I_ASIDENTIFIERS			 158
#define X509V3_F_V2I_ASN1_BIT_STRING			 101
#define X509V3_F_V2I_AUTHORITY_INFO_ACCESS		 139
#define X509V3_F_V2I_AUTHORITY_KEYID			 119
#define X509V3_F_V2I_BASIC_CONSTRAINTS			 102
#define X509V3_F_V2I_CRLD				 134
#define X509V3_F_V2I_EXTENDED_KEY_USAGE			 103
#define X509V3_F_V2I_GENERAL_NAMES			 118
#define X509V3_F_V2I_GENERAL_NAME_EX			 117
#define X509V3_F_V2I_IPADDRBLOCKS			 159
#define X509V3_F_V2I_ISSUER_ALT				 153
#define X509V3_F_V2I_NAME_CONSTRAINTS			 147
#define X509V3_F_V2I_POLICY_CONSTRAINTS			 146
#define X509V3_F_V2I_POLICY_MAPPINGS			 145
#define X509V3_F_V2I_SUBJECT_ALT			 154
#define X509V3_F_V3_ADDR_VALIDATE_PATH_INTERNAL		 160
#define X509V3_F_V3_GENERIC_EXTENSION			 116
#define X509V3_F_X509V3_ADD1_I2D			 140
#define X509V3_F_X509V3_ADD_VALUE			 105
#define X509V3_F_X509V3_EXT_ADD				 104
#define X509V3_F_X509V3_EXT_ADD_ALIAS			 106
#define X509V3_F_X509V3_EXT_CONF			 107
#define X509V3_F_X509V3_EXT_I2D				 136
#define X509V3_F_X509V3_EXT_NCONF			 152
#define X509V3_F_X509V3_GET_SECTION			 142
#define X509V3_F_X509V3_GET_STRING			 143
#define X509V3_F_X509V3_GET_VALUE_BOOL			 110
#define X509V3_F_X509V3_PARSE_LIST			 109
#define X509V3_F_X509_PURPOSE_ADD			 137
#define X509V3_F_X509_PURPOSE_SET			 141

/* Reason codes. */
#define X509V3_R_BAD_IP_ADDRESS				 118
#define X509V3_R_BAD_OBJECT				 119
#define X509V3_R_BN_DEC2BN_ERROR			 100
#define X509V3_R_BN_TO_ASN1_INTEGER_ERROR		 101
#define X509V3_R_DIRNAME_ERROR				 149
#define X509V3_R_DUPLICATE_ZONE_ID			 133
#define X509V3_R_ERROR_CONVERTING_ZONE			 131
#define X509V3_R_ERROR_CREATING_EXTENSION		 144
#define X509V3_R_ERROR_IN_EXTENSION			 128
#define X509V3_R_EXPECTED_A_SECTION_NAME		 137
#define X509V3_R_EXTENSION_EXISTS			 145
#define X509V3_R_EXTENSION_NAME_ERROR			 115
#define X509V3_R_EXTENSION_NOT_FOUND			 102
#define X509V3_R_EXTENSION_SETTING_NOT_SUPPORTED	 103
#define X509V3_R_EXTENSION_VALUE_ERROR			 116
#define X509V3_R_ILLEGAL_EMPTY_EXTENSION		 151
#define X509V3_R_ILLEGAL_HEX_DIGIT			 113
#define X509V3_R_INCORRECT_POLICY_SYNTAX_TAG		 152
#define X509V3_R_INVALID_ASNUMBER			 160
#define X509V3_R_INVALID_ASRANGE			 161
#define X509V3_R_INVALID_BOOLEAN_STRING			 104
#define X509V3_R_INVALID_EXTENSION_STRING		 105
#define X509V3_R_INVALID_INHERITANCE			 162
#define X509V3_R_INVALID_IPADDRESS			 163
#define X509V3_R_INVALID_NAME				 106
#define X509V3_R_INVALID_NULL_ARGUMENT			 107
#define X509V3_R_INVALID_NULL_NAME			 108
#define X509V3_R_INVALID_NULL_VALUE			 109
#define X509V3_R_INVALID_NUMBER				 140
#define X509V3_R_INVALID_NUMBERS			 141
#define X509V3_R_INVALID_OBJECT_IDENTIFIER		 110
#define X509V3_R_INVALID_OPTION				 138
#define X509V3_R_INVALID_POLICY_IDENTIFIER		 134
#define X509V3_R_INVALID_PROXY_POLICY_SETTING		 153
#define X509V3_R_INVALID_PURPOSE			 146
#define X509V3_R_INVALID_SAFI				 164
#define X509V3_R_INVALID_SECTION			 135
#define X509V3_R_INVALID_SYNTAX				 143
#define X509V3_R_ISSUER_DECODE_ERROR			 126
#define X509V3_R_MISSING_VALUE				 124
#define X509V3_R_NEED_ORGANIZATION_AND_NUMBERS		 142
#define X509V3_R_NO_CONFIG_DATABASE			 136
#define X509V3_R_NO_ISSUER_CERTIFICATE			 121
#define X509V3_R_NO_ISSUER_DETAILS			 127
#define X509V3_R_NO_POLICY_IDENTIFIER			 139
#define X509V3_R_NO_PROXY_CERT_POLICY_LANGUAGE_DEFINED	 154
#define X509V3_R_NO_PUBLIC_KEY				 114
#define X509V3_R_NO_SUBJECT_DETAILS			 125
#define X509V3_R_ODD_NUMBER_OF_DIGITS			 112
#define X509V3_R_OPERATION_NOT_DEFINED			 148
#define X509V3_R_OTHERNAME_ERROR			 147
#define X509V3_R_POLICY_LANGUAGE_ALREADTY_DEFINED	 155
#define X509V3_R_POLICY_PATH_LENGTH			 156
#define X509V3_R_POLICY_PATH_LENGTH_ALREADTY_DEFINED	 157
#define X509V3_R_POLICY_SYNTAX_NOT_CURRENTLY_SUPPORTED	 158
#define X509V3_R_POLICY_WHEN_PROXY_LANGUAGE_REQUIRES_NO_POLICY 159
#define X509V3_R_SECTION_NOT_FOUND			 150
#define X509V3_R_UNABLE_TO_GET_ISSUER_DETAILS		 122
#define X509V3_R_UNABLE_TO_GET_ISSUER_KEYID		 123
#define X509V3_R_UNKNOWN_BIT_STRING_ARGUMENT		 111
#define X509V3_R_UNKNOWN_EXTENSION			 129
#define X509V3_R_UNKNOWN_EXTENSION_NAME			 130
#define X509V3_R_UNKNOWN_OPTION				 120
#define X509V3_R_UNSUPPORTED_OPTION			 117
#define X509V3_R_USER_TOO_LONG				 132

#ifdef  __cplusplus
}
#endif
#endif
