/*
  zip_close.c -- close zip archive and update changes
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


#include "zipint.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif


static int add_data(zip_t *, zip_source_t *, zip_dirent_t *, zip_uint32_t);
static int copy_data(zip_t *, zip_uint64_t);
static int copy_source(zip_t *, zip_source_t *, zip_int64_t);
static int write_cdir(zip_t *, const zip_filelist_t *, zip_uint64_t);
static int write_data_descriptor(zip_t *za, const zip_dirent_t *dirent, int is_zip64);

ZIP_EXTERN int
zip_close(zip_t *za) {
    zip_uint64_t i, j, survivors, unchanged_offset;
    zip_int64_t off;
    int error;
    zip_filelist_t *filelist;
    int changed;

    if (za == NULL)
        return -1;

    changed = _zip_changed(za, &survivors);

    /* don't create zip files with no entries */
    if (survivors == 0) {
        if ((za->open_flags & ZIP_TRUNCATE) || changed) {
            if (zip_source_remove(za->src) < 0) {
                if (!((zip_error_code_zip(zip_source_error(za->src)) == ZIP_ER_REMOVE) && (zip_error_code_system(zip_source_error(za->src)) == ENOENT))) {
                    zip_error_set_from_source(&za->error, za->src);
                    return -1;
                }
            }
        }
        zip_discard(za);
        return 0;
    }

    if (!changed) {
        zip_discard(za);
        return 0;
    }

    if (survivors > za->nentry) {
        zip_error_set(&za->error, ZIP_ER_INTERNAL, 0);
        return -1;
    }

    if ((filelist = (zip_filelist_t *)malloc(sizeof(filelist[0]) * (size_t)survivors)) == NULL)
        return -1;

    unchanged_offset = ZIP_UINT64_MAX;
    /* create list of files with index into original archive  */
    for (i = j = 0; i < za->nentry; i++) {
        if (za->entry[i].orig != NULL && ZIP_ENTRY_HAS_CHANGES(&za->entry[i])) {
            unchanged_offset = ZIP_MIN(unchanged_offset, za->entry[i].orig->offset);
        }
        if (za->entry[i].deleted) {
            continue;
        }

        if (j >= survivors) {
            free(filelist);
            zip_error_set(&za->error, ZIP_ER_INTERNAL, 0);
            return -1;
        }

        filelist[j].idx = i;
        j++;
    }
    if (j < survivors) {
        free(filelist);
        zip_error_set(&za->error, ZIP_ER_INTERNAL, 0);
        return -1;
    }

    if ((zip_source_supports(za->src) & ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_BEGIN_WRITE_CLONING)) == 0) {
        unchanged_offset = 0;
    }
    else {
        if (unchanged_offset == ZIP_UINT64_MAX) {
            /* we're keeping all file data, find the end of the last one */
            zip_uint64_t last_index = ZIP_UINT64_MAX;
            unchanged_offset = 0;

            for (i = 0; i < za->nentry; i++) {
                if (za->entry[i].orig != NULL) {
                    if (za->entry[i].orig->offset >= unchanged_offset) {
                        unchanged_offset = za->entry[i].orig->offset;
                        last_index = i;
                    }
                }
            }
            if (last_index != ZIP_UINT64_MAX) {
                if ((unchanged_offset = _zip_file_get_end(za, last_index, &za->error)) == 0) {
                    free(filelist);
                    return -1;
                }
            }
        }
        if (unchanged_offset > 0) {
            if (zip_source_begin_write_cloning(za->src, unchanged_offset) < 0) {
                /* cloning not supported, need to copy everything */
                unchanged_offset = 0;
            }
        }
    }
    if (unchanged_offset == 0) {
        if (zip_source_begin_write(za->src) < 0) {
            zip_error_set_from_source(&za->error, za->src);
            free(filelist);
            return -1;
        }
    }

    if (_zip_progress_start(za->progress) != 0) {
        zip_error_set(&za->error, ZIP_ER_CANCELLED, 0);
        zip_source_rollback_write(za->src);
        free(filelist);
        return -1;
    }
    error = 0;
    for (j = 0; j < survivors; j++) {
        int new_data;
        zip_entry_t *entry;
        zip_dirent_t *de;

        if (_zip_progress_subrange(za->progress, (double)j / (double)survivors, (double)(j + 1) / (double)survivors) != 0) {
            zip_error_set(&za->error, ZIP_ER_CANCELLED, 0);
            error = 1;
            break;
        }

        i = filelist[j].idx;
        entry = za->entry + i;

        if (entry->orig != NULL && entry->orig->offset < unchanged_offset) {
            /* already implicitly copied by cloning */
            continue;
        }

        new_data = (ZIP_ENTRY_DATA_CHANGED(entry) || ZIP_ENTRY_CHANGED(entry, ZIP_DIRENT_COMP_METHOD) || ZIP_ENTRY_CHANGED(entry, ZIP_DIRENT_ENCRYPTION_METHOD));

        /* create new local directory entry */
        if (entry->changes == NULL) {
            if ((entry->changes = _zip_dirent_clone(entry->orig)) == NULL) {
                zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
                error = 1;
                break;
            }
        }
        de = entry->changes;

        if (_zip_read_local_ef(za, i) < 0) {
            error = 1;
            break;
        }

        if ((off = zip_source_tell_write(za->src)) < 0) {
            zip_error_set_from_source(&za->error, za->src);
            error = 1;
            break;
        }
        de->offset = (zip_uint64_t)off;

        if (new_data) {
            zip_source_t *zs;

            zs = NULL;
            if (!ZIP_ENTRY_DATA_CHANGED(entry)) {
                if ((zs = _zip_source_zip_new(za, i, ZIP_FL_UNCHANGED, 0, 0, NULL, &za->error)) == NULL) {
                    error = 1;
                    break;
                }
            }

            /* add_data writes dirent */
            if (add_data(za, zs ? zs : entry->source, de, entry->changes ? entry->changes->changed : 0) < 0) {
                error = 1;
                if (zs)
                    zip_source_free(zs);
                break;
            }
            if (zs)
                zip_source_free(zs);
        }
        else {
            zip_uint64_t offset;

            if (de->encryption_method != ZIP_EM_TRAD_PKWARE) {
                /* when copying data, all sizes are known -> no data descriptor needed */
                /* except for PKWare encryption, where removing the data descriptor breaks password validation */
                de->bitflags &= (zip_uint16_t)~ZIP_GPBF_DATA_DESCRIPTOR;
            }
            if (_zip_dirent_write(za, de, ZIP_FL_LOCAL) < 0) {
                error = 1;
                break;
            }
            if ((offset = _zip_file_get_offset(za, i, &za->error)) == 0) {
                error = 1;
                break;
            }
            if (zip_source_seek(za->src, (zip_int64_t)offset, SEEK_SET) < 0) {
                zip_error_set_from_source(&za->error, za->src);
                error = 1;
                break;
            }
            if (copy_data(za, de->comp_size) < 0) {
                error = 1;
                break;
            }

            if (de->bitflags & ZIP_GPBF_DATA_DESCRIPTOR) {
                if (write_data_descriptor(za, de, _zip_dirent_needs_zip64(de, 0)) < 0) {
                    error = 1;
                    break;
                }
            }
        }
    }

    if (!error) {
        if (write_cdir(za, filelist, survivors) < 0)
            error = 1;
    }

    free(filelist);

    if (!error) {
        if (zip_source_commit_write(za->src) != 0) {
            zip_error_set_from_source(&za->error, za->src);
            error = 1;
        }
        _zip_progress_end(za->progress);
    }

    if (error) {
        zip_source_rollback_write(za->src);
        return -1;
    }

    zip_discard(za);

    return 0;
}


static int
add_data(zip_t *za, zip_source_t *src, zip_dirent_t *de, zip_uint32_t changed) {
    zip_int64_t offstart, offdata, offend, data_length;
    zip_stat_t st;
    zip_file_attributes_t attributes;
    zip_source_t *src_final, *src_tmp;
    int ret;
    int is_zip64;
    zip_flags_t flags;
    bool needs_recompress, needs_decompress, needs_crc, needs_compress, needs_reencrypt, needs_decrypt, needs_encrypt;

    if (zip_source_stat(src, &st) < 0) {
        zip_error_set_from_source(&za->error, src);
        return -1;
    }

    if ((st.valid & ZIP_STAT_COMP_METHOD) == 0) {
        st.valid |= ZIP_STAT_COMP_METHOD;
        st.comp_method = ZIP_CM_STORE;
    }

    if (ZIP_CM_IS_DEFAULT(de->comp_method) && st.comp_method != ZIP_CM_STORE)
        de->comp_method = st.comp_method;
    else if (de->comp_method == ZIP_CM_STORE && (st.valid & ZIP_STAT_SIZE)) {
        st.valid |= ZIP_STAT_COMP_SIZE;
        st.comp_size = st.size;
    }
    else {
        /* we'll recompress */
        st.valid &= ~ZIP_STAT_COMP_SIZE;
    }

    if ((st.valid & ZIP_STAT_ENCRYPTION_METHOD) == 0) {
        st.valid |= ZIP_STAT_ENCRYPTION_METHOD;
        st.encryption_method = ZIP_EM_NONE;
    }

    flags = ZIP_EF_LOCAL;

    if ((st.valid & ZIP_STAT_SIZE) == 0) {
        flags |= ZIP_FL_FORCE_ZIP64;
        data_length = -1;
    }
    else {
        de->uncomp_size = st.size;
        /* this is technically incorrect (copy_source counts compressed data), but it's the best we have */
        data_length = (zip_int64_t)st.size;

        if ((st.valid & ZIP_STAT_COMP_SIZE) == 0) {
            zip_uint64_t max_compressed_size;
            zip_uint16_t compression_method = ZIP_CM_ACTUAL(de->comp_method);

            if (compression_method == ZIP_CM_STORE) {
                max_compressed_size = st.size;
            }
            else {
                zip_compression_algorithm_t *algorithm = _zip_get_compression_algorithm(compression_method, true);
                if (algorithm == NULL) {
                    max_compressed_size = ZIP_UINT64_MAX;
                }
                else {
                    max_compressed_size = algorithm->maximum_compressed_size(st.size);
                }
            }

            if (max_compressed_size > 0xffffffffu) {
                flags |= ZIP_FL_FORCE_ZIP64;
            }
        }
        else {
            de->comp_size = st.comp_size;
            data_length = (zip_int64_t)st.comp_size;
        }
    }

    if ((offstart = zip_source_tell_write(za->src)) < 0) {
        zip_error_set_from_source(&za->error, za->src);
        return -1;
    }

    /* as long as we don't support non-seekable output, clear data descriptor bit */
    de->bitflags &= (zip_uint16_t)~ZIP_GPBF_DATA_DESCRIPTOR;
    if ((is_zip64 = _zip_dirent_write(za, de, flags)) < 0) {
        return -1;
    }

    needs_recompress = st.comp_method != ZIP_CM_ACTUAL(de->comp_method);
    needs_decompress = needs_recompress && (st.comp_method != ZIP_CM_STORE);
    /* in these cases we can compute the CRC ourselves, so we do */
    needs_crc = (st.comp_method == ZIP_CM_STORE) || needs_decompress;
    needs_compress = needs_recompress && (de->comp_method != ZIP_CM_STORE);

    needs_reencrypt = needs_recompress || (de->changed & ZIP_DIRENT_PASSWORD) || (de->encryption_method != st.encryption_method);
    needs_decrypt = needs_reencrypt && (st.encryption_method != ZIP_EM_NONE);
    needs_encrypt = needs_reencrypt && (de->encryption_method != ZIP_EM_NONE);

    src_final = src;
    zip_source_keep(src_final);

    if (needs_decrypt) {
        zip_encryption_implementation impl;

        if ((impl = _zip_get_encryption_implementation(st.encryption_method, ZIP_CODEC_DECODE)) == NULL) {
            zip_error_set(&za->error, ZIP_ER_ENCRNOTSUPP, 0);
            zip_source_free(src_final);
            return -1;
        }
        if ((src_tmp = impl(za, src_final, st.encryption_method, ZIP_CODEC_DECODE, za->default_password)) == NULL) {
            /* error set by impl */
            zip_source_free(src_final);
            return -1;
        }

        zip_source_free(src_final);
        src_final = src_tmp;
    }

    if (needs_decompress) {
        if ((src_tmp = zip_source_decompress(za, src_final, st.comp_method)) == NULL) {
            zip_source_free(src_final);
            return -1;
        }

        zip_source_free(src_final);
        src_final = src_tmp;
    }

    if (needs_crc) {
        if ((src_tmp = zip_source_crc_create(src_final, 0, &za->error)) == NULL) {
            zip_source_free(src_final);
            return -1;
        }

        zip_source_free(src_final);
        src_final = src_tmp;
    }

    if (needs_compress) {
        if ((src_tmp = zip_source_compress(za, src_final, de->comp_method, de->compression_level)) == NULL) {
            zip_source_free(src_final);
            return -1;
        }

        zip_source_free(src_final);
        src_final = src_tmp;
    }


    if (needs_encrypt) {
        zip_encryption_implementation impl;
        const char *password = NULL;

        if (de->password) {
            password = de->password;
        }
        else if (za->default_password) {
            password = za->default_password;
        }

        if ((impl = _zip_get_encryption_implementation(de->encryption_method, ZIP_CODEC_ENCODE)) == NULL) {
            zip_error_set(&za->error, ZIP_ER_ENCRNOTSUPP, 0);
            zip_source_free(src_final);
            return -1;
        }

        if (de->encryption_method == ZIP_EM_TRAD_PKWARE) {
            de->bitflags |= ZIP_GPBF_DATA_DESCRIPTOR;

            /* PKWare encryption uses last_mod, make sure it gets the right value. */
            if (de->changed & ZIP_DIRENT_LAST_MOD) {
                zip_stat_t st_mtime;
                zip_stat_init(&st_mtime);
                st_mtime.valid = ZIP_STAT_MTIME;
                st_mtime.mtime = de->last_mod;
                if ((src_tmp = _zip_source_window_new(src_final, 0, -1, &st_mtime, NULL, NULL, 0, &za->error)) == NULL) {
                    zip_source_free(src_final);
                    return -1;
                }
                zip_source_free(src_final);
                src_final = src_tmp;
            }
        }

        if ((src_tmp = impl(za, src_final, de->encryption_method, ZIP_CODEC_ENCODE, password)) == NULL) {
            /* error set by impl */
            zip_source_free(src_final);
            return -1;
        }

        zip_source_free(src_final);
        src_final = src_tmp;
    }


    if ((offdata = zip_source_tell_write(za->src)) < 0) {
        zip_error_set_from_source(&za->error, za->src);
        return -1;
    }

    ret = copy_source(za, src_final, data_length);

    if (zip_source_stat(src_final, &st) < 0) {
        zip_error_set_from_source(&za->error, src_final);
        ret = -1;
    }

    if (zip_source_get_file_attributes(src_final, &attributes) != 0) {
        zip_error_set_from_source(&za->error, src_final);
        ret = -1;
    }

    zip_source_free(src_final);

    if (ret < 0) {
        return -1;
    }

    if ((offend = zip_source_tell_write(za->src)) < 0) {
        zip_error_set_from_source(&za->error, za->src);
        return -1;
    }

    if (zip_source_seek_write(za->src, offstart, SEEK_SET) < 0) {
        zip_error_set_from_source(&za->error, za->src);
        return -1;
    }

    if ((st.valid & (ZIP_STAT_COMP_METHOD | ZIP_STAT_CRC | ZIP_STAT_SIZE)) != (ZIP_STAT_COMP_METHOD | ZIP_STAT_CRC | ZIP_STAT_SIZE)) {
        zip_error_set(&za->error, ZIP_ER_INTERNAL, 0);
        return -1;
    }

    if ((de->changed & ZIP_DIRENT_LAST_MOD) == 0) {
        if (st.valid & ZIP_STAT_MTIME)
            de->last_mod = st.mtime;
        else
            time(&de->last_mod);
    }
    de->comp_method = st.comp_method;
    de->crc = st.crc;
    de->uncomp_size = st.size;
    de->comp_size = (zip_uint64_t)(offend - offdata);
    _zip_dirent_apply_attributes(de, &attributes, (flags & ZIP_FL_FORCE_ZIP64) != 0, changed);

    if ((ret = _zip_dirent_write(za, de, flags)) < 0)
        return -1;

    if (is_zip64 != ret) {
        /* Zip64 mismatch between preliminary file header written before data and final file header written afterwards */
        zip_error_set(&za->error, ZIP_ER_INTERNAL, 0);
        return -1;
    }

    if (zip_source_seek_write(za->src, offend, SEEK_SET) < 0) {
        zip_error_set_from_source(&za->error, za->src);
        return -1;
    }

    if (de->bitflags & ZIP_GPBF_DATA_DESCRIPTOR) {
        if (write_data_descriptor(za, de, is_zip64) < 0) {
            return -1;
        }
    }

    return 0;
}


static int
copy_data(zip_t *za, zip_uint64_t len) {
    DEFINE_BYTE_ARRAY(buf, BUFSIZE);
    double total = (double)len;

    if (!byte_array_init(buf, BUFSIZE)) {
        zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
        return -1;
    }

    while (len > 0) {
        zip_uint64_t n = ZIP_MIN(len, BUFSIZE);

        if (_zip_read(za->src, buf, n, &za->error) < 0) {
            byte_array_fini(buf);
            return -1;
        }

        if (_zip_write(za, buf, n) < 0) {
            byte_array_fini(buf);
            return -1;
        }

        len -= n;

        if (_zip_progress_update(za->progress, (total - (double)len) / total) != 0) {
            zip_error_set(&za->error, ZIP_ER_CANCELLED, 0);
            return -1;
        }
    }

    byte_array_fini(buf);
    return 0;
}


static int
copy_source(zip_t *za, zip_source_t *src, zip_int64_t data_length) {
    DEFINE_BYTE_ARRAY(buf, BUFSIZE);
    zip_int64_t n, current;
    int ret;

    if (zip_source_open(src) < 0) {
        zip_error_set_from_source(&za->error, src);
        return -1;
    }

    if (!byte_array_init(buf, BUFSIZE)) {
        zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
        return -1;
    }

    ret = 0;
    current = 0;
    while ((n = zip_source_read(src, buf, BUFSIZE)) > 0) {
        if (_zip_write(za, buf, (zip_uint64_t)n) < 0) {
            ret = -1;
            break;
        }
        if (n == BUFSIZE && za->progress && data_length > 0) {
            current += n;
            if (_zip_progress_update(za->progress, (double)current / (double)data_length) != 0) {
                zip_error_set(&za->error, ZIP_ER_CANCELLED, 0);
                ret = -1;
                break;
            }
        }
    }

    if (n < 0) {
        zip_error_set_from_source(&za->error, src);
        ret = -1;
    }

    byte_array_fini(buf);

    zip_source_close(src);

    return ret;
}

static int
write_cdir(zip_t *za, const zip_filelist_t *filelist, zip_uint64_t survivors) {
    if (zip_source_tell_write(za->src) < 0) {
        return -1;
    }

    if (_zip_cdir_write(za, filelist, survivors) < 0) {
        return -1;
    }

    if (zip_source_tell_write(za->src) < 0) {
        return -1;
    }

    return 0;
}


int
_zip_changed(const zip_t *za, zip_uint64_t *survivorsp) {
    int changed;
    zip_uint64_t i, survivors;

    changed = 0;
    survivors = 0;

    if (za->comment_changed || za->ch_flags != za->flags) {
        changed = 1;
    }

    for (i = 0; i < za->nentry; i++) {
        if (ZIP_ENTRY_HAS_CHANGES(&za->entry[i])) {
            changed = 1;
        }
        if (!za->entry[i].deleted) {
            survivors++;
        }
    }

    if (survivorsp) {
        *survivorsp = survivors;
    }

    return changed;
}

static int
write_data_descriptor(zip_t *za, const zip_dirent_t *de, int is_zip64) {
    zip_buffer_t *buffer = _zip_buffer_new(NULL, MAX_DATA_DESCRIPTOR_LENGTH);
    int ret = 0;

    if (buffer == NULL) {
        zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
        return -1;
    }

    _zip_buffer_put(buffer, DATADES_MAGIC, 4);
    _zip_buffer_put_32(buffer, de->crc);
    if (is_zip64) {
        _zip_buffer_put_64(buffer, de->comp_size);
        _zip_buffer_put_64(buffer, de->uncomp_size);
    }
    else {
        _zip_buffer_put_32(buffer, (zip_uint32_t)de->comp_size);
        _zip_buffer_put_32(buffer, (zip_uint32_t)de->uncomp_size);
    }

    if (!_zip_buffer_ok(buffer)) {
        zip_error_set(&za->error, ZIP_ER_INTERNAL, 0);
        ret = -1;
    }
    else {
        ret = _zip_write(za, _zip_buffer_data(buffer), _zip_buffer_offset(buffer));
    }

    _zip_buffer_free(buffer);

    return ret;
}
