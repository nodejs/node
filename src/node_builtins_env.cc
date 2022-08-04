#include "node_builtins_env.h"
#include "env-inl.h"
#include "node_external_reference.h"

namespace node {
namespace builtins {

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

bool BuiltinEnv::Add(const char* id, const UnionBytes& source) {
  return BuiltinLoader::GetInstance()->Add(id, source);
}

bool BuiltinEnv::Exists(const char* id) {
  return BuiltinLoader::GetInstance()->Exists(id);
}

Local<Object> BuiltinEnv::GetSourceObject(Local<Context> context) {
  return BuiltinLoader::GetInstance()->GetSourceObject(context);
}

Local<String> BuiltinEnv::GetConfigString(Isolate* isolate) {
  return BuiltinLoader::GetInstance()->GetConfigString(isolate);
}

void BuiltinEnv::GetModuleCategories(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Object> result = Object::New(isolate);

  // Copy from the per-process categories
  std::set<std::string> cannot_be_required =
      BuiltinLoader::GetInstance()->GetCannotBeRequired();
  std::set<std::string> can_be_required =
      BuiltinLoader::GetInstance()->GetCanBeRequired();

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

void BuiltinEnv::GetCacheUsage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<Object> result = Object::New(isolate);
  result
      ->Set(env->context(),
            OneByteString(isolate, "compiledWithCache"),
            ToJsSet(context, env->builtins_with_cache))
      .FromJust();
  result
      ->Set(env->context(),
            OneByteString(isolate, "compiledWithoutCache"),
            ToJsSet(context, env->builtins_without_cache))
      .FromJust();

  result
      ->Set(env->context(),
            OneByteString(isolate, "compiledInSnapshot"),
            ToV8Value(env->context(), env->builtins_in_snapshot)
                .ToLocalChecked())
      .FromJust();

  args.GetReturnValue().Set(result);
}

void BuiltinEnv::ModuleIdsGetter(Local<Name> property,
                                      const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();

  std::vector<std::string> ids =
      BuiltinLoader::GetInstance()->GetBuiltinIds();
  info.GetReturnValue().Set(
      ToV8Value(isolate->GetCurrentContext(), ids).ToLocalChecked());
}

void BuiltinEnv::ConfigStringGetter(
    Local<Name> property, const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(GetConfigString(info.GetIsolate()));
}

void BuiltinEnv::RecordResult(const char* id,
                                   BuiltinLoader::Result result,
                                   Environment* env) {
  if (result == BuiltinLoader::Result::kWithCache) {
    env->builtins_with_cache.insert(id);
  } else {
    env->builtins_without_cache.insert(id);
  }
}
void BuiltinEnv::CompileFunction(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  node::Utf8Value id_v(env->isolate(), args[0].As<String>());
  const char* id = *id_v;
  BuiltinLoader::Result result;
  MaybeLocal<Function> maybe =
      BuiltinLoader::GetInstance()->CompileAsModule(
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
MaybeLocal<Function> BuiltinEnv::LookupAndCompile(
    Local<Context> context,
    const char* id,
    std::vector<Local<String>>* parameters,
    Environment* optional_env) {
  BuiltinLoader::Result result;
  MaybeLocal<Function> maybe =
      BuiltinLoader::GetInstance()->LookupAndCompile(
          context, id, parameters, &result);
  if (optional_env != nullptr) {
    RecordResult(id, result, optional_env);
  }
  return maybe;
}

void HasCachedBuiltins(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
      v8::Boolean::New(args.GetIsolate(), has_code_cache));
}

// TODO(joyeecheung): It is somewhat confusing that Class::Initialize
// is used to initialize to the binding, but it is the current convention.
// Rename this across the code base to something that makes more sense.
void BuiltinEnv::Initialize(Local<Object> target,
                                 Local<Value> unused,
                                 Local<Context> context,
                                 void* priv) {
  Environment* env = Environment::GetCurrent(context);

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
                    FIXED_ONE_BYTE_STRING(env->isolate(), "builtinIds"),
                    ModuleIdsGetter,
                    nullptr,
                    MaybeLocal<Value>(),
                    DEFAULT,
                    None,
                    SideEffectType::kHasNoSideEffect)
      .Check();

  target
      ->SetAccessor(context,
                    FIXED_ONE_BYTE_STRING(env->isolate(), "moduleCategories"),
                    GetModuleCategories,
                    nullptr,
                    Local<Value>(),
                    DEFAULT,
                    None,
                    SideEffectType::kHasNoSideEffect)
      .Check();

  SetMethod(context, target, "getCacheUsage", BuiltinEnv::GetCacheUsage);
  SetMethod(
      context, target, "compileFunction", BuiltinEnv::CompileFunction);
  SetMethod(context, target, "hasCachedBuiltins", HasCachedBuiltins);
  // internalBinding('builtins') should be frozen
  target->SetIntegrityLevel(context, IntegrityLevel::kFrozen).FromJust();
}

void BuiltinEnv::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(ConfigStringGetter);
  registry->Register(ModuleIdsGetter);
  registry->Register(GetModuleCategories);
  registry->Register(GetCacheUsage);
  registry->Register(CompileFunction);
  registry->Register(HasCachedBuiltins);
}

}  // namespace builtins
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(
    builtins, node::builtins::BuiltinEnv::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(
    builtins,
    node::builtins::BuiltinEnv::RegisterExternalReferences)
