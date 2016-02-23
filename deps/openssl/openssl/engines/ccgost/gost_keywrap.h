/**********************************************************************
 *                         gost_keywrap.h                             *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *       This file is distributed under the same license as OpenSSL   *
 *                                                                    *
 * Implementation of CryptoPro key wrap algorithm, as defined in      *
 * RFC 4357 p 6.3 and 6.4                                             *
 * Doesn't need OpenSSL                                               *
 **********************************************************************/
#ifndef GOST_KEYWRAP_H
# define GOST_KEYWRAP_H
# include <string.h>
# include "gost89.h"
/*-
 * Diversifies key using random UserKey Material
 * Implements RFC 4357 p 6.5 key diversification algorithm
 *
 * inputKey - 32byte key to be diversified
 * ukm - 8byte user key material
 * outputKey - 32byte buffer to store diversified key
 *
 */
void keyDiversifyCryptoPro(gost_ctx * ctx,
                           const unsigned char *inputKey,
                           const unsigned char *ukm,
                           unsigned char *outputKey);
/*-
 * Wraps key using RFC 4357 6.3
 * ctx - gost encryption context, initialized with some S-boxes
 * keyExchangeKey (KEK) 32-byte (256-bit) shared key
 * ukm - 8 byte (64 bit) user key material,
 * sessionKey - 32-byte (256-bit) key to be wrapped
 * wrappedKey - 44-byte buffer to store wrapped key
 */

int keyWrapCryptoPro(gost_ctx * ctx,
                     const unsigned char *keyExchangeKey,
                     const unsigned char *ukm,
                     const unsigned char *sessionKey,
                     unsigned char *wrappedKey);
/*-
 * Unwraps key using RFC 4357 6.4
 * ctx - gost encryption context, initialized with some S-boxes
 * keyExchangeKey 32-byte shared key
 * wrappedKey  44 byte key to be unwrapped (concatenation of 8-byte UKM,
 * 32 byte  encrypted key and 4 byte MAC
 *
 * sessionKEy - 32byte buffer to store sessionKey in
 * Returns 1 if key is decrypted successfully, and 0 if MAC doesn't match
 */

int keyUnwrapCryptoPro(gost_ctx * ctx,
                       const unsigned char *keyExchangeKey,
                       const unsigned char *wrappedKey,
                       unsigned char *sessionKey);
#endif
