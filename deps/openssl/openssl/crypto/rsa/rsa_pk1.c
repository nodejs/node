/*
 * Copyright 1995-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * RSA low level APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include "internal/constant_time.h"

#include <stdio.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
/* Just for the SSL_MAX_MASTER_KEY_LENGTH value */
#include <openssl/ssl.h>
#include "internal/cryptlib.h"
#include "crypto/rsa.h"
#include "rsa_local.h"

int RSA_padding_add_PKCS1_type_1(unsigned char *to, int tlen,
                                 const unsigned char *from, int flen)
{
    int j;
    unsigned char *p;

    if (flen > (tlen - RSA_PKCS1_PADDING_SIZE)) {
        ERR_raise(ERR_LIB_RSA, RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
        return 0;
    }

    p = (unsigned char *)to;

    *(p++) = 0;
    *(p++) = 1;                 /* Private Key BT (Block Type) */

    /* pad out with 0xff data */
    j = tlen - 3 - flen;
    memset(p, 0xff, j);
    p += j;
    *(p++) = '\0';
    memcpy(p, from, (unsigned int)flen);
    return 1;
}

int RSA_padding_check_PKCS1_type_1(unsigned char *to, int tlen,
                                   const unsigned char *from, int flen,
                                   int num)
{
    int i, j;
    const unsigned char *p;

    p = from;

    /*
     * The format is
     * 00 || 01 || PS || 00 || D
     * PS - padding string, at least 8 bytes of FF
     * D  - data.
     */

    if (num < RSA_PKCS1_PADDING_SIZE)
        return -1;

    /* Accept inputs with and without the leading 0-byte. */
    if (num == flen) {
        if ((*p++) != 0x00) {
            ERR_raise(ERR_LIB_RSA, RSA_R_INVALID_PADDING);
            return -1;
        }
        flen--;
    }

    if ((num != (flen + 1)) || (*(p++) != 0x01)) {
        ERR_raise(ERR_LIB_RSA, RSA_R_BLOCK_TYPE_IS_NOT_01);
        return -1;
    }

    /* scan over padding data */
    j = flen - 1;               /* one for type. */
    for (i = 0; i < j; i++) {
        if (*p != 0xff) {       /* should decrypt to 0xff */
            if (*p == 0) {
                p++;
                break;
            } else {
                ERR_raise(ERR_LIB_RSA, RSA_R_BAD_FIXED_HEADER_DECRYPT);
                return -1;
            }
        }
        p++;
    }

    if (i == j) {
        ERR_raise(ERR_LIB_RSA, RSA_R_NULL_BEFORE_BLOCK_MISSING);
        return -1;
    }

    if (i < 8) {
        ERR_raise(ERR_LIB_RSA, RSA_R_BAD_PAD_BYTE_COUNT);
        return -1;
    }
    i++;                        /* Skip over the '\0' */
    j -= i;
    if (j > tlen) {
        ERR_raise(ERR_LIB_RSA, RSA_R_DATA_TOO_LARGE);
        return -1;
    }
    memcpy(to, p, (unsigned int)j);

    return j;
}

int ossl_rsa_padding_add_PKCS1_type_2_ex(OSSL_LIB_CTX *libctx, unsigned char *to,
                                         int tlen, const unsigned char *from,
                                         int flen)
{
    int i, j;
    unsigned char *p;

    if (flen > (tlen - RSA_PKCS1_PADDING_SIZE)) {
        ERR_raise(ERR_LIB_RSA, RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
        return 0;
    }

    p = (unsigned char *)to;

    *(p++) = 0;
    *(p++) = 2;                 /* Public Key BT (Block Type) */

    /* pad out with non-zero random data */
    j = tlen - 3 - flen;

    if (RAND_bytes_ex(libctx, p, j) <= 0)
        return 0;
    for (i = 0; i < j; i++) {
        if (*p == '\0')
            do {
                if (RAND_bytes_ex(libctx, p, 1) <= 0)
                    return 0;
            } while (*p == '\0');
        p++;
    }

    *(p++) = '\0';

    memcpy(p, from, (unsigned int)flen);
    return 1;
}

int RSA_padding_add_PKCS1_type_2(unsigned char *to, int tlen,
                                 const unsigned char *from, int flen)
{
    return ossl_rsa_padding_add_PKCS1_type_2_ex(NULL, to, tlen, from, flen);
}

int RSA_padding_check_PKCS1_type_2(unsigned char *to, int tlen,
                                   const unsigned char *from, int flen,
                                   int num)
{
    int i;
    /* |em| is the encoded message, zero-padded to exactly |num| bytes */
    unsigned char *em = NULL;
    unsigned int good, found_zero_byte, mask;
    int zero_index = 0, msg_index, mlen = -1;

    if (tlen <= 0 || flen <= 0)
        return -1;

    /*
     * PKCS#1 v1.5 decryption. See "PKCS #1 v2.2: RSA Cryptography Standard",
     * section 7.2.2.
     */

    if (flen > num || num < RSA_PKCS1_PADDING_SIZE) {
        ERR_raise(ERR_LIB_RSA, RSA_R_PKCS_DECODING_ERROR);
        return -1;
    }

    em = OPENSSL_malloc(num);
    if (em == NULL) {
        ERR_raise(ERR_LIB_RSA, ERR_R_MALLOC_FAILURE);
        return -1;
    }
    /*
     * Caller is encouraged to pass zero-padded message created with
     * BN_bn2binpad. Trouble is that since we can't read out of |from|'s
     * bounds, it's impossible to have an invariant memory access pattern
     * in case |from| was not zero-padded in advance.
     */
    for (from += flen, em += num, i = 0; i < num; i++) {
        mask = ~constant_time_is_zero(flen);
        flen -= 1 & mask;
        from -= 1 & mask;
        *--em = *from & mask;
    }

    good = constant_time_is_zero(em[0]);
    good &= constant_time_eq(em[1], 2);

    /* scan over padding data */
    found_zero_byte = 0;
    for (i = 2; i < num; i++) {
        unsigned int equals0 = constant_time_is_zero(em[i]);

        zero_index = constant_time_select_int(~found_zero_byte & equals0,
                                              i, zero_index);
        found_zero_byte |= equals0;
    }

    /*
     * PS must be at least 8 bytes long, and it starts two bytes into |em|.
     * If we never found a 0-byte, then |zero_index| is 0 and the check
     * also fails.
     */
    good &= constant_time_ge(zero_index, 2 + 8);

    /*
     * Skip the zero byte. This is incorrect if we never found a zero-byte
     * but in this case we also do not copy the message out.
     */
    msg_index = zero_index + 1;
    mlen = num - msg_index;

    /*
     * For good measure, do this check in constant time as well.
     */
    good &= constant_time_ge(tlen, mlen);

    /*
     * Move the result in-place by |num|-RSA_PKCS1_PADDING_SIZE-|mlen| bytes to the left.
     * Then if |good| move |mlen| bytes from |em|+RSA_PKCS1_PADDING_SIZE to |to|.
     * Otherwise leave |to| unchanged.
     * Copy the memory back in a way that does not reveal the size of
     * the data being copied via a timing side channel. This requires copying
     * parts of the buffer multiple times based on the bits set in the real
     * length. Clear bits do a non-copy with identical access pattern.
     * The loop below has overall complexity of O(N*log(N)).
     */
    tlen = constant_time_select_int(constant_time_lt(num - RSA_PKCS1_PADDING_SIZE, tlen),
                                    num - RSA_PKCS1_PADDING_SIZE, tlen);
    for (msg_index = 1; msg_index < num - RSA_PKCS1_PADDING_SIZE; msg_index <<= 1) {
        mask = ~constant_time_eq(msg_index & (num - RSA_PKCS1_PADDING_SIZE - mlen), 0);
        for (i = RSA_PKCS1_PADDING_SIZE; i < num - msg_index; i++)
            em[i] = constant_time_select_8(mask, em[i + msg_index], em[i]);
    }
    for (i = 0; i < tlen; i++) {
        mask = good & constant_time_lt(i, mlen);
        to[i] = constant_time_select_8(mask, em[i + RSA_PKCS1_PADDING_SIZE], to[i]);
    }

    OPENSSL_clear_free(em, num);
#ifndef FIPS_MODULE
    /*
     * This trick doesn't work in the FIPS provider because libcrypto manages
     * the error stack. Instead we opt not to put an error on the stack at all
     * in case of padding failure in the FIPS provider.
     */
    ERR_raise(ERR_LIB_RSA, RSA_R_PKCS_DECODING_ERROR);
    err_clear_last_constant_time(1 & good);
#endif

    return constant_time_select_int(good, mlen, -1);
}

/*
 * ossl_rsa_padding_check_PKCS1_type_2_TLS() checks and removes the PKCS1 type 2
 * padding from a decrypted RSA message in a TLS signature. The result is stored
 * in the buffer pointed to by |to| which should be |tlen| bytes long. |tlen|
 * must be at least SSL_MAX_MASTER_KEY_LENGTH. The original decrypted message
 * should be stored in |from| which must be |flen| bytes in length and padded
 * such that |flen == RSA_size()|. The TLS protocol version that the client
 * originally requested should be passed in |client_version|. Some buggy clients
 * can exist which use the negotiated version instead of the originally
 * requested protocol version. If it is necessary to work around this bug then
 * the negotiated protocol version can be passed in |alt_version|, otherwise 0
 * should be passed.
 *
 * If the passed message is publicly invalid or some other error that can be
 * treated in non-constant time occurs then -1 is returned. On success the
 * length of the decrypted data is returned. This will always be
 * SSL_MAX_MASTER_KEY_LENGTH. If an error occurs that should be treated in
 * constant time then this function will appear to return successfully, but the
 * decrypted data will be randomly generated (as per
 * https://tools.ietf.org/html/rfc5246#section-7.4.7.1).
 */
int ossl_rsa_padding_check_PKCS1_type_2_TLS(OSSL_LIB_CTX *libctx,
                                            unsigned char *to, size_t tlen,
                                            const unsigned char *from,
                                            size_t flen, int client_version,
                                            int alt_version)
{
    unsigned int i, good, version_good;
    unsigned char rand_premaster_secret[SSL_MAX_MASTER_KEY_LENGTH];

    /*
     * If these checks fail then either the message in publicly invalid, or
     * we've been called incorrectly. We can fail immediately.
     */
    if (flen < RSA_PKCS1_PADDING_SIZE + SSL_MAX_MASTER_KEY_LENGTH
            || tlen < SSL_MAX_MASTER_KEY_LENGTH) {
        ERR_raise(ERR_LIB_RSA, RSA_R_PKCS_DECODING_ERROR);
        return -1;
    }

    /*
     * Generate a random premaster secret to use in the event that we fail
     * to decrypt.
     */
    if (RAND_priv_bytes_ex(libctx, rand_premaster_secret,
                           sizeof(rand_premaster_secret)) <= 0) {
        ERR_raise(ERR_LIB_RSA, ERR_R_INTERNAL_ERROR);
        return -1;
    }

    good = constant_time_is_zero(from[0]);
    good &= constant_time_eq(from[1], 2);

    /* Check we have the expected padding data */
    for (i = 2; i < flen - SSL_MAX_MASTER_KEY_LENGTH - 1; i++)
        good &= ~constant_time_is_zero_8(from[i]);
    good &= constant_time_is_zero_8(from[flen - SSL_MAX_MASTER_KEY_LENGTH - 1]);


    /*
     * If the version in the decrypted pre-master secret is correct then
     * version_good will be 0xff, otherwise it'll be zero. The
     * Klima-Pokorny-Rosa extension of Bleichenbacher's attack
     * (http://eprint.iacr.org/2003/052/) exploits the version number
     * check as a "bad version oracle". Thus version checks are done in
     * constant time and are treated like any other decryption error.
     */
    version_good =
        constant_time_eq(from[flen - SSL_MAX_MASTER_KEY_LENGTH],
                         (client_version >> 8) & 0xff);
    version_good &=
        constant_time_eq(from[flen - SSL_MAX_MASTER_KEY_LENGTH + 1],
                         client_version & 0xff);

    /*
     * The premaster secret must contain the same version number as the
     * ClientHello to detect version rollback attacks (strangely, the
     * protocol does not offer such protection for DH ciphersuites).
     * However, buggy clients exist that send the negotiated protocol
     * version instead if the server does not support the requested
     * protocol version. If SSL_OP_TLS_ROLLBACK_BUG is set then we tolerate
     * such clients. In that case alt_version will be non-zero and set to
     * the negotiated version.
     */
    if (alt_version > 0) {
        unsigned int workaround_good;

        workaround_good =
            constant_time_eq(from[flen - SSL_MAX_MASTER_KEY_LENGTH],
                             (alt_version >> 8) & 0xff);
        workaround_good &=
            constant_time_eq(from[flen - SSL_MAX_MASTER_KEY_LENGTH + 1],
                             alt_version & 0xff);
        version_good |= workaround_good;
    }

    good &= version_good;


    /*
     * Now copy the result over to the to buffer if good, or random data if
     * not good.
     */
    for (i = 0; i < SSL_MAX_MASTER_KEY_LENGTH; i++) {
        to[i] =
            constant_time_select_8(good,
                                   from[flen - SSL_MAX_MASTER_KEY_LENGTH + i],
                                   rand_premaster_secret[i]);
    }

    /*
     * We must not leak whether a decryption failure occurs because of
     * Bleichenbacher's attack on PKCS #1 v1.5 RSA padding (see RFC 2246,
     * section 7.4.7.1). The code follows that advice of the TLS RFC and
     * generates a random premaster secret for the case that the decrypt
     * fails. See https://tools.ietf.org/html/rfc5246#section-7.4.7.1
     * So, whether we actually succeeded or not, return success.
     */

    return SSL_MAX_MASTER_KEY_LENGTH;
}
