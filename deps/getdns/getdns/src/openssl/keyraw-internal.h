/*
 * keyraw.h -- raw key and signature access and conversion - OpenSSL
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
 
#ifndef GLDNS_KEYRAW_INTERNAL_H
#define GLDNS_KEYRAW_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif
#if GLDNS_BUILD_CONFIG_HAVE_SSL
#  include <openssl/ssl.h>
#  include <openssl/evp.h>

/** 
 * Get the PKEY id for GOST, loads GOST into openssl as a side effect.
 * Only available if GOST is compiled into the library and openssl.
 * \return the gost id for EVP_CTX creation.
 */
int gldns_key_EVP_load_gost_id(void);

/** Release the engine reference held for the GOST engine. */
void gldns_key_EVP_unload_gost(void);

/**
 * Like gldns_key_buf2dsa, but uses raw buffer.
 * \param[in] key the uncompressed wireformat of the key.
 * \param[in] len length of key data
 * \return a DSA * structure with the key material
 */
DSA *gldns_key_buf2dsa_raw(unsigned char* key, size_t len);

/**
 * Converts a holding buffer with key material to EVP PKEY in openssl.
 * Only available if ldns was compiled with GOST.
 * \param[in] key data to convert
 * \param[in] keylen length of the key data
 * \return the key or NULL on error.
 */
EVP_PKEY* gldns_gost2pkey_raw(unsigned char* key, size_t keylen);

/**
 * Converts a holding buffer with key material to EVP PKEY in openssl.
 * Only available if ldns was compiled with ECDSA.
 * \param[in] key data to convert
 * \param[in] keylen length of the key data
 * \param[in] algo precise algorithm to initialize ECC group values.
 * \return the key or NULL on error.
 */
EVP_PKEY* gldns_ecdsa2pkey_raw(unsigned char* key, size_t keylen, uint8_t algo);

/**
 * Like gldns_key_buf2rsa, but uses raw buffer.
 * \param[in] key the uncompressed wireformat of the key.
 * \param[in] len length of key data
 * \return a RSA * structure with the key material
 */
RSA *gldns_key_buf2rsa_raw(unsigned char* key, size_t len);

/**
 * Converts a holding buffer with key material to EVP PKEY in openssl.
 * Only available if ldns was compiled with ED25519.
 * \param[in] key the uncompressed wireformat of the key.
 * \param[in] len length of key data
 * \return the key or NULL on error.
 */
EVP_PKEY* gldns_ed255192pkey_raw(const unsigned char* key, size_t len);

/**
 * Converts a holding buffer with key material to EVP PKEY in openssl.
 * Only available if ldns was compiled with ED448.
 * \param[in] key the uncompressed wireformat of the key.
 * \param[in] len length of key data
 * \return the key or NULL on error.
 */
EVP_PKEY* gldns_ed4482pkey_raw(const unsigned char* key, size_t len);

/**
 * Utility function to calculate hash using generic EVP_MD pointer.
 * \param[in] data the data to hash.
 * \param[in] len  length of data.
 * \param[out] dest the destination of the hash, must be large enough.
 * \param[in] md the message digest to use.
 * \return true if worked, false on failure.
 */
int gldns_digest_evp(unsigned char* data, unsigned int len, 
	unsigned char* dest, const EVP_MD* md);

#endif /* GLDNS_BUILD_CONFIG_HAVE_SSL */

#ifdef __cplusplus
}
#endif

#endif /* GLDNS_KEYRAW_INTERNAL_H */
