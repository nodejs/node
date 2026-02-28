/*
 *  Error message information
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#include "mbedtls/error.h"

#if defined(MBEDTLS_ERROR_C) || defined(MBEDTLS_ERROR_STRERROR_DUMMY)

#if defined(MBEDTLS_ERROR_C)

#include "mbedtls/platform.h"

#include <stdio.h>
#include <string.h>

#if defined(MBEDTLS_AES_C)
#include "mbedtls/aes.h"
#endif

#if defined(MBEDTLS_ARIA_C)
#include "mbedtls/aria.h"
#endif

#if defined(MBEDTLS_ASN1_PARSE_C)
#include "mbedtls/asn1.h"
#endif

#if defined(MBEDTLS_BASE64_C)
#include "mbedtls/base64.h"
#endif

#if defined(MBEDTLS_BIGNUM_C)
#include "mbedtls/bignum.h"
#endif

#if defined(MBEDTLS_CAMELLIA_C)
#include "mbedtls/camellia.h"
#endif

#if defined(MBEDTLS_CCM_C)
#include "mbedtls/ccm.h"
#endif

#if defined(MBEDTLS_CHACHA20_C)
#include "mbedtls/chacha20.h"
#endif

#if defined(MBEDTLS_CHACHAPOLY_C)
#include "mbedtls/chachapoly.h"
#endif

#if defined(MBEDTLS_CIPHER_C)
#include "mbedtls/cipher.h"
#endif

#if defined(MBEDTLS_CTR_DRBG_C)
#include "mbedtls/ctr_drbg.h"
#endif

#if defined(MBEDTLS_DES_C)
#include "mbedtls/des.h"
#endif

#if defined(MBEDTLS_DHM_C)
#include "mbedtls/dhm.h"
#endif

#if defined(MBEDTLS_ECP_C)
#include "mbedtls/ecp.h"
#endif

#if defined(MBEDTLS_ENTROPY_C)
#include "mbedtls/entropy.h"
#endif

#if defined(MBEDTLS_ERROR_C)
#include "mbedtls/error.h"
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#endif

#if defined(MBEDTLS_GCM_C)
#include "mbedtls/gcm.h"
#endif

#if defined(MBEDTLS_HKDF_C)
#include "mbedtls/hkdf.h"
#endif

#if defined(MBEDTLS_HMAC_DRBG_C)
#include "mbedtls/hmac_drbg.h"
#endif

#if defined(MBEDTLS_LMS_C)
#include "mbedtls/lms.h"
#endif

#if defined(MBEDTLS_MD_C)
#include "mbedtls/md.h"
#endif

#if defined(MBEDTLS_NET_C)
#include "mbedtls/net_sockets.h"
#endif

#if defined(MBEDTLS_OID_C)
#include "mbedtls/oid.h"
#endif

#if defined(MBEDTLS_PEM_PARSE_C) || defined(MBEDTLS_PEM_WRITE_C)
#include "mbedtls/pem.h"
#endif

#if defined(MBEDTLS_PK_C)
#include "mbedtls/pk.h"
#endif

#if defined(MBEDTLS_PKCS12_C)
#include "mbedtls/pkcs12.h"
#endif

#if defined(MBEDTLS_PKCS5_C)
#include "mbedtls/pkcs5.h"
#endif

#if defined(MBEDTLS_PKCS7_C)
#include "mbedtls/pkcs7.h"
#endif

#if defined(MBEDTLS_POLY1305_C)
#include "mbedtls/poly1305.h"
#endif

#if defined(MBEDTLS_RSA_C)
#include "mbedtls/rsa.h"
#endif

#if defined(MBEDTLS_SHA1_C)
#include "mbedtls/sha1.h"
#endif

#if defined(MBEDTLS_SHA256_C)
#include "mbedtls/sha256.h"
#endif

#if defined(MBEDTLS_SHA3_C)
#include "mbedtls/sha3.h"
#endif

#if defined(MBEDTLS_SHA512_C)
#include "mbedtls/sha512.h"
#endif

#if defined(MBEDTLS_SSL_TLS_C)
#include "mbedtls/ssl.h"
#endif

#if defined(MBEDTLS_THREADING_C)
#include "mbedtls/threading.h"
#endif

#if defined(MBEDTLS_X509_USE_C) || defined(MBEDTLS_X509_CREATE_C)
#include "mbedtls/x509.h"
#endif


const char *mbedtls_high_level_strerr(int error_code)
{
    int high_level_error_code;

    if (error_code < 0) {
        error_code = -error_code;
    }

    /* Extract the high-level part from the error code. */
    high_level_error_code = error_code & 0xFF80;

    switch (high_level_error_code) {
    /* Begin Auto-Generated Code. */
    #if defined(MBEDTLS_CIPHER_C)
        case -(MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE):
            return( "CIPHER - The selected feature is not available" );
        case -(MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA):
            return( "CIPHER - Bad input parameters" );
        case -(MBEDTLS_ERR_CIPHER_ALLOC_FAILED):
            return( "CIPHER - Failed to allocate memory" );
        case -(MBEDTLS_ERR_CIPHER_INVALID_PADDING):
            return( "CIPHER - Input data contains invalid padding and is rejected" );
        case -(MBEDTLS_ERR_CIPHER_FULL_BLOCK_EXPECTED):
            return( "CIPHER - Decryption of block requires a full block" );
        case -(MBEDTLS_ERR_CIPHER_AUTH_FAILED):
            return( "CIPHER - Authentication failed (for AEAD modes)" );
        case -(MBEDTLS_ERR_CIPHER_INVALID_CONTEXT):
            return( "CIPHER - The context is invalid. For example, because it was freed" );
#endif /* MBEDTLS_CIPHER_C */

#if defined(MBEDTLS_DHM_C)
        case -(MBEDTLS_ERR_DHM_BAD_INPUT_DATA):
            return( "DHM - Bad input parameters" );
        case -(MBEDTLS_ERR_DHM_READ_PARAMS_FAILED):
            return( "DHM - Reading of the DHM parameters failed" );
        case -(MBEDTLS_ERR_DHM_MAKE_PARAMS_FAILED):
            return( "DHM - Making of the DHM parameters failed" );
        case -(MBEDTLS_ERR_DHM_READ_PUBLIC_FAILED):
            return( "DHM - Reading of the public values failed" );
        case -(MBEDTLS_ERR_DHM_MAKE_PUBLIC_FAILED):
            return( "DHM - Making of the public value failed" );
        case -(MBEDTLS_ERR_DHM_CALC_SECRET_FAILED):
            return( "DHM - Calculation of the DHM secret failed" );
        case -(MBEDTLS_ERR_DHM_INVALID_FORMAT):
            return( "DHM - The ASN.1 data is not formatted correctly" );
        case -(MBEDTLS_ERR_DHM_ALLOC_FAILED):
            return( "DHM - Allocation of memory failed" );
        case -(MBEDTLS_ERR_DHM_FILE_IO_ERROR):
            return( "DHM - Read or write of file failed" );
        case -(MBEDTLS_ERR_DHM_SET_GROUP_FAILED):
            return( "DHM - Setting the modulus and generator failed" );
#endif /* MBEDTLS_DHM_C */

#if defined(MBEDTLS_ECP_C)
        case -(MBEDTLS_ERR_ECP_BAD_INPUT_DATA):
            return( "ECP - Bad input parameters to function" );
        case -(MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL):
            return( "ECP - The buffer is too small to write to" );
        case -(MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE):
            return( "ECP - The requested feature is not available, for example, the requested curve is not supported" );
        case -(MBEDTLS_ERR_ECP_VERIFY_FAILED):
            return( "ECP - The signature is not valid" );
        case -(MBEDTLS_ERR_ECP_ALLOC_FAILED):
            return( "ECP - Memory allocation failed" );
        case -(MBEDTLS_ERR_ECP_RANDOM_FAILED):
            return( "ECP - Generation of random value, such as ephemeral key, failed" );
        case -(MBEDTLS_ERR_ECP_INVALID_KEY):
            return( "ECP - Invalid private or public key" );
        case -(MBEDTLS_ERR_ECP_SIG_LEN_MISMATCH):
            return( "ECP - The buffer contains a valid signature followed by more data" );
        case -(MBEDTLS_ERR_ECP_IN_PROGRESS):
            return( "ECP - Operation in progress, call again with the same parameters to continue" );
#endif /* MBEDTLS_ECP_C */

#if defined(MBEDTLS_MD_C)
        case -(MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE):
            return( "MD - The selected feature is not available" );
        case -(MBEDTLS_ERR_MD_BAD_INPUT_DATA):
            return( "MD - Bad input parameters to function" );
        case -(MBEDTLS_ERR_MD_ALLOC_FAILED):
            return( "MD - Failed to allocate memory" );
        case -(MBEDTLS_ERR_MD_FILE_IO_ERROR):
            return( "MD - Opening or reading of file failed" );
#endif /* MBEDTLS_MD_C */

#if defined(MBEDTLS_PEM_PARSE_C) || defined(MBEDTLS_PEM_WRITE_C)
        case -(MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT):
            return( "PEM - No PEM header or footer found" );
        case -(MBEDTLS_ERR_PEM_INVALID_DATA):
            return( "PEM - PEM string is not as expected" );
        case -(MBEDTLS_ERR_PEM_ALLOC_FAILED):
            return( "PEM - Failed to allocate memory" );
        case -(MBEDTLS_ERR_PEM_INVALID_ENC_IV):
            return( "PEM - RSA IV is not in hex-format" );
        case -(MBEDTLS_ERR_PEM_UNKNOWN_ENC_ALG):
            return( "PEM - Unsupported key encryption algorithm" );
        case -(MBEDTLS_ERR_PEM_PASSWORD_REQUIRED):
            return( "PEM - Private key password can't be empty" );
        case -(MBEDTLS_ERR_PEM_PASSWORD_MISMATCH):
            return( "PEM - Given private key password does not allow for correct decryption" );
        case -(MBEDTLS_ERR_PEM_FEATURE_UNAVAILABLE):
            return( "PEM - Unavailable feature, e.g. hashing/encryption combination" );
        case -(MBEDTLS_ERR_PEM_BAD_INPUT_DATA):
            return( "PEM - Bad input parameters to function" );
#endif /* MBEDTLS_PEM_PARSE_C || MBEDTLS_PEM_WRITE_C */

#if defined(MBEDTLS_PK_C)
        case -(MBEDTLS_ERR_PK_ALLOC_FAILED):
            return( "PK - Memory allocation failed" );
        case -(MBEDTLS_ERR_PK_TYPE_MISMATCH):
            return( "PK - Type mismatch, eg attempt to encrypt with an ECDSA key" );
        case -(MBEDTLS_ERR_PK_BAD_INPUT_DATA):
            return( "PK - Bad input parameters to function" );
        case -(MBEDTLS_ERR_PK_FILE_IO_ERROR):
            return( "PK - Read/write of file failed" );
        case -(MBEDTLS_ERR_PK_KEY_INVALID_VERSION):
            return( "PK - Unsupported key version" );
        case -(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT):
            return( "PK - Invalid key tag or value" );
        case -(MBEDTLS_ERR_PK_UNKNOWN_PK_ALG):
            return( "PK - Key algorithm is unsupported (only RSA and EC are supported)" );
        case -(MBEDTLS_ERR_PK_PASSWORD_REQUIRED):
            return( "PK - Private key password can't be empty" );
        case -(MBEDTLS_ERR_PK_PASSWORD_MISMATCH):
            return( "PK - Given private key password does not allow for correct decryption" );
        case -(MBEDTLS_ERR_PK_INVALID_PUBKEY):
            return( "PK - The pubkey tag or value is invalid (only RSA and EC are supported)" );
        case -(MBEDTLS_ERR_PK_INVALID_ALG):
            return( "PK - The algorithm tag or value is invalid" );
        case -(MBEDTLS_ERR_PK_UNKNOWN_NAMED_CURVE):
            return( "PK - Elliptic curve is unsupported (only NIST curves are supported)" );
        case -(MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE):
            return( "PK - Unavailable feature, e.g. RSA disabled for RSA key" );
        case -(MBEDTLS_ERR_PK_SIG_LEN_MISMATCH):
            return( "PK - The buffer contains a valid signature followed by more data" );
        case -(MBEDTLS_ERR_PK_BUFFER_TOO_SMALL):
            return( "PK - The output buffer is too small" );
#endif /* MBEDTLS_PK_C */

#if defined(MBEDTLS_PKCS12_C)
        case -(MBEDTLS_ERR_PKCS12_BAD_INPUT_DATA):
            return( "PKCS12 - Bad input parameters to function" );
        case -(MBEDTLS_ERR_PKCS12_FEATURE_UNAVAILABLE):
            return( "PKCS12 - Feature not available, e.g. unsupported encryption scheme" );
        case -(MBEDTLS_ERR_PKCS12_PBE_INVALID_FORMAT):
            return( "PKCS12 - PBE ASN.1 data not as expected" );
        case -(MBEDTLS_ERR_PKCS12_PASSWORD_MISMATCH):
            return( "PKCS12 - Given private key password does not allow for correct decryption" );
#endif /* MBEDTLS_PKCS12_C */

#if defined(MBEDTLS_PKCS5_C)
        case -(MBEDTLS_ERR_PKCS5_BAD_INPUT_DATA):
            return( "PKCS5 - Bad input parameters to function" );
        case -(MBEDTLS_ERR_PKCS5_INVALID_FORMAT):
            return( "PKCS5 - Unexpected ASN.1 data" );
        case -(MBEDTLS_ERR_PKCS5_FEATURE_UNAVAILABLE):
            return( "PKCS5 - Requested encryption or digest alg not available" );
        case -(MBEDTLS_ERR_PKCS5_PASSWORD_MISMATCH):
            return( "PKCS5 - Given private key password does not allow for correct decryption" );
#endif /* MBEDTLS_PKCS5_C */

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
        case -(MBEDTLS_ERR_PKCS7_BAD_INPUT_DATA):
            return( "PKCS7 - Input invalid" );
        case -(MBEDTLS_ERR_PKCS7_ALLOC_FAILED):
            return( "PKCS7 - Allocation of memory failed" );
        case -(MBEDTLS_ERR_PKCS7_VERIFY_FAIL):
            return( "PKCS7 - Verification Failed" );
        case -(MBEDTLS_ERR_PKCS7_CERT_DATE_INVALID):
            return( "PKCS7 - The PKCS #7 date issued/expired dates are invalid" );
#endif /* MBEDTLS_PKCS7_C */

#if defined(MBEDTLS_RSA_C)
        case -(MBEDTLS_ERR_RSA_BAD_INPUT_DATA):
            return( "RSA - Bad input parameters to function" );
        case -(MBEDTLS_ERR_RSA_INVALID_PADDING):
            return( "RSA - Input data contains invalid padding and is rejected" );
        case -(MBEDTLS_ERR_RSA_KEY_GEN_FAILED):
            return( "RSA - Something failed during generation of a key" );
        case -(MBEDTLS_ERR_RSA_KEY_CHECK_FAILED):
            return( "RSA - Key failed to pass the validity check of the library" );
        case -(MBEDTLS_ERR_RSA_PUBLIC_FAILED):
            return( "RSA - The public key operation failed" );
        case -(MBEDTLS_ERR_RSA_PRIVATE_FAILED):
            return( "RSA - The private key operation failed" );
        case -(MBEDTLS_ERR_RSA_VERIFY_FAILED):
            return( "RSA - The PKCS#1 verification failed" );
        case -(MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE):
            return( "RSA - The output buffer for decryption is not large enough" );
        case -(MBEDTLS_ERR_RSA_RNG_FAILED):
            return( "RSA - The random generator failed to generate non-zeros" );
#endif /* MBEDTLS_RSA_C */

#if defined(MBEDTLS_SSL_TLS_C)
        case -(MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS):
            return( "SSL - A cryptographic operation is in progress. Try again later" );
        case -(MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE):
            return( "SSL - The requested feature is not available" );
        case -(MBEDTLS_ERR_SSL_BAD_INPUT_DATA):
            return( "SSL - Bad input parameters to function" );
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
            return( "SSL - A TLS 1.3 NewSessionTicket message has been received" );
        case -(MBEDTLS_ERR_SSL_CANNOT_READ_EARLY_DATA):
            return( "SSL - Not possible to read early data" );
        case -(MBEDTLS_ERR_SSL_RECEIVED_EARLY_DATA):
            return( "SSL - * Early data has been received as part of an on-going handshake. This error code can be returned only on server side if and only if early data has been enabled by means of the mbedtls_ssl_conf_early_data() API. This error code can then be returned by mbedtls_ssl_handshake(), mbedtls_ssl_handshake_step(), mbedtls_ssl_read() or mbedtls_ssl_write() if early data has been received as part of the handshake sequence they triggered. To read the early data, call mbedtls_ssl_read_early_data()" );
        case -(MBEDTLS_ERR_SSL_CANNOT_WRITE_EARLY_DATA):
            return( "SSL - Not possible to write early data" );
        case -(MBEDTLS_ERR_SSL_CACHE_ENTRY_NOT_FOUND):
            return( "SSL - Cache entry not found" );
        case -(MBEDTLS_ERR_SSL_ALLOC_FAILED):
            return( "SSL - Memory allocation failed" );
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
        case -(MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL):
            return( "SSL - A buffer is too small to receive or write a message" );
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
            return( "SSL - Attempt to verify a certificate without an expected hostname. This is usually insecure.  In TLS clients, when a client authenticates a server through its certificate, the client normally checks three things: - the certificate chain must be valid; - the chain must start from a trusted CA; - the certificate must cover the server name that is expected by the client.  Omitting any of these checks is generally insecure, and can allow a malicious server to impersonate a legitimate server.  The third check may be safely skipped in some unusual scenarios, such as networks where eavesdropping is a risk but not active attacks, or a private PKI where the client equally trusts all servers that are accredited by the root CA.  You should call mbedtls_ssl_set_hostname() with the expected server name before starting a TLS handshake on a client (unless the client is set up to only use PSK-based authentication, which does not rely on the host name). If you have determined that server name verification is not required for security in your scenario, call mbedtls_ssl_set_hostname() with \\p NULL as the server name.  This error is raised if all of the following conditions are met:  - A TLS client is configured with the authentication mode #MBEDTLS_SSL_VERIFY_REQUIRED (default). - Certificate authentication is enabled. - The client does not call mbedtls_ssl_set_hostname(). - The configuration option #MBEDTLS_SSL_CLI_ALLOW_WEAK_CERTIFICATE_VERIFICATION_WITHOUT_HOSTNAME is not enabled" );
#endif /* MBEDTLS_SSL_TLS_C */

#if defined(MBEDTLS_X509_USE_C) || defined(MBEDTLS_X509_CREATE_C)
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
        case -(MBEDTLS_ERR_X509_ALLOC_FAILED):
            return( "X509 - Allocation of memory failed" );
        case -(MBEDTLS_ERR_X509_FILE_IO_ERROR):
            return( "X509 - Read/write of file failed" );
        case -(MBEDTLS_ERR_X509_BUFFER_TOO_SMALL):
            return( "X509 - Destination buffer is too small" );
        case -(MBEDTLS_ERR_X509_FATAL_ERROR):
            return( "X509 - A fatal error occurred, eg the chain is too long or the vrfy callback failed" );
#endif /* MBEDTLS_X509_USE_C || MBEDTLS_X509_CREATE_C */
        /* End Auto-Generated Code. */

        default:
            break;
    }

    return NULL;
}

const char *mbedtls_low_level_strerr(int error_code)
{
    int low_level_error_code;

    if (error_code < 0) {
        error_code = -error_code;
    }

    /* Extract the low-level part from the error code. */
    low_level_error_code = error_code & ~0xFF80;

    switch (low_level_error_code) {
    /* Begin Auto-Generated Code. */
    #if defined(MBEDTLS_AES_C)
        case -(MBEDTLS_ERR_AES_INVALID_KEY_LENGTH):
            return( "AES - Invalid key length" );
        case -(MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH):
            return( "AES - Invalid data input length" );
        case -(MBEDTLS_ERR_AES_BAD_INPUT_DATA):
            return( "AES - Invalid input data" );
#endif /* MBEDTLS_AES_C */

#if defined(MBEDTLS_ARIA_C)
        case -(MBEDTLS_ERR_ARIA_BAD_INPUT_DATA):
            return( "ARIA - Bad input data" );
        case -(MBEDTLS_ERR_ARIA_INVALID_INPUT_LENGTH):
            return( "ARIA - Invalid data input length" );
#endif /* MBEDTLS_ARIA_C */

#if defined(MBEDTLS_ASN1_PARSE_C)
        case -(MBEDTLS_ERR_ASN1_OUT_OF_DATA):
            return( "ASN1 - Out of data when parsing an ASN1 data structure" );
        case -(MBEDTLS_ERR_ASN1_UNEXPECTED_TAG):
            return( "ASN1 - ASN1 tag was of an unexpected value" );
        case -(MBEDTLS_ERR_ASN1_INVALID_LENGTH):
            return( "ASN1 - Error when trying to determine the length or invalid length" );
        case -(MBEDTLS_ERR_ASN1_LENGTH_MISMATCH):
            return( "ASN1 - Actual length differs from expected length" );
        case -(MBEDTLS_ERR_ASN1_INVALID_DATA):
            return( "ASN1 - Data is invalid" );
        case -(MBEDTLS_ERR_ASN1_ALLOC_FAILED):
            return( "ASN1 - Memory allocation failed" );
        case -(MBEDTLS_ERR_ASN1_BUF_TOO_SMALL):
            return( "ASN1 - Buffer too small when writing ASN.1 data structure" );
#endif /* MBEDTLS_ASN1_PARSE_C */

#if defined(MBEDTLS_BASE64_C)
        case -(MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL):
            return( "BASE64 - Output buffer too small" );
        case -(MBEDTLS_ERR_BASE64_INVALID_CHARACTER):
            return( "BASE64 - Invalid character in input" );
#endif /* MBEDTLS_BASE64_C */

#if defined(MBEDTLS_BIGNUM_C)
        case -(MBEDTLS_ERR_MPI_FILE_IO_ERROR):
            return( "BIGNUM - An error occurred while reading from or writing to a file" );
        case -(MBEDTLS_ERR_MPI_BAD_INPUT_DATA):
            return( "BIGNUM - Bad input parameters to function" );
        case -(MBEDTLS_ERR_MPI_INVALID_CHARACTER):
            return( "BIGNUM - There is an invalid character in the digit string" );
        case -(MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL):
            return( "BIGNUM - The buffer is too small to write to" );
        case -(MBEDTLS_ERR_MPI_NEGATIVE_VALUE):
            return( "BIGNUM - The input arguments are negative or result in illegal output" );
        case -(MBEDTLS_ERR_MPI_DIVISION_BY_ZERO):
            return( "BIGNUM - The input argument for division is zero, which is not allowed" );
        case -(MBEDTLS_ERR_MPI_NOT_ACCEPTABLE):
            return( "BIGNUM - The input arguments are not acceptable" );
        case -(MBEDTLS_ERR_MPI_ALLOC_FAILED):
            return( "BIGNUM - Memory allocation failed" );
#endif /* MBEDTLS_BIGNUM_C */

#if defined(MBEDTLS_CAMELLIA_C)
        case -(MBEDTLS_ERR_CAMELLIA_BAD_INPUT_DATA):
            return( "CAMELLIA - Bad input data" );
        case -(MBEDTLS_ERR_CAMELLIA_INVALID_INPUT_LENGTH):
            return( "CAMELLIA - Invalid data input length" );
#endif /* MBEDTLS_CAMELLIA_C */

#if defined(MBEDTLS_CCM_C)
        case -(MBEDTLS_ERR_CCM_BAD_INPUT):
            return( "CCM - Bad input parameters to the function" );
        case -(MBEDTLS_ERR_CCM_AUTH_FAILED):
            return( "CCM - Authenticated decryption failed" );
#endif /* MBEDTLS_CCM_C */

#if defined(MBEDTLS_CHACHA20_C)
        case -(MBEDTLS_ERR_CHACHA20_BAD_INPUT_DATA):
            return( "CHACHA20 - Invalid input parameter(s)" );
#endif /* MBEDTLS_CHACHA20_C */

#if defined(MBEDTLS_CHACHAPOLY_C)
        case -(MBEDTLS_ERR_CHACHAPOLY_BAD_STATE):
            return( "CHACHAPOLY - The requested operation is not permitted in the current state" );
        case -(MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED):
            return( "CHACHAPOLY - Authenticated decryption failed: data was not authentic" );
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

#if defined(MBEDTLS_DES_C)
        case -(MBEDTLS_ERR_DES_INVALID_INPUT_LENGTH):
            return( "DES - The data input has an invalid length" );
#endif /* MBEDTLS_DES_C */

#if defined(MBEDTLS_ENTROPY_C)
        case -(MBEDTLS_ERR_ENTROPY_SOURCE_FAILED):
            return( "ENTROPY - Critical entropy source failure" );
        case -(MBEDTLS_ERR_ENTROPY_MAX_SOURCES):
            return( "ENTROPY - No more sources can be added" );
        case -(MBEDTLS_ERR_ENTROPY_NO_SOURCES_DEFINED):
            return( "ENTROPY - No sources have been added to poll" );
        case -(MBEDTLS_ERR_ENTROPY_NO_STRONG_SOURCE):
            return( "ENTROPY - No strong sources have been added to poll" );
        case -(MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR):
            return( "ENTROPY - Read/write error in file" );
#endif /* MBEDTLS_ENTROPY_C */

#if defined(MBEDTLS_ERROR_C)
        case -(MBEDTLS_ERR_ERROR_GENERIC_ERROR):
            return( "ERROR - Generic error" );
        case -(MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED):
            return( "ERROR - This is a bug in the library" );
#endif /* MBEDTLS_ERROR_C */

#if defined(MBEDTLS_PLATFORM_C)
        case -(MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED):
            return( "PLATFORM - Hardware accelerator failed" );
        case -(MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED):
            return( "PLATFORM - The requested feature is not supported by the platform" );
#endif /* MBEDTLS_PLATFORM_C */

#if defined(MBEDTLS_GCM_C)
        case -(MBEDTLS_ERR_GCM_AUTH_FAILED):
            return( "GCM - Authenticated decryption failed" );
        case -(MBEDTLS_ERR_GCM_BAD_INPUT):
            return( "GCM - Bad input parameters to function" );
        case -(MBEDTLS_ERR_GCM_BUFFER_TOO_SMALL):
            return( "GCM - An output buffer is too small" );
#endif /* MBEDTLS_GCM_C */

#if defined(MBEDTLS_HKDF_C)
        case -(MBEDTLS_ERR_HKDF_BAD_INPUT_DATA):
            return( "HKDF - Bad input parameters to function" );
#endif /* MBEDTLS_HKDF_C */

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

#if defined(MBEDTLS_LMS_C)
        case -(MBEDTLS_ERR_LMS_BAD_INPUT_DATA):
            return( "LMS - Bad data has been input to an LMS function" );
        case -(MBEDTLS_ERR_LMS_OUT_OF_PRIVATE_KEYS):
            return( "LMS - Specified LMS key has utilised all of its private keys" );
        case -(MBEDTLS_ERR_LMS_VERIFY_FAILED):
            return( "LMS - LMS signature verification failed" );
        case -(MBEDTLS_ERR_LMS_ALLOC_FAILED):
            return( "LMS - LMS failed to allocate space for a private key" );
        case -(MBEDTLS_ERR_LMS_BUFFER_TOO_SMALL):
            return( "LMS - Input/output buffer is too small to contain requited data" );
#endif /* MBEDTLS_LMS_C */

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
        case -(MBEDTLS_ERR_NET_BUFFER_TOO_SMALL):
            return( "NET - Buffer is too small to hold the data" );
        case -(MBEDTLS_ERR_NET_INVALID_CONTEXT):
            return( "NET - The context is invalid, eg because it was free()ed" );
        case -(MBEDTLS_ERR_NET_POLL_FAILED):
            return( "NET - Polling the net context failed" );
        case -(MBEDTLS_ERR_NET_BAD_INPUT_DATA):
            return( "NET - Input invalid" );
#endif /* MBEDTLS_NET_C */

#if defined(MBEDTLS_OID_C)
        case -(MBEDTLS_ERR_OID_NOT_FOUND):
            return( "OID - OID is not found" );
        case -(MBEDTLS_ERR_OID_BUF_TOO_SMALL):
            return( "OID - output buffer is too small" );
#endif /* MBEDTLS_OID_C */

#if defined(MBEDTLS_POLY1305_C)
        case -(MBEDTLS_ERR_POLY1305_BAD_INPUT_DATA):
            return( "POLY1305 - Invalid input parameter(s)" );
#endif /* MBEDTLS_POLY1305_C */

#if defined(MBEDTLS_SHA1_C)
        case -(MBEDTLS_ERR_SHA1_BAD_INPUT_DATA):
            return( "SHA1 - SHA-1 input data was malformed" );
#endif /* MBEDTLS_SHA1_C */

#if defined(MBEDTLS_SHA256_C)
        case -(MBEDTLS_ERR_SHA256_BAD_INPUT_DATA):
            return( "SHA256 - SHA-256 input data was malformed" );
#endif /* MBEDTLS_SHA256_C */

#if defined(MBEDTLS_SHA3_C)
        case -(MBEDTLS_ERR_SHA3_BAD_INPUT_DATA):
            return( "SHA3 - SHA-3 input data was malformed" );
#endif /* MBEDTLS_SHA3_C */

#if defined(MBEDTLS_SHA512_C)
        case -(MBEDTLS_ERR_SHA512_BAD_INPUT_DATA):
            return( "SHA512 - SHA-512 input data was malformed" );
#endif /* MBEDTLS_SHA512_C */

#if defined(MBEDTLS_THREADING_C)
        case -(MBEDTLS_ERR_THREADING_BAD_INPUT_DATA):
            return( "THREADING - Bad input parameters to function" );
        case -(MBEDTLS_ERR_THREADING_MUTEX_ERROR):
            return( "THREADING - Locking / unlocking / free failed with error code" );
#endif /* MBEDTLS_THREADING_C */
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
