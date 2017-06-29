#include "tracing/trace_event.h"

namespace node {
namespace tracing {

v8::TracingController* controller_ = nullptr;

void TraceEventHelper::SetTracingController(v8::TracingController* controller) {
  controller_ = controller;
}

v8::TracingController* TraceEventHelper::GetTracingController() {
  return controller_;
}

}  // namespace tracing
}  // namespace node
