#ifndef SRC_NODE_CONTEXTIFY_H_
#define SRC_NODE_CONTEXTIFY_H_

#include "node_internals.h"
#include "node_watchdog.h"
#include "base_object-inl.h"

namespace node {
namespace contextify {

class ContextifyContext {
 protected:
  // V8 reserves the first field in context objects for the debugger. We use the
  // second field to hold a reference to the sandbox object.
  enum { kSandboxObjectIndex = 1 };

  Environment* const env_;
  v8::Persistent<v8::Context> context_;

 public:
  ContextifyContext(Environment* env,
                    v8::Local<v8::Object> sandbox_obj,
                    v8::Local<v8::Object> options_obj);
  ~ContextifyContext();

  v8::Local<v8::Value> CreateDataWrapper(Environment* env);
  v8::Local<v8::Context> CreateV8Context(Environment* env,
      v8::Local<v8::Object> sandbox_obj, v8::Local<v8::Object> options_obj);
  static void Init(Environment* env, v8::Local<v8::Object> target);

  static ContextifyContext* ContextFromContextifiedSandbox(
      Environment* env,
      const v8::Local<v8::Object>& sandbox);

  inline Environment* env() const {
    return env_;
  }

  inline v8::Local<v8::Context> context() const {
    return PersistentToLocal(env()->isolate(), context_);
  }

  inline v8::Local<v8::Object> global_proxy() const {
    return context()->Global();
  }

  inline v8::Local<v8::Object> sandbox() const {
    return v8::Local<v8::Object>::Cast(
        context()->GetEmbedderData(kSandboxObjectIndex));
  }

 private:
  static void MakeContext(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsContext(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WeakCallback(
      const v8::WeakCallbackInfo<ContextifyContext>& data);
  static void PropertyGetterCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Value>& args);
  static void PropertySetterCallback(
      v8::Local<v8::Name> property,
      v8::Local<v8::Value> value,
      const v8::PropertyCallbackInfo<v8::Value>& args);
  static void PropertyDescriptorCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Value>& args);
  static void PropertyDefinerCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyDescriptor& desc,
      const v8::PropertyCallbackInfo<v8::Value>& args);
  static void PropertyDeleterCallback(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Boolean>& args);
  static void PropertyEnumeratorCallback(
      const v8::PropertyCallbackInfo<v8::Array>& args);
  static void IndexedPropertyGetterCallback(
      uint32_t index,
      const v8::PropertyCallbackInfo<v8::Value>& args);
  static void IndexedPropertySetterCallback(
      uint32_t index,
      v8::Local<v8::Value> value,
      const v8::PropertyCallbackInfo<v8::Value>& args);
  static void IndexedPropertyDescriptorCallback(
      uint32_t index,
      const v8::PropertyCallbackInfo<v8::Value>& args);
  static void IndexedPropertyDefinerCallback(
      uint32_t index,
      const v8::PropertyDescriptor& desc,
      const v8::PropertyCallbackInfo<v8::Value>& args);
  static void IndexedPropertyDeleterCallback(
      uint32_t index,
      const v8::PropertyCallbackInfo<v8::Boolean>& args);
};

v8::Maybe<bool> GetBreakOnSigintArg(
    Environment* env, v8::Local<v8::Value> options);
v8::Maybe<int64_t> GetTimeoutArg(
    Environment* env, v8::Local<v8::Value> options);
v8::MaybeLocal<v8::Integer> GetLineOffsetArg(
    Environment* env, v8::Local<v8::Value> options);
v8::MaybeLocal<v8::Integer> GetColumnOffsetArg(
    Environment* env, v8::Local<v8::Value> options);
v8::MaybeLocal<v8::Context> GetContextArg(
    Environment* env, v8::Local<v8::Value> options);

}  // namespace contextify
}  // namespace node

#endif  // SRC_NODE_CONTEXTIFY_H_
