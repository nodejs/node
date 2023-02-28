/*
  zip_algorithm_deflate.c -- deflate (de)compression routines
  Copyright (C) 2017-2021 Dieter Baron and Thomas Klausner

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

#include "zipint.h"

#include <limits.h>
#include <stdlib.h>
#include <zlib.h>

struct ctx {
    zip_error_t *error;
    bool compress;
    int compression_flags;
    bool end_of_input;
    z_stream zstr;
};


static zip_uint64_t
maximum_compressed_size(zip_uint64_t uncompressed_size) {
    /* max deflate size increase: size + ceil(size/16k)*5+6 */

    zip_uint64_t compressed_size = uncompressed_size + (uncompressed_size + 16383) / 16384 * 5 + 6;

    if (compressed_size < uncompressed_size) {
        return ZIP_UINT64_MAX;
    }
    return compressed_size;
}


static void *
allocate(bool compress, int compression_flags, zip_error_t *error) {
    struct ctx *ctx;

    if ((ctx = (struct ctx *)malloc(sizeof(*ctx))) == NULL) {
        zip_error_set(error, ZIP_ET_SYS, errno);
        return NULL;
    }

    ctx->error = error;
    ctx->compress = compress;
    ctx->compression_flags = compression_flags;
    if (ctx->compression_flags < 1 || ctx->compression_flags > 9) {
        ctx->compression_flags = Z_BEST_COMPRESSION;
    }
    ctx->end_of_input = false;

    ctx->zstr.zalloc = Z_NULL;
    ctx->zstr.zfree = Z_NULL;
    ctx->zstr.opaque = NULL;

    return ctx;
}


static void *
compress_allocate(zip_uint16_t method, int compression_flags, zip_error_t *error) {
    return allocate(true, compression_flags, error);
}


static void *
decompress_allocate(zip_uint16_t method, int compression_flags, zip_error_t *error) {
    return allocate(false, compression_flags, error);
}


static void
deallocate(void *ud) {
    struct ctx *ctx = (struct ctx *)ud;

    free(ctx);
}


static zip_uint16_t
general_purpose_bit_flags(void *ud) {
    struct ctx *ctx = (struct ctx *)ud;

    if (!ctx->compress) {
        return 0;
    }

    if (ctx->compression_flags < 3) {
        return 2 << 1;
    }
    else if (ctx->compression_flags > 7) {
        return 1 << 1;
    }
    return 0;
}


static bool
start(void *ud, zip_stat_t *st, zip_file_attributes_t *attributes) {
    struct ctx *ctx = (struct ctx *)ud;
    int ret;

    ctx->zstr.avail_in = 0;
    ctx->zstr.next_in = NULL;
    ctx->zstr.avail_out = 0;
    ctx->zstr.next_out = NULL;

    if (ctx->compress) {
        /* negative value to tell zlib not to write a header */
        ret = deflateInit2(&ctx->zstr, ctx->compression_flags, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    }
    else {
        ret = inflateInit2(&ctx->zstr, -MAX_WBITS);
    }

    if (ret != Z_OK) {
        zip_error_set(ctx->error, ZIP_ER_ZLIB, ret);
        return false;
    }


    return true;
}


static bool
end(void *ud) {
    struct ctx *ctx = (struct ctx *)ud;
    int err;

    if (ctx->compress) {
        err = deflateEnd(&ctx->zstr);
    }
    else {
        err = inflateEnd(&ctx->zstr);
    }

    if (err != Z_OK) {
        zip_error_set(ctx->error, ZIP_ER_ZLIB, err);
        return false;
    }

    return true;
}


static bool
input(void *ud, zip_uint8_t *data, zip_uint64_t length) {
    struct ctx *ctx = (struct ctx *)ud;

    if (length > UINT_MAX || ctx->zstr.avail_in > 0) {
        zip_error_set(ctx->error, ZIP_ER_INVAL, 0);
        return false;
    }

    ctx->zstr.avail_in = (uInt)length;
    ctx->zstr.next_in = (Bytef *)data;

    return true;
}


static void
end_of_input(void *ud) {
    struct ctx *ctx = (struct ctx *)ud;

    ctx->end_of_input = true;
}


static zip_compression_status_t
process(void *ud, zip_uint8_t *data, zip_uint64_t *length) {
    struct ctx *ctx = (struct ctx *)ud;
    uInt avail_out;

    int ret;

    avail_out = (uInt)ZIP_MIN(UINT_MAX, *length);
    ctx->zstr.avail_out = avail_out;
    ctx->zstr.next_out = (Bytef *)data;

    if (ctx->compress) {
        ret = deflate(&ctx->zstr, ctx->end_of_input ? Z_FINISH : 0);
    }
    else {
        ret = inflate(&ctx->zstr, Z_SYNC_FLUSH);
    }

    *length = avail_out - ctx->zstr.avail_out;

    switch (ret) {
    case Z_OK:
        return ZIP_COMPRESSION_OK;

    case Z_STREAM_END:
        return ZIP_COMPRESSION_END;

    case Z_BUF_ERROR:
        if (ctx->zstr.avail_in == 0) {
            return ZIP_COMPRESSION_NEED_DATA;
        }

        /* fallthrough */

    default:
        zip_error_set(ctx->error, ZIP_ER_ZLIB, ret);
        return ZIP_COMPRESSION_ERROR;
    }
}

/* clang-format off */

zip_compression_algorithm_t zip_algorithm_deflate_compress = {
    maximum_compressed_size,
    compress_allocate,
    deallocate,
    general_purpose_bit_flags,
    20,
    start,
    end,
    input,
    end_of_input,
    process
};


zip_compression_algorithm_t zip_algorithm_deflate_decompress = {
    maximum_compressed_size,
    decompress_allocate,
    deallocate,
    general_purpose_bit_flags,
    20,
    start,
    end,
    input,
    end_of_input,
    process
};

/* clang-format on */
