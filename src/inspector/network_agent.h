#ifndef SRC_INSPECTOR_NETWORK_AGENT_H_
#define SRC_INSPECTOR_NETWORK_AGENT_H_

#include "node/inspector/protocol/Network.h"
#include "v8.h"

#include <unordered_map>

namespace node {

namespace inspector {
namespace protocol {

std::unique_ptr<Network::Request> Request(const String& url,
                                          const String& method);

class NetworkAgent : public Network::Backend {
 public:
  NetworkAgent();

  void Wire(UberDispatcher* dispatcher);

  void emitNotification(const String& event,
                        std::unique_ptr<protocol::DictionaryValue> params);

  DispatchResponse getResponseBody(const String& in_requestId,
                                   String* out_body) override;

  void requestWillBeSent(std::unique_ptr<protocol::DictionaryValue> params);

  void responseReceived(std::unique_ptr<protocol::DictionaryValue> params);

  void dataReceived(std::unique_ptr<protocol::DictionaryValue> params);

  void loadingFinished(std::unique_ptr<protocol::DictionaryValue> params);

 private:
  std::shared_ptr<Network::Frontend> frontend_;
  std::unordered_map<String, String> request_id_to_response_;
  using EventNotifier =
      void (NetworkAgent::*)(std::unique_ptr<protocol::DictionaryValue>);
  std::unordered_map<String, EventNotifier> event_notifier_map_;
};

}  // namespace protocol
}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NETWORK_AGENT_H_
