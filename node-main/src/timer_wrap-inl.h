#ifndef SRC_TIMER_WRAP_INL_H_
#define SRC_TIMER_WRAP_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "timer_wrap.h"

#include <utility>

#include "env.h"
#include "uv.h"

namespace node {

template <typename... Args>
inline TimerWrap::TimerWrap(Environment* env, Args&&... args)
    : env_(env), fn_(std::forward<Args>(args)...) {
  uv_timer_init(env->event_loop(), &timer_);
  timer_.data = this;
}

template <typename... Args>
inline TimerWrapHandle::TimerWrapHandle(Environment* env, Args&&... args) {
  timer_ = new TimerWrap(env, std::forward<Args>(args)...);
  env->AddCleanupHook(CleanupHook, this);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_TIMER_WRAP_INL_H_
