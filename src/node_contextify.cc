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
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::None;
using v8::Object;
using v8::ObjectTemplate;
using v8::Persistent;
using v8::PropertyCallbackInfo;
using v8::Script;
using v8::String;
using v8::TryCatch;
using v8::Value;
using v8::V8;


class ContextifyContext : ObjectWrap {
 private:
  Persistent<Object> sandbox_;
  Persistent<Object> proxy_global_;
  static Persistent<Function> data_wrapper_ctor;

 public:
  Persistent<Context> context_;
  static Persistent<FunctionTemplate> js_tmpl;

  explicit ContextifyContext(Local<Object> sandbox) :
      sandbox_(node_isolate, sandbox) {
  }


  ~ContextifyContext() {
    context_.Dispose();
    proxy_global_.Dispose();
    sandbox_.Dispose();
  }


  // We override ObjectWrap::Wrap so that we can create our context after
  // we have a reference to our "host" JavaScript object.  If we try to use
  // handle_ in the ContextifyContext constructor, it will be empty since it's
  // set in ObjectWrap::Wrap.
  inline void Wrap(Local<Object> handle) {
    HandleScope scope(node_isolate);
    ObjectWrap::Wrap(handle);
    Local<Context> v8_context = CreateV8Context();
    context_.Reset(node_isolate, v8_context);
    proxy_global_.Reset(node_isolate, v8_context->Global());
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

    js_tmpl.Reset(node_isolate, FunctionTemplate::New(New));
    Local<FunctionTemplate> ljs_tmpl = PersistentToLocal(node_isolate, js_tmpl);
    ljs_tmpl->InstanceTemplate()->SetInternalFieldCount(1);

    Local<String> class_name
        = FIXED_ONE_BYTE_STRING(node_isolate, "ContextifyContext");
    ljs_tmpl->SetClassName(class_name);
    target->Set(class_name, ljs_tmpl->GetFunction());

    NODE_SET_METHOD(target, "makeContext", MakeContext);
  }


  // args[0] = the sandbox object
  static void New(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    if (!args[0]->IsObject()) {
      return ThrowTypeError("sandbox argument must be an object.");
    }
    ContextifyContext* ctx = new ContextifyContext(args[0].As<Object>());
    ctx->Wrap(args.This());
  }


  static void MakeContext(const FunctionCallbackInfo<Value>& args) {
    Local<Object> sandbox = args[0].As<Object>();

    Local<FunctionTemplate> ljs_tmpl = PersistentToLocal(node_isolate, js_tmpl);
    Local<Value> constructor_args[] = { sandbox };
    Local<Object> contextify_context_object =
        ljs_tmpl->GetFunction()->NewInstance(1, constructor_args);

    Local<String> hidden_name =
        FIXED_ONE_BYTE_STRING(node_isolate, "_contextifyHidden");
    sandbox->SetHiddenValue(hidden_name, contextify_context_object);
  }


  static const Local<Context> ContextFromContextifiedSandbox(
      const Local<Object>& sandbox) {
    Local<String> hidden_name =
        FIXED_ONE_BYTE_STRING(node_isolate, "_contextifyHidden");
    Local<Object> hidden_context =
        sandbox->GetHiddenValue(hidden_name).As<Object>();

    if (hidden_context.IsEmpty()) {
      ThrowTypeError("sandbox argument must have been converted to a context.");
      return Local<Context>();
    }

    ContextifyContext* ctx =
        ObjectWrap::Unwrap<ContextifyContext>(hidden_context);
    Persistent<Context> context;
    context.Reset(node_isolate, ctx->context_);
    return PersistentToLocal(node_isolate, context);
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
        ContextifyContext::ContextFromContextifiedSandbox(sandbox);
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


Persistent<FunctionTemplate> ContextifyContext::js_tmpl;
Persistent<Function> ContextifyContext::data_wrapper_ctor;

Persistent<FunctionTemplate> ContextifyScript::script_tmpl;

void InitContextify(Local<Object> target) {
  HandleScope scope(node_isolate);
  ContextifyContext::Init(target);
  ContextifyScript::Init(target);
}

}  // namespace node

NODE_MODULE(node_contextify, node::InitContextify);
