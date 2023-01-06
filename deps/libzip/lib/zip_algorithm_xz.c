/*
  zip_algorithm_xz.c -- LZMA/XZ (de)compression routines
  Bazed on zip_algorithm_deflate.c -- deflate (de)compression routines
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
#include <lzma.h>
#include <stdlib.h>
#include <zlib.h>

enum header_state { INCOMPLETE, OUTPUT, DONE };

#define HEADER_BYTES_ZIP 9
#define HEADER_MAGIC_LENGTH 4
#define HEADER_MAGIC1_OFFSET 0
#define HEADER_MAGIC2_OFFSET 2
#define HEADER_SIZE_OFFSET 9
#define HEADER_SIZE_LENGTH 8
#define HEADER_PARAMETERS_LENGTH 5
#define HEADER_LZMA_ALONE_LENGTH (HEADER_PARAMETERS_LENGTH + HEADER_SIZE_LENGTH)

struct ctx {
    zip_error_t *error;
    bool compress;
    zip_uint32_t compression_flags;
    bool end_of_input;
    lzma_stream zstr;
    zip_uint16_t method;
    /* header member is used for converting from zip to "lzma alone"
     * format
     *
     * "lzma alone" file format starts with:
     * 5 bytes lzma parameters
     * 8 bytes uncompressed size
     * compressed data
     *
     * zip archive on-disk format starts with
     * 4 bytes magic (first two bytes vary, e.g. 0x0914 or 0x1002, next bytes are 0x0500)
     * 5 bytes lzma parameters
     * compressed data
     *
     * we read the data into a header of the form
     * 4 bytes magic
     * 5 bytes lzma parameters
     * 8 bytes uncompressed size
     */
    zip_uint8_t header[HEADER_MAGIC_LENGTH + HEADER_LZMA_ALONE_LENGTH];
    zip_uint8_t header_bytes_offset;
    enum header_state header_state;
    zip_uint64_t uncompresssed_size;
};


static zip_uint64_t
maximum_compressed_size(zip_uint64_t uncompressed_size) {
    /*
     According to https://sourceforge.net/p/sevenzip/discussion/45797/thread/b6bd62f8/

     1) you can use
     outSize = 1.10 * originalSize + 64 KB.
     in most cases outSize is less then 1.02 from originalSize.
     2) You can try LZMA2, where
     outSize can be = 1.001 * originalSize + 1 KB.
     */
    /* 13 bytes added for lzma alone header */
    zip_uint64_t compressed_size = (zip_uint64_t)((double)uncompressed_size * 1.1) + 64 * 1024 + 13;

    if (compressed_size < uncompressed_size) {
        return ZIP_UINT64_MAX;
    }
    return compressed_size;
}


static void *
allocate(bool compress, int compression_flags, zip_error_t *error, zip_uint16_t method) {
    struct ctx *ctx;

    if ((ctx = (struct ctx *)malloc(sizeof(*ctx))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    ctx->error = error;
    ctx->compress = compress;
    if (compression_flags < 0 || compression_flags > 9) {
        ctx->compression_flags = 6; /* default value */
    } else {
	ctx->compression_flags = (zip_uint32_t)compression_flags;
    }
    ctx->compression_flags |= LZMA_PRESET_EXTREME;
    ctx->end_of_input = false;
    memset(ctx->header, 0, sizeof(ctx->header));
    ctx->header_bytes_offset = 0;
    if (ZIP_CM_LZMA) {
        ctx->header_state = INCOMPLETE;
    }
    else {
        ctx->header_state = DONE;
    }
    memset(&ctx->zstr, 0, sizeof(ctx->zstr));
    ctx->method = method;
    return ctx;
}


static void *
compress_allocate(zip_uint16_t method, int compression_flags, zip_error_t *error) {
    return allocate(true, compression_flags, error, method);
}


static void *
decompress_allocate(zip_uint16_t method, int compression_flags, zip_error_t *error) {
    return allocate(false, compression_flags, error, method);
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

    if (ctx->method == ZIP_CM_LZMA) {
        /* liblzma always returns an EOS/EOPM marker, see
	 * https://sourceforge.net/p/lzmautils/discussion/708858/thread/84c5dbb9/#a5e4/3764 */
	return 1 << 1;
    }
    return 0;
}

static int
map_error(lzma_ret ret) {
    switch (ret) {
    case LZMA_DATA_ERROR:
    case LZMA_UNSUPPORTED_CHECK:
        return ZIP_ER_COMPRESSED_DATA;

    case LZMA_MEM_ERROR:
        return ZIP_ER_MEMORY;

    case LZMA_OPTIONS_ERROR:
        return ZIP_ER_INVAL;

    default:
        return ZIP_ER_INTERNAL;
    }
}


static bool
start(void *ud, zip_stat_t *st, zip_file_attributes_t *attributes) {
    struct ctx *ctx = (struct ctx *)ud;
    lzma_ret ret;

    lzma_options_lzma opt_lzma;
    lzma_lzma_preset(&opt_lzma, ctx->compression_flags);
    lzma_filter filters[] = {
        {.id = (ctx->method == ZIP_CM_LZMA ? LZMA_FILTER_LZMA1 : LZMA_FILTER_LZMA2), .options = &opt_lzma},
        {.id = LZMA_VLI_UNKNOWN, .options = NULL},
    };

    ctx->zstr.avail_in = 0;
    ctx->zstr.next_in = NULL;
    ctx->zstr.avail_out = 0;
    ctx->zstr.next_out = NULL;

    if (ctx->compress) {
        if (ctx->method == ZIP_CM_LZMA)
            ret = lzma_alone_encoder(&ctx->zstr, filters[0].options);
        else
            ret = lzma_stream_encoder(&ctx->zstr, filters, LZMA_CHECK_CRC64);
    }
    else {
        if (ctx->method == ZIP_CM_LZMA)
            ret = lzma_alone_decoder(&ctx->zstr, UINT64_MAX);
        else
            ret = lzma_stream_decoder(&ctx->zstr, UINT64_MAX, LZMA_CONCATENATED);
    }

    if (ret != LZMA_OK) {
        zip_error_set(ctx->error, map_error(ret), 0);
        return false;
    }

    /* If general purpose bits 1 & 2 are both zero, write real uncompressed size in header. */
    if ((attributes->valid & ZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS) && (attributes->general_purpose_bit_mask & 0x6) == 0x6 && (attributes->general_purpose_bit_flags & 0x06) == 0 && (st->valid & ZIP_STAT_SIZE)) {
        ctx->uncompresssed_size = st->size;
    }
    else {
        ctx->uncompresssed_size = ZIP_UINT64_MAX;
    }

    return true;
}


static bool
end(void *ud) {
    struct ctx *ctx = (struct ctx *)ud;

    lzma_end(&ctx->zstr);
    return true;
}


static bool
input(void *ud, zip_uint8_t *data, zip_uint64_t length) {
    struct ctx *ctx = (struct ctx *)ud;

    if (length > UINT_MAX || ctx->zstr.avail_in > 0) {
        zip_error_set(ctx->error, ZIP_ER_INVAL, 0);
        return false;
    }

    /* For decompression of LZMA1: Have we read the full "lzma alone" header yet? */
    if (ctx->method == ZIP_CM_LZMA && !ctx->compress && ctx->header_state == INCOMPLETE) {
        /* if not, get more of the data */
        zip_uint8_t got = (zip_uint8_t)ZIP_MIN(HEADER_BYTES_ZIP - ctx->header_bytes_offset, length);
        (void)memcpy_s(ctx->header + ctx->header_bytes_offset, sizeof(ctx->header) - ctx->header_bytes_offset, data, got);
        ctx->header_bytes_offset += got;
        length -= got;
        data += got;
        /* Do we have a complete header now? */
        if (ctx->header_bytes_offset == HEADER_BYTES_ZIP) {
            Bytef empty_buffer[1];
            zip_buffer_t *buffer;
            /* check magic */
            if (ctx->header[HEADER_MAGIC2_OFFSET] != 0x05 || ctx->header[HEADER_MAGIC2_OFFSET + 1] != 0x00) {
                /* magic does not match */
                zip_error_set(ctx->error, ZIP_ER_COMPRESSED_DATA, 0);
                return false;
            }
            /* set size of uncompressed data in "lzma alone" header to "unknown" */
            if ((buffer = _zip_buffer_new(ctx->header + HEADER_SIZE_OFFSET, HEADER_SIZE_LENGTH)) == NULL) {
                zip_error_set(ctx->error, ZIP_ER_MEMORY, 0);
                return false;
            }
            _zip_buffer_put_64(buffer, ctx->uncompresssed_size);
            _zip_buffer_free(buffer);
            /* Feed header into "lzma alone" decoder, for
             * initialization; this should not produce output. */
            ctx->zstr.next_in = (void *)(ctx->header + HEADER_MAGIC_LENGTH);
            ctx->zstr.avail_in = HEADER_LZMA_ALONE_LENGTH;
            ctx->zstr.total_in = 0;
            ctx->zstr.next_out = empty_buffer;
            ctx->zstr.avail_out = sizeof(*empty_buffer);
            ctx->zstr.total_out = 0;
            /* this just initializes the decoder and does not produce output, so it consumes the complete header */
            if (lzma_code(&ctx->zstr, LZMA_RUN) != LZMA_OK || ctx->zstr.total_out > 0) {
                zip_error_set(ctx->error, ZIP_ER_COMPRESSED_DATA, 0);
                return false;
            }
            ctx->header_state = DONE;
        }
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
    lzma_ret ret;
    /* for compression of LZMA1 */
    if (ctx->method == ZIP_CM_LZMA && ctx->compress) {
        if (ctx->header_state == INCOMPLETE) {
            /* write magic to output buffer */
            ctx->header[0] = 0x09;
            ctx->header[1] = 0x14;
            ctx->header[2] = 0x05;
            ctx->header[3] = 0x00;
            /* generate lzma parameters into output buffer */
            ctx->zstr.avail_out = HEADER_LZMA_ALONE_LENGTH;
            ctx->zstr.next_out = ctx->header + HEADER_MAGIC_LENGTH;
            ret = lzma_code(&ctx->zstr, LZMA_RUN);
            if (ret != LZMA_OK || ctx->zstr.avail_out != 0) {
                /* assume that the whole header will be provided with the first call to lzma_code */
                return ZIP_COMPRESSION_ERROR;
            }
            ctx->header_state = OUTPUT;
        }
        if (ctx->header_state == OUTPUT) {
            /* write header */
            zip_uint8_t write_len = (zip_uint8_t)ZIP_MIN(HEADER_BYTES_ZIP - ctx->header_bytes_offset, *length);
            (void)memcpy_s(data, *length, ctx->header + ctx->header_bytes_offset, write_len);
            ctx->header_bytes_offset += write_len;
            *length = write_len;
            if (ctx->header_bytes_offset == HEADER_BYTES_ZIP) {
                ctx->header_state = DONE;
            }
            return ZIP_COMPRESSION_OK;
        }
    }

    avail_out = (uInt)ZIP_MIN(UINT_MAX, *length);
    ctx->zstr.avail_out = avail_out;
    ctx->zstr.next_out = (Bytef *)data;

    ret = lzma_code(&ctx->zstr, ctx->end_of_input ? LZMA_FINISH : LZMA_RUN);
    *length = avail_out - ctx->zstr.avail_out;

    switch (ret) {
    case LZMA_OK:
        return ZIP_COMPRESSION_OK;

    case LZMA_STREAM_END:
        return ZIP_COMPRESSION_END;

    case LZMA_BUF_ERROR:
        if (ctx->zstr.avail_in == 0) {
            return ZIP_COMPRESSION_NEED_DATA;
        }

        /* fallthrough */
    default:
        zip_error_set(ctx->error, map_error(ret), 0);
        return ZIP_COMPRESSION_ERROR;
    }
}

/* Version Required should be set to 63 (6.3) because this compression
   method was only defined in appnote.txt version 6.3.8, but Winzip
   does not unpack it if the value is not 20. */

/* clang-format off */

zip_compression_algorithm_t zip_algorithm_xz_compress = {
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


zip_compression_algorithm_t zip_algorithm_xz_decompress = {
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
