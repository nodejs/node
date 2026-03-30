#ifndef SRC_INSPECTOR_DOM_STORAGE_AGENT_H_
#define SRC_INSPECTOR_DOM_STORAGE_AGENT_H_

#include <string>
#include "env.h"
#include "node/inspector/protocol/DOMStorage.h"
#include "notification_emitter.h"
#include "v8.h"

namespace node {
namespace inspector {

class DOMStorageAgent : public protocol::DOMStorage::Backend,
                        public NotificationEmitter {
 public:
  explicit DOMStorageAgent(Environment* env);
  ~DOMStorageAgent() override;

  void Wire(protocol::UberDispatcher* dispatcher);

  protocol::DispatchResponse enable() override;
  protocol::DispatchResponse disable() override;
  protocol::DispatchResponse getDOMStorageItems(
      std::unique_ptr<protocol::DOMStorage::StorageId> storageId,
      std::unique_ptr<protocol::Array<protocol::Array<protocol::String>>>*
          items) override;
  protocol::DispatchResponse setDOMStorageItem(
      std::unique_ptr<protocol::DOMStorage::StorageId> storageId,
      const std::string& key,
      const std::string& value) override;
  protocol::DispatchResponse removeDOMStorageItem(
      std::unique_ptr<protocol::DOMStorage::StorageId> storageId,
      const std::string& key) override;
  protocol::DispatchResponse clear(
      std::unique_ptr<protocol::DOMStorage::StorageId> storageId) override;

  void domStorageItemAdded(v8::Local<v8::Context> context,
                           v8::Local<v8::Object> params);
  void domStorageItemRemoved(v8::Local<v8::Context> context,
                             v8::Local<v8::Object> params);
  void domStorageItemUpdated(v8::Local<v8::Context> context,
                             v8::Local<v8::Object> params);
  void domStorageItemsCleared(v8::Local<v8::Context> context,
                              v8::Local<v8::Object> params);
  void registerStorage(v8::Local<v8::Context> context,
                       v8::Local<v8::Object> params);
  bool canEmit(const std::string& domain) override;

  DOMStorageAgent(const DOMStorageAgent&) = delete;
  DOMStorageAgent& operator=(const DOMStorageAgent&) = delete;

 private:
  std::unique_ptr<protocol::DOMStorage::Frontend> frontend_;
  std::unordered_map<std::string, std::string> local_storage_map_ = {};
  std::unordered_map<std::string, std::string> session_storage_map_ = {};
  bool enabled_ = false;
  Environment* env_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_DOM_STORAGE_AGENT_H_
