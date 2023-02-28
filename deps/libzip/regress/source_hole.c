/*
 source_hole.c -- source for handling huge files that are mostly NULs
 Copyright (C) 2014-2021 Dieter Baron and Thomas Klausner

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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zip.h"

/* public API */

zip_source_t *source_hole_create(const char *, int flags, zip_error_t *);


#ifndef EFTYPE
#define EFTYPE EINVAL
#endif


#define MY_MIN(a, b) ((a) < (b) ? (a) : (b))

#define FRAGMENT_SIZE (8 * 1024)

#define MARK_BEGIN "NiH0"
#define MARK_DATA "NiH1"
#define MARK_NUL "NiH2"


typedef struct buffer {
    zip_uint64_t fragment_size;
    zip_uint8_t **fragment;
    zip_uint64_t nfragments;
    zip_uint64_t size;
    zip_uint64_t offset;
} buffer_t;

static void buffer_free(buffer_t *buffer);
static buffer_t *buffer_from_file(const char *fname, int flags, zip_error_t *error);
static buffer_t *buffer_new(void);
static zip_int64_t buffer_read(buffer_t *buffer, zip_uint8_t *data, zip_uint64_t length, zip_error_t *error);
static int buffer_read_file(buffer_t *buffer, FILE *f, zip_error_t *error);
static zip_int64_t buffer_seek(buffer_t *buffer, void *data, zip_uint64_t length, zip_error_t *error);
static int buffer_to_file(buffer_t *buffer, const char *fname, zip_error_t *error);
static zip_int64_t buffer_write(buffer_t *buffer, const zip_uint8_t *data, zip_uint64_t length, zip_error_t *error);
static zip_uint64_t get_u64(const zip_uint8_t *b);
static int only_nul(const zip_uint8_t *data, zip_uint64_t length);
static int write_nuls(zip_uint64_t n, FILE *f);
static int write_u64(zip_uint64_t u64, FILE *f);


typedef struct hole {
    zip_error_t error;
    char *fname;
    buffer_t *in;
    buffer_t *out;
} hole_t;

static hole_t *hole_new(const char *fname, int flags, zip_error_t *error);
static zip_int64_t source_hole_cb(void *ud, void *data, zip_uint64_t length, zip_source_cmd_t command);


zip_source_t *
source_hole_create(const char *fname, int flags, zip_error_t *error) {
    hole_t *ud = hole_new(fname, flags, error);

    if (ud == NULL) {
        return NULL;
    }
    return zip_source_function_create(source_hole_cb, ud, error);
}


static void
buffer_free(buffer_t *buffer) {
    zip_uint64_t i;

    if (buffer == NULL) {
        return;
    }

    if (buffer->fragment) {
        for (i = 0; i < buffer->nfragments; i++) {
            free(buffer->fragment[i]);
        }
        free(buffer->fragment);
    }
    free(buffer);
}


static buffer_t *
buffer_from_file(const char *fname, int flags, zip_error_t *error) {
    buffer_t *buffer;
    FILE *f;

    if ((buffer = buffer_new()) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    if ((flags & ZIP_TRUNCATE) == 0) {
        if ((f = fopen(fname, "rb")) == NULL) {
            if (!(errno == ENOENT && (flags & ZIP_CREATE))) {
                buffer_free(buffer);
                return NULL;
            }
        }
        else {
            if (buffer_read_file(buffer, f, error) < 0) {
                buffer_free(buffer);
                fclose(f);
                return NULL;
            }
            fclose(f);
        }
    }

    return buffer;
}


static buffer_t *
buffer_new(void) {
    buffer_t *buffer;

    if ((buffer = (buffer_t *)malloc(sizeof(*buffer))) == NULL) {
        return NULL;
    }

    buffer->fragment = NULL;
    buffer->nfragments = 0;
    buffer->fragment_size = FRAGMENT_SIZE;
    buffer->size = 0;
    buffer->offset = 0;

    return buffer;
}


static zip_int64_t
buffer_read(buffer_t *buffer, zip_uint8_t *data, zip_uint64_t length, zip_error_t *error) {
    zip_uint64_t n, i, fragment_offset;

    length = MY_MIN(length, buffer->size - buffer->offset);

    if (length == 0) {
        return 0;
    }
    if (length > ZIP_INT64_MAX) {
        return -1;
    }

    i = buffer->offset / buffer->fragment_size;
    fragment_offset = buffer->offset % buffer->fragment_size;
    n = 0;
    while (n < length) {
        zip_uint64_t left = MY_MIN(length - n, buffer->fragment_size - fragment_offset);

        if (buffer->fragment[i]) {
            memcpy(data + n, buffer->fragment[i] + fragment_offset, left);
        }
        else {
            memset(data + n, 0, left);
        }

        n += left;
        i++;
        fragment_offset = 0;
    }

    buffer->offset += n;
    return (zip_int64_t)n;
}


static int
buffer_read_file(buffer_t *buffer, FILE *f, zip_error_t *error) {
    zip_uint8_t b[20];
    zip_uint64_t i;

    if (fread(b, 20, 1, f) != 1) {
        zip_error_set(error, ZIP_ER_READ, errno);
        return -1;
    }

    if (memcmp(b, MARK_BEGIN, 4) != 0) {
        zip_error_set(error, ZIP_ER_READ, EFTYPE);
        return -1;
    }

    buffer->fragment_size = get_u64(b + 4);
    buffer->size = get_u64(b + 12);
    
    if (buffer->fragment_size == 0) {
        zip_error_set(error, ZIP_ER_INCONS, 0);
        return -1;
    }

    buffer->nfragments = buffer->size / buffer->fragment_size;
    if (buffer->size % buffer->fragment_size != 0) {
        buffer->nfragments += 1;
    }
    
    if ((buffer->nfragments > SIZE_MAX / sizeof(buffer->fragment[0])) || ((buffer->fragment = (zip_uint8_t **)malloc(sizeof(buffer->fragment[0]) * buffer->nfragments)) == NULL)) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return -1;
    }

    for (i = 0; i < buffer->nfragments; i++) {
        buffer->fragment[i] = NULL;
    }

    i = 0;
    while (i < buffer->nfragments) {
        if (fread(b, 4, 1, f) != 1) {
            zip_error_set(error, ZIP_ER_READ, errno);
            return -1;
        }

        if (memcmp(b, MARK_DATA, 4) == 0) {
            if (buffer->fragment_size > SIZE_MAX) {
                zip_error_set(error, ZIP_ER_MEMORY, 0);
                return -1;
            }
            if ((buffer->fragment[i] = (zip_uint8_t *)malloc(buffer->fragment_size)) == NULL) {
                zip_error_set(error, ZIP_ER_MEMORY, 0);
                return -1;
            }
            if (fread(buffer->fragment[i], buffer->fragment_size, 1, f) != 1) {
                zip_error_set(error, ZIP_ER_READ, errno);
                return -1;
            }
            i++;
        }
        else if (memcmp(b, MARK_NUL, 4) == 0) {
            if (fread(b, 8, 1, f) != 1) {
                zip_error_set(error, ZIP_ER_READ, errno);
                return -1;
            }
            i += get_u64(b);
        }
        else {
            zip_error_set(error, ZIP_ER_READ, EFTYPE);
            return -1;
        }
    }

    return 0;
}

static zip_int64_t
buffer_seek(buffer_t *buffer, void *data, zip_uint64_t length, zip_error_t *error) {
    zip_int64_t new_offset = zip_source_seek_compute_offset(buffer->offset, buffer->size, data, length, error);

    if (new_offset < 0) {
        return -1;
    }

    buffer->offset = (zip_uint64_t)new_offset;
    return 0;
}


static int
buffer_to_file(buffer_t *buffer, const char *fname, zip_error_t *error) {
    FILE *f = fopen(fname, "wb");
    zip_uint64_t i;
    zip_uint64_t nul_run;

    if (f == NULL) {
        zip_error_set(error, ZIP_ER_OPEN, errno);
        return -1;
    }

    fwrite(MARK_BEGIN, 4, 1, f);
    write_u64(buffer->fragment_size, f);
    write_u64(buffer->size, f);

    nul_run = 0;
    for (i = 0; i * buffer->fragment_size < buffer->size; i++) {
        if (buffer->fragment[i] == NULL || only_nul(buffer->fragment[i], buffer->fragment_size)) {
            nul_run++;
        }
        else {
            if (nul_run > 0) {
                write_nuls(nul_run, f);
                nul_run = 0;
            }
            fwrite(MARK_DATA, 4, 1, f);

            fwrite(buffer->fragment[i], 1, buffer->fragment_size, f);
        }
    }

    if (nul_run > 0) {
        write_nuls(nul_run, f);
    }

    if (fclose(f) != 0) {
        zip_error_set(error, ZIP_ER_WRITE, errno);
        return -1;
    }

    return 0;
}


static zip_int64_t
buffer_write(buffer_t *buffer, const zip_uint8_t *data, zip_uint64_t length, zip_error_t *error) {
    zip_uint8_t **fragment;
    if (buffer->offset + length > buffer->nfragments * buffer->fragment_size) {
        zip_uint64_t needed_fragments = (buffer->offset + length + buffer->fragment_size - 1) / buffer->fragment_size;
        zip_uint64_t new_capacity = buffer->nfragments;
        zip_uint64_t i;

        if (new_capacity == 0) {
            new_capacity = 4;
        }
        while (new_capacity < needed_fragments) {
            new_capacity *= 2;
        }

        fragment = realloc(buffer->fragment, new_capacity * sizeof(*fragment));

        if (fragment == NULL) {
            zip_error_set(error, ZIP_ER_MEMORY, 0);
            return -1;
        }

        for (i = buffer->nfragments; i < new_capacity; i++) {
            fragment[i] = NULL;
        }

        buffer->fragment = fragment;
        buffer->nfragments = new_capacity;
    }

    if (!only_nul(data, length)) {
        zip_uint64_t idx, n, fragment_offset;

        idx = buffer->offset / buffer->fragment_size;
        fragment_offset = buffer->offset % buffer->fragment_size;
        n = 0;

        while (n < length) {
            zip_uint64_t left = MY_MIN(length - n, buffer->fragment_size - fragment_offset);

            if (buffer->fragment[idx] == NULL) {
                if ((buffer->fragment[idx] = (zip_uint8_t *)malloc(buffer->fragment_size)) == NULL) {
                    zip_error_set(error, ZIP_ER_MEMORY, 0);
                    return -1;
                }
                memset(buffer->fragment[idx], 0, buffer->fragment_size);
            }
            memcpy(buffer->fragment[idx] + fragment_offset, data + n, left);

            n += left;
            idx++;
            fragment_offset = 0;
        }
    }

    buffer->offset += length;
    if (buffer->offset > buffer->size) {
        buffer->size = buffer->offset;
    }

    return (zip_int64_t)length;
}


static zip_uint64_t
get_u64(const zip_uint8_t *b) {
    zip_uint64_t i;

    i = (zip_uint64_t)b[0] << 56 | (zip_uint64_t)b[1] << 48 | (zip_uint64_t)b[2] << 40 | (zip_uint64_t)b[3] << 32 | (zip_uint64_t)b[4] << 24 | (zip_uint64_t)b[5] << 16 | (zip_uint64_t)b[6] << 8 | (zip_uint64_t)b[7];

    return i;
}


static int
only_nul(const zip_uint8_t *data, zip_uint64_t length) {
    zip_uint64_t i;

    for (i = 0; i < length; i++) {
        if (data[i] != '\0') {
            return 0;
        }
    }

    return 1;
}


static int
write_nuls(zip_uint64_t n, FILE *f) {
    if (fwrite(MARK_NUL, 4, 1, f) != 1) {
        return -1;
    }
    return write_u64(n, f);
}


static int
write_u64(zip_uint64_t u64, FILE *f) {
    zip_uint8_t b[8];

    b[0] = (zip_uint8_t)((u64 >> 56) & 0xff);
    b[1] = (zip_uint8_t)((u64 >> 48) & 0xff);
    b[2] = (zip_uint8_t)((u64 >> 40) & 0xff);
    b[3] = (zip_uint8_t)((u64 >> 32) & 0xff);
    b[4] = (zip_uint8_t)((u64 >> 24) & 0xff);
    b[5] = (zip_uint8_t)((u64 >> 16) & 0xff);
    b[6] = (zip_uint8_t)((u64 >> 8) & 0xff);
    b[7] = (zip_uint8_t)(u64 & 0xff);

    return fwrite(b, 8, 1, f) == 1 ? 0 : -1;
}


static void
hole_free(hole_t *hole) {
    if (hole == NULL) {
        return;
    }
    zip_error_fini(&hole->error);
    buffer_free(hole->in);
    buffer_free(hole->out);
    free(hole->fname);
    free(hole);
}


static hole_t *
hole_new(const char *fname, int flags, zip_error_t *error) {
    hole_t *ctx = (hole_t *)malloc(sizeof(*ctx));

    if (ctx == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    if ((ctx->fname = strdup(fname)) == NULL) {
        free(ctx);
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    if ((ctx->in = buffer_from_file(fname, flags, error)) == NULL) {
        free(ctx);
        return NULL;
    }

    zip_error_init(&ctx->error);
    ctx->out = NULL;

    return ctx;
}


static zip_int64_t
source_hole_cb(void *ud, void *data, zip_uint64_t length, zip_source_cmd_t command) {
    hole_t *ctx = (hole_t *)ud;

    switch (command) {
    case ZIP_SOURCE_BEGIN_WRITE:
        ctx->out = buffer_new();
        return 0;

    case ZIP_SOURCE_CLOSE:
        return 0;

    case ZIP_SOURCE_COMMIT_WRITE:
        if (buffer_to_file(ctx->out, ctx->fname, &ctx->error) < 0) {
            return -1;
        }
        buffer_free(ctx->in);
        ctx->in = ctx->out;
        ctx->out = NULL;
        return 0;

    case ZIP_SOURCE_ERROR:
        return zip_error_to_data(&ctx->error, data, length);

    case ZIP_SOURCE_FREE:
        hole_free(ctx);
        return 0;

    case ZIP_SOURCE_OPEN:
        ctx->in->offset = 0;
        return 0;

    case ZIP_SOURCE_READ:
        return buffer_read(ctx->in, data, length, &ctx->error);

    case ZIP_SOURCE_REMOVE:
        buffer_free(ctx->in);
        ctx->in = buffer_new();
        buffer_free(ctx->out);
        ctx->out = NULL;
        (void)remove(ctx->fname);
        return 0;

    case ZIP_SOURCE_ROLLBACK_WRITE:
        buffer_free(ctx->out);
        ctx->out = NULL;
        return 0;

    case ZIP_SOURCE_SEEK:
        return buffer_seek(ctx->in, data, length, &ctx->error);

    case ZIP_SOURCE_SEEK_WRITE:
        return buffer_seek(ctx->out, data, length, &ctx->error);

    case ZIP_SOURCE_STAT: {
        zip_stat_t *st = ZIP_SOURCE_GET_ARGS(zip_stat_t, data, length, &ctx->error);

        if (st == NULL) {
            return -1;
        }

        /* TODO: return ENOENT if fname doesn't exist */

        st->valid |= ZIP_STAT_SIZE;
        st->size = ctx->in->size;
        return 0;
    }

    case ZIP_SOURCE_TELL:
        return (zip_int64_t)ctx->in->offset;

    case ZIP_SOURCE_TELL_WRITE:
        return (zip_int64_t)ctx->out->offset;

    case ZIP_SOURCE_WRITE:
        return buffer_write(ctx->out, data, length, &ctx->error);

    case ZIP_SOURCE_SUPPORTS:
        return zip_source_make_command_bitmap(ZIP_SOURCE_BEGIN_WRITE, ZIP_SOURCE_COMMIT_WRITE, ZIP_SOURCE_CLOSE, ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_REMOVE, ZIP_SOURCE_ROLLBACK_WRITE, ZIP_SOURCE_SEEK, ZIP_SOURCE_SEEK_WRITE, ZIP_SOURCE_STAT, ZIP_SOURCE_TELL, ZIP_SOURCE_TELL_WRITE, ZIP_SOURCE_WRITE, -1);

    default:
        zip_error_set(&ctx->error, ZIP_ER_OPNOTSUPP, 0);
        return -1;
    }
}
