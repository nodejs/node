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

#include "uv.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>


static uv_prepare_t prepare_handle;

static uv_async_t async1_handle;
/* static uv_handle_t async2_handle; */

static int prepare_cb_called = 0;

static volatile int async1_cb_called = 0;
static int async1_closed = 0;
/* static volatile int async2_cb_called = 0; */

static int close_cb_called = 0;

static uintptr_t thread1_id = 0;
#if 0
static uintptr_t thread2_id = 0;
static uintptr_t thread3_id = 0;
#endif


/* Thread 1 makes sure that async1_cb_called reaches 3 before exiting. */
void thread1_entry(void *arg) {
  uv_sleep(50);

  while (1) {
    switch (async1_cb_called) {
      case 0:
        uv_async_send(&async1_handle);
        break;

      case 1:
        uv_async_send(&async1_handle);
        break;

      case 2:
        uv_async_send(&async1_handle);
        break;

      default:
        return;
    }
  }
}

#if 0
/* Thread 2 calls uv_async_send on async_handle_2 8 times. */
void thread2_entry(void *arg) {
  int i;

  while (1) {
    switch (async1_cb_called) {
      case 0:
        uv_async_send(&async2_handle);
        break;

      case 1:
        uv_async_send(&async2_handle);
        break;

      case 2:
        uv_async_send(&async2_handle);
        break;
    }
    uv_sleep(5);
  }

  if (async1_cb_called == 20) {
    uv_close(handle);
  }
}


/* Thread 3 calls uv_async_send on async_handle_2 8 times
 * after waiting half a second first.
 */
void thread3_entry(void *arg) {
  int i;

  for (i = 0; i < 8; i++) {
    uv_async_send(&async2_handle);
  }
}
#endif


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle != NULL);
  close_cb_called++;
}


static void async1_cb(uv_async_t* handle, int status) {
  ASSERT(handle == &async1_handle);
  ASSERT(status == 0);

  async1_cb_called++;
  printf("async1_cb #%d\n", async1_cb_called);

  if (async1_cb_called > 2 && !async1_closed) {
    async1_closed = 1;
    uv_close((uv_handle_t*)handle, close_cb);
  }
}


#if 0
static void async2_cb(uv_handle_t* handle, int status) {
  ASSERT(handle == &async2_handle);
  ASSERT(status == 0);

  async2_cb_called++;
  printf("async2_cb #%d\n", async2_cb_called);

  if (async2_cb_called == 16) {
    uv_close(handle);
  }
}
#endif


static void prepare_cb(uv_prepare_t* handle, int status) {
  ASSERT(handle == &prepare_handle);
  ASSERT(status == 0);

  switch (prepare_cb_called) {
    case 0:
      thread1_id = uv_create_thread(thread1_entry, NULL);
      ASSERT(thread1_id != 0);
      break;

#if 0
    case 1:
      thread2_id = uv_create_thread(thread2_entry, NULL);
      ASSERT(thread2_id != 0);
      break;

    case 2:
      thread3_id = uv_create_thread(thread3_entry, NULL);
      ASSERT(thread3_id != 0);
      break;
#endif

    case 1:
      uv_close((uv_handle_t*)handle, close_cb);
      break;

    default:
      FATAL("Should never get here");
  }

  prepare_cb_called++;
}


TEST_IMPL(async) {
  int r;

  r = uv_prepare_init(uv_default_loop(), &prepare_handle);
  ASSERT(r == 0);
  r = uv_prepare_start(&prepare_handle, prepare_cb);
  ASSERT(r == 0);

  r = uv_async_init(uv_default_loop(), &async1_handle, async1_cb);
  ASSERT(r == 0);

#if 0
  r = uv_async_init(&async2_handle, async2_cb, close_cb, NULL);
  ASSERT(r == 0);
#endif

  r = uv_run(uv_default_loop());
  ASSERT(r == 0);

  r = uv_wait_thread(thread1_id);
  ASSERT(r == 0);
#if 0
  r = uv_wait_thread(thread2_id);
  ASSERT(r == 0);
  r = uv_wait_thread(thread3_id);
  ASSERT(r == 0);
#endif

  ASSERT(prepare_cb_called == 2);
  ASSERT(async1_cb_called > 2);
  /* ASSERT(async2_cb_called = 16); */
  ASSERT(close_cb_called == 2);

  return 0;
}
