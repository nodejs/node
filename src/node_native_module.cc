#include "node_native_module.h"
#include "node_errors.h"

namespace node {

namespace per_process {
native_module::NativeModuleLoader native_module_loader;
}  // namespace per_process

namespace native_module {

using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::DEFAULT;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Integer;
using v8::IntegrityLevel;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::None;
using v8::Object;
using v8::PropertyCallbackInfo;
using v8::Script;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::Set;
using v8::SideEffectType;
using v8::String;
using v8::Uint8Array;
using v8::Value;

void NativeModuleLoader::InitializeModuleCategories() {
  if (module_categories_.is_initialized) {
    DCHECK(!module_categories_.can_be_required.empty());
    return;
  }

  std::vector<std::string> prefixes = {
#if !HAVE_OPENSSL
    "internal/crypto/",
#endif  // !HAVE_OPENSSL

    "internal/bootstrap/",
    "internal/per_context/",
    "internal/deps/",
    "internal/main/"
  };

  module_categories_.cannot_be_required = std::set<std::string> {
#if !HAVE_INSPECTOR
      "inspector",
      "internal/util/inspector",
#endif  // !HAVE_INSPECTOR

#if !NODE_USE_V8_PLATFORM || !defined(NODE_HAVE_I18N_SUPPORT)
      "trace_events",
#endif  // !NODE_USE_V8_PLATFORM

#if !HAVE_OPENSSL
      "crypto",
      "https",
      "http2",
      "tls",
      "_tls_common",
      "_tls_wrap",
      "internal/http2/core",
      "internal/http2/compat",
      "internal/policy/manifest",
      "internal/process/policy",
      "internal/streams/lazy_transform",
#endif  // !HAVE_OPENSSL

      "sys",  // Deprecated.
      "internal/test/binding",
      "internal/v8_prof_polyfill",
      "internal/v8_prof_processor",
  };

  for (auto const& x : source_) {
    const std::string& id = x.first;
    for (auto const& prefix : prefixes) {
      if (prefix.length() > id.length()) {
        continue;
      }
      if (id.find(prefix) == 0) {
        module_categories_.cannot_be_required.emplace(id);
      }
    }
  }

  for (auto const& x : source_) {
    const std::string& id = x.first;
    if (0 == module_categories_.cannot_be_required.count(id)) {
      module_categories_.can_be_required.emplace(id);
    }
  }

  module_categories_.is_initialized = true;
}

// TODO(joyeecheung): make these more general and put them into util.h
Local<Object> MapToObject(Local<Context> context,
                          const NativeModuleRecordMap& in) {
  Isolate* isolate = context->GetIsolate();
  Local<Object> out = Object::New(isolate);
  for (auto const& x : in) {
    Local<String> key = OneByteString(isolate, x.first.c_str(), x.first.size());
    out->Set(context, key, x.second.ToStringChecked(isolate)).FromJust();
  }
  return out;
}

Local<Set> ToJsSet(Local<Context> context,
                   const std::set<std::string>& in) {
  Isolate* isolate = context->GetIsolate();
  Local<Set> out = Set::New(isolate);
  for (auto const& x : in) {
    out->Add(context, OneByteString(isolate, x.c_str(), x.size()))
        .ToLocalChecked();
  }
  return out;
}

bool NativeModuleLoader::Exists(const char* id) {
  return source_.find(id) != source_.end();
}

void NativeModuleLoader::GetModuleCategories(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  per_process::native_module_loader.InitializeModuleCategories();

  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Object> result = Object::New(isolate);

  // Copy from the per-process categories
  std::set<std::string> cannot_be_required =
      per_process::native_module_loader.module_categories_.cannot_be_required;
  std::set<std::string> can_be_required =
      per_process::native_module_loader.module_categories_.can_be_required;

  if (!env->owns_process_state()) {
    can_be_required.erase("trace_events");
    cannot_be_required.insert("trace_events");
  }

  result
      ->Set(context,
            OneByteString(isolate, "cannotBeRequired"),
            ToJsSet(context, cannot_be_required))
      .FromJust();
  result
      ->Set(context,
            OneByteString(isolate, "canBeRequired"),
            ToJsSet(context, can_be_required))
      .FromJust();
  info.GetReturnValue().Set(result);
}

void NativeModuleLoader::GetCacheUsage(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Object> result = Object::New(isolate);
  result
      ->Set(env->context(),
            OneByteString(isolate, "compiledWithCache"),
            ToJsSet(context, env->native_modules_with_cache))
      .FromJust();
  result
      ->Set(env->context(),
            OneByteString(isolate, "compiledWithoutCache"),
            ToJsSet(context, env->native_modules_without_cache))
      .FromJust();
  args.GetReturnValue().Set(result);
}

void NativeModuleLoader::ModuleIdsGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();

  const NativeModuleRecordMap& source_ =
      per_process::native_module_loader.source_;
  std::vector<Local<Value>> ids;
  ids.reserve(source_.size());

  for (auto const& x : source_) {
    ids.emplace_back(OneByteString(isolate, x.first.c_str(), x.first.size()));
  }

  info.GetReturnValue().Set(Array::New(isolate, ids.data(), ids.size()));
}

void NativeModuleLoader::ConfigStringGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(
      per_process::native_module_loader.GetConfigString(info.GetIsolate()));
}

Local<Object> NativeModuleLoader::GetSourceObject(
    Local<Context> context) const {
  return MapToObject(context, source_);
}

Local<String> NativeModuleLoader::GetConfigString(Isolate* isolate) const {
  return config_.ToStringChecked(isolate);
}

NativeModuleLoader::NativeModuleLoader() : config_(GetConfig()) {
  LoadJavaScriptSource();
  LoadCodeCache();
}

// This is supposed to be run only by the main thread in
// tools/generate_code_cache.js
void NativeModuleLoader::GetCodeCache(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  CHECK(env->is_main_thread());

  CHECK(args[0]->IsString());
  node::Utf8Value id_v(isolate, args[0].As<String>());
  const char* id = *id_v;

  const NativeModuleLoader& loader = per_process::native_module_loader;
  MaybeLocal<Uint8Array> ret = loader.GetCodeCache(isolate, id);
  if (!ret.IsEmpty()) {
    args.GetReturnValue().Set(ret.ToLocalChecked());
  }
}

// This is supposed to be run only by the main thread in
// tools/generate_code_cache.js
MaybeLocal<Uint8Array> NativeModuleLoader::GetCodeCache(Isolate* isolate,
                                                        const char* id) const {
  EscapableHandleScope scope(isolate);
  Mutex::ScopedLock lock(code_cache_mutex_);

  ScriptCompiler::CachedData* cached_data = nullptr;
  const auto it = code_cache_.find(id);
  if (it == code_cache_.end()) {
    // The module has not been compiled before.
    return MaybeLocal<Uint8Array>();
  }

  cached_data = it->second.get();

  Local<ArrayBuffer> buf = ArrayBuffer::New(isolate, cached_data->length);
  memcpy(buf->GetContents().Data(), cached_data->data, cached_data->length);
  return scope.Escape(Uint8Array::New(buf, 0, cached_data->length));
}

void NativeModuleLoader::CompileFunction(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  node::Utf8Value id(env->isolate(), args[0].As<String>());

  MaybeLocal<Function> result = CompileAsModule(env, *id);
  if (!result.IsEmpty()) {
    args.GetReturnValue().Set(result.ToLocalChecked());
  }
}

MaybeLocal<Function> NativeModuleLoader::CompileAsModule(Environment* env,
                                                         const char* id) {
  std::vector<Local<String>> parameters = {env->exports_string(),
                                           env->require_string(),
                                           env->module_string(),
                                           env->process_string(),
                                           env->internal_binding_string(),
                                           env->primordials_string()};
  return per_process::native_module_loader.LookupAndCompile(
      env->context(), id, &parameters, env);
}

// Returns Local<Function> of the compiled module if return_code_cache
// is false (we are only compiling the function).
// Otherwise return a Local<Object> containing the cache.
MaybeLocal<Function> NativeModuleLoader::LookupAndCompile(
    Local<Context> context,
    const char* id,
    std::vector<Local<String>>* parameters,
    Environment* optional_env) {
  Isolate* isolate = context->GetIsolate();
  EscapableHandleScope scope(isolate);

  const auto source_it = source_.find(id);
  CHECK_NE(source_it, source_.end());
  Local<String> source = source_it->second.ToStringChecked(isolate);

  std::string filename_s = id + std::string(".js");
  Local<String> filename =
      OneByteString(isolate, filename_s.c_str(), filename_s.size());
  Local<Integer> line_offset = Integer::New(isolate, 0);
  Local<Integer> column_offset = Integer::New(isolate, 0);
  ScriptOrigin origin(filename, line_offset, column_offset, True(isolate));

  Mutex::ScopedLock lock(code_cache_mutex_);

  ScriptCompiler::CachedData* cached_data = nullptr;
  {
    auto cache_it = code_cache_.find(id);
    if (cache_it != code_cache_.end()) {
      // Transfer ownership to ScriptCompiler::Source later.
      cached_data = cache_it->second.release();
      code_cache_.erase(cache_it);
    }
  }

  const bool use_cache = cached_data != nullptr;
  ScriptCompiler::CompileOptions options =
      use_cache ? ScriptCompiler::kConsumeCodeCache
                : ScriptCompiler::kEagerCompile;
  ScriptCompiler::Source script_source(source, origin, cached_data);

  MaybeLocal<Function> maybe_fun =
      ScriptCompiler::CompileFunctionInContext(context,
                                               &script_source,
                                               parameters->size(),
                                               parameters->data(),
                                               0,
                                               nullptr,
                                               options);

  // This could fail when there are early errors in the native modules,
  // e.g. the syntax errors
  if (maybe_fun.IsEmpty()) {
    // In the case of early errors, v8 is already capable of
    // decorating the stack for us - note that we use CompileFunctionInContext
    // so there is no need to worry about wrappers.
    return MaybeLocal<Function>();
  }

  Local<Function> fun = maybe_fun.ToLocalChecked();
  // XXX(joyeecheung): this bookkeeping is not exactly accurate because
  // it only starts after the Environment is created, so the per_context.js
  // will never be in any of these two sets, but the two sets are only for
  // testing anyway.
  if (use_cache) {
    if (optional_env != nullptr) {
      // This could happen when Node is run with any v8 flag, but
      // the cache is not generated with one
      if (script_source.GetCachedData()->rejected) {
        optional_env->native_modules_without_cache.insert(id);
      } else {
        optional_env->native_modules_with_cache.insert(id);
      }
    }
  } else {
    if (optional_env != nullptr) {
      optional_env->native_modules_without_cache.insert(id);
    }
  }

  // Generate new cache for next compilation
  std::unique_ptr<ScriptCompiler::CachedData> new_cached_data(
      ScriptCompiler::CreateCodeCacheForFunction(fun));
  CHECK_NOT_NULL(new_cached_data);

  // The old entry should've been erased by now so we can just emplace
  code_cache_.emplace(id, std::move(new_cached_data));

  return scope.Escape(fun);
}

void NativeModuleLoader::Initialize(Local<Object> target,
                                    Local<Value> unused,
                                    Local<Context> context,
                                    void* priv) {
  Environment* env = Environment::GetCurrent(context);

  CHECK(target
            ->SetAccessor(env->context(),
                          env->config_string(),
                          ConfigStringGetter,
                          nullptr,
                          MaybeLocal<Value>(),
                          DEFAULT,
                          None,
                          SideEffectType::kHasNoSideEffect)
            .FromJust());
  CHECK(target
            ->SetAccessor(env->context(),
                          FIXED_ONE_BYTE_STRING(env->isolate(), "moduleIds"),
                          ModuleIdsGetter,
                          nullptr,
                          MaybeLocal<Value>(),
                          DEFAULT,
                          None,
                          SideEffectType::kHasNoSideEffect)
            .FromJust());

  CHECK(target
            ->SetAccessor(
                env->context(),
                FIXED_ONE_BYTE_STRING(env->isolate(), "moduleCategories"),
                GetModuleCategories,
                nullptr,
                env->as_callback_data(),
                DEFAULT,
                None,
                SideEffectType::kHasNoSideEffect)
            .FromJust());

  env->SetMethod(
      target, "getCacheUsage", NativeModuleLoader::GetCacheUsage);
  env->SetMethod(
      target, "compileFunction", NativeModuleLoader::CompileFunction);
  env->SetMethod(target, "getCodeCache", NativeModuleLoader::GetCodeCache);
  // internalBinding('native_module') should be frozen
  target->SetIntegrityLevel(context, IntegrityLevel::kFrozen).FromJust();
}

}  // namespace native_module
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(
    native_module, node::native_module::NativeModuleLoader::Initialize)
