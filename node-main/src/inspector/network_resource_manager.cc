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
}

std::string NetworkResourceManager::Get(const std::string& url) {
  Mutex::ScopedLock lock(mutex_);
  auto it = resources_.find(url);
  if (it != resources_.end()) return it->second;
  return {};
}

void NetworkResourceManager::Erase(const std::string& stream_id) {
  Mutex::ScopedLock lock(mutex_);
  resources_.erase(stream_id);
}

}  // namespace inspector
}  // namespace node
