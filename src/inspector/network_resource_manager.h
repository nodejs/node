// network_resource_manager.h
#ifndef SRC_INSPECTOR_NETWORK_RESOURCE_MANAGER_H_
#define SRC_INSPECTOR_NETWORK_RESOURCE_MANAGER_H_

#include <atomic>
#include <string>
#include <unordered_map>
#include "node_mutex.h"

namespace node {
namespace inspector {

class NetworkResourceManager {
 public:
  void Put(const std::string& url, const std::string& data);
  std::string Get(const std::string& url);

  // Erase resource and mapping by stream id
  void Erase(const std::string& stream_id);

 private:
  std::unordered_map<std::string, std::string> resources_;
  Mutex mutex_;  // Protects access to resources_
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NETWORK_RESOURCE_MANAGER_H_
