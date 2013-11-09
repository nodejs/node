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

#include "node.h"
#include "node_buffer.h"
#include "node_crypto.h"
#include "node_crypto_bio.h"
#include "node_crypto_groups.h"
#include "tls_wrap.h"  // TLSCallbacks

#include "env.h"
#include "env-inl.h"
#include "string_bytes.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#define strcasecmp _stricmp
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10000000L
#define OPENSSL_CONST const
#else
#define OPENSSL_CONST
#endif

#define ASSERT_IS_STRING_OR_BUFFER(val) do {                  \
    if (!Buffer::HasInstance(val) && !val->IsString()) {      \
      return ThrowTypeError("Not a string or buffer");        \
    }                                                         \
  } while (0)

#define ASSERT_IS_BUFFER(val) do {                            \
    if (!Buffer::HasInstance(val)) {                          \
      return ThrowTypeError("Not a buffer");                  \
    }                                                         \
  } while (0)

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

using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::Exception;
using v8::False;
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
using v8::ThrowException;
using v8::V8;
using v8::Value;


// Forcibly clear OpenSSL's error stack on return. This stops stale errors
// from popping up later in the lifecycle of crypto operations where they
// would cause spurious failures. It's a rather blunt method, though.
// ERR_clear_error() isn't necessarily cheap either.
struct ClearErrorOnReturn {
  ~ClearErrorOnReturn() { ERR_clear_error(); }
};

static uv_rwlock_t* locks;

const char* root_certs[] = {
#include "node_root_certs.h"  // NOLINT(build/include_order)
  NULL
};

X509_STORE* root_cert_store;

// Just to generate static methods
template class SSLWrap<TLSCallbacks>;
template void SSLWrap<TLSCallbacks>::AddMethods(Handle<FunctionTemplate> t);
template SSL_SESSION* SSLWrap<TLSCallbacks>::GetSessionCallback(
    SSL* s,
    unsigned char* key,
    int len,
    int* copy);
template int SSLWrap<TLSCallbacks>::NewSessionCallback(SSL* s,
                                                       SSL_SESSION* sess);
template void SSLWrap<TLSCallbacks>::OnClientHello(
    void* arg,
    const ClientHelloParser::ClientHello& hello);

#ifdef OPENSSL_NPN_NEGOTIATED
template int SSLWrap<TLSCallbacks>::AdvertiseNextProtoCallback(
    SSL* s,
    const unsigned char** data,
    unsigned int* len,
    void* arg);
template int SSLWrap<TLSCallbacks>::SelectNextProtoCallback(
    SSL* s,
    unsigned char** out,
    unsigned char* outlen,
    const unsigned char* in,
    unsigned int inlen,
    void* arg);
#endif


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


static int CryptoPemCallback(char *buf, int size, int rwflag, void *u) {
  if (u) {
    size_t buflen = static_cast<size_t>(size);
    size_t len = strlen(static_cast<const char*>(u));
    len = len > buflen ? buflen : len;
    memcpy(buf, u, len);
    return len;
  }

  return 0;
}


void ThrowCryptoErrorHelper(unsigned long err, bool is_type_error) {
  HandleScope scope(node_isolate);
  char errmsg[128];
  ERR_error_string_n(err, errmsg, sizeof(errmsg));
  if (is_type_error)
    ThrowTypeError(errmsg);
  else
    ThrowError(errmsg);
}


void ThrowCryptoError(unsigned long err) {
  ThrowCryptoErrorHelper(err, false);
}


void ThrowCryptoTypeError(unsigned long err) {
  ThrowCryptoErrorHelper(err, true);
}


bool EntropySource(unsigned char* buffer, size_t length) {
  // RAND_bytes() can return 0 to indicate that the entropy data is not truly
  // random. That's okay, it's still better than V8's stock source of entropy,
  // which is /dev/urandom on UNIX platforms and the current time on Windows.
  return RAND_bytes(buffer, length) != -1;
}


void SecureContext::Initialize(Environment* env, Handle<Object> target) {
  Local<FunctionTemplate> t = FunctionTemplate::New(SecureContext::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "SecureContext"));

  NODE_SET_PROTOTYPE_METHOD(t, "init", SecureContext::Init);
  NODE_SET_PROTOTYPE_METHOD(t, "setKey", SecureContext::SetKey);
  NODE_SET_PROTOTYPE_METHOD(t, "setCert", SecureContext::SetCert);
  NODE_SET_PROTOTYPE_METHOD(t, "addCACert", SecureContext::AddCACert);
  NODE_SET_PROTOTYPE_METHOD(t, "addCRL", SecureContext::AddCRL);
  NODE_SET_PROTOTYPE_METHOD(t, "addRootCerts", SecureContext::AddRootCerts);
  NODE_SET_PROTOTYPE_METHOD(t, "setCiphers", SecureContext::SetCiphers);
  NODE_SET_PROTOTYPE_METHOD(t, "setECDHCurve", SecureContext::SetECDHCurve);
  NODE_SET_PROTOTYPE_METHOD(t, "setOptions", SecureContext::SetOptions);
  NODE_SET_PROTOTYPE_METHOD(t, "setSessionIdContext",
                               SecureContext::SetSessionIdContext);
  NODE_SET_PROTOTYPE_METHOD(t, "setSessionTimeout",
                               SecureContext::SetSessionTimeout);
  NODE_SET_PROTOTYPE_METHOD(t, "close", SecureContext::Close);
  NODE_SET_PROTOTYPE_METHOD(t, "loadPKCS12", SecureContext::LoadPKCS12);
  NODE_SET_PROTOTYPE_METHOD(t, "getTicketKeys", SecureContext::GetTicketKeys);
  NODE_SET_PROTOTYPE_METHOD(t, "setTicketKeys", SecureContext::SetTicketKeys);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "SecureContext"),
              t->GetFunction());
  env->set_secure_context_constructor_template(t);
}


void SecureContext::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  new SecureContext(env, args.This());
}


void SecureContext::Init(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  OPENSSL_CONST SSL_METHOD *method = SSLv23_method();

  if (args.Length() == 1 && args[0]->IsString()) {
    const String::Utf8Value sslmethod(args[0]);

    if (strcmp(*sslmethod, "SSLv2_method") == 0) {
#ifndef OPENSSL_NO_SSL2
      method = SSLv2_method();
#else
      return ThrowError("SSLv2 methods disabled");
#endif
    } else if (strcmp(*sslmethod, "SSLv2_server_method") == 0) {
#ifndef OPENSSL_NO_SSL2
      method = SSLv2_server_method();
#else
      return ThrowError("SSLv2 methods disabled");
#endif
    } else if (strcmp(*sslmethod, "SSLv2_client_method") == 0) {
#ifndef OPENSSL_NO_SSL2
      method = SSLv2_client_method();
#else
      return ThrowError("SSLv2 methods disabled");
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
    } else if (strcmp(*sslmethod, "TLSv1_1_method") == 0) {
      method = TLSv1_1_method();
    } else if (strcmp(*sslmethod, "TLSv1_1_server_method") == 0) {
      method = TLSv1_1_server_method();
    } else if (strcmp(*sslmethod, "TLSv1_1_client_method") == 0) {
      method = TLSv1_1_client_method();
    } else if (strcmp(*sslmethod, "TLSv1_2_method") == 0) {
      method = TLSv1_2_method();
    } else if (strcmp(*sslmethod, "TLSv1_2_server_method") == 0) {
      method = TLSv1_2_server_method();
    } else if (strcmp(*sslmethod, "TLSv1_2_client_method") == 0) {
      method = TLSv1_2_client_method();
    } else {
      return ThrowError("Unknown method");
    }
  }

  sc->ctx_ = SSL_CTX_new(method);

  // SSL session cache configuration
  SSL_CTX_set_session_cache_mode(sc->ctx_,
                                 SSL_SESS_CACHE_SERVER |
                                 SSL_SESS_CACHE_NO_INTERNAL |
                                 SSL_SESS_CACHE_NO_AUTO_CLEAR);
  SSL_CTX_sess_set_get_cb(sc->ctx_, SSLWrap<Connection>::GetSessionCallback);
  SSL_CTX_sess_set_new_cb(sc->ctx_, SSLWrap<Connection>::NewSessionCallback);

  sc->ca_store_ = NULL;
}


// Takes a string or buffer and loads it into a BIO.
// Caller responsible for BIO_free_all-ing the returned object.
static BIO* LoadBIO(Handle<Value> v) {
  BIO* bio = NodeBIO::New();
  if (!bio)
    return NULL;

  HandleScope scope(node_isolate);

  int r = -1;

  if (v->IsString()) {
    const String::Utf8Value s(v);
    r = BIO_write(bio, *s, s.length());
  } else if (Buffer::HasInstance(v)) {
    char* buffer_data = Buffer::Data(v);
    size_t buffer_length = Buffer::Length(v);
    r = BIO_write(bio, buffer_data, buffer_length);
  }

  if (r <= 0) {
    BIO_free_all(bio);
    return NULL;
  }

  return bio;
}


// Takes a string or buffer and loads it into an X509
// Caller responsible for X509_free-ing the returned object.
static X509* LoadX509(Handle<Value> v) {
  HandleScope scope(node_isolate);

  BIO *bio = LoadBIO(v);
  if (!bio)
    return NULL;

  X509 * x509 = PEM_read_bio_X509(bio, NULL, CryptoPemCallback, NULL);
  if (!x509) {
    BIO_free_all(bio);
    return NULL;
  }

  BIO_free_all(bio);
  return x509;
}


void SecureContext::SetKey(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  unsigned int len = args.Length();
  if (len != 1 && len != 2) {
    return ThrowTypeError("Bad parameter");
  }
  if (len == 2 && !args[1]->IsString()) {
    return ThrowTypeError("Bad parameter");
  }

  BIO *bio = LoadBIO(args[0]);
  if (!bio)
    return;

  String::Utf8Value passphrase(args[1]);

  EVP_PKEY* key = PEM_read_bio_PrivateKey(bio,
                                          NULL,
                                          CryptoPemCallback,
                                          len == 1 ? NULL : *passphrase);

  if (!key) {
    BIO_free_all(bio);
    unsigned long err = ERR_get_error();
    if (!err) {
      return ThrowError("PEM_read_bio_PrivateKey");
    }
    return ThrowCryptoError(err);
  }

  SSL_CTX_use_PrivateKey(sc->ctx_, key);
  EVP_PKEY_free(key);
  BIO_free_all(bio);
}


// Read a file that contains our certificate in "PEM" format,
// possibly followed by a sequence of CA certificates that should be
// sent to the peer in the Certificate message.
//
// Taken from OpenSSL - editted for style.
int SSL_CTX_use_certificate_chain(SSL_CTX *ctx, BIO *in) {
  int ret = 0;
  X509 *x = NULL;

  x = PEM_read_bio_X509_AUX(in, NULL, CryptoPemCallback, NULL);

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

    while ((ca = PEM_read_bio_X509(in, NULL, CryptoPemCallback, NULL))) {
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
  if (x != NULL)
    X509_free(x);
  return ret;
}


void SecureContext::SetCert(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  if (args.Length() != 1) {
    return ThrowTypeError("Bad parameter");
  }

  BIO* bio = LoadBIO(args[0]);
  if (!bio)
    return;

  int rv = SSL_CTX_use_certificate_chain(sc->ctx_, bio);

  BIO_free_all(bio);

  if (!rv) {
    unsigned long err = ERR_get_error();
    if (!err) {
      return ThrowError("SSL_CTX_use_certificate_chain");
    }
    return ThrowCryptoError(err);
  }
}


void SecureContext::AddCACert(const FunctionCallbackInfo<Value>& args) {
  bool newCAStore = false;
  HandleScope scope(node_isolate);

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  if (args.Length() != 1) {
    return ThrowTypeError("Bad parameter");
  }

  if (!sc->ca_store_) {
    sc->ca_store_ = X509_STORE_new();
    newCAStore = true;
  }

  X509* x509 = LoadX509(args[0]);
  if (!x509)
    return;

  X509_STORE_add_cert(sc->ca_store_, x509);
  SSL_CTX_add_client_CA(sc->ctx_, x509);

  X509_free(x509);

  if (newCAStore) {
    SSL_CTX_set_cert_store(sc->ctx_, sc->ca_store_);
  }
}


void SecureContext::AddCRL(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  if (args.Length() != 1) {
    return ThrowTypeError("Bad parameter");
  }

  ClearErrorOnReturn clear_error_on_return;
  (void) &clear_error_on_return;  // Silence compiler warning.

  BIO *bio = LoadBIO(args[0]);
  if (!bio)
    return;

  X509_CRL *x509 = PEM_read_bio_X509_CRL(bio, NULL, CryptoPemCallback, NULL);

  if (x509 == NULL) {
    BIO_free_all(bio);
    return;
  }

  X509_STORE_add_crl(sc->ca_store_, x509);
  X509_STORE_set_flags(sc->ca_store_, X509_V_FLAG_CRL_CHECK |
                                      X509_V_FLAG_CRL_CHECK_ALL);
  BIO_free_all(bio);
  X509_CRL_free(x509);
}



void SecureContext::AddRootCerts(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  assert(sc->ca_store_ == NULL);

  if (!root_cert_store) {
    root_cert_store = X509_STORE_new();

    for (int i = 0; root_certs[i]; i++) {
      BIO* bp = NodeBIO::New();

      if (!BIO_write(bp, root_certs[i], strlen(root_certs[i]))) {
        BIO_free_all(bp);
        return;
      }

      X509 *x509 = PEM_read_bio_X509(bp, NULL, CryptoPemCallback, NULL);

      if (x509 == NULL) {
        BIO_free_all(bp);
        return;
      }

      X509_STORE_add_cert(root_cert_store, x509);

      BIO_free_all(bp);
      X509_free(x509);
    }
  }

  sc->ca_store_ = root_cert_store;
  SSL_CTX_set_cert_store(sc->ctx_, sc->ca_store_);
}


void SecureContext::SetCiphers(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  if (args.Length() != 1 || !args[0]->IsString()) {
    return ThrowTypeError("Bad parameter");
  }

  const String::Utf8Value ciphers(args[0]);
  SSL_CTX_set_cipher_list(sc->ctx_, *ciphers);
}


void SecureContext::SetECDHCurve(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  if (args.Length() != 1 || !args[0]->IsString())
    return ThrowTypeError("First argument should be a string");

  String::Utf8Value curve(args[0]);

  int nid = OBJ_sn2nid(*curve);

  if (nid == NID_undef)
    return ThrowTypeError("First argument should be a valid curve name");

  EC_KEY* ecdh = EC_KEY_new_by_curve_name(nid);

  if (ecdh == NULL)
    return ThrowTypeError("First argument should be a valid curve name");

  SSL_CTX_set_options(sc->ctx_, SSL_OP_SINGLE_ECDH_USE);
  SSL_CTX_set_tmp_ecdh(sc->ctx_, ecdh);

  EC_KEY_free(ecdh);
}


void SecureContext::SetOptions(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  if (args.Length() != 1 || !args[0]->IntegerValue()) {
    return ThrowTypeError("Bad parameter");
  }

  SSL_CTX_set_options(sc->ctx_, args[0]->IntegerValue());
}


void SecureContext::SetSessionIdContext(
    const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  if (args.Length() != 1 || !args[0]->IsString()) {
    return ThrowTypeError("Bad parameter");
  }

  const String::Utf8Value sessionIdContext(args[0]);
  const unsigned char* sid_ctx =
      reinterpret_cast<const unsigned char*>(*sessionIdContext);
  unsigned int sid_ctx_len = sessionIdContext.length();

  int r = SSL_CTX_set_session_id_context(sc->ctx_, sid_ctx, sid_ctx_len);
  if (r == 1)
    return;

  BIO* bio;
  BUF_MEM* mem;
  Local<String> message;

  bio = BIO_new(BIO_s_mem());
  if (bio == NULL) {
    message = FIXED_ONE_BYTE_STRING(node_isolate,
                                    "SSL_CTX_set_session_id_context error");
  } else {
    ERR_print_errors(bio);
    BIO_get_mem_ptr(bio, &mem);
    message = OneByteString(node_isolate, mem->data, mem->length);
    BIO_free_all(bio);
  }

  ThrowException(Exception::TypeError(message));
}


void SecureContext::SetSessionTimeout(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  if (args.Length() != 1 || !args[0]->IsInt32()) {
    return ThrowTypeError("Bad parameter");
  }

  int32_t sessionTimeout = args[0]->Int32Value();
  SSL_CTX_set_timeout(sc->ctx_, sessionTimeout);
}


void SecureContext::Close(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  SecureContext* sc = Unwrap<SecureContext>(args.This());
  sc->FreeCTXMem();
}


// Takes .pfx or .p12 and password in string or buffer format
void SecureContext::LoadPKCS12(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  BIO* in = NULL;
  PKCS12* p12 = NULL;
  EVP_PKEY* pkey = NULL;
  X509* cert = NULL;
  STACK_OF(X509)* extraCerts = NULL;
  char* pass = NULL;
  bool ret = false;

  SecureContext* sc = Unwrap<SecureContext>(args.This());

  if (args.Length() < 1) {
    return ThrowTypeError("Bad parameter");
  }

  in = LoadBIO(args[0]);
  if (in == NULL) {
    return ThrowError("Unable to load BIO");
  }

  if (args.Length() >= 2) {
    ASSERT_IS_BUFFER(args[1]);

    int passlen = Buffer::Length(args[1]);
    if (passlen < 0) {
      BIO_free_all(in);
      return ThrowTypeError("Bad password");
    }
    pass = new char[passlen + 1];
    int pass_written = DecodeWrite(pass, passlen, args[1], BINARY);

    assert(pass_written == passlen);
    pass[passlen] = '\0';
  }

  if (d2i_PKCS12_bio(in, &p12) &&
      PKCS12_parse(p12, pass, &pkey, &cert, &extraCerts) &&
      SSL_CTX_use_certificate(sc->ctx_, cert) &&
      SSL_CTX_use_PrivateKey(sc->ctx_, pkey)) {
    // set extra certs
    while (X509* x509 = sk_X509_pop(extraCerts)) {
      if (!sc->ca_store_) {
        sc->ca_store_ = X509_STORE_new();
        SSL_CTX_set_cert_store(sc->ctx_, sc->ca_store_);
      }

      X509_STORE_add_cert(sc->ca_store_, x509);
      SSL_CTX_add_client_CA(sc->ctx_, x509);
      X509_free(x509);
    }

    EVP_PKEY_free(pkey);
    X509_free(cert);
    sk_X509_free(extraCerts);

    ret = true;
  }

  PKCS12_free(p12);
  BIO_free_all(in);
  delete[] pass;

  if (!ret) {
    unsigned long err = ERR_get_error();
    const char* str = ERR_reason_error_string(err);
    return ThrowError(str);
  }
}


void SecureContext::GetTicketKeys(const FunctionCallbackInfo<Value>& args) {
#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_get_tlsext_ticket_keys)
  HandleScope handle_scope(args.GetIsolate());

  SecureContext* wrap = Unwrap<SecureContext>(args.This());

  Local<Object> buff = Buffer::New(wrap->env(), 48);
  if (SSL_CTX_get_tlsext_ticket_keys(wrap->ctx_,
                                     Buffer::Data(buff),
                                     Buffer::Length(buff)) != 1) {
    return ThrowError("Failed to fetch tls ticket keys");
  }

  args.GetReturnValue().Set(buff);
#endif  // !def(OPENSSL_NO_TLSEXT) && def(SSL_CTX_get_tlsext_ticket_keys)
}


void SecureContext::SetTicketKeys(const FunctionCallbackInfo<Value>& args) {
#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_get_tlsext_ticket_keys)
  HandleScope scope(node_isolate);

  if (args.Length() < 1 ||
      !Buffer::HasInstance(args[0]) ||
      Buffer::Length(args[0]) != 48) {
    return ThrowTypeError("Bad argument");
  }

  SecureContext* wrap = Unwrap<SecureContext>(args.This());

  if (SSL_CTX_set_tlsext_ticket_keys(wrap->ctx_,
                                     Buffer::Data(args[0]),
                                     Buffer::Length(args[0])) != 1) {
    return ThrowError("Failed to fetch tls ticket keys");
  }

  args.GetReturnValue().Set(true);
#endif  // !def(OPENSSL_NO_TLSEXT) && def(SSL_CTX_get_tlsext_ticket_keys)
}


template <class Base>
void SSLWrap<Base>::AddMethods(Handle<FunctionTemplate> t) {
  HandleScope scope(node_isolate);

  NODE_SET_PROTOTYPE_METHOD(t, "getPeerCertificate", GetPeerCertificate);
  NODE_SET_PROTOTYPE_METHOD(t, "getSession", GetSession);
  NODE_SET_PROTOTYPE_METHOD(t, "setSession", SetSession);
  NODE_SET_PROTOTYPE_METHOD(t, "loadSession", LoadSession);
  NODE_SET_PROTOTYPE_METHOD(t, "isSessionReused", IsSessionReused);
  NODE_SET_PROTOTYPE_METHOD(t, "isInitFinished", IsInitFinished);
  NODE_SET_PROTOTYPE_METHOD(t, "verifyError", VerifyError);
  NODE_SET_PROTOTYPE_METHOD(t, "getCurrentCipher", GetCurrentCipher);
  NODE_SET_PROTOTYPE_METHOD(t, "receivedShutdown", ReceivedShutdown);
  NODE_SET_PROTOTYPE_METHOD(t, "endParser", EndParser);
  NODE_SET_PROTOTYPE_METHOD(t, "renegotiate", Renegotiate);

#ifdef OPENSSL_NPN_NEGOTIATED
  NODE_SET_PROTOTYPE_METHOD(t, "getNegotiatedProtocol", GetNegotiatedProto);
  NODE_SET_PROTOTYPE_METHOD(t, "setNPNProtocols", SetNPNProtocols);
#endif  // OPENSSL_NPN_NEGOTIATED
}


template <class Base>
SSL_SESSION* SSLWrap<Base>::GetSessionCallback(SSL* s,
                                               unsigned char* key,
                                               int len,
                                               int* copy) {
  HandleScope scope(node_isolate);

  Base* w = static_cast<Base*>(SSL_get_app_data(s));

  *copy = 0;
  SSL_SESSION* sess = w->next_sess_;
  w->next_sess_ = NULL;

  return sess;
}


template <class Base>
int SSLWrap<Base>::NewSessionCallback(SSL* s, SSL_SESSION* sess) {
  HandleScope scope(node_isolate);

  Base* w = static_cast<Base*>(SSL_get_app_data(s));
  Environment* env = w->ssl_env();

  if (!w->session_callbacks_)
    return 0;

  // Check if session is small enough to be stored
  int size = i2d_SSL_SESSION(sess, NULL);
  if (size > SecureContext::kMaxSessionSize)
    return 0;

  // Serialize session
  Local<Object> buff = Buffer::New(env, size);
  unsigned char* serialized = reinterpret_cast<unsigned char*>(
      Buffer::Data(buff));
  memset(serialized, 0, size);
  i2d_SSL_SESSION(sess, &serialized);

  Local<Object> session = Buffer::New(env,
                                      reinterpret_cast<char*>(sess->session_id),
                                      sess->session_id_length);
  Local<Value> argv[] = { session, buff };
  w->MakeCallback(env->onnewsession_string(), ARRAY_SIZE(argv), argv);

  return 0;
}


template <class Base>
void SSLWrap<Base>::OnClientHello(void* arg,
                                  const ClientHelloParser::ClientHello& hello) {
  HandleScope scope(node_isolate);

  Base* w = static_cast<Base*>(arg);
  Environment* env = w->ssl_env();

  Local<Object> hello_obj = Object::New();
  Local<Object> buff = Buffer::New(
      env,
      reinterpret_cast<const char*>(hello.session_id()),
      hello.session_size());
  hello_obj->Set(env->session_id_string(), buff);
  if (hello.servername() == NULL) {
    hello_obj->Set(env->servername_string(), String::Empty(node_isolate));
  } else {
    Local<String> servername = OneByteString(node_isolate,
                                             hello.servername(),
                                             hello.servername_size());
    hello_obj->Set(env->servername_string(), servername);
  }
  hello_obj->Set(env->tls_ticket_string(), Boolean::New(hello.has_ticket()));

  Local<Value> argv[] = { hello_obj };
  w->MakeCallback(env->onclienthello_string(), ARRAY_SIZE(argv), argv);
}


// TODO(indutny): Split it into multiple smaller functions
template <class Base>
void SSLWrap<Base>::GetPeerCertificate(
    const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Base* w = Unwrap<Base>(args.This());
  Environment* env = w->ssl_env();

  Local<Object> info = Object::New();
  X509* peer_cert = SSL_get_peer_certificate(w->ssl_);
  if (peer_cert != NULL) {
    BIO* bio = BIO_new(BIO_s_mem());
    BUF_MEM* mem;
    if (X509_NAME_print_ex(bio,
                           X509_get_subject_name(peer_cert),
                           0,
                           X509_NAME_FLAGS) > 0) {
      BIO_get_mem_ptr(bio, &mem);
      info->Set(env->subject_string(),
                OneByteString(node_isolate, mem->data, mem->length));
    }
    (void) BIO_reset(bio);

    X509_NAME* issuer_name = X509_get_issuer_name(peer_cert);
    if (X509_NAME_print_ex(bio, issuer_name, 0, X509_NAME_FLAGS) > 0) {
      BIO_get_mem_ptr(bio, &mem);
      info->Set(env->issuer_string(),
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
      info->Set(env->subjectaltname_string(),
                OneByteString(node_isolate, mem->data, mem->length));

      (void) BIO_reset(bio);
    }

    EVP_PKEY* pkey = X509_get_pubkey(peer_cert);
    RSA* rsa = NULL;
    if (pkey != NULL)
      rsa = EVP_PKEY_get1_RSA(pkey);

    if (rsa != NULL) {
        BN_print(bio, rsa->n);
        BIO_get_mem_ptr(bio, &mem);
        info->Set(env->modulus_string(),
                  OneByteString(node_isolate, mem->data, mem->length));
        (void) BIO_reset(bio);

        BN_print(bio, rsa->e);
        BIO_get_mem_ptr(bio, &mem);
        info->Set(env->exponent_string(),
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
    info->Set(env->valid_from_string(),
              OneByteString(node_isolate, mem->data, mem->length));
    (void) BIO_reset(bio);

    ASN1_TIME_print(bio, X509_get_notAfter(peer_cert));
    BIO_get_mem_ptr(bio, &mem);
    info->Set(env->valid_to_string(),
              OneByteString(node_isolate, mem->data, mem->length));
    BIO_free_all(bio);

    unsigned int md_size, i;
    unsigned char md[EVP_MAX_MD_SIZE];
    if (X509_digest(peer_cert, EVP_sha1(), md, &md_size)) {
      const char hex[] = "0123456789ABCDEF";
      char fingerprint[EVP_MAX_MD_SIZE * 3];

      // TODO(indutny): Unify it with buffer's code
      for (i = 0; i < md_size; i++) {
        fingerprint[3*i] = hex[(md[i] & 0xf0) >> 4];
        fingerprint[(3*i)+1] = hex[(md[i] & 0x0f)];
        fingerprint[(3*i)+2] = ':';
      }

      if (md_size > 0) {
        fingerprint[(3*(md_size-1))+2] = '\0';
      } else {
        fingerprint[0] = '\0';
      }

      info->Set(env->fingerprint_string(),
                OneByteString(node_isolate, fingerprint));
    }

    STACK_OF(ASN1_OBJECT)* eku = static_cast<STACK_OF(ASN1_OBJECT)*>(
        X509_get_ext_d2i(peer_cert, NID_ext_key_usage, NULL, NULL));
    if (eku != NULL) {
      Local<Array> ext_key_usage = Array::New();
      char buf[256];

      int j = 0;
      for (int i = 0; i < sk_ASN1_OBJECT_num(eku); i++) {
        if (OBJ_obj2txt(buf, sizeof(buf), sk_ASN1_OBJECT_value(eku, i), 1) >= 0)
          ext_key_usage->Set(j++, OneByteString(node_isolate, buf));
      }

      sk_ASN1_OBJECT_pop_free(eku, ASN1_OBJECT_free);
      info->Set(env->ext_key_usage_string(), ext_key_usage);
    }

    X509_free(peer_cert);
  }

  args.GetReturnValue().Set(info);
}


template <class Base>
void SSLWrap<Base>::GetSession(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Base* w = Unwrap<Base>(args.This());

  SSL_SESSION* sess = SSL_get_session(w->ssl_);
  if (sess == NULL)
    return;

  int slen = i2d_SSL_SESSION(sess, NULL);
  assert(slen > 0);

  unsigned char* sbuf = new unsigned char[slen];
  unsigned char* p = sbuf;
  i2d_SSL_SESSION(sess, &p);
  args.GetReturnValue().Set(Encode(sbuf, slen, BINARY));
  delete[] sbuf;
}


template <class Base>
void SSLWrap<Base>::SetSession(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Base* w = Unwrap<Base>(args.This());

  if (args.Length() < 1 ||
      (!args[0]->IsString() && !Buffer::HasInstance(args[0]))) {
    return ThrowTypeError("Bad argument");
  }

  ASSERT_IS_BUFFER(args[0]);
  ssize_t slen = Buffer::Length(args[0]);

  if (slen < 0)
    return ThrowTypeError("Bad argument");

  char* sbuf = new char[slen];

  ssize_t wlen = DecodeWrite(sbuf, slen, args[0], BINARY);
  assert(wlen == slen);

  const unsigned char* p = reinterpret_cast<const unsigned char*>(sbuf);
  SSL_SESSION* sess = d2i_SSL_SESSION(NULL, &p, wlen);

  delete[] sbuf;

  if (sess == NULL)
    return;

  int r = SSL_set_session(w->ssl_, sess);
  SSL_SESSION_free(sess);

  if (!r)
    return ThrowError("SSL_set_session error");
}


template <class Base>
void SSLWrap<Base>::LoadSession(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Base* w = Unwrap<Base>(args.This());
  Environment* env = w->ssl_env();

  if (args.Length() >= 1 && Buffer::HasInstance(args[0])) {
    ssize_t slen = Buffer::Length(args[0]);
    char* sbuf = Buffer::Data(args[0]);

    const unsigned char* p = reinterpret_cast<unsigned char*>(sbuf);
    SSL_SESSION* sess = d2i_SSL_SESSION(NULL, &p, slen);

    // Setup next session and move hello to the BIO buffer
    if (w->next_sess_ != NULL)
      SSL_SESSION_free(w->next_sess_);
    w->next_sess_ = sess;

    Local<Object> info = Object::New();
#ifndef OPENSSL_NO_TLSEXT
    if (sess->tlsext_hostname == NULL) {
      info->Set(env->servername_string(), False(node_isolate));
    } else {
      info->Set(env->servername_string(),
                OneByteString(node_isolate, sess->tlsext_hostname));
    }
#endif
    args.GetReturnValue().Set(info);
  }
}


template <class Base>
void SSLWrap<Base>::IsSessionReused(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  Base* w = Unwrap<Base>(args.This());
  bool yes = SSL_session_reused(w->ssl_);
  args.GetReturnValue().Set(yes);
}


template <class Base>
void SSLWrap<Base>::ReceivedShutdown(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  Base* w = Unwrap<Base>(args.This());
  bool yes = SSL_get_shutdown(w->ssl_) == SSL_RECEIVED_SHUTDOWN;
  args.GetReturnValue().Set(yes);
}


template <class Base>
void SSLWrap<Base>::EndParser(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  Base* w = Unwrap<Base>(args.This());
  w->hello_parser_.End();
}


template <class Base>
void SSLWrap<Base>::Renegotiate(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Base* w = Unwrap<Base>(args.This());

  ClearErrorOnReturn clear_error_on_return;
  (void) &clear_error_on_return;  // Silence unused variable warning.

  bool yes = SSL_renegotiate(w->ssl_) == 1;
  args.GetReturnValue().Set(yes);
}


template <class Base>
void SSLWrap<Base>::IsInitFinished(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  Base* w = Unwrap<Base>(args.This());
  bool yes = SSL_is_init_finished(w->ssl_);
  args.GetReturnValue().Set(yes);
}


#define CASE_X509_ERR(CODE) case X509_V_ERR_##CODE: reason = #CODE; break;
template <class Base>
void SSLWrap<Base>::VerifyError(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Base* w = Unwrap<Base>(args.This());

  // XXX(indutny) Do this check in JS land?
  X509* peer_cert = SSL_get_peer_certificate(w->ssl_);
  if (peer_cert == NULL) {
    // We requested a certificate and they did not send us one.
    // Definitely an error.
    // XXX(indutny) is this the right error message?
    Local<String> s =
        FIXED_ONE_BYTE_STRING(node_isolate, "UNABLE_TO_GET_ISSUER_CERT");
    return args.GetReturnValue().Set(Exception::Error(s));
  }
  X509_free(peer_cert);

  long x509_verify_error = SSL_get_verify_result(w->ssl_);

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

  if (s.IsEmpty())
    s = OneByteString(node_isolate, reason);

  args.GetReturnValue().Set(Exception::Error(s));
}
#undef CASE_X509_ERR


template <class Base>
void SSLWrap<Base>::GetCurrentCipher(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Base* w = Unwrap<Base>(args.This());
  Environment* env = w->ssl_env();

  OPENSSL_CONST SSL_CIPHER* c = SSL_get_current_cipher(w->ssl_);
  if (c == NULL)
    return;

  Local<Object> info = Object::New();
  const char* cipher_name = SSL_CIPHER_get_name(c);
  info->Set(env->name_string(), OneByteString(node_isolate, cipher_name));
  const char* cipher_version = SSL_CIPHER_get_version(c);
  info->Set(env->version_string(), OneByteString(node_isolate, cipher_version));
  args.GetReturnValue().Set(info);
}


#ifdef OPENSSL_NPN_NEGOTIATED
template <class Base>
int SSLWrap<Base>::AdvertiseNextProtoCallback(SSL* s,
                                              const unsigned char** data,
                                              unsigned int* len,
                                              void* arg) {
  Base* w = static_cast<Base*>(arg);

  if (w->npn_protos_.IsEmpty()) {
    // No initialization - no NPN protocols
    *data = reinterpret_cast<const unsigned char*>("");
    *len = 0;
  } else {
    Local<Object> obj = PersistentToLocal(node_isolate, w->npn_protos_);
    *data = reinterpret_cast<const unsigned char*>(Buffer::Data(obj));
    *len = Buffer::Length(obj);
  }

  return SSL_TLSEXT_ERR_OK;
}


template <class Base>
int SSLWrap<Base>::SelectNextProtoCallback(SSL* s,
                                           unsigned char** out,
                                           unsigned char* outlen,
                                           const unsigned char* in,
                                           unsigned int inlen,
                                           void* arg) {
  Base* w = static_cast<Base*>(arg);

  // Release old protocol handler if present
  w->selected_npn_proto_.Dispose();

  if (w->npn_protos_.IsEmpty()) {
    // We should at least select one protocol
    // If server is using NPN
    *out = reinterpret_cast<unsigned char*>(const_cast<char*>("http/1.1"));
    *outlen = 8;

    // set status: unsupported
    w->selected_npn_proto_.Reset(node_isolate, False(node_isolate));

    return SSL_TLSEXT_ERR_OK;
  }

  Local<Object> obj = PersistentToLocal(node_isolate, w->npn_protos_);
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
    w->selected_npn_proto_.Reset(node_isolate, result);

  return SSL_TLSEXT_ERR_OK;
}


template <class Base>
void SSLWrap<Base>::GetNegotiatedProto(
    const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Base* w = Unwrap<Base>(args.This());

  if (w->is_client()) {
    if (w->selected_npn_proto_.IsEmpty() == false) {
      args.GetReturnValue().Set(w->selected_npn_proto_);
    }
    return;
  }

  const unsigned char* npn_proto;
  unsigned int npn_proto_len;

  SSL_get0_next_proto_negotiated(w->ssl_, &npn_proto, &npn_proto_len);

  if (!npn_proto)
    return args.GetReturnValue().Set(false);

  args.GetReturnValue().Set(
      OneByteString(node_isolate, npn_proto, npn_proto_len));
}


template <class Base>
void SSLWrap<Base>::SetNPNProtocols(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Base* w = Unwrap<Base>(args.This());

  if (args.Length() < 1 || !Buffer::HasInstance(args[0]))
    return ThrowTypeError("Must give a Buffer as first argument");

  w->npn_protos_.Reset(node_isolate, args[0].As<Object>());
}
#endif  // OPENSSL_NPN_NEGOTIATED


void Connection::OnClientHelloParseEnd(void* arg) {
  Connection* conn = static_cast<Connection*>(arg);

  // Write all accumulated data
  int r = BIO_write(conn->bio_read_,
                    reinterpret_cast<char*>(conn->hello_data_),
                    conn->hello_offset_);
  conn->HandleBIOError(conn->bio_read_, "BIO_write", r);
  conn->SetShutdownFlags();
}


#ifdef SSL_PRINT_DEBUG
# define DEBUG_PRINT(...) fprintf (stderr, __VA_ARGS__)
#else
# define DEBUG_PRINT(...)
#endif


int Connection::HandleBIOError(BIO *bio, const char* func, int rv) {
  if (rv >= 0)
    return rv;

  int retry = BIO_should_retry(bio);
  (void) retry;  // unused if !defined(SSL_PRINT_DEBUG)

  if (BIO_should_write(bio)) {
    DEBUG_PRINT("[%p] BIO: %s want write. should retry %d\n",
                ssl_,
                func,
                retry);
    return 0;

  } else if (BIO_should_read(bio)) {
    DEBUG_PRINT("[%p] BIO: %s want read. should retry %d\n", ssl_, func, retry);
    return 0;

  } else {
    char ssl_error_buf[512];
    ERR_error_string_n(rv, ssl_error_buf, sizeof(ssl_error_buf));

    HandleScope scope(node_isolate);
    Local<Value> exception =
        Exception::Error(OneByteString(node_isolate, ssl_error_buf));
    object()->Set(FIXED_ONE_BYTE_STRING(node_isolate, "error"), exception);

    DEBUG_PRINT("[%p] BIO: %s failed: (%d) %s\n",
                ssl_,
                func,
                rv,
                ssl_error_buf);

    return rv;
  }

  return 0;
}


int Connection::HandleSSLError(const char* func,
                               int rv,
                               ZeroStatus zs,
                               SyscallStatus ss) {
  ClearErrorOnReturn clear_error_on_return;
  (void) &clear_error_on_return;  // Silence unused variable warning.

  if (rv > 0)
    return rv;
  if (rv == 0 && zs == kZeroIsNotAnError)
    return rv;

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
    Local<Value> exception =
        Exception::Error(FIXED_ONE_BYTE_STRING(node_isolate, "ZERO_RETURN"));
    object()->Set(FIXED_ONE_BYTE_STRING(node_isolate, "error"), exception);
    return rv;

  } else if (err == SSL_ERROR_SYSCALL && ss == kIgnoreSyscall) {
    return 0;

  } else {
    HandleScope scope(node_isolate);
    BUF_MEM* mem;
    BIO *bio;

    assert(err == SSL_ERROR_SSL || err == SSL_ERROR_SYSCALL);

    // XXX We need to drain the error queue for this thread or else OpenSSL
    // has the possibility of blocking connections? This problem is not well
    // understood. And we should be somehow propagating these errors up
    // into JavaScript. There is no test which demonstrates this problem.
    // https://github.com/joyent/node/issues/1719
    bio = BIO_new(BIO_s_mem());
    if (bio != NULL) {
      ERR_print_errors(bio);
      BIO_get_mem_ptr(bio, &mem);
      Local<Value> exception =
          Exception::Error(OneByteString(node_isolate, mem->data, mem->length));
      object()->Set(FIXED_ONE_BYTE_STRING(node_isolate, "error"), exception);
      BIO_free_all(bio);
    }

    return rv;
  }

  return 0;
}


void Connection::ClearError() {
#ifndef NDEBUG
  HandleScope scope(node_isolate);

  // We should clear the error in JS-land
  Local<String> error_key = FIXED_ONE_BYTE_STRING(node_isolate, "error");
  Local<Value> error = object()->Get(error_key);
  assert(error->BooleanValue() == false);
#endif  // NDEBUG
}


void Connection::SetShutdownFlags() {
  HandleScope scope(node_isolate);

  int flags = SSL_get_shutdown(ssl_);

  if (flags & SSL_SENT_SHUTDOWN) {
    Local<String> sent_shutdown_key =
        FIXED_ONE_BYTE_STRING(node_isolate, "sentShutdown");
    object()->Set(sent_shutdown_key, True(node_isolate));
  }

  if (flags & SSL_RECEIVED_SHUTDOWN) {
    Local<String> received_shutdown_key =
        FIXED_ONE_BYTE_STRING(node_isolate, "receivedShutdown");
    object()->Set(received_shutdown_key, True(node_isolate));
  }
}


void Connection::Initialize(Environment* env, Handle<Object> target) {
  Local<FunctionTemplate> t = FunctionTemplate::New(Connection::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "Connection"));

  NODE_SET_PROTOTYPE_METHOD(t, "encIn", Connection::EncIn);
  NODE_SET_PROTOTYPE_METHOD(t, "clearOut", Connection::ClearOut);
  NODE_SET_PROTOTYPE_METHOD(t, "clearIn", Connection::ClearIn);
  NODE_SET_PROTOTYPE_METHOD(t, "encOut", Connection::EncOut);
  NODE_SET_PROTOTYPE_METHOD(t, "clearPending", Connection::ClearPending);
  NODE_SET_PROTOTYPE_METHOD(t, "encPending", Connection::EncPending);
  NODE_SET_PROTOTYPE_METHOD(t, "start", Connection::Start);
  NODE_SET_PROTOTYPE_METHOD(t, "shutdown", Connection::Shutdown);
  NODE_SET_PROTOTYPE_METHOD(t, "close", Connection::Close);

  SSLWrap<Connection>::AddMethods(t);

#ifdef OPENSSL_NPN_NEGOTIATED
  NODE_SET_PROTOTYPE_METHOD(t,
                            "getNegotiatedProtocol",
                            Connection::GetNegotiatedProto);
  NODE_SET_PROTOTYPE_METHOD(t,
                            "setNPNProtocols",
                            Connection::SetNPNProtocols);
#endif


#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  NODE_SET_PROTOTYPE_METHOD(t, "getServername", Connection::GetServername);
  NODE_SET_PROTOTYPE_METHOD(t, "setSNICallback",  Connection::SetSNICallback);
#endif

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "Connection"),
              t->GetFunction());
}


int VerifyCallback(int preverify_ok, X509_STORE_CTX* ctx) {
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


#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
int Connection::SelectSNIContextCallback_(SSL *s, int *ad, void* arg) {
  HandleScope scope(node_isolate);

  Connection* conn = static_cast<Connection*>(SSL_get_app_data(s));
  Environment* env = conn->env();

  const char* servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);

  if (servername) {
    conn->servername_.Reset(node_isolate,
                            OneByteString(node_isolate, servername));

    // Call the SNI callback and use its return value as context
    if (!conn->sniObject_.IsEmpty()) {
      conn->sniContext_.Dispose();

      Local<Value> arg = PersistentToLocal(node_isolate, conn->servername_);
      Local<Value> ret = conn->MakeCallback(env->onselect_string(), 1, &arg);

      // If ret is SecureContext
      Local<FunctionTemplate> secure_context_constructor_template =
          env->secure_context_constructor_template();
      if (secure_context_constructor_template->HasInstance(ret)) {
        conn->sniContext_.Reset(node_isolate, ret);
        SecureContext* sc = Unwrap<SecureContext>(ret.As<Object>());
        SSL_set_SSL_CTX(s, sc->ctx_);
      } else {
        return SSL_TLSEXT_ERR_NOACK;
      }
    }
  }

  return SSL_TLSEXT_ERR_OK;
}
#endif

void Connection::New(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1 || !args[0]->IsObject()) {
    return ThrowError("First argument must be a crypto module Credentials");
  }

  SecureContext* sc = Unwrap<SecureContext>(args[0]->ToObject());
  Environment* env = sc->env();

  bool is_server = args[1]->BooleanValue();

  SSLWrap<Connection>::Kind kind =
      is_server ? SSLWrap<Connection>::kServer : SSLWrap<Connection>::kClient;
  Connection* conn = new Connection(env, args.This(), sc, kind);
  conn->ssl_ = SSL_new(sc->ctx_);
  conn->bio_read_ = NodeBIO::New();
  conn->bio_write_ = NodeBIO::New();

  SSL_set_app_data(conn->ssl_, conn);

  if (is_server)
    SSL_set_info_callback(conn->ssl_, SSLInfoCallback);

#ifdef OPENSSL_NPN_NEGOTIATED
  if (is_server) {
    // Server should advertise NPN protocols
    SSL_CTX_set_next_protos_advertised_cb(
        sc->ctx_,
        SSLWrap<Connection>::AdvertiseNextProtoCallback,
        conn);
  } else {
    // Client should select protocol from advertised
    // If server supports NPN
    SSL_CTX_set_next_proto_select_cb(
        sc->ctx_,
        SSLWrap<Connection>::SelectNextProtoCallback,
        conn);
  }
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  if (is_server) {
    SSL_CTX_set_tlsext_servername_callback(sc->ctx_, SelectSNIContextCallback_);
  } else if (args[2]->IsString()) {
    const String::Utf8Value servername(args[2]);
    SSL_set_tlsext_host_name(conn->ssl_, *servername);
  }
#endif

  SSL_set_bio(conn->ssl_, conn->bio_read_, conn->bio_write_);

#ifdef SSL_MODE_RELEASE_BUFFERS
  long mode = SSL_get_mode(conn->ssl_);
  SSL_set_mode(conn->ssl_, mode | SSL_MODE_RELEASE_BUFFERS);
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
      if (reject_unauthorized)
        verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    }
  } else {
    // Note request_cert and reject_unauthorized are ignored for clients.
    verify_mode = SSL_VERIFY_NONE;
  }


  // Always allow a connection. We'll reject in javascript.
  SSL_set_verify(conn->ssl_, verify_mode, VerifyCallback);

  if (is_server) {
    SSL_set_accept_state(conn->ssl_);
  } else {
    SSL_set_connect_state(conn->ssl_);
  }
}


void Connection::SSLInfoCallback(const SSL *ssl_, int where, int ret) {
  if (!(where & (SSL_CB_HANDSHAKE_START | SSL_CB_HANDSHAKE_DONE)))
    return;

  // Be compatible with older versions of OpenSSL. SSL_get_app_data() wants
  // a non-const SSL* in OpenSSL <= 0.9.7e.
  SSL* ssl = const_cast<SSL*>(ssl_);
  Connection* conn = static_cast<Connection*>(SSL_get_app_data(ssl));
  Environment* env = conn->env();
  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());

  if (where & SSL_CB_HANDSHAKE_START) {
    conn->MakeCallback(env->onhandshakestart_string(), 0, NULL);
  }

  if (where & SSL_CB_HANDSHAKE_DONE) {
    conn->MakeCallback(env->onhandshakedone_string(), 0, NULL);
  }
}


void Connection::EncIn(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Connection* conn = Unwrap<Connection>(args.This());

  if (args.Length() < 3) {
    return ThrowTypeError("Takes 3 parameters");
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowTypeError("Second argument should be a buffer");
  }

  char* buffer_data = Buffer::Data(args[0]);
  size_t buffer_length = Buffer::Length(args[0]);

  size_t off = args[1]->Int32Value();
  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowError("off + len > buffer.length");
  }

  int bytes_written;
  char* data = buffer_data + off;

  if (conn->is_server() && !conn->hello_parser_.IsEnded()) {
    // Just accumulate data, everything will be pushed to BIO later
    if (conn->hello_parser_.IsPaused()) {
      bytes_written = 0;
    } else {
      // Copy incoming data to the internal buffer
      // (which has a size of the biggest possible TLS frame)
      size_t available = sizeof(conn->hello_data_) - conn->hello_offset_;
      size_t copied = len < available ? len : available;
      memcpy(conn->hello_data_ + conn->hello_offset_, data, copied);
      conn->hello_offset_ += copied;

      conn->hello_parser_.Parse(conn->hello_data_, conn->hello_offset_);
      bytes_written = copied;
    }
  } else {
    bytes_written = BIO_write(conn->bio_read_, data, len);
    conn->HandleBIOError(conn->bio_read_, "BIO_write", bytes_written);
    conn->SetShutdownFlags();
  }

  args.GetReturnValue().Set(bytes_written);
}


void Connection::ClearOut(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Connection* conn = Unwrap<Connection>(args.This());

  if (args.Length() < 3) {
    return ThrowTypeError("Takes 3 parameters");
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowTypeError("Second argument should be a buffer");
  }

  char* buffer_data = Buffer::Data(args[0]);
  size_t buffer_length = Buffer::Length(args[0]);

  size_t off = args[1]->Int32Value();
  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowError("off + len > buffer.length");
  }

  if (!SSL_is_init_finished(conn->ssl_)) {
    int rv;

    if (conn->is_server()) {
      rv = SSL_accept(conn->ssl_);
      conn->HandleSSLError("SSL_accept:ClearOut",
                           rv,
                           kZeroIsAnError,
                           kSyscallError);
    } else {
      rv = SSL_connect(conn->ssl_);
      conn->HandleSSLError("SSL_connect:ClearOut",
                           rv,
                           kZeroIsAnError,
                           kSyscallError);
    }

    if (rv < 0) {
      return args.GetReturnValue().Set(rv);
    }
  }

  int bytes_read = SSL_read(conn->ssl_, buffer_data + off, len);
  conn->HandleSSLError("SSL_read:ClearOut",
                       bytes_read,
                       kZeroIsNotAnError,
                       kSyscallError);
  conn->SetShutdownFlags();

  args.GetReturnValue().Set(bytes_read);
}


void Connection::ClearPending(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  Connection* conn = Unwrap<Connection>(args.This());
  int bytes_pending = BIO_pending(conn->bio_read_);
  args.GetReturnValue().Set(bytes_pending);
}


void Connection::EncPending(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  Connection* conn = Unwrap<Connection>(args.This());
  int bytes_pending = BIO_pending(conn->bio_write_);
  args.GetReturnValue().Set(bytes_pending);
}


void Connection::EncOut(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Connection* conn = Unwrap<Connection>(args.This());

  if (args.Length() < 3) {
    return ThrowTypeError("Takes 3 parameters");
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowTypeError("Second argument should be a buffer");
  }

  char* buffer_data = Buffer::Data(args[0]);
  size_t buffer_length = Buffer::Length(args[0]);

  size_t off = args[1]->Int32Value();
  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowError("off + len > buffer.length");
  }

  int bytes_read = BIO_read(conn->bio_write_, buffer_data + off, len);

  conn->HandleBIOError(conn->bio_write_, "BIO_read:EncOut", bytes_read);
  conn->SetShutdownFlags();

  args.GetReturnValue().Set(bytes_read);
}


void Connection::ClearIn(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Connection* conn = Unwrap<Connection>(args.This());

  if (args.Length() < 3) {
    return ThrowTypeError("Takes 3 parameters");
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowTypeError("Second argument should be a buffer");
  }

  char* buffer_data = Buffer::Data(args[0]);
  size_t buffer_length = Buffer::Length(args[0]);

  size_t off = args[1]->Int32Value();
  size_t len = args[2]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowError("off + len > buffer.length");
  }

  if (!SSL_is_init_finished(conn->ssl_)) {
    int rv;
    if (conn->is_server()) {
      rv = SSL_accept(conn->ssl_);
      conn->HandleSSLError("SSL_accept:ClearIn",
                           rv,
                           kZeroIsAnError,
                           kSyscallError);
    } else {
      rv = SSL_connect(conn->ssl_);
      conn->HandleSSLError("SSL_connect:ClearIn",
                           rv,
                           kZeroIsAnError,
                           kSyscallError);
    }

    if (rv < 0) {
      return args.GetReturnValue().Set(rv);
    }
  }

  int bytes_written = SSL_write(conn->ssl_, buffer_data + off, len);

  conn->HandleSSLError("SSL_write:ClearIn",
                       bytes_written,
                       len == 0 ? kZeroIsNotAnError : kZeroIsAnError,
                       kSyscallError);
  conn->SetShutdownFlags();

  args.GetReturnValue().Set(bytes_written);
}


void Connection::Start(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Connection* conn = Unwrap<Connection>(args.This());

  int rv = 0;
  if (!SSL_is_init_finished(conn->ssl_)) {
    if (conn->is_server()) {
      rv = SSL_accept(conn->ssl_);
      conn->HandleSSLError("SSL_accept:Start",
                           rv,
                           kZeroIsAnError,
                           kSyscallError);
    } else {
      rv = SSL_connect(conn->ssl_);
      conn->HandleSSLError("SSL_connect:Start",
                           rv,
                           kZeroIsAnError,
                           kSyscallError);
    }
  }
  args.GetReturnValue().Set(rv);
}


void Connection::Shutdown(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Connection* conn = Unwrap<Connection>(args.This());

  if (conn->ssl_ == NULL) {
    return args.GetReturnValue().Set(false);
  }

  int rv = SSL_shutdown(conn->ssl_);
  conn->HandleSSLError("SSL_shutdown", rv, kZeroIsNotAnError, kIgnoreSyscall);
  conn->SetShutdownFlags();
  args.GetReturnValue().Set(rv);
}


void Connection::Close(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Connection* conn = Unwrap<Connection>(args.This());

  if (conn->ssl_ != NULL) {
    SSL_free(conn->ssl_);
    conn->ssl_ = NULL;
  }
}


#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
void Connection::GetServername(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Connection* conn = Unwrap<Connection>(args.This());

  if (conn->is_server() && !conn->servername_.IsEmpty()) {
    args.GetReturnValue().Set(conn->servername_);
  } else {
    args.GetReturnValue().Set(false);
  }
}


void Connection::SetSNICallback(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Connection* conn = Unwrap<Connection>(args.This());

  if (args.Length() < 1 || !args[0]->IsFunction()) {
    return ThrowError("Must give a Function as first argument");
  }

  Local<Object> obj = Object::New();
  obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "onselect"), args[0]);
  conn->sniObject_.Reset(node_isolate, obj);
}
#endif


void CipherBase::Initialize(Environment* env, Handle<Object> target) {
  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(t, "init", Init);
  NODE_SET_PROTOTYPE_METHOD(t, "initiv", InitIv);
  NODE_SET_PROTOTYPE_METHOD(t, "update", Update);
  NODE_SET_PROTOTYPE_METHOD(t, "final", Final);
  NODE_SET_PROTOTYPE_METHOD(t, "setAutoPadding", SetAutoPadding);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "CipherBase"),
              t->GetFunction());
}


void CipherBase::New(const FunctionCallbackInfo<Value>& args) {
  assert(args.IsConstructCall() == true);
  CipherKind kind = args[0]->IsTrue() ? kCipher : kDecipher;
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  new CipherBase(env, args.This(), kind);
}


void CipherBase::Init(const char* cipher_type,
                      const char* key_buf,
                      int key_buf_len) {
  HandleScope scope(node_isolate);

  assert(cipher_ == NULL);
  cipher_ = EVP_get_cipherbyname(cipher_type);
  if (cipher_ == NULL) {
    return ThrowError("Unknown cipher");
  }

  unsigned char key[EVP_MAX_KEY_LENGTH];
  unsigned char iv[EVP_MAX_IV_LENGTH];

  int key_len = EVP_BytesToKey(cipher_,
                               EVP_md5(),
                               NULL,
                               reinterpret_cast<const unsigned char*>(key_buf),
                               key_buf_len,
                               1,
                               key,
                               iv);

  EVP_CIPHER_CTX_init(&ctx_);
  EVP_CipherInit_ex(&ctx_, cipher_, NULL, NULL, NULL, kind_ == kCipher);
  if (!EVP_CIPHER_CTX_set_key_length(&ctx_, key_len)) {
    EVP_CIPHER_CTX_cleanup(&ctx_);
    return ThrowError("Invalid key length");
  }

  EVP_CipherInit_ex(&ctx_,
                    NULL,
                    NULL,
                    reinterpret_cast<unsigned char*>(key),
                    reinterpret_cast<unsigned char*>(iv),
                    kind_ == kCipher);
  initialised_ = true;
}


void CipherBase::Init(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  CipherBase* cipher = Unwrap<CipherBase>(args.This());

  if (args.Length() < 2 ||
      !(args[0]->IsString() && Buffer::HasInstance(args[1]))) {
    return ThrowError("Must give cipher-type, key");
  }

  const String::Utf8Value cipher_type(args[0]);
  const char* key_buf = Buffer::Data(args[1]);
  ssize_t key_buf_len = Buffer::Length(args[1]);
  cipher->Init(*cipher_type, key_buf, key_buf_len);
}


void CipherBase::InitIv(const char* cipher_type,
                        const char* key,
                        int key_len,
                        const char* iv,
                        int iv_len) {
  HandleScope scope(node_isolate);

  cipher_ = EVP_get_cipherbyname(cipher_type);
  if (cipher_ == NULL) {
    return ThrowError("Unknown cipher");
  }

  /* OpenSSL versions up to 0.9.8l failed to return the correct
     iv_length (0) for ECB ciphers */
  if (EVP_CIPHER_iv_length(cipher_) != iv_len &&
      !(EVP_CIPHER_mode(cipher_) == EVP_CIPH_ECB_MODE && iv_len == 0)) {
    return ThrowError("Invalid IV length");
  }
  EVP_CIPHER_CTX_init(&ctx_);
  EVP_CipherInit_ex(&ctx_, cipher_, NULL, NULL, NULL, kind_ == kCipher);
  if (!EVP_CIPHER_CTX_set_key_length(&ctx_, key_len)) {
    EVP_CIPHER_CTX_cleanup(&ctx_);
    return ThrowError("Invalid key length");
  }

  EVP_CipherInit_ex(&ctx_,
                    NULL,
                    NULL,
                    reinterpret_cast<const unsigned char*>(key),
                    reinterpret_cast<const unsigned char*>(iv),
                    kind_ == kCipher);
  initialised_ = true;
}


void CipherBase::InitIv(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  CipherBase* cipher = Unwrap<CipherBase>(args.This());

  if (args.Length() < 3 || !args[0]->IsString()) {
    return ThrowError("Must give cipher-type, key, and iv as argument");
  }

  ASSERT_IS_BUFFER(args[1]);
  ASSERT_IS_BUFFER(args[2]);

  const String::Utf8Value cipher_type(args[0]);
  ssize_t key_len = Buffer::Length(args[1]);
  const char* key_buf = Buffer::Data(args[1]);
  ssize_t iv_len = Buffer::Length(args[2]);
  const char* iv_buf = Buffer::Data(args[2]);
  cipher->InitIv(*cipher_type, key_buf, key_len, iv_buf, iv_len);
}


bool CipherBase::Update(const char* data,
                        int len,
                        unsigned char** out,
                        int* out_len) {
  if (!initialised_)
    return 0;
  *out_len = len + EVP_CIPHER_CTX_block_size(&ctx_);
  *out = new unsigned char[*out_len];
  return EVP_CipherUpdate(&ctx_,
                          *out,
                          out_len,
                          reinterpret_cast<const unsigned char*>(data),
                          len);
}


void CipherBase::Update(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  CipherBase* cipher = Unwrap<CipherBase>(args.This());

  ASSERT_IS_STRING_OR_BUFFER(args[0]);

  unsigned char* out = NULL;
  bool r;
  int out_len = 0;

  // Only copy the data if we have to, because it's a string
  if (args[0]->IsString()) {
    Local<String> string = args[0].As<String>();
    enum encoding encoding = ParseEncoding(args[1], BINARY);
    if (!StringBytes::IsValidString(string, encoding))
      return ThrowTypeError("Bad input string");
    size_t buflen = StringBytes::StorageSize(string, encoding);
    char* buf = new char[buflen];
    size_t written = StringBytes::Write(buf, buflen, string, encoding);
    r = cipher->Update(buf, written, &out, &out_len);
    delete[] buf;
  } else {
    char* buf = Buffer::Data(args[0]);
    size_t buflen = Buffer::Length(args[0]);
    r = cipher->Update(buf, buflen, &out, &out_len);
  }

  if (!r) {
    delete[] out;
    return ThrowCryptoTypeError(ERR_get_error());
  }

  Local<Object> buf = Buffer::New(env, reinterpret_cast<char*>(out), out_len);
  if (out)
    delete[] out;

  args.GetReturnValue().Set(buf);
}


bool CipherBase::SetAutoPadding(bool auto_padding) {
  if (!initialised_)
    return false;
  return EVP_CIPHER_CTX_set_padding(&ctx_, auto_padding);
}


void CipherBase::SetAutoPadding(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  CipherBase* cipher = Unwrap<CipherBase>(args.This());
  cipher->SetAutoPadding(args.Length() < 1 || args[0]->BooleanValue());
}


bool CipherBase::Final(unsigned char** out, int *out_len) {
  if (!initialised_)
    return false;

  *out = new unsigned char[EVP_CIPHER_CTX_block_size(&ctx_)];
  bool r = EVP_CipherFinal_ex(&ctx_, *out, out_len);
  EVP_CIPHER_CTX_cleanup(&ctx_);
  initialised_ = false;

  return r;
}


void CipherBase::Final(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  CipherBase* cipher = Unwrap<CipherBase>(args.This());

  unsigned char* out_value = NULL;
  int out_len = -1;
  Local<Value> outString;

  bool r = cipher->Final(&out_value, &out_len);

  if (out_len <= 0 || !r) {
    delete[] out_value;
    out_value = NULL;
    out_len = 0;
    if (!r)
      return ThrowCryptoTypeError(ERR_get_error());
  }

  args.GetReturnValue().Set(
      Buffer::New(env, reinterpret_cast<char*>(out_value), out_len));
}


void Hmac::Initialize(Environment* env, v8::Handle<v8::Object> target) {
  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(t, "init", HmacInit);
  NODE_SET_PROTOTYPE_METHOD(t, "update", HmacUpdate);
  NODE_SET_PROTOTYPE_METHOD(t, "digest", HmacDigest);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "Hmac"), t->GetFunction());
}


void Hmac::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  new Hmac(env, args.This());
}


void Hmac::HmacInit(const char* hash_type, const char* key, int key_len) {
  HandleScope scope(node_isolate);

  assert(md_ == NULL);
  md_ = EVP_get_digestbyname(hash_type);
  if (md_ == NULL) {
    return ThrowError("Unknown message digest");
  }
  HMAC_CTX_init(&ctx_);
  if (key_len == 0) {
    HMAC_Init(&ctx_, "", 0, md_);
  } else {
    HMAC_Init(&ctx_, key, key_len, md_);
  }
  initialised_ = true;
}


void Hmac::HmacInit(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Hmac* hmac = Unwrap<Hmac>(args.This());

  if (args.Length() < 2 || !args[0]->IsString()) {
    return ThrowError("Must give hashtype string, key as arguments");
  }

  ASSERT_IS_BUFFER(args[1]);

  const String::Utf8Value hash_type(args[0]);
  const char* buffer_data = Buffer::Data(args[1]);
  size_t buffer_length = Buffer::Length(args[1]);
  hmac->HmacInit(*hash_type, buffer_data, buffer_length);
}


bool Hmac::HmacUpdate(const char* data, int len) {
  if (!initialised_)
    return false;
  HMAC_Update(&ctx_, reinterpret_cast<const unsigned char*>(data), len);
  return true;
}


void Hmac::HmacUpdate(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Hmac* hmac = Unwrap<Hmac>(args.This());

  ASSERT_IS_STRING_OR_BUFFER(args[0]);

  // Only copy the data if we have to, because it's a string
  bool r;
  if (args[0]->IsString()) {
    Local<String> string = args[0].As<String>();
    enum encoding encoding = ParseEncoding(args[1], BINARY);
    if (!StringBytes::IsValidString(string, encoding))
      return ThrowTypeError("Bad input string");
    size_t buflen = StringBytes::StorageSize(string, encoding);
    char* buf = new char[buflen];
    size_t written = StringBytes::Write(buf, buflen, string, encoding);
    r = hmac->HmacUpdate(buf, written);
    delete[] buf;
  } else {
    char* buf = Buffer::Data(args[0]);
    size_t buflen = Buffer::Length(args[0]);
    r = hmac->HmacUpdate(buf, buflen);
  }

  if (!r) {
    return ThrowTypeError("HmacUpdate fail");
  }
}


bool Hmac::HmacDigest(unsigned char** md_value, unsigned int* md_len) {
  if (!initialised_)
    return false;
  *md_value = new unsigned char[EVP_MAX_MD_SIZE];
  HMAC_Final(&ctx_, *md_value, md_len);
  HMAC_CTX_cleanup(&ctx_);
  initialised_ = false;
  return true;
}


void Hmac::HmacDigest(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Hmac* hmac = Unwrap<Hmac>(args.This());

  enum encoding encoding = BUFFER;
  if (args.Length() >= 1) {
    encoding = ParseEncoding(args[0]->ToString(), BUFFER);
  }

  unsigned char* md_value = NULL;
  unsigned int md_len = 0;

  bool r = hmac->HmacDigest(&md_value, &md_len);
  if (!r) {
    md_value = NULL;
    md_len = 0;
  }

  Local<Value> rc = StringBytes::Encode(
        reinterpret_cast<const char*>(md_value), md_len, encoding);
  delete[] md_value;
  args.GetReturnValue().Set(rc);
}


void Hash::Initialize(Environment* env, v8::Handle<v8::Object> target) {
  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(t, "update", HashUpdate);
  NODE_SET_PROTOTYPE_METHOD(t, "digest", HashDigest);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "Hash"), t->GetFunction());
}


void Hash::New(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() == 0 || !args[0]->IsString()) {
    return ThrowError("Must give hashtype string as argument");
  }

  const String::Utf8Value hash_type(args[0]);

  Environment* env = Environment::GetCurrent(args.GetIsolate());
  Hash* hash = new Hash(env, args.This());
  if (!hash->HashInit(*hash_type)) {
    return ThrowError("Digest method not supported");
  }
}


bool Hash::HashInit(const char* hash_type) {
  assert(md_ == NULL);
  md_ = EVP_get_digestbyname(hash_type);
  if (md_ == NULL)
    return false;
  EVP_MD_CTX_init(&mdctx_);
  EVP_DigestInit_ex(&mdctx_, md_, NULL);
  initialised_ = true;
  return true;
}


bool Hash::HashUpdate(const char* data, int len) {
  if (!initialised_)
    return false;
  EVP_DigestUpdate(&mdctx_, data, len);
  return true;
}


void Hash::HashUpdate(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Hash* hash = Unwrap<Hash>(args.This());

  ASSERT_IS_STRING_OR_BUFFER(args[0]);

  // Only copy the data if we have to, because it's a string
  bool r;
  if (args[0]->IsString()) {
    Local<String> string = args[0].As<String>();
    enum encoding encoding = ParseEncoding(args[1], BINARY);
    if (!StringBytes::IsValidString(string, encoding))
      return ThrowTypeError("Bad input string");
    size_t buflen = StringBytes::StorageSize(string, encoding);
    char* buf = new char[buflen];
    size_t written = StringBytes::Write(buf, buflen, string, encoding);
    r = hash->HashUpdate(buf, written);
    delete[] buf;
  } else {
    char* buf = Buffer::Data(args[0]);
    size_t buflen = Buffer::Length(args[0]);
    r = hash->HashUpdate(buf, buflen);
  }

  if (!r) {
    return ThrowTypeError("HashUpdate fail");
  }
}


void Hash::HashDigest(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Hash* hash = Unwrap<Hash>(args.This());

  if (!hash->initialised_) {
    return ThrowError("Not initialized");
  }

  enum encoding encoding = BUFFER;
  if (args.Length() >= 1) {
    encoding = ParseEncoding(args[0]->ToString(), BUFFER);
  }

  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;

  EVP_DigestFinal_ex(&hash->mdctx_, md_value, &md_len);
  EVP_MD_CTX_cleanup(&hash->mdctx_);
  hash->initialised_ = false;

  Local<Value> rc = StringBytes::Encode(
      reinterpret_cast<const char*>(md_value), md_len, encoding);
  args.GetReturnValue().Set(rc);
}


void Sign::Initialize(Environment* env, v8::Handle<v8::Object> target) {
  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(t, "init", SignInit);
  NODE_SET_PROTOTYPE_METHOD(t, "update", SignUpdate);
  NODE_SET_PROTOTYPE_METHOD(t, "sign", SignFinal);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "Sign"), t->GetFunction());
}


void Sign::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  new Sign(env, args.This());
}


void Sign::SignInit(const char* sign_type) {
  HandleScope scope(node_isolate);

  assert(md_ == NULL);
  md_ = EVP_get_digestbyname(sign_type);
  if (!md_) {
    return ThrowError("Uknown message digest");
  }
  EVP_MD_CTX_init(&mdctx_);
  EVP_SignInit_ex(&mdctx_, md_, NULL);
  initialised_ = true;
}


void Sign::SignInit(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Sign* sign = Unwrap<Sign>(args.This());

  if (args.Length() == 0 || !args[0]->IsString()) {
    return ThrowError("Must give signtype string as argument");
  }

  const String::Utf8Value sign_type(args[0]);
  sign->SignInit(*sign_type);
}


bool Sign::SignUpdate(const char* data, int len) {
  if (!initialised_)
    return false;
  EVP_SignUpdate(&mdctx_, data, len);
  return true;
}


void Sign::SignUpdate(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Sign* sign = Unwrap<Sign>(args.This());

  ASSERT_IS_STRING_OR_BUFFER(args[0]);

  // Only copy the data if we have to, because it's a string
  int r;
  if (args[0]->IsString()) {
    Local<String> string = args[0].As<String>();
    enum encoding encoding = ParseEncoding(args[1], BINARY);
    if (!StringBytes::IsValidString(string, encoding))
      return ThrowTypeError("Bad input string");
    size_t buflen = StringBytes::StorageSize(string, encoding);
    char* buf = new char[buflen];
    size_t written = StringBytes::Write(buf, buflen, string, encoding);
    r = sign->SignUpdate(buf, written);
    delete[] buf;
  } else {
    char* buf = Buffer::Data(args[0]);
    size_t buflen = Buffer::Length(args[0]);
    r = sign->SignUpdate(buf, buflen);
  }

  if (!r) {
    return ThrowTypeError("SignUpdate fail");
  }
}


bool Sign::SignFinal(const char* key_pem,
                     int key_pem_len,
                     const char* passphrase,
                     unsigned char** sig,
                     unsigned int *sig_len) {
  if (!initialised_) {
    ThrowError("Sign not initalised");
    return false;
  }

  BIO* bp = NULL;
  EVP_PKEY* pkey = NULL;
  bool fatal = true;

  bp = BIO_new(BIO_s_mem());
  if (bp == NULL)
    goto exit;

  if (!BIO_write(bp, key_pem, key_pem_len))
    goto exit;

  pkey = PEM_read_bio_PrivateKey(bp,
                                 NULL,
                                 CryptoPemCallback,
                                 const_cast<char*>(passphrase));
  if (pkey == NULL)
    goto exit;

  if (EVP_SignFinal(&mdctx_, *sig, sig_len, pkey))
    fatal = false;

  initialised_ = false;

 exit:
  if (pkey != NULL)
    EVP_PKEY_free(pkey);
  if (bp != NULL)
    BIO_free_all(bp);

  EVP_MD_CTX_cleanup(&mdctx_);

  if (fatal) {
    unsigned long err = ERR_get_error();
    if (err) {
      ThrowCryptoError(err);
    } else {
      ThrowError("PEM_read_bio_PrivateKey");
    }
    return false;
  }

  return true;
}


void Sign::SignFinal(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Sign* sign = Unwrap<Sign>(args.This());

  unsigned char* md_value;
  unsigned int md_len;

  unsigned int len = args.Length();
  enum encoding encoding = BUFFER;
  if (len >= 2 && args[1]->IsString()) {
    encoding = ParseEncoding(args[1]->ToString(), BUFFER);
  }

  String::Utf8Value passphrase(args[2]);

  ASSERT_IS_BUFFER(args[0]);
  size_t buf_len = Buffer::Length(args[0]);
  char* buf = Buffer::Data(args[0]);

  md_len = 8192;  // Maximum key size is 8192 bits
  md_value = new unsigned char[md_len];

  bool r = sign->SignFinal(buf,
                           buf_len,
                           len >= 3 && !args[2]->IsNull() ? *passphrase : NULL,
                           &md_value,
                           &md_len);
  if (!r) {
    delete[] md_value;
    md_value = NULL;
    md_len = 0;
  }

  Local<Value> rc = StringBytes::Encode(
      reinterpret_cast<const char*>(md_value), md_len, encoding);
  delete[] md_value;
  args.GetReturnValue().Set(rc);
}


void Verify::Initialize(Environment* env, v8::Handle<v8::Object> target) {
  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(t, "init", VerifyInit);
  NODE_SET_PROTOTYPE_METHOD(t, "update", VerifyUpdate);
  NODE_SET_PROTOTYPE_METHOD(t, "verify", VerifyFinal);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "Verify"), t->GetFunction());
}


void Verify::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  new Verify(env, args.This());
}


void Verify::VerifyInit(const char* verify_type) {
  HandleScope scope(node_isolate);

  assert(md_ == NULL);
  md_ = EVP_get_digestbyname(verify_type);
  if (md_ == NULL) {
    return ThrowError("Unknown message digest");
  }

  EVP_MD_CTX_init(&mdctx_);
  EVP_VerifyInit_ex(&mdctx_, md_, NULL);
  initialised_ = true;
}


void Verify::VerifyInit(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Verify* verify = Unwrap<Verify>(args.This());

  if (args.Length() == 0 || !args[0]->IsString()) {
    return ThrowError("Must give verifytype string as argument");
  }

  const String::Utf8Value verify_type(args[0]);
  verify->VerifyInit(*verify_type);
}


bool Verify::VerifyUpdate(const char* data, int len) {
  if (!initialised_)
    return false;
  EVP_VerifyUpdate(&mdctx_, data, len);
  return true;
}


void Verify::VerifyUpdate(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Verify* verify = Unwrap<Verify>(args.This());

  ASSERT_IS_STRING_OR_BUFFER(args[0]);

  // Only copy the data if we have to, because it's a string
  bool r;
  if (args[0]->IsString()) {
    Local<String> string = args[0].As<String>();
    enum encoding encoding = ParseEncoding(args[1], BINARY);
    if (!StringBytes::IsValidString(string, encoding))
      return ThrowTypeError("Bad input string");
    size_t buflen = StringBytes::StorageSize(string, encoding);
    char* buf = new char[buflen];
    size_t written = StringBytes::Write(buf, buflen, string, encoding);
    r = verify->VerifyUpdate(buf, written);
    delete[] buf;
  } else {
    char* buf = Buffer::Data(args[0]);
    size_t buflen = Buffer::Length(args[0]);
    r = verify->VerifyUpdate(buf, buflen);
  }

  if (!r) {
    return ThrowTypeError("VerifyUpdate fail");
  }
}


bool Verify::VerifyFinal(const char* key_pem,
                         int key_pem_len,
                         const char* sig,
                         int siglen) {
  HandleScope scope(node_isolate);

  if (!initialised_) {
    ThrowError("Verify not initalised");
    return false;
  }

  ClearErrorOnReturn clear_error_on_return;
  (void) &clear_error_on_return;  // Silence compiler warning.

  EVP_PKEY* pkey = NULL;
  BIO* bp = NULL;
  X509* x509 = NULL;
  bool fatal = true;
  int r = 0;

  bp = BIO_new(BIO_s_mem());
  if (bp == NULL)
    goto exit;

  if (!BIO_write(bp, key_pem, key_pem_len))
    goto exit;

  // Check if this is a PKCS#8 or RSA public key before trying as X.509.
  // Split this out into a separate function once we have more than one
  // consumer of public keys.
  if (strncmp(key_pem, PUBLIC_KEY_PFX, PUBLIC_KEY_PFX_LEN) == 0) {
    pkey = PEM_read_bio_PUBKEY(bp, NULL, CryptoPemCallback, NULL);
    if (pkey == NULL)
      goto exit;
  } else if (strncmp(key_pem, PUBRSA_KEY_PFX, PUBRSA_KEY_PFX_LEN) == 0) {
    RSA* rsa = PEM_read_bio_RSAPublicKey(bp, NULL, CryptoPemCallback, NULL);
    if (rsa) {
      pkey = EVP_PKEY_new();
      if (pkey)
        EVP_PKEY_set1_RSA(pkey, rsa);
      RSA_free(rsa);
    }
    if (pkey == NULL)
      goto exit;
  } else {
    // X.509 fallback
    x509 = PEM_read_bio_X509(bp, NULL, CryptoPemCallback, NULL);
    if (x509 == NULL)
      goto exit;

    pkey = X509_get_pubkey(x509);
    if (pkey == NULL)
      goto exit;
  }

  fatal = false;
  r = EVP_VerifyFinal(&mdctx_,
                      reinterpret_cast<const unsigned char*>(sig),
                      siglen,
                      pkey);

 exit:
  if (pkey != NULL)
    EVP_PKEY_free(pkey);
  if (bp != NULL)
    BIO_free_all(bp);
  if (x509 != NULL)
    X509_free(x509);

  EVP_MD_CTX_cleanup(&mdctx_);
  initialised_ = false;

  if (fatal) {
    unsigned long err = ERR_get_error();
    ThrowCryptoError(err);
    return false;
  }

  return r == 1;
}


void Verify::VerifyFinal(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Verify* verify = Unwrap<Verify>(args.This());

  ASSERT_IS_BUFFER(args[0]);
  char* kbuf = Buffer::Data(args[0]);
  ssize_t klen = Buffer::Length(args[0]);

  ASSERT_IS_STRING_OR_BUFFER(args[1]);
  // BINARY works for both buffers and binary strings.
  enum encoding encoding = BINARY;
  if (args.Length() >= 3) {
    encoding = ParseEncoding(args[2]->ToString(), BINARY);
  }

  ssize_t hlen = StringBytes::Size(args[1], encoding);

  // only copy if we need to, because it's a string.
  char* hbuf;
  if (args[1]->IsString()) {
    hbuf = new char[hlen];
    ssize_t hwritten = StringBytes::Write(hbuf, hlen, args[1], encoding);
    assert(hwritten == hlen);
  } else {
    hbuf = Buffer::Data(args[1]);
  }

  bool rc = verify->VerifyFinal(kbuf, klen, hbuf, hlen);
  if (args[1]->IsString()) {
    delete[] hbuf;
  }
  args.GetReturnValue().Set(rc);
}


void DiffieHellman::Initialize(Environment* env, Handle<Object> target) {
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

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "DiffieHellman"),
              t->GetFunction());

  Local<FunctionTemplate> t2 = FunctionTemplate::New(DiffieHellmanGroup);
  t2->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(t2, "generateKeys", GenerateKeys);
  NODE_SET_PROTOTYPE_METHOD(t2, "computeSecret", ComputeSecret);
  NODE_SET_PROTOTYPE_METHOD(t2, "getPrime", GetPrime);
  NODE_SET_PROTOTYPE_METHOD(t2, "getGenerator", GetGenerator);
  NODE_SET_PROTOTYPE_METHOD(t2, "getPublicKey", GetPublicKey);
  NODE_SET_PROTOTYPE_METHOD(t2, "getPrivateKey", GetPrivateKey);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "DiffieHellmanGroup"),
              t2->GetFunction());
}


bool DiffieHellman::Init(int primeLength) {
  dh = DH_new();
  DH_generate_parameters_ex(dh, primeLength, DH_GENERATOR_2, 0);
  bool result = VerifyContext();
  if (!result)
    return false;
  initialised_ = true;
  return true;
}


bool DiffieHellman::Init(const char* p, int p_len) {
  dh = DH_new();
  dh->p = BN_bin2bn(reinterpret_cast<const unsigned char*>(p), p_len, 0);
  dh->g = BN_new();
  if (!BN_set_word(dh->g, 2))
    return false;
  bool result = VerifyContext();
  if (!result)
    return false;
  initialised_ = true;
  return true;
}


bool DiffieHellman::Init(const char* p, int p_len, const char* g, int g_len) {
  dh = DH_new();
  dh->p = BN_bin2bn(reinterpret_cast<const unsigned char*>(p), p_len, 0);
  dh->g = BN_bin2bn(reinterpret_cast<const unsigned char*>(g), g_len, 0);
  initialised_ = true;
  return true;
}


void DiffieHellman::DiffieHellmanGroup(
    const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Environment* env = Environment::GetCurrent(args.GetIsolate());
  DiffieHellman* diffieHellman = new DiffieHellman(env, args.This());

  if (args.Length() != 1 || !args[0]->IsString()) {
    return ThrowError("No group name given");
  }

  const String::Utf8Value group_name(args[0]);
  for (unsigned int i = 0; i < ARRAY_SIZE(modp_groups); ++i) {
    const modp_group* it = modp_groups + i;

    if (strcasecmp(*group_name, it->name) != 0)
      continue;

    diffieHellman->Init(it->prime,
                        it->prime_size,
                        it->gen,
                        it->gen_size);
    return;
  }

  ThrowError("Unknown group");
}


void DiffieHellman::New(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Environment* env = Environment::GetCurrent(args.GetIsolate());
  DiffieHellman* diffieHellman =
      new DiffieHellman(env, args.This());
  bool initialized = false;

  if (args.Length() > 0) {
    if (args[0]->IsInt32()) {
      initialized = diffieHellman->Init(args[0]->Int32Value());
    } else {
      initialized = diffieHellman->Init(Buffer::Data(args[0]),
                                        Buffer::Length(args[0]));
    }
  }

  if (!initialized) {
    return ThrowError("Initialization failed");
  }
}


void DiffieHellman::GenerateKeys(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  DiffieHellman* diffieHellman = Unwrap<DiffieHellman>(args.This());

  if (!diffieHellman->initialised_) {
    return ThrowError("Not initialized");
  }

  if (!DH_generate_key(diffieHellman->dh)) {
    return ThrowError("Key generation failed");
  }

  int dataSize = BN_num_bytes(diffieHellman->dh->pub_key);
  char* data = new char[dataSize];
  BN_bn2bin(diffieHellman->dh->pub_key,
            reinterpret_cast<unsigned char*>(data));

  args.GetReturnValue().Set(Encode(data, dataSize, BUFFER));
  delete[] data;
}


void DiffieHellman::GetPrime(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  DiffieHellman* diffieHellman = Unwrap<DiffieHellman>(args.This());

  if (!diffieHellman->initialised_) {
    return ThrowError("Not initialized");
  }

  int dataSize = BN_num_bytes(diffieHellman->dh->p);
  char* data = new char[dataSize];
  BN_bn2bin(diffieHellman->dh->p, reinterpret_cast<unsigned char*>(data));

  args.GetReturnValue().Set(Encode(data, dataSize, BUFFER));
  delete[] data;
}


void DiffieHellman::GetGenerator(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  DiffieHellman* diffieHellman = Unwrap<DiffieHellman>(args.This());

  if (!diffieHellman->initialised_) {
    return ThrowError("Not initialized");
  }

  int dataSize = BN_num_bytes(diffieHellman->dh->g);
  char* data = new char[dataSize];
  BN_bn2bin(diffieHellman->dh->g, reinterpret_cast<unsigned char*>(data));

  args.GetReturnValue().Set(Encode(data, dataSize, BUFFER));
  delete[] data;
}


void DiffieHellman::GetPublicKey(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  DiffieHellman* diffieHellman = Unwrap<DiffieHellman>(args.This());

  if (!diffieHellman->initialised_) {
    return ThrowError("Not initialized");
  }

  if (diffieHellman->dh->pub_key == NULL) {
    return ThrowError("No public key - did you forget to generate one?");
  }

  int dataSize = BN_num_bytes(diffieHellman->dh->pub_key);
  char* data = new char[dataSize];
  BN_bn2bin(diffieHellman->dh->pub_key,
            reinterpret_cast<unsigned char*>(data));

  args.GetReturnValue().Set(Encode(data, dataSize, BUFFER));
  delete[] data;
}


void DiffieHellman::GetPrivateKey(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  DiffieHellman* diffieHellman = Unwrap<DiffieHellman>(args.This());

  if (!diffieHellman->initialised_) {
    return ThrowError("Not initialized");
  }

  if (diffieHellman->dh->priv_key == NULL) {
    return ThrowError("No private key - did you forget to generate one?");
  }

  int dataSize = BN_num_bytes(diffieHellman->dh->priv_key);
  char* data = new char[dataSize];
  BN_bn2bin(diffieHellman->dh->priv_key,
            reinterpret_cast<unsigned char*>(data));

  args.GetReturnValue().Set(Encode(data, dataSize, BUFFER));
  delete[] data;
}


void DiffieHellman::ComputeSecret(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  DiffieHellman* diffieHellman = Unwrap<DiffieHellman>(args.This());

  if (!diffieHellman->initialised_) {
    return ThrowError("Not initialized");
  }

  ClearErrorOnReturn clear_error_on_return;
  (void) &clear_error_on_return;  // Silence compiler warning.
  BIGNUM* key = NULL;

  if (args.Length() == 0) {
    return ThrowError("First argument must be other party's public key");
  } else {
    ASSERT_IS_BUFFER(args[0]);
    key = BN_bin2bn(
        reinterpret_cast<unsigned char*>(Buffer::Data(args[0])),
        Buffer::Length(args[0]),
        0);
  }

  int dataSize = DH_size(diffieHellman->dh);
  char* data = new char[dataSize];

  int size = DH_compute_key(reinterpret_cast<unsigned char*>(data),
                            key,
                            diffieHellman->dh);

  if (size == -1) {
    int checkResult;
    int checked;

    checked = DH_check_pub_key(diffieHellman->dh, key, &checkResult);
    BN_free(key);
    delete[] data;

    if (!checked) {
      return ThrowError("Invalid key");
    } else if (checkResult) {
      if (checkResult & DH_CHECK_PUBKEY_TOO_SMALL) {
        return ThrowError("Supplied key is too small");
      } else if (checkResult & DH_CHECK_PUBKEY_TOO_LARGE) {
        return ThrowError("Supplied key is too large");
      } else {
        return ThrowError("Invalid key");
      }
    } else {
      return ThrowError("Invalid key");
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

  args.GetReturnValue().Set(Encode(data, dataSize, BUFFER));
  delete[] data;
}


void DiffieHellman::SetPublicKey(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  DiffieHellman* diffieHellman = Unwrap<DiffieHellman>(args.This());

  if (!diffieHellman->initialised_) {
    return ThrowError("Not initialized");
  }

  if (args.Length() == 0) {
    return ThrowError("First argument must be public key");
  } else {
    ASSERT_IS_BUFFER(args[0]);
    diffieHellman->dh->pub_key = BN_bin2bn(
        reinterpret_cast<unsigned char*>(Buffer::Data(args[0])),
        Buffer::Length(args[0]), 0);
  }
}


void DiffieHellman::SetPrivateKey(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  DiffieHellman* diffieHellman = Unwrap<DiffieHellman>(args.This());

  if (!diffieHellman->initialised_) {
    return ThrowError("Not initialized");
  }

  if (args.Length() == 0) {
    return ThrowError("First argument must be private key");
  } else {
    ASSERT_IS_BUFFER(args[0]);
    diffieHellman->dh->priv_key = BN_bin2bn(
        reinterpret_cast<unsigned char*>(Buffer::Data(args[0])),
        Buffer::Length(args[0]),
        0);
  }
}


bool DiffieHellman::VerifyContext() {
  int codes;
  if (!DH_check(dh, &codes))
    return false;
  if (codes & DH_CHECK_P_NOT_SAFE_PRIME)
    return false;
  if (codes & DH_CHECK_P_NOT_PRIME)
    return false;
  if (codes & DH_UNABLE_TO_CHECK_GENERATOR)
    return false;
  if (codes & DH_NOT_SUITABLE_GENERATOR)
    return false;
  return true;
}


class PBKDF2Request : public AsyncWrap {
 public:
  PBKDF2Request(Environment* env,
                Local<Object> object,
                ssize_t passlen,
                char* pass,
                ssize_t saltlen,
                char* salt,
                ssize_t iter,
                ssize_t keylen)
      : AsyncWrap(env, object),
        error_(0),
        passlen_(passlen),
        pass_(pass),
        saltlen_(saltlen),
        salt_(salt),
        keylen_(keylen),
        key_(static_cast<char*>(malloc(keylen))),
        iter_(iter) {
    if (key() == NULL)
      FatalError("node::PBKDF2Request()", "Out of Memory");
  }

  ~PBKDF2Request() {
    persistent().Dispose();
  }

  uv_work_t* work_req() {
    return &work_req_;
  }

  inline ssize_t passlen() const {
    return passlen_;
  }

  inline char* pass() const {
    return pass_;
  }

  inline ssize_t saltlen() const {
    return saltlen_;
  }

  inline char* salt() const {
    return salt_;
  }

  inline ssize_t keylen() const {
    return keylen_;
  }

  inline char* key() const {
    return key_;
  }

  inline ssize_t iter() const {
    return iter_;
  }

  inline void release() {
    free(pass_);
    passlen_ = 0;
    free(salt_);
    saltlen_ = 0;
    free(key_);
    keylen_ = 0;
  }

  inline int error() const {
    return error_;
  }

  inline void set_error(int err) {
    error_ = err;
  }

  // TODO(trevnorris): Make private and make work with CONTAINER_OF macro.
  uv_work_t work_req_;

 private:
  int error_;
  ssize_t passlen_;
  char* pass_;
  ssize_t saltlen_;
  char* salt_;
  ssize_t keylen_;
  char* key_;
  ssize_t iter_;
};


void EIO_PBKDF2(PBKDF2Request* req) {
  req->set_error(PKCS5_PBKDF2_HMAC_SHA1(
    req->pass(),
    req->passlen(),
    reinterpret_cast<unsigned char*>(req->salt()),
    req->saltlen(),
    req->iter(),
    req->keylen(),
    reinterpret_cast<unsigned char*>(req->key())));
  memset(req->pass(), 0, req->passlen());
  memset(req->salt(), 0, req->saltlen());
}


void EIO_PBKDF2(uv_work_t* work_req) {
  PBKDF2Request* req = CONTAINER_OF(work_req, PBKDF2Request, work_req_);
  EIO_PBKDF2(req);
}


void EIO_PBKDF2After(PBKDF2Request* req, Local<Value> argv[2]) {
  if (req->error()) {
    argv[0] = Undefined(node_isolate);
    argv[1] = Encode(req->key(), req->keylen(), BUFFER);
    memset(req->key(), 0, req->keylen());
  } else {
    argv[0] = Exception::Error(
        FIXED_ONE_BYTE_STRING(node_isolate, "PBKDF2 error"));
    argv[1] = Undefined(node_isolate);
  }
}


void EIO_PBKDF2After(uv_work_t* work_req, int status) {
  assert(status == 0);
  PBKDF2Request* req = CONTAINER_OF(work_req, PBKDF2Request, work_req_);
  Environment* env = req->env();
  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());
  Local<Value> argv[2];
  EIO_PBKDF2After(req, argv);
  req->MakeCallback(env->ondone_string(), ARRAY_SIZE(argv), argv);
  req->release();
  delete req;
}


void PBKDF2(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  const char* type_error = NULL;
  char* pass = NULL;
  char* salt = NULL;
  ssize_t passlen = -1;
  ssize_t saltlen = -1;
  ssize_t keylen = -1;
  ssize_t pass_written = -1;
  ssize_t salt_written = -1;
  ssize_t iter = -1;
  PBKDF2Request* req = NULL;
  Local<Object> obj;

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

  pass = static_cast<char*>(malloc(passlen));
  if (pass == NULL) {
    FatalError("node::PBKDF2()", "Out of Memory");
  }
  pass_written = DecodeWrite(pass, passlen, args[0], BINARY);
  assert(pass_written == passlen);

  ASSERT_IS_BUFFER(args[1]);
  saltlen = Buffer::Length(args[1]);
  if (saltlen < 0) {
    type_error = "Bad salt";
    goto err;
  }

  salt = static_cast<char*>(malloc(saltlen));
  if (salt == NULL) {
    FatalError("node::PBKDF2()", "Out of Memory");
  }
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

  obj = Object::New();
  req = new PBKDF2Request(env, obj, passlen, pass, saltlen, salt, iter, keylen);

  if (args[4]->IsFunction()) {
    obj->Set(env->ondone_string(), args[4]);
    uv_queue_work(env->event_loop(),
                  req->work_req(),
                  EIO_PBKDF2,
                  EIO_PBKDF2After);
  } else {
    Local<Value> argv[2];
    EIO_PBKDF2(req);
    EIO_PBKDF2After(req, argv);
    if (argv[0]->IsObject())
      ThrowException(argv[0]);
    else
      args.GetReturnValue().Set(argv[1]);
  }
  return;

 err:
  free(salt);
  free(pass);
  return ThrowTypeError(type_error);
}


// Only instantiate within a valid HandleScope.
class RandomBytesRequest : public AsyncWrap {
 public:
  RandomBytesRequest(Environment* env, Local<Object> object, size_t size)
      : AsyncWrap(env, object),
        error_(0),
        size_(size),
        data_(static_cast<char*>(malloc(size))) {
    if (data() == NULL)
      FatalError("node::RandomBytesRequest()", "Out of Memory");
  }

  ~RandomBytesRequest() {
    persistent().Dispose();
  }

  uv_work_t* work_req() {
    return &work_req_;
  }

  inline size_t size() const {
    return size_;
  }

  inline char* data() const {
    return data_;
  }

  inline void release() {
    free(data_);
    size_ = 0;
  }

  inline void return_memory(char** d, size_t* len) {
    *d = data_;
    data_ = NULL;
    *len = size_;
    size_ = 0;
  }

  inline unsigned long error() const {
    return error_;
  }

  inline void set_error(unsigned long err) {
    error_ = err;
  }

  // TODO(trevnorris): Make private and make work with CONTAINER_OF macro.
  uv_work_t work_req_;

 private:
  unsigned long error_;
  size_t size_;
  char* data_;
};


template <bool pseudoRandom>
void RandomBytesWork(uv_work_t* work_req) {
  RandomBytesRequest* req = CONTAINER_OF(work_req,
                                         RandomBytesRequest,
                                         work_req_);
  int r;

  if (pseudoRandom == true) {
    r = RAND_pseudo_bytes(reinterpret_cast<unsigned char*>(req->data()),
                          req->size());
  } else {
    r = RAND_bytes(reinterpret_cast<unsigned char*>(req->data()), req->size());
  }

  // RAND_bytes() returns 0 on error. RAND_pseudo_bytes() returns 0 when the
  // result is not cryptographically strong - but that's not an error.
  if (r == 0 && pseudoRandom == false) {
    req->set_error(ERR_get_error());
  } else if (r == -1) {
    req->set_error(static_cast<unsigned long>(-1));
  }
}


// don't call this function without a valid HandleScope
void RandomBytesCheck(RandomBytesRequest* req, Local<Value> argv[2]) {
  if (req->error()) {
    char errmsg[256] = "Operation not supported";

    if (req->error() != static_cast<unsigned long>(-1))
      ERR_error_string_n(req->error(), errmsg, sizeof errmsg);

    argv[0] = Exception::Error(OneByteString(node_isolate, errmsg));
    argv[1] = Null(node_isolate);
    req->release();
  } else {
    char* data = NULL;
    size_t size;
    req->return_memory(&data, &size);
    argv[0] = Null(node_isolate);
    argv[1] = Buffer::Use(data, size);
  }
}


void RandomBytesAfter(uv_work_t* work_req, int status) {
  assert(status == 0);
  RandomBytesRequest* req = CONTAINER_OF(work_req,
                                         RandomBytesRequest,
                                         work_req_);
  Environment* env = req->env();
  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());
  Local<Value> argv[2];
  RandomBytesCheck(req, argv);
  req->MakeCallback(env->ondone_string(), ARRAY_SIZE(argv), argv);
  delete req;
}


template <bool pseudoRandom>
void RandomBytes(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  // maybe allow a buffer to write to? cuts down on object creation
  // when generating random data in a loop
  if (!args[0]->IsUint32()) {
    return ThrowTypeError("Argument #1 must be number > 0");
  }

  const uint32_t size = args[0]->Uint32Value();
  if (size > Buffer::kMaxLength) {
    return ThrowTypeError("size > Buffer::kMaxLength");
  }

  Local<Object> obj = Object::New();
  RandomBytesRequest* req = new RandomBytesRequest(env, obj, size);

  if (args[1]->IsFunction()) {
    obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "ondone"), args[1]);
    uv_queue_work(env->event_loop(),
                  req->work_req(),
                  RandomBytesWork<pseudoRandom>,
                  RandomBytesAfter);
    args.GetReturnValue().Set(obj);
  } else {
    Local<Value> argv[2];
    RandomBytesWork<pseudoRandom>(req->work_req());
    RandomBytesCheck(req, argv);
    delete req;

    if (!argv[0]->IsNull())
      ThrowException(argv[0]);
    else
      args.GetReturnValue().Set(argv[1]);
  }
}


void GetSSLCiphers(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

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
    arr->Set(i, OneByteString(node_isolate, SSL_CIPHER_get_name(cipher)));
  }

  SSL_free(ssl);
  SSL_CTX_free(ctx);

  args.GetReturnValue().Set(arr);
}


template <class TypeName>
static void array_push_back(const TypeName* md,
                            const char* from,
                            const char* to,
                            void* arg) {
  Local<Array>& arr = *static_cast<Local<Array>*>(arg);
  arr->Set(arr->Length(), OneByteString(node_isolate, from));
}


void GetCiphers(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  Local<Array> arr = Array::New();
  EVP_CIPHER_do_all_sorted(array_push_back<EVP_CIPHER>, &arr);
  args.GetReturnValue().Set(arr);
}


void GetHashes(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  Local<Array> arr = Array::New();
  EVP_MD_do_all_sorted(array_push_back<EVP_MD>, &arr);
  args.GetReturnValue().Set(arr);
}


void Certificate::Initialize(Handle<Object> target) {
  HandleScope scope(node_isolate);

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(t, "verifySpkac", VerifySpkac);
  NODE_SET_PROTOTYPE_METHOD(t, "exportPublicKey", ExportPublicKey);
  NODE_SET_PROTOTYPE_METHOD(t, "exportChallenge", ExportChallenge);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "Certificate"),
              t->GetFunction());
}


void Certificate::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  new Certificate(env, args.This());
}


bool Certificate::VerifySpkac(const char* data, unsigned int len) {
  bool i = 0;
  EVP_PKEY* pkey = NULL;
  NETSCAPE_SPKI* spki = NULL;

  spki = NETSCAPE_SPKI_b64_decode(data, len);
  if (spki == NULL)
    goto exit;

  pkey = X509_PUBKEY_get(spki->spkac->pubkey);
  if (pkey == NULL)
    goto exit;

  i = NETSCAPE_SPKI_verify(spki, pkey) > 0;

 exit:
  if (pkey != NULL)
    EVP_PKEY_free(pkey);

  if (spki != NULL)
    NETSCAPE_SPKI_free(spki);

  return i;
}


void Certificate::VerifySpkac(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Certificate* certificate = Unwrap<Certificate>(args.This());
  bool i = false;

  if (args.Length() < 1)
    return ThrowTypeError("Missing argument");

  ASSERT_IS_BUFFER(args[0]);

  size_t length = Buffer::Length(args[0]);
  if (length == 0)
    return args.GetReturnValue().Set(i);

  char* data = Buffer::Data(args[0]);
  assert(data != NULL);

  i = certificate->VerifySpkac(data, length) > 0;

  args.GetReturnValue().Set(i);
}


const char* Certificate::ExportPublicKey(const char* data, int len) {
  char* buf = NULL;
  EVP_PKEY* pkey = NULL;
  NETSCAPE_SPKI* spki = NULL;

  BIO* bio = BIO_new(BIO_s_mem());
  if (bio == NULL)
    goto exit;

  spki = NETSCAPE_SPKI_b64_decode(data, len);
  if (spki == NULL)
    goto exit;

  pkey = NETSCAPE_SPKI_get_pubkey(spki);
  if (pkey == NULL)
    goto exit;

  if (PEM_write_bio_PUBKEY(bio, pkey) <= 0)
    goto exit;

  BIO_write(bio, "\0", 1);
  BUF_MEM* ptr;
  BIO_get_mem_ptr(bio, &ptr);

  buf = new char[ptr->length];
  memcpy(buf, ptr->data, ptr->length);

 exit:
  if (pkey != NULL)
    EVP_PKEY_free(pkey);

  if (spki != NULL)
    NETSCAPE_SPKI_free(spki);

  if (bio != NULL)
    BIO_free_all(bio);

  return buf;
}


void Certificate::ExportPublicKey(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Certificate* certificate = Unwrap<Certificate>(args.This());

  if (args.Length() < 1)
    return ThrowTypeError("Missing argument");

  ASSERT_IS_BUFFER(args[0]);

  size_t length = Buffer::Length(args[0]);
  if (length == 0)
    return args.GetReturnValue().SetEmptyString();

  char* data = Buffer::Data(args[0]);
  assert(data != NULL);

  const char* pkey = certificate->ExportPublicKey(data, length);
  if (pkey == NULL)
    return args.GetReturnValue().SetEmptyString();

  Local<Value> out = Encode(pkey, strlen(pkey), BUFFER);

  delete[] pkey;

  args.GetReturnValue().Set(out);
}


const char* Certificate::ExportChallenge(const char* data, int len) {
  NETSCAPE_SPKI* sp = NULL;

  sp = NETSCAPE_SPKI_b64_decode(data, len);
  if (sp == NULL)
    return NULL;

  const char* buf = NULL;
  buf = reinterpret_cast<const char*>(ASN1_STRING_data(sp->spkac->challenge));

  return buf;
}


void Certificate::ExportChallenge(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Certificate* crt = Unwrap<Certificate>(args.This());

  if (args.Length() < 1)
    return ThrowTypeError("Missing argument");

  ASSERT_IS_BUFFER(args[0]);

  size_t len = Buffer::Length(args[0]);
  if (len == 0)
    return args.GetReturnValue().SetEmptyString();

  char* data = Buffer::Data(args[0]);
  assert(data != NULL);

  const char* cert = crt->ExportChallenge(data, len);
  if (cert == NULL)
    return args.GetReturnValue().SetEmptyString();

  Local<Value> outString = Encode(cert, strlen(cert), BUFFER);

  delete[] cert;

  args.GetReturnValue().Set(outString);
}


void InitCryptoOnce() {
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  OpenSSL_add_all_digests();
  SSL_load_error_strings();
  ERR_load_crypto_strings();

  crypto_lock_init();
  CRYPTO_set_locking_callback(crypto_lock_cb);
  CRYPTO_THREADID_set_callback(crypto_threadid_cb);

  // Turn off compression. Saves memory and protects against BEAST attacks.
#if !defined(OPENSSL_NO_COMP)
#if OPENSSL_VERSION_NUMBER < 0x00908000L
  STACK_OF(SSL_COMP)* comp_methods = SSL_COMP_get_compression_method();
#else
  STACK_OF(SSL_COMP)* comp_methods = SSL_COMP_get_compression_methods();
#endif
  sk_SSL_COMP_zero(comp_methods);
  assert(sk_SSL_COMP_num(comp_methods) == 0);
#endif
}


// FIXME(bnoordhuis) Handle global init correctly.
void InitCrypto(Handle<Object> target,
                Handle<Value> unused,
                Handle<Context> context) {
  static uv_once_t init_once = UV_ONCE_INIT;
  uv_once(&init_once, InitCryptoOnce);

  Environment* env = Environment::GetCurrent(context);
  SecureContext::Initialize(env, target);
  Connection::Initialize(env, target);
  CipherBase::Initialize(env, target);
  DiffieHellman::Initialize(env, target);
  Hmac::Initialize(env, target);
  Hash::Initialize(env, target);
  Sign::Initialize(env, target);
  Verify::Initialize(env, target);
  Certificate::Initialize(target);

  NODE_SET_METHOD(target, "PBKDF2", PBKDF2);
  NODE_SET_METHOD(target, "randomBytes", RandomBytes<false>);
  NODE_SET_METHOD(target, "pseudoRandomBytes", RandomBytes<true>);
  NODE_SET_METHOD(target, "getSSLCiphers", GetSSLCiphers);
  NODE_SET_METHOD(target, "getCiphers", GetCiphers);
  NODE_SET_METHOD(target, "getHashes", GetHashes);
}

}  // namespace crypto
}  // namespace node

NODE_MODULE_CONTEXT_AWARE(node_crypto, node::crypto::InitCrypto)
