#include "node_diagnostics_channel.h"

#include "env-inl.h"
#include "node_external_reference.h"
#include "util-inl.h"
#include "v8.h"

#include <cstdint>

namespace node {
namespace diagnostics_channel {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::SnapshotCreator;
using v8::String;
using v8::Value;

BindingData::BindingData(Realm* realm,
                         Local<Object> wrap,
                         InternalFieldInfo* info)
    : SnapshotableObject(realm, wrap, type_int),
      subscribers_(realm->isolate(),
                   kMaxChannels,
                   MAYBE_FIELD_PTR(info, subscribers)) {
  if (info == nullptr) {
    wrap->Set(realm->context(),
              FIXED_ONE_BYTE_STRING(realm->isolate(), "subscribers"),
              subscribers_.GetJSArray())
        .Check();
  } else {
    subscribers_.Deserialize(realm->context());
  }
  subscribers_.MakeWeak();
}

void BindingData::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("subscribers", subscribers_);
}

uint32_t BindingData::GetOrCreateChannelIndex(const std::string& name) {
  auto it = channel_indices_.find(name);
  if (it != channel_indices_.end()) {
    return it->second;
  }
  CHECK_LT(next_channel_index_, kMaxChannels);
  uint32_t index = next_channel_index_++;
  channel_indices_.emplace(name, index);
  return index;
}

void BindingData::GetOrCreateChannelIndex(
    const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  BindingData* binding = realm->GetBindingData<BindingData>();
  CHECK_NOT_NULL(binding);

  CHECK(args[0]->IsString());
  Utf8Value name(realm->isolate(), args[0]);

  uint32_t index = binding->GetOrCreateChannelIndex(*name);
  args.GetReturnValue().Set(index);
}

void BindingData::SetPublishCallback(
    const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  BindingData* binding = realm->GetBindingData<BindingData>();
  CHECK_NOT_NULL(binding);

  CHECK(args[0]->IsFunction());
  binding->publish_callback_.Reset(realm->isolate(),
                                   args[0].As<v8::Function>());
}

bool BindingData::PrepareForSerialization(Local<Context> context,
                                          SnapshotCreator* creator) {
  DCHECK_NULL(internal_field_info_);
  internal_field_info_ = InternalFieldInfoBase::New<InternalFieldInfo>(type());
  internal_field_info_->subscribers =
      subscribers_.Serialize(context, creator);
  publish_callback_.Reset();
  return true;
}

InternalFieldInfoBase* BindingData::Serialize(int index) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  InternalFieldInfo* info = internal_field_info_;
  internal_field_info_ = nullptr;
  return info;
}

void BindingData::Deserialize(Local<Context> context,
                              Local<Object> holder,
                              int index,
                              InternalFieldInfoBase* info) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  HandleScope scope(Isolate::GetCurrent());
  Realm* realm = Realm::GetCurrent(context);
  BindingData* binding = realm->AddBindingData<BindingData>(
      holder, static_cast<InternalFieldInfo*>(info));
  CHECK_NOT_NULL(binding);
}

void BindingData::CreatePerIsolateProperties(IsolateData* isolate_data,
                                             Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();
  SetMethod(
      isolate, target, "getOrCreateChannelIndex", GetOrCreateChannelIndex);
  SetMethod(isolate, target, "setPublishCallback", SetPublishCallback);
}

void BindingData::CreatePerContextProperties(Local<Object> target,
                                             Local<Value> unused,
                                             Local<Context> context,
                                             void* priv) {
  Realm* realm = Realm::GetCurrent(context);
  BindingData* const binding = realm->AddBindingData<BindingData>(target);
  if (binding == nullptr) return;
}

void BindingData::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetOrCreateChannelIndex);
  registry->Register(SetPublishCallback);
}

Channel::Channel(BindingData* binding_data, uint32_t index, const char* name)
    : binding_data_(binding_data), index_(index), name_(name) {}

Channel Channel::Get(Environment* env, const char* name) {
  Realm* realm = env->principal_realm();
  BindingData* binding = realm->GetBindingData<BindingData>();
  if (binding == nullptr) {
    return Channel(nullptr, 0, name);
  }
  uint32_t index = binding->GetOrCreateChannelIndex(std::string(name));
  return Channel(binding, index, name);
}

void Channel::Publish(Environment* env, Local<Value> message) const {
  if (!HasSubscribers()) return;

  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = env->context();
  Context::Scope context_scope(context);

  if (binding_data_->publish_callback_.IsEmpty()) return;

  Local<v8::Function> callback =
      binding_data_->publish_callback_.Get(isolate);
  Local<String> channel_name =
      String::NewFromUtf8(isolate, name_).ToLocalChecked();

  Local<Value> argv[] = {channel_name, message};
  USE(callback->Call(context, v8::Undefined(isolate), 2, argv));
}

}  // namespace diagnostics_channel
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    diagnostics_channel,
    node::diagnostics_channel::BindingData::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(
    diagnostics_channel,
    node::diagnostics_channel::BindingData::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(
    diagnostics_channel,
    node::diagnostics_channel::BindingData::RegisterExternalReferences)
