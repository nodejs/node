#ifndef SRC_NODE_CONTEXTIFY_H_
#define SRC_NODE_CONTEXTIFY_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object-inl.h"
#include "node_context_data.h"
#include "node_errors.h"

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
  ~ContextifyContext();
  static void CleanupHook(void* arg);

  v8::MaybeLocal<v8::Object> CreateDataWrapper(Environment* env);
  v8::MaybeLocal<v8::Context> CreateV8Context(Environment* env,
                                              v8::Local<v8::Object> sandbox_obj,
                                              const ContextOptions& options);
  static void Init(Environment* env, v8::Local<v8::Object> target);

  static ContextifyContext* ContextFromContextifiedSandbox(
      Environment* env,
      const v8::Local<v8::Object>& sandbox);

  inline Environment* env() const {
    return env_;
  }

  inline v8::Local<v8::Context> context() const {
    return PersistentToLocal::Default(env()->isolate(), context_);
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
  v8::Global<v8::Context> context_;
};

class ContextifyScript : public BaseObject {
 public:
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(ContextifyScript)
  SET_SELF_SIZE(ContextifyScript)

  ContextifyScript(Environment* env, v8::Local<v8::Object> object);
  ~ContextifyScript() override;

  static void Init(Environment* env, v8::Local<v8::Object> target);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static bool InstanceOf(Environment* env, const v8::Local<v8::Value>& args);
  static void CreateCachedData(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RunInThisContext(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RunInContext(const v8::FunctionCallbackInfo<v8::Value>& args);
  static bool EvalMachine(Environment* env,
                          const int64_t timeout,
                          const bool display_errors,
                          const bool break_on_sigint,
                          const bool break_on_first_line,
                          const v8::FunctionCallbackInfo<v8::Value>& args);

  inline uint32_t id() { return id_; }

 private:
  v8::Global<v8::UnboundScript> script_;
  uint32_t id_;
};

class CompiledFnEntry final : public BaseObject {
 public:
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(CompiledFnEntry)
  SET_SELF_SIZE(CompiledFnEntry)

  CompiledFnEntry(Environment* env,
                  v8::Local<v8::Object> object,
                  uint32_t id,
                  v8::Local<v8::ScriptOrModule> script);
  ~CompiledFnEntry();

 private:
  uint32_t id_;
  v8::Global<v8::ScriptOrModule> script_;

  static void WeakCallback(const v8::WeakCallbackInfo<CompiledFnEntry>& data);
};

}  // namespace contextify
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CONTEXTIFY_H_
