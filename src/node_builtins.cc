#include "node_builtins.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "module_wrap.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_threadsafe_cow-inl.h"
#include "quic/guard.h"
#include "simdutf.h"
#include "util-inl.h"
#include "v8-value.h"

namespace node {
namespace builtins {

using loader::HostDefinedOptions;
using v8::Boolean;
using v8::Context;
using v8::Data;
using v8::EscapableHandleScope;
using v8::FixedArray;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::IntegrityLevel;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::Module;
using v8::ModuleRequest;
using v8::Name;
using v8::None;
using v8::Object;
using v8::ObjectTemplate;
using v8::PrimitiveArray;
using v8::PropertyCallbackInfo;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::Set;
using v8::SideEffectType;
using v8::String;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;

BuiltinLoader::BuiltinLoader()
    : config_(GetConfig()), code_cache_(std::make_shared<BuiltinCodeCache>()) {
  LoadJavaScriptSource();
#ifdef NODE_SHARED_BUILTIN_UNDICI_UNDICI_PATH
  AddExternalizedBuiltin("internal/deps/undici/undici",
                         STRINGIFY(NODE_SHARED_BUILTIN_UNDICI_UNDICI_PATH));
#endif  // NODE_SHARED_BUILTIN_UNDICI_UNDICI_PATH

#if HAVE_AMARO
#ifdef NODE_SHARED_BUILTIN_AMARO_DIST_INDEX_PATH
  AddExternalizedBuiltin("internal/deps/amaro/dist/index",
                         STRINGIFY(NODE_SHARED_BUILTIN_AMARO_DIST_INDEX_PATH));
#endif  // NODE_SHARED_BUILTIN_AMARO_DIST_INDEX_PATH
#endif  // HAVE_AMARO
}

std::ranges::keys_view<std::ranges::ref_view<const BuiltinSourceMap>>
BuiltinLoader::GetBuiltinIds() const {
  return std::views::keys(*source_.read());
}

bool BuiltinLoader::Exists(const char* id) {
  auto source = source_.read();
  return source->find(id) != source->end();
}

const BuiltinSource* BuiltinLoader::AddFromDisk(const char* id,
                                                const std::string& filename,
                                                const UnionBytes& source) {
  BuiltinSourceType type = GetBuiltinSourceType(id, filename);
  auto result = source_.write()->emplace(id, BuiltinSource{id, source, type});
  return &(result.first->second);
}

void BuiltinLoader::GetNatives(Local<Name> property,
                               const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  Local<Object> out = Object::New(isolate);
  auto source = env->builtin_loader()->source_.read();
  for (auto const& x : *source) {
    Local<String> key = OneByteString(isolate, x.first);
    if (out->Set(context, key, x.second.source.ToStringChecked(isolate))
            .IsNothing()) {
      return;
    }
  }
  info.GetReturnValue().Set(out);
}

Local<String> BuiltinLoader::GetConfigString(Isolate* isolate) {
  return config_.ToStringChecked(isolate);
}

BuiltinLoader::BuiltinCategories BuiltinLoader::GetBuiltinCategories() const {
  BuiltinCategories builtin_categories;

  const std::vector<std::string_view> prefixes = {
#if !HAVE_OPENSSL
    "internal/crypto/",
    "internal/debugger/",
#endif  // !HAVE_OPENSSL

    "internal/bootstrap/",
    "internal/per_context/",
    "internal/deps/",
    "internal/main/"
  };

  builtin_categories.cannot_be_required = std::set<std::string> {
#if !HAVE_INSPECTOR
    "inspector", "inspector/promises", "internal/util/inspector",
        "internal/inspector/network", "internal/inspector/network_http",
        "internal/inspector/network_http2", "internal/inspector/network_undici",
        "internal/inspector_async_hook", "internal/inspector_network_tracking",
#endif  // !HAVE_INSPECTOR

#if !NODE_USE_V8_PLATFORM || !defined(NODE_HAVE_I18N_SUPPORT)
        "trace_events",
#endif  // !NODE_USE_V8_PLATFORM || !defined(NODE_HAVE_I18N_SUPPORT)

#if !HAVE_OPENSSL
        "crypto", "crypto/promises", "https", "http2", "tls", "_tls_common",
        "_tls_wrap", "internal/tls/parse-cert-string", "internal/tls/common",
        "internal/tls/wrap", "internal/tls/secure-context",
        "internal/http2/core", "internal/http2/compat",
        "internal/streams/lazy_transform",
#endif           // !HAVE_OPENSSL
#ifndef OPENSSL_NO_QUIC
        "internal/quic/quic", "internal/quic/symbols", "internal/quic/stats",
        "internal/quic/state",
#endif             // !OPENSSL_NO_QUIC
        "quic",    // Experimental.
        "sqlite",  // Experimental.
        "sys",     // Deprecated.
        "wasi",    // Experimental.
#if !HAVE_SQLITE
        "internal/webstorage",  // Experimental.
#endif
        "internal/test/binding", "internal/v8_prof_polyfill",
  };

  auto source = source_.read();
  for (auto const& x : *source) {
    const std::string& id = x.first;
    for (auto const& prefix : prefixes) {
      if (prefix.length() > id.length()) {
        continue;
      }
      if (id.starts_with(prefix) &&
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
  if (strncmp(id, "deps/v8/tools", strlen("deps/v8/tools")) == 0) {
    // V8 tools scripts are .mjs files.
    filename += ".mjs";
  } else {
    filename += ".js";
  }

  return filename;
}
#endif  // NODE_BUILTIN_MODULES_PATH

const BuiltinSource* BuiltinLoader::LoadBuiltinSource(Isolate* isolate,
                                                      const char* id) {
#ifndef NODE_BUILTIN_MODULES_PATH
  auto source = source_.read();
  const auto source_it = source->find(id);
  if (source_it == source->end()) [[unlikely]] {
    fprintf(stderr, "Cannot find native builtin: \"%s\".\n", id);
    ABORT();
  }
  return &(source_it->second);
#else   // !NODE_BUILTIN_MODULES_PATH
  std::string filename = OnDiskFileName(id);
  return AddExternalizedBuiltin(id, filename.c_str());
#endif  // NODE_BUILTIN_MODULES_PATH
}

namespace {
static Mutex externalized_builtins_mutex;
std::unordered_map<std::string, std::unique_ptr<StaticExternalTwoByteResource>>
    externalized_builtin_sources;
}  // namespace

const BuiltinSource* BuiltinLoader::AddExternalizedBuiltin(
    const char* id, const char* filename) {
  StaticExternalTwoByteResource* resource;
  {
    Mutex::ScopedLock lock(externalized_builtins_mutex);
    auto it = externalized_builtin_sources.find(id);
    if (it == externalized_builtin_sources.end()) {
      std::string source;
      int r = ReadFileSync(&source, filename);
      if (r != 0) {
        fprintf(stderr,
                "Cannot load externalized builtin: \"%s:%s\".\n",
                id,
                filename);
        ABORT();
      }
      size_t expected_u16_length =
          simdutf::utf16_length_from_utf8(source.data(), source.length());
      auto out = std::make_shared<std::vector<uint16_t>>(expected_u16_length);
      size_t u16_length = simdutf::convert_utf8_to_utf16(
          source.data(),
          source.length(),
          reinterpret_cast<char16_t*>(out->data()));
      out->resize(u16_length);

      auto result = externalized_builtin_sources.emplace(
          id,
          std::make_unique<StaticExternalTwoByteResource>(
              out->data(), out->size(), out));
      CHECK(result.second);
      it = result.first;
    }
    // OK to get the raw pointer, since externalized_builtin_sources owns
    // the resource, resources are never removed from the map, and
    // externalized_builtin_sources has static lifetime.
    resource = it->second.get();
  }

  return AddFromDisk(id, filename, UnionBytes(resource));
}

MaybeLocal<Data> BuiltinLoader::LookupAndCompile(Local<Context> context,
                                                 const char* id,
                                                 Realm* optional_realm) {
  Isolate* isolate = Isolate::GetCurrent();
  const BuiltinSource* builtin_source = LoadBuiltinSource(isolate, id);
  if (builtin_source == nullptr) {
    THROW_ERR_MODULE_NOT_FOUND(isolate, "Cannot find module %s", id);
    return MaybeLocal<Data>();
  }
  return LookupAndCompile(context, builtin_source, optional_realm);
}

MaybeLocal<Data> BuiltinLoader::LookupAndCompile(
    Local<Context> context,
    const BuiltinSource* builtin_source,
    Realm* optional_realm) {
  Isolate* isolate = Isolate::GetCurrent();
  EscapableHandleScope scope(isolate);

  BuiltinCodeCacheData cached_data{};
  {
    // Note: The lock here should not extend into the
    // `CompileFunction()` call below, because this function may recurse if
    // there is a syntax error during bootstrap (because the fatal exception
    // handler is invoked, which may load built-in modules).
    RwLock::ScopedLock lock(code_cache_->mutex);
    auto cache_it = code_cache_->map.find(builtin_source->id);
    if (cache_it != code_cache_->map.end()) {
      // Transfer ownership to ScriptCompiler::Source later.
      cached_data = cache_it->second;
    }
  }

  const bool has_cache = cached_data.data != nullptr;
  ScriptCompiler::CompileOptions options =
      has_cache ? ScriptCompiler::kConsumeCodeCache
                : ScriptCompiler::kNoCompileOptions;
  if (should_eager_compile_) {
    options = ScriptCompiler::kEagerCompile;
  } else if (!to_eager_compile_.empty()) {
    if (to_eager_compile_.contains(builtin_source->id)) {
      options = ScriptCompiler::kEagerCompile;
    }
  }

  per_process::Debug(
      DebugCategory::CODE_CACHE,
      "Compiling %s %s code cache %s\n",
      builtin_source->id,
      has_cache ? "with" : "without",
      options == ScriptCompiler::kEagerCompile ? "eagerly" : "lazily");

  // The ownership of cached_data_ptr will be transferred to
  // ScriptCompiler::Source.
  ScriptCompiler::CachedData* cached_data_ptr =
      has_cache ? cached_data.AsCachedData().release() : nullptr;
  std::string filename_s = std::string("node:") + builtin_source->id;
  Local<String> filename = OneByteString(isolate, filename_s);
  Local<String> source = builtin_source->source.ToStringChecked(isolate);

  Local<Data> compiled_result;
  // This needs to be retrieved after compilation, as the ownership of
  // cached data will be transferred to script source.
  bool is_cache_accepted = false;
  bool is_buffer_owned = cached_data_ptr
                             ? (cached_data_ptr->buffer_policy ==
                                ScriptCompiler::CachedData::BufferOwned)
                             : false;
  if (builtin_source->type == BuiltinSourceType::kSourceTextModule) {
    Local<PrimitiveArray> host_defined_options;
    if (optional_realm != nullptr) {
      host_defined_options =
          PrimitiveArray::New(isolate, HostDefinedOptions::kLength);
      host_defined_options->Set(
          isolate,
          HostDefinedOptions::kID,
          optional_realm->isolate_data()->builtin_source_text_module_hdo());
    }
    ScriptOrigin origin(filename,
                        0,
                        0,
                        true,            // is cross origin
                        -1,              // script id
                        Local<Value>(),  // source map URL
                        false,           // is opaque
                        false,           // is WASM
                        true,            // is ES Module
                        host_defined_options);
    ScriptCompiler::Source script_source(source, origin, cached_data_ptr);
    MaybeLocal<Module> maybe_mod =
        ScriptCompiler::CompileModule(isolate, &script_source, options);
    if (maybe_mod.IsEmpty()) {
      // In the case of early errors, v8 is already capable of
      // decorating the stack for us.
      return MaybeLocal<Data>();
    }
    compiled_result = maybe_mod.ToLocalChecked();
    is_cache_accepted = has_cache && !script_source.GetCachedData()->rejected;
  } else {
    ScriptOrigin origin(filename, 0, 0, true);
    ScriptCompiler::Source script_source(source, origin, cached_data_ptr);
    LocalVector<String> parameters(isolate);
    auto params_it = BuiltinInfo::parameter_map.find(builtin_source->type);
    CHECK_NE(params_it, BuiltinInfo::parameter_map.end());
    parameters.reserve(params_it->second.size());
    for (const std::string& param : params_it->second) {
      parameters.push_back(OneByteString(isolate, param));
    }
    MaybeLocal<Function> maybe_fun =
        ScriptCompiler::CompileFunction(context,
                                        &script_source,
                                        parameters.size(),
                                        parameters.data(),
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
      return MaybeLocal<Data>();
    }

    compiled_result = fun;
    is_cache_accepted = has_cache && !script_source.GetCachedData()->rejected;
  }

  // XXX(joyeecheung): this bookkeeping is not exactly accurate because
  // it only starts after the Environment is created, so the per_context.js
  // will never be in any of these two sets, but the two sets are only for
  // testing anyway.
  Result result =
      is_cache_accepted ? Result::kWithCache : Result::kWithoutCache;
  if (optional_realm != nullptr) {
    DCHECK_EQ(this, optional_realm->env()->builtin_loader());
    RecordResult(builtin_source->id, result, optional_realm);
  }

  if (has_cache) {
    per_process::Debug(DebugCategory::CODE_CACHE,
                       "Code cache of %s (%s) %s\n",
                       builtin_source->id,
                       is_buffer_owned ? "BufferOwned" : "BufferNotOwned",
                       is_cache_accepted ? "is accepted" : "is rejected");
  }

  if (result == Result::kWithoutCache && optional_realm != nullptr &&
      !optional_realm->env()->isolate_data()->is_building_snapshot()) {
    // We failed to accept this cache, maybe because it was rejected, maybe
    // because it wasn't present. Either way, we'll attempt to replace this
    // code cache info with a new one.
    // This is only done when the isolate is not being serialized because
    // V8 does not support serializing code cache with an unfinalized read-only
    // space (which is what isolates pending to be serialized have).
    SaveCodeCache(builtin_source->id, compiled_result);
  }

  return scope.Escape(compiled_result);
}

void BuiltinLoader::SaveCodeCache(const std::string& id, Local<Data> data) {
  std::shared_ptr<ScriptCompiler::CachedData> new_cached_data;
  if (data->IsModule()) {
    Local<Module> mod = data.As<Module>();
    new_cached_data.reset(
        ScriptCompiler::CreateCodeCache(mod->GetUnboundModuleScript()));
  } else {
    Local<Function> fun = data.As<Value>().As<Function>();
    new_cached_data.reset(ScriptCompiler::CreateCodeCacheForFunction(fun));
  }
  CHECK_NOT_NULL(new_cached_data);

  {
    RwLock::ScopedLock lock(code_cache_->mutex);
    code_cache_->map.insert_or_assign(
        id, BuiltinCodeCacheData(std::move(new_cached_data)));
  }
}

MaybeLocal<Function> BuiltinLoader::LookupAndCompileFunction(
    Local<Context> context, const char* id, Realm* optional_realm) {
  Isolate* isolate = Isolate::GetCurrent();
  LocalVector<String> parameters(isolate);

  Local<Data> data;
  if (!LookupAndCompile(context, id, optional_realm).ToLocal(&data)) {
    return MaybeLocal<Function>();
  }

  CHECK(data->IsValue());
  Local<Value> value = data.As<Value>();
  CHECK(value->IsFunction());
  return value.As<Function>();
}

MaybeLocal<Value> BuiltinLoader::CompileAndCall(Local<Context> context,
                                                const char* id,
                                                Realm* realm) {
  Isolate* isolate = Isolate::GetCurrent();
  const BuiltinSource* builtin_source = LoadBuiltinSource(isolate, id);
  if (builtin_source == nullptr) {
    THROW_ERR_MODULE_NOT_FOUND(isolate, "Cannot find module %s", id);
    return MaybeLocal<Value>();
  }

  if (builtin_source->type == BuiltinSourceType::kSourceTextModule) {
    Local<Value> value;
    if (!ImportBuiltinSourceTextModule(realm, id).ToLocal(&value)) {
      return MaybeLocal<Value>();
    }
    return value;
  }

  Local<Data> data;
  if (!LookupAndCompile(context, builtin_source, realm).ToLocal(&data)) {
    return MaybeLocal<Value>();
  }
  CHECK(data->IsValue());
  Local<Value> value = data.As<Value>();
  CHECK(value->IsFunction());
  Local<Function> fn = value.As<Function>();
  Local<Value> receiver = Undefined(Isolate::GetCurrent());

  switch (builtin_source->type) {
    case BuiltinSourceType::kBootstrapRealm: {
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
      Local<Value> arguments[] = {realm->process_object(),
                                  get_linked_binding,
                                  get_internal_binding,
                                  realm->primordials()};
      return fn->Call(context, receiver, 4, arguments);
    }
    case BuiltinSourceType::kMainScript:
    case BuiltinSourceType::kBootstrapScript: {
      Local<Value> arguments[] = {realm->process_object(),
                                  realm->builtin_module_require(),
                                  realm->internal_binding_loader(),
                                  realm->primordials()};
      return fn->Call(context, receiver, 4, arguments);
    }
    case BuiltinSourceType::kFunction:          // Handled in JS land.
    case BuiltinSourceType::kPerContextScript:  // Handled by
                                                // InitializePrimordials
    default:
      UNREACHABLE();
  }
}

MaybeLocal<Value> BuiltinLoader::CompileAndCallWith(Local<Context> context,
                                                    const char* id,
                                                    int argc,
                                                    Local<Value> argv[],
                                                    Realm* optional_realm) {
  // Arguments must match the parameters specified in
  // BuiltinLoader::LookupAndCompile().
  MaybeLocal<Function> maybe_fn =
      LookupAndCompileFunction(context, id, optional_realm);
  Local<Function> fn;
  if (!maybe_fn.ToLocal(&fn)) {
    return MaybeLocal<Value>();
  }
  Local<Value> undefined = Undefined(Isolate::GetCurrent());
  return fn->Call(context, undefined, argc, argv);
}

bool BuiltinLoader::CompileAllBuiltinsAndCopyCodeCache(
    Local<Context> context,
    const std::vector<std::string>& eager_builtins,
    std::vector<CodeCacheInfo>* out) {
  auto ids = GetBuiltinIds();
  bool all_succeeded = true;
  constexpr std::string_view primordial_prefix = "internal/per_context/";
  constexpr std::string_view bootstrap_prefix = "internal/bootstrap/";
  constexpr std::string_view main_prefix = "internal/main/";
  to_eager_compile_ =
      std::unordered_set(eager_builtins.begin(), eager_builtins.end());

  for (const auto& id : ids) {
    // Eagerly compile primordials/boostrap/main scripts during code cache
    // generation.
    if (id.starts_with(primordial_prefix) || id.starts_with(bootstrap_prefix) ||
        id.starts_with(main_prefix)) {
      to_eager_compile_.emplace(id);
    }

    TryCatch bootstrapCatch(Isolate::GetCurrent());
    auto data = LookupAndCompile(context, id.data(), nullptr);
    if (bootstrapCatch.HasCaught()) {
      per_process::Debug(DebugCategory::CODE_CACHE,
                         "Failed to compile code cache for %s\n",
                         id);
      all_succeeded = false;
      PrintCaughtException(Isolate::GetCurrent(), context, bootstrapCatch);
    } else {
      // This is used by the snapshot builder, so save the code cache
      // unconditionally.
      SaveCodeCache(id, data.ToLocalChecked());
    }
  }

  RwLock::ScopedReadLock lock(code_cache_->mutex);
  for (const auto& [id, data] : code_cache_->map) {
    out->push_back({id, data});
  }
  return all_succeeded;
}

void BuiltinLoader::RefreshCodeCache(const std::vector<CodeCacheInfo>& in) {
  RwLock::ScopedLock lock(code_cache_->mutex);
  code_cache_->map.reserve(in.size());
  DCHECK(code_cache_->map.empty());
  for (auto const& [id, data] : in) {
    auto result = code_cache_->map.emplace(id, data);
    USE(result.second);
    DCHECK(result.second);
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
           .ToLocal(&cannot_be_required_js) ||
      result
          ->Set(context,
                FIXED_ONE_BYTE_STRING(isolate, "cannotBeRequired"),
                cannot_be_required_js)
          .IsNothing() ||
      !ToV8Value(context, builtin_categories.can_be_required)
           .ToLocal(&can_be_required_js) ||
      result
          ->Set(context,
                FIXED_ONE_BYTE_STRING(isolate, "canBeRequired"),
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
           .ToLocal(&builtins_with_cache_js) ||
      result
          ->Set(context,
                FIXED_ONE_BYTE_STRING(isolate, "compiledWithCache"),
                builtins_with_cache_js)
          .IsNothing() ||
      !ToV8Value(context, realm->builtins_without_cache)
           .ToLocal(&builtins_without_cache_js) ||
      result
          ->Set(context,
                FIXED_ONE_BYTE_STRING(isolate, "compiledWithoutCache"),
                builtins_without_cache_js)
          .IsNothing() ||
      !ToV8Value(context, realm->builtins_in_snapshot)
           .ToLocal(&builtins_in_snapshot_js) ||
      result
          ->Set(context,
                FIXED_ONE_BYTE_STRING(isolate, "compiledInSnapshot"),
                builtins_in_snapshot_js)
          .IsNothing()) {
    return;
  }

  args.GetReturnValue().Set(result);
}

void BuiltinLoader::BuiltinIdsGetter(Local<Name> property,
                                     const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();

  auto ids = env->builtin_loader()->GetBuiltinIds();
  Local<Value> ret;
  if (ToV8Value(isolate->GetCurrentContext(), ids).ToLocal(&ret)) {
    info.GetReturnValue().Set(ret);
  }
}

void BuiltinLoader::ConfigStringGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  info.GetReturnValue().Set(
      env->builtin_loader()->GetConfigString(info.GetIsolate()));
}

void BuiltinLoader::RecordResult(const std::string& id,
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
  Local<Function> fn;
  if (realm->env()
          ->builtin_loader()
          ->LookupAndCompileFunction(realm->context(), id, realm)
          .ToLocal(&fn)) {
    args.GetReturnValue().Set(fn);
  }
}

std::string ResolveRequestForBuiltin(const std::string& specifier) {
  // Currently this is only ever hit by V8 tools. Importing any other specifiers
  // from a builtin would result in a module not found error later.
  if (specifier[0] == '.' && specifier[1] == '/') {
    // Currently the V8 tool sources are all flat.
    // ./file.mjs -> internal/deps/v8/tools/file
    DCHECK(specifier.ends_with(".mjs"));
    return std::string("internal/deps/v8/tools/") +
           specifier.substr(2, specifier.length() - 6);
  }
  return specifier;
}

MaybeLocal<Module> BuiltinLoader::LoadBuiltinSourceTextModule(Realm* realm,
                                                              const char* id) {
  Isolate* isolate = realm->isolate();
  if (module_cache_.find(id) != module_cache_.end()) {
    return module_cache_[id].Get(isolate);
  }

  Local<Context> context = realm->context();
  Local<Data> data;

  if (!LookupAndCompile(context, id, realm).ToLocal(&data)) {
    return MaybeLocal<Module>();
  }

  CHECK(data->IsModule());
  Local<Module> mod = data.As<Module>();

  auto pair = module_cache_.emplace(id, v8::Global<v8::Module>(isolate, mod));
  CHECK(pair.second);
  Local<FixedArray> requests = mod->GetModuleRequests();

  // Pre-fetch all dependencies.
  if (requests->Length() > 0) {
    for (int i = 0; i < requests->Length(); i++) {
      Local<ModuleRequest> req = requests->Get(context, i).As<ModuleRequest>();
      std::string specifier =
          Utf8Value(isolate, req->GetSpecifier()).ToString();
      std::string resolved_id = ResolveRequestForBuiltin(specifier);
      if (LoadBuiltinSourceTextModule(realm, resolved_id.c_str()).IsEmpty()) {
        return MaybeLocal<Module>();
      }
    }
  }

  return mod;
}

MaybeLocal<Value> BuiltinLoader::ImportBuiltinSourceTextModule(Realm* realm,
                                                               const char* id) {
  Isolate* isolate = realm->isolate();
  EscapableHandleScope scope(isolate);
  Local<Module> mod;
  if (!LoadBuiltinSourceTextModule(realm, id).ToLocal(&mod)) {
    return MaybeLocal<Value>();
  }
  Local<Context> context = realm->context();
  if (mod->InstantiateModule(context, ResolveModuleCallback).IsEmpty()) {
    return MaybeLocal<Value>();
  }
  Local<Value> promise;
  if (!mod->Evaluate(context).ToLocal(&promise)) {
    return MaybeLocal<Value>();
  }

  LocalVector<Name> names{isolate,
                          {
                              OneByteString(isolate, "promise"),
                              OneByteString(isolate, "namespace"),
                          }};
  LocalVector<Value> values{isolate,
                            {
                                promise,
                                mod->GetModuleNamespace(),
                            }};
  auto result = Object::New(
      isolate, v8::Null(isolate), names.data(), values.data(), names.size());
  return scope.Escape(result);
}

MaybeLocal<Module> BuiltinLoader::ResolveModuleCallback(
    Local<Context> context,
    Local<String> specifier,
    Local<FixedArray> import_attributes,
    Local<Module> referrer) {
  Realm* realm = Realm::GetCurrent(context);

  Utf8Value specifier_v(realm->isolate(), specifier);
  std::string resolved = ResolveRequestForBuiltin(specifier_v.ToString());
  return realm->env()->builtin_loader()->LoadBuiltinSourceTextModule(
      realm, resolved.c_str());
}

void BuiltinLoader::ImportBuiltinSourceTextModule(
    const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  CHECK(args[0]->IsString());
  node::Utf8Value id(realm->isolate(), args[0].As<String>());
  Local<Value> value;
  if (realm->env()
          ->builtin_loader()
          ->ImportBuiltinSourceTextModule(realm, *id)
          .ToLocal(&value)) {
    args.GetReturnValue().Set(value);
  }
}

void BuiltinLoader::HasCachedBuiltins(const FunctionCallbackInfo<Value>& args) {
  auto instance = Environment::GetCurrent(args)->builtin_loader();
  RwLock::ScopedReadLock lock(instance->code_cache_->mutex);
  args.GetReturnValue().Set(
      Boolean::New(args.GetIsolate(), instance->code_cache_->has_code_cache));
}

void SetInternalLoaders(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  CHECK(args[1]->IsFunction());
  DCHECK(realm->internal_binding_loader().IsEmpty());
  DCHECK(realm->builtin_module_require().IsEmpty());
  realm->set_internal_binding_loader(args[0].As<Function>());
  realm->set_builtin_module_require(args[1].As<Function>());
}

void BuiltinLoader::CopySourceAndCodeCacheReferenceFrom(
    const BuiltinLoader* other) {
  code_cache_ = other->code_cache_;
  source_ = other->source_;
}

void BuiltinLoader::CreatePerIsolateProperties(IsolateData* isolate_data,
                                               Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();

  target->SetNativeDataProperty(isolate_data->config_string(),
                                ConfigStringGetter,
                                nullptr,
                                Local<Value>(),
                                None,
                                SideEffectType::kHasNoSideEffect);

  target->SetNativeDataProperty(FIXED_ONE_BYTE_STRING(isolate, "builtinIds"),
                                BuiltinIdsGetter,
                                nullptr,
                                Local<Value>(),
                                None,
                                SideEffectType::kHasNoSideEffect);

  target->SetNativeDataProperty(
      FIXED_ONE_BYTE_STRING(isolate, "builtinCategories"),
      GetBuiltinCategories,
      nullptr,
      Local<Value>(),
      None,
      SideEffectType::kHasNoSideEffect);

  target->SetNativeDataProperty(FIXED_ONE_BYTE_STRING(isolate, "natives"),
                                GetNatives,
                                nullptr,
                                Local<Value>(),
                                None,
                                SideEffectType::kHasNoSideEffect);

  SetMethod(isolate, target, "getCacheUsage", BuiltinLoader::GetCacheUsage);
  SetMethod(isolate, target, "compileFunction", BuiltinLoader::CompileFunction);
  SetMethod(isolate, target, "hasCachedBuiltins", HasCachedBuiltins);
  SetMethod(isolate, target, "setInternalLoaders", SetInternalLoaders);
  SetMethod(isolate,
            target,
            "importBuiltinSourceTextModule",
            ImportBuiltinSourceTextModule);
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
  registry->Register(SetInternalLoaders);
  registry->Register(GetNatives);
  registry->Register(ImportBuiltinSourceTextModule);

  RegisterExternalReferencesForInternalizedBuiltinCode(registry);
}

}  // namespace builtins
}  // namespace node

NODE_BINDING_PER_ISOLATE_INIT(
    builtins, node::builtins::BuiltinLoader::CreatePerIsolateProperties)
NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    builtins, node::builtins::BuiltinLoader::CreatePerContextProperties)
NODE_BINDING_EXTERNAL_REFERENCE(
    builtins, node::builtins::BuiltinLoader::RegisterExternalReferences)
