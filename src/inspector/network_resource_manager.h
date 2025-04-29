// network_resource_manager.h
#ifndef SRC_INSPECTOR_NETWORK_RESOURCE_MANAGER_H_
#define SRC_INSPECTOR_NETWORK_RESOURCE_MANAGER_H_

#include <atomic>
#include <string>
#include <unordered_map>

namespace node {
namespace inspector {

class NetworkResourceManager {
 public:
  static void Put(const std::string& url, const std::string& data);
  static std::string Get(const std::string& url);

  // Accessor to get URL for a given stream id
  static std::string GetUrlForStreamId(uint64_t stream_id);
  // Erase resource and mapping by stream id
  static void EraseByStreamId(uint64_t stream_id);
  // Returns the stream id for a given url, or 0 if not found
  static uint64_t GetStreamId(const std::string& url);

 private:
  static uint64_t NextStreamId();
  static std::unordered_map<std::string, std::string> resources_;
  static std::unordered_map<std::string, uint64_t> url_to_stream_id_;
  static std::atomic<uint64_t> stream_id_counter_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_NETWORK_RESOURCE_MANAGER_H_
