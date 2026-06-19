#ifndef SRC_TRACING_TRACE_EVENT_HELPER_H_
#define SRC_TRACING_TRACE_EVENT_HELPER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8-platform.h"

namespace node::tracing {

class TraceEventHelper {
 public:
  static v8::TracingController* GetTracingController();
  static void SetTracingController(v8::TracingController* controller);

  static inline const uint8_t* GetCategoryGroupEnabled(const char* group) {
#if !defined(V8_USE_PERFETTO)
    v8::TracingController* controller = GetTracingController();
    static const uint8_t disabled = 0;
    if (controller == nullptr) [[unlikely]] {
      return &disabled;
    }
    return controller->GetCategoryGroupEnabled(group);
#else
    return nullptr;
#endif
  }
};

}  // namespace node::tracing

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_TRACING_TRACE_EVENT_HELPER_H_
