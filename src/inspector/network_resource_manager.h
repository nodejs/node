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

  // Accessor to get URL for a given stream id
  std::string GetUrlForStreamId(uint64_t stream_id);
  // Erase resource and mapping by stream id
  void EraseByStreamId(uint64_t stream_id);
  // Returns the stream id for a given url, or 0 if not found
  uint64_t GetStreamId(const std::string& url);

 private:
  uint64_t NextStreamId();
  std::unordered_map<std::string, std::string> resources_;
  std::unordered_map<std::string, uint64_t> url_to_stream_id_;
  std::atomic<uint64_t> stream_id_counter_{1};
  Mutex mutex_;  // Protects access to resources_ and url_to_stream_id_
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NETWORK_RESOURCE_MANAGER_H_
