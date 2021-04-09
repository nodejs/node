/*
 *  Author: Viktor Dukhovni
 *  License: THIS CODE IS IN THE PUBLIC DOMAIN.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef _MSC_VER
# define strcasecmp _stricmp
#endif

#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/safestack.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/bn.h>

#if OPENSSL_VERSION_NUMBER < 0x1000000fL
#error "OpenSSL 1.0.0 or higher required"
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define X509_up_ref(x) CRYPTO_add(&((x)->references), 1, CRYPTO_LOCK_X509)
#define X509_STORE_CTX_get0_cert(ctx) ((ctx)->cert)
#define X509_STORE_CTX_get0_untrusted(ctx) ((ctx)->untrusted)
#define X509_STORE_CTX_get0_chain(ctx) ((ctx)->chain)
#define X509_STORE_CTX_get_verify(ctx) ((ctx)->verify)
#define X509_STORE_CTX_get_verify_cb(ctx) ((ctx)->verify_cb)
#define X509_STORE_CTX_set0_verified_chain(ctx, xs) ((ctx)->chain = xs)
#define X509_STORE_CTX_set0_untrusted X509_STORE_CTX_set_chain
#define X509_STORE_CTX_set0_trusted_stack X509_STORE_CTX_trusted_stack
#define X509_STORE_CTX_set_verify(ctx, v) ((ctx)->verify = v)
#define X509_STORE_CTX_set_error_depth(ctx, d) ((ctx)->error_depth = d)
#define X509_STORE_CTX_set_current_cert(ctx, x) ((ctx)->current_cert = x)
#define ASN1_STRING_get0_data ASN1_STRING_data
#define X509_getm_notBefore X509_get_notBefore
#define X509_getm_notAfter X509_get_notAfter
#define CRYPTO_ONCE_STATIC_INIT 0
#define CRYPTO_THREAD_run_once run_once
typedef int CRYPTO_ONCE;
#endif

#if OPENSSL_VERSION_NUMBER < 0x10002000L
/* 
 * Disabled the #warning preprocessor directive, because warnings are
 * considered failures in the getdns unit tests.
 * Also #warning directive is not portable.
 *
 * #warning "OpenSSL 1.0.1 and earlier are EOL, upgrade to 1.0.2 or later"
 */
#define SSL_is_server(s) ((s)->server)
#define SSL_get0_param(s) ((s)->param)
#endif

#include "danessl.h"

#define DANESSL_F_ADD_SKID		100
#define DANESSL_F_ADD_TLSA		101
#define DANESSL_F_CHECK_END_ENTITY	102
#define DANESSL_F_CTX_INIT		103
#define DANESSL_F_DANESSL_VERIFY_CHAIN	113
#define DANESSL_F_GROW_CHAIN		104
#define DANESSL_F_INIT			105
#define DANESSL_F_LIBRARY_INIT		106
#define DANESSL_F_LIST_ALLOC		107
#define DANESSL_F_MATCH			108
#define DANESSL_F_PUSH_EXT		109
#define DANESSL_F_SET_TRUST_ANCHOR	110
#define DANESSL_F_VERIFY_CERT		111
#define DANESSL_F_WRAP_CERT		112

#define DANESSL_R_BAD_CERT		100
#define DANESSL_R_BAD_CERT_PKEY		101
#define DANESSL_R_BAD_DATA_LENGTH	102
#define DANESSL_R_BAD_DIGEST		103
#define DANESSL_R_BAD_NULL_DATA		104
#define DANESSL_R_BAD_PKEY		105
#define DANESSL_R_BAD_SELECTOR		106
#define DANESSL_R_BAD_USAGE		107
#define DANESSL_R_INIT			108
#define DANESSL_R_LIBRARY_INIT		109
#define DANESSL_R_NOSIGN_KEY		110
#define DANESSL_R_SCTX_INIT		111
#define DANESSL_R_SUPPORT		112

#ifndef OPENSSL_NO_ERR
#define	DANESSL_F_PLACEHOLDER		0		/* FIRST! Value TBD */
static ERR_STRING_DATA dane_str_functs[] = {
    {DANESSL_F_PLACEHOLDER,		"DANE library"},	/* FIRST!!! */
    {DANESSL_F_ADD_SKID,		"add_skid"},
    {DANESSL_F_ADD_TLSA,		"DANESSL_add_tlsa"},
    {DANESSL_F_CHECK_END_ENTITY,	"check_end_entity"},
    {DANESSL_F_CTX_INIT,		"DANESSL_CTX_init"},
    {DANESSL_F_GROW_CHAIN,		"grow_chain"},
    {DANESSL_F_INIT,			"DANESSL_init"},
    {DANESSL_F_LIBRARY_INIT,		"DANESSL_library_init"},
    {DANESSL_F_LIST_ALLOC,		"list_alloc"},
    {DANESSL_F_MATCH,			"match"},
    {DANESSL_F_PUSH_EXT,		"push_ext"},
    {DANESSL_F_SET_TRUST_ANCHOR,	"set_trust_anchor"},
    {DANESSL_F_VERIFY_CERT,		"verify_cert"},
    {DANESSL_F_WRAP_CERT,		"wrap_cert"},
    {0,					NULL}
};
static ERR_STRING_DATA dane_str_reasons[] = {
    {DANESSL_R_BAD_CERT,	"Bad TLSA record certificate"},
    {DANESSL_R_BAD_CERT_PKEY,	"Bad TLSA record certificate public key"},
    {DANESSL_R_BAD_DATA_LENGTH,	"Bad TLSA record digest length"},
    {DANESSL_R_BAD_DIGEST,	"Bad TLSA record digest"},
    {DANESSL_R_BAD_NULL_DATA,	"Bad TLSA record null data"},
    {DANESSL_R_BAD_PKEY,	"Bad TLSA record public key"},
    {DANESSL_R_BAD_SELECTOR,	"Bad TLSA record selector"},
    {DANESSL_R_BAD_USAGE,	"Bad TLSA record usage"},
    {DANESSL_R_INIT,		"DANESSL_init() required"},
    {DANESSL_R_LIBRARY_INIT,	"DANESSL_library_init() required"},
    {DANESSL_R_NOSIGN_KEY,	"Certificate usage 2 requires EC support"},
    {DANESSL_R_SCTX_INIT,	"DANESSL_CTX_init() required"},
    {DANESSL_R_SUPPORT,		"DANE library features not supported"},
    {0,				NULL}
};
#endif

#define DANEerr(f, r) ERR_PUT_error(err_lib_dane, (f), (r), __FILE__, __LINE__)

static int err_lib_dane = -1;
static int dane_idx = -1;

#ifdef X509_V_FLAG_PARTIAL_CHAIN       /* OpenSSL >= 1.0.2 */
static int wrap_to_root = 0;
#else
static int wrap_to_root = 1;
#endif

static void (*cert_free)(void *) = (void (*)(void *)) X509_free;
static void (*pkey_free)(void *) = (void (*)(void *)) EVP_PKEY_free;

typedef struct dane_list {
    struct dane_list *next;
    void *value;
} *dane_list;

#define LINSERT(h, e) do { (e)->next = (h); (h) = (e); } while (0)

typedef struct DANE_HOST_LIST {
    struct DANE_HOST_LIST *next;
    char *value;
} *DANE_HOST_LIST;

typedef struct dane_data {
    size_t datalen;
    unsigned char data[0];
} *dane_data;

typedef struct DANE_DATA_LIST {
    struct DANE_DATA_LIST *next;
    dane_data value;
} *DANE_DATA_LIST;

typedef struct dane_mtype {
    int mdlen;
    const EVP_MD *md;
    DANE_DATA_LIST data;
} *dane_mtype;

typedef struct DANE_MTYPE_LIST {
    struct DANE_MTYPE_LIST *next;
    dane_mtype value;
} *DANE_MTYPE_LIST;

typedef struct dane_selector {
    uint8_t selector;
    DANE_MTYPE_LIST mtype;
} *dane_selector;

typedef struct DANE_SELECTOR_LIST {
    struct DANE_SELECTOR_LIST *next;
    dane_selector value;
} *DANE_SELECTOR_LIST;

typedef struct DANE_PKEY_LIST {
    struct DANE_PKEY_LIST *next;
    EVP_PKEY *value;
} *DANE_PKEY_LIST;

typedef struct DANE_CERT_LIST {
    struct DANE_CERT_LIST *next;
    X509 *value;
} *DANE_CERT_LIST;

typedef struct DANESSL {
    int            (*verify)(X509_STORE_CTX *);
    STACK_OF(X509) *roots;
    STACK_OF(X509) *chain;
    X509           *match;		/* Matched cert */
    const char     *thost;		/* TLSA base domain */
    char	   *mhost;		/* Matched peer name */
    DANE_PKEY_LIST pkeys;
    DANE_CERT_LIST certs;
    DANE_HOST_LIST hosts;
    DANE_SELECTOR_LIST selectors[DANESSL_USAGE_LAST + 1];
    int            depth;
    int		   mdpth;		/* Depth of matched cert */
    int		   multi;		/* Multi-label wildcards? */
    int		   count;		/* Number of TLSA records */
} DANESSL;

#ifndef X509_V_ERR_HOSTNAME_MISMATCH
#define X509_V_ERR_HOSTNAME_MISMATCH X509_V_ERR_APPLICATION_VERIFICATION
#endif

static int match(DANE_SELECTOR_LIST slist, X509 *cert, int depth)
{
    int matched;

    /*
     * Note, set_trust_anchor() needs to know whether the match was for a
     * pkey digest or a certificate digest.  We return MATCHED_PKEY or
     * MATCHED_CERT accordingly.
     */
#define MATCHED_CERT (DANESSL_SELECTOR_CERT + 1)
#define MATCHED_PKEY (DANESSL_SELECTOR_SPKI + 1)

    /*
     * Loop over each selector, mtype, and associated data element looking
     * for a match.
     */
    for (matched = 0; !matched && slist; slist = slist->next) {
	DANE_MTYPE_LIST m;
	unsigned char mdbuf[EVP_MAX_MD_SIZE];
	unsigned char *buf;
	unsigned char *buf2;
	unsigned int len;

	/*
	 * Extract ASN.1 DER form of certificate or public key.
	 */
	switch (slist->value->selector) {
	case DANESSL_SELECTOR_CERT:
	    len = i2d_X509(cert, NULL);
	    buf2 = buf = (unsigned char *) OPENSSL_malloc(len);
	    if (buf)
		i2d_X509(cert, &buf2);
	    break;
	case DANESSL_SELECTOR_SPKI:
	    len = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(cert), NULL);
	    buf2 = buf = (unsigned char *) OPENSSL_malloc(len);
	    if (buf)
		i2d_X509_PUBKEY(X509_get_X509_PUBKEY(cert), &buf2);
	    break;
	default:
	    /* To suppress buf and len uninitialized variable warnings */
	    OPENSSL_assert(slist->value->selector == DANESSL_SELECTOR_CERT
		|| slist->value->selector == DANESSL_SELECTOR_SPKI);
	    len = 0;
	    buf2 = buf = NULL;
	    break;
	}

	if (buf == NULL) {
	    DANEerr(DANESSL_F_MATCH, ERR_R_MALLOC_FAILURE);
	    return 0;
	}
	OPENSSL_assert(buf2 - buf == len);

	/*
	 * Loop over each mtype and data element
	 */
	for (m = slist->value->mtype; !matched && m; m = m->next) {
	    DANE_DATA_LIST d;
	    unsigned char *cmpbuf = buf;
	    unsigned int cmplen = len;

	    /*
	     * If it is a digest, compute the corresponding digest of the
	     * DER data for comparison, otherwise, use the full object.
	     */
	    if (m->value->md) {
		cmpbuf = mdbuf;
		if (!EVP_Digest(buf, len, cmpbuf, &cmplen, m->value->md, 0))
		    matched = -1;
	    }
	    for (d = m->value->data; !matched && d; d = d->next)
		if (cmplen == d->value->datalen &&
		    memcmp(cmpbuf, d->value->data, cmplen) == 0)
		    matched = slist->value->selector + 1;
	}

	OPENSSL_free(buf);
    }

    return matched;
}

static int push_ext(X509 *cert, X509_EXTENSION *ext)
{
    if (ext) {
	if (X509_add_ext(cert, ext, -1))
	    return 1;
	X509_EXTENSION_free(ext);
    }
    DANEerr(DANESSL_F_PUSH_EXT, ERR_R_MALLOC_FAILURE);
    return 0;
}

static int add_ext(X509 *issuer, X509 *subject, int ext_nid, char *ext_val)
{
    X509V3_CTX v3ctx;

    X509V3_set_ctx(&v3ctx, issuer, subject, 0, 0, 0);
    return push_ext(subject, X509V3_EXT_conf_nid(0, &v3ctx, ext_nid, ext_val));
}

static int set_serial(X509 *cert, AUTHORITY_KEYID *akid, X509 *subject)
{
    int ret = 0;
    BIGNUM *bn;

    if (akid && akid->serial)
	return (X509_set_serialNumber(cert, akid->serial));

    /*
     * Add one to subject's serial to avoid collisions between TA serial and
     * serial of signing root.
     */
    if ((bn = ASN1_INTEGER_to_BN(X509_get_serialNumber(subject), 0)) != 0
	&& BN_add_word(bn, 1)
	&& BN_to_ASN1_INTEGER(bn, X509_get_serialNumber(cert)))
	ret = 1;

    if (bn)
	BN_free(bn);
    return ret;
}

static int add_akid(X509 *cert, AUTHORITY_KEYID *akid)
{
    int nid = NID_authority_key_identifier;
    ASN1_OCTET_STRING *id;
    unsigned char c = 0;
    int ret = 0;

    /*
     * 0 will never be our subject keyid from a SHA-1 hash, but it could be
     * our subject keyid if forced from child's akid.  If so, set our
     * authority keyid to 1.  This way we are never self-signed, and thus
     * exempt from any potential (off by default for now in OpenSSL)
     * self-signature checks!
     */
    id =  (akid && akid->keyid) ? akid->keyid : 0;
    if (id && ASN1_STRING_length(id) == 1 && *ASN1_STRING_get0_data(id) == c)
	c = 1;

    if ((akid = AUTHORITY_KEYID_new()) != 0
	&& (akid->keyid = ASN1_OCTET_STRING_new()) != 0
	&& ASN1_OCTET_STRING_set(akid->keyid, (void *) &c, 1)
	&& X509_add1_ext_i2d(cert, nid, akid, 0, X509V3_ADD_APPEND))
	ret = 1;
    if (akid)
	AUTHORITY_KEYID_free(akid);
    return ret;
}

static int add_skid(X509 *cert, AUTHORITY_KEYID *akid)
{
    int nid = NID_subject_key_identifier;

    if (!akid || !akid->keyid)
	return add_ext(0, cert, nid, "hash");
    return X509_add1_ext_i2d(cert, nid, akid->keyid, 0, X509V3_ADD_APPEND) > 0;
}

static X509_NAME *akid_issuer_name(AUTHORITY_KEYID *akid)
{
    if (akid && akid->issuer) {
	int     i;
	GENERAL_NAMES *gens = akid->issuer;

	for (i = 0; i < sk_GENERAL_NAME_num(gens); ++i) {
	    GENERAL_NAME *gn = sk_GENERAL_NAME_value(gens, i);

	    if (gn->type == GEN_DIRNAME)
		return (gn->d.dirn);
	}
    }
    return 0;
}

static int set_issuer_name(X509 *cert, AUTHORITY_KEYID *akid, X509_NAME *subj)
{
    X509_NAME *name = akid_issuer_name(akid);

    /*
     * If subject's akid specifies an authority key identifer issuer name, we
     * must use that.
     */
    if (name)
	return X509_set_issuer_name(cert, name);
    return X509_set_issuer_name(cert, subj);
}

static int grow_chain(DANESSL *dane, int trusted, X509 *cert)
{
    STACK_OF(X509) **xs = trusted ? &dane->roots : &dane->chain;
    static ASN1_OBJECT *serverAuth = 0;

#define UNTRUSTED 0
#define TRUSTED 1

    if (trusted && serverAuth == 0 &&
       (serverAuth = OBJ_nid2obj(NID_server_auth)) == 0) {
	DANEerr(DANESSL_F_GROW_CHAIN, ERR_R_MALLOC_FAILURE);
	return 0;
    }
    if (!*xs && (*xs = sk_X509_new_null()) == 0) {
	DANEerr(DANESSL_F_GROW_CHAIN, ERR_R_MALLOC_FAILURE);
	return 0;
    }

    if (cert) {
	if (trusted && !X509_add1_trust_object(cert, serverAuth))
	    return 0;
	X509_up_ref(cert);
	if (!sk_X509_push(*xs, cert)) {
	    X509_free(cert);
	    DANEerr(DANESSL_F_GROW_CHAIN, ERR_R_MALLOC_FAILURE);
	    return 0;
	}
    }
    return 1;
}

static int wrap_issuer(
	DANESSL *dane,
	EVP_PKEY *key,
	X509 *subject,
	int depth,
	int top
)
{
    int ret = 1;
    X509 *cert = 0;
    AUTHORITY_KEYID *akid;
    X509_NAME *name = X509_get_issuer_name(subject);
    EVP_PKEY *newkey = key ? key : X509_get_pubkey(subject);

#define WRAP_MID 0		/* Ensure intermediate. */
#define WRAP_TOP 1		/* Ensure self-signed. */

    if (name == 0 || newkey == 0 || (cert = X509_new()) == 0)
	return 0;

    /*
     * Record the depth of the trust-anchor certificate.
     */
    if (dane->depth < 0)
	dane->depth = depth + 1;

    /*
     * XXX: Uncaught error condition:
     *
     * The return value is NULL both when the extension is missing, and when
     * OpenSSL rans out of memory while parsing the extension.
     */
    ERR_clear_error();
    akid = X509_get_ext_d2i(subject, NID_authority_key_identifier, 0, 0);
    /* XXX: Should we peek at the error stack here??? */

    /*
     * If top is true generate a self-issued root CA, otherwise an
     * intermediate CA and possibly its self-signed issuer.
     *
     * CA cert valid for +/- 30 days
     */
    if (!X509_set_version(cert, 2)
	|| !set_serial(cert, akid, subject)
	|| !set_issuer_name(cert, akid, name)
	|| !X509_gmtime_adj(X509_getm_notBefore(cert), -30 * 86400L)
	|| !X509_gmtime_adj(X509_getm_notAfter(cert), 30 * 86400L)
	|| !X509_set_subject_name(cert, name)
	|| !X509_set_pubkey(cert, newkey)
	|| !add_ext(0, cert, NID_basic_constraints, "CA:TRUE")
	|| (!top && !add_akid(cert, akid))
	|| !add_skid(cert, akid)
	|| (!top && wrap_to_root &&
	    !wrap_issuer(dane, newkey, cert, depth, WRAP_TOP))) {
	ret = 0;
    }
    if (akid)
	AUTHORITY_KEYID_free(akid);
    if (!key)
	EVP_PKEY_free(newkey);
    if (ret) {
	if (!top && wrap_to_root)
	    ret = grow_chain(dane, UNTRUSTED, cert);
	else
	    ret = grow_chain(dane, TRUSTED, cert);
    }
    if (cert)
	X509_free(cert);
    return ret;
}

static int wrap_cert(DANESSL *dane, X509 *tacert, int depth)
{
    if (dane->depth < 0)
	dane->depth = depth + 1;

    /*
     * If the TA certificate is self-issued, or need not be, use it directly.
     * Otherwise, synthesize requisuite ancestors.
     */
    if (!wrap_to_root
	|| X509_check_issued(tacert, tacert) == X509_V_OK)
	return grow_chain(dane, TRUSTED, tacert);

    if (wrap_issuer(dane, 0, tacert, depth, WRAP_MID))
	return grow_chain(dane, UNTRUSTED, tacert);
    return 0;
}

static int ta_signed(DANESSL *dane, X509 *cert, int depth)
{
    DANE_CERT_LIST x;
    DANE_PKEY_LIST k;
    EVP_PKEY *pk;
    int done = 0;

    /*
     * First check whether issued and signed by a TA cert, this is cheaper
     * than the bare-public key checks below, since we can determine whether
     * the candidate TA certificate issued the certificate to be checked
     * first (name comparisons), before we bother with signature checks
     * (public key operations).
     */
    for (x = dane->certs; !done && x; x = x->next) {
	if (X509_check_issued(x->value, cert) == X509_V_OK) {
	    if ((pk = X509_get_pubkey(x->value)) == 0) {
		/*
		 * The cert originally contained a valid pkey, which does
		 * not just vanish, so this is most likely a memory error.
		 */
		done = -1;
		break;
	    }
	    /* Check signature, since some other TA may work if not this. */
	    if (X509_verify(cert, pk) > 0)
		done = wrap_cert(dane, x->value, depth) ? 1 : -1;
	    EVP_PKEY_free(pk);
	}
    }

    /*
     * With bare TA public keys, we can't check whether the trust chain is
     * issued by the key, but we can determine whether it is signed by the
     * key, so we go with that.
     *
     * Ideally, the corresponding certificate was presented in the chain, and we
     * matched it by its public key digest one level up.  This code is here
     * to handle adverse conditions imposed by sloppy administrators of
     * receiving systems with poorly constructed chains.
     *
     * We'd like to optimize out keys that should not match when the cert's
     * authority key id does not match the key id of this key computed via
     * the RFC keyid algorithm (SHA-1 digest of public key bit-string sans
     * ASN1 tag and length thus also excluding the unused bits field that is
     * logically part of the length).  However, some CAs have a non-standard
     * authority keyid, so we lose.  Too bad.
     *
     * This may push errors onto the stack when the certificate signature is
     * not of the right type or length, throw these away,
     */
    for (k = dane->pkeys; !done && k; k = k->next)
	if (X509_verify(cert, k->value) > 0)
	    done = wrap_issuer(dane, k->value, cert, depth, WRAP_MID) ? 1 : -1;
	else
	    ERR_clear_error();

    return done;
}

static int set_trust_anchor(X509_STORE_CTX *ctx, DANESSL *dane, X509 *cert)
{
    int matched = 0;
    int n;
    int i;
    int depth = 0;
    EVP_PKEY *takey;
    X509 *ca;
    STACK_OF(X509) *in = X509_STORE_CTX_get0_untrusted(ctx);

    if (!grow_chain(dane, UNTRUSTED, 0))
	return -1;

    /*
     * Accept a degenerate case: depth 0 self-signed trust-anchor.
     */
    if (X509_check_issued(cert, cert) == X509_V_OK) {
	dane->depth = 0;
	matched = match(dane->selectors[DANESSL_USAGE_DANE_TA], cert, 0);
	if (matched > 0 && !grow_chain(dane, TRUSTED, cert))
	    matched = -1;
	return matched;
    }

    /* Make a shallow copy of the input untrusted chain. */
    if ((in = sk_X509_dup(in)) == 0) {
	DANEerr(DANESSL_F_SET_TRUST_ANCHOR, ERR_R_MALLOC_FAILURE);
	return -1;
    }

    /*
     * At each iteration we consume the issuer of the current cert.  This
     * reduces the length of the "in" chain by one.  If no issuer is found,
     * we are done.  We also stop when a certificate matches a TA in the
     * peer's TLSA RRset.
     *
     * Caller ensures that the initial certificate is not self-signed.
     */
    for (n = sk_X509_num(in); n > 0; --n, ++depth) {
	for (i = 0; i < n; ++i)
	    if (X509_check_issued(sk_X509_value(in, i), cert) == X509_V_OK)
		break;

	/*
	 * Final untrusted element with no issuer in the peer's chain, it may
	 * however be signed by a pkey or cert obtained via a TLSA RR.
	 */
	if (i == n)
	    break;

	/* Peer's chain contains an issuer ca. */
	ca = sk_X509_delete(in, i);

	/* If not a trust anchor, record untrusted ca and continue. */
	if ((matched = match(dane->selectors[DANESSL_USAGE_DANE_TA], ca,
			     depth + 1)) == 0) {
	    if (grow_chain(dane, UNTRUSTED, ca)) {
		if (X509_check_issued(ca, ca) != X509_V_OK) {
		    /* Restart with issuer as subject */
		    cert = ca;
		    continue;
		}
		/* Final self-signed element, skip ta_signed() check. */
		cert = 0;
	    } else
		matched = -1;
	} else if (matched == MATCHED_CERT) {
	    if (!wrap_cert(dane, ca, depth))
		matched = -1;
	} else if (matched == MATCHED_PKEY) {
	    if ((takey = X509_get_pubkey(ca)) == 0 ||
		!wrap_issuer(dane, takey, cert, depth, WRAP_MID)) {
		if (takey)
		    EVP_PKEY_free(takey);
		else
		    DANEerr(DANESSL_F_SET_TRUST_ANCHOR, ERR_R_MALLOC_FAILURE);
		matched = -1;
	    }
	}
	break;
    }

    /* Shallow free the duplicated input untrusted chain. */
    sk_X509_free(in);

    /*
     * When the loop exits, if "cert" is set, it is not self-signed and has
     * no issuer in the chain, we check for a possible signature via a DNS
     * obtained TA cert or public key.
     */
    if (matched == 0 && cert)
	 matched = ta_signed(dane, cert, depth);

    return matched;
}

static int check_end_entity(X509_STORE_CTX *ctx, DANESSL *dane, X509 *cert)
{
    int matched;

    matched = match(dane->selectors[DANESSL_USAGE_DANE_EE], cert, 0);
    if (matched > 0) {
	dane->mdpth = 0;
	dane->match = cert;
	X509_up_ref(cert);
	if (X509_STORE_CTX_get0_chain(ctx) == 0) {
	    STACK_OF(X509) *chain = sk_X509_new_null();

	    if (chain && sk_X509_push(chain, cert)) {
		X509_up_ref(cert);
		X509_STORE_CTX_set0_verified_chain(ctx, chain);
	    } else {
                if (chain)
                    sk_X509_free(chain);
		DANEerr(DANESSL_F_CHECK_END_ENTITY, ERR_R_MALLOC_FAILURE);
		return -1;
	    }
	}
    }
    return matched;
}

static int match_name(const char *certid, DANESSL *dane)
{
    int multi = dane->multi;
    DANE_HOST_LIST hosts = dane->hosts;

    for (/* NOP */; hosts; hosts = hosts->next) {
	int match_subdomain = 0;
	const char *domain = hosts->value;
	const char *parent;
	int idlen;
	int domlen;

	if (*domain == '.' && domain[1] != '\0') {
	    ++domain;
	    match_subdomain = 1;
	}

	/*
	 * Sub-domain match: certid is any sub-domain of hostname.
	 */
	if (match_subdomain) {
	    if ((idlen = strlen(certid)) > (domlen = strlen(domain)) + 1
		&& certid[idlen - domlen - 1] == '.'
		&& !strcasecmp(certid + (idlen - domlen), domain))
		return 1;
	    else
		continue;
	}

	/*
	 * Exact match and initial "*" match. The initial "*" in a certid
	 * matches one (if multi is false) or more hostname components under
	 * the condition that the certid contains multiple hostname components.
	 */
	if (!strcasecmp(certid, domain)
	    || (certid[0] == '*' && certid[1] == '.' && certid[2] != 0
		&& (parent = strchr(domain, '.')) != 0
		&& (idlen = strlen(certid + 1)) <= (domlen = strlen(parent))
		&& strcasecmp(multi ? parent + domlen - idlen : parent,
			      certid + 1) == 0))
	    return 1;
    }
    return 0;
}

static const char *check_name(const char *name, int len)
{
    register const char *cp = name + len;

    while (len > 0 && *--cp == 0)
	--len;				/* Ignore trailing NULs */
    if (len <= 0)
	return 0;
    for (cp = name; *cp; cp++) {
	register char c = *cp;
	if (!((c >= 'a' && c <= 'z') ||
	      (c >= '0' && c <= '9') ||
	      (c >= 'A' && c <= 'Z') ||
	      (c == '.' || c == '-') ||
	      (c == '*')))
	    return 0;			/* Only LDH, '.' and '*' */
    }
    if (cp - name != len)		/* Guard against internal NULs */
	return 0;
    return name;
}

static const char *parse_dns_name(const GENERAL_NAME *gn)
{
    if (gn->type != GEN_DNS)
	return 0;
    if (ASN1_STRING_type(gn->d.ia5) != V_ASN1_IA5STRING)
	return 0;
    return check_name((const char *)ASN1_STRING_get0_data(gn->d.ia5),
		      ASN1_STRING_length(gn->d.ia5));
}

static char *parse_subject_name(X509 *cert)
{
    X509_NAME *name = X509_get_subject_name(cert);
    X509_NAME_ENTRY *entry;
    ASN1_STRING *entry_str;
    unsigned char *namebuf;
    int nid = NID_commonName;
    int len;
    int i;

    if (name == 0 || (i = X509_NAME_get_index_by_NID(name, nid, -1)) < 0)
	return 0;
    if ((entry = X509_NAME_get_entry(name, i)) == 0)
	return 0;
    if ((entry_str = X509_NAME_ENTRY_get_data(entry)) == 0)
	return 0;

    if ((len = ASN1_STRING_to_UTF8(&namebuf, entry_str)) < 0)
	return 0;
    if (len <= 0 || check_name((char *) namebuf, len) == 0) {
	OPENSSL_free(namebuf);
	return 0;
    }
    return (char *) namebuf;
}

static int name_check(DANESSL *dane, X509 *cert)
{
    int matched = 0;
    int got_altname = 0;
    GENERAL_NAMES *gens;

    gens = X509_get_ext_d2i(cert, NID_subject_alt_name, 0, 0);
    if (gens) {
	int n = sk_GENERAL_NAME_num(gens);
	int i;

	for (i = 0; i < n; ++i) {
	    const GENERAL_NAME *gn = sk_GENERAL_NAME_value(gens, i);
	    const char *certid;

	    if (gn->type != GEN_DNS)
		continue;
	    got_altname = 1;
	    certid = parse_dns_name(gn);
	    if (certid && *certid) {
		if ((matched = match_name(certid, dane)) == 0)
		    continue;
		if ((dane->mhost = OPENSSL_strdup(certid)) == 0)
		    matched = -1;
		break;
	    }
	}
	GENERAL_NAMES_free(gens);
    }

    /*
     * XXX: Should the subjectName be skipped when *any* altnames are present,
     * or only when DNS altnames are present?
     */
    if (got_altname == 0) {
	char *certid = parse_subject_name(cert);
	if (certid != 0 && *certid
	    && (matched = match_name(certid, dane)) != 0)
	    dane->mhost = OPENSSL_strdup(certid);
	if (certid)
	    OPENSSL_free(certid);
    }
    return matched;
}

static int verify_chain(X509_STORE_CTX *ctx)
{
    DANE_SELECTOR_LIST issuer_rrs;
    DANE_SELECTOR_LIST leaf_rrs;
    int (*cb)(int, X509_STORE_CTX *) = X509_STORE_CTX_get_verify_cb(ctx);
    int ssl_idx = SSL_get_ex_data_X509_STORE_CTX_idx();
    SSL *ssl = X509_STORE_CTX_get_ex_data(ctx, ssl_idx);
    DANESSL *dane = SSL_get_ex_data(ssl, dane_idx);
    X509 *cert = X509_STORE_CTX_get0_cert(ctx);
    STACK_OF(X509) *chain = X509_STORE_CTX_get0_chain(ctx);
    int chain_length = sk_X509_num(chain);
    int matched = 0;

    issuer_rrs = dane->selectors[DANESSL_USAGE_PKIX_TA];
    leaf_rrs = dane->selectors[DANESSL_USAGE_PKIX_EE];

    /* Restore OpenSSL's internal_verify() as the signature check function */
    X509_STORE_CTX_set_verify(ctx, dane->verify);

    if ((matched = name_check(dane, cert)) < 0) {
	X509_STORE_CTX_set_error(ctx, X509_V_ERR_OUT_OF_MEM);
	return 0;
    }

    if (!matched) {
	X509_STORE_CTX_set_error_depth(ctx, 0);
	X509_STORE_CTX_set_current_cert(ctx, cert);
	X509_STORE_CTX_set_error(ctx, X509_V_ERR_HOSTNAME_MISMATCH);
	if (!cb(0, ctx))
	    return 0;
    }
    matched = 0;

    /*
     * Satisfy at least one usage 0 or 1 constraint, unless we've already
     * matched a usage 2 trust anchor.
     *
     * XXX: internal_verify() doesn't callback with top certs that are not
     * self-issued.  This is fixed in OpenSSL 1.1.0.
     */
    if (dane->roots && sk_X509_num(dane->roots)) {
	X509 *top = sk_X509_value(chain, dane->depth);

	dane->mdpth = dane->depth;
	dane->match = top;
	X509_up_ref(top);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	if (X509_check_issued(top, top) != X509_V_OK) {
	    X509_STORE_CTX_set_error_depth(ctx, dane->depth);
	    X509_STORE_CTX_set_current_cert(ctx, top);
	    if (!cb(1, ctx))
		return 0;
	}
#endif
	/* Pop synthetic trust-anchor ancestors off the chain! */
	while (--chain_length > dane->depth)
	    X509_free(sk_X509_pop(chain));
    } else {
	int n = 0;
	X509 *xn = cert;

	/*
	 * Check for an EE match, then a CA match at depths > 0, and
	 * finally, if the EE cert is self-issued, for a depth 0 CA match.
	 */
	if (leaf_rrs)
	    matched = match(leaf_rrs, xn, 0);
	if (!matched && issuer_rrs) {
	    for (n = chain_length-1; !matched && n >= 0; --n) {
		xn = sk_X509_value(chain, n);
		if (n > 0 || X509_check_issued(xn, xn) == X509_V_OK)
		    matched = match(issuer_rrs, xn, n);
	    }
	}

	if (matched < 0) {
	    X509_STORE_CTX_set_error(ctx, X509_V_ERR_OUT_OF_MEM);
	    return 0;
	}

	if (!matched) {
	    X509_STORE_CTX_set_current_cert(ctx, cert);
	    X509_STORE_CTX_set_error_depth(ctx, 0);
	    X509_STORE_CTX_set_error(ctx, X509_V_ERR_CERT_UNTRUSTED);
	    if (!cb(0, ctx))
		return 0;
	} else {
	    dane->mdpth = n;
	    dane->match = xn;
	    X509_up_ref(xn);
	}
    }

    /* Tail recurse into OpenSSL's internal_verify */
    return dane->verify(ctx);
}

static void dane_reset(DANESSL *dane)
{
    dane->depth = -1;
    if (dane->mhost) {
	OPENSSL_free(dane->mhost);
	dane->mhost = 0;
    }
    if (dane->roots) {
	sk_X509_pop_free(dane->roots, X509_free);
	dane->roots = 0;
    }
    if (dane->chain) {
	sk_X509_pop_free(dane->chain, X509_free);
	dane->chain = 0;
    }
    if (dane->match) {
	X509_free(dane->match);
	dane->match = 0;
    }
    dane->mdpth = -1;
}

static
int verify_cert(X509_STORE_CTX *ctx, void *unused_ctx)
{
    static int ssl_idx = -1;
    SSL *ssl;
    DANESSL *dane;
    int (*cb)(int, X509_STORE_CTX *) = X509_STORE_CTX_get_verify_cb(ctx);
    X509 *cert = X509_STORE_CTX_get0_cert(ctx);
    int matched;

    if (ssl_idx < 0)
	ssl_idx = SSL_get_ex_data_X509_STORE_CTX_idx();
    if (dane_idx < 0) {
	DANEerr(DANESSL_F_VERIFY_CERT, ERR_R_MALLOC_FAILURE);
	return -1;
    }

    ssl = X509_STORE_CTX_get_ex_data(ctx, ssl_idx);
    if ((dane = SSL_get_ex_data(ssl, dane_idx)) == 0 || cert == 0)
	return X509_verify_cert(ctx);

    /* Reset for verification of a new chain, perhaps a renegotiation. */
    dane_reset(dane);

    if (dane->selectors[DANESSL_USAGE_DANE_EE]) {
	if ((matched = check_end_entity(ctx, dane, cert)) > 0) {
	    X509_STORE_CTX_set_error_depth(ctx, 0);
	    X509_STORE_CTX_set_current_cert(ctx, cert);
	    return cb(1, ctx);
	}
	if (matched < 0) {
	    X509_STORE_CTX_set_error(ctx, X509_V_ERR_OUT_OF_MEM);
	    return -1;
	}
	/* Fail now, if all we have is DANE-EE TLSA records */
	if (!matched
	    && !dane->selectors[DANESSL_USAGE_DANE_TA]
	    && !dane->selectors[DANESSL_USAGE_PKIX_EE]
	    && !dane->selectors[DANESSL_USAGE_PKIX_TA]) {
	    X509_STORE_CTX_set_current_cert(ctx, cert);
	    X509_STORE_CTX_set_error_depth(ctx, 0);
	    X509_STORE_CTX_set_error(ctx, X509_V_ERR_CERT_UNTRUSTED);
	    return cb(0, ctx);
	}
    }

    if (dane->selectors[DANESSL_USAGE_DANE_TA]) {
	if ((matched = set_trust_anchor(ctx, dane, cert)) < 0) {
	    X509_STORE_CTX_set_error(ctx, X509_V_ERR_OUT_OF_MEM);
	    return -1;
	}
	if (matched) {
	    /*
	     * Check that setting the untrusted chain updates the expected
	     * structure member at the expected offset.
	     */
	    X509_STORE_CTX_set0_trusted_stack(ctx, dane->roots);
	    X509_STORE_CTX_set0_untrusted(ctx, dane->chain);
	    OPENSSL_assert(dane->chain == X509_STORE_CTX_get0_untrusted(ctx));
	}
    }

    /*
     * Name checks and usage 0/1 constraint enforcement are delayed until
     * X509_verify_cert() builds the full chain and calls our verify_chain()
     * wrapper.
     */
    dane->verify = X509_STORE_CTX_get_verify(ctx);
    X509_STORE_CTX_set_verify(ctx, verify_chain);

    if (X509_verify_cert(ctx))
	return 1;

    /*
     * If the chain is invalid, clear any matching cert or hostname, to
     * protect callers that might erroneously rely on these alone without
     * checking the validation status.
     */
    if (dane->match) {
	X509_free(dane->match);
	dane->match = 0;
    }
    if (dane->mhost) {
	OPENSSL_free(dane->mhost);
	dane->mhost = 0;
    }
    return 0;
}

static dane_list list_alloc(size_t vsize)
{
    void *value = (void *) OPENSSL_malloc(vsize);
    dane_list l;

    if (value == 0) {
	DANEerr(DANESSL_F_LIST_ALLOC, ERR_R_MALLOC_FAILURE);
	return 0;
    }
    if ((l = (dane_list) OPENSSL_malloc(sizeof(*l))) == 0) {
	OPENSSL_free(value);
	DANEerr(DANESSL_F_LIST_ALLOC, ERR_R_MALLOC_FAILURE);
	return 0;
    }
    l->next = 0;
    l->value = value;
    return l;
}

static void list_free(void *list, void (*f)(void *))
{
    dane_list head = (dane_list) list;
    dane_list next;

    for (/* NOP */; head; head = next) {
	next = head->next;
	if (f && head->value)
	    f(head->value);
	OPENSSL_free(head);
    }
}

static void ossl_free(void *p)
{
    OPENSSL_free(p);
}

static void dane_mtype_free(void *p)
{
    list_free(((dane_mtype) p)->data, ossl_free);
    OPENSSL_free(p);
}

static void dane_selector_free(void *p)
{
    list_free(((dane_selector) p)->mtype, dane_mtype_free);
    OPENSSL_free(p);
}

void DANESSL_cleanup(SSL *ssl)
{
    DANESSL *dane;
    int u;

    if (dane_idx < 0 || (dane = SSL_get_ex_data(ssl, dane_idx)) == 0)
	return;
    (void) SSL_set_ex_data(ssl, dane_idx, 0);

    dane_reset(dane);
    if (dane->hosts)
	list_free(dane->hosts, ossl_free);
    for (u = 0; u <= DANESSL_USAGE_LAST; ++u)
	if (dane->selectors[u])
	    list_free(dane->selectors[u], dane_selector_free);
    if (dane->pkeys)
	list_free(dane->pkeys, pkey_free);
    if (dane->certs)
	list_free(dane->certs, cert_free);
    OPENSSL_free(dane);
}

static DANE_HOST_LIST host_list_init(const char **src)
{
    DANE_HOST_LIST head = 0;

    while (*src) {
	DANE_HOST_LIST elem = (DANE_HOST_LIST) OPENSSL_malloc(sizeof(*elem));
	if (elem == 0) {
	    list_free(head, ossl_free);
	    return 0;
	}
	elem->value = OPENSSL_strdup(*src++);
	LINSERT(head, elem);
    }
    return head;
}


int DANESSL_get_match_cert(SSL *ssl, X509 **match, const char **mhost, int *depth)
{
    DANESSL *dane;

    if (dane_idx < 0 || (dane = SSL_get_ex_data(ssl, dane_idx)) == 0) {
	DANEerr(DANESSL_F_ADD_TLSA, DANESSL_R_INIT);
	return -1;
    }

    if (dane->match) {
	if (match)
	    *match = dane->match;
	if (mhost)
	    *mhost = dane->mhost;
	if (depth)
	    *depth = dane->mdpth;
    }

    return (dane->match != 0);
}

int DANESSL_verify_chain(SSL *ssl, STACK_OF(X509) *chain)
{
    int ret;
    X509 *cert;
    X509_STORE_CTX *store_ctx;
    SSL_CTX *ssl_ctx = SSL_get_SSL_CTX(ssl);
    X509_STORE *store = SSL_CTX_get_cert_store(ssl_ctx);
    int store_ctx_idx = SSL_get_ex_data_X509_STORE_CTX_idx();

    cert = sk_X509_value(chain, 0);
    if ((store_ctx = X509_STORE_CTX_new()) == NULL) {
	DANEerr(DANESSL_F_DANESSL_VERIFY_CHAIN, ERR_R_MALLOC_FAILURE);
	return 0;
    }
    if (!X509_STORE_CTX_init(store_ctx, store, cert, chain)) {
	X509_STORE_CTX_free(store_ctx);
	return 0;
    }
    X509_STORE_CTX_set_ex_data(store_ctx, store_ctx_idx, ssl);

    X509_STORE_CTX_set_default(store_ctx,
	    SSL_is_server(ssl) ? "ssl_client" : "ssl_server");
    X509_VERIFY_PARAM_set1(X509_STORE_CTX_get0_param(store_ctx),
	    SSL_get0_param(ssl));

    if (SSL_get_verify_callback(ssl))
	X509_STORE_CTX_set_verify_cb(store_ctx, SSL_get_verify_callback(ssl));

    ret = verify_cert(store_ctx, NULL);

    SSL_set_verify_result(ssl, X509_STORE_CTX_get_error(store_ctx));
    X509_STORE_CTX_free(store_ctx);

    return (ret);
}


int DANESSL_add_tlsa(
	SSL *ssl,
	uint8_t usage,
	uint8_t selector,
	const char *mdname,
	unsigned const char *data,
	size_t dlen
)
{
    DANESSL *dane;
    DANE_SELECTOR_LIST s = 0;
    DANE_MTYPE_LIST m = 0;
    DANE_DATA_LIST d = 0;
    DANE_CERT_LIST xlist = 0;
    DANE_PKEY_LIST klist = 0;
    const EVP_MD *md = 0;

    if (dane_idx < 0 || (dane = SSL_get_ex_data(ssl, dane_idx)) == 0) {
	DANEerr(DANESSL_F_ADD_TLSA, DANESSL_R_INIT);
	return -1;
    }

    if (usage > DANESSL_USAGE_LAST) {
	DANEerr(DANESSL_F_ADD_TLSA, DANESSL_R_BAD_USAGE);
	return 0;
    }
    if (selector > DANESSL_SELECTOR_LAST) {
	DANEerr(DANESSL_F_ADD_TLSA, DANESSL_R_BAD_SELECTOR);
	return 0;
    }

    /* Support built-in standard one-digit mtypes */
    if (mdname && *mdname && mdname[1] == '\0') 
	switch (*mdname - '0') {
	    case DANESSL_MATCHING_FULL: mdname = 0;	   break;
	    case DANESSL_MATCHING_2256: mdname = "sha256"; break;
	    case DANESSL_MATCHING_2512: mdname = "sha512"; break;
	}
    if (mdname && *mdname && (md = EVP_get_digestbyname(mdname)) == 0) {
	DANEerr(DANESSL_F_ADD_TLSA, DANESSL_R_BAD_DIGEST);
	return 0;
    }
    if (mdname && *mdname && (int)dlen != EVP_MD_size(md)) {
	DANEerr(DANESSL_F_ADD_TLSA, DANESSL_R_BAD_DATA_LENGTH);
	return 0;
    }
    if (!data) {
	DANEerr(DANESSL_F_ADD_TLSA, DANESSL_R_BAD_NULL_DATA);
	return 0;
    }

    /*
     * Full Certificate or Public Key when NULL or empty digest name
     */
    if (!mdname || !*mdname) {
	X509 *x = 0;
	EVP_PKEY *k = 0;
	const unsigned char *p = data;

#define xklistinit(lvar, ltype, var, freeFunc) do { \
	    (lvar) = (ltype) OPENSSL_malloc(sizeof(*(lvar))); \
	    if ((lvar) == 0) { \
		DANEerr(DANESSL_F_ADD_TLSA, ERR_R_MALLOC_FAILURE); \
		freeFunc((var)); \
		return 0; \
	    } \
	    (lvar)->next = 0; \
	    lvar->value = var; \
	} while (0)
#define xkfreeret(ret) do { \
	    if (xlist) list_free(xlist, cert_free); \
	    if (klist) list_free(klist, pkey_free); \
	    return (ret); \
	} while (0)

	switch (selector) {
	case DANESSL_SELECTOR_CERT:
	    if (!d2i_X509(&x, &p, dlen) || (int)dlen != p - data) {
		if (x)
		    X509_free(x);
		DANEerr(DANESSL_F_ADD_TLSA, DANESSL_R_BAD_CERT);
		return 0;
	    }
	    k = X509_get_pubkey(x);
	    EVP_PKEY_free(k);
	    if (k == 0) {
		X509_free(x);
		DANEerr(DANESSL_F_ADD_TLSA, DANESSL_R_BAD_CERT_PKEY);
		return 0;
	    }
	    if (usage == DANESSL_USAGE_DANE_TA)
		xklistinit(xlist, DANE_CERT_LIST, x, X509_free);
	    break;

	case DANESSL_SELECTOR_SPKI:
	    if (!d2i_PUBKEY(&k, &p, dlen) || (int)dlen != p - data) {
		if (k)
		    EVP_PKEY_free(k);
		DANEerr(DANESSL_F_ADD_TLSA, DANESSL_R_BAD_PKEY);
		return 0;
	    }
	    if (usage == DANESSL_USAGE_DANE_TA)
		xklistinit(klist, DANE_PKEY_LIST, k, EVP_PKEY_free);
	    break;
	}
    }

    /* Find insertion point and don't add duplicate elements. */
    for (s = dane->selectors[usage]; s; s = s->next) {
	if (s->value->selector == selector) {
	    for (m = s->value->mtype; m; m = m->next) {
		if (m->value->md == md) {
		    for (d = m->value->data; d; d = d->next)
			if (d->value->datalen == dlen &&
			    memcmp(d->value->data, data, dlen) == 0)
			    xkfreeret(1);
		    break;
		}
	    }
	    break;
	}
    }

    if ((d = (DANE_DATA_LIST) list_alloc(sizeof(*d->value) + dlen)) == 0)
	xkfreeret(0);
    d->value->datalen = dlen;
    memcpy(d->value->data, data, dlen);
    if (!m) {
	if ((m = (DANE_MTYPE_LIST) list_alloc(sizeof(*m->value))) == 0) {
	    list_free(d, ossl_free);
	    xkfreeret(0);
	}
	m->value->data = 0;
	if ((m->value->md = md) != 0)
	    m->value->mdlen = dlen;
	if (!s) {
	    if ((s = (DANE_SELECTOR_LIST) list_alloc(sizeof(*s->value))) == 0) {
		list_free(m, dane_mtype_free);
		xkfreeret(0);
	    }
	    s->value->mtype = 0;
	    s->value->selector = selector;
	    LINSERT(dane->selectors[usage], s);
	}
	LINSERT(s->value->mtype, m);
    }
    LINSERT(m->value->data, d);

    if (xlist)
	LINSERT(dane->certs, xlist);
    else if (klist)
	LINSERT(dane->pkeys, klist);
    ++dane->count;
    return 1;
}

int DANESSL_init(SSL *ssl, const char *sni_domain, const char **hostnames)
{
    DANESSL *dane;
    int i;

    if (dane_idx < 0) {
	DANEerr(DANESSL_F_INIT, DANESSL_R_LIBRARY_INIT);
	return -1;
    }

    if (sni_domain && !SSL_set_tlsext_host_name(ssl, sni_domain))
	return 0;

    if ((dane = (DANESSL *) OPENSSL_malloc(sizeof(DANESSL))) == 0) {
	DANEerr(DANESSL_F_INIT, ERR_R_MALLOC_FAILURE);
	return 0;
    }
    if (!SSL_set_ex_data(ssl, dane_idx, dane)) {
	DANEerr(DANESSL_F_INIT, ERR_R_MALLOC_FAILURE);
	OPENSSL_free(dane);
	return 0;
    }

    dane->pkeys = 0;
    dane->certs = 0;
    dane->chain = 0;
    dane->match = 0;
    dane->roots = 0;
    dane->depth = -1;
    dane->mhost = 0;			/* Future SSL control interface */
    dane->mdpth = 0;			/* Future SSL control interface */
    dane->multi = 0;			/* Future SSL control interface */
    dane->count = 0;
    dane->hosts = 0;

    for (i = 0; i <= DANESSL_USAGE_LAST; ++i)
	dane->selectors[i] = 0;

    if (hostnames && (dane->hosts = host_list_init(hostnames)) == 0) {
	DANEerr(DANESSL_F_INIT, ERR_R_MALLOC_FAILURE);
	DANESSL_cleanup(ssl);
	return 0;
    }

    return 1;
}

int DANESSL_CTX_init(SSL_CTX *ctx)
{
    if (dane_idx >= 0) {
	SSL_CTX_set_cert_verify_callback(ctx, verify_cert, 0);
	return 1;
    }
    DANEerr(DANESSL_F_CTX_INIT, DANESSL_R_LIBRARY_INIT);
    return -1;
}

static void dane_init(void)
{
    /*
     * Store library id in zeroth function slot, used to locate the library
     * name.  This must be done before we load the error strings.
     */
    err_lib_dane = ERR_get_next_error_library();
#ifndef OPENSSL_NO_ERR
    if (err_lib_dane > 0) {
	dane_str_functs[0].error |= ERR_PACK(err_lib_dane, 0, 0);
	ERR_load_strings(err_lib_dane, dane_str_functs);
	ERR_load_strings(err_lib_dane, dane_str_reasons);
    }
#endif

    /*
     * Register SHA-2 digests, if implemented and not already registered.
     */
#if defined(LN_sha256) && defined(NID_sha256) && !defined(OPENSSL_NO_SHA256)
    if (!EVP_get_digestbyname(LN_sha224))
	EVP_add_digest(EVP_sha224());
    if (!EVP_get_digestbyname(LN_sha256))
	EVP_add_digest(EVP_sha256());
#endif
#if defined(LN_sha512) && defined(NID_sha512) && !defined(OPENSSL_NO_SHA512)
    if (!EVP_get_digestbyname(LN_sha384))
	EVP_add_digest(EVP_sha384());
    if (!EVP_get_digestbyname(LN_sha512))
	EVP_add_digest(EVP_sha512());
#endif

    /*
     * Register an SSL index for the connection-specific DANESSL structure.
     * Using a separate index makes it possible to add DANE support to
     * existing OpenSSL releases that don't have a suitable pointer in the
     * SSL structure.
     */
    dane_idx = SSL_get_ex_new_index(0, 0, 0, 0, 0);
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static void run_once(volatile int *once, void (*init)(void))
{
    int wlock = 0;

    CRYPTO_r_lock(CRYPTO_LOCK_SSL_CTX);
    if (!*once) {
	CRYPTO_r_unlock(CRYPTO_LOCK_SSL_CTX);
	CRYPTO_w_lock(CRYPTO_LOCK_SSL_CTX);
	wlock = 1;
	if (!*once) {
	    *once = 1;
	    init();
	}
    }
    if (wlock)
	CRYPTO_w_unlock(CRYPTO_LOCK_SSL_CTX);
    else
	CRYPTO_r_unlock(CRYPTO_LOCK_SSL_CTX);
}
#endif

int DANESSL_library_init(void)
{
    static CRYPTO_ONCE once = CRYPTO_ONCE_STATIC_INIT;

    (void) CRYPTO_THREAD_run_once(&once, dane_init);

#if defined(LN_sha256)
    /* No DANE without SHA256 support */
    if (dane_idx >= 0 && EVP_get_digestbyname(LN_sha256) != 0)
	return 1;
#endif
    DANEerr(DANESSL_F_LIBRARY_INIT, DANESSL_R_SUPPORT);
    return 0;
}
