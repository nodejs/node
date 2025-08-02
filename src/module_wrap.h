#ifndef SRC_MODULE_WRAP_H_
#define SRC_MODULE_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "base_object.h"
#include "v8-script.h"

namespace node {

class IsolateData;
class Environment;
class ExternalReferenceRegistry;

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
  kID = 8,
  kLength = 9,
};

enum ModulePhase : int {
  kSourcePhase = 1,
  kEvaluationPhase = 2,
};

/**
 * ModuleCacheKey is used to uniquely identify a module request
 * in the module cache. It is a composition of the module specifier
 * and the import attributes. ModuleImportPhase is not included
 * in the key.
 */
struct ModuleCacheKey : public MemoryRetainer {
  using ImportAttributeVector =
      std::vector<std::pair<std::string, std::string>>;

  std::string specifier;
  ImportAttributeVector import_attributes;
  // A hash of the specifier, and import attributes.
  // This does not guarantee uniqueness, but is used to reduce
  // the number of comparisons needed when checking for equality.
  std::size_t hash;

  SET_MEMORY_INFO_NAME(ModuleCacheKey)
  SET_SELF_SIZE(ModuleCacheKey)
  void MemoryInfo(MemoryTracker* tracker) const override;

  template <int elements_per_attribute = 3>
  static ModuleCacheKey From(v8::Local<v8::Context> context,
                             v8::Local<v8::String> specifier,
                             v8::Local<v8::FixedArray> import_attributes);
  static ModuleCacheKey From(v8::Local<v8::Context> context,
                             v8::Local<v8::ModuleRequest> v8_request);

  struct Hash {
    std::size_t operator()(const ModuleCacheKey& request) const {
      return request.hash;
    }
  };

  // Equality operator for ModuleCacheKey.
  bool operator==(const ModuleCacheKey& other) const {
    // Hash does not provide uniqueness guarantee, so ignore it.
    return specifier == other.specifier &&
           import_attributes == other.import_attributes;
  }

 private:
  // Use public ModuleCacheKey::From to create instances.
  ModuleCacheKey(std::string specifier,
                 ImportAttributeVector import_attributes,
                 std::size_t hash)
      : specifier(specifier),
        import_attributes(import_attributes),
        hash(hash) {}
};

class ModuleWrap : public BaseObject {
  using ResolveCache =
      std::unordered_map<ModuleCacheKey, uint32_t, ModuleCacheKey::Hash>;

 public:
  enum InternalFields {
    kModuleSlot = BaseObject::kInternalFieldCount,
    kURLSlot,
    kModuleSourceObjectSlot,
    kSyntheticEvaluationStepsSlot,
    kContextObjectSlot,   // Object whose creation context is the target Context
    kLinkedRequestsSlot,  // Array of linked requests
    kInternalFieldCount
  };

  static void CreatePerIsolateProperties(IsolateData* isolate_data,
                                         v8::Local<v8::ObjectTemplate> target);
  static void CreatePerContextProperties(v8::Local<v8::Object> target,
                                         v8::Local<v8::Value> unused,
                                         v8::Local<v8::Context> context,
                                         void* priv);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static void HostInitializeImportMetaObjectCallback(
      v8::Local<v8::Context> context,
      v8::Local<v8::Module> module,
      v8::Local<v8::Object> meta);

  static void HasTopLevelAwait(const v8::FunctionCallbackInfo<v8::Value>& args);

  v8::Local<v8::Context> context() const;
  v8::Maybe<bool> CheckUnsettledTopLevelAwait();

  SET_MEMORY_INFO_NAME(ModuleWrap)
  SET_SELF_SIZE(ModuleWrap)
  SET_NO_MEMORY_INFO()

  bool IsNotIndicativeOfMemoryLeakAtExit() const override {
    // XXX: The garbage collection rules for ModuleWrap are *super* unclear.
    // Do these objects ever get GC'd? Are we just okay with leaking them?
    return true;
  }

  bool IsLinked() const { return linked_; }

  ModuleWrap* GetLinkedRequest(uint32_t index);

  static v8::Local<v8::PrimitiveArray> GetHostDefinedOptions(
      v8::Isolate* isolate, v8::Local<v8::Symbol> symbol);

  // When user_cached_data is not std::nullopt, use the code cache if it's not
  // nullptr, otherwise don't use code cache.
  // TODO(joyeecheung): when it is std::nullopt, use on-disk cache
  // See: https://github.com/nodejs/node/issues/47472
  static v8::MaybeLocal<v8::Module> CompileSourceTextModule(
      Realm* realm,
      v8::Local<v8::String> source_text,
      v8::Local<v8::String> url,
      int line_offset,
      int column_offset,
      v8::Local<v8::PrimitiveArray> host_defined_options,
      std::optional<v8::ScriptCompiler::CachedData*> user_cached_data,
      bool* cache_rejected);

  static void CreateRequiredModuleFacade(
      const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  ModuleWrap(Realm* realm,
             v8::Local<v8::Object> object,
             v8::Local<v8::Module> module,
             v8::Local<v8::String> url,
             v8::Local<v8::Object> context_object,
             v8::Local<v8::Value> synthetic_evaluation_step);
  ~ModuleWrap() override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetModuleRequests(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void InstantiateSync(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EvaluateSync(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetNamespaceSync(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetModuleSourceObject(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetModuleSourceObject(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Link(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Instantiate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Evaluate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetNamespace(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsGraphAsync(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetStatus(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetError(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void SetImportModuleDynamicallyCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetInitializeImportMetaObjectCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static v8::MaybeLocal<v8::Value> SyntheticModuleEvaluationStepsCallback(
      v8::Local<v8::Context> context, v8::Local<v8::Module> module);
  static void SetSyntheticExport(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CreateCachedData(const v8::FunctionCallbackInfo<v8::Value>& args);

  static v8::MaybeLocal<v8::Module> ResolveModuleCallback(
      v8::Local<v8::Context> context,
      v8::Local<v8::String> specifier,
      v8::Local<v8::FixedArray> import_attributes,
      v8::Local<v8::Module> referrer);
  static v8::MaybeLocal<v8::Object> ResolveSourceCallback(
      v8::Local<v8::Context> context,
      v8::Local<v8::String> specifier,
      v8::Local<v8::FixedArray> import_attributes,
      v8::Local<v8::Module> referrer);
  static ModuleWrap* GetFromModule(node::Environment*, v8::Local<v8::Module>);

  // This method may throw a JavaScript exception, so the return type is
  // wrapped in a Maybe.
  static v8::Maybe<ModuleWrap*> ResolveModule(
      v8::Local<v8::Context> context,
      v8::Local<v8::String> specifier,
      v8::Local<v8::FixedArray> import_attributes,
      v8::Local<v8::Module> referrer);

  v8::Global<v8::Module> module_;
  ResolveCache resolve_cache_;
  contextify::ContextifyContext* contextify_context_ = nullptr;
  bool synthetic_ = false;
  bool linked_ = false;
  int module_hash_;
};

}  // namespace loader
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_MODULE_WRAP_H_
