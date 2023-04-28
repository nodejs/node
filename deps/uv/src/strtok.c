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

#include <stdlib.h>
#include "strtok.h"

char* uv__strtok(char* str, const char* sep, char** itr) {
  const char* sep_itr;
  char* tmp;
  char* start;

  if (str == NULL)
    start = tmp = *itr;
  else
    start = tmp = str;

  if (tmp == NULL)
    return NULL;

  while (*tmp != '\0') {
    sep_itr = sep;
    while (*sep_itr != '\0') {
      if (*tmp == *sep_itr) {
        *itr = tmp + 1;
        *tmp = '\0';
        return start;
      }
      sep_itr++;
    }
    tmp++;
  }
  *itr = NULL;
  return start;
}
