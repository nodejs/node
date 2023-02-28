/*
  zip_get_archive_flag.c -- set archive global flag
  Copyright (C) 2008-2021 Dieter Baron and Thomas Klausner

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
zip_set_archive_flag(zip_t *za, zip_flags_t flag, int value) {
    unsigned int new_flags;

    if (value)
        new_flags = za->ch_flags | flag;
    else
        new_flags = za->ch_flags & ~flag;

    if (new_flags == za->ch_flags)
        return 0;

    if (ZIP_IS_RDONLY(za)) {
        zip_error_set(&za->error, ZIP_ER_RDONLY, 0);
        return -1;
    }

    if ((flag & ZIP_AFL_RDONLY) && value && (za->ch_flags & ZIP_AFL_RDONLY) == 0) {
        if (_zip_changed(za, NULL)) {
            zip_error_set(&za->error, ZIP_ER_CHANGED, 0);
            return -1;
        }
    }

    za->ch_flags = new_flags;

    return 0;
}
