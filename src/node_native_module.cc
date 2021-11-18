#include "node_native_module.h"
#include "debug_utils-inl.h"
#include "node_internals.h"
#include "util-inl.h"

namespace node {
namespace native_module {

using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;

NativeModuleLoader NativeModuleLoader::instance_;

NativeModuleLoader::NativeModuleLoader() : config_(GetConfig()) {
  LoadJavaScriptSource();
}

NativeModuleLoader* NativeModuleLoader::GetInstance() {
  return &instance_;
}

bool NativeModuleLoader::Exists(const char* id) {
  return source_.find(id) != source_.end();
}

bool NativeModuleLoader::Add(const char* id, const UnionBytes& source) {
  if (Exists(id)) {
    return false;
  }
  source_.emplace(id, source);
  return true;
}

Local<Object> NativeModuleLoader::GetSourceObject(Local<Context> context) {
  Isolate* isolate = context->GetIsolate();
  Local<Object> out = Object::New(isolate);
  for (auto const& x : source_) {
    Local<String> key = OneByteString(isolate, x.first.c_str(), x.first.size());
    out->Set(context, key, x.second.ToStringChecked(isolate)).FromJust();
  }
  return out;
}

Local<String> NativeModuleLoader::GetConfigString(Isolate* isolate) {
  return config_.ToStringChecked(isolate);
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

MaybeLocal<Function> NativeModuleLoader::CompileAsModule(
    Local<Context> context,
    const char* id,
    NativeModuleLoader::Result* result) {
  Isolate* isolate = context->GetIsolate();
  std::vector<Local<String>> parameters = {
      FIXED_ONE_BYTE_STRING(isolate, "exports"),
      FIXED_ONE_BYTE_STRING(isolate, "require"),
      FIXED_ONE_BYTE_STRING(isolate, "module"),
      FIXED_ONE_BYTE_STRING(isolate, "process"),
      FIXED_ONE_BYTE_STRING(isolate, "internalBinding"),
      FIXED_ONE_BYTE_STRING(isolate, "primordials")};
  return LookupAndCompile(context, id, &parameters, result);
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
MaybeLocal<Function> NativeModuleLoader::LookupAndCompile(
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

}  // namespace native_module
}  // namespace node
