/*
  zip_source_winzip_aes_decode.c -- Winzip AES decryption routines
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

    zip_uint64_t data_length;
    zip_uint64_t current_position;

    zip_winzip_aes_t *aes_ctx;
    zip_error_t error;
};


static int decrypt_header(zip_source_t *src, struct winzip_aes *ctx);
static void winzip_aes_free(struct winzip_aes *);
static zip_int64_t winzip_aes_decrypt(zip_source_t *src, void *ud, void *data, zip_uint64_t len, zip_source_cmd_t cmd);
static struct winzip_aes *winzip_aes_new(zip_uint16_t encryption_method, const char *password, zip_error_t *error);


zip_source_t *
zip_source_winzip_aes_decode(zip_t *za, zip_source_t *src, zip_uint16_t encryption_method, int flags, const char *password) {
    zip_source_t *s2;
    zip_stat_t st;
    zip_uint64_t aux_length;
    struct winzip_aes *ctx;

    if ((encryption_method != ZIP_EM_AES_128 && encryption_method != ZIP_EM_AES_192 && encryption_method != ZIP_EM_AES_256) || password == NULL || src == NULL) {
        zip_error_set(&za->error, ZIP_ER_INVAL, 0);
        return NULL;
    }
    if (flags & ZIP_CODEC_ENCODE) {
        zip_error_set(&za->error, ZIP_ER_ENCRNOTSUPP, 0);
        return NULL;
    }

    if (zip_source_stat(src, &st) != 0) {
        zip_error_set_from_source(&za->error, src);
        return NULL;
    }

    aux_length = WINZIP_AES_PASSWORD_VERIFY_LENGTH + SALT_LENGTH(encryption_method) + HMAC_LENGTH;

    if ((st.valid & ZIP_STAT_COMP_SIZE) == 0 || st.comp_size < aux_length) {
        zip_error_set(&za->error, ZIP_ER_OPNOTSUPP, 0);
        return NULL;
    }

    if ((ctx = winzip_aes_new(encryption_method, password, &za->error)) == NULL) {
        return NULL;
    }

    ctx->data_length = st.comp_size - aux_length;

    if ((s2 = zip_source_layered(za, src, winzip_aes_decrypt, ctx)) == NULL) {
        winzip_aes_free(ctx);
        return NULL;
    }

    return s2;
}


static int
decrypt_header(zip_source_t *src, struct winzip_aes *ctx) {
    zip_uint8_t header[WINZIP_AES_MAX_HEADER_LENGTH];
    zip_uint8_t password_verification[WINZIP_AES_PASSWORD_VERIFY_LENGTH];
    unsigned int headerlen;
    zip_int64_t n;

    headerlen = WINZIP_AES_PASSWORD_VERIFY_LENGTH + SALT_LENGTH(ctx->encryption_method);
    if ((n = zip_source_read(src, header, headerlen)) < 0) {
        zip_error_set_from_source(&ctx->error, src);
        return -1;
    }

    if (n != headerlen) {
        zip_error_set(&ctx->error, ZIP_ER_EOF, 0);
        return -1;
    }

    if ((ctx->aes_ctx = _zip_winzip_aes_new((zip_uint8_t *)ctx->password, strlen(ctx->password), header, ctx->encryption_method, password_verification, &ctx->error)) == NULL) {
        return -1;
    }
    if (memcmp(password_verification, header + SALT_LENGTH(ctx->encryption_method), WINZIP_AES_PASSWORD_VERIFY_LENGTH) != 0) {
        _zip_winzip_aes_free(ctx->aes_ctx);
        ctx->aes_ctx = NULL;
        zip_error_set(&ctx->error, ZIP_ER_WRONGPASSWD, 0);
        return -1;
    }
    return 0;
}


static bool
verify_hmac(zip_source_t *src, struct winzip_aes *ctx) {
    unsigned char computed[SHA1_LENGTH], from_file[HMAC_LENGTH];
    if (zip_source_read(src, from_file, HMAC_LENGTH) < HMAC_LENGTH) {
        zip_error_set_from_source(&ctx->error, src);
        return false;
    }

    if (!_zip_winzip_aes_finish(ctx->aes_ctx, computed)) {
        zip_error_set(&ctx->error, ZIP_ER_INTERNAL, 0);
        return false;
    }
    _zip_winzip_aes_free(ctx->aes_ctx);
    ctx->aes_ctx = NULL;

    if (memcmp(from_file, computed, HMAC_LENGTH) != 0) {
        zip_error_set(&ctx->error, ZIP_ER_CRC, 0);
        return false;
    }

    return true;
}


static zip_int64_t
winzip_aes_decrypt(zip_source_t *src, void *ud, void *data, zip_uint64_t len, zip_source_cmd_t cmd) {
    struct winzip_aes *ctx;
    zip_int64_t n;

    ctx = (struct winzip_aes *)ud;

    switch (cmd) {
    case ZIP_SOURCE_OPEN:
        if (decrypt_header(src, ctx) < 0) {
            return -1;
        }
        ctx->current_position = 0;
        return 0;

    case ZIP_SOURCE_READ:
        if (len > ctx->data_length - ctx->current_position) {
            len = ctx->data_length - ctx->current_position;
        }

        if (len == 0) {
            if (!verify_hmac(src, ctx)) {
                return -1;
            }
            return 0;
        }

        if ((n = zip_source_read(src, data, len)) < 0) {
            zip_error_set_from_source(&ctx->error, src);
            return -1;
        }
        ctx->current_position += (zip_uint64_t)n;

        if (!_zip_winzip_aes_decrypt(ctx->aes_ctx, (zip_uint8_t *)data, (zip_uint64_t)n)) {
            zip_error_set(&ctx->error, ZIP_ER_INTERNAL, 0);
            return -1;
        }

        return n;

    case ZIP_SOURCE_CLOSE:
        return 0;

    case ZIP_SOURCE_STAT: {
        zip_stat_t *st;

        st = (zip_stat_t *)data;

        st->encryption_method = ZIP_EM_NONE;
        st->valid |= ZIP_STAT_ENCRYPTION_METHOD;
        if (st->valid & ZIP_STAT_COMP_SIZE) {
            st->comp_size -= 12 + SALT_LENGTH(ctx->encryption_method);
        }

        return 0;
    }

    case ZIP_SOURCE_SUPPORTS:
        return zip_source_make_command_bitmap(ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_CLOSE, ZIP_SOURCE_STAT, ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, ZIP_SOURCE_SUPPORTS_REOPEN, -1);

    case ZIP_SOURCE_ERROR:
        return zip_error_to_data(&ctx->error, data, len);

    case ZIP_SOURCE_FREE:
        winzip_aes_free(ctx);
        return 0;

    default:
        return zip_source_pass_to_lower_layer(src, data, len, cmd);
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
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        free(ctx);
        return NULL;
    }

    ctx->encryption_method = encryption_method;
    ctx->aes_ctx = NULL;

    zip_error_init(&ctx->error);

    return ctx;
}
