/*
  zip_stat_init.c -- initialize struct zip_stat.
  Copyright (C) 2006-2021 Dieter Baron and Thomas Klausner

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

#include <string.h>

#include "zipint.h"


ZIP_EXTERN void
zip_stat_init(zip_stat_t *st) {
    st->valid = 0;
    st->name = NULL;
    st->index = ZIP_UINT64_MAX;
    st->crc = 0;
    st->mtime = (time_t)-1;
    st->size = 0;
    st->comp_size = 0;
    st->comp_method = ZIP_CM_STORE;
    st->encryption_method = ZIP_EM_NONE;
}


int
_zip_stat_merge(zip_stat_t *dst, const zip_stat_t *src, zip_error_t *error) {
    /* name is not merged, since zip_stat_t doesn't own it, and src may not be valid as long as dst */
    if (src->valid & ZIP_STAT_INDEX) {
        dst->index = src->index;
    }
    if (src->valid & ZIP_STAT_SIZE) {
        dst->size = src->size;
    }
    if (src->valid & ZIP_STAT_COMP_SIZE) {
        dst->comp_size = src->comp_size;
    }
    if (src->valid & ZIP_STAT_MTIME) {
        dst->mtime = src->mtime;
    }
    if (src->valid & ZIP_STAT_CRC) {
        dst->crc = src->crc;
    }
    if (src->valid & ZIP_STAT_COMP_METHOD) {
        dst->comp_method = src->comp_method;
    }
    if (src->valid & ZIP_STAT_ENCRYPTION_METHOD) {
        dst->encryption_method = src->encryption_method;
    }
    if (src->valid & ZIP_STAT_FLAGS) {
        dst->flags = src->flags;
    }
    dst->valid |= src->valid;

    return 0;
}
