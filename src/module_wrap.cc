#include "module_wrap.h"

#include "env.h"
#include "memory_tracker-inl.h"
#include "node_contextify.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_process.h"
#include "node_url.h"
#include "node_watchdog.h"
#include "util-inl.h"

#include <sys/stat.h>  // S_IFDIR

#include <algorithm>

namespace node {
namespace loader {

using errors::TryCatchScope;

using node::contextify::ContextifyContext;
using node::url::URL;
using node::url::URL_FLAGS_FAILED;
using v8::Array;
using v8::ArrayBufferView;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::IntegrityLevel;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::MicrotaskQueue;
using v8::Module;
using v8::Number;
using v8::Object;
using v8::PrimitiveArray;
using v8::Promise;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::ScriptOrModule;
using v8::String;
using v8::UnboundModuleScript;
using v8::Undefined;
using v8::Value;

ModuleWrap::ModuleWrap(Environment* env,
                       Local<Object> object,
                       Local<Module> module,
                       Local<String> url)
  : BaseObject(env, object),
    module_(env->isolate(), module),
    id_(env->get_next_module_id()) {
  env->id_to_module_map.emplace(id_, this);

  Local<Value> undefined = Undefined(env->isolate());
  object->SetInternalField(kURLSlot, url);
  object->SetInternalField(kSyntheticEvaluationStepsSlot, undefined);
  object->SetInternalField(kContextObjectSlot, undefined);
}

ModuleWrap::~ModuleWrap() {
  HandleScope scope(env()->isolate());
  Local<Module> module = module_.Get(env()->isolate());
  env()->id_to_module_map.erase(id_);
  auto range = env()->hash_to_module_map.equal_range(module->GetIdentityHash());
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == this) {
      env()->hash_to_module_map.erase(it);
      break;
    }
  }
}

Local<Context> ModuleWrap::context() const {
  Local<Value> obj = object()->GetInternalField(kContextObjectSlot);
  if (obj.IsEmpty()) return {};
  return obj.As<Object>()->CreationContext();
}

ModuleWrap* ModuleWrap::GetFromModule(Environment* env,
                                      Local<Module> module) {
  auto range = env->hash_to_module_map.equal_range(module->GetIdentityHash());
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->module_ == module) {
      return it->second;
    }
  }
  return nullptr;
}

ModuleWrap* ModuleWrap::GetFromID(Environment* env, uint32_t id) {
  auto module_wrap_it = env->id_to_module_map.find(id);
  if (module_wrap_it == env->id_to_module_map.end()) {
    return nullptr;
  }
  return module_wrap_it->second;
}

// new ModuleWrap(url, context, source, lineOffset, columnOffset)
// new ModuleWrap(url, context, exportNames, syntheticExecutionFunction)
void ModuleWrap::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  CHECK_GE(args.Length(), 3);

  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  Local<Object> that = args.This();

  CHECK(args[0]->IsString());
  Local<String> url = args[0].As<String>();

  Local<Context> context;
  ContextifyContext* contextify_context = nullptr;
  if (args[1]->IsUndefined()) {
    context = that->CreationContext();
  } else {
    CHECK(args[1]->IsObject());
    contextify_context = ContextifyContext::ContextFromContextifiedSandbox(
        env, args[1].As<Object>());
    CHECK_NOT_NULL(contextify_context);
    context = contextify_context->context();
  }

  Local<Integer> line_offset;
  Local<Integer> column_offset;

  bool synthetic = args[2]->IsArray();
  if (synthetic) {
    // new ModuleWrap(url, context, exportNames, syntheticExecutionFunction)
    CHECK(args[3]->IsFunction());
  } else {
    // new ModuleWrap(url, context, source, lineOffset, columOffset, cachedData)
    CHECK(args[2]->IsString());
    CHECK(args[3]->IsNumber());
    line_offset = args[3].As<Integer>();
    CHECK(args[4]->IsNumber());
    column_offset = args[4].As<Integer>();
  }

  Local<PrimitiveArray> host_defined_options =
      PrimitiveArray::New(isolate, HostDefinedOptions::kLength);
  host_defined_options->Set(isolate, HostDefinedOptions::kType,
                            Number::New(isolate, ScriptType::kModule));

  ShouldNotAbortOnUncaughtScope no_abort_scope(env);
  TryCatchScope try_catch(env);

  Local<Module> module;

  {
    Context::Scope context_scope(context);
    if (synthetic) {
      CHECK(args[2]->IsArray());
      Local<Array> export_names_arr = args[2].As<Array>();

      uint32_t len = export_names_arr->Length();
      std::vector<Local<String>> export_names(len);
      for (uint32_t i = 0; i < len; i++) {
        Local<Value> export_name_val =
            export_names_arr->Get(context, i).ToLocalChecked();
        CHECK(export_name_val->IsString());
        export_names[i] = export_name_val.As<String>();
      }

      module = Module::CreateSyntheticModule(isolate, url, export_names,
        SyntheticModuleEvaluationStepsCallback);
    } else {
      ScriptCompiler::CachedData* cached_data = nullptr;
      if (!args[5]->IsUndefined()) {
        CHECK(args[5]->IsArrayBufferView());
        Local<ArrayBufferView> cached_data_buf = args[5].As<ArrayBufferView>();
        uint8_t* data = static_cast<uint8_t*>(
            cached_data_buf->Buffer()->GetBackingStore()->Data());
        cached_data =
            new ScriptCompiler::CachedData(data + cached_data_buf->ByteOffset(),
                                           cached_data_buf->ByteLength());
      }

      Local<String> source_text = args[2].As<String>();
      ScriptOrigin origin(url,
                          line_offset,                      // line offset
                          column_offset,                    // column offset
                          True(isolate),                    // is cross origin
                          Local<Integer>(),                 // script id
                          Local<Value>(),                   // source map URL
                          False(isolate),                   // is opaque (?)
                          False(isolate),                   // is WASM
                          True(isolate),                    // is ES Module
                          host_defined_options);
      ScriptCompiler::Source source(source_text, origin, cached_data);
      ScriptCompiler::CompileOptions options;
      if (source.GetCachedData() == nullptr) {
        options = ScriptCompiler::kNoCompileOptions;
      } else {
        options = ScriptCompiler::kConsumeCodeCache;
      }
      if (!ScriptCompiler::CompileModule(isolate, &source, options)
               .ToLocal(&module)) {
        if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
          CHECK(!try_catch.Message().IsEmpty());
          CHECK(!try_catch.Exception().IsEmpty());
          AppendExceptionLine(env, try_catch.Exception(), try_catch.Message(),
                              ErrorHandlingMode::MODULE_ERROR);
          try_catch.ReThrow();
        }
        return;
      }
      if (options == ScriptCompiler::kConsumeCodeCache &&
          source.GetCachedData()->rejected) {
        THROW_ERR_VM_MODULE_CACHED_DATA_REJECTED(
            env, "cachedData buffer was rejected");
        try_catch.ReThrow();
        return;
      }
    }
  }

  if (!that->Set(context, env->url_string(), url).FromMaybe(false)) {
    return;
  }

  ModuleWrap* obj = new ModuleWrap(env, that, module, url);

  if (synthetic) {
    obj->synthetic_ = true;
    obj->object()->SetInternalField(kSyntheticEvaluationStepsSlot, args[3]);
  }

  // Use the extras object as an object whose CreationContext() will be the
  // original `context`, since the `Context` itself strictly speaking cannot
  // be stored in an internal field.
  obj->object()->SetInternalField(kContextObjectSlot,
      context->GetExtrasBindingObject());
  obj->contextify_context_ = contextify_context;

  env->hash_to_module_map.emplace(module->GetIdentityHash(), obj);

  host_defined_options->Set(isolate, HostDefinedOptions::kID,
                            Number::New(isolate, obj->id()));

  that->SetIntegrityLevel(context, IntegrityLevel::kFrozen);
  args.GetReturnValue().Set(that);
}

void ModuleWrap::Link(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());

  Local<Object> that = args.This();

  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, that);

  if (obj->linked_)
    return;
  obj->linked_ = true;

  Local<Function> resolver_arg = args[0].As<Function>();

  Local<Context> mod_context = obj->context();
  Local<Module> module = obj->module_.Get(isolate);

  const int module_requests_length = module->GetModuleRequestsLength();
  MaybeStackBuffer<Local<Value>, 16> promises(module_requests_length);

  // call the dependency resolve callbacks
  for (int i = 0; i < module_requests_length; i++) {
    Local<String> specifier = module->GetModuleRequest(i);
    Utf8Value specifier_utf8(env->isolate(), specifier);
    std::string specifier_std(*specifier_utf8, specifier_utf8.length());

    Local<Value> argv[] = {
      specifier
    };

    MaybeLocal<Value> maybe_resolve_return_value =
        resolver_arg->Call(mod_context, that, arraysize(argv), argv);
    if (maybe_resolve_return_value.IsEmpty()) {
      return;
    }
    Local<Value> resolve_return_value =
        maybe_resolve_return_value.ToLocalChecked();
    if (!resolve_return_value->IsPromise()) {
      env->ThrowError("linking error, expected resolver to return a promise");
    }
    Local<Promise> resolve_promise = resolve_return_value.As<Promise>();
    obj->resolve_cache_[specifier_std].Reset(env->isolate(), resolve_promise);

    promises[i] = resolve_promise;
  }

  args.GetReturnValue().Set(
      Array::New(isolate, promises.out(), promises.length()));
}

void ModuleWrap::Instantiate(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();
  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.This());
  Local<Context> context = obj->context();
  Local<Module> module = obj->module_.Get(isolate);
  TryCatchScope try_catch(env);
  USE(module->InstantiateModule(context, ResolveCallback));

  // clear resolve cache on instantiate
  obj->resolve_cache_.clear();

  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    CHECK(!try_catch.Message().IsEmpty());
    CHECK(!try_catch.Exception().IsEmpty());
    AppendExceptionLine(env, try_catch.Exception(), try_catch.Message(),
                        ErrorHandlingMode::MODULE_ERROR);
    try_catch.ReThrow();
    return;
  }
}

void ModuleWrap::Evaluate(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.This());
  Local<Context> context = obj->context();
  Local<Module> module = obj->module_.Get(isolate);

  ContextifyContext* contextify_context = obj->contextify_context_;
  std::shared_ptr<MicrotaskQueue> microtask_queue;
  if (contextify_context != nullptr)
      microtask_queue = contextify_context->microtask_queue();

  // module.evaluate(timeout, breakOnSigint)
  CHECK_EQ(args.Length(), 2);

  CHECK(args[0]->IsNumber());
  int64_t timeout = args[0]->IntegerValue(env->context()).FromJust();

  CHECK(args[1]->IsBoolean());
  bool break_on_sigint = args[1]->IsTrue();

  ShouldNotAbortOnUncaughtScope no_abort_scope(env);
  TryCatchScope try_catch(env);
  Isolate::SafeForTerminationScope safe_for_termination(env->isolate());

  bool timed_out = false;
  bool received_signal = false;
  MaybeLocal<Value> result;
  auto run = [&]() {
    MaybeLocal<Value> result = module->Evaluate(context);
    if (!result.IsEmpty() && microtask_queue)
      microtask_queue->PerformCheckpoint(isolate);
    return result;
  };
  if (break_on_sigint && timeout != -1) {
    Watchdog wd(isolate, timeout, &timed_out);
    SigintWatchdog swd(isolate, &received_signal);
    result = run();
  } else if (break_on_sigint) {
    SigintWatchdog swd(isolate, &received_signal);
    result = run();
  } else if (timeout != -1) {
    Watchdog wd(isolate, timeout, &timed_out);
    result = run();
  } else {
    result = run();
  }

  if (result.IsEmpty()) {
    CHECK(try_catch.HasCaught());
  }

  // Convert the termination exception into a regular exception.
  if (timed_out || received_signal) {
    if (!env->is_main_thread() && env->is_stopping())
      return;
    env->isolate()->CancelTerminateExecution();
    // It is possible that execution was terminated by another timeout in
    // which this timeout is nested, so check whether one of the watchdogs
    // from this invocation is responsible for termination.
    if (timed_out) {
      THROW_ERR_SCRIPT_EXECUTION_TIMEOUT(env, timeout);
    } else if (received_signal) {
      THROW_ERR_SCRIPT_EXECUTION_INTERRUPTED(env);
    }
  }

  if (try_catch.HasCaught()) {
    if (!try_catch.HasTerminated())
      try_catch.ReThrow();
    return;
  }

  // If TLA is enabled, `result` is the evaluation's promise.
  // Otherwise, `result` is the last evaluated value of the module,
  // which could be a promise, which would result in it being incorrectly
  // unwrapped when the higher level code awaits the evaluation.
  if (env->isolate_data()->options()->experimental_top_level_await) {
    args.GetReturnValue().Set(result.ToLocalChecked());
  }
}

void ModuleWrap::GetNamespace(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();
  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.This());

  Local<Module> module = obj->module_.Get(isolate);

  switch (module->GetStatus()) {
    case v8::Module::Status::kUninstantiated:
    case v8::Module::Status::kInstantiating:
      return env->ThrowError(
          "cannot get namespace, module has not been instantiated");
    case v8::Module::Status::kInstantiated:
    case v8::Module::Status::kEvaluating:
    case v8::Module::Status::kEvaluated:
    case v8::Module::Status::kErrored:
      break;
    default:
      UNREACHABLE();
  }

  Local<Value> result = module->GetModuleNamespace();
  args.GetReturnValue().Set(result);
}

void ModuleWrap::GetStatus(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.This());

  Local<Module> module = obj->module_.Get(isolate);

  args.GetReturnValue().Set(module->GetStatus());
}

void ModuleWrap::GetStaticDependencySpecifiers(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.This());

  Local<Module> module = obj->module_.Get(env->isolate());

  int count = module->GetModuleRequestsLength();

  MaybeStackBuffer<Local<Value>, 16> specifiers(count);

  for (int i = 0; i < count; i++)
    specifiers[i] = module->GetModuleRequest(i);

  args.GetReturnValue().Set(
      Array::New(env->isolate(), specifiers.out(), count));
}

void ModuleWrap::GetError(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.This());

  Local<Module> module = obj->module_.Get(isolate);
  args.GetReturnValue().Set(module->GetException());
}

MaybeLocal<Module> ModuleWrap::ResolveCallback(Local<Context> context,
                                               Local<String> specifier,
                                               Local<Module> referrer) {
  Environment* env = Environment::GetCurrent(context);
  if (env == nullptr) {
    Isolate* isolate = context->GetIsolate();
    THROW_ERR_EXECUTION_ENVIRONMENT_NOT_AVAILABLE(isolate);
    return MaybeLocal<Module>();
  }

  Isolate* isolate = env->isolate();

  ModuleWrap* dependent = GetFromModule(env, referrer);
  if (dependent == nullptr) {
    env->ThrowError("linking error, null dep");
    return MaybeLocal<Module>();
  }

  Utf8Value specifier_utf8(isolate, specifier);
  std::string specifier_std(*specifier_utf8, specifier_utf8.length());

  if (dependent->resolve_cache_.count(specifier_std) != 1) {
    env->ThrowError("linking error, not in local cache");
    return MaybeLocal<Module>();
  }

  Local<Promise> resolve_promise =
      dependent->resolve_cache_[specifier_std].Get(isolate);

  if (resolve_promise->State() != Promise::kFulfilled) {
    env->ThrowError("linking error, dependency promises must be resolved on "
                    "instantiate");
    return MaybeLocal<Module>();
  }

  Local<Object> module_object = resolve_promise->Result().As<Object>();
  if (module_object.IsEmpty() || !module_object->IsObject()) {
    env->ThrowError("linking error, expected a valid module object from "
                    "resolver");
    return MaybeLocal<Module>();
  }

  ModuleWrap* module;
  ASSIGN_OR_RETURN_UNWRAP(&module, module_object, MaybeLocal<Module>());
  return module->module_.Get(isolate);
}

static MaybeLocal<Promise> ImportModuleDynamically(
    Local<Context> context,
    Local<ScriptOrModule> referrer,
    Local<String> specifier) {
  Isolate* iso = context->GetIsolate();
  Environment* env = Environment::GetCurrent(context);
  if (env == nullptr) {
    THROW_ERR_EXECUTION_ENVIRONMENT_NOT_AVAILABLE(iso);
    return MaybeLocal<Promise>();
  }

  EscapableHandleScope handle_scope(iso);

  Local<Function> import_callback =
    env->host_import_module_dynamically_callback();

  Local<PrimitiveArray> options = referrer->GetHostDefinedOptions();
  if (options->Length() != HostDefinedOptions::kLength) {
    Local<Promise::Resolver> resolver;
    if (!Promise::Resolver::New(context).ToLocal(&resolver)) return {};
    resolver
        ->Reject(context,
                 v8::Exception::TypeError(FIXED_ONE_BYTE_STRING(
                     context->GetIsolate(), "Invalid host defined options")))
        .ToChecked();
    return handle_scope.Escape(resolver->GetPromise());
  }

  Local<Value> object;

  int type = options->Get(iso, HostDefinedOptions::kType)
                 .As<Number>()
                 ->Int32Value(context)
                 .ToChecked();
  uint32_t id = options->Get(iso, HostDefinedOptions::kID)
                    .As<Number>()
                    ->Uint32Value(context)
                    .ToChecked();
  if (type == ScriptType::kScript) {
    contextify::ContextifyScript* wrap = env->id_to_script_map.find(id)->second;
    object = wrap->object();
  } else if (type == ScriptType::kModule) {
    ModuleWrap* wrap = ModuleWrap::GetFromID(env, id);
    object = wrap->object();
  } else if (type == ScriptType::kFunction) {
    auto it = env->id_to_function_map.find(id);
    CHECK_NE(it, env->id_to_function_map.end());
    object = it->second->object();
  } else {
    UNREACHABLE();
  }

  Local<Value> import_args[] = {
    object,
    Local<Value>(specifier),
  };

  Local<Value> result;
  if (import_callback->Call(
        context,
        Undefined(iso),
        arraysize(import_args),
        import_args).ToLocal(&result)) {
    CHECK(result->IsPromise());
    return handle_scope.Escape(result.As<Promise>());
  }

  return MaybeLocal<Promise>();
}

void ModuleWrap::SetImportModuleDynamicallyCallback(
    const FunctionCallbackInfo<Value>& args) {
  Isolate* iso = args.GetIsolate();
  Environment* env = Environment::GetCurrent(args);
  HandleScope handle_scope(iso);

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  Local<Function> import_callback = args[0].As<Function>();
  env->set_host_import_module_dynamically_callback(import_callback);

  iso->SetHostImportModuleDynamicallyCallback(ImportModuleDynamically);
}

void ModuleWrap::HostInitializeImportMetaObjectCallback(
    Local<Context> context, Local<Module> module, Local<Object> meta) {
  Environment* env = Environment::GetCurrent(context);
  if (env == nullptr)
    return;
  ModuleWrap* module_wrap = GetFromModule(env, module);

  if (module_wrap == nullptr) {
    return;
  }

  Local<Object> wrap = module_wrap->object();
  Local<Function> callback =
      env->host_initialize_import_meta_object_callback();
  Local<Value> args[] = { wrap, meta };
  TryCatchScope try_catch(env);
  USE(callback->Call(
        context, Undefined(env->isolate()), arraysize(args), args));
  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    try_catch.ReThrow();
  }
}

void ModuleWrap::SetInitializeImportMetaObjectCallback(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  Local<Function> import_meta_callback = args[0].As<Function>();
  env->set_host_initialize_import_meta_object_callback(import_meta_callback);

  isolate->SetHostInitializeImportMetaObjectCallback(
      HostInitializeImportMetaObjectCallback);
}

MaybeLocal<Value> ModuleWrap::SyntheticModuleEvaluationStepsCallback(
    Local<Context> context, Local<Module> module) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  ModuleWrap* obj = GetFromModule(env, module);

  TryCatchScope try_catch(env);
  Local<Function> synthetic_evaluation_steps =
      obj->object()->GetInternalField(kSyntheticEvaluationStepsSlot)
          .As<Function>();
  obj->object()->SetInternalField(
      kSyntheticEvaluationStepsSlot, Undefined(isolate));
  MaybeLocal<Value> ret = synthetic_evaluation_steps->Call(context,
      obj->object(), 0, nullptr);
  if (ret.IsEmpty()) {
    CHECK(try_catch.HasCaught());
  }
  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    CHECK(!try_catch.Message().IsEmpty());
    CHECK(!try_catch.Exception().IsEmpty());
    try_catch.ReThrow();
    return MaybeLocal<Value>();
  }
  return Undefined(isolate);
}

void ModuleWrap::SetSyntheticExport(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Object> that = args.This();

  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, that);

  CHECK(obj->synthetic_);

  CHECK_EQ(args.Length(), 2);

  CHECK(args[0]->IsString());
  Local<String> export_name = args[0].As<String>();

  Local<Value> export_value = args[1];

  Local<Module> module = obj->module_.Get(isolate);
  USE(module->SetSyntheticModuleExport(isolate, export_name, export_value));
}

void ModuleWrap::CreateCachedData(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Object> that = args.This();

  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, that);

  CHECK(!obj->synthetic_);

  Local<Module> module = obj->module_.Get(isolate);

  CHECK_LT(module->GetStatus(), v8::Module::Status::kEvaluating);

  Local<UnboundModuleScript> unbound_module_script =
      module->GetUnboundModuleScript();
  std::unique_ptr<ScriptCompiler::CachedData> cached_data(
      ScriptCompiler::CreateCodeCache(unbound_module_script));
  Environment* env = Environment::GetCurrent(args);
  if (!cached_data) {
    args.GetReturnValue().Set(Buffer::New(env, 0).ToLocalChecked());
  } else {
    MaybeLocal<Object> buf =
        Buffer::Copy(env,
                     reinterpret_cast<const char*>(cached_data->data),
                     cached_data->length);
    args.GetReturnValue().Set(buf.ToLocalChecked());
  }
}

void ModuleWrap::Initialize(Local<Object> target,
                            Local<Value> unused,
                            Local<Context> context,
                            void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  Local<FunctionTemplate> tpl = env->NewFunctionTemplate(New);
  tpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "ModuleWrap"));
  tpl->InstanceTemplate()->SetInternalFieldCount(
      ModuleWrap::kInternalFieldCount);
  tpl->Inherit(BaseObject::GetConstructorTemplate(env));

  env->SetProtoMethod(tpl, "link", Link);
  env->SetProtoMethod(tpl, "instantiate", Instantiate);
  env->SetProtoMethod(tpl, "evaluate", Evaluate);
  env->SetProtoMethod(tpl, "setExport", SetSyntheticExport);
  env->SetProtoMethodNoSideEffect(tpl, "createCachedData", CreateCachedData);
  env->SetProtoMethodNoSideEffect(tpl, "getNamespace", GetNamespace);
  env->SetProtoMethodNoSideEffect(tpl, "getStatus", GetStatus);
  env->SetProtoMethodNoSideEffect(tpl, "getError", GetError);
  env->SetProtoMethodNoSideEffect(tpl, "getStaticDependencySpecifiers",
                                  GetStaticDependencySpecifiers);

  target->Set(env->context(), FIXED_ONE_BYTE_STRING(isolate, "ModuleWrap"),
              tpl->GetFunction(context).ToLocalChecked()).Check();
  env->SetMethod(target,
                 "setImportModuleDynamicallyCallback",
                 SetImportModuleDynamicallyCallback);
  env->SetMethod(target,
                 "setInitializeImportMetaObjectCallback",
                 SetInitializeImportMetaObjectCallback);

#define V(name)                                                                \
    target->Set(context,                                                       \
      FIXED_ONE_BYTE_STRING(env->isolate(), #name),                            \
      Integer::New(env->isolate(), Module::Status::name))                      \
        .FromJust()
    V(kUninstantiated);
    V(kInstantiating);
    V(kInstantiated);
    V(kEvaluating);
    V(kEvaluated);
    V(kErrored);
#undef V
}

}  // namespace loader
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(module_wrap,
                                   node::loader::ModuleWrap::Initialize)
