#ifndef SRC_INSPECTOR_TRACING_AGENT_H_
#define SRC_INSPECTOR_TRACING_AGENT_H_

#include "node/inspector/protocol/NodeTracing.h"
#include "tracing/agent.h"
#include "v8.h"


namespace node {
class Environment;

namespace inspector {
class MainThreadHandle;

namespace protocol {

class TracingAgent : public NodeTracing::Backend {
 public:
  explicit TracingAgent(Environment*, std::shared_ptr<MainThreadHandle>);
  ~TracingAgent() override;

  void Wire(UberDispatcher* dispatcher);

  DispatchResponse start(
      std::unique_ptr<protocol::NodeTracing::TraceConfig> traceConfig) override;
  DispatchResponse stop() override;
  DispatchResponse getCategories(
      std::unique_ptr<protocol::Array<String>>* categories) override;

 private:
  Environment* env_;
  std::shared_ptr<MainThreadHandle> main_thread_;
  tracing::AgentWriterHandle trace_writer_;
  int frontend_object_id_;
  std::shared_ptr<NodeTracing::Frontend> frontend_;
};


}  // namespace protocol
}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_TRACING_AGENT_H_
