/*
  zip_source_winzip_aes_encode.c -- Winzip AES encryption routines
  Copyright (C) 2009-2021 Dieter Baron and Thomas Klausner

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


struct winzip_aes {
    char *password;
    zip_uint16_t encryption_method;

    zip_uint8_t data[ZIP_MAX(WINZIP_AES_MAX_HEADER_LENGTH, SHA1_LENGTH)];
    zip_buffer_t *buffer;

    zip_winzip_aes_t *aes_ctx;
    bool eof;
    zip_error_t error;
};


static int encrypt_header(zip_source_t *src, struct winzip_aes *ctx);
static void winzip_aes_free(struct winzip_aes *);
static zip_int64_t winzip_aes_encrypt(zip_source_t *src, void *ud, void *data, zip_uint64_t len, zip_source_cmd_t cmd);
static struct winzip_aes *winzip_aes_new(zip_uint16_t encryption_method, const char *password, zip_error_t *error);


zip_source_t *
zip_source_winzip_aes_encode(zip_t *za, zip_source_t *src, zip_uint16_t encryption_method, int flags, const char *password) {
    zip_source_t *s2;
    struct winzip_aes *ctx;

    if ((encryption_method != ZIP_EM_AES_128 && encryption_method != ZIP_EM_AES_192 && encryption_method != ZIP_EM_AES_256) || password == NULL || src == NULL) {
        zip_error_set(&za->error, ZIP_ER_INVAL, 0);
        return NULL;
    }

    if ((ctx = winzip_aes_new(encryption_method, password, &za->error)) == NULL) {
        return NULL;
    }

    if ((s2 = zip_source_layered(za, src, winzip_aes_encrypt, ctx)) == NULL) {
        winzip_aes_free(ctx);
        return NULL;
    }

    return s2;
}


static int
encrypt_header(zip_source_t *src, struct winzip_aes *ctx) {
    zip_uint16_t salt_length = SALT_LENGTH(ctx->encryption_method);
    if (!zip_secure_random(ctx->data, salt_length)) {
        zip_error_set(&ctx->error, ZIP_ER_INTERNAL, 0);
        return -1;
    }

    if ((ctx->aes_ctx = _zip_winzip_aes_new((zip_uint8_t *)ctx->password, strlen(ctx->password), ctx->data, ctx->encryption_method, ctx->data + salt_length, &ctx->error)) == NULL) {
        return -1;
    }

    if ((ctx->buffer = _zip_buffer_new(ctx->data, salt_length + WINZIP_AES_PASSWORD_VERIFY_LENGTH)) == NULL) {
        _zip_winzip_aes_free(ctx->aes_ctx);
        ctx->aes_ctx = NULL;
        zip_error_set(&ctx->error, ZIP_ER_MEMORY, 0);
        return -1;
    }

    return 0;
}


static zip_int64_t
winzip_aes_encrypt(zip_source_t *src, void *ud, void *data, zip_uint64_t length, zip_source_cmd_t cmd) {
    struct winzip_aes *ctx;
    zip_int64_t ret;
    zip_uint64_t buffer_n;

    ctx = (struct winzip_aes *)ud;

    switch (cmd) {
    case ZIP_SOURCE_OPEN:
        ctx->eof = false;
        if (encrypt_header(src, ctx) < 0) {
            return -1;
        }
        return 0;

    case ZIP_SOURCE_READ:
        buffer_n = 0;

        if (ctx->buffer) {
            buffer_n = _zip_buffer_read(ctx->buffer, data, length);

            data = (zip_uint8_t *)data + buffer_n;
            length -= buffer_n;

            if (_zip_buffer_eof(ctx->buffer)) {
                _zip_buffer_free(ctx->buffer);
                ctx->buffer = NULL;
            }
        }

        if (ctx->eof) {
            return (zip_int64_t)buffer_n;
        }

        if ((ret = zip_source_read(src, data, length)) < 0) {
            zip_error_set_from_source(&ctx->error, src);
            return -1;
        }

        if (!_zip_winzip_aes_encrypt(ctx->aes_ctx, data, (zip_uint64_t)ret)) {
            zip_error_set(&ctx->error, ZIP_ER_INTERNAL, 0);
            /* TODO: return partial read? */
            return -1;
        }

        if ((zip_uint64_t)ret < length) {
            ctx->eof = true;
            if (!_zip_winzip_aes_finish(ctx->aes_ctx, ctx->data)) {
                zip_error_set(&ctx->error, ZIP_ER_INTERNAL, 0);
                /* TODO: return partial read? */
                return -1;
            }
            _zip_winzip_aes_free(ctx->aes_ctx);
            ctx->aes_ctx = NULL;
            if ((ctx->buffer = _zip_buffer_new(ctx->data, HMAC_LENGTH)) == NULL) {
                zip_error_set(&ctx->error, ZIP_ER_MEMORY, 0);
                /* TODO: return partial read? */
                return -1;
            }
            buffer_n += _zip_buffer_read(ctx->buffer, (zip_uint8_t *)data + ret, length - (zip_uint64_t)ret);
        }

        return (zip_int64_t)(buffer_n + (zip_uint64_t)ret);

    case ZIP_SOURCE_CLOSE:
        return 0;

    case ZIP_SOURCE_STAT: {
        zip_stat_t *st;

        st = (zip_stat_t *)data;
        st->encryption_method = ctx->encryption_method;
        st->valid |= ZIP_STAT_ENCRYPTION_METHOD;
        if (st->valid & ZIP_STAT_COMP_SIZE) {
            st->comp_size += 12 + SALT_LENGTH(ctx->encryption_method);
        }

        return 0;
    }

    case ZIP_SOURCE_GET_FILE_ATTRIBUTES: {
        zip_file_attributes_t *attributes = (zip_file_attributes_t *)data;
        if (length < sizeof(*attributes)) {
            zip_error_set(&ctx->error, ZIP_ER_INVAL, 0);
            return -1;
        }
        attributes->valid |= ZIP_FILE_ATTRIBUTES_VERSION_NEEDED;
        attributes->version_needed = 51;

        return 0;
    }

    case ZIP_SOURCE_SUPPORTS:
        return zip_source_make_command_bitmap(ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_CLOSE, ZIP_SOURCE_STAT, ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, ZIP_SOURCE_GET_FILE_ATTRIBUTES, -1);

    case ZIP_SOURCE_ERROR:
        return zip_error_to_data(&ctx->error, data, length);

    case ZIP_SOURCE_FREE:
        winzip_aes_free(ctx);
        return 0;

    default:
        return zip_source_pass_to_lower_layer(src, data, length, cmd);
    }
}


static void
winzip_aes_free(struct winzip_aes *ctx) {
    if (ctx == NULL) {
        return;
    }

    _zip_crypto_clear(ctx->password, strlen(ctx->password));
    free(ctx->password);
    zip_error_fini(&ctx->error);
    _zip_buffer_free(ctx->buffer);
    _zip_winzip_aes_free(ctx->aes_ctx);
    free(ctx);
}


static struct winzip_aes *
winzip_aes_new(zip_uint16_t encryption_method, const char *password, zip_error_t *error) {
    struct winzip_aes *ctx;

    if ((ctx = (struct winzip_aes *)malloc(sizeof(*ctx))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    if ((ctx->password = strdup(password)) == NULL) {
        free(ctx);
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    ctx->encryption_method = encryption_method;
    ctx->buffer = NULL;
    ctx->aes_ctx = NULL;

    zip_error_init(&ctx->error);

    ctx->eof = false;
    return ctx;
}
