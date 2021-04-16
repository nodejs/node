/*
 * keyraw.h -- raw key and signature access and conversion
 *
 * Copyright (c) 2005-2008, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

/**
 * \file
 *
 * raw key and signature access and conversion
 *
 * Since those functions heavily rely op cryptographic operations,
 * this module is dependent on openssl.
 * 
 */
 
#ifndef GLDNS_KEYRAW_H
#define GLDNS_KEYRAW_H

#include "keyraw-internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * get the length of the keydata in bits
 * \param[in] keydata the raw key data
 * \param[in] len the length of the keydata
 * \param[in] alg the cryptographic algorithm this is a key for
 * \return the keysize in bits, or 0 on error
 */
size_t gldns_rr_dnskey_key_size_raw(const unsigned char *keydata,
	const size_t len, int alg);

/**
 * Calculates keytag of DNSSEC key, operates on wireformat rdata.
 * \param[in] key the key as uncompressed wireformat rdata.
 * \param[in] keysize length of key data.
 * \return the keytag
 */
uint16_t gldns_calc_keytag_raw(const uint8_t* key, size_t keysize);

#ifdef __cplusplus
}
#endif

#endif /* GLDNS_KEYRAW_H */
