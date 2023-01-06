/*
  zip_unchange.c -- undo changes to file in zip archive
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

#include "zipint.h"


ZIP_EXTERN int
zip_unchange(zip_t *za, zip_uint64_t idx) {
    return _zip_unchange(za, idx, 0);
}


int
_zip_unchange(zip_t *za, zip_uint64_t idx, int allow_duplicates) {
    zip_int64_t i;
    bool renamed;

    if (idx >= za->nentry) {
        zip_error_set(&za->error, ZIP_ER_INVAL, 0);
        return -1;
    }

    renamed = za->entry[idx].changes && (za->entry[idx].changes->changed & ZIP_DIRENT_FILENAME);
    if (!allow_duplicates && (renamed || za->entry[idx].deleted)) {
        const char *orig_name = NULL;
        const char *changed_name = NULL;

        if (za->entry[idx].orig != NULL) {
            if ((orig_name = _zip_get_name(za, idx, ZIP_FL_UNCHANGED, &za->error)) == NULL) {
                return -1;
            }

            i = _zip_name_locate(za, orig_name, 0, NULL);
            if (i >= 0 && (zip_uint64_t)i != idx) {
                zip_error_set(&za->error, ZIP_ER_EXISTS, 0);
                return -1;
            }
        }

        if (renamed) {
            if ((changed_name = _zip_get_name(za, idx, 0, &za->error)) == NULL) {
                return -1;
            }
        }

        if (orig_name) {
            if (_zip_hash_add(za->names, (const zip_uint8_t *)orig_name, idx, 0, &za->error) == false) {
                return -1;
            }
        }
        if (changed_name) {
            if (_zip_hash_delete(za->names, (const zip_uint8_t *)changed_name, &za->error) == false) {
                _zip_hash_delete(za->names, (const zip_uint8_t *)orig_name, NULL);
                return -1;
            }
        }
    }

    _zip_dirent_free(za->entry[idx].changes);
    za->entry[idx].changes = NULL;

    _zip_unchange_data(za->entry + idx);

    return 0;
}
