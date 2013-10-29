// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node.h"
#include "node_internals.h"
#include "node_watchdog.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "weak-object.h"
#include "weak-object-inl.h"

namespace node {

using v8::AccessType;
using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::None;
using v8::Object;
using v8::ObjectTemplate;
using v8::Persistent;
using v8::PropertyCallbackInfo;
using v8::Script;
using v8::String;
using v8::TryCatch;
using v8::V8;
using v8::Value;


class ContextifyContext {
 private:
  Environment* const env_;
  Persistent<Object> sandbox_;
  Persistent<Context> context_;
  Persistent<Object> proxy_global_;
  int references_;

 public:
  explicit ContextifyContext(Environment* env, Local<Object> sandbox)
      : env_(env),
        sandbox_(env->isolate(), sandbox),
        context_(env->isolate(), CreateV8Context(env)),
        proxy_global_(env->isolate(), context()->Global()),
        // Wait for sandbox_, proxy_global_, and context_ to die
        references_(3) {
    sandbox_.MakeWeak(this, WeakCallback);
    sandbox_.MarkIndependent();
    context_.MakeWeak(this, WeakCallback);
    context_.MarkIndependent();
    proxy_global_.MakeWeak(this, WeakCallback);
    proxy_global_.MarkIndependent();
  }


  ~ContextifyContext() {
    context_.Dispose();
    proxy_global_.Dispose();
    sandbox_.Dispose();
  }


  inline Environment* env() const {
    return env_;
  }


  inline Local<Context> context() const {
    return PersistentToLocal(env()->isolate(), context_);
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
    HandleScope scope(node_isolate);

    Local<Context> context = PersistentToLocal(env()->isolate(), context_);
    Local<Object> global = context->Global()->GetPrototype()->ToObject();
    Local<Object> sandbox = PersistentToLocal(env()->isolate(), sandbox_);

    Local<Function> clone_property_method;

    Local<Array> names = global->GetOwnPropertyNames();
    int length = names->Length();
    for (int i = 0; i < length; i++) {
      Local<String> key = names->Get(i)->ToString();
      bool has = sandbox->HasOwnProperty(key);
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
          Local<String> code = FIXED_ONE_BYTE_STRING(node_isolate,
              "(function cloneProperty(source, key, target) {\n"
              "  try {\n"
              "    var desc = Object.getOwnPropertyDescriptor(source, key);\n"
              "    if (desc.value === source) desc.value = target;\n"
              "    Object.defineProperty(target, key, desc);\n"
              "  } catch (e) {\n"
              "   // Catch sealed properties errors\n"
              "  }\n"
              "})");

          Local<String> fname = FIXED_ONE_BYTE_STRING(node_isolate,
              "binding:script");
          Local<Script> script = Script::Compile(code, fname);
          clone_property_method = Local<Function>::Cast(script->Run());
          assert(clone_property_method->IsFunction());
        }
        Local<Value> args[] = { global, key, sandbox };
        clone_property_method->Call(global, ARRAY_SIZE(args), args);
      }
    }
  }


  // This is an object that just keeps an internal pointer to this
  // ContextifyContext.  It's passed to the NamedPropertyHandler.  If we
  // pass the main JavaScript context object we're embedded in, then the
  // NamedPropertyHandler will store a reference to it forever and keep it
  // from getting gc'd.
  Local<Value> CreateDataWrapper(Environment* env) {
    HandleScope scope(node_isolate);
    Local<Object> wrapper =
        env->script_data_constructor_function()->NewInstance();
    Wrap<ContextifyContext>(wrapper, this);
    return scope.Close(wrapper);
  }


  Local<Context> CreateV8Context(Environment* env) {
    HandleScope scope(node_isolate);
    Local<FunctionTemplate> function_template = FunctionTemplate::New();
    function_template->SetHiddenPrototype(true);

    Local<Object> sandbox = PersistentToLocal(node_isolate, sandbox_);
    function_template->SetClassName(sandbox->GetConstructorName());

    Local<ObjectTemplate> object_template =
        function_template->InstanceTemplate();
    object_template->SetNamedPropertyHandler(GlobalPropertyGetterCallback,
                                             GlobalPropertySetterCallback,
                                             GlobalPropertyQueryCallback,
                                             GlobalPropertyDeleterCallback,
                                             GlobalPropertyEnumeratorCallback,
                                             CreateDataWrapper(env));
    object_template->SetAccessCheckCallbacks(GlobalPropertyNamedAccessCheck,
                                             GlobalPropertyIndexedAccessCheck);
    return scope.Close(Context::New(node_isolate, NULL, object_template));
  }


  static void Init(Environment* env, Local<Object> target) {
    Local<FunctionTemplate> function_template = FunctionTemplate::New();
    function_template->InstanceTemplate()->SetInternalFieldCount(1);
    env->set_script_data_constructor_function(function_template->GetFunction());

    NODE_SET_METHOD(target, "makeContext", MakeContext);
    NODE_SET_METHOD(target, "isContext", IsContext);
  }


  static void MakeContext(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args.GetIsolate());
    HandleScope handle_scope(args.GetIsolate());

    if (!args[0]->IsObject()) {
      return ThrowTypeError("sandbox argument must be an object.");
    }
    Local<Object> sandbox = args[0].As<Object>();

    Local<String> hidden_name =
        FIXED_ONE_BYTE_STRING(node_isolate, "_contextifyHidden");

    // Don't allow contextifying a sandbox multiple times.
    assert(sandbox->GetHiddenValue(hidden_name).IsEmpty());

    ContextifyContext* context = new ContextifyContext(env, sandbox);
    Local<External> hidden_context = External::New(context);
    sandbox->SetHiddenValue(hidden_name, hidden_context);
  }


  static void IsContext(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    if (!args[0]->IsObject()) {
      ThrowTypeError("sandbox must be an object");
      return;
    }
    Local<Object> sandbox = args[0].As<Object>();

    Local<String> hidden_name =
        FIXED_ONE_BYTE_STRING(node_isolate, "_contextifyHidden");

    args.GetReturnValue().Set(!sandbox->GetHiddenValue(hidden_name).IsEmpty());
  }


  template <class T>
  static void WeakCallback(Isolate* isolate,
                           Persistent<T>* target,
                           ContextifyContext* context) {
    target->ClearWeak();
    if (--context->references_ == 0)
      delete context;
  }


  static ContextifyContext* ContextFromContextifiedSandbox(
      const Local<Object>& sandbox) {
    Local<String> hidden_name =
        FIXED_ONE_BYTE_STRING(node_isolate, "_contextifyHidden");
    Local<Value> context_external_v = sandbox->GetHiddenValue(hidden_name);
    if (context_external_v.IsEmpty() || !context_external_v->IsExternal()) {
      return NULL;
    }
    Local<External> context_external = context_external_v.As<External>();

    return static_cast<ContextifyContext*>(context_external->Value());
  }


  static bool GlobalPropertyNamedAccessCheck(Local<Object> host,
                                             Local<Value> key,
                                             AccessType type,
                                             Local<Value> data) {
    return true;
  }


  static bool GlobalPropertyIndexedAccessCheck(Local<Object> host,
                                               uint32_t key,
                                               AccessType type,
                                               Local<Value> data) {
    return true;
  }


  static void GlobalPropertyGetterCallback(
      Local<String> property,
      const PropertyCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    ContextifyContext* ctx =
        Unwrap<ContextifyContext>(args.Data().As<Object>());

    Local<Object> sandbox = PersistentToLocal(node_isolate, ctx->sandbox_);
    Local<Value> rv = sandbox->GetRealNamedProperty(property);
    if (rv.IsEmpty()) {
      Local<Object> proxy_global = PersistentToLocal(node_isolate,
                                                     ctx->proxy_global_);
      rv = proxy_global->GetRealNamedProperty(property);
    }
    if (!rv.IsEmpty() && rv == ctx->sandbox_) {
      rv = PersistentToLocal(node_isolate, ctx->proxy_global_);
    }

    args.GetReturnValue().Set(rv);
  }


  static void GlobalPropertySetterCallback(
      Local<String> property,
      Local<Value> value,
      const PropertyCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    ContextifyContext* ctx =
        Unwrap<ContextifyContext>(args.Data().As<Object>());

    PersistentToLocal(node_isolate, ctx->sandbox_)->Set(property, value);
  }


  static void GlobalPropertyQueryCallback(
      Local<String> property,
      const PropertyCallbackInfo<Integer>& args) {
    HandleScope scope(node_isolate);

    ContextifyContext* ctx =
        Unwrap<ContextifyContext>(args.Data().As<Object>());

    Local<Object> sandbox = PersistentToLocal(node_isolate, ctx->sandbox_);
    Local<Object> proxy_global = PersistentToLocal(node_isolate,
                                                   ctx->proxy_global_);

    bool in_sandbox = sandbox->GetRealNamedProperty(property).IsEmpty();
    bool in_proxy_global =
        proxy_global->GetRealNamedProperty(property).IsEmpty();
    if (!in_sandbox || !in_proxy_global) {
      args.GetReturnValue().Set(None);
    }
  }


  static void GlobalPropertyDeleterCallback(
      Local<String> property,
      const PropertyCallbackInfo<Boolean>& args) {
    HandleScope scope(node_isolate);

    ContextifyContext* ctx =
        Unwrap<ContextifyContext>(args.Data().As<Object>());

    bool success = PersistentToLocal(node_isolate,
                                     ctx->sandbox_)->Delete(property);
    if (!success) {
      success = PersistentToLocal(node_isolate,
                                  ctx->proxy_global_)->Delete(property);
    }
    args.GetReturnValue().Set(success);
  }


  static void GlobalPropertyEnumeratorCallback(
      const PropertyCallbackInfo<Array>& args) {
    HandleScope scope(node_isolate);

    ContextifyContext* ctx =
        Unwrap<ContextifyContext>(args.Data().As<Object>());

    Local<Object> sandbox = PersistentToLocal(node_isolate, ctx->sandbox_);
    args.GetReturnValue().Set(sandbox->GetPropertyNames());
  }
};

class ContextifyScript : public WeakObject {
 private:
  Persistent<Script> script_;

 public:
  static void Init(Environment* env, Local<Object> target) {
    HandleScope scope(node_isolate);
    Local<String> class_name =
        FIXED_ONE_BYTE_STRING(node_isolate, "ContextifyScript");

    Local<FunctionTemplate> script_tmpl = FunctionTemplate::New(New);
    script_tmpl->InstanceTemplate()->SetInternalFieldCount(1);
    script_tmpl->SetClassName(class_name);
    NODE_SET_PROTOTYPE_METHOD(script_tmpl, "runInContext", RunInContext);
    NODE_SET_PROTOTYPE_METHOD(script_tmpl,
                              "runInThisContext",
                              RunInThisContext);

    target->Set(class_name, script_tmpl->GetFunction());
    env->set_script_context_constructor_template(script_tmpl);
  }


  // args: code, [options]
  static void New(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    if (!args.IsConstructCall()) {
      return ThrowError("Must call vm.Script as a constructor.");
    }

    ContextifyScript* contextify_script =
        new ContextifyScript(args.GetIsolate(), args.This());

    TryCatch try_catch;
    Local<String> code = args[0]->ToString();
    Local<String> filename = GetFilenameArg(args, 1);
    bool display_errors = GetDisplayErrorsArg(args, 1);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return;
    }

    Local<Context> context = Context::GetCurrent();
    Context::Scope context_scope(context);

    Local<Script> v8_script = Script::New(code, filename);

    if (v8_script.IsEmpty()) {
      if (display_errors) {
        DisplayExceptionLine(try_catch.Message());
      }
      try_catch.ReThrow();
      return;
    }
    contextify_script->script_.Reset(node_isolate, v8_script);
  }


  static bool InstanceOf(Environment* env, const Local<Value>& value) {
    return !value.IsEmpty() &&
           env->script_context_constructor_template()->HasInstance(value);
  }


  // args: [options]
  static void RunInThisContext(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args.GetIsolate());
    HandleScope handle_scope(args.GetIsolate());

    // Assemble arguments
    TryCatch try_catch;
    uint64_t timeout = GetTimeoutArg(args, 0);
    bool display_errors = GetDisplayErrorsArg(args, 0);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return;
    }

    // Do the eval within this context
    EvalMachine(env, timeout, display_errors, args, try_catch);
  }

  // args: sandbox, [options]
  static void RunInContext(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    // Assemble arguments
    TryCatch try_catch;
    if (!args[0]->IsObject()) {
      return ThrowTypeError("contextifiedSandbox argument must be an object.");
    }
    Local<Object> sandbox = args[0].As<Object>();
    int64_t timeout = GetTimeoutArg(args, 1);
    bool display_errors = GetDisplayErrorsArg(args, 1);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return;
    }

    // Get the context from the sandbox
    ContextifyContext* contextify_context =
        ContextifyContext::ContextFromContextifiedSandbox(sandbox);
    if (contextify_context == NULL) {
      return ThrowTypeError(
          "sandbox argument must have been converted to a context.");
    }

    // Do the eval within the context
    Context::Scope context_scope(contextify_context->context());
    EvalMachine(contextify_context->env(),
                timeout,
                display_errors,
                args,
                try_catch);
    contextify_context->CopyProperties();
  }

  static int64_t GetTimeoutArg(const FunctionCallbackInfo<Value>& args,
                               const int i) {
    if (args[i]->IsUndefined() || args[i]->IsString()) {
      return -1;
    }
    if (!args[i]->IsObject()) {
      ThrowTypeError("options must be an object");
      return -1;
    }

    Local<String> key = FIXED_ONE_BYTE_STRING(node_isolate, "timeout");
    Local<Value> value = args[i].As<Object>()->Get(key);
    if (value->IsUndefined()) {
      return -1;
    }
    int64_t timeout = value->IntegerValue();

    if (timeout <= 0) {
      ThrowRangeError("timeout must be a positive number");
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
      ThrowTypeError("options must be an object");
      return false;
    }

    Local<String> key = FIXED_ONE_BYTE_STRING(node_isolate, "displayErrors");
    Local<Value> value = args[i].As<Object>()->Get(key);

    return value->IsUndefined() ? true : value->BooleanValue();
  }


  static Local<String> GetFilenameArg(const FunctionCallbackInfo<Value>& args,
                                      const int i) {
    Local<String> defaultFilename =
        FIXED_ONE_BYTE_STRING(node_isolate, "evalmachine.<anonymous>");

    if (args[i]->IsUndefined()) {
      return defaultFilename;
    }
    if (args[i]->IsString()) {
      return args[i].As<String>();
    }
    if (!args[i]->IsObject()) {
      ThrowTypeError("options must be an object");
      return Local<String>();
    }

    Local<String> key = FIXED_ONE_BYTE_STRING(node_isolate, "filename");
    Local<Value> value = args[i].As<Object>()->Get(key);

    return value->IsUndefined() ? defaultFilename : value->ToString();
  }


  static void EvalMachine(Environment* env,
                          const int64_t timeout,
                          const bool display_errors,
                          const FunctionCallbackInfo<Value>& args,
                          TryCatch& try_catch) {
    if (!ContextifyScript::InstanceOf(env, args.This())) {
      return ThrowTypeError(
          "Script methods can only be called on script instances.");
    }

    ContextifyScript* wrapped_script =
        Unwrap<ContextifyScript>(args.This());
    Local<Script> script = PersistentToLocal(node_isolate,
                                             wrapped_script->script_);

    Local<Value> result;
    if (timeout != -1) {
      Watchdog wd(timeout);
      result = script->Run();
    } else {
      result = script->Run();
    }

    if (try_catch.HasCaught() && try_catch.HasTerminated()) {
      V8::CancelTerminateExecution(args.GetIsolate());
      return ThrowError("Script execution timed out.");
    }

    if (result.IsEmpty()) {
      // Error occurred during execution of the script.
      if (display_errors) {
        DisplayExceptionLine(try_catch.Message());
      }
      try_catch.ReThrow();
      return;
    }

    args.GetReturnValue().Set(result);
  }


  ContextifyScript(Isolate* isolate, Local<Object> object)
      : WeakObject(isolate, object) {
  }


  ~ContextifyScript() {
    script_.Dispose();
  }
};


void InitContextify(Handle<Object> target,
                    Handle<Value> unused,
                    Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  ContextifyContext::Init(env, target);
  ContextifyScript::Init(env, target);
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE(node_contextify, node::InitContextify);
