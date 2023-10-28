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

static const unsigned char *ares__buf_fetch(const ares__buf_t *buf, size_t *len)
{
  if (len != NULL) {
    *len = 0;
  }

  if (buf == NULL || len == NULL || buf->data == NULL) {
    return NULL;
  }

  *len = buf->data_len - buf->offset;
  return buf->data + buf->offset;
}

ares_status_t ares__buf_consume(ares__buf_t *buf, size_t len)
{
  size_t remaining_len;

  ares__buf_fetch(buf, &remaining_len);

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

  if (buf == NULL || u16 == NULL || remaining_len < sizeof(*u16)) {
    return ARES_EBADRESP;
  }

  *u16 =
    (unsigned short)((unsigned short)(ptr[0]) << 8 | (unsigned short)ptr[1]);

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
                                        unsigned char **bytes)
{
  size_t               remaining_len;
  const unsigned char *ptr = ares__buf_fetch(buf, &remaining_len);

  if (buf == NULL || bytes == NULL || len == 0 || remaining_len < len) {
    return ARES_EBADRESP;
  }

  *bytes = ares_malloc(len);
  if (*bytes == NULL) {
    return ARES_ENOMEM;
  }

  memcpy(*bytes, ptr, len);
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

/* Reserved characters for names that need to be escaped */
static ares_bool_t is_reservedch(int ch)
{
  switch (ch) {
    case '"':
    case '.':
    case ';':
    case '\\':
    case '(':
    case ')':
    case '@':
    case '$':
      return ARES_TRUE;
    default:
      break;
  }

  return ARES_FALSE;
}

static ares_bool_t ares__isprint(int ch)
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
static ares_bool_t is_hostnamech(int ch)
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

static ares_status_t ares__buf_fetch_dnsname_into_buf(ares__buf_t *buf,
                                                      ares__buf_t *dest,
                                                      size_t       len,
                                                      ares_bool_t  is_hostname)
{
  size_t               remaining_len;
  const unsigned char *ptr = ares__buf_fetch(buf, &remaining_len);
  ares_status_t        status;
  size_t               i;

  if (buf == NULL || len == 0 || remaining_len < len) {
    return ARES_EBADRESP;
  }

  for (i = 0; i < len; i++) {
    unsigned char c = ptr[i];

    /* Hostnames have a very specific allowed character set.  Anything outside
     * of that (non-printable and reserved included) are disallowed */
    if (is_hostname && !is_hostnamech(c)) {
      status = ARES_EBADRESP;
      goto fail;
    }

    /* NOTE: dest may be NULL if the user is trying to skip the name. validation
     *       still occurs above. */
    if (dest == NULL) {
      continue;
    }

    /* Non-printable characters need to be output as \DDD */
    if (!ares__isprint(c)) {
      unsigned char escape[4];

      escape[0] = '\\';
      escape[1] = '0' + (c / 100);
      escape[2] = '0' + ((c % 100) / 10);
      escape[3] = '0' + (c % 10);

      status = ares__buf_append(dest, escape, sizeof(escape));
      if (status != ARES_SUCCESS) {
        goto fail;
      }

      continue;
    }

    /* Reserved characters need to be escaped, otherwise normal */
    if (is_reservedch(c)) {
      status = ares__buf_append_byte(dest, '\\');
      if (status != ARES_SUCCESS) {
        goto fail;
      }
    }

    status = ares__buf_append_byte(dest, c);
    if (status != ARES_SUCCESS) {
      return status;
    }
  }

  return ares__buf_consume(buf, len);

fail:
  return status;
}

size_t ares__buf_consume_whitespace(ares__buf_t *buf, int include_linefeed)
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

size_t ares__buf_consume_line(ares__buf_t *buf, int include_linefeed)
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
  if (include_linefeed && i > 0 && i < remaining_len && ptr[i] == '\n') {
    i++;
  }
  if (i > 0) {
    ares__buf_consume(buf, i);
  }
  return i;
}

ares_status_t ares__buf_begins_with(const ares__buf_t   *buf,
                                    const unsigned char *data, size_t data_len)
{
  size_t               remaining_len = 0;
  const unsigned char *ptr           = ares__buf_fetch(buf, &remaining_len);

  if (ptr == NULL || data == NULL || data_len == 0) {
    return ARES_EFORMERR;
  }

  if (data_len > remaining_len) {
    return ARES_EBADRESP;
  }

  if (memcmp(ptr, data, data_len) == 0) {
    return ARES_EBADRESP;
  }

  return ARES_SUCCESS;
}

size_t ares__buf_len(const ares__buf_t *buf)
{
  size_t len = 0;
  ares__buf_fetch(buf, &len);
  return len;
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

#define ARES_DNS_HEADER_SIZE 12

ares_status_t ares__buf_parse_dns_name(ares__buf_t *buf, char **name,
                                       ares_bool_t is_hostname)
{
  size_t        save_offset = 0;
  unsigned char c;
  ares_status_t status;
  ares__buf_t  *namebuf     = NULL;
  size_t        label_start = ares__buf_get_position(buf);

  if (buf == NULL) {
    return ARES_EFORMERR;
  }

  if (name != NULL) {
    namebuf = ares__buf_create();
    if (namebuf == NULL) {
      status = ARES_ENOMEM;
      goto fail;
    }
  }

  /* XXX: LibraryTest.ExpandName and LibraryTest.ExpandNameFailure are not
   *      complete DNS messages but rather non-valid minimized snippets to try
   *      to test the old parser, so we have to turn off this sanity check
   *      it appears until these test cases are rewritten
   */
#if 0
  if (ares__buf_get_position(buf) < ARES_DNS_HEADER_SIZE) {
    status = ARES_EFORMERR;
    goto fail;
  }
#endif
  /* The compression scheme allows a domain name in a message to be
   * represented as either:
   *
   * - a sequence of labels ending in a zero octet
   * - a pointer
   * - a sequence of labels ending with a pointer
   */
  while (1) {
    /* Keep track of the minimum label starting position to prevent forward
     * jumping */
    if (label_start > ares__buf_get_position(buf)) {
      label_start = ares__buf_get_position(buf);
    }

    status = ares__buf_fetch_bytes(buf, &c, 1);
    if (status != ARES_SUCCESS) {
      goto fail;
    }

    /* Pointer/Redirect */
    if ((c & 0xc0) == 0xc0) {
      /* The pointer takes the form of a two octet sequence:
       *
       *   +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *   | 1  1|                OFFSET                   |
       *   +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       *
       * The first two bits are ones.  This allows a pointer to be distinguished
       * from a label, since the label must begin with two zero bits because
       * labels are restricted to 63 octets or less.  (The 10 and 01
       * combinations are reserved for future use.)  The OFFSET field specifies
       * an offset from the start of the message (i.e., the first octet of the
       * ID field in the domain header).  A zero offset specifies the first byte
       * of the ID field, etc.
       */
      size_t offset = (size_t)((c & 0x3F) << 8);

      /* Fetch second byte of the redirect length */
      status = ares__buf_fetch_bytes(buf, &c, 1);
      if (status != ARES_SUCCESS) {
        goto fail;
      }

      offset |= ((size_t)c);

      /* According to RFC 1035 4.1.4:
       *    In this scheme, an entire domain name or a list of labels at
       *    the end of a domain name is replaced with a pointer to a prior
       *    occurance of the same name.
       * Note the word "prior", meaning it must go backwards.  This was
       * confirmed via the ISC BIND code that it also prevents forward
       * pointers.
       */
      if (offset >= label_start) {
        status = ARES_EBADNAME;
        goto fail;
      }

      /* First time we make a jump, save the current position */
      if (save_offset == 0) {
        save_offset = ares__buf_get_position(buf);
      }

      status = ares__buf_set_position(buf, offset);
      if (status != ARES_SUCCESS) {
        status = ARES_EBADNAME;
        goto fail;
      }

      continue;
    } else if ((c & 0xc0) != 0) {
      /* 10 and 01 are reserved */
      status = ARES_EBADNAME;
      goto fail;
    } else if (c == 0) {
      /* termination via zero octet*/
      break;
    }

    /* New label */

    /* Labels are separated by periods */
    if (ares__buf_len(namebuf) != 0 && name != NULL) {
      status = ares__buf_append_byte(namebuf, '.');
      if (status != ARES_SUCCESS) {
        goto fail;
      }
    }

    status = ares__buf_fetch_dnsname_into_buf(buf, namebuf, c, is_hostname);
    if (status != ARES_SUCCESS) {
      goto fail;
    }
  }

  /* Restore offset read after first redirect/pointer as this is where the DNS
   * message continues */
  if (save_offset) {
    ares__buf_set_position(buf, save_offset);
  }

  if (name != NULL) {
    *name = ares__buf_finish_str(namebuf, NULL);
    if (*name == NULL) {
      status = ARES_ENOMEM;
      goto fail;
    }
  }

  return ARES_SUCCESS;

fail:
  /* We want badname response if we couldn't parse */
  if (status == ARES_EBADRESP) {
    status = ARES_EBADNAME;
  }

  ares__buf_destroy(namebuf);
  return status;
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

    /* XXX: Maybe we should scan to make sure it is printable? */
    if (bin != NULL) {
      status = ares__buf_fetch_bytes_into_buf(buf, binbuf, len);
    } else {
      status = ares__buf_consume(buf, len);
    }

    if (status != ARES_SUCCESS) {
      break;
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
