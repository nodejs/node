/*
  zip_extra_field.c -- manipulate extra fields
  Copyright (C) 2012-2021 Dieter Baron and Thomas Klausner

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
#include <string.h>

#include "zipint.h"


zip_extra_field_t *
_zip_ef_clone(const zip_extra_field_t *ef, zip_error_t *error) {
    zip_extra_field_t *head, *prev, *def;

    head = prev = NULL;

    while (ef) {
        if ((def = _zip_ef_new(ef->id, ef->size, ef->data, ef->flags)) == NULL) {
            zip_error_set(error, ZIP_ER_MEMORY, 0);
            _zip_ef_free(head);
            return NULL;
        }

        if (head == NULL)
            head = def;
        if (prev)
            prev->next = def;
        prev = def;

        ef = ef->next;
    }

    return head;
}


zip_extra_field_t *
_zip_ef_delete_by_id(zip_extra_field_t *ef, zip_uint16_t id, zip_uint16_t id_idx, zip_flags_t flags) {
    zip_extra_field_t *head, *prev;
    int i;

    i = 0;
    head = ef;
    prev = NULL;
    for (; ef; ef = (prev ? prev->next : head)) {
        if ((ef->flags & flags & ZIP_EF_BOTH) && ((ef->id == id) || (id == ZIP_EXTRA_FIELD_ALL))) {
            if (id_idx == ZIP_EXTRA_FIELD_ALL || i == id_idx) {
                ef->flags &= ~(flags & ZIP_EF_BOTH);
                if ((ef->flags & ZIP_EF_BOTH) == 0) {
                    if (prev)
                        prev->next = ef->next;
                    else
                        head = ef->next;
                    ef->next = NULL;
                    _zip_ef_free(ef);

                    if (id_idx == ZIP_EXTRA_FIELD_ALL)
                        continue;
                }
            }

            i++;
            if (i > id_idx)
                break;
        }
        prev = ef;
    }

    return head;
}


void
_zip_ef_free(zip_extra_field_t *ef) {
    zip_extra_field_t *ef2;

    while (ef) {
        ef2 = ef->next;
        free(ef->data);
        free(ef);
        ef = ef2;
    }
}


const zip_uint8_t *
_zip_ef_get_by_id(const zip_extra_field_t *ef, zip_uint16_t *lenp, zip_uint16_t id, zip_uint16_t id_idx, zip_flags_t flags, zip_error_t *error) {
    static const zip_uint8_t empty[1] = {'\0'};

    int i;

    i = 0;
    for (; ef; ef = ef->next) {
        if (ef->id == id && (ef->flags & flags & ZIP_EF_BOTH)) {
            if (i < id_idx) {
                i++;
                continue;
            }

            if (lenp)
                *lenp = ef->size;
            if (ef->size > 0)
                return ef->data;
            else
                return empty;
        }
    }

    zip_error_set(error, ZIP_ER_NOENT, 0);
    return NULL;
}


zip_extra_field_t *
_zip_ef_merge(zip_extra_field_t *to, zip_extra_field_t *from) {
    zip_extra_field_t *ef2, *tt, *tail;
    int duplicate;

    if (to == NULL)
        return from;

    for (tail = to; tail->next; tail = tail->next)
        ;

    for (; from; from = ef2) {
        ef2 = from->next;

        duplicate = 0;
        for (tt = to; tt; tt = tt->next) {
            if (tt->id == from->id && tt->size == from->size && (tt->size == 0 || memcmp(tt->data, from->data, tt->size) == 0)) {
                tt->flags |= (from->flags & ZIP_EF_BOTH);
                duplicate = 1;
                break;
            }
        }

        from->next = NULL;
        if (duplicate)
            _zip_ef_free(from);
        else
            tail = tail->next = from;
    }

    return to;
}


zip_extra_field_t *
_zip_ef_new(zip_uint16_t id, zip_uint16_t size, const zip_uint8_t *data, zip_flags_t flags) {
    zip_extra_field_t *ef;

    if ((ef = (zip_extra_field_t *)malloc(sizeof(*ef))) == NULL)
        return NULL;

    ef->next = NULL;
    ef->flags = flags;
    ef->id = id;
    ef->size = size;
    if (size > 0) {
        if ((ef->data = (zip_uint8_t *)_zip_memdup(data, size, NULL)) == NULL) {
            free(ef);
            return NULL;
        }
    }
    else
        ef->data = NULL;

    return ef;
}


bool
_zip_ef_parse(const zip_uint8_t *data, zip_uint16_t len, zip_flags_t flags, zip_extra_field_t **ef_head_p, zip_error_t *error) {
    zip_buffer_t *buffer;
    zip_extra_field_t *ef, *ef2, *ef_head;

    if ((buffer = _zip_buffer_new((zip_uint8_t *)data, len)) == NULL) {
        zip_error_set(error, ZIP_ER_MEMORY, 0);
        return false;
    }

    ef_head = ef = NULL;

    while (_zip_buffer_ok(buffer) && _zip_buffer_left(buffer) >= 4) {
        zip_uint16_t fid, flen;
        zip_uint8_t *ef_data;

        fid = _zip_buffer_get_16(buffer);
        flen = _zip_buffer_get_16(buffer);
        ef_data = _zip_buffer_get(buffer, flen);

        if (ef_data == NULL) {
            zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_INVALID_EF_LENGTH);
            _zip_buffer_free(buffer);
            _zip_ef_free(ef_head);
            return false;
        }

        if ((ef2 = _zip_ef_new(fid, flen, ef_data, flags)) == NULL) {
            zip_error_set(error, ZIP_ER_MEMORY, 0);
            _zip_buffer_free(buffer);
            _zip_ef_free(ef_head);
            return false;
        }

        if (ef_head) {
            ef->next = ef2;
            ef = ef2;
        }
        else
            ef_head = ef = ef2;
    }

    if (!_zip_buffer_eof(buffer)) {
        /* Android APK files align stored file data with padding in extra fields; ignore. */
        /* see https://android.googlesource.com/platform/build/+/master/tools/zipalign/ZipAlign.cpp */
        /* buffer is at most 64k long, so this can't overflow. */
        size_t glen = _zip_buffer_left(buffer);
        zip_uint8_t *garbage;
        garbage = _zip_buffer_get(buffer, glen);
        if (glen >= 4 || garbage == NULL || memcmp(garbage, "\0\0\0", (size_t)glen) != 0) {
            zip_error_set(error, ZIP_ER_INCONS, ZIP_ER_DETAIL_EF_TRAILING_GARBAGE);
            _zip_buffer_free(buffer);
            _zip_ef_free(ef_head);
            return false;
        }
    }

    _zip_buffer_free(buffer);

    if (ef_head_p) {
        *ef_head_p = ef_head;
    }
    else {
        _zip_ef_free(ef_head);
    }

    return true;
}


zip_extra_field_t *
_zip_ef_remove_internal(zip_extra_field_t *ef) {
    zip_extra_field_t *ef_head;
    zip_extra_field_t *prev, *next;

    ef_head = ef;
    prev = NULL;

    while (ef) {
        if (ZIP_EF_IS_INTERNAL(ef->id)) {
            next = ef->next;
            if (ef_head == ef)
                ef_head = next;
            ef->next = NULL;
            _zip_ef_free(ef);
            if (prev)
                prev->next = next;
            ef = next;
        }
        else {
            prev = ef;
            ef = ef->next;
        }
    }

    return ef_head;
}


zip_uint16_t
_zip_ef_size(const zip_extra_field_t *ef, zip_flags_t flags) {
    zip_uint16_t size;

    size = 0;
    for (; ef; ef = ef->next) {
        if (ef->flags & flags & ZIP_EF_BOTH)
            size = (zip_uint16_t)(size + 4 + ef->size);
    }

    return size;
}


int
_zip_ef_write(zip_t *za, const zip_extra_field_t *ef, zip_flags_t flags) {
    zip_uint8_t b[4];
    zip_buffer_t *buffer = _zip_buffer_new(b, sizeof(b));

    if (buffer == NULL) {
        return -1;
    }

    for (; ef; ef = ef->next) {
        if (ef->flags & flags & ZIP_EF_BOTH) {
            _zip_buffer_set_offset(buffer, 0);
            _zip_buffer_put_16(buffer, ef->id);
            _zip_buffer_put_16(buffer, ef->size);
            if (!_zip_buffer_ok(buffer)) {
                zip_error_set(&za->error, ZIP_ER_INTERNAL, 0);
                _zip_buffer_free(buffer);
                return -1;
            }
            if (_zip_write(za, b, 4) < 0) {
                _zip_buffer_free(buffer);
                return -1;
            }
            if (ef->size > 0) {
                if (_zip_write(za, ef->data, ef->size) < 0) {
                    _zip_buffer_free(buffer);
                    return -1;
                }
            }
        }
    }

    _zip_buffer_free(buffer);
    return 0;
}


int
_zip_read_local_ef(zip_t *za, zip_uint64_t idx) {
    zip_entry_t *e;
    unsigned char b[4];
    zip_buffer_t *buffer;
    zip_uint16_t fname_len, ef_len;

    if (idx >= za->nentry) {
        zip_error_set(&za->error, ZIP_ER_INVAL, 0);
        return -1;
    }

    e = za->entry + idx;

    if (e->orig == NULL || e->orig->local_extra_fields_read)
        return 0;

    if (e->orig->offset + 26 > ZIP_INT64_MAX) {
        zip_error_set(&za->error, ZIP_ER_SEEK, EFBIG);
        return -1;
    }

    if (zip_source_seek(za->src, (zip_int64_t)(e->orig->offset + 26), SEEK_SET) < 0) {
        zip_error_set_from_source(&za->error, za->src);
        return -1;
    }

    if ((buffer = _zip_buffer_new_from_source(za->src, sizeof(b), b, &za->error)) == NULL) {
        return -1;
    }

    fname_len = _zip_buffer_get_16(buffer);
    ef_len = _zip_buffer_get_16(buffer);

    if (!_zip_buffer_eof(buffer)) {
        _zip_buffer_free(buffer);
        zip_error_set(&za->error, ZIP_ER_INTERNAL, 0);
        return -1;
    }

    _zip_buffer_free(buffer);

    if (ef_len > 0) {
        zip_extra_field_t *ef;
        zip_uint8_t *ef_raw;

        if (zip_source_seek(za->src, fname_len, SEEK_CUR) < 0) {
            zip_error_set(&za->error, ZIP_ER_SEEK, errno);
            return -1;
        }

        ef_raw = _zip_read_data(NULL, za->src, ef_len, 0, &za->error);

        if (ef_raw == NULL)
            return -1;

        if (!_zip_ef_parse(ef_raw, ef_len, ZIP_EF_LOCAL, &ef, &za->error)) {
            free(ef_raw);
            return -1;
        }
        free(ef_raw);

        if (ef) {
            ef = _zip_ef_remove_internal(ef);
            e->orig->extra_fields = _zip_ef_merge(e->orig->extra_fields, ef);
        }
    }

    e->orig->local_extra_fields_read = 1;

    if (e->changes && e->changes->local_extra_fields_read == 0) {
        e->changes->extra_fields = e->orig->extra_fields;
        e->changes->local_extra_fields_read = 1;
    }

    return 0;
}
