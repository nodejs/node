#ifndef SRC_INSPECTOR_WORKER_AGENT_H_
#define SRC_INSPECTOR_WORKER_AGENT_H_

#include "node/inspector/protocol/NodeWorker.h"
#include "v8.h"


namespace node {
namespace inspector {
class WorkerManagerEventHandle;
class WorkerManager;

namespace protocol {
class NodeWorkers;

class WorkerAgent : public NodeWorker::Backend {
 public:
  explicit WorkerAgent(std::weak_ptr<WorkerManager> manager);
  ~WorkerAgent() override = default;

  void Wire(UberDispatcher* dispatcher);

  DispatchResponse sendMessageToWorker(const String& message,
                                       const String& sessionId) override;

  DispatchResponse enable(bool waitForDebuggerOnStart) override;
  DispatchResponse disable() override;

 private:
  std::shared_ptr<NodeWorker::Frontend> frontend_;
  std::weak_ptr<WorkerManager> manager_;
  std::unique_ptr<WorkerManagerEventHandle> event_handle_;
  std::shared_ptr<NodeWorkers> workers_;
};
}  // namespace protocol
}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_WORKER_AGENT_H_
