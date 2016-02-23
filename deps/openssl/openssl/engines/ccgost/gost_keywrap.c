/**********************************************************************
 *                          keywrap.c                                 *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *         This file is distributed under the same license as OpenSSL *
 *                                                                    *
 * Implementation of CryptoPro key wrap algorithm, as defined in      *
 *               RFC 4357 p 6.3 and 6.4                               *
 *                  Doesn't need OpenSSL                              *
 **********************************************************************/
#include <string.h>
#include "gost89.h"
#include "gost_keywrap.h"

/*-
 * Diversifies key using random UserKey Material
 * Implements RFC 4357 p 6.5 key diversification algorithm
 *
 * inputKey - 32byte key to be diversified
 * ukm - 8byte user key material
 * outputKey - 32byte buffer to store diversified key
 *
 */
void keyDiversifyCryptoPro(gost_ctx * ctx, const unsigned char *inputKey,
                           const unsigned char *ukm, unsigned char *outputKey)
{

    u4 k, s1, s2;
    int i, j, mask;
    unsigned char S[8];
    memcpy(outputKey, inputKey, 32);
    for (i = 0; i < 8; i++) {
        /* Make array of integers from key */
        /* Compute IV S */
        s1 = 0, s2 = 0;
        for (j = 0, mask = 1; j < 8; j++, mask <<= 1) {
            k = ((u4) outputKey[4 * j]) | (outputKey[4 * j + 1] << 8) |
                (outputKey[4 * j + 2] << 16) | (outputKey[4 * j + 3] << 24);
            if (mask & ukm[i]) {
                s1 += k;
            } else {
                s2 += k;
            }
        }
        S[0] = (unsigned char)(s1 & 0xff);
        S[1] = (unsigned char)((s1 >> 8) & 0xff);
        S[2] = (unsigned char)((s1 >> 16) & 0xff);
        S[3] = (unsigned char)((s1 >> 24) & 0xff);
        S[4] = (unsigned char)(s2 & 0xff);
        S[5] = (unsigned char)((s2 >> 8) & 0xff);
        S[6] = (unsigned char)((s2 >> 16) & 0xff);
        S[7] = (unsigned char)((s2 >> 24) & 0xff);
        gost_key(ctx, outputKey);
        gost_enc_cfb(ctx, S, outputKey, outputKey, 4);
    }
}

/*-
 * Wraps key using RFC 4357 6.3
 * ctx - gost encryption context, initialized with some S-boxes
 * keyExchangeKey (KEK) 32-byte (256-bit) shared key
 * ukm - 8 byte (64 bit) user key material,
 * sessionKey - 32-byte (256-bit) key to be wrapped
 * wrappedKey - 44-byte buffer to store wrapped key
 */

int keyWrapCryptoPro(gost_ctx * ctx, const unsigned char *keyExchangeKey,
                     const unsigned char *ukm,
                     const unsigned char *sessionKey,
                     unsigned char *wrappedKey)
{
    unsigned char kek_ukm[32];
    keyDiversifyCryptoPro(ctx, keyExchangeKey, ukm, kek_ukm);
    gost_key(ctx, kek_ukm);
    memcpy(wrappedKey, ukm, 8);
    gost_enc(ctx, sessionKey, wrappedKey + 8, 4);
    gost_mac_iv(ctx, 32, ukm, sessionKey, 32, wrappedKey + 40);
    return 1;
}

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

int keyUnwrapCryptoPro(gost_ctx * ctx, const unsigned char *keyExchangeKey,
                       const unsigned char *wrappedKey,
                       unsigned char *sessionKey)
{
    unsigned char kek_ukm[32], cek_mac[4];
    keyDiversifyCryptoPro(ctx, keyExchangeKey, wrappedKey
                          /* First 8 bytes of wrapped Key is ukm */
                          , kek_ukm);
    gost_key(ctx, kek_ukm);
    gost_dec(ctx, wrappedKey + 8, sessionKey, 4);
    gost_mac_iv(ctx, 32, wrappedKey, sessionKey, 32, cek_mac);
    if (memcmp(cek_mac, wrappedKey + 40, 4)) {
        return 0;
    }
    return 1;
}
