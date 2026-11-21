/**
 * \file mbedtls_config.h
 *
 * \brief Configuration options (set of defines)
 *
 *  This set of compile-time options may be used to enable
 *  or disable features selectively, and reduce the global
 *  memory footprint.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/**
 * This is an optional version symbol that enables compatibility handling of
 * config files.
 *
 * It is equal to the #MBEDTLS_VERSION_NUMBER of the Mbed TLS version that
 * introduced the config format we want to be compatible with.
 */
#define MBEDTLS_CONFIG_VERSION 0x04000000

/**
 * \name SECTION: Platform abstraction layer
 *
 * This section sets platform specific settings.
 * \{
 */

/**
 * \def MBEDTLS_NET_C
 *
 * Enable the TCP and UDP over IPv6/IPv4 networking routines.
 *
 * \note This module only works on POSIX/Unix (including Linux, BSD and OS X)
 * and Windows. For other platforms, you'll want to disable it, and write your
 * own networking callbacks to be passed to \c mbedtls_ssl_set_bio().
 *
 * \note See also our Knowledge Base article about porting to a new
 * environment:
 * https://mbed-tls.readthedocs.io/en/latest/kb/how-to/how-do-i-port-mbed-tls-to-a-new-environment-OS
 *
 * Module:  library/net_sockets.c
 *
 * This module provides networking routines.
 */
#define MBEDTLS_NET_C

/**
 * \def MBEDTLS_TIMING_ALT
 *
 * Uncomment to provide your own alternate implementation for
 * mbedtls_timing_get_timer(), mbedtls_set_alarm(), mbedtls_set/get_delay()
 *
 * Only works if you have MBEDTLS_TIMING_C enabled.
 *
 * You will need to provide a header "timing_alt.h" and an implementation at
 * compile time.
 */
//#define MBEDTLS_TIMING_ALT

/**
 * \def MBEDTLS_TIMING_C
 *
 * Enable the semi-portable timing interface.
 *
 * \note The provided implementation only works on POSIX/Unix (including Linux,
 * BSD and OS X) and Windows. On other platforms, you can either disable that
 * module and provide your own implementations of the callbacks needed by
 * \c mbedtls_ssl_set_timer_cb() for DTLS, or leave it enabled and provide
 * your own implementation of the whole module by setting
 * \c MBEDTLS_TIMING_ALT in the current file.
 *
 * \note The timing module will include time.h on suitable platforms
 *       regardless of the setting of MBEDTLS_HAVE_TIME, unless
 *       MBEDTLS_TIMING_ALT is used. See timing.c for more information.
 *
 * \note See also our Knowledge Base article about porting to a new
 * environment:
 * https://mbed-tls.readthedocs.io/en/latest/kb/how-to/how-do-i-port-mbed-tls-to-a-new-environment-OS
 *
 * Module:  library/timing.c
 */
#define MBEDTLS_TIMING_C

/** \} name SECTION: Platform abstraction layer */

/**
 * \name SECTION: General configuration options
 *
 * This section contains Mbed TLS build settings that are not associated
 * with a particular module.
 * \{
 */

/**
 * \def MBEDTLS_ERROR_C
 *
 * Enable error code to error string conversion.
 *
 * Module:  library/error.c
 * Caller:
 *
 * This module enables mbedtls_strerror().
 */
#define MBEDTLS_ERROR_C

/**
 * \def MBEDTLS_ERROR_STRERROR_DUMMY
 *
 * Enable a dummy error function to make use of mbedtls_strerror() in
 * third party libraries easier when MBEDTLS_ERROR_C is disabled
 * (no effect when MBEDTLS_ERROR_C is enabled).
 *
 * You can safely disable this if MBEDTLS_ERROR_C is enabled, or if you're
 * not using mbedtls_strerror() or error_strerror() in your application.
 *
 * Disable if you run into name conflicts and want to really remove the
 * mbedtls_strerror()
 */
#define MBEDTLS_ERROR_STRERROR_DUMMY

/**
 * \def MBEDTLS_VERSION_C
 *
 * Enable run-time version information.
 *
 * Module:  library/version.c
 *
 * This module provides run-time version information.
 */
#define MBEDTLS_VERSION_C

/**
 * \def MBEDTLS_VERSION_FEATURES
 *
 * Allow run-time checking of compile-time enabled features. Thus allowing users
 * to check at run-time if the library is for instance compiled with threading
 * support via mbedtls_version_check_feature().
 *
 * Requires: MBEDTLS_VERSION_C
 *
 * Comment this to disable run-time checking and save ROM space
 */
#define MBEDTLS_VERSION_FEATURES

/**
 * \def MBEDTLS_CONFIG_FILE
 *
 * If defined, this is a header which will be included instead of
 * `"mbedtls/mbedtls_config.h"`.
 * This header file specifies the compile-time configuration of Mbed TLS.
 * Unlike other configuration options, this one must be defined on the
 * compiler command line: a definition in `mbedtls_config.h` would have
 * no effect.
 *
 * This macro is expanded after an <tt>\#include</tt> directive. This is a popular but
 * non-standard feature of the C language, so this feature is only available
 * with compilers that perform macro expansion on an <tt>\#include</tt> line.
 *
 * The value of this symbol is typically a path in double quotes, either
 * absolute or relative to a directory on the include search path.
 */
//#define MBEDTLS_CONFIG_FILE "mbedtls/mbedtls_config.h"

/**
 * \def MBEDTLS_USER_CONFIG_FILE
 *
 * If defined, this is a header which will be included after
 * `"mbedtls/mbedtls_config.h"` or #MBEDTLS_CONFIG_FILE.
 * This allows you to modify the default configuration, including the ability
 * to undefine options that are enabled by default.
 *
 * This macro is expanded after an <tt>\#include</tt> directive. This is a popular but
 * non-standard feature of the C language, so this feature is only available
 * with compilers that perform macro expansion on an <tt>\#include</tt> line.
 *
 * The value of this symbol is typically a path in double quotes, either
 * absolute or relative to a directory on the include search path.
 */
//#define MBEDTLS_USER_CONFIG_FILE "/dev/null"

/** \} name SECTION: General configuration options */

/**
 * \name SECTION: TLS feature selection
 *
 * This section sets support for features that are or are not needed
 * within the modules that are enabled.
 * \{
 */

/**
 * \def MBEDTLS_SSL_NULL_CIPHERSUITES
 *
 * Enable ciphersuites without encryption.
 *
 * Warning: Only do so when you know what you are doing. This allows for
 * channels without any encryption. All data are transmitted in clear.
 *
 * Uncomment this macro to enable the NULL ciphersuites
 */
//#define MBEDTLS_SSL_NULL_CIPHERSUITES

/**
 * \def MBEDTLS_DEBUG_C
 *
 * Enable the debug functions.
 *
 * Module:  library/debug.c
 * Caller:  library/ssl_msg.c
 *          library/ssl_tls.c
 *          library/ssl_tls12_*.c
 *          library/ssl_tls13_*.c
 *
 * This module provides debugging functions.
 */
#define MBEDTLS_DEBUG_C

/**
 * \def MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
 *
 * Enable the ECDHE-ECDSA based ciphersuite modes in SSL / TLS.
 *
 * Requires: PSA_WANT_ALG_ECDH
 *           PSA_WANT_ALG_ECDSA
 *           MBEDTLS_X509_CRT_PARSE_C
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
 *      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384
 *      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA
 *      MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384
 *      MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384
 *      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
 *      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
 *      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA
 *      MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256
 *      MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256
 */
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED

/**
 * \def MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED
 *
 * Enable the ECDHE-PSK based ciphersuite modes in SSL / TLS.
 *
 * Requires: PSA_WANT_ALG_ECDH
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      MBEDTLS_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384
 *      MBEDTLS_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA
 *      MBEDTLS_TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384
 *      MBEDTLS_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
 *      MBEDTLS_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA
 *      MBEDTLS_TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256
 */
#define MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED

/**
 * \def MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
 *
 * Enable the ECDHE-RSA based ciphersuite modes in SSL / TLS.
 *
 * Requires: PSA_WANT_ALG_ECDH
 *           PSA_WANT_ALG_RSA_PKCS1V15_SIGN
 *           MBEDTLS_X509_CRT_PARSE_C
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
 *      MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384
 *      MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA
 *      MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384
 *      MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384
 *      MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
 *      MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
 *      MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
 *      MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256
 *      MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256
 */
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED

/**
 * \def MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED
 *
 * Enable the ECJPAKE based ciphersuite modes in SSL / TLS.
 *
 * \warning This is currently experimental. EC J-PAKE support is based on the
 * Thread v1.0.0 specification; incompatible changes to the specification
 * might still happen. For this reason, this is disabled by default.
 *
 * Requires: PSA_WANT_ALG_JPAKE
 *           PSA_WANT_ALG_SHA_256
 *           PSA_WANT_ECC_SECP_R1_256
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8
 */
//#define MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED

/**
 * \def MBEDTLS_KEY_EXCHANGE_PSK_ENABLED
 *
 * Enable the PSK based ciphersuite modes in SSL / TLS.
 *
 * This enables the following ciphersuites (if other requisites are
 * enabled as well):
 *      MBEDTLS_TLS_PSK_WITH_AES_256_GCM_SHA384
 *      MBEDTLS_TLS_PSK_WITH_AES_256_CBC_SHA384
 *      MBEDTLS_TLS_PSK_WITH_AES_256_CBC_SHA
 *      MBEDTLS_TLS_PSK_WITH_CAMELLIA_256_GCM_SHA384
 *      MBEDTLS_TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384
 *      MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256
 *      MBEDTLS_TLS_PSK_WITH_AES_128_CBC_SHA256
 *      MBEDTLS_TLS_PSK_WITH_AES_128_CBC_SHA
 *      MBEDTLS_TLS_PSK_WITH_CAMELLIA_128_GCM_SHA256
 *      MBEDTLS_TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256
 */
#define MBEDTLS_KEY_EXCHANGE_PSK_ENABLED

/**
 * \def MBEDTLS_SSL_ALL_ALERT_MESSAGES
 *
 * Enable sending of alert messages in case of encountered errors as per RFC.
 * If you choose not to send the alert messages, Mbed TLS can still communicate
 * with other servers, only debugging of failures is harder.
 *
 * The advantage of not sending alert messages, is that no information is given
 * about reasons for failures thus preventing adversaries of gaining intel.
 *
 * Enable sending of all alert messages
 */
#define MBEDTLS_SSL_ALL_ALERT_MESSAGES

/**
 * \def MBEDTLS_SSL_ALPN
 *
 * Enable support for RFC 7301 Application Layer Protocol Negotiation.
 *
 * Comment this macro to disable support for ALPN.
 */
#define MBEDTLS_SSL_ALPN

/**
 * \def MBEDTLS_SSL_ASYNC_PRIVATE
 *
 * Enable asynchronous external private key operations in SSL. This allows
 * you to configure an SSL connection to call an external cryptographic
 * module to perform private key operations instead of performing the
 * operation inside the library.
 *
 * Requires: MBEDTLS_X509_CRT_PARSE_C
 */
//#define MBEDTLS_SSL_ASYNC_PRIVATE

/**
 * \def MBEDTLS_SSL_CACHE_C
 *
 * Enable simple SSL cache implementation.
 *
 * Module:  library/ssl_cache.c
 * Caller:
 *
 * Requires: MBEDTLS_SSL_CACHE_C
 */
#define MBEDTLS_SSL_CACHE_C

/**
 * \def MBEDTLS_SSL_CLI_C
 *
 * Enable the SSL/TLS client code.
 *
 * Module:  library/ssl*_client.c
 * Caller:
 *
 * Requires: MBEDTLS_SSL_TLS_C
 *
 * \warning You must call psa_crypto_init() before doing any TLS operations.
 *
 * This module is required for SSL/TLS client support.
 */
#define MBEDTLS_SSL_CLI_C

/**
 * \def MBEDTLS_SSL_CONTEXT_SERIALIZATION
 *
 * Enable serialization of the TLS context structures, through use of the
 * functions mbedtls_ssl_context_save() and mbedtls_ssl_context_load().
 *
 * This pair of functions allows one side of a connection to serialize the
 * context associated with the connection, then free or re-use that context
 * while the serialized state is persisted elsewhere, and finally deserialize
 * that state to a live context for resuming read/write operations on the
 * connection. From a protocol perspective, the state of the connection is
 * unaffected, in particular this is entirely transparent to the peer.
 *
 * Note: this is distinct from TLS session resumption, which is part of the
 * protocol and fully visible by the peer. TLS session resumption enables
 * establishing new connections associated to a saved session with shorter,
 * lighter handshakes, while context serialization is a local optimization in
 * handling a single, potentially long-lived connection.
 *
 * Enabling these APIs makes some SSL structures larger, as 64 extra bytes are
 * saved after the handshake to allow for more efficient serialization, so if
 * you don't need this feature you'll save RAM by disabling it.
 *
 * Requires: PSA_WANT_ALG_GCM or PSA_WANT_ALG_CCM or PSA_WANT_ALG_CHACHA20_POLY1305
 *
 * Comment to disable the context serialization APIs.
 */
#define MBEDTLS_SSL_CONTEXT_SERIALIZATION

/**
 * \def MBEDTLS_SSL_COOKIE_C
 *
 * Enable basic implementation of DTLS cookies for hello verification.
 *
 * Module:  library/ssl_cookie.c
 * Caller:
 */
#define MBEDTLS_SSL_COOKIE_C

/**
 * \def MBEDTLS_SSL_DEBUG_ALL
 *
 * Enable the debug messages in SSL module for all issues.
 * Debug messages have been disabled in some places to prevent timing
 * attacks due to (unbalanced) debugging function calls.
 *
 * If you need all error reporting you should enable this during debugging,
 * but remove this for production servers that should log as well.
 *
 * Uncomment this macro to report all debug messages on errors introducing
 * a timing side-channel.
 *
 */
//#define MBEDTLS_SSL_DEBUG_ALL

/**
 * \def MBEDTLS_SSL_DTLS_ANTI_REPLAY
 *
 * Enable support for the anti-replay mechanism in DTLS.
 *
 * Requires: MBEDTLS_SSL_TLS_C
 *           MBEDTLS_SSL_PROTO_DTLS
 *
 * \warning Disabling this is often a security risk!
 * See mbedtls_ssl_conf_dtls_anti_replay() for details.
 *
 * Comment this to disable anti-replay in DTLS.
 */
#define MBEDTLS_SSL_DTLS_ANTI_REPLAY

/**
 * \def MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE
 *
 * Enable server-side support for clients that reconnect from the same port.
 *
 * Some clients unexpectedly close the connection and try to reconnect using the
 * same source port. This needs special support from the server to handle the
 * new connection securely, as described in section 4.2.8 of RFC 6347. This
 * flag enables that support.
 *
 * Requires: MBEDTLS_SSL_DTLS_HELLO_VERIFY
 *
 * Comment this to disable support for clients reusing the source port.
 */
#define MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE

/**
 * \def MBEDTLS_SSL_DTLS_CONNECTION_ID
 *
 * Enable support for the DTLS Connection ID (CID) extension,
 * which allows to identify DTLS connections across changes
 * in the underlying transport. The CID functionality is described
 * in RFC 9146.
 *
 * Setting this option enables the SSL APIs `mbedtls_ssl_set_cid()`,
 * mbedtls_ssl_get_own_cid()`, `mbedtls_ssl_get_peer_cid()` and
 * `mbedtls_ssl_conf_cid()`. See the corresponding documentation for
 * more information.
 *
 * The maximum lengths of outgoing and incoming CIDs can be configured
 * through the options
 * - MBEDTLS_SSL_CID_OUT_LEN_MAX
 * - MBEDTLS_SSL_CID_IN_LEN_MAX.
 *
 * Requires: MBEDTLS_SSL_PROTO_DTLS
 *
 * Uncomment to enable the Connection ID extension.
 */
#define MBEDTLS_SSL_DTLS_CONNECTION_ID

/**
 * \def MBEDTLS_SSL_DTLS_HELLO_VERIFY
 *
 * Enable support for HelloVerifyRequest on DTLS servers.
 *
 * This feature is highly recommended to prevent DTLS servers being used as
 * amplifiers in DoS attacks against other hosts. It should always be enabled
 * unless you know for sure amplification cannot be a problem in the
 * environment in which your server operates.
 *
 * \warning Disabling this can be a security risk! (see above)
 *
 * Requires: MBEDTLS_SSL_PROTO_DTLS
 *
 * Comment this to disable support for HelloVerifyRequest.
 */
#define MBEDTLS_SSL_DTLS_HELLO_VERIFY

/**
 * \def MBEDTLS_SSL_DTLS_SRTP
 *
 * Enable support for negotiation of DTLS-SRTP (RFC 5764)
 * through the use_srtp extension.
 *
 * \note This feature provides the minimum functionality required
 * to negotiate the use of DTLS-SRTP and to allow the derivation of
 * the associated SRTP packet protection key material.
 * In particular, the SRTP packet protection itself, as well as the
 * demultiplexing of RTP and DTLS packets at the datagram layer
 * (see Section 5 of RFC 5764), are not handled by this feature.
 * Instead, after successful completion of a handshake negotiating
 * the use of DTLS-SRTP, the extended key exporter API
 * mbedtls_ssl_conf_export_keys_cb() should be used to implement
 * the key exporter described in Section 4.2 of RFC 5764 and RFC 5705
 * (this is implemented in the SSL example programs).
 * The resulting key should then be passed to an SRTP stack.
 *
 * Setting this option enables the runtime API
 * mbedtls_ssl_conf_dtls_srtp_protection_profiles()
 * through which the supported DTLS-SRTP protection
 * profiles can be configured. You must call this API at
 * runtime if you wish to negotiate the use of DTLS-SRTP.
 *
 * Requires: MBEDTLS_SSL_PROTO_DTLS
 *
 * Uncomment this to enable support for use_srtp extension.
 */
//#define MBEDTLS_SSL_DTLS_SRTP

/**
 * \def MBEDTLS_SSL_EARLY_DATA
 *
 * Enable support for RFC 8446 TLS 1.3 early data.
 *
 * Requires: MBEDTLS_SSL_SESSION_TICKETS and either
 *           MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED or
 *           MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED
 *
 * Comment this to disable support for early data. If MBEDTLS_SSL_PROTO_TLS1_3
 * is not enabled, this option does not have any effect on the build.
 *
 * \note The maximum amount of early data can be set with
 *       MBEDTLS_SSL_MAX_EARLY_DATA_SIZE.
 *
 */
//#define MBEDTLS_SSL_EARLY_DATA

/** \def MBEDTLS_SSL_ENCRYPT_THEN_MAC
 *
 * Enable support for Encrypt-then-MAC, RFC 7366.
 *
 * This allows peers that both support it to use a more robust protection for
 * ciphersuites using CBC, providing deep resistance against timing attacks
 * on the padding or underlying cipher.
 *
 * This only affects CBC ciphersuites, and is useless if none is defined.
 *
 * Requires: MBEDTLS_SSL_PROTO_TLS1_2
 *
 * Comment this macro to disable support for Encrypt-then-MAC
 */
#define MBEDTLS_SSL_ENCRYPT_THEN_MAC

/** \def MBEDTLS_SSL_EXTENDED_MASTER_SECRET
 *
 * Enable support for RFC 7627: Session Hash and Extended Master Secret
 * Extension.
 *
 * This was introduced as "the proper fix" to the Triple Handshake family of
 * attacks, but it is recommended to always use it (even if you disable
 * renegotiation), since it actually fixes a more fundamental issue in the
 * original SSL/TLS design, and has implications beyond Triple Handshake.
 *
 * Requires: MBEDTLS_SSL_PROTO_TLS1_2
 *
 * Comment this macro to disable support for Extended Master Secret.
 */
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET

/**
 * \def MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
 *
 * This option controls the availability of the API mbedtls_ssl_get_peer_cert()
 * giving access to the peer's certificate after completion of the handshake.
 *
 * Unless you need mbedtls_ssl_peer_cert() in your application, it is
 * recommended to disable this option for reduced RAM usage.
 *
 * \note If this option is disabled, mbedtls_ssl_get_peer_cert() is still
 *       defined, but always returns \c NULL.
 *
 * \note This option has no influence on the protection against the
 *       triple handshake attack. Even if it is disabled, Mbed TLS will
 *       still ensure that certificates do not change during renegotiation,
 *       for example by keeping a hash of the peer's certificate.
 *
 * \note This option is required if MBEDTLS_SSL_PROTO_TLS1_3 is set.
 *
 * Comment this macro to disable storing the peer's certificate
 * after the handshake.
 */
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE

/**
 * \def MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
 *
 * Enable support for RFC 6066 max_fragment_length extension in SSL.
 *
 * Comment this macro to disable support for the max_fragment_length extension
 */
#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH

/**
 * \def MBEDTLS_SSL_PROTO_DTLS
 *
 * Enable support for DTLS (all available versions).
 *
 * Enable this and MBEDTLS_SSL_PROTO_TLS1_2 to enable DTLS 1.2.
 *
 * Requires: MBEDTLS_SSL_PROTO_TLS1_2
 *
 * Comment this macro to disable support for DTLS
 */
#define MBEDTLS_SSL_PROTO_DTLS

/**
 * \def MBEDTLS_SSL_PROTO_TLS1_2
 *
 * Enable support for TLS 1.2 (and DTLS 1.2 if DTLS is enabled).
 *
 * Requires: PSA_WANT_ALG_SHA_256 or PSA_WANT_ALG_SHA_384
 *
 * Comment this macro to disable support for TLS 1.2 / DTLS 1.2
 */
#define MBEDTLS_SSL_PROTO_TLS1_2

/**
 * \def MBEDTLS_SSL_PROTO_TLS1_3
 *
 * Enable support for TLS 1.3.
 *
 * \note See docs/architecture/tls13-support.md for a description of the TLS
 *       1.3 support that this option enables.
 *
 * Requires: MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
 * Requires: MBEDTLS_PSA_CRYPTO_C
 *
 * Uncomment this macro to enable the support for TLS 1.3.
 */
#define MBEDTLS_SSL_PROTO_TLS1_3

/**
 * \def MBEDTLS_SSL_RECORD_SIZE_LIMIT
 *
 * Enable support for RFC 8449 record_size_limit extension in SSL (TLS 1.3 only).
 *
 * Requires: MBEDTLS_SSL_PROTO_TLS1_3
 *
 * Uncomment this macro to enable support for the record_size_limit extension
 */
//#define MBEDTLS_SSL_RECORD_SIZE_LIMIT

/**
 * \def MBEDTLS_SSL_KEYING_MATERIAL_EXPORT
 *
 * When this option is enabled, the client and server can extract additional
 * shared symmetric keys after an SSL handshake using the function
 * mbedtls_ssl_export_keying_material().
 *
 * The process for deriving the keys is specified in RFC 5705 for TLS 1.2 and
 * in RFC 8446, Section 7.5, for TLS 1.3.
 *
 * Comment this macro to disable mbedtls_ssl_export_keying_material().
 */
#define MBEDTLS_SSL_KEYING_MATERIAL_EXPORT

/**
 * \def MBEDTLS_SSL_RENEGOTIATION
 *
 * Enable support for TLS renegotiation.
 *
 * The two main uses of renegotiation are (1) refresh keys on long-lived
 * connections and (2) client authentication after the initial handshake.
 * If you don't need renegotiation, it's probably better to disable it, since
 * it has been associated with security issues in the past and is easy to
 * misuse/misunderstand.
 *
 * Requires: MBEDTLS_SSL_PROTO_TLS1_2
 *
 * Comment this to disable support for renegotiation.
 *
 * \note   Even if this option is disabled, both client and server are aware
 *         of the Renegotiation Indication Extension (RFC 5746) used to
 *         prevent the SSL renegotiation attack (see RFC 5746 Sect. 1).
 *         (See \c mbedtls_ssl_conf_legacy_renegotiation for the
 *          configuration of this extension).
 *
 */
#define MBEDTLS_SSL_RENEGOTIATION

/**
 * \def MBEDTLS_SSL_SERVER_NAME_INDICATION
 *
 * Enable support for RFC 6066 server name indication (SNI) in SSL.
 *
 * Requires: MBEDTLS_X509_CRT_PARSE_C
 *
 * Comment this macro to disable support for server name indication in SSL
 */
#define MBEDTLS_SSL_SERVER_NAME_INDICATION

/**
 * \def MBEDTLS_SSL_SESSION_TICKETS
 *
 * Enable support for RFC 5077 session tickets in SSL.
 * Client-side, provides full support for session tickets (maintenance of a
 * session store remains the responsibility of the application, though).
 * Server-side, you also need to provide callbacks for writing and parsing
 * tickets, including authenticated encryption and key management. Example
 * callbacks are provided by MBEDTLS_SSL_TICKET_C.
 *
 * Comment this macro to disable support for SSL session tickets
 */
#define MBEDTLS_SSL_SESSION_TICKETS

/**
 * \def MBEDTLS_SSL_SRV_C
 *
 * Enable the SSL/TLS server code.
 *
 * Module:  library/ssl*_server.c
 * Caller:
 *
 * Requires: MBEDTLS_SSL_TLS_C
 *
 * \warning You must call psa_crypto_init() before doing any TLS operations.
 *
 * This module is required for SSL/TLS server support.
 */
#define MBEDTLS_SSL_SRV_C

/**
 * \def MBEDTLS_SSL_TICKET_C
 *
 * Enable an implementation of TLS server-side callbacks for session tickets.
 *
 * Module:  library/ssl_ticket.c
 * Caller:
 *
 * Requires: PSA_WANT_ALG_GCM or PSA_WANT_ALG_CCM or PSA_WANT_ALG_CHACHA20_POLY1305
 */
#define MBEDTLS_SSL_TICKET_C

/**
 * \def MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE
 *
 * Enable TLS 1.3 middlebox compatibility mode.
 *
 * As specified in Section D.4 of RFC 8446, TLS 1.3 offers a compatibility
 * mode to make a TLS 1.3 connection more likely to pass through middle boxes
 * expecting TLS 1.2 traffic.
 *
 * Turning on the compatibility mode comes at the cost of a few added bytes
 * on the wire, but it doesn't affect compatibility with TLS 1.3 implementations
 * that don't use it. Therefore, unless transmission bandwidth is critical and
 * you know that middlebox compatibility issues won't occur, it is therefore
 * recommended to set this option.
 *
 * Comment to disable compatibility mode for TLS 1.3. If
 * MBEDTLS_SSL_PROTO_TLS1_3 is not enabled, this option does not have any
 * effect on the build.
 *
 */
#define MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE

/**
 * \def MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED
 *
 * Enable TLS 1.3 ephemeral key exchange mode.
 *
 * Requires: PSA_WANT_ALG_ECDH or PSA_WANT_ALG_FFDH
 *           MBEDTLS_X509_CRT_PARSE_C
 *           and at least one of:
 *               PSA_WANT_ALG_ECDSA
 *               PSA_WANT_ALG_RSA_PSS
 *
 * Comment to disable support for the ephemeral key exchange mode in TLS 1.3.
 * If MBEDTLS_SSL_PROTO_TLS1_3 is not enabled, this option does not have any
 * effect on the build.
 *
 */
#define MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED

/**
 * \def MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED
 *
 * Enable TLS 1.3 PSK key exchange mode.
 *
 * Comment to disable support for the PSK key exchange mode in TLS 1.3. If
 * MBEDTLS_SSL_PROTO_TLS1_3 is not enabled, this option does not have any
 * effect on the build.
 *
 */
#define MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED

/**
 * \def MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED
 *
 * Enable TLS 1.3 PSK ephemeral key exchange mode.
 *
 * Requires: PSA_WANT_ALG_ECDH or PSA_WANT_ALG_FFDH
 *
 * Comment to disable support for the PSK ephemeral key exchange mode in
 * TLS 1.3. If MBEDTLS_SSL_PROTO_TLS1_3 is not enabled, this option does not
 * have any effect on the build.
 *
 */
#define MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED

/**
 * \def MBEDTLS_SSL_TLS_C
 *
 * Enable the generic SSL/TLS code.
 *
 * Module:  library/ssl_tls.c
 * Caller:  library/ssl*_client.c
 *          library/ssl*_server.c
 *
 * Requires: PSA_WANT_ALG_SHA_256 or PSA_WANT_ALG_SHA_384
 *           and at least one of the MBEDTLS_SSL_PROTO_XXX defines
 *
 * This module is required for SSL/TLS.
 */
#define MBEDTLS_SSL_TLS_C

/**
 * \def MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH
 *
 * When this option is enabled, the SSL buffer will be resized automatically
 * based on the negotiated maximum fragment length in each direction.
 *
 * Requires: MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
 */
//#define MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH

//#define MBEDTLS_PSK_MAX_LEN               32 /**< Max size of TLS pre-shared keys, in bytes (default 256 or 384 bits) */
//#define MBEDTLS_SSL_CACHE_DEFAULT_MAX_ENTRIES      50 /**< Maximum entries in cache */
//#define MBEDTLS_SSL_CACHE_DEFAULT_TIMEOUT       86400 /**< 1 day  */

/** \def MBEDTLS_SSL_CID_IN_LEN_MAX
 *
 * The maximum length of CIDs used for incoming DTLS messages.
 *
 */
//#define MBEDTLS_SSL_CID_IN_LEN_MAX 32

/** \def MBEDTLS_SSL_CID_OUT_LEN_MAX
 *
 * The maximum length of CIDs used for outgoing DTLS messages.
 *
 */
//#define MBEDTLS_SSL_CID_OUT_LEN_MAX 32

/** \def MBEDTLS_SSL_CID_TLS1_3_PADDING_GRANULARITY
 *
 * This option controls the use of record plaintext padding
 * in TLS 1.3 and when using the Connection ID extension in DTLS 1.2.
 *
 * The padding will always be chosen so that the length of the
 * padded plaintext is a multiple of the value of this option.
 *
 * Note: A value of \c 1 means that no padding will be used
 *       for outgoing records.
 *
 * Note: On systems lacking division instructions,
 *       a power of two should be preferred.
 */
//#define MBEDTLS_SSL_CID_TLS1_3_PADDING_GRANULARITY 16

/**
 * Complete list of ciphersuites to use, in order of preference.
 *
 * \warning No dependency checking is done on that field! This option can only
 * be used to restrict the set of available ciphersuites. It is your
 * responsibility to make sure the needed modules are active.
 *
 * Use this to save a few hundred bytes of ROM (default ordering of all
 * available ciphersuites) and a few to a few hundred bytes of RAM.
 *
 * The value below is only an example, not the default.
 */
//#define MBEDTLS_SSL_CIPHERSUITES MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256

//#define MBEDTLS_SSL_COOKIE_TIMEOUT        60 /**< Default expiration delay of DTLS cookies, in seconds if HAVE_TIME, or in number of cookies issued */

/** \def MBEDTLS_SSL_DTLS_MAX_BUFFERING
 *
 * Maximum number of heap-allocated bytes for the purpose of
 * DTLS handshake message reassembly and future message buffering.
 *
 * This should be at least 9/8 * MBEDTLS_SSL_IN_CONTENT_LEN
 * to account for a reassembled handshake message of maximum size,
 * together with its reassembly bitmap.
 *
 * A value of 2 * MBEDTLS_SSL_IN_CONTENT_LEN (32768 by default)
 * should be sufficient for all practical situations as it allows
 * to reassembly a large handshake message (such as a certificate)
 * while buffering multiple smaller handshake messages.
 *
 */
//#define MBEDTLS_SSL_DTLS_MAX_BUFFERING             32768

/** \def MBEDTLS_SSL_IN_CONTENT_LEN
 *
 * Maximum length (in bytes) of incoming plaintext fragments.
 *
 * This determines the size of the incoming TLS I/O buffer in such a way
 * that it is capable of holding the specified amount of plaintext data,
 * regardless of the protection mechanism used.
 *
 * \note When using a value less than the default of 16KB on the client, it is
 *       recommended to use the Maximum Fragment Length (MFL) extension to
 *       inform the server about this limitation. On the server, there
 *       is no supported, standardized way of informing the client about
 *       restriction on the maximum size of incoming messages, and unless
 *       the limitation has been communicated by other means, it is recommended
 *       to only change the outgoing buffer size #MBEDTLS_SSL_OUT_CONTENT_LEN
 *       while keeping the default value of 16KB for the incoming buffer.
 *
 * Uncomment to set the maximum plaintext size of the incoming I/O buffer.
 */
//#define MBEDTLS_SSL_IN_CONTENT_LEN              16384

/**
 * \def MBEDTLS_SSL_MAX_EARLY_DATA_SIZE
 *
 * The default maximum amount of 0-RTT data. See the documentation of
 * \c mbedtls_ssl_conf_max_early_data_size() for more information.
 *
 * It must be positive and smaller than UINT32_MAX.
 *
 * If MBEDTLS_SSL_EARLY_DATA is not defined, this default value does not
 * have any impact on the build.
 */
//#define MBEDTLS_SSL_MAX_EARLY_DATA_SIZE        1024

/** \def MBEDTLS_SSL_OUT_CONTENT_LEN
 *
 * Maximum length (in bytes) of outgoing plaintext fragments.
 *
 * This determines the size of the outgoing TLS I/O buffer in such a way
 * that it is capable of holding the specified amount of plaintext data,
 * regardless of the protection mechanism used.
 *
 * It is possible to save RAM by setting a smaller outward buffer, while keeping
 * the default inward 16384 byte buffer to conform to the TLS specification.
 *
 * The minimum required outward buffer size is determined by the handshake
 * protocol's usage. Handshaking will fail if the outward buffer is too small.
 * The specific size requirement depends on the configured ciphers and any
 * certificate data which is sent during the handshake.
 *
 * Uncomment to set the maximum plaintext size of the outgoing I/O buffer.
 */
//#define MBEDTLS_SSL_OUT_CONTENT_LEN             16384

/**
 * \def MBEDTLS_SSL_TLS1_3_DEFAULT_NEW_SESSION_TICKETS
 *
 * Default number of NewSessionTicket messages to be sent by a TLS 1.3 server
 * after handshake completion. This is not used in TLS 1.2 and relevant only if
 * the MBEDTLS_SSL_SESSION_TICKETS option is enabled.
 *
 */
//#define MBEDTLS_SSL_TLS1_3_DEFAULT_NEW_SESSION_TICKETS 1

/**
 * \def MBEDTLS_SSL_TLS1_3_TICKET_AGE_TOLERANCE
 *
 * Maximum allowed ticket age difference in milliseconds tolerated between
 * server and client. Default value is 6000. This is not used in TLS 1.2.
 *
 * - The client ticket age is the time difference between the time when the
 *   client proposes to the server to use the ticket and the time the client
 *   received the ticket from the server.
 * - The server ticket age is the time difference between the time when the
 *   server receives a proposition from the client to use the ticket and the
 *   time when the ticket was created by the server.
 *
 * The ages might be different due to the client and server clocks not running
 * at the same pace. The typical accuracy of an RTC crystal is ±100 to ±20 parts
 * per million (360 to 72 milliseconds per hour). Default tolerance window is
 * 6s, thus in the worst case clients and servers must sync up their system time
 * every 6000/360/2~=8 hours.
 *
 * See section 8.3 of the TLS 1.3 specification(RFC 8446) for more information.
 */
//#define MBEDTLS_SSL_TLS1_3_TICKET_AGE_TOLERANCE 6000

/**
 * \def MBEDTLS_SSL_TLS1_3_TICKET_NONCE_LENGTH
 *
 * Size in bytes of a ticket nonce. This is not used in TLS 1.2.
 *
 * This must be less than 256.
 */
//#define MBEDTLS_SSL_TLS1_3_TICKET_NONCE_LENGTH 32

/** \} name SECTION: TLS feature selection */

/**
 * \name SECTION: X.509 feature selection
 *
 * This section sets Certificate related options.
 * \{
 */

/**
 * \def MBEDTLS_PKCS7_C
 *
 * Enable PKCS #7 core for using PKCS #7-formatted signatures.
 * RFC Link - https://tools.ietf.org/html/rfc2315
 *
 * Module:  library/pkcs7.c
 *
 * Requires: MBEDTLS_ASN1_PARSE_C, MBEDTLS_PK_PARSE_C,
 *           MBEDTLS_X509_CRT_PARSE_C MBEDTLS_X509_CRL_PARSE_C,
 *           MBEDTLS_MD_C
 *
 * This module is required for the PKCS #7 parsing modules.
 */
#define MBEDTLS_PKCS7_C

/**
 * \def MBEDTLS_X509_CREATE_C
 *
 * Enable X.509 core for creating certificates.
 *
 * Module:  library/x509_create.c
 *
 * Requires: MBEDTLS_ASN1_WRITE_C, MBEDTLS_PK_PARSE_C
 *
 * \warning You must call psa_crypto_init() before doing any X.509 operation.
 *
 * This module is the basis for creating X.509 certificates and CSRs.
 */
#define MBEDTLS_X509_CREATE_C

/**
 * \def MBEDTLS_X509_CRL_PARSE_C
 *
 * Enable X.509 CRL parsing.
 *
 * Module:  library/x509_crl.c
 * Caller:  library/x509_crt.c
 *
 * Requires: MBEDTLS_X509_USE_C
 *
 * This module is required for X.509 CRL parsing.
 */
#define MBEDTLS_X509_CRL_PARSE_C

/**
 * \def MBEDTLS_X509_CRT_PARSE_C
 *
 * Enable X.509 certificate parsing.
 *
 * Module:  library/x509_crt.c
 * Caller:  library/ssl_tls.c
 *          library/ssl*_client.c
 *          library/ssl*_server.c
 *
 * Requires: MBEDTLS_X509_USE_C
 *
 * This module is required for X.509 certificate parsing.
 */
#define MBEDTLS_X509_CRT_PARSE_C

/**
 * \def MBEDTLS_X509_CRT_WRITE_C
 *
 * Enable creating X.509 certificates.
 *
 * Module:  library/x509_crt_write.c
 *
 * Requires: MBEDTLS_X509_CREATE_C
 *
 * This module is required for X.509 certificate creation.
 */
#define MBEDTLS_X509_CRT_WRITE_C

/**
 * \def MBEDTLS_X509_CSR_PARSE_C
 *
 * Enable X.509 Certificate Signing Request (CSR) parsing.
 *
 * Module:  library/x509_csr.c
 * Caller:  library/x509_crt_write.c
 *
 * Requires: MBEDTLS_X509_USE_C
 *
 * This module is used for reading X.509 certificate request.
 */
#define MBEDTLS_X509_CSR_PARSE_C

/**
 * \def MBEDTLS_X509_CSR_WRITE_C
 *
 * Enable creating X.509 Certificate Signing Requests (CSR).
 *
 * Module:  library/x509_csr_write.c
 *
 * Requires: MBEDTLS_X509_CREATE_C
 *
 * This module is required for X.509 certificate request writing.
 */
#define MBEDTLS_X509_CSR_WRITE_C

/**
 * \def MBEDTLS_X509_REMOVE_INFO
 *
 * Disable mbedtls_x509_*_info() and related APIs.
 *
 * Uncomment to omit mbedtls_x509_*_info(), as well as mbedtls_debug_print_crt()
 * and other functions/constants only used by these functions, thus reducing
 * the code footprint by several KB.
 */
//#define MBEDTLS_X509_REMOVE_INFO

/**
 * \def MBEDTLS_X509_RSASSA_PSS_SUPPORT
 *
 * Enable parsing and verification of X.509 certificates, CRLs and CSRS
 * signed with RSASSA-PSS (aka PKCS#1 v2.1).
 *
 * Requires: PSA_WANT_ALG_RSA_PSS
 *
 * Comment this macro to disallow using RSASSA-PSS in certificates.
 */
#define MBEDTLS_X509_RSASSA_PSS_SUPPORT

/**
 * \def MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK
 *
 * If set, this enables the X.509 API `mbedtls_x509_crt_verify_with_ca_cb()`
 * and the SSL API `mbedtls_ssl_conf_ca_cb()` which allow users to configure
 * the set of trusted certificates through a callback instead of a linked
 * list.
 *
 * This is useful for example in environments where a large number of trusted
 * certificates is present and storing them in a linked list isn't efficient
 * enough, or when the set of trusted certificates changes frequently.
 *
 * See the documentation of `mbedtls_x509_crt_verify_with_ca_cb()` and
 * `mbedtls_ssl_conf_ca_cb()` for more information.
 *
 * Requires: MBEDTLS_X509_CRT_PARSE_C
 *
 * Uncomment to enable trusted certificate callbacks.
 */
//#define MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK

/**
 * \def MBEDTLS_X509_USE_C
 *
 * Enable X.509 core for using certificates.
 *
 * Module:  library/x509.c
 * Caller:  library/x509_crl.c
 *          library/x509_crt.c
 *          library/x509_csr.c
 *
 * Requires: MBEDTLS_ASN1_PARSE_C, MBEDTLS_PK_PARSE_C
 *
 * \warning You must call psa_crypto_init() before doing any X.509 operation.
 *
 * This module is required for the X.509 parsing modules.
 */
#define MBEDTLS_X509_USE_C

//#define MBEDTLS_X509_MAX_FILE_PATH_LEN     512 /**< Maximum length of a path/filename string in bytes including the null terminator character ('\0'). */
//#define MBEDTLS_X509_MAX_INTERMEDIATE_CA   8   /**< Maximum number of intermediate CAs in a verification chain. */

/** \} name SECTION: X.509 feature selection */
