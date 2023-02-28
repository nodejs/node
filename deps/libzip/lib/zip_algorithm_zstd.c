/*
  zip_algorithm_zstd.c -- zstd (de)compression routines
  Copyright (C) 2020-2021 Dieter Baron and Thomas Klausner

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
#include <zstd.h>
#include <zstd_errors.h>

struct ctx {
    zip_error_t *error;
    bool compress;
    int compression_flags;
    bool end_of_input;
    ZSTD_DStream *zdstream;
    ZSTD_CStream *zcstream;
    ZSTD_outBuffer out;
    ZSTD_inBuffer in;
};

static zip_uint64_t
maximum_compressed_size(zip_uint64_t uncompressed_size) {
    return ZSTD_compressBound(uncompressed_size);
}


static void *
allocate(bool compress, int compression_flags, zip_error_t *error) {
    struct ctx *ctx;

    /* 0: let zstd choose */
    if (compression_flags < ZSTD_minCLevel() || compression_flags > ZSTD_maxCLevel()) {
        compression_flags = 0;
    }

    if ((ctx = (struct ctx *)malloc(sizeof(*ctx))) == NULL) {
        return NULL;
    }

    ctx->error = error;
    ctx->compress = compress;
    ctx->compression_flags = compression_flags;
    ctx->end_of_input = false;

    ctx->zdstream = NULL;
    ctx->zcstream = NULL;
    ctx->in.src = NULL;
    ctx->in.pos = 0;
    ctx->in.size = 0;
    ctx->out.dst = NULL;
    ctx->out.pos = 0;
    ctx->out.size = 0;

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
    /* struct ctx *ctx = (struct ctx *)ud; */
    return 0;
}

static int
map_error(size_t ret) {
    switch (ret) {
    case ZSTD_error_no_error:
        return ZIP_ER_OK;

    case ZSTD_error_corruption_detected:
    case ZSTD_error_checksum_wrong:
    case ZSTD_error_dictionary_corrupted:
    case ZSTD_error_dictionary_wrong:
        return ZIP_ER_COMPRESSED_DATA;

    case ZSTD_error_memory_allocation:
        return ZIP_ER_MEMORY;

    case ZSTD_error_parameter_unsupported:
    case ZSTD_error_parameter_outOfBound:
        return ZIP_ER_INVAL;

    default:
        return ZIP_ER_INTERNAL;
    }
}


static bool
start(void *ud, zip_stat_t *st, zip_file_attributes_t *attributes) {
    struct ctx *ctx = (struct ctx *)ud;
    ctx->in.src = NULL;
    ctx->in.pos = 0;
    ctx->in.size = 0;
    ctx->out.dst = NULL;
    ctx->out.pos = 0;
    ctx->out.size = 0;
    if (ctx->compress) {
        size_t ret;
        ctx->zcstream = ZSTD_createCStream();
        if (ctx->zcstream == NULL) {
            zip_error_set(ctx->error, ZIP_ER_MEMORY, 0);
            return false;
        }
        ret = ZSTD_initCStream(ctx->zcstream, ctx->compression_flags);
        if (ZSTD_isError(ret)) {
            zip_error_set(ctx->error, ZIP_ER_ZLIB, map_error(ret));
            return false;
        }
    }
    else {
        ctx->zdstream = ZSTD_createDStream();
        if (ctx->zdstream == NULL) {
            zip_error_set(ctx->error, ZIP_ER_MEMORY, 0);
            return false;
        }
    }

    return true;
}


static bool
end(void *ud) {
    struct ctx *ctx = (struct ctx *)ud;
    size_t ret;

    if (ctx->compress) {
        ret = ZSTD_freeCStream(ctx->zcstream);
        ctx->zcstream = NULL;
    }
    else {
        ret = ZSTD_freeDStream(ctx->zdstream);
        ctx->zdstream = NULL;
    }

    if (ZSTD_isError(ret)) {
        zip_error_set(ctx->error, map_error(ret), 0);
        return false;
    }

    return true;
}


static bool
input(void *ud, zip_uint8_t *data, zip_uint64_t length) {
    struct ctx *ctx = (struct ctx *)ud;
    if (length > SIZE_MAX || ctx->in.pos != ctx->in.size) {
        zip_error_set(ctx->error, ZIP_ER_INVAL, 0);
        return false;
    }
    ctx->in.src = (const void *)data;
    ctx->in.size = (size_t)length;
    ctx->in.pos = 0;
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

    size_t ret;

    if (ctx->in.pos == ctx->in.size && !ctx->end_of_input) {
        *length = 0;
        return ZIP_COMPRESSION_NEED_DATA;
    }

    ctx->out.dst = data;
    ctx->out.pos = 0;
    ctx->out.size = ZIP_MIN(SIZE_MAX, *length);

    if (ctx->compress) {
        if (ctx->in.pos == ctx->in.size && ctx->end_of_input) {
            ret = ZSTD_endStream(ctx->zcstream, &ctx->out);
            if (ret == 0) {
                *length = ctx->out.pos;
                return ZIP_COMPRESSION_END;
            }
        }
        else {
            ret = ZSTD_compressStream(ctx->zcstream, &ctx->out, &ctx->in);
        }
    }
    else {
        ret = ZSTD_decompressStream(ctx->zdstream, &ctx->out, &ctx->in);
    }
    if (ZSTD_isError(ret)) {
        zip_error_set(ctx->error, map_error(ret), 0);
        return ZIP_COMPRESSION_ERROR;
    }

    *length = ctx->out.pos;
    if (ctx->in.pos == ctx->in.size) {
        return ZIP_COMPRESSION_NEED_DATA;
    }

    return ZIP_COMPRESSION_OK;
}

/* Version Required should be set to 63 (6.3) because this compression
   method was only defined in appnote.txt version 6.3.7, but Winzip
   does not unpack it if the value is not 20. */

/* clang-format off */

zip_compression_algorithm_t zip_algorithm_zstd_compress = {
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


zip_compression_algorithm_t zip_algorithm_zstd_decompress = {
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
