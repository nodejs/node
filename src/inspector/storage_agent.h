#ifndef SRC_INSPECTOR_STORAGE_AGENT_H_
#define SRC_INSPECTOR_STORAGE_AGENT_H_

#include <string>
#include "env.h"
#include "inspector/notification_emitter.h"
#include "node/inspector/protocol/Storage.h"

namespace node {
namespace inspector {
namespace protocol {

class StorageAgent : public protocol::Storage::Backend {
 public:
  explicit StorageAgent(Environment* env);
  ~StorageAgent() override;

  void Wire(protocol::UberDispatcher* dispatcher);

  DispatchResponse getStorageKey(std::optional<protocol::String> frameId,
                                 protocol::String* storageKey) override;

  StorageAgent(const StorageAgent&) = delete;
  StorageAgent& operator=(const StorageAgent&) = delete;

 private:
  std::unique_ptr<protocol::Storage::Frontend> frontend_;
  Environment* env_;
  std::string to_file_url(const std::filesystem::path& input);
};
}  // namespace protocol
}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_STORAGE_AGENT_H_
