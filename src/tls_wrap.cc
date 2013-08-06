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
#include "node_buffer.h"  // Buffer
#include "node_crypto.h"  // SecureContext
#include "node_crypto_bio.h"  // NodeBIO
#include "node_crypto_clienthello.h"  // ClientHelloParser
#include "node_crypto_clienthello-inl.h"
#include "node_wrap.h"  // WithGenericStream
#include "node_counters.h"
#include "node_internals.h"

namespace node {

using crypto::SecureContext;
using v8::Array;
using v8::Boolean;
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
using v8::Persistent;
using v8::String;
using v8::Value;

static Cached<String> onread_sym;
static Cached<String> onerror_sym;
static Cached<String> onhandshakestart_sym;
static Cached<String> onhandshakedone_sym;
static Cached<String> onclienthello_sym;
static Cached<String> onnewsession_sym;
static Cached<String> subject_sym;
static Cached<String> subjectaltname_sym;
static Cached<String> modulus_sym;
static Cached<String> exponent_sym;
static Cached<String> issuer_sym;
static Cached<String> valid_from_sym;
static Cached<String> valid_to_sym;
static Cached<String> fingerprint_sym;
static Cached<String> name_sym;
static Cached<String> version_sym;
static Cached<String> ext_key_usage_sym;
static Cached<String> sessionid_sym;
static Cached<String> tls_ticket_sym;
static Cached<String> servername_sym;
static Cached<String> sni_context_sym;

static Persistent<Function> tlsWrap;

static const int X509_NAME_FLAGS = ASN1_STRFLGS_ESC_CTRL
                                 | ASN1_STRFLGS_ESC_MSB
                                 | XN_FLAG_SEP_MULTILINE
                                 | XN_FLAG_FN_SN;


TLSCallbacks::TLSCallbacks(Kind kind,
                           Handle<Object> sc,
                           StreamWrapCallbacks* old)
    : StreamWrapCallbacks(old),
      kind_(kind),
      ssl_(NULL),
      enc_in_(NULL),
      enc_out_(NULL),
      clear_in_(NULL),
      write_size_(0),
      pending_write_item_(NULL),
      started_(false),
      established_(false),
      shutdown_(false),
      session_callbacks_(false),
      next_sess_(NULL) {

  // Persist SecureContext
  sc_ = ObjectWrap::Unwrap<SecureContext>(sc);
  sc_handle_.Reset(node_isolate, sc);

  Local<Object> object = NewInstance(tlsWrap);
  WRAP(object, this);
  persistent().Reset(node_isolate, object);

  // Initialize queue for clearIn writes
  QUEUE_INIT(&write_item_queue_);

  // We've our own session callbacks
  SSL_CTX_sess_set_get_cb(sc_->ctx_, GetSessionCallback);
  SSL_CTX_sess_set_new_cb(sc_->ctx_, NewSessionCallback);

  InitSSL();
}


SSL_SESSION* TLSCallbacks::GetSessionCallback(SSL* s,
                                              unsigned char* key,
                                              int len,
                                              int* copy) {
  HandleScope scope(node_isolate);

  TLSCallbacks* c = static_cast<TLSCallbacks*>(SSL_get_app_data(s));

  *copy = 0;
  SSL_SESSION* sess = c->next_sess_;
  c->next_sess_ = NULL;

  return sess;
}


int TLSCallbacks::NewSessionCallback(SSL* s, SSL_SESSION* sess) {
  HandleScope scope(node_isolate);

  TLSCallbacks* c = static_cast<TLSCallbacks*>(SSL_get_app_data(s));
  if (!c->session_callbacks_)
    return 0;

  // Check if session is small enough to be stored
  int size = i2d_SSL_SESSION(sess, NULL);
  if (size > SecureContext::kMaxSessionSize)
    return 0;

  // Serialize session
  Local<Object> buff = Buffer::New(size);
  unsigned char* serialized = reinterpret_cast<unsigned char*>(
      Buffer::Data(buff));
  memset(serialized, 0, size);
  i2d_SSL_SESSION(sess, &serialized);

  Local<Object> session = Buffer::New(reinterpret_cast<char*>(sess->session_id),
                                      sess->session_id_length);
  Handle<Value> argv[2] = { session, buff };
  MakeCallback(c->object(), onnewsession_sym, ARRAY_SIZE(argv), argv);

  return 0;
}


TLSCallbacks::~TLSCallbacks() {
  SSL_free(ssl_);
  ssl_ = NULL;
  enc_in_ = NULL;
  enc_out_ = NULL;
  delete clear_in_;
  clear_in_ = NULL;

  sc_ = NULL;
  sc_handle_.Dispose();
  persistent().Dispose();

#ifdef OPENSSL_NPN_NEGOTIATED
  npn_protos_.Dispose();
  selected_npn_proto_.Dispose();
#endif  // OPENSSL_NPN_NEGOTIATED

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


static int VerifyCallback(int preverify_ok, X509_STORE_CTX *ctx) {
  // Quoting SSL_set_verify(3ssl):
  //
  //   The VerifyCallback function is used to control the behaviour when
  //   the SSL_VERIFY_PEER flag is set. It must be supplied by the
  //   application and receives two arguments: preverify_ok indicates,
  //   whether the verification of the certificate in question was passed
  //   (preverify_ok=1) or not (preverify_ok=0). x509_ctx is a pointer to
  //   the complete context used for the certificate chain verification.
  //
  //   The certificate chain is checked starting with the deepest nesting
  //   level (the root CA certificate) and worked upward to the peer's
  //   certificate.  At each level signatures and issuer attributes are
  //   checked.  Whenever a verification error is found, the error number is
  //   stored in x509_ctx and VerifyCallback is called with preverify_ok=0.
  //   By applying X509_CTX_store_* functions VerifyCallback can locate the
  //   certificate in question and perform additional steps (see EXAMPLES).
  //   If no error is found for a certificate, VerifyCallback is called
  //   with preverify_ok=1 before advancing to the next level.
  //
  //   The return value of VerifyCallback controls the strategy of the
  //   further verification process. If VerifyCallback returns 0, the
  //   verification process is immediately stopped with "verification
  //   failed" state. If SSL_VERIFY_PEER is set, a verification failure
  //   alert is sent to the peer and the TLS/SSL handshake is terminated. If
  //   VerifyCallback returns 1, the verification process is continued. If
  //   VerifyCallback always returns 1, the TLS/SSL handshake will not be
  //   terminated with respect to verification failures and the connection
  //   will be established. The calling process can however retrieve the
  //   error code of the last verification error using
  //   SSL_get_verify_result(3) or by maintaining its own error storage
  //   managed by VerifyCallback.
  //
  //   If no VerifyCallback is specified, the default callback will be
  //   used.  Its return value is identical to preverify_ok, so that any
  //   verification failure will lead to a termination of the TLS/SSL
  //   handshake with an alert message, if SSL_VERIFY_PEER is set.
  //
  // Since we cannot perform I/O quickly enough in this callback, we ignore
  // all preverify_ok errors and let the handshake continue. It is
  // imparative that the user use Connection::VerifyError after the
  // 'secure' callback has been made.
  return 1;
}


void TLSCallbacks::InitSSL() {
  assert(ssl_ == NULL);

  // Initialize SSL
  ssl_ = SSL_new(sc_->ctx_);
  enc_in_ = BIO_new(NodeBIO::GetMethod());
  enc_out_ = BIO_new(NodeBIO::GetMethod());

  SSL_set_bio(ssl_, enc_in_, enc_out_);

  // NOTE: This could be overriden in SetVerifyMode
  SSL_set_verify(ssl_, SSL_VERIFY_NONE, VerifyCallback);

#ifdef SSL_MODE_RELEASE_BUFFERS
  long mode = SSL_get_mode(ssl_);
  SSL_set_mode(ssl_, mode | SSL_MODE_RELEASE_BUFFERS);
#endif  // SSL_MODE_RELEASE_BUFFERS

  SSL_set_app_data(ssl_, this);
  SSL_set_info_callback(ssl_, SSLInfoCallback);

  if (kind_ == kTLSServer) {
    SSL_set_accept_state(ssl_);

#ifdef OPENSSL_NPN_NEGOTIATED
    // Server should advertise NPN protocols
    SSL_CTX_set_next_protos_advertised_cb(sc_->ctx_,
                                          AdvertiseNextProtoCallback,
                                          this);
#endif  // OPENSSL_NPN_NEGOTIATED

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
    SSL_CTX_set_tlsext_servername_callback(sc_->ctx_, SelectSNIContextCallback);
    SSL_CTX_set_tlsext_servername_arg(sc_->ctx_, this);
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  } else if (kind_ == kTLSClient) {
    SSL_set_connect_state(ssl_);

#ifdef OPENSSL_NPN_NEGOTIATED
    // Client should select protocol from list of advertised
    // If server supports NPN
    SSL_CTX_set_next_proto_select_cb(sc_->ctx_,
                                     SelectNextProtoCallback,
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
  HandleScope scope(node_isolate);

  if (args.Length() < 1 || !args[0]->IsObject())
    return ThrowTypeError("First argument should be a StreamWrap instance");
  if (args.Length() < 2 || !args[1]->IsObject())
    return ThrowTypeError("Second argument should be a SecureContext instance");
  if (args.Length() < 3 || !args[2]->IsBoolean())
    return ThrowTypeError("Third argument should be boolean");

  Local<Object> stream = args[0].As<Object>();
  Local<Object> sc = args[1].As<Object>();
  Kind kind = args[2]->IsTrue() ? kTLSServer : kTLSClient;

  TLSCallbacks* callbacks = NULL;
  WITH_GENERIC_STREAM(stream, {
    callbacks = new TLSCallbacks(kind, sc, wrap->callbacks());
    wrap->OverrideCallbacks(callbacks);
  });

  if (callbacks == NULL) {
    return args.GetReturnValue().SetNull();
  }

  args.GetReturnValue().Set(callbacks->persistent());
}


void TLSCallbacks::Start(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  if (wrap->started_)
    return ThrowError("Already started.");
  wrap->started_ = true;

  // Send ClientHello handshake
  assert(wrap->kind_ == kTLSClient);
  wrap->ClearOut();
  wrap->EncOut();
}


void TLSCallbacks::SSLInfoCallback(const SSL* ssl_, int where, int ret) {
  // Be compatible with older versions of OpenSSL. SSL_get_app_data() wants
  // a non-const SSL* in OpenSSL <= 0.9.7e.
  SSL* ssl = const_cast<SSL*>(ssl_);
  if (where & SSL_CB_HANDSHAKE_START) {
    HandleScope scope(node_isolate);
    TLSCallbacks* c = static_cast<TLSCallbacks*>(SSL_get_app_data(ssl));
    Local<Object> object = c->object();
    if (object->Has(onhandshakestart_sym))
      MakeCallback(object, onhandshakestart_sym, 0, NULL);
  }
  if (where & SSL_CB_HANDSHAKE_DONE) {
    HandleScope scope(node_isolate);
    TLSCallbacks* c = static_cast<TLSCallbacks*>(SSL_get_app_data(ssl));
    c->established_ = true;
    Local<Object> object = c->object();
    if (object->Has(onhandshakedone_sym))
      MakeCallback(object, onhandshakedone_sym, 0, NULL);
  }
}


void TLSCallbacks::EncOut() {
  // Ignore cycling data if ClientHello wasn't yet parsed
  if (!hello_.IsEnded())
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
  HandleScope scope(node_isolate);

  TLSCallbacks* callbacks = static_cast<TLSCallbacks*>(req->data);

  // Handle error
  if (status) {
    // Ignore errors after shutdown
    if (callbacks->shutdown_)
      return;

    // Notify about error
    Local<Value> arg = String::Concat(
        FIXED_ONE_BYTE_STRING(node_isolate, "write cb error, status: "),
        Integer::New(status, node_isolate)->ToString());
    MakeCallback(callbacks->object(), onerror_sym, 1, &arg);
    callbacks->InvokeQueued(status);
    return;
  }

  // Commit
  NodeBIO::FromBIO(callbacks->enc_out_)->Read(NULL, callbacks->write_size_);

  // Try writing more data
  callbacks->write_size_ = 0;
  callbacks->EncOut();
}


Handle<Value> TLSCallbacks::GetSSLError(int status, int* err) {
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
  return Handle<Value>();
}


void TLSCallbacks::ClearOut() {
  // Ignore cycling data if ClientHello wasn't yet parsed
  if (!hello_.IsEnded())
    return;

  HandleScope scope(node_isolate);

  assert(ssl_ != NULL);

  char out[kClearOutChunkSize];
  int read;
  do {
    read = SSL_read(ssl_, out, sizeof(out));
    if (read > 0) {
      Local<Value> argv[] = {
        Integer::New(read, node_isolate),
        Buffer::New(out, read)
      };
      MakeCallback(Self(), onread_sym, ARRAY_SIZE(argv), argv);
    }
  } while (read > 0);

  if (read == -1) {
    int err;
    Handle<Value> argv = GetSSLError(read, &err);

    if (!argv.IsEmpty())
      MakeCallback(object(), onerror_sym, 1, &argv);
  }
}


bool TLSCallbacks::ClearIn() {
  // Ignore cycling data if ClientHello wasn't yet parsed
  if (!hello_.IsEnded())
    return false;

  HandleScope scope(node_isolate);

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

  // Error or partial write
  int err;
  Handle<Value> argv = GetSSLError(written, &err);
  if (!argv.IsEmpty())
    MakeCallback(object(), onerror_sym, 1, &argv);

  return false;
}


int TLSCallbacks::DoWrite(WriteWrap* w,
                          uv_buf_t* bufs,
                          size_t count,
                          uv_stream_t* send_handle,
                          uv_write_cb cb) {
  HandleScope scope(node_isolate);

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
    Handle<Value> argv = GetSSLError(written, &err);
    if (!argv.IsEmpty()) {
      MakeCallback(object(), onerror_sym, 1, &argv);
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


uv_buf_t TLSCallbacks::DoAlloc(uv_handle_t* handle, size_t suggested_size) {
  size_t size = suggested_size;
  char* data = NodeBIO::FromBIO(enc_in_)->PeekWritable(&size);
  return uv_buf_init(data, size);
}


void TLSCallbacks::DoRead(uv_stream_t* handle,
                          ssize_t nread,
                          uv_buf_t buf,
                          uv_handle_type pending) {
  if (nread < 0)  {
    // Error should be emitted only after all data was read
    ClearOut();
    Local<Value> arg = Integer::New(nread, node_isolate);
    MakeCallback(Self(), onread_sym, 1, &arg);
    return;
  }

  // Only client connections can receive data
  assert(ssl_ != NULL);

  // Commit read data
  NodeBIO* enc_in = NodeBIO::FromBIO(enc_in_);
  enc_in->Commit(nread);

  // Parse ClientHello first
  if (!hello_.IsEnded()) {
    size_t avail = 0;
    uint8_t* data = reinterpret_cast<uint8_t*>(enc_in->Peek(&avail));
    assert(avail == 0 || data != NULL);
    return hello_.Parse(data, avail);
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


#define CASE_X509_ERR(CODE) case X509_V_ERR_##CODE: reason = #CODE; break;
void TLSCallbacks::VerifyError(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  // XXX Do this check in JS land?
  X509* peer_cert = SSL_get_peer_certificate(wrap->ssl_);
  if (peer_cert == NULL) {
    // We requested a certificate and they did not send us one.
    // Definitely an error.
    // XXX is this the right error message?
    Local<String> s =
        FIXED_ONE_BYTE_STRING(node_isolate, "UNABLE_TO_GET_ISSUER_CERT");
    return args.GetReturnValue().Set(Exception::Error(s));
  }
  X509_free(peer_cert);

  long x509_verify_error = SSL_get_verify_result(wrap->ssl_);

  const char* reason = NULL;
  Local<String> s;
  switch (x509_verify_error) {
  case X509_V_OK:
    return args.GetReturnValue().SetNull();
  CASE_X509_ERR(UNABLE_TO_GET_ISSUER_CERT)
  CASE_X509_ERR(UNABLE_TO_GET_CRL)
  CASE_X509_ERR(UNABLE_TO_DECRYPT_CERT_SIGNATURE)
  CASE_X509_ERR(UNABLE_TO_DECRYPT_CRL_SIGNATURE)
  CASE_X509_ERR(UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY)
  CASE_X509_ERR(CERT_SIGNATURE_FAILURE)
  CASE_X509_ERR(CRL_SIGNATURE_FAILURE)
  CASE_X509_ERR(CERT_NOT_YET_VALID)
  CASE_X509_ERR(CERT_HAS_EXPIRED)
  CASE_X509_ERR(CRL_NOT_YET_VALID)
  CASE_X509_ERR(CRL_HAS_EXPIRED)
  CASE_X509_ERR(ERROR_IN_CERT_NOT_BEFORE_FIELD)
  CASE_X509_ERR(ERROR_IN_CERT_NOT_AFTER_FIELD)
  CASE_X509_ERR(ERROR_IN_CRL_LAST_UPDATE_FIELD)
  CASE_X509_ERR(ERROR_IN_CRL_NEXT_UPDATE_FIELD)
  CASE_X509_ERR(OUT_OF_MEM)
  CASE_X509_ERR(DEPTH_ZERO_SELF_SIGNED_CERT)
  CASE_X509_ERR(SELF_SIGNED_CERT_IN_CHAIN)
  CASE_X509_ERR(UNABLE_TO_GET_ISSUER_CERT_LOCALLY)
  CASE_X509_ERR(UNABLE_TO_VERIFY_LEAF_SIGNATURE)
  CASE_X509_ERR(CERT_CHAIN_TOO_LONG)
  CASE_X509_ERR(CERT_REVOKED)
  CASE_X509_ERR(INVALID_CA)
  CASE_X509_ERR(PATH_LENGTH_EXCEEDED)
  CASE_X509_ERR(INVALID_PURPOSE)
  CASE_X509_ERR(CERT_UNTRUSTED)
  CASE_X509_ERR(CERT_REJECTED)
  default:
    s = OneByteString(node_isolate,
                      X509_verify_cert_error_string(x509_verify_error));
    break;
  }

  if (s.IsEmpty()) {
    s = OneByteString(node_isolate, reason);
  }

  args.GetReturnValue().Set(Exception::Error(s));
}
#undef CASE_X509_ERR


void TLSCallbacks::SetVerifyMode(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  if (args.Length() < 2 || !args[0]->IsBoolean() || !args[1]->IsBoolean())
    return ThrowTypeError("Bad arguments, expected two booleans");

  int verify_mode;
  if (wrap->kind_ == kTLSServer) {
    bool request_cert = args[0]->IsTrue();
    if (!request_cert) {
      // Note reject_unauthorized ignored.
      verify_mode = SSL_VERIFY_NONE;
    } else {
      bool reject_unauthorized = args[1]->IsTrue();
      verify_mode = SSL_VERIFY_PEER;
      if (reject_unauthorized) verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    }
  } else {
    // Note request_cert and reject_unauthorized are ignored for clients.
    verify_mode = SSL_VERIFY_NONE;
  }

  // Always allow a connection. We'll reject in javascript.
  SSL_set_verify(wrap->ssl_, verify_mode, VerifyCallback);
}


void TLSCallbacks::IsSessionReused(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);
  bool yes = SSL_session_reused(wrap->ssl_);
  args.GetReturnValue().Set(yes);
}


void TLSCallbacks::EnableSessionCallbacks(
    const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  wrap->session_callbacks_ = true;
  EnableHelloParser(args);
}


void TLSCallbacks::EnableHelloParser(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  wrap->hello_.Start(OnClientHello, OnClientHelloParseEnd, wrap);
}


void TLSCallbacks::OnClientHello(void* arg,
                                 const ClientHelloParser::ClientHello& hello) {
  HandleScope scope(node_isolate);

  TLSCallbacks* c = static_cast<TLSCallbacks*>(arg);

  Local<Object> hello_obj = Object::New();
  Local<Object> buff = Buffer::New(
      reinterpret_cast<const char*>(hello.session_id()),
                                    hello.session_size());
  hello_obj->Set(sessionid_sym, buff);
  if (hello.servername() == NULL) {
    hello_obj->Set(servername_sym, String::Empty(node_isolate));
  } else {
    Local<String> servername = OneByteString(node_isolate,
                                             hello.servername(),
                                             hello.servername_size());
    hello_obj->Set(servername_sym, servername);
  }
  hello_obj->Set(tls_ticket_sym, Boolean::New(hello.has_ticket()));

  Handle<Value> argv[1] = { hello_obj };
  MakeCallback(c->object(), onclienthello_sym, 1, argv);
}


void TLSCallbacks::OnClientHelloParseEnd(void* arg) {
  TLSCallbacks* c = static_cast<TLSCallbacks*>(arg);
  c->Cycle();
}


void TLSCallbacks::GetPeerCertificate(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  Local<Object> info = Object::New();
  X509* peer_cert = SSL_get_peer_certificate(wrap->ssl_);
  if (peer_cert != NULL) {
    BIO* bio = BIO_new(BIO_s_mem());
    BUF_MEM* mem;
    if (X509_NAME_print_ex(bio,
                           X509_get_subject_name(peer_cert),
                           0,
                           X509_NAME_FLAGS) > 0) {
      BIO_get_mem_ptr(bio, &mem);
      info->Set(subject_sym,
                OneByteString(node_isolate, mem->data, mem->length));
    }
    (void) BIO_reset(bio);

    if (X509_NAME_print_ex(bio,
                           X509_get_issuer_name(peer_cert),
                           0,
                           X509_NAME_FLAGS) > 0) {
      BIO_get_mem_ptr(bio, &mem);
      info->Set(issuer_sym,
                OneByteString(node_isolate, mem->data, mem->length));
    }
    (void) BIO_reset(bio);

    int index = X509_get_ext_by_NID(peer_cert, NID_subject_alt_name, -1);
    if (index >= 0) {
      X509_EXTENSION* ext;
      int rv;

      ext = X509_get_ext(peer_cert, index);
      assert(ext != NULL);

      rv = X509V3_EXT_print(bio, ext, 0, 0);
      assert(rv == 1);

      BIO_get_mem_ptr(bio, &mem);
      info->Set(subjectaltname_sym,
                OneByteString(node_isolate, mem->data, mem->length));

      (void) BIO_reset(bio);
    }

    EVP_PKEY* pkey = NULL;
    RSA* rsa = NULL;
    if (NULL != (pkey = X509_get_pubkey(peer_cert)) &&
        NULL != (rsa = EVP_PKEY_get1_RSA(pkey))) {
      BN_print(bio, rsa->n);
      BIO_get_mem_ptr(bio, &mem);
      info->Set(modulus_sym,
                OneByteString(node_isolate, mem->data, mem->length));
      (void) BIO_reset(bio);

      BN_print(bio, rsa->e);
      BIO_get_mem_ptr(bio, &mem);
      info->Set(exponent_sym,
                OneByteString(node_isolate, mem->data, mem->length));
      (void) BIO_reset(bio);
    }

    if (pkey != NULL) {
      EVP_PKEY_free(pkey);
      pkey = NULL;
    }
    if (rsa != NULL) {
      RSA_free(rsa);
      rsa = NULL;
    }

    ASN1_TIME_print(bio, X509_get_notBefore(peer_cert));
    BIO_get_mem_ptr(bio, &mem);
    info->Set(valid_from_sym,
              OneByteString(node_isolate, mem->data, mem->length));
    (void) BIO_reset(bio);

    ASN1_TIME_print(bio, X509_get_notAfter(peer_cert));
    BIO_get_mem_ptr(bio, &mem);
    info->Set(valid_to_sym,
              OneByteString(node_isolate, mem->data, mem->length));
    BIO_free_all(bio);

    unsigned int md_size, i;
    unsigned char md[EVP_MAX_MD_SIZE];
    if (X509_digest(peer_cert, EVP_sha1(), md, &md_size)) {
      const char hex[] = "0123456789ABCDEF";
      char fingerprint[EVP_MAX_MD_SIZE * 3];

      for (i = 0; i < md_size; i++) {
        fingerprint[3*i] = hex[(md[i] & 0xf0) >> 4];
        fingerprint[(3*i)+1] = hex[(md[i] & 0x0f)];
        fingerprint[(3*i)+2] = ':';
      }

      if (md_size > 0)
        fingerprint[(3*(md_size-1))+2] = '\0';
      else
        fingerprint[0] = '\0';

      info->Set(fingerprint_sym, OneByteString(node_isolate, fingerprint));
    }

    STACK_OF(ASN1_OBJECT)* eku = static_cast<STACK_OF(ASN1_OBJECT)*>(
        X509_get_ext_d2i(peer_cert,
                         NID_ext_key_usage,
                         NULL,
                         NULL));
    if (eku != NULL) {
      Local<Array> ext_key_usage = Array::New();
      char buf[256];

      for (int i = 0; i < sk_ASN1_OBJECT_num(eku); i++) {
        memset(buf, 0, sizeof(buf));
        OBJ_obj2txt(buf, sizeof(buf) - 1, sk_ASN1_OBJECT_value(eku, i), 1);
        ext_key_usage->Set(Integer::New(i, node_isolate),
                           OneByteString(node_isolate, buf));
      }

      sk_ASN1_OBJECT_pop_free(eku, ASN1_OBJECT_free);
      info->Set(ext_key_usage_sym, ext_key_usage);
    }

    X509_free(peer_cert);
  }

  args.GetReturnValue().Set(info);
}


void TLSCallbacks::GetSession(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  SSL_SESSION* sess = SSL_get_session(wrap->ssl_);
  if (!sess) return;

  int slen = i2d_SSL_SESSION(sess, NULL);
  assert(slen > 0);

  if (slen > 0) {
    unsigned char* sbuf = new unsigned char[slen];
    unsigned char* p = sbuf;
    i2d_SSL_SESSION(sess, &p);
    Local<Value> s = Encode(sbuf, slen, BINARY);
    args.GetReturnValue().Set(s);
    delete[] sbuf;
    return;
  }

  args.GetReturnValue().SetNull();
}


void TLSCallbacks::SetSession(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  if (wrap->started_)
    return ThrowError("Already started.");

  if (args.Length() < 1 ||
      (!args[0]->IsString() && !Buffer::HasInstance(args[0]))) {
    return ThrowTypeError("Bad argument");
  }

  size_t slen = Buffer::Length(args[0]);
  char* sbuf = new char[slen];

  ssize_t wlen = DecodeWrite(sbuf, slen, args[0], BINARY);
  assert(wlen == static_cast<ssize_t>(slen));

  const unsigned char* p = reinterpret_cast<const unsigned char*>(sbuf);
  SSL_SESSION* sess = d2i_SSL_SESSION(NULL, &p, wlen);

  delete[] sbuf;

  if (!sess) return;

  int r = SSL_set_session(wrap->ssl_, sess);
  SSL_SESSION_free(sess);

  if (!r) {
    return ThrowError("SSL_set_session error");
  }
}


void TLSCallbacks::LoadSession(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  if (args.Length() >= 1 && Buffer::HasInstance(args[0])) {
    ssize_t slen = Buffer::Length(args[0]);
    char* sbuf = Buffer::Data(args[0]);

    const unsigned char* p = reinterpret_cast<unsigned char*>(sbuf);
    SSL_SESSION* sess = d2i_SSL_SESSION(NULL, &p, slen);

    // Setup next session and move hello to the BIO buffer
    if (wrap->next_sess_ != NULL)
      SSL_SESSION_free(wrap->next_sess_);
    wrap->next_sess_ = sess;

    Local<Object> info = Object::New();
#ifndef OPENSSL_NO_TLSEXT
    if (sess->tlsext_hostname == NULL) {
      info->Set(servername_sym, False(node_isolate));
    } else {
      info->Set(servername_sym,
                OneByteString(node_isolate, sess->tlsext_hostname));
    }
#endif
    args.GetReturnValue().Set(info);
  }
}

void TLSCallbacks::EndParser(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  wrap->hello_.End();
}


void TLSCallbacks::GetCurrentCipher(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  const SSL_CIPHER* c;

  c = SSL_get_current_cipher(wrap->ssl_);
  if (c == NULL)
    return;

  const char* cipher_name = SSL_CIPHER_get_name(c);
  const char* cipher_version = SSL_CIPHER_get_version(c);

  Local<Object> info = Object::New();
  info->Set(name_sym, OneByteString(node_isolate, cipher_name));
  info->Set(version_sym, OneByteString(node_isolate, cipher_version));
  args.GetReturnValue().Set(info);
}


#ifdef OPENSSL_NPN_NEGOTIATED
int TLSCallbacks::AdvertiseNextProtoCallback(SSL* s,
                                             const unsigned char** data,
                                             unsigned int* len,
                                             void* arg) {
  TLSCallbacks* p = static_cast<TLSCallbacks*>(arg);

  if (p->npn_protos_.IsEmpty()) {
    // No initialization - no NPN protocols
    *data = reinterpret_cast<const unsigned char*>("");
    *len = 0;
  } else {
    Local<Object> obj = PersistentToLocal(node_isolate, p->npn_protos_);
    *data = reinterpret_cast<const unsigned char*>(Buffer::Data(obj));
    *len = Buffer::Length(obj);
  }

  return SSL_TLSEXT_ERR_OK;
}


int TLSCallbacks::SelectNextProtoCallback(SSL* s,
                                          unsigned char** out,
                                          unsigned char* outlen,
                                          const unsigned char* in,
                                          unsigned int inlen,
                                          void* arg) {
  TLSCallbacks* p = static_cast<TLSCallbacks*>(arg);

  // Release old protocol handler if present
  p->selected_npn_proto_.Dispose();

  if (p->npn_protos_.IsEmpty()) {
    // We should at least select one protocol
    // If server is using NPN
    *out = reinterpret_cast<unsigned char*>(const_cast<char*>("http/1.1"));
    *outlen = 8;

    // set status: unsupported
    p->selected_npn_proto_.Reset(node_isolate, False(node_isolate));

    return SSL_TLSEXT_ERR_OK;
  }

  Local<Object> obj = PersistentToLocal(node_isolate, p->npn_protos_);
  const unsigned char* npn_protos =
      reinterpret_cast<const unsigned char*>(Buffer::Data(obj));
  size_t len = Buffer::Length(obj);

  int status = SSL_select_next_proto(out, outlen, in, inlen, npn_protos, len);
  Handle<Value> result;
  switch (status) {
    case OPENSSL_NPN_UNSUPPORTED:
      result = Null(node_isolate);
      break;
    case OPENSSL_NPN_NEGOTIATED:
      result = OneByteString(node_isolate, *out, *outlen);
      break;
    case OPENSSL_NPN_NO_OVERLAP:
      result = False(node_isolate);
      break;
    default:
      break;
  }

  if (!result.IsEmpty())
    p->selected_npn_proto_.Reset(node_isolate, result);

  return SSL_TLSEXT_ERR_OK;
}


void TLSCallbacks::GetNegotiatedProto(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  if (wrap->kind_ == kTLSClient) {
    if (wrap->selected_npn_proto_.IsEmpty() == false) {
      args.GetReturnValue().Set(wrap->selected_npn_proto_);
    }
    return;
  }

  const unsigned char* npn_proto;
  unsigned int npn_proto_len;

  SSL_get0_next_proto_negotiated(wrap->ssl_, &npn_proto, &npn_proto_len);

  if (!npn_proto) {
    return args.GetReturnValue().Set(false);
  }

  args.GetReturnValue().Set(
      OneByteString(node_isolate, npn_proto, npn_proto_len));
}


void TLSCallbacks::SetNPNProtocols(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  if (args.Length() < 1 || !Buffer::HasInstance(args[0]))
    return ThrowTypeError("Must give a Buffer as first argument");

  wrap->npn_protos_.Reset(node_isolate, args[0].As<Object>());
}
#endif  // OPENSSL_NPN_NEGOTIATED


#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
void TLSCallbacks::GetServername(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

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

  TLSCallbacks* wrap;
  UNWRAP(args.This(), TLSCallbacks, wrap);

  if (args.Length() < 1 || !args[0]->IsString())
    return ThrowTypeError("First argument should be a string");

  if (wrap->started_)
    return ThrowError("Already started.");

  if (wrap->kind_ != kTLSClient)
    return;

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  String::Utf8Value servername(args[0].As<String>());
  SSL_set_tlsext_host_name(wrap->ssl_, *servername);
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
}


int TLSCallbacks::SelectSNIContextCallback(SSL* s, int* ad, void* arg) {
  HandleScope scope(node_isolate);

  TLSCallbacks* p = static_cast<TLSCallbacks*>(arg);

  const char* servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);

  if (servername != NULL) {
    // Call the SNI callback and use its return value as context
    Local<Object> object = p->object();
    Local<Value> ctx;
    if (object->Has(sni_context_sym)) {
      ctx = object->Get(sni_context_sym);
    }

    if (ctx.IsEmpty() || ctx->IsUndefined())
      return SSL_TLSEXT_ERR_NOACK;

    p->sni_context_.Dispose();
    p->sni_context_.Reset(node_isolate, ctx);

    SecureContext* sc = ObjectWrap::Unwrap<SecureContext>(ctx.As<Object>());
    SSL_set_SSL_CTX(s, sc->ctx_);
  }

  return SSL_TLSEXT_ERR_OK;
}
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB


void TLSCallbacks::Initialize(Handle<Object> target) {
  HandleScope scope(node_isolate);

  NODE_SET_METHOD(target, "wrap", TLSCallbacks::Wrap);

  Local<FunctionTemplate> t = FunctionTemplate::New();
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "TLSWrap"));

  NODE_SET_PROTOTYPE_METHOD(t, "start", Start);
  NODE_SET_PROTOTYPE_METHOD(t, "getPeerCertificate", GetPeerCertificate);
  NODE_SET_PROTOTYPE_METHOD(t, "getSession", GetSession);
  NODE_SET_PROTOTYPE_METHOD(t, "setSession", SetSession);
  NODE_SET_PROTOTYPE_METHOD(t, "loadSession", LoadSession);
  NODE_SET_PROTOTYPE_METHOD(t, "endParser", EndParser);
  NODE_SET_PROTOTYPE_METHOD(t, "getCurrentCipher", GetCurrentCipher);
  NODE_SET_PROTOTYPE_METHOD(t, "verifyError", VerifyError);
  NODE_SET_PROTOTYPE_METHOD(t, "setVerifyMode", SetVerifyMode);
  NODE_SET_PROTOTYPE_METHOD(t, "isSessionReused", IsSessionReused);
  NODE_SET_PROTOTYPE_METHOD(t,
                            "enableSessionCallbacks",
                            EnableSessionCallbacks);
  NODE_SET_PROTOTYPE_METHOD(t,
                            "enableHelloParser",
                            EnableHelloParser);

#ifdef OPENSSL_NPN_NEGOTIATED
  NODE_SET_PROTOTYPE_METHOD(t, "getNegotiatedProtocol", GetNegotiatedProto);
  NODE_SET_PROTOTYPE_METHOD(t, "setNPNProtocols", SetNPNProtocols);
#endif  // OPENSSL_NPN_NEGOTIATED

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  NODE_SET_PROTOTYPE_METHOD(t, "getServername", GetServername);
  NODE_SET_PROTOTYPE_METHOD(t, "setServername", SetServername);
#endif  // SSL_CRT_SET_TLSEXT_SERVERNAME_CB

  tlsWrap.Reset(node_isolate, t->GetFunction());

  onread_sym = FIXED_ONE_BYTE_STRING(node_isolate, "onread");
  onerror_sym = FIXED_ONE_BYTE_STRING(node_isolate, "onerror");
  onhandshakestart_sym =
      FIXED_ONE_BYTE_STRING(node_isolate, "onhandshakestart");
  onhandshakedone_sym = FIXED_ONE_BYTE_STRING(node_isolate, "onhandshakedone");
  onclienthello_sym = FIXED_ONE_BYTE_STRING(node_isolate, "onclienthello");
  onnewsession_sym = FIXED_ONE_BYTE_STRING(node_isolate, "onnewsession");

  subject_sym = FIXED_ONE_BYTE_STRING(node_isolate, "subject");
  issuer_sym = FIXED_ONE_BYTE_STRING(node_isolate, "issuer");
  valid_from_sym = FIXED_ONE_BYTE_STRING(node_isolate, "valid_from");
  valid_to_sym = FIXED_ONE_BYTE_STRING(node_isolate, "valid_to");
  subjectaltname_sym = FIXED_ONE_BYTE_STRING(node_isolate, "subjectaltname");
  modulus_sym = FIXED_ONE_BYTE_STRING(node_isolate, "modulus");
  exponent_sym = FIXED_ONE_BYTE_STRING(node_isolate, "exponent");
  fingerprint_sym = FIXED_ONE_BYTE_STRING(node_isolate, "fingerprint");
  name_sym = FIXED_ONE_BYTE_STRING(node_isolate, "name");
  version_sym = FIXED_ONE_BYTE_STRING(node_isolate, "version");
  ext_key_usage_sym = FIXED_ONE_BYTE_STRING(node_isolate, "ext_key_usage");
  sessionid_sym = FIXED_ONE_BYTE_STRING(node_isolate, "sessionId");
  tls_ticket_sym = FIXED_ONE_BYTE_STRING(node_isolate, "tlsTicket");
  servername_sym = FIXED_ONE_BYTE_STRING(node_isolate, "servername");
  sni_context_sym = FIXED_ONE_BYTE_STRING(node_isolate, "sni_context");
}

}  // namespace node

NODE_MODULE(node_tls_wrap, node::TLSCallbacks::Initialize)
