#ifndef SRC_INSPECTOR_NODE_NETWORK_AGENT_H_
#define SRC_INSPECTOR_NODE_NETWORK_AGENT_H_

#include "env.h"
#include "node/inspector/protocol/NodeNetwork.h"
#include "v8.h"

#include <unordered_map>

namespace node {
class Environment;

namespace inspector {
namespace protocol {

class NodeNetworkAgent : public NodeNetwork::Backend {
 public:
  explicit NodeNetworkAgent(Environment* env);

  void emitNotification(const String& event,
                        std::unique_ptr<protocol::DictionaryValue> params);

  void Wire(UberDispatcher* dispatcher);

  void requestWillBeSent(std::unique_ptr<protocol::DictionaryValue> params);

  void responseReceived(std::unique_ptr<protocol::DictionaryValue> params);

  void loadingFinished(std::unique_ptr<protocol::DictionaryValue> params);

 private:
  Environment* env_;
  std::shared_ptr<NodeNetwork::Frontend> frontend_;
  using EventNotifier =
      void (NodeNetworkAgent::*)(std::unique_ptr<protocol::DictionaryValue>);
  std::unordered_map<String, EventNotifier> event_notifier_map_;
};

}  // namespace protocol
}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NODE_NETWORK_AGENT_H_
