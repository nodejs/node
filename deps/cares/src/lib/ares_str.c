/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) The c-ares project and its contributors
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
#include "ares_str.h"
#include "ares.h"
#include "ares_private.h"

#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif

size_t ares_strlen(const char *str)
{
  if (str == NULL) {
    return 0;
  }

  return strlen(str);
}

char *ares_strdup(const char *s1)
{
  size_t len;
  char  *out;

  if (s1 == NULL) {
    return NULL;
  }

  len = ares_strlen(s1);

  /* Don't see how this is possible */
  if (len == SIZE_MAX) {
    return NULL;
  }

  out = ares_malloc(len + 1);
  if (out == NULL) {
    return NULL;
  }

  if (len) {
    memcpy(out, s1, len);
  }

  out[len] = 0;
  return out;
}

size_t ares_strcpy(char *dest, const char *src, size_t dest_size)
{
  size_t len = 0;

  if (dest == NULL || dest_size == 0) {
    return 0;
  }

  len = ares_strlen(src);

  if (len >= dest_size) {
    len = dest_size - 1;
  }

  if (len) {
    memcpy(dest, src, len);
  }

  dest[len] = 0;
  return len;
}

ares_bool_t ares_str_isnum(const char *str)
{
  size_t i;

  if (str == NULL || *str == 0) {
    return ARES_FALSE;
  }

  for (i = 0; str[i] != 0; i++) {
    if (str[i] < '0' || str[i] > '9') {
      return ARES_FALSE;
    }
  }
  return ARES_TRUE;
}
