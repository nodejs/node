/*
  zip_fdopen.c -- open read-only archive from file descriptor
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


#include "zipint.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


ZIP_EXTERN zip_t *
zip_fdopen(int fd_orig, int _flags, int *zep) {
    int fd;
    FILE *fp;
    zip_t *za;
    zip_source_t *src;
    struct zip_error error;

    if (_flags < 0 || (_flags & ~(ZIP_CHECKCONS | ZIP_RDONLY))) {
        _zip_set_open_error(zep, NULL, ZIP_ER_INVAL);
        return NULL;
    }

#ifndef ENABLE_FDOPEN
    _zip_set_open_error(zep, NULL, ZIP_ER_OPNOTSUPP);
    return NULL;
#else
    /* We dup() here to avoid messing with the passed in fd.
       We could not restore it to the original state in case of error. */

    if ((fd = dup(fd_orig)) < 0) {
        _zip_set_open_error(zep, NULL, ZIP_ER_OPEN);
        return NULL;
    }

    if ((fp = fdopen(fd, "rb")) == NULL) {
        close(fd);
        _zip_set_open_error(zep, NULL, ZIP_ER_OPEN);
        return NULL;
    }

    zip_error_init(&error);
    if ((src = zip_source_filep_create(fp, 0, -1, &error)) == NULL) {
        fclose(fp);
        _zip_set_open_error(zep, &error, 0);
        zip_error_fini(&error);
        return NULL;
    }

    if ((za = zip_open_from_source(src, _flags, &error)) == NULL) {
        zip_source_free(src);
        _zip_set_open_error(zep, &error, 0);
        zip_error_fini(&error);
        return NULL;
    }

    zip_error_fini(&error);
    close(fd_orig);
    return za;
#endif
}
