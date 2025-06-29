#include "inspector/network_resource_manager.h"
#include <atomic>
#include <iostream>
#include <string>
#include <unordered_map>

namespace node {
namespace inspector {

void NetworkResourceManager::Put(const std::string& url,
                                 const std::string& data) {
  Mutex::ScopedLock lock(mutex_);
  resources_[url] = data;
  url_to_stream_id_[url] = ++stream_id_counter_;
}

std::string NetworkResourceManager::Get(const std::string& url) {
  Mutex::ScopedLock lock(mutex_);
  auto it = resources_.find(url);
  if (it != resources_.end()) return it->second;
  return {};
}

uint64_t NetworkResourceManager::NextStreamId() {
  return ++stream_id_counter_;
}

std::string NetworkResourceManager::GetUrlForStreamId(uint64_t stream_id) {
  Mutex::ScopedLock lock(mutex_);
  for (const auto& pair : url_to_stream_id_) {
    if (pair.second == stream_id) {
      return pair.first;
    }
  }
  return std::string();
}

void NetworkResourceManager::EraseByStreamId(uint64_t stream_id) {
  Mutex::ScopedLock lock(mutex_);
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
  Mutex::ScopedLock lock(mutex_);
  auto it = url_to_stream_id_.find(url);
  if (it != url_to_stream_id_.end()) return it->second;
  return 0;
}

}  // namespace inspector
}  // namespace node
