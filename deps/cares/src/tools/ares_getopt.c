/*
 * Original file name getopt.c  Initial import into the c-ares source tree
 * on 2007-04-11.  Lifted from version 5.2 of the 'Open Mash' project with
 * the modified BSD license, BSD license without the advertising clause.
 *
 */

/*
 * getopt.c --
 *
 *      Standard UNIX getopt function.  Code is from BSD.
 *
 * Copyright (c) 1987-2001 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * A. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * B. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * C. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* #if !defined(lint)
 * static char sccsid[] = "@(#)getopt.c 8.2 (Berkeley) 4/2/94";
 * #endif
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ares_getopt.h"

int        opterr = 1; /* if error message should be printed */
int        optind = 1; /* index into parent argv vector */
int        optopt = 0; /* character checked for validity */
static int optreset;   /* reset getopt */
char      *optarg;     /* argument associated with option */

#define BADCH  (int)'?'
#define BADARG (int)':'
#define EMSG   (char *)""

/*
 * ares_getopt --
 *    Parse argc/argv argument vector.
 */
int ares_getopt(int nargc, char * const nargv[], const char *ostr)
{
  static char *place = EMSG; /* option letter processing */
  char        *oli;          /* option letter list index */

  if (optreset || !*place) { /* update scanning pointer */
    optreset = 0;
    if (optind >= nargc || *(place = nargv[optind]) != '-') {
      place = EMSG;
      return (EOF);
    }
    if (place[1] && *++place == '-') { /* found "--" */
      ++optind;
      place = EMSG;
      return (EOF);
    }
  } /* option letter okay? */
  if ((optopt = (int)*place++) == (int)':' ||
      (oli = strchr(ostr, optopt)) == NULL) {
    /*
     * if the user didn't specify '-' as an option,
     * assume it means EOF.
     */
    if (optopt == (int)'-') {
      return (EOF);
    }
    if (!*place) {
      ++optind;
    }
    if (opterr && *ostr != ':') {
      (void)fprintf(stderr, "%s: illegal option -- %c\n", __FILE__, optopt);
    }
    return (BADCH);
  }
  if (*++oli != ':') { /* don't need argument */
    optarg = NULL;
    if (!*place) {
      ++optind;
    }
  } else {                          /* need an argument */
    if (*place) {                   /* no white space */
      optarg = place;
    } else if (nargc <= ++optind) { /* no arg */
      place = EMSG;
      if (*ostr == ':') {
        return (BADARG);
      }
      if (opterr) {
        (void)fprintf(stderr, "%s: option requires an argument -- %c\n",
                      __FILE__, optopt);
      }
      return (BADCH);
    } else { /* white space */
      optarg = nargv[optind];
    }
    place = EMSG;
    ++optind;
  }
  return (optopt); /* dump back option letter */
}
