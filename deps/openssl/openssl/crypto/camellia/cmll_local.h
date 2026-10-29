/*
 * Copyright 2006-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* ====================================================================
 * Copyright 2006 NTT (Nippon Telegraph and Telephone Corporation) .
 * ALL RIGHTS RESERVED.
 *
 * Intellectual Property information for Camellia:
 *     http://info.isl.ntt.co.jp/crypt/eng/info/chiteki.html
 *
 * News Release for Announcement of Camellia open source:
 *     http://www.ntt.co.jp/news/news06e/0604/060413a.html
 *
 * The Camellia Code included herein is developed by
 * NTT (Nippon Telegraph and Telephone Corporation), and is contributed
 * to the OpenSSL project.
 */

#ifndef OSSL_CRYPTO_CAMELLIA_CMLL_LOCAL_H
#define OSSL_CRYPTO_CAMELLIA_CMLL_LOCAL_H

#include <stdint.h>
#include <openssl/camellia.h>

int Camellia_Ekeygen(int keyBitLength, const uint8_t *rawKey,
    KEY_TABLE_TYPE keyTable);
void Camellia_EncryptBlock_Rounds(int grandRounds, const uint8_t plaintext[],
    const KEY_TABLE_TYPE keyTable,
    uint8_t ciphertext[]);
void Camellia_DecryptBlock_Rounds(int grandRounds, const uint8_t ciphertext[],
    const KEY_TABLE_TYPE keyTable,
    uint8_t plaintext[]);
void Camellia_EncryptBlock(int keyBitLength, const uint8_t plaintext[],
    const KEY_TABLE_TYPE keyTable, uint8_t ciphertext[]);
void Camellia_DecryptBlock(int keyBitLength, const uint8_t ciphertext[],
    const KEY_TABLE_TYPE keyTable, uint8_t plaintext[]);
#endif /* #ifndef OSSL_CRYPTO_CAMELLIA_CMLL_LOCAL_H */
