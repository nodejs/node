#ifndef SRC_INSPECTOR_TRACING_AGENT_H_
#define SRC_INSPECTOR_TRACING_AGENT_H_

#include "node/inspector/protocol/NodeTracing.h"
#include "tracing/agent.h"
#include "v8.h"


namespace node {
class Environment;

namespace inspector {
namespace protocol {

class TracingAgent : public NodeTracing::Backend {
 public:
  explicit TracingAgent(Environment*);
  ~TracingAgent() override;

  void Wire(UberDispatcher* dispatcher);

  DispatchResponse start(
      std::unique_ptr<protocol::NodeTracing::TraceConfig> traceConfig) override;
  DispatchResponse stop() override;
  DispatchResponse getCategories(
      std::unique_ptr<protocol::Array<String>>* categories) override;

 private:
  void DisconnectTraceClient();

  Environment* env_;
  tracing::AgentWriterHandle trace_writer_;
  std::unique_ptr<NodeTracing::Frontend> frontend_;
};


}  // namespace protocol
}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_TRACING_AGENT_H_
