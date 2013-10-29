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

#include "tls_wrap.h"
#include "async-wrap.h"
#include "async-wrap-inl.h"
#include "node_buffer.h"  // Buffer
#include "node_crypto.h"  // SecureContext
#include "node_crypto_bio.h"  // NodeBIO
#include "node_crypto_clienthello.h"  // ClientHelloParser
#include "node_crypto_clienthello-inl.h"
#include "node_wrap.h"  // WithGenericStream
#include "node_counters.h"
#include "node_internals.h"
#include "util.h"
#include "util-inl.h"

namespace node {

using crypto::SSLWrap;
using crypto::SecureContext;
using v8::Boolean;
using v8::Context;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Null;
using v8::Object;
using v8::String;
using v8::Value;

static const int X509_NAME_FLAGS = ASN1_STRFLGS_ESC_CTRL
                                 | ASN1_STRFLGS_ESC_MSB
                                 | XN_FLAG_SEP_MULTILINE
                                 | XN_FLAG_FN_SN;


TLSCallbacks::TLSCallbacks(Environment* env,
                           Kind kind,
                           Handle<Object> sc,
                           StreamWrapCallbacks* old)
    : SSLWrap<TLSCallbacks>(env, Unwrap<SecureContext>(sc), kind),
      StreamWrapCallbacks(old),
      AsyncWrap(env, env->tls_wrap_constructor_function()->NewInstance()),
      enc_in_(NULL),
      enc_out_(NULL),
      clear_in_(NULL),
      write_size_(0),
      pending_write_item_(NULL),
      started_(false),
      established_(false),
      shutdown_(false) {

  // Persist SecureContext
  sc_ = Unwrap<SecureContext>(sc);
  sc_handle_.Reset(node_isolate, sc);

  node::Wrap<TLSCallbacks>(object(), this);

  // Initialize queue for clearIn writes
  QUEUE_INIT(&write_item_queue_);

  // We've our own session callbacks
  SSL_CTX_sess_set_get_cb(sc_->ctx_, SSLWrap<TLSCallbacks>::GetSessionCallback);
  SSL_CTX_sess_set_new_cb(sc_->ctx_, SSLWrap<TLSCallbacks>::NewSessionCallback);

  InitSSL();
}


TLSCallbacks::~TLSCallbacks() {
  enc_in_ = NULL;
  enc_out_ = NULL;
  delete clear_in_;
  clear_in_ = NULL;

  sc_ = NULL;
  sc_handle_.Dispose();
  persistent().Dispose();

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  sni_context_.Dispose();
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
}


void TLSCallbacks::InvokeQueued(int status) {
  // Empty queue - ignore call
  if (pending_write_item_ == NULL)
    return;

  QUEUE* q = &pending_write_item_->member_;
  pending_write_item_ = NULL;

  // Process old queue
  while (q != &write_item_queue_) {
    QUEUE* next = static_cast<QUEUE*>(QUEUE_NEXT(q));
    WriteItem* wi = container_of(q, WriteItem, member_);
    wi->cb_(&wi->w_->req_, status);
    delete wi;
    q = next;
  }
}


void TLSCallbacks::InitSSL() {
  // Initialize SSL
  enc_in_ = NodeBIO::New();
  enc_out_ = NodeBIO::New();

  SSL_set_bio(ssl_, enc_in_, enc_out_);

  // NOTE: This could be overriden in SetVerifyMode
  SSL_set_verify(ssl_, SSL_VERIFY_NONE, crypto::VerifyCallback);

#ifdef SSL_MODE_RELEASE_BUFFERS
  long mode = SSL_get_mode(ssl_);
  SSL_set_mode(ssl_, mode | SSL_MODE_RELEASE_BUFFERS);
#endif  // SSL_MODE_RELEASE_BUFFERS

  SSL_set_app_data(ssl_, this);
  SSL_set_info_callback(ssl_, SSLInfoCallback);

  if (is_server()) {
    SSL_set_accept_state(ssl_);

#ifdef OPENSSL_NPN_NEGOTIATED
    // Server should advertise NPN protocols
    SSL_CTX_set_next_protos_advertised_cb(
        sc_->ctx_,
        SSLWrap<TLSCallbacks>::AdvertiseNextProtoCallback,
        this);
#endif  // OPENSSL_NPN_NEGOTIATED

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
    SSL_CTX_set_tlsext_servername_callback(sc_->ctx_, SelectSNIContextCallback);
    SSL_CTX_set_tlsext_servername_arg(sc_->ctx_, this);
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  } else if (is_client()) {
    SSL_set_connect_state(ssl_);

#ifdef OPENSSL_NPN_NEGOTIATED
    // Client should select protocol from list of advertised
    // If server supports NPN
    SSL_CTX_set_next_proto_select_cb(
        sc_->ctx_,
        SSLWrap<TLSCallbacks>::SelectNextProtoCallback,
        this);
#endif  // OPENSSL_NPN_NEGOTIATED
  } else {
    // Unexpected
    abort();
  }

  // Initialize ring for queud clear data
  clear_in_ = new NodeBIO();
}


void TLSCallbacks::Wrap(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  if (args.Length() < 1 || !args[0]->IsObject())
    return ThrowTypeError("First argument should be a StreamWrap instance");
  if (args.Length() < 2 || !args[1]->IsObject())
    return ThrowTypeError("Second argument should be a SecureContext instance");
  if (args.Length() < 3 || !args[2]->IsBoolean())
    return ThrowTypeError("Third argument should be boolean");

  Local<Object> stream = args[0].As<Object>();
  Local<Object> sc = args[1].As<Object>();
  Kind kind = args[2]->IsTrue() ? SSLWrap<TLSCallbacks>::kServer :
                                  SSLWrap<TLSCallbacks>::kClient;

  TLSCallbacks* callbacks = NULL;
  WITH_GENERIC_STREAM(env, stream, {
    callbacks = new TLSCallbacks(env, kind, sc, wrap->callbacks());
    wrap->OverrideCallbacks(callbacks);
  });

  if (callbacks == NULL) {
    return args.GetReturnValue().SetNull();
  }

  args.GetReturnValue().Set(callbacks->persistent());
}


void TLSCallbacks::Start(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.This());

  if (wrap->started_)
    return ThrowError("Already started.");
  wrap->started_ = true;

  // Send ClientHello handshake
  assert(wrap->is_client());
  wrap->ClearOut();
  wrap->EncOut();
}


void TLSCallbacks::SSLInfoCallback(const SSL* ssl_, int where, int ret) {
  if (!(where & (SSL_CB_HANDSHAKE_START | SSL_CB_HANDSHAKE_DONE)))
    return;

  // Be compatible with older versions of OpenSSL. SSL_get_app_data() wants
  // a non-const SSL* in OpenSSL <= 0.9.7e.
  SSL* ssl = const_cast<SSL*>(ssl_);
  TLSCallbacks* c = static_cast<TLSCallbacks*>(SSL_get_app_data(ssl));
  Environment* env = c->env();
  // There should be a Context::Scope a few stack frames down.
  assert(env->context() == env->isolate()->GetCurrentContext());
  HandleScope handle_scope(env->isolate());
  Local<Object> object = c->object();

  if (where & SSL_CB_HANDSHAKE_START) {
    Local<Value> callback = object->Get(env->onhandshakestart_string());
    if (callback->IsFunction()) {
      c->MakeCallback(callback.As<Function>(), 0, NULL);
    }
  }

  if (where & SSL_CB_HANDSHAKE_DONE) {
    c->established_ = true;
    Local<Value> callback = object->Get(env->onhandshakedone_string());
    if (callback->IsFunction()) {
      c->MakeCallback(callback.As<Function>(), 0, NULL);
    }
  }
}


void TLSCallbacks::EncOut() {
  // Ignore cycling data if ClientHello wasn't yet parsed
  if (!hello_parser_.IsEnded())
    return;

  // Write in progress
  if (write_size_ != 0)
    return;

  // Split-off queue
  if (established_ && !QUEUE_EMPTY(&write_item_queue_)) {
    pending_write_item_ = container_of(QUEUE_NEXT(&write_item_queue_),
                                       WriteItem,
                                       member_);
    QUEUE_INIT(&write_item_queue_);
  }

  // No data to write
  if (BIO_pending(enc_out_) == 0) {
    InvokeQueued(0);
    return;
  }

  char* data = NodeBIO::FromBIO(enc_out_)->Peek(&write_size_);
  assert(write_size_ != 0);

  write_req_.data = this;
  uv_buf_t buf = uv_buf_init(data, write_size_);
  int r = uv_write(&write_req_, wrap()->stream(), &buf, 1, EncOutCb);

  // Ignore errors, this should be already handled in js
  if (!r) {
    if (wrap()->is_tcp()) {
      NODE_COUNT_NET_BYTES_SENT(write_size_);
    } else if (wrap()->is_named_pipe()) {
      NODE_COUNT_PIPE_BYTES_SENT(write_size_);
    }
  }
}


void TLSCallbacks::EncOutCb(uv_write_t* req, int status) {
  TLSCallbacks* callbacks = static_cast<TLSCallbacks*>(req->data);
  Environment* env = callbacks->env();

  // Handle error
  if (status) {
    // Ignore errors after shutdown
    if (callbacks->shutdown_)
      return;

    // Notify about error
    Context::Scope context_scope(env->context());
    HandleScope handle_scope(env->isolate());
    Local<Value> arg = String::Concat(
        FIXED_ONE_BYTE_STRING(node_isolate, "write cb error, status: "),
        Integer::New(status, node_isolate)->ToString());
    callbacks->MakeCallback(env->onerror_string(), 1, &arg);
    callbacks->InvokeQueued(status);
    return;
  }

  // Commit
  NodeBIO::FromBIO(callbacks->enc_out_)->Read(NULL, callbacks->write_size_);

  // Try writing more data
  callbacks->write_size_ = 0;
  callbacks->EncOut();
}


Local<Value> TLSCallbacks::GetSSLError(int status, int* err) {
  HandleScope scope(node_isolate);

  *err = SSL_get_error(ssl_, status);
  switch (*err) {
    case SSL_ERROR_NONE:
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      break;
    case SSL_ERROR_ZERO_RETURN:
      return scope.Close(FIXED_ONE_BYTE_STRING(node_isolate, "ZERO_RETURN"));
      break;
    default:
      {
        BUF_MEM* mem;
        BIO* bio;

        assert(*err == SSL_ERROR_SSL || *err == SSL_ERROR_SYSCALL);

        bio = BIO_new(BIO_s_mem());
        assert(bio != NULL);
        ERR_print_errors(bio);
        BIO_get_mem_ptr(bio, &mem);
        Local<String> message =
            OneByteString(node_isolate, mem->data, mem->length);
        Local<Value> exception = Exception::Error(message);
        BIO_free_all(bio);

        return scope.Close(exception);
      }
  }
  return Local<Value>();
}


void TLSCallbacks::ClearOut() {
  // Ignore cycling data if ClientHello wasn't yet parsed
  if (!hello_parser_.IsEnded())
    return;

  Context::Scope context_scope(env()->context());
  HandleScope handle_scope(env()->isolate());

  assert(ssl_ != NULL);

  char out[kClearOutChunkSize];
  int read;
  do {
    read = SSL_read(ssl_, out, sizeof(out));
    if (read > 0) {
      Local<Value> argv[] = {
        Integer::New(read, node_isolate),
        Buffer::New(env(), out, read)
      };
      wrap()->MakeCallback(env()->onread_string(), ARRAY_SIZE(argv), argv);
    }
  } while (read > 0);

  if (read == -1) {
    int err;
    Handle<Value> arg = GetSSLError(read, &err);

    if (!arg.IsEmpty()) {
      MakeCallback(env()->onerror_string(), 1, &arg);
    }
  }
}


bool TLSCallbacks::ClearIn() {
  // Ignore cycling data if ClientHello wasn't yet parsed
  if (!hello_parser_.IsEnded())
    return false;

  int written = 0;
  while (clear_in_->Length() > 0) {
    size_t avail = 0;
    char* data = clear_in_->Peek(&avail);
    written = SSL_write(ssl_, data, avail);
    assert(written == -1 || written == static_cast<int>(avail));
    if (written == -1)
      break;
    clear_in_->Read(NULL, avail);
  }

  // All written
  if (clear_in_->Length() == 0) {
    assert(written >= 0);
    return true;
  }

  Context::Scope context_scope(env()->context());
  HandleScope handle_scope(env()->isolate());

  // Error or partial write
  int err;
  Handle<Value> arg = GetSSLError(written, &err);
  if (!arg.IsEmpty()) {
    MakeCallback(env()->onerror_string(), 1, &arg);
  }

  return false;
}


int TLSCallbacks::DoWrite(WriteWrap* w,
                          uv_buf_t* bufs,
                          size_t count,
                          uv_stream_t* send_handle,
                          uv_write_cb cb) {
  assert(send_handle == NULL);

  // Queue callback to execute it on next tick
  WriteItem* wi = new WriteItem(w, cb);
  bool empty = true;

  // Empty writes should not go through encryption process
  size_t i;
  for (i = 0; i < count; i++)
    if (bufs[i].len > 0) {
      empty = false;
      break;
    }
  if (empty) {
    ClearOut();
    // However if there any data that should be written to socket,
    // callback should not be invoked immediately
    if (BIO_pending(enc_out_) == 0)
      return uv_write(&w->req_, wrap()->stream(), bufs, count, cb);
  }

  QUEUE_INSERT_TAIL(&write_item_queue_, &wi->member_);

  // Write queued data
  if (empty) {
    EncOut();
    return 0;
  }

  // Process enqueued data first
  if (!ClearIn()) {
    // If there're still data to process - enqueue current one
    for (i = 0; i < count; i++)
      clear_in_->Write(bufs[i].base, bufs[i].len);
    return 0;
  }

  int written = 0;
  for (i = 0; i < count; i++) {
    written = SSL_write(ssl_, bufs[i].base, bufs[i].len);
    assert(written == -1 || written == static_cast<int>(bufs[i].len));
    if (written == -1)
      break;
  }

  if (i != count) {
    int err;
    Context::Scope context_scope(env()->context());
    HandleScope handle_scope(env()->isolate());
    Handle<Value> arg = GetSSLError(written, &err);
    if (!arg.IsEmpty()) {
      MakeCallback(env()->onerror_string(), 1, &arg);
      return -1;
    }

    // No errors, queue rest
    for (; i < count; i++)
      clear_in_->Write(bufs[i].base, bufs[i].len);
  }

  // Try writing data immediately
  EncOut();

  return 0;
}


void TLSCallbacks::AfterWrite(WriteWrap* w) {
  // Intentionally empty
}


void TLSCallbacks::DoAlloc(uv_handle_t* handle,
                           size_t suggested_size,
                           uv_buf_t* buf) {
  buf->base = NodeBIO::FromBIO(enc_in_)->PeekWritable(&suggested_size);
  buf->len = suggested_size;
}


void TLSCallbacks::DoRead(uv_stream_t* handle,
                          ssize_t nread,
                          const uv_buf_t* buf,
                          uv_handle_type pending) {
  if (nread < 0)  {
    // Error should be emitted only after all data was read
    ClearOut();
    Context::Scope context_scope(env()->context());
    HandleScope handle_scope(env()->isolate());
    Local<Value> arg = Integer::New(nread, node_isolate);
    wrap()->MakeCallback(env()->onread_string(), 1, &arg);
    return;
  }

  // Only client connections can receive data
  assert(ssl_ != NULL);

  // Commit read data
  NodeBIO* enc_in = NodeBIO::FromBIO(enc_in_);
  enc_in->Commit(nread);

  // Parse ClientHello first
  if (!hello_parser_.IsEnded()) {
    size_t avail = 0;
    uint8_t* data = reinterpret_cast<uint8_t*>(enc_in->Peek(&avail));
    assert(avail == 0 || data != NULL);
    return hello_parser_.Parse(data, avail);
  }

  // Cycle OpenSSL's state
  Cycle();
}


int TLSCallbacks::DoShutdown(ShutdownWrap* req_wrap, uv_shutdown_cb cb) {
  if (SSL_shutdown(ssl_) == 0)
    SSL_shutdown(ssl_);
  shutdown_ = true;
  EncOut();
  return StreamWrapCallbacks::DoShutdown(req_wrap, cb);
}


void TLSCallbacks::SetVerifyMode(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.This());

  if (args.Length() < 2 || !args[0]->IsBoolean() || !args[1]->IsBoolean())
    return ThrowTypeError("Bad arguments, expected two booleans");

  int verify_mode;
  if (wrap->is_server()) {
    bool request_cert = args[0]->IsTrue();
    if (!request_cert) {
      // Note reject_unauthorized ignored.
      verify_mode = SSL_VERIFY_NONE;
    } else {
      bool reject_unauthorized = args[1]->IsTrue();
      verify_mode = SSL_VERIFY_PEER;
      if (reject_unauthorized)
        verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    }
  } else {
    // Note request_cert and reject_unauthorized are ignored for clients.
    verify_mode = SSL_VERIFY_NONE;
  }

  // Always allow a connection. We'll reject in javascript.
  SSL_set_verify(wrap->ssl_, verify_mode, crypto::VerifyCallback);
}


void TLSCallbacks::EnableSessionCallbacks(
    const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.This());

  wrap->enable_session_callbacks();
  EnableHelloParser(args);
}


void TLSCallbacks::EnableHelloParser(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.This());

  wrap->hello_parser_.Start(SSLWrap<TLSCallbacks>::OnClientHello,
                            OnClientHelloParseEnd,
                            wrap);
}


void TLSCallbacks::OnClientHelloParseEnd(void* arg) {
  TLSCallbacks* c = static_cast<TLSCallbacks*>(arg);
  c->Cycle();
}


#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
void TLSCallbacks::GetServername(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.This());

  const char* servername = SSL_get_servername(wrap->ssl_,
                                              TLSEXT_NAMETYPE_host_name);
  if (servername != NULL) {
    args.GetReturnValue().Set(OneByteString(node_isolate, servername));
  } else {
    args.GetReturnValue().Set(false);
  }
}


void TLSCallbacks::SetServername(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.This());

  if (args.Length() < 1 || !args[0]->IsString())
    return ThrowTypeError("First argument should be a string");

  if (wrap->started_)
    return ThrowError("Already started.");

  if (!wrap->is_client())
    return;

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  String::Utf8Value servername(args[0].As<String>());
  SSL_set_tlsext_host_name(wrap->ssl_, *servername);
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
}


int TLSCallbacks::SelectSNIContextCallback(SSL* s, int* ad, void* arg) {
  HandleScope scope(node_isolate);

  TLSCallbacks* p = static_cast<TLSCallbacks*>(arg);
  Environment* env = p->env();

  const char* servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);

  if (servername != NULL) {
    // Call the SNI callback and use its return value as context
    Local<Object> object = p->object();
    Local<Value> ctx = object->Get(env->sni_context_string());

    if (!ctx->IsObject())
      return SSL_TLSEXT_ERR_NOACK;

    p->sni_context_.Dispose();
    p->sni_context_.Reset(node_isolate, ctx);

    SecureContext* sc = Unwrap<SecureContext>(ctx.As<Object>());
    SSL_set_SSL_CTX(s, sc->ctx_);
  }

  return SSL_TLSEXT_ERR_OK;
}
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB


void TLSCallbacks::Initialize(Handle<Object> target,
                              Handle<Value> unused,
                              Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  NODE_SET_METHOD(target, "wrap", TLSCallbacks::Wrap);

  Local<FunctionTemplate> t = FunctionTemplate::New();
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "TLSWrap"));

  NODE_SET_PROTOTYPE_METHOD(t, "start", Start);
  NODE_SET_PROTOTYPE_METHOD(t, "setVerifyMode", SetVerifyMode);
  NODE_SET_PROTOTYPE_METHOD(t,
                            "enableSessionCallbacks",
                            EnableSessionCallbacks);
  NODE_SET_PROTOTYPE_METHOD(t,
                            "enableHelloParser",
                            EnableHelloParser);

  SSLWrap<TLSCallbacks>::AddMethods(t);

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  NODE_SET_PROTOTYPE_METHOD(t, "getServername", GetServername);
  NODE_SET_PROTOTYPE_METHOD(t, "setServername", SetServername);
#endif  // SSL_CRT_SET_TLSEXT_SERVERNAME_CB

  env->set_tls_wrap_constructor_function(t->GetFunction());
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE(node_tls_wrap, node::TLSCallbacks::Initialize)
