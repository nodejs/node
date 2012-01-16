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


static uv_idle_t idle;

static const int max_opened = 10000;
static const int max_delta = 4000;

static int opened = 0;
static int closed = 0;


void work_cb(uv_work_t* work) {
  /* continue as fast as possible */
}


void after_work_cb(uv_work_t* work) {
  free(work);
  closed++;
}


void make_eio_req(void) {
  uv_work_t* w;

  opened++;

  w = (uv_work_t*) malloc(sizeof(*w));
  ASSERT(w != NULL);

  uv_queue_work(uv_default_loop(), w, work_cb, after_work_cb);
}


void idle_cb(uv_idle_t* idle, int status) {
  ASSERT(opened - closed < max_delta);
  if (opened <= max_opened) {
    int i;
    for (i = 0; i < 30; i++) {
      make_eio_req();
    }
  } else {
    int r;

    r = uv_idle_stop(idle);
    uv_unref(uv_default_loop());
    ASSERT(r == 0);
  }
}


TEST_IMPL(eio_overflow) {
  int r;

  r = uv_idle_init(uv_default_loop(), &idle);
  ASSERT(r == 0);

  r = uv_idle_start(&idle, idle_cb);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop());
  ASSERT(r == 0);

  return 0;
}
