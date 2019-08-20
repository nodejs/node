#include "runtime_agent.h"

#include "env-inl.h"
#include "inspector_agent.h"

namespace node {
namespace inspector {
namespace protocol {

RuntimeAgent::RuntimeAgent()
  : notify_when_waiting_for_disconnect_(false) {}

void RuntimeAgent::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<NodeRuntime::Frontend>(dispatcher->channel());
  NodeRuntime::Dispatcher::wire(dispatcher, this);
}

DispatchResponse RuntimeAgent::notifyWhenWaitingForDisconnect(bool enabled) {
  notify_when_waiting_for_disconnect_ = enabled;
  return DispatchResponse::OK();
}

bool RuntimeAgent::notifyWaitingForDisconnect() {
  if (notify_when_waiting_for_disconnect_) {
    frontend_->waitingForDisconnect();
    return true;
  }
  return false;
}
}  // namespace protocol
}  // namespace inspector
}  // namespace node
