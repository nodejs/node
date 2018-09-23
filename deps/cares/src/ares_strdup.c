

/* Copyright 1998 by the Massachusetts Institute of Technology.
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

#include "ares_setup.h"
#include "ares_strdup.h"

#ifndef HAVE_STRDUP
char *ares_strdup(const char *s1)
{
  size_t sz;
  char * s2;

  if(s1) {
    sz = strlen(s1);
    if(sz < (size_t)-1) {
      sz++;
      if(sz < ((size_t)-1) / sizeof(char)) {
        s2 = malloc(sz * sizeof(char));
        if(s2) {
          memcpy(s2, s1, sz * sizeof(char));
          return s2;
        }
      }
    }
  }
  return (char *)NULL;
}
#endif
