#include <node.h>
#include <v8.h>

namespace {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Script;
using v8::String;
using v8::Value;

void MakeContext(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope handle_scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  node::Environment* env = node::GetCurrentEnvironment(context);
  assert(env);

  node::ContextifyOptions options(
      String::NewFromUtf8Literal(isolate, "Addon Context"),
      String::NewFromUtf8Literal(isolate, "addon://about"),
      false,
      false,
      node::ContextifyOptions::MicrotaskMode::kDefault);
  // Create a new context with Node.js-specific vm setup.
  v8::MaybeLocal<Context> maybe_context =
      node::MakeContextify(env, {}, options);
  v8::Local<Context> vm_context;
  if (!maybe_context.ToLocal(&vm_context)) {
    return;
  }

  // Return the global proxy object.
  args.GetReturnValue().Set(vm_context->Global());
}

void RunInContext(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope handle_scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  node::Environment* env = node::GetCurrentEnvironment(context);
  assert(env);

  Local<Object> sandbox_obj = args[0].As<Object>();
  v8::MaybeLocal<Context> maybe_context =
      node::GetContextified(env, sandbox_obj);
  v8::Local<Context> vm_context;
  if (!maybe_context.ToLocal(&vm_context)) {
    return;
  }
  Context::Scope context_scope(vm_context);

  if (args.Length() < 2 || !args[1]->IsString()) {
    return;
  }
  Local<String> source = args[1].As<String>();
  Local<Script> script;
  Local<Value> result;

  if (Script::Compile(vm_context, source).ToLocal(&script) &&
      script->Run(vm_context).ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
}

void Initialize(Local<Object> exports,
                Local<Value> module,
                Local<Context> context) {
  NODE_SET_METHOD(exports, "makeContext", MakeContext);
  NODE_SET_METHOD(exports, "runInContext", RunInContext);
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
