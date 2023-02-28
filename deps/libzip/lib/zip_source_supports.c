/*
  zip_source_supports.c -- check for supported functions
  Copyright (C) 2014-2021 Dieter Baron and Thomas Klausner

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


#include <stdarg.h>

#include "zipint.h"


zip_int64_t
zip_source_supports(zip_source_t *src) {
    return src->supports;
}

bool
zip_source_supports_reopen(zip_source_t *src) {
    return (zip_source_supports(src) & ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_SUPPORTS_REOPEN)) != 0;
}

ZIP_EXTERN zip_int64_t
zip_source_make_command_bitmap(zip_source_cmd_t cmd0, ...) {
    zip_int64_t bitmap;
    va_list ap;

    bitmap = ZIP_SOURCE_MAKE_COMMAND_BITMASK(cmd0);


    va_start(ap, cmd0);
    for (;;) {
        int cmd = va_arg(ap, int);
        if (cmd < 0) {
            break;
        }
        bitmap |= ZIP_SOURCE_MAKE_COMMAND_BITMASK(cmd);
    }
    va_end(ap);

    return bitmap;
}
