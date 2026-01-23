#include "dom_storage_agent.h"
#include "env-inl.h"
#include "inspector/inspector_object_utils.h"
#include "v8-isolate.h"

namespace node {
namespace inspector {

using v8::Array;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

std::unique_ptr<protocol::DOMStorage::StorageId> createStorageIdFromObject(
    Local<Context> context, Local<Object> storage_id_obj) {
  protocol::String security_origin;
  if (!ObjectGetProtocolString(context, storage_id_obj, "securityOrigin")
           .To(&security_origin)) {
    return {};
  }
  bool is_local_storage =
      ObjectGetBool(context, storage_id_obj, "isLocalStorage").FromMaybe(false);
  protocol::String storageKey;
  if (!ObjectGetProtocolString(context, storage_id_obj, "storageKey")
           .To(&storageKey)) {
    return {};
  }

  return protocol::DOMStorage::StorageId::create()
      .setSecurityOrigin(security_origin)
      .setIsLocalStorage(is_local_storage)
      .setStorageKey(storageKey)
      .build();
}

DOMStorageAgent::DOMStorageAgent(Environment* env) : env_(env) {}

DOMStorageAgent::~DOMStorageAgent() {}

void DOMStorageAgent::Wire(protocol::UberDispatcher* dispatcher) {
  frontend_ =
      std::make_unique<protocol::DOMStorage::Frontend>(dispatcher->channel());
  protocol::DOMStorage::Dispatcher::wire(dispatcher, this);
  addEventNotifier("domStorageItemAdded",
                   [this](v8::Local<v8::Context> ctx, v8::Local<v8::Object> p) {
                     this->domStorageItemAdded(ctx, p);
                   });
  addEventNotifier("domStorageItemRemoved",
                   [this](v8::Local<v8::Context> ctx, v8::Local<v8::Object> p) {
                     this->domStorageItemRemoved(ctx, p);
                   });
  addEventNotifier("domStorageItemUpdated",
                   [this](v8::Local<v8::Context> ctx, v8::Local<v8::Object> p) {
                     this->domStorageItemUpdated(ctx, p);
                   });
  addEventNotifier("domStorageItemsCleared",
                   [this](v8::Local<v8::Context> ctx, v8::Local<v8::Object> p) {
                     this->domStorageItemsCleared(ctx, p);
                   });
  addEventNotifier("registerStorage",
                   [this](v8::Local<v8::Context> ctx, v8::Local<v8::Object> p) {
                     this->registerStorage(ctx, p);
                   });
}

protocol::DispatchResponse DOMStorageAgent::enable() {
  this->enabled_ = true;
  return protocol::DispatchResponse::Success();
}

protocol::DispatchResponse DOMStorageAgent::disable() {
  this->enabled_ = false;
  return protocol::DispatchResponse::Success();
}

protocol::DispatchResponse DOMStorageAgent::getDOMStorageItems(
    std::unique_ptr<protocol::DOMStorage::StorageId> storageId,
    std::unique_ptr<protocol::Array<protocol::Array<protocol::String>>>*
        items) {
  if (!enabled_) {
    return protocol::DispatchResponse::ServerError(
        "DOMStorage domain is not enabled");
  }
  bool is_local_storage = storageId->getIsLocalStorage();
  const std::unordered_map<std::string, std::string>& storage_map =
      is_local_storage ? local_storage_map_ : session_storage_map_;
  auto result =
      std::make_unique<protocol::Array<protocol::Array<protocol::String>>>();
  for (const auto& pair : storage_map) {
    auto item = std::make_unique<protocol::Array<protocol::String>>();
    item->push_back(pair.first);
    item->push_back(pair.second);
    result->push_back(std::move(item));
  }
  *items = std::move(result);
  return protocol::DispatchResponse::Success();
}

protocol::DispatchResponse DOMStorageAgent::setDOMStorageItem(
    std::unique_ptr<protocol::DOMStorage::StorageId> storageId,
    const std::string& key,
    const std::string& value) {
  return protocol::DispatchResponse::ServerError("Not implemented");
}

protocol::DispatchResponse DOMStorageAgent::removeDOMStorageItem(
    std::unique_ptr<protocol::DOMStorage::StorageId> storageId,
    const std::string& key) {
  return protocol::DispatchResponse::ServerError("Not implemented");
}

protocol::DispatchResponse DOMStorageAgent::clear(
    std::unique_ptr<protocol::DOMStorage::StorageId> storageId) {
  return protocol::DispatchResponse::ServerError("Not implemented");
}

void DOMStorageAgent::domStorageItemAdded(Local<Context> context,
                                          Local<Object> params) {
  Local<Object> storage_id_obj;
  if (!ObjectGetObject(context, params, "storageId").ToLocal(&storage_id_obj)) {
    return;
  }

  std::unique_ptr<protocol::DOMStorage::StorageId> storage_id =
      createStorageIdFromObject(context, storage_id_obj);
  if (!storage_id) {
    return;
  }

  protocol::String key;
  if (!ObjectGetProtocolString(context, params, "key").To(&key)) {
    return;
  }
  protocol::String new_value;
  if (!ObjectGetProtocolString(context, params, "newValue").To(&new_value)) {
    return;
  }
  frontend_->domStorageItemAdded(std::move(storage_id), key, new_value);
}

void DOMStorageAgent::domStorageItemRemoved(Local<Context> context,
                                            Local<Object> params) {
  Local<Object> storage_id_obj;
  if (!ObjectGetObject(context, params, "storageId").ToLocal(&storage_id_obj)) {
    return;
  }
  std::unique_ptr<protocol::DOMStorage::StorageId> storage_id =
      createStorageIdFromObject(context, storage_id_obj);

  if (!storage_id) {
    return;
  }

  protocol::String key;
  if (!ObjectGetProtocolString(context, params, "key").To(&key)) {
    return;
  }
  frontend_->domStorageItemRemoved(std::move(storage_id), key);
}

void DOMStorageAgent::domStorageItemUpdated(Local<Context> context,
                                            Local<Object> params) {
  Local<Object> storage_id_obj;
  if (!ObjectGetObject(context, params, "storageId").ToLocal(&storage_id_obj)) {
    return;
  }

  std::unique_ptr<protocol::DOMStorage::StorageId> storage_id =
      createStorageIdFromObject(context, storage_id_obj);

  if (!storage_id) {
    return;
  }

  protocol::String key;
  if (!ObjectGetProtocolString(context, params, "key").To(&key)) {
    return;
  }
  protocol::String old_value;
  if (!ObjectGetProtocolString(context, params, "oldValue").To(&old_value)) {
    return;
  }
  protocol::String new_value;
  if (!ObjectGetProtocolString(context, params, "newValue").To(&new_value)) {
    return;
  }
  frontend_->domStorageItemUpdated(
      std::move(storage_id), key, old_value, new_value);
}

void DOMStorageAgent::domStorageItemsCleared(Local<Context> context,
                                             Local<Object> params) {
  Local<Object> storage_id_obj;
  if (!ObjectGetObject(context, params, "storageId").ToLocal(&storage_id_obj)) {
    return;
  }
  std::unique_ptr<protocol::DOMStorage::StorageId> storage_id =
      createStorageIdFromObject(context, storage_id_obj);

  if (!storage_id) {
    return;
  }
  frontend_->domStorageItemsCleared(std::move(storage_id));
}

void DOMStorageAgent::registerStorage(Local<Context> context,
                                      Local<Object> params) {
  Isolate* isolate = env_->isolate();
  HandleScope handle_scope(isolate);
  bool is_local_storage;
  if (!ObjectGetBool(context, params, "isLocalStorage").To(&is_local_storage)) {
    return;
  }
  Local<Object> storage_map_obj;
  if (!ObjectGetObject(context, params, "storageMap")
           .ToLocal(&storage_map_obj)) {
    return;
  }
  std::unordered_map<std::string, std::string>& storage_map =
      is_local_storage ? local_storage_map_ : session_storage_map_;
  Local<Array> property_names;
  if (!storage_map_obj->GetOwnPropertyNames(context).ToLocal(&property_names)) {
    return;
  }
  uint32_t length = property_names->Length();
  for (uint32_t i = 0; i < length; ++i) {
    Local<Value> key_value;
    if (!property_names->Get(context, i).ToLocal(&key_value)) {
      return;
    }
    Local<Value> value_value;
    if (!storage_map_obj->Get(context, key_value).ToLocal(&value_value)) {
      return;
    }
    node::Utf8Value key_utf8(isolate, key_value);
    node::Utf8Value value_utf8(isolate, value_value);
    storage_map[*key_utf8] = *value_utf8;
  }
}

bool DOMStorageAgent::canEmit(const std::string& domain) {
  return domain == "DOMStorage";
}
}  // namespace inspector
}  // namespace node
