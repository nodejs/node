#include "inspector/storage_agent.h"
#include <string>
#include "env-inl.h"
#include "inspector/protocol_helper.h"
#include "node_url.h"
#include "util-inl.h"
#include "v8-isolate.h"
#include "v8-local-handle.h"

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
  *storageKey = node::url::FromFilePath(local_storage_file);
  return protocol::DispatchResponse::Success();
}

}  // namespace protocol
}  // namespace inspector
}  // namespace node
