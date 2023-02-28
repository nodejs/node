/*
  zip_stat_index.c -- get information about file by index
  Copyright (C) 1999-2020 Dieter Baron and Thomas Klausner

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


ZIP_EXTERN int
zip_stat_index(zip_t *za, zip_uint64_t index, zip_flags_t flags, zip_stat_t *st) {
    const char *name;
    zip_dirent_t *de;

    if ((de = _zip_get_dirent(za, index, flags, NULL)) == NULL)
        return -1;

    if ((name = zip_get_name(za, index, flags)) == NULL)
        return -1;


    if ((flags & ZIP_FL_UNCHANGED) == 0 && ZIP_ENTRY_DATA_CHANGED(za->entry + index)) {
        zip_entry_t *entry = za->entry + index;

        if (zip_source_stat(entry->source, st) < 0) {
            zip_error_set(&za->error, ZIP_ER_CHANGED, 0);
            return -1;
        }

        if (entry->changes != NULL && entry->changes->changed & ZIP_DIRENT_LAST_MOD) {
            st->mtime = de->last_mod;
            st->valid |= ZIP_STAT_MTIME;
        }
    }
    else {
        zip_stat_init(st);

        st->crc = de->crc;
        st->size = de->uncomp_size;
        st->mtime = de->last_mod;
        st->comp_size = de->comp_size;
        st->comp_method = (zip_uint16_t)de->comp_method;
        st->encryption_method = de->encryption_method;
        st->valid = (de->crc_valid ? ZIP_STAT_CRC : 0) | ZIP_STAT_SIZE | ZIP_STAT_MTIME | ZIP_STAT_COMP_SIZE | ZIP_STAT_COMP_METHOD | ZIP_STAT_ENCRYPTION_METHOD;
    }

    st->index = index;
    st->name = name;
    st->valid |= ZIP_STAT_INDEX | ZIP_STAT_NAME;

    return 0;
}
