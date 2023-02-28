/*
  fread.c -- test cases for reading from zip archives
  Copyright (C) 2004-2021 Dieter Baron and Thomas Klausner

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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef HAVE_GETOPT
#include "getopt.h"
#endif

#include "zip.h"

enum when { WHEN_NEVER, WHEN_OPEN, WHEN_READ, WHEN_CLOSE };

const char *when_name[] = {"no", "zip_fopen", "zip_fread", "zip_fclose"};

static int do_read(zip_t *z, const char *name, zip_flags_t flags, enum when when_ex, int ze_ex, int se_ex);

int verbose;

const char *progname;
#define USAGE "usage: %s [-v] archive\n"

int
main(int argc, char *argv[]) {
    int fail, ze;
    int c;
    zip_t *z;
    zip_source_t *zs;
    char *archive;
    zip_int64_t idx;

    verbose = 0;
    fail = 0;

    progname = argv[0];

    while ((c = getopt(argc, argv, "v")) != -1) {
        switch (c) {
        case 'v':
            verbose = 1;
            break;

        default:
            fprintf(stderr, USAGE, progname);
            return 1;
        }
    }


    if (argc - optind != 1) {
        fprintf(stderr, USAGE, progname);
        return 1;
    }

    archive = argv[optind];

    if ((z = zip_open(archive, 0, &ze)) == NULL) {
        zip_error_t error;
        zip_error_init_with_code(&error, ze);
        fprintf(stderr, "%s: can't open zip archive '%s': %s\n", progname, archive, zip_error_strerror(&error));
        zip_error_fini(&error);
        return 1;
    }

    fail += do_read(z, "storedok", 0, WHEN_NEVER, 0, 0);
    fail += do_read(z, "deflateok", 0, WHEN_NEVER, 0, 0);
    fail += do_read(z, "storedcrcerror", 0, WHEN_READ, ZIP_ER_CRC, 0);
    fail += do_read(z, "deflatecrcerror", 0, WHEN_READ, ZIP_ER_CRC, 0);
    fail += do_read(z, "deflatezliberror", 0, WHEN_READ, ZIP_ER_ZLIB, -3);
#ifndef __clang_analyzer__ /* This test intentionally violates nullability. */
    fail += do_read(z, NULL, 0, WHEN_OPEN, ZIP_ER_INVAL, 0);
#endif
    fail += do_read(z, "nosuchfile", 0, WHEN_OPEN, ZIP_ER_NOENT, 0);
    fail += do_read(z, "deflatezliberror", ZIP_FL_COMPRESSED, WHEN_NEVER, 0, 0);
    fail += do_read(z, "deflatecrcerror", ZIP_FL_COMPRESSED, WHEN_NEVER, 0, 0);
    fail += do_read(z, "storedcrcerror", ZIP_FL_COMPRESSED, WHEN_READ, ZIP_ER_CRC, 0);
    fail += do_read(z, "storedok", ZIP_FL_COMPRESSED, WHEN_NEVER, 0, 0);

    fail += do_read(z, "cryptok", 0, WHEN_OPEN, ZIP_ER_NOPASSWD, 0);
    zip_set_default_password(z, "crypt");
    fail += do_read(z, "cryptok", 0, WHEN_NEVER, 0, 0);
    zip_set_default_password(z, "wrong");
    fail += do_read(z, "cryptok", 0, WHEN_OPEN, ZIP_ER_WRONGPASSWD, 0);
    zip_set_default_password(z, NULL);

    zs = zip_source_buffer(z, "asdf", 4, 0);
    if ((idx = zip_name_locate(z, "storedok", 0)) < 0) {
        fprintf(stderr, "%s: can't locate 'storedok' in zip archive '%s': %s\n", progname, archive, zip_strerror(z));
        fail++;
    }
    else {
        if (zip_replace(z, (zip_uint64_t)idx, zs) < 0) {
            fprintf(stderr, "%s: can't replace 'storedok' in zip archive '%s': %s\n", progname, archive, zip_strerror(z));
            fail++;
        }
        else {
            fail += do_read(z, "storedok", 0, WHEN_NEVER, 0, 0);
            fail += do_read(z, "storedok", ZIP_FL_UNCHANGED, WHEN_NEVER, 0, 0);
        }
    }
    if ((idx = zip_name_locate(z, "storedok", 0)) < 0) {
        fprintf(stderr, "%s: can't locate 'storedok' in zip archive '%s': %s\n", progname, archive, zip_strerror(z));
        fail++;
    }
    else {
        if (zip_delete(z, (zip_uint64_t)idx) < 0) {
            fprintf(stderr, "%s: can't replace 'storedok' in zip archive '%s': %s\n", progname, archive, zip_strerror(z));
            fail++;
        }
        else {
            fail += do_read(z, "storedok", 0, WHEN_OPEN, ZIP_ER_NOENT, 0);
            fail += do_read(z, "storedok", ZIP_FL_UNCHANGED, WHEN_NEVER, 0, 0);
        }
    }
    zs = zip_source_buffer(z, "asdf", 4, 0);
    if (zip_file_add(z, "new_file", zs, 0) < 0) {
        fprintf(stderr, "%s: can't add file to zip archive '%s': %s\n", progname, archive, zip_strerror(z));
        fail++;
    }
    else {
        fail += do_read(z, "new_file", 0, WHEN_NEVER, 0, 0);
    }

    zip_unchange_all(z);
    if (zip_close(z) == -1) {
        fprintf(stderr, "%s: can't close zip archive '%s': %s\n", progname, archive, zip_strerror(z));
        return 1;
    }

    exit(fail ? 1 : 0);
}


static int
do_read(zip_t *z, const char *name, zip_flags_t flags, enum when when_ex, int ze_ex, int se_ex) {
    zip_file_t *zf;
    enum when when_got;
    zip_error_t error_got, error_ex;
    zip_error_t *zf_error;
    int err;
    char b[8192];
    zip_int64_t n;

    when_got = WHEN_NEVER;
    zip_error_init(&error_got);
    zip_error_init(&error_ex);
    zip_error_set(&error_ex, ze_ex, se_ex);

    if ((zf = zip_fopen(z, name, flags)) == NULL) {
        when_got = WHEN_OPEN;
        zf_error = zip_get_error(z);
        zip_error_set(&error_got, zip_error_code_zip(zf_error), zip_error_code_system(zf_error));
    }
    else {
        while ((n = zip_fread(zf, b, sizeof(b))) > 0)
            ;
        if (n < 0) {
            when_got = WHEN_READ;
            zf_error = zip_file_get_error(zf);
            zip_error_set(&error_got, zip_error_code_zip(zf_error), zip_error_code_system(zf_error));
        }
        err = zip_fclose(zf);
        if (when_got == WHEN_NEVER && err != 0) {
            when_got = WHEN_CLOSE;
            zip_error_init_with_code(&error_got, err);
        }
    }

    if (when_got != when_ex || zip_error_code_zip(&error_got) != zip_error_code_zip(&error_ex) || zip_error_code_system(&error_got) != zip_error_code_system(&error_ex)) {
        printf("%s: %s: got %s error (%s), expected %s error (%s)\n", progname, name, when_name[when_got], zip_error_strerror(&error_got), when_name[when_ex], zip_error_strerror(&error_ex));
        zip_error_fini(&error_got);
        zip_error_fini(&error_ex);
        return 1;
    }
    else if (verbose)
        printf("%s: %s: passed\n", progname, name);

    return 0;
}
