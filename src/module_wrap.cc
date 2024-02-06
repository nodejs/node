#include "module_wrap.h"

#include "env.h"
#include "memory_tracker-inl.h"
#include "node_contextify.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_process-inl.h"
#include "node_watchdog.h"
#include "util-inl.h"

#include <sys/stat.h>  // S_IFDIR

#include <algorithm>

namespace node {
namespace loader {

using errors::TryCatchScope;

using node::contextify::ContextifyContext;
using v8::Array;
using v8::ArrayBufferView;
using v8::Context;
using v8::EscapableHandleScope;
using v8::FixedArray;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::IntegrityLevel;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::MicrotaskQueue;
using v8::Module;
using v8::ModuleRequest;
using v8::Object;
using v8::ObjectTemplate;
using v8::PrimitiveArray;
using v8::Promise;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::Symbol;
using v8::UnboundModuleScript;
using v8::Undefined;
using v8::Value;

ModuleWrap::ModuleWrap(Realm* realm,
                       Local<Object> object,
                       Local<Module> module,
                       Local<String> url,
                       Local<Object> context_object,
                       Local<Value> synthetic_evaluation_step)
    : BaseObject(realm, object),
      module_(realm->isolate(), module),
      module_hash_(module->GetIdentityHash()) {
  realm->env()->hash_to_module_map.emplace(module_hash_, this);
  object->SetInternalFieldForNodeCore(kModuleSlot, module);
  object->SetInternalField(kURLSlot, url);
  object->SetInternalField(kSyntheticEvaluationStepsSlot,
                           synthetic_evaluation_step);
  object->SetInternalField(kContextObjectSlot, context_object);

  if (!synthetic_evaluation_step->IsUndefined()) {
    synthetic_ = true;
  }
  MakeWeak();
  module_.SetWeak();
}

ModuleWrap::~ModuleWrap() {
  auto range = env()->hash_to_module_map.equal_range(module_hash_);
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == this) {
      env()->hash_to_module_map.erase(it);
      break;
    }
  }
}

Local<Context> ModuleWrap::context() const {
  Local<Value> obj = object()->GetInternalField(kContextObjectSlot).As<Value>();
  // If this fails, there is likely a bug e.g. ModuleWrap::context() is accessed
  // before the ModuleWrap constructor completes.
  CHECK(obj->IsObject());
  return obj.As<Object>()->GetCreationContextChecked();
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

// new ModuleWrap(url, context, source, lineOffset, columnOffset)
// new ModuleWrap(url, context, exportNames, syntheticExecutionFunction)
void ModuleWrap::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  CHECK_GE(args.Length(), 3);

  Realm* realm = Realm::GetCurrent(args);
  Isolate* isolate = realm->isolate();

  Local<Object> that = args.This();

  CHECK(args[0]->IsString());
  Local<String> url = args[0].As<String>();

  Local<Context> context;
  ContextifyContext* contextify_context = nullptr;
  if (args[1]->IsUndefined()) {
    context = that->GetCreationContextChecked();
  } else {
    CHECK(args[1]->IsObject());
    contextify_context = ContextifyContext::ContextFromContextifiedSandbox(
        realm->env(), args[1].As<Object>());
    CHECK_NOT_NULL(contextify_context);
    context = contextify_context->context();
  }

  int line_offset = 0;
  int column_offset = 0;

  bool synthetic = args[2]->IsArray();
  if (synthetic) {
    // new ModuleWrap(url, context, exportNames, syntheticExecutionFunction)
    CHECK(args[3]->IsFunction());
  } else {
    // new ModuleWrap(url, context, source, lineOffset, columOffset, cachedData)
    CHECK(args[2]->IsString());
    CHECK(args[3]->IsNumber());
    line_offset = args[3].As<Int32>()->Value();
    CHECK(args[4]->IsNumber());
    column_offset = args[4].As<Int32>()->Value();
  }

  Local<PrimitiveArray> host_defined_options =
      PrimitiveArray::New(isolate, HostDefinedOptions::kLength);
  Local<Symbol> id_symbol = Symbol::New(isolate, url);
  host_defined_options->Set(isolate, HostDefinedOptions::kID, id_symbol);

  ShouldNotAbortOnUncaughtScope no_abort_scope(realm->env());
  TryCatchScope try_catch(realm->env());

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
        uint8_t* data =
            static_cast<uint8_t*>(cached_data_buf->Buffer()->Data());
        cached_data =
            new ScriptCompiler::CachedData(data + cached_data_buf->ByteOffset(),
                                           cached_data_buf->ByteLength());
      }

      Local<String> source_text = args[2].As<String>();
      ScriptOrigin origin(isolate,
                          url,
                          line_offset,
                          column_offset,
                          true,                             // is cross origin
                          -1,                               // script id
                          Local<Value>(),                   // source map URL
                          false,                            // is opaque (?)
                          false,                            // is WASM
                          true,                             // is ES Module
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
          AppendExceptionLine(realm->env(),
                              try_catch.Exception(),
                              try_catch.Message(),
                              ErrorHandlingMode::MODULE_ERROR);
          try_catch.ReThrow();
        }
        return;
      }
      if (options == ScriptCompiler::kConsumeCodeCache &&
          source.GetCachedData()->rejected) {
        THROW_ERR_VM_MODULE_CACHED_DATA_REJECTED(
            realm, "cachedData buffer was rejected");
        try_catch.ReThrow();
        return;
      }

      if (that->Set(context,
                    realm->env()->source_map_url_string(),
                    module->GetUnboundModuleScript()->GetSourceMappingURL())
              .IsNothing()) {
        return;
      }
    }
  }

  if (!that->Set(context, realm->isolate_data()->url_string(), url)
           .FromMaybe(false)) {
    return;
  }

  if (that->SetPrivate(context,
                       realm->isolate_data()->host_defined_option_symbol(),
                       id_symbol)
          .IsNothing()) {
    return;
  }

  // Use the extras object as an object whose GetCreationContext() will be the
  // original `context`, since the `Context` itself strictly speaking cannot
  // be stored in an internal field.
  Local<Object> context_object = context->GetExtrasBindingObject();
  Local<Value> synthetic_evaluation_step =
      synthetic ? args[3] : Undefined(realm->isolate()).As<v8::Value>();

  ModuleWrap* obj = new ModuleWrap(
      realm, that, module, url, context_object, synthetic_evaluation_step);

  obj->contextify_context_ = contextify_context;

  that->SetIntegrityLevel(context, IntegrityLevel::kFrozen);
  args.GetReturnValue().Set(that);
}

static Local<Object> createImportAttributesContainer(
    Realm* realm,
    Isolate* isolate,
    Local<FixedArray> raw_attributes,
    const int elements_per_attribute) {
  CHECK_EQ(raw_attributes->Length() % elements_per_attribute, 0);
  Local<Object> attributes =
      Object::New(isolate, v8::Null(isolate), nullptr, nullptr, 0);
  for (int i = 0; i < raw_attributes->Length(); i += elements_per_attribute) {
    attributes
        ->Set(realm->context(),
              raw_attributes->Get(realm->context(), i).As<String>(),
              raw_attributes->Get(realm->context(), i + 1).As<Value>())
        .ToChecked();
  }

  return attributes;
}

void ModuleWrap::Link(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
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

  Local<FixedArray> module_requests = module->GetModuleRequests();
  const int module_requests_length = module_requests->Length();
  MaybeStackBuffer<Local<Value>, 16> promises(module_requests_length);

  // call the dependency resolve callbacks
  for (int i = 0; i < module_requests_length; i++) {
    Local<ModuleRequest> module_request =
        module_requests->Get(realm->context(), i).As<ModuleRequest>();
    Local<String> specifier = module_request->GetSpecifier();
    Utf8Value specifier_utf8(realm->isolate(), specifier);
    std::string specifier_std(*specifier_utf8, specifier_utf8.length());

    Local<FixedArray> raw_attributes = module_request->GetImportAssertions();
    Local<Object> attributes =
        createImportAttributesContainer(realm, isolate, raw_attributes, 3);

    Local<Value> argv[] = {
        specifier,
        attributes,
    };

    MaybeLocal<Value> maybe_resolve_return_value =
        resolver_arg->Call(mod_context, that, arraysize(argv), argv);
    if (maybe_resolve_return_value.IsEmpty()) {
      return;
    }
    Local<Value> resolve_return_value =
        maybe_resolve_return_value.ToLocalChecked();
    if (!resolve_return_value->IsPromise()) {
      THROW_ERR_VM_MODULE_LINK_FAILURE(
          realm, "request for '%s' did not return promise", specifier_std);
      return;
    }
    Local<Promise> resolve_promise = resolve_return_value.As<Promise>();
    obj->resolve_cache_[specifier_std].Reset(isolate, resolve_promise);

    promises[i] = resolve_promise;
  }

  args.GetReturnValue().Set(
      Array::New(isolate, promises.out(), promises.length()));
}

void ModuleWrap::Instantiate(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();
  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.This());
  Local<Context> context = obj->context();
  Local<Module> module = obj->module_.Get(isolate);
  TryCatchScope try_catch(realm->env());
  USE(module->InstantiateModule(context, ResolveModuleCallback));

  // clear resolve cache on instantiate
  obj->resolve_cache_.clear();

  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    CHECK(!try_catch.Message().IsEmpty());
    CHECK(!try_catch.Exception().IsEmpty());
    AppendExceptionLine(realm->env(),
                        try_catch.Exception(),
                        try_catch.Message(),
                        ErrorHandlingMode::MODULE_ERROR);
    try_catch.ReThrow();
    return;
  }
}

void ModuleWrap::Evaluate(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  Isolate* isolate = realm->isolate();
  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.This());
  Local<Context> context = obj->context();
  Local<Module> module = obj->module_.Get(isolate);

  ContextifyContext* contextify_context = obj->contextify_context_;
  MicrotaskQueue* microtask_queue = nullptr;
  if (contextify_context != nullptr)
      microtask_queue = contextify_context->microtask_queue();

  // module.evaluate(timeout, breakOnSigint)
  CHECK_EQ(args.Length(), 2);

  CHECK(args[0]->IsNumber());
  int64_t timeout = args[0]->IntegerValue(realm->context()).FromJust();

  CHECK(args[1]->IsBoolean());
  bool break_on_sigint = args[1]->IsTrue();

  ShouldNotAbortOnUncaughtScope no_abort_scope(realm->env());
  TryCatchScope try_catch(realm->env());
  Isolate::SafeForTerminationScope safe_for_termination(isolate);

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
    if (!realm->env()->is_main_thread() && realm->env()->is_stopping()) return;
    isolate->CancelTerminateExecution();
    // It is possible that execution was terminated by another timeout in
    // which this timeout is nested, so check whether one of the watchdogs
    // from this invocation is responsible for termination.
    if (timed_out) {
      THROW_ERR_SCRIPT_EXECUTION_TIMEOUT(realm->env(), timeout);
    } else if (received_signal) {
      THROW_ERR_SCRIPT_EXECUTION_INTERRUPTED(realm->env());
    }
  }

  if (try_catch.HasCaught()) {
    if (!try_catch.HasTerminated())
      try_catch.ReThrow();
    return;
  }

  args.GetReturnValue().Set(result.ToLocalChecked());
}

void ModuleWrap::GetNamespace(const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();
  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.This());

  Local<Module> module = obj->module_.Get(isolate);

  switch (module->GetStatus()) {
    case v8::Module::Status::kUninstantiated:
    case v8::Module::Status::kInstantiating:
      return realm->env()->ThrowError(
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
  Realm* realm = Realm::GetCurrent(args);
  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.This());

  Local<Module> module = obj->module_.Get(realm->isolate());

  Local<FixedArray> module_requests = module->GetModuleRequests();
  int count = module_requests->Length();

  MaybeStackBuffer<Local<Value>, 16> specifiers(count);

  for (int i = 0; i < count; i++) {
    Local<ModuleRequest> module_request =
        module_requests->Get(realm->context(), i).As<ModuleRequest>();
    specifiers[i] = module_request->GetSpecifier();
  }

  args.GetReturnValue().Set(
      Array::New(realm->isolate(), specifiers.out(), count));
}

void ModuleWrap::GetError(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  ModuleWrap* obj;
  ASSIGN_OR_RETURN_UNWRAP(&obj, args.This());

  Local<Module> module = obj->module_.Get(isolate);
  args.GetReturnValue().Set(module->GetException());
}

MaybeLocal<Module> ModuleWrap::ResolveModuleCallback(
    Local<Context> context,
    Local<String> specifier,
    Local<FixedArray> import_attributes,
    Local<Module> referrer) {
  Isolate* isolate = context->GetIsolate();
  Environment* env = Environment::GetCurrent(context);
  if (env == nullptr) {
    THROW_ERR_EXECUTION_ENVIRONMENT_NOT_AVAILABLE(isolate);
    return MaybeLocal<Module>();
  }

  Utf8Value specifier_utf8(isolate, specifier);
  std::string specifier_std(*specifier_utf8, specifier_utf8.length());

  ModuleWrap* dependent = GetFromModule(env, referrer);
  if (dependent == nullptr) {
    THROW_ERR_VM_MODULE_LINK_FAILURE(
        env, "request for '%s' is from invalid module", specifier_std);
    return MaybeLocal<Module>();
  }

  if (dependent->resolve_cache_.count(specifier_std) != 1) {
    THROW_ERR_VM_MODULE_LINK_FAILURE(
        env, "request for '%s' is not in cache", specifier_std);
    return MaybeLocal<Module>();
  }

  Local<Promise> resolve_promise =
      dependent->resolve_cache_[specifier_std].Get(isolate);

  if (resolve_promise->State() != Promise::kFulfilled) {
    THROW_ERR_VM_MODULE_LINK_FAILURE(
        env, "request for '%s' is not yet fulfilled", specifier_std);
    return MaybeLocal<Module>();
  }

  Local<Object> module_object = resolve_promise->Result().As<Object>();
  if (module_object.IsEmpty() || !module_object->IsObject()) {
    THROW_ERR_VM_MODULE_LINK_FAILURE(
        env, "request for '%s' did not return an object", specifier_std);
    return MaybeLocal<Module>();
  }

  ModuleWrap* module;
  ASSIGN_OR_RETURN_UNWRAP(&module, module_object, MaybeLocal<Module>());
  return module->module_.Get(isolate);
}

static MaybeLocal<Promise> ImportModuleDynamically(
    Local<Context> context,
    Local<v8::Data> host_defined_options,
    Local<Value> resource_name,
    Local<String> specifier,
    Local<FixedArray> import_attributes) {
  Isolate* isolate = context->GetIsolate();
  Environment* env = Environment::GetCurrent(context);
  if (env == nullptr) {
    THROW_ERR_EXECUTION_ENVIRONMENT_NOT_AVAILABLE(isolate);
    return MaybeLocal<Promise>();
  }
  Realm* realm = Realm::GetCurrent(context);
  if (realm == nullptr) {
    // Fallback to the principal realm if it's in a vm context.
    realm = env->principal_realm();
  }

  EscapableHandleScope handle_scope(isolate);

  Local<Function> import_callback =
      realm->host_import_module_dynamically_callback();
  Local<Value> id;

  Local<FixedArray> options = host_defined_options.As<FixedArray>();
  // Get referrer id symbol from the host-defined options.
  // If the host-defined options are empty, get the referrer id symbol
  // from the realm global object.
  if (options->Length() == HostDefinedOptions::kLength) {
    id = options->Get(context, HostDefinedOptions::kID).As<Symbol>();
  } else {
    id = context->Global()
             ->GetPrivate(context, env->host_defined_option_symbol())
             .ToLocalChecked();
  }

  Local<Object> attributes =
      createImportAttributesContainer(realm, isolate, import_attributes, 2);

  Local<Value> import_args[] = {
      id,
      Local<Value>(specifier),
      attributes,
      resource_name,
  };

  Local<Value> result;
  if (import_callback->Call(
        context,
        Undefined(isolate),
        arraysize(import_args),
        import_args).ToLocal(&result)) {
    CHECK(result->IsPromise());
    return handle_scope.Escape(result.As<Promise>());
  }

  return MaybeLocal<Promise>();
}

void ModuleWrap::SetImportModuleDynamicallyCallback(
    const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Realm* realm = Realm::GetCurrent(args);
  HandleScope handle_scope(isolate);

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  Local<Function> import_callback = args[0].As<Function>();
  realm->set_host_import_module_dynamically_callback(import_callback);

  isolate->SetHostImportModuleDynamicallyCallback(ImportModuleDynamically);
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
  Realm* realm = Realm::GetCurrent(context);
  if (realm == nullptr) {
    // Fallback to the principal realm if it's in a vm context.
    realm = env->principal_realm();
  }

  Local<Object> wrap = module_wrap->object();
  Local<Function> callback =
      realm->host_initialize_import_meta_object_callback();
  Local<Value> id;
  if (!wrap->GetPrivate(context, env->host_defined_option_symbol())
           .ToLocal(&id)) {
    return;
  }
  DCHECK(id->IsSymbol());
  Local<Value> args[] = {id, meta};
  TryCatchScope try_catch(env);
  USE(callback->Call(
      context, Undefined(realm->isolate()), arraysize(args), args));
  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    try_catch.ReThrow();
  }
}

void ModuleWrap::SetInitializeImportMetaObjectCallback(
    const FunctionCallbackInfo<Value>& args) {
  Realm* realm = Realm::GetCurrent(args);
  Isolate* isolate = realm->isolate();

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  Local<Function> import_meta_callback = args[0].As<Function>();
  realm->set_host_initialize_import_meta_object_callback(import_meta_callback);

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
      obj->object()
          ->GetInternalField(kSyntheticEvaluationStepsSlot)
          .As<Value>()
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

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) {
    return MaybeLocal<Value>();
  }

  resolver->Resolve(context, Undefined(isolate)).ToChecked();
  return resolver->GetPromise();
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

void ModuleWrap::CreatePerIsolateProperties(IsolateData* isolate_data,
                                            Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();

  Local<FunctionTemplate> tpl = NewFunctionTemplate(isolate, New);
  tpl->InstanceTemplate()->SetInternalFieldCount(
      ModuleWrap::kInternalFieldCount);

  SetProtoMethod(isolate, tpl, "link", Link);
  SetProtoMethod(isolate, tpl, "instantiate", Instantiate);
  SetProtoMethod(isolate, tpl, "evaluate", Evaluate);
  SetProtoMethod(isolate, tpl, "setExport", SetSyntheticExport);
  SetProtoMethodNoSideEffect(
      isolate, tpl, "createCachedData", CreateCachedData);
  SetProtoMethodNoSideEffect(isolate, tpl, "getNamespace", GetNamespace);
  SetProtoMethodNoSideEffect(isolate, tpl, "getStatus", GetStatus);
  SetProtoMethodNoSideEffect(isolate, tpl, "getError", GetError);
  SetProtoMethodNoSideEffect(isolate,
                             tpl,
                             "getStaticDependencySpecifiers",
                             GetStaticDependencySpecifiers);

  SetConstructorFunction(isolate, target, "ModuleWrap", tpl);

  SetMethod(isolate,
            target,
            "setImportModuleDynamicallyCallback",
            SetImportModuleDynamicallyCallback);
  SetMethod(isolate,
            target,
            "setInitializeImportMetaObjectCallback",
            SetInitializeImportMetaObjectCallback);
}

void ModuleWrap::CreatePerContextProperties(Local<Object> target,
                                            Local<Value> unused,
                                            Local<Context> context,
                                            void* priv) {
  Realm* realm = Realm::GetCurrent(context);
  Isolate* isolate = realm->isolate();
#define V(name)                                                                \
  target                                                                       \
      ->Set(context,                                                           \
            FIXED_ONE_BYTE_STRING(isolate, #name),                             \
            Integer::New(isolate, Module::Status::name))                       \
      .FromJust()
  V(kUninstantiated);
  V(kInstantiating);
  V(kInstantiated);
  V(kEvaluating);
  V(kEvaluated);
  V(kErrored);
#undef V
}

void ModuleWrap::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);

  registry->Register(Link);
  registry->Register(Instantiate);
  registry->Register(Evaluate);
  registry->Register(SetSyntheticExport);
  registry->Register(CreateCachedData);
  registry->Register(GetNamespace);
  registry->Register(GetStatus);
  registry->Register(GetError);
  registry->Register(GetStaticDependencySpecifiers);

  registry->Register(SetImportModuleDynamicallyCallback);
  registry->Register(SetInitializeImportMetaObjectCallback);
}
}  // namespace loader
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    module_wrap, node::loader::ModuleWrap::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(
    module_wrap, node::loader::ModuleWrap::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(
    module_wrap, node::loader::ModuleWrap::RegisterExternalReferences)
