#ifndef SRC_NODE_DIAGNOSTICS_CHANNEL_H_
#define SRC_NODE_DIAGNOSTICS_CHANNEL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cinttypes>
#include <string>
#include <unordered_map>
#include "aliased_buffer.h"
#include "node_snapshotable.h"

namespace node {
class ExternalReferenceRegistry;

namespace diagnostics_channel {

class Channel;

class BindingData : public SnapshotableObject {
 public:
  static constexpr size_t kMaxChannels = 1024;

  struct InternalFieldInfo : public node::InternalFieldInfoBase {
    AliasedBufferIndex subscribers;
  };

  BindingData(Realm* realm,
              v8::Local<v8::Object> wrap,
              InternalFieldInfo* info = nullptr);

  SERIALIZABLE_OBJECT_METHODS()
  SET_BINDING_ID(diagnostics_channel_binding_data)

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_SELF_SIZE(BindingData)
  SET_MEMORY_INFO_NAME(BindingData)

  AliasedUint32Array subscribers_;

  uint32_t next_channel_index_ = 0;
  std::unordered_map<std::string, uint32_t> channel_indices_;

  uint32_t GetOrCreateChannelIndex(const std::string& name);

  v8::Global<v8::Function> publish_callback_;

  static void GetOrCreateChannelIndex(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetPublishCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void CreatePerIsolateProperties(IsolateData* isolate_data,
                                         v8::Local<v8::ObjectTemplate> target);
  static void CreatePerContextProperties(v8::Local<v8::Object> target,
                                         v8::Local<v8::Value> unused,
                                         v8::Local<v8::Context> context,
                                         void* priv);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

 private:
  InternalFieldInfo* internal_field_info_ = nullptr;
};

class Channel {
 public:
  static Channel Get(Environment* env, const char* name);

  inline bool HasSubscribers() const {
    return binding_data_ != nullptr && binding_data_->subscribers_[index_] > 0;
  }

  void Publish(Environment* env, v8::Local<v8::Value> message) const;

 private:
  Channel(BindingData* binding_data, uint32_t index, const char* name);

  BindingData* binding_data_;
  uint32_t index_;
  const char* name_;
};

}  // namespace diagnostics_channel
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_DIAGNOSTICS_CHANNEL_H_
