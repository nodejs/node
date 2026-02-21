/* Copyright libuv project contributors. All rights reserved.
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

#ifndef UV_STRSCPY_H_
#define UV_STRSCPY_H_

/* Include uv.h for its definitions of size_t and ssize_t.
 * size_t can be obtained directly from <stddef.h> but ssize_t requires
 * some hoop jumping on Windows that I didn't want to duplicate here.
 */
#include "uv.h"

/* Copies up to |n-1| bytes from |s| to |d| and always zero-terminates
 * the result, except when |n==0|. Returns the number of bytes copied
 * or UV_E2BIG if |d| is too small.
 *
 * See https://www.kernel.org/doc/htmldocs/kernel-api/API-strscpy.html
 */
ssize_t uv__strscpy(char* d, const char* s, size_t n);

#endif  /* UV_STRSCPY_H_ */
