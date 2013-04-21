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

#include "node_crypto.h"
#include "node_crypto_groups.h"
#include "v8.h"

#include "node.h"
#include "node_buffer.h"
#include "node_root_certs.h"

#include <string.h>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

#include <stdlib.h>
#include <errno.h>

#if OPENSSL_VERSION_NUMBER >= 0x10000000L
# define OPENSSL_CONST const
#else
# define OPENSSL_CONST
#endif

#define ASSERT_IS_BUFFER(val) \
  if (!Buffer::HasInstance(val)) { \
    return ThrowException(Exception::TypeError(String::New("Not a buffer"))); \
  }

static const char PUBLIC_KEY_PFX[] =  "-----BEGIN PUBLIC KEY-----";
static const int PUBLIC_KEY_PFX_LEN = sizeof(PUBLIC_KEY_PFX) - 1;
static const char PUBRSA_KEY_PFX[] =  "-----BEGIN RSA PUBLIC KEY-----";
static const int PUBRSA_KEY_PFX_LEN = sizeof(PUBRSA_KEY_PFX) - 1;
static const int X509_NAME_FLAGS = ASN1_STRFLGS_ESC_CTRL
                                 | ASN1_STRFLGS_ESC_MSB
                                 | XN_FLAG_SEP_MULTILINE
                                 | XN_FLAG_FN_SN;

namespace node {
namespace crypto {

using namespace v8;

static Persistent<String> errno_symbol;
static Persistent<String> syscall_symbol;
static Persistent<String> subject_symbol;
static Persistent<String> subjectaltname_symbol;
static Persistent<String> modulus_symbol;
static Persistent<String> exponent_symbol;
static Persistent<String> issuer_symbol;
static Persistent<String> valid_from_symbol;
static Persistent<String> valid_to_symbol;
static Persistent<String> fingerprint_symbol;
static Persistent<String> name_symbol;
static Persistent<String> version_symbol;
static Persistent<String> ext_key_usage_symbol;
static Persistent<String> onhandshakestart_sym;
static Persistent<String> onhandshakedone_sym;
static Persistent<String> onclienthello_sym;
static Persistent<String> onnewsession_sym;
static Persistent<String> sessionid_sym;

static Persistent<FunctionTemplate> secure_context_constructor;

static uv_rwlock_t* locks;


static void crypto_threadid_cb(CRYPTO_THREADID* tid) {
  CRYPTO_THREADID_set_numeric(tid, uv_thread_self());
}


static void crypto_lock_init(void) {
  int i, n;

  n = CRYPTO_num_locks();
  locks = new uv_rwlock_t[n];

  for (i = 0; i < n; i++)
    if (uv_rwlock_init(locks + i))
      abort();
}


static void crypto_lock_cb(int mode, int n, const char* file, int line) {
  assert((mode & CRYPTO_LOCK) || (mode & CRYPTO_UNLOCK));
  assert((mode & CRYPTO_READ) || (mode & CRYPTO_WRITE));

  if (mode & CRYPTO_LOCK) {
    if (mode & CRYPTO_READ)
      uv_rwlock_rdlock(locks + n);
    else
      uv_rwlock_wrlock(locks + n);
  } else {
    if (mode & CRYPTO_READ)
      uv_rwlock_rdunlock(locks + n);
    else
      uv_rwlock_wrunlock(locks + n);
  }
}


Handle<Value> ThrowCryptoErrorHelper(unsigned long err, bool is_type_error) {
  HandleScope scope;
  char errmsg[128];
  ERR_error_string_n(err, errmsg, sizeof(errmsg));
  return is_type_error ? ThrowTypeError(errmsg) : ThrowError(errmsg);
}


Handle<Value> ThrowCryptoError(unsigned long err) {
  return ThrowCryptoErrorHelper(err, false);
}


Handle<Value> ThrowCryptoTypeError(unsigned long err) {
  return ThrowCryptoErrorHelper(err, true);
}


void SecureContext::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(SecureContext::New);
  secure_context_constructor = Persistent<FunctionTemplate>::New(t);

  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("SecureContext"));

  NODE_SET_PROTOTYPE_METHOD(t, "init", SecureContext::Init);
  NODE_SET_PROTOTYPE_METHOD(t, "setKey", SecureContext::SetKey);
  NODE_SET_PROTOTYPE_METHOD(t, "setCert", SecureContext::SetCert);
  NODE_SET_PROTOTYPE_METHOD(t, "addCACert", SecureContext::AddCACert);
  NODE_SET_PROTOTYPE_METHOD(t, "addCRL", SecureContext::AddCRL);
  NODE_SET_PROTOTYPE_METHOD(t, "addRootCerts", SecureContext::AddRootCerts);
  NODE_SET_PROTOTYPE_METHOD(t, "setCiphers", SecureContext::SetCiphers);
  NODE_SET_PROTOTYPE_METHOD(t, "setOptions", SecureContext::SetOptions);
  NODE_SET_PROTOTYPE_METHOD(t, "setSessionIdContext",
                               SecureContext::SetSessionIdContext);
  NODE_SET_PROTOTYPE_METHOD(t, "close", SecureContext::Close);
  NODE_SET_PROTOTYPE_METHOD(t, "loadPKCS12", SecureContext::LoadPKCS12);

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
    String::Utf8Value sslmethod(args[0]);

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

  // SSL session cache configuration
  SSL_CTX_set_session_cache_mode(sc->ctx_,
                                 SSL_SESS_CACHE_SERVER |
                                 SSL_SESS_CACHE_NO_INTERNAL |
                                 SSL_SESS_CACHE_NO_AUTO_CLEAR);
  SSL_CTX_sess_set_get_cb(sc->ctx_, GetSessionCallback);
  SSL_CTX_sess_set_new_cb(sc->ctx_, NewSessionCallback);

  sc->ca_store_ = NULL;
  return True();
}


SSL_SESSION* SecureContext::GetSessionCallback(SSL* s,
                                               unsigned char* key,
                                               int len,
                                               int* copy) {
  HandleScope scope;

  Connection* p = static_cast<Connection*>(SSL_get_app_data(s));

  *copy = 0;
  SSL_SESSION* sess = p->next_sess_;
  p->next_sess_ = NULL;

  return sess;
}


void SessionDataFree(char* data, void* hint) {
  delete[] data;
}


int SecureContext::NewSessionCallback(SSL* s, SSL_SESSION* sess) {
  HandleScope scope;

  Connection* p = static_cast<Connection*>(SSL_get_app_data(s));

  // Check if session is small enough to be stored
  int size = i2d_SSL_SESSION(sess, NULL);
  if (size > kMaxSessionSize) return 0;

  // Serialize session
  char* serialized = new char[size];
  unsigned char* pserialized = reinterpret_cast<unsigned char*>(serialized);
  memset(serialized, 0, size);
  i2d_SSL_SESSION(sess, &pserialized);

  Handle<Value> argv[2] = {
    Buffer::New(reinterpret_cast<char*>(sess->session_id),
                sess->session_id_length)->handle_,
    Buffer::New(serialized, size, SessionDataFree, NULL)->handle_
  };

  if (onnewsession_sym.IsEmpty()) {
    onnewsession_sym = NODE_PSYMBOL("onnewsession");
  }
  MakeCallback(p->handle_, onnewsession_sym, ARRAY_SIZE(argv), argv);

  return 0;
}


// Takes a string or buffer and loads it into a BIO.
// Caller responsible for BIO_free-ing the returned object.
static BIO* LoadBIO (Handle<Value> v) {
  BIO *bio = BIO_new(BIO_s_mem());
  if (!bio) return NULL;

  HandleScope scope;

  int r = -1;

  if (v->IsString()) {
    String::Utf8Value s(v);
    r = BIO_write(bio, *s, s.length());
  } else if (Buffer::HasInstance(v)) {
    char* buffer_data = Buffer::Data(v);
    size_t buffer_length = Buffer::Length(v);
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

  unsigned int len = args.Length();
  if (len != 1 && len != 2) {
    return ThrowException(Exception::TypeError(String::New("Bad parameter")));
  }
  if (len == 2 && !args[1]->IsString()) {
    return ThrowException(Exception::TypeError(String::New("Bad parameter")));
  }

  BIO *bio = LoadBIO(args[0]);
  if (!bio) return False();

  String::Utf8Value passphrase(args[1]);

  EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, NULL, NULL,
                                          len == 1 ? NULL : *passphrase);

  if (!key) {
    BIO_free(bio);
    unsigned long err = ERR_get_error();
    if (!err) {
      return ThrowException(Exception::Error(
          String::New("PEM_read_bio_PrivateKey")));
    }
    return ThrowCryptoError(err);
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
    return ThrowCryptoError(err);
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

  String::Utf8Value ciphers(args[0]);
  SSL_CTX_set_cipher_list(sc->ctx_, *ciphers);

  return True();
}

Handle<Value> SecureContext::SetOptions(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() != 1 || !args[0]->IntegerValue()) {
    return ThrowException(Exception::TypeError(String::New("Bad parameter")));
  }

  SSL_CTX_set_options(sc->ctx_, args[0]->IntegerValue());

  return True();
}

Handle<Value> SecureContext::SetSessionIdContext(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() != 1 || !args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New("Bad parameter")));
  }

  String::Utf8Value sessionIdContext(args[0]);
  const unsigned char* sid_ctx = (const unsigned char*) *sessionIdContext;
  unsigned int sid_ctx_len = sessionIdContext.length();

  int r = SSL_CTX_set_session_id_context(sc->ctx_, sid_ctx, sid_ctx_len);
  if (r != 1) {
    Local<String> message;
    BIO* bio;
    BUF_MEM* mem;
    if ((bio = BIO_new(BIO_s_mem()))) {
      ERR_print_errors(bio);
      BIO_get_mem_ptr(bio, &mem);
      message = String::New(mem->data, mem->length);
      BIO_free(bio);
    } else {
      message = String::New("SSL_CTX_set_session_id_context error");
    }
    return ThrowException(Exception::TypeError(message));
  }

  return True();
}

Handle<Value> SecureContext::Close(const Arguments& args) {
  HandleScope scope;
  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());
  sc->FreeCTXMem();
  return False();
}

//Takes .pfx or .p12 and password in string or buffer format
Handle<Value> SecureContext::LoadPKCS12(const Arguments& args) {
  HandleScope scope;

  BIO* in = NULL;
  PKCS12* p12 = NULL;
  EVP_PKEY* pkey = NULL;
  X509* cert = NULL;
  STACK_OF(X509)* extraCerts = NULL;
  char* pass = NULL;
  bool ret = false;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() < 1) {
    return ThrowException(Exception::TypeError(
          String::New("Bad parameter")));
  }

  in = LoadBIO(args[0]);
  if (in == NULL) {
    return ThrowException(Exception::Error(
          String::New("Unable to load BIO")));
  }

  if (args.Length() >= 2) {
    ASSERT_IS_BUFFER(args[1]);

    int passlen = Buffer::Length(args[1]);
    if (passlen < 0) {
      BIO_free(in);
      return ThrowException(Exception::TypeError(
            String::New("Bad password")));
    }
    pass = new char[passlen + 1];
    int pass_written = DecodeWrite(pass, passlen, args[1], BINARY);

    assert(pass_written == passlen);
    pass[passlen] = '\0';
  }

  if (d2i_PKCS12_bio(in, &p12) &&
      PKCS12_parse(p12, pass, &pkey, &cert, &extraCerts) &&
      SSL_CTX_use_certificate(sc->ctx_, cert) &&
      SSL_CTX_use_PrivateKey(sc->ctx_, pkey))
  {
    // set extra certs
    while (X509* x509 = sk_X509_pop(extraCerts)) {
      if (!sc->ca_store_) {
        sc->ca_store_ = X509_STORE_new();
        SSL_CTX_set_cert_store(sc->ctx_, sc->ca_store_);
      }

      X509_STORE_add_cert(sc->ca_store_, x509);
      SSL_CTX_add_client_CA(sc->ctx_, x509);
    }

    EVP_PKEY_free(pkey);
    X509_free(cert);
    sk_X509_free(extraCerts);

    ret = true;
  }

  PKCS12_free(p12);
  BIO_free(in);
  delete[] pass;

  if (!ret) {
    unsigned long err = ERR_get_error();
    const char* str = ERR_reason_error_string(err);
    return ThrowException(Exception::Error(String::New(str)));
  }

  return True();
}


size_t ClientHelloParser::Write(const uint8_t* data, size_t len) {
  HandleScope scope;

  // Just accumulate data, everything will be pushed to BIO later
  if (state_ == kPaused) return 0;

  // Copy incoming data to the internal buffer
  // (which has a size of the biggest possible TLS frame)
  size_t available = sizeof(data_) - offset_;
  size_t copied = len < available ? len : available;
  memcpy(data_ + offset_, data, copied);
  offset_ += copied;

  // Vars for parsing hello
  bool is_clienthello = false;
  uint8_t session_size = -1;
  uint8_t* session_id = NULL;
  Local<Object> hello;
  Handle<Value> argv[1];

  switch (state_) {
   case kWaiting:
    // >= 5 bytes for header parsing
    if (offset_ < 5) break;

    if (data_[0] == kChangeCipherSpec || data_[0] == kAlert ||
        data_[0] == kHandshake || data_[0] == kApplicationData) {
      frame_len_ = (data_[3] << 8) + data_[4];
      state_ = kTLSHeader;
      body_offset_ = 5;
    } else {
      frame_len_ = (data_[0] << 8) + data_[1];
      state_ = kSSLHeader;
      if (*data_ & 0x40) {
        // header with padding
        body_offset_ = 3;
      } else {
        // without padding
        body_offset_ = 2;
      }
    }

    // Sanity check (too big frame, or too small)
    if (frame_len_ >= sizeof(data_)) {
      // Let OpenSSL handle it
      Finish();
      return copied;
    }
   case kTLSHeader:
   case kSSLHeader:
    // >= 5 + frame size bytes for frame parsing
    if (offset_ < body_offset_ + frame_len_) break;

    // Skip unsupported frames and gather some data from frame

    // TODO: Check protocol version
    if (data_[body_offset_] == kClientHello) {
      is_clienthello = true;
      uint8_t* body;
      size_t session_offset;

      if (state_ == kTLSHeader) {
        // Skip frame header, hello header, protocol version and random data
        session_offset = body_offset_ + 4 + 2 + 32;

        if (session_offset + 1 < offset_) {
          body = data_ + session_offset;
          session_size = *body;
          session_id = body + 1;
        }
      } else if (state_ == kSSLHeader) {
        // Skip header, version
        session_offset = body_offset_ + 3;

        if (session_offset + 4 < offset_) {
          body = data_ + session_offset;

          int ciphers_size = (body[0] << 8) + body[1];

          if (body + 4 + ciphers_size < data_ + offset_) {
            session_size = (body[2] << 8) + body[3];
            session_id = body + 4 + ciphers_size;
          }
        }
      } else {
        // Whoa? How did we get here?
        abort();
      }

      // Check if we overflowed (do not reply with any private data)
      if (session_id == NULL ||
          session_size > 32 ||
          session_id + session_size > data_ + offset_) {
        Finish();
        return copied;
      }

      // TODO: Parse other things?
    }

    // Not client hello - let OpenSSL handle it
    if (!is_clienthello) {
      Finish();
      return copied;
    }

    // Parse frame, call javascript handler and
    // move parser into the paused state
    if (onclienthello_sym.IsEmpty()) {
      onclienthello_sym = NODE_PSYMBOL("onclienthello");
    }
    if (sessionid_sym.IsEmpty()) {
      sessionid_sym = NODE_PSYMBOL("sessionId");
    }

    state_ = kPaused;
    hello = Object::New();
    hello->Set(sessionid_sym,
               Buffer::New(reinterpret_cast<char*>(session_id),
                           session_size)->handle_);

    argv[0] = hello;
    MakeCallback(conn_->handle_, onclienthello_sym, 1, argv);
    break;
   case kEnded:
   default:
    break;
  }

  return copied;
}


void ClientHelloParser::Finish() {
  assert(state_ != kEnded);
  state_ = kEnded;

  // Write all accumulated data
  int r = BIO_write(conn_->bio_read_, reinterpret_cast<char*>(data_), offset_);
  conn_->HandleBIOError(conn_->bio_read_, "BIO_write", r);
  conn_->SetShutdownFlags();
}


#ifdef SSL_PRINT_DEBUG
# define DEBUG_PRINT(...) fprintf (stderr, __VA_ARGS__)
#else
# define DEBUG_PRINT(...)
#endif


int Connection::HandleBIOError(BIO *bio, const char* func, int rv) {
  if (rv >= 0) return rv;

  int retry = BIO_should_retry(bio);
  (void) retry; // unused if !defined(SSL_PRINT_DEBUG)

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


int Connection::HandleSSLError(const char* func, int rv, ZeroStatus zs) {
  // Forcibly clear OpenSSL's error stack on return. This stops stale errors
  // from popping up later in the lifecycle of the SSL connection where they
  // would cause spurious failures. It's a rather blunt method, though.
  // ERR_clear_error() isn't necessarily cheap either.
  struct ClearErrorOnReturn {
    ~ClearErrorOnReturn() { ERR_clear_error(); }
  };
  ClearErrorOnReturn clear_error_on_return;
  (void) &clear_error_on_return;  // Silence unused variable warning.

  if (rv > 0) return rv;
  if ((rv == 0) && (zs == kZeroIsNotAnError)) return rv;

  int err = SSL_get_error(ssl_, rv);

  if (err == SSL_ERROR_NONE) {
    return 0;

  } else if (err == SSL_ERROR_WANT_WRITE) {
    DEBUG_PRINT("[%p] SSL: %s want write\n", ssl_, func);
    return 0;

  } else if (err == SSL_ERROR_WANT_READ) {
    DEBUG_PRINT("[%p] SSL: %s want read\n", ssl_, func);
    return 0;

  } else if (err == SSL_ERROR_ZERO_RETURN) {
    handle_->Set(String::New("error"),
                 Exception::Error(String::New("ZERO_RETURN")));
    return rv;

  } else {
    HandleScope scope;
    BUF_MEM* mem;
    BIO *bio;

    assert(err == SSL_ERROR_SSL || err == SSL_ERROR_SYSCALL);

    // XXX We need to drain the error queue for this thread or else OpenSSL
    // has the possibility of blocking connections? This problem is not well
    // understood. And we should be somehow propagating these errors up
    // into JavaScript. There is no test which demonstrates this problem.
    // https://github.com/joyent/node/issues/1719
    if ((bio = BIO_new(BIO_s_mem()))) {
      ERR_print_errors(bio);
      BIO_get_mem_ptr(bio, &mem);
      Local<Value> e = Exception::Error(String::New(mem->data, mem->length));
      handle_->Set(String::New("error"), e);
      BIO_free(bio);
    }

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
  NODE_SET_PROTOTYPE_METHOD(t, "getSession", Connection::GetSession);
  NODE_SET_PROTOTYPE_METHOD(t, "setSession", Connection::SetSession);
  NODE_SET_PROTOTYPE_METHOD(t, "loadSession", Connection::LoadSession);
  NODE_SET_PROTOTYPE_METHOD(t, "isSessionReused", Connection::IsSessionReused);
  NODE_SET_PROTOTYPE_METHOD(t, "isInitFinished", Connection::IsInitFinished);
  NODE_SET_PROTOTYPE_METHOD(t, "verifyError", Connection::VerifyError);
  NODE_SET_PROTOTYPE_METHOD(t, "getCurrentCipher", Connection::GetCurrentCipher);
  NODE_SET_PROTOTYPE_METHOD(t, "start", Connection::Start);
  NODE_SET_PROTOTYPE_METHOD(t, "shutdown", Connection::Shutdown);
  NODE_SET_PROTOTYPE_METHOD(t, "receivedShutdown", Connection::ReceivedShutdown);
  NODE_SET_PROTOTYPE_METHOD(t, "close", Connection::Close);

#ifdef OPENSSL_NPN_NEGOTIATED
  NODE_SET_PROTOTYPE_METHOD(t, "getNegotiatedProtocol", Connection::GetNegotiatedProto);
  NODE_SET_PROTOTYPE_METHOD(t, "setNPNProtocols", Connection::SetNPNProtocols);
#endif


#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  NODE_SET_PROTOTYPE_METHOD(t, "getServername", Connection::GetServername);
  NODE_SET_PROTOTYPE_METHOD(t, "setSNICallback",  Connection::SetSNICallback);
#endif

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

#ifdef OPENSSL_NPN_NEGOTIATED

int Connection::AdvertiseNextProtoCallback_(SSL *s,
                                            const unsigned char** data,
                                            unsigned int *len,
                                            void *arg) {

  Connection *p = static_cast<Connection*>(SSL_get_app_data(s));

  if (p->npnProtos_.IsEmpty()) {
    // No initialization - no NPN protocols
    *data = reinterpret_cast<const unsigned char*>("");
    *len = 0;
  } else {
    *data = reinterpret_cast<const unsigned char*>(Buffer::Data(p->npnProtos_));
    *len = Buffer::Length(p->npnProtos_);
  }

  return SSL_TLSEXT_ERR_OK;
}

int Connection::SelectNextProtoCallback_(SSL *s,
                             unsigned char** out, unsigned char* outlen,
                             const unsigned char* in,
                             unsigned int inlen, void *arg) {
  Connection *p = static_cast<Connection*> SSL_get_app_data(s);

  // Release old protocol handler if present
  if (!p->selectedNPNProto_.IsEmpty()) {
    p->selectedNPNProto_.Dispose();
  }

  if (p->npnProtos_.IsEmpty()) {
    // We should at least select one protocol
    // If server is using NPN
    *out = reinterpret_cast<unsigned char*>(const_cast<char*>("http/1.1"));
    *outlen = 8;

    // set status unsupported
    p->selectedNPNProto_ = Persistent<Value>::New(False());

    return SSL_TLSEXT_ERR_OK;
  }

  const unsigned char* npnProtos =
      reinterpret_cast<const unsigned char*>(Buffer::Data(p->npnProtos_));

  int status = SSL_select_next_proto(out, outlen, in, inlen, npnProtos,
                                     Buffer::Length(p->npnProtos_));

  switch (status) {
    case OPENSSL_NPN_UNSUPPORTED:
      p->selectedNPNProto_ = Persistent<Value>::New(Null());
      break;
    case OPENSSL_NPN_NEGOTIATED:
      p->selectedNPNProto_ = Persistent<Value>::New(String::New(
                                 reinterpret_cast<const char*>(*out), *outlen
                             ));
      break;
    case OPENSSL_NPN_NO_OVERLAP:
      p->selectedNPNProto_ = Persistent<Value>::New(False());
      break;
    default:
      break;
  }

  return SSL_TLSEXT_ERR_OK;
}
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
int Connection::SelectSNIContextCallback_(SSL *s, int *ad, void* arg) {
  HandleScope scope;

  Connection *p = static_cast<Connection*> SSL_get_app_data(s);

  const char* servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);

  if (servername) {
    if (!p->servername_.IsEmpty()) {
      p->servername_.Dispose();
    }
    p->servername_ = Persistent<String>::New(String::New(servername));

    // Call the SNI callback and use its return value as context
    if (!p->sniObject_.IsEmpty()) {
      if (!p->sniContext_.IsEmpty()) {
        p->sniContext_.Dispose();
      }

      // Get callback init args
      Local<Value> argv[1] = {*p->servername_};

      // Call it
      Local<Value> ret = Local<Value>::New(MakeCallback(p->sniObject_,
                                                        "onselect",
                                                        ARRAY_SIZE(argv),
                                                        argv));

      // If ret is SecureContext
      if (secure_context_constructor->HasInstance(ret)) {
        p->sniContext_ = Persistent<Value>::New(ret);
        SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(
                                Local<Object>::Cast(ret));
        SSL_set_SSL_CTX(s, sc->ctx_);
      } else {
        return SSL_TLSEXT_ERR_NOACK;
      }
    }
  }

  return SSL_TLSEXT_ERR_OK;
}
#endif

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

  SSL_set_app_data(p->ssl_, p);

  if (is_server) SSL_set_info_callback(p->ssl_, SSLInfoCallback);

#ifdef OPENSSL_NPN_NEGOTIATED
  if (is_server) {
    // Server should advertise NPN protocols
    SSL_CTX_set_next_protos_advertised_cb(sc->ctx_,
                                          AdvertiseNextProtoCallback_,
                                          NULL);
  } else {
    // Client should select protocol from advertised
    // If server supports NPN
    SSL_CTX_set_next_proto_select_cb(sc->ctx_,
                                     SelectNextProtoCallback_,
                                     NULL);
  }
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  if (is_server) {
    SSL_CTX_set_tlsext_servername_callback(sc->ctx_, SelectSNIContextCallback_);
  } else {
    String::Utf8Value servername(args[2]);
    SSL_set_tlsext_host_name(p->ssl_, *servername);
  }
#endif

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


void Connection::SSLInfoCallback(const SSL *ssl_, int where, int ret) {
  // Be compatible with older versions of OpenSSL. SSL_get_app_data() wants
  // a non-const SSL* in OpenSSL <= 0.9.7e.
  SSL* ssl = const_cast<SSL*>(ssl_);
  if (where & SSL_CB_HANDSHAKE_START) {
    HandleScope scope;
    Connection* c = static_cast<Connection*>(SSL_get_app_data(ssl));
    if (onhandshakestart_sym.IsEmpty()) {
      onhandshakestart_sym = NODE_PSYMBOL("onhandshakestart");
    }
    MakeCallback(c->handle_, onhandshakestart_sym, 0, NULL);
  }
  if (where & SSL_CB_HANDSHAKE_DONE) {
    HandleScope scope;
    Connection* c = static_cast<Connection*>(SSL_get_app_data(ssl));
    if (onhandshakedone_sym.IsEmpty()) {
      onhandshakedone_sym = NODE_PSYMBOL("onhandshakedone");
    }
    MakeCallback(c->handle_, onhandshakedone_sym, 0, NULL);
  }
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

  char* buffer_data = Buffer::Data(args[0]);
  size_t buffer_length = Buffer::Length(args[0]);

  size_t off = args[1]->Int32Value();
  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowException(Exception::Error(
          String::New("off + len > buffer.length")));
  }

  int bytes_written;
  char* data = buffer_data + off;

  if (ss->is_server_ && !ss->hello_parser_.ended()) {
    bytes_written = ss->hello_parser_.Write(reinterpret_cast<uint8_t*>(data),
                                            len);
  } else {
    bytes_written = BIO_write(ss->bio_read_, data, len);
    ss->HandleBIOError(ss->bio_read_, "BIO_write", bytes_written);
    ss->SetShutdownFlags();
  }

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

  char* buffer_data = Buffer::Data(args[0]);
  size_t buffer_length = Buffer::Length(args[0]);

  size_t off = args[1]->Int32Value();
  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowException(Exception::Error(
          String::New("off + len > buffer.length")));
  }

  if (!SSL_is_init_finished(ss->ssl_)) {
    int rv;

    if (ss->is_server_) {
      rv = SSL_accept(ss->ssl_);
      ss->HandleSSLError("SSL_accept:ClearOut", rv, kZeroIsAnError);
    } else {
      rv = SSL_connect(ss->ssl_);
      ss->HandleSSLError("SSL_connect:ClearOut", rv, kZeroIsAnError);
    }

    if (rv < 0) return scope.Close(Integer::New(rv));
  }

  int bytes_read = SSL_read(ss->ssl_, buffer_data + off, len);
  ss->HandleSSLError("SSL_read:ClearOut", bytes_read, kZeroIsNotAnError);
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

  char* buffer_data = Buffer::Data(args[0]);
  size_t buffer_length = Buffer::Length(args[0]);

  size_t off = args[1]->Int32Value();
  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowException(Exception::Error(
          String::New("off + len > buffer.length")));
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

  char* buffer_data = Buffer::Data(args[0]);
  size_t buffer_length = Buffer::Length(args[0]);

  size_t off = args[1]->Int32Value();
  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowException(Exception::Error(
          String::New("off + len > buffer.length")));
  }

  if (!SSL_is_init_finished(ss->ssl_)) {
    int rv;
    if (ss->is_server_) {
      rv = SSL_accept(ss->ssl_);
      ss->HandleSSLError("SSL_accept:ClearIn", rv, kZeroIsAnError);
    } else {
      rv = SSL_connect(ss->ssl_);
      ss->HandleSSLError("SSL_connect:ClearIn", rv, kZeroIsAnError);
    }

    if (rv < 0) return scope.Close(Integer::New(rv));
  }

  int bytes_written = SSL_write(ss->ssl_, buffer_data + off, len);

  ss->HandleSSLError("SSL_write:ClearIn",
                     bytes_written,
                     len == 0 ? kZeroIsNotAnError : kZeroIsAnError);
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
    BIO* bio = BIO_new(BIO_s_mem());
    BUF_MEM* mem;
    if (X509_NAME_print_ex(bio, X509_get_subject_name(peer_cert), 0,
                           X509_NAME_FLAGS) > 0) {
      BIO_get_mem_ptr(bio, &mem);
      info->Set(subject_symbol, String::New(mem->data, mem->length));
    }
    (void) BIO_reset(bio);

    if (X509_NAME_print_ex(bio, X509_get_issuer_name(peer_cert), 0,
                           X509_NAME_FLAGS) > 0) {
      BIO_get_mem_ptr(bio, &mem);
      info->Set(issuer_symbol, String::New(mem->data, mem->length));
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
      info->Set(subjectaltname_symbol, String::New(mem->data, mem->length));

      (void) BIO_reset(bio);
    }

    EVP_PKEY *pkey = NULL;
    RSA *rsa = NULL;
    if( NULL != (pkey = X509_get_pubkey(peer_cert))
        && NULL != (rsa = EVP_PKEY_get1_RSA(pkey)) ) {
        BN_print(bio, rsa->n);
        BIO_get_mem_ptr(bio, &mem);
        info->Set(modulus_symbol, String::New(mem->data, mem->length) );
        (void) BIO_reset(bio);

        BN_print(bio, rsa->e);
        BIO_get_mem_ptr(bio, &mem);
        info->Set(exponent_symbol, String::New(mem->data, mem->length) );
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
    info->Set(valid_from_symbol, String::New(mem->data, mem->length));
    (void) BIO_reset(bio);

    ASN1_TIME_print(bio, X509_get_notAfter(peer_cert));
    BIO_get_mem_ptr(bio, &mem);
    info->Set(valid_to_symbol, String::New(mem->data, mem->length));
    BIO_free(bio);

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
      char buf[256];

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

Handle<Value> Connection::GetSession(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->ssl_ == NULL) return Undefined();

  SSL_SESSION* sess = SSL_get_session(ss->ssl_);
  if (!sess) return Undefined();

  int slen = i2d_SSL_SESSION(sess, NULL);
  assert(slen > 0);

  if (slen > 0) {
    unsigned char* sbuf = new unsigned char[slen];
    unsigned char* p = sbuf;
    i2d_SSL_SESSION(sess, &p);
    Local<Value> s = Encode(sbuf, slen, BINARY);
    delete[] sbuf;
    return scope.Close(s);
  }

  return Null();
}

Handle<Value> Connection::SetSession(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (args.Length() < 1 ||
      (!args[0]->IsString() && !Buffer::HasInstance(args[0]))) {
    Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
    return ThrowException(exception);
  }

  ASSERT_IS_BUFFER(args[0]);
  ssize_t slen = Buffer::Length(args[0]);

  if (slen < 0) {
    Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
    return ThrowException(exception);
  }

  char* sbuf = new char[slen];

  ssize_t wlen = DecodeWrite(sbuf, slen, args[0], BINARY);
  assert(wlen == slen);

  const unsigned char* p = reinterpret_cast<const unsigned char*>(sbuf);
  SSL_SESSION* sess = d2i_SSL_SESSION(NULL, &p, wlen);

  delete [] sbuf;

  if (!sess)
    return Undefined();

  int r = SSL_set_session(ss->ssl_, sess);
  SSL_SESSION_free(sess);

  if (!r) {
    Local<String> eStr = String::New("SSL_set_session error");
    return ThrowException(Exception::Error(eStr));
  }

  return True();
}

Handle<Value> Connection::LoadSession(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (args.Length() >= 1 && Buffer::HasInstance(args[0])) {
    ssize_t slen = Buffer::Length(args[0].As<Object>());
    char* sbuf = Buffer::Data(args[0].As<Object>());

    const unsigned char* p = reinterpret_cast<unsigned char*>(sbuf);
    SSL_SESSION* sess = d2i_SSL_SESSION(NULL, &p, slen);

    // Setup next session and move hello to the BIO buffer
    if (ss->next_sess_ != NULL) {
      SSL_SESSION_free(ss->next_sess_);
    }
    ss->next_sess_ = sess;
  }

  ss->hello_parser_.Finish();

  return True();
}

Handle<Value> Connection::IsSessionReused(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->ssl_ == NULL || SSL_session_reused(ss->ssl_) == false) {
    return False();
  }

  return True();
}


Handle<Value> Connection::Start(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (!SSL_is_init_finished(ss->ssl_)) {
    int rv;
    if (ss->is_server_) {
      rv = SSL_accept(ss->ssl_);
      ss->HandleSSLError("SSL_accept:Start", rv, kZeroIsAnError);
    } else {
      rv = SSL_connect(ss->ssl_);
      ss->HandleSSLError("SSL_connect:Start", rv, kZeroIsAnError);
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
  ss->HandleSSLError("SSL_shutdown", rv, kZeroIsNotAnError);
  ss->SetShutdownFlags();

  return scope.Close(Integer::New(rv));
}


Handle<Value> Connection::ReceivedShutdown(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->ssl_ == NULL) return False();
  int r = SSL_get_shutdown(ss->ssl_);

  if (r & SSL_RECEIVED_SHUTDOWN) return True();

  return False();
}


Handle<Value> Connection::IsInitFinished(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->ssl_ == NULL || SSL_is_init_finished(ss->ssl_) == false) {
    return False();
  }

  return True();
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
    return scope.Close(Exception::Error(
          String::New("UNABLE_TO_GET_ISSUER_CERT")));
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

  return scope.Close(Exception::Error(s));
}


Handle<Value> Connection::GetCurrentCipher(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  OPENSSL_CONST SSL_CIPHER *c;

  if ( ss->ssl_ == NULL ) return Undefined();
  c = SSL_get_current_cipher(ss->ssl_);
  if ( c == NULL ) return Undefined();
  Local<Object> info = Object::New();
  const char* cipher_name = SSL_CIPHER_get_name(c);
  info->Set(name_symbol, String::New(cipher_name));
  const char* cipher_version = SSL_CIPHER_get_version(c);
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

#ifdef OPENSSL_NPN_NEGOTIATED
Handle<Value> Connection::GetNegotiatedProto(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->is_server_) {
    const unsigned char* npn_proto;
    unsigned int npn_proto_len;

    SSL_get0_next_proto_negotiated(ss->ssl_, &npn_proto, &npn_proto_len);

    if (!npn_proto) {
      return False();
    }

    return scope.Close(String::New(reinterpret_cast<const char*>(npn_proto),
                                   npn_proto_len));
  } else {
    return ss->selectedNPNProto_;
  }
}

Handle<Value> Connection::SetNPNProtocols(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (args.Length() < 1 || !Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::Error(String::New(
           "Must give a Buffer as first argument")));
  }

  // Release old handle
  if (!ss->npnProtos_.IsEmpty()) {
    ss->npnProtos_.Dispose();
  }
  ss->npnProtos_ = Persistent<Object>::New(args[0]->ToObject());

  return True();
};
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
Handle<Value> Connection::GetServername(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (ss->is_server_ && !ss->servername_.IsEmpty()) {
    return ss->servername_;
  } else {
    return False();
  }
}

Handle<Value> Connection::SetSNICallback(const Arguments& args) {
  HandleScope scope;

  Connection *ss = Connection::Unwrap(args);

  if (args.Length() < 1 || !args[0]->IsFunction()) {
    return ThrowException(Exception::Error(String::New(
           "Must give a Function as first argument")));
  }

  // Release old handle
  if (!ss->sniObject_.IsEmpty()) {
    ss->sniObject_.Dispose();
  }
  ss->sniObject_ = Persistent<Object>::New(Object::New());
  ss->sniObject_->Set(String::New("onselect"), args[0]);

  return True();
}
#endif


class Cipher : public ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "init", CipherInit);
    NODE_SET_PROTOTYPE_METHOD(t, "initiv", CipherInitIv);
    NODE_SET_PROTOTYPE_METHOD(t, "update", CipherUpdate);
    NODE_SET_PROTOTYPE_METHOD(t, "setAutoPadding", SetAutoPadding);
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
    int key_len = EVP_BytesToKey(cipher, EVP_md5(), NULL,
      (unsigned char*) key_buf, key_buf_len, 1, key, iv);

    EVP_CIPHER_CTX_init(&ctx);
    EVP_CipherInit_ex(&ctx, cipher, NULL, NULL, NULL, true);
    if (!EVP_CIPHER_CTX_set_key_length(&ctx, key_len)) {
      fprintf(stderr, "node-crypto : Invalid key length %d\n", key_len);
      EVP_CIPHER_CTX_cleanup(&ctx);
      return false;
    }
    EVP_CipherInit_ex(&ctx, NULL, NULL,
      (unsigned char*)key,
      (unsigned char*)iv, true);
    initialised_ = true;
    return true;
  }


  bool CipherInitIv(char* cipherType,
                    char* key,
                    int key_len,
                    char* iv,
                    int iv_len) {
    cipher = EVP_get_cipherbyname(cipherType);
    if(!cipher) {
      fprintf(stderr, "node-crypto : Unknown cipher %s\n", cipherType);
      return false;
    }
    /* OpenSSL versions up to 0.9.8l failed to return the correct
       iv_length (0) for ECB ciphers */
    if (EVP_CIPHER_iv_length(cipher) != iv_len &&
      !(EVP_CIPHER_mode(cipher) == EVP_CIPH_ECB_MODE && iv_len == 0)) {
      fprintf(stderr, "node-crypto : Invalid IV length %d\n", iv_len);
      return false;
    }
    EVP_CIPHER_CTX_init(&ctx);
    EVP_CipherInit_ex(&ctx, cipher, NULL, NULL, NULL, true);
    if (!EVP_CIPHER_CTX_set_key_length(&ctx, key_len)) {
      fprintf(stderr, "node-crypto : Invalid key length %d\n", key_len);
      EVP_CIPHER_CTX_cleanup(&ctx);
      return false;
    }
    EVP_CipherInit_ex(&ctx, NULL, NULL,
      (unsigned char*)key,
      (unsigned char*)iv, true);
    initialised_ = true;
    return true;
  }

  int CipherUpdate(char* data, int len, unsigned char** out, int* out_len) {
    if (!initialised_) return 0;
    *out_len=len+EVP_CIPHER_CTX_block_size(&ctx);
    *out= new unsigned char[*out_len];
    return EVP_CipherUpdate(&ctx, *out, out_len, (unsigned char*)data, len);
  }

  int SetAutoPadding(bool auto_padding) {
    if (!initialised_) return 0;
    return EVP_CIPHER_CTX_set_padding(&ctx, auto_padding ? 1 : 0);
  }

  int CipherFinal(unsigned char** out, int *out_len) {
    if (!initialised_) return 0;
    *out = new unsigned char[EVP_CIPHER_CTX_block_size(&ctx)];
    int r = EVP_CipherFinal_ex(&ctx,*out, out_len);
    EVP_CIPHER_CTX_cleanup(&ctx);
    initialised_ = false;
    return r;
  }


 protected:

  static Handle<Value> New(const Arguments& args) {
    HandleScope scope;

    Cipher *cipher = new Cipher();
    cipher->Wrap(args.This());
    return args.This();
  }

  static Handle<Value> CipherInit(const Arguments& args) {
    HandleScope scope;

    Cipher *cipher = ObjectWrap::Unwrap<Cipher>(args.This());

    if (args.Length() <= 1
        || !args[0]->IsString()
        || !(args[1]->IsString() || Buffer::HasInstance(args[1])))
    {
      return ThrowException(Exception::Error(String::New(
        "Must give cipher-type, key")));
    }

    ASSERT_IS_BUFFER(args[1]);
    ssize_t key_buf_len = Buffer::Length(args[1]);

    if (key_buf_len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    char* key_buf = new char[key_buf_len];
    ssize_t key_written = DecodeWrite(key_buf, key_buf_len, args[1], BINARY);
    assert(key_written == key_buf_len);

    String::Utf8Value cipherType(args[0]);

    bool r = cipher->CipherInit(*cipherType, key_buf, key_buf_len);

    delete [] key_buf;

    if (!r) return ThrowCryptoError(ERR_get_error());

    return args.This();
  }


  static Handle<Value> CipherInitIv(const Arguments& args) {
    Cipher *cipher = ObjectWrap::Unwrap<Cipher>(args.This());

    HandleScope scope;


    if (args.Length() <= 2
        || !args[0]->IsString()
        || !(args[1]->IsString() || Buffer::HasInstance(args[1]))
        || !(args[2]->IsString() || Buffer::HasInstance(args[2])))
    {
      return ThrowException(Exception::Error(String::New(
        "Must give cipher-type, key, and iv as argument")));
    }

    ASSERT_IS_BUFFER(args[1]);
    ssize_t key_len = Buffer::Length(args[1]);

    if (key_len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    ASSERT_IS_BUFFER(args[2]);
    ssize_t iv_len = Buffer::Length(args[2]);

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

    String::Utf8Value cipherType(args[0]);

    bool r = cipher->CipherInitIv(*cipherType, key_buf,key_len,iv_buf,iv_len);

    delete [] key_buf;
    delete [] iv_buf;

    if (!r) return ThrowCryptoError(ERR_get_error());

    return args.This();
  }

  static Handle<Value> CipherUpdate(const Arguments& args) {
    Cipher *cipher = ObjectWrap::Unwrap<Cipher>(args.This());

    HandleScope scope;

    ASSERT_IS_BUFFER(args[0]);

    unsigned char* out=0;
    int out_len=0, r;
    char* buffer_data = Buffer::Data(args[0]);
    size_t buffer_length = Buffer::Length(args[0]);

    r = cipher->CipherUpdate(buffer_data, buffer_length, &out, &out_len);

    if (r == 0) {
      delete [] out;
      return ThrowCryptoTypeError(ERR_get_error());
    }

    Local<Value> outString;
    outString = Encode(out, out_len, BUFFER);

    if (out) delete [] out;

    return scope.Close(outString);
  }

  static Handle<Value> SetAutoPadding(const Arguments& args) {
    HandleScope scope;
    Cipher *cipher = ObjectWrap::Unwrap<Cipher>(args.This());

    cipher->SetAutoPadding(args.Length() < 1 || args[0]->BooleanValue());

    return Undefined();
  }

  static Handle<Value> CipherFinal(const Arguments& args) {
    Cipher *cipher = ObjectWrap::Unwrap<Cipher>(args.This());

    HandleScope scope;

    unsigned char* out_value = NULL;
    int out_len = -1;
    Local<Value> outString ;

    int r = cipher->CipherFinal(&out_value, &out_len);

    if (out_len <= 0 || r == 0) {
      delete[] out_value;
      out_value = NULL;
      if (r == 0) return ThrowCryptoTypeError(ERR_get_error());
    }

    outString = Encode(out_value, out_len, BUFFER);

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
    NODE_SET_PROTOTYPE_METHOD(t, "finaltol", DecipherFinal); // remove someday
    NODE_SET_PROTOTYPE_METHOD(t, "setAutoPadding", SetAutoPadding);

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
    EVP_CipherInit_ex(&ctx, cipher_, NULL, NULL, NULL, false);
    if (!EVP_CIPHER_CTX_set_key_length(&ctx, key_len)) {
      fprintf(stderr, "node-crypto : Invalid key length %d\n", key_len);
      EVP_CIPHER_CTX_cleanup(&ctx);
      return false;
    }
    EVP_CipherInit_ex(&ctx, NULL, NULL,
      (unsigned char*)key,
      (unsigned char*)iv, false);
    initialised_ = true;
    return true;
  }


  bool DecipherInitIv(char* cipherType,
                      char* key,
                      int key_len,
                      char* iv,
                      int iv_len) {
    cipher_ = EVP_get_cipherbyname(cipherType);
    if(!cipher_) {
      fprintf(stderr, "node-crypto : Unknown cipher %s\n", cipherType);
      return false;
    }
    /* OpenSSL versions up to 0.9.8l failed to return the correct
      iv_length (0) for ECB ciphers */
    if (EVP_CIPHER_iv_length(cipher_) != iv_len &&
      !(EVP_CIPHER_mode(cipher_) == EVP_CIPH_ECB_MODE && iv_len == 0)) {
      fprintf(stderr, "node-crypto : Invalid IV length %d\n", iv_len);
      return false;
    }
    EVP_CIPHER_CTX_init(&ctx);
    EVP_CipherInit_ex(&ctx, cipher_, NULL, NULL, NULL, false);
    if (!EVP_CIPHER_CTX_set_key_length(&ctx, key_len)) {
      fprintf(stderr, "node-crypto : Invalid key length %d\n", key_len);
      EVP_CIPHER_CTX_cleanup(&ctx);
      return false;
    }
    EVP_CipherInit_ex(&ctx, NULL, NULL,
      (unsigned char*)key,
      (unsigned char*)iv, false);
    initialised_ = true;
    return true;
  }

  int DecipherUpdate(char* data, int len, unsigned char** out, int* out_len) {
    if (!initialised_) {
      *out_len = 0;
      *out = NULL;
      return 0;
    }

    *out_len=len+EVP_CIPHER_CTX_block_size(&ctx);
    *out= new unsigned char[*out_len];

    return EVP_CipherUpdate(&ctx, *out, out_len, (unsigned char*)data, len);
  }

  int SetAutoPadding(bool auto_padding) {
    if (!initialised_) return 0;
    return EVP_CIPHER_CTX_set_padding(&ctx, auto_padding ? 1 : 0);
  }

  // coverity[alloc_arg]
  int DecipherFinal(unsigned char** out, int *out_len) {
    int r;

    if (!initialised_) {
      *out_len = 0;
      *out = NULL;
      return 0;
    }

    *out = new unsigned char[EVP_CIPHER_CTX_block_size(&ctx)];
    r = EVP_CipherFinal_ex(&ctx,*out,out_len);
    EVP_CIPHER_CTX_cleanup(&ctx);
    initialised_ = false;
    return r;
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

    if (args.Length() <= 1
        || !args[0]->IsString()
        || !(args[1]->IsString() || Buffer::HasInstance(args[1])))
    {
      return ThrowException(Exception::Error(String::New(
        "Must give cipher-type, key as argument")));
    }

    ASSERT_IS_BUFFER(args[1]);
    ssize_t key_len = Buffer::Length(args[1]);

    if (key_len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    char* key_buf = new char[key_len];
    ssize_t key_written = DecodeWrite(key_buf, key_len, args[1], BINARY);
    assert(key_written == key_len);

    String::Utf8Value cipherType(args[0]);

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

    if (args.Length() <= 2
        || !args[0]->IsString()
        || !(args[1]->IsString() || Buffer::HasInstance(args[1]))
        || !(args[2]->IsString() || Buffer::HasInstance(args[2])))
    {
      return ThrowException(Exception::Error(String::New(
        "Must give cipher-type, key, and iv as argument")));
    }

    ASSERT_IS_BUFFER(args[1]);
    ssize_t key_len = Buffer::Length(args[1]);

    if (key_len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    ASSERT_IS_BUFFER(args[2]);
    ssize_t iv_len = Buffer::Length(args[2]);

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

    String::Utf8Value cipherType(args[0]);

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

    ASSERT_IS_BUFFER(args[0]);

    ssize_t len;

    char* buf;
    // if alloc_buf then buf must be deleted later
    bool alloc_buf = false;
    char* buffer_data = Buffer::Data(args[0]);
    size_t buffer_length = Buffer::Length(args[0]);

    buf = buffer_data;
    len = buffer_length;

    unsigned char* out=0;
    int out_len=0;
    int r = cipher->DecipherUpdate(buf, len, &out, &out_len);

    if (!r) {
      delete [] out;
      return ThrowCryptoTypeError(ERR_get_error());
    }

    Local<Value> outString;
    outString = Encode(out, out_len, BUFFER);

    if (out) delete [] out;

    if (alloc_buf) delete [] buf;
    return scope.Close(outString);

  }

  static Handle<Value> SetAutoPadding(const Arguments& args) {
    HandleScope scope;
    Decipher *cipher = ObjectWrap::Unwrap<Decipher>(args.This());

    cipher->SetAutoPadding(args.Length() < 1 || args[0]->BooleanValue());

    return Undefined();
  }

  static Handle<Value> DecipherFinal(const Arguments& args) {
    HandleScope scope;

    Decipher *cipher = ObjectWrap::Unwrap<Decipher>(args.This());

    unsigned char* out_value = NULL;
    int out_len = -1;
    Local<Value> outString;

    int r = cipher->DecipherFinal(&out_value, &out_len);

    if (out_len <= 0 || r == 0) {
      delete [] out_value; // allocated even if out_len == 0
      out_value = NULL;
      if (r == 0) return ThrowCryptoTypeError(ERR_get_error());
    }

    outString = Encode(out_value, out_len, BUFFER);
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
    if (key_len == 0) {
      HMAC_Init(&ctx, "", 0, md);
    } else {
      HMAC_Init(&ctx, key, key_len, md);
    }
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

    ASSERT_IS_BUFFER(args[1]);
    ssize_t len = Buffer::Length(args[1]);

    if (len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    String::Utf8Value hashType(args[0]);

    bool r;

    if( Buffer::HasInstance(args[1])) {
      char* buffer_data = Buffer::Data(args[1]);
      size_t buffer_length = Buffer::Length(args[1]);

      r = hmac->HmacInit(*hashType, buffer_data, buffer_length);
    } else {
      char* buf = new char[len];
      ssize_t written = DecodeWrite(buf, len, args[1], BINARY);
      assert(written == len);

      r = hmac->HmacInit(*hashType, buf, len);

      delete [] buf;
    }

    if (!r) {
      return ThrowException(Exception::Error(String::New("hmac error")));
    }

    return args.This();
  }

  static Handle<Value> HmacUpdate(const Arguments& args) {
    Hmac *hmac = ObjectWrap::Unwrap<Hmac>(args.This());

    HandleScope scope;

    ASSERT_IS_BUFFER(args[0]);

    int r;

    char* buffer_data = Buffer::Data(args[0]);
    size_t buffer_length = Buffer::Length(args[0]);

    r = hmac->HmacUpdate(buffer_data, buffer_length);

    if (!r) {
      Local<Value> exception = Exception::TypeError(String::New("HmacUpdate fail"));
      return ThrowException(exception);
    }

    return args.This();
  }

  static Handle<Value> HmacDigest(const Arguments& args) {
    Hmac *hmac = ObjectWrap::Unwrap<Hmac>(args.This());

    HandleScope scope;

    unsigned char* md_value = NULL;
    unsigned int md_len = 0;
    Local<Value> outString;

    int r = hmac->HmacDigest(&md_value, &md_len);
    if (r == 0) {
      md_value = NULL;
      md_len = 0;
    }

    outString = Encode(md_value, md_len, BUFFER);

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
    if(!md) return false;
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

    String::Utf8Value hashType(args[0]);

    Hash *hash = new Hash();
    if (!hash->HashInit(*hashType)) {
      delete hash;
      return ThrowException(Exception::Error(String::New(
        "Digest method not supported")));
    }

    hash->Wrap(args.This());
    return args.This();
  }

  static Handle<Value> HashUpdate(const Arguments& args) {
    HandleScope scope;

    Hash *hash = ObjectWrap::Unwrap<Hash>(args.This());

    ASSERT_IS_BUFFER(args[0]);

    int r;

    char* buffer_data = Buffer::Data(args[0]);
    size_t buffer_length = Buffer::Length(args[0]);
    r = hash->HashUpdate(buffer_data, buffer_length);

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

    Local<Value> outString;

    outString = Encode(md_value, md_len, BUFFER);

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

    String::Utf8Value signType(args[0]);

    bool r = sign->SignInit(*signType);

    if (!r) {
      return ThrowException(Exception::Error(String::New("SignInit error")));
    }

    return args.This();
  }

  static Handle<Value> SignUpdate(const Arguments& args) {
    Sign *sign = ObjectWrap::Unwrap<Sign>(args.This());

    HandleScope scope;

    ASSERT_IS_BUFFER(args[0]);

    int r;

    char* buffer_data = Buffer::Data(args[0]);
    size_t buffer_length = Buffer::Length(args[0]);

    r = sign->SignUpdate(buffer_data, buffer_length);

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
    Local<Value> outString;

    ASSERT_IS_BUFFER(args[0]);
    ssize_t len = Buffer::Length(args[0]);

    char* buf = new char[len];
    ssize_t written = DecodeWrite(buf, len, args[0], BUFFER);
    assert(written == len);

    md_len = 8192; // Maximum key size is 8192 bits
    md_value = new unsigned char[md_len];

    int r = sign->SignFinal(&md_value, &md_len, buf, len);
    if (r == 0) {
      md_value = NULL;
      md_len = r;
    }

    delete [] buf;

    outString = Encode(md_value, md_len, BUFFER);

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

    // Check if this is a PKCS#8 or RSA public key before trying as X.509.
    // Split this out into a separate function once we have more than one
    // consumer of public keys.
    if (strncmp(key_pem, PUBLIC_KEY_PFX, PUBLIC_KEY_PFX_LEN) == 0) {
      pkey = PEM_read_bio_PUBKEY(bp, NULL, NULL, NULL);
      if (pkey == NULL) {
        ERR_print_errors_fp(stderr);
        return 0;
      }
    } else if (strncmp(key_pem, PUBRSA_KEY_PFX, PUBRSA_KEY_PFX_LEN) == 0) {
      RSA* rsa = PEM_read_bio_RSAPublicKey(bp, NULL, NULL, NULL);
      if (rsa) {
        pkey = EVP_PKEY_new();
        if (pkey) EVP_PKEY_set1_RSA(pkey, rsa);
        RSA_free(rsa);
      }
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

    String::Utf8Value verifyType(args[0]);

    bool r = verify->VerifyInit(*verifyType);

    if (!r) {
      return ThrowException(Exception::Error(String::New("VerifyInit error")));
    }

    return args.This();
  }


  static Handle<Value> VerifyUpdate(const Arguments& args) {
    HandleScope scope;

    Verify *verify = ObjectWrap::Unwrap<Verify>(args.This());

    ASSERT_IS_BUFFER(args[0]);

    int r;

    char* buffer_data = Buffer::Data(args[0]);
    size_t buffer_length = Buffer::Length(args[0]);

    r = verify->VerifyUpdate(buffer_data, buffer_length);

    if (!r) {
      Local<Value> exception = Exception::TypeError(String::New("VerifyUpdate fail"));
      return ThrowException(exception);
    }

    return args.This();
  }


  static Handle<Value> VerifyFinal(const Arguments& args) {
    HandleScope scope;

    Verify *verify = ObjectWrap::Unwrap<Verify>(args.This());

    ASSERT_IS_BUFFER(args[0]);
    ssize_t klen = Buffer::Length(args[0]);

    if (klen < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    char* kbuf = new char[klen];
    ssize_t kwritten = DecodeWrite(kbuf, klen, args[0], BINARY);
    assert(kwritten == klen);

    ASSERT_IS_BUFFER(args[1]);
    ssize_t hlen = Buffer::Length(args[1]);

    if (hlen < 0) {
      delete [] kbuf;
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    unsigned char* hbuf = new unsigned char[hlen];
    ssize_t hwritten = DecodeWrite((char*)hbuf, hlen, args[1], BINARY);
    assert(hwritten == hlen);

    int r;

    r = verify->VerifyFinal(kbuf, klen, hbuf, hlen);

    delete [] kbuf;
    delete [] hbuf;

    return Boolean::New(r && r != -1);
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

class DiffieHellman : public ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "generateKeys", GenerateKeys);
    NODE_SET_PROTOTYPE_METHOD(t, "computeSecret", ComputeSecret);
    NODE_SET_PROTOTYPE_METHOD(t, "getPrime", GetPrime);
    NODE_SET_PROTOTYPE_METHOD(t, "getGenerator", GetGenerator);
    NODE_SET_PROTOTYPE_METHOD(t, "getPublicKey", GetPublicKey);
    NODE_SET_PROTOTYPE_METHOD(t, "getPrivateKey", GetPrivateKey);
    NODE_SET_PROTOTYPE_METHOD(t, "setPublicKey", SetPublicKey);
    NODE_SET_PROTOTYPE_METHOD(t, "setPrivateKey", SetPrivateKey);

    target->Set(String::NewSymbol("DiffieHellman"), t->GetFunction());

    Local<FunctionTemplate> t2 = FunctionTemplate::New(DiffieHellmanGroup);
    t2->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t2, "generateKeys", GenerateKeys);
    NODE_SET_PROTOTYPE_METHOD(t2, "computeSecret", ComputeSecret);
    NODE_SET_PROTOTYPE_METHOD(t2, "getPrime", GetPrime);
    NODE_SET_PROTOTYPE_METHOD(t2, "getGenerator", GetGenerator);
    NODE_SET_PROTOTYPE_METHOD(t2, "getPublicKey", GetPublicKey);
    NODE_SET_PROTOTYPE_METHOD(t2, "getPrivateKey", GetPrivateKey);

    target->Set(String::NewSymbol("DiffieHellmanGroup"), t2->GetFunction());
  }

  bool Init(int primeLength) {
    dh = DH_new();
    DH_generate_parameters_ex(dh, primeLength, DH_GENERATOR_2, 0);
    bool result = VerifyContext();
    if (!result) return false;
    initialised_ = true;
    return true;
  }

  bool Init(unsigned char* p, int p_len) {
    dh = DH_new();
    dh->p = BN_bin2bn(p, p_len, 0);
    dh->g = BN_new();
    if (!BN_set_word(dh->g, 2)) return false;
    bool result = VerifyContext();
    if (!result) return false;
    initialised_ = true;
    return true;
  }

  bool Init(unsigned char* p, int p_len, unsigned char* g, int g_len) {
    dh = DH_new();
    dh->p = BN_bin2bn(p, p_len, 0);
    dh->g = BN_bin2bn(g, g_len, 0);
    initialised_ = true;
    return true;
  }

 protected:
  static Handle<Value> DiffieHellmanGroup(const Arguments& args) {
    HandleScope scope;

    DiffieHellman* diffieHellman = new DiffieHellman();

    if (args.Length() != 1 || !args[0]->IsString()) {
      return ThrowException(Exception::Error(
          String::New("No group name given")));
    }

    String::Utf8Value group_name(args[0]);

    modp_group* it = modp_groups;

    while(it->name != NULL) {
      if (!strcasecmp(*group_name, it->name))
          break;
      it++;
    }

    if (it->name != NULL) {
      diffieHellman->Init(it->prime, it->prime_size,
              it->gen, it->gen_size);
    } else {
      return ThrowException(Exception::Error(
          String::New("Unknown group")));
    }

    diffieHellman->Wrap(args.This());

    return args.This();
  }

  static Handle<Value> New(const Arguments& args) {
    HandleScope scope;

    DiffieHellman* diffieHellman = new DiffieHellman();
    bool initialized = false;

    if (args.Length() > 0) {
      if (args[0]->IsInt32()) {
        initialized = diffieHellman->Init(args[0]->Int32Value());
      } else {
        initialized = diffieHellman->Init(
                reinterpret_cast<unsigned char*>(Buffer::Data(args[0])),
                Buffer::Length(args[0]));
      }
    }

    if (!initialized) {
      return ThrowException(Exception::Error(
            String::New("Initialization failed")));
    }

    diffieHellman->Wrap(args.This());

    return args.This();
  }

  static Handle<Value> GenerateKeys(const Arguments& args) {
    DiffieHellman* diffieHellman =
      ObjectWrap::Unwrap<DiffieHellman>(args.This());

    HandleScope scope;

    if (!diffieHellman->initialised_) {
      return ThrowException(Exception::Error(
            String::New("Not initialized")));
    }

    if (!DH_generate_key(diffieHellman->dh)) {
      return ThrowException(Exception::Error(
            String::New("Key generation failed")));
    }

    Local<Value> outString;

    int dataSize = BN_num_bytes(diffieHellman->dh->pub_key);
    char* data = new char[dataSize];
    BN_bn2bin(diffieHellman->dh->pub_key,
        reinterpret_cast<unsigned char*>(data));

    outString = Encode(data, dataSize, BUFFER);
    delete[] data;

    return scope.Close(outString);
  }

  static Handle<Value> GetPrime(const Arguments& args) {
    DiffieHellman* diffieHellman =
      ObjectWrap::Unwrap<DiffieHellman>(args.This());

    HandleScope scope;

    if (!diffieHellman->initialised_) {
      return ThrowException(Exception::Error(String::New("Not initialized")));
    }

    int dataSize = BN_num_bytes(diffieHellman->dh->p);
    char* data = new char[dataSize];
    BN_bn2bin(diffieHellman->dh->p, reinterpret_cast<unsigned char*>(data));

    Local<Value> outString;

    outString = Encode(data, dataSize, BUFFER);

    delete[] data;

    return scope.Close(outString);
  }

  static Handle<Value> GetGenerator(const Arguments& args) {
    DiffieHellman* diffieHellman =
      ObjectWrap::Unwrap<DiffieHellman>(args.This());

    HandleScope scope;

    if (!diffieHellman->initialised_) {
      return ThrowException(Exception::Error(String::New("Not initialized")));
    }

    int dataSize = BN_num_bytes(diffieHellman->dh->g);
    char* data = new char[dataSize];
    BN_bn2bin(diffieHellman->dh->g, reinterpret_cast<unsigned char*>(data));

    Local<Value> outString;

    outString = Encode(data, dataSize, BUFFER);

    delete[] data;

    return scope.Close(outString);
  }

  static Handle<Value> GetPublicKey(const Arguments& args) {
    DiffieHellman* diffieHellman =
      ObjectWrap::Unwrap<DiffieHellman>(args.This());

    HandleScope scope;

    if (!diffieHellman->initialised_) {
      return ThrowException(Exception::Error(String::New("Not initialized")));
    }

    if (diffieHellman->dh->pub_key == NULL) {
      return ThrowException(Exception::Error(
            String::New("No public key - did you forget to generate one?")));
    }

    int dataSize = BN_num_bytes(diffieHellman->dh->pub_key);
    char* data = new char[dataSize];
    BN_bn2bin(diffieHellman->dh->pub_key,
        reinterpret_cast<unsigned char*>(data));

    Local<Value> outString;

    outString = Encode(data, dataSize, BUFFER);

    delete[] data;

    return scope.Close(outString);
  }

  static Handle<Value> GetPrivateKey(const Arguments& args) {
    DiffieHellman* diffieHellman =
      ObjectWrap::Unwrap<DiffieHellman>(args.This());

    HandleScope scope;

    if (!diffieHellman->initialised_) {
      return ThrowException(Exception::Error(String::New("Not initialized")));
    }

    if (diffieHellman->dh->priv_key == NULL) {
      return ThrowException(Exception::Error(
            String::New("No private key - did you forget to generate one?")));
    }

    int dataSize = BN_num_bytes(diffieHellman->dh->priv_key);
    char* data = new char[dataSize];
    BN_bn2bin(diffieHellman->dh->priv_key,
        reinterpret_cast<unsigned char*>(data));

    Local<Value> outString;

    outString = Encode(data, dataSize, BUFFER);

    delete[] data;

    return scope.Close(outString);
  }

  static Handle<Value> ComputeSecret(const Arguments& args) {
    HandleScope scope;

    DiffieHellman* diffieHellman =
      ObjectWrap::Unwrap<DiffieHellman>(args.This());

    if (!diffieHellman->initialised_) {
      return ThrowException(Exception::Error(String::New("Not initialized")));
    }

    BIGNUM* key = 0;

    if (args.Length() == 0) {
      return ThrowException(Exception::Error(
            String::New("First argument must be other party's public key")));
    } else {
      ASSERT_IS_BUFFER(args[0]);
      key = BN_bin2bn(
        reinterpret_cast<unsigned char*>(Buffer::Data(args[0])),
        Buffer::Length(args[0]), 0);
    }

    int dataSize = DH_size(diffieHellman->dh);
    char* data = new char[dataSize];

    int size = DH_compute_key(reinterpret_cast<unsigned char*>(data),
      key, diffieHellman->dh);

    if (size == -1) {
      int checkResult;
      int checked;

      checked = DH_check_pub_key(diffieHellman->dh, key, &checkResult);
      BN_free(key);
      delete[] data;

      if (!checked) {
        return ThrowException(Exception::Error(String::New("Invalid key")));
      } else if (checkResult) {
        if (checkResult & DH_CHECK_PUBKEY_TOO_SMALL) {
          return ThrowException(Exception::Error(
                String::New("Supplied key is too small")));
        } else if (checkResult & DH_CHECK_PUBKEY_TOO_LARGE) {
          return ThrowException(Exception::Error(
                String::New("Supplied key is too large")));
        } else {
          return ThrowException(Exception::Error(String::New("Invalid key")));
        }
      } else {
        return ThrowException(Exception::Error(String::New("Invalid key")));
      }
    }

    BN_free(key);
    assert(size >= 0);

    // DH_size returns number of bytes in a prime number
    // DH_compute_key returns number of bytes in a remainder of exponent, which
    // may have less bytes than a prime number. Therefore add 0-padding to the
    // allocated buffer.
    if (size != dataSize) {
      assert(dataSize > size);
      memmove(data + dataSize - size, data, size);
      memset(data, 0, dataSize - size);
    }

    Local<Value> outString;

    outString = Encode(data, dataSize, BUFFER);

    delete[] data;
    return scope.Close(outString);
  }

  static Handle<Value> SetPublicKey(const Arguments& args) {
    HandleScope scope;

    DiffieHellman* diffieHellman =
      ObjectWrap::Unwrap<DiffieHellman>(args.This());

    if (!diffieHellman->initialised_) {
      return ThrowException(Exception::Error(String::New("Not initialized")));
    }

    if (args.Length() == 0) {
      return ThrowException(Exception::Error(
            String::New("First argument must be public key")));
    } else {
      ASSERT_IS_BUFFER(args[0]);
      diffieHellman->dh->pub_key =
        BN_bin2bn(
          reinterpret_cast<unsigned char*>(Buffer::Data(args[0])),
          Buffer::Length(args[0]), 0);
    }

    return args.This();
  }

  static Handle<Value> SetPrivateKey(const Arguments& args) {
    HandleScope scope;

    DiffieHellman* diffieHellman =
      ObjectWrap::Unwrap<DiffieHellman>(args.This());

    if (!diffieHellman->initialised_) {
      return ThrowException(Exception::Error(
            String::New("Not initialized")));
    }

    if (args.Length() == 0) {
      return ThrowException(Exception::Error(
            String::New("First argument must be private key")));
    } else {
      ASSERT_IS_BUFFER(args[0]);
      diffieHellman->dh->priv_key =
        BN_bin2bn(
          reinterpret_cast<unsigned char*>(Buffer::Data(args[0])),
          Buffer::Length(args[0]), 0);
    }

    return args.This();
  }

  DiffieHellman() : ObjectWrap() {
    initialised_ = false;
    dh = NULL;
  }

  ~DiffieHellman() {
    if (dh != NULL) {
      DH_free(dh);
    }
  }

 private:
  bool VerifyContext() {
    int codes;
    if (!DH_check(dh, &codes)) return false;
    if (codes & DH_CHECK_P_NOT_SAFE_PRIME) return false;
    if (codes & DH_CHECK_P_NOT_PRIME) return false;
    if (codes & DH_UNABLE_TO_CHECK_GENERATOR) return false;
    if (codes & DH_NOT_SUITABLE_GENERATOR) return false;
    return true;
  }

  bool initialised_;
  DH* dh;
};


struct pbkdf2_req {
  uv_work_t work_req;
  int err;
  char* pass;
  size_t passlen;
  char* salt;
  size_t saltlen;
  size_t iter;
  char* key;
  size_t keylen;
  Persistent<Object> obj;
};


void EIO_PBKDF2(pbkdf2_req* req) {
  req->err = PKCS5_PBKDF2_HMAC_SHA1(
    req->pass,
    req->passlen,
    (unsigned char*)req->salt,
    req->saltlen,
    req->iter,
    req->keylen,
    (unsigned char*)req->key);
  memset(req->pass, 0, req->passlen);
  memset(req->salt, 0, req->saltlen);
}


void EIO_PBKDF2(uv_work_t* work_req) {
  pbkdf2_req* req = container_of(work_req, pbkdf2_req, work_req);
  EIO_PBKDF2(req);
}


void EIO_PBKDF2After(pbkdf2_req* req, Local<Value> argv[2]) {
  if (req->err) {
    argv[0] = Local<Value>::New(Undefined());
    argv[1] = Encode(req->key, req->keylen, BUFFER);
    memset(req->key, 0, req->keylen);
  } else {
    argv[0] = Exception::Error(String::New("PBKDF2 error"));
    argv[1] = Local<Value>::New(Undefined());
  }

  delete[] req->pass;
  delete[] req->salt;
  delete[] req->key;
  delete req;
}


void EIO_PBKDF2After(uv_work_t* work_req, int status) {
  assert(status == 0);
  pbkdf2_req* req = container_of(work_req, pbkdf2_req, work_req);
  HandleScope scope;
  Local<Value> argv[2];
  Persistent<Object> obj = req->obj;
  EIO_PBKDF2After(req, argv);
  MakeCallback(obj, "ondone", ARRAY_SIZE(argv), argv);
  obj.Dispose();
}


Handle<Value> PBKDF2(const Arguments& args) {
  HandleScope scope;

  const char* type_error = NULL;
  char* pass = NULL;
  char* salt = NULL;
  ssize_t passlen = -1;
  ssize_t saltlen = -1;
  ssize_t keylen = -1;
  ssize_t pass_written = -1;
  ssize_t salt_written = -1;
  ssize_t iter = -1;
  pbkdf2_req* req = NULL;

  if (args.Length() != 4 && args.Length() != 5) {
    type_error = "Bad parameter";
    goto err;
  }

  ASSERT_IS_BUFFER(args[0]);
  passlen = Buffer::Length(args[0]);
  if (passlen < 0) {
    type_error = "Bad password";
    goto err;
  }

  pass = new char[passlen];
  pass_written = DecodeWrite(pass, passlen, args[0], BINARY);
  assert(pass_written == passlen);

  ASSERT_IS_BUFFER(args[1]);
  saltlen = Buffer::Length(args[1]);
  if (saltlen < 0) {
    type_error = "Bad salt";
    goto err;
  }

  salt = new char[saltlen];
  salt_written = DecodeWrite(salt, saltlen, args[1], BINARY);
  assert(salt_written == saltlen);

  if (!args[2]->IsNumber()) {
    type_error = "Iterations not a number";
    goto err;
  }

  iter = args[2]->Int32Value();
  if (iter < 0) {
    type_error = "Bad iterations";
    goto err;
  }

  if (!args[3]->IsNumber()) {
    type_error = "Key length not a number";
    goto err;
  }

  keylen = args[3]->Int32Value();
  if (keylen < 0) {
    type_error = "Bad key length";
    goto err;
  }

  req = new pbkdf2_req;
  req->err = 0;
  req->pass = pass;
  req->passlen = passlen;
  req->salt = salt;
  req->saltlen = saltlen;
  req->iter = iter;
  req->key = new char[keylen];
  req->keylen = keylen;

  if (args[4]->IsFunction()) {
    req->obj = Persistent<Object>::New(Object::New());
    req->obj->Set(String::New("ondone"), args[4]);
    uv_queue_work(uv_default_loop(),
                  &req->work_req,
                  EIO_PBKDF2,
                  EIO_PBKDF2After);
    return Undefined();
  } else {
    Local<Value> argv[2];
    EIO_PBKDF2(req);
    EIO_PBKDF2After(req, argv);
    if (argv[0]->IsObject()) return ThrowException(argv[0]);
    return scope.Close(argv[1]);
  }

err:
  delete[] salt;
  delete[] pass;
  return ThrowException(Exception::TypeError(String::New(type_error)));
}


struct RandomBytesRequest {
  ~RandomBytesRequest();
  Persistent<Object> obj_;
  unsigned long error_; // openssl error code or zero
  uv_work_t work_req_;
  size_t size_;
  char* data_;
};


RandomBytesRequest::~RandomBytesRequest() {
  if (obj_.IsEmpty()) return;
  obj_.Dispose();
  obj_.Clear();
}


void RandomBytesFree(char* data, void* hint) {
  delete[] data;
}


template <bool pseudoRandom>
void RandomBytesWork(uv_work_t* work_req) {
  RandomBytesRequest* req = container_of(work_req,
                                         RandomBytesRequest,
                                         work_req_);
  int r;

  if (pseudoRandom == true) {
    r = RAND_pseudo_bytes(reinterpret_cast<unsigned char*>(req->data_),
                          req->size_);
  } else {
    r = RAND_bytes(reinterpret_cast<unsigned char*>(req->data_), req->size_);
  }

  // RAND_bytes() returns 0 on error. RAND_pseudo_bytes() returns 0 when the
  // result is not cryptographically strong - but that's not an error.
  if (r == 0 && pseudoRandom == false) {
    req->error_ = ERR_get_error();
  } else if (r == -1) {
    req->error_ = static_cast<unsigned long>(-1);
  }
}


// don't call this function without a valid HandleScope
void RandomBytesCheck(RandomBytesRequest* req, Local<Value> argv[2]) {
  if (req->error_) {
    char errmsg[256] = "Operation not supported";

    if (req->error_ != (unsigned long) -1)
      ERR_error_string_n(req->error_, errmsg, sizeof errmsg);

    argv[0] = Exception::Error(String::New(errmsg));
    argv[1] = Local<Value>::New(Null());
  }
  else {
    // avoids the malloc + memcpy
    Buffer* buffer = Buffer::New(req->data_, req->size_, RandomBytesFree, NULL);
    argv[0] = Local<Value>::New(Null());
    argv[1] = Local<Object>::New(buffer->handle_);
  }
}


void RandomBytesAfter(uv_work_t* work_req, int status) {
  assert(status == 0);
  RandomBytesRequest* req = container_of(work_req,
                                         RandomBytesRequest,
                                         work_req_);
  HandleScope scope;
  Local<Value> argv[2];
  RandomBytesCheck(req, argv);
  MakeCallback(req->obj_, "ondone", ARRAY_SIZE(argv), argv);
  delete req;
}


template <bool pseudoRandom>
Handle<Value> RandomBytes(const Arguments& args) {
  HandleScope scope;

  // maybe allow a buffer to write to? cuts down on object creation
  // when generating random data in a loop
  if (!args[0]->IsUint32()) {
    return ThrowTypeError("Argument #1 must be number > 0");
  }

  const uint32_t size = args[0]->Uint32Value();
  if (size > Buffer::kMaxLength) {
    return ThrowTypeError("size > Buffer::kMaxLength");
  }

  RandomBytesRequest* req = new RandomBytesRequest();
  req->error_ = 0;
  req->data_ = new char[size];
  req->size_ = size;

  if (args[1]->IsFunction()) {
    req->obj_ = Persistent<Object>::New(Object::New());
    req->obj_->Set(String::New("ondone"), args[1]);

    uv_queue_work(uv_default_loop(),
                  &req->work_req_,
                  RandomBytesWork<pseudoRandom>,
                  RandomBytesAfter);

    return req->obj_;
  }
  else {
    Local<Value> argv[2];
    RandomBytesWork<pseudoRandom>(&req->work_req_);
    RandomBytesCheck(req, argv);
    delete req;

    if (!argv[0]->IsNull())
      return ThrowException(argv[0]);
    else
      return argv[1];
  }
}


Handle<Value> GetSSLCiphers(const Arguments& args) {
  HandleScope scope;

  SSL_CTX* ctx = SSL_CTX_new(TLSv1_server_method());
  if (ctx == NULL) {
    return ThrowError("SSL_CTX_new() failed.");
  }

  SSL* ssl = SSL_new(ctx);
  if (ssl == NULL) {
    SSL_CTX_free(ctx);
    return ThrowError("SSL_new() failed.");
  }

  Local<Array> arr = Array::New();
  STACK_OF(SSL_CIPHER)* ciphers = SSL_get_ciphers(ssl);

  for (int i = 0; i < sk_SSL_CIPHER_num(ciphers); ++i) {
    SSL_CIPHER* cipher = sk_SSL_CIPHER_value(ciphers, i);
    arr->Set(i, String::New(SSL_CIPHER_get_name(cipher)));
  }

  SSL_free(ssl);
  SSL_CTX_free(ctx);

  return scope.Close(arr);
}


template <class TypeName>
static void array_push_back(const TypeName* md,
                            const char* from,
                            const char* to,
                            void* arg) {
  Local<Array>& arr = *static_cast<Local<Array>*>(arg);
  arr->Set(arr->Length(), String::New(from));
}


Handle<Value> GetCiphers(const Arguments& args) {
  HandleScope scope;
  Local<Array> arr = Array::New();
  EVP_CIPHER_do_all_sorted(array_push_back<EVP_CIPHER>, &arr);
  return scope.Close(arr);
}


Handle<Value> GetHashes(const Arguments& args) {
  HandleScope scope;
  Local<Array> arr = Array::New();
  EVP_MD_do_all_sorted(array_push_back<EVP_MD>, &arr);
  return scope.Close(arr);
}


void InitCrypto(Handle<Object> target) {
  HandleScope scope;

  SSL_library_init();
  OpenSSL_add_all_algorithms();
  OpenSSL_add_all_digests();
  SSL_load_error_strings();
  ERR_load_crypto_strings();

  crypto_lock_init();
  CRYPTO_set_locking_callback(crypto_lock_cb);
  CRYPTO_THREADID_set_callback(crypto_threadid_cb);

  // Turn off compression. Saves memory - do it in userland.
#if !defined(OPENSSL_NO_COMP)
  STACK_OF(SSL_COMP)* comp_methods =
#if OPENSSL_VERSION_NUMBER < 0x00908000L
    SSL_COMP_get_compression_method()
#else
    SSL_COMP_get_compression_methods()
#endif
  ;
  sk_SSL_COMP_zero(comp_methods);
  assert(sk_SSL_COMP_num(comp_methods) == 0);
#endif

  SecureContext::Initialize(target);
  Connection::Initialize(target);
  Cipher::Initialize(target);
  Decipher::Initialize(target);
  DiffieHellman::Initialize(target);
  Hmac::Initialize(target);
  Hash::Initialize(target);
  Sign::Initialize(target);
  Verify::Initialize(target);

  NODE_SET_METHOD(target, "PBKDF2", PBKDF2);
  NODE_SET_METHOD(target, "randomBytes", RandomBytes<false>);
  NODE_SET_METHOD(target, "pseudoRandomBytes", RandomBytes<true>);
  NODE_SET_METHOD(target, "getSSLCiphers", GetSSLCiphers);
  NODE_SET_METHOD(target, "getCiphers", GetCiphers);
  NODE_SET_METHOD(target, "getHashes", GetHashes);

  subject_symbol    = NODE_PSYMBOL("subject");
  issuer_symbol     = NODE_PSYMBOL("issuer");
  valid_from_symbol = NODE_PSYMBOL("valid_from");
  valid_to_symbol   = NODE_PSYMBOL("valid_to");
  subjectaltname_symbol = NODE_PSYMBOL("subjectaltname");
  modulus_symbol        = NODE_PSYMBOL("modulus");
  exponent_symbol       = NODE_PSYMBOL("exponent");
  fingerprint_symbol   = NODE_PSYMBOL("fingerprint");
  name_symbol       = NODE_PSYMBOL("name");
  version_symbol    = NODE_PSYMBOL("version");
  ext_key_usage_symbol = NODE_PSYMBOL("ext_key_usage");
}

}  // namespace crypto
}  // namespace node

NODE_MODULE(node_crypto, node::crypto::InitCrypto)
