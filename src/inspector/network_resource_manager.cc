#include "inspector/network_resource_manager.h"
#include <atomic>
#include <iostream>
#include <string>
#include <unordered_map>

namespace node {
namespace inspector {

std::unordered_map<std::string, std::string> NetworkResourceManager::resources_;
std::unordered_map<std::string, uint64_t>
    NetworkResourceManager::url_to_stream_id_;
std::atomic<uint64_t> NetworkResourceManager::stream_id_counter_{1};

void NetworkResourceManager::Put(const std::string& url,
                                 const std::string& data) {
  resources_[url] = data;
  url_to_stream_id_[url] = ++stream_id_counter_;
}

std::string NetworkResourceManager::Get(const std::string& url) {
  auto it = resources_.find(url);
  if (it != resources_.end()) return it->second;
  return {};
}

uint64_t NetworkResourceManager::NextStreamId() {
  return ++stream_id_counter_;
}

std::string NetworkResourceManager::GetUrlForStreamId(uint64_t stream_id) {
  for (const auto& pair : url_to_stream_id_) {
    if (pair.second == stream_id) {
      return pair.first;
    }
  }
  return std::string();
}

void NetworkResourceManager::EraseByStreamId(uint64_t stream_id) {
  for (auto it = url_to_stream_id_.begin(); it != url_to_stream_id_.end();
       ++it) {
    if (it->second == stream_id) {
      resources_.erase(it->first);
      url_to_stream_id_.erase(it);
      break;
    }
  }
}

uint64_t NetworkResourceManager::GetStreamId(const std::string& url) {
  auto it = url_to_stream_id_.find(url);
  if (it != url_to_stream_id_.end()) return it->second;
  return 0;
}

}  // namespace inspector
}  // namespace node
