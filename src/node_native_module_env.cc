#include "node_native_module_env.h"
#include "env-inl.h"

namespace node {
namespace native_module {

using v8::Context;
using v8::DEFAULT;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::IntegrityLevel;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::None;
using v8::Object;
using v8::PropertyCallbackInfo;
using v8::Set;
using v8::SideEffectType;
using v8::String;
using v8::Value;

// TODO(joyeecheung): make these more general and put them into util.h
Local<Set> ToJsSet(Local<Context> context, const std::set<std::string>& in) {
  Isolate* isolate = context->GetIsolate();
  Local<Set> out = Set::New(isolate);
  for (auto const& x : in) {
    out->Add(context, OneByteString(isolate, x.c_str(), x.size()))
        .ToLocalChecked();
  }
  return out;
}

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

void NativeModuleEnv::GetCacheUsage(const FunctionCallbackInfo<Value>& args) {
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
  if (!maybe.IsEmpty()) {
    args.GetReturnValue().Set(maybe.ToLocalChecked());
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
                    env->as_callback_data(),
                    DEFAULT,
                    None,
                    SideEffectType::kHasNoSideEffect)
      .Check();

  env->SetMethod(target, "getCacheUsage", NativeModuleEnv::GetCacheUsage);
  env->SetMethod(target, "compileFunction", NativeModuleEnv::CompileFunction);
  // internalBinding('native_module') should be frozen
  target->SetIntegrityLevel(context, IntegrityLevel::kFrozen).FromJust();
}

}  // namespace native_module
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(
    native_module, node::native_module::NativeModuleEnv::Initialize)
