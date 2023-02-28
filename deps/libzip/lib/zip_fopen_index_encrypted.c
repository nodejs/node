/*
  zip_fopen_index_encrypted.c -- open file for reading by index w/ password
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


#include <stdio.h>
#include <stdlib.h>

#include "zipint.h"

static zip_file_t *_zip_file_new(zip_t *za);


ZIP_EXTERN zip_file_t *
zip_fopen_index_encrypted(zip_t *za, zip_uint64_t index, zip_flags_t flags, const char *password) {
    zip_file_t *zf;
    zip_source_t *src;

    if (password != NULL && password[0] == '\0') {
        password = NULL;
    }
    
    if ((src = _zip_source_zip_new(za, index, flags, 0, 0, password, &za->error)) == NULL)
        return NULL;

    if (zip_source_open(src) < 0) {
        zip_error_set_from_source(&za->error, src);
        zip_source_free(src);
        return NULL;
    }

    if ((zf = _zip_file_new(za)) == NULL) {
        zip_source_free(src);
        return NULL;
    }

    zf->src = src;

    return zf;
}


static zip_file_t *
_zip_file_new(zip_t *za) {
    zip_file_t *zf;

    if ((zf = (zip_file_t *)malloc(sizeof(struct zip_file))) == NULL) {
        zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    zf->za = za;
    zip_error_init(&zf->error);
    zf->src = NULL;

    return zf;
}
