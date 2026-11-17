/*
 * Copyright 2005-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OPENSSL_DTLS1_H
#define OPENSSL_DTLS1_H
#pragma once

#include <openssl/macros.h>
#ifndef OPENSSL_NO_DEPRECATED_3_0
#define HEADER_DTLS1_H
#endif

#include <openssl/prov_ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/opensslconf.h>

/* DTLS*_VERSION constants are defined in prov_ssl.h */
#ifndef OPENSSL_NO_DEPRECATED_3_0
#define DTLS_MIN_VERSION DTLS1_VERSION
#define DTLS_MAX_VERSION DTLS1_3_VERSION
#endif
#define DTLS1_VERSION_MAJOR 0xFE

/* Special value for method supporting multiple versions */
#define DTLS_ANY_VERSION 0x1FFFF

/* DTLS records and messages lengths and offsets */

#define DTLS1_COOKIE_LENGTH 255

#define DTLS1_RT_HEADER_SEQ_OFFS 5
#define DTLS1_RT_HEADER_SEQ_LEN 6
#define DTLS1_RT_HEADER_LENGTH 13

#define DTLS1_HM_HEADER_LENGTH 12

#define DTLS1_HM_BAD_FRAGMENT -2
#define DTLS1_HM_FRAGMENT_RETRY -3

#define DTLS1_CCS_HEADER_LENGTH 1

#define DTLS1_AL_HEADER_LENGTH 2

#define DTLS1_TMO_ALERT_COUNT 12

/* DTLS 1.3 Unified header */
#define DTLS13_UNI_HDR_FIXED_LENGTH 5
#define DTLS13_UNI_HDR_FIX_BITS 0x20
#define DTLS13_UNI_HDR_CID_BIT 0x10
#define DTLS13_UNI_HDR_SEQ_BIT 0x08
#define DTLS13_UNI_HDR_SEQ_OFF 1
#define DTLS13_UNI_HDR_LEN_BIT 0x04
#define DTLS13_UNI_HDR_FIX_BITS_MASK 0xe0
#define DTLS13_UNI_HDR_EPOCH_BITS_MASK 0x03

#define DTLS13_CIPHERTEXT_MINSIZE 16

#ifdef __cplusplus
}
#endif
#endif
