#include "network_inspector.h"

namespace node {
namespace inspector {

NetworkInspector::NetworkInspector(
    Environment* env,
    v8_inspector::V8Inspector* v8_inspector,
    std::shared_ptr<NetworkResourceManager> network_resource_manager)
    : enabled_(false),
      env_(env),
      network_resource_manager_(std::move(network_resource_manager)) {
  network_agent_ = std::make_unique<NetworkAgent>(
      this, v8_inspector, env, network_resource_manager_);
}
NetworkInspector::~NetworkInspector() {
  network_agent_.reset();
}

void NetworkInspector::Wire(protocol::UberDispatcher* dispatcher) {
  network_agent_->Wire(dispatcher);
}

bool NetworkInspector::canEmit(const std::string& domain) {
  return domain == "Network";
}

void NetworkInspector::emitNotification(v8::Local<v8::Context> context,
                                        const std::string& domain,
                                        const std::string& method,
                                        v8::Local<v8::Object> params) {
  if (domain == "Network") {
    network_agent_->emitNotification(context, method, params);
  } else {
    UNREACHABLE("Unknown domain");
  }
}

void NetworkInspector::Enable() {
  if (auto agent = env_->inspector_agent()) {
    agent->EnableNetworkTracking();
  }
  enabled_ = true;
}

void NetworkInspector::Disable() {
  if (auto agent = env_->inspector_agent()) {
    agent->DisableNetworkTracking();
  }
  enabled_ = false;
}

}  // namespace inspector
}  // namespace node
