/*
  zipmerge.c -- merge zip archives
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


#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#ifndef HAVE_GETOPT
#include "getopt.h"
#endif

#include "zip.h"

char *progname;

#define PROGRAM "zipmerge"

#define USAGE "usage: %s [-DhIikSsV] target-zip zip...\n"

char help_head[] = PROGRAM " (" PACKAGE ") by Dieter Baron and Thomas Klausner\n\n";

char help[] = "\n\
  -h       display this help message\n\
  -V       display version number\n\
  -D       ignore directory component in file names\n\
  -I       ignore case in file names\n\
  -i       ask before overwriting files\n\
  -k       don't compress when adding uncompressed files\n\
  -S       don't overwrite identical files\n\
  -s       overwrite identical files without asking\n\
\n\
Report bugs to <info@libzip.org>.\n";

char version_string[] = PROGRAM " (" PACKAGE " " VERSION ")\n\
Copyright (C) 2004-2022 Dieter Baron and Thomas Klausner\n\
" PACKAGE " comes with ABSOLUTELY NO WARRANTY, to the extent permitted by law.\n";

#define OPTIONS "hVDiIksS"

#define CONFIRM_ALL_YES 0x001
#define CONFIRM_ALL_NO 0x002
#define CONFIRM_SAME_YES 0x010
#define CONFIRM_SAME_NO 0x020

int confirm;
zip_flags_t name_flags;
int keep_stored;

static int confirm_replace(zip_t *, const char *, zip_uint64_t, zip_t *, const char *, zip_uint64_t);
static void copy_extra_fields(zip_t *destination_archive, zip_uint64_t destination_index, zip_t *source_archive, zip_uint64_t source_index, zip_flags_t flags);
static int copy_file(zip_t *destination_archive, zip_int64_t destination_index, zip_t *source_archive, zip_uint64_t source_index, const char* name);
static zip_t *merge_zip(zip_t *, const char *, const char *);


int
main(int argc, char *argv[]) {
    zip_t *za;
    zip_t **zs;
    int c, err;
    unsigned int i, n;
    char *tname;

    progname = argv[0];

    confirm = CONFIRM_ALL_YES;
    name_flags = 0;
    keep_stored = 0;

    while ((c = getopt(argc, argv, OPTIONS)) != -1) {
        switch (c) {
        case 'D':
            name_flags |= ZIP_FL_NODIR;
            break;
        case 'I':
            name_flags |= ZIP_FL_NOCASE;
            break;
        case 'i':
            confirm &= ~CONFIRM_ALL_YES;
            break;
        case 'k':
            keep_stored = 1;
            break;
        case 'S':
            confirm &= ~CONFIRM_SAME_YES;
            confirm |= CONFIRM_SAME_NO;
            break;
        case 's':
            confirm &= ~CONFIRM_SAME_NO;
            confirm |= CONFIRM_SAME_YES;
            break;

        case 'h':
            fputs(help_head, stdout);
            printf(USAGE, progname);
            fputs(help, stdout);
            exit(0);
        case 'V':
            fputs(version_string, stdout);
            exit(0);

        default:
            fprintf(stderr, USAGE, progname);
            exit(2);
        }
    }

    if (argc < optind + 2) {
        fprintf(stderr, USAGE, progname);
        exit(2);
    }

    tname = argv[optind++];
    argv += optind;

    n = (unsigned int)(argc - optind);
    if ((zs = (zip_t **)malloc(sizeof(zs[0]) * n)) == NULL) {
        fprintf(stderr, "%s: out of memory\n", progname);
        exit(1);
    }

    if ((za = zip_open(tname, ZIP_CREATE, &err)) == NULL) {
        zip_error_t error;
        zip_error_init_with_code(&error, err);
        fprintf(stderr, "%s: can't open zip archive '%s': %s\n", progname, tname, zip_error_strerror(&error));
        zip_error_fini(&error);
        exit(1);
    }

    for (i = 0; i < n; i++) {
        if ((zs[i] = merge_zip(za, tname, argv[i])) == NULL)
            exit(1);
    }

    if (zip_close(za) < 0) {
        fprintf(stderr, "%s: cannot write zip archive '%s': %s\n", progname, tname, zip_strerror(za));
        exit(1);
    }

    for (i = 0; i < n; i++)
        zip_close(zs[i]);

    exit(0);
}


static int
confirm_replace(zip_t *za, const char *tname, zip_uint64_t it, zip_t *zs, const char *sname, zip_uint64_t is) {
    char line[1024];
    struct zip_stat st, ss;

    if (confirm & CONFIRM_ALL_YES)
        return 1;
    else if (confirm & CONFIRM_ALL_NO)
        return 0;

    if (zip_stat_index(za, it, ZIP_FL_UNCHANGED, &st) < 0) {
        fprintf(stderr, "%s: cannot stat file %" PRIu64 " in '%s': %s\n", progname, it, tname, zip_strerror(za));
        return -1;
    }
    if (zip_stat_index(zs, is, 0, &ss) < 0) {
        fprintf(stderr, "%s: cannot stat file %" PRIu64 " in '%s': %s\n", progname, is, sname, zip_strerror(zs));
        return -1;
    }

    if (st.size == ss.size && st.crc == ss.crc) {
        if (confirm & CONFIRM_SAME_YES)
            return 1;
        else if (confirm & CONFIRM_SAME_NO)
            return 0;
    }

    printf("replace '%s' (%" PRIu64 " / %08x) in `%s'\n"
           "   with '%s' (%" PRIu64 " / %08x) from `%s'? ",
           st.name, st.size, st.crc, tname, ss.name, ss.size, ss.crc, sname);
    fflush(stdout);

    if (fgets(line, sizeof(line), stdin) == NULL) {
        fprintf(stderr, "%s: read error from stdin: %s\n", progname, strerror(errno));
        return -1;
    }

    if (tolower((unsigned char)line[0]) == 'y')
        return 1;

    return 0;
}


static zip_t *
merge_zip(zip_t *za, const char *tname, const char *sname) {
    zip_t *zs;
    zip_int64_t ret, idx;
    zip_uint64_t i;
    int err;
    const char *fname;

    if ((zs = zip_open(sname, 0, &err)) == NULL) {
        zip_error_t error;
        zip_error_init_with_code(&error, err);
        fprintf(stderr, "%s: can't open zip archive '%s': %s\n", progname, sname, zip_error_strerror(&error));
        zip_error_fini(&error);
        return NULL;
    }

    ret = zip_get_num_entries(zs, 0);
    if (ret < 0) {
        fprintf(stderr, "%s: cannot get number of entries for '%s': %s\n", progname, sname, zip_strerror(za));
        return NULL;
    }
    for (i = 0; i < (zip_uint64_t)ret; i++) {
        fname = zip_get_name(zs, i, 0);

        if ((idx = zip_name_locate(za, fname, name_flags)) >= 0) {
            switch (confirm_replace(za, tname, (zip_uint64_t)idx, zs, sname, i)) {
            case 0:
                break;

            case 1:
                if (copy_file(za, idx, zs, i, NULL) < 0) {
                    fprintf(stderr, "%s: cannot replace '%s' in `%s': %s\n", progname, fname, tname, zip_strerror(za));
                    zip_close(zs);
                    return NULL;
                }
                break;

            case -1:
                zip_close(zs);
                return NULL;

            default:
                fprintf(stderr,
                        "%s: internal error: "
                        "unexpected return code from confirm (%d)\n",
                        progname, err);
                zip_close(zs);
                return NULL;
            }
        }
        else {
            if (copy_file(za, -1, zs, i, fname) < 0) {
                fprintf(stderr, "%s: cannot add '%s' to `%s': %s\n", progname, fname, tname, zip_strerror(za));
                zip_close(zs);
                return NULL;
            }
        }
    }

    return zs;
}


static int copy_file(zip_t *destination_archive, zip_int64_t destination_index, zip_t *source_archive, zip_uint64_t source_index, const char* name) {
    zip_source_t *source = zip_source_zip(destination_archive, source_archive, source_index, 0, 0, 0);

    if (source == NULL) {
        return -1;
    }

    if (destination_index >= 0) {
        if (zip_replace(destination_archive, (zip_uint64_t)destination_index, source) < 0) {
            zip_source_free(source);
            return -1;
        }
    }
    else {
        destination_index = zip_file_add(destination_archive, name, source, 0);
        if (destination_index < 0) {
            zip_source_free(source);
            return -1;
        }
    }

    copy_extra_fields(destination_archive, (zip_uint64_t)destination_index, source_archive, source_index, ZIP_FL_CENTRAL);
    copy_extra_fields(destination_archive, (zip_uint64_t)destination_index, source_archive, source_index, ZIP_FL_LOCAL);
    if (keep_stored) {
        zip_stat_t st;
        if (zip_stat_index(source_archive, source_index, 0, &st) == 0 && (st.valid & ZIP_STAT_COMP_METHOD) && st.comp_method == ZIP_CM_STORE) {
            zip_set_file_compression(destination_archive, destination_index, ZIP_CM_STORE, 0);
        }
    }

    return 0;
}


static void copy_extra_fields(zip_t *destination_archive, zip_uint64_t destination_index, zip_t *source_archive, zip_uint64_t source_index, zip_flags_t flags) {
    zip_int16_t n;
    zip_uint16_t i, id, length;
    const zip_uint8_t *data;

    if ((n = zip_file_extra_fields_count(source_archive, source_index, flags)) < 0) {
        return;
    }

    for (i = 0; i < n; i++) {
        if ((data = zip_file_extra_field_get(source_archive, source_index, i, &id, &length, flags)) == NULL) {
            continue;
        }
        zip_file_extra_field_set(destination_archive, destination_index, id, ZIP_EXTRA_FIELD_NEW, data, length, flags);
    }
}
