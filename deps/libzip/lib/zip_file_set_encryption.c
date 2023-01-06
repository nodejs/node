/*
  zip_file_set_encryption.c -- set encryption for file in archive
  Copyright (C) 2016-2021 Dieter Baron and Thomas Klausner

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

#include <stdlib.h>
#include <string.h>

ZIP_EXTERN int
zip_file_set_encryption(zip_t *za, zip_uint64_t idx, zip_uint16_t method, const char *password) {
    zip_entry_t *e;
    zip_uint16_t old_method;

    if (idx >= za->nentry) {
        zip_error_set(&za->error, ZIP_ER_INVAL, 0);
        return -1;
    }

    if (ZIP_IS_RDONLY(za)) {
        zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
        return -1;
    }

    if (method != ZIP_EM_NONE && _zip_get_encryption_implementation(method, ZIP_CODEC_ENCODE) == NULL) {
        zip_error_set(&za->error, ZIP_ER_ENCRNOTSUPP, 0);
        return -1;
    }

    e = za->entry + idx;

    old_method = (e->orig == NULL ? ZIP_EM_NONE : e->orig->encryption_method);

    if (method == old_method && password == NULL) {
        if (e->changes) {
            if (e->changes->changed & ZIP_DIRENT_PASSWORD) {
                _zip_crypto_clear(e->changes->password, strlen(e->changes->password));
                free(e->changes->password);
                e->changes->password = (e->orig == NULL ? NULL : e->orig->password);
            }
            e->changes->changed &= ~(ZIP_DIRENT_ENCRYPTION_METHOD | ZIP_DIRENT_PASSWORD);
            if (e->changes->changed == 0) {
                _zip_dirent_free(e->changes);
                e->changes = NULL;
            }
        }
    }
    else {
        char *our_password = NULL;

        if (password) {
            if ((our_password = strdup(password)) == NULL) {
                zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
                return -1;
            }
        }

        if (e->changes == NULL) {
            if ((e->changes = _zip_dirent_clone(e->orig)) == NULL) {
                if (our_password) {
                    _zip_crypto_clear(our_password, strlen(our_password));
                }
                free(our_password);
                zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
                return -1;
            }
        }

        e->changes->encryption_method = method;
        e->changes->changed |= ZIP_DIRENT_ENCRYPTION_METHOD;
        if (password) {
            e->changes->password = our_password;
            e->changes->changed |= ZIP_DIRENT_PASSWORD;
        }
        else {
            if (e->changes->changed & ZIP_DIRENT_PASSWORD) {
                _zip_crypto_clear(e->changes->password, strlen(e->changes->password));
                free(e->changes->password);
                e->changes->password = e->orig ? e->orig->password : NULL;
                e->changes->changed &= ~ZIP_DIRENT_PASSWORD;
            }
        }
    }

    return 0;
}
