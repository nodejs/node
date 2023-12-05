/* MIT License
 *
 * Copyright (c) 2010 Daniel Stenberg
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
#ifndef HEADER_CARES_NOWARN_H
#define HEADER_CARES_NOWARN_H

long  aresx_uztosl(size_t uznum);
int   aresx_uztosi(size_t uznum);
short aresx_uztoss(size_t uznum);

short aresx_sitoss(int sinum);

int aresx_sltosi(long slnum);

int aresx_sztosi(ares_ssize_t sznum);

unsigned int aresx_sztoui(ares_ssize_t sznum);

unsigned short aresx_sitous(int sinum);

#if defined(__INTEL_COMPILER) && defined(__unix__)

int aresx_FD_ISSET(int fd, fd_set *fdset);

void aresx_FD_SET(int fd, fd_set *fdset);

void aresx_FD_ZERO(fd_set *fdset);

unsigned short aresx_htons(unsigned short usnum);

unsigned short aresx_ntohs(unsigned short usnum);

#ifndef BUILDING_ARES_NOWARN_C
#  undef  FD_ISSET
#  define FD_ISSET(a,b) aresx_FD_ISSET((a),(b))
#  undef  FD_SET
#  define FD_SET(a,b)   aresx_FD_SET((a),(b))
#  undef  FD_ZERO
#  define FD_ZERO(a)    aresx_FD_ZERO((a))
#  undef  htons
#  define htons(a)      aresx_htons((a))
#  undef  ntohs
#  define ntohs(a)      aresx_ntohs((a))
#endif

#endif /* __INTEL_COMPILER && __unix__ */

#endif /* HEADER_CARES_NOWARN_H */
