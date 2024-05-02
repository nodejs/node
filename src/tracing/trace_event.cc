#include "tracing/trace_event.h"
#include "node.h"

namespace node {
namespace tracing {

Agent* g_agent = nullptr;
v8::TracingController* g_controller = nullptr;

void TraceEventHelper::SetAgent(Agent* agent) {
  if (agent) {
    g_agent = agent;
    g_controller = agent->GetTracingController();
  } else {
    g_agent = nullptr;
    g_controller = nullptr;
  }
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

v8::TracingController* GetTracingController() {
  return tracing::TraceEventHelper::GetTracingController();
}

void SetTracingController(v8::TracingController* controller) {
  tracing::TraceEventHelper::SetTracingController(controller);
}

}  // namespace node
