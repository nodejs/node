/*
  zip_source_read.c -- read data from zip_source
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


#include "zipint.h"


zip_int64_t
zip_source_read(zip_source_t *src, void *data, zip_uint64_t len) {
    zip_uint64_t bytes_read;
    zip_int64_t n;

    if (src->source_closed) {
        return -1;
    }
    if (!ZIP_SOURCE_IS_OPEN_READING(src) || len > ZIP_INT64_MAX || (len > 0 && data == NULL)) {
        zip_error_set(&src->error, ZIP_ER_INVAL, 0);
        return -1;
    }

    if (src->had_read_error) {
        return -1;
    }

    if (_zip_source_eof(src)) {
        return 0;
    }

    if (len == 0) {
        return 0;
    }

    bytes_read = 0;
    while (bytes_read < len) {
        if ((n = _zip_source_call(src, (zip_uint8_t *)data + bytes_read, len - bytes_read, ZIP_SOURCE_READ)) < 0) {
            src->had_read_error = true;
            if (bytes_read == 0) {
                return -1;
            }
            else {
                return (zip_int64_t)bytes_read;
            }
        }

        if (n == 0) {
            src->eof = 1;
            break;
        }

        bytes_read += (zip_uint64_t)n;
    }

    if (src->bytes_read + bytes_read < src->bytes_read) {
        src->bytes_read = ZIP_UINT64_MAX;
    }
    else {
        src->bytes_read += bytes_read;
    }
    return (zip_int64_t)bytes_read;
}


bool
_zip_source_eof(zip_source_t *src) {
    return src->eof;
}
