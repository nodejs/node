#ifndef SRC_TIMER_WRAP_H_
#define SRC_TIMER_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "memory_tracker.h"
#include "env.h"
#include "uv.h"

#include <functional>

namespace node {

// Utility class that makes working with libuv timers a bit easier.
class TimerWrap final : public MemoryRetainer {
 public:
  using TimerCb = std::function<void(void*)>;

  TimerWrap(Environment* env, TimerCb fn, void* user_data);
  TimerWrap(const TimerWrap&) = delete;

  inline Environment* env() const { return env_; }

  // Completely stops the timer, making it no longer usable.
  void Stop(bool close = true);

  // Starts / Restarts the Timer
  void Update(uint64_t interval, uint64_t repeat = 0);

  void Ref();

  void Unref();

  SET_NO_MEMORY_INFO();
  SET_MEMORY_INFO_NAME(TimerWrap)
  SET_SELF_SIZE(TimerWrap)

 private:
  static void TimerClosedCb(uv_handle_t* handle);
  static void OnTimeout(uv_timer_t* timer);
  ~TimerWrap() = default;

  Environment* env_;
  TimerCb fn_;
  uv_timer_t timer_;
  void* user_data_ = nullptr;

  friend std::unique_ptr<TimerWrap>::deleter_type;
};

class TimerWrapHandle : public MemoryRetainer  {
 public:
  TimerWrapHandle(
      Environment* env,
      TimerWrap::TimerCb fn,
      void* user_data = nullptr);

  TimerWrapHandle(const TimerWrapHandle&) = delete;

  ~TimerWrapHandle() { Stop(); }

  void Update(uint64_t interval, uint64_t repeat = 0);

  void Ref();

  void Unref();

  void Stop(bool close = true);

  void MemoryInfo(node::MemoryTracker* tracker) const override;

  SET_MEMORY_INFO_NAME(TimerWrapHandle)
  SET_SELF_SIZE(TimerWrapHandle)

 private:
  static void CleanupHook(void* data);
  TimerWrap* timer_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_TIMER_WRAP_H_
