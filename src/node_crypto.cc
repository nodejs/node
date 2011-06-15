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

#include <node_crypto.h>
#include <v8.h>

#include <node.h>
#include <node_buffer.h>
#include <node_root_certs.h>

#include <string.h>
#include <stdlib.h>

#include <errno.h>

#if OPENSSL_VERSION_NUMBER >= 0x10000000L
# define OPENSSL_CONST const
#else
# define OPENSSL_CONST
#endif

#define ASSERT_IS_STRING_OR_BUFFER(val) \
  if (!val->IsString() && !Buffer::HasInstance(val)) { \
    return ThrowException(Exception::TypeError(String::New("Not a string or buffer"))); \
  }

static const char *PUBLIC_KEY_PFX =  "-----BEGIN PUBLIC KEY-----";
static const int PUBLIC_KEY_PFX_LEN = strlen(PUBLIC_KEY_PFX);

namespace node {
namespace crypto {

using namespace v8;

static Persistent<String> errno_symbol;
static Persistent<String> syscall_symbol;
static Persistent<String> subject_symbol;
static Persistent<String> issuer_symbol;
static Persistent<String> valid_from_symbol;
static Persistent<String> valid_to_symbol;
static Persistent<String> fingerprint_symbol;
static Persistent<String> name_symbol;
static Persistent<String> version_symbol;
static Persistent<String> ext_key_usage_symbol;


void SecureContext::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(SecureContext::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("SecureContext"));

  NODE_SET_PROTOTYPE_METHOD(t, "init", SecureContext::Init);
  NODE_SET_PROTOTYPE_METHOD(t, "setKey", SecureContext::SetKey);
  NODE_SET_PROTOTYPE_METHOD(t, "setCert", SecureContext::SetCert);
  NODE_SET_PROTOTYPE_METHOD(t, "addCACert", SecureContext::AddCACert);
  NODE_SET_PROTOTYPE_METHOD(t, "addCRL", SecureContext::AddCRL);
  NODE_SET_PROTOTYPE_METHOD(t, "addRootCerts", SecureContext::AddRootCerts);
  NODE_SET_PROTOTYPE_METHOD(t, "setCiphers", SecureContext::SetCiphers);
  NODE_SET_PROTOTYPE_METHOD(t, "close", SecureContext::Close);

  target->Set(String::NewSymbol("SecureContext"), t->GetFunction());
}


Handle<Value> SecureContext::New(const Arguments& args) {
  HandleScope scope;
  SecureContext *p = new SecureContext();
  p->Wrap(args.Holder());
  return args.This();
}


Handle<Value> SecureContext::Init(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  OPENSSL_CONST SSL_METHOD *method = SSLv23_method();

  if (args.Length() == 1 && args[0]->IsString()) {
    String::Utf8Value sslmethod(args[0]->ToString());

    if (strcmp(*sslmethod, "SSLv2_method") == 0) {
#ifndef OPENSSL_NO_SSL2
      method = SSLv2_method();
#else
      return ThrowException(Exception::Error(String::New("SSLv2 methods disabled")));
#endif
    } else if (strcmp(*sslmethod, "SSLv2_server_method") == 0) {
#ifndef OPENSSL_NO_SSL2
      method = SSLv2_server_method();
#else
      return ThrowException(Exception::Error(String::New("SSLv2 methods disabled")));
#endif
    } else if (strcmp(*sslmethod, "SSLv2_client_method") == 0) {
#ifndef OPENSSL_NO_SSL2
      method = SSLv2_client_method();
#else
      return ThrowException(Exception::Error(String::New("SSLv2 methods disabled")));
#endif
    } else if (strcmp(*sslmethod, "SSLv3_method") == 0) {
      method = SSLv3_method();
    } else if (strcmp(*sslmethod, "SSLv3_server_method") == 0) {
      method = SSLv3_server_method();
    } else if (strcmp(*sslmethod, "SSLv3_client_method") == 0) {
      method = SSLv3_client_method();
    } else if (strcmp(*sslmethod, "SSLv23_method") == 0) {
      method = SSLv23_method();
    } else if (strcmp(*sslmethod, "SSLv23_server_method") == 0) {
      method = SSLv23_server_method();
    } else if (strcmp(*sslmethod, "SSLv23_client_method") == 0) {
      method = SSLv23_client_method();
    } else if (strcmp(*sslmethod, "TLSv1_method") == 0) {
      method = TLSv1_method();
    } else if (strcmp(*sslmethod, "TLSv1_server_method") == 0) {
      method = TLSv1_server_method();
    } else if (strcmp(*sslmethod, "TLSv1_client_method") == 0) {
      method = TLSv1_client_method();
    } else {
      return ThrowException(Exception::Error(String::New("Unknown method")));
    }
  }

  sc->ctx_ = SSL_CTX_new(method);
  // Enable session caching?
  SSL_CTX_set_session_cache_mode(sc->ctx_, SSL_SESS_CACHE_SERVER);
  // SSL_CTX_set_session_cache_mode(sc->ctx_,SSL_SESS_CACHE_OFF);

  sc->ca_store_ = NULL;
  return True();
}


// Takes a string or buffer and loads it into a BIO.
// Caller responsible for BIO_free-ing the returned object.
static BIO* LoadBIO (Handle<Value> v) {
  BIO *bio = BIO_new(BIO_s_mem());
  if (!bio) return NULL;

  HandleScope scope;

  int r;

  if (v->IsString()) {
    String::Utf8Value s(v->ToString());
    r = BIO_write(bio, *s, s.length());
  } else if (Buffer::HasInstance(v)) {
    Local<Object> buffer_obj = v->ToObject();
    char *buffer_data = Buffer::Data(buffer_obj);
    size_t buffer_length = Buffer::Length(buffer_obj);
    r = BIO_write(bio, buffer_data, buffer_length);
  }

  if (r <= 0) {
    BIO_free(bio);
    return NULL;
  }

  return bio;
}


// Takes a string or buffer and loads it into an X509
// Caller responsible for X509_free-ing the returned object.
static X509* LoadX509 (Handle<Value> v) {
  HandleScope scope; // necessary?

  BIO *bio = LoadBIO(v);
  if (!bio) return NULL;

  X509 * x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
  if (!x509) {
    BIO_free(bio);
    return NULL;
  }

  BIO_free(bio);
  return x509;
}


Handle<Value> SecureContext::SetKey(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() != 1) {
    return ThrowException(Exception::TypeError(String::New("Bad parameter")));
  }

  BIO *bio = LoadBIO(args[0]);
  if (!bio) return False();

  EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);

  if (!key) {
    BIO_free(bio);
    return False();
  }

  SSL_CTX_use_PrivateKey(sc->ctx_, key);
  EVP_PKEY_free(key);
  BIO_free(bio);

  return True();
}


// Read a file that contains our certificate in "PEM" format,
// possibly followed by a sequence of CA certificates that should be
// sent to the peer in the Certificate message.
//
// Taken from OpenSSL - editted for style.
int SSL_CTX_use_certificate_chain(SSL_CTX *ctx, BIO *in) {
  int ret = 0;
  X509 *x = NULL;

  x = PEM_read_bio_X509_AUX(in, NULL, NULL, NULL);

  if (x == NULL) {
    SSLerr(SSL_F_SSL_CTX_USE_CERTIFICATE_CHAIN_FILE, ERR_R_PEM_LIB);
    goto end;
  }

  ret = SSL_CTX_use_certificate(ctx, x);

  if (ERR_peek_error() != 0) {
    // Key/certificate mismatch doesn't imply ret==0 ...
    ret = 0;
  }

  if (ret) {
    // If we could set up our certificate, now proceed to
    // the CA certificates.
    X509 *ca;
    int r;
    unsigned long err;

    if (ctx->extra_certs != NULL) {
      sk_X509_pop_free(ctx->extra_certs, X509_free);
      ctx->extra_certs = NULL;
    }

    while ((ca = PEM_read_bio_X509(in, NULL, NULL, NULL))) {
      r = SSL_CTX_add_extra_chain_cert(ctx, ca);

      if (!r) {
        X509_free(ca);
        ret = 0;
        goto end;
      }
      // Note that we must not free r if it was successfully
      // added to the chain (while we must free the main
      // certificate, since its reference count is increased
      // by SSL_CTX_use_certificate).
    }

    // When the while loop ends, it's usually just EOF.
    err = ERR_peek_last_error();
    if (ERR_GET_LIB(err) == ERR_LIB_PEM &&
        ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
      ERR_clear_error();
    } else  {
      // some real error
      ret = 0;
    }
  }

end:
  if (x != NULL) X509_free(x);
  return ret;
}


Handle<Value> SecureContext::SetCert(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() != 1) {
    return ThrowException(Exception::TypeError(
          String::New("Bad parameter")));
  }

  BIO* bio = LoadBIO(args[0]);
  if (!bio) return False();

  int rv = SSL_CTX_use_certificate_chain(sc->ctx_, bio);

  BIO_free(bio);

  if (!rv) {
    unsigned long err = ERR_get_error();
    if (!err) {
      return ThrowException(Exception::Error(
          String::New("SSL_CTX_use_certificate_chain")));
    }
    char string[120];
    ERR_error_string(err, string);
    return ThrowException(Exception::Error(String::New(string)));
  }

  return True();
}


Handle<Value> SecureContext::AddCACert(const Arguments& args) {
  bool newCAStore = false;
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() != 1) {
    return ThrowException(Exception::TypeError(String::New("Bad parameter")));
  }

  if (!sc->ca_store_) {
    sc->ca_store_ = X509_STORE_new();
    newCAStore = true;
  }

  X509* x509 = LoadX509(args[0]);
  if (!x509) return False();

  X509_STORE_add_cert(sc->ca_store_, x509);
  SSL_CTX_add_client_CA(sc->ctx_, x509);

  X509_free(x509);

  if (newCAStore) {
    SSL_CTX_set_cert_store(sc->ctx_, sc->ca_store_);
  }

  return True();
}


Handle<Value> SecureContext::AddCRL(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() != 1) {
    return ThrowException(Exception::TypeError(String::New("Bad parameter")));
  }

  BIO *bio = LoadBIO(args[0]);
  if (!bio) return False();

  X509_CRL *x509 = PEM_read_bio_X509_CRL(bio, NULL, NULL, NULL);

  if (x509 == NULL) {
    BIO_free(bio);
    return False();
  }

  X509_STORE_add_crl(sc->ca_store_, x509);

  X509_STORE_set_flags(sc->ca_store_, X509_V_FLAG_CRL_CHECK |
                                      X509_V_FLAG_CRL_CHECK_ALL);

  BIO_free(bio);
  X509_CRL_free(x509);

  return True();
}



Handle<Value> SecureContext::AddRootCerts(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  assert(sc->ca_store_ == NULL);

  if (!root_cert_store) {
    root_cert_store = X509_STORE_new();

    for (int i = 0; root_certs[i]; i++) {
      BIO *bp = BIO_new(BIO_s_mem());

      if (!BIO_write(bp, root_certs[i], strlen(root_certs[i]))) {
        BIO_free(bp);
        return False();
      }

      X509 *x509 = PEM_read_bio_X509(bp, NULL, NULL, NULL);

      if (x509 == NULL) {
        BIO_free(bp);
        return False();
      }

      X509_STORE_add_cert(root_cert_store, x509);

      BIO_free(bp);
      X509_free(x509);
    }
  }

  sc->ca_store_ = root_cert_store;
  SSL_CTX_set_cert_store(sc->ctx_, sc->ca_store_);

  return True();
}


Handle<Value> SecureContext::SetCiphers(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() != 1 || !args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New("Bad parameter")));
  }

  String::Utf8Value ciphers(args[0]->ToString());
  SSL_CTX_set_cipher_list(sc->ctx_, *ciphers);

  return True();
}


Handle<Value> SecureContext::Close(const Arguments& args) {
  HandleScope scope;
  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());
  sc->FreeCTXMem();
  return False();
}


#ifdef SSL_PRINT_DEBUG
# define DEBUG_PRINT(...) fprintf (stderr, __VA_ARGS__)
#else
# define DEBUG_PRINT(...)
#endif


int Connection::HandleBIOError(BIO *bio, const char* func, int rv) {
  if (rv >= 0) return rv;

  int retry = BIO_should_retry(bio);

  if (BIO_should_write(bio)) {
    DEBUG_PRINT("[%p] BIO: %s want write. should retry %d\n", ssl_, func, retry);
    return 0;

  } else if (BIO_should_read(bio)) {
    DEBUG_PRINT("[%p] BIO: %s want read. should retry %d\n", ssl_, func, retry);
    return 0;

  } else {
   static char ssl_error_buf[512];
    ERR_error_string_n(rv, ssl_error_buf, sizeof(ssl_error_buf));

    HandleScope scope;
    Local<Value> e = Exception::Error(String::New(ssl_error_buf));
    handle_->Set(String::New("error"), e);

    DEBUG_PRINT("[%p] BIO: %s failed: (%d) %s\n", ssl_, func, rv, ssl_error_buf);

    return rv;
  }

  return 0;
}


int Connection::HandleSSLError(const char* func, int rv) {
  if (rv >= 0) return rv;

  int err = SSL_get_error(ssl_, rv);

  if (err == SSL_ERROR_WANT_WRITE) {
    DEBUG_PRINT("[%p] SSL: %s want write\n", ssl_, func);
    return 0;

  } else if (err == SSL_ERROR_WANT_READ) {
    DEBUG_PRINT("[%p] SSL: %s want read\n", ssl_, func);
    return 0;

  } else {
    static char ssl_error_buf[512];
    ERR_error_string_n(err, ssl_error_buf, sizeof(ssl_error_buf));

    HandleScope scope;
    Local<Value> e = Exception::Error(String::New(ssl_error_buf));
    handle_->Set(String::New("error"), e);

    DEBUG_PRINT("[%p] SSL: %s failed: (%d:%d) %s\n", ssl_, func, err, rv, ssl_error_buf);

    return rv;
  }

  return 0;
}


void Connection::ClearError() {
#ifndef NDEBUG
  HandleScope scope;

  // We should clear the error in JS-land
  assert(handle_->Get(String::New("error"))->BooleanValue() == false);
#endif // NDEBUG
}


void Connection::SetShutdownFlags() {
  HandleScope scope;

  int flags = SSL_get_shutdown(ssl_);

  if (flags & SSL_SENT_SHUTDOWN) {
    handle_->Set(String::New("sentShutdown"), True());
  }

  if (flags & SSL_RECEIVED_SHUTDOWN) {
    handle_->Set(String::New("receivedShutdown"), True());
  }
}


void Connection::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Connection::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("Connection"));

  NODE_SET_PROTOTYPE_METHOD(t, "encIn", Connection::EncIn);
  NODE_SET_PROTOTYPE_METHOD(t, "clearOut", Connection::ClearOut);
  NODE_SET_PROTOTYPE_METHOD(t, "clearIn", Connection::ClearIn);
  NODE_SET_PROTOTYPE_METHOD(t, "encOut", Connection::EncOut);
  NODE_SET_PROTOTYPE_METHOD(t, "clearPending", Connection::ClearPending);
  NODE_SET_PROTOTYPE_METHOD(t, "encPending", Connection::EncPending);
  NODE_SET_PROTOTYPE_METHOD(t, "getPeerCertificate", Connection::GetPeerCertificate);
  NODE_SET_PROTOTYPE_METHOD(t, "isInitFinished", Connection::IsInitFinished);
  NODE_SET_PROTOTYPE_METHOD(t, "verifyError", Connection::VerifyError);
  NODE_SET_PROTOTYPE_METHOD(t, "getCurrentCipher", Connection::GetCurrentCipher);
  NODE_SET_PROTOTYPE_METHOD(t, "start", Connection::Start);
  NODE_SET_PROTOTYPE_METHOD(t, "shutdown", Connection::Shutdown);
  NODE_SET_PROTOTYPE_METHOD(t, "receivedShutdown", Connection::ReceivedShutdown);
  NODE_SET_PROTOTYPE_METHOD(t, "close", Connection::Close);

  target->Set(String::NewSymbol("Connection"), t->GetFunction());
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


Handle<Value> Connection::New(const Arguments& args) {
  HandleScope scope;

  Connection *p = new Connection();
  p->Wrap(args.Holder());

  if (args.Length() < 1 || !args[0]->IsObject()) {
    return ThrowException(Exception::Error(String::New(
      "First argument must be a crypto module Credentials")));
  }

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args[0]->ToObject());

  bool is_server = args[1]->BooleanValue();

  p->ssl_ = SSL_new(sc->ctx_);
  p->bio_read_ = BIO_new(BIO_s_mem());
  p->bio_write_ = BIO_new(BIO_s_mem());
  SSL_set_bio(p->ssl_, p->bio_read_, p->bio_write_);

#ifdef SSL_MODE_RELEASE_BUFFERS
  long mode = SSL_get_mode(p->ssl_);
  SSL_set_mode(p->ssl_, mode | SSL_MODE_RELEASE_BUFFERS);
#endif


  int verify_mode;
  if (is_server) {
    bool request_cert = args[2]->BooleanValue();
    if (!request_cert) {
      // Note reject_unauthorized ignored.
      verify_mode = SSL_VERIFY_NONE;
    } else {
      bool reject_unauthorized = args[3]->BooleanValue();
      verify_mode = SSL_VERIFY_PEER;
      if (reject_unauthorized) verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    }
  } else {
    // Note request_cert and reject_unauthorized are ignored for clients.
    verify_mode = SSL_VERIFY_NONE;
  }


  // Always allow a connection. We'll reject in javascript.
  SSL_set_verify(p->ssl_, verify_mode, VerifyCallback);

  if ((p->is_server_ = is_server)) {
    SSL_set_accept_state(p->ssl_);
  } else {
    SSL_set_connect_state(p->ssl_);
  }

  return args.This();
}


Handle<Value> Connection::EncIn(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (args.Length() < 3) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 3 parameters")));
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  Local<Object> buffer_obj = args[0]->ToObject();
  char *buffer_data = Buffer::Data(buffer_obj);
  size_t buffer_length = Buffer::Length(buffer_obj);

  size_t off = args[1]->Int32Value();
  if (off >= buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  int bytes_written = BIO_write(ss->bio_read_, buffer_data + off, len);
  ss->HandleBIOError(ss->bio_read_, "BIO_write", bytes_written);
  ss->SetShutdownFlags();

  return scope.Close(Integer::New(bytes_written));
}


Handle<Value> Connection::ClearOut(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (args.Length() < 3) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 3 parameters")));
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  Local<Object> buffer_obj = args[0]->ToObject();
  char *buffer_data = Buffer::Data(buffer_obj);
  size_t buffer_length = Buffer::Length(buffer_obj);

  size_t off = args[1]->Int32Value();
  if (off >= buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  if (!SSL_is_init_finished(ss->ssl_)) {
    int rv;

    if (ss->is_server_) {
      rv = SSL_accept(ss->ssl_);
      ss->HandleSSLError("SSL_accept:ClearOut", rv);
    } else {
      rv = SSL_connect(ss->ssl_);
      ss->HandleSSLError("SSL_connect:ClearOut", rv);
    }

    if (rv < 0) return scope.Close(Integer::New(rv));
  }

  int bytes_read = SSL_read(ss->ssl_, buffer_data + off, len);
  ss->HandleSSLError("SSL_read:ClearOut", bytes_read);
  ss->SetShutdownFlags();

  return scope.Close(Integer::New(bytes_read));
}


Handle<Value> Connection::ClearPending(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  int bytes_pending = BIO_pending(ss->bio_read_);
  return scope.Close(Integer::New(bytes_pending));
}


Handle<Value> Connection::EncPending(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  int bytes_pending = BIO_pending(ss->bio_write_);
  return scope.Close(Integer::New(bytes_pending));
}


Handle<Value> Connection::EncOut(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (args.Length() < 3) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 3 parameters")));
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  Local<Object> buffer_obj = args[0]->ToObject();
  char *buffer_data = Buffer::Data(buffer_obj);
  size_t buffer_length = Buffer::Length(buffer_obj);

  size_t off = args[1]->Int32Value();
  if (off >= buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  int bytes_read = BIO_read(ss->bio_write_, buffer_data + off, len);

  ss->HandleBIOError(ss->bio_write_, "BIO_read:EncOut", bytes_read);
  ss->SetShutdownFlags();

  return scope.Close(Integer::New(bytes_read));
}


Handle<Value> Connection::ClearIn(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (args.Length() < 3) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 3 parameters")));
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  Local<Object> buffer_obj = args[0]->ToObject();
  char *buffer_data = Buffer::Data(buffer_obj);
  size_t buffer_length = Buffer::Length(buffer_obj);

  size_t off = args[1]->Int32Value();
  if (off > buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  if (!SSL_is_init_finished(ss->ssl_)) {
    int rv;
    if (ss->is_server_) {
      rv = SSL_accept(ss->ssl_);
      ss->HandleSSLError("SSL_accept:ClearIn", rv);
    } else {
      rv = SSL_connect(ss->ssl_);
      ss->HandleSSLError("SSL_connect:ClearIn", rv);
    }

    if (rv < 0) return scope.Close(Integer::New(rv));
  }

  int bytes_written = SSL_write(ss->ssl_, buffer_data + off, len);

  ss->HandleSSLError("SSL_write:ClearIn", bytes_written);
  ss->SetShutdownFlags();

  return scope.Close(Integer::New(bytes_written));
}


Handle<Value> Connection::GetPeerCertificate(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->ssl_ == NULL) return Undefined();
  Local<Object> info = Object::New();
  X509* peer_cert = SSL_get_peer_certificate(ss->ssl_);
  if (peer_cert != NULL) {
    char* subject = X509_NAME_oneline(X509_get_subject_name(peer_cert), 0, 0);
    if (subject != NULL) {
      info->Set(subject_symbol, String::New(subject));
      OPENSSL_free(subject);
    }
    char* issuer = X509_NAME_oneline(X509_get_issuer_name(peer_cert), 0, 0);
    if (subject != NULL) {
      info->Set(issuer_symbol, String::New(issuer));
      OPENSSL_free(issuer);
    }
    char buf[256];
    BIO* bio = BIO_new(BIO_s_mem());
    ASN1_TIME_print(bio, X509_get_notBefore(peer_cert));
    memset(buf, 0, sizeof(buf));
    BIO_read(bio, buf, sizeof(buf) - 1);
    info->Set(valid_from_symbol, String::New(buf));
    ASN1_TIME_print(bio, X509_get_notAfter(peer_cert));
    memset(buf, 0, sizeof(buf));
    BIO_read(bio, buf, sizeof(buf) - 1);
    BIO_free(bio);
    info->Set(valid_to_symbol, String::New(buf));

    unsigned int md_size, i;
    unsigned char md[EVP_MAX_MD_SIZE];
    if (X509_digest(peer_cert, EVP_sha1(), md, &md_size)) {
      const char hex[] = "0123456789ABCDEF";
      char fingerprint[EVP_MAX_MD_SIZE * 3];

      for (i=0; i<md_size; i++) {
        fingerprint[3*i] = hex[(md[i] & 0xf0) >> 4];
        fingerprint[(3*i)+1] = hex[(md[i] & 0x0f)];
        fingerprint[(3*i)+2] = ':';
      }

      if (md_size > 0) {
        fingerprint[(3*(md_size-1))+2] = '\0';
      }
      else {
        fingerprint[0] = '\0';
      }

      info->Set(fingerprint_symbol, String::New(fingerprint));
    }

    STACK_OF(ASN1_OBJECT) *eku = (STACK_OF(ASN1_OBJECT) *)X509_get_ext_d2i(
        peer_cert, NID_ext_key_usage, NULL, NULL);
    if (eku != NULL) {
      Local<Array> ext_key_usage = Array::New();

      for (int i = 0; i < sk_ASN1_OBJECT_num(eku); i++) {
        memset(buf, 0, sizeof(buf));
        OBJ_obj2txt(buf, sizeof(buf) - 1, sk_ASN1_OBJECT_value(eku, i), 1);
        ext_key_usage->Set(Integer::New(i), String::New(buf));
      }

      sk_ASN1_OBJECT_pop_free(eku, ASN1_OBJECT_free);
      info->Set(ext_key_usage_symbol, ext_key_usage);
    }

    X509_free(peer_cert);
  }
  return scope.Close(info);
}

Handle<Value> Connection::Start(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (!SSL_is_init_finished(ss->ssl_)) {
    int rv;
    if (ss->is_server_) {
      rv = SSL_accept(ss->ssl_);
      ss->HandleSSLError("SSL_accept:Start", rv);
    } else {
      rv = SSL_connect(ss->ssl_);
      ss->HandleSSLError("SSL_connect:Start", rv);
    }

    return scope.Close(Integer::New(rv));
  }

  return scope.Close(Integer::New(0));
}


Handle<Value> Connection::Shutdown(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->ssl_ == NULL) return False();
  int rv = SSL_shutdown(ss->ssl_);

  ss->HandleSSLError("SSL_shutdown", rv);
  ss->SetShutdownFlags();

  return scope.Close(Integer::New(rv));
}


Handle<Value> Connection::ReceivedShutdown(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->ssl_ == NULL) return False();
  int r = SSL_get_shutdown(ss->ssl_);

  if (r | SSL_RECEIVED_SHUTDOWN) return True();

  return False();
}


Handle<Value> Connection::IsInitFinished(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->ssl_ == NULL) return False();
  return SSL_is_init_finished(ss->ssl_) ? True() : False();
}


Handle<Value> Connection::VerifyError(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->ssl_ == NULL) return Null();


  // XXX Do this check in JS land?
  X509* peer_cert = SSL_get_peer_certificate(ss->ssl_);
  if (peer_cert == NULL) {
    // We requested a certificate and they did not send us one.
    // Definitely an error.
    // XXX is this the right error message?
    return scope.Close(String::New("UNABLE_TO_GET_ISSUER_CERT"));
  }
  X509_free(peer_cert);


  long x509_verify_error = SSL_get_verify_result(ss->ssl_);

  Local<String> s;

  switch (x509_verify_error) {
    case X509_V_OK:
      return Null();

    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
      s = String::New("UNABLE_TO_GET_ISSUER_CERT");
      break;

    case X509_V_ERR_UNABLE_TO_GET_CRL:
      s = String::New("UNABLE_TO_GET_CRL");
      break;

    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
      s = String::New("UNABLE_TO_DECRYPT_CERT_SIGNATURE");
      break;

    case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
      s = String::New("UNABLE_TO_DECRYPT_CRL_SIGNATURE");
      break;

    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
      s = String::New("UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY");
      break;

    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
      s = String::New("CERT_SIGNATURE_FAILURE");
      break;

    case X509_V_ERR_CRL_SIGNATURE_FAILURE:
      s = String::New("CRL_SIGNATURE_FAILURE");
      break;

    case X509_V_ERR_CERT_NOT_YET_VALID:
      s = String::New("CERT_NOT_YET_VALID");
      break;

    case X509_V_ERR_CERT_HAS_EXPIRED:
      s = String::New("CERT_HAS_EXPIRED");
      break;

    case X509_V_ERR_CRL_NOT_YET_VALID:
      s = String::New("CRL_NOT_YET_VALID");
      break;

    case X509_V_ERR_CRL_HAS_EXPIRED:
      s = String::New("CRL_HAS_EXPIRED");
      break;

    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
      s = String::New("ERROR_IN_CERT_NOT_BEFORE_FIELD");
      break;

    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
      s = String::New("ERROR_IN_CERT_NOT_AFTER_FIELD");
      break;

    case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
      s = String::New("ERROR_IN_CRL_LAST_UPDATE_FIELD");
      break;

    case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
      s = String::New("ERROR_IN_CRL_NEXT_UPDATE_FIELD");
      break;

    case X509_V_ERR_OUT_OF_MEM:
      s = String::New("OUT_OF_MEM");
      break;

    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
      s = String::New("DEPTH_ZERO_SELF_SIGNED_CERT");
      break;

    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
      s = String::New("SELF_SIGNED_CERT_IN_CHAIN");
      break;

    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
      s = String::New("UNABLE_TO_GET_ISSUER_CERT_LOCALLY");
      break;

    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
      s = String::New("UNABLE_TO_VERIFY_LEAF_SIGNATURE");
      break;

    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
      s = String::New("CERT_CHAIN_TOO_LONG");
      break;

    case X509_V_ERR_CERT_REVOKED:
      s = String::New("CERT_REVOKED");
      break;

    case X509_V_ERR_INVALID_CA:
      s = String::New("INVALID_CA");
      break;

    case X509_V_ERR_PATH_LENGTH_EXCEEDED:
      s = String::New("PATH_LENGTH_EXCEEDED");
      break;

    case X509_V_ERR_INVALID_PURPOSE:
      s = String::New("INVALID_PURPOSE");
      break;

    case X509_V_ERR_CERT_UNTRUSTED:
      s = String::New("CERT_UNTRUSTED");
      break;

    case X509_V_ERR_CERT_REJECTED:
      s = String::New("CERT_REJECTED");
      break;

    default:
      s = String::New(X509_verify_cert_error_string(x509_verify_error));
      break;
  }

  return scope.Close(s);
}


Handle<Value> Connection::GetCurrentCipher(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  OPENSSL_CONST SSL_CIPHER *c;

  if ( ss->ssl_ == NULL ) return Undefined();
  c = SSL_get_current_cipher(ss->ssl_);
  if ( c == NULL ) return Undefined();
  Local<Object> info = Object::New();
  const char *cipher_name = SSL_CIPHER_get_name(c);
  info->Set(name_symbol, String::New(cipher_name));
  const char *cipher_version = SSL_CIPHER_get_version(c);
  info->Set(version_symbol, String::New(cipher_version));
  return scope.Close(info);
}

Handle<Value> Connection::Close(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->ssl_ != NULL) {
    SSL_free(ss->ssl_);
    ss->ssl_ = NULL;
  }
  return True();
}


static void HexEncode(unsigned char *md_value,
                      int md_len,
                      char** md_hexdigest,
                      int* md_hex_len) {
  *md_hex_len = (2*(md_len));
  *md_hexdigest = new char[*md_hex_len + 1];
  for (int i = 0; i < md_len; i++) {
    snprintf((char *)(*md_hexdigest + (i*2)), 3, "%02x",  md_value[i]);
  }
}

#define hex2i(c) ((c) <= '9' ? ((c) - '0') : (c) <= 'Z' ? ((c) - 'A' + 10) \
                 : ((c) - 'a' + 10))

static void HexDecode(unsigned char *input,
                      int length,
                      char** buf64,
                      int* buf64_len) {
  *buf64_len = (length/2);
  *buf64 = new char[length/2 + 1];
  char *b = *buf64;
  for(int i = 0; i < length-1; i+=2) {
    b[i/2]  = (hex2i(input[i])<<4) | (hex2i(input[i+1]));
  }
}


void base64(unsigned char *input, int length, char** buf64, int* buf64_len) {
  BIO *b64 = BIO_new(BIO_f_base64());
  BIO *bmem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, bmem);
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  int len = BIO_write(b64, input, length);
  assert(len == length);
  int r = BIO_flush(b64);
  assert(r == 1);

  BUF_MEM *bptr;
  BIO_get_mem_ptr(b64, &bptr);

  *buf64_len = bptr->length;
  *buf64 = new char[*buf64_len+1];
  memcpy(*buf64, bptr->data, *buf64_len);
  char* b = *buf64;
  b[*buf64_len] = 0;

  BIO_free_all(b64);
}


void unbase64(unsigned char *input,
               int length,
               char** buffer,
               int* buffer_len) {
  BIO *b64, *bmem;
  *buffer = new char[length];
  memset(*buffer, 0, length);

  b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  bmem = BIO_new_mem_buf(input, length);
  bmem = BIO_push(b64, bmem);

  *buffer_len = BIO_read(bmem, *buffer, length);
  BIO_free_all(bmem);
}


// LengthWithoutIncompleteUtf8 from V8 d8-posix.cc
// see http://v8.googlecode.com/svn/trunk/src/d8-posix.cc
static int LengthWithoutIncompleteUtf8(char* buffer, int len) {
  int answer = len;
  // 1-byte encoding.
  static const int kUtf8SingleByteMask = 0x80;
  static const int kUtf8SingleByteValue = 0x00;
  // 2-byte encoding.
  static const int kUtf8TwoByteMask = 0xe0;
  static const int kUtf8TwoByteValue = 0xc0;
  // 3-byte encoding.
  static const int kUtf8ThreeByteMask = 0xf0;
  static const int kUtf8ThreeByteValue = 0xe0;
  // 4-byte encoding.
  static const int kUtf8FourByteMask = 0xf8;
  static const int kUtf8FourByteValue = 0xf0;
  // Subsequent bytes of a multi-byte encoding.
  static const int kMultiByteMask = 0xc0;
  static const int kMultiByteValue = 0x80;
  int multi_byte_bytes_seen = 0;
  while (answer > 0) {
    int c = buffer[answer - 1];
    // Ends in valid single-byte sequence?
    if ((c & kUtf8SingleByteMask) == kUtf8SingleByteValue) return answer;
    // Ends in one or more subsequent bytes of a multi-byte value?
    if ((c & kMultiByteMask) == kMultiByteValue) {
      multi_byte_bytes_seen++;
      answer--;
    } else {
      if ((c & kUtf8TwoByteMask) == kUtf8TwoByteValue) {
        if (multi_byte_bytes_seen >= 1) {
          return answer + 2;
        }
        return answer - 1;
      } else if ((c & kUtf8ThreeByteMask) == kUtf8ThreeByteValue) {
        if (multi_byte_bytes_seen >= 2) {
          return answer + 3;
        }
        return answer - 1;
      } else if ((c & kUtf8FourByteMask) == kUtf8FourByteValue) {
        if (multi_byte_bytes_seen >= 3) {
          return answer + 4;
        }
        return answer - 1;
      } else {
        return answer;  // Malformed UTF-8.
      }
    }
  }
  return 0;
}


// local decrypt final without strict padding check
// to work with php mcrypt
// see http://www.mail-archive.com/openssl-dev@openssl.org/msg19927.html
int local_EVP_DecryptFinal_ex(EVP_CIPHER_CTX *ctx,
                              unsigned char *out,
                              int *outl) {
  int i,b;
  int n;

  *outl=0;
  b=ctx->cipher->block_size;

  if (ctx->flags & EVP_CIPH_NO_PADDING) {
    if(ctx->buf_len) {
      EVPerr(EVP_F_EVP_DECRYPTFINAL,EVP_R_DATA_NOT_MULTIPLE_OF_BLOCK_LENGTH);
      return 0;
    }
    *outl = 0;
    return 1;
  }

  if (b > 1) {
    if (ctx->buf_len || !ctx->final_used) {
      EVPerr(EVP_F_EVP_DECRYPTFINAL,EVP_R_WRONG_FINAL_BLOCK_LENGTH);
      return(0);
    }

    if (b > (int)(sizeof(ctx->final) / sizeof(ctx->final[0]))) {
      EVPerr(EVP_F_EVP_DECRYPTFINAL,EVP_R_BAD_DECRYPT);
      return(0);
    }

    n=ctx->final[b-1];

    if (n > b) {
      EVPerr(EVP_F_EVP_DECRYPTFINAL,EVP_R_BAD_DECRYPT);
      return(0);
    }

    for (i=0; i<n; i++) {
      if (ctx->final[--b] != n) {
        EVPerr(EVP_F_EVP_DECRYPTFINAL,EVP_R_BAD_DECRYPT);
        return(0);
      }
    }

    n=ctx->cipher->block_size-n;

    for (i=0; i<n; i++) {
      out[i]=ctx->final[i];
    }
    *outl=n;
  } else {
    *outl=0;
  }

  return(1);
}


class Cipher : public ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "init", CipherInit);
    NODE_SET_PROTOTYPE_METHOD(t, "initiv", CipherInitIv);
    NODE_SET_PROTOTYPE_METHOD(t, "update", CipherUpdate);
    NODE_SET_PROTOTYPE_METHOD(t, "final", CipherFinal);

    target->Set(String::NewSymbol("Cipher"), t->GetFunction());
  }


  bool CipherInit(char* cipherType, char* key_buf, int key_buf_len) {
    cipher = EVP_get_cipherbyname(cipherType);
    if(!cipher) {
      fprintf(stderr, "node-crypto : Unknown cipher %s\n", cipherType);
      return false;
    }

    unsigned char key[EVP_MAX_KEY_LENGTH],iv[EVP_MAX_IV_LENGTH];
    int key_len = EVP_BytesToKey(cipher, EVP_md5(), NULL, (unsigned char*) key_buf, key_buf_len, 1, key, iv);

    EVP_CIPHER_CTX_init(&ctx);
    EVP_CipherInit(&ctx,cipher,(unsigned char *)key,(unsigned char *)iv, true);
    if (!EVP_CIPHER_CTX_set_key_length(&ctx,key_len)) {
      fprintf(stderr, "node-crypto : Invalid key length %d\n", key_len);
      EVP_CIPHER_CTX_cleanup(&ctx);
      return false;
    }
    initialised_ = true;
    return true;
  }


  bool CipherInitIv(char* cipherType,
                    char* key,
                    int key_len,
                    char *iv,
                    int iv_len) {
    cipher = EVP_get_cipherbyname(cipherType);
    if(!cipher) {
      fprintf(stderr, "node-crypto : Unknown cipher %s\n", cipherType);
      return false;
    }
    if (EVP_CIPHER_iv_length(cipher)!=iv_len) {
      fprintf(stderr, "node-crypto : Invalid IV length %d\n", iv_len);
      return false;
    }
    EVP_CIPHER_CTX_init(&ctx);
    EVP_CipherInit(&ctx,cipher,(unsigned char *)key,(unsigned char *)iv, true);
    if (!EVP_CIPHER_CTX_set_key_length(&ctx,key_len)) {
      fprintf(stderr, "node-crypto : Invalid key length %d\n", key_len);
      EVP_CIPHER_CTX_cleanup(&ctx);
      return false;
    }
    initialised_ = true;
    return true;
  }


  int CipherUpdate(char* data, int len, unsigned char** out, int* out_len) {
    if (!initialised_) return 0;
    *out_len=len+EVP_CIPHER_CTX_block_size(&ctx);
    *out= new unsigned char[*out_len];

    EVP_CipherUpdate(&ctx, *out, out_len, (unsigned char*)data, len);
    return 1;
  }

  int CipherFinal(unsigned char** out, int *out_len) {
    if (!initialised_) return 0;
    *out = new unsigned char[EVP_CIPHER_CTX_block_size(&ctx)];
    EVP_CipherFinal(&ctx,*out,out_len);
    EVP_CIPHER_CTX_cleanup(&ctx);
    initialised_ = false;
    return 1;
  }


 protected:

  static Handle<Value> New (const Arguments& args) {
    HandleScope scope;

    Cipher *cipher = new Cipher();
    cipher->Wrap(args.This());
    return args.This();
  }

  static Handle<Value> CipherInit(const Arguments& args) {
    HandleScope scope;

    Cipher *cipher = ObjectWrap::Unwrap<Cipher>(args.This());

    cipher->incomplete_base64=NULL;

    if (args.Length() <= 1 || !args[0]->IsString() || !args[1]->IsString()) {
      return ThrowException(Exception::Error(String::New(
        "Must give cipher-type, key")));
    }

    ASSERT_IS_STRING_OR_BUFFER(args[1]);
    ssize_t key_buf_len = DecodeBytes(args[1], BINARY);

    if (key_buf_len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    char* key_buf = new char[key_buf_len];
    ssize_t key_written = DecodeWrite(key_buf, key_buf_len, args[1], BINARY);
    assert(key_written == key_buf_len);

    String::Utf8Value cipherType(args[0]->ToString());

    bool r = cipher->CipherInit(*cipherType, key_buf, key_buf_len);

    delete [] key_buf;

    if (!r) {
      return ThrowException(Exception::Error(String::New("CipherInit error")));
    }

    return args.This();
  }


  static Handle<Value> CipherInitIv(const Arguments& args) {
    Cipher *cipher = ObjectWrap::Unwrap<Cipher>(args.This());

    HandleScope scope;

    cipher->incomplete_base64=NULL;

    if (args.Length() <= 2 || !args[0]->IsString() || !args[1]->IsString() || !args[2]->IsString()) {
      return ThrowException(Exception::Error(String::New(
        "Must give cipher-type, key, and iv as argument")));
    }

    ASSERT_IS_STRING_OR_BUFFER(args[1]);
    ssize_t key_len = DecodeBytes(args[1], BINARY);

    if (key_len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    ASSERT_IS_STRING_OR_BUFFER(args[2]);
    ssize_t iv_len = DecodeBytes(args[2], BINARY);

    if (iv_len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    char* key_buf = new char[key_len];
    ssize_t key_written = DecodeWrite(key_buf, key_len, args[1], BINARY);
    assert(key_written == key_len);

    char* iv_buf = new char[iv_len];
    ssize_t iv_written = DecodeWrite(iv_buf, iv_len, args[2], BINARY);
    assert(iv_written == iv_len);

    String::Utf8Value cipherType(args[0]->ToString());

    bool r = cipher->CipherInitIv(*cipherType, key_buf,key_len,iv_buf,iv_len);

    delete [] key_buf;
    delete [] iv_buf;

    if (!r) {
      return ThrowException(Exception::Error(String::New("CipherInitIv error")));
    }

    return args.This();
  }

  static Handle<Value> CipherUpdate(const Arguments& args) {
    Cipher *cipher = ObjectWrap::Unwrap<Cipher>(args.This());

    HandleScope scope;

    ASSERT_IS_STRING_OR_BUFFER(args[0]);

    enum encoding enc = ParseEncoding(args[1]);
    ssize_t len = DecodeBytes(args[0], enc);

    if (len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    unsigned char *out=0;
    int out_len=0, r;
    if (Buffer::HasInstance(args[0])) {
      Local<Object> buffer_obj = args[0]->ToObject();
      char *buffer_data = Buffer::Data(buffer_obj);
      size_t buffer_length = Buffer::Length(buffer_obj);

      r = cipher->CipherUpdate(buffer_data, buffer_length, &out, &out_len);
    } else {
      char* buf = new char[len];
      ssize_t written = DecodeWrite(buf, len, args[0], enc);
      assert(written == len);
      r = cipher->CipherUpdate(buf, len,&out,&out_len);
      delete [] buf;
    }

    if (!r) {
      delete [] out;
      Local<Value> exception = Exception::TypeError(String::New("DecipherUpdate fail"));
      return ThrowException(exception);
    }

    Local<Value> outString;
    if (out_len==0) {
      outString=String::New("");
    } else {
      if (args.Length() <= 2 || !args[2]->IsString()) {
        // Binary
        outString = Encode(out, out_len, BINARY);
      } else {
        char* out_hexdigest;
        int out_hex_len;
        String::Utf8Value encoding(args[2]->ToString());
        if (strcasecmp(*encoding, "hex") == 0) {
          // Hex encoding
          HexEncode(out, out_len, &out_hexdigest, &out_hex_len);
          outString = Encode(out_hexdigest, out_hex_len, BINARY);
          delete [] out_hexdigest;
        } else if (strcasecmp(*encoding, "base64") == 0) {
          // Base64 encoding
          // Check to see if we need to add in previous base64 overhang
          if (cipher->incomplete_base64!=NULL){
            unsigned char* complete_base64 = new unsigned char[out_len+cipher->incomplete_base64_len+1];
            memcpy(complete_base64, cipher->incomplete_base64, cipher->incomplete_base64_len);
            memcpy(&complete_base64[cipher->incomplete_base64_len], out, out_len);
            delete [] out;

            delete [] cipher->incomplete_base64;
            cipher->incomplete_base64=NULL;

            out=complete_base64;
            out_len += cipher->incomplete_base64_len;
          }

          // Check to see if we need to trim base64 stream
          if (out_len%3!=0){
            cipher->incomplete_base64_len = out_len%3;
            cipher->incomplete_base64 = new char[cipher->incomplete_base64_len+1];
            memcpy(cipher->incomplete_base64,
                   &out[out_len-cipher->incomplete_base64_len],
                   cipher->incomplete_base64_len);
            out_len -= cipher->incomplete_base64_len;
            out[out_len]=0;
          }

          base64(out, out_len, &out_hexdigest, &out_hex_len);
          outString = Encode(out_hexdigest, out_hex_len, BINARY);
          delete [] out_hexdigest;
        } else if (strcasecmp(*encoding, "binary") == 0) {
          outString = Encode(out, out_len, BINARY);
        } else {
          fprintf(stderr, "node-crypto : Cipher .update encoding "
                          "can be binary, hex or base64\n");
        }
      }
    }

    if (out) delete [] out;

    return scope.Close(outString);
  }

  static Handle<Value> CipherFinal(const Arguments& args) {
    Cipher *cipher = ObjectWrap::Unwrap<Cipher>(args.This());

    HandleScope scope;

    unsigned char* out_value;
    int out_len;
    char* out_hexdigest;
    int out_hex_len;
    Local<Value> outString ;

    int r = cipher->CipherFinal(&out_value, &out_len);

    if (out_len == 0 || r == 0) {
      return scope.Close(String::New(""));
    }

    if (args.Length() == 0 || !args[0]->IsString()) {
      // Binary
      outString = Encode(out_value, out_len, BINARY);
    } else {
      String::Utf8Value encoding(args[0]->ToString());
      if (strcasecmp(*encoding, "hex") == 0) {
        // Hex encoding
        HexEncode(out_value, out_len, &out_hexdigest, &out_hex_len);
        outString = Encode(out_hexdigest, out_hex_len, BINARY);
        delete [] out_hexdigest;
      } else if (strcasecmp(*encoding, "base64") == 0) {
        base64(out_value, out_len, &out_hexdigest, &out_hex_len);
        outString = Encode(out_hexdigest, out_hex_len, BINARY);
        delete [] out_hexdigest;
      } else if (strcasecmp(*encoding, "binary") == 0) {
        outString = Encode(out_value, out_len, BINARY);
      } else {
        fprintf(stderr, "node-crypto : Cipher .final encoding "
                        "can be binary, hex or base64\n");
      }
    }
    delete [] out_value;
    return scope.Close(outString);
  }

  Cipher () : ObjectWrap ()
  {
    initialised_ = false;
  }

  ~Cipher () {
    if (initialised_) {
      EVP_CIPHER_CTX_cleanup(&ctx);
    }
  }

 private:

  EVP_CIPHER_CTX ctx; /* coverity[member_decl] */
  const EVP_CIPHER *cipher; /* coverity[member_decl] */
  bool initialised_;
  char* incomplete_base64; /* coverity[member_decl] */
  int incomplete_base64_len; /* coverity[member_decl] */

};



class Decipher : public ObjectWrap {
 public:
  static void
  Initialize (v8::Handle<v8::Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "init", DecipherInit);
    NODE_SET_PROTOTYPE_METHOD(t, "initiv", DecipherInitIv);
    NODE_SET_PROTOTYPE_METHOD(t, "update", DecipherUpdate);
    NODE_SET_PROTOTYPE_METHOD(t, "final", DecipherFinal);
    NODE_SET_PROTOTYPE_METHOD(t, "finaltol", DecipherFinalTolerate);

    target->Set(String::NewSymbol("Decipher"), t->GetFunction());
  }

  bool DecipherInit(char* cipherType, char* key_buf, int key_buf_len) {
    cipher_ = EVP_get_cipherbyname(cipherType);

    if(!cipher_) {
      fprintf(stderr, "node-crypto : Unknown cipher %s\n", cipherType);
      return false;
    }

    unsigned char key[EVP_MAX_KEY_LENGTH],iv[EVP_MAX_IV_LENGTH];
    int key_len = EVP_BytesToKey(cipher_,
                                 EVP_md5(),
                                 NULL,
                                 (unsigned char*)(key_buf),
                                 key_buf_len,
                                 1,
                                 key,
                                 iv);

    EVP_CIPHER_CTX_init(&ctx);
    EVP_CipherInit(&ctx,
                   cipher_,
                   (unsigned char*)(key),
                   (unsigned char *)(iv),
                   false);
    if (!EVP_CIPHER_CTX_set_key_length(&ctx,key_len)) {
      fprintf(stderr, "node-crypto : Invalid key length %d\n", key_len);
      EVP_CIPHER_CTX_cleanup(&ctx);
      return false;
    }
    initialised_ = true;
    return true;
  }


  bool DecipherInitIv(char* cipherType,
                      char* key,
                      int key_len,
                      char *iv,
                      int iv_len) {
    cipher_ = EVP_get_cipherbyname(cipherType);
    if(!cipher_) {
      fprintf(stderr, "node-crypto : Unknown cipher %s\n", cipherType);
      return false;
    }
    if (EVP_CIPHER_iv_length(cipher_) != iv_len) {
      fprintf(stderr, "node-crypto : Invalid IV length %d\n", iv_len);
      return false;
    }
    EVP_CIPHER_CTX_init(&ctx);
    EVP_CipherInit(&ctx,
                   cipher_,
                   (unsigned char*)(key),
                   (unsigned char *)(iv),
                   false);
    if (!EVP_CIPHER_CTX_set_key_length(&ctx,key_len)) {
      fprintf(stderr, "node-crypto : Invalid key length %d\n", key_len);
      EVP_CIPHER_CTX_cleanup(&ctx);
      return false;
    }
    initialised_ = true;
    return true;
  }

  int DecipherUpdate(char* data, int len, unsigned char** out, int* out_len) {
    if (!initialised_) return 0;
    *out_len=len+EVP_CIPHER_CTX_block_size(&ctx);
    *out= new unsigned char[*out_len];

    EVP_CipherUpdate(&ctx, *out, out_len, (unsigned char*)data, len);
    return 1;
  }

  // coverity[alloc_arg]
  int DecipherFinal(unsigned char** out, int *out_len, bool tolerate_padding) {
    if (!initialised_) return 0;
    *out = new unsigned char[EVP_CIPHER_CTX_block_size(&ctx)];
    if (tolerate_padding) {
      local_EVP_DecryptFinal_ex(&ctx,*out,out_len);
    } else {
      EVP_CipherFinal(&ctx,*out,out_len);
    }
    EVP_CIPHER_CTX_cleanup(&ctx);
    initialised_ = false;
    return 1;
  }


 protected:

  static Handle<Value> New (const Arguments& args) {
    HandleScope scope;

    Decipher *cipher = new Decipher();
    cipher->Wrap(args.This());
    return args.This();
  }

  static Handle<Value> DecipherInit(const Arguments& args) {
    Decipher *cipher = ObjectWrap::Unwrap<Decipher>(args.This());

    HandleScope scope;

    cipher->incomplete_utf8=NULL;
    cipher->incomplete_hex_flag=false;

    if (args.Length() <= 1 || !args[0]->IsString() || !args[1]->IsString()) {
      return ThrowException(Exception::Error(String::New(
        "Must give cipher-type, key as argument")));
    }

    ASSERT_IS_STRING_OR_BUFFER(args[1]);
    ssize_t key_len = DecodeBytes(args[1], BINARY);

    if (key_len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    char* key_buf = new char[key_len];
    ssize_t key_written = DecodeWrite(key_buf, key_len, args[1], BINARY);
    assert(key_written == key_len);

    String::Utf8Value cipherType(args[0]->ToString());

    bool r = cipher->DecipherInit(*cipherType, key_buf,key_len);

    delete [] key_buf;

    if (!r) {
      return ThrowException(Exception::Error(String::New("DecipherInit error")));
    }

    return args.This();
  }

  static Handle<Value> DecipherInitIv(const Arguments& args) {
    Decipher *cipher = ObjectWrap::Unwrap<Decipher>(args.This());

    HandleScope scope;

    cipher->incomplete_utf8=NULL;
    cipher->incomplete_hex_flag=false;

    if (args.Length() <= 2 || !args[0]->IsString() || !args[1]->IsString() || !args[2]->IsString()) {
      return ThrowException(Exception::Error(String::New(
        "Must give cipher-type, key, and iv as argument")));
    }

    ASSERT_IS_STRING_OR_BUFFER(args[1]);
    ssize_t key_len = DecodeBytes(args[1], BINARY);

    if (key_len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    ASSERT_IS_STRING_OR_BUFFER(args[2]);
    ssize_t iv_len = DecodeBytes(args[2], BINARY);

    if (iv_len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    char* key_buf = new char[key_len];
    ssize_t key_written = DecodeWrite(key_buf, key_len, args[1], BINARY);
    assert(key_written == key_len);

    char* iv_buf = new char[iv_len];
    ssize_t iv_written = DecodeWrite(iv_buf, iv_len, args[2], BINARY);
    assert(iv_written == iv_len);

    String::Utf8Value cipherType(args[0]->ToString());

    bool r = cipher->DecipherInitIv(*cipherType, key_buf,key_len,iv_buf,iv_len);

    delete [] key_buf;
    delete [] iv_buf;

    if (!r) {
      return ThrowException(Exception::Error(String::New("DecipherInitIv error")));
    }

    return args.This();
  }

  static Handle<Value> DecipherUpdate(const Arguments& args) {
    HandleScope scope;

    Decipher *cipher = ObjectWrap::Unwrap<Decipher>(args.This());

    ASSERT_IS_STRING_OR_BUFFER(args[0]);

    ssize_t len = DecodeBytes(args[0], BINARY);
    if (len < 0) {
        return ThrowException(Exception::Error(String::New(
            "node`DecodeBytes() failed")));
    }

    char* buf;
    // if alloc_buf then buf must be deleted later
    bool alloc_buf = false;
    if (Buffer::HasInstance(args[0])) {
      Local<Object> buffer_obj = args[0]->ToObject();
      char *buffer_data = Buffer::Data(buffer_obj);
      size_t buffer_length = Buffer::Length(buffer_obj);

      buf = buffer_data;
      len = buffer_length;
    } else {
      alloc_buf = true;
      buf = new char[len];
      ssize_t written = DecodeWrite(buf, len, args[0], BINARY);
      assert(written == len);
    }

    char* ciphertext;
    int ciphertext_len;

    if (args.Length() <= 1 || !args[1]->IsString()) {
      // Binary - do nothing
    } else {
      String::Utf8Value encoding(args[1]->ToString());
      if (strcasecmp(*encoding, "hex") == 0) {
        // Hex encoding
        // Do we have a previous hex carry over?
        if (cipher->incomplete_hex_flag) {
          char* complete_hex = new char[len+2];
          memcpy(complete_hex, &cipher->incomplete_hex, 1);
          memcpy(complete_hex+1, buf, len);
          if (alloc_buf) {
            delete [] buf;
            alloc_buf = false;
          }
          buf = complete_hex;
          len += 1;
        }
        // Do we have an incomplete hex stream?
        if ((len>0) && (len % 2 !=0)) {
          len--;
          cipher->incomplete_hex=buf[len];
          cipher->incomplete_hex_flag=true;
          buf[len]=0;
        }
        HexDecode((unsigned char*)buf, len, (char **)&ciphertext, &ciphertext_len);

        if (alloc_buf) {
          delete [] buf;
        }
        buf = ciphertext;
        len = ciphertext_len;
        alloc_buf = true;

      } else if (strcasecmp(*encoding, "base64") == 0) {
        unbase64((unsigned char*)buf, len, (char **)&ciphertext, &ciphertext_len);
        if (alloc_buf) {
          delete [] buf;
        }
        buf = ciphertext;
        len = ciphertext_len;
        alloc_buf = true;

      } else if (strcasecmp(*encoding, "binary") == 0) {
        // Binary - do nothing

      } else {
        fprintf(stderr, "node-crypto : Decipher .update encoding "
                        "can be binary, hex or base64\n");
      }
    }

    unsigned char *out=0;
    int out_len=0;
    int r = cipher->DecipherUpdate(buf, len, &out, &out_len);

    if (!r) {
      delete [] out;
      Local<Value> exception = Exception::TypeError(String::New("DecipherUpdate fail"));
      return ThrowException(exception);
    }

    Local<Value> outString;
    if (out_len==0) {
      outString=String::New("");
    } else if (args.Length() <= 2 || !args[2]->IsString()) {
      outString = Encode(out, out_len, BINARY);
    } else {
      enum encoding enc = ParseEncoding(args[2]);
      if (enc == UTF8) {
        // See if we have any overhang from last utf8 partial ending
        if (cipher->incomplete_utf8!=NULL) {
          char* complete_out = new char[cipher->incomplete_utf8_len + out_len];
          memcpy(complete_out, cipher->incomplete_utf8, cipher->incomplete_utf8_len);
          memcpy((char *)complete_out+cipher->incomplete_utf8_len, out, out_len);
          delete [] out;

          delete [] cipher->incomplete_utf8;
          cipher->incomplete_utf8 = NULL;

          out = (unsigned char*)complete_out;
          out_len += cipher->incomplete_utf8_len;
        }
        // Check to see if we have a complete utf8 stream
        int utf8_len = LengthWithoutIncompleteUtf8((char *)out, out_len);
        if (utf8_len<out_len) { // We have an incomplete ut8 ending
          cipher->incomplete_utf8_len = out_len-utf8_len;
          cipher->incomplete_utf8 = new unsigned char[cipher->incomplete_utf8_len+1];
          memcpy(cipher->incomplete_utf8, &out[utf8_len], cipher->incomplete_utf8_len);
        }
        outString = Encode(out, utf8_len, enc);
      } else {
        outString = Encode(out, out_len, enc);
      }
    }

    if (out) delete [] out;

    if (alloc_buf) delete [] buf;
    return scope.Close(outString);

  }

  static Handle<Value> DecipherFinal(const Arguments& args) {
    HandleScope scope;

    Decipher *cipher = ObjectWrap::Unwrap<Decipher>(args.This());

    unsigned char* out_value;
    int out_len;
    Local<Value> outString;

    int r = cipher->DecipherFinal(&out_value, &out_len, false);

    if (out_len == 0 || r == 0) {
      return scope.Close(String::New(""));
    }


    if (args.Length() == 0 || !args[0]->IsString()) {
      outString = Encode(out_value, out_len, BINARY);
    } else {
      enum encoding enc = ParseEncoding(args[0]);
      if (enc == UTF8) {
        // See if we have any overhang from last utf8 partial ending
        if (cipher->incomplete_utf8!=NULL) {
          char* complete_out = new char[cipher->incomplete_utf8_len + out_len];
          memcpy(complete_out, cipher->incomplete_utf8, cipher->incomplete_utf8_len);
          memcpy((char *)complete_out+cipher->incomplete_utf8_len, out_value, out_len);

          delete [] cipher->incomplete_utf8;
          cipher->incomplete_utf8=NULL;

          outString = Encode(complete_out, cipher->incomplete_utf8_len+out_len, enc);
          delete [] complete_out;
        } else {
          outString = Encode(out_value, out_len, enc);
        }
      } else {
        outString = Encode(out_value, out_len, enc);
      }
    }
    delete [] out_value;
    return scope.Close(outString);
  }

  static Handle<Value> DecipherFinalTolerate(const Arguments& args) {
    Decipher *cipher = ObjectWrap::Unwrap<Decipher>(args.This());

    HandleScope scope;

    unsigned char* out_value;
    int out_len;
    Local<Value> outString ;

    int r = cipher->DecipherFinal(&out_value, &out_len, true);

    if (out_len == 0 || r == 0) {
      delete [] out_value;
      return scope.Close(String::New(""));
    }


    if (args.Length() == 0 || !args[0]->IsString()) {
      outString = Encode(out_value, out_len, BINARY);
    } else {
      enum encoding enc = ParseEncoding(args[0]);
      if (enc == UTF8) {
        // See if we have any overhang from last utf8 partial ending
        if (cipher->incomplete_utf8!=NULL) {
          char* complete_out = new char[cipher->incomplete_utf8_len + out_len];
          memcpy(complete_out, cipher->incomplete_utf8, cipher->incomplete_utf8_len);
          memcpy((char *)complete_out+cipher->incomplete_utf8_len, out_value, out_len);

          delete [] cipher->incomplete_utf8;
          cipher->incomplete_utf8 = NULL;

          outString = Encode(complete_out, cipher->incomplete_utf8_len+out_len, enc);
          delete [] complete_out;
        } else {
          outString = Encode(out_value, out_len, enc);
        }
      } else {
        outString = Encode(out_value, out_len, enc);
      }
    }
    delete [] out_value;
    return scope.Close(outString);
  }

  Decipher () : ObjectWrap () {
    initialised_ = false;
  }

  ~Decipher () {
    if (initialised_) {
      EVP_CIPHER_CTX_cleanup(&ctx);
    }
  }

 private:

  EVP_CIPHER_CTX ctx;
  const EVP_CIPHER *cipher_;
  bool initialised_;
  unsigned char* incomplete_utf8;
  int incomplete_utf8_len;
  char incomplete_hex;
  bool incomplete_hex_flag;
};




class Hmac : public ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "init", HmacInit);
    NODE_SET_PROTOTYPE_METHOD(t, "update", HmacUpdate);
    NODE_SET_PROTOTYPE_METHOD(t, "digest", HmacDigest);

    target->Set(String::NewSymbol("Hmac"), t->GetFunction());
  }

  bool HmacInit(char* hashType, char* key, int key_len) {
    md = EVP_get_digestbyname(hashType);
    if(!md) {
      fprintf(stderr, "node-crypto : Unknown message digest %s\n", hashType);
      return false;
    }
    HMAC_CTX_init(&ctx);
    HMAC_Init(&ctx, key, key_len, md);
    initialised_ = true;
    return true;

  }

  int HmacUpdate(char* data, int len) {
    if (!initialised_) return 0;
    HMAC_Update(&ctx, (unsigned char*)data, len);
    return 1;
  }

  int HmacDigest(unsigned char** md_value, unsigned int *md_len) {
    if (!initialised_) return 0;
    *md_value = new unsigned char[EVP_MAX_MD_SIZE];
    HMAC_Final(&ctx, *md_value, md_len);
    HMAC_CTX_cleanup(&ctx);
    initialised_ = false;
    return 1;
  }


 protected:

  static Handle<Value> New (const Arguments& args) {
    HandleScope scope;

    Hmac *hmac = new Hmac();
    hmac->Wrap(args.This());
    return args.This();
  }

  static Handle<Value> HmacInit(const Arguments& args) {
    Hmac *hmac = ObjectWrap::Unwrap<Hmac>(args.This());

    HandleScope scope;

    if (args.Length() == 0 || !args[0]->IsString()) {
      return ThrowException(Exception::Error(String::New(
        "Must give hashtype string as argument")));
    }

    ASSERT_IS_STRING_OR_BUFFER(args[1]);
    ssize_t len = DecodeBytes(args[1], BINARY);

    if (len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    char* buf = new char[len];
    ssize_t written = DecodeWrite(buf, len, args[1], BINARY);
    assert(written == len);

    String::Utf8Value hashType(args[0]->ToString());

    bool r = hmac->HmacInit(*hashType, buf, len);

    delete [] buf;

    if (!r) {
      return ThrowException(Exception::Error(String::New("hmac error")));
    }

    return args.This();
  }

  static Handle<Value> HmacUpdate(const Arguments& args) {
    Hmac *hmac = ObjectWrap::Unwrap<Hmac>(args.This());

    HandleScope scope;

    ASSERT_IS_STRING_OR_BUFFER(args[0]);
    enum encoding enc = ParseEncoding(args[1]);
    ssize_t len = DecodeBytes(args[0], enc);

    if (len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    int r;

    if( Buffer::HasInstance(args[0])) {
      Local<Object> buffer_obj = args[0]->ToObject();
      char *buffer_data = Buffer::Data(buffer_obj);
      size_t buffer_length = Buffer::Length(buffer_obj);

      r = hmac->HmacUpdate(buffer_data, buffer_length);
    } else {
      char* buf = new char[len];
      ssize_t written = DecodeWrite(buf, len, args[0], enc);
      assert(written == len);
      r = hmac->HmacUpdate(buf, len);
      delete [] buf;
    }

    if (!r) {
      Local<Value> exception = Exception::TypeError(String::New("HmacUpdate fail"));
      return ThrowException(exception);
    }

    return args.This();
  }

  static Handle<Value> HmacDigest(const Arguments& args) {
    Hmac *hmac = ObjectWrap::Unwrap<Hmac>(args.This());

    HandleScope scope;

    unsigned char* md_value;
    unsigned int md_len;
    char* md_hexdigest;
    int md_hex_len;
    Local<Value> outString ;

    int r = hmac->HmacDigest(&md_value, &md_len);

    if (md_len == 0 || r == 0) {
      return scope.Close(String::New(""));
    }

    if (args.Length() == 0 || !args[0]->IsString()) {
      // Binary
      outString = Encode(md_value, md_len, BINARY);
    } else {
      String::Utf8Value encoding(args[0]->ToString());
      if (strcasecmp(*encoding, "hex") == 0) {
        // Hex encoding
        HexEncode(md_value, md_len, &md_hexdigest, &md_hex_len);
        outString = Encode(md_hexdigest, md_hex_len, BINARY);
        delete [] md_hexdigest;
      } else if (strcasecmp(*encoding, "base64") == 0) {
        base64(md_value, md_len, &md_hexdigest, &md_hex_len);
        outString = Encode(md_hexdigest, md_hex_len, BINARY);
        delete [] md_hexdigest;
      } else if (strcasecmp(*encoding, "binary") == 0) {
        outString = Encode(md_value, md_len, BINARY);
      } else {
        fprintf(stderr, "node-crypto : Hmac .digest encoding "
                        "can be binary, hex or base64\n");
      }
    }
    delete [] md_value;
    return scope.Close(outString);
  }

  Hmac () : ObjectWrap () {
    initialised_ = false;
  }

  ~Hmac () {
    if (initialised_) {
      HMAC_CTX_cleanup(&ctx);
    }
  }

 private:

  HMAC_CTX ctx; /* coverity[member_decl] */
  const EVP_MD *md; /* coverity[member_decl] */
  bool initialised_;
};


class Hash : public ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "update", HashUpdate);
    NODE_SET_PROTOTYPE_METHOD(t, "digest", HashDigest);

    target->Set(String::NewSymbol("Hash"), t->GetFunction());
  }

  bool HashInit (const char* hashType) {
    md = EVP_get_digestbyname(hashType);
    if(!md) {
      fprintf(stderr, "node-crypto : Unknown message digest %s\n", hashType);
      return false;
    }
    EVP_MD_CTX_init(&mdctx);
    EVP_DigestInit_ex(&mdctx, md, NULL);
    initialised_ = true;
    return true;
  }

  int HashUpdate(char* data, int len) {
    if (!initialised_) return 0;
    EVP_DigestUpdate(&mdctx, data, len);
    return 1;
  }


 protected:

  static Handle<Value> New (const Arguments& args) {
    HandleScope scope;

    if (args.Length() == 0 || !args[0]->IsString()) {
      return ThrowException(Exception::Error(String::New(
        "Must give hashtype string as argument")));
    }

    Hash *hash = new Hash();
    hash->Wrap(args.This());

    String::Utf8Value hashType(args[0]->ToString());

    hash->HashInit(*hashType);

    return args.This();
  }

  static Handle<Value> HashUpdate(const Arguments& args) {
    HandleScope scope;

    Hash *hash = ObjectWrap::Unwrap<Hash>(args.This());

    ASSERT_IS_STRING_OR_BUFFER(args[0]);
    enum encoding enc = ParseEncoding(args[1]);
    ssize_t len = DecodeBytes(args[0], enc);

    if (len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    int r;

    if (Buffer::HasInstance(args[0])) {
      Local<Object> buffer_obj = args[0]->ToObject();
      char *buffer_data = Buffer::Data(buffer_obj);
      size_t buffer_length = Buffer::Length(buffer_obj);
      r = hash->HashUpdate(buffer_data, buffer_length);
    } else {
      char* buf = new char[len];
      ssize_t written = DecodeWrite(buf, len, args[0], enc);
      assert(written == len);
      r = hash->HashUpdate(buf, len);
      delete[] buf;
    }

    if (!r) {
      Local<Value> exception = Exception::TypeError(String::New("HashUpdate fail"));
      return ThrowException(exception);
    }

    return args.This();
  }

  static Handle<Value> HashDigest(const Arguments& args) {
    HandleScope scope;

    Hash *hash = ObjectWrap::Unwrap<Hash>(args.This());

    if (!hash->initialised_) {
      return ThrowException(Exception::Error(String::New("Not initialized")));
    }

    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    EVP_DigestFinal_ex(&hash->mdctx, md_value, &md_len);
    EVP_MD_CTX_cleanup(&hash->mdctx);
    hash->initialised_ = false;

    if (md_len == 0) {
      return scope.Close(String::New(""));
    }

    Local<Value> outString;

    if (args.Length() == 0 || !args[0]->IsString()) {
      // Binary
      outString = Encode(md_value, md_len, BINARY);
    } else {
      String::Utf8Value encoding(args[0]->ToString());
      if (strcasecmp(*encoding, "hex") == 0) {
        // Hex encoding
        char* md_hexdigest;
        int md_hex_len;
        HexEncode(md_value, md_len, &md_hexdigest, &md_hex_len);
        outString = Encode(md_hexdigest, md_hex_len, BINARY);
        delete [] md_hexdigest;
      } else if (strcasecmp(*encoding, "base64") == 0) {
        char* md_hexdigest;
        int md_hex_len;
        base64(md_value, md_len, &md_hexdigest, &md_hex_len);
        outString = Encode(md_hexdigest, md_hex_len, BINARY);
        delete [] md_hexdigest;
      } else if (strcasecmp(*encoding, "binary") == 0) {
        outString = Encode(md_value, md_len, BINARY);
      } else {
        fprintf(stderr, "node-crypto : Hash .digest encoding "
                        "can be binary, hex or base64\n");
      }
    }

    return scope.Close(outString);
  }

  Hash () : ObjectWrap () {
    initialised_ = false;
  }

  ~Hash () {
    if (initialised_) {
      EVP_MD_CTX_cleanup(&mdctx);
    }
  }

 private:

  EVP_MD_CTX mdctx; /* coverity[member_decl] */
  const EVP_MD *md; /* coverity[member_decl] */
  bool initialised_;
};

class Sign : public ObjectWrap {
 public:
  static void
  Initialize (v8::Handle<v8::Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "init", SignInit);
    NODE_SET_PROTOTYPE_METHOD(t, "update", SignUpdate);
    NODE_SET_PROTOTYPE_METHOD(t, "sign", SignFinal);

    target->Set(String::NewSymbol("Sign"), t->GetFunction());
  }

  bool SignInit (const char* signType) {
    md = EVP_get_digestbyname(signType);
    if(!md) {
      printf("Unknown message digest %s\n", signType);
      return false;
    }
    EVP_MD_CTX_init(&mdctx);
    EVP_SignInit_ex(&mdctx, md, NULL);
    initialised_ = true;
    return true;

  }

  int SignUpdate(char* data, int len) {
    if (!initialised_) return 0;
    EVP_SignUpdate(&mdctx, data, len);
    return 1;
  }

  int SignFinal(unsigned char** md_value,
                unsigned int *md_len,
                char* key_pem,
                int key_pemLen) {
    if (!initialised_) return 0;

    BIO *bp = NULL;
    EVP_PKEY* pkey;
    bp = BIO_new(BIO_s_mem());
    if(!BIO_write(bp, key_pem, key_pemLen)) return 0;

    pkey = PEM_read_bio_PrivateKey( bp, NULL, NULL, NULL );
    if (pkey == NULL) return 0;

    EVP_SignFinal(&mdctx, *md_value, md_len, pkey);
    EVP_MD_CTX_cleanup(&mdctx);
    initialised_ = false;
    EVP_PKEY_free(pkey);
    BIO_free(bp);
    return 1;
  }


 protected:

  static Handle<Value> New (const Arguments& args) {
    HandleScope scope;

    Sign *sign = new Sign();
    sign->Wrap(args.This());

    return args.This();
  }

  static Handle<Value> SignInit(const Arguments& args) {
    HandleScope scope;

    Sign *sign = ObjectWrap::Unwrap<Sign>(args.This());

    if (args.Length() == 0 || !args[0]->IsString()) {
      return ThrowException(Exception::Error(String::New(
        "Must give signtype string as argument")));
    }

    String::Utf8Value signType(args[0]->ToString());

    bool r = sign->SignInit(*signType);

    if (!r) {
      return ThrowException(Exception::Error(String::New("SignInit error")));
    }

    return args.This();
  }

  static Handle<Value> SignUpdate(const Arguments& args) {
    Sign *sign = ObjectWrap::Unwrap<Sign>(args.This());

    HandleScope scope;

    ASSERT_IS_STRING_OR_BUFFER(args[0]);
    enum encoding enc = ParseEncoding(args[1]);
    ssize_t len = DecodeBytes(args[0], enc);

    if (len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    int r;

    if (Buffer::HasInstance(args[0])) {
      Local<Object> buffer_obj = args[0]->ToObject();
      char *buffer_data = Buffer::Data(buffer_obj);
      size_t buffer_length = Buffer::Length(buffer_obj);

      r = sign->SignUpdate(buffer_data, buffer_length);
    } else {
      char* buf = new char[len];
      ssize_t written = DecodeWrite(buf, len, args[0], enc);
      assert(written == len);
      r = sign->SignUpdate(buf, len);
      delete [] buf;
    }

    if (!r) {
      Local<Value> exception = Exception::TypeError(String::New("SignUpdate fail"));
      return ThrowException(exception);
    }

    return args.This();
  }

  static Handle<Value> SignFinal(const Arguments& args) {
    Sign *sign = ObjectWrap::Unwrap<Sign>(args.This());

    HandleScope scope;

    unsigned char* md_value;
    unsigned int md_len;
    char* md_hexdigest;
    int md_hex_len;
    Local<Value> outString;

    md_len = 8192; // Maximum key size is 8192 bits
    md_value = new unsigned char[md_len];

    ASSERT_IS_STRING_OR_BUFFER(args[0]);
    ssize_t len = DecodeBytes(args[0], BINARY);

    if (len < 0) {
      delete [] md_value;
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    char* buf = new char[len];
    ssize_t written = DecodeWrite(buf, len, args[0], BINARY);
    assert(written == len);

    int r = sign->SignFinal(&md_value, &md_len, buf, len);

    delete [] buf;

    if (md_len == 0 || r == 0) {
      delete [] md_value;
      return scope.Close(String::New(""));
    }

    if (args.Length() == 1 || !args[1]->IsString()) {
      // Binary
      outString = Encode(md_value, md_len, BINARY);
    } else {
      String::Utf8Value encoding(args[1]->ToString());
      if (strcasecmp(*encoding, "hex") == 0) {
        // Hex encoding
        HexEncode(md_value, md_len, &md_hexdigest, &md_hex_len);
        outString = Encode(md_hexdigest, md_hex_len, BINARY);
        delete [] md_hexdigest;
      } else if (strcasecmp(*encoding, "base64") == 0) {
        base64(md_value, md_len, &md_hexdigest, &md_hex_len);
        outString = Encode(md_hexdigest, md_hex_len, BINARY);
        delete [] md_hexdigest;
      } else if (strcasecmp(*encoding, "binary") == 0) {
        outString = Encode(md_value, md_len, BINARY);
      } else {
        outString = String::New("");
        fprintf(stderr, "node-crypto : Sign .sign encoding "
                        "can be binary, hex or base64\n");
      }
    }

    delete [] md_value;
    return scope.Close(outString);
  }

  Sign () : ObjectWrap () {
    initialised_ = false;
  }

  ~Sign () {
    if (initialised_) {
      EVP_MD_CTX_cleanup(&mdctx);
    }
  }

 private:

  EVP_MD_CTX mdctx; /* coverity[member_decl] */
  const EVP_MD *md; /* coverity[member_decl] */
  bool initialised_;
};

class Verify : public ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "init", VerifyInit);
    NODE_SET_PROTOTYPE_METHOD(t, "update", VerifyUpdate);
    NODE_SET_PROTOTYPE_METHOD(t, "verify", VerifyFinal);

    target->Set(String::NewSymbol("Verify"), t->GetFunction());
  }


  bool VerifyInit (const char* verifyType) {
    md = EVP_get_digestbyname(verifyType);
    if(!md) {
      fprintf(stderr, "node-crypto : Unknown message digest %s\n", verifyType);
      return false;
    }
    EVP_MD_CTX_init(&mdctx);
    EVP_VerifyInit_ex(&mdctx, md, NULL);
    initialised_ = true;
    return true;
  }


  int VerifyUpdate(char* data, int len) {
    if (!initialised_) return 0;
    EVP_VerifyUpdate(&mdctx, data, len);
    return 1;
  }


  int VerifyFinal(char* key_pem, int key_pemLen, unsigned char* sig, int siglen) {
    if (!initialised_) return 0;

    EVP_PKEY* pkey = NULL;
    BIO *bp = NULL;
    X509 *x509 = NULL;
    int r = 0;

    bp = BIO_new(BIO_s_mem());
    if (bp == NULL) {
      ERR_print_errors_fp(stderr);
      return 0;
    }
    if(!BIO_write(bp, key_pem, key_pemLen)) {
      ERR_print_errors_fp(stderr);
      return 0;
    }

    // Check if this is a PKCS#8 public key before trying as X.509
    if (strncmp(key_pem, PUBLIC_KEY_PFX, PUBLIC_KEY_PFX_LEN) == 0) {
      pkey = PEM_read_bio_PUBKEY(bp, NULL, NULL, NULL);
      if (pkey == NULL) {
        ERR_print_errors_fp(stderr);
        return 0;
      }
    } else {
      // X.509 fallback
      x509 = PEM_read_bio_X509(bp, NULL, NULL, NULL);
      if (x509 == NULL) {
        ERR_print_errors_fp(stderr);
        return 0;
      }

      pkey = X509_get_pubkey(x509);
      if (pkey == NULL) {
        ERR_print_errors_fp(stderr);
        return 0;
      }
    }

    r = EVP_VerifyFinal(&mdctx, sig, siglen, pkey);

    if(pkey != NULL)
      EVP_PKEY_free (pkey);
    if (x509 != NULL)
      X509_free(x509);
    if (bp != NULL)
      BIO_free(bp);
    EVP_MD_CTX_cleanup(&mdctx);
    initialised_ = false;

    return r;
  }


 protected:

  static Handle<Value> New (const Arguments& args) {
    HandleScope scope;

    Verify *verify = new Verify();
    verify->Wrap(args.This());

    return args.This();
  }


  static Handle<Value> VerifyInit(const Arguments& args) {
    Verify *verify = ObjectWrap::Unwrap<Verify>(args.This());

    HandleScope scope;

    if (args.Length() == 0 || !args[0]->IsString()) {
      return ThrowException(Exception::Error(String::New(
        "Must give verifytype string as argument")));
    }

    String::Utf8Value verifyType(args[0]->ToString());

    bool r = verify->VerifyInit(*verifyType);

    if (!r) {
      return ThrowException(Exception::Error(String::New("VerifyInit error")));
    }

    return args.This();
  }


  static Handle<Value> VerifyUpdate(const Arguments& args) {
    HandleScope scope;

    Verify *verify = ObjectWrap::Unwrap<Verify>(args.This());

    ASSERT_IS_STRING_OR_BUFFER(args[0]);
    enum encoding enc = ParseEncoding(args[1]);
    ssize_t len = DecodeBytes(args[0], enc);

    if (len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    int r;

    if(Buffer::HasInstance(args[0])) {
      Local<Object> buffer_obj = args[0]->ToObject();
      char *buffer_data = Buffer::Data(buffer_obj);
      size_t buffer_length = Buffer::Length(buffer_obj);

      r = verify->VerifyUpdate(buffer_data, buffer_length);
    } else {
      char* buf = new char[len];
      ssize_t written = DecodeWrite(buf, len, args[0], enc);
      assert(written == len);
      r = verify->VerifyUpdate(buf, len);
      delete [] buf;
    }

    if (!r) {
      Local<Value> exception = Exception::TypeError(String::New("VerifyUpdate fail"));
      return ThrowException(exception);
    }

    return args.This();
  }


  static Handle<Value> VerifyFinal(const Arguments& args) {
    HandleScope scope;

    Verify *verify = ObjectWrap::Unwrap<Verify>(args.This());

    ASSERT_IS_STRING_OR_BUFFER(args[0]);
    ssize_t klen = DecodeBytes(args[0], BINARY);

    if (klen < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    char* kbuf = new char[klen];
    ssize_t kwritten = DecodeWrite(kbuf, klen, args[0], BINARY);
    assert(kwritten == klen);

    ASSERT_IS_STRING_OR_BUFFER(args[1]);
    ssize_t hlen = DecodeBytes(args[1], BINARY);

    if (hlen < 0) {
      delete [] kbuf;
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    unsigned char* hbuf = new unsigned char[hlen];
    ssize_t hwritten = DecodeWrite((char *)hbuf, hlen, args[1], BINARY);
    assert(hwritten == hlen);
    unsigned char* dbuf;
    int dlen;

    int r=-1;

    if (args.Length() == 2 || !args[2]->IsString()) {
      // Binary
      r = verify->VerifyFinal(kbuf, klen, hbuf, hlen);
    } else {
      String::Utf8Value encoding(args[2]->ToString());
      if (strcasecmp(*encoding, "hex") == 0) {
        // Hex encoding
        HexDecode(hbuf, hlen, (char **)&dbuf, &dlen);
        r = verify->VerifyFinal(kbuf, klen, dbuf, dlen);
        delete [] dbuf;
      } else if (strcasecmp(*encoding, "base64") == 0) {
        // Base64 encoding
        unbase64(hbuf, hlen, (char **)&dbuf, &dlen);
        r = verify->VerifyFinal(kbuf, klen, dbuf, dlen);
        delete [] dbuf;
      } else if (strcasecmp(*encoding, "binary") == 0) {
        r = verify->VerifyFinal(kbuf, klen, hbuf, hlen);
      } else {
        fprintf(stderr, "node-crypto : Verify .verify encoding "
                        "can be binary, hex or base64\n");
      }
    }

    delete [] kbuf;
    delete [] hbuf;

    return scope.Close(Integer::New(r));
  }

  Verify () : ObjectWrap () {
    initialised_ = false;
  }

  ~Verify () {
    if (initialised_) {
      EVP_MD_CTX_cleanup(&mdctx);
    }
  }

 private:

  EVP_MD_CTX mdctx; /* coverity[member_decl] */
  const EVP_MD *md; /* coverity[member_decl] */
  bool initialised_;

};


void InitCrypto(Handle<Object> target) {
  HandleScope scope;

  SSL_library_init();
  OpenSSL_add_all_algorithms();
  OpenSSL_add_all_digests();
  SSL_load_error_strings();
  ERR_load_crypto_strings();

  // Turn off compression. Saves memory - do it in userland.
#ifdef SSL_COMP_get_compression_methods
  // Before OpenSSL 0.9.8 this was not possible.
  STACK_OF(SSL_COMP)* comp_methods = SSL_COMP_get_compression_methods();
  sk_SSL_COMP_zero(comp_methods);
  assert(sk_SSL_COMP_num(comp_methods) == 0);
#endif

  SecureContext::Initialize(target);
  Connection::Initialize(target);
  Cipher::Initialize(target);
  Decipher::Initialize(target);
  Hmac::Initialize(target);
  Hash::Initialize(target);
  Sign::Initialize(target);
  Verify::Initialize(target);

  subject_symbol    = NODE_PSYMBOL("subject");
  issuer_symbol     = NODE_PSYMBOL("issuer");
  valid_from_symbol = NODE_PSYMBOL("valid_from");
  valid_to_symbol   = NODE_PSYMBOL("valid_to");
  fingerprint_symbol   = NODE_PSYMBOL("fingerprint");
  name_symbol       = NODE_PSYMBOL("name");
  version_symbol    = NODE_PSYMBOL("version");
  ext_key_usage_symbol = NODE_PSYMBOL("ext_key_usage");
}

}  // namespace crypto
}  // namespace node

NODE_MODULE(node_crypto, node::crypto::InitCrypto);

