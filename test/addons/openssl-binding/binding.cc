#include <assert.h>
#include <node.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

namespace {

inline void RandomBytes(const v8::FunctionCallbackInfo<v8::Value>& info) {
  assert(info[0]->IsArrayBufferView());
  auto view = info[0].As<v8::ArrayBufferView>();
  auto byte_offset = view->ByteOffset();
  auto byte_length = view->ByteLength();
  assert(view->HasBuffer());
  auto buffer = view->Buffer();
  auto contents = buffer->GetBackingStore();
  auto data = static_cast<unsigned char*>(contents->Data()) + byte_offset;
  assert(RAND_poll());
  auto rval = RAND_bytes(data, static_cast<int>(byte_length));
  info.GetReturnValue().Set(rval > 0);
}

inline void Hash(const v8::FunctionCallbackInfo<v8::Value>& info) {
  assert(info[0]->IsArrayBufferView());
  auto view = info[0].As<v8::ArrayBufferView>();
  auto byte_offset = view->ByteOffset();
  auto len = view->ByteLength();
  assert(view->HasBuffer());
  auto buffer = view->Buffer();
  auto contents = buffer->GetBackingStore();
  auto data = static_cast<unsigned char*>(contents->Data()) + byte_offset;
  unsigned char md[MD5_DIGEST_LENGTH];
  MD5_CTX c;
  auto rval = MD5_Init(&c) && MD5_Update(&c, data, len) && MD5_Final(md, &c);
  info.GetReturnValue().Set(rval > 0);
}

inline void Initialize(v8::Local<v8::Object> exports,
                       v8::Local<v8::Value> module,
                       v8::Local<v8::Context> context) {
  auto isolate = context->GetIsolate();
  auto key = v8::String::NewFromUtf8(
      isolate, "randomBytes").ToLocalChecked();
  auto value = v8::FunctionTemplate::New(isolate, RandomBytes)
                   ->GetFunction(context)
                   .ToLocalChecked();
  assert(exports->Set(context, key, value).IsJust());

  const SSL_METHOD* method = TLSv1_2_server_method();
  assert(method != nullptr);

  key = v8::String::NewFromUtf8(isolate, "hash").ToLocalChecked();
  value = v8::FunctionTemplate::New(isolate, Hash)
              ->GetFunction(context)
              .ToLocalChecked();
  assert(exports->Set(context, key, value).IsJust());
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
