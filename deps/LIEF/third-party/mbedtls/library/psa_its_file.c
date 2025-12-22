/*
 *  PSA ITS simulator over stdio files.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_PSA_ITS_FILE_C)

#include "mbedtls/platform.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include "psa_crypto_its.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if !defined(PSA_ITS_STORAGE_PREFIX)
#define PSA_ITS_STORAGE_PREFIX ""
#endif

#define PSA_ITS_STORAGE_FILENAME_PATTERN "%08x%08x"
#define PSA_ITS_STORAGE_SUFFIX ".psa_its"
#define PSA_ITS_STORAGE_FILENAME_LENGTH         \
    (sizeof(PSA_ITS_STORAGE_PREFIX) - 1 +    /*prefix without terminating 0*/ \
     16 +  /*UID (64-bit number in hex)*/                               \
     sizeof(PSA_ITS_STORAGE_SUFFIX) - 1 +    /*suffix without terminating 0*/ \
     1 /*terminating null byte*/)
#define PSA_ITS_STORAGE_TEMP \
    PSA_ITS_STORAGE_PREFIX "tempfile" PSA_ITS_STORAGE_SUFFIX

/* The maximum value of psa_storage_info_t.size */
#define PSA_ITS_MAX_SIZE 0xffffffff

#define PSA_ITS_MAGIC_STRING "PSA\0ITS\0"
#define PSA_ITS_MAGIC_LENGTH 8

/* As rename fails on Windows if the new filepath already exists,
 * use MoveFileExA with the MOVEFILE_REPLACE_EXISTING flag instead.
 * Returns 0 on success, nonzero on failure. */
#if defined(_WIN32)
#define rename_replace_existing(oldpath, newpath) \
    (!MoveFileExA(oldpath, newpath, MOVEFILE_REPLACE_EXISTING))
#else
#define rename_replace_existing(oldpath, newpath) rename(oldpath, newpath)
#endif

typedef struct {
    uint8_t magic[PSA_ITS_MAGIC_LENGTH];
    uint8_t size[sizeof(uint32_t)];
    uint8_t flags[sizeof(psa_storage_create_flags_t)];
} psa_its_file_header_t;

static void psa_its_fill_filename(psa_storage_uid_t uid, char *filename)
{
    /* Break up the UID into two 32-bit pieces so as not to rely on
     * long long support in snprintf. */
    mbedtls_snprintf(filename, PSA_ITS_STORAGE_FILENAME_LENGTH,
                     "%s" PSA_ITS_STORAGE_FILENAME_PATTERN "%s",
                     PSA_ITS_STORAGE_PREFIX,
                     (unsigned) (uid >> 32),
                     (unsigned) (uid & 0xffffffff),
                     PSA_ITS_STORAGE_SUFFIX);
}

static psa_status_t psa_its_read_file(psa_storage_uid_t uid,
                                      struct psa_storage_info_t *p_info,
                                      FILE **p_stream)
{
    char filename[PSA_ITS_STORAGE_FILENAME_LENGTH];
    psa_its_file_header_t header;
    size_t n;

    *p_stream = NULL;
    psa_its_fill_filename(uid, filename);
    *p_stream = fopen(filename, "rb");
    if (*p_stream == NULL) {
        return PSA_ERROR_DOES_NOT_EXIST;
    }

    /* Ensure no stdio buffering of secrets, as such buffers cannot be wiped. */
    mbedtls_setbuf(*p_stream, NULL);

    n = fread(&header, 1, sizeof(header), *p_stream);
    if (n != sizeof(header)) {
        return PSA_ERROR_DATA_CORRUPT;
    }
    if (memcmp(header.magic, PSA_ITS_MAGIC_STRING,
               PSA_ITS_MAGIC_LENGTH) != 0) {
        return PSA_ERROR_DATA_CORRUPT;
    }

    p_info->size  = MBEDTLS_GET_UINT32_LE(header.size, 0);
    p_info->flags = MBEDTLS_GET_UINT32_LE(header.flags, 0);

    return PSA_SUCCESS;
}

psa_status_t psa_its_get_info(psa_storage_uid_t uid,
                              struct psa_storage_info_t *p_info)
{
    psa_status_t status;
    FILE *stream = NULL;
    status = psa_its_read_file(uid, p_info, &stream);
    if (stream != NULL) {
        fclose(stream);
    }
    return status;
}

psa_status_t psa_its_get(psa_storage_uid_t uid,
                         uint32_t data_offset,
                         uint32_t data_length,
                         void *p_data,
                         size_t *p_data_length)
{
    psa_status_t status;
    FILE *stream = NULL;
    size_t n;
    struct psa_storage_info_t info;

    status = psa_its_read_file(uid, &info, &stream);
    if (status != PSA_SUCCESS) {
        goto exit;
    }
    status = PSA_ERROR_INVALID_ARGUMENT;
    if (data_offset + data_length < data_offset) {
        goto exit;
    }
#if SIZE_MAX < 0xffffffff
    if (data_offset + data_length > SIZE_MAX) {
        goto exit;
    }
#endif
    if (data_offset + data_length > info.size) {
        goto exit;
    }

    status = PSA_ERROR_STORAGE_FAILURE;
#if LONG_MAX < 0xffffffff
    while (data_offset > LONG_MAX) {
        if (fseek(stream, LONG_MAX, SEEK_CUR) != 0) {
            goto exit;
        }
        data_offset -= LONG_MAX;
    }
#endif
    if (fseek(stream, data_offset, SEEK_CUR) != 0) {
        goto exit;
    }
    n = fread(p_data, 1, data_length, stream);
    if (n != data_length) {
        goto exit;
    }
    status = PSA_SUCCESS;
    if (p_data_length != NULL) {
        *p_data_length = n;
    }

exit:
    if (stream != NULL) {
        fclose(stream);
    }
    return status;
}

psa_status_t psa_its_set(psa_storage_uid_t uid,
                         uint32_t data_length,
                         const void *p_data,
                         psa_storage_create_flags_t create_flags)
{
    if (uid == 0) {
        return PSA_ERROR_INVALID_HANDLE;
    }

    psa_status_t status = PSA_ERROR_STORAGE_FAILURE;
    char filename[PSA_ITS_STORAGE_FILENAME_LENGTH];
    FILE *stream = NULL;
    psa_its_file_header_t header;
    size_t n;

    memcpy(header.magic, PSA_ITS_MAGIC_STRING, PSA_ITS_MAGIC_LENGTH);
    MBEDTLS_PUT_UINT32_LE(data_length, header.size, 0);
    MBEDTLS_PUT_UINT32_LE(create_flags, header.flags, 0);

    psa_its_fill_filename(uid, filename);
    stream = fopen(PSA_ITS_STORAGE_TEMP, "wb");

    if (stream == NULL) {
        goto exit;
    }

    /* Ensure no stdio buffering of secrets, as such buffers cannot be wiped. */
    mbedtls_setbuf(stream, NULL);

    status = PSA_ERROR_INSUFFICIENT_STORAGE;
    n = fwrite(&header, 1, sizeof(header), stream);
    if (n != sizeof(header)) {
        goto exit;
    }
    if (data_length != 0) {
        n = fwrite(p_data, 1, data_length, stream);
        if (n != data_length) {
            goto exit;
        }
    }
    status = PSA_SUCCESS;

exit:
    if (stream != NULL) {
        int ret = fclose(stream);
        if (status == PSA_SUCCESS && ret != 0) {
            status = PSA_ERROR_INSUFFICIENT_STORAGE;
        }
    }
    if (status == PSA_SUCCESS) {
        if (rename_replace_existing(PSA_ITS_STORAGE_TEMP, filename) != 0) {
            status = PSA_ERROR_STORAGE_FAILURE;
        }
    }
    /* The temporary file may still exist, but only in failure cases where
     * we're already reporting an error. So there's nothing we can do on
     * failure. If the function succeeded, and in some error cases, the
     * temporary file doesn't exist and so remove() is expected to fail.
     * Thus we just ignore the return status of remove(). */
    (void) remove(PSA_ITS_STORAGE_TEMP);
    return status;
}

psa_status_t psa_its_remove(psa_storage_uid_t uid)
{
    char filename[PSA_ITS_STORAGE_FILENAME_LENGTH];
    FILE *stream;
    psa_its_fill_filename(uid, filename);
    stream = fopen(filename, "rb");
    if (stream == NULL) {
        return PSA_ERROR_DOES_NOT_EXIST;
    }
    fclose(stream);
    if (remove(filename) != 0) {
        return PSA_ERROR_STORAGE_FAILURE;
    }
    return PSA_SUCCESS;
}

#endif /* MBEDTLS_PSA_ITS_FILE_C */
