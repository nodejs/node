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
#include <assert.h>

namespace node {

using v8::V8;


Watchdog::Watchdog(uint64_t ms)
  : thread_created_(false)
  , destroyed_(false) {

  loop_ = uv_loop_new();
  if (!loop_)
    return;

  int rc = uv_async_init(loop_, &async_, &Watchdog::Async);
  assert(rc == 0);

  rc = uv_timer_init(loop_, &timer_);
  assert(rc == 0);

  rc = uv_timer_start(&timer_, &Watchdog::Timer, ms, 0);
  if (rc) {
    return;
  }

  rc = uv_thread_create(&thread_, &Watchdog::Run, this);
  if (rc) {
    return;
  }
  thread_created_ = true;
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

  if (thread_created_) {
    uv_async_send(&async_);
    uv_thread_join(&thread_);
  }

  if (loop_) {
    uv_loop_delete(loop_);
  }

  destroyed_ = true;
}


void Watchdog::Run(void* arg) {
  Watchdog* wd = static_cast<Watchdog*>(arg);

  // UV_RUN_ONCE so async_ or timer_ wakeup exits uv_run() call.
  uv_run(wd->loop_, UV_RUN_ONCE);

  // Loop ref count reaches zero when both handles are closed.
  uv_close(reinterpret_cast<uv_handle_t*>(&wd->async_), NULL);
  uv_close(reinterpret_cast<uv_handle_t*>(&wd->timer_), NULL);

  // UV_RUN_DEFAULT so that libuv has a chance to clean up.
  uv_run(wd->loop_, UV_RUN_DEFAULT);
}


void Watchdog::Async(uv_async_t* async, int status) {
}


void Watchdog::Timer(uv_timer_t* timer, int status) {
  V8::TerminateExecution();
}


}  // namespace node
