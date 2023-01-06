/*
  zip_source_layered.c -- create layered source
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


#include <stdlib.h>

#include "zipint.h"


zip_source_t *
zip_source_layered(zip_t *za, zip_source_t *src, zip_source_layered_callback cb, void *ud) {
    if (za == NULL)
        return NULL;

    return zip_source_layered_create(src, cb, ud, &za->error);
}


zip_source_t *
zip_source_layered_create(zip_source_t *src, zip_source_layered_callback cb, void *ud, zip_error_t *error) {
    zip_source_t *zs;
    zip_int64_t lower_supports, supports;

    lower_supports = zip_source_supports(src);
    supports = cb(src, ud, &lower_supports, sizeof(lower_supports), ZIP_SOURCE_SUPPORTS);
    if (supports < 0) {
        zip_error_set(error,ZIP_ER_INVAL, 0); /* Initialize in case cb doesn't return valid error. */
        cb(src, ud, error, sizeof(*error), ZIP_SOURCE_ERROR);
        return NULL;
    }

    if ((zs = _zip_source_new(error)) == NULL) {
        return NULL;
    }

    zip_source_keep(src);
    zs->src = src;
    zs->cb.l = cb;
    zs->ud = ud;
    zs->supports = supports;

    /* Layered sources can't support writing, since we currently have no use case. If we want to revisit this, we have to define how the two sources interact. */
    zs->supports &= ~(ZIP_SOURCE_SUPPORTS_WRITABLE & ~ZIP_SOURCE_SUPPORTS_SEEKABLE);

    return zs;
}
