#include "tracing/trace_event.h"

namespace node {
namespace tracing {

Agent* g_agent = nullptr;
v8::TracingController* g_controller = nullptr;

void TraceEventHelper::SetAgent(Agent* agent) {
  g_agent = agent;
  g_controller = agent->GetTracingController();
}

Agent* TraceEventHelper::GetAgent() {
  return g_agent;
}

v8::TracingController* TraceEventHelper::GetTracingController() {
  return g_controller;
}

void TraceEventHelper::SetTracingController(v8::TracingController* controller) {
  g_controller = controller;
}

}  // namespace tracing
}  // namespace node
