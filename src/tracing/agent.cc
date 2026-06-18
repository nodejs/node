#include "tracing/agent.h"
#include "tracing/agent_legacy.h"
#include "tracing/trace_event.h"

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

  auto agent = new LegacyTracingAgent();

  g_agent = agent;
  TraceEventHelper::SetTracingController(agent->GetTracingController());
  return std::unique_ptr<Agent, Agent::Deleter>(agent);
}

}  // namespace tracing
}  // namespace node
