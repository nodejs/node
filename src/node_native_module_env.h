#ifndef SRC_NODE_NATIVE_MODULE_ENV_H_
#define SRC_NODE_NATIVE_MODULE_ENV_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <vector>
#include "node_native_module.h"

namespace node {
class Environment;
class ExternalReferenceRegistry;
namespace native_module {

// TODO(joyeecheung): since it's safer to make the code cache part of the
// embedded snapshot, there is no need to separate NativeModuleEnv and
// NativeModuleLoader. Merge the two.
class NativeModuleEnv {
 public:
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context,
                         void* priv);

  static v8::MaybeLocal<v8::Function> LookupAndCompile(
      v8::Local<v8::Context> context,
      const char* id,
      std::vector<v8::Local<v8::String>>* parameters,
      Environment* optional_env);

  static v8::Local<v8::Object> GetSourceObject(v8::Local<v8::Context> context);
  // Returns config.gypi as a JSON string
  static v8::Local<v8::String> GetConfigString(v8::Isolate* isolate);
  static bool Exists(const char* id);
  static bool Add(const char* id, const UnionBytes& source);

  static bool CompileAllModules(v8::Local<v8::Context> context);
  static void RefreshCodeCache(const std::vector<CodeCacheInfo>& in);
  static void CopyCodeCache(std::vector<CodeCacheInfo>* out);

 private:
  static void RecordResult(const char* id,
                           NativeModuleLoader::Result result,
                           Environment* env);
  static void GetModuleCategories(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  static void GetCacheUsage(const v8::FunctionCallbackInfo<v8::Value>& args);
  // Passing ids of builtin module source code into JS land as
  // internalBinding('native_module').moduleIds
  static void ModuleIdsGetter(v8::Local<v8::Name> property,
                              const v8::PropertyCallbackInfo<v8::Value>& info);
  // Passing config.gypi into JS land as internalBinding('native_module').config
  static void ConfigStringGetter(
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  // Compile a specific native module as a function
  static void CompileFunction(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HasCachedBuiltins(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static bool has_code_cache_;
};

}  // namespace native_module

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_NATIVE_MODULE_ENV_H_
