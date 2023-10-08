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

#include "ares.h"
#include "ares_nowarn.h"
#include "ares_private.h"

/* This is an internal function.  Its contract is to read a line from
 * a file into a dynamically allocated buffer, zeroing the trailing
 * newline if there is one.  The calling routine may call
 * ares__read_line multiple times with the same buf and bufsize
 * pointers; *buf will be reallocated and *bufsize adjusted as
 * appropriate.  The initial value of *buf should be NULL.  After the
 * calling routine is done reading lines, it should free *buf.
 */
int ares__read_line(FILE *fp, char **buf, size_t *bufsize)
{
  char *newbuf;
  size_t offset = 0;
  size_t len;

  if (*buf == NULL)
    {
      *buf = ares_malloc(128);
      if (!*buf)
        return ARES_ENOMEM;
      *bufsize = 128;
    }

  for (;;)
    {
      int bytestoread = aresx_uztosi(*bufsize - offset);

      if (!fgets(*buf + offset, bytestoread, fp))
        return (offset != 0) ? 0 : (ferror(fp)) ? ARES_EFILE : ARES_EOF;
      len = offset + strlen(*buf + offset);
      if ((*buf)[len - 1] == '\n')
        {
          (*buf)[len - 1] = 0;
          break;
        }
      offset = len;
      if(len < *bufsize - 1)
        continue;

      /* Allocate more space. */
      newbuf = ares_realloc(*buf, *bufsize * 2);
      if (!newbuf)
        {
          ares_free(*buf);
          *buf = NULL;
          return ARES_ENOMEM;
        }
      *buf = newbuf;
      *bufsize *= 2;
    }
  return ARES_SUCCESS;
}
