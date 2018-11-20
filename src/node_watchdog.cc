#include "node_watchdog.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"

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

  uv_close(reinterpret_cast<uv_handle_t*>(&async_), nullptr);

  // UV_RUN_DEFAULT so that libuv has a chance to clean up.
  uv_run(loop_, UV_RUN_DEFAULT);

  int rc = uv_loop_close(loop_);
  CHECK_EQ(0, rc);
  delete loop_;
  loop_ = nullptr;

  destroyed_ = true;
}


void Watchdog::Run(void* arg) {
  Watchdog* wd = static_cast<Watchdog*>(arg);

  // UV_RUN_DEFAULT the loop will be stopped either by the async or the
  // timer handle.
  uv_run(wd->loop_, UV_RUN_DEFAULT);

  // Loop ref count reaches zero when both handles are closed.
  // Close the timer handle on this side and let Destroy() close async_
  uv_close(reinterpret_cast<uv_handle_t*>(&wd->timer_), nullptr);
}


void Watchdog::Async(uv_async_t* async) {
  Watchdog* w = ContainerOf(&Watchdog::async_, async);
  uv_stop(w->loop_);
}


void Watchdog::Timer(uv_timer_t* timer) {
  Watchdog* w = ContainerOf(&Watchdog::timer_, timer);
  uv_stop(w->loop_);
  V8::TerminateExecution(w->env()->isolate());
}


}  // namespace node
