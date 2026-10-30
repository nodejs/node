#include "dom_storage_agent.h"
#include <optional>
#include "base_object-inl.h"
#include "env-inl.h"
#include "inspector/inspector_object_utils.h"
#include "node_realm-inl.h"
#include "path.h"
#include "util.h"
#include "v8-exception.h"
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

static void ThrowEventError(v8::Isolate* isolate, const std::string& message) {
  isolate->ThrowException(v8::Exception::TypeError(
      v8::String::NewFromUtf8(isolate, message.c_str()).ToLocalChecked()));
}

std::unique_ptr<protocol::DOMStorage::StorageId> createStorageIdFromObject(
    Local<Context> context, Local<Object> storage_id_obj) {
  protocol::String security_origin;
  Isolate* isolate = Isolate::GetCurrent();
  if (!ObjectGetProtocolString(context, storage_id_obj, "securityOrigin")
           .To(&security_origin)) {
    ThrowEventError(isolate, "Missing securityOrigin in storageId");
    return {};
  }
  bool is_local_storage =
      ObjectGetBool(context, storage_id_obj, "isLocalStorage").FromMaybe(false);
  protocol::String storageKey;
  if (!ObjectGetProtocolString(context, storage_id_obj, "storageKey")
           .To(&storageKey)) {
    ThrowEventError(isolate, "Missing storageKey in storageId");
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
  const StorageMap* storage_map =
      is_local_storage ? &local_storage_map_ : &session_storage_map_;
  std::optional<StorageMap> storage_map_fallback;
  if (storage_map->empty()) {
    bool is_uninitialized = false;
    auto web_storage_obj = getWebStorage(is_local_storage, &is_uninitialized);
    // Each way of failing below says something different about the store, so
    // each reports a different reason. A frontend that cannot read a store
    // otherwise has no way to tell a missing one from a corrupt one.
    if (!web_storage_obj && !is_uninitialized) {
      return protocol::DispatchResponse::ServerError(
          "Could not read DOM storage items: storage is unavailable");
    }
    std::string open_error;
    if (web_storage_obj) {
      storage_map_fallback = web_storage_obj.value()->GetAll(&open_error);
    } else if (is_local_storage) {
      std::string location = env_->options()->localstorage_file;
      if (location != node::webstorage::kInMemoryPath) {
        location = ToNamespacedPath(env_, location);
      }
      storage_map_fallback =
          node::webstorage::Storage::ReadAllFromFile(location, &open_error);
    } else {
      storage_map_fallback.emplace();
    }
    if (!open_error.empty()) {
      // Pass the reason along; "the file was written by a newer Node.js" and
      // "the file is locked" are not the same problem to the user. Prefix with
      // "Uncaught Error: " to match the v8::Message format without invoking
      // JavaScript during a V8 interrupt.
      return protocol::DispatchResponse::ServerError(
          "Could not read DOM storage items: Uncaught Error: " + open_error);
    }
    if (!storage_map_fallback.has_value()) {
      return protocol::DispatchResponse::ServerError(
          "Could not read DOM storage items: the backing file is malformed");
    }
    storage_map = &storage_map_fallback.value();
  }

  auto result =
      std::make_unique<protocol::Array<protocol::Array<protocol::String>>>();
  for (const auto& pair : *storage_map) {
    auto item = std::make_unique<protocol::Array<protocol::String>>();
    item->push_back(protocol::StringUtil::fromUTF16(
        reinterpret_cast<const uint16_t*>(pair.first.data()),
        pair.first.size()));
    item->push_back(protocol::StringUtil::fromUTF16(
        reinterpret_cast<const uint16_t*>(pair.second.data()),
        pair.second.size()));
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
  Isolate* isolate = env_->isolate();
  Local<Object> storage_id_obj;
  if (!ObjectGetObject(context, params, "storageId").ToLocal(&storage_id_obj)) {
    ThrowEventError(isolate, "Missing storageId in event");
    return;
  }

  std::unique_ptr<protocol::DOMStorage::StorageId> storage_id =
      createStorageIdFromObject(context, storage_id_obj);
  if (!storage_id) {
    return;
  }

  protocol::String key;
  if (!ObjectGetProtocolString(context, params, "key").To(&key)) {
    ThrowEventError(isolate, "Missing key in event");
    return;
  }
  protocol::String new_value;
  if (!ObjectGetProtocolString(context, params, "newValue").To(&new_value)) {
    ThrowEventError(isolate, "Missing newValue in event");
    return;
  }
  frontend_->domStorageItemAdded(std::move(storage_id), key, new_value);
}

void DOMStorageAgent::domStorageItemRemoved(Local<Context> context,
                                            Local<Object> params) {
  Isolate* isolate = env_->isolate();
  Local<Object> storage_id_obj;
  if (!ObjectGetObject(context, params, "storageId").ToLocal(&storage_id_obj)) {
    ThrowEventError(isolate, "Missing storageId in event");
    return;
  }
  std::unique_ptr<protocol::DOMStorage::StorageId> storage_id =
      createStorageIdFromObject(context, storage_id_obj);

  if (!storage_id) {
    return;
  }

  protocol::String key;
  if (!ObjectGetProtocolString(context, params, "key").To(&key)) {
    ThrowEventError(isolate, "Missing key in event");
    return;
  }
  frontend_->domStorageItemRemoved(std::move(storage_id), key);
}

void DOMStorageAgent::domStorageItemUpdated(Local<Context> context,
                                            Local<Object> params) {
  Isolate* isolate = env_->isolate();
  Local<Object> storage_id_obj;
  if (!ObjectGetObject(context, params, "storageId").ToLocal(&storage_id_obj)) {
    ThrowEventError(isolate, "Missing storageId in event");
    return;
  }

  std::unique_ptr<protocol::DOMStorage::StorageId> storage_id =
      createStorageIdFromObject(context, storage_id_obj);

  if (!storage_id) {
    return;
  }

  protocol::String key;
  if (!ObjectGetProtocolString(context, params, "key").To(&key)) {
    ThrowEventError(isolate, "Missing key in event");
    return;
  }
  protocol::String old_value;
  if (!ObjectGetProtocolString(context, params, "oldValue").To(&old_value)) {
    ThrowEventError(isolate, "Missing oldValue in event");
    return;
  }
  protocol::String new_value;
  if (!ObjectGetProtocolString(context, params, "newValue").To(&new_value)) {
    ThrowEventError(isolate, "Missing newValue in event");
    return;
  }
  frontend_->domStorageItemUpdated(
      std::move(storage_id), key, old_value, new_value);
}

void DOMStorageAgent::domStorageItemsCleared(Local<Context> context,
                                             Local<Object> params) {
  Isolate* isolate = env_->isolate();
  Local<Object> storage_id_obj;
  if (!ObjectGetObject(context, params, "storageId").ToLocal(&storage_id_obj)) {
    ThrowEventError(isolate, "Missing storageId in event");
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
    ThrowEventError(isolate, "Missing isLocalStorage in event");
    return;
  }
  Local<Object> storage_map_obj;
  if (!ObjectGetObject(context, params, "storageMap")
           .ToLocal(&storage_map_obj)) {
    ThrowEventError(isolate, "Missing storageMap in event");
    return;
  }
  StorageMap& storage_map =
      is_local_storage ? local_storage_map_ : session_storage_map_;
  Local<Array> property_names;
  if (!storage_map_obj->GetOwnPropertyNames(context).ToLocal(&property_names)) {
    ThrowEventError(isolate, "Failed to get property names from storageMap");
    return;
  }
  uint32_t length = property_names->Length();
  for (uint32_t i = 0; i < length; ++i) {
    Local<Value> key_value;
    if (!property_names->Get(context, i).ToLocal(&key_value)) {
      ThrowEventError(isolate, "Failed to get key from storageMap");
      return;
    }
    Local<Value> value_value;
    if (!storage_map_obj->Get(context, key_value).ToLocal(&value_value)) {
      ThrowEventError(isolate, "Failed to get value from storageMap");
      return;
    }
    node::TwoByteValue key_utf16(isolate, key_value);
    node::TwoByteValue value_utf16(isolate, value_value);
    storage_map[key_utf16.ToU16String()] = value_utf16.ToU16String();
  }
}

std::optional<node::webstorage::Storage*> DOMStorageAgent::getWebStorage(
    bool is_local_storage, bool* is_uninitialized) {
  *is_uninitialized = false;
  if (!env_->options()->webstorage) {
    return std::nullopt;
  }
  if (is_local_storage && env_->options()->localstorage_file.empty()) {
    return std::nullopt;
  }
  if (auto* binding = env_->principal_realm()
                          ->GetBindingData<node::webstorage::BindingData>()) {
    const auto& storage_ptr =
        is_local_storage ? binding->local_storage : binding->session_storage;
    if (storage_ptr) {
      return storage_ptr.get();
    }
  }
  if (env_->no_browser_globals()) {
    return std::nullopt;
  }
  v8::Isolate* isolate = env_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = env_->context();
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::String> prop_name =
      is_local_storage ? FIXED_ONE_BYTE_STRING(isolate, "localStorage")
                       : FIXED_ONE_BYTE_STRING(isolate, "sessionStorage");
  // Protocol messages from a remote frontend are dispatched from a V8 API
  // interrupt, where executing JavaScript is disallowed. Both globals are
  // defined as lazy JS getters, so check whether the property is still an
  // accessor before reading it.
  bool is_accessor = false;
  if (!global->HasRealNamedCallbackProperty(context, prop_name)
           .To(&is_accessor)) {
    return std::nullopt;
  }
  if (is_accessor) {
    if (env_->is_processing_v8_interrupt()) {
      *is_uninitialized = true;
      return std::nullopt;
    }
    if (!env_->can_call_into_js()) {
      return std::nullopt;
    }
  }
  v8::Local<v8::Value> web_storage_val;
  v8::TryCatch try_catch(isolate);
  if (!global->Get(context, prop_name).ToLocal(&web_storage_val) ||
      !web_storage_val->IsObject() || try_catch.HasCaught()) {
    return std::nullopt;
  }
  node::webstorage::Storage* storage;
  ASSIGN_OR_RETURN_UNWRAP(
      &storage, web_storage_val.As<v8::Object>(), std::nullopt);
  return storage;
}

bool DOMStorageAgent::canEmit(const std::string& domain) {
  return domain == "DOMStorage";
}
}  // namespace inspector
}  // namespace node
