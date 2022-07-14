#include "crypto/crypto_context.h"
#include "crypto/crypto_bio.h"
#include "crypto/crypto_common.h"
#include "crypto/crypto_util.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_buffer.h"
#include "node_options.h"
#include "util.h"
#include "v8.h"

#include <openssl/x509.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif  // !OPENSSL_NO_ENGINE

namespace node {

using v8::Array;
using v8::ArrayBufferView;
using v8::Context;
using v8::DontDelete;
using v8::Exception;
using v8::External;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::Signature;
using v8::String;
using v8::Value;

namespace crypto {
static const char* const root_certs[] = {
#include "node_root_certs.h"  // NOLINT(build/include_order)
};

static const char system_cert_path[] = NODE_OPENSSL_SYSTEM_CERT_PATH;

static X509_STORE* root_cert_store;

static bool extra_root_certs_loaded = false;

// Takes a string or buffer and loads it into a BIO.
// Caller responsible for BIO_free_all-ing the returned object.
BIOPointer LoadBIO(Environment* env, Local<Value> v) {
  HandleScope scope(env->isolate());

  if (v->IsString()) {
    Utf8Value s(env->isolate(), v);
    return NodeBIO::NewFixed(*s, s.length());
  }

  if (v->IsArrayBufferView()) {
    ArrayBufferViewContents<char> buf(v.As<ArrayBufferView>());
    return NodeBIO::NewFixed(buf.data(), buf.length());
  }

  return nullptr;
}

namespace {
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
      // TODO(tniessen): SSL_CTX_get_issuer does not allow the caller to
      // distinguish between a failed operation and an empty result. Fix that
      // and then handle the potential error properly here (set ret to 0).
      *issuer_ = SSL_CTX_get_issuer(ctx, x.get());
      // NOTE: get_cert_store doesn't increment reference count,
      // no need to free `store`
    } else {
      // Increment issuer reference count
      issuer_->reset(X509_dup(issuer));
      if (!*issuer_) {
        ret = 0;
      }
    }
  }

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

}  // namespace

X509_STORE* NewRootCertStore() {
  static std::vector<X509*> root_certs_vector;
  static Mutex root_certs_vector_mutex;
  Mutex::ScopedLock lock(root_certs_vector_mutex);

  if (root_certs_vector.empty() &&
      per_process::cli_options->ssl_openssl_cert_store == false) {
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
    ERR_set_mark();
    X509_STORE_load_locations(store, system_cert_path, nullptr);
    ERR_pop_to_mark();
  }

  Mutex::ScopedLock cli_lock(node::per_process::cli_options_mutex);
  if (per_process::cli_options->ssl_openssl_cert_store) {
    X509_STORE_set_default_paths(store);
  } else {
    for (X509* cert : root_certs_vector) {
      X509_up_ref(cert);
      X509_STORE_add_cert(store, cert);
    }
  }

  return store;
}

void GetRootCertificates(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Value> result[arraysize(root_certs)];

  for (size_t i = 0; i < arraysize(root_certs); i++) {
    if (!String::NewFromOneByte(
            env->isolate(),
            reinterpret_cast<const uint8_t*>(root_certs[i]))
            .ToLocal(&result[i])) {
      return;
    }
  }

  args.GetReturnValue().Set(
      Array::New(env->isolate(), result, arraysize(root_certs)));
}

bool SecureContext::HasInstance(Environment* env, const Local<Value>& value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> SecureContext::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl = env->secure_context_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        SecureContext::kInternalFieldCount);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "SecureContext"));

    env->SetProtoMethod(tmpl, "init", Init);
    env->SetProtoMethod(tmpl, "setKey", SetKey);
    env->SetProtoMethod(tmpl, "setCert", SetCert);
    env->SetProtoMethod(tmpl, "addCACert", AddCACert);
    env->SetProtoMethod(tmpl, "addCRL", AddCRL);
    env->SetProtoMethod(tmpl, "addRootCerts", AddRootCerts);
    env->SetProtoMethod(tmpl, "setCipherSuites", SetCipherSuites);
    env->SetProtoMethod(tmpl, "setCiphers", SetCiphers);
    env->SetProtoMethod(tmpl, "setSigalgs", SetSigalgs);
    env->SetProtoMethod(tmpl, "setECDHCurve", SetECDHCurve);
    env->SetProtoMethod(tmpl, "setDHParam", SetDHParam);
    env->SetProtoMethod(tmpl, "setMaxProto", SetMaxProto);
    env->SetProtoMethod(tmpl, "setMinProto", SetMinProto);
    env->SetProtoMethod(tmpl, "getMaxProto", GetMaxProto);
    env->SetProtoMethod(tmpl, "getMinProto", GetMinProto);
    env->SetProtoMethod(tmpl, "setOptions", SetOptions);
    env->SetProtoMethod(tmpl, "setSessionIdContext", SetSessionIdContext);
    env->SetProtoMethod(tmpl, "setSessionTimeout", SetSessionTimeout);
    env->SetProtoMethod(tmpl, "close", Close);
    env->SetProtoMethod(tmpl, "loadPKCS12", LoadPKCS12);
    env->SetProtoMethod(tmpl, "setTicketKeys", SetTicketKeys);
    env->SetProtoMethod(tmpl, "setFreeListLength", SetFreeListLength);
    env->SetProtoMethod(tmpl, "enableTicketKeyCallback",
        EnableTicketKeyCallback);

    env->SetProtoMethodNoSideEffect(tmpl, "getTicketKeys", GetTicketKeys);
    env->SetProtoMethodNoSideEffect(tmpl, "getCertificate",
        GetCertificate<true>);
    env->SetProtoMethodNoSideEffect(tmpl, "getIssuer",
        GetCertificate<false>);

  #ifndef OPENSSL_NO_ENGINE
    env->SetProtoMethod(tmpl, "setEngineKey", SetEngineKey);
    env->SetProtoMethod(tmpl, "setClientCertEngine", SetClientCertEngine);
  #endif  // !OPENSSL_NO_ENGINE

  #define SET_INTEGER_CONSTANTS(name, value)                                   \
      tmpl->Set(FIXED_ONE_BYTE_STRING(env->isolate(), name),                   \
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
                              Local<Value>(),
                              Signature::New(env->isolate(), tmpl));

    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(env->isolate(), "_external"),
        ctx_getter_templ,
        Local<FunctionTemplate>(),
        static_cast<PropertyAttribute>(ReadOnly | DontDelete));

    env->set_secure_context_constructor_template(tmpl);
  }
  return tmpl;
}

void SecureContext::Initialize(Environment* env, Local<Object> target) {
  env->SetConstructorFunction(
      target,
      "SecureContext",
      GetConstructorTemplate(env),
      Environment::SetConstructorFunctionFlag::NONE);

  env->SetMethodNoSideEffect(target, "getRootCertificates",
                             GetRootCertificates);
  // Exposed for testing purposes only.
  env->SetMethodNoSideEffect(target, "isExtraRootCertsFileLoaded",
                             IsExtraRootCertsFileLoaded);
}

void SecureContext::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(Init);
  registry->Register(SetKey);
  registry->Register(SetCert);
  registry->Register(AddCACert);
  registry->Register(AddCRL);
  registry->Register(AddRootCerts);
  registry->Register(SetCipherSuites);
  registry->Register(SetCiphers);
  registry->Register(SetSigalgs);
  registry->Register(SetECDHCurve);
  registry->Register(SetDHParam);
  registry->Register(SetMaxProto);
  registry->Register(SetMinProto);
  registry->Register(GetMaxProto);
  registry->Register(GetMinProto);
  registry->Register(SetOptions);
  registry->Register(SetSessionIdContext);
  registry->Register(SetSessionTimeout);
  registry->Register(Close);
  registry->Register(LoadPKCS12);
  registry->Register(SetTicketKeys);
  registry->Register(SetFreeListLength);
  registry->Register(EnableTicketKeyCallback);
  registry->Register(GetTicketKeys);
  registry->Register(GetCertificate<true>);
  registry->Register(GetCertificate<false>);

#ifndef OPENSSL_NO_ENGINE
  registry->Register(SetEngineKey);
  registry->Register(SetClientCertEngine);
#endif  // !OPENSSL_NO_ENGINE

  registry->Register(CtxGetter);

  registry->Register(GetRootCertificates);
  registry->Register(IsExtraRootCertsFileLoaded);
}

SecureContext* SecureContext::Create(Environment* env) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
          ->InstanceTemplate()
          ->NewInstance(env->context()).ToLocal(&obj)) {
    return nullptr;
  }

  return new SecureContext(env, obj);
}

SecureContext::SecureContext(Environment* env, Local<Object> wrap)
    : BaseObject(env, wrap) {
  MakeWeak();
  env->isolate()->AdjustAmountOfExternalAllocatedMemory(kExternalSize);
}

inline void SecureContext::Reset() {
  if (ctx_ != nullptr) {
    env()->isolate()->AdjustAmountOfExternalAllocatedMemory(-kExternalSize);
  }
  ctx_.reset();
  cert_.reset();
  issuer_.reset();
}

SecureContext::~SecureContext() {
  Reset();
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

  if (max_version == 0)
    max_version = kMaxSupportedVersion;

  if (args[0]->IsString()) {
    Utf8Value sslmethod(env->isolate(), args[0]);

    // Note that SSLv2 and SSLv3 are disallowed but SSLv23_method and friends
    // are still accepted.  They are OpenSSL's way of saying that all known
    // protocols below TLS 1.3 are supported unless explicitly disabled (which
    // we do below for SSLv2 and SSLv3.)
    if (sslmethod == "SSLv2_method" ||
        sslmethod == "SSLv2_server_method" ||
        sslmethod == "SSLv2_client_method") {
      THROW_ERR_TLS_INVALID_PROTOCOL_METHOD(env, "SSLv2 methods disabled");
      return;
    } else if (sslmethod == "SSLv3_method" ||
               sslmethod == "SSLv3_server_method" ||
               sslmethod == "SSLv3_client_method") {
      THROW_ERR_TLS_INVALID_PROTOCOL_METHOD(env, "SSLv3 methods disabled");
      return;
    } else if (sslmethod == "SSLv23_method") {
      max_version = TLS1_2_VERSION;
    } else if (sslmethod == "SSLv23_server_method") {
      max_version = TLS1_2_VERSION;
      method = TLS_server_method();
    } else if (sslmethod == "SSLv23_client_method") {
      max_version = TLS1_2_VERSION;
      method = TLS_client_method();
    } else if (sslmethod == "TLS_method") {
      min_version = 0;
      max_version = kMaxSupportedVersion;
    } else if (sslmethod == "TLS_server_method") {
      min_version = 0;
      max_version = kMaxSupportedVersion;
      method = TLS_server_method();
    } else if (sslmethod == "TLS_client_method") {
      min_version = 0;
      max_version = kMaxSupportedVersion;
      method = TLS_client_method();
    } else if (sslmethod == "TLSv1_method") {
      min_version = TLS1_VERSION;
      max_version = TLS1_VERSION;
    } else if (sslmethod == "TLSv1_server_method") {
      min_version = TLS1_VERSION;
      max_version = TLS1_VERSION;
      method = TLS_server_method();
    } else if (sslmethod == "TLSv1_client_method") {
      min_version = TLS1_VERSION;
      max_version = TLS1_VERSION;
      method = TLS_client_method();
    } else if (sslmethod == "TLSv1_1_method") {
      min_version = TLS1_1_VERSION;
      max_version = TLS1_1_VERSION;
    } else if (sslmethod == "TLSv1_1_server_method") {
      min_version = TLS1_1_VERSION;
      max_version = TLS1_1_VERSION;
      method = TLS_server_method();
    } else if (sslmethod == "TLSv1_1_client_method") {
      min_version = TLS1_1_VERSION;
      max_version = TLS1_1_VERSION;
      method = TLS_client_method();
    } else if (sslmethod == "TLSv1_2_method") {
      min_version = TLS1_2_VERSION;
      max_version = TLS1_2_VERSION;
    } else if (sslmethod == "TLSv1_2_server_method") {
      min_version = TLS1_2_VERSION;
      max_version = TLS1_2_VERSION;
      method = TLS_server_method();
    } else if (sslmethod == "TLSv1_2_client_method") {
      min_version = TLS1_2_VERSION;
      max_version = TLS1_2_VERSION;
      method = TLS_client_method();
    } else {
      const std::string msg("Unknown method: ");
      THROW_ERR_TLS_INVALID_PROTOCOL_METHOD(env, (msg + * sslmethod).c_str());
      return;
    }
  }

  sc->ctx_.reset(SSL_CTX_new(method));
  if (!sc->ctx_) {
    return ThrowCryptoError(env, ERR_get_error(), "SSL_CTX_new");
  }
  SSL_CTX_set_app_data(sc->ctx_.get(), sc);

  // Disable SSLv2 in the case when method == TLS_method() and the
  // cipher list contains SSLv2 ciphers (not the default, should be rare.)
  // The bundled OpenSSL doesn't have SSLv2 support but the system OpenSSL may.
  // SSLv3 is disabled because it's susceptible to downgrade attacks (POODLE.)
  SSL_CTX_set_options(sc->ctx_.get(), SSL_OP_NO_SSLv2);
  SSL_CTX_set_options(sc->ctx_.get(), SSL_OP_NO_SSLv3);
#if OPENSSL_VERSION_MAJOR >= 3
  SSL_CTX_set_options(sc->ctx_.get(), SSL_OP_ALLOW_CLIENT_RENEGOTIATION);
#endif

  // Enable automatic cert chaining. This is enabled by default in OpenSSL, but
  // disabled by default in BoringSSL. Enable it explicitly to make the
  // behavior match when Node is built with BoringSSL.
  SSL_CTX_clear_mode(sc->ctx_.get(), SSL_MODE_NO_AUTO_CHAIN);

  // SSL session cache configuration
  SSL_CTX_set_session_cache_mode(sc->ctx_.get(),
                                 SSL_SESS_CACHE_CLIENT |
                                 SSL_SESS_CACHE_SERVER |
                                 SSL_SESS_CACHE_NO_INTERNAL |
                                 SSL_SESS_CACHE_NO_AUTO_CLEAR);

  SSL_CTX_set_min_proto_version(sc->ctx_.get(), min_version);
  SSL_CTX_set_max_proto_version(sc->ctx_.get(), max_version);

  // OpenSSL 1.1.0 changed the ticket key size, but the OpenSSL 1.0.x size was
  // exposed in the public API. To retain compatibility, install a callback
  // which restores the old algorithm.
  if (RAND_bytes(sc->ticket_key_name_, sizeof(sc->ticket_key_name_)) <= 0 ||
      RAND_bytes(sc->ticket_key_hmac_, sizeof(sc->ticket_key_hmac_)) <= 0 ||
      RAND_bytes(sc->ticket_key_aes_, sizeof(sc->ticket_key_aes_)) <= 0) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Error generating ticket keys");
  }
  SSL_CTX_set_tlsext_ticket_key_cb(sc->ctx_.get(), TicketCompatibilityCallback);
}

SSLPointer SecureContext::CreateSSL() {
  return SSLPointer(SSL_new(ctx_.get()));
}

void SecureContext::SetNewSessionCallback(NewSessionCb cb) {
  SSL_CTX_sess_set_new_cb(ctx_.get(), cb);
}

void SecureContext::SetGetSessionCallback(GetSessionCb cb) {
  SSL_CTX_sess_set_get_cb(ctx_.get(), cb);
}

void SecureContext::SetSelectSNIContextCallback(SelectSNIContextCb cb) {
  SSL_CTX_set_tlsext_servername_callback(ctx_.get(), cb);
}

void SecureContext::SetKeylogCallback(KeylogCb cb) {
  SSL_CTX_set_keylog_callback(ctx_.get(), cb);
}

void SecureContext::SetKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  CHECK_GE(args.Length(), 1);  // Private key argument is mandatory

  BIOPointer bio(LoadBIO(env, args[0]));
  if (!bio)
    return;

  ByteSource passphrase;
  if (args[1]->IsString())
    passphrase = ByteSource::FromString(env, args[1].As<String>());
  // This redirection is necessary because the PasswordCallback expects a
  // pointer to a pointer to the passphrase ByteSource to allow passing in
  // const ByteSources.
  const ByteSource* pass_ptr = &passphrase;

  EVPKeyPointer key(
      PEM_read_bio_PrivateKey(bio.get(),
                              nullptr,
                              PasswordCallback,
                              &pass_ptr));

  if (!key)
    return ThrowCryptoError(env, ERR_get_error(), "PEM_read_bio_PrivateKey");

  if (!SSL_CTX_use_PrivateKey(sc->ctx_.get(), key.get()))
    return ThrowCryptoError(env, ERR_get_error(), "SSL_CTX_use_PrivateKey");
}

void SecureContext::SetSigalgs(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  Environment* env = sc->env();
  ClearErrorOnReturn clear_error_on_return;

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());

  const Utf8Value sigalgs(env->isolate(), args[0]);

  if (!SSL_CTX_set1_sigalgs_list(sc->ctx_.get(), *sigalgs))
    return ThrowCryptoError(env, ERR_get_error());
}

#ifndef OPENSSL_NO_ENGINE
void SecureContext::SetEngineKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  CHECK_EQ(args.Length(), 2);

  CryptoErrorStore errors;
  Utf8Value engine_id(env->isolate(), args[1]);
  EnginePointer engine = LoadEngineById(*engine_id, &errors);
  if (!engine) {
    Local<Value> exception;
    if (errors.ToException(env).ToLocal(&exception))
      env->isolate()->ThrowException(exception);
    return;
  }

  if (!ENGINE_init(engine.get())) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Failure to initialize engine");
  }

  engine.finish_on_exit = true;

  Utf8Value key_name(env->isolate(), args[0]);
  EVPKeyPointer key(ENGINE_load_private_key(engine.get(), *key_name,
                                            nullptr, nullptr));

  if (!key)
    return ThrowCryptoError(env, ERR_get_error(), "ENGINE_load_private_key");

  if (!SSL_CTX_use_PrivateKey(sc->ctx_.get(), key.get()))
    return ThrowCryptoError(env, ERR_get_error(), "SSL_CTX_use_PrivateKey");

  sc->private_key_engine_ = std::move(engine);
}
#endif  // !OPENSSL_NO_ENGINE

void SecureContext::SetCert(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  CHECK_GE(args.Length(), 1);  // Certificate argument is mandator

  BIOPointer bio(LoadBIO(env, args[0]));
  if (!bio)
    return;

  sc->cert_.reset();
  sc->issuer_.reset();

  if (!SSL_CTX_use_certificate_chain(
          sc->ctx_.get(),
          std::move(bio),
          &sc->cert_,
          &sc->issuer_)) {
    return ThrowCryptoError(
        env,
        ERR_get_error(),
        "SSL_CTX_use_certificate_chain");
  }
}

void SecureContext::AddCACert(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  ClearErrorOnReturn clear_error_on_return;

  CHECK_GE(args.Length(), 1);  // CA certificate argument is mandatory

  BIOPointer bio(LoadBIO(env, args[0]));
  if (!bio)
    return;

  X509_STORE* cert_store = SSL_CTX_get_cert_store(sc->ctx_.get());
  while (X509* x509 = PEM_read_bio_X509_AUX(
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

  CHECK_GE(args.Length(), 1);  // CRL argument is mandatory

  ClearErrorOnReturn clear_error_on_return;

  BIOPointer bio(LoadBIO(env, args[0]));
  if (!bio)
    return;

  DeleteFnPtr<X509_CRL, X509_CRL_free> crl(
      PEM_read_bio_X509_CRL(bio.get(), nullptr, NoPasswordCallback, nullptr));

  if (!crl)
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to parse CRL");

  X509_STORE* cert_store = SSL_CTX_get_cert_store(sc->ctx_.get());
  if (cert_store == root_cert_store) {
    cert_store = NewRootCertStore();
    SSL_CTX_set_cert_store(sc->ctx_.get(), cert_store);
  }

  X509_STORE_add_crl(cert_store, crl.get());
  X509_STORE_set_flags(cert_store,
                       X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
}

void SecureContext::AddRootCerts(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  ClearErrorOnReturn clear_error_on_return;

  if (root_cert_store == nullptr) {
    root_cert_store = NewRootCertStore();
  }

  // Increment reference count so global store is not deleted along with CTX.
  X509_STORE_up_ref(root_cert_store);
  SSL_CTX_set_cert_store(sc->ctx_.get(), root_cert_store);
}

void SecureContext::SetCipherSuites(const FunctionCallbackInfo<Value>& args) {
  // BoringSSL doesn't allow API config of TLS1.3 cipher suites.
#ifndef OPENSSL_IS_BORINGSSL
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  Environment* env = sc->env();
  ClearErrorOnReturn clear_error_on_return;

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());

  const Utf8Value ciphers(env->isolate(), args[0]);
  if (!SSL_CTX_set_ciphersuites(sc->ctx_.get(), *ciphers))
    return ThrowCryptoError(env, ERR_get_error(), "Failed to set ciphers");
#endif
}

void SecureContext::SetCiphers(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  Environment* env = sc->env();
  ClearErrorOnReturn clear_error_on_return;

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());

  Utf8Value ciphers(env->isolate(), args[0]);
  if (!SSL_CTX_set_cipher_list(sc->ctx_.get(), *ciphers)) {
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)

    if (strlen(*ciphers) == 0 && ERR_GET_REASON(err) == SSL_R_NO_CIPHER_MATCH) {
      // TLS1.2 ciphers were deliberately cleared, so don't consider
      // SSL_R_NO_CIPHER_MATCH to be an error (this is how _set_cipher_suites()
      // works). If the user actually sets a value (like "no-such-cipher"), then
      // that's actually an error.
      return;
    }
    return ThrowCryptoError(env, err, "Failed to set ciphers");
  }
}

void SecureContext::SetECDHCurve(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  Environment* env = sc->env();

  CHECK_GE(args.Length(), 1);  // ECDH curve name argument is mandatory
  CHECK(args[0]->IsString());

  Utf8Value curve(env->isolate(), args[0]);

  if (strcmp(*curve, "auto") != 0 &&
      !SSL_CTX_set1_curves_list(sc->ctx_.get(), *curve)) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to set ECDH curve");
  }
}

void SecureContext::SetDHParam(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  Environment* env = sc->env();
  ClearErrorOnReturn clear_error_on_return;

  CHECK_GE(args.Length(), 1);  // DH argument is mandatory

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

  if (!SSL_CTX_set_tmp_dh(sc->ctx_.get(), dh.get())) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Error setting temp DH parameter");
  }
}

void SecureContext::SetMinProto(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsInt32());

  int version = args[0].As<Int32>()->Value();

  CHECK(SSL_CTX_set_min_proto_version(sc->ctx_.get(), version));
}

void SecureContext::SetMaxProto(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsInt32());

  int version = args[0].As<Int32>()->Value();

  CHECK(SSL_CTX_set_max_proto_version(sc->ctx_.get(), version));
}

void SecureContext::GetMinProto(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  CHECK_EQ(args.Length(), 0);

  long version =  // NOLINT(runtime/int)
    SSL_CTX_get_min_proto_version(sc->ctx_.get());
  args.GetReturnValue().Set(static_cast<uint32_t>(version));
}

void SecureContext::GetMaxProto(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  CHECK_EQ(args.Length(), 0);

  long version =  // NOLINT(runtime/int)
    SSL_CTX_get_max_proto_version(sc->ctx_.get());
  args.GetReturnValue().Set(static_cast<uint32_t>(version));
}

void SecureContext::SetOptions(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsNumber());

  int64_t val = args[0]->IntegerValue(env->context()).FromMaybe(0);

  SSL_CTX_set_options(sc->ctx_.get(),
                      static_cast<long>(val));  // NOLINT(runtime/int)
}

void SecureContext::SetSessionIdContext(
    const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());
  Environment* env = sc->env();

  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  const Utf8Value sessionIdContext(env->isolate(), args[0]);
  const unsigned char* sid_ctx =
      reinterpret_cast<const unsigned char*>(*sessionIdContext);
  unsigned int sid_ctx_len = sessionIdContext.length();

  if (SSL_CTX_set_session_id_context(sc->ctx_.get(), sid_ctx, sid_ctx_len) == 1)
    return;

  BUF_MEM* mem;
  Local<String> message;

  BIOPointer bio(BIO_new(BIO_s_mem()));
  if (!bio) {
    message = FIXED_ONE_BYTE_STRING(env->isolate(),
                                    "SSL_CTX_set_session_id_context error");
  } else {
    ERR_print_errors(bio.get());
    BIO_get_mem_ptr(bio.get(), &mem);
    message = OneByteString(env->isolate(), mem->data, mem->length);
  }

  env->isolate()->ThrowException(Exception::TypeError(message));
}

void SecureContext::SetSessionTimeout(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.Holder());

  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsInt32());

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
  if (!in) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Unable to load PFX certificate");
  }

  if (args.Length() >= 2) {
    THROW_AND_RETURN_IF_NOT_BUFFER(env, args[1], "Pass phrase");
    Local<ArrayBufferView> abv = args[1].As<ArrayBufferView>();
    size_t passlen = abv->ByteLength();
    pass.resize(passlen + 1);
    abv->CopyContents(pass.data(), passlen);
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
    // TODO(@jasnell): Should this use ThrowCryptoError?
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
    const char* str = ERR_reason_error_string(err);
    str = str != nullptr ? str : "Unknown error";

    return env->ThrowError(str);
  }
}

#ifndef OPENSSL_NO_ENGINE
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
  CHECK(!sc->client_cert_engine_provided_);

  CryptoErrorStore errors;
  const Utf8Value engine_id(env->isolate(), args[0]);
  EnginePointer engine = LoadEngineById(*engine_id, &errors);
  if (!engine) {
    Local<Value> exception;
    if (errors.ToException(env).ToLocal(&exception))
      env->isolate()->ThrowException(exception);
    return;
  }

  // Note that this takes another reference to `engine`.
  if (!SSL_CTX_set_client_cert_engine(sc->ctx_.get(), engine.get()))
    return ThrowCryptoError(env, ERR_get_error());
  sc->client_cert_engine_provided_ = true;
}
#endif  // !OPENSSL_NO_ENGINE

void SecureContext::GetTicketKeys(const FunctionCallbackInfo<Value>& args) {
#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_get_tlsext_ticket_keys)

  SecureContext* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  Local<Object> buff;
  if (!Buffer::New(wrap->env(), 48).ToLocal(&buff))
    return;

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

  CHECK_GE(args.Length(), 1);  // Ticket keys argument is mandatory
  CHECK(args[0]->IsArrayBufferView());
  ArrayBufferViewContents<char> buf(args[0].As<ArrayBufferView>());

  CHECK_EQ(buf.length(), 48);

  memcpy(wrap->ticket_key_name_, buf.data(), 16);
  memcpy(wrap->ticket_key_hmac_, buf.data() + 16, 16);
  memcpy(wrap->ticket_key_aes_, buf.data() + 32, 16);

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

  Local<Value> argv[3];

  if (!Buffer::Copy(
          env,
          reinterpret_cast<char*>(name),
          kTicketPartSize).ToLocal(&argv[0]) ||
      !Buffer::Copy(
          env,
          reinterpret_cast<char*>(iv),
          kTicketPartSize).ToLocal(&argv[1])) {
    return -1;
  }

  argv[2] = enc != 0 ? v8::True(env->isolate()) : v8::False(env->isolate());

  Local<Value> ret;
  if (!node::MakeCallback(
          env->isolate(),
          sc->object(),
          env->ticketkeycallback_string(),
          arraysize(argv),
          argv,
          {0, 0}).ToLocal(&ret) ||
      !ret->IsArray()) {
    return -1;
  }
  Local<Array> arr = ret.As<Array>();

  Local<Value> val;
  if (!arr->Get(env->context(), kTicketKeyReturnIndex).ToLocal(&val) ||
      !val->IsInt32()) {
    return -1;
  }

  int r = val.As<Int32>()->Value();
  if (r < 0)
    return r;

  Local<Value> hmac;
  Local<Value> aes;

  if (!arr->Get(env->context(), kTicketKeyHMACIndex).ToLocal(&hmac) ||
      !arr->Get(env->context(), kTicketKeyAESIndex).ToLocal(&aes) ||
      Buffer::Length(aes) != kTicketPartSize) {
    return -1;
  }

  if (enc) {
    Local<Value> name_val;
    Local<Value> iv_val;
    if (!arr->Get(env->context(), kTicketKeyNameIndex).ToLocal(&name_val) ||
        !arr->Get(env->context(), kTicketKeyIVIndex).ToLocal(&iv_val) ||
        Buffer::Length(name_val) != kTicketPartSize ||
        Buffer::Length(iv_val) != kTicketPartSize) {
      return -1;
    }

    name_val.As<ArrayBufferView>()->CopyContents(name, kTicketPartSize);
    iv_val.As<ArrayBufferView>()->CopyContents(iv, kTicketPartSize);
  }

  ArrayBufferViewContents<unsigned char> hmac_buf(hmac);
  HMAC_Init_ex(hctx,
               hmac_buf.data(),
               hmac_buf.length(),
               EVP_sha256(),
               nullptr);

  ArrayBufferViewContents<unsigned char> aes_key(aes.As<ArrayBufferView>());
  if (enc) {
    EVP_EncryptInit_ex(ectx,
                       EVP_aes_128_cbc(),
                       nullptr,
                       aes_key.data(),
                       iv);
  } else {
    EVP_DecryptInit_ex(ectx,
                       EVP_aes_128_cbc(),
                       nullptr,
                       aes_key.data(),
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
  Local<Object> buff;
  if (!Buffer::New(env, size).ToLocal(&buff))
    return;
  unsigned char* serialized = reinterpret_cast<unsigned char*>(
      Buffer::Data(buff));
  i2d_X509(cert, &serialized);

  args.GetReturnValue().Set(buff);
}

namespace {
unsigned long AddCertsFromFile(  // NOLINT(runtime/int)
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
}  // namespace

// UseExtraCaCerts is called only once at the start of the Node.js process.
void UseExtraCaCerts(const std::string& file) {
  ClearErrorOnReturn clear_error_on_return;

  if (root_cert_store == nullptr) {
    root_cert_store = NewRootCertStore();

    if (!file.empty()) {
      unsigned long err = AddCertsFromFile(  // NOLINT(runtime/int)
                                           root_cert_store,
                                           file.c_str());
      if (err) {
        fprintf(stderr,
                "Warning: Ignoring extra certs from `%s`, load failed: %s\n",
                file.c_str(),
                ERR_error_string(err, nullptr));
      } else {
        extra_root_certs_loaded = true;
      }
    }
  }
}

// Exposed to JavaScript strictly for testing purposes.
void IsExtraRootCertsFileLoaded(
    const FunctionCallbackInfo<Value>& args) {
  return args.GetReturnValue().Set(extra_root_certs_loaded);
}

}  // namespace crypto
}  // namespace node
