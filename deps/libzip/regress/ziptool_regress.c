#include "zip.h"

#include <sys/stat.h>

#define ZIP_MIN(a, b) ((a) < (b) ? (a) : (b))

#define FOR_REGRESS

typedef enum { SOURCE_TYPE_NONE, SOURCE_TYPE_IN_MEMORY, SOURCE_TYPE_HOLE } source_type_t;

source_type_t source_type = SOURCE_TYPE_NONE;
zip_uint64_t fragment_size = 0;
zip_file_t *z_files[16];
unsigned int z_files_count;

static int add_nul(char *argv[]);
static int cancel(char *argv[]);
static int regress_fopen(char *argv[]);
static int regress_fread(char *argv[]);
static int regress_fseek(char *argv[]);
static int is_seekable(char *argv[]);
static int unchange_one(char *argv[]);
static int unchange_all(char *argv[]);
static int zin_close(char *argv[]);

#define OPTIONS_REGRESS "F:Hm"

#define USAGE_REGRESS " [-Hm] [-F fragment-size]"

#define GETOPT_REGRESS                              \
    case 'H':                                       \
        source_type = SOURCE_TYPE_HOLE;             \
        break;                                      \
    case 'm':                                       \
        source_type = SOURCE_TYPE_IN_MEMORY;        \
        break;                                      \
    case 'F':                                       \
        fragment_size = strtoull(optarg, NULL, 10); \
        break;

/* clang-format off */

#define DISPATCH_REGRESS \
    {"add_nul", 2, "name length", "add NUL bytes", add_nul}, \
    {"cancel", 1, "limit", "cancel writing archive when limit% have been written (calls print_progress)", cancel}, \
    {"fopen", 1, "name", "open archive entry", regress_fopen}, \
    {"fread", 2, "file_index length", "read from fopened file and print", regress_fread}, \
    {"fseek", 3, "file_index offset whence", "seek in fopened file", regress_fseek}, \
    {"is_seekable", 1, "index", "report if entry is seekable", is_seekable}, \
    {"unchange", 1, "index", "revert changes for entry", unchange_one}, \
    {"unchange_all", 0, "", "revert all changes", unchange_all}, \
    {"zin_close", 1, "index", "close input zip_source (for internal tests)", zin_close}

#define PRECLOSE_REGRESS                                         \
  do {                                                           \
      unsigned int file_idx = 0;                                 \
      for (file_idx = 0; file_idx < z_files_count; ++file_idx) { \
          if (zip_fclose (z_files[file_idx]) != 0) {             \
              err = 1;                                           \
          }                                                      \
      }                                                          \
  }                                                              \
  while (0)

/* clang-format on */


zip_t *ziptool_open(const char *archive, int flags, zip_error_t *error, zip_uint64_t offset, zip_uint64_t len);


#include "ziptool.c"


zip_source_t *memory_src = NULL;

static int get_whence(const char *str);
zip_source_t *source_hole_create(const char *, int flags, zip_error_t *);
static zip_t *read_to_memory(const char *archive, int flags, zip_error_t *error, zip_source_t **srcp);
static zip_source_t *source_nul(zip_t *za, zip_uint64_t length);


static int
add_nul(char *argv[]) {
    zip_source_t *zs;
    zip_uint64_t length = strtoull(argv[1], NULL, 10);

    if ((zs = source_nul(za, length)) == NULL) {
        fprintf(stderr, "can't create zip_source for length: %s\n", zip_strerror(za));
        return -1;
    }

    if (zip_add(za, argv[0], zs) == -1) {
        zip_source_free(zs);
        fprintf(stderr, "can't add file '%s': %s\n", argv[0], zip_strerror(za));
        return -1;
    }
    return 0;
}

static int
cancel_callback(zip_t *archive, void *ud) {
    if (progress_userdata.percentage >= progress_userdata.limit) {
        return -1;
    }
    return 0;
}

static int
cancel(char *argv[]) {
    zip_int64_t percent;
    percent = strtoll(argv[0], NULL, 10);
    if (percent > 100 || percent < 0) {
        fprintf(stderr, "invalid percentage '%" PRId64 "' for cancel (valid: 0 <= x <= 100)\n", percent);
        return -1;
    }
    progress_userdata.limit = ((double)percent) / 100;

    zip_register_cancel_callback_with_state(za, cancel_callback, NULL, NULL);

    /* needs the percentage updates from print_progress */
    print_progress(argv);
    return 0;
}

static int
is_seekable(char *argv[]) {
    zip_uint64_t idx;
    zip_file_t *zf;

    idx = strtoull(argv[0], NULL, 10);
    if ((zf = zip_fopen_index(za, idx, 0)) == NULL) {
        fprintf(stderr, "can't open file at index '%" PRIu64 "': %s\n", idx, zip_strerror(za));
        return -1;
    }
    switch (zip_file_is_seekable(zf)) {
    case -1:
	fprintf(stderr, "can't check if file %" PRIu64 " is seekable: %s\n", idx, zip_strerror(za));
	return -1;
    case 0:
	printf("%" PRIu64 ": NOT seekable\n", idx);
	break;
    case 1:
	printf("%" PRIu64 ": seekable\n", idx);
	break;
    }
    return 0;
}

static int
regress_fseek(char *argv[]) {
    zip_uint64_t file_idx;
    zip_file_t *zf;
    zip_int64_t offset;
    int whence;

    file_idx = strtoull(argv[0], NULL, 10);
    offset = strtoll(argv[1], NULL, 10);
    whence = get_whence(argv[2]);
    if (file_idx >= z_files_count || z_files[file_idx] == NULL) {
        fprintf(stderr, "trying to seek in invalid opened file\n");
        return -1;
    }
    zf = z_files[file_idx];

    if (zip_fseek(zf, offset, whence) == -1) {
	fprintf(stderr, "can't seek in file %" PRIu64 ": %s\n", file_idx, zip_strerror(za));
	return -1;
    }
    return 0;
}

static int
unchange_all(char *argv[]) {
    if (zip_unchange_all(za) < 0) {
        fprintf(stderr, "can't revert changes to archive: %s\n", zip_strerror(za));
        return -1;
    }
    return 0;
}


static int
unchange_one(char *argv[]) {
    zip_uint64_t idx;

    idx = strtoull(argv[0], NULL, 10);

    if (zip_unchange(za, idx) < 0) {
	fprintf(stderr, "can't revert changes for entry %" PRIu64 ": %s", idx, zip_strerror(za));
	return -1;
    }

    return 0;
}

static int
zin_close(char *argv[]) {
    zip_uint64_t idx;

    idx = strtoull(argv[0], NULL, 10);
    if (idx >= z_in_count) {
        fprintf(stderr, "invalid argument '%" PRIu64 "', only %u zip sources open\n", idx, z_in_count);
        return -1;
    }
    if (zip_close(z_in[idx]) < 0) {
        fprintf(stderr, "can't close source archive: %s\n", zip_strerror(z_in[idx]));
        return -1;
    }
    z_in[idx] = z_in[z_in_count];
    z_in_count--;

    return 0;
}

static int
regress_fopen(char *argv[]) {
    if (z_files_count >= (sizeof(z_files) / sizeof(*z_files))) {
        fprintf(stderr, "too many open files\n");
        return -1;
    }
    if ((z_files[z_files_count] = zip_fopen(za, argv[0], 0)) == NULL) {
        fprintf(stderr, "can't open entry '%s' from input archive: %s\n", argv[0], zip_strerror(za));
        return -1;
    }
    printf("opened '%s' as file %u\n", argv[0], z_files_count);
    z_files_count += 1;
    return 0;
}


static int
regress_fread(char *argv[]) {
    zip_uint64_t file_idx;
    zip_uint64_t length;
    char buf[8192];
    zip_int64_t n;
    zip_file_t *f;

    file_idx = strtoull(argv[0], NULL, 10);
    length = strtoull(argv[1], NULL, 10);

    if (file_idx >= z_files_count || z_files[file_idx] == NULL) {
        fprintf(stderr, "trying to read from invalid opened file\n");
        return -1;
    }
    f = z_files[file_idx];
    while (length > 0) {
        zip_uint64_t to_read;

        if (length > sizeof (buf)) {
            to_read = sizeof (buf);
        } else {
            to_read = length;
        }
        n = zip_fread(f, buf, to_read);
        if (n < 0) {
            fprintf(stderr, "can't read opened file %" PRIu64 ": %s\n", file_idx, zip_file_strerror(f));
            return -1;
        }
        if (n == 0) {
#if 0
            fprintf(stderr, "premature end of opened file %" PRIu64 "\n", file_idx);
            return -1;
#else
            break;
#endif
        }
        if (fwrite(buf, (size_t)n, 1, stdout) != 1) {
            fprintf(stderr, "can't write file contents to stdout: %s\n", strerror(errno));
            return -1;
        }
        length -= n;
    }
    return 0;
}


static zip_t *
read_hole(const char *archive, int flags, zip_error_t *error) {
    zip_source_t *src = NULL;
    zip_t *zs = NULL;

    if (strcmp(archive, "/dev/stdin") == 0) {
        zip_error_set(error, ZIP_ER_OPNOTSUPP, 0);
        return NULL;
    }

    if ((src = source_hole_create(archive, flags, error)) == NULL || (zs = zip_open_from_source(src, flags, error)) == NULL) {
        zip_source_free(src);
    }

    return zs;
}


static int get_whence(const char *str) {
    if (strcasecmp(str, "set") == 0) {
        return SEEK_SET;
    }
    else if (strcasecmp(str, "cur") == 0) {
        return SEEK_CUR;
    }
    else if (strcasecmp(str, "end") == 0) {
        return SEEK_END;
    }
    else {
        return 100; /* invalid */
    }
}


static zip_t *
read_to_memory(const char *archive, int flags, zip_error_t *error, zip_source_t **srcp) {
    zip_source_t *src;
    zip_t *zb;
    FILE *fp;

    if (strcmp(archive, "/dev/stdin") == 0) {
        zip_error_set(error, ZIP_ER_OPNOTSUPP, 0);
        return NULL;
    }

    if ((fp = fopen(archive, "rb")) == NULL) {
        if (errno == ENOENT) {
            src = zip_source_buffer_create(NULL, 0, 0, error);
        }
        else {
            zip_error_set(error, ZIP_ER_OPEN, errno);
            return NULL;
        }
    }
    else {
        struct stat st;

        if (fstat(fileno(fp), &st) < 0) {
            fclose(fp);
            zip_error_set(error, ZIP_ER_OPEN, errno);
            return NULL;
        }
        if (fragment_size == 0) {
            char *buf;
            if ((buf = malloc((size_t)st.st_size)) == NULL) {
                fclose(fp);
                zip_error_set(error, ZIP_ER_MEMORY, 0);
                return NULL;
            }
            if (fread(buf, (size_t)st.st_size, 1, fp) < 1) {
                free(buf);
                fclose(fp);
                zip_error_set(error, ZIP_ER_READ, errno);
                return NULL;
            }
            src = zip_source_buffer_create(buf, (zip_uint64_t)st.st_size, 1, error);
            if (src == NULL) {
                free(buf);
            }
        }
        else {
            zip_uint64_t nfragments, i, left;
            zip_buffer_fragment_t *fragments;

            nfragments = ((size_t)st.st_size + fragment_size - 1) / fragment_size;
            if ((fragments = malloc(sizeof(fragments[0]) * nfragments)) == NULL) {
                fclose(fp);
                zip_error_set(error, ZIP_ER_MEMORY, 0);
                return NULL;
            }
            for (i = 0; i < nfragments; i++) {
                left = ZIP_MIN(fragment_size, (size_t)st.st_size - i * fragment_size);
                if ((fragments[i].data = malloc(left)) == NULL) {
#ifndef __clang_analyzer__
                    /* fragments is initialized up to i - 1*/
                    while (--i > 0) {
                        free(fragments[i].data);
                    }
#endif
                    free(fragments);
                    fclose(fp);
                    zip_error_set(error, ZIP_ER_MEMORY, 0);
                    return NULL;
                }
                fragments[i].length = left;
                if (fread(fragments[i].data, left, 1, fp) < 1) {
#ifndef __clang_analyzer__
                    /* fragments is initialized up to i - 1*/
                    while (--i > 0) {
                        free(fragments[i].data);
                    }
#endif
                    free(fragments);
                    fclose(fp);
                    zip_error_set(error, ZIP_ER_READ, errno);
                    return NULL;
                }
            }
            src = zip_source_buffer_fragment_create(fragments, nfragments, 1, error);
            if (src == NULL) {
                for (i = 0; i < nfragments; i++) {
                    free(fragments[i].data);
                }
                free(fragments);
                fclose(fp);
                return NULL;
            }
            free(fragments);
        }
        fclose(fp);
    }
    if (src == NULL) {
        return NULL;
    }
    zb = zip_open_from_source(src, flags, error);
    if (zb == NULL) {
        zip_source_free(src);
        return NULL;
    }
    zip_source_keep(src);
    *srcp = src;
    return zb;
}


typedef struct source_nul {
    zip_error_t error;
    zip_uint64_t length;
    zip_uint64_t offset;
} source_nul_t;

static zip_int64_t
source_nul_cb(void *ud, void *data, zip_uint64_t length, zip_source_cmd_t command) {
    source_nul_t *ctx = (source_nul_t *)ud;

    switch (command) {
    case ZIP_SOURCE_CLOSE:
        return 0;

    case ZIP_SOURCE_ERROR:
        return zip_error_to_data(&ctx->error, data, length);

    case ZIP_SOURCE_FREE:
        free(ctx);
        return 0;

    case ZIP_SOURCE_OPEN:
        ctx->offset = 0;
        return 0;

    case ZIP_SOURCE_READ:
        if (length > ZIP_INT64_MAX) {
            zip_error_set(&ctx->error, ZIP_ER_INVAL, 0);
            return -1;
        }

        if (length > ctx->length - ctx->offset) {
            length = ctx->length - ctx->offset;
        }

        memset(data, 0, length);
        ctx->offset += length;
        return (zip_int64_t)length;

    case ZIP_SOURCE_STAT: {
        zip_stat_t *st = ZIP_SOURCE_GET_ARGS(zip_stat_t, data, length, &ctx->error);

        if (st == NULL) {
            return -1;
        }

        st->valid |= ZIP_STAT_SIZE;
        st->size = ctx->length;

        return 0;
    }

    case ZIP_SOURCE_SUPPORTS:
        return zip_source_make_command_bitmap(ZIP_SOURCE_CLOSE, ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_STAT, -1);

    default:
        zip_error_set(&ctx->error, ZIP_ER_OPNOTSUPP, 0);
        return -1;
    }
}

static zip_source_t *
source_nul(zip_t *zs, zip_uint64_t length) {
    source_nul_t *ctx;
    zip_source_t *src;

    if ((ctx = (source_nul_t *)malloc(sizeof(*ctx))) == NULL) {
        zip_error_set(zip_get_error(zs), ZIP_ER_MEMORY, 0);
        return NULL;
    }

    zip_error_init(&ctx->error);
    ctx->length = length;
    ctx->offset = 0;

    if ((src = zip_source_function(zs, source_nul_cb, ctx)) == NULL) {
        free(ctx);
        return NULL;
    }

    return src;
}


static int
write_memory_src_to_file(const char *archive, zip_source_t *src) {
    zip_stat_t zst;
    char *buf;
    FILE *fp;

    if (zip_source_stat(src, &zst) < 0) {
        fprintf(stderr, "zip_source_stat on buffer failed: %s\n", zip_error_strerror(zip_source_error(src)));
        return -1;
    }
    if (zip_source_open(src) < 0) {
        if (zip_error_code_zip(zip_source_error(src)) == ZIP_ER_DELETED) {
            if (unlink(archive) < 0 && errno != ENOENT) {
                fprintf(stderr, "unlink failed: %s\n", strerror(errno));
                return -1;
            }
            return 0;
        }
        fprintf(stderr, "zip_source_open on buffer failed: %s\n", zip_error_strerror(zip_source_error(src)));
        return -1;
    }
    if ((buf = malloc(zst.size)) == NULL) {
        fprintf(stderr, "malloc failed: %s\n", strerror(errno));
        zip_source_close(src);
        return -1;
    }
    if (zip_source_read(src, buf, zst.size) < (zip_int64_t)zst.size) {
        fprintf(stderr, "zip_source_read on buffer failed: %s\n", zip_error_strerror(zip_source_error(src)));
        zip_source_close(src);
        free(buf);
        return -1;
    }
    zip_source_close(src);
    if ((fp = fopen(archive, "wb")) == NULL) {
        fprintf(stderr, "fopen failed: %s\n", strerror(errno));
        free(buf);
        return -1;
    }
    if (fwrite(buf, zst.size, 1, fp) < 1) {
        fprintf(stderr, "fwrite failed: %s\n", strerror(errno));
        free(buf);
        fclose(fp);
        return -1;
    }
    free(buf);
    if (fclose(fp) != 0) {
        fprintf(stderr, "fclose failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}


zip_t *
ziptool_open(const char *archive, int flags, zip_error_t *error, zip_uint64_t offset, zip_uint64_t len) {
    switch (source_type) {
    case SOURCE_TYPE_NONE:
        za = read_from_file(archive, flags, error, offset, len);
        break;

    case SOURCE_TYPE_IN_MEMORY:
        za = read_to_memory(archive, flags, error, &memory_src);
        break;

    case SOURCE_TYPE_HOLE:
        za = read_hole(archive, flags, error);
        break;
    }

    return za;
}


int
ziptool_post_close(const char *archive) {
    if (source_type == SOURCE_TYPE_IN_MEMORY) {
        if (write_memory_src_to_file(archive, memory_src) < 0) {
            return -1;
        }
        zip_source_free(memory_src);
    }

    return 0;
}
