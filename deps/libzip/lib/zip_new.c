/*
  zip_new.c -- create and init struct zip
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


/* _zip_new:
   creates a new zipfile struct, and sets the contents to zero; returns
   the new struct. */

zip_t *
_zip_new(zip_error_t *error) {
    zip_t *za;

    za = (zip_t *)malloc(sizeof(struct zip));
    if (!za) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    if ((za->names = _zip_hash_new(error)) == NULL) {
        free(za);
        return NULL;
    }

    za->src = NULL;
    za->open_flags = 0;
    zip_error_init(&za->error);
    za->flags = za->ch_flags = 0;
    za->default_password = NULL;
    za->comment_orig = za->comment_changes = NULL;
    za->comment_changed = 0;
    za->nentry = za->nentry_alloc = 0;
    za->entry = NULL;
    za->nopen_source = za->nopen_source_alloc = 0;
    za->open_source = NULL;
    za->progress = NULL;

    return za;
}
