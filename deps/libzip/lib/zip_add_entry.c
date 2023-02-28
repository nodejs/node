/*
  zip_add_entry.c -- create and init struct zip_entry
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


/* NOTE: Signed due to -1 on error.  See zip_add.c for more details. */

zip_int64_t
_zip_add_entry(zip_t *za) {
    zip_uint64_t idx;

    if (za->nentry + 1 >= za->nentry_alloc) {
        zip_entry_t *rentries;
        zip_uint64_t nalloc = za->nentry_alloc;
        zip_uint64_t additional_entries = 2 * nalloc;
        zip_uint64_t realloc_size;

        if (additional_entries < 16) {
            additional_entries = 16;
        }
        else if (additional_entries > 1024) {
            additional_entries = 1024;
        }
        /* neither + nor * overflows can happen: nentry_alloc * sizeof(struct zip_entry) < UINT64_MAX */
        nalloc += additional_entries;
        realloc_size = sizeof(struct zip_entry) * (size_t)nalloc;

        if (sizeof(struct zip_entry) * (size_t)za->nentry_alloc > realloc_size) {
            zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
            return -1;
        }
        rentries = (zip_entry_t *)realloc(za->entry, sizeof(struct zip_entry) * (size_t)nalloc);
        if (!rentries) {
            zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
            return -1;
        }
        za->entry = rentries;
        za->nentry_alloc = nalloc;
    }

    idx = za->nentry++;

    _zip_entry_init(za->entry + idx);

    return (zip_int64_t)idx;
}
