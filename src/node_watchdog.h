#ifndef SRC_NODE_WATCHDOG_H_
#define SRC_NODE_WATCHDOG_H_

#include "v8.h"
#include "uv.h"

#include "env.h"

namespace node {

class Watchdog {
 public:
  explicit Watchdog(Environment* env, uint64_t ms);
  ~Watchdog();

  void Dispose();

  inline Environment* env() const { return env_; }

 private:
  void Destroy();

  static void Run(void* arg);
  static void Async(uv_async_t* async);
  static void Timer(uv_timer_t* timer);

  Environment* env_;
  uv_thread_t thread_;
  uv_loop_t* loop_;
  uv_async_t async_;
  uv_timer_t timer_;
  bool destroyed_;
};

}  // namespace node

#endif  // SRC_NODE_WATCHDOG_H_
