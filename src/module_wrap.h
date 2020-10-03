#ifndef SRC_MODULE_WRAP_H_
#define SRC_MODULE_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <unordered_map>
#include <string>
#include <vector>
#include "base_object.h"

namespace node {

class Environment;

namespace contextify {
class ContextifyContext;
}

namespace loader {

enum ScriptType : int {
  kScript,
  kModule,
  kFunction,
};

enum HostDefinedOptions : int {
  kType = 8,
  kID = 9,
  kLength = 10,
};

class ModuleWrap : public BaseObject {
 public:
  enum InternalFields {
    kModuleWrapBaseField = BaseObject::kInternalFieldCount,
    kURLSlot,
    kSyntheticEvaluationStepsSlot,
    kContextObjectSlot,  // Object whose creation context is the target Context
    kInternalFieldCount
  };

  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context,
                         void* priv);
  static void HostInitializeImportMetaObjectCallback(
      v8::Local<v8::Context> context,
      v8::Local<v8::Module> module,
      v8::Local<v8::Object> meta);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("resolve_cache", resolve_cache_);
  }

  inline uint32_t id() { return id_; }
  v8::Local<v8::Context> context() const;
  static ModuleWrap* GetFromID(node::Environment*, uint32_t id);

  SET_MEMORY_INFO_NAME(ModuleWrap)
  SET_SELF_SIZE(ModuleWrap)

  bool IsNotIndicativeOfMemoryLeakAtExit() const override {
    // XXX: The garbage collection rules for ModuleWrap are *super* unclear.
    // Do these objects ever get GC'd? Are we just okay with leaking them?
    return true;
  }

 private:
  ModuleWrap(Environment* env,
             v8::Local<v8::Object> object,
             v8::Local<v8::Module> module,
             v8::Local<v8::String> url);
  ~ModuleWrap() override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Link(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Instantiate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Evaluate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetNamespace(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetStatus(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetError(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetStaticDependencySpecifiers(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void SetImportModuleDynamicallyCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetInitializeImportMetaObjectCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static v8::MaybeLocal<v8::Value> SyntheticModuleEvaluationStepsCallback(
      v8::Local<v8::Context> context, v8::Local<v8::Module> module);
  static void SetSyntheticExport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CreateCachedData(const v8::FunctionCallbackInfo<v8::Value>& args);

  static v8::MaybeLocal<v8::Module> ResolveCallback(
      v8::Local<v8::Context> context,
      v8::Local<v8::String> specifier,
      v8::Local<v8::Module> referrer);
  static ModuleWrap* GetFromModule(node::Environment*, v8::Local<v8::Module>);

  v8::Global<v8::Module> module_;
  std::unordered_map<std::string, v8::Global<v8::Promise>> resolve_cache_;
  contextify::ContextifyContext* contextify_context_ = nullptr;
  bool synthetic_ = false;
  bool linked_ = false;
  uint32_t id_;
};

}  // namespace loader
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_MODULE_WRAP_H_
