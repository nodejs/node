#include "network_agent.h"

namespace node {
namespace inspector {
namespace protocol {

std::unique_ptr<Network::Request> Request(const String& url,
                                          const String& method) {
  return Network::Request::create().setUrl(url).setMethod(method).build();
}

NetworkAgent::NetworkAgent() {
  event_notifier_map_["requestWillBeSent"] = &NetworkAgent::requestWillBeSent;
  event_notifier_map_["responseReceived"] = &NetworkAgent::responseReceived;
  event_notifier_map_["dataReceived"] = &NetworkAgent::dataReceived;
  event_notifier_map_["loadingFinished"] = &NetworkAgent::loadingFinished;
}

void NetworkAgent::emitNotification(
    const String& event, std::unique_ptr<protocol::DictionaryValue> params) {
  auto it = event_notifier_map_.find(event);
  if (it != event_notifier_map_.end()) {
    (this->*(it->second))(std::move(params));
  }
}

void NetworkAgent::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Network::Frontend>(dispatcher->channel());
  Network::Dispatcher::wire(dispatcher, this);
}

DispatchResponse NetworkAgent::getResponseBody(const String& in_requestId,
                                               String* out_body) {
  auto it = request_id_to_response_.find(in_requestId);
  if (it != request_id_to_response_.end()) {
    *out_body = it->second;
    request_id_to_response_.erase(it);
  }
  return DispatchResponse::OK();
}

void NetworkAgent::requestWillBeSent(
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
}

void NetworkAgent::responseReceived(
    std::unique_ptr<protocol::DictionaryValue> params) {
  String request_id;
  params->getString("requestId", &request_id);
  double timestamp;
  params->getDouble("timestamp", &timestamp);

  frontend_->responseReceived(request_id, timestamp);
}

void NetworkAgent::dataReceived(
    std::unique_ptr<protocol::DictionaryValue> params) {
  String request_id;
  params->getString("requestId", &request_id);
  double timestamp;
  params->getDouble("timestamp", &timestamp);
  int data_length;
  params->getInteger("dataLength", &data_length);

  frontend_->dataReceived(request_id, timestamp, data_length);
}

void NetworkAgent::loadingFinished(
    std::unique_ptr<protocol::DictionaryValue> params) {
  String request_id;
  params->getString("requestId", &request_id);
  double timestamp;
  params->getDouble("timestamp", &timestamp);
  int encoded_data_length;
  params->getInteger("encodedDataLength", &encoded_data_length);
  String response;
  params->getString("response", &response);

  request_id_to_response_[request_id] = response;
  frontend_->loadingFinished(request_id, timestamp, encoded_data_length);
}

}  // namespace protocol
}  // namespace inspector
}  // namespace node
