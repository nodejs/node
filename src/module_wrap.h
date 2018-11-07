#ifndef SRC_MODULE_WRAP_H_
#define SRC_MODULE_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <unordered_map>
#include <string>
#include <vector>
#include "node_url.h"
#include "base_object-inl.h"

namespace node {
namespace loader {

enum PackageMainCheck : bool {
    CheckMain = true,
    IgnoreMain = false
};

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

v8::Maybe<url::URL> Resolve(Environment* env,
                            const std::string& specifier,
                            const url::URL& base,
                            PackageMainCheck read_pkg_json = CheckMain);

class ModuleWrap : public BaseObject {
 public:
  static const std::string EXTENSIONS[];
  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context);
  static void HostInitializeImportMetaObjectCallback(
      v8::Local<v8::Context> context,
      v8::Local<v8::Module> module,
      v8::Local<v8::Object> meta);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("url", url_);
    tracker->TrackField("resolve_cache", resolve_cache_);
  }

  inline uint32_t id() { return id_; }
  static ModuleWrap* GetFromID(node::Environment*, uint32_t id);

  SET_MEMORY_INFO_NAME(ModuleWrap)
  SET_SELF_SIZE(ModuleWrap)

 private:
  ModuleWrap(Environment* env,
             v8::Local<v8::Object> object,
             v8::Local<v8::Module> module,
             v8::Local<v8::String> url);
  ~ModuleWrap();

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Link(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Instantiate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Evaluate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Namespace(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetStatus(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetError(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetStaticDependencySpecifiers(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Resolve(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetImportModuleDynamicallyCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetInitializeImportMetaObjectCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static v8::MaybeLocal<v8::Module> ResolveCallback(
      v8::Local<v8::Context> context,
      v8::Local<v8::String> specifier,
      v8::Local<v8::Module> referrer);
  static ModuleWrap* GetFromModule(node::Environment*, v8::Local<v8::Module>);

  Persistent<v8::Module> module_;
  Persistent<v8::String> url_;
  bool linked_ = false;
  std::unordered_map<std::string, Persistent<v8::Promise>> resolve_cache_;
  Persistent<v8::Context> context_;
  uint32_t id_;
};

}  // namespace loader
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_MODULE_WRAP_H_
