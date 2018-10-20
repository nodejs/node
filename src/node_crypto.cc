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
#include "node_errors.h"
#include "node_constants.h"
#include "node_crypto.h"
#include "node_crypto_bio.h"
#include "node_crypto_groups.h"
#include "node_crypto_clienthello-inl.h"
#include "node_mutex.h"
#include "node_internals.h"
#include "tls_wrap.h"  // TLSWrap

#include "async_wrap-inl.h"
#include "env-inl.h"
#include "string_bytes.h"
#include "util-inl.h"
#include "v8.h"

#include <errno.h>
#include <limits.h>  // INT_MAX
#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

static const int X509_NAME_FLAGS = ASN1_STRFLGS_ESC_CTRL
                                 | ASN1_STRFLGS_UTF8_CONVERT
                                 | XN_FLAG_SEP_MULTILINE
                                 | XN_FLAG_FN_SN;

namespace node {
namespace crypto {

using v8::Array;
using v8::Boolean;
using v8::ConstructorBehavior;
using v8::Context;
using v8::DontDelete;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::External;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Nothing;
using v8::Null;
using v8::Object;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::SideEffectType;
using v8::Signature;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;

#ifdef OPENSSL_NO_OCB
# define IS_OCB_MODE(mode) false
#else
# define IS_OCB_MODE(mode) ((mode) == EVP_CIPH_OCB_MODE)
#endif

struct StackOfX509Deleter {
  void operator()(STACK_OF(X509)* p) const { sk_X509_pop_free(p, X509_free); }
};
using StackOfX509 = std::unique_ptr<STACK_OF(X509), StackOfX509Deleter>;

struct StackOfXASN1Deleter {
  void operator()(STACK_OF(ASN1_OBJECT)* p) const {
    sk_ASN1_OBJECT_pop_free(p, ASN1_OBJECT_free);
  }
};
using StackOfASN1 = std::unique_ptr<STACK_OF(ASN1_OBJECT), StackOfXASN1Deleter>;

// OPENSSL_free is a macro, so we need a wrapper function.
struct OpenSSLBufferDeleter {
  void operator()(char* pointer) const { OPENSSL_free(pointer); }
};
using OpenSSLBuffer = std::unique_ptr<char[], OpenSSLBufferDeleter>;

static const char* const root_certs[] = {
#include "node_root_certs.h"  // NOLINT(build/include_order)
};

static const char system_cert_path[] = NODE_OPENSSL_SYSTEM_CERT_PATH;

static std::string extra_root_certs_file;  // NOLINT(runtime/string)

static X509_STORE* root_cert_store;

// Just to generate static methods
template void SSLWrap<TLSWrap>::AddMethods(Environment* env,
                                           Local<FunctionTemplate> t);
template void SSLWrap<TLSWrap>::ConfigureSecureContext(SecureContext* sc);
template void SSLWrap<TLSWrap>::SetSNIContext(SecureContext* sc);
template int SSLWrap<TLSWrap>::SetCACerts(SecureContext* sc);
template SSL_SESSION* SSLWrap<TLSWrap>::GetSessionCallback(
    SSL* s,
    const unsigned char* key,
    int len,
    int* copy);
template int SSLWrap<TLSWrap>::NewSessionCallback(SSL* s,
                                                  SSL_SESSION* sess);
template void SSLWrap<TLSWrap>::OnClientHello(
    void* arg,
    const ClientHelloParser::ClientHello& hello);
template int SSLWrap<TLSWrap>::TLSExtStatusCallback(SSL* s, void* arg);
template void SSLWrap<TLSWrap>::DestroySSL();
template int SSLWrap<TLSWrap>::SSLCertCallback(SSL* s, void* arg);
template void SSLWrap<TLSWrap>::WaitForCertCb(CertCb cb, void* arg);
template int SSLWrap<TLSWrap>::SelectALPNCallback(
    SSL* s,
    const unsigned char** out,
    unsigned char* outlen,
    const unsigned char* in,
    unsigned int inlen,
    void* arg);


static int PasswordCallback(char* buf, int size, int rwflag, void* u) {
  if (u) {
    size_t buflen = static_cast<size_t>(size);
    size_t len = strlen(static_cast<const char*>(u));
    len = len > buflen ? buflen : len;
    memcpy(buf, u, len);
    return len;
  }

  return 0;
}

// Loads OpenSSL engine by engine id and returns it. The loaded engine
// gets a reference so remember the corresponding call to ENGINE_free.
// In case of error the appropriate js exception is scheduled
// and nullptr is returned.
#ifndef OPENSSL_NO_ENGINE
static ENGINE* LoadEngineById(const char* engine_id, char (*errmsg)[1024]) {
  MarkPopErrorOnReturn mark_pop_error_on_return;

  ENGINE* engine = ENGINE_by_id(engine_id);

  if (engine == nullptr) {
    // Engine not found, try loading dynamically.
    engine = ENGINE_by_id("dynamic");
    if (engine != nullptr) {
      if (!ENGINE_ctrl_cmd_string(engine, "SO_PATH", engine_id, 0) ||
          !ENGINE_ctrl_cmd_string(engine, "LOAD", nullptr, 0)) {
        ENGINE_free(engine);
        engine = nullptr;
      }
    }
  }

  if (engine == nullptr) {
    int err = ERR_get_error();
    if (err != 0) {
      ERR_error_string_n(err, *errmsg, sizeof(*errmsg));
    } else {
      snprintf(*errmsg, sizeof(*errmsg),
               "Engine \"%s\" was not found", engine_id);
    }
  }

  return engine;
}
#endif  // !OPENSSL_NO_ENGINE

// This callback is used to avoid the default passphrase callback in OpenSSL
// which will typically prompt for the passphrase. The prompting is designed
// for the OpenSSL CLI, but works poorly for Node.js because it involves
// synchronous interaction with the controlling terminal, something we never
// want, and use this function to avoid it.
static int NoPasswordCallback(char* buf, int size, int rwflag, void* u) {
  return 0;
}


struct CryptoErrorVector : public std::vector<std::string> {
  inline void Capture() {
    clear();
    while (auto err = ERR_get_error()) {
      char buf[256];
      ERR_error_string_n(err, buf, sizeof(buf));
      push_back(buf);
    }
    std::reverse(begin(), end());
  }

  inline Local<Value> ToException(
      Environment* env,
      Local<String> exception_string = Local<String>()) const {
    if (exception_string.IsEmpty()) {
      CryptoErrorVector copy(*this);
      if (copy.empty()) copy.push_back("no error");  // But possibly a bug...
      // Use last element as the error message, everything else goes
      // into the .opensslErrorStack property on the exception object.
      auto exception_string =
          String::NewFromUtf8(env->isolate(), copy.back().data(),
                              NewStringType::kNormal, copy.back().size())
          .ToLocalChecked();
      copy.pop_back();
      return copy.ToException(env, exception_string);
    }

    Local<Value> exception_v = Exception::Error(exception_string);
    CHECK(!exception_v.IsEmpty());

    if (!empty()) {
      Local<Array> array = Array::New(env->isolate(), size());
      CHECK(!array.IsEmpty());

      for (const std::string& string : *this) {
        const size_t index = &string - &front();
        Local<String> value =
            String::NewFromUtf8(env->isolate(), string.data(),
                                NewStringType::kNormal, string.size())
            .ToLocalChecked();
        array->Set(env->context(), index, value).FromJust();
      }

      CHECK(exception_v->IsObject());
      Local<Object> exception = exception_v.As<Object>();
      exception->Set(env->context(),
                     env->openssl_error_stack(), array).FromJust();
    }

    return exception_v;
  }
};


void ThrowCryptoError(Environment* env,
                      unsigned long err,  // NOLINT(runtime/int)
                      const char* message = nullptr) {
  char message_buffer[128] = {0};
  if (err != 0 || message == nullptr) {
    ERR_error_string_n(err, message_buffer, sizeof(message_buffer));
    message = message_buffer;
  }
  HandleScope scope(env->isolate());
  auto exception_string =
      String::NewFromUtf8(env->isolate(), message, NewStringType::kNormal)
      .ToLocalChecked();
  CryptoErrorVector errors;
  errors.Capture();
  auto exception = errors.ToException(env, exception_string);
  env->isolate()->ThrowException(exception);
}


// Ensure that OpenSSL has enough entropy (at least 256 bits) for its PRNG.
// The entropy pool starts out empty and needs to fill up before the PRNG
// can be used securely.  Once the pool is filled, it never dries up again;
// its contents is stirred and reused when necessary.
//
// OpenSSL normally fills the pool automatically but not when someone starts
// generating random numbers before the pool is full: in that case OpenSSL
// keeps lowering the entropy estimate to thwart attackers trying to guess
// the initial state of the PRNG.
//
// When that happens, we will have to wait until enough entropy is available.
// That should normally never take longer than a few milliseconds.
//
// OpenSSL draws from /dev/random and /dev/urandom.  While /dev/random may
// block pending "true" randomness, /dev/urandom is a CSPRNG that doesn't
// block under normal circumstances.
//
// The only time when /dev/urandom may conceivably block is right after boot,
// when the whole system is still low on entropy.  That's not something we can
// do anything about.
inline void CheckEntropy() {
  for (;;) {
    int status = RAND_status();
    CHECK_GE(status, 0);  // Cannot fail.
    if (status != 0)
      break;

    // Give up, RAND_poll() not supported.
    if (RAND_poll() == 0)
      break;
  }
}


bool EntropySource(unsigned char* buffer, size_t length) {
  // Ensure that OpenSSL's PRNG is properly seeded.
  CheckEntropy();
  // RAND_bytes() can return 0 to indicate that the entropy data is not truly
  // random. That's okay, it's still better than V8's stock source of entropy,
  // which is /dev/urandom on UNIX platforms and the current time on Windows.
  return RAND_bytes(buffer, length) != -1;
}


void SecureContext::Initialize(Environment* env, Local<Object> target) {
  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  Local<String> secureContextString =
      FIXED_ONE_BYTE_STRING(env->isolate(), "SecureContext");
  t->SetClassName(secureContextString);

  env->SetProtoMethod(t, "init", Init);
  env->SetProtoMethod(t, "setKey", SetKey);
  env->SetProtoMethod(t, "setCert", SetCert);
  env->SetProtoMethod(t, "addCACert", AddCACert);
  env->SetProtoMethod(t, "addCRL", AddCRL);
  env->SetProtoMethod(t, "addRootCerts", AddRootCerts);
  env->SetProtoMethod(t, "setCiphers", SetCiphers);
  env->SetProtoMethod(t, "setECDHCurve", SetECDHCurve);
  env->SetProtoMethod(t, "setDHParam", SetDHParam);
  env->SetProtoMethod(t, "setOptions", SetOptions);
  env->SetProtoMethod(t, "setSessionIdContext", SetSessionIdContext);
  env->SetProtoMethod(t, "setSessionTimeout", SetSessionTimeout);
  env->SetProtoMethod(t, "close", Close);
  env->SetProtoMethod(t, "loadPKCS12", LoadPKCS12);
#ifndef OPENSSL_NO_ENGINE
  env->SetProtoMethod(t, "setClientCertEngine", SetClientCertEngine);
#endif  // !OPENSSL_NO_ENGINE
  env->SetProtoMethodNoSideEffect(t, "getTicketKeys", GetTicketKeys);
  env->SetProtoMethod(t, "setTicketKeys", SetTicketKeys);
  env->SetProtoMethod(t, "setFreeListLength", SetFreeListLength);
  env->SetProtoMethod(t, "enableTicketKeyCallback", EnableTicketKeyCallback);
  env->SetProtoMethodNoSideEffect(t, "getCertificate", GetCertificate<true>);
  env->SetProtoMethodNoSideEffect(t, "getIssuer", GetCertificate<false>);

#define SET_INTEGER_CONSTANTS(name, value)                                     \
    t->Set(FIXED_ONE_BYTE_STRING(env->isolate(), name),                        \
           Integer::NewFromUnsigned(env->isolate(), value));
  SET_INTEGER_CONSTANTS("kTicketKeyReturnIndex", kTicketKeyReturnIndex);
  SET_INTEGER_CONSTANTS("kTicketKeyHMACIndex", kTicketKeyHMACIndex);
  SET_INTEGER_CONSTANTS("kTicketKeyAESIndex", kTicketKeyAESIndex);
  SET_INTEGER_CONSTANTS("kTicketKeyNameIndex", kTicketKeyNameIndex);
  SET_INTEGER_CONSTANTS("kTicketKeyIVIndex", kTicketKeyIVIndex);

#undef SET_INTEGER_CONSTANTS

  Local<FunctionTemplate> ctx_getter_templ =
      FunctionTemplate::New(env->isolate(),
                            CtxGetter,
                            env->as_external(),
                            Signature::New(env->isolate(), t));


  t->PrototypeTemplate()->SetAccessorProperty(
      FIXED_ONE_BYTE_STRING(env->isolate(), "_external"),
      ctx_getter_templ,
      Local<FunctionTemplate>(),
      static_cast<PropertyAttribute>(ReadOnly | DontDelete));

  target->Set(secureContextString,
              t->GetFunction(env->context()).ToLocalChecked());
  env->set_secure_context_constructor_template(t);
}


void SecureContext::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  new SecureContext(env, args.This());
}


void SecureContext::Init(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  Environment* env = sc->env();

  CHECK_EQ(args.Length(), 3);
  CHECK(args[1]->IsInt32());
  CHECK(args[2]->IsInt32());

  int min_version = args[1].As<Int32>()->Value();
  int max_version = args[2].As<Int32>()->Value();
  const SSL_METHOD* method = TLS_method();

  if (args[0]->IsString()) {
    const node::Utf8Value sslmethod(env->isolate(), args[0]);

    // Note that SSLv2 and SSLv3 are disallowed but SSLv23_method and friends
    // are still accepted.  They are OpenSSL's way of saying that all known
    // protocols are supported unless explicitly disabled (which we do below
    // for SSLv2 and SSLv3.)
    if (strcmp(*sslmethod, "SSLv2_method") == 0) {
      return env->ThrowError("SSLv2 methods disabled");
    } else if (strcmp(*sslmethod, "SSLv2_server_method") == 0) {
      return env->ThrowError("SSLv2 methods disabled");
    } else if (strcmp(*sslmethod, "SSLv2_client_method") == 0) {
      return env->ThrowError("SSLv2 methods disabled");
    } else if (strcmp(*sslmethod, "SSLv3_method") == 0) {
      return env->ThrowError("SSLv3 methods disabled");
    } else if (strcmp(*sslmethod, "SSLv3_server_method") == 0) {
      return env->ThrowError("SSLv3 methods disabled");
    } else if (strcmp(*sslmethod, "SSLv3_client_method") == 0) {
      return env->ThrowError("SSLv3 methods disabled");
    } else if (strcmp(*sslmethod, "SSLv23_method") == 0) {
      // noop
    } else if (strcmp(*sslmethod, "SSLv23_server_method") == 0) {
      method = TLS_server_method();
    } else if (strcmp(*sslmethod, "SSLv23_client_method") == 0) {
      method = TLS_client_method();
    } else if (strcmp(*sslmethod, "TLS_method") == 0) {
      min_version = 0;
      max_version = 0;
    } else if (strcmp(*sslmethod, "TLSv1_method") == 0) {
      min_version = TLS1_VERSION;
      max_version = TLS1_VERSION;
    } else if (strcmp(*sslmethod, "TLSv1_server_method") == 0) {
      min_version = TLS1_VERSION;
      max_version = TLS1_VERSION;
      method = TLS_server_method();
    } else if (strcmp(*sslmethod, "TLSv1_client_method") == 0) {
      min_version = TLS1_VERSION;
      max_version = TLS1_VERSION;
      method = TLS_client_method();
    } else if (strcmp(*sslmethod, "TLSv1_1_method") == 0) {
      min_version = TLS1_1_VERSION;
      max_version = TLS1_1_VERSION;
    } else if (strcmp(*sslmethod, "TLSv1_1_server_method") == 0) {
      min_version = TLS1_1_VERSION;
      max_version = TLS1_1_VERSION;
      method = TLS_server_method();
    } else if (strcmp(*sslmethod, "TLSv1_1_client_method") == 0) {
      min_version = TLS1_1_VERSION;
      max_version = TLS1_1_VERSION;
      method = TLS_client_method();
    } else if (strcmp(*sslmethod, "TLSv1_2_method") == 0) {
      min_version = TLS1_2_VERSION;
      max_version = TLS1_2_VERSION;
    } else if (strcmp(*sslmethod, "TLSv1_2_server_method") == 0) {
      min_version = TLS1_2_VERSION;
      max_version = TLS1_2_VERSION;
      method = TLS_server_method();
    } else if (strcmp(*sslmethod, "TLSv1_2_client_method") == 0) {
      min_version = TLS1_2_VERSION;
      max_version = TLS1_2_VERSION;
      method = TLS_client_method();
    } else {
      return env->ThrowError("Unknown method");
    }
  }

  sc->ctx_.reset(SSL_CTX_new(method));
  SSL_CTX_set_app_data(sc->ctx_.get(), sc);

  // Disable SSLv2 in the case when method == TLS_method() and the
  // cipher list contains SSLv2 ciphers (not the default, should be rare.)
  // The bundled OpenSSL doesn't have SSLv2 support but the system OpenSSL may.
  // SSLv3 is disabled because it's susceptible to downgrade attacks (POODLE.)
  SSL_CTX_set_options(sc->ctx_.get(), SSL_OP_NO_SSLv2);
  SSL_CTX_set_options(sc->ctx_.get(), SSL_OP_NO_SSLv3);

  // Enable automatic cert chaining. This is enabled by default in OpenSSL, but
  // disabled by default in BoringSSL. Enable it explicitly to make the
  // behavior match when Node is built with BoringSSL.
  SSL_CTX_clear_mode(sc->ctx_.get(), SSL_MODE_NO_AUTO_CHAIN);

  // SSL session cache configuration
  SSL_CTX_set_session_cache_mode(sc->ctx_.get(),
                                 SSL_SESS_CACHE_SERVER |
                                 SSL_SESS_CACHE_NO_INTERNAL |
                                 SSL_SESS_CACHE_NO_AUTO_CLEAR);

  SSL_CTX_set_min_proto_version(sc->ctx_.get(), min_version);

  if (max_version == 0) {
    // Selecting some secureProtocol methods allows the TLS version to be "any
    // supported", but we don't support TLSv1.3, even if OpenSSL does.
    max_version = TLS1_2_VERSION;
  }
  SSL_CTX_set_max_proto_version(sc->ctx_.get(), max_version);
  // OpenSSL 1.1.0 changed the ticket key size, but the OpenSSL 1.0.x size was
  // exposed in the public API. To retain compatibility, install a callback
  // which restores the old algorithm.
  if (RAND_bytes(sc->ticket_key_name_, sizeof(sc->ticket_key_name_)) <= 0 ||
      RAND_bytes(sc->ticket_key_hmac_, sizeof(sc->ticket_key_hmac_)) <= 0 ||
      RAND_bytes(sc->ticket_key_aes_, sizeof(sc->ticket_key_aes_)) <= 0) {
    return env->ThrowError("Error generating ticket keys");
  }
  SSL_CTX_set_tlsext_ticket_key_cb(sc->ctx_.get(), TicketCompatibilityCallback);
}


// Takes a string or buffer and loads it into a BIO.
// Caller responsible for BIO_free_all-ing the returned object.
static BIOPointer LoadBIO(Environment* env, Local<Value> v) {
  HandleScope scope(env->isolate());

  if (v->IsString()) {
    const node::Utf8Value s(env->isolate(), v);
    return NodeBIO::NewFixed(*s, s.length());
  }

  if (Buffer::HasInstance(v)) {
    return NodeBIO::NewFixed(Buffer::Data(v), Buffer::Length(v));
  }

  return nullptr;
}


void SecureContext::SetKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  unsigned int len = args.Length();
  if (len < 1) {
    return THROW_ERR_MISSING_ARGS(env, "Private key argument is mandatory");
  }

  if (len > 2) {
    return env->ThrowError("Only private key and pass phrase are expected");
  }

  if (len == 2) {
    if (args[1]->IsUndefined() || args[1]->IsNull())
      len = 1;
    else
      THROW_AND_RETURN_IF_NOT_STRING(env, args[1], "Pass phrase");
  }

  BIOPointer bio(LoadBIO(env, args[0]));
  if (!bio)
    return;

  node::Utf8Value passphrase(env->isolate(), args[1]);

  EVPKeyPointer key(
      PEM_read_bio_PrivateKey(bio.get(),
                              nullptr,
                              PasswordCallback,
                              len == 1 ? nullptr : *passphrase));

  if (!key) {
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
    if (!err) {
      return env->ThrowError("PEM_read_bio_PrivateKey");
    }
    return ThrowCryptoError(env, err);
  }

  int rv = SSL_CTX_use_PrivateKey(sc->ctx_.get(), key.get());

  if (!rv) {
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
    if (!err)
      return env->ThrowError("SSL_CTX_use_PrivateKey");
    return ThrowCryptoError(env, err);
  }
}


int SSL_CTX_get_issuer(SSL_CTX* ctx, X509* cert, X509** issuer) {
  X509_STORE* store = SSL_CTX_get_cert_store(ctx);
  DeleteFnPtr<X509_STORE_CTX, X509_STORE_CTX_free> store_ctx(
      X509_STORE_CTX_new());
  return store_ctx.get() != nullptr &&
         X509_STORE_CTX_init(store_ctx.get(), store, nullptr, nullptr) == 1 &&
         X509_STORE_CTX_get1_issuer(issuer, store_ctx.get(), cert) == 1;
}


int SSL_CTX_use_certificate_chain(SSL_CTX* ctx,
                                  X509Pointer&& x,
                                  STACK_OF(X509)* extra_certs,
                                  X509Pointer* cert,
                                  X509Pointer* issuer_) {
  CHECK(!*issuer_);
  CHECK(!*cert);
  X509* issuer = nullptr;

  int ret = SSL_CTX_use_certificate(ctx, x.get());

  if (ret) {
    // If we could set up our certificate, now proceed to
    // the CA certificates.
    SSL_CTX_clear_extra_chain_certs(ctx);

    for (int i = 0; i < sk_X509_num(extra_certs); i++) {
      X509* ca = sk_X509_value(extra_certs, i);

      // NOTE: Increments reference count on `ca`
      if (!SSL_CTX_add1_chain_cert(ctx, ca)) {
        ret = 0;
        issuer = nullptr;
        break;
      }
      // Note that we must not free r if it was successfully
      // added to the chain (while we must free the main
      // certificate, since its reference count is increased
      // by SSL_CTX_use_certificate).

      // Find issuer
      if (issuer != nullptr || X509_check_issued(ca, x.get()) != X509_V_OK)
        continue;

      issuer = ca;
    }
  }

  // Try getting issuer from a cert store
  if (ret) {
    if (issuer == nullptr) {
      ret = SSL_CTX_get_issuer(ctx, x.get(), &issuer);
      ret = ret < 0 ? 0 : 1;
      // NOTE: get_cert_store doesn't increment reference count,
      // no need to free `store`
    } else {
      // Increment issuer reference count
      issuer = X509_dup(issuer);
      if (issuer == nullptr) {
        ret = 0;
      }
    }
  }

  issuer_->reset(issuer);

  if (ret && x != nullptr) {
    cert->reset(X509_dup(x.get()));
    if (!*cert)
      ret = 0;
  }
  return ret;
}


// Read a file that contains our certificate in "PEM" format,
// possibly followed by a sequence of CA certificates that should be
// sent to the peer in the Certificate message.
//
// Taken from OpenSSL - edited for style.
int SSL_CTX_use_certificate_chain(SSL_CTX* ctx,
                                  BIOPointer&& in,
                                  X509Pointer* cert,
                                  X509Pointer* issuer) {
  // Just to ensure that `ERR_peek_last_error` below will return only errors
  // that we are interested in
  ERR_clear_error();

  X509Pointer x(
      PEM_read_bio_X509_AUX(in.get(), nullptr, NoPasswordCallback, nullptr));

  if (!x)
    return 0;

  unsigned long err = 0;  // NOLINT(runtime/int)

  StackOfX509 extra_certs(sk_X509_new_null());
  if (!extra_certs)
    return 0;

  while (X509Pointer extra {PEM_read_bio_X509(in.get(),
                                    nullptr,
                                    NoPasswordCallback,
                                    nullptr)}) {
    if (sk_X509_push(extra_certs.get(), extra.get())) {
      extra.release();
      continue;
    }

    return 0;
  }

  // When the while loop ends, it's usually just EOF.
  err = ERR_peek_last_error();
  if (ERR_GET_LIB(err) == ERR_LIB_PEM &&
      ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
    ERR_clear_error();
  } else {
    // some real error
    return 0;
  }

  return SSL_CTX_use_certificate_chain(ctx,
                                       std::move(x),
                                       extra_certs.get(),
                                       cert,
                                       issuer);
}


void SecureContext::SetCert(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  if (args.Length() != 1) {
    return THROW_ERR_MISSING_ARGS(env, "Certificate argument is mandatory");
  }

  BIOPointer bio(LoadBIO(env, args[0]));
  if (!bio)
    return;

  sc->cert_.reset();
  sc->issuer_.reset();

  int rv = SSL_CTX_use_certificate_chain(sc->ctx_.get(),
                                         std::move(bio),
                                         &sc->cert_,
                                         &sc->issuer_);

  if (!rv) {
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
    if (!err) {
      return env->ThrowError("SSL_CTX_use_certificate_chain");
    }
    return ThrowCryptoError(env, err);
  }
}


static X509_STORE* NewRootCertStore() {
  static std::vector<X509*> root_certs_vector;
  static Mutex root_certs_vector_mutex;
  Mutex::ScopedLock lock(root_certs_vector_mutex);

  if (root_certs_vector.empty()) {
    for (size_t i = 0; i < arraysize(root_certs); i++) {
      X509* x509 =
          PEM_read_bio_X509(NodeBIO::NewFixed(root_certs[i],
                                              strlen(root_certs[i])).get(),
                            nullptr,   // no re-use of X509 structure
                            NoPasswordCallback,
                            nullptr);  // no callback data

      // Parse errors from the built-in roots are fatal.
      CHECK_NOT_NULL(x509);

      root_certs_vector.push_back(x509);
    }
  }

  X509_STORE* store = X509_STORE_new();
  if (*system_cert_path != '\0') {
    X509_STORE_load_locations(store, system_cert_path, nullptr);
  }
  // TODO(addaleax): Replace `ssl_openssl_cert_store` with
  // `per_process_opts->ssl_openssl_cert_store`.
  if (ssl_openssl_cert_store) {
    X509_STORE_set_default_paths(store);
  } else {
    for (X509* cert : root_certs_vector) {
      X509_up_ref(cert);
      X509_STORE_add_cert(store, cert);
    }
  }

  return store;
}


void SecureContext::AddCACert(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  ClearErrorOnReturn clear_error_on_return;

  if (args.Length() != 1) {
    return THROW_ERR_MISSING_ARGS(env, "CA certificate argument is mandatory");
  }

  BIOPointer bio(LoadBIO(env, args[0]));
  if (!bio)
    return;

  X509_STORE* cert_store = SSL_CTX_get_cert_store(sc->ctx_.get());
  while (X509* x509 = PEM_read_bio_X509(
      bio.get(), nullptr, NoPasswordCallback, nullptr)) {
    if (cert_store == root_cert_store) {
      cert_store = NewRootCertStore();
      SSL_CTX_set_cert_store(sc->ctx_.get(), cert_store);
    }
    X509_STORE_add_cert(cert_store, x509);
    SSL_CTX_add_client_CA(sc->ctx_.get(), x509);
    X509_free(x509);
  }
}


void SecureContext::AddCRL(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  if (args.Length() != 1) {
    return THROW_ERR_MISSING_ARGS(env, "CRL argument is mandatory");
  }

  ClearErrorOnReturn clear_error_on_return;

  BIOPointer bio(LoadBIO(env, args[0]));
  if (!bio)
    return;

  DeleteFnPtr<X509_CRL, X509_CRL_free> crl(
      PEM_read_bio_X509_CRL(bio.get(), nullptr, NoPasswordCallback, nullptr));

  if (!crl)
    return env->ThrowError("Failed to parse CRL");

  X509_STORE* cert_store = SSL_CTX_get_cert_store(sc->ctx_.get());
  if (cert_store == root_cert_store) {
    cert_store = NewRootCertStore();
    SSL_CTX_set_cert_store(sc->ctx_.get(), cert_store);
  }

  X509_STORE_add_crl(cert_store, crl.get());
  X509_STORE_set_flags(cert_store,
                       X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
}


void UseExtraCaCerts(const std::string& file) {
  extra_root_certs_file = file;
}


static unsigned long AddCertsFromFile(  // NOLINT(runtime/int)
    X509_STORE* store,
    const char* file) {
  ERR_clear_error();
  MarkPopErrorOnReturn mark_pop_error_on_return;

  BIOPointer bio(BIO_new_file(file, "r"));
  if (!bio)
    return ERR_get_error();

  while (X509* x509 =
      PEM_read_bio_X509(bio.get(), nullptr, NoPasswordCallback, nullptr)) {
    X509_STORE_add_cert(store, x509);
    X509_free(x509);
  }

  unsigned long err = ERR_peek_error();  // NOLINT(runtime/int)
  // Ignore error if its EOF/no start line found.
  if (ERR_GET_LIB(err) == ERR_LIB_PEM &&
      ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
    return 0;
  }

  return err;
}

void SecureContext::AddRootCerts(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  ClearErrorOnReturn clear_error_on_return;

  if (!root_cert_store) {
    root_cert_store = NewRootCertStore();

    if (!extra_root_certs_file.empty()) {
      unsigned long err = AddCertsFromFile(  // NOLINT(runtime/int)
                                           root_cert_store,
                                           extra_root_certs_file.c_str());
      if (err) {
        // We do not call back into JS after this line anyway, so ignoring
        // the return value of ProcessEmitWarning does not affect how a
        // possible exception would be propagated.
        ProcessEmitWarning(sc->env(),
                           "Ignoring extra certs from `%s`, "
                           "load failed: %s\n",
                           extra_root_certs_file.c_str(),
                           ERR_error_string(err, nullptr));
      }
    }
  }

  // Increment reference count so global store is not deleted along with CTX.
  X509_STORE_up_ref(root_cert_store);
  SSL_CTX_set_cert_store(sc->ctx_.get(), root_cert_store);
}


void SecureContext::SetCiphers(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  Environment* env = sc->env();
  ClearErrorOnReturn clear_error_on_return;

  if (args.Length() != 1) {
    return THROW_ERR_MISSING_ARGS(env, "Ciphers argument is mandatory");
  }

  THROW_AND_RETURN_IF_NOT_STRING(env, args[0], "Ciphers");

  const node::Utf8Value ciphers(args.GetIsolate(), args[0]);
  SSL_CTX_set_cipher_list(sc->ctx_.get(), *ciphers);
}


void SecureContext::SetECDHCurve(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  Environment* env = sc->env();

  if (args.Length() != 1)
    return THROW_ERR_MISSING_ARGS(env, "ECDH curve name argument is mandatory");

  THROW_AND_RETURN_IF_NOT_STRING(env, args[0], "ECDH curve name");

  node::Utf8Value curve(env->isolate(), args[0]);

  if (strcmp(*curve, "auto") == 0)
    return;

  if (!SSL_CTX_set1_curves_list(sc->ctx_.get(), *curve))
    return env->ThrowError("Failed to set ECDH curve");
}


void SecureContext::SetDHParam(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  Environment* env = sc->env();
  ClearErrorOnReturn clear_error_on_return;

  // Auto DH is not supported in openssl 1.0.1, so dhparam needs
  // to be specified explicitly
  if (args.Length() != 1)
    return THROW_ERR_MISSING_ARGS(env, "DH argument is mandatory");

  DHPointer dh;
  {
    BIOPointer bio(LoadBIO(env, args[0]));
    if (!bio)
      return;

    dh.reset(PEM_read_bio_DHparams(bio.get(), nullptr, nullptr, nullptr));
  }

  // Invalid dhparam is silently discarded and DHE is no longer used.
  if (!dh)
    return;

  const BIGNUM* p;
  DH_get0_pqg(dh.get(), &p, nullptr, nullptr);
  const int size = BN_num_bits(p);
  if (size < 1024) {
    return THROW_ERR_INVALID_ARG_VALUE(
        env, "DH parameter is less than 1024 bits");
  } else if (size < 2048) {
    args.GetReturnValue().Set(FIXED_ONE_BYTE_STRING(
        env->isolate(), "DH parameter is less than 2048 bits"));
  }

  SSL_CTX_set_options(sc->ctx_.get(), SSL_OP_SINGLE_DH_USE);
  int r = SSL_CTX_set_tmp_dh(sc->ctx_.get(), dh.get());

  if (!r)
    return env->ThrowTypeError("Error setting temp DH parameter");
}


void SecureContext::SetOptions(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  int64_t val;

  if (args.Length() != 1 ||
      !args[0]->IntegerValue(args.GetIsolate()->GetCurrentContext()).To(&val)) {
    return THROW_ERR_INVALID_ARG_TYPE(
        sc->env(), "Options must be an integer value");
  }

  SSL_CTX_set_options(sc->ctx_.get(),
                      static_cast<long>(val));  // NOLINT(runtime/int)
}


void SecureContext::SetSessionIdContext(
    const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  Environment* env = sc->env();

  if (args.Length() != 1) {
    return THROW_ERR_MISSING_ARGS(
        env, "Session ID context argument is mandatory");
  }

  THROW_AND_RETURN_IF_NOT_STRING(env, args[0], "Session ID context");

  const node::Utf8Value sessionIdContext(args.GetIsolate(), args[0]);
  const unsigned char* sid_ctx =
      reinterpret_cast<const unsigned char*>(*sessionIdContext);
  unsigned int sid_ctx_len = sessionIdContext.length();

  int r = SSL_CTX_set_session_id_context(sc->ctx_.get(), sid_ctx, sid_ctx_len);
  if (r == 1)
    return;

  BUF_MEM* mem;
  Local<String> message;

  BIOPointer bio(BIO_new(BIO_s_mem()));
  if (!bio) {
    message = FIXED_ONE_BYTE_STRING(args.GetIsolate(),
                                    "SSL_CTX_set_session_id_context error");
  } else {
    ERR_print_errors(bio.get());
    BIO_get_mem_ptr(bio.get(), &mem);
    message = OneByteString(args.GetIsolate(), mem->data, mem->length);
  }

  args.GetIsolate()->ThrowException(Exception::TypeError(message));
}


void SecureContext::SetSessionTimeout(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  if (args.Length() != 1 || !args[0]->IsInt32()) {
    return THROW_ERR_INVALID_ARG_TYPE(
        sc->env(), "Session timeout must be a 32-bit integer");
  }

  int32_t sessionTimeout = args[0].As<Int32>()->Value();
  SSL_CTX_set_timeout(sc->ctx_.get(), sessionTimeout);
}


void SecureContext::Close(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  sc->Reset();
}


// Takes .pfx or .p12 and password in string or buffer format
void SecureContext::LoadPKCS12(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  std::vector<char> pass;
  bool ret = false;

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  ClearErrorOnReturn clear_error_on_return;

  if (args.Length() < 1) {
    return THROW_ERR_MISSING_ARGS(env, "PFX certificate argument is mandatory");
  }

  BIOPointer in(LoadBIO(env, args[0]));
  if (!in)
    return env->ThrowError("Unable to load BIO");

  if (args.Length() >= 2) {
    THROW_AND_RETURN_IF_NOT_BUFFER(env, args[1], "Pass phrase");
    size_t passlen = Buffer::Length(args[1]);
    pass.resize(passlen + 1);
    memcpy(pass.data(), Buffer::Data(args[1]), passlen);
    pass[passlen] = '\0';
  }

  // Free previous certs
  sc->issuer_.reset();
  sc->cert_.reset();

  X509_STORE* cert_store = SSL_CTX_get_cert_store(sc->ctx_.get());

  DeleteFnPtr<PKCS12, PKCS12_free> p12;
  EVPKeyPointer pkey;
  X509Pointer cert;
  StackOfX509 extra_certs;

  PKCS12* p12_ptr = nullptr;
  EVP_PKEY* pkey_ptr = nullptr;
  X509* cert_ptr = nullptr;
  STACK_OF(X509)* extra_certs_ptr = nullptr;
  if (d2i_PKCS12_bio(in.get(), &p12_ptr) &&
      (p12.reset(p12_ptr), true) &&  // Move ownership to the smart pointer.
      PKCS12_parse(p12.get(), pass.data(),
                   &pkey_ptr,
                   &cert_ptr,
                   &extra_certs_ptr) &&
      (pkey.reset(pkey_ptr), cert.reset(cert_ptr),
       extra_certs.reset(extra_certs_ptr), true) &&  // Move ownership.
      SSL_CTX_use_certificate_chain(sc->ctx_.get(),
                                    std::move(cert),
                                    extra_certs.get(),
                                    &sc->cert_,
                                    &sc->issuer_) &&
      SSL_CTX_use_PrivateKey(sc->ctx_.get(), pkey.get())) {
    // Add CA certs too
    for (int i = 0; i < sk_X509_num(extra_certs.get()); i++) {
      X509* ca = sk_X509_value(extra_certs.get(), i);

      if (cert_store == root_cert_store) {
        cert_store = NewRootCertStore();
        SSL_CTX_set_cert_store(sc->ctx_.get(), cert_store);
      }
      X509_STORE_add_cert(cert_store, ca);
      SSL_CTX_add_client_CA(sc->ctx_.get(), ca);
    }
    ret = true;
  }

  if (!ret) {
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
    const char* str = ERR_reason_error_string(err);
    return env->ThrowError(str);
  }
}


#ifndef OPENSSL_NO_ENGINE
// Helper for the smart pointer.
void ENGINE_free_fn(ENGINE* engine) { ENGINE_free(engine); }

void SecureContext::SetClientCertEngine(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  MarkPopErrorOnReturn mark_pop_error_on_return;

  // SSL_CTX_set_client_cert_engine does not itself support multiple
  // calls by cleaning up before overwriting the client_cert_engine
  // internal context variable.
  // Instead of trying to fix up this problem we in turn also do not
  // support multiple calls to SetClientCertEngine.
  if (sc->client_cert_engine_provided_) {
    return env->ThrowError(
        "Multiple calls to SetClientCertEngine are not allowed");
  }

  const node::Utf8Value engine_id(env->isolate(), args[0]);
  char errmsg[1024];
  DeleteFnPtr<ENGINE, ENGINE_free_fn> engine(
      LoadEngineById(*engine_id, &errmsg));

  if (!engine)
    return env->ThrowError(errmsg);

  // Note that this takes another reference to `engine`.
  int r = SSL_CTX_set_client_cert_engine(sc->ctx_.get(), engine.get());
  if (r == 0)
    return ThrowCryptoError(env, ERR_get_error());
  sc->client_cert_engine_provided_ = true;
}
#endif  // !OPENSSL_NO_ENGINE


void SecureContext::GetTicketKeys(const FunctionCallbackInfo<Value>& args) {
#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_get_tlsext_ticket_keys)

  SecureContext* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  Local<Object> buff = Buffer::New(wrap->env(), 48).ToLocalChecked();
  memcpy(Buffer::Data(buff), wrap->ticket_key_name_, 16);
  memcpy(Buffer::Data(buff) + 16, wrap->ticket_key_hmac_, 16);
  memcpy(Buffer::Data(buff) + 32, wrap->ticket_key_aes_, 16);

  args.GetReturnValue().Set(buff);
#endif  // !def(OPENSSL_NO_TLSEXT) && def(SSL_CTX_get_tlsext_ticket_keys)
}


void SecureContext::SetTicketKeys(const FunctionCallbackInfo<Value>& args) {
#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_get_tlsext_ticket_keys)
  SecureContext* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  Environment* env = wrap->env();

  if (args.Length() < 1) {
    return THROW_ERR_MISSING_ARGS(env, "Ticket keys argument is mandatory");
  }

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "Ticket keys");

  if (Buffer::Length(args[0]) != 48) {
    return THROW_ERR_INVALID_ARG_VALUE(
        env, "Ticket keys length must be 48 bytes");
  }

  memcpy(wrap->ticket_key_name_, Buffer::Data(args[0]), 16);
  memcpy(wrap->ticket_key_hmac_, Buffer::Data(args[0]) + 16, 16);
  memcpy(wrap->ticket_key_aes_, Buffer::Data(args[0]) + 32, 16);

  args.GetReturnValue().Set(true);
#endif  // !def(OPENSSL_NO_TLSEXT) && def(SSL_CTX_get_tlsext_ticket_keys)
}


void SecureContext::SetFreeListLength(const FunctionCallbackInfo<Value>& args) {
}


// Currently, EnableTicketKeyCallback and TicketKeyCallback are only present for
// the regression test in test/parallel/test-https-resume-after-renew.js.
void SecureContext::EnableTicketKeyCallback(
    const FunctionCallbackInfo<Value>& args) {
  SecureContext* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  SSL_CTX_set_tlsext_ticket_key_cb(wrap->ctx_.get(), TicketKeyCallback);
}


int SecureContext::TicketKeyCallback(SSL* ssl,
                                     unsigned char* name,
                                     unsigned char* iv,
                                     EVP_CIPHER_CTX* ectx,
                                     HMAC_CTX* hctx,
                                     int enc) {
  static const int kTicketPartSize = 16;

  SecureContext* sc = static_cast<SecureContext*>(
      SSL_CTX_get_app_data(SSL_get_SSL_CTX(ssl)));

  Environment* env = sc->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Buffer::Copy(env,
                 reinterpret_cast<char*>(name),
                 kTicketPartSize).ToLocalChecked(),
    Buffer::Copy(env,
                 reinterpret_cast<char*>(iv),
                 kTicketPartSize).ToLocalChecked(),
    Boolean::New(env->isolate(), enc != 0)
  };

  Local<Value> ret = node::MakeCallback(env->isolate(),
                                        sc->object(),
                                        env->ticketkeycallback_string(),
                                        arraysize(argv),
                                        argv,
                                        {0, 0}).ToLocalChecked();
  Local<Array> arr = ret.As<Array>();

  int r =
      arr->Get(kTicketKeyReturnIndex)->Int32Value(env->context()).FromJust();
  if (r < 0)
    return r;

  Local<Value> hmac = arr->Get(kTicketKeyHMACIndex);
  Local<Value> aes = arr->Get(kTicketKeyAESIndex);
  if (Buffer::Length(aes) != kTicketPartSize)
    return -1;

  if (enc) {
    Local<Value> name_val = arr->Get(kTicketKeyNameIndex);
    Local<Value> iv_val = arr->Get(kTicketKeyIVIndex);

    if (Buffer::Length(name_val) != kTicketPartSize ||
        Buffer::Length(iv_val) != kTicketPartSize) {
      return -1;
    }

    memcpy(name, Buffer::Data(name_val), kTicketPartSize);
    memcpy(iv, Buffer::Data(iv_val), kTicketPartSize);
  }

  HMAC_Init_ex(hctx,
               Buffer::Data(hmac),
               Buffer::Length(hmac),
               EVP_sha256(),
               nullptr);

  const unsigned char* aes_key =
      reinterpret_cast<unsigned char*>(Buffer::Data(aes));
  if (enc) {
    EVP_EncryptInit_ex(ectx,
                       EVP_aes_128_cbc(),
                       nullptr,
                       aes_key,
                       iv);
  } else {
    EVP_DecryptInit_ex(ectx,
                       EVP_aes_128_cbc(),
                       nullptr,
                       aes_key,
                       iv);
  }

  return r;
}


int SecureContext::TicketCompatibilityCallback(SSL* ssl,
                                               unsigned char* name,
                                               unsigned char* iv,
                                               EVP_CIPHER_CTX* ectx,
                                               HMAC_CTX* hctx,
                                               int enc) {
  SecureContext* sc = static_cast<SecureContext*>(
      SSL_CTX_get_app_data(SSL_get_SSL_CTX(ssl)));

  if (enc) {
    memcpy(name, sc->ticket_key_name_, sizeof(sc->ticket_key_name_));
    if (RAND_bytes(iv, 16) <= 0 ||
        EVP_EncryptInit_ex(ectx, EVP_aes_128_cbc(), nullptr,
                           sc->ticket_key_aes_, iv) <= 0 ||
        HMAC_Init_ex(hctx, sc->ticket_key_hmac_, sizeof(sc->ticket_key_hmac_),
                     EVP_sha256(), nullptr) <= 0) {
      return -1;
    }
    return 1;
  }

  if (memcmp(name, sc->ticket_key_name_, sizeof(sc->ticket_key_name_)) != 0) {
    // The ticket key name does not match. Discard the ticket.
    return 0;
  }

  if (EVP_DecryptInit_ex(ectx, EVP_aes_128_cbc(), nullptr, sc->ticket_key_aes_,
                         iv) <= 0 ||
      HMAC_Init_ex(hctx, sc->ticket_key_hmac_, sizeof(sc->ticket_key_hmac_),
                   EVP_sha256(), nullptr) <= 0) {
    return -1;
  }
  return 1;
}


void SecureContext::CtxGetter(const FunctionCallbackInfo<Value>& info) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, info.This());
  Local<External> ext = External::New(info.GetIsolate(), sc->ctx_.get());
  info.GetReturnValue().Set(ext);
}


template <bool primary>
void SecureContext::GetCertificate(const FunctionCallbackInfo<Value>& args) {
  SecureContext* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  Environment* env = wrap->env();
  X509* cert;

  if (primary)
    cert = wrap->cert_.get();
  else
    cert = wrap->issuer_.get();
  if (cert == nullptr)
    return args.GetReturnValue().SetNull();

  int size = i2d_X509(cert, nullptr);
  Local<Object> buff = Buffer::New(env, size).ToLocalChecked();
  unsigned char* serialized = reinterpret_cast<unsigned char*>(
      Buffer::Data(buff));
  i2d_X509(cert, &serialized);

  args.GetReturnValue().Set(buff);
}


template <class Base>
void SSLWrap<Base>::AddMethods(Environment* env, Local<FunctionTemplate> t) {
  HandleScope scope(env->isolate());

  env->SetProtoMethodNoSideEffect(t, "getPeerCertificate", GetPeerCertificate);
  env->SetProtoMethodNoSideEffect(t, "getFinished", GetFinished);
  env->SetProtoMethodNoSideEffect(t, "getPeerFinished", GetPeerFinished);
  env->SetProtoMethodNoSideEffect(t, "getSession", GetSession);
  env->SetProtoMethod(t, "setSession", SetSession);
  env->SetProtoMethod(t, "loadSession", LoadSession);
  env->SetProtoMethodNoSideEffect(t, "isSessionReused", IsSessionReused);
  env->SetProtoMethodNoSideEffect(t, "verifyError", VerifyError);
  env->SetProtoMethodNoSideEffect(t, "getCurrentCipher", GetCurrentCipher);
  env->SetProtoMethod(t, "endParser", EndParser);
  env->SetProtoMethod(t, "certCbDone", CertCbDone);
  env->SetProtoMethod(t, "renegotiate", Renegotiate);
  env->SetProtoMethodNoSideEffect(t, "getTLSTicket", GetTLSTicket);
  env->SetProtoMethod(t, "newSessionDone", NewSessionDone);
  env->SetProtoMethod(t, "setOCSPResponse", SetOCSPResponse);
  env->SetProtoMethod(t, "requestOCSP", RequestOCSP);
  env->SetProtoMethodNoSideEffect(t, "getEphemeralKeyInfo",
                                  GetEphemeralKeyInfo);
  env->SetProtoMethodNoSideEffect(t, "getProtocol", GetProtocol);

#ifdef SSL_set_max_send_fragment
  env->SetProtoMethod(t, "setMaxSendFragment", SetMaxSendFragment);
#endif  // SSL_set_max_send_fragment

  env->SetProtoMethodNoSideEffect(t, "getALPNNegotiatedProtocol",
                                  GetALPNNegotiatedProto);
  env->SetProtoMethod(t, "setALPNProtocols", SetALPNProtocols);
}


template <class Base>
void SSLWrap<Base>::ConfigureSecureContext(SecureContext* sc) {
  // OCSP stapling
  SSL_CTX_set_tlsext_status_cb(sc->ctx_.get(), TLSExtStatusCallback);
  SSL_CTX_set_tlsext_status_arg(sc->ctx_.get(), nullptr);
}


template <class Base>
SSL_SESSION* SSLWrap<Base>::GetSessionCallback(SSL* s,
                                               const unsigned char* key,
                                               int len,
                                               int* copy) {
  Base* w = static_cast<Base*>(SSL_get_app_data(s));

  *copy = 0;
  return w->next_sess_.release();
}


template <class Base>
int SSLWrap<Base>::NewSessionCallback(SSL* s, SSL_SESSION* sess) {
  Base* w = static_cast<Base*>(SSL_get_app_data(s));
  Environment* env = w->ssl_env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  if (!w->session_callbacks_)
    return 0;

  // Check if session is small enough to be stored
  int size = i2d_SSL_SESSION(sess, nullptr);
  if (size > SecureContext::kMaxSessionSize)
    return 0;

  // Serialize session
  Local<Object> buff = Buffer::New(env, size).ToLocalChecked();
  unsigned char* serialized = reinterpret_cast<unsigned char*>(
      Buffer::Data(buff));
  memset(serialized, 0, size);
  i2d_SSL_SESSION(sess, &serialized);

  unsigned int session_id_length;
  const unsigned char* session_id = SSL_SESSION_get_id(sess,
                                                       &session_id_length);
  Local<Object> session = Buffer::Copy(
      env,
      reinterpret_cast<const char*>(session_id),
      session_id_length).ToLocalChecked();
  Local<Value> argv[] = { session, buff };
  w->awaiting_new_session_ = true;
  w->MakeCallback(env->onnewsession_string(), arraysize(argv), argv);

  return 0;
}


template <class Base>
void SSLWrap<Base>::OnClientHello(void* arg,
                                  const ClientHelloParser::ClientHello& hello) {
  Base* w = static_cast<Base*>(arg);
  Environment* env = w->ssl_env();
  HandleScope handle_scope(env->isolate());
  Local<Context> context = env->context();
  Context::Scope context_scope(context);

  Local<Object> hello_obj = Object::New(env->isolate());
  Local<Object> buff = Buffer::Copy(
      env,
      reinterpret_cast<const char*>(hello.session_id()),
      hello.session_size()).ToLocalChecked();
  hello_obj->Set(context, env->session_id_string(), buff).FromJust();
  if (hello.servername() == nullptr) {
    hello_obj->Set(context,
                   env->servername_string(),
                   String::Empty(env->isolate())).FromJust();
  } else {
    Local<String> servername = OneByteString(env->isolate(),
                                             hello.servername(),
                                             hello.servername_size());
    hello_obj->Set(context, env->servername_string(), servername).FromJust();
  }
  hello_obj->Set(context,
                 env->tls_ticket_string(),
                 Boolean::New(env->isolate(), hello.has_ticket())).FromJust();
  hello_obj->Set(context,
                 env->ocsp_request_string(),
                 Boolean::New(env->isolate(), hello.ocsp_request())).FromJust();

  Local<Value> argv[] = { hello_obj };
  w->MakeCallback(env->onclienthello_string(), arraysize(argv), argv);
}


static bool SafeX509ExtPrint(BIO* out, X509_EXTENSION* ext) {
  const X509V3_EXT_METHOD* method = X509V3_EXT_get(ext);

  if (method != X509V3_EXT_get_nid(NID_subject_alt_name))
    return false;

  GENERAL_NAMES* names = static_cast<GENERAL_NAMES*>(X509V3_EXT_d2i(ext));
  if (names == nullptr)
    return false;

  for (int i = 0; i < sk_GENERAL_NAME_num(names); i++) {
    GENERAL_NAME* gen = sk_GENERAL_NAME_value(names, i);

    if (i != 0)
      BIO_write(out, ", ", 2);

    if (gen->type == GEN_DNS) {
      ASN1_IA5STRING* name = gen->d.dNSName;

      BIO_write(out, "DNS:", 4);
      BIO_write(out, name->data, name->length);
    } else {
      STACK_OF(CONF_VALUE)* nval = i2v_GENERAL_NAME(
          const_cast<X509V3_EXT_METHOD*>(method), gen, nullptr);
      if (nval == nullptr)
        return false;
      X509V3_EXT_val_prn(out, nval, 0, 0);
      sk_CONF_VALUE_pop_free(nval, X509V3_conf_free);
    }
  }
  sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);

  return true;
}


static void AddFingerprintDigest(const unsigned char* md,
                                 unsigned int md_size,
                                 char (*fingerprint)[3 * EVP_MAX_MD_SIZE + 1]) {
  unsigned int i;
  const char hex[] = "0123456789ABCDEF";

  for (i = 0; i < md_size; i++) {
    (*fingerprint)[3*i] = hex[(md[i] & 0xf0) >> 4];
    (*fingerprint)[(3*i)+1] = hex[(md[i] & 0x0f)];
    (*fingerprint)[(3*i)+2] = ':';
  }

  if (md_size > 0) {
    (*fingerprint)[(3*(md_size-1))+2] = '\0';
  } else {
    (*fingerprint)[0] = '\0';
  }
}

static Local<Object> X509ToObject(Environment* env, X509* cert) {
  EscapableHandleScope scope(env->isolate());
  Local<Context> context = env->context();
  Local<Object> info = Object::New(env->isolate());

  BIOPointer bio(BIO_new(BIO_s_mem()));
  BUF_MEM* mem;
  if (X509_NAME_print_ex(bio.get(),
                         X509_get_subject_name(cert),
                         0,
                         X509_NAME_FLAGS) > 0) {
    BIO_get_mem_ptr(bio.get(), &mem);
    info->Set(context, env->subject_string(),
              String::NewFromUtf8(env->isolate(), mem->data,
                                  NewStringType::kNormal,
                                  mem->length).ToLocalChecked()).FromJust();
  }
  USE(BIO_reset(bio.get()));

  X509_NAME* issuer_name = X509_get_issuer_name(cert);
  if (X509_NAME_print_ex(bio.get(), issuer_name, 0, X509_NAME_FLAGS) > 0) {
    BIO_get_mem_ptr(bio.get(), &mem);
    info->Set(context, env->issuer_string(),
              String::NewFromUtf8(env->isolate(), mem->data,
                                  NewStringType::kNormal,
                                  mem->length).ToLocalChecked()).FromJust();
  }
  USE(BIO_reset(bio.get()));

  int nids[] = { NID_subject_alt_name, NID_info_access };
  Local<String> keys[] = { env->subjectaltname_string(),
                           env->infoaccess_string() };
  CHECK_EQ(arraysize(nids), arraysize(keys));
  for (size_t i = 0; i < arraysize(nids); i++) {
    int index = X509_get_ext_by_NID(cert, nids[i], -1);
    if (index < 0)
      continue;

    X509_EXTENSION* ext = X509_get_ext(cert, index);
    CHECK_NOT_NULL(ext);

    if (!SafeX509ExtPrint(bio.get(), ext)) {
      CHECK_EQ(1, X509V3_EXT_print(bio.get(), ext, 0, 0));
    }

    BIO_get_mem_ptr(bio.get(), &mem);
    info->Set(context, keys[i],
              String::NewFromUtf8(env->isolate(), mem->data,
                                  NewStringType::kNormal,
                                  mem->length).ToLocalChecked()).FromJust();

    USE(BIO_reset(bio.get()));
  }

  EVPKeyPointer pkey(X509_get_pubkey(cert));
  RSAPointer rsa;
  if (pkey)
    rsa.reset(EVP_PKEY_get1_RSA(pkey.get()));

  if (rsa) {
    const BIGNUM* n;
    const BIGNUM* e;
    RSA_get0_key(rsa.get(), &n, &e, nullptr);
    BN_print(bio.get(), n);
    BIO_get_mem_ptr(bio.get(), &mem);
    info->Set(context, env->modulus_string(),
              String::NewFromUtf8(env->isolate(), mem->data,
                                  NewStringType::kNormal,
                                  mem->length).ToLocalChecked()).FromJust();
    USE(BIO_reset(bio.get()));

    uint64_t exponent_word = static_cast<uint64_t>(BN_get_word(e));
    uint32_t lo = static_cast<uint32_t>(exponent_word);
    uint32_t hi = static_cast<uint32_t>(exponent_word >> 32);
    if (hi == 0) {
      BIO_printf(bio.get(), "0x%x", lo);
    } else {
      BIO_printf(bio.get(), "0x%x%08x", hi, lo);
    }
    BIO_get_mem_ptr(bio.get(), &mem);
    info->Set(context, env->exponent_string(),
              String::NewFromUtf8(env->isolate(), mem->data,
                                  NewStringType::kNormal,
                                  mem->length).ToLocalChecked()).FromJust();
    USE(BIO_reset(bio.get()));

    int size = i2d_RSA_PUBKEY(rsa.get(), nullptr);
    CHECK_GE(size, 0);
    Local<Object> pubbuff = Buffer::New(env, size).ToLocalChecked();
    unsigned char* pubserialized =
        reinterpret_cast<unsigned char*>(Buffer::Data(pubbuff));
    i2d_RSA_PUBKEY(rsa.get(), &pubserialized);
    info->Set(env->pubkey_string(), pubbuff);
  }

  pkey.reset();
  rsa.reset();

  ASN1_TIME_print(bio.get(), X509_get_notBefore(cert));
  BIO_get_mem_ptr(bio.get(), &mem);
  info->Set(context, env->valid_from_string(),
            String::NewFromUtf8(env->isolate(), mem->data,
                                NewStringType::kNormal,
                                mem->length).ToLocalChecked()).FromJust();
  USE(BIO_reset(bio.get()));

  ASN1_TIME_print(bio.get(), X509_get_notAfter(cert));
  BIO_get_mem_ptr(bio.get(), &mem);
  info->Set(context, env->valid_to_string(),
            String::NewFromUtf8(env->isolate(), mem->data,
                                NewStringType::kNormal,
                                mem->length).ToLocalChecked()).FromJust();
  bio.reset();

  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int md_size;
  char fingerprint[EVP_MAX_MD_SIZE * 3 + 1];
  if (X509_digest(cert, EVP_sha1(), md, &md_size)) {
      AddFingerprintDigest(md, md_size, &fingerprint);
      info->Set(context, env->fingerprint_string(),
                OneByteString(env->isolate(), fingerprint)).FromJust();
  }
  if (X509_digest(cert, EVP_sha256(), md, &md_size)) {
      AddFingerprintDigest(md, md_size, &fingerprint);
      info->Set(context, env->fingerprint256_string(),
                OneByteString(env->isolate(), fingerprint)).FromJust();
  }

  StackOfASN1 eku(static_cast<STACK_OF(ASN1_OBJECT)*>(
      X509_get_ext_d2i(cert, NID_ext_key_usage, nullptr, nullptr)));
  if (eku) {
    Local<Array> ext_key_usage = Array::New(env->isolate());
    char buf[256];

    int j = 0;
    for (int i = 0; i < sk_ASN1_OBJECT_num(eku.get()); i++) {
      if (OBJ_obj2txt(buf,
                      sizeof(buf),
                      sk_ASN1_OBJECT_value(eku.get(), i), 1) >= 0) {
        ext_key_usage->Set(context,
                           j++,
                           OneByteString(env->isolate(), buf)).FromJust();
      }
    }

    eku.reset();
    info->Set(context, env->ext_key_usage_string(), ext_key_usage).FromJust();
  }

  if (ASN1_INTEGER* serial_number = X509_get_serialNumber(cert)) {
    BignumPointer bn(ASN1_INTEGER_to_BN(serial_number, nullptr));
    if (bn) {
      OpenSSLBuffer buf(BN_bn2hex(bn.get()));
      if (buf) {
        info->Set(context, env->serial_number_string(),
                  OneByteString(env->isolate(), buf.get())).FromJust();
      }
    }
  }

  // Raw DER certificate
  int size = i2d_X509(cert, nullptr);
  Local<Object> buff = Buffer::New(env, size).ToLocalChecked();
  unsigned char* serialized = reinterpret_cast<unsigned char*>(
      Buffer::Data(buff));
  i2d_X509(cert, &serialized);
  info->Set(context, env->raw_string(), buff).FromJust();

  return scope.Escape(info);
}


static Local<Object> AddIssuerChainToObject(X509Pointer* cert,
                                            Local<Object> object,
                                            StackOfX509&& peer_certs,
                                            Environment* const env) {
  Local<Context> context = env->isolate()->GetCurrentContext();
  cert->reset(sk_X509_delete(peer_certs.get(), 0));
  for (;;) {
    int i;
    for (i = 0; i < sk_X509_num(peer_certs.get()); i++) {
      X509* ca = sk_X509_value(peer_certs.get(), i);
      if (X509_check_issued(ca, cert->get()) != X509_V_OK)
        continue;

      Local<Object> ca_info = X509ToObject(env, ca);
      object->Set(context, env->issuercert_string(), ca_info).FromJust();
      object = ca_info;

      // NOTE: Intentionally freeing cert that is not used anymore.
      // Delete cert and continue aggregating issuers.
      cert->reset(sk_X509_delete(peer_certs.get(), i));
      break;
    }

    // Issuer not found, break out of the loop.
    if (i == sk_X509_num(peer_certs.get()))
      break;
  }
  return object;
}


static StackOfX509 CloneSSLCerts(X509Pointer&& cert,
                                 const STACK_OF(X509)* const ssl_certs) {
  StackOfX509 peer_certs(sk_X509_new(nullptr));
  if (cert)
    sk_X509_push(peer_certs.get(), cert.release());
  for (int i = 0; i < sk_X509_num(ssl_certs); i++) {
    X509Pointer cert(X509_dup(sk_X509_value(ssl_certs, i)));
    if (!cert || !sk_X509_push(peer_certs.get(), cert.get()))
      return StackOfX509();
    // `cert` is now managed by the stack.
    cert.release();
  }
  return peer_certs;
}


static Local<Object> GetLastIssuedCert(X509Pointer* cert,
                                       const SSLPointer& ssl,
                                       Local<Object> issuer_chain,
                                       Environment* const env) {
  Local<Context> context = env->isolate()->GetCurrentContext();
  while (X509_check_issued(cert->get(), cert->get()) != X509_V_OK) {
    X509* ca;
    if (SSL_CTX_get_issuer(SSL_get_SSL_CTX(ssl.get()), cert->get(), &ca) <= 0)
      break;

    Local<Object> ca_info = X509ToObject(env, ca);
    issuer_chain->Set(context, env->issuercert_string(), ca_info).FromJust();
    issuer_chain = ca_info;

    // Delete previous cert and continue aggregating issuers.
    cert->reset(ca);
  }
  return issuer_chain;
}


template <class Base>
void SSLWrap<Base>::GetPeerCertificate(
    const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->ssl_env();

  ClearErrorOnReturn clear_error_on_return;

  Local<Object> result;
  // Used to build the issuer certificate chain.
  Local<Object> issuer_chain;

  // NOTE: This is because of the odd OpenSSL behavior. On client `cert_chain`
  // contains the `peer_certificate`, but on server it doesn't.
  X509Pointer cert(
      w->is_server() ? SSL_get_peer_certificate(w->ssl_.get()) : nullptr);
  STACK_OF(X509)* ssl_certs = SSL_get_peer_cert_chain(w->ssl_.get());
  if (!cert && (ssl_certs == nullptr || sk_X509_num(ssl_certs) == 0))
    goto done;

  // Short result requested.
  if (args.Length() < 1 || !args[0]->IsTrue()) {
    result = X509ToObject(env, cert ? cert.get() : sk_X509_value(ssl_certs, 0));
    goto done;
  }

  if (auto peer_certs = CloneSSLCerts(std::move(cert), ssl_certs)) {
    // First and main certificate.
    X509Pointer cert(sk_X509_value(peer_certs.get(), 0));
    CHECK(cert);
    result = X509ToObject(env, cert.release());

    issuer_chain =
        AddIssuerChainToObject(&cert, result, std::move(peer_certs), env);
    issuer_chain = GetLastIssuedCert(&cert, w->ssl_, issuer_chain, env);
    // Last certificate should be self-signed.
    if (X509_check_issued(cert.get(), cert.get()) == X509_V_OK)
      issuer_chain->Set(env->context(),
                        env->issuercert_string(),
                        issuer_chain).FromJust();
  }

 done:
  if (result.IsEmpty())
    result = Object::New(env->isolate());
  args.GetReturnValue().Set(result);
}


template <class Base>
void SSLWrap<Base>::GetFinished(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Base* w;
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

  char* buf = Malloc(len);
  CHECK_EQ(len, SSL_get_finished(w->ssl_.get(), buf, len));
  args.GetReturnValue().Set(Buffer::New(env, buf, len).ToLocalChecked());
}


template <class Base>
void SSLWrap<Base>::GetPeerFinished(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Base* w;
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

  char* buf = Malloc(len);
  CHECK_EQ(len, SSL_get_peer_finished(w->ssl_.get(), buf, len));
  args.GetReturnValue().Set(Buffer::New(env, buf, len).ToLocalChecked());
}


template <class Base>
void SSLWrap<Base>::GetSession(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  SSL_SESSION* sess = SSL_get_session(w->ssl_.get());
  if (sess == nullptr)
    return;

  int slen = i2d_SSL_SESSION(sess, nullptr);
  CHECK_GT(slen, 0);

  char* sbuf = Malloc(slen);
  unsigned char* p = reinterpret_cast<unsigned char*>(sbuf);
  i2d_SSL_SESSION(sess, &p);
  args.GetReturnValue().Set(Buffer::New(env, sbuf, slen).ToLocalChecked());
}


template <class Base>
void SSLWrap<Base>::SetSession(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  if (args.Length() < 1) {
    return THROW_ERR_MISSING_ARGS(env, "Session argument is mandatory");
  }

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "Session");
  size_t slen = Buffer::Length(args[0]);
  std::vector<char> sbuf(slen);
  if (char* p = Buffer::Data(args[0]))
    sbuf.assign(p, p + slen);

  const unsigned char* p = reinterpret_cast<const unsigned char*>(sbuf.data());
  SSLSessionPointer sess(d2i_SSL_SESSION(nullptr, &p, slen));

  if (sess == nullptr)
    return;

  int r = SSL_set_session(w->ssl_.get(), sess.get());

  if (!r)
    return env->ThrowError("SSL_set_session error");
}


template <class Base>
void SSLWrap<Base>::LoadSession(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  if (args.Length() >= 1 && Buffer::HasInstance(args[0])) {
    ssize_t slen = Buffer::Length(args[0]);
    char* sbuf = Buffer::Data(args[0]);

    const unsigned char* p = reinterpret_cast<unsigned char*>(sbuf);
    SSL_SESSION* sess = d2i_SSL_SESSION(nullptr, &p, slen);

    // Setup next session and move hello to the BIO buffer
    w->next_sess_.reset(sess);
  }
}


template <class Base>
void SSLWrap<Base>::IsSessionReused(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  bool yes = SSL_session_reused(w->ssl_.get());
  args.GetReturnValue().Set(yes);
}


template <class Base>
void SSLWrap<Base>::EndParser(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  w->hello_parser_.End();
}


template <class Base>
void SSLWrap<Base>::Renegotiate(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  ClearErrorOnReturn clear_error_on_return;

  // XXX(sam) Return/throw an error, don't discard the SSL error reason.
  bool yes = SSL_renegotiate(w->ssl_.get()) == 1;
  args.GetReturnValue().Set(yes);
}


template <class Base>
void SSLWrap<Base>::GetTLSTicket(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->ssl_env();

  SSL_SESSION* sess = SSL_get_session(w->ssl_.get());
  if (sess == nullptr)
    return;

  const unsigned char* ticket;
  size_t length;
  SSL_SESSION_get0_ticket(sess, &ticket, &length);

  if (ticket == nullptr)
    return;

  Local<Object> buff = Buffer::Copy(
      env, reinterpret_cast<const char*>(ticket), length).ToLocalChecked();

  args.GetReturnValue().Set(buff);
}


template <class Base>
void SSLWrap<Base>::NewSessionDone(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  w->awaiting_new_session_ = false;
  w->NewSessionDoneCb();
}


template <class Base>
void SSLWrap<Base>::SetOCSPResponse(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();

  if (args.Length() < 1)
    return THROW_ERR_MISSING_ARGS(env, "OCSP response argument is mandatory");

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "OCSP response");

  w->ocsp_response_.Reset(args.GetIsolate(), args[0].As<Object>());
}


template <class Base>
void SSLWrap<Base>::RequestOCSP(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  SSL_set_tlsext_status_type(w->ssl_.get(), TLSEXT_STATUSTYPE_ocsp);
}


template <class Base>
void SSLWrap<Base>::GetEphemeralKeyInfo(
    const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();

  CHECK(w->ssl_);

  // tmp key is available on only client
  if (w->is_server())
    return args.GetReturnValue().SetNull();

  Local<Object> info = Object::New(env->isolate());

  EVP_PKEY* key;

  if (SSL_get_server_tmp_key(w->ssl_.get(), &key)) {
    int kid = EVP_PKEY_id(key);
    switch (kid) {
      case EVP_PKEY_DH:
        info->Set(context, env->type_string(),
                  FIXED_ONE_BYTE_STRING(env->isolate(), "DH")).FromJust();
        info->Set(context, env->size_string(),
                  Integer::New(env->isolate(), EVP_PKEY_bits(key))).FromJust();
        break;
      case EVP_PKEY_EC:
      // TODO(shigeki) Change this to EVP_PKEY_X25519 and add EVP_PKEY_X448
      // after upgrading to 1.1.1.
      case NID_X25519:
        {
          const char* curve_name;
          if (kid == EVP_PKEY_EC) {
            EC_KEY* ec = EVP_PKEY_get1_EC_KEY(key);
            int nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec));
            curve_name = OBJ_nid2sn(nid);
            EC_KEY_free(ec);
          } else {
            curve_name = OBJ_nid2sn(kid);
          }
          info->Set(context, env->type_string(),
                    FIXED_ONE_BYTE_STRING(env->isolate(), "ECDH")).FromJust();
          info->Set(context, env->name_string(),
                    OneByteString(args.GetIsolate(),
                                  curve_name)).FromJust();
          info->Set(context, env->size_string(),
                    Integer::New(env->isolate(),
                                 EVP_PKEY_bits(key))).FromJust();
        }
        break;
    }
    EVP_PKEY_free(key);
  }

  return args.GetReturnValue().Set(info);
}


#ifdef SSL_set_max_send_fragment
template <class Base>
void SSLWrap<Base>::SetMaxSendFragment(
    const FunctionCallbackInfo<Value>& args) {
  CHECK(args.Length() >= 1 && args[0]->IsNumber());

  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  int rv = SSL_set_max_send_fragment(
      w->ssl_.get(),
      args[0]->Int32Value(w->ssl_env()->context()).FromJust());
  args.GetReturnValue().Set(rv);
}
#endif  // SSL_set_max_send_fragment


template <class Base>
void SSLWrap<Base>::VerifyError(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  // XXX(bnoordhuis) The UNABLE_TO_GET_ISSUER_CERT error when there is no
  // peer certificate is questionable but it's compatible with what was
  // here before.
  long x509_verify_error =  // NOLINT(runtime/int)
      X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT;
  if (X509* peer_cert = SSL_get_peer_certificate(w->ssl_.get())) {
    X509_free(peer_cert);
    x509_verify_error = SSL_get_verify_result(w->ssl_.get());
  }

  if (x509_verify_error == X509_V_OK)
    return args.GetReturnValue().SetNull();

  const char* reason = X509_verify_cert_error_string(x509_verify_error);
  const char* code = reason;
#define CASE_X509_ERR(CODE) case X509_V_ERR_##CODE: code = #CODE; break;
  switch (x509_verify_error) {
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
  }
#undef CASE_X509_ERR

  Isolate* isolate = args.GetIsolate();
  Local<String> reason_string = OneByteString(isolate, reason);
  Local<Value> exception_value = Exception::Error(reason_string);
  Local<Object> exception_object =
    exception_value->ToObject(isolate->GetCurrentContext()).ToLocalChecked();
  exception_object->Set(w->env()->context(), w->env()->code_string(),
                        OneByteString(isolate, code)).FromJust();
  args.GetReturnValue().Set(exception_object);
}


template <class Base>
void SSLWrap<Base>::GetCurrentCipher(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->ssl_env();
  Local<Context> context = env->context();

  const SSL_CIPHER* c = SSL_get_current_cipher(w->ssl_.get());
  if (c == nullptr)
    return;

  Local<Object> info = Object::New(env->isolate());
  const char* cipher_name = SSL_CIPHER_get_name(c);
  info->Set(context, env->name_string(),
            OneByteString(args.GetIsolate(), cipher_name)).FromJust();
  info->Set(context, env->version_string(),
            OneByteString(args.GetIsolate(), "TLSv1/SSLv3")).FromJust();
  args.GetReturnValue().Set(info);
}


template <class Base>
void SSLWrap<Base>::GetProtocol(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  const char* tls_version = SSL_get_version(w->ssl_.get());
  args.GetReturnValue().Set(OneByteString(args.GetIsolate(), tls_version));
}


template <class Base>
int SSLWrap<Base>::SelectALPNCallback(SSL* s,
                                      const unsigned char** out,
                                      unsigned char* outlen,
                                      const unsigned char* in,
                                      unsigned int inlen,
                                      void* arg) {
  Base* w = static_cast<Base*>(SSL_get_app_data(s));
  Environment* env = w->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> alpn_buffer =
      w->object()->GetPrivate(
          env->context(),
          env->alpn_buffer_private_symbol()).ToLocalChecked();
  CHECK(Buffer::HasInstance(alpn_buffer));
  const unsigned char* alpn_protos =
      reinterpret_cast<const unsigned char*>(Buffer::Data(alpn_buffer));
  unsigned alpn_protos_len = Buffer::Length(alpn_buffer);
  int status = SSL_select_next_proto(const_cast<unsigned char**>(out), outlen,
                                     alpn_protos, alpn_protos_len, in, inlen);
  // According to 3.2. Protocol Selection of RFC7301, fatal
  // no_application_protocol alert shall be sent but OpenSSL 1.0.2 does not
  // support it yet. See
  // https://rt.openssl.org/Ticket/Display.html?id=3463&user=guest&pass=guest
  return status == OPENSSL_NPN_NEGOTIATED ? SSL_TLSEXT_ERR_OK
                                          : SSL_TLSEXT_ERR_NOACK;
}


template <class Base>
void SSLWrap<Base>::GetALPNNegotiatedProto(
    const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());

  const unsigned char* alpn_proto;
  unsigned int alpn_proto_len;

  SSL_get0_alpn_selected(w->ssl_.get(), &alpn_proto, &alpn_proto_len);

  if (!alpn_proto)
    return args.GetReturnValue().Set(false);

  args.GetReturnValue().Set(
      OneByteString(args.GetIsolate(), alpn_proto, alpn_proto_len));
}


template <class Base>
void SSLWrap<Base>::SetALPNProtocols(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();
  if (args.Length() < 1 || !Buffer::HasInstance(args[0]))
    return env->ThrowTypeError("Must give a Buffer as first argument");

  if (w->is_client()) {
    const unsigned char* alpn_protos =
        reinterpret_cast<const unsigned char*>(Buffer::Data(args[0]));
    unsigned alpn_protos_len = Buffer::Length(args[0]);
    int r = SSL_set_alpn_protos(w->ssl_.get(), alpn_protos, alpn_protos_len);
    CHECK_EQ(r, 0);
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


template <class Base>
int SSLWrap<Base>::TLSExtStatusCallback(SSL* s, void* arg) {
  Base* w = static_cast<Base*>(SSL_get_app_data(s));
  Environment* env = w->env();
  HandleScope handle_scope(env->isolate());

  if (w->is_client()) {
    // Incoming response
    const unsigned char* resp;
    int len = SSL_get_tlsext_status_ocsp_resp(s, &resp);
    Local<Value> arg;
    if (resp == nullptr) {
      arg = Null(env->isolate());
    } else {
      arg =
          Buffer::Copy(env, reinterpret_cast<const char*>(resp), len)
          .ToLocalChecked();
    }

    w->MakeCallback(env->onocspresponse_string(), 1, &arg);

    // Somehow, client is expecting different return value here
    return 1;
  } else {
    // Outgoing response
    if (w->ocsp_response_.IsEmpty())
      return SSL_TLSEXT_ERR_NOACK;

    Local<Object> obj = PersistentToLocal(env->isolate(), w->ocsp_response_);
    char* resp = Buffer::Data(obj);
    size_t len = Buffer::Length(obj);

    // OpenSSL takes control of the pointer after accepting it
    char* data = node::Malloc(len);
    memcpy(data, resp, len);

    if (!SSL_set_tlsext_status_ocsp_resp(s, data, len))
      free(data);
    w->ocsp_response_.Reset();

    return SSL_TLSEXT_ERR_OK;
  }
}


template <class Base>
void SSLWrap<Base>::WaitForCertCb(CertCb cb, void* arg) {
  cert_cb_ = cb;
  cert_cb_arg_ = arg;
}


template <class Base>
int SSLWrap<Base>::SSLCertCallback(SSL* s, void* arg) {
  Base* w = static_cast<Base*>(SSL_get_app_data(s));

  if (!w->is_server())
    return 1;

  if (!w->is_waiting_cert_cb())
    return 1;

  if (w->cert_cb_running_)
    return -1;

  Environment* env = w->env();
  Local<Context> context = env->context();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(context);
  w->cert_cb_running_ = true;

  Local<Object> info = Object::New(env->isolate());

  const char* servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);
  if (servername == nullptr) {
    info->Set(context,
              env->servername_string(),
              String::Empty(env->isolate())).FromJust();
  } else {
    Local<String> str = OneByteString(env->isolate(), servername,
                                      strlen(servername));
    info->Set(context, env->servername_string(), str).FromJust();
  }

  const bool ocsp = (SSL_get_tlsext_status_type(s) == TLSEXT_STATUSTYPE_ocsp);
  info->Set(context, env->ocsp_request_string(),
            Boolean::New(env->isolate(), ocsp)).FromJust();

  Local<Value> argv[] = { info };
  w->MakeCallback(env->oncertcb_string(), arraysize(argv), argv);

  if (!w->cert_cb_running_)
    return 1;

  // Performing async action, wait...
  return -1;
}


template <class Base>
void SSLWrap<Base>::CertCbDone(const FunctionCallbackInfo<Value>& args) {
  Base* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.Holder());
  Environment* env = w->env();

  CHECK(w->is_waiting_cert_cb() && w->cert_cb_running_);

  Local<Object> object = w->object();
  Local<Value> ctx = object->Get(env->sni_context_string());
  Local<FunctionTemplate> cons = env->secure_context_constructor_template();

  // Not an object, probably undefined or null
  if (!ctx->IsObject())
    goto fire_cb;

  if (cons->HasInstance(ctx)) {
    SecureContext* sc;
    ASSIGN_OR_RETURN_UNWRAP(&sc, ctx.As<Object>());
    w->sni_context_.Reset(env->isolate(), ctx);

    int rv;

    // NOTE: reference count is not increased by this API methods
    X509* x509 = SSL_CTX_get0_certificate(sc->ctx_.get());
    EVP_PKEY* pkey = SSL_CTX_get0_privatekey(sc->ctx_.get());
    STACK_OF(X509)* chain;

    rv = SSL_CTX_get0_chain_certs(sc->ctx_.get(), &chain);
    if (rv)
      rv = SSL_use_certificate(w->ssl_.get(), x509);
    if (rv)
      rv = SSL_use_PrivateKey(w->ssl_.get(), pkey);
    if (rv && chain != nullptr)
      rv = SSL_set1_chain(w->ssl_.get(), chain);
    if (rv)
      rv = w->SetCACerts(sc);
    if (!rv) {
      unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
      if (!err)
        return env->ThrowError("CertCbDone");
      return ThrowCryptoError(env, err);
    }
  } else {
    // Failure: incorrect SNI context object
    Local<Value> err = Exception::TypeError(env->sni_context_err_string());
    w->MakeCallback(env->onerror_string(), 1, &err);
    return;
  }

 fire_cb:
  CertCb cb;
  void* arg;

  cb = w->cert_cb_;
  arg = w->cert_cb_arg_;

  w->cert_cb_running_ = false;
  w->cert_cb_ = nullptr;
  w->cert_cb_arg_ = nullptr;

  cb(arg);
}


template <class Base>
void SSLWrap<Base>::DestroySSL() {
  if (!ssl_)
    return;

  env_->isolate()->AdjustAmountOfExternalAllocatedMemory(-kExternalSize);
  ssl_.reset();
}


template <class Base>
void SSLWrap<Base>::SetSNIContext(SecureContext* sc) {
  ConfigureSecureContext(sc);
  CHECK_EQ(SSL_set_SSL_CTX(ssl_.get(), sc->ctx_.get()), sc->ctx_.get());

  SetCACerts(sc);
}


template <class Base>
int SSLWrap<Base>::SetCACerts(SecureContext* sc) {
  int err = SSL_set1_verify_cert_store(ssl_.get(),
                                       SSL_CTX_get_cert_store(sc->ctx_.get()));
  if (err != 1)
    return err;

  STACK_OF(X509_NAME)* list = SSL_dup_CA_list(
      SSL_CTX_get_client_CA_list(sc->ctx_.get()));

  // NOTE: `SSL_set_client_CA_list` takes the ownership of `list`
  SSL_set_client_CA_list(ssl_.get(), list);
  return 1;
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
  // imperative that the user use Connection::VerifyError after the
  // 'secure' callback has been made.
  return 1;
}

static bool IsSupportedAuthenticatedMode(int mode) {
  return mode == EVP_CIPH_CCM_MODE ||
         mode == EVP_CIPH_GCM_MODE ||
         IS_OCB_MODE(mode);
}

void CipherBase::Initialize(Environment* env, Local<Object> target) {
  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(t, "init", Init);
  env->SetProtoMethod(t, "initiv", InitIv);
  env->SetProtoMethod(t, "update", Update);
  env->SetProtoMethod(t, "final", Final);
  env->SetProtoMethod(t, "setAutoPadding", SetAutoPadding);
  env->SetProtoMethodNoSideEffect(t, "getAuthTag", GetAuthTag);
  env->SetProtoMethod(t, "setAuthTag", SetAuthTag);
  env->SetProtoMethod(t, "setAAD", SetAAD);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "CipherBase"),
              t->GetFunction(env->context()).ToLocalChecked());
}


void CipherBase::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  CipherKind kind = args[0]->IsTrue() ? kCipher : kDecipher;
  Environment* env = Environment::GetCurrent(args);
  new CipherBase(env, args.This(), kind);
}

void CipherBase::CommonInit(const char* cipher_type,
                            const EVP_CIPHER* cipher,
                            const unsigned char* key,
                            int key_len,
                            const unsigned char* iv,
                            int iv_len,
                            unsigned int auth_tag_len) {
  CHECK(!ctx_);
  ctx_.reset(EVP_CIPHER_CTX_new());

  const int mode = EVP_CIPHER_mode(cipher);
  if (mode == EVP_CIPH_WRAP_MODE)
    EVP_CIPHER_CTX_set_flags(ctx_.get(), EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

  const bool encrypt = (kind_ == kCipher);
  if (1 != EVP_CipherInit_ex(ctx_.get(), cipher, nullptr,
                             nullptr, nullptr, encrypt)) {
    return ThrowCryptoError(env(), ERR_get_error(),
                            "Failed to initialize cipher");
  }

  if (IsSupportedAuthenticatedMode(mode)) {
    CHECK_GE(iv_len, 0);
    if (!InitAuthenticated(cipher_type, iv_len, auth_tag_len))
      return;
  }

  if (!EVP_CIPHER_CTX_set_key_length(ctx_.get(), key_len)) {
    ctx_.reset();
    return env()->ThrowError("Invalid key length");
  }

  if (1 != EVP_CipherInit_ex(ctx_.get(), nullptr, nullptr, key, iv, encrypt)) {
    return ThrowCryptoError(env(), ERR_get_error(),
                            "Failed to initialize cipher");
  }
}

void CipherBase::Init(const char* cipher_type,
                      const char* key_buf,
                      int key_buf_len,
                      unsigned int auth_tag_len) {
  HandleScope scope(env()->isolate());
  MarkPopErrorOnReturn mark_pop_error_on_return;

#ifdef NODE_FIPS_MODE
  if (FIPS_mode()) {
    return env()->ThrowError(
        "crypto.createCipher() is not supported in FIPS mode.");
  }
#endif  // NODE_FIPS_MODE

  const EVP_CIPHER* const cipher = EVP_get_cipherbyname(cipher_type);
  if (cipher == nullptr)
    return env()->ThrowError("Unknown cipher");

  unsigned char key[EVP_MAX_KEY_LENGTH];
  unsigned char iv[EVP_MAX_IV_LENGTH];

  int key_len = EVP_BytesToKey(cipher,
                               EVP_md5(),
                               nullptr,
                               reinterpret_cast<const unsigned char*>(key_buf),
                               key_buf_len,
                               1,
                               key,
                               iv);
  CHECK_NE(key_len, 0);

  const int mode = EVP_CIPHER_mode(cipher);
  if (kind_ == kCipher && (mode == EVP_CIPH_CTR_MODE ||
                           mode == EVP_CIPH_GCM_MODE ||
                           mode == EVP_CIPH_CCM_MODE)) {
    // Ignore the return value (i.e. possible exception) because we are
    // not calling back into JS anyway.
    ProcessEmitWarning(env(),
                       "Use Cipheriv for counter mode of %s",
                       cipher_type);
  }

  CommonInit(cipher_type, cipher, key, key_len, iv,
             EVP_CIPHER_iv_length(cipher), auth_tag_len);
}


void CipherBase::Init(const FunctionCallbackInfo<Value>& args) {
  CipherBase* cipher;
  ASSIGN_OR_RETURN_UNWRAP(&cipher, args.Holder());

  CHECK_GE(args.Length(), 3);

  const node::Utf8Value cipher_type(args.GetIsolate(), args[0]);
  const char* key_buf = Buffer::Data(args[1]);
  ssize_t key_buf_len = Buffer::Length(args[1]);

  // Don't assign to cipher->auth_tag_len_ directly; the value might not
  // represent a valid length at this point.
  unsigned int auth_tag_len;
  if (args[2]->IsUint32()) {
    auth_tag_len = args[2].As<Uint32>()->Value();
  } else {
    CHECK(args[2]->IsInt32() && args[2].As<Int32>()->Value() == -1);
    auth_tag_len = kNoAuthTagLength;
  }

  cipher->Init(*cipher_type, key_buf, key_buf_len, auth_tag_len);
}

void CipherBase::InitIv(const char* cipher_type,
                        const unsigned char* key,
                        int key_len,
                        const unsigned char* iv,
                        int iv_len,
                        unsigned int auth_tag_len) {
  HandleScope scope(env()->isolate());
  MarkPopErrorOnReturn mark_pop_error_on_return;

  const EVP_CIPHER* const cipher = EVP_get_cipherbyname(cipher_type);
  if (cipher == nullptr) {
    return env()->ThrowError("Unknown cipher");
  }

  const int expected_iv_len = EVP_CIPHER_iv_length(cipher);
  const int mode = EVP_CIPHER_mode(cipher);
  const bool is_authenticated_mode = IsSupportedAuthenticatedMode(mode);
  const bool has_iv = iv_len >= 0;

  // Throw if no IV was passed and the cipher requires an IV
  if (!has_iv && expected_iv_len != 0) {
    char msg[128];
    snprintf(msg, sizeof(msg), "Missing IV for cipher %s", cipher_type);
    return env()->ThrowError(msg);
  }

  // Throw if an IV was passed which does not match the cipher's fixed IV length
  if (!is_authenticated_mode && has_iv && iv_len != expected_iv_len) {
    return env()->ThrowError("Invalid IV length");
  }

  CommonInit(cipher_type, cipher, key, key_len, iv, iv_len, auth_tag_len);
}


void CipherBase::InitIv(const FunctionCallbackInfo<Value>& args) {
  CipherBase* cipher;
  ASSIGN_OR_RETURN_UNWRAP(&cipher, args.Holder());
  Environment* env = cipher->env();

  CHECK_GE(args.Length(), 4);

  const node::Utf8Value cipher_type(env->isolate(), args[0]);
  ssize_t key_len = Buffer::Length(args[1]);
  const unsigned char* key_buf = reinterpret_cast<unsigned char*>(
      Buffer::Data(args[1]));
  ssize_t iv_len;
  const unsigned char* iv_buf;
  if (args[2]->IsNull()) {
    iv_buf = nullptr;
    iv_len = -1;
  } else {
    iv_buf = reinterpret_cast<unsigned char*>(Buffer::Data(args[2]));
    iv_len = Buffer::Length(args[2]);
  }

  // Don't assign to cipher->auth_tag_len_ directly; the value might not
  // represent a valid length at this point.
  unsigned int auth_tag_len;
  if (args[3]->IsUint32()) {
    auth_tag_len = args[3].As<Uint32>()->Value();
  } else {
    CHECK(args[3]->IsInt32() && args[3].As<Int32>()->Value() == -1);
    auth_tag_len = kNoAuthTagLength;
  }

  cipher->InitIv(*cipher_type, key_buf, key_len, iv_buf, iv_len, auth_tag_len);
}


static bool IsValidGCMTagLength(unsigned int tag_len) {
  return tag_len == 4 || tag_len == 8 || (tag_len >= 12 && tag_len <= 16);
}

bool CipherBase::InitAuthenticated(const char* cipher_type, int iv_len,
                                   unsigned int auth_tag_len) {
  CHECK(IsAuthenticatedMode());
  MarkPopErrorOnReturn mark_pop_error_on_return;

  if (!EVP_CIPHER_CTX_ctrl(ctx_.get(),
                           EVP_CTRL_AEAD_SET_IVLEN,
                           iv_len,
                           nullptr)) {
    env()->ThrowError("Invalid IV length");
    return false;
  }

  const int mode = EVP_CIPHER_CTX_mode(ctx_.get());
  if (mode == EVP_CIPH_CCM_MODE || IS_OCB_MODE(mode)) {
    if (auth_tag_len == kNoAuthTagLength) {
      char msg[128];
      snprintf(msg, sizeof(msg), "authTagLength required for %s", cipher_type);
      env()->ThrowError(msg);
      return false;
    }

#ifdef NODE_FIPS_MODE
    // TODO(tniessen) Support CCM decryption in FIPS mode
    if (mode == EVP_CIPH_CCM_MODE && kind_ == kDecipher && FIPS_mode()) {
      env()->ThrowError("CCM decryption not supported in FIPS mode");
      return false;
    }
#endif

    // Tell OpenSSL about the desired length.
    if (!EVP_CIPHER_CTX_ctrl(ctx_.get(), EVP_CTRL_AEAD_SET_TAG, auth_tag_len,
                             nullptr)) {
      env()->ThrowError("Invalid authentication tag length");
      return false;
    }

    // Remember the given authentication tag length for later.
    auth_tag_len_ = auth_tag_len;

    if (mode == EVP_CIPH_CCM_MODE) {
      // Restrict the message length to min(INT_MAX, 2^(8*(15-iv_len))-1) bytes.
      CHECK(iv_len >= 7 && iv_len <= 13);
      max_message_size_ = INT_MAX;
      if (iv_len == 12) max_message_size_ = 16777215;
      if (iv_len == 13) max_message_size_ = 65535;
    }
  } else {
    CHECK_EQ(mode, EVP_CIPH_GCM_MODE);

    if (auth_tag_len != kNoAuthTagLength) {
      if (!IsValidGCMTagLength(auth_tag_len)) {
        char msg[50];
        snprintf(msg, sizeof(msg),
            "Invalid GCM authentication tag length: %u", auth_tag_len);
        env()->ThrowError(msg);
        return false;
      }

      // Remember the given authentication tag length for later.
      auth_tag_len_ = auth_tag_len;
    }
  }

  return true;
}


bool CipherBase::CheckCCMMessageLength(int message_len) {
  CHECK(ctx_);
  CHECK(EVP_CIPHER_CTX_mode(ctx_.get()) == EVP_CIPH_CCM_MODE);

  if (message_len > max_message_size_) {
    env()->ThrowError("Message exceeds maximum size");
    return false;
  }

  return true;
}


bool CipherBase::IsAuthenticatedMode() const {
  // Check if this cipher operates in an AEAD mode that we support.
  CHECK(ctx_);
  const int mode = EVP_CIPHER_CTX_mode(ctx_.get());
  return IsSupportedAuthenticatedMode(mode);
}


void CipherBase::GetAuthTag(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CipherBase* cipher;
  ASSIGN_OR_RETURN_UNWRAP(&cipher, args.Holder());

  // Only callable after Final and if encrypting.
  if (cipher->ctx_ ||
      cipher->kind_ != kCipher ||
      cipher->auth_tag_len_ == kNoAuthTagLength) {
    return args.GetReturnValue().SetUndefined();
  }

  Local<Object> buf =
      Buffer::Copy(env, cipher->auth_tag_, cipher->auth_tag_len_)
      .ToLocalChecked();
  args.GetReturnValue().Set(buf);
}


void CipherBase::SetAuthTag(const FunctionCallbackInfo<Value>& args) {
  CipherBase* cipher;
  ASSIGN_OR_RETURN_UNWRAP(&cipher, args.Holder());

  if (!cipher->ctx_ ||
      !cipher->IsAuthenticatedMode() ||
      cipher->kind_ != kDecipher) {
    return args.GetReturnValue().Set(false);
  }

  // TODO(tniessen): Throw if the authentication tag has already been set.
  if (cipher->auth_tag_state_ == kAuthTagPassedToOpenSSL)
    return args.GetReturnValue().Set(true);

  unsigned int tag_len = Buffer::Length(args[0]);
  const int mode = EVP_CIPHER_CTX_mode(cipher->ctx_.get());
  if (mode == EVP_CIPH_GCM_MODE) {
    if (cipher->auth_tag_len_ != kNoAuthTagLength &&
        cipher->auth_tag_len_ != tag_len) {
      char msg[50];
      snprintf(msg, sizeof(msg),
        "Invalid GCM authentication tag length: %u", tag_len);
      return cipher->env()->ThrowError(msg);
    }
    if (!IsValidGCMTagLength(tag_len)) {
      char msg[125];
      snprintf(msg, sizeof(msg),
          "Permitting authentication tag lengths of %u bytes is deprecated. "
          "Valid GCM tag lengths are 4, 8, 12, 13, 14, 15, 16.", tag_len);
      ProcessEmitDeprecationWarning(cipher->env(), msg, "DEP0090");
    }
  } else if (mode == EVP_CIPH_OCB_MODE) {
    // At this point, the tag length is already known and must match the
    // length of the given authentication tag.
    CHECK(mode == EVP_CIPH_CCM_MODE || IS_OCB_MODE(mode));
    CHECK_NE(cipher->auth_tag_len_, kNoAuthTagLength);
    if (cipher->auth_tag_len_ != tag_len) {
      char msg[50];
      snprintf(msg, sizeof(msg),
          "Invalid authentication tag length: %u", tag_len);
      return cipher->env()->ThrowError(msg);
    }
  }

  // Note: we don't use std::min() here to work around a header conflict.
  cipher->auth_tag_len_ = tag_len;
  cipher->auth_tag_state_ = kAuthTagKnown;
  if (cipher->auth_tag_len_ > sizeof(cipher->auth_tag_))
    cipher->auth_tag_len_ = sizeof(cipher->auth_tag_);

  memset(cipher->auth_tag_, 0, sizeof(cipher->auth_tag_));
  memcpy(cipher->auth_tag_, Buffer::Data(args[0]), cipher->auth_tag_len_);

  args.GetReturnValue().Set(true);
}


bool CipherBase::MaybePassAuthTagToOpenSSL() {
  if (auth_tag_state_ == kAuthTagKnown) {
    if (!EVP_CIPHER_CTX_ctrl(ctx_.get(),
                             EVP_CTRL_AEAD_SET_TAG,
                             auth_tag_len_,
                             reinterpret_cast<unsigned char*>(auth_tag_))) {
      return false;
    }
    auth_tag_state_ = kAuthTagPassedToOpenSSL;
  }
  return true;
}


bool CipherBase::SetAAD(const char* data, unsigned int len, int plaintext_len) {
  if (!ctx_ || !IsAuthenticatedMode())
    return false;
  MarkPopErrorOnReturn mark_pop_error_on_return;

  int outlen;
  const int mode = EVP_CIPHER_CTX_mode(ctx_.get());

  // When in CCM mode, we need to set the authentication tag and the plaintext
  // length in advance.
  if (mode == EVP_CIPH_CCM_MODE) {
    if (plaintext_len < 0) {
      env()->ThrowError("plaintextLength required for CCM mode with AAD");
      return false;
    }

    if (!CheckCCMMessageLength(plaintext_len))
      return false;

    if (kind_ == kDecipher) {
      if (!MaybePassAuthTagToOpenSSL())
        return false;
    }

    // Specify the plaintext length.
    if (!EVP_CipherUpdate(ctx_.get(), nullptr, &outlen, nullptr, plaintext_len))
      return false;
  }

  return 1 == EVP_CipherUpdate(ctx_.get(),
                               nullptr,
                               &outlen,
                               reinterpret_cast<const unsigned char*>(data),
                               len);
}


void CipherBase::SetAAD(const FunctionCallbackInfo<Value>& args) {
  CipherBase* cipher;
  ASSIGN_OR_RETURN_UNWRAP(&cipher, args.Holder());

  CHECK_EQ(args.Length(), 2);
  CHECK(args[1]->IsInt32());
  int plaintext_len = args[1].As<Int32>()->Value();

  bool b = cipher->SetAAD(Buffer::Data(args[0]), Buffer::Length(args[0]),
                          plaintext_len);
  args.GetReturnValue().Set(b);  // Possibly report invalid state failure
}


CipherBase::UpdateResult CipherBase::Update(const char* data,
                                            int len,
                                            unsigned char** out,
                                            int* out_len) {
  if (!ctx_)
    return kErrorState;
  MarkPopErrorOnReturn mark_pop_error_on_return;

  const int mode = EVP_CIPHER_CTX_mode(ctx_.get());

  if (mode == EVP_CIPH_CCM_MODE) {
    if (!CheckCCMMessageLength(len))
      return kErrorMessageSize;
  }

  // Pass the authentication tag to OpenSSL if possible. This will only happen
  // once, usually on the first update.
  if (kind_ == kDecipher && IsAuthenticatedMode()) {
    CHECK(MaybePassAuthTagToOpenSSL());
  }

  *out_len = 0;
  int buff_len = len + EVP_CIPHER_CTX_block_size(ctx_.get());
  // For key wrapping algorithms, get output size by calling
  // EVP_CipherUpdate() with null output.
  if (kind_ == kCipher && mode == EVP_CIPH_WRAP_MODE &&
      EVP_CipherUpdate(ctx_.get(),
                       nullptr,
                       &buff_len,
                       reinterpret_cast<const unsigned char*>(data),
                       len) != 1) {
    return kErrorState;
  }

  *out = Malloc<unsigned char>(buff_len);
  int r = EVP_CipherUpdate(ctx_.get(),
                           *out,
                           out_len,
                           reinterpret_cast<const unsigned char*>(data),
                           len);

  CHECK_LE(*out_len, buff_len);

  // When in CCM mode, EVP_CipherUpdate will fail if the authentication tag is
  // invalid. In that case, remember the error and throw in final().
  if (!r && kind_ == kDecipher && mode == EVP_CIPH_CCM_MODE) {
    pending_auth_failed_ = true;
    return kSuccess;
  }
  return r == 1 ? kSuccess : kErrorState;
}


void CipherBase::Update(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CipherBase* cipher;
  ASSIGN_OR_RETURN_UNWRAP(&cipher, args.Holder());

  unsigned char* out = nullptr;
  UpdateResult r;
  int out_len = 0;

  // Only copy the data if we have to, because it's a string
  if (args[0]->IsString()) {
    StringBytes::InlineDecoder decoder;
    if (!decoder.Decode(env, args[0].As<String>(), args[1], UTF8)
             .FromMaybe(false))
      return;
    r = cipher->Update(decoder.out(), decoder.size(), &out, &out_len);
  } else {
    char* buf = Buffer::Data(args[0]);
    size_t buflen = Buffer::Length(args[0]);
    r = cipher->Update(buf, buflen, &out, &out_len);
  }

  if (r != kSuccess) {
    free(out);
    if (r == kErrorState) {
      ThrowCryptoError(env, ERR_get_error(),
                       "Trying to add data in unsupported state");
    }
    return;
  }

  CHECK(out != nullptr || out_len == 0);
  Local<Object> buf =
      Buffer::New(env, reinterpret_cast<char*>(out), out_len).ToLocalChecked();

  args.GetReturnValue().Set(buf);
}


bool CipherBase::SetAutoPadding(bool auto_padding) {
  if (!ctx_)
    return false;
  MarkPopErrorOnReturn mark_pop_error_on_return;
  return EVP_CIPHER_CTX_set_padding(ctx_.get(), auto_padding);
}


void CipherBase::SetAutoPadding(const FunctionCallbackInfo<Value>& args) {
  CipherBase* cipher;
  ASSIGN_OR_RETURN_UNWRAP(&cipher, args.Holder());

  bool b = cipher->SetAutoPadding(args.Length() < 1 || args[0]->IsTrue());
  args.GetReturnValue().Set(b);  // Possibly report invalid state failure
}


bool CipherBase::Final(unsigned char** out, int* out_len) {
  if (!ctx_)
    return false;

  const int mode = EVP_CIPHER_CTX_mode(ctx_.get());

  *out = Malloc<unsigned char>(
      static_cast<size_t>(EVP_CIPHER_CTX_block_size(ctx_.get())));

  if (kind_ == kDecipher && IsSupportedAuthenticatedMode(mode)) {
    MaybePassAuthTagToOpenSSL();
  }

  // In CCM mode, final() only checks whether authentication failed in update().
  // EVP_CipherFinal_ex must not be called and will fail.
  bool ok;
  if (kind_ == kDecipher && mode == EVP_CIPH_CCM_MODE) {
    ok = !pending_auth_failed_;
  } else {
    ok = EVP_CipherFinal_ex(ctx_.get(), *out, out_len) == 1;

    if (ok && kind_ == kCipher && IsAuthenticatedMode()) {
      // In GCM mode, the authentication tag length can be specified in advance,
      // but defaults to 16 bytes when encrypting. In CCM and OCB mode, it must
      // always be given by the user.
      if (auth_tag_len_ == kNoAuthTagLength) {
        CHECK(mode == EVP_CIPH_GCM_MODE);
        auth_tag_len_ = sizeof(auth_tag_);
      }
      CHECK_EQ(1, EVP_CIPHER_CTX_ctrl(ctx_.get(), EVP_CTRL_AEAD_GET_TAG,
                      auth_tag_len_,
                      reinterpret_cast<unsigned char*>(auth_tag_)));
    }
  }

  ctx_.reset();

  return ok;
}


void CipherBase::Final(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CipherBase* cipher;
  ASSIGN_OR_RETURN_UNWRAP(&cipher, args.Holder());
  if (cipher->ctx_ == nullptr) return env->ThrowError("Unsupported state");

  unsigned char* out_value = nullptr;
  int out_len = -1;

  // Check IsAuthenticatedMode() first, Final() destroys the EVP_CIPHER_CTX.
  const bool is_auth_mode = cipher->IsAuthenticatedMode();
  bool r = cipher->Final(&out_value, &out_len);

  if (out_len <= 0 || !r) {
    free(out_value);
    out_value = nullptr;
    out_len = 0;
    if (!r) {
      const char* msg = is_auth_mode ?
          "Unsupported state or unable to authenticate data" :
          "Unsupported state";

      return ThrowCryptoError(env,
                              ERR_get_error(),
                              msg);
    }
  }

  Local<Object> buf = Buffer::New(
      env,
      reinterpret_cast<char*>(out_value),
      out_len).ToLocalChecked();
  args.GetReturnValue().Set(buf);
}


void Hmac::Initialize(Environment* env, v8::Local<Object> target) {
  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(t, "init", HmacInit);
  env->SetProtoMethod(t, "update", HmacUpdate);
  env->SetProtoMethod(t, "digest", HmacDigest);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "Hmac"),
              t->GetFunction(env->context()).ToLocalChecked());
}


void Hmac::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  new Hmac(env, args.This());
}


void Hmac::HmacInit(const char* hash_type, const char* key, int key_len) {
  HandleScope scope(env()->isolate());

  const EVP_MD* md = EVP_get_digestbyname(hash_type);
  if (md == nullptr) {
    return env()->ThrowError("Unknown message digest");
  }
  if (key_len == 0) {
    key = "";
  }
  ctx_.reset(HMAC_CTX_new());
  if (!ctx_ || !HMAC_Init_ex(ctx_.get(), key, key_len, md, nullptr)) {
    ctx_.reset();
    return ThrowCryptoError(env(), ERR_get_error());
  }
}


void Hmac::HmacInit(const FunctionCallbackInfo<Value>& args) {
  Hmac* hmac;
  ASSIGN_OR_RETURN_UNWRAP(&hmac, args.Holder());
  Environment* env = hmac->env();

  const node::Utf8Value hash_type(env->isolate(), args[0]);
  const char* buffer_data = Buffer::Data(args[1]);
  size_t buffer_length = Buffer::Length(args[1]);
  hmac->HmacInit(*hash_type, buffer_data, buffer_length);
}


bool Hmac::HmacUpdate(const char* data, int len) {
  if (!ctx_)
    return false;
  int r = HMAC_Update(ctx_.get(),
                      reinterpret_cast<const unsigned char*>(data),
                      len);
  return r == 1;
}


void Hmac::HmacUpdate(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Hmac* hmac;
  ASSIGN_OR_RETURN_UNWRAP(&hmac, args.Holder());

  // Only copy the data if we have to, because it's a string
  bool r = false;
  if (args[0]->IsString()) {
    StringBytes::InlineDecoder decoder;
    if (decoder.Decode(env, args[0].As<String>(), args[1], UTF8)
            .FromMaybe(false)) {
      r = hmac->HmacUpdate(decoder.out(), decoder.size());
    }
  } else {
    CHECK(args[0]->IsArrayBufferView());
    char* buf = Buffer::Data(args[0]);
    size_t buflen = Buffer::Length(args[0]);
    r = hmac->HmacUpdate(buf, buflen);
  }

  args.GetReturnValue().Set(r);
}


void Hmac::HmacDigest(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Hmac* hmac;
  ASSIGN_OR_RETURN_UNWRAP(&hmac, args.Holder());

  enum encoding encoding = BUFFER;
  if (args.Length() >= 1) {
    encoding = ParseEncoding(env->isolate(), args[0], BUFFER);
  }
  CHECK_NE(encoding, UCS2);  // Digest does not support UTF-16

  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len = 0;

  if (hmac->ctx_) {
    HMAC_Final(hmac->ctx_.get(), md_value, &md_len);
    hmac->ctx_.reset();
  }

  Local<Value> error;
  MaybeLocal<Value> rc =
      StringBytes::Encode(env->isolate(),
                          reinterpret_cast<const char*>(md_value),
                          md_len,
                          encoding,
                          &error);
  if (rc.IsEmpty()) {
    CHECK(!error.IsEmpty());
    env->isolate()->ThrowException(error);
    return;
  }
  args.GetReturnValue().Set(rc.ToLocalChecked());
}


void Hash::Initialize(Environment* env, v8::Local<Object> target) {
  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(t, "update", HashUpdate);
  env->SetProtoMethod(t, "digest", HashDigest);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "Hash"),
              t->GetFunction(env->context()).ToLocalChecked());
}


void Hash::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const node::Utf8Value hash_type(env->isolate(), args[0]);

  Hash* hash = new Hash(env, args.This());
  if (!hash->HashInit(*hash_type)) {
    return ThrowCryptoError(env, ERR_get_error(),
                            "Digest method not supported");
  }
}


bool Hash::HashInit(const char* hash_type) {
  const EVP_MD* md = EVP_get_digestbyname(hash_type);
  if (md == nullptr)
    return false;
  mdctx_.reset(EVP_MD_CTX_new());
  if (!mdctx_ || EVP_DigestInit_ex(mdctx_.get(), md, nullptr) <= 0) {
    mdctx_.reset();
    return false;
  }
  return true;
}


bool Hash::HashUpdate(const char* data, int len) {
  if (!mdctx_)
    return false;
  EVP_DigestUpdate(mdctx_.get(), data, len);
  return true;
}


void Hash::HashUpdate(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Hash* hash;
  ASSIGN_OR_RETURN_UNWRAP(&hash, args.Holder());

  // Only copy the data if we have to, because it's a string
  bool r = true;
  if (args[0]->IsString()) {
    StringBytes::InlineDecoder decoder;
    if (!decoder.Decode(env, args[0].As<String>(), args[1], UTF8)
             .FromMaybe(false)) {
      args.GetReturnValue().Set(false);
      return;
    }
    r = hash->HashUpdate(decoder.out(), decoder.size());
  } else if (args[0]->IsArrayBufferView()) {
    char* buf = Buffer::Data(args[0]);
    size_t buflen = Buffer::Length(args[0]);
    r = hash->HashUpdate(buf, buflen);
  }

  args.GetReturnValue().Set(r);
}


void Hash::HashDigest(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Hash* hash;
  ASSIGN_OR_RETURN_UNWRAP(&hash, args.Holder());

  enum encoding encoding = BUFFER;
  if (args.Length() >= 1) {
    encoding = ParseEncoding(env->isolate(), args[0], BUFFER);
  }

  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;

  EVP_DigestFinal_ex(hash->mdctx_.get(), md_value, &md_len);

  Local<Value> error;
  MaybeLocal<Value> rc =
      StringBytes::Encode(env->isolate(),
                          reinterpret_cast<const char*>(md_value),
                          md_len,
                          encoding,
                          &error);
  if (rc.IsEmpty()) {
    CHECK(!error.IsEmpty());
    env->isolate()->ThrowException(error);
    return;
  }
  args.GetReturnValue().Set(rc.ToLocalChecked());
}


SignBase::Error SignBase::Init(const char* sign_type) {
  CHECK_NULL(mdctx_);
  // Historically, "dss1" and "DSS1" were DSA aliases for SHA-1
  // exposed through the public API.
  if (strcmp(sign_type, "dss1") == 0 ||
      strcmp(sign_type, "DSS1") == 0) {
    sign_type = "SHA1";
  }
  const EVP_MD* md = EVP_get_digestbyname(sign_type);
  if (md == nullptr)
    return kSignUnknownDigest;

  mdctx_.reset(EVP_MD_CTX_new());
  if (!mdctx_ || !EVP_DigestInit_ex(mdctx_.get(), md, nullptr)) {
    mdctx_.reset();
    return kSignInit;
  }

  return kSignOk;
}


SignBase::Error SignBase::Update(const char* data, int len) {
  if (mdctx_ == nullptr)
    return kSignNotInitialised;
  if (!EVP_DigestUpdate(mdctx_.get(), data, len))
    return kSignUpdate;
  return kSignOk;
}


void SignBase::CheckThrow(SignBase::Error error) {
  HandleScope scope(env()->isolate());

  switch (error) {
    case kSignUnknownDigest:
      return env()->ThrowError("Unknown message digest");

    case kSignNotInitialised:
      return env()->ThrowError("Not initialised");

    case kSignInit:
    case kSignUpdate:
    case kSignPrivateKey:
    case kSignPublicKey:
      {
        unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
        if (err)
          return ThrowCryptoError(env(), err);
        switch (error) {
          case kSignInit:
            return env()->ThrowError("EVP_SignInit_ex failed");
          case kSignUpdate:
            return env()->ThrowError("EVP_SignUpdate failed");
          case kSignPrivateKey:
            return env()->ThrowError("PEM_read_bio_PrivateKey failed");
          case kSignPublicKey:
            return env()->ThrowError("PEM_read_bio_PUBKEY failed");
          default:
            ABORT();
        }
      }

    case kSignOk:
      return;
  }
}

static bool ApplyRSAOptions(const EVPKeyPointer& pkey,
                            EVP_PKEY_CTX* pkctx,
                            int padding,
                            int salt_len) {
  if (EVP_PKEY_id(pkey.get()) == EVP_PKEY_RSA ||
      EVP_PKEY_id(pkey.get()) == EVP_PKEY_RSA2) {
    if (EVP_PKEY_CTX_set_rsa_padding(pkctx, padding) <= 0)
      return false;
    if (padding == RSA_PKCS1_PSS_PADDING) {
      if (EVP_PKEY_CTX_set_rsa_pss_saltlen(pkctx, salt_len) <= 0)
        return false;
    }
  }

  return true;
}



void Sign::Initialize(Environment* env, v8::Local<Object> target) {
  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(t, "init", SignInit);
  env->SetProtoMethod(t, "update", SignUpdate);
  env->SetProtoMethod(t, "sign", SignFinal);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "Sign"),
              t->GetFunction(env->context()).ToLocalChecked());
}


void Sign::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  new Sign(env, args.This());
}


void Sign::SignInit(const FunctionCallbackInfo<Value>& args) {
  Sign* sign;
  ASSIGN_OR_RETURN_UNWRAP(&sign, args.Holder());

  const node::Utf8Value sign_type(args.GetIsolate(), args[0]);
  sign->CheckThrow(sign->Init(*sign_type));
}


void Sign::SignUpdate(const FunctionCallbackInfo<Value>& args) {
  Sign* sign;
  ASSIGN_OR_RETURN_UNWRAP(&sign, args.Holder());

  Error err;
  char* buf = Buffer::Data(args[0]);
  size_t buflen = Buffer::Length(args[0]);
  err = sign->Update(buf, buflen);

  sign->CheckThrow(err);
}

static MallocedBuffer<unsigned char> Node_SignFinal(EVPMDPointer&& mdctx,
                                                    const EVPKeyPointer& pkey,
                                                    int padding,
                                                    int pss_salt_len) {
  unsigned char m[EVP_MAX_MD_SIZE];
  unsigned int m_len;

  if (!EVP_DigestFinal_ex(mdctx.get(), m, &m_len))
    return MallocedBuffer<unsigned char>();

  int signed_sig_len = EVP_PKEY_size(pkey.get());
  CHECK_GE(signed_sig_len, 0);
  size_t sig_len = static_cast<size_t>(signed_sig_len);
  MallocedBuffer<unsigned char> sig(sig_len);

  EVPKeyCtxPointer pkctx(EVP_PKEY_CTX_new(pkey.get(), nullptr));
  if (pkctx &&
      EVP_PKEY_sign_init(pkctx.get()) > 0 &&
      ApplyRSAOptions(pkey, pkctx.get(), padding, pss_salt_len) &&
      EVP_PKEY_CTX_set_signature_md(pkctx.get(),
                                    EVP_MD_CTX_md(mdctx.get())) > 0 &&
      EVP_PKEY_sign(pkctx.get(), sig.data, &sig_len, m, m_len) > 0) {
    sig.Truncate(sig_len);
    return sig;
  }

  return MallocedBuffer<unsigned char>();
}

Sign::SignResult Sign::SignFinal(
    const char* key_pem,
    int key_pem_len,
    const char* passphrase,
    int padding,
    int salt_len) {
  if (!mdctx_)
    return SignResult(kSignNotInitialised);

  EVPMDPointer mdctx = std::move(mdctx_);

  BIOPointer bp(BIO_new_mem_buf(const_cast<char*>(key_pem), key_pem_len));
  if (!bp)
    return SignResult(kSignPrivateKey);

  EVPKeyPointer pkey(PEM_read_bio_PrivateKey(bp.get(),
                                             nullptr,
                                             PasswordCallback,
                                             const_cast<char*>(passphrase)));

  // Errors might be injected into OpenSSL's error stack
  // without `pkey` being set to nullptr;
  // cf. the test of `test_bad_rsa_privkey.pem` for an example.
  if (!pkey || 0 != ERR_peek_error())
    return SignResult(kSignPrivateKey);

#ifdef NODE_FIPS_MODE
  /* Validate DSA2 parameters from FIPS 186-4 */
  if (FIPS_mode() && EVP_PKEY_DSA == pkey->type) {
    size_t L = BN_num_bits(pkey->pkey.dsa->p);
    size_t N = BN_num_bits(pkey->pkey.dsa->q);
    bool result = false;

    if (L == 1024 && N == 160)
      result = true;
    else if (L == 2048 && N == 224)
      result = true;
    else if (L == 2048 && N == 256)
      result = true;
    else if (L == 3072 && N == 256)
      result = true;

    if (!result) {
      return kSignPrivateKey;
    }
  }
#endif  // NODE_FIPS_MODE

  MallocedBuffer<unsigned char> buffer =
      Node_SignFinal(std::move(mdctx), pkey, padding, salt_len);
  Error error = buffer.is_empty() ? kSignPrivateKey : kSignOk;
  return SignResult(error, std::move(buffer));
}


void Sign::SignFinal(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Sign* sign;
  ASSIGN_OR_RETURN_UNWRAP(&sign, args.Holder());

  unsigned int len = args.Length();

  node::Utf8Value passphrase(env->isolate(), args[1]);

  size_t buf_len = Buffer::Length(args[0]);
  char* buf = Buffer::Data(args[0]);

  CHECK(args[2]->IsInt32());
  int padding = args[2].As<Int32>()->Value();

  CHECK(args[3]->IsInt32());
  int salt_len = args[3].As<Int32>()->Value();

  ClearErrorOnReturn clear_error_on_return;

  SignResult ret = sign->SignFinal(
      buf,
      buf_len,
      len >= 2 && !args[1]->IsNull() ? *passphrase : nullptr,
      padding,
      salt_len);

  if (ret.error != kSignOk)
    return sign->CheckThrow(ret.error);

  MallocedBuffer<unsigned char> sig =
      std::move(ret.signature);

  Local<Object> rc =
      Buffer::New(env, reinterpret_cast<char*>(sig.release()), sig.size)
      .ToLocalChecked();
  args.GetReturnValue().Set(rc);
}

enum ParsePublicKeyResult {
  kParsePublicOk,
  kParsePublicNotRecognized,
  kParsePublicFailed
};

static ParsePublicKeyResult TryParsePublicKey(
    EVPKeyPointer* pkey,
    const BIOPointer& bp,
    const char* name,
    // NOLINTNEXTLINE(runtime/int)
    std::function<EVP_PKEY*(const unsigned char** p, long l)> parse) {
  unsigned char* der_data;
  long der_len;  // NOLINT(runtime/int)

  // This skips surrounding data and decodes PEM to DER.
  {
    MarkPopErrorOnReturn mark_pop_error_on_return;
    if (PEM_bytes_read_bio(&der_data, &der_len, nullptr, name,
                           bp.get(), nullptr, nullptr) != 1)
      return kParsePublicNotRecognized;
  }

  // OpenSSL might modify the pointer, so we need to make a copy before parsing.
  const unsigned char* p = der_data;
  pkey->reset(parse(&p, der_len));
  OPENSSL_clear_free(der_data, der_len);

  return *pkey ? kParsePublicOk : kParsePublicFailed;
}

static ParsePublicKeyResult ParsePublicKey(EVPKeyPointer* pkey,
                                           const char* key_pem,
                                           int key_pem_len) {
  BIOPointer bp(BIO_new_mem_buf(const_cast<char*>(key_pem), key_pem_len));
  if (!bp)
    return kParsePublicFailed;

  ParsePublicKeyResult ret;

  // Try PKCS#8 first.
  ret = TryParsePublicKey(pkey, bp, "PUBLIC KEY",
      [](const unsigned char** p, long l) {  // NOLINT(runtime/int)
        return d2i_PUBKEY(nullptr, p, l);
      });
  if (ret != kParsePublicNotRecognized)
    return ret;

  // Maybe it is PKCS#1.
  CHECK(BIO_reset(bp.get()));
  ret = TryParsePublicKey(pkey, bp, "RSA PUBLIC KEY",
      [](const unsigned char** p, long l) {  // NOLINT(runtime/int)
        return d2i_PublicKey(EVP_PKEY_RSA, nullptr, p, l);
      });
  if (ret != kParsePublicNotRecognized)
    return ret;

  // X.509 fallback.
  CHECK(BIO_reset(bp.get()));
  return TryParsePublicKey(pkey, bp, "CERTIFICATE",
      [](const unsigned char** p, long l) {  // NOLINT(runtime/int)
        X509Pointer x509(d2i_X509(nullptr, p, l));
        return x509 ? X509_get_pubkey(x509.get()) : nullptr;
      });
}

void Verify::Initialize(Environment* env, v8::Local<Object> target) {
  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(t, "init", VerifyInit);
  env->SetProtoMethod(t, "update", VerifyUpdate);
  env->SetProtoMethod(t, "verify", VerifyFinal);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "Verify"),
              t->GetFunction(env->context()).ToLocalChecked());
}


void Verify::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  new Verify(env, args.This());
}


void Verify::VerifyInit(const FunctionCallbackInfo<Value>& args) {
  Verify* verify;
  ASSIGN_OR_RETURN_UNWRAP(&verify, args.Holder());

  const node::Utf8Value verify_type(args.GetIsolate(), args[0]);
  verify->CheckThrow(verify->Init(*verify_type));
}


void Verify::VerifyUpdate(const FunctionCallbackInfo<Value>& args) {
  Verify* verify;
  ASSIGN_OR_RETURN_UNWRAP(&verify, args.Holder());

  Error err;
  char* buf = Buffer::Data(args[0]);
  size_t buflen = Buffer::Length(args[0]);
  err = verify->Update(buf, buflen);

  verify->CheckThrow(err);
}


SignBase::Error Verify::VerifyFinal(const char* key_pem,
                                    int key_pem_len,
                                    const char* sig,
                                    int siglen,
                                    int padding,
                                    int saltlen,
                                    bool* verify_result) {
  if (!mdctx_)
    return kSignNotInitialised;

  EVPKeyPointer pkey;
  unsigned char m[EVP_MAX_MD_SIZE];
  unsigned int m_len;
  *verify_result = false;
  EVPMDPointer mdctx = std::move(mdctx_);

  if (ParsePublicKey(&pkey, key_pem, key_pem_len) != kParsePublicOk)
    return kSignPublicKey;

  if (!EVP_DigestFinal_ex(mdctx.get(), m, &m_len))
    return kSignPublicKey;

  EVPKeyCtxPointer pkctx(EVP_PKEY_CTX_new(pkey.get(), nullptr));
  if (pkctx &&
      EVP_PKEY_verify_init(pkctx.get()) > 0 &&
      ApplyRSAOptions(pkey, pkctx.get(), padding, saltlen) &&
      EVP_PKEY_CTX_set_signature_md(pkctx.get(),
                                    EVP_MD_CTX_md(mdctx.get())) > 0) {
    const int r = EVP_PKEY_verify(pkctx.get(),
                                  reinterpret_cast<const unsigned char*>(sig),
                                  siglen,
                                  m,
                                  m_len);
    *verify_result = r == 1;
  }

  return kSignOk;
}


void Verify::VerifyFinal(const FunctionCallbackInfo<Value>& args) {
  ClearErrorOnReturn clear_error_on_return;

  Verify* verify;
  ASSIGN_OR_RETURN_UNWRAP(&verify, args.Holder());

  char* kbuf = Buffer::Data(args[0]);
  ssize_t klen = Buffer::Length(args[0]);

  char* hbuf = Buffer::Data(args[1]);
  ssize_t hlen = Buffer::Length(args[1]);

  CHECK(args[2]->IsInt32());
  int padding = args[2].As<Int32>()->Value();

  CHECK(args[3]->IsInt32());
  int salt_len = args[3].As<Int32>()->Value();

  bool verify_result;
  Error err = verify->VerifyFinal(kbuf, klen, hbuf, hlen, padding, salt_len,
                                  &verify_result);
  if (err != kSignOk)
    return verify->CheckThrow(err);
  args.GetReturnValue().Set(verify_result);
}


template <PublicKeyCipher::Operation operation,
          PublicKeyCipher::EVP_PKEY_cipher_init_t EVP_PKEY_cipher_init,
          PublicKeyCipher::EVP_PKEY_cipher_t EVP_PKEY_cipher>
bool PublicKeyCipher::Cipher(const char* key_pem,
                             int key_pem_len,
                             const char* passphrase,
                             int padding,
                             const unsigned char* data,
                             int len,
                             unsigned char** out,
                             size_t* out_len) {
  EVPKeyPointer pkey;

  // Check if this is a PKCS#8 or RSA public key before trying as X.509 and
  // private key.
  if (operation == kPublic) {
    ParsePublicKeyResult pkeyres = ParsePublicKey(&pkey, key_pem, key_pem_len);
    if (pkeyres == kParsePublicFailed)
      return false;
  }
  if (!pkey) {
    // Private key fallback.
    BIOPointer bp(BIO_new_mem_buf(const_cast<char*>(key_pem), key_pem_len));
    if (!bp)
      return false;
    pkey.reset(PEM_read_bio_PrivateKey(bp.get(),
                                       nullptr,
                                       PasswordCallback,
                                       const_cast<char*>(passphrase)));
    if (!pkey)
      return false;
  }

  EVPKeyCtxPointer ctx(EVP_PKEY_CTX_new(pkey.get(), nullptr));
  if (!ctx)
    return false;
  if (EVP_PKEY_cipher_init(ctx.get()) <= 0)
    return false;
  if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), padding) <= 0)
    return false;

  if (EVP_PKEY_cipher(ctx.get(), nullptr, out_len, data, len) <= 0)
    return false;

  *out = Malloc<unsigned char>(*out_len);

  if (EVP_PKEY_cipher(ctx.get(), *out, out_len, data, len) <= 0)
    return false;

  return true;
}


template <PublicKeyCipher::Operation operation,
          PublicKeyCipher::EVP_PKEY_cipher_init_t EVP_PKEY_cipher_init,
          PublicKeyCipher::EVP_PKEY_cipher_t EVP_PKEY_cipher>
void PublicKeyCipher::Cipher(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "Key");
  char* kbuf = Buffer::Data(args[0]);
  ssize_t klen = Buffer::Length(args[0]);

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[1], "Data");
  char* buf = Buffer::Data(args[1]);
  ssize_t len = Buffer::Length(args[1]);

  uint32_t padding;
  if (!args[2]->Uint32Value(env->context()).To(&padding)) return;

  String::Utf8Value passphrase(args.GetIsolate(), args[3]);

  unsigned char* out_value = nullptr;
  size_t out_len = 0;

  ClearErrorOnReturn clear_error_on_return;

  bool r = Cipher<operation, EVP_PKEY_cipher_init, EVP_PKEY_cipher>(
      kbuf,
      klen,
      args.Length() >= 4 && !args[3]->IsNull() ? *passphrase : nullptr,
      padding,
      reinterpret_cast<const unsigned char*>(buf),
      len,
      &out_value,
      &out_len);

  if (out_len == 0 || !r) {
    free(out_value);
    out_value = nullptr;
    out_len = 0;
    if (!r) {
      return ThrowCryptoError(env,
        ERR_get_error());
    }
  }

  Local<Object> vbuf =
      Buffer::New(env, reinterpret_cast<char*>(out_value), out_len)
      .ToLocalChecked();
  args.GetReturnValue().Set(vbuf);
}


void DiffieHellman::Initialize(Environment* env, Local<Object> target) {
  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);

  const PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(t, "generateKeys", GenerateKeys);
  env->SetProtoMethod(t, "computeSecret", ComputeSecret);
  env->SetProtoMethodNoSideEffect(t, "getPrime", GetPrime);
  env->SetProtoMethodNoSideEffect(t, "getGenerator", GetGenerator);
  env->SetProtoMethodNoSideEffect(t, "getPublicKey", GetPublicKey);
  env->SetProtoMethodNoSideEffect(t, "getPrivateKey", GetPrivateKey);
  env->SetProtoMethod(t, "setPublicKey", SetPublicKey);
  env->SetProtoMethod(t, "setPrivateKey", SetPrivateKey);

  Local<FunctionTemplate> verify_error_getter_templ =
      FunctionTemplate::New(env->isolate(),
                            DiffieHellman::VerifyErrorGetter,
                            env->as_external(),
                            Signature::New(env->isolate(), t),
                            /* length */ 0,
                            // TODO(TimothyGu): should be deny
                            ConstructorBehavior::kAllow,
                            SideEffectType::kHasNoSideEffect);

  t->InstanceTemplate()->SetAccessorProperty(
      env->verify_error_string(),
      verify_error_getter_templ,
      Local<FunctionTemplate>(),
      attributes);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "DiffieHellman"),
              t->GetFunction(env->context()).ToLocalChecked());

  Local<FunctionTemplate> t2 = env->NewFunctionTemplate(DiffieHellmanGroup);
  t2->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(t2, "generateKeys", GenerateKeys);
  env->SetProtoMethod(t2, "computeSecret", ComputeSecret);
  env->SetProtoMethodNoSideEffect(t2, "getPrime", GetPrime);
  env->SetProtoMethodNoSideEffect(t2, "getGenerator", GetGenerator);
  env->SetProtoMethodNoSideEffect(t2, "getPublicKey", GetPublicKey);
  env->SetProtoMethodNoSideEffect(t2, "getPrivateKey", GetPrivateKey);

  Local<FunctionTemplate> verify_error_getter_templ2 =
      FunctionTemplate::New(env->isolate(),
                            DiffieHellman::VerifyErrorGetter,
                            env->as_external(),
                            Signature::New(env->isolate(), t2),
                            /* length */ 0,
                            // TODO(TimothyGu): should be deny
                            ConstructorBehavior::kAllow,
                            SideEffectType::kHasNoSideEffect);

  t2->InstanceTemplate()->SetAccessorProperty(
      env->verify_error_string(),
      verify_error_getter_templ2,
      Local<FunctionTemplate>(),
      attributes);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "DiffieHellmanGroup"),
              t2->GetFunction(env->context()).ToLocalChecked());
}


bool DiffieHellman::Init(int primeLength, int g) {
  dh_.reset(DH_new());
  if (!DH_generate_parameters_ex(dh_.get(), primeLength, g, 0))
    return false;
  return VerifyContext();
}


bool DiffieHellman::Init(const char* p, int p_len, int g) {
  dh_.reset(DH_new());
  BIGNUM* bn_p =
      BN_bin2bn(reinterpret_cast<const unsigned char*>(p), p_len, nullptr);
  BIGNUM* bn_g = BN_new();
  if (!BN_set_word(bn_g, g) ||
      !DH_set0_pqg(dh_.get(), bn_p, nullptr, bn_g)) {
    BN_free(bn_p);
    BN_free(bn_g);
    return false;
  }
  return VerifyContext();
}


bool DiffieHellman::Init(const char* p, int p_len, const char* g, int g_len) {
  dh_.reset(DH_new());
  BIGNUM* bn_p = BN_bin2bn(reinterpret_cast<const unsigned char*>(p), p_len, 0);
  BIGNUM* bn_g = BN_bin2bn(reinterpret_cast<const unsigned char*>(g), g_len, 0);
  if (!DH_set0_pqg(dh_.get(), bn_p, nullptr, bn_g)) {
    BN_free(bn_p);
    BN_free(bn_g);
    return false;
  }
  return VerifyContext();
}


void DiffieHellman::DiffieHellmanGroup(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  DiffieHellman* diffieHellman = new DiffieHellman(env, args.This());

  if (args.Length() != 1) {
    return THROW_ERR_MISSING_ARGS(env, "Group name argument is mandatory");
  }

  THROW_AND_RETURN_IF_NOT_STRING(env, args[0], "Group name");

  bool initialized = false;

  const node::Utf8Value group_name(env->isolate(), args[0]);
  for (size_t i = 0; i < arraysize(modp_groups); ++i) {
    const modp_group* it = modp_groups + i;

    if (!StringEqualNoCase(*group_name, it->name))
      continue;

    initialized = diffieHellman->Init(it->prime,
                                      it->prime_size,
                                      it->gen,
                                      it->gen_size);
    if (!initialized)
      env->ThrowError("Initialization failed");
    return;
  }

  env->ThrowError("Unknown group");
}


void DiffieHellman::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  DiffieHellman* diffieHellman =
      new DiffieHellman(env, args.This());
  bool initialized = false;

  if (args.Length() == 2) {
    if (args[0]->IsInt32()) {
      if (args[1]->IsInt32()) {
        initialized = diffieHellman->Init(args[0].As<Int32>()->Value(),
                                          args[1].As<Int32>()->Value());
      }
    } else {
      if (args[1]->IsInt32()) {
        initialized = diffieHellman->Init(Buffer::Data(args[0]),
                                          Buffer::Length(args[0]),
                                          args[1].As<Int32>()->Value());
      } else {
        initialized = diffieHellman->Init(Buffer::Data(args[0]),
                                          Buffer::Length(args[0]),
                                          Buffer::Data(args[1]),
                                          Buffer::Length(args[1]));
      }
    }
  }

  if (!initialized) {
    return ThrowCryptoError(env, ERR_get_error(), "Initialization failed");
  }
}


void DiffieHellman::GenerateKeys(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  DiffieHellman* diffieHellman;
  ASSIGN_OR_RETURN_UNWRAP(&diffieHellman, args.Holder());

  if (!DH_generate_key(diffieHellman->dh_.get())) {
    return ThrowCryptoError(env, ERR_get_error(), "Key generation failed");
  }

  const BIGNUM* pub_key;
  DH_get0_key(diffieHellman->dh_.get(), &pub_key, nullptr);
  const int size = BN_num_bytes(pub_key);
  CHECK_GE(size, 0);
  char* data = Malloc(size);
  CHECK_EQ(size,
           BN_bn2binpad(pub_key, reinterpret_cast<unsigned char*>(data), size));
  args.GetReturnValue().Set(Buffer::New(env, data, size).ToLocalChecked());
}


void DiffieHellman::GetField(const FunctionCallbackInfo<Value>& args,
                             const BIGNUM* (*get_field)(const DH*),
                             const char* err_if_null) {
  Environment* env = Environment::GetCurrent(args);

  DiffieHellman* dh;
  ASSIGN_OR_RETURN_UNWRAP(&dh, args.Holder());

  const BIGNUM* num = get_field(dh->dh_.get());
  if (num == nullptr) return env->ThrowError(err_if_null);

  const int size = BN_num_bytes(num);
  CHECK_GE(size, 0);
  char* data = Malloc(size);
  CHECK_EQ(size,
           BN_bn2binpad(num, reinterpret_cast<unsigned char*>(data), size));
  args.GetReturnValue().Set(Buffer::New(env, data, size).ToLocalChecked());
}

void DiffieHellman::GetPrime(const FunctionCallbackInfo<Value>& args) {
  GetField(args, [](const DH* dh) -> const BIGNUM* {
    const BIGNUM* p;
    DH_get0_pqg(dh, &p, nullptr, nullptr);
    return p;
  }, "p is null");
}


void DiffieHellman::GetGenerator(const FunctionCallbackInfo<Value>& args) {
  GetField(args, [](const DH* dh) -> const BIGNUM* {
    const BIGNUM* g;
    DH_get0_pqg(dh, nullptr, nullptr, &g);
    return g;
  }, "g is null");
}


void DiffieHellman::GetPublicKey(const FunctionCallbackInfo<Value>& args) {
  GetField(args, [](const DH* dh) -> const BIGNUM* {
    const BIGNUM* pub_key;
    DH_get0_key(dh, &pub_key, nullptr);
    return pub_key;
  }, "No public key - did you forget to generate one?");
}


void DiffieHellman::GetPrivateKey(const FunctionCallbackInfo<Value>& args) {
  GetField(args, [](const DH* dh) -> const BIGNUM* {
    const BIGNUM* priv_key;
    DH_get0_key(dh, nullptr, &priv_key);
    return priv_key;
  }, "No private key - did you forget to generate one?");
}


void DiffieHellman::ComputeSecret(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  DiffieHellman* diffieHellman;
  ASSIGN_OR_RETURN_UNWRAP(&diffieHellman, args.Holder());

  ClearErrorOnReturn clear_error_on_return;

  if (args.Length() == 0) {
    return THROW_ERR_MISSING_ARGS(
        env, "Other party's public key argument is mandatory");
  }

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "Other party's public key");
  BignumPointer key(BN_bin2bn(
      reinterpret_cast<unsigned char*>(Buffer::Data(args[0])),
      Buffer::Length(args[0]),
      0));

  MallocedBuffer<char> data(DH_size(diffieHellman->dh_.get()));

  int size = DH_compute_key(reinterpret_cast<unsigned char*>(data.data),
                            key.get(),
                            diffieHellman->dh_.get());

  if (size == -1) {
    int checkResult;
    int checked;

    checked = DH_check_pub_key(diffieHellman->dh_.get(),
                               key.get(),
                               &checkResult);

    if (!checked) {
      return ThrowCryptoError(env, ERR_get_error(), "Invalid Key");
    } else if (checkResult) {
      if (checkResult & DH_CHECK_PUBKEY_TOO_SMALL) {
        return env->ThrowError("Supplied key is too small");
      } else if (checkResult & DH_CHECK_PUBKEY_TOO_LARGE) {
        return env->ThrowError("Supplied key is too large");
      } else {
        return env->ThrowError("Invalid key");
      }
    } else {
      return env->ThrowError("Invalid key");
    }

    UNREACHABLE();
  }

  CHECK_GE(size, 0);

  // DH_size returns number of bytes in a prime number
  // DH_compute_key returns number of bytes in a remainder of exponent, which
  // may have less bytes than a prime number. Therefore add 0-padding to the
  // allocated buffer.
  if (static_cast<size_t>(size) != data.size) {
    CHECK_GT(data.size, static_cast<size_t>(size));
    memmove(data.data + data.size - size, data.data, size);
    memset(data.data, 0, data.size - size);
  }

  args.GetReturnValue().Set(
      Buffer::New(env->isolate(), data.release(), data.size).ToLocalChecked());
}

void DiffieHellman::SetKey(const v8::FunctionCallbackInfo<Value>& args,
                           int (*set_field)(DH*, BIGNUM*), const char* what) {
  Environment* env = Environment::GetCurrent(args);

  DiffieHellman* dh;
  ASSIGN_OR_RETURN_UNWRAP(&dh, args.Holder());

  char errmsg[64];

  if (args.Length() == 0) {
    snprintf(errmsg, sizeof(errmsg), "%s argument is mandatory", what);
    return THROW_ERR_MISSING_ARGS(env, errmsg);
  }

  if (!Buffer::HasInstance(args[0])) {
    snprintf(errmsg, sizeof(errmsg), "%s must be a buffer", what);
    return THROW_ERR_INVALID_ARG_TYPE(env, errmsg);
  }

  BIGNUM* num =
      BN_bin2bn(reinterpret_cast<unsigned char*>(Buffer::Data(args[0])),
                Buffer::Length(args[0]), nullptr);
  CHECK_NOT_NULL(num);
  CHECK_EQ(1, set_field(dh->dh_.get(), num));
}


void DiffieHellman::SetPublicKey(const FunctionCallbackInfo<Value>& args) {
  SetKey(args,
         [](DH* dh, BIGNUM* num) { return DH_set0_key(dh, num, nullptr); },
         "Public key");
}

void DiffieHellman::SetPrivateKey(const FunctionCallbackInfo<Value>& args) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && \
    OPENSSL_VERSION_NUMBER < 0x10100070L
// Older versions of OpenSSL 1.1.0 have a DH_set0_key which does not work for
// Node. See https://github.com/openssl/openssl/pull/4384.
#error "OpenSSL 1.1.0 revisions before 1.1.0g are not supported"
#endif
  SetKey(args,
         [](DH* dh, BIGNUM* num) { return DH_set0_key(dh, nullptr, num); },
         "Private key");
}


void DiffieHellman::VerifyErrorGetter(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(args.GetIsolate());

  DiffieHellman* diffieHellman;
  ASSIGN_OR_RETURN_UNWRAP(&diffieHellman, args.Holder());

  args.GetReturnValue().Set(diffieHellman->verifyError_);
}


bool DiffieHellman::VerifyContext() {
  int codes;
  if (!DH_check(dh_.get(), &codes))
    return false;
  verifyError_ = codes;
  return true;
}


void ECDH::Initialize(Environment* env, Local<Object> target) {
  HandleScope scope(env->isolate());

  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);

  t->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(t, "generateKeys", GenerateKeys);
  env->SetProtoMethod(t, "computeSecret", ComputeSecret);
  env->SetProtoMethodNoSideEffect(t, "getPublicKey", GetPublicKey);
  env->SetProtoMethodNoSideEffect(t, "getPrivateKey", GetPrivateKey);
  env->SetProtoMethod(t, "setPublicKey", SetPublicKey);
  env->SetProtoMethod(t, "setPrivateKey", SetPrivateKey);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "ECDH"),
              t->GetFunction(env->context()).ToLocalChecked());
}


void ECDH::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  MarkPopErrorOnReturn mark_pop_error_on_return;

  // TODO(indutny): Support raw curves?
  CHECK(args[0]->IsString());
  node::Utf8Value curve(env->isolate(), args[0]);

  int nid = OBJ_sn2nid(*curve);
  if (nid == NID_undef)
    return THROW_ERR_INVALID_ARG_VALUE(env,
        "First argument should be a valid curve name");

  ECKeyPointer key(EC_KEY_new_by_curve_name(nid));
  if (!key)
    return env->ThrowError("Failed to create EC_KEY using curve name");

  new ECDH(env, args.This(), std::move(key));
}


void ECDH::GenerateKeys(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  ECDH* ecdh;
  ASSIGN_OR_RETURN_UNWRAP(&ecdh, args.Holder());

  if (!EC_KEY_generate_key(ecdh->key_.get()))
    return env->ThrowError("Failed to generate EC_KEY");
}


ECPointPointer ECDH::BufferToPoint(Environment* env,
                                   const EC_GROUP* group,
                                   char* data,
                                   size_t len) {
  int r;

  ECPointPointer pub(EC_POINT_new(group));
  if (!pub) {
    env->ThrowError("Failed to allocate EC_POINT for a public key");
    return pub;
  }

  r = EC_POINT_oct2point(
      group,
      pub.get(),
      reinterpret_cast<unsigned char*>(data),
      len,
      nullptr);
  if (!r)
    return ECPointPointer();

  return pub;
}


void ECDH::ComputeSecret(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "Data");

  ECDH* ecdh;
  ASSIGN_OR_RETURN_UNWRAP(&ecdh, args.Holder());

  MarkPopErrorOnReturn mark_pop_error_on_return;

  if (!ecdh->IsKeyPairValid())
    return env->ThrowError("Invalid key pair");

  ECPointPointer pub(
      ECDH::BufferToPoint(env,
                          ecdh->group_,
                          Buffer::Data(args[0]),
                          Buffer::Length(args[0])));
  if (!pub) {
    args.GetReturnValue().Set(
        FIXED_ONE_BYTE_STRING(env->isolate(),
        "ERR_CRYPTO_ECDH_INVALID_PUBLIC_KEY"));
    return;
  }

  // NOTE: field_size is in bits
  int field_size = EC_GROUP_get_degree(ecdh->group_);
  size_t out_len = (field_size + 7) / 8;
  char* out = node::Malloc(out_len);

  int r = ECDH_compute_key(out, out_len, pub.get(), ecdh->key_.get(), nullptr);
  if (!r) {
    free(out);
    return env->ThrowError("Failed to compute ECDH key");
  }

  Local<Object> buf = Buffer::New(env, out, out_len).ToLocalChecked();
  args.GetReturnValue().Set(buf);
}


void ECDH::GetPublicKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  // Conversion form
  CHECK_EQ(args.Length(), 1);

  ECDH* ecdh;
  ASSIGN_OR_RETURN_UNWRAP(&ecdh, args.Holder());

  const EC_POINT* pub = EC_KEY_get0_public_key(ecdh->key_.get());
  if (pub == nullptr)
    return env->ThrowError("Failed to get ECDH public key");

  int size;
  CHECK(args[0]->IsUint32());
  uint32_t val = args[0].As<Uint32>()->Value();
  point_conversion_form_t form = static_cast<point_conversion_form_t>(val);

  size = EC_POINT_point2oct(ecdh->group_, pub, form, nullptr, 0, nullptr);
  if (size == 0)
    return env->ThrowError("Failed to get public key length");

  unsigned char* out = node::Malloc<unsigned char>(size);

  int r = EC_POINT_point2oct(ecdh->group_, pub, form, out, size, nullptr);
  if (r != size) {
    free(out);
    return env->ThrowError("Failed to get public key");
  }

  Local<Object> buf =
      Buffer::New(env, reinterpret_cast<char*>(out), size).ToLocalChecked();
  args.GetReturnValue().Set(buf);
}


void ECDH::GetPrivateKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  ECDH* ecdh;
  ASSIGN_OR_RETURN_UNWRAP(&ecdh, args.Holder());

  const BIGNUM* b = EC_KEY_get0_private_key(ecdh->key_.get());
  if (b == nullptr)
    return env->ThrowError("Failed to get ECDH private key");

  const int size = BN_num_bytes(b);
  unsigned char* out = node::Malloc<unsigned char>(size);
  CHECK_EQ(size, BN_bn2binpad(b, out, size));

  Local<Object> buf =
      Buffer::New(env, reinterpret_cast<char*>(out), size).ToLocalChecked();
  args.GetReturnValue().Set(buf);
}


void ECDH::SetPrivateKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  ECDH* ecdh;
  ASSIGN_OR_RETURN_UNWRAP(&ecdh, args.Holder());

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "Private key");

  BignumPointer priv(BN_bin2bn(
      reinterpret_cast<unsigned char*>(Buffer::Data(args[0].As<Object>())),
      Buffer::Length(args[0].As<Object>()),
      nullptr));
  if (!priv)
    return env->ThrowError("Failed to convert Buffer to BN");

  if (!ecdh->IsKeyValidForCurve(priv)) {
    return env->ThrowError("Private key is not valid for specified curve.");
  }

  int result = EC_KEY_set_private_key(ecdh->key_.get(), priv.get());
  priv.reset();

  if (!result) {
    return env->ThrowError("Failed to convert BN to a private key");
  }

  // To avoid inconsistency, clear the current public key in-case computing
  // the new one fails for some reason.
  EC_KEY_set_public_key(ecdh->key_.get(), nullptr);

  MarkPopErrorOnReturn mark_pop_error_on_return;
  USE(&mark_pop_error_on_return);

  const BIGNUM* priv_key = EC_KEY_get0_private_key(ecdh->key_.get());
  CHECK_NOT_NULL(priv_key);

  ECPointPointer pub(EC_POINT_new(ecdh->group_));
  CHECK(pub);

  if (!EC_POINT_mul(ecdh->group_, pub.get(), priv_key,
                    nullptr, nullptr, nullptr)) {
    return env->ThrowError("Failed to generate ECDH public key");
  }

  if (!EC_KEY_set_public_key(ecdh->key_.get(), pub.get()))
    return env->ThrowError("Failed to set generated public key");
}


void ECDH::SetPublicKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  ECDH* ecdh;
  ASSIGN_OR_RETURN_UNWRAP(&ecdh, args.Holder());

  THROW_AND_RETURN_IF_NOT_BUFFER(env, args[0], "Public key");

  MarkPopErrorOnReturn mark_pop_error_on_return;

  ECPointPointer pub(
      ECDH::BufferToPoint(env,
                          ecdh->group_,
                          Buffer::Data(args[0].As<Object>()),
                          Buffer::Length(args[0].As<Object>())));
  if (!pub)
    return env->ThrowError("Failed to convert Buffer to EC_POINT");

  int r = EC_KEY_set_public_key(ecdh->key_.get(), pub.get());
  if (!r)
    return env->ThrowError("Failed to set EC_POINT as the public key");
}


bool ECDH::IsKeyValidForCurve(const BignumPointer& private_key) {
  CHECK(group_);
  CHECK(private_key);
  // Private keys must be in the range [1, n-1].
  // Ref: Section 3.2.1 - http://www.secg.org/sec1-v2.pdf
  if (BN_cmp(private_key.get(), BN_value_one()) < 0) {
    return false;
  }
  BignumPointer order(BN_new());
  CHECK(order);
  return EC_GROUP_get_order(group_, order.get(), nullptr) &&
         BN_cmp(private_key.get(), order.get()) < 0;
}


bool ECDH::IsKeyPairValid() {
  MarkPopErrorOnReturn mark_pop_error_on_return;
  USE(&mark_pop_error_on_return);
  return 1 == EC_KEY_check_key(key_.get());
}


// TODO(addaleax): If there is an `AsyncWrap`, it currently has no access to
// this object. This makes proper reporting of memory usage impossible.
struct CryptoJob : public ThreadPoolWork {
  Environment* const env;
  std::unique_ptr<AsyncWrap> async_wrap;
  inline explicit CryptoJob(Environment* env) : ThreadPoolWork(env), env(env) {}
  inline void AfterThreadPoolWork(int status) final;
  virtual void AfterThreadPoolWork() = 0;
  static inline void Run(std::unique_ptr<CryptoJob> job, Local<Value> wrap);
};


void CryptoJob::AfterThreadPoolWork(int status) {
  CHECK(status == 0 || status == UV_ECANCELED);
  std::unique_ptr<CryptoJob> job(this);
  if (status == UV_ECANCELED) return;
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  CHECK_EQ(false, async_wrap->persistent().IsWeak());
  AfterThreadPoolWork();
}


void CryptoJob::Run(std::unique_ptr<CryptoJob> job, Local<Value> wrap) {
  CHECK(wrap->IsObject());
  CHECK_EQ(nullptr, job->async_wrap);
  job->async_wrap.reset(Unwrap<AsyncWrap>(wrap.As<Object>()));
  CHECK_EQ(false, job->async_wrap->persistent().IsWeak());
  job->ScheduleWork();
  job.release();  // Run free, little job!
}


inline void CopyBuffer(Local<Value> buf, std::vector<char>* vec) {
  vec->clear();
  if (auto p = Buffer::Data(buf)) vec->assign(p, p + Buffer::Length(buf));
}


struct RandomBytesJob : public CryptoJob {
  unsigned char* data;
  size_t size;
  CryptoErrorVector errors;
  Maybe<int> rc;

  inline explicit RandomBytesJob(Environment* env)
      : CryptoJob(env), rc(Nothing<int>()) {}

  inline void DoThreadPoolWork() override {
    CheckEntropy();  // Ensure that OpenSSL's PRNG is properly seeded.
    rc = Just(RAND_bytes(data, size));
    if (0 == rc.FromJust()) errors.Capture();
  }

  inline void AfterThreadPoolWork() override {
    Local<Value> arg = ToResult();
    async_wrap->MakeCallback(env->ondone_string(), 1, &arg);
  }

  inline Local<Value> ToResult() const {
    if (errors.empty()) return Undefined(env->isolate());
    return errors.ToException(env);
  }
};


void RandomBytes(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsArrayBufferView());  // buffer; wrap object retains ref.
  CHECK(args[1]->IsUint32());  // offset
  CHECK(args[2]->IsUint32());  // size
  CHECK(args[3]->IsObject() || args[3]->IsUndefined());  // wrap object
  const uint32_t offset = args[1].As<Uint32>()->Value();
  const uint32_t size = args[2].As<Uint32>()->Value();
  CHECK_GE(offset + size, offset);  // Overflow check.
  CHECK_LE(offset + size, Buffer::Length(args[0]));  // Bounds check.
  Environment* env = Environment::GetCurrent(args);
  std::unique_ptr<RandomBytesJob> job(new RandomBytesJob(env));
  job->data = reinterpret_cast<unsigned char*>(Buffer::Data(args[0])) + offset;
  job->size = size;
  if (args[3]->IsObject()) return RandomBytesJob::Run(std::move(job), args[3]);
  env->PrintSyncTrace();
  job->DoThreadPoolWork();
  args.GetReturnValue().Set(job->ToResult());
}


struct PBKDF2Job : public CryptoJob {
  unsigned char* keybuf_data;
  size_t keybuf_size;
  std::vector<char> pass;
  std::vector<char> salt;
  uint32_t iteration_count;
  const EVP_MD* digest;
  Maybe<bool> success;

  inline explicit PBKDF2Job(Environment* env)
      : CryptoJob(env), success(Nothing<bool>()) {}

  inline ~PBKDF2Job() override {
    Cleanse();
  }

  inline void DoThreadPoolWork() override {
    auto salt_data = reinterpret_cast<const unsigned char*>(salt.data());
    const bool ok =
        PKCS5_PBKDF2_HMAC(pass.data(), pass.size(), salt_data, salt.size(),
                          iteration_count, digest, keybuf_size, keybuf_data);
    success = Just(ok);
    Cleanse();
  }

  inline void AfterThreadPoolWork() override {
    Local<Value> arg = ToResult();
    async_wrap->MakeCallback(env->ondone_string(), 1, &arg);
  }

  inline Local<Value> ToResult() const {
    return Boolean::New(env->isolate(), success.FromJust());
  }

  inline void Cleanse() {
    OPENSSL_cleanse(pass.data(), pass.size());
    OPENSSL_cleanse(salt.data(), salt.size());
    pass.clear();
    salt.clear();
  }
};


inline void PBKDF2(const FunctionCallbackInfo<Value>& args) {
  auto rv = args.GetReturnValue();
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsArrayBufferView());  // keybuf; wrap object retains ref.
  CHECK(args[1]->IsArrayBufferView());  // pass
  CHECK(args[2]->IsArrayBufferView());  // salt
  CHECK(args[3]->IsUint32());  // iteration_count
  CHECK(args[4]->IsString());  // digest_name
  CHECK(args[5]->IsObject() || args[5]->IsUndefined());  // wrap object
  std::unique_ptr<PBKDF2Job> job(new PBKDF2Job(env));
  job->keybuf_data = reinterpret_cast<unsigned char*>(Buffer::Data(args[0]));
  job->keybuf_size = Buffer::Length(args[0]);
  CopyBuffer(args[1], &job->pass);
  CopyBuffer(args[2], &job->salt);
  job->iteration_count = args[3].As<Uint32>()->Value();
  Utf8Value digest_name(args.GetIsolate(), args[4]);
  job->digest = EVP_get_digestbyname(*digest_name);
  if (job->digest == nullptr) return rv.Set(-1);
  if (args[5]->IsObject()) return PBKDF2Job::Run(std::move(job), args[5]);
  env->PrintSyncTrace();
  job->DoThreadPoolWork();
  rv.Set(job->ToResult());
}


#ifndef OPENSSL_NO_SCRYPT
struct ScryptJob : public CryptoJob {
  unsigned char* keybuf_data;
  size_t keybuf_size;
  std::vector<char> pass;
  std::vector<char> salt;
  uint32_t N;
  uint32_t r;
  uint32_t p;
  uint64_t maxmem;
  CryptoErrorVector errors;

  inline explicit ScryptJob(Environment* env) : CryptoJob(env) {}

  inline ~ScryptJob() override {
    Cleanse();
  }

  inline bool Validate() {
    if (1 == EVP_PBE_scrypt(nullptr, 0, nullptr, 0, N, r, p, maxmem,
                            nullptr, 0)) {
      return true;
    } else {
      // Note: EVP_PBE_scrypt() does not always put errors on the error stack.
      errors.Capture();
      return false;
    }
  }

  inline void DoThreadPoolWork() override {
    auto salt_data = reinterpret_cast<const unsigned char*>(salt.data());
    if (1 != EVP_PBE_scrypt(pass.data(), pass.size(), salt_data, salt.size(),
                            N, r, p, maxmem, keybuf_data, keybuf_size)) {
      errors.Capture();
    }
  }

  inline void AfterThreadPoolWork() override {
    Local<Value> arg = ToResult();
    async_wrap->MakeCallback(env->ondone_string(), 1, &arg);
  }

  inline Local<Value> ToResult() const {
    if (errors.empty()) return Undefined(env->isolate());
    return errors.ToException(env);
  }

  inline void Cleanse() {
    OPENSSL_cleanse(pass.data(), pass.size());
    OPENSSL_cleanse(salt.data(), salt.size());
    pass.clear();
    salt.clear();
  }
};


void Scrypt(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsArrayBufferView());  // keybuf; wrap object retains ref.
  CHECK(args[1]->IsArrayBufferView());  // pass
  CHECK(args[2]->IsArrayBufferView());  // salt
  CHECK(args[3]->IsUint32());  // N
  CHECK(args[4]->IsUint32());  // r
  CHECK(args[5]->IsUint32());  // p
  CHECK(args[6]->IsNumber());  // maxmem
  CHECK(args[7]->IsObject() || args[7]->IsUndefined());  // wrap object
  std::unique_ptr<ScryptJob> job(new ScryptJob(env));
  job->keybuf_data = reinterpret_cast<unsigned char*>(Buffer::Data(args[0]));
  job->keybuf_size = Buffer::Length(args[0]);
  CopyBuffer(args[1], &job->pass);
  CopyBuffer(args[2], &job->salt);
  job->N = args[3].As<Uint32>()->Value();
  job->r = args[4].As<Uint32>()->Value();
  job->p = args[5].As<Uint32>()->Value();
  Local<Context> ctx = env->isolate()->GetCurrentContext();
  job->maxmem = static_cast<uint64_t>(args[6]->IntegerValue(ctx).ToChecked());
  if (!job->Validate()) {
    // EVP_PBE_scrypt() does not always put errors on the error stack
    // and therefore ToResult() may or may not return an exception
    // object.  Return a sentinel value to inform JS land it should
    // throw an ERR_CRYPTO_SCRYPT_PARAMETER_ERROR on our behalf.
    auto result = job->ToResult();
    if (result->IsUndefined()) result = Null(args.GetIsolate());
    return args.GetReturnValue().Set(result);
  }
  if (args[7]->IsObject()) return ScryptJob::Run(std::move(job), args[7]);
  env->PrintSyncTrace();
  job->DoThreadPoolWork();
  args.GetReturnValue().Set(job->ToResult());
}
#endif  // OPENSSL_NO_SCRYPT


class KeyPairGenerationConfig {
 public:
  virtual EVPKeyCtxPointer Setup() = 0;
  virtual bool Configure(const EVPKeyCtxPointer& ctx) {
    return true;
  }
  virtual ~KeyPairGenerationConfig() {}
};

class RSAKeyPairGenerationConfig : public KeyPairGenerationConfig {
 public:
  RSAKeyPairGenerationConfig(unsigned int modulus_bits, unsigned int exponent)
    : modulus_bits_(modulus_bits), exponent_(exponent) {}

  EVPKeyCtxPointer Setup() override {
    return EVPKeyCtxPointer(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
  }

  bool Configure(const EVPKeyCtxPointer& ctx) override {
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), modulus_bits_) <= 0)
      return false;

    // 0x10001 is the default RSA exponent.
    if (exponent_ != 0x10001) {
      BignumPointer bn(BN_new());
      CHECK_NOT_NULL(bn.get());
      CHECK(BN_set_word(bn.get(), exponent_));
      if (EVP_PKEY_CTX_set_rsa_keygen_pubexp(ctx.get(), bn.get()) <= 0)
        return false;
    }

    return true;
  }

 private:
  const unsigned int modulus_bits_;
  const unsigned int exponent_;
};

class DSAKeyPairGenerationConfig : public KeyPairGenerationConfig {
 public:
  DSAKeyPairGenerationConfig(unsigned int modulus_bits, int divisor_bits)
    : modulus_bits_(modulus_bits), divisor_bits_(divisor_bits) {}

  EVPKeyCtxPointer Setup() override {
    EVPKeyCtxPointer param_ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_DSA, nullptr));
    if (!param_ctx)
      return nullptr;

    if (EVP_PKEY_paramgen_init(param_ctx.get()) <= 0)
      return nullptr;

    if (EVP_PKEY_CTX_set_dsa_paramgen_bits(param_ctx.get(), modulus_bits_) <= 0)
      return nullptr;

    if (divisor_bits_ != -1) {
      if (EVP_PKEY_CTX_ctrl(param_ctx.get(), EVP_PKEY_DSA, EVP_PKEY_OP_PARAMGEN,
                            EVP_PKEY_CTRL_DSA_PARAMGEN_Q_BITS, divisor_bits_,
                            nullptr) <= 0) {
        return nullptr;
      }
    }

    EVP_PKEY* params = nullptr;
    if (EVP_PKEY_paramgen(param_ctx.get(), &params) <= 0)
      return nullptr;
    param_ctx.reset();

    EVPKeyCtxPointer key_ctx(EVP_PKEY_CTX_new(params, nullptr));
    EVP_PKEY_free(params);
    return key_ctx;
  }

 private:
  const unsigned int modulus_bits_;
  const int divisor_bits_;
};

class ECKeyPairGenerationConfig : public KeyPairGenerationConfig {
 public:
  ECKeyPairGenerationConfig(int curve_nid, int param_encoding)
    : curve_nid_(curve_nid), param_encoding_(param_encoding) {}

  EVPKeyCtxPointer Setup() override {
    EVPKeyCtxPointer param_ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr));
    if (!param_ctx)
      return nullptr;

    if (EVP_PKEY_paramgen_init(param_ctx.get()) <= 0)
      return nullptr;

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(param_ctx.get(),
                                               curve_nid_) <= 0)
      return nullptr;

    if (EVP_PKEY_CTX_set_ec_param_enc(param_ctx.get(), param_encoding_) <= 0)
      return nullptr;

    EVP_PKEY* params = nullptr;
    if (EVP_PKEY_paramgen(param_ctx.get(), &params) <= 0)
      return nullptr;
    param_ctx.reset();

    EVPKeyCtxPointer key_ctx(EVP_PKEY_CTX_new(params, nullptr));
    EVP_PKEY_free(params);
    return key_ctx;
  }

 private:
  const int curve_nid_;
  const int param_encoding_;
};

enum PKEncodingType {
  // RSAPublicKey / RSAPrivateKey according to PKCS#1.
  PK_ENCODING_PKCS1,
  // PrivateKeyInfo or EncryptedPrivateKeyInfo according to PKCS#8.
  PK_ENCODING_PKCS8,
  // SubjectPublicKeyInfo according to X.509.
  PK_ENCODING_SPKI,
  // ECPrivateKey according to SEC1.
  PK_ENCODING_SEC1
};

enum PKFormatType {
  PK_FORMAT_DER,
  PK_FORMAT_PEM
};

struct KeyPairEncodingConfig {
  PKEncodingType type_;
  PKFormatType format_;
};

typedef KeyPairEncodingConfig PublicKeyEncodingConfig;

struct PrivateKeyEncodingConfig : public KeyPairEncodingConfig {
  const EVP_CIPHER* cipher_;
  // This char* will be passed to OPENSSL_clear_free.
  std::shared_ptr<char> passphrase_;
  unsigned int passphrase_length_;
};

class GenerateKeyPairJob : public CryptoJob {
 public:
  GenerateKeyPairJob(Environment* env,
                     std::unique_ptr<KeyPairGenerationConfig> config,
                     PublicKeyEncodingConfig public_key_encoding,
                     PrivateKeyEncodingConfig private_key_encoding)
    : CryptoJob(env),
    config_(std::move(config)),
    public_key_encoding_(public_key_encoding),
    private_key_encoding_(private_key_encoding),
    pkey_(nullptr) {}

  inline void DoThreadPoolWork() override {
    if (!GenerateKey())
      errors_.Capture();
  }

  inline bool GenerateKey() {
    // Make sure that the CSPRNG is properly seeded so the results are secure.
    CheckEntropy();

    // Create the key generation context.
    EVPKeyCtxPointer ctx = config_->Setup();
    if (!ctx)
      return false;

    // Initialize key generation.
    if (EVP_PKEY_keygen_init(ctx.get()) <= 0)
      return false;

    // Configure key generation.
    if (!config_->Configure(ctx))
      return false;

    // Generate the key.
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx.get(), &pkey) != 1)
      return false;
    pkey_.reset(pkey);
    return true;
  }

  inline void AfterThreadPoolWork() override {
    Local<Value> args[3];
    ToResult(&args[0], &args[1], &args[2]);
    async_wrap->MakeCallback(env->ondone_string(), 3, args);
  }

  inline void ToResult(Local<Value>* err,
                       Local<Value>* pubkey,
                       Local<Value>* privkey) {
    if (pkey_ && EncodeKeys(pubkey, privkey)) {
      CHECK(errors_.empty());
      *err = Undefined(env->isolate());
    } else {
      if (errors_.empty())
        errors_.Capture();
      CHECK(!errors_.empty());
      *err = errors_.ToException(env);
      *pubkey = Undefined(env->isolate());
      *privkey = Undefined(env->isolate());
    }
  }

  inline bool EncodeKeys(Local<Value>* pubkey, Local<Value>* privkey) {
    EVP_PKEY* pkey = pkey_.get();
    BIOPointer bio(BIO_new(BIO_s_mem()));
    CHECK(bio);

    // Encode the public key.
    if (public_key_encoding_.type_ == PK_ENCODING_PKCS1) {
      // PKCS#1 is only valid for RSA keys.
      CHECK_EQ(EVP_PKEY_id(pkey), EVP_PKEY_RSA);
      RSAPointer rsa(EVP_PKEY_get1_RSA(pkey));
      if (public_key_encoding_.format_ == PK_FORMAT_PEM) {
        // Encode PKCS#1 as PEM.
        if (PEM_write_bio_RSAPublicKey(bio.get(), rsa.get()) != 1)
          return false;
      } else {
        // Encode PKCS#1 as DER.
        CHECK_EQ(public_key_encoding_.format_, PK_FORMAT_DER);
        if (i2d_RSAPublicKey_bio(bio.get(), rsa.get()) != 1)
          return false;
      }
    } else {
      CHECK_EQ(public_key_encoding_.type_, PK_ENCODING_SPKI);
      if (public_key_encoding_.format_ == PK_FORMAT_PEM) {
        // Encode SPKI as PEM.
        if (PEM_write_bio_PUBKEY(bio.get(), pkey) != 1)
          return false;
      } else {
        // Encode SPKI as DER.
        CHECK_EQ(public_key_encoding_.format_, PK_FORMAT_DER);
        if (i2d_PUBKEY_bio(bio.get(), pkey) != 1)
          return false;
      }
    }

    // Convert the contents of the BIO to a JavaScript object.
    BIOToStringOrBuffer(bio.get(), public_key_encoding_.format_, pubkey);
    USE(BIO_reset(bio.get()));

    // Now do the same for the private key (which is a bit more difficult).
    if (private_key_encoding_.type_ == PK_ENCODING_PKCS1) {
      // PKCS#1 is only permitted for RSA keys.
      CHECK_EQ(EVP_PKEY_id(pkey), EVP_PKEY_RSA);

      RSAPointer rsa(EVP_PKEY_get1_RSA(pkey));
      if (private_key_encoding_.format_ == PK_FORMAT_PEM) {
        // Encode PKCS#1 as PEM.
        char* pass = private_key_encoding_.passphrase_.get();
        if (PEM_write_bio_RSAPrivateKey(
                bio.get(), rsa.get(),
                private_key_encoding_.cipher_,
                reinterpret_cast<unsigned char*>(pass),
                private_key_encoding_.passphrase_length_,
                nullptr, nullptr) != 1)
          return false;
      } else {
        // Encode PKCS#1 as DER. This does not permit encryption.
        CHECK_EQ(private_key_encoding_.format_, PK_FORMAT_DER);
        CHECK_NULL(private_key_encoding_.cipher_);
        if (i2d_RSAPrivateKey_bio(bio.get(), rsa.get()) != 1)
          return false;
      }
    } else if (private_key_encoding_.type_ == PK_ENCODING_PKCS8) {
      if (private_key_encoding_.format_ == PK_FORMAT_PEM) {
        // Encode PKCS#8 as PEM.
        if (PEM_write_bio_PKCS8PrivateKey(
                bio.get(), pkey,
                private_key_encoding_.cipher_,
                private_key_encoding_.passphrase_.get(),
                private_key_encoding_.passphrase_length_,
                nullptr, nullptr) != 1)
          return false;
      } else {
        // Encode PKCS#8 as DER.
        CHECK_EQ(private_key_encoding_.format_, PK_FORMAT_DER);
        if (i2d_PKCS8PrivateKey_bio(
                bio.get(), pkey,
                private_key_encoding_.cipher_,
                private_key_encoding_.passphrase_.get(),
                private_key_encoding_.passphrase_length_,
                nullptr, nullptr) != 1)
          return false;
      }
    } else {
      CHECK_EQ(private_key_encoding_.type_, PK_ENCODING_SEC1);

      // SEC1 is only permitted for EC keys.
      CHECK_EQ(EVP_PKEY_id(pkey), EVP_PKEY_EC);

      ECKeyPointer ec_key(EVP_PKEY_get1_EC_KEY(pkey));
      if (private_key_encoding_.format_ == PK_FORMAT_PEM) {
        // Encode SEC1 as PEM.
        char* pass = private_key_encoding_.passphrase_.get();
        if (PEM_write_bio_ECPrivateKey(
                bio.get(), ec_key.get(),
                private_key_encoding_.cipher_,
                reinterpret_cast<unsigned char*>(pass),
                private_key_encoding_.passphrase_length_,
                nullptr, nullptr) != 1)
          return false;
      } else {
        // Encode SEC1 as DER. This does not permit encryption.
        CHECK_EQ(private_key_encoding_.format_, PK_FORMAT_DER);
        CHECK_NULL(private_key_encoding_.cipher_);
        if (i2d_ECPrivateKey_bio(bio.get(), ec_key.get()) != 1)
          return false;
      }
    }

    BIOToStringOrBuffer(bio.get(), private_key_encoding_.format_, privkey);
    return true;
  }

  inline void BIOToStringOrBuffer(BIO* bio, PKFormatType format,
                                  Local<Value>* out) const {
    BUF_MEM* bptr;
    BIO_get_mem_ptr(bio, &bptr);
    if (format == PK_FORMAT_PEM) {
      // PEM is an ASCII format, so we will return it as a string.
      *out = String::NewFromUtf8(env->isolate(), bptr->data,
                                 NewStringType::kNormal,
                                 bptr->length).ToLocalChecked();
    } else {
      CHECK_EQ(format, PK_FORMAT_DER);
      // DER is binary, return it as a buffer.
      *out = Buffer::Copy(env, bptr->data, bptr->length).ToLocalChecked();
    }
  }

 private:
  CryptoErrorVector errors_;
  std::unique_ptr<KeyPairGenerationConfig> config_;
  PublicKeyEncodingConfig public_key_encoding_;
  PrivateKeyEncodingConfig private_key_encoding_;
  EVPKeyPointer pkey_;
};

void GenerateKeyPair(const FunctionCallbackInfo<Value>& args,
                     unsigned int n_opts,
                     std::unique_ptr<KeyPairGenerationConfig> config) {
  Environment* env = Environment::GetCurrent(args);
  PublicKeyEncodingConfig public_key_encoding;
  PrivateKeyEncodingConfig private_key_encoding;

  // Public key encoding: type (int) + pem (bool)
  CHECK(args[n_opts]->IsInt32());
  public_key_encoding.type_ = static_cast<PKEncodingType>(
      args[n_opts].As<Int32>()->Value());
  CHECK(args[n_opts + 1]->IsInt32());
  public_key_encoding.format_ = static_cast<PKFormatType>(
      args[n_opts + 1].As<Int32>()->Value());

  // Private key encoding: type (int) + pem (bool) + cipher (optional, string) +
  //                       passphrase (optional, string)
  CHECK(args[n_opts + 2]->IsInt32());
  private_key_encoding.type_ = static_cast<PKEncodingType>(
      args[n_opts + 2].As<Int32>()->Value());
  CHECK(args[n_opts + 1]->IsInt32());
  private_key_encoding.format_ = static_cast<PKFormatType>(
      args[n_opts + 3].As<Int32>()->Value());
  if (args[n_opts + 4]->IsString()) {
    String::Utf8Value cipher_name(env->isolate(),
                                  args[n_opts + 4].As<String>());
    private_key_encoding.cipher_ = EVP_get_cipherbyname(*cipher_name);
    if (private_key_encoding.cipher_ == nullptr)
      return env->ThrowError("Unknown cipher");

    // We need to take ownership of the string and want to avoid creating an
    // unnecessary copy in memory, that's why we are not using String::Utf8Value
    // here.
    CHECK(args[n_opts + 5]->IsString());
    Local<String> passphrase = args[n_opts + 5].As<String>();
    int len = passphrase->Utf8Length(env->isolate());
    private_key_encoding.passphrase_length_ = len;
    void* mem = OPENSSL_malloc(private_key_encoding.passphrase_length_ + 1);
    CHECK_NOT_NULL(mem);
    private_key_encoding.passphrase_.reset(static_cast<char*>(mem),
        [len](char* p) {
          OPENSSL_clear_free(p, len);
        });
    passphrase->WriteUtf8(env->isolate(),
                          private_key_encoding.passphrase_.get());
  } else {
    CHECK(args[n_opts + 5]->IsNullOrUndefined());
    private_key_encoding.cipher_ = nullptr;
    private_key_encoding.passphrase_length_ = 0;
  }

  std::unique_ptr<GenerateKeyPairJob> job(
      new GenerateKeyPairJob(env, std::move(config), public_key_encoding,
                             private_key_encoding));
  if (args[n_opts + 6]->IsObject())
    return GenerateKeyPairJob::Run(std::move(job), args[n_opts + 6]);
  env->PrintSyncTrace();
  job->DoThreadPoolWork();
  Local<Value> err, pubkey, privkey;
  job->ToResult(&err, &pubkey, &privkey);

  bool (*IsNotTrue)(Maybe<bool>) = [](Maybe<bool> maybe) {
    return maybe.IsNothing() || !maybe.ToChecked();
  };
  Local<Array> ret = Array::New(env->isolate(), 3);
  if (IsNotTrue(ret->Set(env->context(), 0, err)) ||
      IsNotTrue(ret->Set(env->context(), 1, pubkey)) ||
      IsNotTrue(ret->Set(env->context(), 2, privkey)))
    return;
  args.GetReturnValue().Set(ret);
}

void GenerateKeyPairRSA(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsUint32());
  const uint32_t modulus_bits = args[0].As<Uint32>()->Value();
  CHECK(args[1]->IsUint32());
  const uint32_t exponent = args[1].As<Uint32>()->Value();
  std::unique_ptr<KeyPairGenerationConfig> config(
      new RSAKeyPairGenerationConfig(modulus_bits, exponent));
  GenerateKeyPair(args, 2, std::move(config));
}

void GenerateKeyPairDSA(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsUint32());
  const uint32_t modulus_bits = args[0].As<Uint32>()->Value();
  CHECK(args[1]->IsInt32());
  const int32_t divisor_bits = args[1].As<Int32>()->Value();
  std::unique_ptr<KeyPairGenerationConfig> config(
      new DSAKeyPairGenerationConfig(modulus_bits, divisor_bits));
  GenerateKeyPair(args, 2, std::move(config));
}

void GenerateKeyPairEC(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  String::Utf8Value curve_name(args.GetIsolate(), args[0].As<String>());
  int curve_nid = EC_curve_nist2nid(*curve_name);
  if (curve_nid == NID_undef)
    curve_nid = OBJ_sn2nid(*curve_name);
  // TODO(tniessen): Should we also support OBJ_ln2nid? (Other APIs don't.)
  if (curve_nid == NID_undef) {
    Environment* env = Environment::GetCurrent(args);
    return env->ThrowTypeError("Invalid ECDH curve name");
  }
  CHECK(args[1]->IsUint32());
  const uint32_t param_encoding = args[1].As<Int32>()->Value();
  CHECK(param_encoding == OPENSSL_EC_NAMED_CURVE ||
        param_encoding == OPENSSL_EC_EXPLICIT_CURVE);
  std::unique_ptr<KeyPairGenerationConfig> config(
      new ECKeyPairGenerationConfig(curve_nid, param_encoding));
  GenerateKeyPair(args, 2, std::move(config));
}


void GetSSLCiphers(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SSLCtxPointer ctx(SSL_CTX_new(TLS_method()));
  CHECK(ctx);

  SSLPointer ssl(SSL_new(ctx.get()));
  CHECK(ssl);

  STACK_OF(SSL_CIPHER)* ciphers = SSL_get_ciphers(ssl.get());
  int n = sk_SSL_CIPHER_num(ciphers);
  Local<Array> arr = Array::New(env->isolate(), n);

  for (int i = 0; i < n; ++i) {
    const SSL_CIPHER* cipher = sk_SSL_CIPHER_value(ciphers, i);
    arr->Set(env->context(),
             i,
             OneByteString(args.GetIsolate(),
                           SSL_CIPHER_get_name(cipher))).FromJust();
  }

  args.GetReturnValue().Set(arr);
}


class CipherPushContext {
 public:
  explicit CipherPushContext(Environment* env)
      : arr(Array::New(env->isolate())),
        env_(env) {
  }

  inline Environment* env() const { return env_; }

  Local<Array> arr;

 private:
  Environment* env_;
};


template <class TypeName>
static void array_push_back(const TypeName* md,
                            const char* from,
                            const char* to,
                            void* arg) {
  CipherPushContext* ctx = static_cast<CipherPushContext*>(arg);
  ctx->arr->Set(ctx->arr->Length(), OneByteString(ctx->env()->isolate(), from));
}


void GetCiphers(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CipherPushContext ctx(env);
  EVP_CIPHER_do_all_sorted(array_push_back<EVP_CIPHER>, &ctx);
  args.GetReturnValue().Set(ctx.arr);
}


void GetHashes(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CipherPushContext ctx(env);
  EVP_MD_do_all_sorted(array_push_back<EVP_MD>, &ctx);
  args.GetReturnValue().Set(ctx.arr);
}


void GetCurves(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  const size_t num_curves = EC_get_builtin_curves(nullptr, 0);
  Local<Array> arr = Array::New(env->isolate(), num_curves);

  if (num_curves) {
    std::vector<EC_builtin_curve> curves(num_curves);

    if (EC_get_builtin_curves(curves.data(), num_curves)) {
      for (size_t i = 0; i < num_curves; i++) {
        arr->Set(env->context(),
                 i,
                 OneByteString(env->isolate(),
                               OBJ_nid2sn(curves[i].nid))).FromJust();
      }
    }
  }

  args.GetReturnValue().Set(arr);
}


bool VerifySpkac(const char* data, unsigned int len) {
  NetscapeSPKIPointer spki(NETSCAPE_SPKI_b64_decode(data, len));
  if (!spki)
    return false;

  EVPKeyPointer pkey(X509_PUBKEY_get(spki->spkac->pubkey));
  if (!pkey)
    return false;

  return NETSCAPE_SPKI_verify(spki.get(), pkey.get()) > 0;
}


void VerifySpkac(const FunctionCallbackInfo<Value>& args) {
  bool verify_result = false;

  size_t length = Buffer::Length(args[0]);
  if (length == 0)
    return args.GetReturnValue().Set(verify_result);

  char* data = Buffer::Data(args[0]);
  CHECK_NOT_NULL(data);

  verify_result = VerifySpkac(data, length);

  args.GetReturnValue().Set(verify_result);
}


char* ExportPublicKey(const char* data, int len, size_t* size) {
  char* buf = nullptr;

  BIOPointer bio(BIO_new(BIO_s_mem()));
  if (!bio)
    return nullptr;

  NetscapeSPKIPointer spki(NETSCAPE_SPKI_b64_decode(data, len));
  if (!spki)
    return nullptr;

  EVPKeyPointer pkey(NETSCAPE_SPKI_get_pubkey(spki.get()));
  if (!pkey)
    return nullptr;

  if (PEM_write_bio_PUBKEY(bio.get(), pkey.get()) <= 0)
    return nullptr;

  BUF_MEM* ptr;
  BIO_get_mem_ptr(bio.get(), &ptr);

  *size = ptr->length;
  buf = Malloc(*size);
  memcpy(buf, ptr->data, *size);

  return buf;
}


void ExportPublicKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  size_t length = Buffer::Length(args[0]);
  if (length == 0)
    return args.GetReturnValue().SetEmptyString();

  char* data = Buffer::Data(args[0]);
  CHECK_NOT_NULL(data);

  size_t pkey_size;
  char* pkey = ExportPublicKey(data, length, &pkey_size);
  if (pkey == nullptr)
    return args.GetReturnValue().SetEmptyString();

  Local<Value> out = Buffer::New(env, pkey, pkey_size).ToLocalChecked();
  args.GetReturnValue().Set(out);
}


OpenSSLBuffer ExportChallenge(const char* data, int len) {
  NetscapeSPKIPointer sp(NETSCAPE_SPKI_b64_decode(data, len));
  if (!sp)
    return nullptr;

  unsigned char* buf = nullptr;
  ASN1_STRING_to_UTF8(&buf, sp->spkac->challenge);

  return OpenSSLBuffer(reinterpret_cast<char*>(buf));
}


void ExportChallenge(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  size_t len = Buffer::Length(args[0]);
  if (len == 0)
    return args.GetReturnValue().SetEmptyString();

  char* data = Buffer::Data(args[0]);
  CHECK_NOT_NULL(data);

  OpenSSLBuffer cert = ExportChallenge(data, len);
  if (!cert)
    return args.GetReturnValue().SetEmptyString();

  Local<Value> outString =
      Encode(env->isolate(), cert.get(), strlen(cert.get()), BUFFER);

  args.GetReturnValue().Set(outString);
}


// Convert the input public key to compressed, uncompressed, or hybrid formats.
void ConvertKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_EQ(args.Length(), 3);

  size_t len = Buffer::Length(args[0]);
  if (len == 0)
    return args.GetReturnValue().SetEmptyString();

  node::Utf8Value curve(env->isolate(), args[1]);

  int nid = OBJ_sn2nid(*curve);
  if (nid == NID_undef)
    return env->ThrowTypeError("Invalid ECDH curve name");

  ECGroupPointer group(
      EC_GROUP_new_by_curve_name(nid));
  if (group == nullptr)
    return env->ThrowError("Failed to get EC_GROUP");

  ECPointPointer pub(
      ECDH::BufferToPoint(env,
                          group.get(),
                          Buffer::Data(args[0]),
                          len));

  if (pub == nullptr)
    return env->ThrowError("Failed to convert Buffer to EC_POINT");

  CHECK(args[2]->IsUint32());
  uint32_t val = args[2].As<Uint32>()->Value();
  point_conversion_form_t form = static_cast<point_conversion_form_t>(val);

  int size = EC_POINT_point2oct(
      group.get(), pub.get(), form, nullptr, 0, nullptr);

  if (size == 0)
    return env->ThrowError("Failed to get public key length");

  unsigned char* out = node::Malloc<unsigned char>(size);

  int r = EC_POINT_point2oct(group.get(), pub.get(), form, out, size, nullptr);
  if (r != size) {
    free(out);
    return env->ThrowError("Failed to get public key");
  }

  Local<Object> buf =
      Buffer::New(env, reinterpret_cast<char*>(out), size).ToLocalChecked();
  args.GetReturnValue().Set(buf);
}


void TimingSafeEqual(const FunctionCallbackInfo<Value>& args) {
  CHECK(Buffer::HasInstance(args[0]));
  CHECK(Buffer::HasInstance(args[1]));

  size_t buf_length = Buffer::Length(args[0]);
  CHECK_EQ(buf_length, Buffer::Length(args[1]));

  const char* buf1 = Buffer::Data(args[0]);
  const char* buf2 = Buffer::Data(args[1]);

  return args.GetReturnValue().Set(CRYPTO_memcmp(buf1, buf2, buf_length) == 0);
}

void InitCryptoOnce() {
  SSL_load_error_strings();
  OPENSSL_no_config();

  // --openssl-config=...
  if (!per_process_opts->openssl_config.empty()) {
    OPENSSL_load_builtin_modules();
#ifndef OPENSSL_NO_ENGINE
    ENGINE_load_builtin_engines();
#endif
    ERR_clear_error();
    CONF_modules_load_file(
        per_process_opts->openssl_config.c_str(),
        nullptr,
        CONF_MFLAGS_DEFAULT_SECTION);
    int err = ERR_get_error();
    if (0 != err) {
      fprintf(stderr,
              "openssl config failed: %s\n",
              ERR_error_string(err, nullptr));
      CHECK_NE(err, 0);
    }
  }

  SSL_library_init();
  OpenSSL_add_all_algorithms();

#ifdef NODE_FIPS_MODE
  /* Override FIPS settings in cnf file, if needed. */
  unsigned long err = 0;  // NOLINT(runtime/int)
  // TODO(addaleax): Use commented part instead.
  /*if (per_process_opts->enable_fips_crypto ||
      per_process_opts->force_fips_crypto) {*/
  if (enable_fips_crypto || force_fips_crypto) {
    if (0 == FIPS_mode() && !FIPS_mode_set(1)) {
      err = ERR_get_error();
    }
  }
  if (0 != err) {
    fprintf(stderr,
            "openssl fips failed: %s\n",
            ERR_error_string(err, nullptr));
    UNREACHABLE();
  }
#endif  // NODE_FIPS_MODE


  // Turn off compression. Saves memory and protects against CRIME attacks.
  // No-op with OPENSSL_NO_COMP builds of OpenSSL.
  sk_SSL_COMP_zero(SSL_COMP_get_compression_methods());

#ifndef OPENSSL_NO_ENGINE
  ERR_load_ENGINE_strings();
  ENGINE_load_builtin_engines();
#endif  // !OPENSSL_NO_ENGINE

  NodeBIO::GetMethod();
}


#ifndef OPENSSL_NO_ENGINE
void SetEngine(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.Length() >= 2 && args[0]->IsString());
  uint32_t flags;
  if (!args[1]->Uint32Value(env->context()).To(&flags)) return;

  ClearErrorOnReturn clear_error_on_return;

  // Load engine.
  const node::Utf8Value engine_id(env->isolate(), args[0]);
  char errmsg[1024];
  ENGINE* engine = LoadEngineById(*engine_id, &errmsg);

  if (engine == nullptr) {
    int err = ERR_get_error();
    if (err == 0)
      return args.GetReturnValue().Set(false);
    return ThrowCryptoError(env, err);
  }

  int r = ENGINE_set_default(engine, flags);
  ENGINE_free(engine);
  if (r == 0)
    return ThrowCryptoError(env, ERR_get_error());

  args.GetReturnValue().Set(true);
}
#endif  // !OPENSSL_NO_ENGINE

#ifdef NODE_FIPS_MODE
void GetFipsCrypto(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(FIPS_mode() ? 1 : 0);
}

void SetFipsCrypto(const FunctionCallbackInfo<Value>& args) {
  // TODO(addaleax): Use options parser variables instead.
  CHECK(!force_fips_crypto);
  Environment* env = Environment::GetCurrent(args);
  const bool enabled = FIPS_mode();
  bool enable;
  if (!args[0]->BooleanValue(env->context()).To(&enable)) return;
  if (enable == enabled)
    return;  // No action needed.
  if (!FIPS_mode_set(enable)) {
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
    return ThrowCryptoError(env, err);
  }
}
#endif /* NODE_FIPS_MODE */

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  static uv_once_t init_once = UV_ONCE_INIT;
  uv_once(&init_once, InitCryptoOnce);

  Environment* env = Environment::GetCurrent(context);
  SecureContext::Initialize(env, target);
  CipherBase::Initialize(env, target);
  DiffieHellman::Initialize(env, target);
  ECDH::Initialize(env, target);
  Hmac::Initialize(env, target);
  Hash::Initialize(env, target);
  Sign::Initialize(env, target);
  Verify::Initialize(env, target);

  env->SetMethodNoSideEffect(target, "certVerifySpkac", VerifySpkac);
  env->SetMethodNoSideEffect(target, "certExportPublicKey", ExportPublicKey);
  env->SetMethodNoSideEffect(target, "certExportChallenge", ExportChallenge);

  env->SetMethodNoSideEffect(target, "ECDHConvertKey", ConvertKey);
#ifndef OPENSSL_NO_ENGINE
  env->SetMethod(target, "setEngine", SetEngine);
#endif  // !OPENSSL_NO_ENGINE

#ifdef NODE_FIPS_MODE
  env->SetMethodNoSideEffect(target, "getFipsCrypto", GetFipsCrypto);
  env->SetMethod(target, "setFipsCrypto", SetFipsCrypto);
#endif

  env->SetMethod(target, "pbkdf2", PBKDF2);
  env->SetMethod(target, "generateKeyPairRSA", GenerateKeyPairRSA);
  env->SetMethod(target, "generateKeyPairDSA", GenerateKeyPairDSA);
  env->SetMethod(target, "generateKeyPairEC", GenerateKeyPairEC);
  NODE_DEFINE_CONSTANT(target, OPENSSL_EC_NAMED_CURVE);
  NODE_DEFINE_CONSTANT(target, OPENSSL_EC_EXPLICIT_CURVE);
  NODE_DEFINE_CONSTANT(target, PK_ENCODING_PKCS1);
  NODE_DEFINE_CONSTANT(target, PK_ENCODING_PKCS8);
  NODE_DEFINE_CONSTANT(target, PK_ENCODING_SPKI);
  NODE_DEFINE_CONSTANT(target, PK_ENCODING_SEC1);
  NODE_DEFINE_CONSTANT(target, PK_FORMAT_DER);
  NODE_DEFINE_CONSTANT(target, PK_FORMAT_PEM);
  env->SetMethod(target, "randomBytes", RandomBytes);
  env->SetMethodNoSideEffect(target, "timingSafeEqual", TimingSafeEqual);
  env->SetMethodNoSideEffect(target, "getSSLCiphers", GetSSLCiphers);
  env->SetMethodNoSideEffect(target, "getCiphers", GetCiphers);
  env->SetMethodNoSideEffect(target, "getHashes", GetHashes);
  env->SetMethodNoSideEffect(target, "getCurves", GetCurves);
  env->SetMethod(target, "publicEncrypt",
                 PublicKeyCipher::Cipher<PublicKeyCipher::kPublic,
                                         EVP_PKEY_encrypt_init,
                                         EVP_PKEY_encrypt>);
  env->SetMethod(target, "privateDecrypt",
                 PublicKeyCipher::Cipher<PublicKeyCipher::kPrivate,
                                         EVP_PKEY_decrypt_init,
                                         EVP_PKEY_decrypt>);
  env->SetMethod(target, "privateEncrypt",
                 PublicKeyCipher::Cipher<PublicKeyCipher::kPrivate,
                                         EVP_PKEY_sign_init,
                                         EVP_PKEY_sign>);
  env->SetMethod(target, "publicDecrypt",
                 PublicKeyCipher::Cipher<PublicKeyCipher::kPublic,
                                         EVP_PKEY_verify_recover_init,
                                         EVP_PKEY_verify_recover>);
#ifndef OPENSSL_NO_SCRYPT
  env->SetMethod(target, "scrypt", Scrypt);
#endif  // OPENSSL_NO_SCRYPT
}

constexpr int search(const char* s, int n, int c) {
  return *s == c ? n : search(s + 1, n + 1, c);
}

std::string GetOpenSSLVersion() {
  // sample openssl version string format
  // for reference: "OpenSSL 1.1.0i 14 Aug 2018"
  char buf[128];
  const int start = search(OPENSSL_VERSION_TEXT, 0, ' ') + 1;
  const int end = search(OPENSSL_VERSION_TEXT + start, start, ' ');
  const int len = end - start;
  snprintf(buf, sizeof(buf), "%.*s", len, &OPENSSL_VERSION_TEXT[start]);
  return std::string(buf);
}

}  // namespace crypto
}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(crypto, node::crypto::Initialize)
