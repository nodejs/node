/*
  add_from_filep.c -- test case for adding file to archive
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


#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "zip.h"

static const char *prg;

int
main(int argc, char *argv[]) {
    const char *archive;
    const char *file;
    const char *name;
    zip_t *za;
    zip_source_t *zs;
    int err;
    FILE *fp;

    prg = argv[0];

    if (argc != 3) {
        fprintf(stderr, "usage: %s archive file\n", prg);
        return 1;
    }

    archive = argv[1];
    file = argv[2];

    if ((za = zip_open(archive, ZIP_CREATE, &err)) == NULL) {
        zip_error_t error;
        zip_error_init_with_code(&error, err);
        fprintf(stderr, "%s: can't open zip archive '%s': %s\n", prg, archive, zip_error_strerror(&error));
        zip_error_fini(&error);
        return 1;
    }

    if ((fp = fopen(file, "rb")) == NULL) {
        fprintf(stderr, "%s: can't open input file '%s': %s\n", prg, file, strerror(errno));
        return 1;
    }

    if ((zs = zip_source_filep(za, fp, 0, -1)) == NULL) {
        fprintf(stderr, "%s: error creating file source for '%s': %s\n", prg, file, zip_strerror(za));
        return 1;
    }

    if ((name = strrchr(file, '/')) == NULL)
        name = file;

    if (zip_add(za, name, zs) == -1) {
        zip_source_free(zs);
        fprintf(stderr, "%s: can't add file '%s': %s\n", prg, file, zip_strerror(za));
        return 1;
    }

    if (zip_close(za) == -1) {
        fprintf(stderr, "%s: can't close zip archive '%s': %s\n", prg, archive, zip_strerror(za));
        return 1;
    }

    return 0;
}
