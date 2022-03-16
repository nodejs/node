/*
 * Copyright 2007-2022 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright Nokia 2007-2019
 * Copyright Siemens AG 2015-2019
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* This app is disabled when OPENSSL_NO_CMP is defined. */

#include <string.h>
#include <ctype.h>

#include "apps.h"
#include "http_server.h"
#include "s_apps.h"
#include "progs.h"

#include "cmp_mock_srv.h"

/* tweaks needed due to missing unistd.h on Windows */
#if defined(_WIN32) && !defined(__BORLANDC__)
# define access _access
#endif
#ifndef F_OK
# define F_OK 0
#endif

#include <openssl/ui.h>
#include <openssl/pkcs12.h>
#include <openssl/ssl.h>

/* explicit #includes not strictly needed since implied by the above: */
#include <stdlib.h>
#include <openssl/cmp.h>
#include <openssl/cmp_util.h>
#include <openssl/crmf.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/store.h>
#include <openssl/objects.h>
#include <openssl/x509.h>

static char *prog;
static char *opt_config = NULL;
#define CMP_SECTION "cmp"
#define SECTION_NAME_MAX 40 /* max length of section name */
#define DEFAULT_SECTION "default"
static char *opt_section = CMP_SECTION;
static int opt_verbosity = OSSL_CMP_LOG_INFO;

static int read_config(void);

static CONF *conf = NULL; /* OpenSSL config file context structure */
static OSSL_CMP_CTX *cmp_ctx = NULL; /* the client-side CMP context */

/* the type of cmp command we want to send */
typedef enum {
    CMP_IR,
    CMP_KUR,
    CMP_CR,
    CMP_P10CR,
    CMP_RR,
    CMP_GENM
} cmp_cmd_t;

/* message transfer */
#ifndef OPENSSL_NO_SOCK
static char *opt_server = NULL;
static char *opt_proxy = NULL;
static char *opt_no_proxy = NULL;
#endif
static char *opt_recipient = NULL;
static char *opt_path = NULL;
static int opt_keep_alive = 1;
static int opt_msg_timeout = -1;
static int opt_total_timeout = -1;

/* server authentication */
static char *opt_trusted = NULL;
static char *opt_untrusted = NULL;
static char *opt_srvcert = NULL;
static char *opt_expect_sender = NULL;
static int opt_ignore_keyusage = 0;
static int opt_unprotected_errors = 0;
static char *opt_extracertsout = NULL;
static char *opt_cacertsout = NULL;

/* client authentication */
static char *opt_ref = NULL;
static char *opt_secret = NULL;
static char *opt_cert = NULL;
static char *opt_own_trusted = NULL;
static char *opt_key = NULL;
static char *opt_keypass = NULL;
static char *opt_digest = NULL;
static char *opt_mac = NULL;
static char *opt_extracerts = NULL;
static int opt_unprotected_requests = 0;

/* generic message */
static char *opt_cmd_s = NULL;
static int opt_cmd = -1;
static char *opt_geninfo = NULL;
static char *opt_infotype_s = NULL;
static int opt_infotype = NID_undef;

/* certificate enrollment */
static char *opt_newkey = NULL;
static char *opt_newkeypass = NULL;
static char *opt_subject = NULL;
static char *opt_issuer = NULL;
static int opt_days = 0;
static char *opt_reqexts = NULL;
static char *opt_sans = NULL;
static int opt_san_nodefault = 0;
static char *opt_policies = NULL;
static char *opt_policy_oids = NULL;
static int opt_policy_oids_critical = 0;
static int opt_popo = OSSL_CRMF_POPO_NONE - 1;
static char *opt_csr = NULL;
static char *opt_out_trusted = NULL;
static int opt_implicit_confirm = 0;
static int opt_disable_confirm = 0;
static char *opt_certout = NULL;
static char *opt_chainout = NULL;

/* certificate enrollment and revocation */
static char *opt_oldcert = NULL;
static int opt_revreason = CRL_REASON_NONE;

/* credentials format */
static char *opt_certform_s = "PEM";
static int opt_certform = FORMAT_PEM;
static char *opt_keyform_s = NULL;
static int opt_keyform = FORMAT_UNDEF;
static char *opt_otherpass = NULL;
static char *opt_engine = NULL;

#ifndef OPENSSL_NO_SOCK
/* TLS connection */
static int opt_tls_used = 0;
static char *opt_tls_cert = NULL;
static char *opt_tls_key = NULL;
static char *opt_tls_keypass = NULL;
static char *opt_tls_extra = NULL;
static char *opt_tls_trusted = NULL;
static char *opt_tls_host = NULL;
#endif

/* client-side debugging */
static int opt_batch = 0;
static int opt_repeat = 1;
static char *opt_reqin = NULL;
static int opt_reqin_new_tid = 0;
static char *opt_reqout = NULL;
static char *opt_rspin = NULL;
static char *opt_rspout = NULL;
static int opt_use_mock_srv = 0;

/* mock server */
#ifndef OPENSSL_NO_SOCK
static char *opt_port = NULL;
static int opt_max_msgs = 0;
#endif
static char *opt_srv_ref = NULL;
static char *opt_srv_secret = NULL;
static char *opt_srv_cert = NULL;
static char *opt_srv_key = NULL;
static char *opt_srv_keypass = NULL;

static char *opt_srv_trusted = NULL;
static char *opt_srv_untrusted = NULL;
static char *opt_rsp_cert = NULL;
static char *opt_rsp_extracerts = NULL;
static char *opt_rsp_capubs = NULL;
static int opt_poll_count = 0;
static int opt_check_after = 1;
static int opt_grant_implicitconf = 0;

static int opt_pkistatus = OSSL_CMP_PKISTATUS_accepted;
static int opt_failure = INT_MIN;
static int opt_failurebits = 0;
static char *opt_statusstring = NULL;
static int opt_send_error = 0;
static int opt_send_unprotected = 0;
static int opt_send_unprot_err = 0;
static int opt_accept_unprotected = 0;
static int opt_accept_unprot_err = 0;
static int opt_accept_raverified = 0;

static X509_VERIFY_PARAM *vpm = NULL;

typedef enum OPTION_choice {
    OPT_COMMON,
    OPT_CONFIG, OPT_SECTION, OPT_VERBOSITY,

    OPT_CMD, OPT_INFOTYPE, OPT_GENINFO,

    OPT_NEWKEY, OPT_NEWKEYPASS, OPT_SUBJECT, OPT_ISSUER,
    OPT_DAYS, OPT_REQEXTS,
    OPT_SANS, OPT_SAN_NODEFAULT,
    OPT_POLICIES, OPT_POLICY_OIDS, OPT_POLICY_OIDS_CRITICAL,
    OPT_POPO, OPT_CSR,
    OPT_OUT_TRUSTED, OPT_IMPLICIT_CONFIRM, OPT_DISABLE_CONFIRM,
    OPT_CERTOUT, OPT_CHAINOUT,

    OPT_OLDCERT, OPT_REVREASON,

#ifndef OPENSSL_NO_SOCK
    OPT_SERVER, OPT_PROXY, OPT_NO_PROXY,
#endif
    OPT_RECIPIENT, OPT_PATH,
    OPT_KEEP_ALIVE, OPT_MSG_TIMEOUT, OPT_TOTAL_TIMEOUT,

    OPT_TRUSTED, OPT_UNTRUSTED, OPT_SRVCERT,
    OPT_EXPECT_SENDER,
    OPT_IGNORE_KEYUSAGE, OPT_UNPROTECTED_ERRORS,
    OPT_EXTRACERTSOUT, OPT_CACERTSOUT,

    OPT_REF, OPT_SECRET, OPT_CERT, OPT_OWN_TRUSTED, OPT_KEY, OPT_KEYPASS,
    OPT_DIGEST, OPT_MAC, OPT_EXTRACERTS,
    OPT_UNPROTECTED_REQUESTS,

    OPT_CERTFORM, OPT_KEYFORM,
    OPT_OTHERPASS,
#ifndef OPENSSL_NO_ENGINE
    OPT_ENGINE,
#endif
    OPT_PROV_ENUM,
    OPT_R_ENUM,

#ifndef OPENSSL_NO_SOCK
    OPT_TLS_USED, OPT_TLS_CERT, OPT_TLS_KEY,
    OPT_TLS_KEYPASS,
    OPT_TLS_EXTRA, OPT_TLS_TRUSTED, OPT_TLS_HOST,
#endif

    OPT_BATCH, OPT_REPEAT,
    OPT_REQIN, OPT_REQIN_NEW_TID, OPT_REQOUT, OPT_RSPIN, OPT_RSPOUT,
    OPT_USE_MOCK_SRV,

#ifndef OPENSSL_NO_SOCK
    OPT_PORT, OPT_MAX_MSGS,
#endif
    OPT_SRV_REF, OPT_SRV_SECRET,
    OPT_SRV_CERT, OPT_SRV_KEY, OPT_SRV_KEYPASS,
    OPT_SRV_TRUSTED, OPT_SRV_UNTRUSTED,
    OPT_RSP_CERT, OPT_RSP_EXTRACERTS, OPT_RSP_CAPUBS,
    OPT_POLL_COUNT, OPT_CHECK_AFTER,
    OPT_GRANT_IMPLICITCONF,
    OPT_PKISTATUS, OPT_FAILURE,
    OPT_FAILUREBITS, OPT_STATUSSTRING,
    OPT_SEND_ERROR, OPT_SEND_UNPROTECTED,
    OPT_SEND_UNPROT_ERR, OPT_ACCEPT_UNPROTECTED,
    OPT_ACCEPT_UNPROT_ERR, OPT_ACCEPT_RAVERIFIED,

    OPT_V_ENUM
} OPTION_CHOICE;

const OPTIONS cmp_options[] = {
    /* entries must be in the same order as enumerated above!! */
    {"help", OPT_HELP, '-', "Display this summary"},
    {"config", OPT_CONFIG, 's',
     "Configuration file to use. \"\" = none. Default from env variable OPENSSL_CONF"},
    {"section", OPT_SECTION, 's',
     "Section(s) in config file to get options from. \"\" = 'default'. Default 'cmp'"},
    {"verbosity", OPT_VERBOSITY, 'N',
     "Log level; 3=ERR, 4=WARN, 6=INFO, 7=DEBUG, 8=TRACE. Default 6 = INFO"},

    OPT_SECTION("Generic message"),
    {"cmd", OPT_CMD, 's', "CMP request to send: ir/cr/kur/p10cr/rr/genm"},
    {"infotype", OPT_INFOTYPE, 's',
     "InfoType name for requesting specific info in genm, e.g. 'signKeyPairTypes'"},
    {"geninfo", OPT_GENINFO, 's',
     "generalInfo integer values to place in request PKIHeader with given OID"},
    {OPT_MORE_STR, 0, 0,
     "specified in the form <OID>:int:<n>, e.g. \"1.2.3.4:int:56789\""},

    OPT_SECTION("Certificate enrollment"),
    {"newkey", OPT_NEWKEY, 's',
     "Private or public key for the requested cert. Default: CSR key or client key"},
    {"newkeypass", OPT_NEWKEYPASS, 's', "New private key pass phrase source"},
    {"subject", OPT_SUBJECT, 's',
     "Distinguished Name (DN) of subject to use in the requested cert template"},
    {OPT_MORE_STR, 0, 0,
     "For kur, default is subject of -csr arg or reference cert (see -oldcert)"},
    {OPT_MORE_STR, 0, 0,
     "this default is used for ir and cr only if no Subject Alt Names are set"},
    {"issuer", OPT_ISSUER, 's',
     "DN of the issuer to place in the requested certificate template"},
    {OPT_MORE_STR, 0, 0,
     "also used as recipient if neither -recipient nor -srvcert are given"},
    {"days", OPT_DAYS, 'N',
     "Requested validity time of the new certificate in number of days"},
    {"reqexts", OPT_REQEXTS, 's',
     "Name of config file section defining certificate request extensions."},
    {OPT_MORE_STR, 0, 0,
     "Augments or replaces any extensions contained CSR given with -csr"},
    {"sans", OPT_SANS, 's',
     "Subject Alt Names (IPADDR/DNS/URI) to add as (critical) cert req extension"},
    {"san_nodefault", OPT_SAN_NODEFAULT, '-',
     "Do not take default SANs from reference certificate (see -oldcert)"},
    {"policies", OPT_POLICIES, 's',
     "Name of config file section defining policies certificate request extension"},
    {"policy_oids", OPT_POLICY_OIDS, 's',
     "Policy OID(s) to add as policies certificate request extension"},
    {"policy_oids_critical", OPT_POLICY_OIDS_CRITICAL, '-',
     "Flag the policy OID(s) given with -policy_oids as critical"},
    {"popo", OPT_POPO, 'n',
     "Proof-of-Possession (POPO) method to use for ir/cr/kur where"},
    {OPT_MORE_STR, 0, 0,
     "-1 = NONE, 0 = RAVERIFIED, 1 = SIGNATURE (default), 2 = KEYENC"},
    {"csr", OPT_CSR, 's',
     "PKCS#10 CSR file in PEM or DER format to convert or to use in p10cr"},
    {"out_trusted", OPT_OUT_TRUSTED, 's',
     "Certificates to trust when verifying newly enrolled certificates"},
    {"implicit_confirm", OPT_IMPLICIT_CONFIRM, '-',
     "Request implicit confirmation of newly enrolled certificates"},
    {"disable_confirm", OPT_DISABLE_CONFIRM, '-',
     "Do not confirm newly enrolled certificate w/o requesting implicit"},
    {OPT_MORE_STR, 0, 0,
     "confirmation. WARNING: This leads to behavior violating RFC 4210"},
    {"certout", OPT_CERTOUT, 's',
     "File to save newly enrolled certificate"},
    {"chainout", OPT_CHAINOUT, 's',
     "File to save the chain of newly enrolled certificate"},

    OPT_SECTION("Certificate enrollment and revocation"),

    {"oldcert", OPT_OLDCERT, 's',
     "Certificate to be updated (defaulting to -cert) or to be revoked in rr;"},
    {OPT_MORE_STR, 0, 0,
     "also used as reference (defaulting to -cert) for subject DN and SANs."},
    {OPT_MORE_STR, 0, 0,
     "Issuer is used as recipient unless -recipient, -srvcert, or -issuer given"},
    {"revreason", OPT_REVREASON, 'n',
     "Reason code to include in revocation request (rr); possible values:"},
    {OPT_MORE_STR, 0, 0,
     "0..6, 8..10 (see RFC5280, 5.3.1) or -1. Default -1 = none included"},

    OPT_SECTION("Message transfer"),
#ifdef OPENSSL_NO_SOCK
    {OPT_MORE_STR, 0, 0,
     "NOTE: -server, -proxy, and -no_proxy not supported due to no-sock build"},
#else
    {"server", OPT_SERVER, 's',
     "[http[s]://]address[:port][/path] of CMP server. Default port 80 or 443."},
    {OPT_MORE_STR, 0, 0,
     "address may be a DNS name or an IP address; path can be overridden by -path"},
    {"proxy", OPT_PROXY, 's',
     "[http[s]://]address[:port][/path] of HTTP(S) proxy to use; path is ignored"},
    {"no_proxy", OPT_NO_PROXY, 's',
     "List of addresses of servers not to use HTTP(S) proxy for"},
    {OPT_MORE_STR, 0, 0,
     "Default from environment variable 'no_proxy', else 'NO_PROXY', else none"},
#endif
    {"recipient", OPT_RECIPIENT, 's',
     "DN of CA. Default: subject of -srvcert, -issuer, issuer of -oldcert or -cert"},
    {"path", OPT_PATH, 's',
     "HTTP path (aka CMP alias) at the CMP server. Default from -server, else \"/\""},
    {"keep_alive", OPT_KEEP_ALIVE, 'N',
     "Persistent HTTP connections. 0: no, 1 (the default): request, 2: require"},
    {"msg_timeout", OPT_MSG_TIMEOUT, 'N',
     "Number of seconds allowed per CMP message round trip, or 0 for infinite"},
    {"total_timeout", OPT_TOTAL_TIMEOUT, 'N',
     "Overall time an enrollment incl. polling may take. Default 0 = infinite"},

    OPT_SECTION("Server authentication"),
    {"trusted", OPT_TRUSTED, 's',
     "Certificates to trust as chain roots when verifying signed CMP responses"},
    {OPT_MORE_STR, 0, 0, "unless -srvcert is given"},
    {"untrusted", OPT_UNTRUSTED, 's',
     "Intermediate CA certs for chain construction for CMP/TLS/enrolled certs"},
    {"srvcert", OPT_SRVCERT, 's',
     "Server cert to pin and trust directly when verifying signed CMP responses"},
    {"expect_sender", OPT_EXPECT_SENDER, 's',
     "DN of expected sender of responses. Defaults to subject of -srvcert, if any"},
    {"ignore_keyusage", OPT_IGNORE_KEYUSAGE, '-',
     "Ignore CMP signer cert key usage, else 'digitalSignature' must be allowed"},
    {"unprotected_errors", OPT_UNPROTECTED_ERRORS, '-',
     "Accept missing or invalid protection of regular error messages and negative"},
    {OPT_MORE_STR, 0, 0,
     "certificate responses (ip/cp/kup), revocation responses (rp), and PKIConf"},
    {OPT_MORE_STR, 0, 0,
     "WARNING: This setting leads to behavior allowing violation of RFC 4210"},
    {"extracertsout", OPT_EXTRACERTSOUT, 's',
     "File to save extra certificates received in the extraCerts field"},
    {"cacertsout", OPT_CACERTSOUT, 's',
     "File to save CA certificates received in the caPubs field of 'ip' messages"},

    OPT_SECTION("Client authentication"),
    {"ref", OPT_REF, 's',
     "Reference value to use as senderKID in case no -cert is given"},
    {"secret", OPT_SECRET, 's',
     "Prefer PBM (over signatures) for protecting msgs with given password source"},
    {"cert", OPT_CERT, 's',
     "Client's CMP signer certificate; its public key must match the -key argument"},
    {OPT_MORE_STR, 0, 0,
     "This also used as default reference for subject DN and SANs."},
    {OPT_MORE_STR, 0, 0,
     "Any further certs included are appended to the untrusted certs"},
    {"own_trusted", OPT_OWN_TRUSTED, 's',
     "Optional certs to verify chain building for own CMP signer cert"},
    {"key", OPT_KEY, 's', "CMP signer private key, not used when -secret given"},
    {"keypass", OPT_KEYPASS, 's',
     "Client private key (and cert and old cert) pass phrase source"},
    {"digest", OPT_DIGEST, 's',
     "Digest to use in message protection and POPO signatures. Default \"sha256\""},
    {"mac", OPT_MAC, 's',
     "MAC algorithm to use in PBM-based message protection. Default \"hmac-sha1\""},
    {"extracerts", OPT_EXTRACERTS, 's',
     "Certificates to append in extraCerts field of outgoing messages."},
    {OPT_MORE_STR, 0, 0,
     "This can be used as the default CMP signer cert chain to include"},
    {"unprotected_requests", OPT_UNPROTECTED_REQUESTS, '-',
     "Send messages without CMP-level protection"},

    OPT_SECTION("Credentials format"),
    {"certform", OPT_CERTFORM, 's',
     "Format (PEM or DER) to use when saving a certificate to a file. Default PEM"},
    {"keyform", OPT_KEYFORM, 's',
     "Format of the key input (ENGINE, other values ignored)"},
    {"otherpass", OPT_OTHERPASS, 's',
     "Pass phrase source potentially needed for loading certificates of others"},
#ifndef OPENSSL_NO_ENGINE
    {"engine", OPT_ENGINE, 's',
     "Use crypto engine with given identifier, possibly a hardware device."},
    {OPT_MORE_STR, 0, 0,
     "Engines may also be defined in OpenSSL config file engine section."},
#endif
    OPT_PROV_OPTIONS,
    OPT_R_OPTIONS,

    OPT_SECTION("TLS connection"),
#ifdef OPENSSL_NO_SOCK
    {OPT_MORE_STR, 0, 0,
     "NOTE: -tls_used and all other TLS options not supported due to no-sock build"},
#else
    {"tls_used", OPT_TLS_USED, '-',
     "Enable using TLS (also when other TLS options are not set)"},
    {"tls_cert", OPT_TLS_CERT, 's',
     "Client's TLS certificate. May include chain to be provided to TLS server"},
    {"tls_key", OPT_TLS_KEY, 's',
     "Private key for the client's TLS certificate"},
    {"tls_keypass", OPT_TLS_KEYPASS, 's',
     "Pass phrase source for the client's private TLS key (and TLS cert)"},
    {"tls_extra", OPT_TLS_EXTRA, 's',
     "Extra certificates to provide to TLS server during TLS handshake"},
    {"tls_trusted", OPT_TLS_TRUSTED, 's',
     "Trusted certificates to use for verifying the TLS server certificate;"},
    {OPT_MORE_STR, 0, 0, "this implies host name validation"},
    {"tls_host", OPT_TLS_HOST, 's',
     "Address to be checked (rather than -server) during TLS host name validation"},
#endif

    OPT_SECTION("Client-side debugging"),
    {"batch", OPT_BATCH, '-',
     "Do not interactively prompt for input when a password is required etc."},
    {"repeat", OPT_REPEAT, 'p',
     "Invoke the transaction the given positive number of times. Default 1"},
    {"reqin", OPT_REQIN, 's', "Take sequence of CMP requests from file(s)"},
    {"reqin_new_tid", OPT_REQIN_NEW_TID, '-',
     "Use fresh transactionID for CMP requests read from -reqin"},
    {"reqout", OPT_REQOUT, 's', "Save sequence of CMP requests to file(s)"},
    {"rspin", OPT_RSPIN, 's',
     "Process sequence of CMP responses provided in file(s), skipping server"},
    {"rspout", OPT_RSPOUT, 's', "Save sequence of CMP responses to file(s)"},

    {"use_mock_srv", OPT_USE_MOCK_SRV, '-',
     "Use internal mock server at API level, bypassing socket-based HTTP"},

    OPT_SECTION("Mock server"),
#ifdef OPENSSL_NO_SOCK
    {OPT_MORE_STR, 0, 0,
     "NOTE: -port and -max_msgs not supported due to no-sock build"},
#else
    {"port", OPT_PORT, 's',
     "Act as HTTP-based mock server listening on given port"},
    {"max_msgs", OPT_MAX_MSGS, 'N',
     "max number of messages handled by HTTP mock server. Default: 0 = unlimited"},
#endif

    {"srv_ref", OPT_SRV_REF, 's',
     "Reference value to use as senderKID of server in case no -srv_cert is given"},
    {"srv_secret", OPT_SRV_SECRET, 's',
     "Password source for server authentication with a pre-shared key (secret)"},
    {"srv_cert", OPT_SRV_CERT, 's', "Certificate of the server"},
    {"srv_key", OPT_SRV_KEY, 's',
     "Private key used by the server for signing messages"},
    {"srv_keypass", OPT_SRV_KEYPASS, 's',
     "Server private key (and cert) pass phrase source"},

    {"srv_trusted", OPT_SRV_TRUSTED, 's',
     "Trusted certificates for client authentication"},
    {"srv_untrusted", OPT_SRV_UNTRUSTED, 's',
     "Intermediate certs that may be useful for verifying CMP protection"},
    {"rsp_cert", OPT_RSP_CERT, 's',
     "Certificate to be returned as mock enrollment result"},
    {"rsp_extracerts", OPT_RSP_EXTRACERTS, 's',
     "Extra certificates to be included in mock certification responses"},
    {"rsp_capubs", OPT_RSP_CAPUBS, 's',
     "CA certificates to be included in mock ip response"},
    {"poll_count", OPT_POLL_COUNT, 'N',
     "Number of times the client must poll before receiving a certificate"},
    {"check_after", OPT_CHECK_AFTER, 'N',
     "The check_after value (time to wait) to include in poll response"},
    {"grant_implicitconf", OPT_GRANT_IMPLICITCONF, '-',
     "Grant implicit confirmation of newly enrolled certificate"},

    {"pkistatus", OPT_PKISTATUS, 'N',
     "PKIStatus to be included in server response. Possible values: 0..6"},
    {"failure", OPT_FAILURE, 'N',
     "A single failure info bit number to include in server response, 0..26"},
    {"failurebits", OPT_FAILUREBITS, 'N',
     "Number representing failure bits to include in server response, 0..2^27 - 1"},
    {"statusstring", OPT_STATUSSTRING, 's',
     "Status string to be included in server response"},
    {"send_error", OPT_SEND_ERROR, '-',
     "Force server to reply with error message"},
    {"send_unprotected", OPT_SEND_UNPROTECTED, '-',
     "Send response messages without CMP-level protection"},
    {"send_unprot_err", OPT_SEND_UNPROT_ERR, '-',
     "In case of negative responses, server shall send unprotected error messages,"},
    {OPT_MORE_STR, 0, 0,
     "certificate responses (ip/cp/kup), and revocation responses (rp)."},
    {OPT_MORE_STR, 0, 0,
     "WARNING: This setting leads to behavior violating RFC 4210"},
    {"accept_unprotected", OPT_ACCEPT_UNPROTECTED, '-',
     "Accept missing or invalid protection of requests"},
    {"accept_unprot_err", OPT_ACCEPT_UNPROT_ERR, '-',
     "Accept unprotected error messages from client"},
    {"accept_raverified", OPT_ACCEPT_RAVERIFIED, '-',
     "Accept RAVERIFIED as proof-of-possession (POPO)"},

    OPT_V_OPTIONS,
    {NULL}
};

typedef union {
    char **txt;
    int *num;
    long *num_long;
} varref;
static varref cmp_vars[] = { /* must be in same order as enumerated above! */
    {&opt_config}, {&opt_section}, {(char **)&opt_verbosity},

    {&opt_cmd_s}, {&opt_infotype_s}, {&opt_geninfo},

    {&opt_newkey}, {&opt_newkeypass}, {&opt_subject}, {&opt_issuer},
    {(char **)&opt_days}, {&opt_reqexts},
    {&opt_sans}, {(char **)&opt_san_nodefault},
    {&opt_policies}, {&opt_policy_oids}, {(char **)&opt_policy_oids_critical},
    {(char **)&opt_popo}, {&opt_csr},
    {&opt_out_trusted},
    {(char **)&opt_implicit_confirm}, {(char **)&opt_disable_confirm},
    {&opt_certout}, {&opt_chainout},

    {&opt_oldcert}, {(char **)&opt_revreason},

#ifndef OPENSSL_NO_SOCK
    {&opt_server}, {&opt_proxy}, {&opt_no_proxy},
#endif
    {&opt_recipient}, {&opt_path}, {(char **)&opt_keep_alive},
    {(char **)&opt_msg_timeout}, {(char **)&opt_total_timeout},

    {&opt_trusted}, {&opt_untrusted}, {&opt_srvcert},
    {&opt_expect_sender},
    {(char **)&opt_ignore_keyusage}, {(char **)&opt_unprotected_errors},
    {&opt_extracertsout}, {&opt_cacertsout},

    {&opt_ref}, {&opt_secret},
    {&opt_cert}, {&opt_own_trusted}, {&opt_key}, {&opt_keypass},
    {&opt_digest}, {&opt_mac}, {&opt_extracerts},
    {(char **)&opt_unprotected_requests},

    {&opt_certform_s}, {&opt_keyform_s},
    {&opt_otherpass},
#ifndef OPENSSL_NO_ENGINE
    {&opt_engine},
#endif

#ifndef OPENSSL_NO_SOCK
    {(char **)&opt_tls_used}, {&opt_tls_cert}, {&opt_tls_key},
    {&opt_tls_keypass},
    {&opt_tls_extra}, {&opt_tls_trusted}, {&opt_tls_host},
#endif

    {(char **)&opt_batch}, {(char **)&opt_repeat},
    {&opt_reqin}, {(char **)&opt_reqin_new_tid},
    {&opt_reqout}, {&opt_rspin}, {&opt_rspout},

    {(char **)&opt_use_mock_srv},
#ifndef OPENSSL_NO_SOCK
    {&opt_port}, {(char **)&opt_max_msgs},
#endif
    {&opt_srv_ref}, {&opt_srv_secret},
    {&opt_srv_cert}, {&opt_srv_key}, {&opt_srv_keypass},
    {&opt_srv_trusted}, {&opt_srv_untrusted},
    {&opt_rsp_cert}, {&opt_rsp_extracerts}, {&opt_rsp_capubs},
    {(char **)&opt_poll_count}, {(char **)&opt_check_after},
    {(char **)&opt_grant_implicitconf},
    {(char **)&opt_pkistatus}, {(char **)&opt_failure},
    {(char **)&opt_failurebits}, {&opt_statusstring},
    {(char **)&opt_send_error}, {(char **)&opt_send_unprotected},
    {(char **)&opt_send_unprot_err}, {(char **)&opt_accept_unprotected},
    {(char **)&opt_accept_unprot_err}, {(char **)&opt_accept_raverified},

    {NULL}
};

#define FUNC (strcmp(OPENSSL_FUNC, "(unknown function)") == 0   \
              ? "CMP" : OPENSSL_FUNC)
#define CMP_print(bio, level, prefix, msg, a1, a2, a3) \
    ((void)(level > opt_verbosity ? 0 : \
            (BIO_printf(bio, "%s:%s:%d:CMP %s: " msg "\n", \
                        FUNC, OPENSSL_FILE, OPENSSL_LINE, prefix, a1, a2, a3))))
#define CMP_DEBUG(m, a1, a2, a3) \
    CMP_print(bio_out, OSSL_CMP_LOG_DEBUG, "debug", m, a1, a2, a3)
#define CMP_debug(msg)             CMP_DEBUG(msg"%s%s%s", "", "", "")
#define CMP_debug1(msg, a1)        CMP_DEBUG(msg"%s%s",   a1, "", "")
#define CMP_debug2(msg, a1, a2)    CMP_DEBUG(msg"%s",     a1, a2, "")
#define CMP_debug3(msg, a1, a2, a3) CMP_DEBUG(msg,        a1, a2, a3)
#define CMP_INFO(msg, a1, a2, a3) \
    CMP_print(bio_out, OSSL_CMP_LOG_INFO, "info", msg, a1, a2, a3)
#define CMP_info(msg)              CMP_INFO(msg"%s%s%s", "", "", "")
#define CMP_info1(msg, a1)         CMP_INFO(msg"%s%s",   a1, "", "")
#define CMP_info2(msg, a1, a2)     CMP_INFO(msg"%s",     a1, a2, "")
#define CMP_info3(msg, a1, a2, a3) CMP_INFO(msg,         a1, a2, a3)
#define CMP_WARN(m, a1, a2, a3) \
    CMP_print(bio_out, OSSL_CMP_LOG_WARNING, "warning", m, a1, a2, a3)
#define CMP_warn(msg)              CMP_WARN(msg"%s%s%s", "", "", "")
#define CMP_warn1(msg, a1)         CMP_WARN(msg"%s%s",   a1, "", "")
#define CMP_warn2(msg, a1, a2)     CMP_WARN(msg"%s",     a1, a2, "")
#define CMP_warn3(msg, a1, a2, a3) CMP_WARN(msg,         a1, a2, a3)
#define CMP_ERR(msg, a1, a2, a3) \
    CMP_print(bio_err, OSSL_CMP_LOG_ERR, "error", msg, a1, a2, a3)
#define CMP_err(msg)               CMP_ERR(msg"%s%s%s", "", "", "")
#define CMP_err1(msg, a1)          CMP_ERR(msg"%s%s",   a1, "", "")
#define CMP_err2(msg, a1, a2)      CMP_ERR(msg"%s",     a1, a2, "")
#define CMP_err3(msg, a1, a2, a3)  CMP_ERR(msg,         a1, a2, a3)

static int print_to_bio_out(const char *func, const char *file, int line,
                            OSSL_CMP_severity level, const char *msg)
{
    return OSSL_CMP_print_to_bio(bio_out, func, file, line, level, msg);
}

static int print_to_bio_err(const char *func, const char *file, int line,
                            OSSL_CMP_severity level, const char *msg)
{
    return OSSL_CMP_print_to_bio(bio_err, func, file, line, level, msg);
}

static int set_verbosity(int level)
{
    if (level < OSSL_CMP_LOG_EMERG || level > OSSL_CMP_LOG_MAX) {
        CMP_err1("Logging verbosity level %d out of range (0 .. 8)", level);
        return 0;
    }
    opt_verbosity = level;
    return 1;
}

static EVP_PKEY *load_key_pwd(const char *uri, int format,
                              const char *pass, ENGINE *eng, const char *desc)
{
    char *pass_string = get_passwd(pass, desc);
    EVP_PKEY *pkey = load_key(uri, format, 0, pass_string, eng, desc);

    clear_free(pass_string);
    return pkey;
}

static X509 *load_cert_pwd(const char *uri, const char *pass, const char *desc)
{
    X509 *cert;
    char *pass_string = get_passwd(pass, desc);

    cert = load_cert_pass(uri, FORMAT_UNDEF, 0, pass_string, desc);
    clear_free(pass_string);
    return cert;
}

static X509_REQ *load_csr_autofmt(const char *infile, const char *desc)
{
    X509_REQ *csr;
    BIO *bio_bak = bio_err;

    bio_err = NULL; /* do not show errors on more than one try */
    csr = load_csr(infile, FORMAT_PEM, desc);
    bio_err = bio_bak;
    if (csr == NULL) {
        ERR_clear_error();
        csr = load_csr(infile, FORMAT_ASN1, desc);
    }
    if (csr == NULL) {
        ERR_print_errors(bio_err);
        BIO_printf(bio_err, "error: unable to load %s from file '%s'\n", desc,
                   infile);
    } else {
        EVP_PKEY *pkey = X509_REQ_get0_pubkey(csr);
        int ret = do_X509_REQ_verify(csr, pkey, NULL /* vfyopts */);

        if (pkey == NULL || ret < 0)
            CMP_warn("error while verifying CSR self-signature");
        else if (ret == 0)
            CMP_warn("CSR self-signature does not match the contents");
    }
    return csr;
}

/* set expected host name/IP addr and clears the email addr in the given ts */
static int truststore_set_host_etc(X509_STORE *ts, const char *host)
{
    X509_VERIFY_PARAM *ts_vpm = X509_STORE_get0_param(ts);

    /* first clear any host names, IP, and email addresses */
    if (!X509_VERIFY_PARAM_set1_host(ts_vpm, NULL, 0)
            || !X509_VERIFY_PARAM_set1_ip(ts_vpm, NULL, 0)
            || !X509_VERIFY_PARAM_set1_email(ts_vpm, NULL, 0))
        return 0;
    X509_VERIFY_PARAM_set_hostflags(ts_vpm,
                                    X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT |
                                    X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    return (host != NULL && X509_VERIFY_PARAM_set1_ip_asc(ts_vpm, host))
        || X509_VERIFY_PARAM_set1_host(ts_vpm, host, 0);
}

/* write OSSL_CMP_MSG DER-encoded to the specified file name item */
static int write_PKIMESSAGE(const OSSL_CMP_MSG *msg, char **filenames)
{
    char *file;

    if (msg == NULL || filenames == NULL) {
        CMP_err("NULL arg to write_PKIMESSAGE");
        return 0;
    }
    if (*filenames == NULL) {
        CMP_err("not enough file names provided for writing PKIMessage");
        return 0;
    }

    file = *filenames;
    *filenames = next_item(file);
    if (OSSL_CMP_MSG_write(file, msg) < 0) {
        CMP_err1("cannot write PKIMessage to file '%s'", file);
        return 0;
    }
    return 1;
}

/* read DER-encoded OSSL_CMP_MSG from the specified file name item */
static OSSL_CMP_MSG *read_PKIMESSAGE(char **filenames)
{
    char *file;
    OSSL_CMP_MSG *ret;

    if (filenames == NULL) {
        CMP_err("NULL arg to read_PKIMESSAGE");
        return NULL;
    }
    if (*filenames == NULL) {
        CMP_err("not enough file names provided for reading PKIMessage");
        return NULL;
    }

    file = *filenames;
    *filenames = next_item(file);

    ret = OSSL_CMP_MSG_read(file, app_get0_libctx(), app_get0_propq());
    if (ret == NULL)
        CMP_err1("cannot read PKIMessage from file '%s'", file);
    return ret;
}

/*-
 * Sends the PKIMessage req and on success place the response in *res
 * basically like OSSL_CMP_MSG_http_perform(), but in addition allows
 * to dump the sequence of requests and responses to files and/or
 * to take the sequence of requests and responses from files.
 */
static OSSL_CMP_MSG *read_write_req_resp(OSSL_CMP_CTX *ctx,
                                         const OSSL_CMP_MSG *req)
{
    OSSL_CMP_MSG *req_new = NULL;
    OSSL_CMP_MSG *res = NULL;
    OSSL_CMP_PKIHEADER *hdr;
    const char *prev_opt_rspin = opt_rspin;

    if (req != NULL && opt_reqout != NULL
            && !write_PKIMESSAGE(req, &opt_reqout))
        goto err;
    if (opt_reqin != NULL && opt_rspin == NULL) {
        if ((req_new = read_PKIMESSAGE(&opt_reqin)) == NULL)
            goto err;
        /*-
         * The transaction ID in req_new read from opt_reqin may not be fresh.
         * In this case the server may complain "Transaction id already in use."
         * The following workaround unfortunately requires re-protection.
         */
        if (opt_reqin_new_tid
                && !OSSL_CMP_MSG_update_transactionID(ctx, req_new))
            goto err;
    }

    if (opt_rspin != NULL) {
        res = read_PKIMESSAGE(&opt_rspin);
    } else {
        const OSSL_CMP_MSG *actual_req = opt_reqin != NULL ? req_new : req;

        res = opt_use_mock_srv
            ? OSSL_CMP_CTX_server_perform(ctx, actual_req)
            : OSSL_CMP_MSG_http_perform(ctx, actual_req);
    }
    if (res == NULL)
        goto err;

    if (opt_reqin != NULL || prev_opt_rspin != NULL) {
        /* need to satisfy nonce and transactionID checks */
        ASN1_OCTET_STRING *nonce;
        ASN1_OCTET_STRING *tid;

        hdr = OSSL_CMP_MSG_get0_header(res);
        nonce = OSSL_CMP_HDR_get0_recipNonce(hdr);
        tid = OSSL_CMP_HDR_get0_transactionID(hdr);
        if (!OSSL_CMP_CTX_set1_senderNonce(ctx, nonce)
                || !OSSL_CMP_CTX_set1_transactionID(ctx, tid)) {
            OSSL_CMP_MSG_free(res);
            res = NULL;
            goto err;
        }
    }

    if (opt_rspout != NULL && !write_PKIMESSAGE(res, &opt_rspout)) {
        OSSL_CMP_MSG_free(res);
        res = NULL;
    }

 err:
    OSSL_CMP_MSG_free(req_new);
    return res;
}

static int set_name(const char *str,
                    int (*set_fn) (OSSL_CMP_CTX *ctx, const X509_NAME *name),
                    OSSL_CMP_CTX *ctx, const char *desc)
{
    if (str != NULL) {
        X509_NAME *n = parse_name(str, MBSTRING_ASC, 1, desc);

        if (n == NULL)
            return 0;
        if (!(*set_fn) (ctx, n)) {
            X509_NAME_free(n);
            CMP_err("out of memory");
            return 0;
        }
        X509_NAME_free(n);
    }
    return 1;
}

static int set_gennames(OSSL_CMP_CTX *ctx, char *names, const char *desc)
{
    char *next;

    for (; names != NULL; names = next) {
        GENERAL_NAME *n;

        next = next_item(names);
        if (strcmp(names, "critical") == 0) {
            (void)OSSL_CMP_CTX_set_option(ctx,
                                          OSSL_CMP_OPT_SUBJECTALTNAME_CRITICAL,
                                          1);
            continue;
        }

        /* try IP address first, then URI or domain name */
        (void)ERR_set_mark();
        n = a2i_GENERAL_NAME(NULL, NULL, NULL, GEN_IPADD, names, 0);
        if (n == NULL)
            n = a2i_GENERAL_NAME(NULL, NULL, NULL,
                                 strchr(names, ':') != NULL ? GEN_URI : GEN_DNS,
                                 names, 0);
        (void)ERR_pop_to_mark();

        if (n == NULL) {
            CMP_err2("bad syntax of %s '%s'", desc, names);
            return 0;
        }
        if (!OSSL_CMP_CTX_push1_subjectAltName(ctx, n)) {
            GENERAL_NAME_free(n);
            CMP_err("out of memory");
            return 0;
        }
        GENERAL_NAME_free(n);
    }
    return 1;
}

static X509_STORE *load_trusted(char *input, int for_new_cert, const char *desc)
{
    X509_STORE *ts = load_certstore(input, opt_otherpass, desc, vpm);

    if (ts == NULL)
        return NULL;
    X509_STORE_set_verify_cb(ts, X509_STORE_CTX_print_verify_cb);

    /* copy vpm to store */
    if (X509_STORE_set1_param(ts, vpm /* may be NULL */)
            && (for_new_cert || truststore_set_host_etc(ts, NULL)))
        return ts;
    BIO_printf(bio_err, "error setting verification parameters for %s\n", desc);
    OSSL_CMP_CTX_print_errors(cmp_ctx);
    X509_STORE_free(ts);
    return NULL;
}

typedef int (*add_X509_stack_fn_t)(void *ctx, const STACK_OF(X509) *certs);

static int setup_certs(char *files, const char *desc, void *ctx,
                       add_X509_stack_fn_t set1_fn)
{
    STACK_OF(X509) *certs;
    int ok;

    if (files == NULL)
        return 1;
    if ((certs = load_certs_multifile(files, opt_otherpass, desc, vpm)) == NULL)
        return 0;
    ok = (*set1_fn)(ctx, certs);
    sk_X509_pop_free(certs, X509_free);
    return ok;
}


/*
 * parse and transform some options, checking their syntax.
 * Returns 1 on success, 0 on error
 */
static int transform_opts(void)
{
    if (opt_cmd_s != NULL) {
        if (!strcmp(opt_cmd_s, "ir")) {
            opt_cmd = CMP_IR;
        } else if (!strcmp(opt_cmd_s, "kur")) {
            opt_cmd = CMP_KUR;
        } else if (!strcmp(opt_cmd_s, "cr")) {
            opt_cmd = CMP_CR;
        } else if (!strcmp(opt_cmd_s, "p10cr")) {
            opt_cmd = CMP_P10CR;
        } else if (!strcmp(opt_cmd_s, "rr")) {
            opt_cmd = CMP_RR;
        } else if (!strcmp(opt_cmd_s, "genm")) {
            opt_cmd = CMP_GENM;
        } else {
            CMP_err1("unknown cmp command '%s'", opt_cmd_s);
            return 0;
        }
    } else {
        CMP_err("no cmp command to execute");
        return 0;
    }

#ifndef OPENSSL_NO_ENGINE
# define FORMAT_OPTIONS (OPT_FMT_PEMDER | OPT_FMT_PKCS12 | OPT_FMT_ENGINE)
#else
# define FORMAT_OPTIONS (OPT_FMT_PEMDER | OPT_FMT_PKCS12)
#endif

    if (opt_keyform_s != NULL
            && !opt_format(opt_keyform_s, FORMAT_OPTIONS, &opt_keyform)) {
        CMP_err("unknown option given for key loading format");
        return 0;
    }

#undef FORMAT_OPTIONS

    if (opt_certform_s != NULL
            && !opt_format(opt_certform_s, OPT_FMT_PEMDER, &opt_certform)) {
        CMP_err("unknown option given for certificate storing format");
        return 0;
    }

    return 1;
}

static OSSL_CMP_SRV_CTX *setup_srv_ctx(ENGINE *engine)
{
    OSSL_CMP_CTX *ctx; /* extra CMP (client) ctx partly used by server */
    OSSL_CMP_SRV_CTX *srv_ctx = ossl_cmp_mock_srv_new(app_get0_libctx(),
                                                      app_get0_propq());

    if (srv_ctx == NULL)
        return NULL;
    ctx = OSSL_CMP_SRV_CTX_get0_cmp_ctx(srv_ctx);

    if (opt_srv_ref == NULL) {
        if (opt_srv_cert == NULL) {
            /* opt_srv_cert should determine the sender */
            CMP_err("must give -srv_ref for mock server if no -srv_cert given");
            goto err;
        }
    } else {
        if (!OSSL_CMP_CTX_set1_referenceValue(ctx, (unsigned char *)opt_srv_ref,
                                              strlen(opt_srv_ref)))
            goto err;
    }

    if (opt_srv_secret != NULL) {
        int res;
        char *pass_str = get_passwd(opt_srv_secret, "PBMAC secret of mock server");

        if (pass_str != NULL) {
            cleanse(opt_srv_secret);
            res = OSSL_CMP_CTX_set1_secretValue(ctx, (unsigned char *)pass_str,
                                                strlen(pass_str));
            clear_free(pass_str);
            if (res == 0)
                goto err;
        }
    } else if (opt_srv_cert == NULL) {
        CMP_err("mock server credentials must be given if -use_mock_srv or -port is used");
        goto err;
    } else {
        CMP_warn("mock server will not be able to handle PBM-protected requests since -srv_secret is not given");
    }

    if (opt_srv_secret == NULL
            && ((opt_srv_cert == NULL) != (opt_srv_key == NULL))) {
        CMP_err("must give both -srv_cert and -srv_key options or neither");
        goto err;
    }
    if (opt_srv_cert != NULL) {
        X509 *srv_cert = load_cert_pwd(opt_srv_cert, opt_srv_keypass,
                                       "certificate of the mock server");

        if (srv_cert == NULL || !OSSL_CMP_CTX_set1_cert(ctx, srv_cert)) {
            X509_free(srv_cert);
            goto err;
        }
        X509_free(srv_cert);
    }
    if (opt_srv_key != NULL) {
        EVP_PKEY *pkey = load_key_pwd(opt_srv_key, opt_keyform,
                                      opt_srv_keypass,
                                      engine, "private key for mock server cert");

        if (pkey == NULL || !OSSL_CMP_CTX_set1_pkey(ctx, pkey)) {
            EVP_PKEY_free(pkey);
            goto err;
        }
        EVP_PKEY_free(pkey);
    }
    cleanse(opt_srv_keypass);

    if (opt_srv_trusted != NULL) {
        X509_STORE *ts =
            load_trusted(opt_srv_trusted, 0, "certs trusted by mock server");

        if (ts == NULL || !OSSL_CMP_CTX_set0_trustedStore(ctx, ts)) {
            X509_STORE_free(ts);
            goto err;
        }
    } else {
        CMP_warn("mock server will not be able to handle signature-protected requests since -srv_trusted is not given");
    }
    if (!setup_certs(opt_srv_untrusted,
                     "untrusted certificates for mock server", ctx,
                     (add_X509_stack_fn_t)OSSL_CMP_CTX_set1_untrusted))
        goto err;

    if (opt_rsp_cert == NULL) {
        CMP_warn("no -rsp_cert given for mock server");
    } else {
        X509 *cert = load_cert_pwd(opt_rsp_cert, opt_keypass,
                                   "cert to be returned by the mock server");

        if (cert == NULL)
            goto err;
        /* from server perspective the server is the client */
        if (!ossl_cmp_mock_srv_set1_certOut(srv_ctx, cert)) {
            X509_free(cert);
            goto err;
        }
        X509_free(cert);
    }
    if (!setup_certs(opt_rsp_extracerts,
                     "CMP extra certificates for mock server", srv_ctx,
                     (add_X509_stack_fn_t)ossl_cmp_mock_srv_set1_chainOut))
        goto err;
    if (!setup_certs(opt_rsp_capubs, "caPubs for mock server", srv_ctx,
                     (add_X509_stack_fn_t)ossl_cmp_mock_srv_set1_caPubsOut))
        goto err;
    (void)ossl_cmp_mock_srv_set_pollCount(srv_ctx, opt_poll_count);
    (void)ossl_cmp_mock_srv_set_checkAfterTime(srv_ctx, opt_check_after);
    if (opt_grant_implicitconf)
        (void)OSSL_CMP_SRV_CTX_set_grant_implicit_confirm(srv_ctx, 1);

    if (opt_failure != INT_MIN) { /* option has been set explicity */
        if (opt_failure < 0 || OSSL_CMP_PKIFAILUREINFO_MAX < opt_failure) {
            CMP_err1("-failure out of range, should be >= 0 and <= %d",
                     OSSL_CMP_PKIFAILUREINFO_MAX);
            goto err;
        }
        if (opt_failurebits != 0)
            CMP_warn("-failurebits overrides -failure");
        else
            opt_failurebits = 1 << opt_failure;
    }
    if ((unsigned)opt_failurebits > OSSL_CMP_PKIFAILUREINFO_MAX_BIT_PATTERN) {
        CMP_err("-failurebits out of range");
        goto err;
    }
    if (!ossl_cmp_mock_srv_set_statusInfo(srv_ctx, opt_pkistatus,
                                          opt_failurebits, opt_statusstring))
        goto err;

    if (opt_send_error)
        (void)ossl_cmp_mock_srv_set_send_error(srv_ctx, 1);

    if (opt_send_unprotected)
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_UNPROTECTED_SEND, 1);
    if (opt_send_unprot_err)
        (void)OSSL_CMP_SRV_CTX_set_send_unprotected_errors(srv_ctx, 1);
    if (opt_accept_unprotected)
        (void)OSSL_CMP_SRV_CTX_set_accept_unprotected(srv_ctx, 1);
    if (opt_accept_unprot_err)
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_UNPROTECTED_ERRORS, 1);
    if (opt_accept_raverified)
        (void)OSSL_CMP_SRV_CTX_set_accept_raverified(srv_ctx, 1);

    return srv_ctx;

 err:
    ossl_cmp_mock_srv_free(srv_ctx);
    return NULL;
}

/*
 * set up verification aspects of OSSL_CMP_CTX w.r.t. opts from config file/CLI.
 * Returns pointer on success, NULL on error
 */
static int setup_verification_ctx(OSSL_CMP_CTX *ctx)
{
    if (!setup_certs(opt_untrusted, "untrusted certificates", ctx,
                     (add_X509_stack_fn_t)OSSL_CMP_CTX_set1_untrusted))
        return 0;

    if (opt_srvcert != NULL || opt_trusted != NULL) {
        X509 *srvcert;
        X509_STORE *ts;
        int ok;

        if (opt_srvcert != NULL) {
            if (opt_trusted != NULL) {
                CMP_warn("-trusted option is ignored since -srvcert option is present");
                opt_trusted = NULL;
            }
            if (opt_recipient != NULL) {
                CMP_warn("-recipient option is ignored since -srvcert option is present");
                opt_recipient = NULL;
            }
            srvcert = load_cert_pwd(opt_srvcert, opt_otherpass,
                                    "directly trusted CMP server certificate");
            ok = srvcert != NULL && OSSL_CMP_CTX_set1_srvCert(ctx, srvcert);
            X509_free(srvcert);
            if (!ok)
                return 0;
        }
        if (opt_trusted != NULL) {
            /*
             * the 0 arg below clears any expected host/ip/email address;
             * opt_expect_sender is used instead
             */
            ts = load_trusted(opt_trusted, 0, "certs trusted by client");

            if (ts == NULL || !OSSL_CMP_CTX_set0_trustedStore(ctx, ts)) {
                X509_STORE_free(ts);
                return 0;
            }
        }
    }

    if (opt_ignore_keyusage)
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_IGNORE_KEYUSAGE, 1);

    if (opt_unprotected_errors)
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_UNPROTECTED_ERRORS, 1);

    if (opt_out_trusted != NULL) { /* for use in OSSL_CMP_certConf_cb() */
        X509_VERIFY_PARAM *out_vpm = NULL;
        X509_STORE *out_trusted =
            load_trusted(opt_out_trusted, 1,
                         "trusted certs for verifying newly enrolled cert");

        if (out_trusted == NULL)
            return 0;
        /* ignore any -attime here, new certs are current anyway */
        out_vpm = X509_STORE_get0_param(out_trusted);
        X509_VERIFY_PARAM_clear_flags(out_vpm, X509_V_FLAG_USE_CHECK_TIME);

        (void)OSSL_CMP_CTX_set_certConf_cb_arg(ctx, out_trusted);
    }

    if (opt_disable_confirm)
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_DISABLE_CONFIRM, 1);

    if (opt_implicit_confirm)
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_IMPLICIT_CONFIRM, 1);

    return 1;
}

#ifndef OPENSSL_NO_SOCK
/*
 * set up ssl_ctx for the OSSL_CMP_CTX based on options from config file/CLI.
 * Returns pointer on success, NULL on error
 */
static SSL_CTX *setup_ssl_ctx(OSSL_CMP_CTX *ctx, const char *host,
                              ENGINE *engine)
{
    STACK_OF(X509) *untrusted = OSSL_CMP_CTX_get0_untrusted(ctx);
    EVP_PKEY *pkey = NULL;
    X509_STORE *trust_store = NULL;
    SSL_CTX *ssl_ctx;
    int i;

    ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (ssl_ctx == NULL)
        return NULL;

    if (opt_tls_trusted != NULL) {
        trust_store = load_trusted(opt_tls_trusted, 0, "trusted TLS certs");
        if (trust_store == NULL)
            goto err;
        SSL_CTX_set_cert_store(ssl_ctx, trust_store);
    }

    if (opt_tls_cert != NULL && opt_tls_key != NULL) {
        X509 *cert;
        STACK_OF(X509) *certs = NULL;
        int ok;

        if (!load_cert_certs(opt_tls_cert, &cert, &certs, 0, opt_tls_keypass,
                             "TLS client certificate (optionally with chain)",
                             vpm))
            /* need opt_tls_keypass if opt_tls_cert is encrypted PKCS#12 file */
            goto err;

        ok = SSL_CTX_use_certificate(ssl_ctx, cert) > 0;
        X509_free(cert);

        /*
         * Any further certs and any untrusted certs are used for constructing
         * the chain to be provided with the TLS client cert to the TLS server.
         */
        if (!ok || !SSL_CTX_set0_chain(ssl_ctx, certs)) {
            CMP_err1("unable to use client TLS certificate file '%s'",
                     opt_tls_cert);
            sk_X509_pop_free(certs, X509_free);
            goto err;
        }
        for (i = 0; i < sk_X509_num(untrusted); i++) {
            cert = sk_X509_value(untrusted, i);
            if (!SSL_CTX_add1_chain_cert(ssl_ctx, cert)) {
                CMP_err("could not add untrusted cert to TLS client cert chain");
                goto err;
            }
        }

        {
            X509_VERIFY_PARAM *tls_vpm = NULL;
            unsigned long bak_flags = 0; /* compiler warns without init */

            if (trust_store != NULL) {
                tls_vpm = X509_STORE_get0_param(trust_store);
                bak_flags = X509_VERIFY_PARAM_get_flags(tls_vpm);
                /* disable any cert status/revocation checking etc. */
                X509_VERIFY_PARAM_clear_flags(tls_vpm,
                                              ~(X509_V_FLAG_USE_CHECK_TIME
                                                | X509_V_FLAG_NO_CHECK_TIME));
            }
            CMP_debug("trying to build cert chain for own TLS cert");
            if (SSL_CTX_build_cert_chain(ssl_ctx,
                                         SSL_BUILD_CHAIN_FLAG_UNTRUSTED |
                                         SSL_BUILD_CHAIN_FLAG_NO_ROOT)) {
                CMP_debug("success building cert chain for own TLS cert");
            } else {
                OSSL_CMP_CTX_print_errors(ctx);
                CMP_warn("could not build cert chain for own TLS cert");
            }
            if (trust_store != NULL)
                X509_VERIFY_PARAM_set_flags(tls_vpm, bak_flags);
        }

        /* If present we append to the list also the certs from opt_tls_extra */
        if (opt_tls_extra != NULL) {
            STACK_OF(X509) *tls_extra = load_certs_multifile(opt_tls_extra,
                                                             opt_otherpass,
                                                             "extra certificates for TLS",
                                                             vpm);
            int res = 1;

            if (tls_extra == NULL)
                goto err;
            for (i = 0; i < sk_X509_num(tls_extra); i++) {
                cert = sk_X509_value(tls_extra, i);
                if (res != 0)
                    res = SSL_CTX_add_extra_chain_cert(ssl_ctx, cert);
                if (res == 0)
                    X509_free(cert);
            }
            sk_X509_free(tls_extra);
            if (res == 0) {
                BIO_printf(bio_err, "error: unable to add TLS extra certs\n");
                goto err;
            }
        }

        pkey = load_key_pwd(opt_tls_key, opt_keyform, opt_tls_keypass,
                            engine, "TLS client private key");
        cleanse(opt_tls_keypass);
        if (pkey == NULL)
            goto err;
        /*
         * verify the key matches the cert,
         * not using SSL_CTX_check_private_key(ssl_ctx)
         * because it gives poor and sometimes misleading diagnostics
         */
        if (!X509_check_private_key(SSL_CTX_get0_certificate(ssl_ctx),
                                    pkey)) {
            CMP_err2("TLS private key '%s' does not match the TLS certificate '%s'\n",
                     opt_tls_key, opt_tls_cert);
            EVP_PKEY_free(pkey);
            pkey = NULL; /* otherwise, for some reason double free! */
            goto err;
        }
        if (SSL_CTX_use_PrivateKey(ssl_ctx, pkey) <= 0) {
            CMP_err1("unable to use TLS client private key '%s'", opt_tls_key);
            EVP_PKEY_free(pkey);
            pkey = NULL; /* otherwise, for some reason double free! */
            goto err;
        }
        EVP_PKEY_free(pkey); /* we do not need the handle any more */
    }
    if (opt_tls_trusted != NULL) {
        /* enable and parameterize server hostname/IP address check */
        if (!truststore_set_host_etc(trust_store,
                                     opt_tls_host != NULL ? opt_tls_host : host))
            goto err;
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
    }
    return ssl_ctx;
 err:
    SSL_CTX_free(ssl_ctx);
    return NULL;
}
#endif /* OPENSSL_NO_SOCK */

/*
 * set up protection aspects of OSSL_CMP_CTX based on options from config
 * file/CLI while parsing options and checking their consistency.
 * Returns 1 on success, 0 on error
 */
static int setup_protection_ctx(OSSL_CMP_CTX *ctx, ENGINE *engine)
{
    if (!opt_unprotected_requests && opt_secret == NULL && opt_key == NULL) {
        CMP_err("must give -key or -secret unless -unprotected_requests is used");
        return 0;
    }

    if (opt_ref == NULL && opt_cert == NULL && opt_subject == NULL) {
        /* cert or subject should determine the sender */
        CMP_err("must give -ref if no -cert and no -subject given");
        return 0;
    }
    if (!opt_secret && ((opt_cert == NULL) != (opt_key == NULL))) {
        CMP_err("must give both -cert and -key options or neither");
        return 0;
    }
    if (opt_secret != NULL) {
        char *pass_string = get_passwd(opt_secret, "PBMAC");
        int res;

        if (pass_string != NULL) {
            cleanse(opt_secret);
            res = OSSL_CMP_CTX_set1_secretValue(ctx,
                                                (unsigned char *)pass_string,
                                                strlen(pass_string));
            clear_free(pass_string);
            if (res == 0)
                return 0;
        }
        if (opt_cert != NULL || opt_key != NULL)
            CMP_warn("-cert and -key not used for protection since -secret is given");
    }
    if (opt_ref != NULL
            && !OSSL_CMP_CTX_set1_referenceValue(ctx, (unsigned char *)opt_ref,
                                                 strlen(opt_ref)))
        return 0;

    if (opt_key != NULL) {
        EVP_PKEY *pkey = load_key_pwd(opt_key, opt_keyform, opt_keypass, engine,
                                      "private key for CMP client certificate");

        if (pkey == NULL || !OSSL_CMP_CTX_set1_pkey(ctx, pkey)) {
            EVP_PKEY_free(pkey);
            return 0;
        }
        EVP_PKEY_free(pkey);
    }
    if (opt_secret == NULL && opt_srvcert == NULL && opt_trusted == NULL)
        CMP_warn("will not authenticate server due to missing -secret, -trusted, or -srvcert");

    if (opt_cert != NULL) {
        X509 *cert;
        STACK_OF(X509) *certs = NULL;
        X509_STORE *own_trusted = NULL;
        int ok;

        if (!load_cert_certs(opt_cert, &cert, &certs, 0, opt_keypass,
                             "CMP client certificate (optionally with chain)",
                             vpm))
            /* opt_keypass is needed if opt_cert is an encrypted PKCS#12 file */
            return 0;
        ok = OSSL_CMP_CTX_set1_cert(ctx, cert);
        X509_free(cert);
        if (!ok) {
            CMP_err("out of memory");
        } else {
            if (opt_own_trusted != NULL) {
                own_trusted = load_trusted(opt_own_trusted, 0,
                                           "trusted certs for verifying own CMP signer cert");
                ok = own_trusted != NULL;
            }
            ok = ok && OSSL_CMP_CTX_build_cert_chain(ctx, own_trusted, certs);
        }
        X509_STORE_free(own_trusted);
        sk_X509_pop_free(certs, X509_free);
        if (!ok)
            return 0;
    } else if (opt_own_trusted != NULL) {
        CMP_warn("-own_trusted option is ignored without -cert");
    }

    if (!setup_certs(opt_extracerts, "extra certificates for CMP", ctx,
                     (add_X509_stack_fn_t)OSSL_CMP_CTX_set1_extraCertsOut))
        return 0;
    cleanse(opt_otherpass);

    if (opt_unprotected_requests)
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_UNPROTECTED_SEND, 1);

    if (opt_digest != NULL) {
        int digest = OBJ_ln2nid(opt_digest);

        if (digest == NID_undef) {
            CMP_err1("digest algorithm name not recognized: '%s'", opt_digest);
            return 0;
        }
        if (!OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_DIGEST_ALGNID, digest)
            || !OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_OWF_ALGNID, digest)) {
            CMP_err1("digest algorithm name not supported: '%s'", opt_digest);
            return 0;
        }
    }

    if (opt_mac != NULL) {
        int mac = OBJ_ln2nid(opt_mac);
        if (mac == NID_undef) {
            CMP_err1("MAC algorithm name not recognized: '%s'", opt_mac);
            return 0;
        }
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_MAC_ALGNID, mac);
    }
    return 1;
}

/*
 * set up IR/CR/KUR/CertConf/RR specific parts of the OSSL_CMP_CTX
 * based on options from config file/CLI.
 * Returns pointer on success, NULL on error
 */
static int setup_request_ctx(OSSL_CMP_CTX *ctx, ENGINE *engine)
{
    X509_REQ *csr = NULL;
    X509_EXTENSIONS *exts = NULL;
    X509V3_CTX ext_ctx;

    if (opt_subject == NULL
            && opt_csr == NULL && opt_oldcert == NULL && opt_cert == NULL
            && opt_cmd != CMP_RR && opt_cmd != CMP_GENM)
        CMP_warn("no -subject given; no -csr or -oldcert or -cert available for fallback");

    if (opt_cmd == CMP_IR || opt_cmd == CMP_CR || opt_cmd == CMP_KUR) {
        if (opt_newkey == NULL && opt_key == NULL && opt_csr == NULL) {
            CMP_err("missing -newkey (or -key) to be certified and no -csr given");
            return 0;
        }
        if (opt_certout == NULL) {
            CMP_err("-certout not given, nowhere to save newly enrolled certificate");
            return 0;
        }
        if (!set_name(opt_subject, OSSL_CMP_CTX_set1_subjectName, ctx, "subject")
                || !set_name(opt_issuer, OSSL_CMP_CTX_set1_issuer, ctx, "issuer"))
            return 0;
    } else {
        const char *msg = "option is ignored for commands other than 'ir', 'cr', and 'kur'";

        if (opt_subject != NULL) {
            if (opt_ref == NULL && opt_cert == NULL) {
                /* use subject as default sender unless oldcert subject is used */
                if (!set_name(opt_subject, OSSL_CMP_CTX_set1_subjectName, ctx, "subject"))
                    return 0;
            } else {
                CMP_warn1("-subject %s since -ref or -cert is given", msg);
            }
        }
        if (opt_issuer != NULL)
            CMP_warn1("-issuer %s", msg);
        if (opt_reqexts != NULL)
            CMP_warn1("-reqexts %s", msg);
        if (opt_san_nodefault)
            CMP_warn1("-san_nodefault %s", msg);
        if (opt_sans != NULL)
            CMP_warn1("-sans %s", msg);
        if (opt_policies != NULL)
            CMP_warn1("-policies %s", msg);
        if (opt_policy_oids != NULL)
            CMP_warn1("-policy_oids %s", msg);
    }
    if (opt_cmd == CMP_KUR) {
        char *ref_cert = opt_oldcert != NULL ? opt_oldcert : opt_cert;

        if (ref_cert == NULL && opt_csr == NULL) {
            CMP_err("missing -oldcert for certificate to be updated and no -csr given");
            return 0;
        }
        if (opt_subject != NULL)
            CMP_warn2("given -subject '%s' overrides the subject of '%s' for KUR",
                      opt_subject, ref_cert != NULL ? ref_cert : opt_csr);
    }
    if (opt_cmd == CMP_RR) {
        if (opt_oldcert == NULL && opt_csr == NULL) {
            CMP_err("missing -oldcert for certificate to be revoked and no -csr given");
            return 0;
        }
        if (opt_oldcert != NULL && opt_csr != NULL)
            CMP_warn("ignoring -csr since certificate to be revoked is given");
    }
    if (opt_cmd == CMP_P10CR && opt_csr == NULL) {
        CMP_err("missing PKCS#10 CSR for p10cr");
        return 0;
    }

    if (opt_recipient == NULL && opt_srvcert == NULL && opt_issuer == NULL
            && opt_oldcert == NULL && opt_cert == NULL)
        CMP_warn("missing -recipient, -srvcert, -issuer, -oldcert or -cert; recipient will be set to \"NULL-DN\"");

    if (opt_cmd == CMP_P10CR || opt_cmd == CMP_RR) {
        const char *msg = "option is ignored for 'p10cr' and 'rr' commands";

        if (opt_newkeypass != NULL)
            CMP_warn1("-newkeytype %s", msg);
        if (opt_newkey != NULL)
            CMP_warn1("-newkey %s", msg);
        if (opt_days != 0)
            CMP_warn1("-days %s", msg);
        if (opt_popo != OSSL_CRMF_POPO_NONE - 1)
            CMP_warn1("-popo %s", msg);
    } else if (opt_newkey != NULL) {
        const char *file = opt_newkey;
        const int format = opt_keyform;
        const char *pass = opt_newkeypass;
        const char *desc = "new private key for cert to be enrolled";
        EVP_PKEY *pkey;
        int priv = 1;
        BIO *bio_bak = bio_err;

        bio_err = NULL; /* suppress diagnostics on first try loading key */
        pkey = load_key_pwd(file, format, pass, engine, desc);
        bio_err = bio_bak;
        if (pkey == NULL) {
            ERR_clear_error();
            desc = opt_csr == NULL
            ? "fallback public key for cert to be enrolled"
            : "public key for checking cert resulting from p10cr";
            pkey = load_pubkey(file, format, 0, pass, engine, desc);
            priv = 0;
        }
        cleanse(opt_newkeypass);
        if (pkey == NULL || !OSSL_CMP_CTX_set0_newPkey(ctx, priv, pkey)) {
            EVP_PKEY_free(pkey);
            return 0;
        }
    }

    if (opt_days > 0
            && !OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_VALIDITY_DAYS,
                                        opt_days)) {
        CMP_err("could not set requested cert validity period");
        return 0;
    }

    if (opt_policies != NULL && opt_policy_oids != NULL) {
        CMP_err("cannot have policies both via -policies and via -policy_oids");
        return 0;
    }

    if (opt_csr != NULL) {
        if (opt_cmd == CMP_GENM) {
            CMP_warn("-csr option is ignored for command 'genm'");
        } else {
            if ((csr = load_csr_autofmt(opt_csr, "PKCS#10 CSR")) == NULL)
                return 0;
            if (!OSSL_CMP_CTX_set1_p10CSR(ctx, csr))
                goto oom;
        }
    }
    if (opt_reqexts != NULL || opt_policies != NULL) {
        if ((exts = sk_X509_EXTENSION_new_null()) == NULL)
            goto oom;
        X509V3_set_ctx(&ext_ctx, NULL, NULL, csr, NULL, X509V3_CTX_REPLACE);
        X509V3_set_nconf(&ext_ctx, conf);
        if (opt_reqexts != NULL
            && !X509V3_EXT_add_nconf_sk(conf, &ext_ctx, opt_reqexts, &exts)) {
            CMP_err1("cannot load certificate request extension section '%s'",
                     opt_reqexts);
            goto exts_err;
        }
        if (opt_policies != NULL
            && !X509V3_EXT_add_nconf_sk(conf, &ext_ctx, opt_policies, &exts)) {
            CMP_err1("cannot load policy cert request extension section '%s'",
                     opt_policies);
            goto exts_err;
        }
        OSSL_CMP_CTX_set0_reqExtensions(ctx, exts);
    }
    X509_REQ_free(csr);
    /* After here, must not goto oom/exts_err */

    if (OSSL_CMP_CTX_reqExtensions_have_SAN(ctx) && opt_sans != NULL) {
        CMP_err("cannot have Subject Alternative Names both via -reqexts and via -sans");
        return 0;
    }
    if (!set_gennames(ctx, opt_sans, "Subject Alternative Name"))
        return 0;

    if (opt_san_nodefault) {
        if (opt_sans != NULL)
            CMP_warn("-opt_san_nodefault has no effect when -sans is used");
        (void)OSSL_CMP_CTX_set_option(ctx,
                                      OSSL_CMP_OPT_SUBJECTALTNAME_NODEFAULT, 1);
    }

    if (opt_policy_oids_critical) {
        if (opt_policy_oids == NULL)
            CMP_warn("-opt_policy_oids_critical has no effect unless -policy_oids is given");
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_POLICIES_CRITICAL, 1);
    }

    while (opt_policy_oids != NULL) {
        ASN1_OBJECT *policy;
        POLICYINFO *pinfo;
        char *next = next_item(opt_policy_oids);

        if ((policy = OBJ_txt2obj(opt_policy_oids, 1)) == 0) {
            CMP_err1("unknown policy OID '%s'", opt_policy_oids);
            return 0;
        }

        if ((pinfo = POLICYINFO_new()) == NULL) {
            ASN1_OBJECT_free(policy);
            return 0;
        }
        pinfo->policyid = policy;

        if (!OSSL_CMP_CTX_push0_policy(ctx, pinfo)) {
            CMP_err1("cannot add policy with OID '%s'", opt_policy_oids);
            POLICYINFO_free(pinfo);
            return 0;
        }
        opt_policy_oids = next;
    }

    if (opt_popo >= OSSL_CRMF_POPO_NONE)
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_POPO_METHOD, opt_popo);

    if (opt_oldcert != NULL) {
        if (opt_cmd == CMP_GENM) {
            CMP_warn("-oldcert option is ignored for command 'genm'");
        } else {
            X509 *oldcert = load_cert_pwd(opt_oldcert, opt_keypass,
                                          opt_cmd == CMP_KUR ?
                                          "certificate to be updated" :
                                          opt_cmd == CMP_RR ?
                                          "certificate to be revoked" :
                                          "reference certificate (oldcert)");
            /* opt_keypass needed if opt_oldcert is an encrypted PKCS#12 file */

            if (oldcert == NULL)
                return 0;
            if (!OSSL_CMP_CTX_set1_oldCert(ctx, oldcert)) {
                X509_free(oldcert);
                CMP_err("out of memory");
                return 0;
            }
            X509_free(oldcert);
        }
    }
    cleanse(opt_keypass);
    if (opt_revreason > CRL_REASON_NONE)
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_REVOCATION_REASON,
                                      opt_revreason);

    return 1;

 oom:
    CMP_err("out of memory");
 exts_err:
    sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);
    X509_REQ_free(csr);
    return 0;
}

static int handle_opt_geninfo(OSSL_CMP_CTX *ctx)
{
    long value;
    ASN1_OBJECT *type;
    ASN1_INTEGER *aint;
    ASN1_TYPE *val;
    OSSL_CMP_ITAV *itav;
    char *endstr;
    char *valptr = strchr(opt_geninfo, ':');

    if (valptr == NULL) {
        CMP_err("missing ':' in -geninfo option");
        return 0;
    }
    valptr[0] = '\0';
    valptr++;

    if (strncasecmp(valptr, "int:", 4) != 0) {
        CMP_err("missing 'int:' in -geninfo option");
        return 0;
    }
    valptr += 4;

    value = strtol(valptr, &endstr, 10);
    if (endstr == valptr || *endstr != '\0') {
        CMP_err("cannot parse int in -geninfo option");
        return 0;
    }

    type = OBJ_txt2obj(opt_geninfo, 1);
    if (type == NULL) {
        CMP_err("cannot parse OID in -geninfo option");
        return 0;
    }

    if ((aint = ASN1_INTEGER_new()) == NULL)
        goto oom;

    val = ASN1_TYPE_new();
    if (!ASN1_INTEGER_set(aint, value) || val == NULL) {
        ASN1_INTEGER_free(aint);
        goto oom;
    }
    ASN1_TYPE_set(val, V_ASN1_INTEGER, aint);
    itav = OSSL_CMP_ITAV_create(type, val);
    if (itav == NULL) {
        ASN1_TYPE_free(val);
        goto oom;
    }

    if (!OSSL_CMP_CTX_push0_geninfo_ITAV(ctx, itav)) {
        OSSL_CMP_ITAV_free(itav);
        return 0;
    }
    return 1;

 oom:
    ASN1_OBJECT_free(type);
    CMP_err("out of memory");
    return 0;
}


/*
 * set up the client-side OSSL_CMP_CTX based on options from config file/CLI
 * while parsing options and checking their consistency.
 * Prints reason for error to bio_err.
 * Returns 1 on success, 0 on error
 */
static int setup_client_ctx(OSSL_CMP_CTX *ctx, ENGINE *engine)
{
    int ret = 0;
    char *host = NULL, *port = NULL, *path = NULL, *used_path = opt_path;
#ifndef OPENSSL_NO_SOCK
    int portnum, ssl;
    static char server_port[32] = { '\0' };
    const char *proxy_host = NULL;
#endif
    char server_buf[200] = "mock server";
    char proxy_buf[200] = "";

    if (!opt_use_mock_srv && opt_rspin == NULL) { /* note: -port is not given */
#ifndef OPENSSL_NO_SOCK
        if (opt_server == NULL) {
            CMP_err("missing -server or -use_mock_srv or -rspin option");
            goto err;
        }
#else
        CMP_err("missing -use_mock_srv or -rspin option; -server option is not supported due to no-sock build");
        goto err;
#endif
    }
#ifndef OPENSSL_NO_SOCK
    if (opt_server == NULL) {
        if (opt_proxy != NULL)
            CMP_warn("ignoring -proxy option since -server is not given");
        if (opt_no_proxy != NULL)
            CMP_warn("ignoring -no_proxy option since -server is not given");
        if (opt_tls_used) {
            CMP_warn("ignoring -tls_used option since -server is not given");
            opt_tls_used = 0;
        }
        goto set_path;
    }
    if (!OSSL_HTTP_parse_url(opt_server, &ssl, NULL /* user */, &host, &port,
                             &portnum, &path, NULL /* q */, NULL /* frag */)) {
        CMP_err1("cannot parse -server URL: %s", opt_server);
        goto err;
    }
    if (ssl && !opt_tls_used) {
        CMP_err("missing -tls_used option since -server URL indicates https");
        goto err;
    }

    BIO_snprintf(server_port, sizeof(server_port), "%s", port);
    if (opt_path == NULL)
        used_path = path;
    if (!OSSL_CMP_CTX_set1_server(ctx, host)
            || !OSSL_CMP_CTX_set_serverPort(ctx, portnum))
        goto oom;
    if (opt_proxy != NULL && !OSSL_CMP_CTX_set1_proxy(ctx, opt_proxy))
        goto oom;
    if (opt_no_proxy != NULL && !OSSL_CMP_CTX_set1_no_proxy(ctx, opt_no_proxy))
        goto oom;
    (void)BIO_snprintf(server_buf, sizeof(server_buf), "http%s://%s:%s/%s",
                       opt_tls_used ? "s" : "", host, port,
                       *used_path == '/' ? used_path + 1 : used_path);

    proxy_host = OSSL_HTTP_adapt_proxy(opt_proxy, opt_no_proxy, host, ssl);
    if (proxy_host != NULL)
        (void)BIO_snprintf(proxy_buf, sizeof(proxy_buf), " via %s", proxy_host);

 set_path:
#endif

    if (!OSSL_CMP_CTX_set1_serverPath(ctx, used_path))
        goto oom;
    if (!transform_opts())
        goto err;

    if (opt_infotype_s != NULL) {
        char id_buf[100] = "id-it-";

        strncat(id_buf, opt_infotype_s, sizeof(id_buf) - strlen(id_buf) - 1);
        if ((opt_infotype = OBJ_sn2nid(id_buf)) == NID_undef) {
            CMP_err("unknown OID name in -infotype option");
            goto err;
        }
    }

    if (!setup_verification_ctx(ctx))
        goto err;

    if (opt_keep_alive != 1)
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_KEEP_ALIVE,
                                      opt_keep_alive);
    if (opt_total_timeout > 0 && opt_msg_timeout > 0
            && opt_total_timeout < opt_msg_timeout) {
        CMP_err2("-total_timeout argument = %d must not be < %d (-msg_timeout)",
                 opt_total_timeout, opt_msg_timeout);
        goto err;
    }
    if (opt_msg_timeout >= 0) /* must do this before setup_ssl_ctx() */
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_MSG_TIMEOUT,
                                      opt_msg_timeout);
    if (opt_total_timeout >= 0)
        (void)OSSL_CMP_CTX_set_option(ctx, OSSL_CMP_OPT_TOTAL_TIMEOUT,
                                      opt_total_timeout);

    if (opt_reqin != NULL && opt_rspin != NULL)
        CMP_warn("-reqin is ignored since -rspin is present");
    if (opt_reqin_new_tid && opt_reqin == NULL)
        CMP_warn("-reqin_new_tid is ignored since -reqin is not present");
    if (opt_reqin != NULL || opt_reqout != NULL
            || opt_rspin != NULL || opt_rspout != NULL || opt_use_mock_srv)
        (void)OSSL_CMP_CTX_set_transfer_cb(ctx, read_write_req_resp);

#ifndef OPENSSL_NO_SOCK
    if (opt_tls_used) {
        APP_HTTP_TLS_INFO *info;

        if (opt_tls_cert != NULL
            || opt_tls_key != NULL || opt_tls_keypass != NULL) {
            if (opt_tls_key == NULL) {
                CMP_err("missing -tls_key option");
                goto err;
            } else if (opt_tls_cert == NULL) {
                CMP_err("missing -tls_cert option");
                goto err;
            }
        }

        if ((info = OPENSSL_zalloc(sizeof(*info))) == NULL)
            goto err;
        (void)OSSL_CMP_CTX_set_http_cb_arg(ctx, info);
        /* info will be freed along with CMP ctx */
        info->server = opt_server;
        info->port = server_port;
        /* workaround for callback design flaw, see #17088: */
        info->use_proxy = proxy_host != NULL;
        info->timeout = OSSL_CMP_CTX_get_option(ctx, OSSL_CMP_OPT_MSG_TIMEOUT);
        info->ssl_ctx = setup_ssl_ctx(ctx, host, engine);

        if (info->ssl_ctx == NULL)
            goto err;
        (void)OSSL_CMP_CTX_set_http_cb(ctx, app_http_tls_cb);
    }
#endif

    if (!setup_protection_ctx(ctx, engine))
        goto err;

    if (!setup_request_ctx(ctx, engine))
        goto err;

    if (!set_name(opt_recipient, OSSL_CMP_CTX_set1_recipient, ctx, "recipient")
            || !set_name(opt_expect_sender, OSSL_CMP_CTX_set1_expected_sender,
                         ctx, "expected sender"))
        goto err;

    if (opt_geninfo != NULL && !handle_opt_geninfo(ctx))
        goto err;

    /* not printing earlier, to minimize confusion in case setup fails before */
    if (opt_rspin != NULL)
        CMP_info("will not contact any server since -rspin is given");
    else
        CMP_info2("will contact %s%s", server_buf, proxy_buf);

    ret = 1;

 err:
    OPENSSL_free(host);
    OPENSSL_free(port);
    OPENSSL_free(path);
    return ret;
 oom:
    CMP_err("out of memory");
    goto err;
}

/*
 * write out the given certificate to the output specified by bio.
 * Depending on options use either PEM or DER format.
 * Returns 1 on success, 0 on error
 */
static int write_cert(BIO *bio, X509 *cert)
{
    if ((opt_certform == FORMAT_PEM && PEM_write_bio_X509(bio, cert))
            || (opt_certform == FORMAT_ASN1 && i2d_X509_bio(bio, cert)))
        return 1;
    if (opt_certform != FORMAT_PEM && opt_certform != FORMAT_ASN1)
        BIO_printf(bio_err,
                   "error: unsupported type '%s' for writing certificates\n",
                   opt_certform_s);
    return 0;
}

/*
 * If destFile != NULL writes out a stack of certs to the given file.
 * In any case frees the certs.
 * Depending on options use either PEM or DER format,
 * where DER does not make much sense for writing more than one cert!
 * Returns number of written certificates on success, -1 on error.
 */
static int save_free_certs(OSSL_CMP_CTX *ctx,
                           STACK_OF(X509) *certs, char *destFile, char *desc)
{
    BIO *bio = NULL;
    int i;
    int n = sk_X509_num(certs);

    if (destFile == NULL)
        goto end;
    CMP_info3("received %d %s certificate(s), saving to file '%s'",
              n, desc, destFile);
    if (n > 1 && opt_certform != FORMAT_PEM)
        CMP_warn("saving more than one certificate in non-PEM format");

    if (destFile == NULL || (bio = BIO_new(BIO_s_file())) == NULL
            || !BIO_write_filename(bio, (char *)destFile)) {
        CMP_err1("could not open file '%s' for writing", destFile);
        n = -1;
        goto end;
    }

    for (i = 0; i < n; i++) {
        if (!write_cert(bio, sk_X509_value(certs, i))) {
            CMP_err1("cannot write certificate to file '%s'", destFile);
            n = -1;
            goto end;
        }
    }

 end:
    BIO_free(bio);
    sk_X509_pop_free(certs, X509_free);
    return n;
}

static void print_itavs(STACK_OF(OSSL_CMP_ITAV) *itavs)
{
    OSSL_CMP_ITAV *itav = NULL;
    char buf[128];
    int i, r;
    int n = sk_OSSL_CMP_ITAV_num(itavs); /* itavs == NULL leads to 0 */

    if (n == 0) {
        CMP_info("genp contains no ITAV");
        return;
    }

    for (i = 0; i < n; i++) {
        itav = sk_OSSL_CMP_ITAV_value(itavs, i);
        r = OBJ_obj2txt(buf, 128, OSSL_CMP_ITAV_get0_type(itav), 0);
        if (r < 0)
            CMP_err("could not get ITAV details");
        else if (r == 0)
            CMP_info("genp contains empty ITAV");
        else
            CMP_info1("genp contains ITAV of type: %s", buf);
    }
}

static char opt_item[SECTION_NAME_MAX + 1];
/* get previous name from a comma or space-separated list of names */
static const char *prev_item(const char *opt, const char *end)
{
    const char *beg;
    size_t len;

    if (end == opt)
        return NULL;
    beg = end;
    while (beg > opt) {
        --beg;
        if (beg[0] == ',' || isspace(beg[0])) {
            ++beg;
            break;
        }
    }
    len = end - beg;
    if (len > SECTION_NAME_MAX) {
        CMP_warn3("using only first %d characters of section name starting with \"%.*s\"",
                  SECTION_NAME_MAX, SECTION_NAME_MAX, beg);
        len = SECTION_NAME_MAX;
    }
    memcpy(opt_item, beg, len);
    opt_item[len] = '\0';
    while (beg > opt) {
        --beg;
        if (beg[0] != ',' && !isspace(beg[0])) {
            ++beg;
            break;
        }
    }
    return beg;
}

/* get str value for name from a comma-separated hierarchy of config sections */
static char *conf_get_string(const CONF *src_conf, const char *groups,
                             const char *name)
{
    char *res = NULL;
    const char *end = groups + strlen(groups);

    while ((end = prev_item(groups, end)) != NULL) {
        if ((res = NCONF_get_string(src_conf, opt_item, name)) != NULL)
            return res;
    }
    return res;
}

/* get long val for name from a comma-separated hierarchy of config sections */
static int conf_get_number_e(const CONF *conf_, const char *groups,
                             const char *name, long *result)
{
    char *str = conf_get_string(conf_, groups, name);
    char *tailptr;
    long res;

    if (str == NULL || *str == '\0')
        return 0;

    res = strtol(str, &tailptr, 10);
    if (res == LONG_MIN || res == LONG_MAX || *tailptr != '\0')
        return 0;

    *result = res;
    return 1;
}

/*
 * use the command line option table to read values from the CMP section
 * of openssl.cnf.  Defaults are taken from the config file, they can be
 * overwritten on the command line.
 */
static int read_config(void)
{
    unsigned int i;
    long num = 0;
    char *txt = NULL;
    const OPTIONS *opt;
    int start_opt = OPT_VERBOSITY - OPT_HELP;
    int start_idx = OPT_VERBOSITY - 2;
    /*
     * starting with offset OPT_VERBOSITY because OPT_CONFIG and OPT_SECTION
     * would not make sense within the config file.
     */
    int n_options = OSSL_NELEM(cmp_options) - 1;

    for (opt = &cmp_options[start_opt], i = start_idx;
         opt->name != NULL; i++, opt++)
        if (!strcmp(opt->name, OPT_SECTION_STR)
                || !strcmp(opt->name, OPT_MORE_STR))
            n_options--;
    OPENSSL_assert(OSSL_NELEM(cmp_vars) == n_options
                 + OPT_PROV__FIRST + 1 - OPT_PROV__LAST
                 + OPT_R__FIRST + 1 - OPT_R__LAST
                 + OPT_V__FIRST + 1 - OPT_V__LAST);
    for (opt = &cmp_options[start_opt], i = start_idx;
         opt->name != NULL; i++, opt++) {
        int provider_option = (OPT_PROV__FIRST <= opt->retval
                               && opt->retval < OPT_PROV__LAST);
        int rand_state_option = (OPT_R__FIRST <= opt->retval
                                 && opt->retval < OPT_R__LAST);
        int verification_option = (OPT_V__FIRST <= opt->retval
                                   && opt->retval < OPT_V__LAST);

        if (strcmp(opt->name, OPT_SECTION_STR) == 0
                || strcmp(opt->name, OPT_MORE_STR) == 0) {
            i--;
            continue;
        }
        if (provider_option || rand_state_option || verification_option)
            i--;
        switch (opt->valtype) {
        case '-':
        case 'p':
        case 'n':
        case 'N':
        case 'l':
            if (!conf_get_number_e(conf, opt_section, opt->name, &num)) {
                ERR_clear_error();
                continue; /* option not provided */
            }
            if (opt->valtype == 'p' && num <= 0) {
                opt_printf_stderr("Non-positive number \"%ld\" for config option -%s\n",
                                  num, opt->name);
                return -1;
            }
            if (opt->valtype == 'N' && num < 0) {
                opt_printf_stderr("Negative number \"%ld\" for config option -%s\n",
                                  num, opt->name);
                return -1;
            }
            break;
        case 's':
        case '>':
        case 'M':
            txt = conf_get_string(conf, opt_section, opt->name);
            if (txt == NULL) {
                ERR_clear_error();
                continue; /* option not provided */
            }
            break;
        default:
            CMP_err2("internal: unsupported type '%c' for option '%s'",
                     opt->valtype, opt->name);
            return 0;
            break;
        }
        if (provider_option || verification_option) {
            int conf_argc = 1;
            char *conf_argv[3];
            char arg1[82];

            BIO_snprintf(arg1, 81, "-%s", (char *)opt->name);
            conf_argv[0] = prog;
            conf_argv[1] = arg1;
            if (opt->valtype == '-') {
                if (num != 0)
                    conf_argc = 2;
            } else {
                conf_argc = 3;
                conf_argv[2] = conf_get_string(conf, opt_section, opt->name);
                /* not NULL */
            }
            if (conf_argc > 1) {
                (void)opt_init(conf_argc, conf_argv, cmp_options);

                if (provider_option
                    ? !opt_provider(opt_next())
                    : !opt_verify(opt_next(), vpm)) {
                    CMP_err2("for option '%s' in config file section '%s'",
                             opt->name, opt_section);
                    return 0;
                }
            }
        } else {
            switch (opt->valtype) {
            case '-':
            case 'p':
            case 'n':
            case 'N':
                if (num < INT_MIN || INT_MAX < num) {
                    BIO_printf(bio_err,
                               "integer value out of range for option '%s'\n",
                               opt->name);
                    return 0;
                }
                *cmp_vars[i].num = (int)num;
                break;
            case 'l':
                *cmp_vars[i].num_long = num;
                break;
            default:
                if (txt != NULL && txt[0] == '\0')
                    txt = NULL; /* reset option on empty string input */
                *cmp_vars[i].txt = txt;
                break;
            }
        }
    }

    return 1;
}

static char *opt_str(void)
{
    char *arg = opt_arg();

    if (arg[0] == '\0') {
        CMP_warn1("%s option argument is empty string, resetting option",
                  opt_flag());
        arg = NULL;
    } else if (arg[0] == '-') {
        CMP_warn1("%s option argument starts with hyphen", opt_flag());
    }
    return arg;
}

/* returns 1 on success, 0 on error, -1 on -help (i.e., stop with success) */
static int get_opts(int argc, char **argv)
{
    OPTION_CHOICE o;

    prog = opt_init(argc, argv, cmp_options);

    while ((o = opt_next()) != OPT_EOF) {
        switch (o) {
        case OPT_EOF:
        case OPT_ERR:
 opthelp:
            BIO_printf(bio_err, "%s: Use -help for summary.\n", prog);
            return 0;
        case OPT_HELP:
            opt_help(cmp_options);
            return -1;
        case OPT_CONFIG: /* has already been handled */
        case OPT_SECTION: /* has already been handled */
            break;
        case OPT_VERBOSITY:
            if (!set_verbosity(opt_int_arg()))
                goto opthelp;
            break;
#ifndef OPENSSL_NO_SOCK
        case OPT_SERVER:
            opt_server = opt_str();
            break;
        case OPT_PROXY:
            opt_proxy = opt_str();
            break;
        case OPT_NO_PROXY:
            opt_no_proxy = opt_str();
            break;
#endif
        case OPT_RECIPIENT:
            opt_recipient = opt_str();
            break;
        case OPT_PATH:
            opt_path = opt_str();
            break;
        case OPT_KEEP_ALIVE:
            opt_keep_alive = opt_int_arg();
            if (opt_keep_alive > 2) {
                CMP_err("-keep_alive argument must be 0, 1, or 2");
                goto opthelp;
            }
            break;
        case OPT_MSG_TIMEOUT:
            opt_msg_timeout = opt_int_arg();
            break;
        case OPT_TOTAL_TIMEOUT:
            opt_total_timeout = opt_int_arg();
            break;
#ifndef OPENSSL_NO_SOCK
        case OPT_TLS_USED:
            opt_tls_used = 1;
            break;
        case OPT_TLS_CERT:
            opt_tls_cert = opt_str();
            break;
        case OPT_TLS_KEY:
            opt_tls_key = opt_str();
            break;
        case OPT_TLS_KEYPASS:
            opt_tls_keypass = opt_str();
            break;
        case OPT_TLS_EXTRA:
            opt_tls_extra = opt_str();
            break;
        case OPT_TLS_TRUSTED:
            opt_tls_trusted = opt_str();
            break;
        case OPT_TLS_HOST:
            opt_tls_host = opt_str();
            break;
#endif

        case OPT_REF:
            opt_ref = opt_str();
            break;
        case OPT_SECRET:
            opt_secret = opt_str();
            break;
        case OPT_CERT:
            opt_cert = opt_str();
            break;
        case OPT_OWN_TRUSTED:
            opt_own_trusted = opt_str();
            break;
        case OPT_KEY:
            opt_key = opt_str();
            break;
        case OPT_KEYPASS:
            opt_keypass = opt_str();
            break;
        case OPT_DIGEST:
            opt_digest = opt_str();
            break;
        case OPT_MAC:
            opt_mac = opt_str();
            break;
        case OPT_EXTRACERTS:
            opt_extracerts = opt_str();
            break;
        case OPT_UNPROTECTED_REQUESTS:
            opt_unprotected_requests = 1;
            break;

        case OPT_TRUSTED:
            opt_trusted = opt_str();
            break;
        case OPT_UNTRUSTED:
            opt_untrusted = opt_str();
            break;
        case OPT_SRVCERT:
            opt_srvcert = opt_str();
            break;
        case OPT_EXPECT_SENDER:
            opt_expect_sender = opt_str();
            break;
        case OPT_IGNORE_KEYUSAGE:
            opt_ignore_keyusage = 1;
            break;
        case OPT_UNPROTECTED_ERRORS:
            opt_unprotected_errors = 1;
            break;
        case OPT_EXTRACERTSOUT:
            opt_extracertsout = opt_str();
            break;
        case OPT_CACERTSOUT:
            opt_cacertsout = opt_str();
            break;

        case OPT_V_CASES:
            if (!opt_verify(o, vpm))
                goto opthelp;
            break;
        case OPT_CMD:
            opt_cmd_s = opt_str();
            break;
        case OPT_INFOTYPE:
            opt_infotype_s = opt_str();
            break;
        case OPT_GENINFO:
            opt_geninfo = opt_str();
            break;

        case OPT_NEWKEY:
            opt_newkey = opt_str();
            break;
        case OPT_NEWKEYPASS:
            opt_newkeypass = opt_str();
            break;
        case OPT_SUBJECT:
            opt_subject = opt_str();
            break;
        case OPT_ISSUER:
            opt_issuer = opt_str();
            break;
        case OPT_DAYS:
            opt_days = opt_int_arg();
            break;
        case OPT_REQEXTS:
            opt_reqexts = opt_str();
            break;
        case OPT_SANS:
            opt_sans = opt_str();
            break;
        case OPT_SAN_NODEFAULT:
            opt_san_nodefault = 1;
            break;
        case OPT_POLICIES:
            opt_policies = opt_str();
            break;
        case OPT_POLICY_OIDS:
            opt_policy_oids = opt_str();
            break;
        case OPT_POLICY_OIDS_CRITICAL:
            opt_policy_oids_critical = 1;
            break;
        case OPT_POPO:
            opt_popo = opt_int_arg();
            if (opt_popo < OSSL_CRMF_POPO_NONE
                    || opt_popo > OSSL_CRMF_POPO_KEYENC) {
                CMP_err("invalid popo spec. Valid values are -1 .. 2");
                goto opthelp;
            }
            break;
        case OPT_CSR:
            opt_csr = opt_arg();
            break;
        case OPT_OUT_TRUSTED:
            opt_out_trusted = opt_str();
            break;
        case OPT_IMPLICIT_CONFIRM:
            opt_implicit_confirm = 1;
            break;
        case OPT_DISABLE_CONFIRM:
            opt_disable_confirm = 1;
            break;
        case OPT_CERTOUT:
            opt_certout = opt_str();
            break;
        case OPT_CHAINOUT:
            opt_chainout = opt_str();
            break;
        case OPT_OLDCERT:
            opt_oldcert = opt_str();
            break;
        case OPT_REVREASON:
            opt_revreason = opt_int_arg();
                if (opt_revreason < CRL_REASON_NONE
                    || opt_revreason > CRL_REASON_AA_COMPROMISE
                    || opt_revreason == 7) {
                CMP_err("invalid revreason. Valid values are -1 .. 6, 8 .. 10");
                goto opthelp;
            }
            break;
        case OPT_CERTFORM:
            opt_certform_s = opt_str();
            break;
        case OPT_KEYFORM:
            opt_keyform_s = opt_str();
            break;
        case OPT_OTHERPASS:
            opt_otherpass = opt_str();
            break;
#ifndef OPENSSL_NO_ENGINE
        case OPT_ENGINE:
            opt_engine = opt_str();
            break;
#endif
        case OPT_PROV_CASES:
            if (!opt_provider(o))
                goto opthelp;
            break;
        case OPT_R_CASES:
            if (!opt_rand(o))
                goto opthelp;
            break;

        case OPT_BATCH:
            opt_batch = 1;
            break;
        case OPT_REPEAT:
            opt_repeat = opt_int_arg();
            break;
        case OPT_REQIN:
            opt_reqin = opt_str();
            break;
        case OPT_REQIN_NEW_TID:
            opt_reqin_new_tid = 1;
            break;
        case OPT_REQOUT:
            opt_reqout = opt_str();
            break;
        case OPT_RSPIN:
            opt_rspin = opt_str();
            break;
        case OPT_RSPOUT:
            opt_rspout = opt_str();
            break;
        case OPT_USE_MOCK_SRV:
            opt_use_mock_srv = 1;
            break;

#ifndef OPENSSL_NO_SOCK
        case OPT_PORT:
            opt_port = opt_str();
            break;
        case OPT_MAX_MSGS:
            opt_max_msgs = opt_int_arg();
            break;
#endif
        case OPT_SRV_REF:
            opt_srv_ref = opt_str();
            break;
        case OPT_SRV_SECRET:
            opt_srv_secret = opt_str();
            break;
        case OPT_SRV_CERT:
            opt_srv_cert = opt_str();
            break;
        case OPT_SRV_KEY:
            opt_srv_key = opt_str();
            break;
        case OPT_SRV_KEYPASS:
            opt_srv_keypass = opt_str();
            break;
        case OPT_SRV_TRUSTED:
            opt_srv_trusted = opt_str();
            break;
        case OPT_SRV_UNTRUSTED:
            opt_srv_untrusted = opt_str();
            break;
        case OPT_RSP_CERT:
            opt_rsp_cert = opt_str();
            break;
        case OPT_RSP_EXTRACERTS:
            opt_rsp_extracerts = opt_str();
            break;
        case OPT_RSP_CAPUBS:
            opt_rsp_capubs = opt_str();
            break;
        case OPT_POLL_COUNT:
            opt_poll_count = opt_int_arg();
            break;
        case OPT_CHECK_AFTER:
            opt_check_after = opt_int_arg();
            break;
        case OPT_GRANT_IMPLICITCONF:
            opt_grant_implicitconf = 1;
            break;
        case OPT_PKISTATUS:
            opt_pkistatus = opt_int_arg();
            break;
        case OPT_FAILURE:
            opt_failure = opt_int_arg();
            break;
        case OPT_FAILUREBITS:
            opt_failurebits = opt_int_arg();
            break;
        case OPT_STATUSSTRING:
            opt_statusstring = opt_str();
            break;
        case OPT_SEND_ERROR:
            opt_send_error = 1;
            break;
        case OPT_SEND_UNPROTECTED:
            opt_send_unprotected = 1;
            break;
        case OPT_SEND_UNPROT_ERR:
            opt_send_unprot_err = 1;
            break;
        case OPT_ACCEPT_UNPROTECTED:
            opt_accept_unprotected = 1;
            break;
        case OPT_ACCEPT_UNPROT_ERR:
            opt_accept_unprot_err = 1;
            break;
        case OPT_ACCEPT_RAVERIFIED:
            opt_accept_raverified = 1;
            break;
        }
    }

    /* No extra args. */
    argc = opt_num_rest();
    argv = opt_rest();
    if (argc != 0)
        goto opthelp;
    return 1;
}

#ifndef OPENSSL_NO_SOCK
static int cmp_server(OSSL_CMP_CTX *srv_cmp_ctx) {
    BIO *acbio;
    BIO *cbio = NULL;
    int keep_alive = 0;
    int msgs = 0;
    int retry = 1;
    int ret = 1;

    if ((acbio = http_server_init_bio(prog, opt_port)) == NULL)
        return 0;
    while (opt_max_msgs <= 0 || msgs < opt_max_msgs) {
        char *path = NULL;
        OSSL_CMP_MSG *req = NULL;
        OSSL_CMP_MSG *resp = NULL;

        ret = http_server_get_asn1_req(ASN1_ITEM_rptr(OSSL_CMP_MSG),
                                       (ASN1_VALUE **)&req, &path,
                                       &cbio, acbio, &keep_alive,
                                       prog, opt_port, 0, 0);
        if (ret == 0) { /* no request yet */
            if (retry) {
                ossl_sleep(1000);
                retry = 0;
                continue;
            }
            ret = 0;
            goto next;
        }
        if (ret++ == -1) /* fatal error */
            break;

        ret = 0;
        msgs++;
        if (req != NULL) {
            if (strcmp(path, "") != 0 && strcmp(path, "pkix/") != 0) {
                (void)http_server_send_status(cbio, 404, "Not Found");
                CMP_err1("expecting empty path or 'pkix/' but got '%s'",
                         path);
                OPENSSL_free(path);
                OSSL_CMP_MSG_free(req);
                goto next;
            }
            OPENSSL_free(path);
            resp = OSSL_CMP_CTX_server_perform(cmp_ctx, req);
            OSSL_CMP_MSG_free(req);
            if (resp == NULL) {
                (void)http_server_send_status(cbio,
                                              500, "Internal Server Error");
                break; /* treated as fatal error */
            }
            ret = http_server_send_asn1_resp(cbio, keep_alive,
                                             "application/pkixcmp",
                                             ASN1_ITEM_rptr(OSSL_CMP_MSG),
                                             (const ASN1_VALUE *)resp);
            OSSL_CMP_MSG_free(resp);
            if (!ret)
                break; /* treated as fatal error */
        }
    next:
        if (!ret) { /* on transmission error, cancel CMP transaction */
            (void)OSSL_CMP_CTX_set1_transactionID(srv_cmp_ctx, NULL);
            (void)OSSL_CMP_CTX_set1_senderNonce(srv_cmp_ctx, NULL);
        }
        if (!ret || !keep_alive
            || OSSL_CMP_CTX_get_status(srv_cmp_ctx) == -1
             /* transaction closed by OSSL_CMP_CTX_server_perform() */) {
            BIO_free_all(cbio);
            cbio = NULL;
        }
    }

    BIO_free_all(cbio);
    BIO_free_all(acbio);
    return ret;
}
#endif

int cmp_main(int argc, char **argv)
{
    char *configfile = NULL;
    int i;
    X509 *newcert = NULL;
    ENGINE *engine = NULL;
    OSSL_CMP_CTX *srv_cmp_ctx = NULL;
    int ret = 0; /* default: failure */

    prog = opt_appname(argv[0]);
    if (argc <= 1) {
        opt_help(cmp_options);
        goto err;
    }

    /*
     * handle options -config, -section, and -verbosity upfront
     * to take effect for other options
     */
    for (i = 1; i < argc - 1; i++) {
        if (*argv[i] == '-') {
            if (!strcmp(argv[i] + 1, cmp_options[OPT_CONFIG - OPT_HELP].name))
                opt_config = argv[++i];
            else if (!strcmp(argv[i] + 1,
                             cmp_options[OPT_SECTION - OPT_HELP].name))
                opt_section = argv[++i];
            else if (strcmp(argv[i] + 1,
                            cmp_options[OPT_VERBOSITY - OPT_HELP].name) == 0
                     && !set_verbosity(atoi(argv[++i])))
                goto err;
        }
    }
    if (opt_section[0] == '\0') /* empty string */
        opt_section = DEFAULT_SECTION;

    vpm = X509_VERIFY_PARAM_new();
    if (vpm == NULL) {
        CMP_err("out of memory");
        goto err;
    }

    /* read default values for options from config file */
    configfile = opt_config != NULL ? opt_config : default_config_file;
    if (configfile != NULL && configfile[0] != '\0' /* non-empty string */
            && (configfile != default_config_file || access(configfile, F_OK) != -1)) {
        CMP_info2("using section(s) '%s' of OpenSSL configuration file '%s'",
                  opt_section, configfile);
        conf = app_load_config(configfile);
        if (conf == NULL) {
            goto err;
        } else {
            if (strcmp(opt_section, CMP_SECTION) == 0) { /* default */
                if (!NCONF_get_section(conf, opt_section))
                    CMP_info2("no [%s] section found in config file '%s';"
                              " will thus use just [default] and unnamed section if present",
                              opt_section, configfile);
            } else {
                const char *end = opt_section + strlen(opt_section);
                while ((end = prev_item(opt_section, end)) != NULL) {
                    if (!NCONF_get_section(conf, opt_item)) {
                        CMP_err2("no [%s] section found in config file '%s'",
                                 opt_item, configfile);
                        goto err;
                    }
                }
            }
            ret = read_config();
            if (!set_verbosity(opt_verbosity)) /* just for checking range */
                ret = -1;
            if (ret <= 0) {
                if (ret == -1)
                    BIO_printf(bio_err, "Use -help for summary.\n");
                goto err;
            }
        }
    }
    (void)BIO_flush(bio_err); /* prevent interference with opt_help() */

    ret = get_opts(argc, argv);
    if (ret <= 0)
        goto err;
    ret = 0;
    if (!app_RAND_load())
        goto err;

    if (opt_batch)
        set_base_ui_method(UI_null());

    if (opt_engine != NULL) {
        engine = setup_engine_methods(opt_engine, 0 /* not: ENGINE_METHOD_ALL */, 0);
        if (engine == NULL) {
            CMP_err1("cannot load engine %s", opt_engine);
            goto err;
        }
    }

    cmp_ctx = OSSL_CMP_CTX_new(app_get0_libctx(), app_get0_propq());
    if (cmp_ctx == NULL)
        goto err;
    OSSL_CMP_CTX_set_log_verbosity(cmp_ctx, opt_verbosity);
    if (!OSSL_CMP_CTX_set_log_cb(cmp_ctx, print_to_bio_out)) {
        CMP_err1("cannot set up error reporting and logging for %s", prog);
        goto err;
    }

#ifndef OPENSSL_NO_SOCK
    if ((opt_tls_cert != NULL || opt_tls_key != NULL
         || opt_tls_keypass != NULL || opt_tls_extra != NULL
         || opt_tls_trusted != NULL || opt_tls_host != NULL)
            && !opt_tls_used)
        CMP_warn("Ingnoring TLS options(s) since -tls_used is not given");
    if (opt_port != NULL) {
        if (opt_tls_used) {
            CMP_err("-tls_used option not supported with -port option");
            goto err;
        }
        if (opt_use_mock_srv || opt_server != NULL || opt_rspin != NULL) {
            CMP_err("cannot use -port with -use_mock_srv, -server, or -rspin options");
            goto err;
        }
    }
    if (opt_server != NULL && opt_use_mock_srv) {
        CMP_err("cannot use both -server and -use_mock_srv options");
        goto err;
    }
#endif
    if (opt_rspin != NULL && opt_use_mock_srv) {
        CMP_err("cannot use both -rspin and -use_mock_srv options");
        goto err;
    }

    if (opt_use_mock_srv
#ifndef OPENSSL_NO_SOCK
        || opt_port != NULL
#endif
        ) {
        OSSL_CMP_SRV_CTX *srv_ctx;

        if ((srv_ctx = setup_srv_ctx(engine)) == NULL)
            goto err;
        srv_cmp_ctx = OSSL_CMP_SRV_CTX_get0_cmp_ctx(srv_ctx);
        OSSL_CMP_CTX_set_transfer_cb_arg(cmp_ctx, srv_ctx);
        if (!OSSL_CMP_CTX_set_log_cb(srv_cmp_ctx, print_to_bio_err)) {
            CMP_err1("cannot set up error reporting and logging for %s", prog);
            goto err;
        }
        OSSL_CMP_CTX_set_log_verbosity(srv_cmp_ctx, opt_verbosity);
    }

#ifndef OPENSSL_NO_SOCK
    if (opt_tls_used && (opt_use_mock_srv || opt_rspin != NULL)) {
        CMP_warn("ignoring -tls_used option since -use_mock_srv or -rspin is given");
        opt_tls_used = 0;
    }

    if (opt_port != NULL) { /* act as very basic CMP HTTP server */
        ret = cmp_server(srv_cmp_ctx);
        goto err;
    }

    /* act as CMP client, possibly using internal mock server */

    if (opt_server != NULL) {
        if (opt_rspin != NULL) {
            CMP_warn("ignoring -server option since -rspin is given");
            opt_server = NULL;
        }
    }
#endif

    if (!setup_client_ctx(cmp_ctx, engine)) {
        CMP_err("cannot set up CMP context");
        goto err;
    }
    for (i = 0; i < opt_repeat; i++) {
        /* everything is ready, now connect and perform the command! */
        switch (opt_cmd) {
        case CMP_IR:
            newcert = OSSL_CMP_exec_IR_ses(cmp_ctx);
            if (newcert != NULL)
                ret = 1;
            break;
        case CMP_KUR:
            newcert = OSSL_CMP_exec_KUR_ses(cmp_ctx);
            if (newcert != NULL)
                ret = 1;
            break;
        case CMP_CR:
            newcert = OSSL_CMP_exec_CR_ses(cmp_ctx);
            if (newcert != NULL)
                ret = 1;
            break;
        case CMP_P10CR:
            newcert = OSSL_CMP_exec_P10CR_ses(cmp_ctx);
            if (newcert != NULL)
                ret = 1;
            break;
        case CMP_RR:
            ret = OSSL_CMP_exec_RR_ses(cmp_ctx);
            break;
        case CMP_GENM:
            {
                STACK_OF(OSSL_CMP_ITAV) *itavs;

                if (opt_infotype != NID_undef) {
                    OSSL_CMP_ITAV *itav =
                        OSSL_CMP_ITAV_create(OBJ_nid2obj(opt_infotype), NULL);
                    if (itav == NULL)
                        goto err;
                    OSSL_CMP_CTX_push0_genm_ITAV(cmp_ctx, itav);
                }

                if ((itavs = OSSL_CMP_exec_GENM_ses(cmp_ctx)) != NULL) {
                    print_itavs(itavs);
                    sk_OSSL_CMP_ITAV_pop_free(itavs, OSSL_CMP_ITAV_free);
                    ret = 1;
                }
                break;
            }
        default:
            break;
        }
        if (OSSL_CMP_CTX_get_status(cmp_ctx) < 0)
            goto err; /* we got no response, maybe even did not send request */

        {
            /* print PKIStatusInfo */
            int status = OSSL_CMP_CTX_get_status(cmp_ctx);
            char *buf = app_malloc(OSSL_CMP_PKISI_BUFLEN, "PKIStatusInfo buf");
            const char *string =
                OSSL_CMP_CTX_snprint_PKIStatus(cmp_ctx, buf,
                                               OSSL_CMP_PKISI_BUFLEN);
            const char *from = "", *server = "";

#ifndef OPENSSL_NO_SOCK
            if (opt_server != NULL) {
                from = " from ";
                server = opt_server;
            }
#endif
            CMP_print(bio_err,
                      status == OSSL_CMP_PKISTATUS_accepted
                      ? OSSL_CMP_LOG_INFO :
                      status == OSSL_CMP_PKISTATUS_rejection
                      || status == OSSL_CMP_PKISTATUS_waiting
                      ? OSSL_CMP_LOG_ERR : OSSL_CMP_LOG_WARNING,
                      status == OSSL_CMP_PKISTATUS_accepted ? "info" :
                      status == OSSL_CMP_PKISTATUS_rejection ? "server error" :
                      status == OSSL_CMP_PKISTATUS_waiting ? "internal error"
                                                           : "warning",
                      "received%s%s %s", from, server,
                      string != NULL ? string : "<unknown PKIStatus>");
            OPENSSL_free(buf);
        }

        if (save_free_certs(cmp_ctx, OSSL_CMP_CTX_get1_extraCertsIn(cmp_ctx),
                            opt_extracertsout, "extra") < 0)
            ret = 0;
        if (!ret)
            goto err;
        ret = 0;
        if (save_free_certs(cmp_ctx, OSSL_CMP_CTX_get1_caPubs(cmp_ctx),
                            opt_cacertsout, "CA") < 0)
            goto err;
        if (newcert != NULL) {
            STACK_OF(X509) *certs = sk_X509_new_null();

            if (!X509_add_cert(certs, newcert, X509_ADD_FLAG_UP_REF)) {
                sk_X509_free(certs);
                goto err;
            }
            if (save_free_certs(cmp_ctx, certs, opt_certout, "enrolled") < 0)
                goto err;
        }
        if (save_free_certs(cmp_ctx, OSSL_CMP_CTX_get1_newChain(cmp_ctx),
                            opt_chainout, "chain") < 0)
            goto err;

        if (!OSSL_CMP_CTX_reinit(cmp_ctx))
            goto err;
    }
    ret = 1;

 err:
    /* in case we ended up here on error without proper cleaning */
    cleanse(opt_keypass);
    cleanse(opt_newkeypass);
    cleanse(opt_otherpass);
#ifndef OPENSSL_NO_SOCK
    cleanse(opt_tls_keypass);
#endif
    cleanse(opt_secret);
    cleanse(opt_srv_keypass);
    cleanse(opt_srv_secret);

    if (ret != 1)
        OSSL_CMP_CTX_print_errors(cmp_ctx);

    ossl_cmp_mock_srv_free(OSSL_CMP_CTX_get_transfer_cb_arg(cmp_ctx));
#ifndef OPENSSL_NO_SOCK
    APP_HTTP_TLS_INFO_free(OSSL_CMP_CTX_get_http_cb_arg(cmp_ctx));
#endif
    X509_STORE_free(OSSL_CMP_CTX_get_certConf_cb_arg(cmp_ctx));
    OSSL_CMP_CTX_free(cmp_ctx);
    X509_VERIFY_PARAM_free(vpm);
    release_engine(engine);

    NCONF_free(conf); /* must not do as long as opt_... variables are used */
    OSSL_CMP_log_close();

    return ret == 0 ? EXIT_FAILURE : EXIT_SUCCESS; /* ret == -1 for -help */
}
