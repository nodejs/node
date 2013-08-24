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

namespace node {

using v8::AccessType;
using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
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
  Persistent<Object> sandbox_;
  Persistent<Object> proxy_global_;
  Persistent<Context> context_;
  static Persistent<Function> data_wrapper_ctor;

 public:
  explicit ContextifyContext(Local<Object> sandbox) :
      sandbox_(node_isolate, sandbox) {
    HandleScope scope(node_isolate);
    Local<Context> v8_context = CreateV8Context();
    context_.Reset(node_isolate, v8_context);
    proxy_global_.Reset(node_isolate, v8_context->Global());
    sandbox_.MakeWeak(this, SandboxFreeCallback);
    sandbox_.MarkIndependent();
  }


  ~ContextifyContext() {
    context_.Dispose();
    proxy_global_.Dispose();
    sandbox_.Dispose();
  }


  // This is an object that just keeps an internal pointer to this
  // ContextifyContext.  It's passed to the NamedPropertyHandler.  If we
  // pass the main JavaScript context object we're embedded in, then the
  // NamedPropertyHandler will store a reference to it forever and keep it
  // from getting gc'd.
  Local<Value> CreateDataWrapper() {
    HandleScope scope(node_isolate);
    Local<Function> ctor = PersistentToLocal(node_isolate, data_wrapper_ctor);
    Local<Object> wrapper = ctor->NewInstance();
    NODE_WRAP(wrapper, this);
    return scope.Close(wrapper);
  }


  Local<Context> CreateV8Context() {
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
                                             CreateDataWrapper());
    object_template->SetAccessCheckCallbacks(GlobalPropertyNamedAccessCheck,
                                             GlobalPropertyIndexedAccessCheck);
    return scope.Close(Context::New(node_isolate, NULL, object_template));
  }


  static void Init(Local<Object> target) {
    HandleScope scope(node_isolate);

    Local<FunctionTemplate> function_template = FunctionTemplate::New();
    function_template->InstanceTemplate()->SetInternalFieldCount(1);
    data_wrapper_ctor.Reset(node_isolate, function_template->GetFunction());

    NODE_SET_METHOD(target, "makeContext", MakeContext);
    NODE_SET_METHOD(target, "isContext", IsContext);
  }


  static void MakeContext(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    if (!args[0]->IsObject()) {
      return ThrowTypeError("sandbox argument must be an object.");
    }
    Local<Object> sandbox = args[0].As<Object>();

    Local<String> hidden_name =
        FIXED_ONE_BYTE_STRING(node_isolate, "_contextifyHidden");

    // Don't allow contextifying a sandbox multiple times.
    assert(sandbox->GetHiddenValue(hidden_name).IsEmpty());

    ContextifyContext* context = new ContextifyContext(sandbox);
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


  static void SandboxFreeCallback(Isolate* isolate,
                                  Persistent<Object>* target,
                                  ContextifyContext* context) {
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

  static Local<Context> V8ContextFromContextifiedSandbox(
      const Local<Object>& sandbox) {
    ContextifyContext* contextify_context =
        ContextFromContextifiedSandbox(sandbox);
    if (contextify_context == NULL) {
      ThrowTypeError("sandbox argument must have been converted to a context.");
      return Local<Context>();
    }

    return PersistentToLocal(node_isolate, contextify_context->context_);
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

    Local<Object> data = args.Data()->ToObject();
    ContextifyContext* ctx = ObjectWrap::Unwrap<ContextifyContext>(data);

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

    Local<Object> data = args.Data()->ToObject();
    ContextifyContext* ctx = ObjectWrap::Unwrap<ContextifyContext>(data);

    PersistentToLocal(node_isolate, ctx->sandbox_)->Set(property, value);
  }


  static void GlobalPropertyQueryCallback(
      Local<String> property,
      const PropertyCallbackInfo<Integer>& args) {
    HandleScope scope(node_isolate);

    Local<Object> data = args.Data()->ToObject();
    ContextifyContext* ctx = ObjectWrap::Unwrap<ContextifyContext>(data);

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

    Local<Object> data = args.Data()->ToObject();
    ContextifyContext* ctx = ObjectWrap::Unwrap<ContextifyContext>(data);

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

    Local<Object> data = args.Data()->ToObject();
    ContextifyContext* ctx = ObjectWrap::Unwrap<ContextifyContext>(data);

    Local<Object> sandbox = PersistentToLocal(node_isolate, ctx->sandbox_);
    args.GetReturnValue().Set(sandbox->GetPropertyNames());
  }
};

class ContextifyScript : ObjectWrap {
 private:
  Persistent<Script> script_;

 public:
  static Persistent<FunctionTemplate> script_tmpl;

  static void Init(Local<Object> target) {
    HandleScope scope(node_isolate);
    Local<String> class_name =
        FIXED_ONE_BYTE_STRING(node_isolate, "ContextifyScript");

    script_tmpl.Reset(node_isolate, FunctionTemplate::New(New));
    Local<FunctionTemplate> lscript_tmpl =
        PersistentToLocal(node_isolate, script_tmpl);
    lscript_tmpl->InstanceTemplate()->SetInternalFieldCount(1);
    lscript_tmpl->SetClassName(class_name);
    NODE_SET_PROTOTYPE_METHOD(lscript_tmpl, "runInContext", RunInContext);
    NODE_SET_PROTOTYPE_METHOD(lscript_tmpl,
                              "runInThisContext",
                              RunInThisContext);

    target->Set(class_name, lscript_tmpl->GetFunction());
  }


  // args: code, [filename]
  static void New(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    if (!args.IsConstructCall()) {
      return ThrowError("Must call vm.Script as a constructor.");
    }

    ContextifyScript *contextify_script = new ContextifyScript();
    contextify_script->Wrap(args.Holder());
    Local<String> code = args[0]->ToString();
    Local<String> filename = GetFilenameArg(args, 1);
    bool display_exception = GetDisplayArg(args, 2);

    Local<Context> context = Context::GetCurrent();
    Context::Scope context_scope(context);

    TryCatch try_catch;

    Local<Script> v8_script = Script::New(code, filename);

    if (v8_script.IsEmpty()) {
      if (display_exception)
        DisplayExceptionLine(try_catch.Message());
      try_catch.ReThrow();
      return;
    }
    contextify_script->script_.Reset(node_isolate, v8_script);
  }


  static bool InstanceOf(const Local<Value>& value) {
    return !value.IsEmpty() &&
        PersistentToLocal(node_isolate, script_tmpl)->HasInstance(value);
  }


  // args: [timeout]
  static void RunInThisContext(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    // Assemble arguments
    TryCatch try_catch;
    uint64_t timeout = GetTimeoutArg(args, 0);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return;
    }

    bool display_exception = GetDisplayArg(args, 1);

    // Do the eval within this context
    EvalMachine(timeout, display_exception, args, try_catch);
  }

  // args: sandbox, [timeout]
  static void RunInContext(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    // Assemble arguments
    TryCatch try_catch;
    if (!args[0]->IsObject()) {
      return ThrowTypeError("sandbox argument must be an object.");
    }
    Local<Object> sandbox = args[0].As<Object>();
    uint64_t timeout = GetTimeoutArg(args, 1);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return;
    }

    bool display_exception = GetDisplayArg(args, 2);

    // Get the context from the sandbox
    Local<Context> context =
        ContextifyContext::V8ContextFromContextifiedSandbox(sandbox);
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return;
    }

    // Do the eval within the context
    Context::Scope context_scope(context);
    EvalMachine(timeout, display_exception, args, try_catch);
  }

  static int64_t GetTimeoutArg(const FunctionCallbackInfo<Value>& args,
                               const int i) {
    if (args[i]->IsUndefined()) {
      return 0;
    }

    int64_t timeout = args[i]->IntegerValue();
    if (timeout < 0) {
      ThrowRangeError("timeout must be a positive number");
    }
    return timeout;
  }

  static bool GetDisplayArg(const FunctionCallbackInfo<Value>& args,
                            const int i) {
    bool display_exception = true;

    if (args[i]->IsBoolean())
      display_exception = args[i]->BooleanValue();

    return display_exception;
  }


  static Local<String> GetFilenameArg(const FunctionCallbackInfo<Value>& args,
                                      const int i) {
    return !args[i]->IsUndefined()
        ? args[i]->ToString()
        : FIXED_ONE_BYTE_STRING(node_isolate, "evalmachine.<anonymous>");
  }


  static void EvalMachine(const int64_t timeout,
                          const bool display_exception,
                          const FunctionCallbackInfo<Value>& args,
                          TryCatch& try_catch) {
    if (!ContextifyScript::InstanceOf(args.This())) {
      return ThrowTypeError(
          "Script methods can only be called on script instances.");
    }

    ContextifyScript* wrapped_script =
        ObjectWrap::Unwrap<ContextifyScript>(args.This());
    Local<Script> script = PersistentToLocal(node_isolate,
                                             wrapped_script->script_);
    if (script.IsEmpty()) {
      if (display_exception)
        DisplayExceptionLine(try_catch.Message());
      try_catch.ReThrow();
      return;
    }

    Local<Value> result;
    if (timeout) {
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
      if (display_exception)
        DisplayExceptionLine(try_catch.Message());
      try_catch.ReThrow();
      return;
    }

    args.GetReturnValue().Set(result);
  }


  ~ContextifyScript() {
    script_.Dispose();
  }
};


Persistent<Function> ContextifyContext::data_wrapper_ctor;
Persistent<FunctionTemplate> ContextifyScript::script_tmpl;

void InitContextify(Local<Object> target) {
  HandleScope scope(node_isolate);
  ContextifyContext::Init(target);
  ContextifyScript::Init(target);
}

}  // namespace node

NODE_MODULE(node_contextify, node::InitContextify);
