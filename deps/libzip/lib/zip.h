#ifndef _HAD_ZIP_H
#define _HAD_ZIP_H

/*
  zip.h -- exported declarations.
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


#ifdef __cplusplus
extern "C" {
#if 0
} /* fix autoindent */
#endif
#endif

#include <zipconf.h>

#ifndef ZIP_EXTERN
#ifndef ZIP_STATIC
#ifdef _WIN32
#define ZIP_EXTERN __declspec(dllimport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define ZIP_EXTERN __attribute__((visibility("default")))
#else
#define ZIP_EXTERN
#endif
#else
#define ZIP_EXTERN
#endif
#endif

#include <stdio.h>
#include <sys/types.h>
#include <time.h>

/* flags for zip_open */

#define ZIP_CREATE 1
#define ZIP_EXCL 2
#define ZIP_CHECKCONS 4
#define ZIP_TRUNCATE 8
#define ZIP_RDONLY 16


/* flags for zip_name_locate, zip_fopen, zip_stat, ... */

#define ZIP_FL_NOCASE 1u       /* ignore case on name lookup */
#define ZIP_FL_NODIR 2u        /* ignore directory component */
#define ZIP_FL_COMPRESSED 4u   /* read compressed data */
#define ZIP_FL_UNCHANGED 8u    /* use original data, ignoring changes */
#define ZIP_FL_RECOMPRESS 16u  /* force recompression of data */
#define ZIP_FL_ENCRYPTED 32u   /* read encrypted data (implies ZIP_FL_COMPRESSED) */
#define ZIP_FL_ENC_GUESS 0u    /* guess string encoding (is default) */
#define ZIP_FL_ENC_RAW 64u     /* get unmodified string */
#define ZIP_FL_ENC_STRICT 128u /* follow specification strictly */
#define ZIP_FL_LOCAL 256u      /* in local header */
#define ZIP_FL_CENTRAL 512u    /* in central directory */
/*                           1024u    reserved for internal use */
#define ZIP_FL_ENC_UTF_8 2048u /* string is UTF-8 encoded */
#define ZIP_FL_ENC_CP437 4096u /* string is CP437 encoded */
#define ZIP_FL_OVERWRITE 8192u /* zip_file_add: if file with name exists, overwrite (replace) it */

/* archive global flags flags */

#define ZIP_AFL_RDONLY 2u /* read only -- cannot be cleared */


/* create a new extra field */

#define ZIP_EXTRA_FIELD_ALL ZIP_UINT16_MAX
#define ZIP_EXTRA_FIELD_NEW ZIP_UINT16_MAX


/* libzip error codes */

#define ZIP_ER_OK 0               /* N No error */
#define ZIP_ER_MULTIDISK 1        /* N Multi-disk zip archives not supported */
#define ZIP_ER_RENAME 2           /* S Renaming temporary file failed */
#define ZIP_ER_CLOSE 3            /* S Closing zip archive failed */
#define ZIP_ER_SEEK 4             /* S Seek error */
#define ZIP_ER_READ 5             /* S Read error */
#define ZIP_ER_WRITE 6            /* S Write error */
#define ZIP_ER_CRC 7              /* N CRC error */
#define ZIP_ER_ZIPCLOSED 8        /* N Containing zip archive was closed */
#define ZIP_ER_NOENT 9            /* N No such file */
#define ZIP_ER_EXISTS 10          /* N File already exists */
#define ZIP_ER_OPEN 11            /* S Can't open file */
#define ZIP_ER_TMPOPEN 12         /* S Failure to create temporary file */
#define ZIP_ER_ZLIB 13            /* Z Zlib error */
#define ZIP_ER_MEMORY 14          /* N Malloc failure */
#define ZIP_ER_CHANGED 15         /* N Entry has been changed */
#define ZIP_ER_COMPNOTSUPP 16     /* N Compression method not supported */
#define ZIP_ER_EOF 17             /* N Premature end of file */
#define ZIP_ER_INVAL 18           /* N Invalid argument */
#define ZIP_ER_NOZIP 19           /* N Not a zip archive */
#define ZIP_ER_INTERNAL 20        /* N Internal error */
#define ZIP_ER_INCONS 21          /* L Zip archive inconsistent */
#define ZIP_ER_REMOVE 22          /* S Can't remove file */
#define ZIP_ER_DELETED 23         /* N Entry has been deleted */
#define ZIP_ER_ENCRNOTSUPP 24     /* N Encryption method not supported */
#define ZIP_ER_RDONLY 25          /* N Read-only archive */
#define ZIP_ER_NOPASSWD 26        /* N No password provided */
#define ZIP_ER_WRONGPASSWD 27     /* N Wrong password provided */
#define ZIP_ER_OPNOTSUPP 28       /* N Operation not supported */
#define ZIP_ER_INUSE 29           /* N Resource still in use */
#define ZIP_ER_TELL 30            /* S Tell error */
#define ZIP_ER_COMPRESSED_DATA 31 /* N Compressed data invalid */
#define ZIP_ER_CANCELLED 32       /* N Operation cancelled */
#define ZIP_ER_DATA_LENGTH 33     /* N Unexpected length of data */

/* type of system error value */

#define ZIP_ET_NONE 0   /* sys_err unused */
#define ZIP_ET_SYS 1    /* sys_err is errno */
#define ZIP_ET_ZLIB 2   /* sys_err is zlib error code */
#define ZIP_ET_LIBZIP 3 /* sys_err is libzip error code */

/* compression methods */

#define ZIP_CM_DEFAULT -1 /* better of deflate or store */
#define ZIP_CM_STORE 0    /* stored (uncompressed) */
#define ZIP_CM_SHRINK 1   /* shrunk */
#define ZIP_CM_REDUCE_1 2 /* reduced with factor 1 */
#define ZIP_CM_REDUCE_2 3 /* reduced with factor 2 */
#define ZIP_CM_REDUCE_3 4 /* reduced with factor 3 */
#define ZIP_CM_REDUCE_4 5 /* reduced with factor 4 */
#define ZIP_CM_IMPLODE 6  /* imploded */
/* 7 - Reserved for Tokenizing compression algorithm */
#define ZIP_CM_DEFLATE 8         /* deflated */
#define ZIP_CM_DEFLATE64 9       /* deflate64 */
#define ZIP_CM_PKWARE_IMPLODE 10 /* PKWARE imploding */
/* 11 - Reserved by PKWARE */
#define ZIP_CM_BZIP2 12 /* compressed using BZIP2 algorithm */
/* 13 - Reserved by PKWARE */
#define ZIP_CM_LZMA 14 /* LZMA (EFS) */
/* 15-17 - Reserved by PKWARE */
#define ZIP_CM_TERSE 18 /* compressed using IBM TERSE (new) */
#define ZIP_CM_LZ77 19  /* IBM LZ77 z Architecture (PFS) */
/* 20 - old value for Zstandard */
#define ZIP_CM_LZMA2 33
#define ZIP_CM_ZSTD 93    /* Zstandard compressed data */
#define ZIP_CM_XZ 95      /* XZ compressed data */
#define ZIP_CM_JPEG 96    /* Compressed Jpeg data */
#define ZIP_CM_WAVPACK 97 /* WavPack compressed data */
#define ZIP_CM_PPMD 98    /* PPMd version I, Rev 1 */

/* encryption methods */

#define ZIP_EM_NONE 0         /* not encrypted */
#define ZIP_EM_TRAD_PKWARE 1  /* traditional PKWARE encryption */
#if 0                         /* Strong Encryption Header not parsed yet */
#define ZIP_EM_DES 0x6601     /* strong encryption: DES */
#define ZIP_EM_RC2_OLD 0x6602 /* strong encryption: RC2, version < 5.2 */
#define ZIP_EM_3DES_168 0x6603
#define ZIP_EM_3DES_112 0x6609
#define ZIP_EM_PKZIP_AES_128 0x660e
#define ZIP_EM_PKZIP_AES_192 0x660f
#define ZIP_EM_PKZIP_AES_256 0x6610
#define ZIP_EM_RC2 0x6702 /* strong encryption: RC2, version >= 5.2 */
#define ZIP_EM_RC4 0x6801
#endif
#define ZIP_EM_AES_128 0x0101 /* Winzip AES encryption */
#define ZIP_EM_AES_192 0x0102
#define ZIP_EM_AES_256 0x0103
#define ZIP_EM_UNKNOWN 0xffff /* unknown algorithm */

#define ZIP_OPSYS_DOS 0x00u
#define ZIP_OPSYS_AMIGA 0x01u
#define ZIP_OPSYS_OPENVMS 0x02u
#define ZIP_OPSYS_UNIX 0x03u
#define ZIP_OPSYS_VM_CMS 0x04u
#define ZIP_OPSYS_ATARI_ST 0x05u
#define ZIP_OPSYS_OS_2 0x06u
#define ZIP_OPSYS_MACINTOSH 0x07u
#define ZIP_OPSYS_Z_SYSTEM 0x08u
#define ZIP_OPSYS_CPM 0x09u
#define ZIP_OPSYS_WINDOWS_NTFS 0x0au
#define ZIP_OPSYS_MVS 0x0bu
#define ZIP_OPSYS_VSE 0x0cu
#define ZIP_OPSYS_ACORN_RISC 0x0du
#define ZIP_OPSYS_VFAT 0x0eu
#define ZIP_OPSYS_ALTERNATE_MVS 0x0fu
#define ZIP_OPSYS_BEOS 0x10u
#define ZIP_OPSYS_TANDEM 0x11u
#define ZIP_OPSYS_OS_400 0x12u
#define ZIP_OPSYS_OS_X 0x13u

#define ZIP_OPSYS_DEFAULT ZIP_OPSYS_UNIX


enum zip_source_cmd {
    ZIP_SOURCE_OPEN,                /* prepare for reading */
    ZIP_SOURCE_READ,                /* read data */
    ZIP_SOURCE_CLOSE,               /* reading is done */
    ZIP_SOURCE_STAT,                /* get meta information */
    ZIP_SOURCE_ERROR,               /* get error information */
    ZIP_SOURCE_FREE,                /* cleanup and free resources */
    ZIP_SOURCE_SEEK,                /* set position for reading */
    ZIP_SOURCE_TELL,                /* get read position */
    ZIP_SOURCE_BEGIN_WRITE,         /* prepare for writing */
    ZIP_SOURCE_COMMIT_WRITE,        /* writing is done */
    ZIP_SOURCE_ROLLBACK_WRITE,      /* discard written changes */
    ZIP_SOURCE_WRITE,               /* write data */
    ZIP_SOURCE_SEEK_WRITE,          /* set position for writing */
    ZIP_SOURCE_TELL_WRITE,          /* get write position */
    ZIP_SOURCE_SUPPORTS,            /* check whether source supports command */
    ZIP_SOURCE_REMOVE,              /* remove file */
    ZIP_SOURCE_RESERVED_1,          /* previously used internally */
    ZIP_SOURCE_BEGIN_WRITE_CLONING, /* like ZIP_SOURCE_BEGIN_WRITE, but keep part of original file */
    ZIP_SOURCE_ACCEPT_EMPTY,        /* whether empty files are valid archives */
    ZIP_SOURCE_GET_FILE_ATTRIBUTES, /* get additional file attributes */
    ZIP_SOURCE_SUPPORTS_REOPEN      /* allow reading from changed entry */
};
typedef enum zip_source_cmd zip_source_cmd_t;

#define ZIP_SOURCE_MAKE_COMMAND_BITMASK(cmd) (((zip_int64_t)1) << (cmd))

#define ZIP_SOURCE_CHECK_SUPPORTED(supported, cmd)  (((supported) & ZIP_SOURCE_MAKE_COMMAND_BITMASK(cmd)) != 0)

/* clang-format off */

#define ZIP_SOURCE_SUPPORTS_READABLE	(ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_OPEN) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_READ) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_CLOSE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_STAT) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_ERROR) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_FREE))

#define ZIP_SOURCE_SUPPORTS_SEEKABLE	(ZIP_SOURCE_SUPPORTS_READABLE \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_SEEK) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_TELL) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_SUPPORTS))

#define ZIP_SOURCE_SUPPORTS_WRITABLE    (ZIP_SOURCE_SUPPORTS_SEEKABLE \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_BEGIN_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_COMMIT_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_ROLLBACK_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_SEEK_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_TELL_WRITE) \
                                         | ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_REMOVE))

/* clang-format on */

/* for use by sources */
struct zip_source_args_seek {
    zip_int64_t offset;
    int whence;
};

typedef struct zip_source_args_seek zip_source_args_seek_t;
#define ZIP_SOURCE_GET_ARGS(type, data, len, error) ((len) < sizeof(type) ? zip_error_set((error), ZIP_ER_INVAL, 0), (type *)NULL : (type *)(data))


/* error information */
/* use zip_error_*() to access */
struct zip_error {
    int zip_err;         /* libzip error code (ZIP_ER_*) */
    int sys_err;         /* copy of errno (E*) or zlib error code */
    char *_Nullable str; /* string representation or NULL */
};

#define ZIP_STAT_NAME 0x0001u
#define ZIP_STAT_INDEX 0x0002u
#define ZIP_STAT_SIZE 0x0004u
#define ZIP_STAT_COMP_SIZE 0x0008u
#define ZIP_STAT_MTIME 0x0010u
#define ZIP_STAT_CRC 0x0020u
#define ZIP_STAT_COMP_METHOD 0x0040u
#define ZIP_STAT_ENCRYPTION_METHOD 0x0080u
#define ZIP_STAT_FLAGS 0x0100u

struct zip_stat {
    zip_uint64_t valid;             /* which fields have valid values */
    const char *_Nullable name;     /* name of the file */
    zip_uint64_t index;             /* index within archive */
    zip_uint64_t size;              /* size of file (uncompressed) */
    zip_uint64_t comp_size;         /* size of file (compressed) */
    time_t mtime;                   /* modification time */
    zip_uint32_t crc;               /* crc of file data */
    zip_uint16_t comp_method;       /* compression method used */
    zip_uint16_t encryption_method; /* encryption method used */
    zip_uint32_t flags;             /* reserved for future use */
};

struct zip_buffer_fragment {
    zip_uint8_t *_Nonnull data;
    zip_uint64_t length;
};

struct zip_file_attributes {
    zip_uint64_t valid;                     /* which fields have valid values */
    zip_uint8_t version;                    /* version of this struct, currently 1 */
    zip_uint8_t host_system;                /* host system on which file was created */
    zip_uint8_t ascii;                      /* flag whether file is ASCII text */
    zip_uint8_t version_needed;             /* minimum version needed to extract file */
    zip_uint32_t external_file_attributes;  /* external file attributes (host-system specific) */
    zip_uint16_t general_purpose_bit_flags; /* general purpose big flags, only some bits are honored */
    zip_uint16_t general_purpose_bit_mask;  /* which bits in general_purpose_bit_flags are valid */
};

#define ZIP_FILE_ATTRIBUTES_HOST_SYSTEM 0x0001u
#define ZIP_FILE_ATTRIBUTES_ASCII 0x0002u
#define ZIP_FILE_ATTRIBUTES_VERSION_NEEDED 0x0004u
#define ZIP_FILE_ATTRIBUTES_EXTERNAL_FILE_ATTRIBUTES 0x0008u
#define ZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS 0x0010u

struct zip;
struct zip_file;
struct zip_source;

typedef struct zip zip_t;
typedef struct zip_error zip_error_t;
typedef struct zip_file zip_file_t;
typedef struct zip_file_attributes zip_file_attributes_t;
typedef struct zip_source zip_source_t;
typedef struct zip_stat zip_stat_t;
typedef struct zip_buffer_fragment zip_buffer_fragment_t;

typedef zip_uint32_t zip_flags_t;

typedef zip_int64_t (*zip_source_callback)(void *_Nullable, void *_Nullable, zip_uint64_t, zip_source_cmd_t);
typedef zip_int64_t (*zip_source_layered_callback)(zip_source_t *_Nonnull, void *_Nullable, void *_Nullable, zip_uint64_t, enum zip_source_cmd);
typedef void (*zip_progress_callback)(zip_t *_Nonnull, double, void *_Nullable);
typedef int (*zip_cancel_callback)(zip_t *_Nonnull, void *_Nullable);

#ifndef ZIP_DISABLE_DEPRECATED
typedef void (*zip_progress_callback_t)(double);
ZIP_EXTERN void zip_register_progress_callback(zip_t *_Nonnull, zip_progress_callback_t _Nullable); /* use zip_register_progress_callback_with_state */

ZIP_EXTERN zip_int64_t zip_add(zip_t *_Nonnull, const char *_Nonnull, zip_source_t *_Nonnull);             /* use zip_file_add */
ZIP_EXTERN zip_int64_t zip_add_dir(zip_t *_Nonnull, const char *_Nonnull);                                 /* use zip_dir_add */
ZIP_EXTERN const char *_Nullable zip_get_file_comment(zip_t *_Nonnull, zip_uint64_t, int *_Nullable, int); /* use zip_file_get_comment */
ZIP_EXTERN int zip_get_num_files(zip_t *_Nonnull);                                                         /* use zip_get_num_entries instead */
ZIP_EXTERN int zip_rename(zip_t *_Nonnull, zip_uint64_t, const char *_Nonnull);                            /* use zip_file_rename */
ZIP_EXTERN int zip_replace(zip_t *_Nonnull, zip_uint64_t, zip_source_t *_Nonnull);                         /* use zip_file_replace */
ZIP_EXTERN int zip_set_file_comment(zip_t *_Nonnull, zip_uint64_t, const char *_Nullable, int);            /* use zip_file_set_comment */
ZIP_EXTERN int zip_error_get_sys_type(int);                                                                /* use zip_error_system_type */
ZIP_EXTERN void zip_error_get(zip_t *_Nonnull, int *_Nullable, int *_Nullable);                            /* use zip_get_error, zip_error_code_zip / zip_error_code_system */
ZIP_EXTERN int zip_error_to_str(char *_Nonnull, zip_uint64_t, int, int);                                   /* use zip_error_init_with_code / zip_error_strerror */
ZIP_EXTERN void zip_file_error_get(zip_file_t *_Nonnull, int *_Nullable, int *_Nullable);                  /* use zip_file_get_error, zip_error_code_zip / zip_error_code_system */
#endif

ZIP_EXTERN int zip_close(zip_t *_Nonnull);
ZIP_EXTERN int zip_delete(zip_t *_Nonnull, zip_uint64_t);
ZIP_EXTERN zip_int64_t zip_dir_add(zip_t *_Nonnull, const char *_Nonnull, zip_flags_t);
ZIP_EXTERN void zip_discard(zip_t *_Nonnull);

ZIP_EXTERN zip_error_t *_Nonnull zip_get_error(zip_t *_Nonnull);
ZIP_EXTERN void zip_error_clear(zip_t *_Nonnull);
ZIP_EXTERN int zip_error_code_zip(const zip_error_t *_Nonnull);
ZIP_EXTERN int zip_error_code_system(const zip_error_t *_Nonnull);
ZIP_EXTERN void zip_error_fini(zip_error_t *_Nonnull);
ZIP_EXTERN void zip_error_init(zip_error_t *_Nonnull);
ZIP_EXTERN void zip_error_init_with_code(zip_error_t *_Nonnull, int);
ZIP_EXTERN void zip_error_set(zip_error_t *_Nullable, int, int);
ZIP_EXTERN void zip_error_set_from_source(zip_error_t *_Nonnull, zip_source_t *_Nullable);
ZIP_EXTERN const char *_Nonnull zip_error_strerror(zip_error_t *_Nonnull);
ZIP_EXTERN int zip_error_system_type(const zip_error_t *_Nonnull);
ZIP_EXTERN zip_int64_t zip_error_to_data(const zip_error_t *_Nonnull, void *_Nonnull, zip_uint64_t);

ZIP_EXTERN int zip_fclose(zip_file_t *_Nonnull);
ZIP_EXTERN zip_t *_Nullable zip_fdopen(int, int, int *_Nullable);
ZIP_EXTERN zip_int64_t zip_file_add(zip_t *_Nonnull, const char *_Nonnull, zip_source_t *_Nonnull, zip_flags_t);
ZIP_EXTERN void zip_file_attributes_init(zip_file_attributes_t *_Nonnull);
ZIP_EXTERN void zip_file_error_clear(zip_file_t *_Nonnull);
ZIP_EXTERN int zip_file_extra_field_delete(zip_t *_Nonnull, zip_uint64_t, zip_uint16_t, zip_flags_t);
ZIP_EXTERN int zip_file_extra_field_delete_by_id(zip_t *_Nonnull, zip_uint64_t, zip_uint16_t, zip_uint16_t, zip_flags_t);
ZIP_EXTERN int zip_file_extra_field_set(zip_t *_Nonnull, zip_uint64_t, zip_uint16_t, zip_uint16_t, const zip_uint8_t *_Nullable, zip_uint16_t, zip_flags_t);
ZIP_EXTERN zip_int16_t zip_file_extra_fields_count(zip_t *_Nonnull, zip_uint64_t, zip_flags_t);
ZIP_EXTERN zip_int16_t zip_file_extra_fields_count_by_id(zip_t *_Nonnull, zip_uint64_t, zip_uint16_t, zip_flags_t);
ZIP_EXTERN const zip_uint8_t *_Nullable zip_file_extra_field_get(zip_t *_Nonnull, zip_uint64_t, zip_uint16_t, zip_uint16_t *_Nullable, zip_uint16_t *_Nullable, zip_flags_t);
ZIP_EXTERN const zip_uint8_t *_Nullable zip_file_extra_field_get_by_id(zip_t *_Nonnull, zip_uint64_t, zip_uint16_t, zip_uint16_t, zip_uint16_t *_Nullable, zip_flags_t);
ZIP_EXTERN const char *_Nullable zip_file_get_comment(zip_t *_Nonnull, zip_uint64_t, zip_uint32_t *_Nullable, zip_flags_t);
ZIP_EXTERN zip_error_t *_Nonnull zip_file_get_error(zip_file_t *_Nonnull);
ZIP_EXTERN int zip_file_get_external_attributes(zip_t *_Nonnull, zip_uint64_t, zip_flags_t, zip_uint8_t *_Nullable, zip_uint32_t *_Nullable);
ZIP_EXTERN int zip_file_is_seekable(zip_file_t *_Nonnull);
ZIP_EXTERN int zip_file_rename(zip_t *_Nonnull, zip_uint64_t, const char *_Nonnull, zip_flags_t);
ZIP_EXTERN int zip_file_replace(zip_t *_Nonnull, zip_uint64_t, zip_source_t *_Nonnull, zip_flags_t);
ZIP_EXTERN int zip_file_set_comment(zip_t *_Nonnull, zip_uint64_t, const char *_Nullable, zip_uint16_t, zip_flags_t);
ZIP_EXTERN int zip_file_set_dostime(zip_t *_Nonnull, zip_uint64_t, zip_uint16_t, zip_uint16_t, zip_flags_t);
ZIP_EXTERN int zip_file_set_encryption(zip_t *_Nonnull, zip_uint64_t, zip_uint16_t, const char *_Nullable);
ZIP_EXTERN int zip_file_set_external_attributes(zip_t *_Nonnull, zip_uint64_t, zip_flags_t, zip_uint8_t, zip_uint32_t);
ZIP_EXTERN int zip_file_set_mtime(zip_t *_Nonnull, zip_uint64_t, time_t, zip_flags_t);
ZIP_EXTERN const char *_Nonnull zip_file_strerror(zip_file_t *_Nonnull);
ZIP_EXTERN zip_file_t *_Nullable zip_fopen(zip_t *_Nonnull, const char *_Nonnull, zip_flags_t);
ZIP_EXTERN zip_file_t *_Nullable zip_fopen_encrypted(zip_t *_Nonnull, const char *_Nonnull, zip_flags_t, const char *_Nullable);
ZIP_EXTERN zip_file_t *_Nullable zip_fopen_index(zip_t *_Nonnull, zip_uint64_t, zip_flags_t);
ZIP_EXTERN zip_file_t *_Nullable zip_fopen_index_encrypted(zip_t *_Nonnull, zip_uint64_t, zip_flags_t, const char *_Nullable);
ZIP_EXTERN zip_int64_t zip_fread(zip_file_t *_Nonnull, void *_Nonnull, zip_uint64_t);
ZIP_EXTERN zip_int8_t zip_fseek(zip_file_t *_Nonnull, zip_int64_t, int);
ZIP_EXTERN zip_int64_t zip_ftell(zip_file_t *_Nonnull);
ZIP_EXTERN const char *_Nullable zip_get_archive_comment(zip_t *_Nonnull, int *_Nullable, zip_flags_t);
ZIP_EXTERN int zip_get_archive_flag(zip_t *_Nonnull, zip_flags_t, zip_flags_t);
ZIP_EXTERN const char *_Nullable zip_get_name(zip_t *_Nonnull, zip_uint64_t, zip_flags_t);
ZIP_EXTERN zip_int64_t zip_get_num_entries(zip_t *_Nonnull, zip_flags_t);
ZIP_EXTERN const char *_Nonnull zip_libzip_version(void);
ZIP_EXTERN zip_int64_t zip_name_locate(zip_t *_Nonnull, const char *_Nonnull, zip_flags_t);
ZIP_EXTERN zip_t *_Nullable zip_open(const char *_Nonnull, int, int *_Nullable);
ZIP_EXTERN zip_t *_Nullable zip_open_from_source(zip_source_t *_Nonnull, int, zip_error_t *_Nullable);
ZIP_EXTERN int zip_register_progress_callback_with_state(zip_t *_Nonnull, double, zip_progress_callback _Nullable, void (*_Nullable)(void *_Nullable), void *_Nullable);
ZIP_EXTERN int zip_register_cancel_callback_with_state(zip_t *_Nonnull, zip_cancel_callback _Nullable, void (*_Nullable)(void *_Nullable), void *_Nullable);
ZIP_EXTERN int zip_set_archive_comment(zip_t *_Nonnull, const char *_Nullable, zip_uint16_t);
ZIP_EXTERN int zip_set_archive_flag(zip_t *_Nonnull, zip_flags_t, int);
ZIP_EXTERN int zip_set_default_password(zip_t *_Nonnull, const char *_Nullable);
ZIP_EXTERN int zip_set_file_compression(zip_t *_Nonnull, zip_uint64_t, zip_int32_t, zip_uint32_t);
ZIP_EXTERN int zip_source_begin_write(zip_source_t *_Nonnull);
ZIP_EXTERN int zip_source_begin_write_cloning(zip_source_t *_Nonnull, zip_uint64_t);
ZIP_EXTERN zip_source_t *_Nullable zip_source_buffer(zip_t *_Nonnull, const void *_Nullable, zip_uint64_t, int);
ZIP_EXTERN zip_source_t *_Nullable zip_source_buffer_create(const void *_Nullable, zip_uint64_t, int, zip_error_t *_Nullable);
ZIP_EXTERN zip_source_t *_Nullable zip_source_buffer_fragment(zip_t *_Nonnull, const zip_buffer_fragment_t *_Nonnull, zip_uint64_t, int);
ZIP_EXTERN zip_source_t *_Nullable zip_source_buffer_fragment_create(const zip_buffer_fragment_t *_Nullable, zip_uint64_t, int, zip_error_t *_Nullable);
ZIP_EXTERN int zip_source_close(zip_source_t *_Nonnull);
ZIP_EXTERN int zip_source_commit_write(zip_source_t *_Nonnull);
ZIP_EXTERN zip_error_t *_Nonnull zip_source_error(zip_source_t *_Nonnull);
ZIP_EXTERN zip_source_t *_Nullable zip_source_file(zip_t *_Nonnull, const char *_Nonnull, zip_uint64_t, zip_int64_t);
ZIP_EXTERN zip_source_t *_Nullable zip_source_file_create(const char *_Nonnull, zip_uint64_t, zip_int64_t, zip_error_t *_Nullable);
ZIP_EXTERN zip_source_t *_Nullable zip_source_filep(zip_t *_Nonnull, FILE *_Nonnull, zip_uint64_t, zip_int64_t);
ZIP_EXTERN zip_source_t *_Nullable zip_source_filep_create(FILE *_Nonnull, zip_uint64_t, zip_int64_t, zip_error_t *_Nullable);
ZIP_EXTERN void zip_source_free(zip_source_t *_Nullable);
ZIP_EXTERN zip_source_t *_Nullable zip_source_function(zip_t *_Nonnull, zip_source_callback _Nonnull, void *_Nullable);
ZIP_EXTERN zip_source_t *_Nullable zip_source_function_create(zip_source_callback _Nonnull, void *_Nullable, zip_error_t *_Nullable);
ZIP_EXTERN int zip_source_get_file_attributes(zip_source_t *_Nonnull, zip_file_attributes_t *_Nonnull);
ZIP_EXTERN int zip_source_is_deleted(zip_source_t *_Nonnull);
ZIP_EXTERN void zip_source_keep(zip_source_t *_Nonnull);
ZIP_EXTERN zip_source_t *_Nullable zip_source_layered(zip_t *_Nullable, zip_source_t *_Nonnull, zip_source_layered_callback _Nonnull, void *_Nullable);
ZIP_EXTERN zip_source_t *_Nullable zip_source_layered_create(zip_source_t *_Nonnull, zip_source_layered_callback _Nonnull, void *_Nullable, zip_error_t *_Nullable);
ZIP_EXTERN zip_int64_t zip_source_make_command_bitmap(zip_source_cmd_t, ...);
ZIP_EXTERN int zip_source_open(zip_source_t *_Nonnull);
ZIP_EXTERN zip_int64_t zip_source_pass_to_lower_layer(zip_source_t *_Nonnull, void *_Nullable, zip_uint64_t, zip_source_cmd_t);
ZIP_EXTERN zip_int64_t zip_source_read(zip_source_t *_Nonnull, void *_Nonnull, zip_uint64_t);
ZIP_EXTERN void zip_source_rollback_write(zip_source_t *_Nonnull);
ZIP_EXTERN int zip_source_seek(zip_source_t *_Nonnull, zip_int64_t, int);
ZIP_EXTERN zip_int64_t zip_source_seek_compute_offset(zip_uint64_t, zip_uint64_t, void *_Nonnull, zip_uint64_t, zip_error_t *_Nullable);
ZIP_EXTERN int zip_source_seek_write(zip_source_t *_Nonnull, zip_int64_t, int);
ZIP_EXTERN int zip_source_stat(zip_source_t *_Nonnull, zip_stat_t *_Nonnull);
ZIP_EXTERN zip_int64_t zip_source_tell(zip_source_t *_Nonnull);
ZIP_EXTERN zip_int64_t zip_source_tell_write(zip_source_t *_Nonnull);
#ifdef _WIN32
ZIP_EXTERN zip_source_t *_Nullable zip_source_win32a(zip_t *_Nonnull, const char *_Nonnull, zip_uint64_t, zip_int64_t);
ZIP_EXTERN zip_source_t *_Nullable zip_source_win32a_create(const char *_Nonnull, zip_uint64_t, zip_int64_t, zip_error_t *_Nullable);
ZIP_EXTERN zip_source_t *_Nullable zip_source_win32handle(zip_t *_Nonnull, void *_Nonnull, zip_uint64_t, zip_int64_t);
ZIP_EXTERN zip_source_t *_Nullable zip_source_win32handle_create(void *_Nonnull, zip_uint64_t, zip_int64_t, zip_error_t *_Nullable);
ZIP_EXTERN zip_source_t *_Nullable zip_source_win32w(zip_t *_Nonnull, const wchar_t *_Nonnull, zip_uint64_t, zip_int64_t);
ZIP_EXTERN zip_source_t *_Nullable zip_source_win32w_create(const wchar_t *_Nonnull, zip_uint64_t, zip_int64_t, zip_error_t *_Nullable);
#endif
ZIP_EXTERN zip_source_t *_Nullable zip_source_window_create(zip_source_t *_Nonnull, zip_uint64_t, zip_int64_t, zip_error_t *_Nullable);
ZIP_EXTERN zip_int64_t zip_source_write(zip_source_t *_Nonnull, const void *_Nullable, zip_uint64_t);
ZIP_EXTERN zip_source_t *_Nullable zip_source_zip(zip_t *_Nonnull, zip_t *_Nonnull, zip_uint64_t, zip_flags_t, zip_uint64_t, zip_int64_t);
ZIP_EXTERN zip_source_t *_Nullable zip_source_zip_create(zip_t *_Nonnull, zip_uint64_t, zip_flags_t, zip_uint64_t, zip_int64_t, zip_error_t *_Nullable);
ZIP_EXTERN int zip_stat(zip_t *_Nonnull, const char *_Nonnull, zip_flags_t, zip_stat_t *_Nonnull);
ZIP_EXTERN int zip_stat_index(zip_t *_Nonnull, zip_uint64_t, zip_flags_t, zip_stat_t *_Nonnull);
ZIP_EXTERN void zip_stat_init(zip_stat_t *_Nonnull);
ZIP_EXTERN const char *_Nonnull zip_strerror(zip_t *_Nonnull);
ZIP_EXTERN int zip_unchange(zip_t *_Nonnull, zip_uint64_t);
ZIP_EXTERN int zip_unchange_all(zip_t *_Nonnull);
ZIP_EXTERN int zip_unchange_archive(zip_t *_Nonnull);
ZIP_EXTERN int zip_compression_method_supported(zip_int32_t method, int compress);
ZIP_EXTERN int zip_encryption_method_supported(zip_uint16_t method, int encode);

#ifdef __cplusplus
}
#endif

#endif /* _HAD_ZIP_H */
