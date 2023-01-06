/*
  zip_dir_add.c -- add directory
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
#include <string.h>

#include "zipint.h"


/* NOTE: Signed due to -1 on error.  See zip_add.c for more details. */

ZIP_EXTERN zip_int64_t
zip_dir_add(zip_t *za, const char *name, zip_flags_t flags) {
    size_t len;
    zip_int64_t idx;
    char *s;
    zip_source_t *source;

    if (ZIP_IS_RDONLY(za)) {
        zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
        return -1;
    }

    if (name == NULL) {
        zip_error_set(&za->error, ZIP_ER_INVAL, 0);
        return -1;
    }

    s = NULL;
    len = strlen(name);

    if (name[len - 1] != '/') {
        if (len > SIZE_MAX - 2 || (s = (char *)malloc(len + 2)) == NULL) {
            zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
            return -1;
        }
        (void)strncpy_s(s, len + 2, name, len);
        s[len] = '/';
        s[len + 1] = '\0';
    }

    if ((source = zip_source_buffer(za, NULL, 0, 0)) == NULL) {
        free(s);
        return -1;
    }

    idx = _zip_file_replace(za, ZIP_UINT64_MAX, s ? s : name, source, flags);

    free(s);

    if (idx < 0)
        zip_source_free(source);
    else {
        if (zip_file_set_external_attributes(za, (zip_uint64_t)idx, 0, ZIP_OPSYS_DEFAULT, ZIP_EXT_ATTRIB_DEFAULT_DIR) < 0) {
            zip_delete(za, (zip_uint64_t)idx);
            return -1;
        }
    }

    return idx;
}
