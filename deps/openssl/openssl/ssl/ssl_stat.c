/*
 * Copyright 1995-2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

#include <stdio.h>
#include "ssl_locl.h"

const char *SSL_state_string_long(const SSL *s)
{
    if (ossl_statem_in_error(s))
        return "error";

    switch (SSL_get_state(s)) {
    case TLS_ST_CR_CERT_STATUS:
        return "SSLv3/TLS read certificate status";
    case TLS_ST_CW_NEXT_PROTO:
        return "SSLv3/TLS write next proto";
    case TLS_ST_SR_NEXT_PROTO:
        return "SSLv3/TLS read next proto";
    case TLS_ST_SW_CERT_STATUS:
        return "SSLv3/TLS write certificate status";
    case TLS_ST_BEFORE:
        return "before SSL initialization";
    case TLS_ST_OK:
        return "SSL negotiation finished successfully";
    case TLS_ST_CW_CLNT_HELLO:
        return "SSLv3/TLS write client hello";
    case TLS_ST_CR_SRVR_HELLO:
        return "SSLv3/TLS read server hello";
    case TLS_ST_CR_CERT:
        return "SSLv3/TLS read server certificate";
    case TLS_ST_CR_KEY_EXCH:
        return "SSLv3/TLS read server key exchange";
    case TLS_ST_CR_CERT_REQ:
        return "SSLv3/TLS read server certificate request";
    case TLS_ST_CR_SESSION_TICKET:
        return "SSLv3/TLS read server session ticket";
    case TLS_ST_CR_SRVR_DONE:
        return "SSLv3/TLS read server done";
    case TLS_ST_CW_CERT:
        return "SSLv3/TLS write client certificate";
    case TLS_ST_CW_KEY_EXCH:
        return "SSLv3/TLS write client key exchange";
    case TLS_ST_CW_CERT_VRFY:
        return "SSLv3/TLS write certificate verify";
    case TLS_ST_CW_CHANGE:
    case TLS_ST_SW_CHANGE:
        return "SSLv3/TLS write change cipher spec";
    case TLS_ST_CW_FINISHED:
    case TLS_ST_SW_FINISHED:
        return "SSLv3/TLS write finished";
    case TLS_ST_CR_CHANGE:
    case TLS_ST_SR_CHANGE:
        return "SSLv3/TLS read change cipher spec";
    case TLS_ST_CR_FINISHED:
    case TLS_ST_SR_FINISHED:
        return "SSLv3/TLS read finished";
    case TLS_ST_SR_CLNT_HELLO:
        return "SSLv3/TLS read client hello";
    case TLS_ST_SW_HELLO_REQ:
        return "SSLv3/TLS write hello request";
    case TLS_ST_SW_SRVR_HELLO:
        return "SSLv3/TLS write server hello";
    case TLS_ST_SW_CERT:
        return "SSLv3/TLS write certificate";
    case TLS_ST_SW_KEY_EXCH:
        return "SSLv3/TLS write key exchange";
    case TLS_ST_SW_CERT_REQ:
        return "SSLv3/TLS write certificate request";
    case TLS_ST_SW_SESSION_TICKET:
        return "SSLv3/TLS write session ticket";
    case TLS_ST_SW_SRVR_DONE:
        return "SSLv3/TLS write server done";
    case TLS_ST_SR_CERT:
        return "SSLv3/TLS read client certificate";
    case TLS_ST_SR_KEY_EXCH:
        return "SSLv3/TLS read client key exchange";
    case TLS_ST_SR_CERT_VRFY:
        return "SSLv3/TLS read certificate verify";
    case DTLS_ST_CR_HELLO_VERIFY_REQUEST:
        return "DTLS1 read hello verify request";
    case DTLS_ST_SW_HELLO_VERIFY_REQUEST:
        return "DTLS1 write hello verify request";
    default:
        return "unknown state";
    }
}

const char *SSL_state_string(const SSL *s)
{
    if (ossl_statem_in_error(s))
        return "SSLERR";

    switch (SSL_get_state(s)) {
    case TLS_ST_SR_NEXT_PROTO:
        return "TRNP";
    case TLS_ST_SW_SESSION_TICKET:
        return "TWST";
    case TLS_ST_SW_CERT_STATUS:
        return "TWCS";
    case TLS_ST_CR_CERT_STATUS:
        return "TRCS";
    case TLS_ST_CR_SESSION_TICKET:
        return "TRST";
    case TLS_ST_CW_NEXT_PROTO:
        return "TWNP";
    case TLS_ST_BEFORE:
        return "PINIT ";
    case TLS_ST_OK:
        return "SSLOK ";
    case TLS_ST_CW_CLNT_HELLO:
        return "TWCH";
    case TLS_ST_CR_SRVR_HELLO:
        return "TRSH";
    case TLS_ST_CR_CERT:
        return "TRSC";
    case TLS_ST_CR_KEY_EXCH:
        return "TRSKE";
    case TLS_ST_CR_CERT_REQ:
        return "TRCR";
    case TLS_ST_CR_SRVR_DONE:
        return "TRSD";
    case TLS_ST_CW_CERT:
        return "TWCC";
    case TLS_ST_CW_KEY_EXCH:
        return "TWCKE";
    case TLS_ST_CW_CERT_VRFY:
        return "TWCV";
    case TLS_ST_SW_CHANGE:
    case TLS_ST_CW_CHANGE:
        return "TWCCS";
    case TLS_ST_SW_FINISHED:
    case TLS_ST_CW_FINISHED:
        return "TWFIN";
    case TLS_ST_SR_CHANGE:
    case TLS_ST_CR_CHANGE:
        return "TRCCS";
    case TLS_ST_SR_FINISHED:
    case TLS_ST_CR_FINISHED:
        return "TRFIN";
    case TLS_ST_SW_HELLO_REQ:
        return "TWHR";
    case TLS_ST_SR_CLNT_HELLO:
        return "TRCH";
    case TLS_ST_SW_SRVR_HELLO:
        return "TWSH";
    case TLS_ST_SW_CERT:
        return "TWSC";
    case TLS_ST_SW_KEY_EXCH:
        return "TWSKE";
    case TLS_ST_SW_CERT_REQ:
        return "TWCR";
    case TLS_ST_SW_SRVR_DONE:
        return "TWSD";
    case TLS_ST_SR_CERT:
        return "TRCC";
    case TLS_ST_SR_KEY_EXCH:
        return "TRCKE";
    case TLS_ST_SR_CERT_VRFY:
        return "TRCV";
    case DTLS_ST_CR_HELLO_VERIFY_REQUEST:
        return "DRCHV";
    case DTLS_ST_SW_HELLO_VERIFY_REQUEST:
        return "DWCHV";
    default:
        return "UNKWN ";
    }
}

const char *SSL_alert_type_string_long(int value)
{
    switch (value >> 8) {
    case SSL3_AL_WARNING:
        return "warning";
    case SSL3_AL_FATAL:
        return "fatal";
    default:
        return "unknown";
    }
}

const char *SSL_alert_type_string(int value)
{
    switch (value >> 8) {
    case SSL3_AL_WARNING:
        return "W";
    case SSL3_AL_FATAL:
        return "F";
    default:
        return "U";
    }
}

const char *SSL_alert_desc_string(int value)
{
    switch (value & 0xff) {
    case SSL3_AD_CLOSE_NOTIFY:
        return "CN";
    case SSL3_AD_UNEXPECTED_MESSAGE:
        return "UM";
    case SSL3_AD_BAD_RECORD_MAC:
        return "BM";
    case SSL3_AD_DECOMPRESSION_FAILURE:
        return "DF";
    case SSL3_AD_HANDSHAKE_FAILURE:
        return "HF";
    case SSL3_AD_NO_CERTIFICATE:
        return "NC";
    case SSL3_AD_BAD_CERTIFICATE:
        return "BC";
    case SSL3_AD_UNSUPPORTED_CERTIFICATE:
        return "UC";
    case SSL3_AD_CERTIFICATE_REVOKED:
        return "CR";
    case SSL3_AD_CERTIFICATE_EXPIRED:
        return "CE";
    case SSL3_AD_CERTIFICATE_UNKNOWN:
        return "CU";
    case SSL3_AD_ILLEGAL_PARAMETER:
        return "IP";
    case TLS1_AD_DECRYPTION_FAILED:
        return "DC";
    case TLS1_AD_RECORD_OVERFLOW:
        return "RO";
    case TLS1_AD_UNKNOWN_CA:
        return "CA";
    case TLS1_AD_ACCESS_DENIED:
        return "AD";
    case TLS1_AD_DECODE_ERROR:
        return "DE";
    case TLS1_AD_DECRYPT_ERROR:
        return "CY";
    case TLS1_AD_EXPORT_RESTRICTION:
        return "ER";
    case TLS1_AD_PROTOCOL_VERSION:
        return "PV";
    case TLS1_AD_INSUFFICIENT_SECURITY:
        return "IS";
    case TLS1_AD_INTERNAL_ERROR:
        return "IE";
    case TLS1_AD_USER_CANCELLED:
        return "US";
    case TLS1_AD_NO_RENEGOTIATION:
        return "NR";
    case TLS1_AD_UNSUPPORTED_EXTENSION:
        return "UE";
    case TLS1_AD_CERTIFICATE_UNOBTAINABLE:
        return "CO";
    case TLS1_AD_UNRECOGNIZED_NAME:
        return "UN";
    case TLS1_AD_BAD_CERTIFICATE_STATUS_RESPONSE:
        return "BR";
    case TLS1_AD_BAD_CERTIFICATE_HASH_VALUE:
        return "BH";
    case TLS1_AD_UNKNOWN_PSK_IDENTITY:
        return "UP";
    default:
        return "UK";
    }
}

const char *SSL_alert_desc_string_long(int value)
{
    switch (value & 0xff) {
    case SSL3_AD_CLOSE_NOTIFY:
        return "close notify";
    case SSL3_AD_UNEXPECTED_MESSAGE:
        return "unexpected_message";
    case SSL3_AD_BAD_RECORD_MAC:
        return "bad record mac";
    case SSL3_AD_DECOMPRESSION_FAILURE:
        return "decompression failure";
    case SSL3_AD_HANDSHAKE_FAILURE:
        return "handshake failure";
    case SSL3_AD_NO_CERTIFICATE:
        return "no certificate";
    case SSL3_AD_BAD_CERTIFICATE:
        return "bad certificate";
    case SSL3_AD_UNSUPPORTED_CERTIFICATE:
        return "unsupported certificate";
    case SSL3_AD_CERTIFICATE_REVOKED:
        return "certificate revoked";
    case SSL3_AD_CERTIFICATE_EXPIRED:
        return "certificate expired";
    case SSL3_AD_CERTIFICATE_UNKNOWN:
        return "certificate unknown";
    case SSL3_AD_ILLEGAL_PARAMETER:
        return "illegal parameter";
    case TLS1_AD_DECRYPTION_FAILED:
        return "decryption failed";
    case TLS1_AD_RECORD_OVERFLOW:
        return "record overflow";
    case TLS1_AD_UNKNOWN_CA:
        return "unknown CA";
    case TLS1_AD_ACCESS_DENIED:
        return "access denied";
    case TLS1_AD_DECODE_ERROR:
        return "decode error";
    case TLS1_AD_DECRYPT_ERROR:
        return "decrypt error";
    case TLS1_AD_EXPORT_RESTRICTION:
        return "export restriction";
    case TLS1_AD_PROTOCOL_VERSION:
        return "protocol version";
    case TLS1_AD_INSUFFICIENT_SECURITY:
        return "insufficient security";
    case TLS1_AD_INTERNAL_ERROR:
        return "internal error";
    case TLS1_AD_USER_CANCELLED:
        return "user canceled";
    case TLS1_AD_NO_RENEGOTIATION:
        return "no renegotiation";
    case TLS1_AD_UNSUPPORTED_EXTENSION:
        return "unsupported extension";
    case TLS1_AD_CERTIFICATE_UNOBTAINABLE:
        return "certificate unobtainable";
    case TLS1_AD_UNRECOGNIZED_NAME:
        return "unrecognized name";
    case TLS1_AD_BAD_CERTIFICATE_STATUS_RESPONSE:
        return "bad certificate status response";
    case TLS1_AD_BAD_CERTIFICATE_HASH_VALUE:
        return "bad certificate hash value";
    case TLS1_AD_UNKNOWN_PSK_IDENTITY:
        return "unknown PSK identity";
    case TLS1_AD_NO_APPLICATION_PROTOCOL:
        return "no application protocol";
    default:
        return "unknown";
    }
}
