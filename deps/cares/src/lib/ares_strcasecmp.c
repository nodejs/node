

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
#include "ares_strcasecmp.h"

#ifndef HAVE_STRCASECMP
int ares_strcasecmp(const char *a, const char *b)
{
#if defined(HAVE_STRCMPI)
  return strcmpi(a, b);
#elif defined(HAVE_STRICMP)
  return stricmp(a, b);
#else
  size_t i;

  for (i = 0; i < (size_t)-1; i++) {
    int c1 = ISUPPER(a[i]) ? tolower(a[i]) : a[i];
    int c2 = ISUPPER(b[i]) ? tolower(b[i]) : b[i];
    if (c1 != c2)
      return c1-c2;
    if (!c1)
      break;
  }
  return 0;
#endif
}
#endif

#ifndef HAVE_STRNCASECMP
int ares_strncasecmp(const char *a, const char *b, size_t n)
{
#if defined(HAVE_STRNCMPI)
  return strncmpi(a, b, n);
#elif defined(HAVE_STRNICMP)
  return strnicmp(a, b, n);
#else
  size_t i;

  for (i = 0; i < n; i++) {
    int c1 = ISUPPER(a[i]) ? tolower(a[i]) : a[i];
    int c2 = ISUPPER(b[i]) ? tolower(b[i]) : b[i];
    if (c1 != c2)
      return c1-c2;
    if (!c1)
      break;
  }
  return 0;
#endif
}
#endif

