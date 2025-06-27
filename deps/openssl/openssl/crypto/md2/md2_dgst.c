/*
 * Copyright 1995-2020 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/*
 * MD2 low level APIs are deprecated for public use, but still ok for
 * internal use.
 */
#include "internal/deprecated.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md2.h>
#include <openssl/opensslv.h>
#include <openssl/crypto.h>

/*
 * Implemented from RFC1319 The MD2 Message-Digest Algorithm
 */

#define UCHAR   unsigned char

static void md2_block(MD2_CTX *c, const unsigned char *d);
/*
 * The magic S table - I have converted it to hex since it is basically just
 * a random byte string.
 */
static const MD2_INT S[256] = {
    0x29, 0x2E, 0x43, 0xC9, 0xA2, 0xD8, 0x7C, 0x01,
    0x3D, 0x36, 0x54, 0xA1, 0xEC, 0xF0, 0x06, 0x13,
    0x62, 0xA7, 0x05, 0xF3, 0xC0, 0xC7, 0x73, 0x8C,
    0x98, 0x93, 0x2B, 0xD9, 0xBC, 0x4C, 0x82, 0xCA,
    0x1E, 0x9B, 0x57, 0x3C, 0xFD, 0xD4, 0xE0, 0x16,
    0x67, 0x42, 0x6F, 0x18, 0x8A, 0x17, 0xE5, 0x12,
    0xBE, 0x4E, 0xC4, 0xD6, 0xDA, 0x9E, 0xDE, 0x49,
    0xA0, 0xFB, 0xF5, 0x8E, 0xBB, 0x2F, 0xEE, 0x7A,
    0xA9, 0x68, 0x79, 0x91, 0x15, 0xB2, 0x07, 0x3F,
    0x94, 0xC2, 0x10, 0x89, 0x0B, 0x22, 0x5F, 0x21,
    0x80, 0x7F, 0x5D, 0x9A, 0x5A, 0x90, 0x32, 0x27,
    0x35, 0x3E, 0xCC, 0xE7, 0xBF, 0xF7, 0x97, 0x03,
    0xFF, 0x19, 0x30, 0xB3, 0x48, 0xA5, 0xB5, 0xD1,
    0xD7, 0x5E, 0x92, 0x2A, 0xAC, 0x56, 0xAA, 0xC6,
    0x4F, 0xB8, 0x38, 0xD2, 0x96, 0xA4, 0x7D, 0xB6,
    0x76, 0xFC, 0x6B, 0xE2, 0x9C, 0x74, 0x04, 0xF1,
    0x45, 0x9D, 0x70, 0x59, 0x64, 0x71, 0x87, 0x20,
    0x86, 0x5B, 0xCF, 0x65, 0xE6, 0x2D, 0xA8, 0x02,
    0x1B, 0x60, 0x25, 0xAD, 0xAE, 0xB0, 0xB9, 0xF6,
    0x1C, 0x46, 0x61, 0x69, 0x34, 0x40, 0x7E, 0x0F,
    0x55, 0x47, 0xA3, 0x23, 0xDD, 0x51, 0xAF, 0x3A,
    0xC3, 0x5C, 0xF9, 0xCE, 0xBA, 0xC5, 0xEA, 0x26,
    0x2C, 0x53, 0x0D, 0x6E, 0x85, 0x28, 0x84, 0x09,
    0xD3, 0xDF, 0xCD, 0xF4, 0x41, 0x81, 0x4D, 0x52,
    0x6A, 0xDC, 0x37, 0xC8, 0x6C, 0xC1, 0xAB, 0xFA,
    0x24, 0xE1, 0x7B, 0x08, 0x0C, 0xBD, 0xB1, 0x4A,
    0x78, 0x88, 0x95, 0x8B, 0xE3, 0x63, 0xE8, 0x6D,
    0xE9, 0xCB, 0xD5, 0xFE, 0x3B, 0x00, 0x1D, 0x39,
    0xF2, 0xEF, 0xB7, 0x0E, 0x66, 0x58, 0xD0, 0xE4,
    0xA6, 0x77, 0x72, 0xF8, 0xEB, 0x75, 0x4B, 0x0A,
    0x31, 0x44, 0x50, 0xB4, 0x8F, 0xED, 0x1F, 0x1A,
    0xDB, 0x99, 0x8D, 0x33, 0x9F, 0x11, 0x83, 0x14,
};

const char *MD2_options(void)
{
    if (sizeof(MD2_INT) == 1)
        return "md2(char)";
    else
        return "md2(int)";
}

int MD2_Init(MD2_CTX *c)
{
    c->num = 0;
    memset(c->state, 0, sizeof(c->state));
    memset(c->cksm, 0, sizeof(c->cksm));
    memset(c->data, 0, sizeof(c->data));
    return 1;
}

int MD2_Update(MD2_CTX *c, const unsigned char *data, size_t len)
{
    register UCHAR *p;

    if (len == 0)
        return 1;

    p = c->data;
    if (c->num != 0) {
        if ((c->num + len) >= MD2_BLOCK) {
            memcpy(&(p[c->num]), data, MD2_BLOCK - c->num);
            md2_block(c, c->data);
            data += (MD2_BLOCK - c->num);
            len -= (MD2_BLOCK - c->num);
            c->num = 0;
            /* drop through and do the rest */
        } else {
            memcpy(&(p[c->num]), data, len);
            /* data+=len; */
            c->num += (int)len;
            return 1;
        }
    }
    /*
     * we now can process the input data in blocks of MD2_BLOCK chars and
     * save the leftovers to c->data.
     */
    while (len >= MD2_BLOCK) {
        md2_block(c, data);
        data += MD2_BLOCK;
        len -= MD2_BLOCK;
    }
    memcpy(p, data, len);
    c->num = (int)len;
    return 1;
}

static void md2_block(MD2_CTX *c, const unsigned char *d)
{
    register MD2_INT t, *sp1, *sp2;
    register int i, j;
    MD2_INT state[48];

    sp1 = c->state;
    sp2 = c->cksm;
    j = sp2[MD2_BLOCK - 1];
    for (i = 0; i < 16; i++) {
        state[i] = sp1[i];
        state[i + 16] = t = d[i];
        state[i + 32] = (t ^ sp1[i]);
        j = sp2[i] ^= S[t ^ j];
    }
    t = 0;
    for (i = 0; i < 18; i++) {
        for (j = 0; j < 48; j += 8) {
            t = state[j + 0] ^= S[t];
            t = state[j + 1] ^= S[t];
            t = state[j + 2] ^= S[t];
            t = state[j + 3] ^= S[t];
            t = state[j + 4] ^= S[t];
            t = state[j + 5] ^= S[t];
            t = state[j + 6] ^= S[t];
            t = state[j + 7] ^= S[t];
        }
        t = (t + i) & 0xff;
    }
    memcpy(sp1, state, 16 * sizeof(MD2_INT));
    OPENSSL_cleanse(state, 48 * sizeof(MD2_INT));
}

int MD2_Final(unsigned char *md, MD2_CTX *c)
{
    int i, v;
    register UCHAR *cp;
    register MD2_INT *p1, *p2;

    cp = c->data;
    p1 = c->state;
    p2 = c->cksm;
    v = MD2_BLOCK - c->num;
    for (i = c->num; i < MD2_BLOCK; i++)
        cp[i] = (UCHAR) v;

    md2_block(c, cp);

    for (i = 0; i < MD2_BLOCK; i++)
        cp[i] = (UCHAR) p2[i];
    md2_block(c, cp);

    for (i = 0; i < 16; i++)
        md[i] = (UCHAR) (p1[i] & 0xff);
    OPENSSL_cleanse(c, sizeof(*c));
    return 1;
}
