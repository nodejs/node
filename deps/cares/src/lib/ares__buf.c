/* MIT License
 *
 * Copyright (c) 2023 Brad House
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ares_setup.h"
#include "ares.h"
#include "ares_private.h"
#include "ares__buf.h"
#include <limits.h>
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif

struct ares__buf {
  const unsigned char *data;          /*!< pointer to start of data buffer */
  size_t               data_len;      /*!< total size of data in buffer */

  unsigned char       *alloc_buf;     /*!< Pointer to allocated data buffer,
                                       *   not used for const buffers */
  size_t               alloc_buf_len; /*!< Size of allocated data buffer */

  size_t               offset;        /*!< Current working offset in buffer */
  size_t               tag_offset;    /*!< Tagged offset in buffer. Uses
                                       *   SIZE_MAX if not set. */
};

ares_bool_t ares__isprint(int ch)
{
  if (ch >= 0x20 && ch <= 0x7E) {
    return ARES_TRUE;
  }
  return ARES_FALSE;
}

/* Character set allowed by hostnames.  This is to include the normal
 * domain name character set plus:
 *  - underscores which are used in SRV records.
 *  - Forward slashes such as are used for classless in-addr.arpa
 *    delegation (CNAMEs)
 *  - Asterisks may be used for wildcard domains in CNAMEs as seen in the
 *    real world.
 * While RFC 2181 section 11 does state not to do validation,
 * that applies to servers, not clients.  Vulnerabilities have been
 * reported when this validation is not performed.  Security is more
 * important than edge-case compatibility (which is probably invalid
 * anyhow). */
ares_bool_t ares__is_hostnamech(int ch)
{
  /* [A-Za-z0-9-*._/]
   * Don't use isalnum() as it is locale-specific
   */
  if (ch >= 'A' && ch <= 'Z') {
    return ARES_TRUE;
  }
  if (ch >= 'a' && ch <= 'z') {
    return ARES_TRUE;
  }
  if (ch >= '0' && ch <= '9') {
    return ARES_TRUE;
  }
  if (ch == '-' || ch == '.' || ch == '_' || ch == '/' || ch == '*') {
    return ARES_TRUE;
  }

  return ARES_FALSE;
}

ares__buf_t *ares__buf_create(void)
{
  ares__buf_t *buf = ares_malloc_zero(sizeof(*buf));
  if (buf == NULL) {
    return NULL;
  }

  buf->tag_offset = SIZE_MAX;
  return buf;
}

ares__buf_t *ares__buf_create_const(const unsigned char *data, size_t data_len)
{
  ares__buf_t *buf;

  if (data == NULL || data_len == 0) {
    return NULL;
  }

  buf = ares__buf_create();
  if (buf == NULL) {
    return NULL;
  }

  buf->data     = data;
  buf->data_len = data_len;

  return buf;
}

void ares__buf_destroy(ares__buf_t *buf)
{
  if (buf == NULL) {
    return;
  }
  ares_free(buf->alloc_buf);
  ares_free(buf);
}

static ares_bool_t ares__buf_is_const(const ares__buf_t *buf)
{
  if (buf == NULL) {
    return ARES_FALSE;
  }

  if (buf->data != NULL && buf->alloc_buf == NULL) {
    return ARES_TRUE;
  }

  return ARES_FALSE;
}

void ares__buf_reclaim(ares__buf_t *buf)
{
  size_t prefix_size;
  size_t data_size;

  if (buf == NULL) {
    return;
  }

  if (ares__buf_is_const(buf)) {
    return;
  }

  /* Silence coverity.  All lengths are zero so would bail out later but
   * coverity doesn't know this */
  if (buf->alloc_buf == NULL) {
    return;
  }

  if (buf->tag_offset != SIZE_MAX && buf->tag_offset < buf->offset) {
    prefix_size = buf->tag_offset;
  } else {
    prefix_size = buf->offset;
  }

  if (prefix_size == 0) {
    return;
  }

  data_size = buf->data_len - prefix_size;

  memmove(buf->alloc_buf, buf->alloc_buf + prefix_size, data_size);
  buf->data      = buf->alloc_buf;
  buf->data_len  = data_size;
  buf->offset   -= prefix_size;
  if (buf->tag_offset != SIZE_MAX) {
    buf->tag_offset -= prefix_size;
  }

  return;
}

static ares_status_t ares__buf_ensure_space(ares__buf_t *buf,
                                            size_t       needed_size)
{
  size_t         remaining_size;
  size_t         alloc_size;
  unsigned char *ptr;

  if (buf == NULL) {
    return ARES_EFORMERR;
  }

  if (ares__buf_is_const(buf)) {
    return ARES_EFORMERR;
  }

  /* When calling ares__buf_finish_str() we end up adding a null terminator,
   * so we want to ensure the size is always sufficient for this as we don't
   * want an ARES_ENOMEM at that point */
  needed_size++;

  /* No need to do an expensive move operation, we have enough to just append */
  remaining_size = buf->alloc_buf_len - buf->data_len;
  if (remaining_size >= needed_size) {
    return ARES_SUCCESS;
  }

  /* See if just moving consumed data frees up enough space */
  ares__buf_reclaim(buf);

  remaining_size = buf->alloc_buf_len - buf->data_len;
  if (remaining_size >= needed_size) {
    return ARES_SUCCESS;
  }

  alloc_size = buf->alloc_buf_len;

  /* Not yet started */
  if (alloc_size == 0) {
    alloc_size = 16; /* Always shifts 1, so ends up being 32 minimum */
  }

  /* Increase allocation by powers of 2 */
  do {
    alloc_size     <<= 1;
    remaining_size   = alloc_size - buf->data_len;
  } while (remaining_size < needed_size);

  ptr = ares_realloc(buf->alloc_buf, alloc_size);
  if (ptr == NULL) {
    return ARES_ENOMEM;
  }

  buf->alloc_buf     = ptr;
  buf->alloc_buf_len = alloc_size;
  buf->data          = ptr;

  return ARES_SUCCESS;
}

ares_status_t ares__buf_set_length(ares__buf_t *buf, size_t len)
{
  if (buf == NULL || ares__buf_is_const(buf)) {
    return ARES_EFORMERR;
  }

  if (len >= buf->alloc_buf_len - buf->offset) {
    return ARES_EFORMERR;
  }

  buf->data_len = len;
  return ARES_SUCCESS;
}

ares_status_t ares__buf_append(ares__buf_t *buf, const unsigned char *data,
                               size_t data_len)
{
  ares_status_t status;

  if (data == NULL || data_len == 0) {
    return ARES_EFORMERR;
  }

  status = ares__buf_ensure_space(buf, data_len);
  if (status != ARES_SUCCESS) {
    return status;
  }

  memcpy(buf->alloc_buf + buf->data_len, data, data_len);
  buf->data_len += data_len;
  return ARES_SUCCESS;
}

ares_status_t ares__buf_append_byte(ares__buf_t *buf, unsigned char byte)
{
  return ares__buf_append(buf, &byte, 1);
}

ares_status_t ares__buf_append_be16(ares__buf_t *buf, unsigned short u16)
{
  ares_status_t status;

  status = ares__buf_append_byte(buf, (unsigned char)((u16 >> 8) & 0xff));
  if (status != ARES_SUCCESS) {
    return status;
  }

  status = ares__buf_append_byte(buf, (unsigned char)(u16 & 0xff));
  if (status != ARES_SUCCESS) {
    return status;
  }

  return ARES_SUCCESS;
}

ares_status_t ares__buf_append_be32(ares__buf_t *buf, unsigned int u32)
{
  ares_status_t status;

  status = ares__buf_append_byte(buf, ((unsigned char)(u32 >> 24) & 0xff));
  if (status != ARES_SUCCESS) {
    return status;
  }

  status = ares__buf_append_byte(buf, ((unsigned char)(u32 >> 16) & 0xff));
  if (status != ARES_SUCCESS) {
    return status;
  }

  status = ares__buf_append_byte(buf, ((unsigned char)(u32 >> 8) & 0xff));
  if (status != ARES_SUCCESS) {
    return status;
  }

  status = ares__buf_append_byte(buf, ((unsigned char)u32 & 0xff));
  if (status != ARES_SUCCESS) {
    return status;
  }

  return ARES_SUCCESS;
}

unsigned char *ares__buf_append_start(ares__buf_t *buf, size_t *len)
{
  ares_status_t status;

  if (len == NULL || *len == 0) {
    return NULL;
  }

  status = ares__buf_ensure_space(buf, *len);
  if (status != ARES_SUCCESS) {
    return NULL;
  }

  /* -1 for possible null terminator for ares__buf_finish_str() */
  *len = buf->alloc_buf_len - buf->data_len - 1;
  return buf->alloc_buf + buf->data_len;
}

void ares__buf_append_finish(ares__buf_t *buf, size_t len)
{
  if (buf == NULL) {
    return;
  }

  buf->data_len += len;
}

unsigned char *ares__buf_finish_bin(ares__buf_t *buf, size_t *len)
{
  unsigned char *ptr = NULL;
  if (buf == NULL || len == NULL || ares__buf_is_const(buf)) {
    return NULL;
  }

  ares__buf_reclaim(buf);

  /* We don't want to return NULL except on failure, may be zero-length */
  if (buf->alloc_buf == NULL &&
      ares__buf_ensure_space(buf, 1) != ARES_SUCCESS) {
    return NULL;
  }
  ptr  = buf->alloc_buf;
  *len = buf->data_len;
  ares_free(buf);
  return ptr;
}

char *ares__buf_finish_str(ares__buf_t *buf, size_t *len)
{
  char  *ptr;
  size_t mylen;

  ptr = (char *)ares__buf_finish_bin(buf, &mylen);
  if (ptr == NULL) {
    return NULL;
  }

  if (len != NULL) {
    *len = mylen;
  }

  /* NOTE: ensured via ares__buf_ensure_space() that there is always at least
   *       1 extra byte available for this specific use-case */
  ptr[mylen] = 0;

  return ptr;
}

void ares__buf_tag(ares__buf_t *buf)
{
  if (buf == NULL) {
    return;
  }

  buf->tag_offset = buf->offset;
}

ares_status_t ares__buf_tag_rollback(ares__buf_t *buf)
{
  if (buf == NULL || buf->tag_offset == SIZE_MAX) {
    return ARES_EFORMERR;
  }

  buf->offset     = buf->tag_offset;
  buf->tag_offset = SIZE_MAX;
  return ARES_SUCCESS;
}

ares_status_t ares__buf_tag_clear(ares__buf_t *buf)
{
  if (buf == NULL || buf->tag_offset == SIZE_MAX) {
    return ARES_EFORMERR;
  }

  buf->tag_offset = SIZE_MAX;
  return ARES_SUCCESS;
}

const unsigned char *ares__buf_tag_fetch(const ares__buf_t *buf, size_t *len)
{
  if (buf == NULL || buf->tag_offset == SIZE_MAX || len == NULL) {
    return NULL;
  }

  *len = buf->offset - buf->tag_offset;
  return buf->data + buf->tag_offset;
}

size_t ares__buf_tag_length(const ares__buf_t *buf)
{
  if (buf == NULL || buf->tag_offset == SIZE_MAX) {
    return 0;
  }
  return buf->offset - buf->tag_offset;
}

ares_status_t ares__buf_tag_fetch_bytes(const ares__buf_t *buf,
                                        unsigned char *bytes, size_t *len)
{
  size_t               ptr_len = 0;
  const unsigned char *ptr     = ares__buf_tag_fetch(buf, &ptr_len);

  if (ptr == NULL || bytes == NULL || len == NULL) {
    return ARES_EFORMERR;
  }

  if (*len < ptr_len) {
    return ARES_EFORMERR;
  }

  *len = ptr_len;

  if (ptr_len > 0) {
    memcpy(bytes, ptr, ptr_len);
  }
  return ARES_SUCCESS;
}

ares_status_t ares__buf_tag_fetch_string(const ares__buf_t *buf, char *str,
                                         size_t len)
{
  size_t        out_len;
  ares_status_t status;
  size_t        i;

  if (str == NULL || len == 0) {
    return ARES_EFORMERR;
  }

  /* Space for NULL terminator */
  out_len = len - 1;

  status = ares__buf_tag_fetch_bytes(buf, (unsigned char *)str, &out_len);
  if (status != ARES_SUCCESS) {
    return status;
  }

  /* NULL terminate */
  str[out_len] = 0;

  /* Validate string is printable */
  for (i = 0; i < out_len; i++) {
    if (!ares__isprint(str[i])) {
      return ARES_EBADSTR;
    }
  }

  return ARES_SUCCESS;
}

static const unsigned char *ares__buf_fetch(const ares__buf_t *buf, size_t *len)
{
  if (len != NULL) {
    *len = 0;
  }

  if (buf == NULL || len == NULL || buf->data == NULL) {
    return NULL;
  }

  *len = buf->data_len - buf->offset;
  if (*len == 0) {
    return NULL;
  }

  return buf->data + buf->offset;
}

ares_status_t ares__buf_consume(ares__buf_t *buf, size_t len)
{
  size_t remaining_len = ares__buf_len(buf);

  if (remaining_len < len) {
    return ARES_EBADRESP;
  }

  buf->offset += len;
  return ARES_SUCCESS;
}

ares_status_t ares__buf_fetch_be16(ares__buf_t *buf, unsigned short *u16)
{
  size_t               remaining_len;
  const unsigned char *ptr = ares__buf_fetch(buf, &remaining_len);
  unsigned int         u32;

  if (buf == NULL || u16 == NULL || remaining_len < sizeof(*u16)) {
    return ARES_EBADRESP;
  }

  /* Do math in an unsigned int in order to prevent warnings due to automatic
   * conversion by the compiler from short to int during shifts */
  u32  = ((unsigned int)(ptr[0]) << 8 | (unsigned int)ptr[1]);
  *u16 = (unsigned short)(u32 & 0xFFFF);

  return ares__buf_consume(buf, sizeof(*u16));
}

ares_status_t ares__buf_fetch_be32(ares__buf_t *buf, unsigned int *u32)
{
  size_t               remaining_len;
  const unsigned char *ptr = ares__buf_fetch(buf, &remaining_len);

  if (buf == NULL || u32 == NULL || remaining_len < sizeof(*u32)) {
    return ARES_EBADRESP;
  }

  *u32 = ((unsigned int)(ptr[0]) << 24 | (unsigned int)(ptr[1]) << 16 |
          (unsigned int)(ptr[2]) << 8 | (unsigned int)(ptr[3]));

  return ares__buf_consume(buf, sizeof(*u32));
}

ares_status_t ares__buf_fetch_bytes(ares__buf_t *buf, unsigned char *bytes,
                                    size_t len)
{
  size_t               remaining_len;
  const unsigned char *ptr = ares__buf_fetch(buf, &remaining_len);

  if (buf == NULL || bytes == NULL || len == 0 || remaining_len < len) {
    return ARES_EBADRESP;
  }

  memcpy(bytes, ptr, len);
  return ares__buf_consume(buf, len);
}

ares_status_t ares__buf_fetch_bytes_dup(ares__buf_t *buf, size_t len,
                                        ares_bool_t     null_term,
                                        unsigned char **bytes)
{
  size_t               remaining_len;
  const unsigned char *ptr = ares__buf_fetch(buf, &remaining_len);

  if (buf == NULL || bytes == NULL || len == 0 || remaining_len < len) {
    return ARES_EBADRESP;
  }

  *bytes = ares_malloc(null_term ? len + 1 : len);
  if (*bytes == NULL) {
    return ARES_ENOMEM;
  }

  memcpy(*bytes, ptr, len);
  if (null_term) {
    (*bytes)[len] = 0;
  }
  return ares__buf_consume(buf, len);
}

ares_status_t ares__buf_fetch_str_dup(ares__buf_t *buf, size_t len, char **str)
{
  size_t               remaining_len;
  const unsigned char *ptr = ares__buf_fetch(buf, &remaining_len);

  if (buf == NULL || str == NULL || len == 0 || remaining_len < len) {
    return ARES_EBADRESP;
  }

  *str = ares_malloc(len + 1);
  if (*str == NULL) {
    return ARES_ENOMEM;
  }

  memcpy(*str, ptr, len);
  (*str)[len] = 0;

  return ares__buf_consume(buf, len);
}

ares_status_t ares__buf_fetch_bytes_into_buf(ares__buf_t *buf,
                                             ares__buf_t *dest, size_t len)
{
  size_t               remaining_len;
  const unsigned char *ptr = ares__buf_fetch(buf, &remaining_len);
  ares_status_t        status;

  if (buf == NULL || dest == NULL || len == 0 || remaining_len < len) {
    return ARES_EBADRESP;
  }

  status = ares__buf_append(dest, ptr, len);
  if (status != ARES_SUCCESS) {
    return status;
  }

  return ares__buf_consume(buf, len);
}

size_t ares__buf_consume_whitespace(ares__buf_t *buf,
                                    ares_bool_t  include_linefeed)
{
  size_t               remaining_len = 0;
  const unsigned char *ptr           = ares__buf_fetch(buf, &remaining_len);
  size_t               i;

  if (ptr == NULL) {
    return 0;
  }

  for (i = 0; i < remaining_len; i++) {
    switch (ptr[i]) {
      case '\r':
      case '\t':
      case ' ':
      case '\v':
      case '\f':
        break;
      case '\n':
        if (!include_linefeed) {
          goto done;
        }
        break;
      default:
        goto done;
    }
  }

done:
  if (i > 0) {
    ares__buf_consume(buf, i);
  }
  return i;
}

size_t ares__buf_consume_nonwhitespace(ares__buf_t *buf)
{
  size_t               remaining_len = 0;
  const unsigned char *ptr           = ares__buf_fetch(buf, &remaining_len);
  size_t               i;

  if (ptr == NULL) {
    return 0;
  }

  for (i = 0; i < remaining_len; i++) {
    switch (ptr[i]) {
      case '\r':
      case '\t':
      case ' ':
      case '\v':
      case '\f':
      case '\n':
        goto done;
      default:
        break;
    }
  }

done:
  if (i > 0) {
    ares__buf_consume(buf, i);
  }
  return i;
}

size_t ares__buf_consume_line(ares__buf_t *buf, ares_bool_t include_linefeed)
{
  size_t               remaining_len = 0;
  const unsigned char *ptr           = ares__buf_fetch(buf, &remaining_len);
  size_t               i;

  if (ptr == NULL) {
    return 0;
  }

  for (i = 0; i < remaining_len; i++) {
    if (ptr[i] == '\n') {
      goto done;
    }
  }

done:
  if (include_linefeed && i < remaining_len && ptr[i] == '\n') {
    i++;
  }

  if (i > 0) {
    ares__buf_consume(buf, i);
  }
  return i;
}

size_t ares__buf_consume_until_charset(ares__buf_t         *buf,
                                       const unsigned char *charset, size_t len,
                                       ares_bool_t require_charset)
{
  size_t               remaining_len = 0;
  const unsigned char *ptr           = ares__buf_fetch(buf, &remaining_len);
  size_t               i;
  ares_bool_t          found = ARES_FALSE;

  if (ptr == NULL || charset == NULL || len == 0) {
    return 0;
  }

  for (i = 0; i < remaining_len; i++) {
    size_t j;
    for (j = 0; j < len; j++) {
      if (ptr[i] == charset[j]) {
        found = ARES_TRUE;
        goto done;
      }
    }
  }

done:
  if (require_charset && !found) {
    return 0;
  }

  if (i > 0) {
    ares__buf_consume(buf, i);
  }
  return i;
}

size_t ares__buf_consume_charset(ares__buf_t *buf, const unsigned char *charset,
                                 size_t len)
{
  size_t               remaining_len = 0;
  const unsigned char *ptr           = ares__buf_fetch(buf, &remaining_len);
  size_t               i;

  if (ptr == NULL || charset == NULL || len == 0) {
    return 0;
  }

  for (i = 0; i < remaining_len; i++) {
    size_t j;
    for (j = 0; j < len; j++) {
      if (ptr[i] == charset[j]) {
        break;
      }
    }
    /* Not found */
    if (j == len) {
      break;
    }
  }

  if (i > 0) {
    ares__buf_consume(buf, i);
  }
  return i;
}

static void ares__buf_destroy_cb(void *arg)
{
  ares__buf_destroy(arg);
}

static ares_bool_t ares__buf_split_isduplicate(ares__llist_t       *list,
                                               const unsigned char *val,
                                               size_t               len,
                                               ares__buf_split_t    flags)
{
  ares__llist_node_t *node;

  for (node = ares__llist_node_first(list); node != NULL;
       node = ares__llist_node_next(node)) {
    const ares__buf_t   *buf  = ares__llist_node_val(node);
    size_t               plen = 0;
    const unsigned char *ptr  = ares__buf_peek(buf, &plen);

    /* Can't be duplicate if lengths mismatch */
    if (plen != len) {
      continue;
    }

    if (flags & ARES_BUF_SPLIT_CASE_INSENSITIVE) {
      if (ares__memeq_ci(ptr, val, len)) {
        return ARES_TRUE;
      }
    } else {
      if (memcmp(ptr, val, len) == 0) {
        return ARES_TRUE;
      }
    }
  }
  return ARES_FALSE;
}

ares_status_t ares__buf_split(ares__buf_t *buf, const unsigned char *delims,
                              size_t delims_len, ares__buf_split_t flags,
                              ares__llist_t **list)
{
  ares_status_t status = ARES_SUCCESS;
  ares_bool_t   first  = ARES_TRUE;

  if (buf == NULL || delims == NULL || delims_len == 0 || list == NULL) {
    return ARES_EFORMERR;
  }

  *list = ares__llist_create(ares__buf_destroy_cb);
  if (*list == NULL) {
    status = ARES_ENOMEM;
    goto done;
  }

  while (ares__buf_len(buf)) {
    size_t len;

    ares__buf_tag(buf);

    len = ares__buf_consume_until_charset(buf, delims, delims_len, ARES_FALSE);

    /* Don't treat a delimiter as part of the length */
    if (!first && len && flags & ARES_BUF_SPLIT_DONT_CONSUME_DELIMS) {
      len--;
    }

    if (len != 0 || flags & ARES_BUF_SPLIT_ALLOW_BLANK) {
      const unsigned char *ptr = ares__buf_tag_fetch(buf, &len);
      ares__buf_t         *data;

      if (!(flags & ARES_BUF_SPLIT_NO_DUPLICATES) ||
          !ares__buf_split_isduplicate(*list, ptr, len, flags)) {
        /* Since we don't allow const buffers of 0 length, and user wants
         * 0-length buffers, swap what we do here */
        if (len) {
          data = ares__buf_create_const(ptr, len);
        } else {
          data = ares__buf_create();
        }

        if (data == NULL) {
          status = ARES_ENOMEM;
          goto done;
        }

        if (ares__llist_insert_last(*list, data) == NULL) {
          ares__buf_destroy(data);
          status = ARES_ENOMEM;
          goto done;
        }
      }
    }

    if (!(flags & ARES_BUF_SPLIT_DONT_CONSUME_DELIMS) &&
        ares__buf_len(buf) != 0) {
      /* Consume delimiter */
      ares__buf_consume(buf, 1);
    }

    first = ARES_FALSE;
  }

done:
  if (status != ARES_SUCCESS) {
    ares__llist_destroy(*list);
    *list = NULL;
  }

  return status;
}

ares_bool_t ares__buf_begins_with(const ares__buf_t   *buf,
                                  const unsigned char *data, size_t data_len)
{
  size_t               remaining_len = 0;
  const unsigned char *ptr           = ares__buf_fetch(buf, &remaining_len);

  if (ptr == NULL || data == NULL || data_len == 0) {
    return ARES_FALSE;
  }

  if (data_len > remaining_len) {
    return ARES_FALSE;
  }

  if (memcmp(ptr, data, data_len) != 0) {
    return ARES_FALSE;
  }

  return ARES_TRUE;
}

size_t ares__buf_len(const ares__buf_t *buf)
{
  if (buf == NULL) {
    return 0;
  }

  return buf->data_len - buf->offset;
}

const unsigned char *ares__buf_peek(const ares__buf_t *buf, size_t *len)
{
  return ares__buf_fetch(buf, len);
}

size_t ares__buf_get_position(const ares__buf_t *buf)
{
  if (buf == NULL) {
    return 0;
  }
  return buf->offset;
}

ares_status_t ares__buf_set_position(ares__buf_t *buf, size_t idx)
{
  if (buf == NULL) {
    return ARES_EFORMERR;
  }

  if (idx > buf->data_len) {
    return ARES_EFORMERR;
  }

  buf->offset = idx;
  return ARES_SUCCESS;
}

ares_status_t ares__buf_parse_dns_binstr(ares__buf_t *buf, size_t remaining_len,
                                         unsigned char **bin, size_t *bin_len,
                                         ares_bool_t allow_multiple)
{
  unsigned char len;
  ares_status_t status;
  ares__buf_t  *binbuf   = NULL;
  size_t        orig_len = ares__buf_len(buf);

  if (buf == NULL) {
    return ARES_EFORMERR;
  }

  if (remaining_len == 0) {
    return ARES_EBADRESP;
  }

  binbuf = ares__buf_create();
  if (binbuf == NULL) {
    return ARES_ENOMEM;
  }

  while (orig_len - ares__buf_len(buf) < remaining_len) {
    status = ares__buf_fetch_bytes(buf, &len, 1);
    if (status != ARES_SUCCESS) {
      break;
    }

    if (len) {
      /* XXX: Maybe we should scan to make sure it is printable? */
      if (bin != NULL) {
        status = ares__buf_fetch_bytes_into_buf(buf, binbuf, len);
      } else {
        status = ares__buf_consume(buf, len);
      }
      if (status != ARES_SUCCESS) {
        break;
      }
    }

    if (!allow_multiple) {
      break;
    }
  }


  if (status != ARES_SUCCESS) {
    ares__buf_destroy(binbuf);
  } else {
    if (bin != NULL) {
      size_t mylen = 0;
      /* NOTE: we use ares__buf_finish_str() here as we guarantee NULL
       *       Termination even though we are technically returning binary data.
       */
      *bin     = (unsigned char *)ares__buf_finish_str(binbuf, &mylen);
      *bin_len = mylen;
    }
  }

  return status;
}

ares_status_t ares__buf_parse_dns_str(ares__buf_t *buf, size_t remaining_len,
                                      char **str, ares_bool_t allow_multiple)
{
  size_t len;
  return ares__buf_parse_dns_binstr(buf, remaining_len, (unsigned char **)str,
                                    &len, allow_multiple);
}

ares_status_t ares__buf_append_num_dec(ares__buf_t *buf, size_t num, size_t len)
{
  size_t i;
  size_t mod;

  if (len == 0) {
    len = ares__count_digits(num);
  }

  mod = ares__pow(10, len);

  for (i = len; i > 0; i--) {
    size_t        digit = (num % mod);
    ares_status_t status;

    mod /= 10;

    /* Silence coverity.  Shouldn't be possible since we calculate it above */
    if (mod == 0) {
      return ARES_EFORMERR;
    }

    digit  /= mod;
    status  = ares__buf_append_byte(buf, '0' + (unsigned char)(digit & 0xFF));
    if (status != ARES_SUCCESS) {
      return status;
    }
  }
  return ARES_SUCCESS;
}

ares_status_t ares__buf_append_num_hex(ares__buf_t *buf, size_t num, size_t len)
{
  size_t                     i;
  static const unsigned char hexbytes[] = "0123456789ABCDEF";

  if (len == 0) {
    len = ares__count_hexdigits(num);
  }

  for (i = len; i > 0; i--) {
    ares_status_t status;
    status = ares__buf_append_byte(buf, hexbytes[(num >> ((i - 1) * 4)) & 0xF]);
    if (status != ARES_SUCCESS) {
      return status;
    }
  }
  return ARES_SUCCESS;
}

ares_status_t ares__buf_append_str(ares__buf_t *buf, const char *str)
{
  return ares__buf_append(buf, (const unsigned char *)str, ares_strlen(str));
}

static ares_status_t ares__buf_hexdump_line(ares__buf_t *buf, size_t idx,
                                            const unsigned char *data,
                                            size_t               len)
{
  size_t        i;
  ares_status_t status;

  /* Address */
  status = ares__buf_append_num_hex(buf, idx, 6);
  if (status != ARES_SUCCESS) {
    return status;
  }

  /* | */
  status = ares__buf_append_str(buf, " | ");
  if (status != ARES_SUCCESS) {
    return status;
  }

  for (i = 0; i < 16; i++) {
    if (i >= len) {
      status = ares__buf_append_str(buf, "  ");
    } else {
      status = ares__buf_append_num_hex(buf, data[i], 2);
    }
    if (status != ARES_SUCCESS) {
      return status;
    }

    status = ares__buf_append_byte(buf, ' ');
    if (status != ARES_SUCCESS) {
      return status;
    }
  }

  /* | */
  status = ares__buf_append_str(buf, " | ");
  if (status != ARES_SUCCESS) {
    return status;
  }

  for (i = 0; i < 16; i++) {
    if (i >= len) {
      break;
    }
    status = ares__buf_append_byte(buf, ares__isprint(data[i]) ? data[i] : '.');
    if (status != ARES_SUCCESS) {
      return status;
    }
  }

  return ares__buf_append_byte(buf, '\n');
}

ares_status_t ares__buf_hexdump(ares__buf_t *buf, const unsigned char *data,
                                size_t len)
{
  size_t i;

  /* Each line is 16 bytes */
  for (i = 0; i < len; i += 16) {
    ares_status_t status;
    status = ares__buf_hexdump_line(buf, i, data + i, len - i);
    if (status != ARES_SUCCESS) {
      return status;
    }
  }

  return ARES_SUCCESS;
}
