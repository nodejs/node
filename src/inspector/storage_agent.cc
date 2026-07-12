#include "inspector/storage_agent.h"
#include <string>
#include "env-inl.h"
#include "node_url.h"

namespace node {
namespace inspector {
namespace protocol {
StorageAgent::StorageAgent(Environment* env) : env_(env) {}
StorageAgent::~StorageAgent() {}

void StorageAgent::Wire(protocol::UberDispatcher* dispatcher) {
  frontend_ =
      std::make_unique<protocol::Storage::Frontend>(dispatcher->channel());
  protocol::Storage::Dispatcher::wire(dispatcher, this);
}
DispatchResponse StorageAgent::getStorageKey(
    std::optional<protocol::String> frameId, protocol::String* storageKey) {
  auto local_storage_file = env_->options()->localstorage_file;
  *storageKey = node::url::FromFilePath(to_absolute_path(local_storage_file));
  return protocol::DispatchResponse::Success();
}

std::string StorageAgent::to_absolute_path(const std::filesystem::path& input) {
  std::filesystem::path abs =
      std::filesystem::weakly_canonical(std::filesystem::absolute(input));
  return abs.generic_string();
}

}  // namespace protocol
}  // namespace inspector
}  // namespace node
