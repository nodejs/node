#include <algorithm>

#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_external_reference.h"
#include "node_native_module_env.h"

namespace node {
namespace native_module {

using v8::Context;
using v8::DEFAULT;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::IntegrityLevel;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Name;
using v8::None;
using v8::Object;
using v8::PropertyCallbackInfo;
using v8::Set;
using v8::SideEffectType;
using v8::String;
using v8::Value;

bool NativeModuleEnv::has_code_cache_ = false;

bool NativeModuleEnv::Add(const char* id, const UnionBytes& source) {
  return NativeModuleLoader::GetInstance()->Add(id, source);
}

bool NativeModuleEnv::Exists(const char* id) {
  return NativeModuleLoader::GetInstance()->Exists(id);
}

Local<Object> NativeModuleEnv::GetSourceObject(Local<Context> context) {
  return NativeModuleLoader::GetInstance()->GetSourceObject(context);
}

Local<String> NativeModuleEnv::GetConfigString(Isolate* isolate) {
  return NativeModuleLoader::GetInstance()->GetConfigString(isolate);
}

bool NativeModuleEnv::CompileAllModules(Local<Context> context) {
  NativeModuleLoader* loader = NativeModuleLoader::GetInstance();
  std::vector<std::string> ids = loader->GetModuleIds();
  bool all_succeeded = true;
  for (const auto& id : ids) {
    // TODO(joyeecheung): compile non-module scripts here too.
    if (!loader->CanBeRequired(id.c_str())) {
      continue;
    }
    v8::TryCatch bootstrapCatch(context->GetIsolate());
    native_module::NativeModuleLoader::Result result;
    USE(loader->CompileAsModule(context, id.c_str(), &result));
    if (bootstrapCatch.HasCaught()) {
      per_process::Debug(DebugCategory::CODE_CACHE,
                         "Failed to compile code cache for %s\n",
                         id.c_str());
      all_succeeded = false;
      PrintCaughtException(context->GetIsolate(), context, bootstrapCatch);
    }
  }
  return all_succeeded;
}

void NativeModuleEnv::CopyCodeCache(std::vector<CodeCacheInfo>* out) {
  NativeModuleLoader* loader = NativeModuleLoader::GetInstance();
  Mutex::ScopedLock lock(loader->code_cache_mutex());
  auto in = loader->code_cache();
  for (auto const& item : *in) {
    out->push_back(
        {item.first,
         {item.second->data, item.second->data + item.second->length}});
  }
}

void NativeModuleEnv::RefreshCodeCache(const std::vector<CodeCacheInfo>& in) {
  NativeModuleLoader* loader = NativeModuleLoader::GetInstance();
  Mutex::ScopedLock lock(loader->code_cache_mutex());
  auto out = loader->code_cache();
  for (auto const& item : in) {
    size_t length = item.data.size();
    uint8_t* buffer = new uint8_t[length];
    memcpy(buffer, item.data.data(), length);
    auto new_cache = std::make_unique<v8::ScriptCompiler::CachedData>(
        buffer, length, v8::ScriptCompiler::CachedData::BufferOwned);
    auto cache_it = out->find(item.id);
    if (cache_it != out->end()) {
      // Release the old cache and replace it with the new copy.
      cache_it->second.reset(new_cache.release());
    } else {
      out->emplace(item.id, new_cache.release());
    }
  }
  NativeModuleEnv::has_code_cache_ = true;
}

void NativeModuleEnv::GetModuleCategories(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Object> result = Object::New(isolate);

  // Copy from the per-process categories
  std::set<std::string> cannot_be_required =
      NativeModuleLoader::GetInstance()->GetCannotBeRequired();
  std::set<std::string> can_be_required =
      NativeModuleLoader::GetInstance()->GetCanBeRequired();

  if (!env->owns_process_state()) {
    can_be_required.erase("trace_events");
    cannot_be_required.insert("trace_events");
  }

  Local<Value> cannot_be_required_js;
  Local<Value> can_be_required_js;

  if (!ToV8Value(context, cannot_be_required).ToLocal(&cannot_be_required_js))
    return;
  if (result
      ->Set(context,
            OneByteString(isolate, "cannotBeRequired"),
            cannot_be_required_js)
      .IsNothing())
    return;
  if (!ToV8Value(context, can_be_required).ToLocal(&can_be_required_js))
    return;
  if (result
      ->Set(context,
            OneByteString(isolate, "canBeRequired"),
            can_be_required_js)
      .IsNothing()) {
    return;
  }
  info.GetReturnValue().Set(result);
}

void NativeModuleEnv::GetCacheUsage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Object> result = Object::New(isolate);

  Local<Value> native_modules_with_cache_js;
  Local<Value> native_modules_without_cache_js;
  Local<Value> native_modules_in_snapshot_js;
  if (!ToV8Value(context, env->native_modules_with_cache)
      .ToLocal(&native_modules_with_cache_js)) {
    return;
  }
  if (result
      ->Set(env->context(),
            OneByteString(isolate, "compiledWithCache"),
            native_modules_with_cache_js)
      .IsNothing()) {
    return;
  }

  if (!ToV8Value(context, env->native_modules_without_cache)
      .ToLocal(&native_modules_without_cache_js)) {
    return;
  }
  if (result
      ->Set(env->context(),
            OneByteString(isolate, "compiledWithoutCache"),
            native_modules_without_cache_js)
      .IsNothing()) {
    return;
  }

  if (!ToV8Value(context, env->native_modules_in_snapshot)
      .ToLocal(&native_modules_without_cache_js)) {
    return;
  }
  if (result
      ->Set(env->context(),
            OneByteString(isolate, "compiledInSnapshot"),
            native_modules_without_cache_js)
      .IsNothing()) {
    return;
  }

  args.GetReturnValue().Set(result);
}

void NativeModuleEnv::ModuleIdsGetter(Local<Name> property,
                                      const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();

  std::vector<std::string> ids =
      NativeModuleLoader::GetInstance()->GetModuleIds();
  info.GetReturnValue().Set(
      ToV8Value(isolate->GetCurrentContext(), ids).ToLocalChecked());
}

void NativeModuleEnv::ConfigStringGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(GetConfigString(info.GetIsolate()));
}

void NativeModuleEnv::RecordResult(const char* id,
                                   NativeModuleLoader::Result result,
                                   Environment* env) {
  if (result == NativeModuleLoader::Result::kWithCache) {
    env->native_modules_with_cache.insert(id);
  } else {
    env->native_modules_without_cache.insert(id);
  }
}
void NativeModuleEnv::CompileFunction(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  node::Utf8Value id_v(env->isolate(), args[0].As<String>());
  const char* id = *id_v;
  NativeModuleLoader::Result result;
  MaybeLocal<Function> maybe =
      NativeModuleLoader::GetInstance()->CompileAsModule(
          env->context(), id, &result);
  RecordResult(id, result, env);
  Local<Function> fn;
  if (maybe.ToLocal(&fn)) {
    args.GetReturnValue().Set(fn);
  }
}

// Returns Local<Function> of the compiled module if return_code_cache
// is false (we are only compiling the function).
// Otherwise return a Local<Object> containing the cache.
MaybeLocal<Function> NativeModuleEnv::LookupAndCompile(
    Local<Context> context,
    const char* id,
    std::vector<Local<String>>* parameters,
    Environment* optional_env) {
  NativeModuleLoader::Result result;
  MaybeLocal<Function> maybe =
      NativeModuleLoader::GetInstance()->LookupAndCompile(
          context, id, parameters, &result);
  if (optional_env != nullptr) {
    RecordResult(id, result, optional_env);
  }
  return maybe;
}

void NativeModuleEnv::HasCachedBuiltins(
    const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
      v8::Boolean::New(args.GetIsolate(), NativeModuleEnv::has_code_cache_));
}

// TODO(joyeecheung): It is somewhat confusing that Class::Initialize
// is used to initialize to the binding, but it is the current convention.
// Rename this across the code base to something that makes more sense.
void NativeModuleEnv::Initialize(Local<Object> target,
                                 Local<Value> unused,
                                 Local<Context> context,
                                 void* priv) {
  Environment* env = Environment::GetCurrent(context);

  target
      ->SetAccessor(env->context(),
                    env->config_string(),
                    ConfigStringGetter,
                    nullptr,
                    MaybeLocal<Value>(),
                    DEFAULT,
                    None,
                    SideEffectType::kHasNoSideEffect)
      .Check();
  target
      ->SetAccessor(env->context(),
                    FIXED_ONE_BYTE_STRING(env->isolate(), "moduleIds"),
                    ModuleIdsGetter,
                    nullptr,
                    MaybeLocal<Value>(),
                    DEFAULT,
                    None,
                    SideEffectType::kHasNoSideEffect)
      .Check();

  target
      ->SetAccessor(env->context(),
                    FIXED_ONE_BYTE_STRING(env->isolate(), "moduleCategories"),
                    GetModuleCategories,
                    nullptr,
                    Local<Value>(),
                    DEFAULT,
                    None,
                    SideEffectType::kHasNoSideEffect)
      .Check();

  env->SetMethod(target, "getCacheUsage", NativeModuleEnv::GetCacheUsage);
  env->SetMethod(target, "compileFunction", NativeModuleEnv::CompileFunction);
  env->SetMethod(target, "hasCachedBuiltins", HasCachedBuiltins);
  // internalBinding('native_module') should be frozen
  target->SetIntegrityLevel(context, IntegrityLevel::kFrozen).FromJust();
}

void NativeModuleEnv::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(ConfigStringGetter);
  registry->Register(ModuleIdsGetter);
  registry->Register(GetModuleCategories);
  registry->Register(GetCacheUsage);
  registry->Register(CompileFunction);
  registry->Register(HasCachedBuiltins);
}

}  // namespace native_module
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(
    native_module, node::native_module::NativeModuleEnv::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(
    native_module,
    node::native_module::NativeModuleEnv::RegisterExternalReferences)
