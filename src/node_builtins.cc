#include "node_builtins.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "util-inl.h"

namespace node {
namespace builtins {

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
using v8::Undefined;
using v8::Value;

BuiltinLoader BuiltinLoader::instance_;

BuiltinLoader::BuiltinLoader() : config_(GetConfig()), has_code_cache_(false) {
  LoadJavaScriptSource();
#if defined(NODE_HAVE_I18N_SUPPORT)
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
#endif  // NODE_HAVE_I18N_SUPPORT
}

BuiltinLoader* BuiltinLoader::GetInstance() {
  return &instance_;
}

bool BuiltinLoader::Exists(const char* id) {
  auto& source = GetInstance()->source_;
  return source.find(id) != source.end();
}

bool BuiltinLoader::Add(const char* id, const UnionBytes& source) {
  auto result = GetInstance()->source_.emplace(id, source);
  return result.second;
}

Local<Object> BuiltinLoader::GetSourceObject(Local<Context> context) {
  Isolate* isolate = context->GetIsolate();
  Local<Object> out = Object::New(isolate);
  auto& source = GetInstance()->source_;
  for (auto const& x : source) {
    Local<String> key = OneByteString(isolate, x.first.c_str(), x.first.size());
    out->Set(context, key, x.second.ToStringChecked(isolate)).FromJust();
  }
  return out;
}

Local<String> BuiltinLoader::GetConfigString(Isolate* isolate) {
  return GetInstance()->config_.ToStringChecked(isolate);
}

std::vector<std::string> BuiltinLoader::GetBuiltinIds() {
  std::vector<std::string> ids;
  ids.reserve(source_.size());
  for (auto const& x : source_) {
    ids.emplace_back(x.first);
  }
  return ids;
}

void BuiltinLoader::InitializeBuiltinCategories() {
  if (builtin_categories_.is_initialized) {
    DCHECK(!builtin_categories_.can_be_required.empty());
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

  builtin_categories_.can_be_required.emplace(
      "internal/deps/cjs-module-lexer/lexer");

  builtin_categories_.cannot_be_required = std::set<std::string> {
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

  for (auto const& x : source_) {
    const std::string& id = x.first;
    for (auto const& prefix : prefixes) {
      if (prefix.length() > id.length()) {
        continue;
      }
      if (id.find(prefix) == 0 &&
          builtin_categories_.can_be_required.count(id) == 0) {
        builtin_categories_.cannot_be_required.emplace(id);
      }
    }
  }

  for (auto const& x : source_) {
    const std::string& id = x.first;
    if (0 == builtin_categories_.cannot_be_required.count(id)) {
      builtin_categories_.can_be_required.emplace(id);
    }
  }

  builtin_categories_.is_initialized = true;
}

const std::set<std::string>& BuiltinLoader::GetCannotBeRequired() {
  InitializeBuiltinCategories();
  return builtin_categories_.cannot_be_required;
}

const std::set<std::string>& BuiltinLoader::GetCanBeRequired() {
  InitializeBuiltinCategories();
  return builtin_categories_.can_be_required;
}

bool BuiltinLoader::CanBeRequired(const char* id) {
  return GetCanBeRequired().count(id) == 1;
}

bool BuiltinLoader::CannotBeRequired(const char* id) {
  return GetCannotBeRequired().count(id) == 1;
}

BuiltinCodeCacheMap* BuiltinLoader::code_cache() {
  return &code_cache_;
}

ScriptCompiler::CachedData* BuiltinLoader::GetCodeCache(const char* id) const {
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

MaybeLocal<String> BuiltinLoader::LoadBuiltinSource(Isolate* isolate,
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

#if defined(NODE_HAVE_I18N_SUPPORT)
void BuiltinLoader::AddExternalizedBuiltin(const char* id,
                                           const char* filename) {
  std::string source;
  int r = ReadFileSync(&source, filename);
  if (r != 0) {
    fprintf(
        stderr, "Cannot load externalized builtin: \"%s:%s\".\n", id, filename);
    ABORT();
    return;
  }

  icu::UnicodeString utf16 = icu::UnicodeString::fromUTF8(
      icu::StringPiece(source.data(), source.length()));
  auto source_utf16 = std::make_unique<icu::UnicodeString>(utf16);
  Add(id,
      UnionBytes(reinterpret_cast<const uint16_t*>((*source_utf16).getBuffer()),
                 utf16.length()));
  // keep source bytes for builtin alive while BuiltinLoader exists
  GetInstance()->externalized_source_bytes_.push_back(std::move(source_utf16));
}
#endif  // NODE_HAVE_I18N_SUPPORT

// Returns Local<Function> of the compiled module if return_code_cache
// is false (we are only compiling the function).
// Otherwise return a Local<Object> containing the cache.
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
MaybeLocal<Function> BuiltinLoader::LookupAndCompile(
    Local<Context> context, const char* id, Environment* optional_env) {
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

  MaybeLocal<Function> maybe = GetInstance()->LookupAndCompileInternal(
      context, id, &parameters, &result);
  if (optional_env != nullptr) {
    RecordResult(id, result, optional_env);
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
  return CompileAndCall(
      context, id, arguments.size(), arguments.data(), realm->env());
}

MaybeLocal<Value> BuiltinLoader::CompileAndCall(Local<Context> context,
                                                const char* id,
                                                int argc,
                                                Local<Value> argv[],
                                                Environment* optional_env) {
  // Arguments must match the parameters specified in
  // BuiltinLoader::LookupAndCompile().
  MaybeLocal<Function> maybe_fn = LookupAndCompile(context, id, optional_env);
  Local<Function> fn;
  if (!maybe_fn.ToLocal(&fn)) {
    return MaybeLocal<Value>();
  }
  Local<Value> undefined = Undefined(context->GetIsolate());
  return fn->Call(context, undefined, argc, argv);
}

bool BuiltinLoader::CompileAllBuiltins(Local<Context> context) {
  BuiltinLoader* loader = GetInstance();
  std::vector<std::string> ids = loader->GetBuiltinIds();
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

void BuiltinLoader::CopyCodeCache(std::vector<CodeCacheInfo>* out) {
  BuiltinLoader* loader = GetInstance();
  Mutex::ScopedLock lock(loader->code_cache_mutex());
  auto in = loader->code_cache();
  for (auto const& item : *in) {
    out->push_back(
        {item.first,
         {item.second->data, item.second->data + item.second->length}});
  }
}

void BuiltinLoader::RefreshCodeCache(const std::vector<CodeCacheInfo>& in) {
  BuiltinLoader* loader = GetInstance();
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

void BuiltinLoader::GetBuiltinCategories(
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

void BuiltinLoader::GetCacheUsage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Object> result = Object::New(isolate);

  Local<Value> builtins_with_cache_js;
  Local<Value> builtins_without_cache_js;
  Local<Value> builtins_in_snapshot_js;
  if (!ToV8Value(context, env->builtins_with_cache)
           .ToLocal(&builtins_with_cache_js)) {
    return;
  }
  if (result
          ->Set(env->context(),
                OneByteString(isolate, "compiledWithCache"),
                builtins_with_cache_js)
          .IsNothing()) {
    return;
  }

  if (!ToV8Value(context, env->builtins_without_cache)
           .ToLocal(&builtins_without_cache_js)) {
    return;
  }
  if (result
          ->Set(env->context(),
                OneByteString(isolate, "compiledWithoutCache"),
                builtins_without_cache_js)
          .IsNothing()) {
    return;
  }

  if (!ToV8Value(context, env->builtins_in_snapshot)
           .ToLocal(&builtins_without_cache_js)) {
    return;
  }
  if (result
          ->Set(env->context(),
                OneByteString(isolate, "compiledInSnapshot"),
                builtins_without_cache_js)
          .IsNothing()) {
    return;
  }

  args.GetReturnValue().Set(result);
}

void BuiltinLoader::BuiltinIdsGetter(Local<Name> property,
                                     const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();

  std::vector<std::string> ids = GetInstance()->GetBuiltinIds();
  info.GetReturnValue().Set(
      ToV8Value(isolate->GetCurrentContext(), ids).ToLocalChecked());
}

void BuiltinLoader::ConfigStringGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(GetConfigString(info.GetIsolate()));
}

void BuiltinLoader::RecordResult(const char* id,
                                 BuiltinLoader::Result result,
                                 Environment* env) {
  if (result == BuiltinLoader::Result::kWithCache) {
    env->builtins_with_cache.insert(id);
  } else {
    env->builtins_without_cache.insert(id);
  }
}

void BuiltinLoader::CompileFunction(const FunctionCallbackInfo<Value>& args) {
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

void BuiltinLoader::HasCachedBuiltins(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
      v8::Boolean::New(args.GetIsolate(), GetInstance()->has_code_cache_));
}

// TODO(joyeecheung): It is somewhat confusing that Class::Initialize
// is used to initialize to the binding, but it is the current convention.
// Rename this across the code base to something that makes more sense.
void BuiltinLoader::Initialize(Local<Object> target,
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
                    FIXED_ONE_BYTE_STRING(isolate, "builtinIds"),
                    BuiltinIdsGetter,
                    nullptr,
                    MaybeLocal<Value>(),
                    DEFAULT,
                    None,
                    SideEffectType::kHasNoSideEffect)
      .Check();

  target
      ->SetAccessor(context,
                    FIXED_ONE_BYTE_STRING(isolate, "builtinCategories"),
                    GetBuiltinCategories,
                    nullptr,
                    Local<Value>(),
                    DEFAULT,
                    None,
                    SideEffectType::kHasNoSideEffect)
      .Check();

  SetMethod(context, target, "getCacheUsage", BuiltinLoader::GetCacheUsage);
  SetMethod(context, target, "compileFunction", BuiltinLoader::CompileFunction);
  SetMethod(context, target, "hasCachedBuiltins", HasCachedBuiltins);
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

NODE_BINDING_CONTEXT_AWARE_INTERNAL(builtins,
                                    node::builtins::BuiltinLoader::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    builtins, node::builtins::BuiltinLoader::RegisterExternalReferences)
