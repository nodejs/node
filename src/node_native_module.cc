#include "node_native_module.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "util-inl.h"

namespace node {
namespace native_module {

using v8::Context;
using v8::DEFAULT;
using v8::EscapableHandleScope;
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
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::Set;
using v8::SideEffectType;
using v8::String;
using v8::Value;

NativeModuleLoader NativeModuleLoader::instance_;

NativeModuleLoader::NativeModuleLoader()
    : config_(GetConfig()), has_code_cache_(false) {
  LoadJavaScriptSource();
}

NativeModuleLoader* NativeModuleLoader::GetInstance() {
  return &instance_;
}

bool NativeModuleLoader::Exists(const char* id) {
  auto& source = GetInstance()->source_;
  return source.find(id) != source.end();
}

bool NativeModuleLoader::Add(const char* id, const UnionBytes& source) {
  auto result = GetInstance()->source_.emplace(id, source);
  return result.second;
}

Local<Object> NativeModuleLoader::GetSourceObject(Local<Context> context) {
  Isolate* isolate = context->GetIsolate();
  Local<Object> out = Object::New(isolate);
  auto& source = GetInstance()->source_;
  for (auto const& x : source) {
    Local<String> key = OneByteString(isolate, x.first.c_str(), x.first.size());
    out->Set(context, key, x.second.ToStringChecked(isolate)).FromJust();
  }
  return out;
}

Local<String> NativeModuleLoader::GetConfigString(Isolate* isolate) {
  return GetInstance()->config_.ToStringChecked(isolate);
}

std::vector<std::string> NativeModuleLoader::GetModuleIds() {
  std::vector<std::string> ids;
  ids.reserve(source_.size());
  for (auto const& x : source_) {
    ids.emplace_back(x.first);
  }
  return ids;
}

void NativeModuleLoader::InitializeModuleCategories() {
  if (module_categories_.is_initialized) {
    DCHECK(!module_categories_.can_be_required.empty());
    return;
  }

  std::vector<std::string> prefixes = {
#if !HAVE_OPENSSL
    "internal/crypto/",
    "internal/debugger/",
#endif  // !HAVE_OPENSSL

    "internal/bootstrap/",
    "internal/per_context/",
    "internal/deps/",
    "internal/main/"
  };

  module_categories_.can_be_required.emplace(
      "internal/deps/cjs-module-lexer/lexer");

  module_categories_.cannot_be_required = std::set<std::string> {
#if !HAVE_INSPECTOR
      "inspector",
      "internal/util/inspector",
#endif  // !HAVE_INSPECTOR

#if !NODE_USE_V8_PLATFORM || !defined(NODE_HAVE_I18N_SUPPORT)
      "trace_events",
#endif  // !NODE_USE_V8_PLATFORM || !defined(NODE_HAVE_I18N_SUPPORT)

#if !HAVE_OPENSSL
      "crypto",
      "crypto/promises",
      "https",
      "http2",
      "tls",
      "_tls_common",
      "_tls_wrap",
      "internal/tls/secure-pair",
      "internal/tls/parse-cert-string",
      "internal/tls/secure-context",
      "internal/http2/core",
      "internal/http2/compat",
      "internal/policy/manifest",
      "internal/process/policy",
      "internal/streams/lazy_transform",
#endif  // !HAVE_OPENSSL
      "sys",  // Deprecated.
      "wasi",  // Experimental.
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
      if (id.find(prefix) == 0 &&
          module_categories_.can_be_required.count(id) == 0) {
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

const std::set<std::string>& NativeModuleLoader::GetCannotBeRequired() {
  InitializeModuleCategories();
  return module_categories_.cannot_be_required;
}

const std::set<std::string>& NativeModuleLoader::GetCanBeRequired() {
  InitializeModuleCategories();
  return module_categories_.can_be_required;
}

bool NativeModuleLoader::CanBeRequired(const char* id) {
  return GetCanBeRequired().count(id) == 1;
}

bool NativeModuleLoader::CannotBeRequired(const char* id) {
  return GetCannotBeRequired().count(id) == 1;
}

NativeModuleCacheMap* NativeModuleLoader::code_cache() {
  return &code_cache_;
}

ScriptCompiler::CachedData* NativeModuleLoader::GetCodeCache(
    const char* id) const {
  Mutex::ScopedLock lock(code_cache_mutex_);
  const auto it = code_cache_.find(id);
  if (it == code_cache_.end()) {
    // The module has not been compiled before.
    return nullptr;
  }
  return it->second.get();
}

#ifdef NODE_BUILTIN_MODULES_PATH
static std::string OnDiskFileName(const char* id) {
  std::string filename = NODE_BUILTIN_MODULES_PATH;
  filename += "/";

  if (strncmp(id, "internal/deps", strlen("internal/deps")) == 0) {
    id += strlen("internal/");
  } else {
    filename += "lib/";
  }
  filename += id;
  filename += ".js";

  return filename;
}
#endif  // NODE_BUILTIN_MODULES_PATH

MaybeLocal<String> NativeModuleLoader::LoadBuiltinModuleSource(Isolate* isolate,
                                                               const char* id) {
#ifdef NODE_BUILTIN_MODULES_PATH
  if (strncmp(id, "embedder_main_", strlen("embedder_main_")) == 0) {
#endif  // NODE_BUILTIN_MODULES_PATH
    const auto source_it = source_.find(id);
    if (UNLIKELY(source_it == source_.end())) {
      fprintf(stderr, "Cannot find native builtin: \"%s\".\n", id);
      ABORT();
    }
    return source_it->second.ToStringChecked(isolate);
#ifdef NODE_BUILTIN_MODULES_PATH
  }
  std::string filename = OnDiskFileName(id);

  std::string contents;
  int r = ReadFileSync(&contents, filename.c_str());
  if (r != 0) {
    const std::string buf = SPrintF("Cannot read local builtin. %s: %s \"%s\"",
                                    uv_err_name(r),
                                    uv_strerror(r),
                                    filename);
    Local<String> message = OneByteString(isolate, buf.c_str());
    isolate->ThrowException(v8::Exception::Error(message));
    return MaybeLocal<String>();
  }
  return String::NewFromUtf8(
      isolate, contents.c_str(), v8::NewStringType::kNormal, contents.length());
#endif  // NODE_BUILTIN_MODULES_PATH
}

// Returns Local<Function> of the compiled module if return_code_cache
// is false (we are only compiling the function).
// Otherwise return a Local<Object> containing the cache.
MaybeLocal<Function> NativeModuleLoader::LookupAndCompileInternal(
    Local<Context> context,
    const char* id,
    std::vector<Local<String>>* parameters,
    NativeModuleLoader::Result* result) {
  Isolate* isolate = context->GetIsolate();
  EscapableHandleScope scope(isolate);

  Local<String> source;
  if (!LoadBuiltinModuleSource(isolate, id).ToLocal(&source)) {
    return {};
  }

  std::string filename_s = std::string("node:") + id;
  Local<String> filename =
      OneByteString(isolate, filename_s.c_str(), filename_s.size());
  ScriptOrigin origin(isolate, filename, 0, 0, true);

  ScriptCompiler::CachedData* cached_data = nullptr;
  {
    // Note: The lock here should not extend into the
    // `CompileFunctionInContext()` call below, because this function may
    // recurse if there is a syntax error during bootstrap (because the fatal
    // exception handler is invoked, which may load built-in modules).
    Mutex::ScopedLock lock(code_cache_mutex_);
    auto cache_it = code_cache_.find(id);
    if (cache_it != code_cache_.end()) {
      // Transfer ownership to ScriptCompiler::Source later.
      cached_data = cache_it->second.release();
      code_cache_.erase(cache_it);
    }
  }

  const bool has_cache = cached_data != nullptr;
  ScriptCompiler::CompileOptions options =
      has_cache ? ScriptCompiler::kConsumeCodeCache
                : ScriptCompiler::kEagerCompile;
  ScriptCompiler::Source script_source(source, origin, cached_data);

  per_process::Debug(DebugCategory::CODE_CACHE,
                     "Compiling %s %s code cache\n",
                     id,
                     has_cache ? "with" : "without");

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
  Local<Function> fun;
  if (!maybe_fun.ToLocal(&fun)) {
    // In the case of early errors, v8 is already capable of
    // decorating the stack for us - note that we use CompileFunctionInContext
    // so there is no need to worry about wrappers.
    return MaybeLocal<Function>();
  }

  // XXX(joyeecheung): this bookkeeping is not exactly accurate because
  // it only starts after the Environment is created, so the per_context.js
  // will never be in any of these two sets, but the two sets are only for
  // testing anyway.

  *result = (has_cache && !script_source.GetCachedData()->rejected)
                ? Result::kWithCache
                : Result::kWithoutCache;

  if (has_cache) {
    per_process::Debug(DebugCategory::CODE_CACHE,
                       "Code cache of %s (%s) %s\n",
                       id,
                       script_source.GetCachedData()->buffer_policy ==
                               ScriptCompiler::CachedData::BufferNotOwned
                           ? "BufferNotOwned"
                           : "BufferOwned",
                       script_source.GetCachedData()->rejected ? "is rejected"
                                                               : "is accepted");
  }

  // Generate new cache for next compilation
  std::unique_ptr<ScriptCompiler::CachedData> new_cached_data(
      ScriptCompiler::CreateCodeCacheForFunction(fun));
  CHECK_NOT_NULL(new_cached_data);

  {
    Mutex::ScopedLock lock(code_cache_mutex_);
    const auto it = code_cache_.find(id);
    // TODO(joyeecheung): it's safer for each thread to have its own
    // copy of the code cache map.
    if (it == code_cache_.end()) {
      code_cache_.emplace(id, std::move(new_cached_data));
    } else {
      it->second.reset(new_cached_data.release());
    }
  }

  return scope.Escape(fun);
}

// Returns Local<Function> of the compiled module if return_code_cache
// is false (we are only compiling the function).
// Otherwise return a Local<Object> containing the cache.
MaybeLocal<Function> NativeModuleLoader::LookupAndCompile(
    Local<Context> context,
    const char* id,
    Environment* optional_env) {
  Result result;
  std::vector<Local<String>> parameters;
  Isolate* isolate = context->GetIsolate();
  // Detects parameters of the scripts based on module ids.
  // internal/bootstrap/loaders: process, getLinkedBinding,
  //                             getInternalBinding, primordials
  if (strcmp(id, "internal/bootstrap/loaders") == 0) {
    parameters = {
        FIXED_ONE_BYTE_STRING(isolate, "process"),
        FIXED_ONE_BYTE_STRING(isolate, "getLinkedBinding"),
        FIXED_ONE_BYTE_STRING(isolate, "getInternalBinding"),
        FIXED_ONE_BYTE_STRING(isolate, "primordials"),
    };
  } else if (strncmp(id,
                     "internal/per_context/",
                     strlen("internal/per_context/")) == 0) {
    // internal/per_context/*: global, exports, primordials
    parameters = {
        FIXED_ONE_BYTE_STRING(isolate, "global"),
        FIXED_ONE_BYTE_STRING(isolate, "exports"),
        FIXED_ONE_BYTE_STRING(isolate, "primordials"),
    };
  } else if (strncmp(id, "internal/main/", strlen("internal/main/")) == 0) {
    // internal/main/*: process, require, internalBinding, primordials
    parameters = {
        FIXED_ONE_BYTE_STRING(isolate, "process"),
        FIXED_ONE_BYTE_STRING(isolate, "require"),
        FIXED_ONE_BYTE_STRING(isolate, "internalBinding"),
        FIXED_ONE_BYTE_STRING(isolate, "primordials"),
    };
  } else if (strncmp(id, "embedder_main_", strlen("embedder_main_")) == 0) {
    // Synthetic embedder main scripts from LoadEnvironment(): process, require
    parameters = {
        FIXED_ONE_BYTE_STRING(isolate, "process"),
        FIXED_ONE_BYTE_STRING(isolate, "require"),
    };
  } else if (strncmp(id,
                     "internal/bootstrap/",
                     strlen("internal/bootstrap/")) == 0) {
    // internal/bootstrap/*: process, require, internalBinding, primordials
    parameters = {
        FIXED_ONE_BYTE_STRING(isolate, "process"),
        FIXED_ONE_BYTE_STRING(isolate, "require"),
        FIXED_ONE_BYTE_STRING(isolate, "internalBinding"),
        FIXED_ONE_BYTE_STRING(isolate, "primordials"),
    };
  } else {
    // others: exports, require, module, process, internalBinding, primordials
    parameters = {
        FIXED_ONE_BYTE_STRING(isolate, "exports"),
        FIXED_ONE_BYTE_STRING(isolate, "require"),
        FIXED_ONE_BYTE_STRING(isolate, "module"),
        FIXED_ONE_BYTE_STRING(isolate, "process"),
        FIXED_ONE_BYTE_STRING(isolate, "internalBinding"),
        FIXED_ONE_BYTE_STRING(isolate, "primordials"),
    };
  }

  MaybeLocal<Function> maybe = GetInstance()->LookupAndCompileInternal(
      context, id, &parameters, &result);
  if (optional_env != nullptr) {
    RecordResult(id, result, optional_env);
  }
  return maybe;
}

bool NativeModuleLoader::CompileAllBuiltins(Local<Context> context) {
  NativeModuleLoader* loader = GetInstance();
  std::vector<std::string> ids = loader->GetModuleIds();
  bool all_succeeded = true;
  std::string v8_tools_prefix = "internal/deps/v8/tools/";
  for (const auto& id : ids) {
    if (id.compare(0, v8_tools_prefix.size(), v8_tools_prefix) == 0) {
      continue;
    }
    v8::TryCatch bootstrapCatch(context->GetIsolate());
    USE(loader->LookupAndCompile(context, id.c_str(), nullptr));
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

void NativeModuleLoader::CopyCodeCache(std::vector<CodeCacheInfo>* out) {
  NativeModuleLoader* loader = GetInstance();
  Mutex::ScopedLock lock(loader->code_cache_mutex());
  auto in = loader->code_cache();
  for (auto const& item : *in) {
    out->push_back(
        {item.first,
         {item.second->data, item.second->data + item.second->length}});
  }
}

void NativeModuleLoader::RefreshCodeCache(
    const std::vector<CodeCacheInfo>& in) {
  NativeModuleLoader* loader = GetInstance();
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
  loader->has_code_cache_ = true;
}

void NativeModuleLoader::GetModuleCategories(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Object> result = Object::New(isolate);

  // Copy from the per-process categories
  std::set<std::string> cannot_be_required =
      GetInstance()->GetCannotBeRequired();
  std::set<std::string> can_be_required = GetInstance()->GetCanBeRequired();

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
  if (!ToV8Value(context, can_be_required).ToLocal(&can_be_required_js)) return;
  if (result
          ->Set(context,
                OneByteString(isolate, "canBeRequired"),
                can_be_required_js)
          .IsNothing()) {
    return;
  }
  info.GetReturnValue().Set(result);
}

void NativeModuleLoader::GetCacheUsage(
    const FunctionCallbackInfo<Value>& args) {
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

void NativeModuleLoader::ModuleIdsGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();

  std::vector<std::string> ids = GetInstance()->GetModuleIds();
  info.GetReturnValue().Set(
      ToV8Value(isolate->GetCurrentContext(), ids).ToLocalChecked());
}

void NativeModuleLoader::ConfigStringGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(GetConfigString(info.GetIsolate()));
}

void NativeModuleLoader::RecordResult(const char* id,
                                      NativeModuleLoader::Result result,
                                      Environment* env) {
  if (result == NativeModuleLoader::Result::kWithCache) {
    env->native_modules_with_cache.insert(id);
  } else {
    env->native_modules_without_cache.insert(id);
  }
}

void NativeModuleLoader::CompileFunction(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  node::Utf8Value id_v(env->isolate(), args[0].As<String>());
  const char* id = *id_v;
  MaybeLocal<Function> maybe =
      GetInstance()->LookupAndCompile(env->context(), id, env);
  Local<Function> fn;
  if (maybe.ToLocal(&fn)) {
    args.GetReturnValue().Set(fn);
  }
}

void NativeModuleLoader::HasCachedBuiltins(
    const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
      v8::Boolean::New(args.GetIsolate(), GetInstance()->has_code_cache_));
}

// TODO(joyeecheung): It is somewhat confusing that Class::Initialize
// is used to initialize to the binding, but it is the current convention.
// Rename this across the code base to something that makes more sense.
void NativeModuleLoader::Initialize(Local<Object> target,
                                    Local<Value> unused,
                                    Local<Context> context,
                                    void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  target
      ->SetAccessor(context,
                    env->config_string(),
                    ConfigStringGetter,
                    nullptr,
                    MaybeLocal<Value>(),
                    DEFAULT,
                    None,
                    SideEffectType::kHasNoSideEffect)
      .Check();
  target
      ->SetAccessor(context,
                    FIXED_ONE_BYTE_STRING(isolate, "moduleIds"),
                    ModuleIdsGetter,
                    nullptr,
                    MaybeLocal<Value>(),
                    DEFAULT,
                    None,
                    SideEffectType::kHasNoSideEffect)
      .Check();

  target
      ->SetAccessor(context,
                    FIXED_ONE_BYTE_STRING(isolate, "moduleCategories"),
                    GetModuleCategories,
                    nullptr,
                    Local<Value>(),
                    DEFAULT,
                    None,
                    SideEffectType::kHasNoSideEffect)
      .Check();

  SetMethod(
      context, target, "getCacheUsage", NativeModuleLoader::GetCacheUsage);
  SetMethod(
      context, target, "compileFunction", NativeModuleLoader::CompileFunction);
  SetMethod(context, target, "hasCachedBuiltins", HasCachedBuiltins);
  // internalBinding('native_module') should be frozen
  target->SetIntegrityLevel(context, IntegrityLevel::kFrozen).FromJust();
}

void NativeModuleLoader::RegisterExternalReferences(
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
    native_module, node::native_module::NativeModuleLoader::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(
    native_module,
    node::native_module::NativeModuleLoader::RegisterExternalReferences)
