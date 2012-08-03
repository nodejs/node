/* Copyright Joyent, Inc. and other Node contributors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef NODE_EV_EMUL_H_
#define NODE_EV_EMUL_H_

#include "uv.h"

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef ev_init
#undef ev_set_cb
#undef ev_set_priority
#undef ev_is_active
#undef ev_timer_init
#undef ev_timer_set
#undef ev_io_init
#undef ev_io_set

#undef EV_P
#undef EV_P_
#undef EV_A
#undef EV_A_
#undef EV_DEFAULT
#undef EV_DEFAULT_
#undef EV_DEFAULT_UC
#undef EV_DEFAULT_UC_

#define EV_P            void
#define EV_P_
#define EV_A
#define EV_A_
#define EV_DEFAULT
#define EV_DEFAULT_
#define EV_DEFAULT_UC
#define EV_DEFAULT_UC_

#define ev_init(w, cb_)                                                       \
  do {                                                                        \
    void* data = (w)->data;                                                   \
    memset((w), 0, sizeof(*(w)));                                             \
    (w)->data = data;                                                         \
    (w)->cb = (cb_);                                                          \
  }                                                                           \
  while (0)

#define ev_set_cb(w, cb_)                                                     \
  do                                                                          \
    (w)->cb = (cb_);                                                          \
  while (0)

#define ev_is_active(w)                                                       \
  (uv_is_active((uv_handle_t*) &(w)->handle))

#define __uv_container_of(ptr, type, field)                                   \
  ((type*) ((char*) (ptr) - offsetof(type, field)))

#define __uv_warn_of(old_, new_)                                              \
  do {                                                                        \
    if (__uv_warn_##old_ || node::no_deprecation) break;                      \
    __uv_warn_##old_ = 1;                                                     \
    fputs("WARNING: " #old_ " is deprecated, use " #new_ "\n", stderr);       \
  }                                                                           \
  while (0)

static int __uv_warn_ev_io    __attribute__((unused));
static int __uv_warn_ev_timer __attribute__((unused));
static int __uv_warn_ev_ref   __attribute__((unused));
static int __uv_warn_ev_unref __attribute__((unused));

struct __ev_io;
typedef struct __ev_io __ev_io;
typedef void (*__ev_io_cb)(__ev_io*, int);

struct __ev_timer;
typedef struct __ev_timer __ev_timer;
typedef void (*__ev_timer_cb)(__ev_timer*, int);

/* Field order matters. Don't touch. */
struct __ev_io {
  __ev_io_cb cb;
  void* data;
  int flags; /* bit 1 == initialized, yes or no? */
  uv_poll_t handle;
  int fd;
  int events;
};

/* Field order matters. Don't touch. */
struct __ev_timer {
  __ev_timer_cb cb;
  void* data;
  int flags;
  uv_timer_t handle;
  double delay;
  double repeat;
};


inline static void __uv_poll_cb(uv_poll_t* handle, int status, int events) {
  __ev_io* w = __uv_container_of(handle, __ev_io, handle);
  w->cb(w, events);
  (void) status;
}


inline static void __uv_timer_cb(uv_timer_t* handle, int status) {
  __ev_timer* w = __uv_container_of(handle, __ev_timer, handle);
  w->cb(w, 0);
  (void) status;
}


inline static void __ev_io_init(__ev_io* w, __ev_io_cb cb, int fd, int events) {
  __uv_warn_of(ev_io, uv_poll_t);
  ev_init(w, cb);
  w->fd = fd;
  w->events = events;
}


inline static void __ev_io_set(__ev_io* w, int fd, int events) {
  __uv_warn_of(ev_io, uv_poll_t);
  w->fd = fd;
  w->events = events;
}


inline static void __ev_io_start(__ev_io* w) {
  __uv_warn_of(ev_io, uv_poll_t);
  if (!(w->flags & 1)) {
    uv_poll_init(uv_default_loop(), &w->handle, w->fd);
    w->flags |= 1;
  }
  uv_poll_start(&w->handle, w->events, __uv_poll_cb);
}


inline static void __ev_io_stop(__ev_io* w) {
  __uv_warn_of(ev_io, uv_poll_t);
  uv_poll_stop(&w->handle);
}


inline static void __ev_timer_init(__ev_timer* w,
                                   __ev_timer_cb cb,
                                   double delay,
                                   double repeat) {
  __uv_warn_of(ev_timer, uv_timer_t);
  ev_init(w, cb);
  w->delay = delay;
  w->repeat = repeat;
}


inline static void __ev_timer_set(__ev_timer* w,
                                  double delay,
                                  double repeat) {
  __uv_warn_of(ev_timer, uv_timer_t);
  w->delay = delay;
  w->repeat = repeat;
}


inline static void __ev_timer_start(__ev_timer* w) {
  __uv_warn_of(ev_timer, uv_timer_t);
  if (!(w->flags & 1)) {
    uv_timer_init(uv_default_loop(), &w->handle);
    w->flags |= 1;
  }
  uv_timer_start(&w->handle,
                 __uv_timer_cb,
                 (int64_t) (w->delay * 1000),
                 (int64_t) (w->repeat * 1000));
}


inline static void __ev_timer_stop(__ev_timer* w) {
  __uv_warn_of(ev_timer, uv_timer_t);
  uv_timer_stop(&w->handle);
}


inline static void __ev_timer_again(__ev_timer* w) {
  __uv_warn_of(ev_timer, uv_timer_t);
  if (w->flags & 1)
    uv_timer_again(&w->handle);
  else
    __ev_timer_start(w); /* work around node-zookeeper bug */
}


/* Evil. Using this will void your warranty. */
inline static void __ev_ref(void) {
  __uv_warn_of(ev_ref, uv_ref);
  uv_default_loop()->active_handles++;
}


/* Evil. Using this will void your warranty. */
inline static void __ev_unref(void) {
  __uv_warn_of(ev_unref, uv_unref);
  uv_default_loop()->active_handles--;
}


inline static double __ev_now(...) {
  return uv_hrtime() / 1e9;
}


inline static void __ev_set_priority(...) {
}


#define ev_io           __ev_io
#define ev_io_init      __ev_io_init
#define ev_io_set       __ev_io_set
#define ev_io_start     __ev_io_start
#define ev_io_stop      __ev_io_stop

#define ev_timer        __ev_timer
#define ev_timer_init   __ev_timer_init
#define ev_timer_set    __ev_timer_set
#define ev_timer_start  __ev_timer_start
#define ev_timer_stop   __ev_timer_stop
#define ev_timer_again  __ev_timer_again

#define ev_ref          __ev_ref
#define ev_unref        __ev_unref

#define ev_now          __ev_now
#define ev_set_priority __ev_set_priority

#undef __uv_container_of
#undef __uv_warn_of

#ifdef __cplusplus
}
#endif

#endif /* NODE_EV_EMUL_H_ */
