/*
  fseek.c -- test tool for seeking in zip archives
  Copyright (C) 2016-2021 Dieter Baron and Thomas Klausner

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

#include "zip.h"

const char *progname;
#define USAGE "usage: %s archive index offset\n"

int
main(int argc, char *argv[]) {
    int ze;
    zip_t *z;
    zip_file_t *zf;
    char *archive;
    zip_int64_t offset, n;
    zip_uint64_t index;
    char b[1024];

    progname = argv[0];

    if (argc != 4) {
        fprintf(stderr, USAGE, progname);
        return 1;
    }

    archive = argv[1];
    index = strtoull(argv[2], NULL, 10);
    offset = (zip_int64_t)strtoull(argv[3], NULL, 10);

    if ((z = zip_open(archive, 0, &ze)) == NULL) {
        zip_error_t error;
        zip_error_init_with_code(&error, ze);
        fprintf(stderr, "%s: can't open zip archive '%s': %s\n", progname, archive, zip_error_strerror(&error));
        zip_error_fini(&error);
        return 1;
    }

    if ((zf = zip_fopen_index(z, index, 0)) == NULL) {
        fprintf(stderr, "%s: can't open file in archive '%s': %s\n", progname, archive, zip_error_strerror(zip_file_get_error(zf)));
        zip_close(z);
        return 1;
    }

    if (zip_fseek(zf, offset, SEEK_SET) < 0) {
        fprintf(stderr, "%s: zip_fseek failed: %s\n", progname, zip_error_strerror(zip_file_get_error(zf)));
        zip_fclose(zf);
        zip_close(z);
        return 1;
    }

    while ((n = zip_fread(zf, b, sizeof(b))) > 0) {
        printf("%.*s", (int)n, b);
    }
    if (n < 0) {
        fprintf(stderr, "%s: zip_fread failed: %s\n", progname, zip_error_strerror(zip_file_get_error(zf)));
        zip_fclose(zf);
        zip_close(z);
        return 1;
    }

    if (zip_fclose(zf) == -1) {
        fprintf(stderr, "%s: can't close zip archive entry %" PRIu64 " in '%s': %s\n", progname, index, archive, zip_strerror(z));
        return 1;
    }

    if (zip_close(z) == -1) {
        fprintf(stderr, "%s: can't close zip archive '%s': %s\n", progname, archive, zip_strerror(z));
        return 1;
    }

    return 0;
}
