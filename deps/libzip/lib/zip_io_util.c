/*
 zip_io_util.c -- I/O helper functions
 Copyright (C) 1999-2021 Dieter Baron and Thomas Klausner

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

int
_zip_read(zip_source_t *src, zip_uint8_t *b, zip_uint64_t length, zip_error_t *error) {
    zip_int64_t n;

    if (length > ZIP_INT64_MAX) {
        zip_error_set(error, ZIP_ER_INTERNAL, 0);
        return -1;
    }

    if ((n = zip_source_read(src, b, length)) < 0) {
        zip_error_set_from_source(error, src);
        return -1;
    }

    if (n < (zip_int64_t)length) {
        zip_error_set(error, ZIP_ER_EOF, 0);
        return -1;
    }

    return 0;
}


zip_uint8_t *
_zip_read_data(zip_buffer_t *buffer, zip_source_t *src, size_t length, bool nulp, zip_error_t *error) {
    zip_uint8_t *r;

    if (length == 0 && !nulp) {
        return NULL;
    }

    r = (zip_uint8_t *)malloc(length + (nulp ? 1 : 0));
    if (!r) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    if (buffer) {
        zip_uint8_t *data = _zip_buffer_get(buffer, length);

        if (data == NULL) {
            zip_error_set(error, ZIP_ER_MEMORY, 0);
            free(r);
            return NULL;
        }
        (void)memcpy_s(r, length, data, length);
    }
    else {
        if (_zip_read(src, r, length, error) < 0) {
            free(r);
            return NULL;
        }
    }

    if (nulp) {
        zip_uint8_t *o;
        /* replace any in-string NUL characters with spaces */
        r[length] = 0;
        for (o = r; o < r + length; o++)
            if (*o == '\0')
                *o = ' ';
    }

    return r;
}


zip_string_t *
_zip_read_string(zip_buffer_t *buffer, zip_source_t *src, zip_uint16_t len, bool nulp, zip_error_t *error) {
    zip_uint8_t *raw;
    zip_string_t *s;

    if ((raw = _zip_read_data(buffer, src, len, nulp, error)) == NULL)
        return NULL;

    s = _zip_string_new(raw, len, ZIP_FL_ENC_GUESS, error);
    free(raw);
    return s;
}


int
_zip_write(zip_t *za, const void *data, zip_uint64_t length) {
    zip_int64_t n;

    if ((n = zip_source_write(za->src, data, length)) < 0) {
        zip_error_set_from_source(&za->error, za->src);
        return -1;
    }
    if ((zip_uint64_t)n != length) {
        zip_error_set(&za->error, ZIP_ER_WRITE, EINTR);
        return -1;
    }

    return 0;
}
