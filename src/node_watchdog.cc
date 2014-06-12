// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node_watchdog.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include <assert.h>

namespace node {

using v8::V8;


Watchdog::Watchdog(Environment* env, uint64_t ms) : env_(env),
                                                    destroyed_(false) {
  int rc;
  loop_ = new uv_loop_t;
  CHECK(loop_);
  rc = uv_loop_init(loop_);
  CHECK_EQ(0, rc);

  rc = uv_async_init(loop_, &async_, &Watchdog::Async);
  CHECK_EQ(0, rc);

  rc = uv_timer_init(loop_, &timer_);
  CHECK_EQ(0, rc);

  rc = uv_timer_start(&timer_, &Watchdog::Timer, ms, 0);
  CHECK_EQ(0, rc);

  rc = uv_thread_create(&thread_, &Watchdog::Run, this);
  CHECK_EQ(0, rc);
}


Watchdog::~Watchdog() {
  Destroy();
}


void Watchdog::Dispose() {
  Destroy();
}


void Watchdog::Destroy() {
  if (destroyed_) {
    return;
  }

  uv_async_send(&async_);
  uv_thread_join(&thread_);

  uv_close(reinterpret_cast<uv_handle_t*>(&async_), NULL);

  // UV_RUN_DEFAULT so that libuv has a chance to clean up.
  uv_run(loop_, UV_RUN_DEFAULT);

  int rc = uv_loop_close(loop_);
  CHECK_EQ(0, rc);
  delete loop_;
  loop_ = NULL;

  destroyed_ = true;
}


void Watchdog::Run(void* arg) {
  Watchdog* wd = static_cast<Watchdog*>(arg);

  // UV_RUN_ONCE so async_ or timer_ wakeup exits uv_run() call.
  uv_run(wd->loop_, UV_RUN_ONCE);

  // Loop ref count reaches zero when both handles are closed.
  // Close the timer handle on this side and let Destroy() close async_
  uv_close(reinterpret_cast<uv_handle_t*>(&wd->timer_), NULL);
}


void Watchdog::Async(uv_async_t* async) {
}


void Watchdog::Timer(uv_timer_t* timer) {
  Watchdog* w = ContainerOf(&Watchdog::timer_, timer);
  V8::TerminateExecution(w->env()->isolate());
}


}  // namespace node
