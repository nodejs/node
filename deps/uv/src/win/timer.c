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

#include <assert.h>
#include <limits.h>

#include "uv.h"
#include "internal.h"
#include "tree.h"


/* The resolution of the high-resolution clock. */
static int64_t uv_ticks_per_msec_ = 0;



void uv_update_time() {
  LARGE_INTEGER counter;

  if (!QueryPerformanceCounter(&counter))
    uv_fatal_error(GetLastError(), "QueryPerformanceCounter");

  LOOP->time = counter.QuadPart / uv_ticks_per_msec_;
}


int64_t uv_now() {
  return LOOP->time;
}


uint64_t uv_hrtime(void) {
  assert(0 && "implement me");
  return 0;
}


void uv_timer_startup() {
  LARGE_INTEGER timer_frequency;

  /* Initialize the event loop time */
  if (!QueryPerformanceFrequency(&timer_frequency))
    uv_fatal_error(GetLastError(), "QueryPerformanceFrequency");
  uv_ticks_per_msec_ = timer_frequency.QuadPart / 1000;
}


static int uv_timer_compare(uv_timer_t* a, uv_timer_t* b) {
  if (a->due < b->due)
    return -1;
  if (a->due > b->due)
    return 1;
  if ((intptr_t)a < (intptr_t)b)
    return -1;
  if ((intptr_t)a > (intptr_t)b)
    return 1;
  return 0;
}


RB_GENERATE_STATIC(uv_timer_tree_s, uv_timer_s, tree_entry, uv_timer_compare);


int uv_timer_init(uv_timer_t* handle) {
  uv_counters()->handle_init++;
  uv_counters()->timer_init++;

  handle->type = UV_TIMER;
  handle->flags = 0;
  handle->error = uv_ok_;
  handle->timer_cb = NULL;
  handle->repeat = 0;

  uv_ref();

  return 0;
}


void uv_timer_endgame(uv_timer_t* handle) {
  if (handle->flags & UV_HANDLE_CLOSING) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb((uv_handle_t*)handle);
    }

    uv_unref();
  }
}


int uv_timer_start(uv_timer_t* handle, uv_timer_cb timer_cb, int64_t timeout, int64_t repeat) {
  if (handle->flags & UV_HANDLE_ACTIVE) {
    RB_REMOVE(uv_timer_tree_s, &LOOP->timers, handle);
  }

  handle->timer_cb = timer_cb;
  handle->due = LOOP->time + timeout;
  handle->repeat = repeat;
  handle->flags |= UV_HANDLE_ACTIVE;

  if (RB_INSERT(uv_timer_tree_s, &LOOP->timers, handle) != NULL) {
    uv_fatal_error(ERROR_INVALID_DATA, "RB_INSERT");
  }

  return 0;
}


int uv_timer_stop(uv_timer_t* handle) {
  if (!(handle->flags & UV_HANDLE_ACTIVE))
    return 0;

  RB_REMOVE(uv_timer_tree_s, &LOOP->timers, handle);

  handle->flags &= ~UV_HANDLE_ACTIVE;

  return 0;
}


int uv_timer_again(uv_timer_t* handle) {
  /* If timer_cb is NULL that means that the timer was never started. */
  if (!handle->timer_cb) {
    uv_set_sys_error(ERROR_INVALID_DATA);
    return -1;
  }

  if (handle->flags & UV_HANDLE_ACTIVE) {
    RB_REMOVE(uv_timer_tree_s, &LOOP->timers, handle);
    handle->flags &= ~UV_HANDLE_ACTIVE;
  }

  if (handle->repeat) {
    handle->due = LOOP->time + handle->repeat;

    if (RB_INSERT(uv_timer_tree_s, &LOOP->timers, handle) != NULL) {
      uv_fatal_error(ERROR_INVALID_DATA, "RB_INSERT");
    }

    handle->flags |= UV_HANDLE_ACTIVE;
  }

  return 0;
}


void uv_timer_set_repeat(uv_timer_t* handle, int64_t repeat) {
  assert(handle->type == UV_TIMER);
  handle->repeat = repeat;
}


int64_t uv_timer_get_repeat(uv_timer_t* handle) {
  assert(handle->type == UV_TIMER);
  return handle->repeat;
}


DWORD uv_get_poll_timeout() {
  uv_timer_t* timer;
  int64_t delta;

  /* Check if there are any running timers */
  timer = RB_MIN(uv_timer_tree_s, &LOOP->timers);
  if (timer) {
    uv_update_time();

    delta = timer->due - LOOP->time;
    if (delta >= UINT_MAX) {
      /* Can't have a timeout greater than UINT_MAX, and a timeout value of */
      /* UINT_MAX means infinite, so that's no good either. */
      return UINT_MAX - 1;
    } else if (delta < 0) {
      /* Negative timeout values are not allowed */
      return 0;
    } else {
      return (DWORD)delta;
    }
  } else {
    /* No timers */
    return INFINITE;
  }
}


void uv_process_timers() {
  uv_timer_t* timer;

  /* Call timer callbacks */
  for (timer = RB_MIN(uv_timer_tree_s, &LOOP->timers);
       timer != NULL && timer->due <= LOOP->time;
       timer = RB_MIN(uv_timer_tree_s, &LOOP->timers)) {
    RB_REMOVE(uv_timer_tree_s, &LOOP->timers, timer);

    if (timer->repeat != 0) {
      /* If it is a repeating timer, reschedule with repeat timeout. */
      timer->due += timer->repeat;
      if (timer->due < LOOP->time) {
        timer->due = LOOP->time;
      }
      if (RB_INSERT(uv_timer_tree_s, &LOOP->timers, timer) != NULL) {
        uv_fatal_error(ERROR_INVALID_DATA, "RB_INSERT");
      }
    } else {
      /* If non-repeating, mark the timer as inactive. */
      timer->flags &= ~UV_HANDLE_ACTIVE;
    }

    timer->timer_cb((uv_timer_t*) timer, 0);
  }
}
