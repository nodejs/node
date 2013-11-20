/* Copyright StrongLoop, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "defs.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void pr_do(FILE *stream,
                  const char *label,
                  const char *fmt,
                  va_list ap);

void *xmalloc(size_t size) {
  void *ptr;

  ptr = malloc(size);
  if (ptr == NULL) {
    pr_err("out of memory, need %lu bytes", (unsigned long) size);
    exit(1);
  }

  return ptr;
}

void pr_info(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pr_do(stdout, "info", fmt, ap);
  va_end(ap);
}

void pr_warn(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pr_do(stderr, "warn", fmt, ap);
  va_end(ap);
}

void pr_err(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  pr_do(stderr, "error", fmt, ap);
  va_end(ap);
}

static void pr_do(FILE *stream,
                  const char *label,
                  const char *fmt,
                  va_list ap) {
  char fmtbuf[1024];
  vsnprintf(fmtbuf, sizeof(fmtbuf), fmt, ap);
  fprintf(stream, "%s:%s: %s\n", _getprogname(), label, fmtbuf);
}
