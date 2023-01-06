#ifndef _HAD_ZIPINT_H
#define _HAD_ZIPINT_H

/*
  zipint.h -- internal declarations.
  Copyright (C) 1999-2022 Dieter Baron and Thomas Klausner

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

#include "compat.h"

#ifdef ZIP_ALLOCATE_BUFFER
#include <stdlib.h>
#endif

#ifndef _ZIP_COMPILING_DEPRECATED
#define ZIP_DISABLE_DEPRECATED
#endif

#include "zip.h"

#define CENTRAL_MAGIC "PK\1\2"
#define LOCAL_MAGIC "PK\3\4"
#define EOCD_MAGIC "PK\5\6"
#define DATADES_MAGIC "PK\7\10"
#define EOCD64LOC_MAGIC "PK\6\7"
#define EOCD64_MAGIC "PK\6\6"
#define CDENTRYSIZE 46u
#define LENTRYSIZE 30
#define MAXCOMLEN 65536
#define MAXEXTLEN 65536
#define EOCDLEN 22
#define EOCD64LOCLEN 20
#define EOCD64LEN 56
#define CDBUFSIZE (MAXCOMLEN + EOCDLEN + EOCD64LOCLEN)
#define BUFSIZE 8192
#define EFZIP64SIZE 28
#define EF_WINZIP_AES_SIZE 7
#define MAX_DATA_DESCRIPTOR_LENGTH 24

#define ZIP_CRYPTO_PKWARE_HEADERLEN 12

#define ZIP_CM_REPLACED_DEFAULT (-2)
#define ZIP_CM_WINZIP_AES 99 /* Winzip AES encrypted */

#define WINZIP_AES_PASSWORD_VERIFY_LENGTH 2
#define WINZIP_AES_MAX_HEADER_LENGTH (16 + WINZIP_AES_PASSWORD_VERIFY_LENGTH)
#define AES_BLOCK_SIZE 16
#define HMAC_LENGTH 10
#define SHA1_LENGTH 20
#define SALT_LENGTH(method) ((method) == ZIP_EM_AES_128 ? 8 : ((method) == ZIP_EM_AES_192 ? 12 : 16))

#define ZIP_CM_IS_DEFAULT(x) ((x) == ZIP_CM_DEFAULT || (x) == ZIP_CM_REPLACED_DEFAULT)
#define ZIP_CM_ACTUAL(x) ((zip_uint16_t)(ZIP_CM_IS_DEFAULT(x) ? ZIP_CM_DEFLATE : (x)))

#define ZIP_EF_UTF_8_COMMENT 0x6375
#define ZIP_EF_UTF_8_NAME 0x7075
#define ZIP_EF_WINZIP_AES 0x9901
#define ZIP_EF_ZIP64 0x0001

#define ZIP_EF_IS_INTERNAL(id) ((id) == ZIP_EF_UTF_8_COMMENT || (id) == ZIP_EF_UTF_8_NAME || (id) == ZIP_EF_WINZIP_AES || (id) == ZIP_EF_ZIP64)

/* according to unzip-6.0's zipinfo.c, this corresponds to a regular file with rw permissions for everyone */
#define ZIP_EXT_ATTRIB_DEFAULT (0100666u << 16)
/* according to unzip-6.0's zipinfo.c, this corresponds to a directory with rwx permissions for everyone */
#define ZIP_EXT_ATTRIB_DEFAULT_DIR (0040777u << 16)

#define ZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS_ALLOWED_MASK 0x0836

#define ZIP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define ZIP_MIN(a, b) ((a) < (b) ? (a) : (b))

/* This section contains API that won't materialize like this.  It's
   placed in the internal section, pending cleanup. */

/* flags for compression and encryption sources */

#define ZIP_CODEC_DECODE 0 /* decompress/decrypt (encode flag not set) */
#define ZIP_CODEC_ENCODE 1 /* compress/encrypt */

typedef zip_source_t *(*zip_encryption_implementation)(zip_t *, zip_source_t *, zip_uint16_t, int, const char *);

zip_encryption_implementation _zip_get_encryption_implementation(zip_uint16_t method, int operation);

/* clang-format off */
enum zip_compression_status {
    ZIP_COMPRESSION_OK,
    ZIP_COMPRESSION_END,
    ZIP_COMPRESSION_ERROR,
    ZIP_COMPRESSION_NEED_DATA
};
/* clang-format on */
typedef enum zip_compression_status zip_compression_status_t;

struct zip_compression_algorithm {
    /* Return maximum compressed size for uncompressed data of given size. */
    zip_uint64_t (*maximum_compressed_size)(zip_uint64_t uncompressed_size);

    /* called once to create new context */
    void *(*allocate)(zip_uint16_t method, int compression_flags, zip_error_t *error);
    /* called once to free context */
    void (*deallocate)(void *ctx);

    /* get compression specific general purpose bitflags */
    zip_uint16_t (*general_purpose_bit_flags)(void *ctx);
    /* minimum version needed when using this algorithm */
    zip_uint8_t version_needed;

    /* start processing */
    bool (*start)(void *ctx, zip_stat_t *st, zip_file_attributes_t *attributes);
    /* stop processing */
    bool (*end)(void *ctx);

    /* provide new input data, remains valid until next call to input or end */
    bool (*input)(void *ctx, zip_uint8_t *data, zip_uint64_t length);

    /* all input data has been provided */
    void (*end_of_input)(void *ctx);

    /* process input data, writing to data, which has room for length bytes, update length to number of bytes written */
    zip_compression_status_t (*process)(void *ctx, zip_uint8_t *data, zip_uint64_t *length);
};
typedef struct zip_compression_algorithm zip_compression_algorithm_t;

extern zip_compression_algorithm_t zip_algorithm_bzip2_compress;
extern zip_compression_algorithm_t zip_algorithm_bzip2_decompress;
extern zip_compression_algorithm_t zip_algorithm_deflate_compress;
extern zip_compression_algorithm_t zip_algorithm_deflate_decompress;
extern zip_compression_algorithm_t zip_algorithm_xz_compress;
extern zip_compression_algorithm_t zip_algorithm_xz_decompress;
extern zip_compression_algorithm_t zip_algorithm_zstd_compress;
extern zip_compression_algorithm_t zip_algorithm_zstd_decompress;

zip_compression_algorithm_t *_zip_get_compression_algorithm(zip_int32_t method, bool compress);

/* This API is not final yet, but we need it internally, so it's private for now. */

const zip_uint8_t *zip_get_extra_field_by_id(zip_t *, int, int, zip_uint16_t, int, zip_uint16_t *);

/* This section contains API that is of limited use until support for
   user-supplied compression/encryption implementation is finished.
   Thus we will keep it private for now. */

zip_source_t *zip_source_compress(zip_t *za, zip_source_t *src, zip_int32_t cm, int compression_flags);
zip_source_t *zip_source_crc_create(zip_source_t *, int, zip_error_t *error);
zip_source_t *zip_source_decompress(zip_t *za, zip_source_t *src, zip_int32_t cm);
zip_source_t *zip_source_pkware_decode(zip_t *, zip_source_t *, zip_uint16_t, int, const char *);
zip_source_t *zip_source_pkware_encode(zip_t *, zip_source_t *, zip_uint16_t, int, const char *);
int zip_source_remove(zip_source_t *);
zip_int64_t zip_source_supports(zip_source_t *src);
bool zip_source_supports_reopen(zip_source_t *src);
zip_source_t *zip_source_winzip_aes_decode(zip_t *, zip_source_t *, zip_uint16_t, int, const char *);
zip_source_t *zip_source_winzip_aes_encode(zip_t *, zip_source_t *, zip_uint16_t, int, const char *);
zip_source_t *zip_source_buffer_with_attributes(zip_t *za, const void *data, zip_uint64_t len, int freep, zip_file_attributes_t *attributes);
zip_source_t *zip_source_buffer_with_attributes_create(const void *data, zip_uint64_t len, int freep, zip_file_attributes_t *attributes, zip_error_t *error);

/* error source for layered sources */

enum zip_les { ZIP_LES_NONE, ZIP_LES_UPPER, ZIP_LES_LOWER, ZIP_LES_INVAL };

#define ZIP_DETAIL_ET_GLOBAL 0
#define ZIP_DETAIL_ET_ENTRY  1

struct _zip_err_info {
    int type;
    const char *description;
};

extern const struct _zip_err_info _zip_err_str[];
extern const int _zip_err_str_count;
extern const struct _zip_err_info _zip_err_details[];
extern const int _zip_err_details_count;

/* macros for libzip-internal errors */
#define MAX_DETAIL_INDEX 0x7fffff
#define MAKE_DETAIL_WITH_INDEX(error, index) ((((index) > MAX_DETAIL_INDEX) ? MAX_DETAIL_INDEX : (int)(index)) << 8 | (error))
#define GET_INDEX_FROM_DETAIL(error) (((error) >> 8) & MAX_DETAIL_INDEX)
#define GET_ERROR_FROM_DETAIL(error) ((error) & 0xff)
#define ADD_INDEX_TO_DETAIL(error, index) MAKE_DETAIL_WITH_INDEX(GET_ERROR_FROM_DETAIL(error), (index))

/* error code for libzip-internal errors */
#define ZIP_ER_DETAIL_NO_DETAIL 0   /* G no detail */
#define ZIP_ER_DETAIL_CDIR_OVERLAPS_EOCD 1  /* G central directory overlaps EOCD, or there is space between them */
#define ZIP_ER_DETAIL_COMMENT_LENGTH_INVALID 2  /* G archive comment length incorrect */
#define ZIP_ER_DETAIL_CDIR_LENGTH_INVALID 3  /* G central directory length invalid */
#define ZIP_ER_DETAIL_CDIR_ENTRY_INVALID 4  /* E central header invalid */
#define ZIP_ER_DETAIL_CDIR_WRONG_ENTRIES_COUNT 5  /* G central directory count of entries is incorrect */
#define ZIP_ER_DETAIL_ENTRY_HEADER_MISMATCH 6  /* E local and central headers do not match */
#define ZIP_ER_DETAIL_EOCD_LENGTH_INVALID 7  /* G wrong EOCD length */
#define ZIP_ER_DETAIL_EOCD64_OVERLAPS_EOCD 8  /* G EOCD64 overlaps EOCD, or there is space between them */
#define ZIP_ER_DETAIL_EOCD64_WRONG_MAGIC 9  /* G EOCD64 magic incorrect */
#define ZIP_ER_DETAIL_EOCD64_MISMATCH 10  /* G EOCD64 and EOCD do not match */
#define ZIP_ER_DETAIL_CDIR_INVALID 11  /* G invalid value in central directory */
#define ZIP_ER_DETAIL_VARIABLE_SIZE_OVERFLOW 12 /* E variable size fields overflow header */
#define ZIP_ER_DETAIL_INVALID_UTF8_IN_FILENAME 13 /* E invalid UTF-8 in filename */
#define ZIP_ER_DETAIL_INVALID_UTF8_IN_COMMENT 13 /* E invalid UTF-8 in comment */
#define ZIP_ER_DETAIL_INVALID_ZIP64_EF 14 /* E invalid Zip64 extra field */
#define ZIP_ER_DETAIL_INVALID_WINZIPAES_EF 14 /* E invalid WinZip AES extra field */
#define ZIP_ER_DETAIL_EF_TRAILING_GARBAGE 15 /* E garbage at end of extra fields */
#define ZIP_ER_DETAIL_INVALID_EF_LENGTH 16 /* E extra field length is invalid */
#define ZIP_ER_DETAIL_INVALID_FILE_LENGTH 17 /* E file length in header doesn't match actual file length */

/* directory entry: general purpose bit flags */

#define ZIP_GPBF_ENCRYPTED 0x0001u         /* is encrypted */
#define ZIP_GPBF_DATA_DESCRIPTOR 0x0008u   /* crc/size after file data */
#define ZIP_GPBF_STRONG_ENCRYPTION 0x0040u /* uses strong encryption */
#define ZIP_GPBF_ENCODING_UTF_8 0x0800u    /* file name encoding is UTF-8 */


/* extra fields */
#define ZIP_EF_LOCAL ZIP_FL_LOCAL                   /* include in local header */
#define ZIP_EF_CENTRAL ZIP_FL_CENTRAL               /* include in central directory */
#define ZIP_EF_BOTH (ZIP_EF_LOCAL | ZIP_EF_CENTRAL) /* include in both */

#define ZIP_FL_FORCE_ZIP64 1024 /* force zip64 extra field (_zip_dirent_write) */

#define ZIP_FL_ENCODING_ALL (ZIP_FL_ENC_GUESS | ZIP_FL_ENC_CP437 | ZIP_FL_ENC_UTF_8)


/* encoding type */
enum zip_encoding_type {
    ZIP_ENCODING_UNKNOWN,      /* not yet analyzed */
    ZIP_ENCODING_ASCII,        /* plain ASCII */
    ZIP_ENCODING_UTF8_KNOWN,   /* is UTF-8 */
    ZIP_ENCODING_UTF8_GUESSED, /* possibly UTF-8 */
    ZIP_ENCODING_CP437,        /* Code Page 437 */
    ZIP_ENCODING_ERROR         /* should be UTF-8 but isn't */
};

typedef enum zip_encoding_type zip_encoding_type_t;

struct zip_hash;
struct zip_progress;

typedef struct zip_cdir zip_cdir_t;
typedef struct zip_dirent zip_dirent_t;
typedef struct zip_entry zip_entry_t;
typedef struct zip_extra_field zip_extra_field_t;
typedef struct zip_string zip_string_t;
typedef struct zip_buffer zip_buffer_t;
typedef struct zip_hash zip_hash_t;
typedef struct zip_progress zip_progress_t;

/* zip archive, part of API */

struct zip {
    zip_source_t *src;       /* data source for archive */
    unsigned int open_flags; /* flags passed to zip_open */
    zip_error_t error;       /* error information */

    unsigned int flags;    /* archive global flags */
    unsigned int ch_flags; /* changed archive global flags */

    char *default_password; /* password used when no other supplied */

    zip_string_t *comment_orig;    /* archive comment */
    zip_string_t *comment_changes; /* changed archive comment */
    bool comment_changed;          /* whether archive comment was changed */

    zip_uint64_t nentry;       /* number of entries */
    zip_uint64_t nentry_alloc; /* number of entries allocated */
    zip_entry_t *entry;        /* entries */

    unsigned int nopen_source;       /* number of open sources using archive */
    unsigned int nopen_source_alloc; /* number of sources allocated */
    zip_source_t **open_source;      /* open sources using archive */

    zip_hash_t *names; /* hash table for name lookup */

    zip_progress_t *progress; /* progress callback for zip_close() */
};

/* file in zip archive, part of API */

struct zip_file {
    zip_t *za;         /* zip archive containing this file */
    zip_error_t error; /* error information */
    zip_source_t *src; /* data source */
};

/* zip archive directory entry (central or local) */

#define ZIP_DIRENT_COMP_METHOD 0x0001u
#define ZIP_DIRENT_FILENAME 0x0002u
#define ZIP_DIRENT_COMMENT 0x0004u
#define ZIP_DIRENT_EXTRA_FIELD 0x0008u
#define ZIP_DIRENT_ATTRIBUTES 0x0010u
#define ZIP_DIRENT_LAST_MOD 0x0020u
#define ZIP_DIRENT_ENCRYPTION_METHOD 0x0040u
#define ZIP_DIRENT_PASSWORD 0x0080u
#define ZIP_DIRENT_ALL ZIP_UINT32_MAX

struct zip_dirent {
    zip_uint32_t changed;
    bool local_extra_fields_read; /*      whether we already read in local header extra fields */
    bool cloned;                  /*      whether this instance is cloned, and thus shares non-changed strings */

    bool crc_valid; /*      if CRC is valid (sometimes not for encrypted archives) */

    zip_uint16_t version_madeby;     /* (c)  version of creator */
    zip_uint16_t version_needed;     /* (cl) version needed to extract */
    zip_uint16_t bitflags;           /* (cl) general purpose bit flag */
    zip_int32_t comp_method;         /* (cl) compression method used (uint16 and ZIP_CM_DEFAULT (-1)) */
    time_t last_mod;                 /* (cl) time of last modification */
    zip_uint32_t crc;                /* (cl) CRC-32 of uncompressed data */
    zip_uint64_t comp_size;          /* (cl) size of compressed data */
    zip_uint64_t uncomp_size;        /* (cl) size of uncompressed data */
    zip_string_t *filename;          /* (cl) file name (NUL-terminated) */
    zip_extra_field_t *extra_fields; /* (cl) extra fields, parsed */
    zip_string_t *comment;           /* (c)  file comment */
    zip_uint32_t disk_number;        /* (c)  disk number start */
    zip_uint16_t int_attrib;         /* (c)  internal file attributes */
    zip_uint32_t ext_attrib;         /* (c)  external file attributes */
    zip_uint64_t offset;             /* (c)  offset of local header */

    zip_uint16_t compression_level; /*      level of compression to use (never valid in orig) */
    zip_uint16_t encryption_method; /*      encryption method, computed from other fields */
    char *password;                 /*      file specific encryption password */
};

/* zip archive central directory */

struct zip_cdir {
    zip_entry_t *entry;        /* directory entries */
    zip_uint64_t nentry;       /* number of entries */
    zip_uint64_t nentry_alloc; /* number of entries allocated */

    zip_uint64_t size;     /* size of central directory */
    zip_uint64_t offset;   /* offset of central directory in file */
    zip_string_t *comment; /* zip archive comment */
    bool is_zip64;         /* central directory in zip64 format */
};

struct zip_extra_field {
    zip_extra_field_t *next;
    zip_flags_t flags; /* in local/central header */
    zip_uint16_t id;   /* header id */
    zip_uint16_t size; /* data size */
    zip_uint8_t *data;
};

enum zip_source_write_state {
    ZIP_SOURCE_WRITE_CLOSED, /* write is not in progress */
    ZIP_SOURCE_WRITE_OPEN,   /* write is in progress */
    ZIP_SOURCE_WRITE_FAILED, /* commit failed, only rollback allowed */
    ZIP_SOURCE_WRITE_REMOVED /* file was removed */
};
typedef enum zip_source_write_state zip_source_write_state_t;

struct zip_source {
    zip_source_t *src;
    union {
        zip_source_callback f;
        zip_source_layered_callback l;
    } cb;
    void *ud;
    zip_error_t error;
    zip_int64_t supports;                 /* supported commands */
    unsigned int open_count;              /* number of times source was opened (directly or as lower layer) */
    zip_source_write_state_t write_state; /* whether source is open for writing */
    bool source_closed;                   /* set if source archive is closed */
    zip_t *source_archive;                /* zip archive we're reading from, NULL if not from archive */
    unsigned int refcount;
    bool eof;                /* EOF reached */
    bool had_read_error;     /* a previous ZIP_SOURCE_READ reported an error */
    zip_uint64_t bytes_read; /* for sources that don't support ZIP_SOURCE_TELL. */
};

#define ZIP_SOURCE_IS_OPEN_READING(src) ((src)->open_count > 0)
#define ZIP_SOURCE_IS_OPEN_WRITING(src) ((src)->write_state == ZIP_SOURCE_WRITE_OPEN)
#define ZIP_SOURCE_IS_LAYERED(src) ((src)->src != NULL)

/* entry in zip archive directory */

struct zip_entry {
    zip_dirent_t *orig;
    zip_dirent_t *changes;
    zip_source_t *source;
    bool deleted;
};


/* file or archive comment, or filename */

struct zip_string {
    zip_uint8_t *raw;                /* raw string */
    zip_uint16_t length;             /* length of raw string */
    enum zip_encoding_type encoding; /* autorecognized encoding */
    zip_uint8_t *converted;          /* autoconverted string */
    zip_uint32_t converted_length;   /* length of converted */
};


/* byte array */

/* For performance, we usually keep 8k byte arrays on the stack.
   However, there are (embedded) systems with a stack size of 12k;
   for those, use malloc()/free() */

#ifdef ZIP_ALLOCATE_BUFFER
#define DEFINE_BYTE_ARRAY(buf, size) zip_uint8_t *buf
#define byte_array_init(buf, size) (((buf) = (zip_uint8_t *)malloc(size)) != NULL)
#define byte_array_fini(buf) (free(buf))
#else
#define DEFINE_BYTE_ARRAY(buf, size) zip_uint8_t buf[size]
#define byte_array_init(buf, size) (1)
#define byte_array_fini(buf) ((void)0)
#endif


/* bounds checked access to memory buffer */

struct zip_buffer {
    bool ok;
    bool free_data;

    zip_uint8_t *data;
    zip_uint64_t size;
    zip_uint64_t offset;
};

/* which files to write in which order */

struct zip_filelist {
    zip_uint64_t idx;
    /* TODO    const char *name; */
};

typedef struct zip_filelist zip_filelist_t;

struct _zip_winzip_aes;
typedef struct _zip_winzip_aes zip_winzip_aes_t;

struct _zip_pkware_keys {
    zip_uint32_t key[3];
};
typedef struct _zip_pkware_keys zip_pkware_keys_t;

#define ZIP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define ZIP_MIN(a, b) ((a) < (b) ? (a) : (b))

#define ZIP_ENTRY_CHANGED(e, f) ((e)->changes && ((e)->changes->changed & (f)))
#define ZIP_ENTRY_DATA_CHANGED(x) ((x)->source != NULL)
#define ZIP_ENTRY_HAS_CHANGES(e) (ZIP_ENTRY_DATA_CHANGED(e) || (e)->deleted || ZIP_ENTRY_CHANGED((e), ZIP_DIRENT_ALL))

#define ZIP_IS_RDONLY(za) ((za)->ch_flags & ZIP_AFL_RDONLY)


#ifdef HAVE_EXPLICIT_MEMSET
#define _zip_crypto_clear(b, l) explicit_memset((b), 0, (l))
#else
#ifdef HAVE_EXPLICIT_BZERO
#define _zip_crypto_clear(b, l) explicit_bzero((b), (l))
#else
#include <string.h>
#define _zip_crypto_clear(b, l) memset((b), 0, (l))
#endif
#endif


zip_int64_t _zip_add_entry(zip_t *);

zip_uint8_t *_zip_buffer_data(zip_buffer_t *buffer);
bool _zip_buffer_eof(zip_buffer_t *buffer);
void _zip_buffer_free(zip_buffer_t *buffer);
zip_uint8_t *_zip_buffer_get(zip_buffer_t *buffer, zip_uint64_t length);
zip_uint16_t _zip_buffer_get_16(zip_buffer_t *buffer);
zip_uint32_t _zip_buffer_get_32(zip_buffer_t *buffer);
zip_uint64_t _zip_buffer_get_64(zip_buffer_t *buffer);
zip_uint8_t _zip_buffer_get_8(zip_buffer_t *buffer);
zip_uint64_t _zip_buffer_left(zip_buffer_t *buffer);
zip_buffer_t *_zip_buffer_new(zip_uint8_t *data, zip_uint64_t size);
zip_buffer_t *_zip_buffer_new_from_source(zip_source_t *src, zip_uint64_t size, zip_uint8_t *buf, zip_error_t *error);
zip_uint64_t _zip_buffer_offset(zip_buffer_t *buffer);
bool _zip_buffer_ok(zip_buffer_t *buffer);
zip_uint8_t *_zip_buffer_peek(zip_buffer_t *buffer, zip_uint64_t length);
int _zip_buffer_put(zip_buffer_t *buffer, const void *src, size_t length);
int _zip_buffer_put_16(zip_buffer_t *buffer, zip_uint16_t i);
int _zip_buffer_put_32(zip_buffer_t *buffer, zip_uint32_t i);
int _zip_buffer_put_64(zip_buffer_t *buffer, zip_uint64_t i);
int _zip_buffer_put_8(zip_buffer_t *buffer, zip_uint8_t i);
zip_uint64_t _zip_buffer_read(zip_buffer_t *buffer, zip_uint8_t *data, zip_uint64_t length);
int _zip_buffer_skip(zip_buffer_t *buffer, zip_uint64_t length);
int _zip_buffer_set_offset(zip_buffer_t *buffer, zip_uint64_t offset);
zip_uint64_t _zip_buffer_size(zip_buffer_t *buffer);

void _zip_cdir_free(zip_cdir_t *);
bool _zip_cdir_grow(zip_cdir_t *cd, zip_uint64_t additional_entries, zip_error_t *error);
zip_cdir_t *_zip_cdir_new(zip_uint64_t, zip_error_t *);
zip_int64_t _zip_cdir_write(zip_t *za, const zip_filelist_t *filelist, zip_uint64_t survivors);
time_t _zip_d2u_time(zip_uint16_t, zip_uint16_t);
void _zip_deregister_source(zip_t *za, zip_source_t *src);

void _zip_dirent_apply_attributes(zip_dirent_t *, zip_file_attributes_t *, bool, zip_uint32_t);
zip_dirent_t *_zip_dirent_clone(const zip_dirent_t *);
void _zip_dirent_free(zip_dirent_t *);
void _zip_dirent_finalize(zip_dirent_t *);
void _zip_dirent_init(zip_dirent_t *);
bool _zip_dirent_needs_zip64(const zip_dirent_t *, zip_flags_t);
zip_dirent_t *_zip_dirent_new(void);
bool zip_dirent_process_ef_zip64(zip_dirent_t * zde, const zip_uint8_t * ef, zip_uint64_t got_len, bool local, zip_error_t * error);
zip_int64_t _zip_dirent_read(zip_dirent_t *zde, zip_source_t *src, zip_buffer_t *buffer, bool local, zip_error_t *error);
void _zip_dirent_set_version_needed(zip_dirent_t *de, bool force_zip64);
zip_int32_t _zip_dirent_size(zip_source_t *src, zip_uint16_t, zip_error_t *);
int _zip_dirent_write(zip_t *za, zip_dirent_t *dirent, zip_flags_t flags);

zip_extra_field_t *_zip_ef_clone(const zip_extra_field_t *, zip_error_t *);
zip_extra_field_t *_zip_ef_delete_by_id(zip_extra_field_t *, zip_uint16_t, zip_uint16_t, zip_flags_t);
void _zip_ef_free(zip_extra_field_t *);
const zip_uint8_t *_zip_ef_get_by_id(const zip_extra_field_t *, zip_uint16_t *, zip_uint16_t, zip_uint16_t, zip_flags_t, zip_error_t *);
zip_extra_field_t *_zip_ef_merge(zip_extra_field_t *, zip_extra_field_t *);
zip_extra_field_t *_zip_ef_new(zip_uint16_t, zip_uint16_t, const zip_uint8_t *, zip_flags_t);
bool _zip_ef_parse(const zip_uint8_t *, zip_uint16_t, zip_flags_t, zip_extra_field_t **, zip_error_t *);
zip_extra_field_t *_zip_ef_remove_internal(zip_extra_field_t *);
zip_uint16_t _zip_ef_size(const zip_extra_field_t *, zip_flags_t);
int _zip_ef_write(zip_t *za, const zip_extra_field_t *ef, zip_flags_t flags);

void _zip_entry_finalize(zip_entry_t *);
void _zip_entry_init(zip_entry_t *);

void _zip_error_clear(zip_error_t *);
void _zip_error_get(const zip_error_t *, int *, int *);

void _zip_error_copy(zip_error_t *dst, const zip_error_t *src);

const zip_uint8_t *_zip_extract_extra_field_by_id(zip_error_t *, zip_uint16_t, int, const zip_uint8_t *, zip_uint16_t, zip_uint16_t *);

int _zip_file_extra_field_prepare_for_change(zip_t *, zip_uint64_t);
int _zip_file_fillbuf(void *, size_t, zip_file_t *);
zip_uint64_t _zip_file_get_end(const zip_t *za, zip_uint64_t index, zip_error_t *error);
zip_uint64_t _zip_file_get_offset(const zip_t *, zip_uint64_t, zip_error_t *);

zip_dirent_t *_zip_get_dirent(zip_t *, zip_uint64_t, zip_flags_t, zip_error_t *);

enum zip_encoding_type _zip_guess_encoding(zip_string_t *, enum zip_encoding_type);
zip_uint8_t *_zip_cp437_to_utf8(const zip_uint8_t *const, zip_uint32_t, zip_uint32_t *, zip_error_t *);

bool _zip_hash_add(zip_hash_t *hash, const zip_uint8_t *name, zip_uint64_t index, zip_flags_t flags, zip_error_t *error);
bool _zip_hash_delete(zip_hash_t *hash, const zip_uint8_t *key, zip_error_t *error);
void _zip_hash_free(zip_hash_t *hash);
zip_int64_t _zip_hash_lookup(zip_hash_t *hash, const zip_uint8_t *name, zip_flags_t flags, zip_error_t *error);
zip_hash_t *_zip_hash_new(zip_error_t *error);
bool _zip_hash_reserve_capacity(zip_hash_t *hash, zip_uint64_t capacity, zip_error_t *error);
bool _zip_hash_revert(zip_hash_t *hash, zip_error_t *error);

int _zip_mkstempm(char *path, int mode, bool create_file);

zip_t *_zip_open(zip_source_t *, unsigned int, zip_error_t *);

void _zip_progress_end(zip_progress_t *progress);
void _zip_progress_free(zip_progress_t *progress);
int _zip_progress_start(zip_progress_t *progress);
int _zip_progress_subrange(zip_progress_t *progress, double start, double end);
int _zip_progress_update(zip_progress_t *progress, double value);

/* this symbol is extern so it can be overridden for regression testing */
ZIP_EXTERN bool zip_secure_random(zip_uint8_t *buffer, zip_uint16_t length);
zip_uint32_t zip_random_uint32(void);

int _zip_read(zip_source_t *src, zip_uint8_t *data, zip_uint64_t length, zip_error_t *error);
int _zip_read_at_offset(zip_source_t *src, zip_uint64_t offset, unsigned char *b, size_t length, zip_error_t *error);
zip_uint8_t *_zip_read_data(zip_buffer_t *buffer, zip_source_t *src, size_t length, bool nulp, zip_error_t *error);
int _zip_read_local_ef(zip_t *, zip_uint64_t);
zip_string_t *_zip_read_string(zip_buffer_t *buffer, zip_source_t *src, zip_uint16_t length, bool nulp, zip_error_t *error);
int _zip_register_source(zip_t *za, zip_source_t *src);

void _zip_set_open_error(int *zep, const zip_error_t *err, int ze);

bool zip_source_accept_empty(zip_source_t *src);
zip_int64_t _zip_source_call(zip_source_t *src, void *data, zip_uint64_t length, zip_source_cmd_t command);
bool _zip_source_eof(zip_source_t *);
zip_source_t *_zip_source_file_or_p(const char *, FILE *, zip_uint64_t, zip_int64_t, const zip_stat_t *, zip_error_t *error);
bool _zip_source_had_error(zip_source_t *);
void _zip_source_invalidate(zip_source_t *src);
zip_source_t *_zip_source_new(zip_error_t *error);
int _zip_source_set_source_archive(zip_source_t *, zip_t *);
zip_source_t *_zip_source_window_new(zip_source_t *src, zip_uint64_t start, zip_int64_t length, zip_stat_t *st, zip_file_attributes_t *attributes, zip_t *source_archive, zip_uint64_t source_index, zip_error_t *error);
zip_source_t *_zip_source_zip_new(zip_t *, zip_uint64_t, zip_flags_t, zip_uint64_t, zip_uint64_t, const char *, zip_error_t *error);

int _zip_stat_merge(zip_stat_t *dst, const zip_stat_t *src, zip_error_t *error);
int _zip_string_equal(const zip_string_t *a, const zip_string_t *b);
void _zip_string_free(zip_string_t *string);
zip_uint32_t _zip_string_crc32(const zip_string_t *string);
const zip_uint8_t *_zip_string_get(zip_string_t *string, zip_uint32_t *lenp, zip_flags_t flags, zip_error_t *error);
zip_uint16_t _zip_string_length(const zip_string_t *string);
zip_string_t *_zip_string_new(const zip_uint8_t *raw, zip_uint16_t length, zip_flags_t flags, zip_error_t *error);
int _zip_string_write(zip_t *za, const zip_string_t *string);
bool _zip_winzip_aes_decrypt(zip_winzip_aes_t *ctx, zip_uint8_t *data, zip_uint64_t length);
bool _zip_winzip_aes_encrypt(zip_winzip_aes_t *ctx, zip_uint8_t *data, zip_uint64_t length);
bool _zip_winzip_aes_finish(zip_winzip_aes_t *ctx, zip_uint8_t *hmac);
void _zip_winzip_aes_free(zip_winzip_aes_t *ctx);
zip_winzip_aes_t *_zip_winzip_aes_new(const zip_uint8_t *password, zip_uint64_t password_length, const zip_uint8_t *salt, zip_uint16_t key_size, zip_uint8_t *password_verify, zip_error_t *error);

void _zip_pkware_encrypt(zip_pkware_keys_t *keys, zip_uint8_t *out, const zip_uint8_t *in, zip_uint64_t len);
void _zip_pkware_decrypt(zip_pkware_keys_t *keys, zip_uint8_t *out, const zip_uint8_t *in, zip_uint64_t len);
zip_pkware_keys_t *_zip_pkware_keys_new(zip_error_t *error);
void _zip_pkware_keys_free(zip_pkware_keys_t *keys);
void _zip_pkware_keys_reset(zip_pkware_keys_t *keys);

int _zip_changed(const zip_t *, zip_uint64_t *);
const char *_zip_get_name(zip_t *, zip_uint64_t, zip_flags_t, zip_error_t *);
int _zip_local_header_read(zip_t *, int);
void *_zip_memdup(const void *, size_t, zip_error_t *);
zip_int64_t _zip_name_locate(zip_t *, const char *, zip_flags_t, zip_error_t *);
zip_t *_zip_new(zip_error_t *);

zip_int64_t _zip_file_replace(zip_t *, zip_uint64_t, const char *, zip_source_t *, zip_flags_t);
int _zip_set_name(zip_t *, zip_uint64_t, const char *, zip_flags_t);
void _zip_u2d_time(time_t, zip_uint16_t *, zip_uint16_t *);
int _zip_unchange(zip_t *, zip_uint64_t, int);
void _zip_unchange_data(zip_entry_t *);
int _zip_write(zip_t *za, const void *data, zip_uint64_t length);

#endif /* zipint.h */
