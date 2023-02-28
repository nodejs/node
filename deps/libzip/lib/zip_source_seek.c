/*
  zip_source_seek.c -- seek to offset
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


#include "zipint.h"


ZIP_EXTERN int
zip_source_seek(zip_source_t *src, zip_int64_t offset, int whence) {
    zip_source_args_seek_t args;

    if (src->source_closed) {
        return -1;
    }
    if (!ZIP_SOURCE_IS_OPEN_READING(src) || (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)) {
        zip_error_set(&src->error, ZIP_ER_INVAL, 0);
        return -1;
    }

    args.offset = offset;
    args.whence = whence;

    if (_zip_source_call(src, &args, sizeof(args), ZIP_SOURCE_SEEK) < 0) {
        return -1;
    }

    src->eof = 0;
    return 0;
}


zip_int64_t
zip_source_seek_compute_offset(zip_uint64_t offset, zip_uint64_t length, void *data, zip_uint64_t data_length, zip_error_t *error) {
    zip_int64_t new_offset;
    zip_source_args_seek_t *args = ZIP_SOURCE_GET_ARGS(zip_source_args_seek_t, data, data_length, error);

    if (args == NULL) {
        return -1;
    }

    switch (args->whence) {
    case SEEK_CUR:
        new_offset = (zip_int64_t)offset + args->offset;
        break;

    case SEEK_END:
        new_offset = (zip_int64_t)length + args->offset;
        break;

    case SEEK_SET:
        new_offset = args->offset;
        break;

    default:
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return -1;
    }

    if (new_offset < 0 || (zip_uint64_t)new_offset > length) {
        zip_error_set(error, ZIP_ER_INVAL, 0);
        return -1;
    }

    return new_offset;
}
