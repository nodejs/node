#ifndef SRC_DEBUG_UTILS_H_
#define SRC_DEBUG_UTILS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async_wrap.h"
#include "env.h"
#include <string>

// Use FORCE_INLINE on functions that have a debug-category-enabled check first
// and then ideally only a single function call following it, to maintain
// performance for the common case (no debugging used).
#ifdef __GNUC__
#define FORCE_INLINE __attribute__((always_inline))
#define COLD_NOINLINE __attribute__((cold, noinline))
#else
#define FORCE_INLINE
#define COLD_NOINLINE
#endif

namespace node {

template <typename... Args>
inline void FORCE_INLINE Debug(Environment* env,
                               DebugCategory cat,
                               const char* format,
                               Args&&... args) {
  if (!UNLIKELY(env->debug_enabled(cat)))
    return;
  fprintf(stderr, format, std::forward<Args>(args)...);
}

inline void FORCE_INLINE Debug(Environment* env,
                               DebugCategory cat,
                               const char* message) {
  if (!UNLIKELY(env->debug_enabled(cat)))
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

// Used internally by the 'real' Debug(AsyncWrap*, ...) functions below, so that
// the FORCE_INLINE flag on them doesn't apply to the contents of this function
// as well.
// We apply COLD_NOINLINE to tell the compiler that it's not worth optimizing
// this function for speed and it should rather focus on keeping it out of
// hot code paths. In particular, we want to keep the string concatenating code
// out of the function containing the original `Debug()` call.
template <typename... Args>
void COLD_NOINLINE UnconditionalAsyncWrapDebug(AsyncWrap* async_wrap,
                                               const char* format,
                                               Args&&... args) {
  Debug(async_wrap->env(),
        static_cast<DebugCategory>(async_wrap->provider_type()),
        async_wrap->diagnostic_name() + " " + format + "\n",
        std::forward<Args>(args)...);
}

template <typename... Args>
inline void FORCE_INLINE Debug(AsyncWrap* async_wrap,
                               const char* format,
                               Args&&... args) {
#ifdef DEBUG
  CHECK_NOT_NULL(async_wrap);
#endif
  DebugCategory cat =
      static_cast<DebugCategory>(async_wrap->provider_type());
  if (!UNLIKELY(async_wrap->env()->debug_enabled(cat)))
    return;
  UnconditionalAsyncWrapDebug(async_wrap, format, std::forward<Args>(args)...);
}

template <typename... Args>
inline void FORCE_INLINE Debug(AsyncWrap* async_wrap,
                               const std::string& format,
                               Args&&... args) {
  Debug(async_wrap, format.c_str(), std::forward<Args>(args)...);
}


}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_DEBUG_UTILS_H_
