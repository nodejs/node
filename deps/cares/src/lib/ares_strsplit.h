#ifndef HEADER_CARES_STRSPLIT_H
#define HEADER_CARES_STRSPLIT_H

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

#include "ares_setup.h"

/* Split a string on delms skipping empty or duplicate elements.
 *
 * param in String to split.
 * param delms String of characters to treat as a delimitor.
 *             Each character in the string is a delimitor so
 *             there can be multiple delimitors to split on.
 *             E.g. ", " will split on all comma's and spaces.
 *             Duplicate entries are removed.
 * param num_elm Return parameter of the number of elements
 *               in the result array.
 *
 * returns an allocated array of allocated string elements.
 *
 */
char **ares__strsplit(const char *in, const char *delms, size_t *num_elm);

/* Frees the result returned from ares__strsplit(). */
void ares__strsplit_free(char **elms, size_t num_elm);


#endif /* HEADER_CARES_STRSPLIT_H */

