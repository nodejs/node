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


#include "uv.h"
#include "task.h"

#include <string.h>

#ifdef __APPLE__
# define NUM_ITERATIONS 20
#else
# define NUM_ITERATIONS 50
#endif

static const char* titles[] = {
  "8L2NY0Kdj0XyNFZnmUZigIOfcWjyNr0SkMmUhKw99VLUsZFrvCQQC3XIRfNR8pjyMjXObllled",
  "jUAcscJN49oLSN8GdmXj2Wo34XX2T2vp2j5khfajNQarlOulp57cE130yiY53ipJFnPyTn5i82",
  "9niCI5icXGFS72XudhXqo5alftmZ1tpE7B3cwUmrq0CCDjC84FzBNB8XAHqvpNQfI2QAQG6ztT",
  "n8qXVXuG6IEHDpabJgTEiwtpY6LHMZ8MgznnMpdHARu5EywufA6hcBaQfetb0YhEsK0ykDd7JU"
};

static void getter_thread_body(void* arg) {
  char buffer[512];

  for (;;) {
    ASSERT(0 == uv_get_process_title(buffer, sizeof(buffer)));
    ASSERT(
      0 == strcmp(buffer, titles[0]) ||
      0 == strcmp(buffer, titles[1]) ||
      0 == strcmp(buffer, titles[2]) ||
      0 == strcmp(buffer, titles[3]));

    uv_sleep(0);
  }
}


static void setter_thread_body(void* arg) {
  int i;

  for (i = 0; i < NUM_ITERATIONS; i++) {
    ASSERT(0 == uv_set_process_title(titles[0]));
    ASSERT(0 == uv_set_process_title(titles[1]));
    ASSERT(0 == uv_set_process_title(titles[2]));
    ASSERT(0 == uv_set_process_title(titles[3]));
  }
}


TEST_IMPL(process_title_threadsafe) {
  uv_thread_t setter_threads[4];
  uv_thread_t getter_thread;
  int i;

#if defined(__sun) || defined(__CYGWIN__) || defined(__MSYS__) || \
    defined(__MVS__)
  RETURN_SKIP("uv_(get|set)_process_title is not implemented.");
#else

  ASSERT(0 == uv_set_process_title(titles[0]));
  ASSERT(0 == uv_thread_create(&getter_thread, getter_thread_body, NULL));

  for (i = 0; i < (int) ARRAY_SIZE(setter_threads); i++)
    ASSERT(0 == uv_thread_create(&setter_threads[i], setter_thread_body, NULL));

  for (i = 0; i < (int) ARRAY_SIZE(setter_threads); i++)
    ASSERT(0 == uv_thread_join(&setter_threads[i]));

  return 0;
#endif
}
