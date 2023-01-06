/*
  zip_algorithm_bzip2.c -- bzip2 (de)compression routines
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

#include <bzlib.h>
#include <limits.h>
#include <stdlib.h>

struct ctx {
    zip_error_t *error;
    bool compress;
    int compression_flags;
    bool end_of_input;
    bz_stream zstr;
};


static zip_uint64_t
maximum_compressed_size(zip_uint64_t uncompressed_size) {
    zip_uint64_t compressed_size = (zip_uint64_t)((double)uncompressed_size * 1.006);

    if (compressed_size < uncompressed_size) {
        return ZIP_UINT64_MAX;
    }
    return compressed_size;
}


static void *
allocate(bool compress, int compression_flags, zip_error_t *error) {
    struct ctx *ctx;

    if ((ctx = (struct ctx *)malloc(sizeof(*ctx))) == NULL) {
        return NULL;
    }

    ctx->error = error;
    ctx->compress = compress;
    ctx->compression_flags = compression_flags;
    if (ctx->compression_flags < 1 || ctx->compression_flags > 9) {
        ctx->compression_flags = 9;
    }
    ctx->end_of_input = false;

    ctx->zstr.bzalloc = NULL;
    ctx->zstr.bzfree = NULL;
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
    return 0;
}


static int
map_error(int ret) {
    switch (ret) {
    case BZ_FINISH_OK:
    case BZ_FLUSH_OK:
    case BZ_OK:
    case BZ_RUN_OK:
    case BZ_STREAM_END:
        return ZIP_ER_OK;

    case BZ_DATA_ERROR:
    case BZ_DATA_ERROR_MAGIC:
    case BZ_UNEXPECTED_EOF:
        return ZIP_ER_COMPRESSED_DATA;

    case BZ_MEM_ERROR:
        return ZIP_ER_MEMORY;

    case BZ_PARAM_ERROR:
        return ZIP_ER_INVAL;

    case BZ_CONFIG_ERROR: /* actually, bzip2 miscompiled */
    case BZ_IO_ERROR:
    case BZ_OUTBUFF_FULL:
    case BZ_SEQUENCE_ERROR:
        return ZIP_ER_INTERNAL;

    default:
        return ZIP_ER_INTERNAL;
    }
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
        ret = BZ2_bzCompressInit(&ctx->zstr, ctx->compression_flags, 0, 30);
    }
    else {
        ret = BZ2_bzDecompressInit(&ctx->zstr, 0, 0);
    }

    if (ret != BZ_OK) {
        zip_error_set(ctx->error, map_error(ret), 0);
        return false;
    }

    return true;
}


static bool
end(void *ud) {
    struct ctx *ctx = (struct ctx *)ud;
    int err;

    if (ctx->compress) {
        err = BZ2_bzCompressEnd(&ctx->zstr);
    }
    else {
        err = BZ2_bzDecompressEnd(&ctx->zstr);
    }

    if (err != BZ_OK) {
        zip_error_set(ctx->error, map_error(err), 0);
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

    ctx->zstr.avail_in = (unsigned int)length;
    ctx->zstr.next_in = (char *)data;

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
    unsigned int avail_out;

    int ret;

    if (ctx->zstr.avail_in == 0 && !ctx->end_of_input) {
        *length = 0;
        return ZIP_COMPRESSION_NEED_DATA;
    }

    avail_out = (unsigned int)ZIP_MIN(UINT_MAX, *length);
    ctx->zstr.avail_out = avail_out;
    ctx->zstr.next_out = (char *)data;

    if (ctx->compress) {
        ret = BZ2_bzCompress(&ctx->zstr, ctx->end_of_input ? BZ_FINISH : BZ_RUN);
    }
    else {
        ret = BZ2_bzDecompress(&ctx->zstr);
    }

    *length = avail_out - ctx->zstr.avail_out;

    switch (ret) {
    case BZ_FINISH_OK: /* compression */
        return ZIP_COMPRESSION_OK;

    case BZ_OK:     /* decompression */
    case BZ_RUN_OK: /* compression */
        if (ctx->zstr.avail_in == 0) {
            return ZIP_COMPRESSION_NEED_DATA;
        }
        return ZIP_COMPRESSION_OK;

    case BZ_STREAM_END:
        return ZIP_COMPRESSION_END;

    default:
        zip_error_set(ctx->error, map_error(ret), 0);
        return ZIP_COMPRESSION_ERROR;
    }
}

/* clang-format off */

zip_compression_algorithm_t zip_algorithm_bzip2_compress = {
    maximum_compressed_size,
    compress_allocate,
    deallocate,
    general_purpose_bit_flags,
    46,
    start,
    end,
    input,
    end_of_input,
    process
};


zip_compression_algorithm_t zip_algorithm_bzip2_decompress = {
    maximum_compressed_size,
    decompress_allocate,
    deallocate,
    general_purpose_bit_flags,
    46,
    start,
    end,
    input,
    end_of_input,
    process
};

/* clang-format on */
