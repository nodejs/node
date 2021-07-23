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

#include "crypto/crypto_tls.h"
#include "crypto/crypto_context.h"
#include "crypto/crypto_common.h"
#include "crypto/crypto_util.h"
#include "crypto/crypto_bio.h"
#include "crypto/crypto_clienthello-inl.h"
#include "allocated_buffer-inl.h"
#include "async_wrap-inl.h"
#include "debug_utils-inl.h"
#include "memory_tracker-inl.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "stream_base-inl.h"
#include "util-inl.h"

namespace node {

using v8::Array;
using v8::ArrayBufferView;
using v8::Context;
using v8::DontDelete;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::False;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Null;
using v8::Object;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::Signature;
using v8::String;
using v8::True;
using v8::Uint32;
using v8::Value;

namespace crypto {

namespace {
SSL_SESSION* GetSessionCallback(
    SSL* s,
    const unsigned char* key,
    int len,
    int* copy) {
  TLSWrap* w = static_cast<TLSWrap*>(SSL_get_app_data(s));
  *copy = 0;
  return w->ReleaseSession();
}

void OnClientHello(
    void* arg,
    const ClientHelloParser::ClientHello& hello) {
  TLSWrap* w = static_cast<TLSWrap*>(arg);
  Environment* env = w->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Object> hello_obj = Object::New(env->isolate());
  Local<String> servername = (hello.servername() == nullptr)
      ? String::Empty(env->isolate())
      : OneByteString(env->isolate(),
                      hello.servername(),
                      hello.servername_size());
  Local<Object> buf =
      Buffer::Copy(
          env,
          reinterpret_cast<const char*>(hello.session_id()),
          hello.session_size()).FromMaybe(Local<Object>());

  if ((buf.IsEmpty() ||
       hello_obj->Set(env->context(), env->session_id_string(), buf)
          .IsNothing()) ||
      hello_obj->Set(env->context(), env->servername_string(), servername)
          .IsNothing() ||
      hello_obj->Set(
          env->context(),
          env->tls_ticket_string(),
          hello.has_ticket()
              ? True(env->isolate())
              : False(env->isolate())).IsNothing()) {
    return;
  }

  Local<Value> argv[] = { hello_obj };
  w->MakeCallback(env->onclienthello_string(), arraysize(argv), argv);
}

void KeylogCallback(const SSL* s, const char* line) {
  TLSWrap* w = static_cast<TLSWrap*>(SSL_get_app_data(s));
  Environment* env = w->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  const size_t size = strlen(line);
  Local<Value> line_bf = Buffer::Copy(env, line, 1 + size)
      .FromMaybe(Local<Value>());
  if (UNLIKELY(line_bf.IsEmpty()))
    return;

  char* data = Buffer::Data(line_bf);
  data[size] = '\n';
  w->MakeCallback(env->onkeylog_string(), 1, &line_bf);
}

int NewSessionCallback(SSL* s, SSL_SESSION* sess) {
  TLSWrap* w = static_cast<TLSWrap*>(SSL_get_app_data(s));
  Environment* env = w->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  if (!w->has_session_callbacks())
    return 0;

  // Check if session is small enough to be stored
  int size = i2d_SSL_SESSION(sess, nullptr);
  if (UNLIKELY(size > SecureContext::kMaxSessionSize))
    return 0;

  // Serialize session
  // TODO(@jasnell): An AllocatedBuffer or BackingStore would be better
  // here to start eliminating unnecessary uses of Buffer where an ordinary
  // Uint8Array would do just fine.
  Local<Object> session = Buffer::New(env, size).FromMaybe(Local<Object>());
  if (UNLIKELY(session.IsEmpty()))
    return 0;

  unsigned char* session_data =
      reinterpret_cast<unsigned char*>(Buffer::Data(session));

  memset(session_data, 0, size);
  i2d_SSL_SESSION(sess, &session_data);

  unsigned int session_id_length;
  const unsigned char* session_id_data =
      SSL_SESSION_get_id(sess, &session_id_length);

  // TODO(@jasnell): An AllocatedBuffer or BackingStore would be better
  // here to start eliminating unnecessary uses of Buffer where an ordinary
  // Uint8Array would do just fine
  Local<Object> session_id = Buffer::Copy(
      env,
      reinterpret_cast<const char*>(session_id_data),
      session_id_length).FromMaybe(Local<Object>());
  if (UNLIKELY(session_id.IsEmpty()))
    return 0;

  Local<Value> argv[] = {
    session_id,
    session
  };

  // On servers, we pause the handshake until callback of 'newSession', which
  // calls NewSessionDoneCb(). On clients, there is no callback to wait for.
  if (w->is_server())
    w->set_awaiting_new_session(true);

  w->MakeCallback(env->onnewsession_string(), arraysize(argv), argv);

  return 0;
}

int SSLCertCallback(SSL* s, void* arg) {
  TLSWrap* w = static_cast<TLSWrap*>(SSL_get_app_data(s));

  if (!w->is_server() || !w->is_waiting_cert_cb())
    return 1;

  if (w->is_cert_cb_running())
    // Not an error. Suspend handshake with SSL_ERROR_WANT_X509_LOOKUP, and
    // handshake will continue after certcb is done.
    return -1;

  Environment* env = w->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  w->set_cert_cb_running();

  Local<Object> info = Object::New(env->isolate());

  const char* servername = GetServerName(s);
  Local<String> servername_str = (servername == nullptr)
      ? String::Empty(env->isolate())
      : OneByteString(env->isolate(), servername, strlen(servername));

  Local<Value> ocsp = (SSL_get_tlsext_status_type(s) == TLSEXT_STATUSTYPE_ocsp)
      ? True(env->isolate())
      : False(env->isolate());

  if (info->Set(env->context(), env->servername_string(), servername_str)
          .IsNothing() ||
      info->Set(env->context(), env->ocsp_request_string(), ocsp).IsNothing()) {
    return 1;
  }

  Local<Value> argv[] = { info };
  w->MakeCallback(env->oncertcb_string(), arraysize(argv), argv);

  return w->is_cert_cb_running() ? -1 : 1;
}

int SelectALPNCallback(
    SSL* s,
    const unsigned char** out,
    unsigned char* outlen,
    const unsigned char* in,
    unsigned int inlen,
    void* arg) {
  TLSWrap* w = static_cast<TLSWrap*>(SSL_get_app_data(s));
  Environment* env = w->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> alpn_buffer =
      w->object()->GetPrivate(
          env->context(),
          env->alpn_buffer_private_symbol()).FromMaybe(Local<Value>());
  if (UNLIKELY(alpn_buffer.IsEmpty()) || !alpn_buffer->IsArrayBufferView())
    return SSL_TLSEXT_ERR_NOACK;

  ArrayBufferViewContents<unsigned char> alpn_protos(alpn_buffer);
  int status = SSL_select_next_proto(
      const_cast<unsigned char**>(out),
      outlen,
      alpn_protos.data(),
      alpn_protos.length(),
      in,
      inlen);

  // According to 3.2. Protocol Selection of RFC7301, fatal
  // no_application_protocol alert shall be sent but OpenSSL 1.0.2 does not
  // support it yet. See
  // https://rt.openssl.org/Ticket/Display.html?id=3463&user=guest&pass=guest
  return status == OPENSSL_NPN_NEGOTIATED
      ? SSL_TLSEXT_ERR_OK
      : SSL_TLSEXT_ERR_NOACK;
}

int TLSExtStatusCallback(SSL* s, void* arg) {
  TLSWrap* w = static_cast<TLSWrap*>(SSL_get_app_data(s));
  Environment* env = w->env();
  HandleScope handle_scope(env->isolate());

  if (w->is_client()) {
    // Incoming response
    Local<Value> arg;
    if (GetSSLOCSPResponse(env, s, Null(env->isolate())).ToLocal(&arg))
      w->MakeCallback(env->onocspresponse_string(), 1, &arg);

    // No async acceptance is possible, so always return 1 to accept the
    // response.  The listener for 'OCSPResponse' event has no control over
    // return value, but it can .destroy() the connection if the response is not
    // acceptable.
    return 1;
  }

  // Outgoing response
  Local<ArrayBufferView> obj =
      w->ocsp_response().FromMaybe(Local<ArrayBufferView>());
  if (UNLIKELY(obj.IsEmpty()))
    return SSL_TLSEXT_ERR_NOACK;

  size_t len = obj->ByteLength();

  // OpenSSL takes control of the pointer after accepting it
  unsigned char* data = MallocOpenSSL<unsigned char>(len);
  obj->CopyContents(data, len);

  if (!SSL_set_tlsext_status_ocsp_resp(s, data, len))
    OPENSSL_free(data);

  w->ClearOcspResponse();

  return SSL_TLSEXT_ERR_OK;
}

void ConfigureSecureContext(SecureContext* sc) {
  // OCSP stapling
  SSL_CTX_set_tlsext_status_cb(sc->ctx_.get(), TLSExtStatusCallback);
  SSL_CTX_set_tlsext_status_arg(sc->ctx_.get(), nullptr);
}

inline bool Set(
    Environment* env,
    Local<Object> target,
    Local<String> name,
    const char* value,
    bool ignore_null = true) {
  if (value == nullptr)
    return ignore_null;
  return !target->Set(
      env->context(),
      name,
      OneByteString(env->isolate(), value))
          .IsNothing();
}
}  // namespace

TLSWrap::TLSWrap(Environment* env,
                 Local<Object> obj,
                 Kind kind,
                 StreamBase* stream,
                 SecureContext* sc)
    : AsyncWrap(env, obj, AsyncWrap::PROVIDER_TLSWRAP),
      StreamBase(env),
      env_(env),
      kind_(kind),
      sc_(sc) {
  MakeWeak();
  CHECK(sc_);
  ssl_ = sc_->CreateSSL();
  CHECK(ssl_);

  sc_->SetGetSessionCallback(GetSessionCallback);
  sc_->SetNewSessionCallback(NewSessionCallback);

  StreamBase::AttachToObject(GetObject());
  stream->PushStreamListener(this);

  env_->isolate()->AdjustAmountOfExternalAllocatedMemory(kExternalSize);

  InitSSL();
  Debug(this, "Created new TLSWrap");
}

TLSWrap::~TLSWrap() {
  Destroy();
}

MaybeLocal<ArrayBufferView> TLSWrap::ocsp_response() const {
  if (ocsp_response_.IsEmpty())
    return MaybeLocal<ArrayBufferView>();
  return PersistentToLocal::Default(env()->isolate(), ocsp_response_);
}

void TLSWrap::ClearOcspResponse() {
  ocsp_response_.Reset();
}

SSL_SESSION* TLSWrap::ReleaseSession() {
  return next_sess_.release();
}

void TLSWrap::InvokeQueued(int status, const char* error_str) {
  Debug(this, "Invoking queued write callbacks (%d, %s)", status, error_str);
  if (!write_callback_scheduled_)
    return;

  if (current_write_) {
    BaseObjectPtr<AsyncWrap> current_write = std::move(current_write_);
    current_write_.reset();
    WriteWrap* w = WriteWrap::FromObject(current_write);
    w->Done(status, error_str);
  }
}

void TLSWrap::NewSessionDoneCb() {
  Debug(this, "New session callback done");
  Cycle();
}

void TLSWrap::InitSSL() {
  // Initialize SSL – OpenSSL takes ownership of these.
  enc_in_ = NodeBIO::New(env()).release();
  enc_out_ = NodeBIO::New(env()).release();

  SSL_set_bio(ssl_.get(), enc_in_, enc_out_);

  // NOTE: This could be overridden in SetVerifyMode
  SSL_set_verify(ssl_.get(), SSL_VERIFY_NONE, VerifyCallback);

#ifdef SSL_MODE_RELEASE_BUFFERS
  SSL_set_mode(ssl_.get(), SSL_MODE_RELEASE_BUFFERS);
#endif  // SSL_MODE_RELEASE_BUFFERS

  // This is default in 1.1.1, but set it anyway, Cycle() doesn't currently
  // re-call ClearIn() if SSL_read() returns SSL_ERROR_WANT_READ, so data can be
  // left sitting in the incoming enc_in_ and never get processed.
  // - https://wiki.openssl.org/index.php/TLS1.3#Non-application_data_records
  SSL_set_mode(ssl_.get(), SSL_MODE_AUTO_RETRY);

#ifdef OPENSSL_IS_BORINGSSL
  // OpenSSL allows renegotiation by default, but BoringSSL disables it.
  // Configure BoringSSL to match OpenSSL's behavior.
  SSL_set_renegotiate_mode(ssl_.get(), ssl_renegotiate_freely);
#endif

  SSL_set_app_data(ssl_.get(), this);
  // Using InfoCallback isn't how we are supposed to check handshake progress:
  //   https://github.com/openssl/openssl/issues/7199#issuecomment-420915993
  //
  // Note on when this gets called on various openssl versions:
  //   https://github.com/openssl/openssl/issues/7199#issuecomment-420670544
  SSL_set_info_callback(ssl_.get(), SSLInfoCallback);

  if (is_server())
    sc_->SetSelectSNIContextCallback(SelectSNIContextCallback);

  ConfigureSecureContext(sc_.get());

  SSL_set_cert_cb(ssl_.get(), SSLCertCallback, this);

  if (is_server()) {
    SSL_set_accept_state(ssl_.get());
  } else if (is_client()) {
    // Enough space for server response (hello, cert)
    NodeBIO::FromBIO(enc_in_)->set_initial(kInitialClientBufferLength);
    SSL_set_connect_state(ssl_.get());
  } else {
    // Unexpected
    ABORT();
  }
}

void TLSWrap::Wrap(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_EQ(args.Length(), 3);
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsObject());
  CHECK(args[2]->IsBoolean());

  Local<Object> sc = args[1].As<Object>();
  Kind kind = args[2]->IsTrue() ? Kind::kServer : Kind::kClient;

  StreamBase* stream = StreamBase::FromObject(args[0].As<Object>());
  CHECK_NOT_NULL(stream);

  Local<Object> obj;
  if (!env->tls_wrap_constructor_function()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return;
  }

  TLSWrap* res = new TLSWrap(env, obj, kind, stream, Unwrap<SecureContext>(sc));

  args.GetReturnValue().Set(res->object());
}

void TLSWrap::Receive(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  ArrayBufferViewContents<char> buffer(args[0]);
  const char* data = buffer.data();
  size_t len = buffer.length();
  Debug(wrap, "Receiving %zu bytes injected from JS", len);

  // Copy given buffer entirely or partiall if handle becomes closed
  while (len > 0 && wrap->IsAlive() && !wrap->IsClosing()) {
    uv_buf_t buf = wrap->OnStreamAlloc(len);
    size_t copy = buf.len > len ? len : buf.len;
    memcpy(buf.base, data, copy);
    buf.len = copy;
    wrap->OnStreamRead(copy, buf);

    data += copy;
    len -= copy;
  }
}

void TLSWrap::Start(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK(!wrap->started_);
  wrap->started_ = true;

  // Send ClientHello handshake
  CHECK(wrap->is_client());
  // Seems odd to read when when we want to send, but SSL_read() triggers a
  // handshake if a session isn't established, and handshake will cause
  // encrypted data to become available for output.
  wrap->ClearOut();
  wrap->EncOut();
}

void TLSWrap::SSLInfoCallback(const SSL* ssl_, int where, int ret) {
  if (!(where & (SSL_CB_HANDSHAKE_START | SSL_CB_HANDSHAKE_DONE)))
    return;

  // SSL_renegotiate_pending() should take `const SSL*`, but it does not.
  SSL* ssl = const_cast<SSL*>(ssl_);
  TLSWrap* c = static_cast<TLSWrap*>(SSL_get_app_data(ssl_));
  Environment* env = c->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Object> object = c->object();

  if (where & SSL_CB_HANDSHAKE_START) {
    Debug(c, "SSLInfoCallback(SSL_CB_HANDSHAKE_START);");
    // Start is tracked to limit number and frequency of renegotiation attempts,
    // since excessive renegotiation may be an attack.
    Local<Value> callback;

    if (object->Get(env->context(), env->onhandshakestart_string())
            .ToLocal(&callback) && callback->IsFunction()) {
      Local<Value> argv[] = { env->GetNow() };
      c->MakeCallback(callback.As<Function>(), arraysize(argv), argv);
    }
  }

  // SSL_CB_HANDSHAKE_START and SSL_CB_HANDSHAKE_DONE are called
  // sending HelloRequest in OpenSSL-1.1.1.
  // We need to check whether this is in a renegotiation state or not.
  if (where & SSL_CB_HANDSHAKE_DONE && !SSL_renegotiate_pending(ssl)) {
    Debug(c, "SSLInfoCallback(SSL_CB_HANDSHAKE_DONE);");
    CHECK(!SSL_renegotiate_pending(ssl));
    Local<Value> callback;

    c->established_ = true;

    if (object->Get(env->context(), env->onhandshakedone_string())
          .ToLocal(&callback) && callback->IsFunction()) {
      c->MakeCallback(callback.As<Function>(), 0, nullptr);
    }
  }
}

void TLSWrap::EncOut() {
  Debug(this, "Trying to write encrypted output");

  // Ignore cycling data if ClientHello wasn't yet parsed
  if (!hello_parser_.IsEnded()) {
    Debug(this, "Returning from EncOut(), hello_parser_ active");
    return;
  }

  // Write in progress
  if (write_size_ != 0) {
    Debug(this, "Returning from EncOut(), write currently in progress");
    return;
  }

  // Wait for `newSession` callback to be invoked
  if (is_awaiting_new_session()) {
    Debug(this, "Returning from EncOut(), awaiting new session");
    return;
  }

  // Split-off queue
  if (established_ && current_write_) {
    Debug(this, "EncOut() write is scheduled");
    write_callback_scheduled_ = true;
  }

  if (ssl_ == nullptr) {
    Debug(this, "Returning from EncOut(), ssl_ == nullptr");
    return;
  }

  // No encrypted output ready to write to the underlying stream.
  if (BIO_pending(enc_out_) == 0) {
    Debug(this, "No pending encrypted output");
    if (pending_cleartext_input_.size() == 0) {
      if (!in_dowrite_) {
        Debug(this, "No pending cleartext input, not inside DoWrite()");
        InvokeQueued(0);
      } else {
        Debug(this, "No pending cleartext input, inside DoWrite()");
        // TODO(@sam-github, @addaleax) If in_dowrite_ is true, appdata was
        // passed to SSL_write().  If we are here, the data was not encrypted to
        // enc_out_ yet.  Calling Done() "works", but since the write is not
        // flushed, its too soon.  Just returning and letting the next EncOut()
        // call Done() passes the test suite, but without more careful analysis,
        // its not clear if it is always correct. Not calling Done() could block
        // data flow, so for now continue to call Done(), just do it in the next
        // tick.
        BaseObjectPtr<TLSWrap> strong_ref{this};
        env()->SetImmediate([this, strong_ref](Environment* env) {
          InvokeQueued(0);
        });
      }
    }
    return;
  }

  char* data[kSimultaneousBufferCount];
  size_t size[arraysize(data)];
  size_t count = arraysize(data);
  write_size_ = NodeBIO::FromBIO(enc_out_)->PeekMultiple(data, size, &count);
  CHECK(write_size_ != 0 && count != 0);

  uv_buf_t buf[arraysize(data)];
  uv_buf_t* bufs = buf;
  for (size_t i = 0; i < count; i++)
    buf[i] = uv_buf_init(data[i], size[i]);

  Debug(this, "Writing %zu buffers to the underlying stream", count);
  StreamWriteResult res = underlying_stream()->Write(bufs, count);
  if (res.err != 0) {
    InvokeQueued(res.err);
    return;
  }

  if (!res.async) {
    Debug(this, "Write finished synchronously");
    HandleScope handle_scope(env()->isolate());

    // Simulate asynchronous finishing, TLS cannot handle this at the moment.
    BaseObjectPtr<TLSWrap> strong_ref{this};
    env()->SetImmediate([this, strong_ref](Environment* env) {
      OnStreamAfterWrite(nullptr, 0);
    });
  }
}

void TLSWrap::OnStreamAfterWrite(WriteWrap* req_wrap, int status) {
  Debug(this, "OnStreamAfterWrite(status = %d)", status);
  if (current_empty_write_) {
    Debug(this, "Had empty write");
    BaseObjectPtr<AsyncWrap> current_empty_write =
        std::move(current_empty_write_);
    current_empty_write_.reset();
    WriteWrap* finishing = WriteWrap::FromObject(current_empty_write);
    finishing->Done(status);
    return;
  }

  if (ssl_ == nullptr) {
    Debug(this, "ssl_ == nullptr, marking as cancelled");
    status = UV_ECANCELED;
  }

  // Handle error
  if (status) {
    if (shutdown_) {
      Debug(this, "Ignoring error after shutdown");
      return;
    }

    // Notify about error
    InvokeQueued(status);
    return;
  }

  // Commit
  NodeBIO::FromBIO(enc_out_)->Read(nullptr, write_size_);

  // Ensure that the progress will be made and `InvokeQueued` will be called.
  ClearIn();

  // Try writing more data
  write_size_ = 0;
  EncOut();
}

MaybeLocal<Value> TLSWrap::GetSSLError(int status, int* err, std::string* msg) {
  EscapableHandleScope scope(env()->isolate());

  // ssl_ is already destroyed in reading EOF by close notify alert.
  if (ssl_ == nullptr)
    return MaybeLocal<Value>();

  *err = SSL_get_error(ssl_.get(), status);
  switch (*err) {
    case SSL_ERROR_NONE:
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_WANT_X509_LOOKUP:
      return MaybeLocal<Value>();

    case SSL_ERROR_ZERO_RETURN:
      return scope.Escape(env()->zero_return_string());

    case SSL_ERROR_SSL:
    case SSL_ERROR_SYSCALL:
      {
        unsigned long ssl_err = ERR_peek_error();  // NOLINT(runtime/int)
        BIO* bio = BIO_new(BIO_s_mem());
        ERR_print_errors(bio);

        BUF_MEM* mem;
        BIO_get_mem_ptr(bio, &mem);

        Isolate* isolate = env()->isolate();
        Local<Context> context = isolate->GetCurrentContext();

        Local<String> message = OneByteString(isolate, mem->data, mem->length);
        Local<Value> exception = Exception::Error(message);
        Local<Object> obj =
            exception->ToObject(context).FromMaybe(Local<Object>());
        if (UNLIKELY(obj.IsEmpty()))
          return MaybeLocal<Value>();

        const char* ls = ERR_lib_error_string(ssl_err);
        const char* fs = ERR_func_error_string(ssl_err);
        const char* rs = ERR_reason_error_string(ssl_err);

        if (!Set(env(), obj, env()->library_string(), ls) ||
            !Set(env(), obj, env()->function_string(), fs)) {
          return MaybeLocal<Value>();
        }

        if (rs != nullptr) {
          if (!Set(env(), obj, env()->reason_string(), rs))
            return MaybeLocal<Value>();

          // SSL has no API to recover the error name from the number, so we
          // transform reason strings like "this error" to "ERR_SSL_THIS_ERROR",
          // which ends up being close to the original error macro name.
          std::string code(rs);

          for (auto& c : code)
            c = (c == ' ') ? '_' : ToUpper(c);

          if (!Set(env(), obj,
                   env()->code_string(),
                   ("ERR_SSL_" + code).c_str())) {
            return MaybeLocal<Value>();
          }
        }

        if (msg != nullptr)
          msg->assign(mem->data, mem->data + mem->length);

        BIO_free_all(bio);

        return scope.Escape(exception);
      }

    default:
      UNREACHABLE();
  }
  UNREACHABLE();
}

void TLSWrap::ClearOut() {
  Debug(this, "Trying to read cleartext output");
  // Ignore cycling data if ClientHello wasn't yet parsed
  if (!hello_parser_.IsEnded()) {
    Debug(this, "Returning from ClearOut(), hello_parser_ active");
    return;
  }

  // No reads after EOF
  if (eof_) {
    Debug(this, "Returning from ClearOut(), EOF reached");
    return;
  }

  if (ssl_ == nullptr) {
    Debug(this, "Returning from ClearOut(), ssl_ == nullptr");
    return;
  }

  MarkPopErrorOnReturn mark_pop_error_on_return;

  char out[kClearOutChunkSize];
  int read;
  for (;;) {
    read = SSL_read(ssl_.get(), out, sizeof(out));
    Debug(this, "Read %d bytes of cleartext output", read);

    if (read <= 0)
      break;

    char* current = out;
    while (read > 0) {
      int avail = read;

      uv_buf_t buf = EmitAlloc(avail);
      if (static_cast<int>(buf.len) < avail)
        avail = buf.len;
      memcpy(buf.base, current, avail);
      EmitRead(avail, buf);

      // Caveat emptor: OnRead() calls into JS land which can result in
      // the SSL context object being destroyed.  We have to carefully
      // check that ssl_ != nullptr afterwards.
      if (ssl_ == nullptr) {
        Debug(this, "Returning from read loop, ssl_ == nullptr");
        return;
      }

      read -= avail;
      current += avail;
    }
  }

  int flags = SSL_get_shutdown(ssl_.get());
  if (!eof_ && flags & SSL_RECEIVED_SHUTDOWN) {
    eof_ = true;
    EmitRead(UV_EOF);
  }

  // We need to check whether an error occurred or the connection was
  // shutdown cleanly (SSL_ERROR_ZERO_RETURN) even when read == 0.
  // See node#1642 and SSL_read(3SSL) for details.
  if (read <= 0) {
    HandleScope handle_scope(env()->isolate());
    int err;

    Local<Value> arg = GetSSLError(read, &err, nullptr)
        .FromMaybe(Local<Value>());

    // Ignore ZERO_RETURN after EOF, it is basically not a error
    if (err == SSL_ERROR_ZERO_RETURN && eof_)
      return;

    if (LIKELY(!arg.IsEmpty())) {
      Debug(this, "Got SSL error (%d), calling onerror", err);
      // When TLS Alert are stored in wbio,
      // it should be flushed to socket before destroyed.
      if (BIO_pending(enc_out_) != 0)
        EncOut();

      MakeCallback(env()->onerror_string(), 1, &arg);
    }
  }
}

void TLSWrap::ClearIn() {
  Debug(this, "Trying to write cleartext input");
  // Ignore cycling data if ClientHello wasn't yet parsed
  if (!hello_parser_.IsEnded()) {
    Debug(this, "Returning from ClearIn(), hello_parser_ active");
    return;
  }

  if (ssl_ == nullptr) {
    Debug(this, "Returning from ClearIn(), ssl_ == nullptr");
    return;
  }

  if (pending_cleartext_input_.size() == 0) {
    Debug(this, "Returning from ClearIn(), no pending data");
    return;
  }

  AllocatedBuffer data = std::move(pending_cleartext_input_);
  MarkPopErrorOnReturn mark_pop_error_on_return;

  NodeBIO::FromBIO(enc_out_)->set_allocate_tls_hint(data.size());
  int written = SSL_write(ssl_.get(), data.data(), data.size());
  Debug(this, "Writing %zu bytes, written = %d", data.size(), written);
  CHECK(written == -1 || written == static_cast<int>(data.size()));

  // All written
  if (written != -1) {
    Debug(this, "Successfully wrote all data to SSL");
    return;
  }

  // Error or partial write
  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  int err;
  std::string error_str;
  MaybeLocal<Value> arg = GetSSLError(written, &err, &error_str);
  if (!arg.IsEmpty()) {
    Debug(this, "Got SSL error (%d)", err);
    write_callback_scheduled_ = true;
    // TODO(@sam-github) Should forward an error object with
    // .code/.function/.etc, if possible.
    return InvokeQueued(UV_EPROTO, error_str.c_str());
  }

  Debug(this, "Pushing data back");
  // Push back the not-yet-written data. This can be skipped in the error
  // case because no further writes would succeed anyway.
  pending_cleartext_input_ = std::move(data);
}

std::string TLSWrap::diagnostic_name() const {
  std::string name = "TLSWrap ";
  name += is_server() ? "server (" : "client (";
  name += std::to_string(static_cast<int64_t>(get_async_id())) + ")";
  return name;
}

AsyncWrap* TLSWrap::GetAsyncWrap() {
  return static_cast<AsyncWrap*>(this);
}

bool TLSWrap::IsIPCPipe() {
  return underlying_stream()->IsIPCPipe();
}

int TLSWrap::GetFD() {
  return underlying_stream()->GetFD();
}

bool TLSWrap::IsAlive() {
  return ssl_ &&
      underlying_stream() != nullptr &&
      underlying_stream()->IsAlive();
}

bool TLSWrap::IsClosing() {
  return underlying_stream()->IsClosing();
}

int TLSWrap::ReadStart() {
  Debug(this, "ReadStart()");
  if (underlying_stream() != nullptr && !eof_)
    return underlying_stream()->ReadStart();
  return 0;
}

int TLSWrap::ReadStop() {
  Debug(this, "ReadStop()");
  return underlying_stream() != nullptr ? underlying_stream()->ReadStop() : 0;
}

const char* TLSWrap::Error() const {
  return error_.empty() ? nullptr : error_.c_str();
}

void TLSWrap::ClearError() {
  error_.clear();
}

// Called by StreamBase::Write() to request async write of clear text into SSL.
// TODO(@sam-github) Should there be a TLSWrap::DoTryWrite()?
int TLSWrap::DoWrite(WriteWrap* w,
                     uv_buf_t* bufs,
                     size_t count,
                     uv_stream_t* send_handle) {
  CHECK_NULL(send_handle);
  Debug(this, "DoWrite()");

  if (ssl_ == nullptr) {
    ClearError();
    error_ = "Write after DestroySSL";
    return UV_EPROTO;
  }

  size_t length = 0;
  size_t i;
  size_t nonempty_i = 0;
  size_t nonempty_count = 0;
  for (i = 0; i < count; i++) {
    length += bufs[i].len;
    if (bufs[i].len > 0) {
      nonempty_i = i;
      nonempty_count += 1;
    }
  }

  // We want to trigger a Write() on the underlying stream to drive the stream
  // system, but don't want to encrypt empty buffers into a TLS frame, so see
  // if we can find something to Write().
  // First, call ClearOut(). It does an SSL_read(), which might cause handshake
  // or other internal messages to be encrypted. If it does, write them later
  // with EncOut().
  // If there is still no encrypted output, call Write(bufs) on the underlying
  // stream. Since the bufs are empty, it won't actually write non-TLS data
  // onto the socket, we just want the side-effects. After, make sure the
  // WriteWrap was accepted by the stream, or that we call Done() on it.
  if (length == 0) {
    Debug(this, "Empty write");
    ClearOut();
    if (BIO_pending(enc_out_) == 0) {
      Debug(this, "No pending encrypted output, writing to underlying stream");
      CHECK(!current_empty_write_);
      current_empty_write_.reset(w->GetAsyncWrap());
      StreamWriteResult res =
          underlying_stream()->Write(bufs, count, send_handle);
      if (!res.async) {
        BaseObjectPtr<TLSWrap> strong_ref{this};
        env()->SetImmediate([this, strong_ref](Environment* env) {
          OnStreamAfterWrite(WriteWrap::FromObject(current_empty_write_), 0);
        });
      }
      return 0;
    }
  }

  // Store the current write wrap
  CHECK(!current_write_);
  current_write_.reset(w->GetAsyncWrap());

  // Write encrypted data to underlying stream and call Done().
  if (length == 0) {
    EncOut();
    return 0;
  }

  AllocatedBuffer data;
  MarkPopErrorOnReturn mark_pop_error_on_return;

  int written = 0;

  // It is common for zero length buffers to be written,
  // don't copy data if there there is one buffer with data
  // and one or more zero length buffers.
  // _http_outgoing.js writes a zero length buffer in
  // in OutgoingMessage.prototype.end.  If there was a large amount
  // of data supplied to end() there is no sense allocating
  // and copying it when it could just be used.

  if (nonempty_count != 1) {
    data = AllocatedBuffer::AllocateManaged(env(), length);
    size_t offset = 0;
    for (i = 0; i < count; i++) {
      memcpy(data.data() + offset, bufs[i].base, bufs[i].len);
      offset += bufs[i].len;
    }

    NodeBIO::FromBIO(enc_out_)->set_allocate_tls_hint(length);
    written = SSL_write(ssl_.get(), data.data(), length);
  } else {
    // Only one buffer: try to write directly, only store if it fails
    uv_buf_t* buf = &bufs[nonempty_i];
    NodeBIO::FromBIO(enc_out_)->set_allocate_tls_hint(buf->len);
    written = SSL_write(ssl_.get(), buf->base, buf->len);

    if (written == -1) {
      data = AllocatedBuffer::AllocateManaged(env(), length);
      memcpy(data.data(), buf->base, buf->len);
    }
  }

  CHECK(written == -1 || written == static_cast<int>(length));
  Debug(this, "Writing %zu bytes, written = %d", length, written);

  if (written == -1) {
    int err;
    MaybeLocal<Value> arg = GetSSLError(written, &err, &error_);

    // If we stopped writing because of an error, it's fatal, discard the data.
    if (!arg.IsEmpty()) {
      // TODO(@jasnell): What are we doing with the error?
      Debug(this, "Got SSL error (%d), returning UV_EPROTO", err);
      current_write_.reset();
      return UV_EPROTO;
    }

    Debug(this, "Saving data for later write");
    // Otherwise, save unwritten data so it can be written later by ClearIn().
    CHECK_EQ(pending_cleartext_input_.size(), 0);
    pending_cleartext_input_ = std::move(data);
  }

  // Write any encrypted/handshake output that may be ready.
  // Guard against sync call of current_write_->Done(), its unsupported.
  in_dowrite_ = true;
  EncOut();
  in_dowrite_ = false;

  return 0;
}

uv_buf_t TLSWrap::OnStreamAlloc(size_t suggested_size) {
  CHECK_NOT_NULL(ssl_);

  size_t size = suggested_size;
  char* base = NodeBIO::FromBIO(enc_in_)->PeekWritable(&size);
  return uv_buf_init(base, size);
}

void TLSWrap::OnStreamRead(ssize_t nread, const uv_buf_t& buf) {
  Debug(this, "Read %zd bytes from underlying stream", nread);

  // Ignore everything after close_notify (rfc5246#section-7.2.1)
  if (eof_)
    return;

  if (nread < 0)  {
    // Error should be emitted only after all data was read
    ClearOut();

    if (nread == UV_EOF) {
      // underlying stream already should have also called ReadStop on itself
      eof_ = true;
    }

    EmitRead(nread);
    return;
  }

  // DestroySSL() is the only thing that un-sets ssl_, but that also removes
  // this TLSWrap as a stream listener, so we should not receive OnStreamRead()
  // calls anymore.
  CHECK(ssl_);

  // Commit the amount of data actually read into the peeked/allocated buffer
  // from the underlying stream.
  NodeBIO* enc_in = NodeBIO::FromBIO(enc_in_);
  enc_in->Commit(nread);

  // Parse ClientHello first, if we need to. It's only parsed if session event
  // listeners are used on the server side.  "ended" is the initial state, so
  // can mean parsing was never started, or that parsing is finished. Either
  // way, ended means we can give the buffered data to SSL.
  if (!hello_parser_.IsEnded()) {
    size_t avail = 0;
    uint8_t* data = reinterpret_cast<uint8_t*>(enc_in->Peek(&avail));
    CHECK_IMPLIES(data == nullptr, avail == 0);
    Debug(this, "Passing %zu bytes to the hello parser", avail);
    return hello_parser_.Parse(data, avail);
  }

  // Cycle OpenSSL's state
  Cycle();
}

ShutdownWrap* TLSWrap::CreateShutdownWrap(Local<Object> req_wrap_object) {
  return underlying_stream()->CreateShutdownWrap(req_wrap_object);
}

int TLSWrap::DoShutdown(ShutdownWrap* req_wrap) {
  Debug(this, "DoShutdown()");
  MarkPopErrorOnReturn mark_pop_error_on_return;

  if (ssl_ && SSL_shutdown(ssl_.get()) == 0)
    SSL_shutdown(ssl_.get());

  shutdown_ = true;
  EncOut();
  return underlying_stream()->DoShutdown(req_wrap);
}

void TLSWrap::SetVerifyMode(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK_EQ(args.Length(), 2);
  CHECK(args[0]->IsBoolean());
  CHECK(args[1]->IsBoolean());
  CHECK_NOT_NULL(wrap->ssl_);

  int verify_mode;
  if (wrap->is_server()) {
    bool request_cert = args[0]->IsTrue();
    if (!request_cert) {
      // If no cert is requested, there will be none to reject as unauthorized.
      verify_mode = SSL_VERIFY_NONE;
    } else {
      bool reject_unauthorized = args[1]->IsTrue();
      verify_mode = SSL_VERIFY_PEER;
      if (reject_unauthorized)
        verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    }
  } else {
    // Servers always send a cert if the cipher is not anonymous (anon is
    // disabled by default), so use VERIFY_NONE and check the cert after the
    // handshake has completed.
    verify_mode = SSL_VERIFY_NONE;
  }

  // Always allow a connection. We'll reject in javascript.
  SSL_set_verify(wrap->ssl_.get(), verify_mode, VerifyCallback);
}

void TLSWrap::EnableSessionCallbacks(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  CHECK_NOT_NULL(wrap->ssl_);
  wrap->enable_session_callbacks();

  // Clients don't use the HelloParser.
  if (wrap->is_client())
    return;

  NodeBIO::FromBIO(wrap->enc_in_)->set_initial(kMaxHelloLength);
  wrap->hello_parser_.Start(OnClientHello,
                            OnClientHelloParseEnd,
                            wrap);
}

void TLSWrap::EnableKeylogCallback(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  CHECK(wrap->sc_);
  wrap->sc_->SetKeylogCallback(KeylogCallback);
}

// Check required capabilities were not excluded from the OpenSSL build:
// - OPENSSL_NO_SSL_TRACE excludes SSL_trace()
// - OPENSSL_NO_STDIO excludes BIO_new_fp()
// HAVE_SSL_TRACE is available on the internal tcp_wrap binding for the tests.
#if defined(OPENSSL_NO_SSL_TRACE) || defined(OPENSSL_NO_STDIO)
# define HAVE_SSL_TRACE 0
#else
# define HAVE_SSL_TRACE 1
#endif

void TLSWrap::EnableTrace(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

#if HAVE_SSL_TRACE
  if (wrap->ssl_) {
    wrap->bio_trace_.reset(BIO_new_fp(stderr,  BIO_NOCLOSE | BIO_FP_TEXT));
    SSL_set_msg_callback(wrap->ssl_.get(), [](int write_p, int version, int
          content_type, const void* buf, size_t len, SSL* ssl, void* arg)
        -> void {
        // BIO_write(), etc., called by SSL_trace, may error. The error should
        // be ignored, trace is a "best effort", and its usually because stderr
        // is a non-blocking pipe, and its buffer has overflowed. Leaving errors
        // on the stack that can get picked up by later SSL_ calls causes
        // unwanted failures in SSL_ calls, so keep the error stack unchanged.
        MarkPopErrorOnReturn mark_pop_error_on_return;
        SSL_trace(write_p,  version, content_type, buf, len, ssl, arg);
    });
    SSL_set_msg_callback_arg(wrap->ssl_.get(), wrap->bio_trace_.get());
  }
#endif
}

void TLSWrap::DestroySSL(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  wrap->Destroy();
  Debug(wrap, "DestroySSL() finished");
}

void TLSWrap::Destroy() {
  if (!ssl_)
    return;

  // If there is a write happening, mark it as finished.
  write_callback_scheduled_ = true;

  // And destroy
  InvokeQueued(UV_ECANCELED, "Canceled because of SSL destruction");

  env()->isolate()->AdjustAmountOfExternalAllocatedMemory(-kExternalSize);
  ssl_.reset();

  enc_in_ = nullptr;
  enc_out_ = nullptr;

  if (underlying_stream() != nullptr)
    underlying_stream()->RemoveStreamListener(this);

  sc_.reset();
}

void TLSWrap::EnableCertCb(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  wrap->WaitForCertCb(OnClientHelloParseEnd, wrap);
}

void TLSWrap::WaitForCertCb(CertCb cb, void* arg) {
  cert_cb_ = cb;
  cert_cb_arg_ = arg;
}

void TLSWrap::OnClientHelloParseEnd(void* arg) {
  TLSWrap* c = static_cast<TLSWrap*>(arg);
  Debug(c, "OnClientHelloParseEnd()");
  c->Cycle();
}

void TLSWrap::GetServername(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK_NOT_NULL(wrap->ssl_);

  const char* servername = SSL_get_servername(wrap->ssl_.get(),
                                              TLSEXT_NAMETYPE_host_name);
  if (servername != nullptr) {
    args.GetReturnValue().Set(OneByteString(env->isolate(), servername));
  } else {
    args.GetReturnValue().Set(false);
  }
}

void TLSWrap::SetServername(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());
  CHECK(!wrap->started_);
  CHECK(wrap->is_client());

  CHECK(wrap->ssl_);

  Utf8Value servername(env->isolate(), args[0].As<String>());
  SSL_set_tlsext_host_name(wrap->ssl_.get(), *servername);
}

int TLSWrap::SelectSNIContextCallback(SSL* s, int* ad, void* arg) {
  TLSWrap* p = static_cast<TLSWrap*>(SSL_get_app_data(s));
  Environment* env = p->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  const char* servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);
  if (!Set(env, p->GetOwner(), env->servername_string(), servername))
    return SSL_TLSEXT_ERR_NOACK;

  Local<Value> ctx = p->object()->Get(env->context(), env->sni_context_string())
      .FromMaybe(Local<Value>());

  if (UNLIKELY(ctx.IsEmpty()) || !ctx->IsObject())
    return SSL_TLSEXT_ERR_NOACK;

  if (!env->secure_context_constructor_template()->HasInstance(ctx)) {
    // Failure: incorrect SNI context object
    Local<Value> err = Exception::TypeError(env->sni_context_err_string());
    p->MakeCallback(env->onerror_string(), 1, &err);
    return SSL_TLSEXT_ERR_NOACK;
  }

  SecureContext* sc = Unwrap<SecureContext>(ctx.As<Object>());
  CHECK_NOT_NULL(sc);
  p->sni_context_ = BaseObjectPtr<SecureContext>(sc);

  ConfigureSecureContext(sc);
  CHECK_EQ(SSL_set_SSL_CTX(p->ssl_.get(), sc->ctx_.get()), sc->ctx_.get());
  p->SetCACerts(sc);

  return SSL_TLSEXT_ERR_OK;
}

int TLSWrap::SetCACerts(SecureContext* sc) {
  int err = SSL_set1_verify_cert_store(
      ssl_.get(), SSL_CTX_get_cert_store(sc->ctx_.get()));
  if (err != 1)
    return err;

  STACK_OF(X509_NAME)* list =
      SSL_dup_CA_list(SSL_CTX_get_client_CA_list(sc->ctx_.get()));

  // NOTE: `SSL_set_client_CA_list` takes the ownership of `list`
  SSL_set_client_CA_list(ssl_.get(), list);
  return 1;
}

#ifndef OPENSSL_NO_PSK

void TLSWrap::SetPskIdentityHint(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* p;
  ASSIGN_OR_RETURN_UNWRAP(&p, args.Holder());
  CHECK_NOT_NULL(p->ssl_);

  Environment* env = p->env();
  Isolate* isolate = env->isolate();

  CHECK(args[0]->IsString());
  Utf8Value hint(isolate, args[0].As<String>());

  if (!SSL_use_psk_identity_hint(p->ssl_.get(), *hint)) {
    Local<Value> err = node::ERR_TLS_PSK_SET_IDENTIY_HINT_FAILED(isolate);
    p->MakeCallback(env->onerror_string(), 1, &err);
  }
}

void TLSWrap::EnablePskCallback(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  CHECK_NOT_NULL(wrap->ssl_);

  SSL_set_psk_server_callback(wrap->ssl_.get(), PskServerCallback);
  SSL_set_psk_client_callback(wrap->ssl_.get(), PskClientCallback);
}

unsigned int TLSWrap::PskServerCallback(
    SSL* s,
    const char* identity,
    unsigned char* psk,
    unsigned int max_psk_len) {
  TLSWrap* p = static_cast<TLSWrap*>(SSL_get_app_data(s));

  Environment* env = p->env();
  HandleScope scope(env->isolate());

  Local<String> identity_str =
      String::NewFromUtf8(env->isolate(), identity).FromMaybe(Local<String>());
  if (UNLIKELY(identity_str.IsEmpty()))
    return 0;

  // Make sure there are no utf8 replacement symbols.
  Utf8Value identity_utf8(env->isolate(), identity_str);
  if (strcmp(*identity_utf8, identity) != 0)
    return 0;

  Local<Value> argv[] = {
    identity_str,
    Integer::NewFromUnsigned(env->isolate(), max_psk_len)
  };

  Local<Value> psk_val =
      p->MakeCallback(env->onpskexchange_symbol(), arraysize(argv), argv)
          .FromMaybe(Local<Value>());
  if (UNLIKELY(psk_val.IsEmpty()) || !psk_val->IsArrayBufferView())
    return 0;

  ArrayBufferViewContents<char> psk_buf(psk_val);

  if (psk_buf.length() > max_psk_len)
    return 0;

  memcpy(psk, psk_buf.data(), psk_buf.length());
  return psk_buf.length();
}

unsigned int TLSWrap::PskClientCallback(
    SSL* s,
    const char* hint,
    char* identity,
    unsigned int max_identity_len,
    unsigned char* psk,
    unsigned int max_psk_len) {
  TLSWrap* p = static_cast<TLSWrap*>(SSL_get_app_data(s));

  Environment* env = p->env();
  HandleScope scope(env->isolate());

  Local<Value> argv[] = {
    Null(env->isolate()),
    Integer::NewFromUnsigned(env->isolate(), max_psk_len),
    Integer::NewFromUnsigned(env->isolate(), max_identity_len)
  };

  if (hint != nullptr) {
    Local<String> local_hint =
        String::NewFromUtf8(env->isolate(), hint).FromMaybe(Local<String>());
    if (UNLIKELY(local_hint.IsEmpty()))
      return 0;

    argv[0] = local_hint;
  }

  Local<Value> ret =
      p->MakeCallback(env->onpskexchange_symbol(), arraysize(argv), argv)
          .FromMaybe(Local<Value>());
  if (UNLIKELY(ret.IsEmpty()) || !ret->IsObject())
    return 0;

  Local<Object> obj = ret.As<Object>();

  Local<Value> psk_val = obj->Get(env->context(), env->psk_string())
      .FromMaybe(Local<Value>());
  if (UNLIKELY(psk_val.IsEmpty()) || !psk_val->IsArrayBufferView())
    return 0;

  ArrayBufferViewContents<char> psk_buf(psk_val);
  if (psk_buf.length() > max_psk_len)
    return 0;

  Local<Value> identity_val = obj->Get(env->context(), env->identity_string())
      .FromMaybe(Local<Value>());
  if (UNLIKELY(identity_val.IsEmpty()) || !identity_val->IsString())
    return 0;

  Utf8Value identity_buf(env->isolate(), identity_val);

  if (identity_buf.length() > max_identity_len)
    return 0;

  memcpy(identity, *identity_buf, identity_buf.length());
  memcpy(psk, psk_buf.data(), psk_buf.length());

  return psk_buf.length();
}

#endif

void TLSWrap::GetWriteQueueSize(const FunctionCallbackInfo<Value>& info) {
  TLSWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, info.This());

  if (!wrap->ssl_)
    return info.GetReturnValue().Set(0);

  uint32_t write_queue_size = BIO_pending(wrap->enc_out_);
  info.GetReturnValue().Set(write_queue_size);
}

void TLSWrap::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("ocsp_response", ocsp_response_);
  tracker->TrackField("sni_context", sni_context_);
  tracker->TrackField("error", error_);
  tracker->TrackFieldWithSize("pending_cleartext_input",
                              pending_cleartext_input_.size(),
                              "AllocatedBuffer");
  if (enc_in_ != nullptr)
    tracker->TrackField("enc_in", NodeBIO::FromBIO(enc_in_));
  if (enc_out_ != nullptr)
    tracker->TrackField("enc_out", NodeBIO::FromBIO(enc_out_));
}

void TLSWrap::CertCbDone(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  CHECK(w->is_waiting_cert_cb() && w->cert_cb_running_);

  Local<Object> object = w->object();
  Local<Value> ctx = object->Get(env->context(), env->sni_context_string())
      .FromMaybe(Local<Value>());
  if (UNLIKELY(ctx.IsEmpty()))
    return;

  Local<FunctionTemplate> cons = env->secure_context_constructor_template();
  if (cons->HasInstance(ctx)) {
    SecureContext* sc = Unwrap<SecureContext>(ctx.As<Object>());
    CHECK_NOT_NULL(sc);
    // Store the SNI context for later use.
    w->sni_context_ = BaseObjectPtr<SecureContext>(sc);

    if (UseSNIContext(w->ssl_, w->sni_context_) && !w->SetCACerts(sc)) {
      // Not clear why sometimes we throw error, and sometimes we call
      // onerror(). Both cause .destroy(), but onerror does a bit more.
      unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
      return ThrowCryptoError(env, err, "CertCbDone");
    }
  } else if (ctx->IsObject()) {
    // Failure: incorrect SNI context object
    Local<Value> err = Exception::TypeError(env->sni_context_err_string());
    w->MakeCallback(env->onerror_string(), 1, &err);
    return;
  }

  CertCb cb;
  void* arg;

  cb = w->cert_cb_;
  arg = w->cert_cb_arg_;

  w->cert_cb_running_ = false;
  w->cert_cb_ = nullptr;
  w->cert_cb_arg_ = nullptr;

  cb(arg);
}

void TLSWrap::SetALPNProtocols(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();
  if (args.Length() < 1 || !Buffer::HasInstance(args[0]))
    return env->ThrowTypeError("Must give a Buffer as first argument");

  if (w->is_client()) {
    CHECK(SetALPN(w->ssl_, args[0]));
  } else {
    CHECK(
        w->object()->SetPrivate(
            env->context(),
            env->alpn_buffer_private_symbol(),
            args[0]).FromJust());
    // Server should select ALPN protocol from list of advertised by client
    SSL_CTX_set_alpn_select_cb(SSL_get_SSL_CTX(w->ssl_.get()),
                               SelectALPNCallback,
                               nullptr);
  }
}

void TLSWrap::GetPeerCertificate(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();

  bool abbreviated = args.Length() < 1 || !args[0]->IsTrue();

  Local<Value> ret;
  if (GetPeerCert(
          env,
          w->ssl_,
          abbreviated,
          w->is_server()).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void TLSWrap::GetPeerX509Certificate(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();

  X509Certificate::GetPeerCertificateFlag flag = w->is_server()
      ? X509Certificate::GetPeerCertificateFlag::SERVER
      : X509Certificate::GetPeerCertificateFlag::NONE;

  Local<Value> ret;
  if (X509Certificate::GetPeerCert(env, w->ssl_, flag).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void TLSWrap::GetCertificate(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();

  Local<Value> ret;
  if (GetCert(env, w->ssl_).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void TLSWrap::GetX509Certificate(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();
  Local<Value> ret;
  if (X509Certificate::GetCert(env, w->ssl_).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void TLSWrap::GetFinished(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  // We cannot just pass nullptr to SSL_get_finished()
  // because it would further be propagated to memcpy(),
  // where the standard requirements as described in ISO/IEC 9899:2011
  // sections 7.21.2.1, 7.21.1.2, and 7.1.4, would be violated.
  // Thus, we use a dummy byte.
  char dummy[1];
  size_t len = SSL_get_finished(w->ssl_.get(), dummy, sizeof dummy);
  if (len == 0)
    return;

  AllocatedBuffer buf = AllocatedBuffer::AllocateManaged(env, len);
  CHECK_EQ(len, SSL_get_finished(w->ssl_.get(), buf.data(), len));
  args.GetReturnValue().Set(buf.ToBuffer().FromMaybe(Local<Value>()));
}

void TLSWrap::GetPeerFinished(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  // We cannot just pass nullptr to SSL_get_peer_finished()
  // because it would further be propagated to memcpy(),
  // where the standard requirements as described in ISO/IEC 9899:2011
  // sections 7.21.2.1, 7.21.1.2, and 7.1.4, would be violated.
  // Thus, we use a dummy byte.
  char dummy[1];
  size_t len = SSL_get_peer_finished(w->ssl_.get(), dummy, sizeof dummy);
  if (len == 0)
    return;

  AllocatedBuffer buf = AllocatedBuffer::AllocateManaged(env, len);
  CHECK_EQ(len, SSL_get_peer_finished(w->ssl_.get(), buf.data(), len));
  args.GetReturnValue().Set(buf.ToBuffer().FromMaybe(Local<Value>()));
}

void TLSWrap::GetSession(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  SSL_SESSION* sess = SSL_get_session(w->ssl_.get());
  if (sess == nullptr)
    return;

  int slen = i2d_SSL_SESSION(sess, nullptr);
  if (slen <= 0)
    return;  // Invalid or malformed session.

  AllocatedBuffer sbuf = AllocatedBuffer::AllocateManaged(env, slen);
  unsigned char* p = reinterpret_cast<unsigned char*>(sbuf.data());
  CHECK_LT(0, i2d_SSL_SESSION(sess, &p));
  args.GetReturnValue().Set(sbuf.ToBuffer().FromMaybe(Local<Value>()));
}

void TLSWrap::SetSession(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  if (args.Length() < 1)
    return THROW_ERR_MISSING_ARGS(env, "Session argument is mandatory");

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "Session");

  SSLSessionPointer sess = GetTLSSession(args[0]);
  if (sess == nullptr)
    return;

  if (!SetTLSSession(w->ssl_, sess))
    return env->ThrowError("SSL_set_session error");
}

void TLSWrap::IsSessionReused(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  bool yes = SSL_session_reused(w->ssl_.get());
  args.GetReturnValue().Set(yes);
}

void TLSWrap::VerifyError(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  // XXX(bnoordhuis) The UNABLE_TO_GET_ISSUER_CERT error when there is no
  // peer certificate is questionable but it's compatible with what was
  // here before.
  long x509_verify_error =  // NOLINT(runtime/int)
      VerifyPeerCertificate(
          w->ssl_,
          X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT);

  if (x509_verify_error == X509_V_OK)
    return args.GetReturnValue().SetNull();

  const char* reason = X509_verify_cert_error_string(x509_verify_error);
  const char* code = X509ErrorCode(x509_verify_error);

  Local<Object> exception =
      Exception::Error(OneByteString(env->isolate(), reason))
          ->ToObject(env->isolate()->GetCurrentContext())
              .FromMaybe(Local<Object>());

  if (Set(env, exception, env->code_string(), code))
    args.GetReturnValue().Set(exception);
}

void TLSWrap::GetCipher(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  args.GetReturnValue().Set(
      GetCipherInfo(env, w->ssl_).FromMaybe(Local<Object>()));
}

void TLSWrap::LoadSession(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  // TODO(@sam-github) check arg length and types in js, and CHECK in c++
  if (args.Length() >= 1 && Buffer::HasInstance(args[0])) {
    ArrayBufferViewContents<unsigned char> sbuf(args[0]);

    const unsigned char* p = sbuf.data();
    SSL_SESSION* sess = d2i_SSL_SESSION(nullptr, &p, sbuf.length());

    // Setup next session and move hello to the BIO buffer
    w->next_sess_.reset(sess);
  }
}

void TLSWrap::GetSharedSigalgs(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  SSL* ssl = w->ssl_.get();
  int nsig = SSL_get_shared_sigalgs(ssl, 0, nullptr, nullptr, nullptr, nullptr,
                                    nullptr);
  MaybeStackBuffer<Local<Value>, 16> ret_arr(nsig);

  for (int i = 0; i < nsig; i++) {
    int hash_nid;
    int sign_nid;
    std::string sig_with_md;

    SSL_get_shared_sigalgs(ssl, i, &sign_nid, &hash_nid, nullptr, nullptr,
                           nullptr);

    switch (sign_nid) {
      case EVP_PKEY_RSA:
        sig_with_md = "RSA+";
        break;

      case EVP_PKEY_RSA_PSS:
        sig_with_md = "RSA-PSS+";
        break;

      case EVP_PKEY_DSA:
        sig_with_md = "DSA+";
        break;

      case EVP_PKEY_EC:
        sig_with_md = "ECDSA+";
        break;

      case NID_ED25519:
        sig_with_md = "Ed25519+";
        break;

      case NID_ED448:
        sig_with_md = "Ed448+";
        break;
#ifndef OPENSSL_NO_GOST
      case NID_id_GostR3410_2001:
        sig_with_md = "gost2001+";
        break;

      case NID_id_GostR3410_2012_256:
        sig_with_md = "gost2012_256+";
        break;

      case NID_id_GostR3410_2012_512:
        sig_with_md = "gost2012_512+";
        break;
#endif  // !OPENSSL_NO_GOST
      default:
        const char* sn = OBJ_nid2sn(sign_nid);

        if (sn != nullptr) {
          sig_with_md = std::string(sn) + "+";
        } else {
          sig_with_md = "UNDEF+";
        }
        break;
    }

    const char* sn_hash = OBJ_nid2sn(hash_nid);
    if (sn_hash != nullptr) {
      sig_with_md += std::string(sn_hash);
    } else {
      sig_with_md += "UNDEF";
    }
    ret_arr[i] = OneByteString(env->isolate(), sig_with_md.c_str());
  }

  args.GetReturnValue().Set(
                 Array::New(env->isolate(), ret_arr.out(), ret_arr.length()));
}

void TLSWrap::ExportKeyingMaterial(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsString());

  Environment* env = Environment::GetCurrent(args);
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  uint32_t olen = args[0].As<Uint32>()->Value();
  Utf8Value label(env->isolate(), args[1]);

  AllocatedBuffer out = AllocatedBuffer::AllocateManaged(env, olen);

  ByteSource context;
  bool use_context = !args[2]->IsUndefined();
  if (use_context)
    context = ByteSource::FromBuffer(args[2]);

  if (SSL_export_keying_material(
          w->ssl_.get(),
          reinterpret_cast<unsigned char*>(out.data()),
          olen,
          *label,
          label.length(),
          reinterpret_cast<const unsigned char*>(context.get()),
          context.size(),
          use_context) != 1) {
    return ThrowCryptoError(
         env,
         ERR_get_error(),
         "SSL_export_keying_material");
  }

  args.GetReturnValue().Set(out.ToBuffer().FromMaybe(Local<Value>()));
}

void TLSWrap::EndParser(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  w->hello_parser_.End();
}

void TLSWrap::Renegotiate(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  ClearErrorOnReturn clear_error_on_return;
  if (SSL_renegotiate(w->ssl_.get()) != 1)
    return ThrowCryptoError(w->env(), ERR_get_error());
}

void TLSWrap::GetTLSTicket(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();

  SSL_SESSION* sess = SSL_get_session(w->ssl_.get());
  if (sess == nullptr)
    return;

  const unsigned char* ticket;
  size_t length;
  SSL_SESSION_get0_ticket(sess, &ticket, &length);

  if (ticket != nullptr) {
    args.GetReturnValue().Set(
        Buffer::Copy(env, reinterpret_cast<const char*>(ticket), length)
            .FromMaybe(Local<Object>()));
  }
}

void TLSWrap::NewSessionDone(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  w->awaiting_new_session_ = false;
  w->NewSessionDoneCb();
}

void TLSWrap::SetOCSPResponse(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();

  if (args.Length() < 1)
    return THROW_ERR_MISSING_ARGS(env, "OCSP response argument is mandatory");

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "OCSP response");

  w->ocsp_response_.Reset(args.GetIsolate(), args[0].As<ArrayBufferView>());
}

void TLSWrap::RequestOCSP(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  SSL_set_tlsext_status_type(w->ssl_.get(), TLSEXT_STATUSTYPE_ocsp);
}

void TLSWrap::GetEphemeralKeyInfo(const FunctionCallbackInfo<Value>& args) {
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = Environment::GetCurrent(args);

  CHECK(w->ssl_);

  // tmp key is available on only client
  if (w->is_server())
    return args.GetReturnValue().SetNull();

  args.GetReturnValue().Set(GetEphemeralKey(env, w->ssl_)
      .FromMaybe(Local<Value>()));

  // TODO(@sam-github) semver-major: else return ThrowCryptoError(env,
  // ERR_get_error())
}

void TLSWrap::GetProtocol(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  args.GetReturnValue().Set(
      OneByteString(env->isolate(), SSL_get_version(w->ssl_.get())));
}

void TLSWrap::GetALPNNegotiatedProto(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  const unsigned char* alpn_proto;
  unsigned int alpn_proto_len;

  SSL_get0_alpn_selected(w->ssl_.get(), &alpn_proto, &alpn_proto_len);

  Local<Value> result;
  if (alpn_proto_len == 0) {
    result = False(env->isolate());
  } else if (alpn_proto_len == sizeof("h2") - 1 &&
             0 == memcmp(alpn_proto, "h2", sizeof("h2") - 1)) {
    result = env->h2_string();
  } else if (alpn_proto_len == sizeof("http/1.1") - 1 &&
             0 == memcmp(alpn_proto, "http/1.1", sizeof("http/1.1") - 1)) {
    result = env->http_1_1_string();
  } else {
    result = OneByteString(env->isolate(), alpn_proto, alpn_proto_len);
  }

  args.GetReturnValue().Set(result);
}

void TLSWrap::Cycle() {
  // Prevent recursion
  if (++cycle_depth_ > 1)
    return;

  for (; cycle_depth_ > 0; cycle_depth_--) {
    ClearIn();
    ClearOut();
    // EncIn() doesn't exist, it happens via stream listener callbacks.
    EncOut();
  }
}

#ifdef SSL_set_max_send_fragment
void TLSWrap::SetMaxSendFragment(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.Length() >= 1 && args[0]->IsNumber());
  Environment* env = Environment::GetCurrent(args);
  TLSWrap* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  int rv = SSL_set_max_send_fragment(
      w->ssl_.get(),
      args[0]->Int32Value(env->context()).FromJust());
  args.GetReturnValue().Set(rv);
}
#endif  // SSL_set_max_send_fragment

void TLSWrap::Initialize(
    Local<Object> target,
    Local<Value> unused,
    Local<Context> context,
    void* priv) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "wrap", TLSWrap::Wrap);

  NODE_DEFINE_CONSTANT(target, HAVE_SSL_TRACE);

  Local<FunctionTemplate> t = BaseObject::MakeLazilyInitializedJSTemplate(env);
  Local<String> tlsWrapString =
      FIXED_ONE_BYTE_STRING(env->isolate(), "TLSWrap");
  t->SetClassName(tlsWrapString);
  t->InstanceTemplate()->SetInternalFieldCount(StreamBase::kInternalFieldCount);

  Local<FunctionTemplate> get_write_queue_size =
      FunctionTemplate::New(env->isolate(),
                            GetWriteQueueSize,
                            Local<Value>(),
                            Signature::New(env->isolate(), t));
  t->PrototypeTemplate()->SetAccessorProperty(
      env->write_queue_size_string(),
      get_write_queue_size,
      Local<FunctionTemplate>(),
      static_cast<PropertyAttribute>(ReadOnly | DontDelete));

  t->Inherit(AsyncWrap::GetConstructorTemplate(env));

  env->SetProtoMethod(t, "certCbDone", CertCbDone);
  env->SetProtoMethod(t, "destroySSL", DestroySSL);
  env->SetProtoMethod(t, "enableCertCb", EnableCertCb);
  env->SetProtoMethod(t, "endParser", EndParser);
  env->SetProtoMethod(t, "enableKeylogCallback", EnableKeylogCallback);
  env->SetProtoMethod(t, "enableSessionCallbacks", EnableSessionCallbacks);
  env->SetProtoMethod(t, "enableTrace", EnableTrace);
  env->SetProtoMethod(t, "getServername", GetServername);
  env->SetProtoMethod(t, "loadSession", LoadSession);
  env->SetProtoMethod(t, "newSessionDone", NewSessionDone);
  env->SetProtoMethod(t, "receive", Receive);
  env->SetProtoMethod(t, "renegotiate", Renegotiate);
  env->SetProtoMethod(t, "requestOCSP", RequestOCSP);
  env->SetProtoMethod(t, "setALPNProtocols", SetALPNProtocols);
  env->SetProtoMethod(t, "setOCSPResponse", SetOCSPResponse);
  env->SetProtoMethod(t, "setServername", SetServername);
  env->SetProtoMethod(t, "setSession", SetSession);
  env->SetProtoMethod(t, "setVerifyMode", SetVerifyMode);
  env->SetProtoMethod(t, "start", Start);

  env->SetProtoMethodNoSideEffect(t, "exportKeyingMaterial",
                                  ExportKeyingMaterial);
  env->SetProtoMethodNoSideEffect(t, "isSessionReused", IsSessionReused);
  env->SetProtoMethodNoSideEffect(t, "getALPNNegotiatedProtocol",
                                  GetALPNNegotiatedProto);
  env->SetProtoMethodNoSideEffect(t, "getCertificate", GetCertificate);
  env->SetProtoMethodNoSideEffect(t, "getX509Certificate", GetX509Certificate);
  env->SetProtoMethodNoSideEffect(t, "getCipher", GetCipher);
  env->SetProtoMethodNoSideEffect(t, "getEphemeralKeyInfo",
                                  GetEphemeralKeyInfo);
  env->SetProtoMethodNoSideEffect(t, "getFinished", GetFinished);
  env->SetProtoMethodNoSideEffect(t, "getPeerCertificate", GetPeerCertificate);
  env->SetProtoMethodNoSideEffect(t, "getPeerX509Certificate",
                                  GetPeerX509Certificate);
  env->SetProtoMethodNoSideEffect(t, "getPeerFinished", GetPeerFinished);
  env->SetProtoMethodNoSideEffect(t, "getProtocol", GetProtocol);
  env->SetProtoMethodNoSideEffect(t, "getSession", GetSession);
  env->SetProtoMethodNoSideEffect(t, "getSharedSigalgs", GetSharedSigalgs);
  env->SetProtoMethodNoSideEffect(t, "getTLSTicket", GetTLSTicket);
  env->SetProtoMethodNoSideEffect(t, "verifyError", VerifyError);

#ifdef SSL_set_max_send_fragment
  env->SetProtoMethod(t, "setMaxSendFragment", SetMaxSendFragment);
#endif  // SSL_set_max_send_fragment

#ifndef OPENSSL_NO_PSK
  env->SetProtoMethod(t, "enablePskCallback", EnablePskCallback);
  env->SetProtoMethod(t, "setPskIdentityHint", SetPskIdentityHint);
#endif  // !OPENSSL_NO_PSK

  StreamBase::AddMethods(env, t);

  Local<Function> fn = t->GetFunction(env->context()).ToLocalChecked();

  env->set_tls_wrap_constructor_function(fn);

  target->Set(env->context(), tlsWrapString, fn).Check();
}

}  // namespace crypto
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(tls_wrap, node::crypto::TLSWrap::Initialize)
