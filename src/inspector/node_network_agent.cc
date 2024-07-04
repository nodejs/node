#include "node_network_agent.h"
#include "network_agent.h"

namespace node {
namespace inspector {
namespace protocol {

NodeNetworkAgent::NodeNetworkAgent(Environment* env) : env_(env) {
  event_notifier_map_["requestWillBeSent"] =
      &NodeNetworkAgent::requestWillBeSent;
  event_notifier_map_["responseReceived"] = &NodeNetworkAgent::responseReceived;
  event_notifier_map_["loadingFinished"] = &NodeNetworkAgent::loadingFinished;
}

void NodeNetworkAgent::emitNotification(
    const String& event, std::unique_ptr<protocol::DictionaryValue> params) {
  auto it = event_notifier_map_.find(event);
  if (it != event_notifier_map_.end()) {
    (this->*(it->second))(std::move(params));
  }
}

void NodeNetworkAgent::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<NodeNetwork::Frontend>(dispatcher->channel());
  NodeNetwork::Dispatcher::wire(dispatcher, this);
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

  env_->inspector_agent()->EmitProtocolEvent(
      protocol::StringUtil::ToStringView("Network.requestWillBeSent"),
      protocol::StringUtil::ToStringView(params->toJSONString()));
}

void NodeNetworkAgent::responseReceived(
    std::unique_ptr<protocol::DictionaryValue> params) {
  String request_id;
  params->getString("requestId", &request_id);
  double timestamp;
  params->getDouble("timestamp", &timestamp);

  frontend_->responseReceived(request_id, timestamp);

  env_->inspector_agent()->EmitProtocolEvent(
      protocol::StringUtil::ToStringView("Network.responseReceived"),
      protocol::StringUtil::ToStringView(params->toJSONString()));
}

void NodeNetworkAgent::loadingFinished(
    std::unique_ptr<protocol::DictionaryValue> params) {
  String request_id;
  params->getString("requestId", &request_id);
  double timestamp;
  params->getDouble("timestamp", &timestamp);

  frontend_->loadingFinished(request_id, timestamp);

  env_->inspector_agent()->EmitProtocolEvent(
      protocol::StringUtil::ToStringView("Network.loadingFinished"),
      protocol::StringUtil::ToStringView(params->toJSONString()));
}

}  // namespace protocol
}  // namespace inspector
}  // namespace node
