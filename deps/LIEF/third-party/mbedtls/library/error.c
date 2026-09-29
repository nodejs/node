/*
 *  Error message information
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "tf_psa_crypto_common.h"

#include "mbedtls/error.h"

#if defined(MBEDTLS_ERROR_C) || defined(MBEDTLS_ERROR_STRERROR_DUMMY)

#if defined(MBEDTLS_ERROR_C)

#include "mbedtls/platform.h"

#include <stdio.h>
#include <string.h>

#if defined(MBEDTLS_NET_C)
#include "mbedtls/net_sockets.h"
#endif

#if defined(MBEDTLS_PKCS7_C)
#include "mbedtls/pkcs7.h"
#endif

#if defined(MBEDTLS_SSL_TLS_C)
#include "mbedtls/ssl.h"
#endif

#if defined(MBEDTLS_X509_USE_C) || \
    defined(MBEDTLS_X509_CREATE_C)
#include "mbedtls/x509.h"
#endif

#if defined(MBEDTLS_AES_C)
#include "mbedtls/private/aes.h"
#endif

#if defined(MBEDTLS_ARIA_C)
#include "mbedtls/private/aria.h"
#endif

#if defined(MBEDTLS_BIGNUM_C)
#include "mbedtls/private/bignum.h"
#endif

#if defined(MBEDTLS_CAMELLIA_C)
#include "mbedtls/private/camellia.h"
#endif

#if defined(MBEDTLS_CHACHAPOLY_C)
#include "mbedtls/private/chachapoly.h"
#endif

#if defined(MBEDTLS_CIPHER_C)
#include "mbedtls/private/cipher.h"
#endif

#if defined(MBEDTLS_CTR_DRBG_C)
#include "mbedtls/private/ctr_drbg.h"
#endif

#if defined(MBEDTLS_ECP_C)
#include "mbedtls/private/ecp.h"
#endif

#if defined(MBEDTLS_ENTROPY_C)
#include "mbedtls/private/entropy.h"
#endif

#if defined(MBEDTLS_HMAC_DRBG_C)
#include "mbedtls/private/hmac_drbg.h"
#endif

#if defined(MBEDTLS_PKCS5_C)
#include "mbedtls/private/pkcs5.h"
#endif

#if defined(MBEDTLS_RSA_C)
#include "mbedtls/private/rsa.h"
#endif


static const char *mbedtls_high_level_strerr(int error_code)
{
    int high_level_error_code;

    if (error_code < 0) {
        error_code = -error_code;
    }

    /* Extract the high-level part from the error code. */
    high_level_error_code = error_code & 0xFF80;

    switch (high_level_error_code) {
    /* Begin Auto-Generated Code. */
#if defined(MBEDTLS_PKCS7_C)
        case -(MBEDTLS_ERR_PKCS7_INVALID_FORMAT):
            return( "PKCS7 - The format is invalid, e.g. different type expected" );
        case -(MBEDTLS_ERR_PKCS7_FEATURE_UNAVAILABLE):
            return( "PKCS7 - Unavailable feature, e.g. anything other than signed data" );
        case -(MBEDTLS_ERR_PKCS7_INVALID_VERSION):
            return( "PKCS7 - The PKCS #7 version element is invalid or cannot be parsed" );
        case -(MBEDTLS_ERR_PKCS7_INVALID_CONTENT_INFO):
            return( "PKCS7 - The PKCS #7 content info is invalid or cannot be parsed" );
        case -(MBEDTLS_ERR_PKCS7_INVALID_ALG):
            return( "PKCS7 - The algorithm tag or value is invalid or cannot be parsed" );
        case -(MBEDTLS_ERR_PKCS7_INVALID_CERT):
            return( "PKCS7 - The certificate tag or value is invalid or cannot be parsed" );
        case -(MBEDTLS_ERR_PKCS7_INVALID_SIGNATURE):
            return( "PKCS7 - Error parsing the signature" );
        case -(MBEDTLS_ERR_PKCS7_INVALID_SIGNER_INFO):
            return( "PKCS7 - Error parsing the signer's info" );
        case -(MBEDTLS_ERR_PKCS7_CERT_DATE_INVALID):
            return( "PKCS7 - The PKCS #7 date issued/expired dates are invalid" );
#endif /* MBEDTLS_PKCS7_C */

#if defined(MBEDTLS_SSL_TLS_C)
        case -(MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS):
            return( "SSL - A cryptographic operation is in progress. Try again later" );
        case -(MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE):
            return( "SSL - The requested feature is not available" );
        case -(MBEDTLS_ERR_SSL_INVALID_MAC):
            return( "SSL - Verification of the message MAC failed" );
        case -(MBEDTLS_ERR_SSL_INVALID_RECORD):
            return( "SSL - An invalid SSL record was received" );
        case -(MBEDTLS_ERR_SSL_CONN_EOF):
            return( "SSL - The connection indicated an EOF" );
        case -(MBEDTLS_ERR_SSL_DECODE_ERROR):
            return( "SSL - A message could not be parsed due to a syntactic error" );
        case -(MBEDTLS_ERR_SSL_NO_RNG):
            return( "SSL - No RNG was provided to the SSL module" );
        case -(MBEDTLS_ERR_SSL_NO_CLIENT_CERTIFICATE):
            return( "SSL - No client certification received from the client, but required by the authentication mode" );
        case -(MBEDTLS_ERR_SSL_UNSUPPORTED_EXTENSION):
            return( "SSL - Client received an extended server hello containing an unsupported extension" );
        case -(MBEDTLS_ERR_SSL_NO_APPLICATION_PROTOCOL):
            return( "SSL - No ALPN protocols supported that the client advertises" );
        case -(MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED):
            return( "SSL - The own private key or pre-shared key is not set, but needed" );
        case -(MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED):
            return( "SSL - No CA Chain is set, but required to operate" );
        case -(MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE):
            return( "SSL - An unexpected message was received from our peer" );
        case -(MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE):
            return( "SSL - A fatal alert message was received from our peer" );
        case -(MBEDTLS_ERR_SSL_UNRECOGNIZED_NAME):
            return( "SSL - No server could be identified matching the client's SNI" );
        case -(MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY):
            return( "SSL - The peer notified us that the connection is going to be closed" );
        case -(MBEDTLS_ERR_SSL_BAD_CERTIFICATE):
            return( "SSL - Processing of the Certificate handshake message failed" );
        case -(MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET):
            return( "SSL - * Received NewSessionTicket Post Handshake Message. This error code is experimental and may be changed or removed without notice" );
        case -(MBEDTLS_ERR_SSL_CANNOT_READ_EARLY_DATA):
            return( "SSL - Not possible to read early data" );
        case -(MBEDTLS_ERR_SSL_RECEIVED_EARLY_DATA):
            return( "SSL - * Early data has been received as part of an on-going handshake. This error code can be returned only on server side if and only if early data has been enabled by means of the mbedtls_ssl_conf_early_data() API. This error code can then be returned by mbedtls_ssl_handshake(), mbedtls_ssl_handshake_step(), mbedtls_ssl_read() or mbedtls_ssl_write() if early data has been received as part of the handshake sequence they triggered. To read the early data, call mbedtls_ssl_read_early_data()" );
        case -(MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA):
            return( "SSL - Not possible to write early data" );
        case -(MBEDTLS_ERR_SSL_CACHE_ENTRY_NOT_FOUND):
            return( "SSL - Cache entry not found" );
        case -(MBEDTLS_ERR_SSL_HW_ACCEL_FAILED):
            return( "SSL - Hardware acceleration function returned with error" );
        case -(MBEDTLS_ERR_SSL_HW_ACCEL_FALLTHROUGH):
            return( "SSL - Hardware acceleration function skipped / left alone data" );
        case -(MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION):
            return( "SSL - Handshake protocol not within min/max boundaries" );
        case -(MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE):
            return( "SSL - The handshake negotiation failed" );
        case -(MBEDTLS_ERR_SSL_SESSION_TICKET_EXPIRED):
            return( "SSL - Session ticket has expired" );
        case -(MBEDTLS_ERR_SSL_PK_TYPE_MISMATCH):
            return( "SSL - Public key type mismatch (eg, asked for RSA key exchange and presented EC key)" );
        case -(MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY):
            return( "SSL - Unknown identity received (eg, PSK identity)" );
        case -(MBEDTLS_ERR_SSL_INTERNAL_ERROR):
            return( "SSL - Internal error (eg, unexpected failure in lower-level module)" );
        case -(MBEDTLS_ERR_SSL_COUNTER_WRAPPING):
            return( "SSL - A counter would wrap (eg, too many messages exchanged)" );
        case -(MBEDTLS_ERR_SSL_WAITING_SERVER_HELLO_RENEGO):
            return( "SSL - Unexpected message at ServerHello in renegotiation" );
        case -(MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED):
            return( "SSL - DTLS client must retry for hello verification" );
        case -(MBEDTLS_ERR_SSL_WANT_READ):
            return( "SSL - No data of requested type currently available on underlying transport" );
        case -(MBEDTLS_ERR_SSL_WANT_WRITE):
            return( "SSL - Connection requires a write call" );
        case -(MBEDTLS_ERR_SSL_TIMEOUT):
            return( "SSL - The operation timed out" );
        case -(MBEDTLS_ERR_SSL_CLIENT_RECONNECT):
            return( "SSL - The client initiated a reconnect from the same port" );
        case -(MBEDTLS_ERR_SSL_UNEXPECTED_RECORD):
            return( "SSL - Record header looks valid but is not expected" );
        case -(MBEDTLS_ERR_SSL_NON_FATAL):
            return( "SSL - The alert message received indicates a non-fatal error" );
        case -(MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER):
            return( "SSL - A field in a message was incorrect or inconsistent with other fields" );
        case -(MBEDTLS_ERR_SSL_CONTINUE_PROCESSING):
            return( "SSL - Internal-only message signaling that further message-processing should be done" );
        case -(MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS):
            return( "SSL - The asynchronous operation is not completed yet" );
        case -(MBEDTLS_ERR_SSL_EARLY_MESSAGE):
            return( "SSL - Internal-only message signaling that a message arrived early" );
        case -(MBEDTLS_ERR_SSL_UNEXPECTED_CID):
            return( "SSL - An encrypted DTLS-frame with an unexpected CID was received" );
        case -(MBEDTLS_ERR_SSL_VERSION_MISMATCH):
            return( "SSL - An operation failed due to an unexpected version or configuration" );
        case -(MBEDTLS_ERR_SSL_BAD_CONFIG):
            return( "SSL - Invalid value in SSL config" );
        case -(MBEDTLS_ERR_SSL_CERTIFICATE_VERIFICATION_WITHOUT_HOSTNAME):
            return( "SSL - Attempt to verify a certificate without an expected hostname. This is usually insecure.  In TLS clients, when a client authenticates a server through its certificate, the client normally checks three things: - the certificate chain must be valid; - the chain must start from a trusted CA; - the certificate must cover the server name that is expected by the client.  Omitting any of these checks is generally insecure, and can allow a malicious server to impersonate a legitimate server.  The third check may be safely skipped in some unusual scenarios, such as networks where eavesdropping is a risk but not active attacks, or a private PKI where the client equally trusts all servers that are accredited by the root CA.  You should call mbedtls_ssl_set_hostname() with the expected server name before starting a TLS handshake on a client (unless the client is set up to only use PSK-based authentication, which does not rely on the host name). If you have determined that server name verification is not required for security in your scenario, call mbedtls_ssl_set_hostname() with \\p NULL as the server name.  This error is raised if all of the following conditions are met:  - A TLS client is configured with the authentication mode #MBEDTLS_SSL_VERIFY_REQUIRED (default). - Certificate authentication is enabled. - The client does not call mbedtls_ssl_set_hostname()" );
#endif /* MBEDTLS_SSL_TLS_C */

#if defined(MBEDTLS_X509_USE_C) || \
    defined(MBEDTLS_X509_CREATE_C)
        case -(MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE):
            return( "X509 - Unavailable feature, e.g. RSA hashing/encryption combination" );
        case -(MBEDTLS_ERR_X509_UNKNOWN_OID):
            return( "X509 - Requested OID is unknown" );
        case -(MBEDTLS_ERR_X509_INVALID_FORMAT):
            return( "X509 - The CRT/CRL/CSR format is invalid, e.g. different type expected" );
        case -(MBEDTLS_ERR_X509_INVALID_VERSION):
            return( "X509 - The CRT/CRL/CSR version element is invalid" );
        case -(MBEDTLS_ERR_X509_INVALID_SERIAL):
            return( "X509 - The serial tag or value is invalid" );
        case -(MBEDTLS_ERR_X509_INVALID_ALG):
            return( "X509 - The algorithm tag or value is invalid" );
        case -(MBEDTLS_ERR_X509_INVALID_NAME):
            return( "X509 - The name tag or value is invalid" );
        case -(MBEDTLS_ERR_X509_INVALID_DATE):
            return( "X509 - The date tag or value is invalid" );
        case -(MBEDTLS_ERR_X509_INVALID_SIGNATURE):
            return( "X509 - The signature tag or value invalid" );
        case -(MBEDTLS_ERR_X509_INVALID_EXTENSIONS):
            return( "X509 - The extension tag or value is invalid" );
        case -(MBEDTLS_ERR_X509_UNKNOWN_VERSION):
            return( "X509 - CRT/CRL/CSR has an unsupported version number" );
        case -(MBEDTLS_ERR_X509_UNKNOWN_SIG_ALG):
            return( "X509 - Signature algorithm (oid) is unsupported" );
        case -(MBEDTLS_ERR_X509_SIG_MISMATCH):
            return( "X509 - Signature algorithms do not match. (see \\c ::mbedtls_x509_crt sig_oid)" );
        case -(MBEDTLS_ERR_X509_CERT_VERIFY_FAILED):
            return( "X509 - Certificate verification failed, e.g. CRL, CA or signature check failed" );
        case -(MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT):
            return( "X509 - Format not recognized as DER or PEM" );
        case -(MBEDTLS_ERR_X509_BAD_INPUT_DATA):
            return( "X509 - Input invalid" );
        case -(MBEDTLS_ERR_X509_FILE_IO_ERROR):
            return( "X509 - Read/write of file failed" );
        case -(MBEDTLS_ERR_X509_FATAL_ERROR):
            return( "X509 - A fatal error occurred, eg the chain is too long or the vrfy callback failed" );
#endif /* MBEDTLS_X509_USE_C || 
          MBEDTLS_X509_CREATE_C */

#if defined(MBEDTLS_CIPHER_C)
        case -(MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE):
            return( "CIPHER - The selected feature is not available" );
        case -(MBEDTLS_ERR_CIPHER_FULL_BLOCK_EXPECTED):
            return( "CIPHER - Decryption of block requires a full block" );
        case -(MBEDTLS_ERR_CIPHER_INVALID_CONTEXT):
            return( "CIPHER - The context is invalid. For example, because it was freed" );
#endif /* MBEDTLS_CIPHER_C */

#if defined(MBEDTLS_ECP_C)
        case -(MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE):
            return( "ECP - The requested feature is not available, for example, the requested curve is not supported" );
        case -(MBEDTLS_ERR_ECP_RANDOM_FAILED):
            return( "ECP - Generation of random value, such as ephemeral key, failed" );
        case -(MBEDTLS_ERR_ECP_INVALID_KEY):
            return( "ECP - Invalid private or public key" );
#endif /* MBEDTLS_ECP_C */

#if defined(MBEDTLS_PKCS5_C)
        case -(MBEDTLS_ERR_PKCS5_INVALID_FORMAT):
            return( "PKCS5 - Unexpected ASN.1 data" );
        case -(MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE):
            return( "PKCS5 - Requested encryption or digest alg not available" );
        case -(MBEDTLS_ERR_PKCS5_PASSWORD_MISMATCH):
            return( "PKCS5 - Given private key password does not allow for correct decryption" );
#endif /* MBEDTLS_PKCS5_C */

#if defined(MBEDTLS_RSA_C)
        case -(MBEDTLS_ERR_RSA_KEY_GEN_FAILED):
            return( "RSA - Something failed during generation of a key" );
        case -(MBEDTLS_ERR_RSA_KEY_CHECK_FAILED):
            return( "RSA - Key failed to pass the validity check of the library" );
        case -(MBEDTLS_ERR_RSA_PUBLIC_FAILED):
            return( "RSA - The public key operation failed" );
        case -(MBEDTLS_ERR_RSA_PRIVATE_FAILED):
            return( "RSA - The private key operation failed" );
        case -(MBEDTLS_ERR_RSA_RNG_FAILED):
            return( "RSA - The random generator failed to generate non-zeros" );
#endif /* MBEDTLS_RSA_C */
        /* End Auto-Generated Code. */

        default:
            break;
    }

    return NULL;
}

static const char *mbedtls_low_level_strerr(int error_code)
{
    int low_level_error_code;

    if (error_code < 0) {
        error_code = -error_code;
    }

    /* Extract the low-level part from the error code. */
    low_level_error_code = error_code & ~0xFF80;

    switch (low_level_error_code) {
    /* Begin Auto-Generated Code. */
#if defined(MBEDTLS_NET_C)
        case -(MBEDTLS_ERR_NET_SOCKET_FAILED):
            return( "NET - Failed to open a socket" );
        case -(MBEDTLS_ERR_NET_CONNECT_FAILED):
            return( "NET - The connection to the given server / port failed" );
        case -(MBEDTLS_ERR_NET_BIND_FAILED):
            return( "NET - Binding of the socket failed" );
        case -(MBEDTLS_ERR_NET_LISTEN_FAILED):
            return( "NET - Could not listen on the socket" );
        case -(MBEDTLS_ERR_NET_ACCEPT_FAILED):
            return( "NET - Could not accept the incoming connection" );
        case -(MBEDTLS_ERR_NET_RECV_FAILED):
            return( "NET - Reading information from the socket failed" );
        case -(MBEDTLS_ERR_NET_SEND_FAILED):
            return( "NET - Sending information through the socket failed" );
        case -(MBEDTLS_ERR_NET_CONN_RESET):
            return( "NET - Connection was reset by peer" );
        case -(MBEDTLS_ERR_NET_UNKNOWN_HOST):
            return( "NET - Failed to get an IP address for the given hostname" );
        case -(MBEDTLS_ERR_NET_INVALID_CONTEXT):
            return( "NET - The context is invalid, eg because it was free()ed" );
        case -(MBEDTLS_ERR_NET_POLL_FAILED):
            return( "NET - Polling the net context failed" );
        case -(MBEDTLS_ERR_NET_BAD_INPUT_DATA):
            return( "NET - Input invalid" );
#endif /* MBEDTLS_NET_C */

#if defined(MBEDTLS_AES_C)
        case -(MBEDTLS_ERR_AES_INVALID_KEY_LENGTH):
            return( "AES - Invalid key length" );
        case -(MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH):
            return( "AES - Invalid data input length" );
#endif /* MBEDTLS_AES_C */

#if defined(MBEDTLS_ARIA_C)
        case -(MBEDTLS_ERR_ARIA_INVALID_INPUT_LENGTH):
            return( "ARIA - Invalid data input length" );
#endif /* MBEDTLS_ARIA_C */

#if defined(MBEDTLS_BIGNUM_C)
        case -(MBEDTLS_ERR_MPI_FILE_IO_ERROR):
            return( "BIGNUM - An error occurred while reading from or writing to a file" );
        case -(MBEDTLS_ERR_MPI_INVALID_CHARACTER):
            return( "BIGNUM - There is an invalid character in the digit string" );
        case -(MBEDTLS_ERR_MPI_NEGATIVE_VALUE):
            return( "BIGNUM - The input arguments are negative or result in illegal output" );
        case -(MBEDTLS_ERR_MPI_DIVISION_BY_ZERO):
            return( "BIGNUM - The input argument for division is zero, which is not allowed" );
        case -(MBEDTLS_ERR_MPI_NOT_ACCEPTABLE):
            return( "BIGNUM - The input arguments are not acceptable" );
#endif /* MBEDTLS_BIGNUM_C */

#if defined(MBEDTLS_CAMELLIA_C)
        case -(MBEDTLS_ERR_CAMELLIA_INVALID_INPUT_LENGTH):
            return( "CAMELLIA - Invalid data input length" );
#endif /* MBEDTLS_CAMELLIA_C */

#if defined(MBEDTLS_CHACHAPOLY_C)
        case -(MBEDTLS_ERR_CHACHAPOLY_BAD_STATE):
            return( "CHACHAPOLY - The requested operation is not permitted in the current state" );
#endif /* MBEDTLS_CHACHAPOLY_C */

#if defined(MBEDTLS_CTR_DRBG_C)
        case -(MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED):
            return( "CTR_DRBG - The entropy source failed" );
        case -(MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG):
            return( "CTR_DRBG - The requested random buffer length is too big" );
        case -(MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG):
            return( "CTR_DRBG - The input (entropy + additional data) is too large" );
        case -(MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR):
            return( "CTR_DRBG - Read or write error in file" );
#endif /* MBEDTLS_CTR_DRBG_C */

#if defined(MBEDTLS_ENTROPY_C)
        case -(MBEDTLS_ERR_ENTROPY_MAX_SOURCES):
            return( "ENTROPY - No more sources can be added" );
        case -(MBEDTLS_ERR_ENTROPY_NO_SOURCES_DEFINED):
            return( "ENTROPY - No sources have been added to poll" );
        case -(MBEDTLS_ERR_ENTROPY_NO_STRONG_SOURCE):
            return( "ENTROPY - No strong sources have been added to poll" );
        case -(MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR):
            return( "ENTROPY - Read/write error in file" );
#endif /* MBEDTLS_ENTROPY_C */

#if defined(MBEDTLS_HMAC_DRBG_C)
        case -(MBEDTLS_ERR_HMAC_DRBG_REQUEST_TOO_BIG):
            return( "HMAC_DRBG - Too many random requested in single call" );
        case -(MBEDTLS_ERR_HMAC_DRBG_INPUT_TOO_BIG):
            return( "HMAC_DRBG - Input too large (Entropy + additional)" );
        case -(MBEDTLS_ERR_HMAC_DRBG_FILE_IO_ERROR):
            return( "HMAC_DRBG - Read/write error in file" );
        case -(MBEDTLS_ERR_HMAC_DRBG_ENTROPY_SOURCE_FAILED):
            return( "HMAC_DRBG - The entropy source failed" );
#endif /* MBEDTLS_HMAC_DRBG_C */
        /* End Auto-Generated Code. */

        default:
            break;
    }

    return NULL;
}

void mbedtls_strerror(int ret, char *buf, size_t buflen)
{
    size_t len;
    int use_ret;
    const char *high_level_error_description = NULL;
    const char *low_level_error_description = NULL;

    if (buflen == 0) {
        return;
    }

    memset(buf, 0x00, buflen);

    if (ret < 0) {
        ret = -ret;
    }

    if (ret & 0xFF80) {
        use_ret = ret & 0xFF80;

        // Translate high level error code.
        high_level_error_description = mbedtls_high_level_strerr(ret);

        if (high_level_error_description == NULL) {
            mbedtls_snprintf(buf, buflen, "UNKNOWN ERROR CODE (%04X)", (unsigned int) use_ret);
        } else {
            mbedtls_snprintf(buf, buflen, "%s", high_level_error_description);
        }

#if defined(MBEDTLS_SSL_TLS_C)
        // Early return in case of a fatal error - do not try to translate low
        // level code.
        if (use_ret == -(MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)) {
            return;
        }
#endif /* MBEDTLS_SSL_TLS_C */
    }

    use_ret = ret & ~0xFF80;

    if (use_ret == 0) {
        return;
    }

    // If high level code is present, make a concatenation between both
    // error strings.
    //
    len = strlen(buf);

    if (len > 0) {
        if (buflen - len < 5) {
            return;
        }

        mbedtls_snprintf(buf + len, buflen - len, " : ");

        buf += len + 3;
        buflen -= len + 3;
    }

    // Translate low level error code.
    low_level_error_description = mbedtls_low_level_strerr(ret);

    if (low_level_error_description == NULL) {
        mbedtls_snprintf(buf, buflen, "UNKNOWN ERROR CODE (%04X)", (unsigned int) use_ret);
    } else {
        mbedtls_snprintf(buf, buflen, "%s", low_level_error_description);
    }
}

#else /* MBEDTLS_ERROR_C */

/*
 * Provide a dummy implementation when MBEDTLS_ERROR_C is not defined
 */
void mbedtls_strerror(int ret, char *buf, size_t buflen)
{
    ((void) ret);

    if (buflen > 0) {
        buf[0] = '\0';
    }
}

#endif /* MBEDTLS_ERROR_C */

#endif /* MBEDTLS_ERROR_C || MBEDTLS_ERROR_STRERROR_DUMMY */
