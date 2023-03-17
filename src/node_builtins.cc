#include "node_builtins.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_threadsafe_cow-inl.h"
#include "simdutf.h"
#include "util-inl.h"

namespace node {
namespace builtins {

using v8::Context;
using v8::DEFAULT;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::IntegrityLevel;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Name;
using v8::None;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyCallbackInfo;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::Set;
using v8::SideEffectType;
using v8::String;
using v8::Undefined;
using v8::Value;

BuiltinLoader::BuiltinLoader()
    : config_(GetConfig()), code_cache_(std::make_shared<BuiltinCodeCache>()) {
  LoadJavaScriptSource();
#ifdef NODE_SHARED_BUILTIN_CJS_MODULE_LEXER_LEXER_PATH
  AddExternalizedBuiltin(
      "internal/deps/cjs-module-lexer/lexer",
      STRINGIFY(NODE_SHARED_BUILTIN_CJS_MODULE_LEXER_LEXER_PATH));
#endif  // NODE_SHARED_BUILTIN_CJS_MODULE_LEXER_LEXER_PATH

#ifdef NODE_SHARED_BUILTIN_CJS_MODULE_LEXER_DIST_LEXER_PATH
  AddExternalizedBuiltin(
      "internal/deps/cjs-module-lexer/dist/lexer",
      STRINGIFY(NODE_SHARED_BUILTIN_CJS_MODULE_LEXER_DIST_LEXER_PATH));
#endif  // NODE_SHARED_BUILTIN_CJS_MODULE_LEXER_DIST_LEXER_PATH

#ifdef NODE_SHARED_BUILTIN_UNDICI_UNDICI_PATH
  AddExternalizedBuiltin("internal/deps/undici/undici",
                         STRINGIFY(NODE_SHARED_BUILTIN_UNDICI_UNDICI_PATH));
#endif  // NODE_SHARED_BUILTIN_UNDICI_UNDICI_PATH
}

bool BuiltinLoader::Exists(const char* id) {
  auto source = source_.read();
  return source->find(id) != source->end();
}

bool BuiltinLoader::Add(const char* id, const UnionBytes& source) {
  auto result = source_.write()->emplace(id, source);
  return result.second;
}

Local<Object> BuiltinLoader::GetSourceObject(Local<Context> context) {
  Isolate* isolate = context->GetIsolate();
  Local<Object> out = Object::New(isolate);
  auto source = source_.read();
  for (auto const& x : *source) {
    Local<String> key = OneByteString(isolate, x.first.c_str(), x.first.size());
    out->Set(context, key, x.second.ToStringChecked(isolate)).FromJust();
  }
  return out;
}

Local<String> BuiltinLoader::GetConfigString(Isolate* isolate) {
  return config_.ToStringChecked(isolate);
}

std::vector<std::string> BuiltinLoader::GetBuiltinIds() const {
  std::vector<std::string> ids;
  auto source = source_.read();
  ids.reserve(source->size());
  for (auto const& x : *source) {
    ids.emplace_back(x.first);
  }
  return ids;
}

BuiltinLoader::BuiltinCategories BuiltinLoader::GetBuiltinCategories() const {
  BuiltinCategories builtin_categories;

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

  builtin_categories.can_be_required.emplace(
      "internal/deps/cjs-module-lexer/lexer");

  builtin_categories.cannot_be_required = std::set<std::string> {
#if !HAVE_INSPECTOR
    "inspector", "inspector/promises", "internal/util/inspector",
#endif  // !HAVE_INSPECTOR

#if !NODE_USE_V8_PLATFORM || !defined(NODE_HAVE_I18N_SUPPORT)
        "trace_events",
#endif  // !NODE_USE_V8_PLATFORM || !defined(NODE_HAVE_I18N_SUPPORT)

#if !HAVE_OPENSSL
        "crypto", "crypto/promises", "https", "http2", "tls", "_tls_common",
        "_tls_wrap", "internal/tls/secure-pair",
        "internal/tls/parse-cert-string", "internal/tls/secure-context",
        "internal/http2/core", "internal/http2/compat",
        "internal/policy/manifest", "internal/process/policy",
        "internal/streams/lazy_transform",
#endif           // !HAVE_OPENSSL
        "sys",   // Deprecated.
        "wasi",  // Experimental.
        "internal/test/binding", "internal/v8_prof_polyfill",
        "internal/v8_prof_processor",
  };

  auto source = source_.read();
  for (auto const& x : *source) {
    const std::string& id = x.first;
    for (auto const& prefix : prefixes) {
      if (prefix.length() > id.length()) {
        continue;
      }
      if (id.find(prefix) == 0 &&
          builtin_categories.can_be_required.count(id) == 0) {
        builtin_categories.cannot_be_required.emplace(id);
      }
    }
  }

  for (auto const& x : *source) {
    const std::string& id = x.first;
    if (0 == builtin_categories.cannot_be_required.count(id)) {
      builtin_categories.can_be_required.emplace(id);
    }
  }

  return builtin_categories;
}

const ScriptCompiler::CachedData* BuiltinLoader::GetCodeCache(
    const char* id) const {
  RwLock::ScopedReadLock lock(code_cache_->mutex);
  const auto it = code_cache_->map.find(id);
  if (it == code_cache_->map.end()) {
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

MaybeLocal<String> BuiltinLoader::LoadBuiltinSource(Isolate* isolate,
                                                    const char* id) const {
  auto source = source_.read();
#ifdef NODE_BUILTIN_MODULES_PATH
  if (strncmp(id, "embedder_main_", strlen("embedder_main_")) == 0) {
#endif  // NODE_BUILTIN_MODULES_PATH
    const auto source_it = source->find(id);
    if (UNLIKELY(source_it == source->end())) {
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

namespace {
static Mutex externalized_builtins_mutex;
std::unordered_map<std::string, std::string> externalized_builtin_sources;
}  // namespace

void BuiltinLoader::AddExternalizedBuiltin(const char* id,
                                           const char* filename) {
  std::string source;
  {
    Mutex::ScopedLock lock(externalized_builtins_mutex);
    auto it = externalized_builtin_sources.find(id);
    if (it != externalized_builtin_sources.end()) {
      source = it->second;
    } else {
      int r = ReadFileSync(&source, filename);
      if (r != 0) {
        fprintf(stderr,
                "Cannot load externalized builtin: \"%s:%s\".\n",
                id,
                filename);
        ABORT();
      }
      externalized_builtin_sources[id] = source;
    }
  }

  Add(id, source);
}

bool BuiltinLoader::Add(const char* id, std::string_view utf8source) {
  size_t expected_u16_length =
      simdutf::utf16_length_from_utf8(utf8source.data(), utf8source.length());
  auto out = std::make_shared<std::vector<uint16_t>>(expected_u16_length);
  size_t u16_length =
      simdutf::convert_utf8_to_utf16(utf8source.data(),
                                     utf8source.length(),
                                     reinterpret_cast<char16_t*>(out->data()));
  out->resize(u16_length);
  return Add(id, UnionBytes(out));
}

MaybeLocal<Function> BuiltinLoader::LookupAndCompileInternal(
    Local<Context> context,
    const char* id,
    std::vector<Local<String>>* parameters,
    BuiltinLoader::Result* result) {
  Isolate* isolate = context->GetIsolate();
  EscapableHandleScope scope(isolate);

  Local<String> source;
  if (!LoadBuiltinSource(isolate, id).ToLocal(&source)) {
    return {};
  }

  std::string filename_s = std::string("node:") + id;
  Local<String> filename =
      OneByteString(isolate, filename_s.c_str(), filename_s.size());
  ScriptOrigin origin(isolate, filename, 0, 0, true);

  ScriptCompiler::CachedData* cached_data = nullptr;
  {
    // Note: The lock here should not extend into the
    // `CompileFunction()` call below, because this function may recurse if
    // there is a syntax error during bootstrap (because the fatal exception
    // handler is invoked, which may load built-in modules).
    RwLock::ScopedLock lock(code_cache_->mutex);
    auto cache_it = code_cache_->map.find(id);
    if (cache_it != code_cache_->map.end()) {
      // Transfer ownership to ScriptCompiler::Source later.
      cached_data = cache_it->second.release();
      code_cache_->map.erase(cache_it);
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
      ScriptCompiler::CompileFunction(context,
                                      &script_source,
                                      parameters->size(),
                                      parameters->data(),
                                      0,
                                      nullptr,
                                      options);

  // This could fail when there are early errors in the built-in modules,
  // e.g. the syntax errors
  Local<Function> fun;
  if (!maybe_fun.ToLocal(&fun)) {
    // In the case of early errors, v8 is already capable of
    // decorating the stack for us - note that we use CompileFunction
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
    RwLock::ScopedLock lock(code_cache_->mutex);
    code_cache_->map[id] = std::move(new_cached_data);
  }

  return scope.Escape(fun);
}

MaybeLocal<Function> BuiltinLoader::LookupAndCompile(Local<Context> context,
                                                     const char* id,
                                                     Realm* optional_realm) {
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
        FIXED_ONE_BYTE_STRING(isolate, "exports"),
        FIXED_ONE_BYTE_STRING(isolate, "primordials"),
    };
  } else if (strncmp(id, "internal/main/", strlen("internal/main/")) == 0 ||
             strncmp(id,
                     "internal/bootstrap/",
                     strlen("internal/bootstrap/")) == 0) {
    // internal/main/*, internal/bootstrap/*: process, require,
    //                                        internalBinding, primordials
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

  MaybeLocal<Function> maybe =
      LookupAndCompileInternal(context, id, &parameters, &result);
  if (optional_realm != nullptr) {
    DCHECK_EQ(this, optional_realm->env()->builtin_loader());
    RecordResult(id, result, optional_realm);
  }
  return maybe;
}

MaybeLocal<Value> BuiltinLoader::CompileAndCall(Local<Context> context,
                                                const char* id,
                                                Realm* realm) {
  Isolate* isolate = context->GetIsolate();
  // Arguments must match the parameters specified in
  // BuiltinLoader::LookupAndCompile().
  std::vector<Local<Value>> arguments;
  // Detects parameters of the scripts based on module ids.
  // internal/bootstrap/loaders: process, getLinkedBinding,
  //                             getInternalBinding, primordials
  if (strcmp(id, "internal/bootstrap/loaders") == 0) {
    Local<Value> get_linked_binding;
    Local<Value> get_internal_binding;
    if (!NewFunctionTemplate(isolate, binding::GetLinkedBinding)
             ->GetFunction(context)
             .ToLocal(&get_linked_binding) ||
        !NewFunctionTemplate(isolate, binding::GetInternalBinding)
             ->GetFunction(context)
             .ToLocal(&get_internal_binding)) {
      return MaybeLocal<Value>();
    }
    arguments = {realm->process_object(),
                 get_linked_binding,
                 get_internal_binding,
                 realm->primordials()};
  } else if (strncmp(id, "internal/main/", strlen("internal/main/")) == 0 ||
             strncmp(id,
                     "internal/bootstrap/",
                     strlen("internal/bootstrap/")) == 0) {
    // internal/main/*, internal/bootstrap/*: process, require,
    //                                        internalBinding, primordials
    arguments = {realm->process_object(),
                 realm->builtin_module_require(),
                 realm->internal_binding_loader(),
                 realm->primordials()};
  } else if (strncmp(id, "embedder_main_", strlen("embedder_main_")) == 0) {
    // Synthetic embedder main scripts from LoadEnvironment(): process, require
    arguments = {
        realm->process_object(),
        realm->builtin_module_require(),
    };
  } else {
    // This should be invoked with the other CompileAndCall() methods, as
    // we are unable to generate the arguments.
    // Currently there are two cases:
    // internal/per_context/*: the arguments are generated in
    //                         InitializePrimordials()
    // all the other cases: the arguments are generated in the JS-land loader.
    UNREACHABLE();
  }
  return CompileAndCall(context, id, arguments.size(), arguments.data(), realm);
}

MaybeLocal<Value> BuiltinLoader::CompileAndCall(Local<Context> context,
                                                const char* id,
                                                int argc,
                                                Local<Value> argv[],
                                                Realm* optional_realm) {
  // Arguments must match the parameters specified in
  // BuiltinLoader::LookupAndCompile().
  MaybeLocal<Function> maybe_fn = LookupAndCompile(context, id, optional_realm);
  Local<Function> fn;
  if (!maybe_fn.ToLocal(&fn)) {
    return MaybeLocal<Value>();
  }
  Local<Value> undefined = Undefined(context->GetIsolate());
  return fn->Call(context, undefined, argc, argv);
}

bool BuiltinLoader::CompileAllBuiltins(Local<Context> context) {
  std::vector<std::string> ids = GetBuiltinIds();
  bool all_succeeded = true;
  std::string v8_tools_prefix = "internal/deps/v8/tools/";
  for (const auto& id : ids) {
    if (id.compare(0, v8_tools_prefix.size(), v8_tools_prefix) == 0) {
      continue;
    }
    v8::TryCatch bootstrapCatch(context->GetIsolate());
    USE(LookupAndCompile(context, id.c_str(), nullptr));
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

void BuiltinLoader::CopyCodeCache(std::vector<CodeCacheInfo>* out) const {
  RwLock::ScopedReadLock lock(code_cache_->mutex);
  for (auto const& item : code_cache_->map) {
    out->push_back(
        {item.first,
         {item.second->data, item.second->data + item.second->length}});
  }
}

void BuiltinLoader::RefreshCodeCache(const std::vector<CodeCacheInfo>& in) {
  RwLock::ScopedLock lock(code_cache_->mutex);
  for (auto const& item : in) {
    size_t length = item.data.size();
    uint8_t* buffer = new uint8_t[length];
    memcpy(buffer, item.data.data(), length);
    auto new_cache = std::make_unique<v8::ScriptCompiler::CachedData>(
        buffer, length, v8::ScriptCompiler::CachedData::BufferOwned);
    code_cache_->map[item.id] = std::move(new_cache);
  }
  code_cache_->has_code_cache = true;
}

void BuiltinLoader::GetBuiltinCategories(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Object> result = Object::New(isolate);

  BuiltinCategories builtin_categories =
      env->builtin_loader()->GetBuiltinCategories();

  if (!env->owns_process_state()) {
    builtin_categories.can_be_required.erase("trace_events");
    builtin_categories.cannot_be_required.insert("trace_events");
  }

  Local<Value> cannot_be_required_js;
  Local<Value> can_be_required_js;

  if (!ToV8Value(context, builtin_categories.cannot_be_required)
           .ToLocal(&cannot_be_required_js))
    return;
  if (result
          ->Set(context,
                OneByteString(isolate, "cannotBeRequired"),
                cannot_be_required_js)
          .IsNothing())
    return;
  if (!ToV8Value(context, builtin_categories.can_be_required)
           .ToLocal(&can_be_required_js))
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

void BuiltinLoader::GetCacheUsage(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  Isolate* isolate = realm->isolate();
  Local<Context> context = realm->context();
  Local<Object> result = Object::New(isolate);

  Local<Value> builtins_with_cache_js;
  Local<Value> builtins_without_cache_js;
  Local<Value> builtins_in_snapshot_js;
  if (!ToV8Value(context, realm->builtins_with_cache)
           .ToLocal(&builtins_with_cache_js)) {
    return;
  }
  if (result
          ->Set(context,
                OneByteString(isolate, "compiledWithCache"),
                builtins_with_cache_js)
          .IsNothing()) {
    return;
  }

  if (!ToV8Value(context, realm->builtins_without_cache)
           .ToLocal(&builtins_without_cache_js)) {
    return;
  }
  if (result
          ->Set(context,
                OneByteString(isolate, "compiledWithoutCache"),
                builtins_without_cache_js)
          .IsNothing()) {
    return;
  }

  if (!ToV8Value(context, realm->builtins_in_snapshot)
           .ToLocal(&builtins_without_cache_js)) {
    return;
  }
  if (result
          ->Set(context,
                OneByteString(isolate, "compiledInSnapshot"),
                builtins_without_cache_js)
          .IsNothing()) {
    return;
  }

  args.GetReturnValue().Set(result);
}

void BuiltinLoader::BuiltinIdsGetter(Local<Name> property,
                                     const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();

  std::vector<std::string> ids = env->builtin_loader()->GetBuiltinIds();
  info.GetReturnValue().Set(
      ToV8Value(isolate->GetCurrentContext(), ids).ToLocalChecked());
}

void BuiltinLoader::ConfigStringGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  info.GetReturnValue().Set(
      env->builtin_loader()->GetConfigString(info.GetIsolate()));
}

void BuiltinLoader::RecordResult(const char* id,
                                 BuiltinLoader::Result result,
                                 Realm* realm) {
  if (result == BuiltinLoader::Result::kWithCache) {
    realm->builtins_with_cache.insert(id);
  } else {
    realm->builtins_without_cache.insert(id);
  }
}

void BuiltinLoader::CompileFunction(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  CHECK(args[0]->IsString());
  node::Utf8Value id_v(realm->isolate(), args[0].As<String>());
  const char* id = *id_v;
  MaybeLocal<Function> maybe = realm->env()->builtin_loader()->LookupAndCompile(
      realm->context(), id, realm);
  Local<Function> fn;
  if (maybe.ToLocal(&fn)) {
    args.GetReturnValue().Set(fn);
  }
}

void BuiltinLoader::HasCachedBuiltins(const FunctionCallbackInfo<Value>& args) {
  auto instance = Environment::GetCurrent(args)->builtin_loader();
  RwLock::ScopedReadLock lock(instance->code_cache_->mutex);
  args.GetReturnValue().Set(v8::Boolean::New(
      args.GetIsolate(), instance->code_cache_->has_code_cache));
}

void BuiltinLoader::CopySourceAndCodeCacheReferenceFrom(
    const BuiltinLoader* other) {
  code_cache_ = other->code_cache_;
  source_ = other->source_;
}

void BuiltinLoader::CreatePerIsolateProperties(IsolateData* isolate_data,
                                               Local<FunctionTemplate> target) {
  Isolate* isolate = isolate_data->isolate();
  Local<ObjectTemplate> proto = target->PrototypeTemplate();

  proto->SetAccessor(isolate_data->config_string(),
                     ConfigStringGetter,
                     nullptr,
                     Local<Value>(),
                     DEFAULT,
                     None,
                     SideEffectType::kHasNoSideEffect);

  proto->SetAccessor(FIXED_ONE_BYTE_STRING(isolate, "builtinIds"),
                     BuiltinIdsGetter,
                     nullptr,
                     Local<Value>(),
                     DEFAULT,
                     None,
                     SideEffectType::kHasNoSideEffect);

  proto->SetAccessor(FIXED_ONE_BYTE_STRING(isolate, "builtinCategories"),
                     GetBuiltinCategories,
                     nullptr,
                     Local<Value>(),
                     DEFAULT,
                     None,
                     SideEffectType::kHasNoSideEffect);

  SetMethod(isolate, proto, "getCacheUsage", BuiltinLoader::GetCacheUsage);
  SetMethod(isolate, proto, "compileFunction", BuiltinLoader::CompileFunction);
  SetMethod(isolate, proto, "hasCachedBuiltins", HasCachedBuiltins);
}

void BuiltinLoader::CreatePerContextProperties(Local<Object> target,
                                               Local<Value> unused,
                                               Local<Context> context,
                                               void* priv) {
  // internalBinding('builtins') should be frozen
  target->SetIntegrityLevel(context, IntegrityLevel::kFrozen).FromJust();
}

void BuiltinLoader::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(ConfigStringGetter);
  registry->Register(BuiltinIdsGetter);
  registry->Register(GetBuiltinCategories);
  registry->Register(GetCacheUsage);
  registry->Register(CompileFunction);
  registry->Register(HasCachedBuiltins);
}

}  // namespace builtins
}  // namespace node

NODE_BINDING_PER_ISOLATE_INIT(
    builtins, node::builtins::BuiltinLoader::CreatePerIsolateProperties)
NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    builtins, node::builtins::BuiltinLoader::CreatePerContextProperties)
NODE_BINDING_EXTERNAL_REFERENCE(
    builtins, node::builtins::BuiltinLoader::RegisterExternalReferences)
