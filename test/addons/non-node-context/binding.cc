#include <node.h>
#include <node_buffer.h>
#include <assert.h>

namespace {

using v8::Context;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::Script;
using v8::String;
using v8::Value;

inline void MakeBufferInNewContext(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  // This should throw an exception, rather than actually do anything.
  MaybeLocal<Object> buf = node::Buffer::Copy(isolate, "foo", 3);
  assert(buf.IsEmpty());
}

inline void RunInNewContext(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  context->Global()->Set(
      context,
      String::NewFromUtf8(isolate, "data").ToLocalChecked(),
      args[1]).FromJust();

  assert(args[0]->IsString());  // source code
  Local<Script> script;
  Local<Value> ret;
  if (!Script::Compile(context, args[0].As<String>()).ToLocal(&script) ||
      !script->Run(context).ToLocal(&ret)) {
    return;
  }

  args.GetReturnValue().Set(ret);
}

inline void Initialize(Local<Object> exports,
                       Local<Value> module,
                       Local<Context> context) {
  NODE_SET_METHOD(exports, "runInNewContext", RunInNewContext);
  NODE_SET_METHOD(exports, "makeBufferInNewContext", MakeBufferInNewContext);
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
