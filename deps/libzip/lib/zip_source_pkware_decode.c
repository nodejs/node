/*
  zip_source_pkware_decode.c -- Traditional PKWARE decryption routines
  Copyright (C) 2009-2020 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <info@libzip.org>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The names of the authors may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <stdlib.h>
#include <string.h>

#include "zipint.h"

struct trad_pkware {
    char *password;
    zip_pkware_keys_t keys;
    zip_error_t error;
};


static int decrypt_header(zip_source_t *, struct trad_pkware *);
static zip_int64_t pkware_decrypt(zip_source_t *, void *, void *, zip_uint64_t, zip_source_cmd_t);
static struct trad_pkware *trad_pkware_new(const char *password, zip_error_t *error);
static void trad_pkware_free(struct trad_pkware *);


zip_source_t *
zip_source_pkware_decode(zip_t *za, zip_source_t *src, zip_uint16_t em, int flags, const char *password) {
    struct trad_pkware *ctx;
    zip_source_t *s2;

    if (password == NULL || src == NULL || em != ZIP_EM_TRAD_PKWARE) {
        zip_error_set(&za->error, ZIP_ER_INVAL, 0);
        return NULL;
    }
    if (flags & ZIP_CODEC_ENCODE) {
        zip_error_set(&za->error, ZIP_ER_ENCRNOTSUPP, 0);
        return NULL;
    }

    if ((ctx = trad_pkware_new(password, &za->error)) == NULL) {
        return NULL;
    }

    if ((s2 = zip_source_layered(za, src, pkware_decrypt, ctx)) == NULL) {
        trad_pkware_free(ctx);
        return NULL;
    }

    return s2;
}


static int
decrypt_header(zip_source_t *src, struct trad_pkware *ctx) {
    zip_uint8_t header[ZIP_CRYPTO_PKWARE_HEADERLEN];
    struct zip_stat st;
    zip_int64_t n;
    bool ok = false;

    if ((n = zip_source_read(src, header, ZIP_CRYPTO_PKWARE_HEADERLEN)) < 0) {
        zip_error_set_from_source(&ctx->error, src);
        return -1;
    }

    if (n != ZIP_CRYPTO_PKWARE_HEADERLEN) {
        zip_error_set(&ctx->error, ZIP_ER_EOF, 0);
        return -1;
    }

    _zip_pkware_decrypt(&ctx->keys, header, header, ZIP_CRYPTO_PKWARE_HEADERLEN);

    if (zip_source_stat(src, &st)) {
        /* stat failed, skip password validation */
        return 0;
    }

    /* password verification - two ways:
     *  mtime - InfoZIP way, to avoid computing complete CRC before encrypting data
     *  CRC - old PKWare way
     */

    if (st.valid & ZIP_STAT_MTIME) {
        unsigned short dostime, dosdate;
        _zip_u2d_time(st.mtime, &dostime, &dosdate);
        if (header[ZIP_CRYPTO_PKWARE_HEADERLEN - 1] == dostime >> 8) {
            ok = true;
        }
    }

    if (st.valid & ZIP_STAT_CRC) {
        if (header[ZIP_CRYPTO_PKWARE_HEADERLEN - 1] == st.crc >> 24) {
            ok = true;
        }
    }

    if (!ok && ((st.valid & (ZIP_STAT_MTIME | ZIP_STAT_CRC)) != 0)) {
        zip_error_set(&ctx->error, ZIP_ER_WRONGPASSWD, 0);
        return -1;
    }

    return 0;
}


static zip_int64_t
pkware_decrypt(zip_source_t *src, void *ud, void *data, zip_uint64_t len, zip_source_cmd_t cmd) {
    struct trad_pkware *ctx;
    zip_int64_t n;

    ctx = (struct trad_pkware *)ud;

    switch (cmd) {
    case ZIP_SOURCE_OPEN:
        _zip_pkware_keys_reset(&ctx->keys);
        _zip_pkware_decrypt(&ctx->keys, NULL, (const zip_uint8_t *)ctx->password, strlen(ctx->password));
        if (decrypt_header(src, ctx) < 0) {
            return -1;
        }
        return 0;

    case ZIP_SOURCE_READ:
        if ((n = zip_source_read(src, data, len)) < 0) {
            zip_error_set_from_source(&ctx->error, src);
            return -1;
        }

        _zip_pkware_decrypt(&ctx->keys, (zip_uint8_t *)data, (zip_uint8_t *)data, (zip_uint64_t)n);
        return n;

    case ZIP_SOURCE_CLOSE:
        return 0;

    case ZIP_SOURCE_STAT: {
        zip_stat_t *st;

        st = (zip_stat_t *)data;

        st->encryption_method = ZIP_EM_NONE;
        st->valid |= ZIP_STAT_ENCRYPTION_METHOD;
        if (st->valid & ZIP_STAT_COMP_SIZE) {
            st->comp_size -= ZIP_CRYPTO_PKWARE_HEADERLEN;
        }

        return 0;
    }

    case ZIP_SOURCE_SUPPORTS:
        return zip_source_make_command_bitmap(ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_CLOSE, ZIP_SOURCE_STAT, ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, ZIP_SOURCE_SUPPORTS_REOPEN, -1);

    case ZIP_SOURCE_ERROR:
        return zip_error_to_data(&ctx->error, data, len);

    case ZIP_SOURCE_FREE:
        trad_pkware_free(ctx);
        return 0;

    default:
        return zip_source_pass_to_lower_layer(src, data, len, cmd);
    }
}


static struct trad_pkware *
trad_pkware_new(const char *password, zip_error_t *error) {
    struct trad_pkware *ctx;

    if ((ctx = (struct trad_pkware *)malloc(sizeof(*ctx))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    if ((ctx->password = strdup(password)) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        free(ctx);
        return NULL;
    }

    zip_error_init(&ctx->error);

    return ctx;
}


static void
trad_pkware_free(struct trad_pkware *ctx) {
    if (ctx == NULL) {
        return;
    }

    free(ctx->password);
    free(ctx);
}
