/*
  zipcmp.c -- compare zip files
  Copyright (C) 2003-2022 Dieter Baron and Thomas Klausner

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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_FTS_H
#include <fts.h>
#endif
#include <zlib.h>

#ifndef HAVE_GETOPT
#include "getopt.h"
#endif

#include "zip.h"

#include "compat.h"

#include "diff_output.h"

struct archive {
    const char *name;
    zip_t *za;
    zip_uint64_t nentry;
    struct entry *entry;
    const char *comment;
    size_t comment_length;
};

struct ef {
    const char *name;
    zip_uint16_t flags;
    zip_uint16_t id;
    zip_uint16_t size;
    const zip_uint8_t *data;
};

struct entry {
    char *name;
    zip_uint64_t size;
    zip_uint32_t crc;
    zip_uint32_t comp_method;
    struct ef *extra_fields;
    zip_uint16_t n_extra_fields;
    const char *comment;
    zip_uint32_t comment_length;
};


typedef struct {
    uint32_t value;
    const char * const name;
} enum_map_t;

const enum_map_t comp_methods[] = {
    { 0, "Stored (no compression)" },
    { 1, "Shrunk" },
    { 2, "Reduced with compression factor 1" },
    { 3, "Reduced with compression factor 2" },
    { 4, "Reduced with compression factor 3" },
    { 5, "Reduced with compression factor 4" },
    { 6, "Imploded" },
    { 7, "Reserved for Tokenizing compression algorithm" },
    { 8, "Deflated" },
    { 9, "Enhanced Deflating using Deflate64(tm)" },
    { 10, "PKWARE Data Compression Library Imploding (old IBM TERSE)" },
    { 11, "11 (Reserved by PKWARE)" },
    { 12, "BZIP2" },
    { 13, "13 (Reserved by PKWARE)" },
    { 14, "LZMA (EFS)" },
    { 15, "15 (Reserved by PKWARE)" },
    { 16, "16 (Reserved by PKWARE)" },
    { 17, "17 (Reserved by PKWARE)" },
    { 18, "IBM TERSE (new)" },
    { 19, "IBM LZ77 z Architecture (PFS)" },
    { 20, "Zstandard compressed data (obsolete)" },
    { 93, "Zstandard compressed data" },
    { 95, "XZ compressed data" },
    { 97, "WavPack compressed data" },
    { 98, "PPMd version I, Rev 1" },
    { 99, "WinZIP AES Encryption" },
    { UINT32_MAX, NULL }
};

const enum_map_t extra_fields[] = {
    /* PKWARE defined */
    { 0x0001, "Zip64 extended information" },
    { 0x0007, "AV Info" },
    { 0x0008, "Reserved for extended language encoding data (PFS)" },
    { 0x0009, "OS/2" },
    { 0x000a, "NTFS" },
    { 0x000c, "OpenVMS" },
    { 0x000d, "UNIX" },
    { 0x000e, "Reserved for file stream and fork descriptors" },
    { 0x000f, "Patch Descriptor" },
    { 0x0014, "PKCS#7 Store for X.509 Certificates" },
    { 0x0015, "X.509 Certificate ID and Signature for individual file" },
    { 0x0016, "X.509 Certificate ID for Central Directory" },
    { 0x0017, "Strong Encryption Header" },
    { 0x0018, "Record Management Controls" },
    { 0x0019, "PKCS#7 Encryption Recipient Certificate List" },
    { 0x0065, "IBM S/390 (Z390), AS/400 (I400) attributes - uncompressed" },
    { 0x0066, "Reserved for IBM S/390 (Z390), AS/400 (I400) attributes - compressed" },
    { 0x4690, "POSZIP 4690 (reserved)" },

    /* Third-Party defined; see InfoZIP unzip sources proginfo/extrafld.txt */
    { 0x07c8, "Info-ZIP Macintosh (old)" },
    { 0x2605, "ZipIt Macintosh (first version)" },
    { 0x2705, "ZipIt Macintosh 1.3.5+ (w/o full filename)" },
    { 0x2805, "ZipIt Macintosh 1.3.5+" },
    { 0x334d, "Info-ZIP Macintosh (new)" },
    { 0x4154, "Tandem NSK" },
    { 0x4341, "Acorn/SparkFS" },
    { 0x4453, "Windows NT security descriptor" },
    { 0x4704, "VM/CMS" },
    { 0x470f, "MVS" },
    { 0x4854, "Theos, old unofficial port" },
    { 0x4b46, "FWKCS MD5" },
    { 0x4c41, "OS/2 access control list (text ACL)" },
    { 0x4d49, "Info-ZIP OpenVMS (obsolete)" },
    { 0x4d63, "Macintosh SmartZIP" },
    { 0x4f4c, "Xceed original location extra field" },
    { 0x5356, "AOS/VS (ACL)" },
    { 0x5455, "extended timestamp" },
    { 0x554e, "Xceed unicode extra field" },
    { 0x5855, "Info-ZIP UNIX (original)" },
    { 0x6375, "Info-ZIP UTF-8 comment field" },
    { 0x6542, "BeOS (BeBox, PowerMac, etc.)" },
    { 0x6854, "Theos" },
    { 0x7075, "Info-ZIP UTF-8 name field" },
    { 0x7441, "AtheOS (AtheOS/Syllable attributes)" },
    { 0x756e, "ASi UNIX" },
    { 0x7855, "Info-ZIP UNIX" },
    { 0x7875, "Info-ZIP UNIX 3rd generation" },
    { 0x9901, "WinZIP AES encryption" },
    { 0xa220, "Microsoft Open Packaging Growth Hint" },
    { 0xcafe, "executable Java JAR file" },
    { 0xfb4a, "SMS/QDOS" }, /* per InfoZIP extrafld.txt */
    { 0xfd4a, "SMS/QDOS" }, /* per appnote.txt */
    { UINT32_MAX, NULL }
};


const char *progname;

#define PROGRAM "zipcmp"

#define USAGE "usage: %s [-hipqtVv] archive1 archive2\n"

char help_head[] = PROGRAM " (" PACKAGE ") by Dieter Baron and Thomas Klausner\n\n";

char help[] = "\n\
  -h       display this help message\n\
  -C       check archive consistencies\n\
  -i       compare names ignoring case distinctions\n\
  -p       compare as many details as possible\n\
  -q       be quiet\n\
  -s       print a summary\n\
  -t       test zip files (compare file contents to checksum)\n\
  -V       display version number\n\
  -v       be verbose (print differences, default)\n\
\n\
Report bugs to <info@libzip.org>.\n";

char version_string[] = PROGRAM " (" PACKAGE " " VERSION ")\n\
Copyright (C) 2003-2022 Dieter Baron and Thomas Klausner\n\
" PACKAGE " comes with ABSOLUTELY NO WARRANTY, to the extent permitted by law.\n";

#define OPTIONS "hVCipqstv"


#define BOTH_ARE_ZIPS(a) (a[0].za && a[1].za)

static int comment_compare(const char *c1, size_t l1, const char *c2, size_t l2);
static int compare_list(char *const name[2], const void *list[2], const zip_uint64_t list_length[2], int element_size, int (*cmp)(const void *a, const void *b), int (*ignore)(const void *list, int last, const void *other), int (*check)(char *const name[2], const void *a, const void *b), void (*print)(char side, const void *element), void (*start_file)(const void *element));
static int compare_zip(char *const zn[]);
static int ef_compare(char *const name[2], const struct entry *e1, const struct entry *e2);
static int ef_order(const void *a, const void *b);
static void ef_print(char side, const void *p);
static int ef_read(zip_t *za, zip_uint64_t idx, struct entry *e);
static int entry_cmp(const void *p1, const void *p2);
static int entry_ignore(const void *p1, int last, const void *o);
static int entry_paranoia_checks(char *const name[2], const void *p1, const void *p2);
static void entry_print(char side, const void *p);
static void entry_start_file(const void *p);
static const char *map_enum(const enum_map_t *map, uint32_t value);

static int is_directory(const char *name);
#ifdef HAVE_FTS_H
static int list_directory(const char *name, struct archive *a);
#endif
static int list_zip(const char *name, struct archive *a);
static int test_file(zip_t *za, zip_uint64_t idx, const char *zipname, const char *filename, zip_uint64_t size, zip_uint32_t crc);

int ignore_case, test_files, paranoid, verbose, have_directory, check_consistency, summary;
int plus_count = 0, minus_count = 0;

diff_output_t output;


int
main(int argc, char *const argv[]) {
    int c;

    progname = argv[0];

    ignore_case = 0;
    test_files = 0;
    check_consistency = 0;
    paranoid = 0;
    have_directory = 0;
    verbose = 1;
    summary = 0;

    while ((c = getopt(argc, argv, OPTIONS)) != -1) {
        switch (c) {
            case 'C':
                check_consistency = 1;
                break;
            case 'i':
                ignore_case = 1;
                break;
            case 'p':
                paranoid = 1;
                break;
            case 'q':
                verbose = 0;
                break;
	    case 's':
		summary = 1;
		break;
            case 't':
                test_files = 1;
                break;
            case 'v':
                verbose = 1;
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

    if (argc != optind + 2) {
        fprintf(stderr, USAGE, progname);
        exit(2);
    }

    exit((compare_zip(argv + optind) == 0) ? 0 : 1);
}


static int
compare_zip(char *const zn[]) {
    struct archive a[2];
    struct entry *e[2];
    zip_uint64_t n[2];
    int i;
    int res;

    for (i = 0; i < 2; i++) {
        a[i].name = zn[i];
        a[i].entry = NULL;
        a[i].nentry = 0;
        a[i].za = NULL;
        a[i].comment = NULL;
        a[i].comment_length = 0;

        if (is_directory(zn[i])) {
#ifndef HAVE_FTS_H
            fprintf(stderr, "%s: reading directories not supported\n", progname);
            exit(2);
#else
            if (list_directory(zn[i], a + i) < 0)
                exit(2);
            have_directory = 1;
            paranoid = 0; /* paranoid checks make no sense for directories, since they compare zip metadata */
#endif
        }
        else {
            if (list_zip(zn[i], a + i) < 0)
                exit(2);
        }
        if (a[i].nentry > 0)
            qsort(a[i].entry, a[i].nentry, sizeof(a[i].entry[0]), entry_cmp);
    }

    diff_output_init(&output, verbose, zn);

    e[0] = a[0].entry;
    e[1] = a[1].entry;
    n[0] = a[0].nentry;
    n[1] = a[1].nentry;
    res = compare_list(zn, (const void **)e, n, sizeof(e[i][0]), entry_cmp, have_directory ? entry_ignore : NULL, paranoid ? entry_paranoia_checks : NULL, entry_print, entry_start_file);

    if (paranoid) {
        if (comment_compare(a[0].comment, a[0].comment_length, a[1].comment, a[1].comment_length) != 0) {
            if (a[0].comment_length > 0) {
                diff_output_data(&output, '-', (const zip_uint8_t *)a[0].comment, a[0].comment_length, "archive comment");
		minus_count++;
            }
            if (a[1].comment_length > 0) {
                diff_output_data(&output, '+', (const zip_uint8_t *)a[1].comment, a[1].comment_length, "archive comment");
		plus_count++;
            }
            res = 1;
        }
    }

    for (i = 0; i < 2; i++) {
        zip_uint64_t j;

        if (a[i].za) {
            zip_close(a[i].za);
        }
        for (j = 0; j < a[i].nentry; j++) {
            free(a[i].entry[j].name);
        }
        free(a[i].entry);
    }

    if (summary) {
	printf("%d files removed, %d files added\n", minus_count, plus_count);
    }

    switch (res) {
    case 0:
        exit(0);

    case 1:
        exit(1);

    default:
        exit(2);
    }
}

#ifdef HAVE_FTS_H
static zip_int64_t
compute_crc(const char *fname) {
    FILE *f;
    uLong crc = crc32(0L, Z_NULL, 0);
    size_t n;
    Bytef buffer[8192];


    if ((f = fopen(fname, "rb")) == NULL) {
        fprintf(stderr, "%s: can't open %s: %s\n", progname, fname, strerror(errno));
        return -1;
    }

    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        crc = crc32(crc, buffer, (unsigned int)n);
    }

    if (ferror(f)) {
        fprintf(stderr, "%s: read error on %s: %s\n", progname, fname, strerror(errno));
        fclose(f);
        return -1;
    }

    fclose(f);

    return (zip_int64_t)crc;
}
#endif


static int
is_directory(const char *name) {
    struct stat st;

    if (stat(name, &st) < 0)
        return 0;

    return S_ISDIR(st.st_mode);
}


#ifdef HAVE_FTS_H
static int
list_directory(const char *name, struct archive *a) {
    FTS *fts;
    FTSENT *ent;
    zip_uint64_t nalloc;
    size_t prefix_length;

    char *const names[2] = {(char *)name, NULL};


    if ((fts = fts_open(names, FTS_NOCHDIR | FTS_LOGICAL, NULL)) == NULL) {
        fprintf(stderr, "%s: can't open directory '%s': %s\n", progname, name, strerror(errno));
        return -1;
    }
    prefix_length = strlen(name) + 1;

    nalloc = 0;

    while ((ent = fts_read(fts))) {
        zip_int64_t crc;

        switch (ent->fts_info) {
            case FTS_DOT:
            case FTS_DP:
            case FTS_DEFAULT:
            case FTS_SL:
            case FTS_NSOK:
                break;

            case FTS_DC:
            case FTS_DNR:
            case FTS_ERR:
            case FTS_NS:
            case FTS_SLNONE:
                /* TODO: error */
                fts_close(fts);
                return -1;

            case FTS_D:
            case FTS_F:
                if (a->nentry >= nalloc) {
                    nalloc += 16;
                    if (nalloc > SIZE_MAX / sizeof(a->entry[0])) {
                        fprintf(stderr, "%s: malloc failure\n", progname);
                        exit(1);
                    }
                    a->entry = realloc(a->entry, sizeof(a->entry[0]) * nalloc);
                    if (a->entry == NULL) {
                        fprintf(stderr, "%s: malloc failure\n", progname);
                        exit(1);
                    }
                }
                
                if (ent->fts_info == FTS_D) {
                    char *dir_name;
                    
                    if (ent->fts_path[prefix_length - 1] == '\0') {
                        break;
                    }
                    
                    size_t dir_name_size = strlen(ent->fts_path + prefix_length) + 2;
                    dir_name = malloc(dir_name_size);
                    if (dir_name == NULL) {
                        fprintf(stderr, "%s: malloc failure\n", progname);
                        exit(1);
                    }
                    snprintf(dir_name, dir_name_size, "%s/", ent->fts_path + prefix_length);
                    a->entry[a->nentry].name = dir_name;
                    a->entry[a->nentry].size = 0;
                    a->entry[a->nentry].crc = 0;
                }
                else {
                    a->entry[a->nentry].name = strdup(ent->fts_path + prefix_length);
                    a->entry[a->nentry].size = (zip_uint64_t)ent->fts_statp->st_size;
                    if ((crc = compute_crc(ent->fts_accpath)) < 0) {
                        fts_close(fts);
                        return -1;
                    }
                
                    a->entry[a->nentry].crc = (zip_uint32_t)crc;
                }
                a->nentry++;
                break;
        }
    }

    if (fts_close(fts)) {
        fprintf(stderr, "%s: error closing directory '%s': %s\n", progname, a->name, strerror(errno));
        return -1;
    }

    return 0;
}
#endif


static int
list_zip(const char *name, struct archive *a) {
    zip_t *za;
    int err;
    struct zip_stat st;
    unsigned int i;

    if ((za = zip_open(name, check_consistency ? ZIP_CHECKCONS : 0, &err)) == NULL) {
        zip_error_t error;
        zip_error_init_with_code(&error, err);
        fprintf(stderr, "%s: cannot open zip archive '%s': %s\n", progname, name, zip_error_strerror(&error));
        zip_error_fini(&error);
        return -1;
    }

    a->za = za;
    a->nentry = (zip_uint64_t)zip_get_num_entries(za, 0);

    if (a->nentry == 0)
        a->entry = NULL;
    else {
        if ((a->nentry > SIZE_MAX / sizeof(a->entry[0])) || (a->entry = (struct entry *)malloc(sizeof(a->entry[0]) * a->nentry)) == NULL) {
            fprintf(stderr, "%s: malloc failure\n", progname);
            exit(1);
        }

        for (i = 0; i < a->nentry; i++) {
            zip_stat_index(za, i, 0, &st);
            a->entry[i].name = strdup(st.name);
            a->entry[i].size = st.size;
            a->entry[i].crc = st.crc;
            if (test_files)
                test_file(za, i, name, st.name, st.size, st.crc);
            if (paranoid) {
                a->entry[i].comp_method = st.comp_method;
                ef_read(za, i, a->entry + i);
                a->entry[i].comment = zip_file_get_comment(za, i, &a->entry[i].comment_length, 0);
            }
            else {
                a->entry[i].comp_method = 0;
                a->entry[i].n_extra_fields = 0;
            }
        }

        if (paranoid) {
            int length;
            a->comment = zip_get_archive_comment(za, &length, 0);
            a->comment_length = (size_t)length;
        }
        else {
            a->comment = NULL;
            a->comment_length = 0;
        }
    }

    return 0;
}


static int
comment_compare(const char *c1, size_t l1, const char *c2, size_t l2) {
    if (l1 != l2)
        return 1;

    if (l1 == 0)
        return 0;

    if (c1 == NULL || c2 == NULL)
        return c1 == c2;

    return memcmp(c1, c2, (size_t)l2);
}


static int compare_list(char *const name[2], const void *list[2], const zip_uint64_t list_length[2], int element_size, int (*cmp)(const void *a, const void *b), int (*ignore)(const void *list, int last, const void *other), int (*check)(char *const name[2], const void *a, const void *b), void (*print)(char side, const void *element), void (*start_file)(const void *element)) {
    unsigned int i[2];
    int j;
    int diff;

#define INC(k) (i[k]++, list[k] = ((const char *)list[k]) + element_size)
#define PRINT(k)                                          \
    do {                                                  \
        if (ignore && ignore(list[k], i[k] >= list_length[k] - 1, i[1-k] < list_length[1-k] ? list[1-k] : NULL)) {   \
            break;                                        \
        }                                                 \
        print((k) ? '+' : '-', list[k]);                  \
        (k) ? plus_count++ : minus_count++;		  \
        diff = 1;                                         \
    } while (0)

    i[0] = i[1] = 0;
    diff = 0;
    while (i[0] < list_length[0] && i[1] < list_length[1]) {
        int c = cmp(list[0], list[1]);

        if (c == 0) {
            if (check) {
                if (start_file) {
                    start_file(list[0]);
                }
                diff |= check(name, list[0], list[1]);
                if (start_file) {
                    diff_output_end_file(&output);
                }
            }
            INC(0);
            INC(1);
        }
        else if (c < 0) {
            PRINT(0);
            INC(0);
        }
        else {
            PRINT(1);
            INC(1);
        }
    }

    for (j = 0; j < 2; j++) {
        while (i[j] < list_length[j]) {
            PRINT(j);
            INC(j);
        }
    }

    return diff;
}


static int
ef_read(zip_t *za, zip_uint64_t idx, struct entry *e) {
    zip_int16_t n_local, n_central;
    zip_uint16_t i;

    if ((n_local = zip_file_extra_fields_count(za, idx, ZIP_FL_LOCAL)) < 0 || (n_central = zip_file_extra_fields_count(za, idx, ZIP_FL_CENTRAL)) < 0) {
        return -1;
    }

    e->n_extra_fields = (zip_uint16_t)(n_local + n_central);

    if ((e->extra_fields = (struct ef *)malloc(sizeof(e->extra_fields[0]) * e->n_extra_fields)) == NULL)
        return -1;

    for (i = 0; i < n_local; i++) {
        e->extra_fields[i].name = e->name;
        e->extra_fields[i].data = zip_file_extra_field_get(za, idx, i, &e->extra_fields[i].id, &e->extra_fields[i].size, ZIP_FL_LOCAL);
        if (e->extra_fields[i].data == NULL)
            return -1;
        e->extra_fields[i].flags = ZIP_FL_LOCAL;
    }
    for (; i < e->n_extra_fields; i++) {
        e->extra_fields[i].name = e->name;
        e->extra_fields[i].data = zip_file_extra_field_get(za, idx, (zip_uint16_t)(i - n_local), &e->extra_fields[i].id, &e->extra_fields[i].size, ZIP_FL_CENTRAL);
        if (e->extra_fields[i].data == NULL)
            return -1;
        e->extra_fields[i].flags = ZIP_FL_CENTRAL;
    }

    qsort(e->extra_fields, e->n_extra_fields, sizeof(e->extra_fields[0]), ef_order);

    return 0;
}


static int
ef_compare(char *const name[2], const struct entry *e1, const struct entry *e2) {
    struct ef *ef[2];
    zip_uint64_t n[2];

    ef[0] = e1->extra_fields;
    ef[1] = e2->extra_fields;
    n[0] = e1->n_extra_fields;
    n[1] = e2->n_extra_fields;

    return compare_list(name, (const void **)ef, n, sizeof(struct ef), ef_order, NULL, NULL, ef_print, NULL);
}


static int
ef_order(const void *ap, const void *bp) {
    const struct ef *a, *b;

    a = (struct ef *)ap;
    b = (struct ef *)bp;

    if (a->flags != b->flags)
        return a->flags - b->flags;
    if (a->id != b->id)
        return a->id - b->id;
    if (a->size != b->size)
        return a->size - b->size;
    return memcmp(a->data, b->data, a->size);
}


static void
ef_print(char side, const void *p) {
    const struct ef *ef = (struct ef *)p;

    diff_output_data(&output, side, ef->data, ef->size, "  %s extra field %s", ef->flags == ZIP_FL_LOCAL ? "local" : "central", map_enum(extra_fields, ef->id));
}


static int
entry_cmp(const void *p1, const void *p2) {
    const struct entry *e1, *e2;
    int c;

    e1 = (struct entry *)p1;
    e2 = (struct entry *)p2;

    if ((c = (ignore_case ? strcasecmp : strcmp)(e1->name, e2->name)) != 0)
        return c;
    if (e1->size != e2->size) {
        if (e1->size > e2->size)
            return 1;
        else
            return -1;
    }
    if (e1->crc != e2->crc)
        return (int)e1->crc - (int)e2->crc;

    return 0;
}


static int
entry_ignore(const void *p, int last, const void *o) {
    const struct entry *e = (const struct entry *)p;
    const struct entry *other = (const struct entry *)o;
    
    size_t length = strlen(e[0].name);
    
    if (length == 0 || e[0].name[length - 1] != '/') {
        /* not a directory */
        return 0;
    }
    
    if (other != NULL && strlen(other->name) > length && strncmp(other->name, e[0].name, length) == 0) {
        /* not empty in other archive */
        return 1;
    }
    
    if (last || (strlen(e[1].name) < length || strncmp(e[0].name, e[1].name, length) != 0)) {
        /* empty in this archive */
        return 0;
    }
    
    /* not empty in this archive */
    return 1;
}


static int
entry_paranoia_checks(char *const name[2], const void *p1, const void *p2) {
    const struct entry *e1, *e2;
    int ret;

    e1 = (struct entry *)p1;
    e2 = (struct entry *)p2;

    ret = 0;

    if (e1->comp_method != e2->comp_method) {
        diff_output(&output, '-', "  compression method %s", map_enum(comp_methods, e1->comp_method));
        diff_output(&output, '+', "  compression method %s", map_enum(comp_methods, e2->comp_method));
        ret = 1;
    }

    if (ef_compare(name, e1, e2) != 0) {
        ret = 1;
    }

    if (comment_compare(e1->comment, e1->comment_length, e2->comment, e2->comment_length) != 0) {
        diff_output_data(&output, '-', (const zip_uint8_t *)e1->comment, e1->comment_length, "  comment");
        diff_output_data(&output, '+', (const zip_uint8_t *)e2->comment, e2->comment_length, "  comment");
        ret = 1;
    }

    return ret;
}


static void entry_print(char side, const void *p) {
    const struct entry *e = (struct entry *)p;

    diff_output_file(&output, side, e->name, e->size, e->crc);
}


static void entry_start_file(const void *p) {
    const struct entry *e = (struct entry *)p;
    
    diff_output_start_file(&output, e->name, e->size, e->crc);
}


static int
test_file(zip_t *za, zip_uint64_t idx, const char *zipname, const char *filename, zip_uint64_t size, zip_uint32_t crc) {
    zip_file_t *zf;
    char buf[8192];
    zip_uint64_t nsize;
    zip_int64_t n;
    zip_uint32_t ncrc;

    if ((zf = zip_fopen_index(za, idx, 0)) == NULL) {
        fprintf(stderr, "%s: %s: cannot open file %s (index %" PRIu64 "): %s\n", progname, zipname, filename, idx, zip_strerror(za));
        return -1;
    }

    ncrc = (zip_uint32_t)crc32(0, NULL, 0);
    nsize = 0;

    while ((n = zip_fread(zf, buf, sizeof(buf))) > 0) {
        nsize += (zip_uint64_t)n;
        ncrc = (zip_uint32_t)crc32(ncrc, (const Bytef *)buf, (unsigned int)n);
    }

    if (n < 0) {
        fprintf(stderr, "%s: %s: error reading file %s (index %" PRIu64 "): %s\n", progname, zipname, filename, idx, zip_file_strerror(zf));
        zip_fclose(zf);
        return -1;
    }

    zip_fclose(zf);

    if (nsize != size) {
        fprintf(stderr, "%s: %s: file %s (index %" PRIu64 "): unexpected length %" PRId64 " (should be %" PRId64 ")\n", progname, zipname, filename, idx, nsize, size);
        return -2;
    }
    if (ncrc != crc) {
        fprintf(stderr, "%s: %s: file %s (index %" PRIu64 "): unexpected length %x (should be %x)\n", progname, zipname, filename, idx, ncrc, crc);
        return -2;
    }

    return 0;
}


static const char *map_enum(const enum_map_t *map, uint32_t value) {
    static char unknown[16];
    size_t i = 0;
    
    while (map[i].value < UINT32_MAX) {
        if (map[i].value == value) {
            return map[i].name;
        }
        i++;
    }
    
    snprintf(unknown, sizeof(unknown), "unknown (%u)", value);
    unknown[sizeof(unknown) - 1] = '\0';
    
    return unknown;
}
