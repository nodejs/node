/*
 * Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include "internal/cryptlib.h"
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>

int RSA_padding_add_SSLv23(unsigned char *to, int tlen,
                           const unsigned char *from, int flen)
{
    int i, j;
    unsigned char *p;

    if (flen > (tlen - 11)) {
        RSAerr(RSA_F_RSA_PADDING_ADD_SSLV23,
               RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
        return (0);
    }

    p = (unsigned char *)to;

    *(p++) = 0;
    *(p++) = 2;                 /* Public Key BT (Block Type) */

    /* pad out with non-zero random data */
    j = tlen - 3 - 8 - flen;

    if (RAND_bytes(p, j) <= 0)
        return (0);
    for (i = 0; i < j; i++) {
        if (*p == '\0')
            do {
                if (RAND_bytes(p, 1) <= 0)
                    return (0);
            } while (*p == '\0');
        p++;
    }

    memset(p, 3, 8);
    p += 8;
    *(p++) = '\0';

    memcpy(p, from, (unsigned int)flen);
    return (1);
}

int RSA_padding_check_SSLv23(unsigned char *to, int tlen,
                             const unsigned char *from, int flen, int num)
{
    int i, j, k;
    const unsigned char *p;

    p = from;
    if (flen < 10) {
        RSAerr(RSA_F_RSA_PADDING_CHECK_SSLV23, RSA_R_DATA_TOO_SMALL);
        return (-1);
    }
    /* Accept even zero-padded input */
    if (flen == num) {
        if (*(p++) != 0) {
            RSAerr(RSA_F_RSA_PADDING_CHECK_SSLV23, RSA_R_BLOCK_TYPE_IS_NOT_02);
            return -1;
        }
        flen--;
    }
    if ((num != (flen + 1)) || (*(p++) != 02)) {
        RSAerr(RSA_F_RSA_PADDING_CHECK_SSLV23, RSA_R_BLOCK_TYPE_IS_NOT_02);
        return (-1);
    }

    /* scan over padding data */
    j = flen - 1;               /* one for type */
    for (i = 0; i < j; i++)
        if (*(p++) == 0)
            break;

    if ((i == j) || (i < 8)) {
        RSAerr(RSA_F_RSA_PADDING_CHECK_SSLV23,
               RSA_R_NULL_BEFORE_BLOCK_MISSING);
        return (-1);
    }
    for (k = -9; k < -1; k++) {
        if (p[k] != 0x03)
            break;
    }
    if (k == -1) {
        RSAerr(RSA_F_RSA_PADDING_CHECK_SSLV23, RSA_R_SSLV3_ROLLBACK_ATTACK);
        return (-1);
    }

    i++;                        /* Skip over the '\0' */
    j -= i;
    if (j > tlen) {
        RSAerr(RSA_F_RSA_PADDING_CHECK_SSLV23, RSA_R_DATA_TOO_LARGE);
        return (-1);
    }
    memcpy(to, p, (unsigned int)j);

    return (j);
}
