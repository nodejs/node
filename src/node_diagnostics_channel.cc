#include "node_diagnostics_channel.h"

#include "base_object-inl.h"
#include "env-inl.h"
#include "node_external_reference.h"
#include "util-inl.h"
#include "v8.h"

#include <cstdint>

namespace node {
namespace diagnostics_channel {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
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
      subscribers_(
          realm->isolate(), kMaxChannels, MAYBE_FIELD_PTR(info, subscribers)) {
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

void BindingData::LinkNativeChannel(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  BindingData* binding = realm->GetBindingData<BindingData>();
  CHECK_NOT_NULL(binding);

  CHECK(args[0]->IsFunction());
  Isolate* isolate = realm->isolate();
  Local<Context> context = realm->context();
  binding->link_callback_.Reset(isolate, args[0].As<Function>());

  // Resolve channels created before the link callback was available.
  for (auto& channel_ptr : binding->channels_) {
    if (channel_ptr && !channel_ptr->IsLinked()) {
      Local<String> name =
          String::NewFromUtf8(isolate, channel_ptr->name_.c_str())
              .ToLocalChecked();
      Local<Value> argv[] = {name};
      Local<Value> result;
      if (binding->link_callback_.Get(isolate)
              ->Call(context, v8::Undefined(isolate), 1, argv)
              .ToLocal(&result) &&
          result->IsObject()) {
        channel_ptr->Link(isolate, result.As<Object>());
      }
    }
  }
}

bool BindingData::PrepareForSerialization(Local<Context> context,
                                          SnapshotCreator* creator) {
  DCHECK_NULL(internal_field_info_);
  internal_field_info_ = InternalFieldInfoBase::New<InternalFieldInfo>(type());
  internal_field_info_->subscribers = subscribers_.Serialize(context, creator);
  link_callback_.Reset();
  channel_wrap_template_.Reset();
  channels_.clear();
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
  SetMethod(isolate, target, "linkNativeChannel", LinkNativeChannel);
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
  registry->Register(LinkNativeChannel);
}

Channel::Channel(Environment* env,
                 Local<Object> wrap,
                 BindingData* binding_data,
                 uint32_t index,
                 std::string name)
    : BaseObject(env, wrap),
      binding_data_(binding_data),
      index_(index),
      name_(std::move(name)) {
  MakeWeak();
}

void Channel::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("js_channel", js_channel_);
  tracker->TrackField("publish_fn", publish_fn_);
}

void Channel::Link(Isolate* isolate, Local<Object> js_channel) {
  js_channel_.Reset(isolate, js_channel);
  // If the channel already has subscribers at link time (JS subscribed before
  // C++ called Get()), cache publish eagerly so Publish() has no extra work.
  if (HasSubscribers()) {
    CachePublishFn(isolate, js_channel);
  }
}

void Channel::Unlink() {
  js_channel_.Reset();
  publish_fn_.Reset();
}

Channel* Channel::Get(Environment* env, const char* name) {
  Realm* realm = env->principal_realm();
  BindingData* binding = realm->GetBindingData<BindingData>();
  if (binding == nullptr) {
    return nullptr;
  }
  uint32_t index = binding->GetOrCreateChannelIndex(std::string(name));

  if (index >= binding->channels_.size()) {
    binding->channels_.resize(index + 1);
  }

  if (!binding->channels_[index]) {
    Isolate* isolate = env->isolate();
    HandleScope handle_scope(isolate);
    Local<Context> context = env->context();

    // Lazily create the wrap template for Channel objects.
    if (binding->channel_wrap_template_.IsEmpty()) {
      Local<FunctionTemplate> tmpl =
          BaseObject::MakeLazilyInitializedJSTemplate(env);
      binding->channel_wrap_template_.Reset(isolate, tmpl);
    }

    Local<Object> wrap;
    if (!binding->channel_wrap_template_.Get(isolate)
             ->GetFunction(context)
             .ToLocalChecked()
             ->NewInstance(context)
             .ToLocal(&wrap)) {
      return nullptr;
    }

    binding->channels_[index] = MakeDetachedBaseObject<Channel>(
        env, wrap, binding, index, std::string(name));
  }

  Channel* channel = binding->channels_[index].get();

  // Late-bind: link to the JS channel when the callback is available.
  if (!binding->link_callback_.IsEmpty() && !channel->IsLinked()) {
    Isolate* isolate = env->isolate();
    HandleScope handle_scope(isolate);
    Local<Context> context = env->context();
    Local<String> js_name = String::NewFromUtf8(isolate, name).ToLocalChecked();
    Local<Value> argv[] = {js_name};
    Local<Value> result;
    if (binding->link_callback_.Get(isolate)
            ->Call(context, v8::Undefined(isolate), 1, argv)
            .ToLocal(&result) &&
        result->IsObject()) {
      channel->Link(isolate, result.As<Object>());
    }
  }

  return channel;
}

void Channel::CachePublishFn(Isolate* isolate, Local<Object> js_channel) {
  Local<Value> publish_val;
  Local<Context> context = isolate->GetCurrentContext();
  if (js_channel->Get(context, FIXED_ONE_BYTE_STRING(isolate, "publish"))
          .ToLocal(&publish_val) &&
      publish_val->IsFunction()) {
    publish_fn_.Reset(isolate, publish_val.As<Function>());
  }
}

void Channel::Publish(Environment* env, Local<Value> message) {
  if (!HasSubscribers()) return;

  if (binding_data_ == nullptr) return;

  if (js_channel_.IsEmpty()) return;

  // Publishing is not possible during shutdown or GC.
  DCHECK(env->can_call_into_js());
  if (!env->can_call_into_js()) return;

  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = env->context();
  Context::Scope context_scope(context);

  Local<Object> js_channel = js_channel_.Get(isolate);

  // publish_fn_ is eagerly cached by Link() when the channel already has
  // subscribers at link time. For channels linked before any JS subscriber
  // existed, cache it here on the first publish — happens exactly once.
  if (publish_fn_.IsEmpty()) {
    CachePublishFn(isolate, js_channel);
    if (publish_fn_.IsEmpty()) return;
  }

  Local<Value> argv[] = {message};
  USE(publish_fn_.Get(isolate)->Call(context, js_channel, 1, argv));
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
