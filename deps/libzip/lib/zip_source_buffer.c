/*
  zip_source_buffer.c -- create zip data source from buffer
  Copyright (C) 1999-2022 Dieter Baron and Thomas Klausner

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

#ifndef WRITE_FRAGMENT_SIZE
#define WRITE_FRAGMENT_SIZE (64 * 1024)
#endif

struct buffer {
    zip_buffer_fragment_t *fragments; /* fragments */
    zip_uint64_t *fragment_offsets;   /* offset of each fragment from start of buffer, nfragments+1 entries */
    zip_uint64_t nfragments;          /* number of allocated fragments */
    zip_uint64_t fragments_capacity;  /* size of fragments (number of pointers) */

    zip_uint64_t first_owned_fragment; /* first fragment to free data from */

    zip_uint64_t shared_fragments; /* number of shared fragments */
    struct buffer *shared_buffer;  /* buffer fragments are shared with */

    zip_uint64_t size;             /* size of buffer */
    zip_uint64_t offset;           /* current offset in buffer */
    zip_uint64_t current_fragment; /* fragment current offset is in */
};

typedef struct buffer buffer_t;

struct read_data {
    zip_error_t error;
    time_t mtime;
    zip_file_attributes_t attributes;
    buffer_t *in;
    buffer_t *out;
};

#define buffer_capacity(buffer) ((buffer)->fragment_offsets[(buffer)->nfragments])
#define buffer_size(buffer) ((buffer)->size)

static buffer_t *buffer_clone(buffer_t *buffer, zip_uint64_t length, zip_error_t *error);
static zip_uint64_t buffer_find_fragment(const buffer_t *buffer, zip_uint64_t offset);
static void buffer_free(buffer_t *buffer);
static bool buffer_grow_fragments(buffer_t *buffer, zip_uint64_t capacity, zip_error_t *error);
static buffer_t *buffer_new(const zip_buffer_fragment_t *fragments, zip_uint64_t nfragments, int free_data, zip_error_t *error);
static zip_int64_t buffer_read(buffer_t *buffer, zip_uint8_t *data, zip_uint64_t length);
static int buffer_seek(buffer_t *buffer, void *data, zip_uint64_t len, zip_error_t *error);
static zip_int64_t buffer_write(buffer_t *buffer, const zip_uint8_t *data, zip_uint64_t length, zip_error_t *);

static zip_int64_t read_data(void *, void *, zip_uint64_t, zip_source_cmd_t);

zip_source_t *zip_source_buffer_with_attributes_create(const void *data, zip_uint64_t len, int freep, zip_file_attributes_t *attributes, zip_error_t *error);
zip_source_t *zip_source_buffer_fragment_with_attributes_create(const zip_buffer_fragment_t *fragments, zip_uint64_t nfragments, int freep, zip_file_attributes_t *attributes, zip_error_t *error);


ZIP_EXTERN zip_source_t *
zip_source_buffer(zip_t *za, const void *data, zip_uint64_t len, int freep) {
    if (za == NULL)
        return NULL;

    return zip_source_buffer_with_attributes_create(data, len, freep, NULL, &za->error);
}


ZIP_EXTERN zip_source_t *
zip_source_buffer_create(const void *data, zip_uint64_t len, int freep, zip_error_t *error) {
    return zip_source_buffer_with_attributes_create(data, len, freep, NULL, error);
}


zip_source_t *
zip_source_buffer_with_attributes_create(const void *data, zip_uint64_t len, int freep, zip_file_attributes_t *attributes, zip_error_t *error) {
    zip_buffer_fragment_t fragment;

    if (data == NULL) {
        if (len > 0) {
            zip_error_set(error, ZIP_ER_INVAL, 0);
            return NULL;
        }

        return zip_source_buffer_fragment_with_attributes_create(NULL, 0, freep, attributes, error);
    }

    fragment.data = (zip_uint8_t *)data;
    fragment.length = len;

    return zip_source_buffer_fragment_with_attributes_create(&fragment, 1, freep, attributes, error);
}


ZIP_EXTERN zip_source_t *
zip_source_buffer_fragment(zip_t *za, const zip_buffer_fragment_t *fragments, zip_uint64_t nfragments, int freep) {
    if (za == NULL) {
        return NULL;
    }

    return zip_source_buffer_fragment_with_attributes_create(fragments, nfragments, freep, NULL, &za->error);
}


ZIP_EXTERN zip_source_t *
zip_source_buffer_fragment_create(const zip_buffer_fragment_t *fragments, zip_uint64_t nfragments, int freep, zip_error_t *error) {
    return zip_source_buffer_fragment_with_attributes_create(fragments, nfragments, freep, NULL, error);
}

zip_source_t *
zip_source_buffer_fragment_with_attributes_create(const zip_buffer_fragment_t *fragments, zip_uint64_t nfragments, int freep, zip_file_attributes_t *attributes, zip_error_t *error) {
    struct read_data *ctx;
    zip_source_t *zs;
    buffer_t *buffer;

    if (fragments == NULL && nfragments > 0) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }

    if ((buffer = buffer_new(fragments, nfragments, freep, error)) == NULL) {
        return NULL;
    }

    if ((ctx = (struct read_data *)malloc(sizeof(*ctx))) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        buffer_free(buffer);
        return NULL;
    }

    ctx->in = buffer;
    ctx->out = NULL;
    ctx->mtime = time(NULL);
    if (attributes) {
        (void)memcpy_s(&ctx->attributes, sizeof(ctx->attributes), attributes, sizeof(ctx->attributes));
    }
    else {
        zip_file_attributes_init(&ctx->attributes);
    }
    zip_error_init(&ctx->error);

    if ((zs = zip_source_function_create(read_data, ctx, error)) == NULL) {
        buffer_free(ctx->in);
        free(ctx);
        return NULL;
    }

    return zs;
}


zip_source_t *
zip_source_buffer_with_attributes(zip_t *za, const void *data, zip_uint64_t len, int freep, zip_file_attributes_t *attributes) {
    return zip_source_buffer_with_attributes_create(data, len, freep, attributes, &za->error);
}

static zip_int64_t
read_data(void *state, void *data, zip_uint64_t len, zip_source_cmd_t cmd) {
    struct read_data *ctx = (struct read_data *)state;

    switch (cmd) {
    case ZIP_SOURCE_BEGIN_WRITE:
        if ((ctx->out = buffer_new(NULL, 0, 0, &ctx->error)) == NULL) {
            return -1;
        }
        ctx->out->offset = 0;
        ctx->out->current_fragment = 0;
        return 0;

    case ZIP_SOURCE_BEGIN_WRITE_CLONING:
        if ((ctx->out = buffer_clone(ctx->in, len, &ctx->error)) == NULL) {
            return -1;
        }
        ctx->out->offset = len;
        ctx->out->current_fragment = ctx->out->nfragments;
        return 0;

    case ZIP_SOURCE_CLOSE:
        return 0;

    case ZIP_SOURCE_COMMIT_WRITE:
        buffer_free(ctx->in);
        ctx->in = ctx->out;
        ctx->out = NULL;
        return 0;

    case ZIP_SOURCE_ERROR:
        return zip_error_to_data(&ctx->error, data, len);

    case ZIP_SOURCE_FREE:
        buffer_free(ctx->in);
        buffer_free(ctx->out);
        free(ctx);
        return 0;

    case ZIP_SOURCE_GET_FILE_ATTRIBUTES: {
        if (len < sizeof(ctx->attributes)) {
            zip_error_set(&ctx->error, ZIP_ER_INVAL, 0);
            return -1;
        }

        (void)memcpy_s(data, sizeof(ctx->attributes), &ctx->attributes, sizeof(ctx->attributes));

        return sizeof(ctx->attributes);
    }

    case ZIP_SOURCE_OPEN:
        ctx->in->offset = 0;
        ctx->in->current_fragment = 0;
        return 0;

    case ZIP_SOURCE_READ:
        if (len > ZIP_INT64_MAX) {
            zip_error_set(&ctx->error, ZIP_ER_INVAL, 0);
            return -1;
        }
        return buffer_read(ctx->in, data, len);

    case ZIP_SOURCE_REMOVE: {
        buffer_t *empty = buffer_new(NULL, 0, 0, &ctx->error);
        if (empty == NULL) {
            return -1;
        }

        buffer_free(ctx->in);
        ctx->in = empty;
        return 0;
    }

    case ZIP_SOURCE_ROLLBACK_WRITE:
        buffer_free(ctx->out);
        ctx->out = NULL;
        return 0;

    case ZIP_SOURCE_SEEK:
        return buffer_seek(ctx->in, data, len, &ctx->error);

    case ZIP_SOURCE_SEEK_WRITE:
        return buffer_seek(ctx->out, data, len, &ctx->error);

    case ZIP_SOURCE_STAT: {
        zip_stat_t *st;

        if (len < sizeof(*st)) {
            zip_error_set(&ctx->error, ZIP_ER_INVAL, 0);
            return -1;
        }

        st = (zip_stat_t *)data;

        zip_stat_init(st);
        st->mtime = ctx->mtime;
        st->size = ctx->in->size;
        st->comp_size = st->size;
        st->comp_method = ZIP_CM_STORE;
        st->encryption_method = ZIP_EM_NONE;
        st->valid = ZIP_STAT_MTIME | ZIP_STAT_SIZE | ZIP_STAT_COMP_SIZE | ZIP_STAT_COMP_METHOD | ZIP_STAT_ENCRYPTION_METHOD;

        return sizeof(*st);
    }

    case ZIP_SOURCE_SUPPORTS:
        return zip_source_make_command_bitmap(ZIP_SOURCE_GET_FILE_ATTRIBUTES, ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_CLOSE, ZIP_SOURCE_STAT, ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, ZIP_SOURCE_SEEK, ZIP_SOURCE_TELL, ZIP_SOURCE_BEGIN_WRITE, ZIP_SOURCE_BEGIN_WRITE_CLONING, ZIP_SOURCE_COMMIT_WRITE, ZIP_SOURCE_REMOVE, ZIP_SOURCE_ROLLBACK_WRITE, ZIP_SOURCE_SEEK_WRITE, ZIP_SOURCE_TELL_WRITE, ZIP_SOURCE_WRITE, ZIP_SOURCE_SUPPORTS_REOPEN, -1);

    case ZIP_SOURCE_TELL:
        if (ctx->in->offset > ZIP_INT64_MAX) {
            zip_error_set(&ctx->error, ZIP_ER_TELL, EOVERFLOW);
            return -1;
        }
        return (zip_int64_t)ctx->in->offset;


    case ZIP_SOURCE_TELL_WRITE:
        if (ctx->out->offset > ZIP_INT64_MAX) {
            zip_error_set(&ctx->error, ZIP_ER_TELL, EOVERFLOW);
            return -1;
        }
        return (zip_int64_t)ctx->out->offset;

    case ZIP_SOURCE_WRITE:
        if (len > ZIP_INT64_MAX) {
            zip_error_set(&ctx->error, ZIP_ER_INVAL, 0);
            return -1;
        }
        return buffer_write(ctx->out, data, len, &ctx->error);

    default:
        zip_error_set(&ctx->error, ZIP_ER_OPNOTSUPP, 0);
        return -1;
    }
}


static buffer_t *
buffer_clone(buffer_t *buffer, zip_uint64_t offset, zip_error_t *error) {
    zip_uint64_t fragment, fragment_offset, waste;
    buffer_t *clone;

    if (offset == 0) {
        return buffer_new(NULL, 0, 1, error);
    }

    if (offset > buffer->size) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return NULL;
    }
    if (buffer->shared_buffer != NULL) {
        zip_error_set(error, ZIP_ER_INUSE, 0);
        return NULL;
    }

    fragment = buffer_find_fragment(buffer, offset);
    fragment_offset = offset - buffer->fragment_offsets[fragment];

    if (fragment_offset == 0) {
        fragment--;
        fragment_offset = buffer->fragments[fragment].length;
    }

    /* TODO: This should also consider the length of the fully shared fragments */
    waste = buffer->fragments[fragment].length - fragment_offset;
    if (waste > offset) {
        zip_error_set(error, ZIP_ER_OPNOTSUPP, 0);
        return NULL;
    }

    if ((clone = buffer_new(buffer->fragments, fragment + 1, 0, error)) == NULL) {
        return NULL;
    }

#ifndef __clang_analyzer__
    /* clone->fragments can't be null, since it was created with at least one fragment */
    clone->fragments[fragment].length = fragment_offset;
#endif
    clone->fragment_offsets[clone->nfragments] = offset;
    clone->size = offset;

    clone->first_owned_fragment = ZIP_MIN(buffer->first_owned_fragment, clone->nfragments);

    buffer->shared_buffer = clone;
    clone->shared_buffer = buffer;
    buffer->shared_fragments = fragment + 1;
    clone->shared_fragments = fragment + 1;
    
    return clone;
}


static zip_uint64_t
buffer_find_fragment(const buffer_t *buffer, zip_uint64_t offset) {
    zip_uint64_t low, high, mid;

    if (buffer->nfragments == 0) {
        return 0;
    }

    low = 0;
    high = buffer->nfragments - 1;

    while (low < high) {
        mid = (high - low) / 2 + low;
        if (buffer->fragment_offsets[mid] > offset) {
            high = mid - 1;
        }
        else if (mid == buffer->nfragments || buffer->fragment_offsets[mid + 1] > offset) {
            return mid;
        }
        else {
            low = mid + 1;
        }
    }

    return low;
}


static void
buffer_free(buffer_t *buffer) {
    zip_uint64_t i;

    if (buffer == NULL) {
        return;
    }

    if (buffer->shared_buffer != NULL) {
        buffer->shared_buffer->shared_buffer = NULL;
        buffer->shared_buffer->shared_fragments = 0;

        buffer->first_owned_fragment = ZIP_MAX(buffer->first_owned_fragment, buffer->shared_fragments);
    }

    for (i = buffer->first_owned_fragment; i < buffer->nfragments; i++) {
        free(buffer->fragments[i].data);
    }
    free(buffer->fragments);
    free(buffer->fragment_offsets);
    free(buffer);
}


static bool
buffer_grow_fragments(buffer_t *buffer, zip_uint64_t capacity, zip_error_t *error) {
    zip_buffer_fragment_t *fragments;
    zip_uint64_t *offsets;

    if (capacity < buffer->fragments_capacity) {
        return true;
    }

    zip_uint64_t fragments_size = sizeof(buffer->fragments[0]) * capacity;
    zip_uint64_t offsets_size = sizeof(buffer->fragment_offsets[0]) * (capacity + 1);

    if (capacity == ZIP_UINT64_MAX || fragments_size < capacity || fragments_size > SIZE_MAX|| offsets_size < capacity || offsets_size > SIZE_MAX) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return false;
    }

    if ((fragments = realloc(buffer->fragments, (size_t)fragments_size)) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return false;
    }
    buffer->fragments = fragments;
    if ((offsets = realloc(buffer->fragment_offsets, (size_t)offsets_size)) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return false;
    }
    buffer->fragment_offsets = offsets;
    buffer->fragments_capacity = capacity;

    return true;
}


static buffer_t *
buffer_new(const zip_buffer_fragment_t *fragments, zip_uint64_t nfragments, int free_data, zip_error_t *error) {
    buffer_t *buffer;

    if ((buffer = malloc(sizeof(*buffer))) == NULL) {
        return NULL;
    }

    buffer->offset = 0;
    buffer->first_owned_fragment = 0;
    buffer->size = 0;
    buffer->fragments = NULL;
    buffer->fragment_offsets = NULL;
    buffer->nfragments = 0;
    buffer->fragments_capacity = 0;
    buffer->shared_buffer = NULL;
    buffer->shared_fragments = 0;

    if (nfragments == 0) {
        if ((buffer->fragment_offsets = malloc(sizeof(buffer->fragment_offsets[0]))) == NULL) {
            free(buffer);
            zip_error_set(error, ZIP_ER_MEMORY, 0);
            return NULL;
        }
        buffer->fragment_offsets[0] = 0;
    }
    else {
        zip_uint64_t i, j, offset;

        if (!buffer_grow_fragments(buffer, nfragments, NULL)) {
            zip_error_set(error, ZIP_ER_MEMORY, 0);
            buffer_free(buffer);
            return NULL;
        }

        offset = 0;
        for (i = 0, j = 0; i < nfragments; i++) {
            if (fragments[i].length == 0) {
                continue;
            }
            if (fragments[i].data == NULL) {
                zip_error_set(error, ZIP_ER_INVAL, 0);
                buffer_free(buffer);
                return NULL;
            }
            buffer->fragments[j].data = fragments[i].data;
            buffer->fragments[j].length = fragments[i].length;
            buffer->fragment_offsets[i] = offset;
            // TODO: overflow
            offset += fragments[i].length;
            j++;
        }
        buffer->nfragments = j;
        buffer->first_owned_fragment = free_data ? 0 : buffer->nfragments;
        buffer->fragment_offsets[buffer->nfragments] = offset;
        buffer->size = offset;
    }

    return buffer;
}

static zip_int64_t
buffer_read(buffer_t *buffer, zip_uint8_t *data, zip_uint64_t length) {
    zip_uint64_t n, i, fragment_offset;

    length = ZIP_MIN(length, buffer->size - buffer->offset);

    if (length == 0) {
        return 0;
    }
    if (length > ZIP_INT64_MAX) {
        return -1;
    }

    i = buffer->current_fragment;
    fragment_offset = buffer->offset - buffer->fragment_offsets[i];
    n = 0;
    while (n < length) {
        zip_uint64_t left = ZIP_MIN(length - n, buffer->fragments[i].length - fragment_offset);
#if ZIP_UINT64_MAX > SIZE_MAX
        left = ZIP_MIN(left, SIZE_MAX);
#endif

        (void)memcpy_s(data + n, (size_t)left, buffer->fragments[i].data + fragment_offset, (size_t)left);

        if (left == buffer->fragments[i].length - fragment_offset) {
            i++;
        }
        n += left;
        fragment_offset = 0;
    }

    buffer->offset += n;
    buffer->current_fragment = i;
    return (zip_int64_t)n;
}


static int
buffer_seek(buffer_t *buffer, void *data, zip_uint64_t len, zip_error_t *error) {
    zip_int64_t new_offset = zip_source_seek_compute_offset(buffer->offset, buffer->size, data, len, error);

    if (new_offset < 0) {
        return -1;
    }

    buffer->offset = (zip_uint64_t)new_offset;
    buffer->current_fragment = buffer_find_fragment(buffer, buffer->offset);
    return 0;
}


static zip_int64_t
buffer_write(buffer_t *buffer, const zip_uint8_t *data, zip_uint64_t length, zip_error_t *error) {
    zip_uint64_t copied, i, fragment_offset, capacity;

    if (buffer->offset + length + WRITE_FRAGMENT_SIZE - 1 < length) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return -1;
    }

    /* grow buffer if needed */
    capacity = buffer_capacity(buffer);
    if (buffer->offset + length > capacity) {
        zip_uint64_t needed_fragments = buffer->nfragments + (length - (capacity - buffer->offset) + WRITE_FRAGMENT_SIZE - 1) / WRITE_FRAGMENT_SIZE;

        if (needed_fragments > buffer->fragments_capacity) {
            zip_uint64_t new_capacity = buffer->fragments_capacity;

            if (new_capacity == 0) {
                new_capacity = 16;
            }
            while (new_capacity < needed_fragments) {
                new_capacity *= 2;
            }

            if (!buffer_grow_fragments(buffer, new_capacity, error)) {
                zip_error_set(error, ZIP_ER_MEMORY, 0);
                return -1;
            }
        }

        while (buffer->nfragments < needed_fragments) {
            if ((buffer->fragments[buffer->nfragments].data = malloc(WRITE_FRAGMENT_SIZE)) == NULL) {
                zip_error_set(error, ZIP_ER_MEMORY, 0);
                return -1;
            }
            buffer->fragments[buffer->nfragments].length = WRITE_FRAGMENT_SIZE;
            buffer->nfragments++;
            capacity += WRITE_FRAGMENT_SIZE;
            buffer->fragment_offsets[buffer->nfragments] = capacity;
        }
    }

    i = buffer->current_fragment;
    fragment_offset = buffer->offset - buffer->fragment_offsets[i];
    copied = 0;
    while (copied < length) {
        zip_uint64_t n = ZIP_MIN(ZIP_MIN(length - copied, buffer->fragments[i].length - fragment_offset), SIZE_MAX);
#if ZIP_UINT64_MAX > SIZE_MAX
        n = ZIP_MIN(n, SIZE_MAX)
#endif

        (void)memcpy_s(buffer->fragments[i].data + fragment_offset, (size_t)n, data + copied, (size_t)n);

        if (n == buffer->fragments[i].length - fragment_offset) {
            i++;
            fragment_offset = 0;
        }
        else {
            fragment_offset += n;
        }
        copied += n;
    }

    buffer->offset += copied;
    buffer->current_fragment = i;
    if (buffer->offset > buffer->size) {
        buffer->size = buffer->offset;
    }

    return (zip_int64_t)copied;
}
