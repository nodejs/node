#include "tracing/agent.h"

#ifdef V8_USE_PERFETTO
#include "tracing/agent_perfetto.h"
#else
#include "tracing/agent_legacy.h"
#endif

#include "tracing/trace_event_helper.h"

namespace node {
namespace tracing {

namespace {
Agent* g_agent = nullptr;
}  // namespace

Agent* Agent::GetInstance() {
  return g_agent;
}

void Agent::Deleter::operator()(Agent* agent) {
  CHECK_EQ(agent, g_agent);
  g_agent = nullptr;
  TraceEventHelper::SetTracingController(nullptr);
  delete agent;
}

std::unique_ptr<Agent, Agent::Deleter> Agent::CreateDefault() {
  CHECK_NULL(g_agent);

#ifdef V8_USE_PERFETTO
  auto agent = new PerfettoTracingAgent();
#else
  auto agent = new LegacyTracingAgent();
#endif

  g_agent = agent;
  TraceEventHelper::SetTracingController(agent->GetTracingController());
  return std::unique_ptr<Agent, Agent::Deleter>(agent);
}

}  // namespace tracing
}  // namespace node
