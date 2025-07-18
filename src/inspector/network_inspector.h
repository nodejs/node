#ifndef SRC_INSPECTOR_NETWORK_INSPECTOR_H_
#define SRC_INSPECTOR_NETWORK_INSPECTOR_H_

#include <memory>
#include "env.h"
#include "network_agent.h"
#include "network_resource_manager.h"

namespace node {
class Environment;

namespace inspector {

class NetworkInspector {
 public:
  explicit NetworkInspector(
      Environment* env,
      v8_inspector::V8Inspector* v8_inspector,
      std::shared_ptr<NetworkResourceManager> network_resource_manager);
  ~NetworkInspector();

  void Wire(protocol::UberDispatcher* dispatcher);

  bool canEmit(const std::string& domain);

  void emitNotification(v8::Local<v8::Context> context,
                        const std::string& domain,
                        const std::string& method,
                        v8::Local<v8::Object> params);

  void Enable();
  void Disable();
  bool IsEnabled() const { return enabled_; }

 private:
  bool enabled_;
  Environment* env_;
  std::unique_ptr<NetworkAgent> network_agent_;
  std::shared_ptr<NetworkResourceManager> network_resource_manager_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NETWORK_INSPECTOR_H_
