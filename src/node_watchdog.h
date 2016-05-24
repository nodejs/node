#ifndef SRC_NODE_WATCHDOG_H_
#define SRC_NODE_WATCHDOG_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"
#include "uv.h"

namespace node {

class Watchdog {
 public:
  explicit Watchdog(v8::Isolate* isolate, uint64_t ms);
  ~Watchdog();

  void Dispose();

  v8::Isolate* isolate() { return isolate_; }

 private:
  void Destroy();

  static void Run(void* arg);
  static void Async(uv_async_t* async);
  static void Timer(uv_timer_t* timer);

  v8::Isolate* isolate_;
  uv_thread_t thread_;
  uv_loop_t* loop_;
  uv_async_t async_;
  uv_timer_t timer_;
  bool destroyed_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_WATCHDOG_H_
