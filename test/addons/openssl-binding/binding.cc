#include <node.h>
#include <assert.h>
#include <openssl/rand.h>

namespace {

using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::ArrayBufferView;
using v8::Object;
using v8::Context;
using v8::String;
using v8::Local;
using v8::Value;


inline void RandomBytes(const FunctionCallbackInfo<Value>& info) {
  assert(info[0]->IsArrayBufferView());
  auto view = info[0].As<ArrayBufferView>();
  auto byte_offset = view->ByteOffset();
  auto byte_length = view->ByteLength();
  assert(view->HasBuffer());
  auto buffer = view->Buffer();
  auto contents = buffer->GetContents();
  auto data = static_cast<unsigned char*>(contents.Data()) + byte_offset;
  assert(RAND_poll());
  auto rval = RAND_bytes(data, static_cast<int>(byte_length));
  info.GetReturnValue().Set(rval > 0);
}

inline void Initialize(Local<Object> exports,
                       Local<Value> module,
                       Local<Context> context) {
  auto isolate = context->GetIsolate();
  auto key = String::NewFromUtf8(isolate, "randomBytes");
  auto value = FunctionTemplate::New(isolate, RandomBytes)->GetFunction();
  assert(exports->Set(context, key, value).IsJust());
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
