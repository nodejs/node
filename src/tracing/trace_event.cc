#include "tracing/trace_event.h"

namespace node {
namespace tracing {

v8::TracingController* g_controller = nullptr;

void TraceEventHelper::SetTracingController(v8::TracingController* controller) {
  g_controller = controller;
}

v8::TracingController* TraceEventHelper::GetTracingController() {
  return g_controller;
}

}  // namespace tracing
}  // namespace node
