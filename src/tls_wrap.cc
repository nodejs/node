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
using v8::EscapableHandleScope;
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

size_t TLSCallbacks::error_off_;
char TLSCallbacks::error_buf_[1024];


TLSCallbacks::TLSCallbacks(Environment* env,
                           Kind kind,
                           Handle<Object> sc,
                           StreamWrapCallbacks* old)
    : SSLWrap<TLSCallbacks>(env, Unwrap<SecureContext>(sc), kind),
      StreamWrapCallbacks(old),
      AsyncWrap(env,
                env->tls_wrap_constructor_function()->NewInstance(),
                AsyncWrap::PROVIDER_TLSWRAP),
      sc_(Unwrap<SecureContext>(sc)),
      sc_handle_(env->isolate(), sc),
      enc_in_(NULL),
      enc_out_(NULL),
      clear_in_(NULL),
      write_size_(0),
      started_(false),
      established_(false),
      shutdown_(false),
      error_(NULL),
      cycle_depth_(0),
      eof_(false) {
  node::Wrap(object(), this);
  MakeWeak(this);

  // Initialize queue for clearIn writes
  QUEUE_INIT(&write_item_queue_);
  QUEUE_INIT(&pending_write_items_);

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
  sc_handle_.Reset();
  persistent().Reset();

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  sni_context_.Reset();
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB

  // Move all writes to pending
  MakePending();

  // And destroy
  while (!QUEUE_EMPTY(&pending_write_items_)) {
    QUEUE* q = QUEUE_HEAD(&pending_write_items_);
    QUEUE_REMOVE(q);

    WriteItem* wi = ContainerOf(&WriteItem::member_, q);
    delete wi;
  }
}


void TLSCallbacks::MakePending() {
  // Aliases
  QUEUE* from = &write_item_queue_;
  QUEUE* to = &pending_write_items_;

  if (QUEUE_EMPTY(from))
    return;

  // Add items to pending
  QUEUE_ADD(to, from);

  // Empty original queue
  QUEUE_INIT(from);
}


bool TLSCallbacks::InvokeQueued(int status) {
  if (QUEUE_EMPTY(&pending_write_items_))
    return false;

  // Process old queue
  QUEUE queue;
  QUEUE* q = QUEUE_HEAD(&pending_write_items_);
  QUEUE_SPLIT(&pending_write_items_, q, &queue);
  while (QUEUE_EMPTY(&queue) == false) {
    q = QUEUE_HEAD(&queue);
    QUEUE_REMOVE(q);

    WriteItem* wi = ContainerOf(&WriteItem::member_, q);
    wi->cb_(&wi->w_->req_, status);
    delete wi;
  }

  return true;
}


void TLSCallbacks::NewSessionDoneCb() {
  Cycle();
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

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  if (is_server()) {
    SSL_CTX_set_tlsext_servername_callback(sc_->ctx_, SelectSNIContextCallback);
  }
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB

  InitNPN(sc_);

  if (is_server()) {
    SSL_set_accept_state(ssl_);
  } else if (is_client()) {
    // Enough space for server response (hello, cert)
    NodeBIO::FromBIO(enc_in_)->set_initial(kInitialClientBufferLength);
    SSL_set_connect_state(ssl_);
  } else {
    // Unexpected
    abort();
  }

  // Initialize ring for queud clear data
  clear_in_ = new NodeBIO();
}


void TLSCallbacks::Wrap(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  if (args.Length() < 1 || !args[0]->IsObject()) {
    return env->ThrowTypeError(
        "First argument should be a StreamWrap instance");
  }
  if (args.Length() < 2 || !args[1]->IsObject()) {
    return env->ThrowTypeError(
        "Second argument should be a SecureContext instance");
  }
  if (args.Length() < 3 || !args[2]->IsBoolean())
    return env->ThrowTypeError("Third argument should be boolean");

  Local<Object> stream = args[0].As<Object>();
  Local<Object> sc = args[1].As<Object>();
  Kind kind = args[2]->IsTrue() ? SSLWrap<TLSCallbacks>::kServer :
                                  SSLWrap<TLSCallbacks>::kClient;

  TLSCallbacks* callbacks = NULL;
  WITH_GENERIC_STREAM(env, stream, {
    callbacks = new TLSCallbacks(env, kind, sc, wrap->callbacks());
    wrap->OverrideCallbacks(callbacks, true);
  });

  if (callbacks == NULL) {
    return args.GetReturnValue().SetNull();
  }

  args.GetReturnValue().Set(callbacks->persistent());
}


void TLSCallbacks::Receive(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.Holder());

  CHECK(Buffer::HasInstance(args[0]));
  char* data = Buffer::Data(args[0]);
  size_t len = Buffer::Length(args[0]);

  uv_buf_t buf;
  uv_stream_t* stream = wrap->wrap()->stream();

  // Copy given buffer entirely or partiall if handle becomes closed
  while (len > 0 && !uv_is_closing(reinterpret_cast<uv_handle_t*>(stream))) {
    wrap->DoAlloc(reinterpret_cast<uv_handle_t*>(stream), len, &buf);
    size_t copy = buf.len > len ? len : buf.len;
    memcpy(buf.base, data, copy);
    buf.len = copy;
    wrap->DoRead(stream, buf.len, &buf, UV_UNKNOWN_HANDLE);

    data += copy;
    len -= copy;
  }
}


void TLSCallbacks::Start(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.Holder());

  if (wrap->started_)
    return env->ThrowError("Already started.");
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
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
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

  // Wait for `newSession` callback to be invoked
  if (is_waiting_new_session())
    return;

  // Split-off queue
  if (established_ && !QUEUE_EMPTY(&write_item_queue_))
    MakePending();

  // No data to write
  if (BIO_pending(enc_out_) == 0) {
    if (clear_in_->Length() == 0)
      InvokeQueued(0);
    return;
  }

  char* data[kSimultaneousBufferCount];
  size_t size[ARRAY_SIZE(data)];
  size_t count = ARRAY_SIZE(data);
  write_size_ = NodeBIO::FromBIO(enc_out_)->PeekMultiple(data, size, &count);
  assert(write_size_ != 0 && count != 0);

  write_req_.data = this;
  uv_buf_t buf[ARRAY_SIZE(data)];
  for (size_t i = 0; i < count; i++)
    buf[i] = uv_buf_init(data[i], size[i]);
  int r = uv_write(&write_req_, wrap()->stream(), buf, count, EncOutCb);

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

  // Handle error
  if (status) {
    // Ignore errors after shutdown
    if (callbacks->shutdown_)
      return;

    // Notify about error
    callbacks->InvokeQueued(status);
    return;
  }

  // Commit
  NodeBIO::FromBIO(callbacks->enc_out_)->Read(NULL, callbacks->write_size_);

  // Try writing more data
  callbacks->write_size_ = 0;
  callbacks->EncOut();
}


int TLSCallbacks::PrintErrorsCb(const char* str, size_t len, void* arg) {
  size_t to_copy = error_off_;
  size_t avail = sizeof(error_buf_) - error_off_ - 1;

  if (avail > to_copy)
    to_copy = avail;

  memcpy(error_buf_, str, avail);
  error_off_ += avail;
  assert(error_off_ < sizeof(error_buf_));

  // Zero-terminate
  error_buf_[error_off_] = '\0';

  return 0;
}


const char* TLSCallbacks::PrintErrors() {
  error_off_ = 0;
  ERR_print_errors_cb(PrintErrorsCb, this);

  return error_buf_;
}


Local<Value> TLSCallbacks::GetSSLError(int status, int* err, const char** msg) {
  EscapableHandleScope scope(env()->isolate());

  *err = SSL_get_error(ssl_, status);
  switch (*err) {
    case SSL_ERROR_NONE:
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      break;
    case SSL_ERROR_ZERO_RETURN:
      return scope.Escape(env()->zero_return_string());
      break;
    default:
      {
        assert(*err == SSL_ERROR_SSL || *err == SSL_ERROR_SYSCALL);

        const char* buf = PrintErrors();

        Local<String> message =
            OneByteString(env()->isolate(), buf, strlen(buf));
        Local<Value> exception = Exception::Error(message);

        if (msg != NULL) {
          assert(*msg == NULL);
          *msg = buf;
        }

        return scope.Escape(exception);
      }
  }
  return Local<Value>();
}


void TLSCallbacks::ClearOut() {
  // Ignore cycling data if ClientHello wasn't yet parsed
  if (!hello_parser_.IsEnded())
    return;

  // No reads after EOF
  if (eof_)
    return;

  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  assert(ssl_ != NULL);

  char out[kClearOutChunkSize];
  int read;
  do {
    read = SSL_read(ssl_, out, sizeof(out));
    if (read > 0) {
      Local<Value> argv[] = {
        Integer::New(env()->isolate(), read),
        Buffer::New(env(), out, read)
      };
      wrap()->MakeCallback(env()->onread_string(), ARRAY_SIZE(argv), argv);
    }
  } while (read > 0);

  int flags = SSL_get_shutdown(ssl_);
  if (!eof_ && flags & SSL_RECEIVED_SHUTDOWN) {
    eof_ = true;
    Local<Value> arg = Integer::New(env()->isolate(), UV_EOF);
    wrap()->MakeCallback(env()->onread_string(), 1, &arg);
  }

  if (read == -1) {
    int err;
    Local<Value> arg = GetSSLError(read, &err, NULL);

    // Ignore ZERO_RETURN after EOF, it is basically not a error
    if (err == SSL_ERROR_ZERO_RETURN && eof_)
      return;

    if (!arg.IsEmpty()) {
      // When TLS Alert are stored in wbio,
      // it should be flushed to socket before destroyed.
      if (BIO_pending(enc_out_) != 0)
        EncOut();

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

  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  // Error or partial write
  int err;
  Local<Value> arg = GetSSLError(written, &err, &error_);
  if (!arg.IsEmpty()) {
    MakePending();
    if (!InvokeQueued(UV_EPROTO))
      error_ = NULL;
    clear_in_->Reset();
  }

  return false;
}


const char* TLSCallbacks::Error() {
  const char* ret = error_;
  error_ = NULL;
  return ret;
}


int TLSCallbacks::TryWrite(uv_buf_t** bufs, size_t* count) {
  // TODO(indutny): Support it
  return 0;
}


int TLSCallbacks::DoWrite(WriteWrap* w,
                          uv_buf_t* bufs,
                          size_t count,
                          uv_stream_t* send_handle,
                          uv_write_cb cb) {
  assert(send_handle == NULL);

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

  // Queue callback to execute it on next tick
  WriteItem* wi = new WriteItem(w, cb);
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
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());
    Local<Value> arg = GetSSLError(written, &err, &error_);
    if (!arg.IsEmpty())
      return UV_EPROTO;

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
  size_t size = 0;
  buf->base = NodeBIO::FromBIO(enc_in_)->PeekWritable(&size);
  buf->len = size;
}


void TLSCallbacks::DoRead(uv_stream_t* handle,
                          ssize_t nread,
                          const uv_buf_t* buf,
                          uv_handle_type pending) {
  if (nread < 0)  {
    // Error should be emitted only after all data was read
    ClearOut();

    // Ignore EOF if received close_notify
    if (nread == UV_EOF) {
      if (eof_)
        return;
      eof_ = true;
    }

    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());
    Local<Value> arg = Integer::New(env()->isolate(), nread);
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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.Holder());

  if (args.Length() < 2 || !args[0]->IsBoolean() || !args[1]->IsBoolean())
    return env->ThrowTypeError("Bad arguments, expected two booleans");

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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.Holder());

  wrap->enable_session_callbacks();
  EnableHelloParser(args);
}


void TLSCallbacks::EnableHelloParser(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.Holder());

  NodeBIO::FromBIO(wrap->enc_in_)->set_initial(kMaxHelloLength);
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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.Holder());

  const char* servername = SSL_get_servername(wrap->ssl_,
                                              TLSEXT_NAMETYPE_host_name);
  if (servername != NULL) {
    args.GetReturnValue().Set(OneByteString(env->isolate(), servername));
  } else {
    args.GetReturnValue().Set(false);
  }
}


void TLSCallbacks::SetServername(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  TLSCallbacks* wrap = Unwrap<TLSCallbacks>(args.Holder());

  if (args.Length() < 1 || !args[0]->IsString())
    return env->ThrowTypeError("First argument should be a string");

  if (wrap->started_)
    return env->ThrowError("Already started.");

  if (!wrap->is_client())
    return;

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  node::Utf8Value servername(args[0].As<String>());
  SSL_set_tlsext_host_name(wrap->ssl_, *servername);
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
}


int TLSCallbacks::SelectSNIContextCallback(SSL* s, int* ad, void* arg) {
  TLSCallbacks* p = static_cast<TLSCallbacks*>(SSL_get_app_data(s));
  Environment* env = p->env();

  const char* servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);

  if (servername == NULL)
    return SSL_TLSEXT_ERR_OK;

  HandleScope scope(env->isolate());
  // Call the SNI callback and use its return value as context
  Local<Object> object = p->object();
  Local<Value> ctx = object->Get(env->sni_context_string());

  // Not an object, probably undefined or null
  if (!ctx->IsObject())
    return SSL_TLSEXT_ERR_NOACK;

  Local<FunctionTemplate> cons = env->secure_context_constructor_template();
  if (!cons->HasInstance(ctx)) {
    // Failure: incorrect SNI context object
    Local<Value> err = Exception::TypeError(env->sni_context_err_string());
    p->MakeCallback(env->onerror_string(), 1, &err);
    return SSL_TLSEXT_ERR_NOACK;
  }

  p->sni_context_.Reset();
  p->sni_context_.Reset(env->isolate(), ctx);

  SecureContext* sc = Unwrap<SecureContext>(ctx.As<Object>());
  InitNPN(sc);
  SSL_set_SSL_CTX(s, sc->ctx_);
  return SSL_TLSEXT_ERR_OK;
}
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB


void TLSCallbacks::Initialize(Handle<Object> target,
                              Handle<Value> unused,
                              Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  NODE_SET_METHOD(target, "wrap", TLSCallbacks::Wrap);

  Local<FunctionTemplate> t = FunctionTemplate::New(env->isolate());
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "TLSWrap"));

  NODE_SET_PROTOTYPE_METHOD(t, "receive", Receive);
  NODE_SET_PROTOTYPE_METHOD(t, "start", Start);
  NODE_SET_PROTOTYPE_METHOD(t, "setVerifyMode", SetVerifyMode);
  NODE_SET_PROTOTYPE_METHOD(t,
                            "enableSessionCallbacks",
                            EnableSessionCallbacks);
  NODE_SET_PROTOTYPE_METHOD(t,
                            "enableHelloParser",
                            EnableHelloParser);

  SSLWrap<TLSCallbacks>::AddMethods(env, t);

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  NODE_SET_PROTOTYPE_METHOD(t, "getServername", GetServername);
  NODE_SET_PROTOTYPE_METHOD(t, "setServername", SetServername);
#endif  // SSL_CRT_SET_TLSEXT_SERVERNAME_CB

  env->set_tls_wrap_constructor_function(t->GetFunction());
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(tls_wrap, node::TLSCallbacks::Initialize)
