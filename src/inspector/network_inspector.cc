#include "network_inspector.h"

namespace node {
namespace inspector {

NetworkInspector::NetworkInspector(
    Environment* env,
    v8_inspector::V8Inspector* v8_inspector,
    std::shared_ptr<NetworkResourceManager> network_resource_manager)
    : enabled_(false),
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

// The network tracking diagnostics channels are subscribed at startup when
// --experimental-network-inspection is set (see
// lib/internal/process/pre_execution.js), so enabling and disabling only
// toggles the flag that gates event emission. This must not call into JS:
// Network.enable/Network.disable are dispatched via an interrupt that can
// land in the middle of arbitrary JavaScript execution.
// Refs: https://github.com/nodejs/node/issues/64308
void NetworkInspector::Enable() {
  enabled_ = true;
}

void NetworkInspector::Disable() {
  enabled_ = false;
}

}  // namespace inspector
}  // namespace node
