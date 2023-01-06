/*
  zip_discard.c -- discard and free struct zip
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


/* zip_discard:
   frees the space allocated to a zipfile struct, and closes the
   corresponding file. */

void
zip_discard(zip_t *za) {
    zip_uint64_t i;

    if (za == NULL)
        return;

    if (za->src) {
        zip_source_close(za->src);
        zip_source_free(za->src);
    }

    free(za->default_password);
    _zip_string_free(za->comment_orig);
    _zip_string_free(za->comment_changes);

    _zip_hash_free(za->names);

    if (za->entry) {
        for (i = 0; i < za->nentry; i++)
            _zip_entry_finalize(za->entry + i);
        free(za->entry);
    }

    for (i = 0; i < za->nopen_source; i++) {
        _zip_source_invalidate(za->open_source[i]);
    }
    free(za->open_source);

    _zip_progress_free(za->progress);

    zip_error_fini(&za->error);

    free(za);

    return;
}
