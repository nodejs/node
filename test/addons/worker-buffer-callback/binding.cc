#include <node.h>
#include <node_buffer.h>
#include <v8.h>

using v8::Context;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

char data[] = "hello";

void Initialize(Local<Object> exports,
                Local<Value> module,
                Local<Context> context) {
  Isolate* isolate = context->GetIsolate();
  exports->Set(context,
               v8::String::NewFromUtf8(
                   isolate, "buffer", v8::NewStringType::kNormal)
                   .ToLocalChecked(),
               node::Buffer::New(
                   isolate,
                   data,
                   sizeof(data),
                   [](char* data, void* hint) {},
                   nullptr).ToLocalChecked()).Check();
}

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
