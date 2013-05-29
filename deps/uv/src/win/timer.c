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
#include "handle-inl.h"


void uv_update_time(uv_loop_t* loop) {
  DWORD ticks;
  ULARGE_INTEGER time;

  ticks = GetTickCount();

  time.QuadPart = loop->time;

  /* GetTickCount() can conceivably wrap around, so when the current tick */
  /* count is lower than the last tick count, we'll assume it has wrapped. */
  /* uv_poll must make sure that the timer can never overflow more than */
  /* once between two subsequent uv_update_time calls. */
  time.LowPart = ticks;
  if (ticks < loop->last_tick_count)
    time.HighPart++;

  /* Remember the last tick count. */
  loop->last_tick_count = ticks;

  /* The GetTickCount() resolution isn't too good. Sometimes it'll happen */
  /* that GetQueuedCompletionStatus() or GetQueuedCompletionStatusEx() has */
  /* waited for a couple of ms but this is not reflected in the GetTickCount */
  /* result yet. Therefore whenever GetQueuedCompletionStatus times out */
  /* we'll add the number of ms that it has waited to the current loop time. */
  /* When that happened the loop time might be a little ms farther than what */
  /* we've just computed, and we shouldn't update the loop time. */
  if (loop->time < time.QuadPart)
    loop->time = time.QuadPart;
}


void uv__time_forward(uv_loop_t* loop, uint64_t msecs) {
  loop->time += msecs;
}


static int uv_timer_compare(uv_timer_t* a, uv_timer_t* b) {
  if (a->due < b->due)
    return -1;
  if (a->due > b->due)
    return 1;
  /*
   *  compare start_id when both has the same due. start_id is
   *  allocated with loop->timer_counter in uv_timer_start().
   */
  if (a->start_id < b->start_id)
    return -1;
  if (a->start_id > b->start_id)
    return 1;
  return 0;
}


RB_GENERATE_STATIC(uv_timer_tree_s, uv_timer_s, tree_entry, uv_timer_compare);


int uv_timer_init(uv_loop_t* loop, uv_timer_t* handle) {
  uv__handle_init(loop, (uv_handle_t*) handle, UV_TIMER);
  handle->timer_cb = NULL;
  handle->repeat = 0;

  return 0;
}


void uv_timer_endgame(uv_loop_t* loop, uv_timer_t* handle) {
  if (handle->flags & UV__HANDLE_CLOSING) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    uv__handle_close(handle);
  }
}


static uint64_t get_clamped_due_time(uint64_t loop_time, uint64_t timeout) {
  uint64_t clamped_timeout;

  clamped_timeout = loop_time + timeout;
  if (clamped_timeout < timeout)
    clamped_timeout = (uint64_t) -1;

  return clamped_timeout;
}


int uv_timer_start(uv_timer_t* handle, uv_timer_cb timer_cb, uint64_t timeout,
    uint64_t repeat) {
  uv_loop_t* loop = handle->loop;
  uv_timer_t* old;

  if (handle->flags & UV_HANDLE_ACTIVE) {
    RB_REMOVE(uv_timer_tree_s, &loop->timers, handle);
  }

  handle->timer_cb = timer_cb;
  handle->due = get_clamped_due_time(loop->time, timeout);
  handle->repeat = repeat;
  handle->flags |= UV_HANDLE_ACTIVE;
  uv__handle_start(handle);

  /* start_id is the second index to be compared in uv__timer_cmp() */
  handle->start_id = handle->loop->timer_counter++;

  old = RB_INSERT(uv_timer_tree_s, &loop->timers, handle);
  assert(old == NULL);

  return 0;
}


int uv_timer_stop(uv_timer_t* handle) {
  uv_loop_t* loop = handle->loop;

  if (!(handle->flags & UV_HANDLE_ACTIVE))
    return 0;

  RB_REMOVE(uv_timer_tree_s, &loop->timers, handle);

  handle->flags &= ~UV_HANDLE_ACTIVE;
  uv__handle_stop(handle);

  return 0;
}


int uv_timer_again(uv_timer_t* handle) {
  uv_loop_t* loop = handle->loop;

  /* If timer_cb is NULL that means that the timer was never started. */
  if (!handle->timer_cb) {
    uv__set_sys_error(loop, ERROR_INVALID_DATA);
    return -1;
  }

  if (handle->flags & UV_HANDLE_ACTIVE) {
    RB_REMOVE(uv_timer_tree_s, &loop->timers, handle);
    handle->flags &= ~UV_HANDLE_ACTIVE;
    uv__handle_stop(handle);
  }

  if (handle->repeat) {
    handle->due = get_clamped_due_time(loop->time, handle->repeat);

    if (RB_INSERT(uv_timer_tree_s, &loop->timers, handle) != NULL) {
      uv_fatal_error(ERROR_INVALID_DATA, "RB_INSERT");
    }

    handle->flags |= UV_HANDLE_ACTIVE;
    uv__handle_start(handle);
  }

  return 0;
}


void uv_timer_set_repeat(uv_timer_t* handle, uint64_t repeat) {
  assert(handle->type == UV_TIMER);
  handle->repeat = repeat;
}


uint64_t uv_timer_get_repeat(const uv_timer_t* handle) {
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
      timer->due = get_clamped_due_time(timer->due, timer->repeat);
      if (timer->due < loop->time) {
        timer->due = loop->time;
      }
      if (RB_INSERT(uv_timer_tree_s, &loop->timers, timer) != NULL) {
        uv_fatal_error(ERROR_INVALID_DATA, "RB_INSERT");
      }
    } else {
      /* If non-repeating, mark the timer as inactive. */
      timer->flags &= ~UV_HANDLE_ACTIVE;
      uv__handle_stop(timer);
    }

    timer->timer_cb((uv_timer_t*) timer, 0);
  }
}
