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

#undef NANOSEC
#define NANOSEC 1000000000


/* The resolution of the high-resolution clock. */
static int64_t uv_ticks_per_msec_ = 0;
static uint64_t uv_hrtime_frequency_ = 0;
static uv_once_t uv_hrtime_init_guard_ = UV_ONCE_INIT;


void uv_update_time(uv_loop_t* loop) {
  DWORD ticks = GetTickCount();

  /* The assumption is made that LARGE_INTEGER.QuadPart has the same type */
  /* loop->time, which happens to be. Is there any way to assert this? */
  LARGE_INTEGER* time = (LARGE_INTEGER*) &loop->time;

  /* If the timer has wrapped, add 1 to it's high-order dword. */
  /* uv_poll must make sure that the timer can never overflow more than */
  /* once between two subsequent uv_update_time calls. */
  if (ticks < time->LowPart) {
    time->HighPart += 1;
  }
  time->LowPart = ticks;
}


int64_t uv_now(uv_loop_t* loop) {
  return loop->time;
}


static void uv_hrtime_init(void) {
  LARGE_INTEGER frequency;

  if (!QueryPerformanceFrequency(&frequency)) {
    uv_hrtime_frequency_ = 0;
    return;
  }

  uv_hrtime_frequency_ = frequency.QuadPart;
}


uint64_t uv_hrtime(void) {
  LARGE_INTEGER counter;

  uv_once(&uv_hrtime_init_guard_, uv_hrtime_init);

  /* If the performance frequency is zero, there's no support. */
  if (!uv_hrtime_frequency_) {
    /* uv_set_sys_error(loop, ERROR_NOT_SUPPORTED); */
    return 0;
  }

  if (!QueryPerformanceCounter(&counter)) {
    /* uv_set_sys_error(loop, GetLastError()); */
    return 0;
  }

  /* Because we have no guarantee about the order of magniture of the */
  /* performance counter frequency, and there may not be much headroom to */
  /* multiply by NANOSEC without overflowing, we use 128-bit math instead. */
  return ((uint64_t) counter.LowPart * NANOSEC / uv_hrtime_frequency_) +
         (((uint64_t) counter.HighPart * NANOSEC / uv_hrtime_frequency_)
         << 32);
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


int uv_timer_init(uv_loop_t* loop, uv_timer_t* handle) {
  loop->counters.handle_init++;
  loop->counters.timer_init++;

  handle->type = UV_TIMER;
  handle->loop = loop;
  handle->flags = 0;
  handle->timer_cb = NULL;
  handle->repeat = 0;

  uv_ref(loop);

  return 0;
}


void uv_timer_endgame(uv_loop_t* loop, uv_timer_t* handle) {
  if (handle->flags & UV_HANDLE_CLOSING) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb((uv_handle_t*)handle);
    }

    uv_unref(loop);
  }
}


int uv_timer_start(uv_timer_t* handle, uv_timer_cb timer_cb, int64_t timeout,
    int64_t repeat) {
  uv_loop_t* loop = handle->loop;

  if (handle->flags & UV_HANDLE_ACTIVE) {
    RB_REMOVE(uv_timer_tree_s, &loop->timers, handle);
  }

  handle->timer_cb = timer_cb;
  handle->due = loop->time + timeout;
  handle->repeat = repeat;
  handle->flags |= UV_HANDLE_ACTIVE;

  if (RB_INSERT(uv_timer_tree_s, &loop->timers, handle) != NULL) {
    uv_fatal_error(ERROR_INVALID_DATA, "RB_INSERT");
  }

  return 0;
}


int uv_timer_stop(uv_timer_t* handle) {
  uv_loop_t* loop = handle->loop;

  if (!(handle->flags & UV_HANDLE_ACTIVE))
    return 0;

  RB_REMOVE(uv_timer_tree_s, &loop->timers, handle);

  handle->flags &= ~UV_HANDLE_ACTIVE;

  return 0;
}


int uv_timer_again(uv_timer_t* handle) {
  uv_loop_t* loop = handle->loop;

  /* If timer_cb is NULL that means that the timer was never started. */
  if (!handle->timer_cb) {
    uv_set_sys_error(loop, ERROR_INVALID_DATA);
    return -1;
  }

  if (handle->flags & UV_HANDLE_ACTIVE) {
    RB_REMOVE(uv_timer_tree_s, &loop->timers, handle);
    handle->flags &= ~UV_HANDLE_ACTIVE;
  }

  if (handle->repeat) {
    handle->due = loop->time + handle->repeat;

    if (RB_INSERT(uv_timer_tree_s, &loop->timers, handle) != NULL) {
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


DWORD uv_get_poll_timeout(uv_loop_t* loop) {
  uv_timer_t* timer;
  int64_t delta;

  /* Check if there are any running timers */
  timer = RB_MIN(uv_timer_tree_s, &loop->timers);
  if (timer) {
    uv_update_time(loop);

    delta = timer->due - loop->time;
    if (delta >= UINT_MAX >> 1) {
      /* A timeout value of UINT_MAX means infinite, so that's no good. But */
      /* more importantly, there's always the risk that GetTickCount wraps. */
      /* uv_update_time can detect this, but we must make sure that the */
      /* tick counter never overflows twice between two subsequent */
      /* uv_update_time calls. We do this by never sleeping more than half */
      /* the time it takes to wrap  the counter - which is huge overkill, */
      /* but hey, it's not so bad to wake up every 25 days. */
      return UINT_MAX >> 1;
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


void uv_process_timers(uv_loop_t* loop) {
  uv_timer_t* timer;

  /* Call timer callbacks */
  for (timer = RB_MIN(uv_timer_tree_s, &loop->timers);
       timer != NULL && timer->due <= loop->time;
       timer = RB_MIN(uv_timer_tree_s, &loop->timers)) {
    RB_REMOVE(uv_timer_tree_s, &loop->timers, timer);

    if (timer->repeat != 0) {
      /* If it is a repeating timer, reschedule with repeat timeout. */
      timer->due += timer->repeat;
      if (timer->due < loop->time) {
        timer->due = loop->time;
      }
      if (RB_INSERT(uv_timer_tree_s, &loop->timers, timer) != NULL) {
        uv_fatal_error(ERROR_INVALID_DATA, "RB_INSERT");
      }
    } else {
      /* If non-repeating, mark the timer as inactive. */
      timer->flags &= ~UV_HANDLE_ACTIVE;
    }

    timer->timer_cb((uv_timer_t*) timer, 0);
  }
}
