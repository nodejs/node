#ifndef SRC_NODE_CONTEXTIFY_H_
#define SRC_NODE_CONTEXTIFY_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_internals.h"
#include "node_context_data.h"
#include "base_object-inl.h"

namespace node {
namespace contextify {

struct ContextOptions {
  v8::Local<v8::String> name;
  v8::Local<v8::String> origin;
  v8::Local<v8::Boolean> allow_code_gen_strings;
  v8::Local<v8::Boolean> allow_code_gen_wasm;
};

class ContextifyContext {
 public:
  ContextifyContext(Environment* env,
                    v8::Local<v8::Object> sandbox_obj,
                    const ContextOptions& options);

  v8::Local<v8::Value> CreateDataWrapper(Environment* env);
  v8::Local<v8::Context> CreateV8Context(Environment* env,
      v8::Local<v8::Object> sandbox_obj, const ContextOptions& options);
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
        context()->GetEmbedderData(ContextEmbedderIndex::kSandboxObject));
  }


  template <typename T>
  static ContextifyContext* Get(const v8::PropertyCallbackInfo<T>& args);

 private:
  static void MakeContext(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsContext(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CompileFunction(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WeakCallback(
      const v8::WeakCallbackInfo<ContextifyContext>& data);
  static void WeakCallbackCompileFn(
      const v8::WeakCallbackInfo<CompileFnEntry>& data);
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
  Environment* const env_;
  Persistent<v8::Context> context_;
};

class ContextifyScript : public BaseObject {
 public:
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(ContextifyScript)
  SET_SELF_SIZE(ContextifyScript)

  ContextifyScript(Environment* env, v8::Local<v8::Object> object);
  ~ContextifyScript();

  static void Init(Environment* env, v8::Local<v8::Object> target);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static bool InstanceOf(Environment* env, const v8::Local<v8::Value>& args);
  static void CreateCachedData(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RunInThisContext(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RunInContext(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DecorateErrorStack(Environment* env,
                                 const v8::TryCatch& try_catch);
  static bool EvalMachine(Environment* env,
                          const int64_t timeout,
                          const bool display_errors,
                          const bool break_on_sigint,
                          const v8::FunctionCallbackInfo<v8::Value>& args);

  inline uint32_t id() { return id_; }

 private:
  node::Persistent<v8::UnboundScript> script_;
  uint32_t id_;
};

}  // namespace contextify
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CONTEXTIFY_H_
