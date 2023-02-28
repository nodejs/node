/*
  zip_source_pass_to_lower_layer.c -- pass command to lower layer
  Copyright (C) 2022 Dieter Baron and Thomas Klausner

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

zip_int64_t zip_source_pass_to_lower_layer(zip_source_t *src, void *data, zip_uint64_t length, zip_source_cmd_t command) {
    switch (command) {
    case ZIP_SOURCE_OPEN:
    case ZIP_SOURCE_CLOSE:
    case ZIP_SOURCE_FREE:
    case ZIP_SOURCE_GET_FILE_ATTRIBUTES:
    case ZIP_SOURCE_SUPPORTS_REOPEN:
        return 0;

    case ZIP_SOURCE_STAT:
        return sizeof(zip_stat_t);

    case ZIP_SOURCE_ACCEPT_EMPTY:
    case ZIP_SOURCE_ERROR:
    case ZIP_SOURCE_READ:
    case ZIP_SOURCE_SEEK:
    case ZIP_SOURCE_TELL:
        return _zip_source_call(src, data, length, command);


    case ZIP_SOURCE_BEGIN_WRITE:
    case ZIP_SOURCE_BEGIN_WRITE_CLONING:
    case ZIP_SOURCE_COMMIT_WRITE:
    case ZIP_SOURCE_REMOVE:
    case ZIP_SOURCE_ROLLBACK_WRITE:
    case ZIP_SOURCE_SEEK_WRITE:
    case ZIP_SOURCE_TELL_WRITE:
    case ZIP_SOURCE_WRITE:
        zip_error_set(&src->error, ZIP_ER_OPNOTSUPP, 0);
        return -1;

    case ZIP_SOURCE_SUPPORTS:
        if (length < sizeof(zip_int64_t)) {
            zip_error_set(&src->error, ZIP_ER_INTERNAL, 0);
            return -1;
        }
        return *(zip_int64_t *)data;

    default:
        zip_error_set(&src->error, ZIP_ER_OPNOTSUPP, 0);
        return -1;
    }
}