/* Copyright (C) 2018 by John Schember <john@nachtimwald.com>
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
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
