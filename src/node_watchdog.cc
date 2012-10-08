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

namespace node {

using v8::V8;


Watchdog::Watchdog(uint64_t ms)
  : timer_started_(false)
  , thread_created_(false)
  , destroyed_(false) {

  loop_ = uv_loop_new();
  if (!loop_)
    return;

  int rc = uv_timer_init(loop_, &timer_);
  if (rc) {
    return;
  }

  rc = uv_timer_start(&timer_, &Watchdog::Timer, ms, 0);
  if (rc) {
    return;
  }
  timer_started_ = true;

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

  if (timer_started_) {
    uv_timer_stop(&timer_);
  }

  if (loop_) {
    uv_loop_delete(loop_);
  }

  if (thread_created_) {
    uv_thread_join(&thread_);
  }

  destroyed_ = true;
}


void Watchdog::Run(void* arg) {
  Watchdog* wd = static_cast<Watchdog*>(arg);
  uv_run(wd->loop_, UV_RUN_DEFAULT);
}


void Watchdog::Timer(uv_timer_t* timer, int status) {
  V8::TerminateExecution();
}


}  // namespace node
