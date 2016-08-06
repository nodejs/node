#include "node.h"
#include "node_internals.h"
#include "node_watchdog.h"
#include "base-object.h"
#include "base-object-inl.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8-debug.h"

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::Boolean;
using v8::Context;
using v8::Debug;
using v8::EscapableHandleScope;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::NamedPropertyHandlerConfiguration;
using v8::Object;
using v8::ObjectTemplate;
using v8::Persistent;
using v8::PropertyAttribute;
using v8::PropertyCallbackInfo;
using v8::Script;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::TryCatch;
using v8::Uint8Array;
using v8::UnboundScript;
using v8::Value;
using v8::WeakCallbackInfo;


class ContextifyContext {
 protected:
  // V8 reserves the first field in context objects for the debugger. We use the
  // second field to hold a reference to the sandbox object.
  enum { kSandboxObjectIndex = 1 };

  Environment* const env_;
  Persistent<Context> context_;

 public:
  ContextifyContext(Environment* env, Local<Object> sandbox_obj) : env_(env) {
    Local<Context> v8_context = CreateV8Context(env, sandbox_obj);
    context_.Reset(env->isolate(), v8_context);

    // Allocation failure or maximum call stack size reached
    if (context_.IsEmpty())
      return;
    context_.SetWeak(this, WeakCallback, v8::WeakCallbackType::kParameter);
    context_.MarkIndependent();
  }


  ~ContextifyContext() {
    context_.Reset();
  }


  inline Environment* env() const {
    return env_;
  }


  inline Local<Context> context() const {
    return PersistentToLocal(env()->isolate(), context_);
  }


  inline Local<Object> global_proxy() const {
    return context()->Global();
  }


  inline Local<Object> sandbox() const {
    return Local<Object>::Cast(context()->GetEmbedderData(kSandboxObjectIndex));
  }

  // XXX(isaacs): This function only exists because of a shortcoming of
  // the V8 SetNamedPropertyHandler function.
  //
  // It does not provide a way to intercept Object.defineProperty(..)
  // calls.  As a result, these properties are not copied onto the
  // contextified sandbox when a new global property is added via either
  // a function declaration or a Object.defineProperty(global, ...) call.
  //
  // Note that any function declarations or Object.defineProperty()
  // globals that are created asynchronously (in a setTimeout, callback,
  // etc.) will happen AFTER the call to copy properties, and thus not be
  // caught.
  //
  // The way to properly fix this is to add some sort of a
  // Object::SetNamedDefinePropertyHandler() function that takes a callback,
  // which receives the property name and property descriptor as arguments.
  //
  // Luckily, such situations are rare, and asynchronously-added globals
  // weren't supported by Node's VM module until 0.12 anyway.  But, this
  // should be fixed properly in V8, and this copy function should be
  // removed once there is a better way.
  void CopyProperties() {
    HandleScope scope(env()->isolate());

    Local<Context> context = PersistentToLocal(env()->isolate(), context_);
    Local<Object> global =
        context->Global()->GetPrototype()->ToObject(env()->isolate());
    Local<Object> sandbox_obj = sandbox();

    Local<Function> clone_property_method;

    Local<Array> names = global->GetOwnPropertyNames();
    int length = names->Length();
    for (int i = 0; i < length; i++) {
      Local<String> key = names->Get(i)->ToString(env()->isolate());
      bool has = sandbox_obj->HasOwnProperty(context, key).FromJust();
      if (!has) {
        // Could also do this like so:
        //
        // PropertyAttribute att = global->GetPropertyAttributes(key_v);
        // Local<Value> val = global->Get(key_v);
        // sandbox->ForceSet(key_v, val, att);
        //
        // However, this doesn't handle ES6-style properties configured with
        // Object.defineProperty, and that's exactly what we're up against at
        // this point.  ForceSet(key,val,att) only supports value properties
        // with the ES3-style attribute flags (DontDelete/DontEnum/ReadOnly),
        // which doesn't faithfully capture the full range of configurations
        // that can be done using Object.defineProperty.
        if (clone_property_method.IsEmpty()) {
          Local<String> code = FIXED_ONE_BYTE_STRING(env()->isolate(),
              "(function cloneProperty(source, key, target) {\n"
              "  if (key === 'Proxy') return;\n"
              "  try {\n"
              "    var desc = Object.getOwnPropertyDescriptor(source, key);\n"
              "    if (desc.value === source) desc.value = target;\n"
              "    Object.defineProperty(target, key, desc);\n"
              "  } catch (e) {\n"
              "   // Catch sealed properties errors\n"
              "  }\n"
              "})");

          Local<Script> script =
              Script::Compile(context, code).ToLocalChecked();
          clone_property_method = Local<Function>::Cast(script->Run());
          CHECK(clone_property_method->IsFunction());
        }
        Local<Value> args[] = { global, key, sandbox_obj };
        clone_property_method->Call(global, arraysize(args), args);
      }
    }
  }


  // This is an object that just keeps an internal pointer to this
  // ContextifyContext.  It's passed to the NamedPropertyHandler.  If we
  // pass the main JavaScript context object we're embedded in, then the
  // NamedPropertyHandler will store a reference to it forever and keep it
  // from getting gc'd.
  Local<Value> CreateDataWrapper(Environment* env) {
    EscapableHandleScope scope(env->isolate());
    Local<Object> wrapper =
        env->script_data_constructor_function()
            ->NewInstance(env->context()).FromMaybe(Local<Object>());
    if (wrapper.IsEmpty())
      return scope.Escape(Local<Value>::New(env->isolate(), Local<Value>()));

    Wrap(wrapper, this);
    return scope.Escape(wrapper);
  }


  Local<Context> CreateV8Context(Environment* env, Local<Object> sandbox_obj) {
    EscapableHandleScope scope(env->isolate());
    Local<FunctionTemplate> function_template =
        FunctionTemplate::New(env->isolate());
    function_template->SetHiddenPrototype(true);

    function_template->SetClassName(sandbox_obj->GetConstructorName());

    Local<ObjectTemplate> object_template =
        function_template->InstanceTemplate();

    NamedPropertyHandlerConfiguration config(GlobalPropertyGetterCallback,
                                             GlobalPropertySetterCallback,
                                             GlobalPropertyQueryCallback,
                                             GlobalPropertyDeleterCallback,
                                             GlobalPropertyEnumeratorCallback,
                                             CreateDataWrapper(env));
    object_template->SetHandler(config);

    Local<Context> ctx = Context::New(env->isolate(), nullptr, object_template);

    if (ctx.IsEmpty()) {
      env->ThrowError("Could not instantiate context");
      return Local<Context>();
    }

    ctx->SetSecurityToken(env->context()->GetSecurityToken());

    // We need to tie the lifetime of the sandbox object with the lifetime of
    // newly created context. We do this by making them hold references to each
    // other. The context can directly hold a reference to the sandbox as an
    // embedder data field. However, we cannot hold a reference to a v8::Context
    // directly in an Object, we instead hold onto the new context's global
    // object instead (which then has a reference to the context).
    ctx->SetEmbedderData(kSandboxObjectIndex, sandbox_obj);
    sandbox_obj->SetPrivate(env->context(),
                            env->contextify_global_private_symbol(),
                            ctx->Global());

    env->AssignToContext(ctx);

    return scope.Escape(ctx);
  }


  static void Init(Environment* env, Local<Object> target) {
    Local<FunctionTemplate> function_template =
        FunctionTemplate::New(env->isolate());
    function_template->InstanceTemplate()->SetInternalFieldCount(1);
    env->set_script_data_constructor_function(function_template->GetFunction());

    env->SetMethod(target, "runInDebugContext", RunInDebugContext);
    env->SetMethod(target, "makeContext", MakeContext);
    env->SetMethod(target, "isContext", IsContext);
  }


  static void RunInDebugContext(const FunctionCallbackInfo<Value>& args) {
    Local<String> script_source(args[0]->ToString(args.GetIsolate()));
    if (script_source.IsEmpty())
      return;  // Exception pending.
    Local<Context> debug_context = Debug::GetDebugContext(args.GetIsolate());
    Environment* env = Environment::GetCurrent(args);
    if (debug_context.IsEmpty()) {
      // Force-load the debug context.
      Debug::GetMirror(args.GetIsolate()->GetCurrentContext(), args[0]);
      debug_context = Debug::GetDebugContext(args.GetIsolate());
      CHECK(!debug_context.IsEmpty());
      // Ensure that the debug context has an Environment assigned in case
      // a fatal error is raised.  The fatal exception handler in node.cc
      // is not equipped to deal with contexts that don't have one and
      // can't easily be taught that due to a deficiency in the V8 API:
      // there is no way for the embedder to tell if the data index is
      // in use.
      const int index = Environment::kContextEmbedderDataIndex;
      debug_context->SetAlignedPointerInEmbedderData(index, env);
    }

    Context::Scope context_scope(debug_context);
    MaybeLocal<Script> script = Script::Compile(debug_context, script_source);
    if (script.IsEmpty())
      return;  // Exception pending.
    args.GetReturnValue().Set(script.ToLocalChecked()->Run());
  }


  static void MakeContext(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);

    if (!args[0]->IsObject()) {
      return env->ThrowTypeError("sandbox argument must be an object.");
    }
    Local<Object> sandbox = args[0].As<Object>();

    // Don't allow contextifying a sandbox multiple times.
    CHECK(
        !sandbox->HasPrivate(
            env->context(),
            env->contextify_context_private_symbol()).FromJust());

    TryCatch try_catch(env->isolate());
    ContextifyContext* context = new ContextifyContext(env, sandbox);

    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return;
    }

    if (context->context().IsEmpty())
      return;

    sandbox->SetPrivate(
        env->context(),
        env->contextify_context_private_symbol(),
        External::New(env->isolate(), context));
  }


  static void IsContext(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);

    if (!args[0]->IsObject()) {
      env->ThrowTypeError("sandbox must be an object");
      return;
    }
    Local<Object> sandbox = args[0].As<Object>();

    auto result =
        sandbox->HasPrivate(env->context(),
                            env->contextify_context_private_symbol());
    args.GetReturnValue().Set(result.FromJust());
  }


  static void WeakCallback(const WeakCallbackInfo<ContextifyContext>& data) {
    ContextifyContext* context = data.GetParameter();
    delete context;
  }


  static ContextifyContext* ContextFromContextifiedSandbox(
      Environment* env,
      const Local<Object>& sandbox) {
    auto maybe_value =
        sandbox->GetPrivate(env->context(),
                            env->contextify_context_private_symbol());
    Local<Value> context_external_v;
    if (maybe_value.ToLocal(&context_external_v) &&
        context_external_v->IsExternal()) {
      Local<External> context_external = context_external_v.As<External>();
      return static_cast<ContextifyContext*>(context_external->Value());
    }
    return nullptr;
  }


  static void GlobalPropertyGetterCallback(
      Local<Name> property,
      const PropertyCallbackInfo<Value>& args) {
    ContextifyContext* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

    // Stil initializing
    if (ctx->context_.IsEmpty())
      return;

    Local<Context> context = ctx->context();
    Local<Object> sandbox = ctx->sandbox();
    MaybeLocal<Value> maybe_rv =
        sandbox->GetRealNamedProperty(context, property);
    if (maybe_rv.IsEmpty()) {
      maybe_rv =
          ctx->global_proxy()->GetRealNamedProperty(context, property);
    }

    Local<Value> rv;
    if (maybe_rv.ToLocal(&rv)) {
      if (rv == sandbox)
        rv = ctx->global_proxy();

      args.GetReturnValue().Set(rv);
    }
  }


  static void GlobalPropertySetterCallback(
      Local<Name> property,
      Local<Value> value,
      const PropertyCallbackInfo<Value>& args) {
    ContextifyContext* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

    // Stil initializing
    if (ctx->context_.IsEmpty())
      return;

    bool is_declared =
        ctx->global_proxy()->HasRealNamedProperty(ctx->context(),
                                                  property).FromJust();
    bool is_contextual_store = ctx->global_proxy() != args.This();

    bool set_property_will_throw =
        args.ShouldThrowOnError() &&
        !is_declared &&
        is_contextual_store;

    if (!set_property_will_throw) {
      ctx->sandbox()->Set(property, value);
    }
  }


  static void GlobalPropertyQueryCallback(
      Local<Name> property,
      const PropertyCallbackInfo<Integer>& args) {
    ContextifyContext* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

    // Stil initializing
    if (ctx->context_.IsEmpty())
      return;

    Local<Context> context = ctx->context();
    Maybe<PropertyAttribute> maybe_prop_attr =
        ctx->sandbox()->GetRealNamedPropertyAttributes(context, property);

    if (maybe_prop_attr.IsNothing()) {
      maybe_prop_attr =
          ctx->global_proxy()->GetRealNamedPropertyAttributes(context,
                                                              property);
    }

    if (maybe_prop_attr.IsJust()) {
      PropertyAttribute prop_attr = maybe_prop_attr.FromJust();
      args.GetReturnValue().Set(prop_attr);
    }
  }


  static void GlobalPropertyDeleterCallback(
      Local<Name> property,
      const PropertyCallbackInfo<Boolean>& args) {
    ContextifyContext* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

    // Stil initializing
    if (ctx->context_.IsEmpty())
      return;

    Maybe<bool> success = ctx->sandbox()->Delete(ctx->context(), property);

    if (success.IsJust())
      args.GetReturnValue().Set(success.FromJust());
  }


  static void GlobalPropertyEnumeratorCallback(
      const PropertyCallbackInfo<Array>& args) {
    ContextifyContext* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Data().As<Object>());

    // Stil initializing
    if (ctx->context_.IsEmpty())
      return;

    args.GetReturnValue().Set(ctx->sandbox()->GetPropertyNames());
  }
};

class ContextifyScript : public BaseObject {
 private:
  Persistent<UnboundScript> script_;

 public:
  static void Init(Environment* env, Local<Object> target) {
    HandleScope scope(env->isolate());
    Local<String> class_name =
        FIXED_ONE_BYTE_STRING(env->isolate(), "ContextifyScript");

    Local<FunctionTemplate> script_tmpl = env->NewFunctionTemplate(New);
    script_tmpl->InstanceTemplate()->SetInternalFieldCount(1);
    script_tmpl->SetClassName(class_name);
    env->SetProtoMethod(script_tmpl, "runInContext", RunInContext);
    env->SetProtoMethod(script_tmpl, "runInThisContext", RunInThisContext);

    target->Set(class_name, script_tmpl->GetFunction());
    env->set_script_context_constructor_template(script_tmpl);
  }


  // args: code, [options]
  static void New(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);

    if (!args.IsConstructCall()) {
      return env->ThrowError("Must call vm.Script as a constructor.");
    }

    ContextifyScript* contextify_script =
        new ContextifyScript(env, args.This());

    TryCatch try_catch(env->isolate());
    Local<String> code = args[0]->ToString(env->isolate());
    Local<String> filename = GetFilenameArg(args, 1);
    Local<Integer> lineOffset = GetLineOffsetArg(args, 1);
    Local<Integer> columnOffset = GetColumnOffsetArg(args, 1);
    bool display_errors = GetDisplayErrorsArg(args, 1);
    MaybeLocal<Uint8Array> cached_data_buf = GetCachedData(env, args, 1);
    bool produce_cached_data = GetProduceCachedData(env, args, 1);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return;
    }

    ScriptCompiler::CachedData* cached_data = nullptr;
    if (!cached_data_buf.IsEmpty()) {
      Local<Uint8Array> ui8 = cached_data_buf.ToLocalChecked();
      ArrayBuffer::Contents contents = ui8->Buffer()->GetContents();
      cached_data = new ScriptCompiler::CachedData(
          static_cast<uint8_t*>(contents.Data()) + ui8->ByteOffset(),
          ui8->ByteLength());
    }

    ScriptOrigin origin(filename, lineOffset, columnOffset);
    ScriptCompiler::Source source(code, origin, cached_data);
    ScriptCompiler::CompileOptions compile_options =
        ScriptCompiler::kNoCompileOptions;

    if (source.GetCachedData() != nullptr)
      compile_options = ScriptCompiler::kConsumeCodeCache;
    else if (produce_cached_data)
      compile_options = ScriptCompiler::kProduceCodeCache;

    MaybeLocal<UnboundScript> v8_script = ScriptCompiler::CompileUnboundScript(
        env->isolate(),
        &source,
        compile_options);

    if (v8_script.IsEmpty()) {
      if (display_errors) {
        DecorateErrorStack(env, try_catch);
      }
      try_catch.ReThrow();
      return;
    }
    contextify_script->script_.Reset(env->isolate(),
                                     v8_script.ToLocalChecked());

    if (compile_options == ScriptCompiler::kConsumeCodeCache) {
      args.This()->Set(
          env->cached_data_rejected_string(),
          Boolean::New(env->isolate(), source.GetCachedData()->rejected));
    } else if (compile_options == ScriptCompiler::kProduceCodeCache) {
      const ScriptCompiler::CachedData* cached_data = source.GetCachedData();
      bool cached_data_produced = cached_data != nullptr;
      if (cached_data_produced) {
        MaybeLocal<Object> buf = Buffer::Copy(
            env,
            reinterpret_cast<const char*>(cached_data->data),
            cached_data->length);
        args.This()->Set(env->cached_data_string(), buf.ToLocalChecked());
      }
      args.This()->Set(
          env->cached_data_produced_string(),
          Boolean::New(env->isolate(), cached_data_produced));
    }
  }


  static bool InstanceOf(Environment* env, const Local<Value>& value) {
    return !value.IsEmpty() &&
           env->script_context_constructor_template()->HasInstance(value);
  }


  // args: [options]
  static void RunInThisContext(const FunctionCallbackInfo<Value>& args) {
    // Assemble arguments
    TryCatch try_catch(args.GetIsolate());
    uint64_t timeout = GetTimeoutArg(args, 0);
    bool display_errors = GetDisplayErrorsArg(args, 0);
    bool break_on_sigint = GetBreakOnSigintArg(args, 0);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return;
    }

    // Do the eval within this context
    Environment* env = Environment::GetCurrent(args);
    EvalMachine(env, timeout, display_errors, break_on_sigint, args,
                &try_catch);
  }

  // args: sandbox, [options]
  static void RunInContext(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);

    int64_t timeout;
    bool display_errors;
    bool break_on_sigint;

    // Assemble arguments
    if (!args[0]->IsObject()) {
      return env->ThrowTypeError(
          "contextifiedSandbox argument must be an object.");
    }

    Local<Object> sandbox = args[0].As<Object>();
    {
      TryCatch try_catch(env->isolate());
      timeout = GetTimeoutArg(args, 1);
      display_errors = GetDisplayErrorsArg(args, 1);
      break_on_sigint = GetBreakOnSigintArg(args, 1);
      if (try_catch.HasCaught()) {
        try_catch.ReThrow();
        return;
      }
    }

    // Get the context from the sandbox
    ContextifyContext* contextify_context =
        ContextifyContext::ContextFromContextifiedSandbox(env, sandbox);
    if (contextify_context == nullptr) {
      return env->ThrowTypeError(
          "sandbox argument must have been converted to a context.");
    }

    if (contextify_context->context().IsEmpty())
      return;

    {
      TryCatch try_catch(env->isolate());
      // Do the eval within the context
      Context::Scope context_scope(contextify_context->context());
      if (EvalMachine(contextify_context->env(),
                      timeout,
                      display_errors,
                      break_on_sigint,
                      args,
                      &try_catch)) {
        contextify_context->CopyProperties();
      }

      if (try_catch.HasCaught()) {
        try_catch.ReThrow();
        return;
      }
    }
  }

  static void DecorateErrorStack(Environment* env, const TryCatch& try_catch) {
    Local<Value> exception = try_catch.Exception();

    if (!exception->IsObject())
      return;

    Local<Object> err_obj = exception.As<Object>();

    if (IsExceptionDecorated(env, err_obj))
      return;

    AppendExceptionLine(env, exception, try_catch.Message(), CONTEXTIFY_ERROR);
    Local<Value> stack = err_obj->Get(env->stack_string());
    auto maybe_value =
        err_obj->GetPrivate(
            env->context(),
            env->arrow_message_private_symbol());

    Local<Value> arrow;
    if (!(maybe_value.ToLocal(&arrow) && arrow->IsString())) {
      return;
    }

    if (stack.IsEmpty() || !stack->IsString()) {
      return;
    }

    Local<String> decorated_stack = String::Concat(arrow.As<String>(),
                                                   stack.As<String>());
    err_obj->Set(env->stack_string(), decorated_stack);
    err_obj->SetPrivate(
        env->context(),
        env->decorated_private_symbol(),
        True(env->isolate()));
  }

  static bool GetBreakOnSigintArg(const FunctionCallbackInfo<Value>& args,
                                  const int i) {
    if (args[i]->IsUndefined() || args[i]->IsString()) {
      return false;
    }
    if (!args[i]->IsObject()) {
      Environment::ThrowTypeError(args.GetIsolate(),
                                  "options must be an object");
      return false;
    }

    Local<String> key = FIXED_ONE_BYTE_STRING(args.GetIsolate(),
                                              "breakOnSigint");
    Local<Value> value = args[i].As<Object>()->Get(key);
    return value->IsTrue();
  }

  static int64_t GetTimeoutArg(const FunctionCallbackInfo<Value>& args,
                               const int i) {
    if (args[i]->IsUndefined() || args[i]->IsString()) {
      return -1;
    }
    if (!args[i]->IsObject()) {
      Environment::ThrowTypeError(args.GetIsolate(),
                                  "options must be an object");
      return -1;
    }

    Local<String> key = FIXED_ONE_BYTE_STRING(args.GetIsolate(), "timeout");
    Local<Value> value = args[i].As<Object>()->Get(key);
    if (value->IsUndefined()) {
      return -1;
    }
    int64_t timeout = value->IntegerValue();

    if (timeout <= 0) {
      Environment::ThrowRangeError(args.GetIsolate(),
                                   "timeout must be a positive number");
      return -1;
    }
    return timeout;
  }


  static bool GetDisplayErrorsArg(const FunctionCallbackInfo<Value>& args,
                                  const int i) {
    if (args[i]->IsUndefined() || args[i]->IsString()) {
      return true;
    }
    if (!args[i]->IsObject()) {
      Environment::ThrowTypeError(args.GetIsolate(),
                                  "options must be an object");
      return false;
    }

    Local<String> key = FIXED_ONE_BYTE_STRING(args.GetIsolate(),
                                              "displayErrors");
    Local<Value> value = args[i].As<Object>()->Get(key);

    return value->IsUndefined() ? true : value->BooleanValue();
  }


  static Local<String> GetFilenameArg(const FunctionCallbackInfo<Value>& args,
                                      const int i) {
    Local<String> defaultFilename =
        FIXED_ONE_BYTE_STRING(args.GetIsolate(), "evalmachine.<anonymous>");

    if (args[i]->IsUndefined()) {
      return defaultFilename;
    }
    if (args[i]->IsString()) {
      return args[i].As<String>();
    }
    if (!args[i]->IsObject()) {
      Environment::ThrowTypeError(args.GetIsolate(),
                                  "options must be an object");
      return Local<String>();
    }

    Local<String> key = FIXED_ONE_BYTE_STRING(args.GetIsolate(), "filename");
    Local<Value> value = args[i].As<Object>()->Get(key);

    if (value->IsUndefined())
      return defaultFilename;
    return value->ToString(args.GetIsolate());
  }


  static MaybeLocal<Uint8Array> GetCachedData(
      Environment* env,
      const FunctionCallbackInfo<Value>& args,
      const int i) {
    if (!args[i]->IsObject()) {
      return MaybeLocal<Uint8Array>();
    }
    Local<Value> value = args[i].As<Object>()->Get(env->cached_data_string());
    if (value->IsUndefined()) {
      return MaybeLocal<Uint8Array>();
    }

    if (!value->IsUint8Array()) {
      Environment::ThrowTypeError(
          args.GetIsolate(),
          "options.cachedData must be a Buffer instance");
      return MaybeLocal<Uint8Array>();
    }

    return value.As<Uint8Array>();
  }


  static bool GetProduceCachedData(
      Environment* env,
      const FunctionCallbackInfo<Value>& args,
      const int i) {
    if (!args[i]->IsObject()) {
      return false;
    }
    Local<Value> value =
        args[i].As<Object>()->Get(env->produce_cached_data_string());

    return value->IsTrue();
  }


  static Local<Integer> GetLineOffsetArg(
                                      const FunctionCallbackInfo<Value>& args,
                                      const int i) {
    Local<Integer> defaultLineOffset = Integer::New(args.GetIsolate(), 0);

    if (!args[i]->IsObject()) {
      return defaultLineOffset;
    }

    Local<String> key = FIXED_ONE_BYTE_STRING(args.GetIsolate(), "lineOffset");
    Local<Value> value = args[i].As<Object>()->Get(key);

    return value->IsUndefined() ? defaultLineOffset : value->ToInteger();
  }


  static Local<Integer> GetColumnOffsetArg(
                                      const FunctionCallbackInfo<Value>& args,
                                      const int i) {
    Local<Integer> defaultColumnOffset = Integer::New(args.GetIsolate(), 0);

    if (!args[i]->IsObject()) {
      return defaultColumnOffset;
    }

    Local<String> key = FIXED_ONE_BYTE_STRING(args.GetIsolate(),
                                              "columnOffset");
    Local<Value> value = args[i].As<Object>()->Get(key);

    return value->IsUndefined() ? defaultColumnOffset : value->ToInteger();
  }


  static bool EvalMachine(Environment* env,
                          const int64_t timeout,
                          const bool display_errors,
                          const bool break_on_sigint,
                          const FunctionCallbackInfo<Value>& args,
                          TryCatch* try_catch) {
    if (!ContextifyScript::InstanceOf(env, args.Holder())) {
      env->ThrowTypeError(
          "Script methods can only be called on script instances.");
      return false;
    }

    ContextifyScript* wrapped_script;
    ASSIGN_OR_RETURN_UNWRAP(&wrapped_script, args.Holder(), false);
    Local<UnboundScript> unbound_script =
        PersistentToLocal(env->isolate(), wrapped_script->script_);
    Local<Script> script = unbound_script->BindToCurrentContext();

    Local<Value> result;
    bool timed_out = false;
    bool received_signal = false;
    if (break_on_sigint && timeout != -1) {
      Watchdog wd(env->isolate(), timeout);
      SigintWatchdog swd(env->isolate());
      result = script->Run();
      timed_out = wd.HasTimedOut();
      received_signal = swd.HasReceivedSignal();
    } else if (break_on_sigint) {
      SigintWatchdog swd(env->isolate());
      result = script->Run();
      received_signal = swd.HasReceivedSignal();
    } else if (timeout != -1) {
      Watchdog wd(env->isolate(), timeout);
      result = script->Run();
      timed_out = wd.HasTimedOut();
    } else {
      result = script->Run();
    }

    if (try_catch->HasCaught()) {
      if (try_catch->HasTerminated())
        env->isolate()->CancelTerminateExecution();

      // It is possible that execution was terminated by another timeout in
      // which this timeout is nested, so check whether one of the watchdogs
      // from this invocation is responsible for termination.
      if (timed_out) {
        env->ThrowError("Script execution timed out.");
      } else if (received_signal) {
        env->ThrowError("Script execution interrupted.");
      }

      // If there was an exception thrown during script execution, re-throw it.
      // If one of the above checks threw, re-throw the exception instead of
      // letting try_catch catch it.
      // If execution has been terminated, but not by one of the watchdogs from
      // this invocation, this will re-throw a `null` value.
      try_catch->ReThrow();

      return false;
    }

    if (result.IsEmpty()) {
      // Error occurred during execution of the script.
      if (display_errors) {
        DecorateErrorStack(env, *try_catch);
      }
      try_catch->ReThrow();
      return false;
    }

    args.GetReturnValue().Set(result);
    return true;
  }


  ContextifyScript(Environment* env, Local<Object> object)
      : BaseObject(env, object) {
    MakeWeak<ContextifyScript>(this);
  }


  ~ContextifyScript() override {
    script_.Reset();
  }
};


void InitContextify(Local<Object> target,
                    Local<Value> unused,
                    Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  ContextifyContext::Init(env, target);
  ContextifyScript::Init(env, target);
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(contextify, node::InitContextify);
