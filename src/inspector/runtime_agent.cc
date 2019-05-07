#include "runtime_agent.h"

#include "env-inl.h"
#include "inspector_agent.h"

namespace node {
namespace inspector {
namespace protocol {

RuntimeAgent::RuntimeAgent(Environment* env)
  : notify_when_waiting_for_disconnect_(false), env_(env) {}

void RuntimeAgent::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<NodeRuntime::Frontend>(dispatcher->channel());
  NodeRuntime::Dispatcher::wire(dispatcher, this);
}

DispatchResponse RuntimeAgent::notifyWhenWaitingForDisconnect(bool enabled) {
  if (!env_->owns_process_state()) {
    return DispatchResponse::Error(
        "NodeRuntime domain can only be used through main thread sessions");
  }
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
