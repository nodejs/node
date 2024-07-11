#include "node_network_agent.h"
#include "network_agent.h"
#include "network_inspector.h"

namespace node {
namespace inspector {
namespace protocol {

NodeNetworkAgent::NodeNetworkAgent(NetworkInspector* inspector)
    : inspector_(inspector) {
  event_notifier_map_["requestWillBeSent"] =
      &NodeNetworkAgent::requestWillBeSent;
  event_notifier_map_["responseReceived"] = &NodeNetworkAgent::responseReceived;
  event_notifier_map_["loadingFinished"] = &NodeNetworkAgent::loadingFinished;
}

void NodeNetworkAgent::emitNotification(
    const String& event, std::unique_ptr<protocol::DictionaryValue> params) {
  if (!inspector_->IsEnabled()) return;
  auto it = event_notifier_map_.find(event);
  if (it != event_notifier_map_.end()) {
    (this->*(it->second))(std::move(params));
  }
}

void NodeNetworkAgent::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<NodeNetwork::Frontend>(dispatcher->channel());
  NodeNetwork::Dispatcher::wire(dispatcher, this);
}

DispatchResponse NodeNetworkAgent::enable() {
  inspector_->Enable();
  return DispatchResponse::OK();
}

DispatchResponse NodeNetworkAgent::disable() {
  inspector_->Disable();
  return DispatchResponse::OK();
}

void NodeNetworkAgent::requestWillBeSent(
    std::unique_ptr<protocol::DictionaryValue> params) {
  String request_id;
  params->getString("requestId", &request_id);
  double timestamp;
  params->getDouble("timestamp", &timestamp);
  double wall_time;
  params->getDouble("wallTime", &wall_time);
  auto request = params->getObject("request");
  String url;
  request->getString("url", &url);
  String method;
  request->getString("method", &method);

  frontend_->requestWillBeSent(
      request_id, Request(url, method), timestamp, wall_time);

  inspector_->emitNotification(
      "Network", "requestWillBeSent", std::move(params));
}

void NodeNetworkAgent::responseReceived(
    std::unique_ptr<protocol::DictionaryValue> params) {
  String request_id;
  params->getString("requestId", &request_id);
  double timestamp;
  params->getDouble("timestamp", &timestamp);

  frontend_->responseReceived(request_id, timestamp);

  inspector_->emitNotification(
      "Network", "responseReceived", std::move(params));
}

void NodeNetworkAgent::loadingFinished(
    std::unique_ptr<protocol::DictionaryValue> params) {
  String request_id;
  params->getString("requestId", &request_id);
  double timestamp;
  params->getDouble("timestamp", &timestamp);

  frontend_->loadingFinished(request_id, timestamp);

  inspector_->emitNotification("Network", "loadingFinished", std::move(params));
}

}  // namespace protocol
}  // namespace inspector
}  // namespace node
