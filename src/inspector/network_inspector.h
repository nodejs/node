#ifndef SRC_INSPECTOR_NETWORK_INSPECTOR_H_
#define SRC_INSPECTOR_NETWORK_INSPECTOR_H_

#include "env.h"
#include "network_agent.h"
#include "node_network_agent.h"
#include "v8.h"

#include <unordered_map>

namespace node {
class Environment;

namespace inspector {

class NetworkInspector {
 public:
  explicit NetworkInspector(Environment* env);
  ~NetworkInspector();

  void Wire(protocol::UberDispatcher* dispatcher);

  bool canEmit(const std::string& domain);

  void emitNotification(const std::string& domain,
                        const std::string& method,
                        std::unique_ptr<protocol::DictionaryValue> params);

  void Enable();
  void Disable();
  bool IsEnabled() const { return enabled_; }

 private:
  bool enabled_;
  Environment* env_;
  std::unique_ptr<protocol::NetworkAgent> network_agent_;
  std::unique_ptr<protocol::NodeNetworkAgent> node_network_agent_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NETWORK_INSPECTOR_H_
