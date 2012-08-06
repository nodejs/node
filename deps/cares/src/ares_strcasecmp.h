#ifndef HEADER_CARES_STRCASECMP_H
#define HEADER_CARES_STRCASECMP_H


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

#ifndef HAVE_STRCASECMP
extern int ares_strcasecmp(const char *a, const char *b);
#endif

#ifndef HAVE_STRNCASECMP
extern int ares_strncasecmp(const char *a, const char *b, size_t n);
#endif

#endif /* HEADER_CARES_STRCASECMP_H */
