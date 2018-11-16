#include "node_native_module.h"
#include "node_errors.h"
#include "node_internals.h"

namespace node {
namespace native_module {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferCreationMode;
using v8::Context;
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
using v8::Object;
using v8::Script;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::Set;
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

void NativeModuleLoader::GetSourceObject(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  args.GetReturnValue().Set(per_process_loader.GetSourceObject(env->context()));
}

Local<Object> NativeModuleLoader::GetSourceObject(
    Local<Context> context) const {
  return MapToObject(context, source_);
}

Local<String> NativeModuleLoader::GetSource(Isolate* isolate,
                                            const char* id) const {
  const auto it = source_.find(id);
  CHECK_NE(it, source_.end());
  return it->second.ToStringChecked(isolate);
}

NativeModuleLoader::NativeModuleLoader() {
  LoadJavaScriptSource();
  LoadJavaScriptHash();
  LoadCodeCache();
  LoadCodeCacheHash();
}

void NativeModuleLoader::CompileCodeCache(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  node::Utf8Value id(env->isolate(), args[0].As<String>());

  // TODO(joyeecheung): allow compiling cache for bootstrapper by
  // switching on id
  Local<Value> result = CompileAsModule(env, *id, true);
  if (!result.IsEmpty()) {
    args.GetReturnValue().Set(result);
  }
}

void NativeModuleLoader::CompileFunction(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsString());
  node::Utf8Value id(env->isolate(), args[0].As<String>());
  Local<Value> result = CompileAsModule(env, *id, false);
  if (!result.IsEmpty()) {
    args.GetReturnValue().Set(result);
  }
}

Local<Value> NativeModuleLoader::CompileAsModule(Environment* env,
                                                 const char* id,
                                                 bool produce_code_cache) {
  return per_process_loader.LookupAndCompile(
      env->context(), id, produce_code_cache, env);
}

// Currently V8 only checks that the length of the source code is the
// same as the code used to generate the hash, so we add an additional
// check here:
// 1. During compile time, when generating node_javascript.cc and
//    node_code_cache.cc, we compute and include the hash of the
//    JavaScript source in both.
// 2. At runtime, we check that the hash of the code being compiled
//   and the hash of the code used to generate the cache
//   (without the parameters) is the same.
// This is based on the assumptions:
// 1. `code_cache_hash` must be in sync with `code_cache`
//     (both defined in node_code_cache.cc)
// 2. `source_hash` must be in sync with `source`
//     (both defined in node_javascript.cc)
// 3. If `source_hash` is in sync with `code_cache_hash`,
//    then the source code used to generate `code_cache`
//    should be in sync with the source code in `source`
// The only variable left, then, are the parameters passed to the
// CompileFunctionInContext. If the parameters used generate the cache
// is different from the one used to compile modules at run time, then
// there could be false postivies, but that should be rare and should fail
// early in the bootstrap process so it should be easy to detect and fix.

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

  const auto it2 = code_cache_hash_.find(id);
  CHECK_NE(it2, code_cache_hash_.end());
  const std::string& code_cache_hash_value = it2->second;

  const auto it3 = source_hash_.find(id);
  CHECK_NE(it3, source_hash_.end());
  const std::string& source_hash_value = it3->second;

  // It may fail when any of the inputs of the `node_js2c` target in
  // node.gyp is modified but the tools/generate_code_cache.js
  // is not re run.
  // FIXME(joyeecheung): Figure out how to resolve the dependency issue.
  // When the code cache was introduced we were at a point where refactoring
  // node.gyp may not be worth the effort.
  CHECK_EQ(code_cache_hash_value, source_hash_value);

  return new ScriptCompiler::CachedData(code_cache_value, code_cache_length);
}

// Returns Local<Function> of the compiled module if produce_code_cache
// is false (we are only compiling the function).
// Otherwise return a Local<Object> containing the cache.
Local<Value> NativeModuleLoader::LookupAndCompile(Local<Context> context,
                                                  const char* id,
                                                  bool produce_code_cache,
                                                  Environment* optional_env) {
  Isolate* isolate = context->GetIsolate();
  EscapableHandleScope scope(isolate);

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
  if (has_code_cache_ && !produce_code_cache) {
    cached_data = GetCachedData(id);
    if (cached_data != nullptr) {
      use_cache = true;
    }
  }

  ScriptCompiler::Source script_source(source, origin, cached_data);

  ScriptCompiler::CompileOptions options;
  if (produce_code_cache) {
    options = ScriptCompiler::kEagerCompile;
  } else if (use_cache) {
    options = ScriptCompiler::kConsumeCodeCache;
  } else {
    options = ScriptCompiler::kNoCompileOptions;
  }

  MaybeLocal<Function> maybe_fun;
  // Currently we assume if Environment is ready, then we must be compiling
  // native modules instead of bootstrappers.
  if (optional_env != nullptr) {
    Local<String> parameters[] = {optional_env->exports_string(),
                                  optional_env->require_string(),
                                  optional_env->module_string(),
                                  optional_env->process_string(),
                                  optional_env->internal_binding_string()};
    maybe_fun = ScriptCompiler::CompileFunctionInContext(context,
                                                         &script_source,
                                                         arraysize(parameters),
                                                         parameters,
                                                         0,
                                                         nullptr,
                                                         options);
  } else {
    // Until we migrate bootstrappers compilations here this is unreachable
    // TODO(joyeecheung): it should be possible to generate the argument names
    // from some special comments for the bootstrapper case.
    // Note that for bootstrappers we may not be able to get the argument
    // names as env->some_string() because we might be compiling before
    // those strings are initialized.
    UNREACHABLE();
  }

  Local<Function> fun;
  // This could fail when there are early errors in the native modules,
  // e.g. the syntax errors
  if (maybe_fun.IsEmpty() || !maybe_fun.ToLocal(&fun)) {
    // In the case of early errors, v8 is already capable of
    // decorating the stack for us - note that we use CompileFunctionInContext
    // so there is no need to worry about wrappers.
    return scope.Escape(Local<Value>());
  }

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

  if (produce_code_cache) {
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
    return scope.Escape(Uint8Array::New(buf, 0, cached_data_length));
  } else {
    return scope.Escape(fun);
  }
}

void NativeModuleLoader::Initialize(Local<Object> target,
                                    Local<Value> unused,
                                    Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(
      target, "getSource", NativeModuleLoader::GetSourceObject);
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
