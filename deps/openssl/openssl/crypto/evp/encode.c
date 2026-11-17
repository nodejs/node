/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <limits.h>
#include <assert.h>
#include "internal/cryptlib.h"
#include <openssl/evp.h>
#include "crypto/evp.h"
#include "evp_local.h"

#if defined(OPENSSL_CPUID_OBJ) && !defined(OPENSSL_NO_ASM) && (defined(__x86_64) || defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
#if !defined(_M_ARM64EC)
#define HAS_IA32CAP_IS_64
#endif /* !defined(_M_ARM64EC) */
#endif

#include "enc_b64_avx2.h"
#include "enc_b64_scalar.h"

static unsigned char conv_ascii2bin(unsigned char a,
    const unsigned char *table);
size_t evp_encodeblock_int(EVP_ENCODE_CTX *ctx, unsigned char *t,
    const unsigned char *f, int dlen, int *wrap_cnt);
static int evp_decodeblock_int(EVP_ENCODE_CTX *ctx, unsigned char *t,
    const unsigned char *f, int n, int eof);
/*-
 * 64 char lines
 * pad input with 0
 * left over chars are set to =
 * 1 byte  => xx==
 * 2 bytes => xxx=
 * 3 bytes => xxxx
 */
#define BIN_PER_LINE (64 / 4 * 3)
#define CHUNKS_PER_LINE (64 / 4)
#define CHAR_PER_LINE (64 + 1)

/*-
 * 0xF0 is a EOLN
 * 0xF1 is ignore but next needs to be 0xF0 (for \r\n processing).
 * 0xF2 is EOF
 * 0xE0 is ignore at start of line.
 * 0xFF is error
 */

#define B64_EOLN 0xF0
#define B64_CR 0xF1
#define B64_EOF 0xF2
#define B64_WS 0xE0
#define B64_ERROR 0xFF
#define B64_NOT_BASE64(a) (((a) | 0x13) == 0xF3)
#define B64_BASE64(a) (!B64_NOT_BASE64(a))

static const unsigned char data_ascii2bin[128] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0,
    0xF0, 0xFF, 0xFF, 0xF1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xF2, 0xFF, 0x3F, 0x34, 0x35,
    0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF,
    0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1A, 0x1B, 0x1C,
    0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
    0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const unsigned char srpdata_ascii2bin[128] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0,
    0xF0, 0xFF, 0xFF, 0xF1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF2, 0x3E, 0x3F, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0xFF,
    0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22,
    0x23, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x24, 0x25, 0x26,
    0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
    0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

#ifndef CHARSET_EBCDIC
static unsigned char conv_ascii2bin(unsigned char a, const unsigned char *table)
{
    if (a & 0x80)
        return B64_ERROR;
    return table[a];
}
#else
static unsigned char conv_ascii2bin(unsigned char a, const unsigned char *table)
{
    a = os_toascii[a];
    if (a & 0x80)
        return B64_ERROR;
    return table[a];
}
#endif

EVP_ENCODE_CTX *EVP_ENCODE_CTX_new(void)
{
    return OPENSSL_zalloc(sizeof(EVP_ENCODE_CTX));
}

void EVP_ENCODE_CTX_free(EVP_ENCODE_CTX *ctx)
{
    OPENSSL_free(ctx);
}

int EVP_ENCODE_CTX_copy(EVP_ENCODE_CTX *dctx, const EVP_ENCODE_CTX *sctx)
{
    memcpy(dctx, sctx, sizeof(EVP_ENCODE_CTX));

    return 1;
}

int EVP_ENCODE_CTX_num(EVP_ENCODE_CTX *ctx)
{
    return ctx->num;
}

void evp_encode_ctx_set_flags(EVP_ENCODE_CTX *ctx, unsigned int flags)
{
    ctx->flags = flags;
}

void EVP_EncodeInit(EVP_ENCODE_CTX *ctx)
{
    ctx->num = 0;
    ctx->line_num = 0;
    ctx->flags = 0;
}

int EVP_EncodeUpdate(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
    const unsigned char *in, int inl)
{
    int i;
    size_t j;
    size_t total = 0;

    *outl = 0;
    if (inl <= 0)
        return 0;
    assert(EVP_ENCODE_B64_LENGTH <= (int)sizeof(ctx->enc_data));
    if (EVP_ENCODE_B64_LENGTH - ctx->num > inl) {
        memcpy(&(ctx->enc_data[ctx->num]), in, inl);
        ctx->num += inl;
        return 1;
    }
    if (ctx->num != 0) {
        i = EVP_ENCODE_B64_LENGTH - ctx->num;
        memcpy(&(ctx->enc_data[ctx->num]), in, i);
        in += i;
        inl -= i;
        int wrap_cnt = 0;
        j = evp_encodeblock_int(ctx, out, ctx->enc_data, EVP_ENCODE_B64_LENGTH,
            &wrap_cnt);
        ctx->num = 0;
        out += j;
        total = j;
        *out = '\0';
    }
    int wrap_cnt = 0;
    if (EVP_ENCODE_B64_LENGTH % 3 != 0) {
        j = evp_encodeblock_int(ctx, out, in, inl - (inl % EVP_ENCODE_B64_LENGTH),
            &wrap_cnt);
    } else {
#if defined(__AVX2__) && defined(HAVE_AVX2_INTRINSICS)
        const int newlines = !(ctx->flags & EVP_ENCODE_CTX_NO_NEWLINES) ? EVP_ENCODE_B64_LENGTH : 0;

        j = encode_base64_avx2(ctx,
            (unsigned char *)out,
            (const unsigned char *)in,
            inl - (inl % EVP_ENCODE_B64_LENGTH), newlines, &wrap_cnt);
#elif defined(HAS_IA32CAP_IS_64) && defined(HAVE_AVX2_INTRINSICS)
        if ((OPENSSL_ia32cap_P[2] & (1u << 5)) != 0) {
            const int newlines = !(ctx->flags & EVP_ENCODE_CTX_NO_NEWLINES) ? EVP_ENCODE_B64_LENGTH : 0;

            j = encode_base64_avx2(ctx,
                (unsigned char *)out,
                (const unsigned char *)in,
                inl - (inl % EVP_ENCODE_B64_LENGTH), newlines, &wrap_cnt);
        } else {
            j = evp_encodeblock_int(ctx, out, in, inl - (inl % EVP_ENCODE_B64_LENGTH),
                &wrap_cnt);
        }
#else
        j = evp_encodeblock_int(ctx, out, in, inl - (inl % EVP_ENCODE_B64_LENGTH),
            &wrap_cnt);
#endif
    }
    in += inl - (inl % EVP_ENCODE_B64_LENGTH);
    inl -= inl - (inl % EVP_ENCODE_B64_LENGTH);
    out += j;
    total += j;
    if ((ctx->flags & EVP_ENCODE_CTX_NO_NEWLINES) == 0 && EVP_ENCODE_B64_LENGTH % 3 != 0) {
        *(out++) = '\n';
        total++;
    }
    *out = '\0';
    if (total > INT_MAX) {
        /* Too much output data! */
        *outl = 0;
        return 0;
    }
    if (inl != 0)
        memcpy(&(ctx->enc_data[0]), in, inl);
    ctx->num = inl;
    *outl = (int)total;

    return 1;
}

void EVP_EncodeFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl)
{
    size_t ret = 0;
    int wrap_cnt = 0;

    if (ctx->num != 0) {
        ret = evp_encodeblock_int(ctx, out, ctx->enc_data, ctx->num,
            &wrap_cnt);
        if (ret > 0) {
            if ((ctx->flags & EVP_ENCODE_CTX_NO_NEWLINES) == 0)
                out[ret++] = '\n';
            out[ret] = '\0';
            ctx->num = 0;
        }
    }
    *outl = (int)ret;
}

int EVP_EncodeBlock(unsigned char *t, const unsigned char *f, int dlen)
{
    int wrap_cnt = 0;

#if defined(__AVX2__) && defined(HAVE_AVX2_INTRINSICS)
    return (int)encode_base64_avx2(NULL, t, f, dlen, 0, &wrap_cnt);
#elif defined(HAS_IA32CAP_IS_64) && defined(HAVE_AVX2_INTRINSICS)
    if ((OPENSSL_ia32cap_P[2] & (1u << 5)) != 0)
        return (int)encode_base64_avx2(NULL, t, f, dlen, 0, &wrap_cnt);
    else
        return (int)evp_encodeblock_int(NULL, t, f, dlen, &wrap_cnt);
#else
    return (int)evp_encodeblock_int(NULL, t, f, dlen, &wrap_cnt);
#endif
}

void EVP_DecodeInit(EVP_ENCODE_CTX *ctx)
{
    /* Only ctx->num and ctx->flags are used during decoding. */
    ctx->num = 0;
    ctx->line_num = 0;
    ctx->flags = 0;
}

/*-
 * -1 for error
 *  0 for last line
 *  1 for full line
 *
 * Note: even though EVP_DecodeUpdate attempts to detect and report end of
 * content, the context doesn't currently remember it and will accept more data
 * in the next call. Therefore, the caller is responsible for checking and
 * rejecting a 0 return value in the middle of content.
 *
 * Note: even though EVP_DecodeUpdate has historically tried to detect end of
 * content based on line length, this has never worked properly. Therefore,
 * we now return 0 when one of the following is true:
 *   - Padding or B64_EOF was detected and the last block is complete.
 *   - Input has zero-length.
 * -1 is returned if:
 *   - Invalid characters are detected.
 *   - There is extra trailing padding, or data after padding.
 *   - B64_EOF is detected after an incomplete base64 block.
 */
int EVP_DecodeUpdate(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
    const unsigned char *in, int inl)
{
    int seof = 0, eof = 0, rv = -1, ret = 0, i, v, tmp, n, decoded_len;
    unsigned char *d;
    const unsigned char *table;

    n = ctx->num;
    d = ctx->enc_data;

    if (n > 0 && d[n - 1] == '=') {
        eof++;
        if (n > 1 && d[n - 2] == '=')
            eof++;
    }

    /* Legacy behaviour: an empty input chunk signals end of input. */
    if (inl == 0) {
        rv = 0;
        goto end;
    }

    if ((ctx->flags & EVP_ENCODE_CTX_USE_SRP_ALPHABET) != 0)
        table = srpdata_ascii2bin;
    else
        table = data_ascii2bin;

    for (i = 0; i < inl; i++) {
        tmp = *(in++);
        v = conv_ascii2bin(tmp, table);
        if (v == B64_ERROR) {
            rv = -1;
            goto end;
        }

        if (tmp == '=') {
            eof++;
        } else if (eof > 0 && B64_BASE64(v)) {
            /* More data after padding. */
            rv = -1;
            goto end;
        }

        if (eof > 2) {
            rv = -1;
            goto end;
        }

        if (v == B64_EOF) {
            seof = 1;
            goto tail;
        }

        /* Only save valid base64 characters. */
        if (B64_BASE64(v)) {
            if (n >= 64) {
                /*
                 * We increment n once per loop, and empty the buffer as soon as
                 * we reach 64 characters, so this can only happen if someone's
                 * manually messed with the ctx. Refuse to write any more data.
                 */
                rv = -1;
                goto end;
            }
            OPENSSL_assert(n < (int)sizeof(ctx->enc_data));
            d[n++] = tmp;
        }

        if (n == 64) {
            decoded_len = evp_decodeblock_int(ctx, out, d, n, eof);
            n = 0;
            if (decoded_len < 0 || (decoded_len == 0 && eof > 0)) {
                rv = -1;
                goto end;
            }
            ret += decoded_len;
            out += decoded_len;
        }
    }

    /*
     * Legacy behaviour: if the current line is a full base64-block (i.e., has
     * 0 mod 4 base64 characters), it is processed immediately. We keep this
     * behaviour as applications may not be calling EVP_DecodeFinal properly.
     */
tail:
    if (n > 0) {
        if ((n & 3) == 0) {
            decoded_len = evp_decodeblock_int(ctx, out, d, n, eof);
            n = 0;
            if (decoded_len < 0 || (decoded_len == 0 && eof > 0)) {
                rv = -1;
                goto end;
            }
            ret += decoded_len;
        } else if (seof) {
            /* EOF in the middle of a base64 block. */
            rv = -1;
            goto end;
        }
    }

    rv = seof || (n == 0 && eof) ? 0 : 1;
end:
    /* Legacy behaviour. This should probably rather be zeroed on error. */
    *outl = ret;
    ctx->num = n;
    return rv;
}

static int evp_decodeblock_int(EVP_ENCODE_CTX *ctx, unsigned char *t,
    const unsigned char *f, int n,
    int eof)
{
    int i, ret = 0, a, b, c, d;
    unsigned long l;
    const unsigned char *table;

    if (eof < -1 || eof > 2)
        return -1;

    if (ctx != NULL && (ctx->flags & EVP_ENCODE_CTX_USE_SRP_ALPHABET) != 0)
        table = srpdata_ascii2bin;
    else
        table = data_ascii2bin;

    /* trim whitespace from the start of the line. */
    while ((n > 0) && (conv_ascii2bin(*f, table) == B64_WS)) {
        f++;
        n--;
    }

    /*
     * strip off stuff at the end of the line ascii2bin values B64_WS,
     * B64_EOLN, B64_EOLN and B64_EOF
     */
    while ((n > 3) && (B64_NOT_BASE64(conv_ascii2bin(f[n - 1], table))))
        n--;

    if (n % 4 != 0)
        return -1;
    if (n == 0)
        return 0;

    /* all 4-byte blocks except the last one do not have padding. */
    for (i = 0; i < n - 4; i += 4) {
        a = conv_ascii2bin(*(f++), table);
        b = conv_ascii2bin(*(f++), table);
        c = conv_ascii2bin(*(f++), table);
        d = conv_ascii2bin(*(f++), table);
        if ((a | b | c | d) & 0x80)
            return -1;
        l = ((((unsigned long)a) << 18L) | (((unsigned long)b) << 12L) | (((unsigned long)c) << 6L) | (((unsigned long)d)));
        *(t++) = (unsigned char)(l >> 16L) & 0xff;
        *(t++) = (unsigned char)(l >> 8L) & 0xff;
        *(t++) = (unsigned char)(l) & 0xff;
        ret += 3;
    }

    /* process the last block that may have padding. */
    a = conv_ascii2bin(*(f++), table);
    b = conv_ascii2bin(*(f++), table);
    c = conv_ascii2bin(*(f++), table);
    d = conv_ascii2bin(*(f++), table);
    if ((a | b | c | d) & 0x80)
        return -1;
    l = ((((unsigned long)a) << 18L) | (((unsigned long)b) << 12L) | (((unsigned long)c) << 6L) | (((unsigned long)d)));

    if (eof == -1)
        eof = (c == '=') + (d == '=');

    switch (eof) {
    case 2:
        *t = (unsigned char)(l >> 16L) & 0xff;
        break;
    case 1:
        *(t++) = (unsigned char)(l >> 16L) & 0xff;
        *t = (unsigned char)(l >> 8L) & 0xff;
        break;
    case 0:
        *(t++) = (unsigned char)(l >> 16L) & 0xff;
        *(t++) = (unsigned char)(l >> 8L) & 0xff;
        *t = (unsigned char)(l) & 0xff;
        break;
    }
    ret += 3 - eof;

    return ret;
}

int EVP_DecodeBlock(unsigned char *t, const unsigned char *f, int n)
{
    return evp_decodeblock_int(NULL, t, f, n, 0);
}

int EVP_DecodeFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl)
{
    int i;

    *outl = 0;
    if (ctx->num != 0) {
        i = evp_decodeblock_int(ctx, out, ctx->enc_data, ctx->num, -1);
        if (i < 0)
            return -1;
        ctx->num = 0;
        *outl = i;
        return 1;
    } else
        return 1;
}
