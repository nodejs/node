#ifndef SRC_DEBUG_UTILS_H_
#define SRC_DEBUG_UTILS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async_wrap.h"
#include "env.h"
#include <string>

namespace node {

template <typename... Args>
inline void Debug(Environment* env,
                  DebugCategory cat,
                  const char* format,
                  Args&&... args) {
  if (!env->debug_enabled(cat))
    return;
  fprintf(stderr, format, std::forward<Args>(args)...);
}

inline void Debug(Environment* env,
                  DebugCategory cat,
                  const char* message) {
  if (!env->debug_enabled(cat))
    return;
  fprintf(stderr, "%s", message);
}

template <typename... Args>
inline void Debug(Environment* env,
                  DebugCategory cat,
                  const std::string& format,
                  Args&&... args) {
  Debug(env, cat, format.c_str(), std::forward<Args>(args)...);
}

template <typename... Args>
inline void Debug(AsyncWrap* async_wrap,
                  const char* format,
                  Args&&... args) {
#ifdef DEBUG
  CHECK_NOT_NULL(async_wrap);
#endif
  DebugCategory cat =
      static_cast<DebugCategory>(async_wrap->provider_type());
  if (!async_wrap->env()->debug_enabled(cat))
    return;
  Debug(async_wrap->env(),
        cat,
        async_wrap->diagnostic_name() + " " + format + "\n",
        std::forward<Args>(args)...);
}

template <typename... Args>
inline void Debug(AsyncWrap* async_wrap,
                  const std::string& format,
                  Args&&... args) {
  Debug(async_wrap, format.c_str(), std::forward<Args>(args)...);
}


}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_DEBUG_UTILS_H_
