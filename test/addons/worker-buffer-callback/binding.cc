#include <node.h>
#include <node_buffer.h>
#include <v8.h>

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

uint32_t free_call_count = 0;

void GetFreeCallCount(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(free_call_count);
}

void Initialize(Local<Object> exports,
                Local<Value> module,
                Local<Context> context) {
  Isolate* isolate = context->GetIsolate();
  NODE_SET_METHOD(exports, "getFreeCallCount", GetFreeCallCount);

  char* data = new char;

  exports->Set(context,
               v8::String::NewFromUtf8(
                   isolate, "buffer").ToLocalChecked(),
               node::Buffer::New(
                   isolate,
                   data,
                   sizeof(char),
                   [](char* data, void* hint) {
                     delete data;
                     free_call_count++;
                   },
                   nullptr).ToLocalChecked()).Check();
}

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
