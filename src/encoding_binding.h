#ifndef SRC_ENCODING_BINDING_H_
#define SRC_ENCODING_BINDING_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cinttypes>
#include "aliased_buffer.h"
#include "node_snapshotable.h"
#include "v8-fast-api-calls.h"

namespace node {
class ExternalReferenceRegistry;

namespace encoding_binding {
class BindingData : public SnapshotableObject {
 public:
  BindingData(Environment* env, v8::Local<v8::Object> obj);

  using InternalFieldInfo = InternalFieldInfoBase;

  SERIALIZABLE_OBJECT_METHODS()
  SET_BINDING_ID(encoding_binding_data)

  SET_NO_MEMORY_INFO()
  SET_SELF_SIZE(BindingData)
  SET_MEMORY_INFO_NAME(BindingData)

  static void EncodeInto(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EncodeUtf8String(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DecodeUTF8(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context,
                         void* priv);
  static void RegisterTimerExternalReferences(
      ExternalReferenceRegistry* registry);
};

}  // namespace encoding_binding

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ENCODING_BINDING_H_