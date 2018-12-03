#include "node_native_module.h"
#include "node_errors.h"
#include "node_internals.h"

namespace node {
namespace native_module {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferCreationMode;
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

void NativeModuleLoader::SourceObjectGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  Local<Context> context = info.GetIsolate()->GetCurrentContext();
  info.GetReturnValue().Set(per_process_loader.GetSourceObject(context));
}

void NativeModuleLoader::ConfigStringGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(
      per_process_loader.GetConfigString(info.GetIsolate()));
}

Local<Object> NativeModuleLoader::GetSourceObject(
    Local<Context> context) const {
  return MapToObject(context, source_);
}

Local<String> NativeModuleLoader::GetConfigString(Isolate* isolate) const {
  return config_.ToStringChecked(isolate);
}

Local<String> NativeModuleLoader::GetSource(Isolate* isolate,
                                            const char* id) const {
  const auto it = source_.find(id);
  CHECK_NE(it, source_.end());
  return it->second.ToStringChecked(isolate);
}

NativeModuleLoader::NativeModuleLoader() : config_(GetConfig()) {
  LoadJavaScriptSource();
  LoadCodeCache();
}

void NativeModuleLoader::CompileCodeCache(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  node::Utf8Value id(env->isolate(), args[0].As<String>());

  // TODO(joyeecheung): allow compiling cache for bootstrapper by
  // switching on id
  MaybeLocal<Value> result =
      CompileAsModule(env, *id, CompilationResultType::kCodeCache);
  if (!result.IsEmpty()) {
    args.GetReturnValue().Set(result.ToLocalChecked());
  }
}

void NativeModuleLoader::CompileFunction(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  node::Utf8Value id(env->isolate(), args[0].As<String>());

  MaybeLocal<Value> result =
      CompileAsModule(env, *id, CompilationResultType::kFunction);
  if (!result.IsEmpty()) {
    args.GetReturnValue().Set(result.ToLocalChecked());
  }
}

// TODO(joyeecheung): it should be possible to generate the argument names
// from some special comments for the bootstrapper case.
MaybeLocal<Value> NativeModuleLoader::CompileAndCall(
    Local<Context> context,
    const char* id,
    std::vector<Local<String>>* parameters,
    std::vector<Local<Value>>* arguments,
    Environment* optional_env) {
  Isolate* isolate = context->GetIsolate();
  MaybeLocal<Value> compiled = per_process_loader.LookupAndCompile(
      context, id, parameters, CompilationResultType::kFunction, nullptr);
  if (compiled.IsEmpty()) {
    return compiled;
  }
  Local<Function> fn = compiled.ToLocalChecked().As<Function>();
  return fn->Call(
      context, v8::Null(isolate), arguments->size(), arguments->data());
}

MaybeLocal<Value> NativeModuleLoader::CompileAsModule(
    Environment* env, const char* id, CompilationResultType result) {
  std::vector<Local<String>> parameters = {env->exports_string(),
                                           env->require_string(),
                                           env->module_string(),
                                           env->process_string(),
                                           env->internal_binding_string()};
  return per_process_loader.LookupAndCompile(
      env->context(), id, &parameters, result, env);
}

// Returns nullptr if there is no code cache corresponding to the id
ScriptCompiler::CachedData* NativeModuleLoader::GetCachedData(
    const char* id) const {
  const auto it = per_process_loader.code_cache_.find(id);
  // This could be false if the module cannot be cached somehow.
  // See lib/internal/bootstrap/cache.js on the modules that cannot be cached
  if (it == per_process_loader.code_cache_.end()) {
    return nullptr;
  }

  const uint8_t* code_cache_value = it->second.one_bytes_data();
  size_t code_cache_length = it->second.length();

  return new ScriptCompiler::CachedData(code_cache_value, code_cache_length);
}

// Returns Local<Function> of the compiled module if return_code_cache
// is false (we are only compiling the function).
// Otherwise return a Local<Object> containing the cache.
MaybeLocal<Value> NativeModuleLoader::LookupAndCompile(
    Local<Context> context,
    const char* id,
    std::vector<Local<String>>* parameters,
    CompilationResultType result_type,
    Environment* optional_env) {
  Isolate* isolate = context->GetIsolate();
  EscapableHandleScope scope(isolate);
  Local<Value> ret;  // Used to convert to MaybeLocal before return

  Local<String> source = GetSource(isolate, id);

  std::string filename_s = id + std::string(".js");
  Local<String> filename =
      OneByteString(isolate, filename_s.c_str(), filename_s.size());
  Local<Integer> line_offset = Integer::New(isolate, 0);
  Local<Integer> column_offset = Integer::New(isolate, 0);
  ScriptOrigin origin(filename, line_offset, column_offset);

  bool use_cache = false;
  ScriptCompiler::CachedData* cached_data = nullptr;

  // 1. We won't even check the existence of the cache if the binary is not
  //    built with them.
  // 2. If we are generating code cache for tools/general_code_cache.js, we
  //    are not going to use any cache ourselves.
  if (has_code_cache_ && result_type == CompilationResultType::kFunction) {
    cached_data = GetCachedData(id);
    if (cached_data != nullptr) {
      use_cache = true;
    }
  }

  ScriptCompiler::Source script_source(source, origin, cached_data);

  ScriptCompiler::CompileOptions options;
  if (result_type == CompilationResultType::kCodeCache) {
    options = ScriptCompiler::kEagerCompile;
  } else if (use_cache) {
    options = ScriptCompiler::kConsumeCodeCache;
  } else {
    options = ScriptCompiler::kNoCompileOptions;
  }

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
    return MaybeLocal<Value>();
  }

  Local<Function> fun = maybe_fun.ToLocalChecked();
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

  if (result_type == CompilationResultType::kCodeCache) {
    std::unique_ptr<ScriptCompiler::CachedData> cached_data(
        ScriptCompiler::CreateCodeCacheForFunction(fun));
    CHECK_NE(cached_data, nullptr);
    size_t cached_data_length = cached_data->length;
    // Since we have no special allocator to create an ArrayBuffer
    // from a new'ed pointer, we will need to copy it - but this
    // code path is only run by the tooling that generates the code
    // cache to be bundled in the binary
    // so it should be fine.
    MallocedBuffer<uint8_t> copied(cached_data->length);
    memcpy(copied.data, cached_data->data, cached_data_length);
    Local<ArrayBuffer> buf =
        ArrayBuffer::New(isolate,
                         copied.release(),
                         cached_data_length,
                         ArrayBufferCreationMode::kInternalized);
    ret = Uint8Array::New(buf, 0, cached_data_length);
  } else {
    ret = fun;
  }

  return scope.Escape(ret);
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
                          env->source_string(),
                          SourceObjectGetter,
                          nullptr,
                          MaybeLocal<Value>(),
                          DEFAULT,
                          None,
                          SideEffectType::kHasNoSideEffect)
            .FromJust());

  env->SetMethod(
      target, "getCacheUsage", NativeModuleLoader::GetCacheUsage);
  env->SetMethod(
      target, "compileFunction", NativeModuleLoader::CompileFunction);
  env->SetMethod(
      target, "compileCodeCache", NativeModuleLoader::CompileCodeCache);
  // internalBinding('native_module') should be frozen
  target->SetIntegrityLevel(context, IntegrityLevel::kFrozen).FromJust();
}

}  // namespace native_module
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(
    native_module, node::native_module::NativeModuleLoader::Initialize)
