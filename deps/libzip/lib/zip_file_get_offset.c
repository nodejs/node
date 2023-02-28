/*
  zip_file_get_offset.c -- get offset of file data in archive.
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
#include <string.h>

#include "zipint.h"


/* _zip_file_get_offset(za, ze):
   Returns the offset of the file data for entry ze.

   On error, fills in za->error and returns 0.
*/

zip_uint64_t
_zip_file_get_offset(const zip_t *za, zip_uint64_t idx, zip_error_t *error) {
    zip_uint64_t offset;
    zip_int32_t size;

    if (za->entry[idx].orig == NULL) {
        zip_error_set(error, ZIP_ER_INTERNAL, 0);
        return 0;
    }

    offset = za->entry[idx].orig->offset;

    if (zip_source_seek(za->src, (zip_int64_t)offset, SEEK_SET) < 0) {
        zip_error_set_from_source(error, za->src);
        return 0;
    }

    /* TODO: cache? */
    if ((size = _zip_dirent_size(za->src, ZIP_EF_LOCAL, error)) < 0)
        return 0;

    if (offset + (zip_uint32_t)size > ZIP_INT64_MAX) {
        zip_error_set(error, ZIP_ER_SEEK, EFBIG);
        return 0;
    }

    return offset + (zip_uint32_t)size;
}

zip_uint64_t
_zip_file_get_end(const zip_t *za, zip_uint64_t index, zip_error_t *error) {
    zip_uint64_t offset;
    zip_dirent_t *entry;

    if ((offset = _zip_file_get_offset(za, index, error)) == 0) {
        return 0;
    }

    entry = za->entry[index].orig;

    if (offset + entry->comp_size < offset || offset + entry->comp_size > ZIP_INT64_MAX) {
        zip_error_set(error, ZIP_ER_SEEK, EFBIG);
        return 0;
    }
    offset += entry->comp_size;

    if (entry->bitflags & ZIP_GPBF_DATA_DESCRIPTOR) {
        zip_uint8_t buf[4];
        if (zip_source_seek(za->src, (zip_int64_t)offset, SEEK_SET) < 0) {
            zip_error_set_from_source(error, za->src);
            return 0;
        }
        if (zip_source_read(za->src, buf, 4) != 4) {
            zip_error_set_from_source(error, za->src);
            return 0;
        }
        if (memcmp(buf, DATADES_MAGIC, 4) == 0) {
            offset += 4;
        }
        offset += 12;
        if (_zip_dirent_needs_zip64(entry, 0)) {
            offset += 8;
        }
        if (offset > ZIP_INT64_MAX) {
            zip_error_set(error, ZIP_ER_SEEK, EFBIG);
            return 0;
        }
    }

    return offset;
}
