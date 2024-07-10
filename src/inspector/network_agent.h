#ifndef SRC_INSPECTOR_NETWORK_AGENT_H_
#define SRC_INSPECTOR_NETWORK_AGENT_H_

#include "env.h"
#include "node/inspector/protocol/Network.h"
#include "v8.h"

#include <unordered_map>

namespace node {
class Environment;

namespace inspector {
namespace protocol {

std::unique_ptr<Network::Request> Request(const String& url,
                                          const String& method);

class NetworkAgent : public Network::Backend {
 public:
  explicit NetworkAgent(Environment* env);

  void Wire(UberDispatcher* dispatcher);

  DispatchResponse enable() override;

  DispatchResponse disable() override;

  void emitNotification(const String& event,
                        std::unique_ptr<protocol::DictionaryValue> params);

  void requestWillBeSent(std::unique_ptr<protocol::DictionaryValue> params);

  void responseReceived(std::unique_ptr<protocol::DictionaryValue> params);

  void loadingFinished(std::unique_ptr<protocol::DictionaryValue> params);

 private:
  bool enabled_;
  Environment* env_;
  std::shared_ptr<Network::Frontend> frontend_;
  using EventNotifier =
      void (NetworkAgent::*)(std::unique_ptr<protocol::DictionaryValue>);
  std::unordered_map<String, EventNotifier> event_notifier_map_;
};

}  // namespace protocol
}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NETWORK_AGENT_H_
