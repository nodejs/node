#include "tracing/trace_event_helper.h"
#include "node.h"

namespace node {
namespace tracing {

v8::TracingController* g_controller = nullptr;

v8::TracingController* TraceEventHelper::GetTracingController() {
  return g_controller;
}

void TraceEventHelper::SetTracingController(v8::TracingController* controller) {
  g_controller = controller;
}

}  // namespace tracing

v8::TracingController* GetTracingController() {
  return tracing::TraceEventHelper::GetTracingController();
}

void SetTracingController(v8::TracingController* controller) {
  tracing::TraceEventHelper::SetTracingController(controller);
}

}  // namespace node
