/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

#ifndef TASK_H_
#define TASK_H_


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_PORT 9123
#define TEST_PORT_2 9124


/* Log to stderr. */
#define LOG(...)    fprintf(stderr, "%s", __VA_ARGS__)
#define LOGF(...)   fprintf(stderr, __VA_ARGS__)

/* Die with fatal error. */
#define FATAL(msg)                                        \
  do {                                                    \
    fprintf(stderr,                                       \
            "Fatal error in %s on line %d: %s\n",         \
            __FILE__,                                     \
            __LINE__,                                     \
            msg);                                         \
    abort();                                              \
  } while (0)



/* Have our own assert, so we are sure it does not get optimized away in
 * a release build.
 */
#define ASSERT(expr)                                      \
 do {                                                     \
  if (!(expr)) {                                          \
    fprintf(stderr,                                       \
            "Assertion failed in %s on line %d: %s\n",    \
            __FILE__,                                     \
            __LINE__,                                     \
            #expr);                                       \
    abort();                                              \
  }                                                       \
 } while (0)


/* Just sugar for wrapping the main() for a task or helper. */
#define TEST_IMPL(name)   \
  int run_test_##name()

#define BENCHMARK_IMPL(name)  \
  int run_benchmark_##name()

#define HELPER_IMPL(name)  \
  int run_helper_##name()


/* Create a thread. Returns the thread identifier, or 0 on failure. */
uintptr_t uv_create_thread(void (*entry)(void* arg), void* arg);

/* Wait for a thread to terminate. Should return 0 if the thread ended, -1 on
 * error.
 */
int uv_wait_thread(uintptr_t thread_id);

/* Pause the calling thread for a number of milliseconds. */
void uv_sleep(int msec);

#endif /* TASK_H_ */
