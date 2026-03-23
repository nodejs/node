#include <node.h>
#include <openssl/ssl.h>

namespace {

// Test: extract SSL_CTX* from a SecureContext object via
// node::crypto::GetSSLCtx.
void GetSSLCtx(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  SSL_CTX* ctx = node::crypto::GetSSLCtx(context, args[0]);
  if (ctx == nullptr) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(
            isolate, "GetSSLCtx returned nullptr for a valid SecureContext")
            .ToLocalChecked()));
    return;
  }

  // Verify the pointer is a valid SSL_CTX by calling an OpenSSL function.
  const SSL_METHOD* method = SSL_CTX_get_ssl_method(ctx);
  if (method == nullptr) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate,
                                "SSL_CTX_get_ssl_method returned nullptr")
            .ToLocalChecked()));
    return;
  }

  args.GetReturnValue().Set(true);
}

// Test: passing a non-SecureContext value returns nullptr.
void GetSSLCtxInvalid(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  SSL_CTX* ctx = node::crypto::GetSSLCtx(context, args[0]);
  args.GetReturnValue().Set(ctx == nullptr);
}

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> module,
                v8::Local<v8::Context> context) {
  NODE_SET_METHOD(exports, "getSSLCtx", GetSSLCtx);
  NODE_SET_METHOD(exports, "getSSLCtxInvalid", GetSSLCtxInvalid);
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
