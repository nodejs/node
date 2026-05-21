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

  // Create a new context with Node.js-specific setup.
  v8::MaybeLocal<Context> maybe_context = node::NewContext(isolate);
  v8::Local<Context> new_context;
  if (!maybe_context.ToLocal(&new_context)) {
    return;
  }
  node::RegisterContext(env, new_context, "Addon Context", "addon://about");

  // Return the global proxy object.
  args.GetReturnValue().Set(new_context->Global());
}

void RunInContext(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope handle_scope(isolate);
  assert(args.Length() == 2);

  assert(args[0]->IsObject());
  Local<Object> global_proxy = args[0].As<Object>();
  v8::MaybeLocal<Context> maybe_context = global_proxy->GetCreationContext();
  v8::Local<Context> new_context;
  if (!maybe_context.ToLocal(&new_context)) {
    return;
  }
  Context::Scope context_scope(new_context);

  assert(args[1]->IsString());
  Local<String> source = args[1].As<String>();
  Local<Script> script;
  Local<Value> result;

  if (Script::Compile(new_context, source).ToLocal(&script) &&
      script->Run(new_context).ToLocal(&result)) {
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
