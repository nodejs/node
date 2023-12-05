/* MIT License
 *
 * Copyright (c) 2018 John Schember
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

#if defined(__MVS__)
#include <strings.h>
#endif

#include "ares_setup.h"
#include "ares.h"
#include "ares_private.h"

void ares__strsplit_free(char **elms, size_t num_elm)
{
  size_t i;

  if (elms == NULL)
    return;

  for (i=0; i<num_elm; i++)
    ares_free(elms[i]);
  ares_free(elms);
}

char **ares__strsplit(const char *in, const char *delms, size_t *num_elm) {
  const char *p;
  char **table;
  void *tmp;
  size_t i, j, k, count;

  if (in == NULL || delms == NULL || num_elm == NULL)
    return NULL;

  *num_elm = 0;

  /* count non-empty delimited substrings */
  count = 0;
  p = in;
  do {
    i = strcspn(p, delms);
    if (i != 0) {
      /* string is non-empty */
      count++;
      p += i;
    }
  } while (*p++ != 0);

  if (count == 0)
    return NULL;
  table = ares_malloc(count * sizeof(*table));
  if (table == NULL)
    return NULL;

  j = 0; /* current table entry */
  /* re-calculate indices and allocate new strings for table */
  for (p = in; j < count; p += i + 1) {
    i = strcspn(p, delms);
    if (i != 0) {
      for (k = 0; k < j; k++) {
        if (strncasecmp(table[k], p, i) == 0 && table[k][i] == 0)
          break;
      }
      if (k == j) {
        /* copy unique strings only */
        table[j] = ares_malloc(i + 1);
        if (table[j] == NULL) {
          ares__strsplit_free(table, j);
          return NULL;
        }
        strncpy(table[j], p, i);
        table[j++][i] = 0;
      } else
        count--;
    }
  }

  tmp = ares_realloc(table, count * sizeof (*table));
  if (tmp != NULL)
    table = tmp;

  *num_elm = count;
  return table;
}
