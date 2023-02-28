/*
  zip_error.c -- zip_error_t helper functions
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

#include "zipint.h"


ZIP_EXTERN int
zip_error_code_system(const zip_error_t *error) {
    return error->sys_err;
}


ZIP_EXTERN int
zip_error_code_zip(const zip_error_t *error) {
    return error->zip_err;
}


ZIP_EXTERN void
zip_error_fini(zip_error_t *err) {
    free(err->str);
    err->str = NULL;
}


ZIP_EXTERN void
zip_error_init(zip_error_t *err) {
    err->zip_err = ZIP_ER_OK;
    err->sys_err = 0;
    err->str = NULL;
}

ZIP_EXTERN void
zip_error_init_with_code(zip_error_t *error, int ze) {
    zip_error_init(error);
    error->zip_err = ze;
    switch (zip_error_system_type(error)) {
        case ZIP_ET_SYS:
        case ZIP_ET_LIBZIP:
            error->sys_err = errno;
            break;
            
        default:
            error->sys_err = 0;
            break;
    }
}


ZIP_EXTERN int
zip_error_system_type(const zip_error_t *error) {
    if (error->zip_err < 0 || error->zip_err >= _zip_err_str_count)
        return ZIP_ET_NONE;

    return _zip_err_str[error->zip_err].type;
}


void
_zip_error_clear(zip_error_t *err) {
    if (err == NULL)
        return;

    err->zip_err = ZIP_ER_OK;
    err->sys_err = 0;
}


void
_zip_error_copy(zip_error_t *dst, const zip_error_t *src) {
    if (dst == NULL) {
        return;
    }

    dst->zip_err = src->zip_err;
    dst->sys_err = src->sys_err;
}


void
_zip_error_get(const zip_error_t *err, int *zep, int *sep) {
    if (zep)
        *zep = err->zip_err;
    if (sep) {
        if (zip_error_system_type(err) != ZIP_ET_NONE)
            *sep = err->sys_err;
        else
            *sep = 0;
    }
}


void
zip_error_set(zip_error_t *err, int ze, int se) {
    if (err) {
        err->zip_err = ze;
        err->sys_err = se;
    }
}


void
zip_error_set_from_source(zip_error_t *err, zip_source_t *src) {
    if (src == NULL) {
        zip_error_set(err, ZIP_ER_INVAL, 0);
        return;
    }

    _zip_error_copy(err, zip_source_error(src));
}


zip_int64_t
zip_error_to_data(const zip_error_t *error, void *data, zip_uint64_t length) {
    int *e = (int *)data;

    if (length < sizeof(int) * 2) {
        return -1;
    }

    e[0] = zip_error_code_zip(error);
    e[1] = zip_error_code_system(error);
    return sizeof(int) * 2;
}
