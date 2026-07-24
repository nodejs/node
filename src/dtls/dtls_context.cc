#include "dtls_context.h"
#include "dtls_session.h"

#if HAVE_OPENSSL && HAVE_DTLS

#include <base_object-inl.h>
#include <crypto/crypto_bio.h>
#include <crypto/crypto_tls_context.h>
#include <crypto/crypto_util.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <node_errors.h>
#include <node_sockaddr-inl.h>
#include <util-inl.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/srtp.h>
#include <openssl/ssl.h>

#include <cstring>

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

namespace dtls {

namespace {
// The cookie secret is 32 bytes (256 bits).
constexpr size_t kCookieSecretLen = 32;
}  // namespace

DTLSContext::DTLSContext(Environment* env,
                         Local<Object> wrap,
                         SSL_CTX* ctx,
                         bool is_server)
    : BaseObject(env, wrap),
      ctx_(ctx),
      is_server_(is_server),
      cookie_secret_(kCookieSecretLen) {
  MakeWeak();

  // Generate random cookie secret for HMAC-based cookie generation.
  CHECK_EQ(RAND_bytes(cookie_secret_.data(), kCookieSecretLen), 1);

  // Cookie generate/verify callbacks are registered on the SSL_CTX so they
  // are inherited by all SSL objects created from it. However, we do NOT set
  // SSL_OP_COOKIE_EXCHANGE on the context -- DTLSv1_listen() sets this option
  // automatically on the per-SSL object when it runs (see d1_lib.c:804 in
  // OpenSSL). This is important: if SSL_OP_COOKIE_EXCHANGE were set on the
  // context, any SSL created from it would attempt a fresh cookie exchange,
  // which is wrong for session SSLs that have already completed cookie
  // verification via DTLSv1_listen().
  SSL_CTX_set_cookie_generate_cb(ctx_.get(), CookieGenerateCallback);
  SSL_CTX_set_cookie_verify_cb(ctx_.get(), CookieVerifyCallback);

  // Store pointer to this context in the SSL_CTX app data for callbacks.
  SSL_CTX_set_app_data(ctx_.get(), this);
}

void DTLSContext::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("cookie_secret", cookie_secret_.size());
  tracker->TrackFieldWithSize("alpn_protos", alpn_protos_.size());
}

Local<FunctionTemplate> DTLSContext::GetConstructorTemplate(Environment* env) {
  auto tmpl = env->dtls_context_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, New);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "DTLSContext"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        BaseObject::kInternalFieldCount);

    SetProtoMethod(isolate, tmpl, "setCert", SetCert);
    SetProtoMethod(isolate, tmpl, "setKey", SetKey);
    SetProtoMethod(isolate, tmpl, "addCACert", AddCACert);
    SetProtoMethod(isolate, tmpl, "setCiphers", SetCiphers);
    SetProtoMethod(isolate, tmpl, "setALPN", SetALPN);
    SetProtoMethod(isolate, tmpl, "setSRTP", SetSRTP);
    SetProtoMethod(isolate, tmpl, "setVerifyMode", SetVerifyMode);
    SetProtoMethod(isolate, tmpl, "loadDefaultCAs", LoadDefaultCAs);
    SetProtoMethod(isolate, tmpl, "setECDHCurve", SetECDHCurve);

    env->set_dtls_context_constructor_template(tmpl);
  }
  return tmpl;
}

void DTLSContext::InitPerContext(Local<Object> target,
                                 Local<Context> context,
                                 Environment* env) {
  SetConstructorFunction(
      context, target, "DTLSContext", GetConstructorTemplate(env));
}

void DTLSContext::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(SetCert);
  registry->Register(SetKey);
  registry->Register(AddCACert);
  registry->Register(SetCiphers);
  registry->Register(SetALPN);
  registry->Register(SetSRTP);
  registry->Register(SetVerifyMode);
  registry->Register(LoadDefaultCAs);
  registry->Register(SetECDHCurve);
}

// new DTLSContext(isServer)
void DTLSContext::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.IsConstructCall());

  bool is_server = args[0]->IsTrue();

  const SSL_METHOD* method;
  if (is_server) {
    method = DTLS_server_method();
  } else {
    method = DTLS_client_method();
  }

  SSL_CTX* ctx = SSL_CTX_new(method);
  if (ctx == nullptr) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                             "Failed to create DTLS SSL_CTX");
  }

  // Default to DTLS 1.2 only. DTLS 1.0 (based on TLS 1.1) is deprecated
  // by RFC 8996 and lacks AEAD cipher suites.
  SSL_CTX_set_min_proto_version(ctx, DTLS1_2_VERSION);
  SSL_CTX_set_max_proto_version(ctx, DTLS1_2_VERSION);

  // Disable OpenSSL's MTU querying (we manage MTU manually).
  SSL_CTX_set_options(ctx, SSL_OP_NO_QUERY_MTU);

  // Enable all workarounds for maximum compatibility.
  SSL_CTX_set_options(ctx, SSL_OP_ALL);

  if (is_server) {
    // NOTE: SSL_OP_COOKIE_EXCHANGE must NOT be set on the context.
    // DTLSv1_listen() sets it per-SSL automatically (see d1_lib.c:804).
    // Setting it here would cause session SSLs created via CreateFromSSL()
    // to attempt a redundant cookie exchange, hanging the handshake.

    // Enable session caching for session resumption.
    SSL_CTX_set_session_cache_mode(
        ctx, SSL_SESS_CACHE_SERVER | SSL_SESS_CACHE_NO_AUTO_CLEAR);
  } else {
    // Client session caching for resumption.
    SSL_CTX_set_session_cache_mode(
        ctx, SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL);
  }

  // NOTE: We do NOT call SSL_CTX_set_default_verify_paths() here.
  // CA loading is handled in JS: if the user provides custom CAs, only
  // those are loaded (via addCACert). Otherwise, system default CAs are
  // loaded via loadDefaultCAs(). This matches Node.js TLS behavior.

  new DTLSContext(env, args.This(), ctx, is_server);
}

void DTLSContext::SetCert(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());
  Environment* env = ctx->env();

  if (!args[0]->IsString()) {
    return THROW_ERR_INVALID_ARG_TYPE(env, "cert must be a string (PEM)");
  }

  Utf8Value cert_pem(env->isolate(), args[0]);

  auto bio = crypto::NodeBIO::NewFixed(*cert_pem, cert_pem.length());
  if (!bio) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to create BIO");
  }

  ncrypto::X509Pointer cert;
  ncrypto::X509Pointer issuer;
  if (crypto::SSL_CTX_use_certificate_chain(
          ctx->ctx_.get(), std::move(bio), &cert, &issuer) != 1) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env, "PEM_read_bio_X509 failed");
  }
}

void DTLSContext::SetKey(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());
  Environment* env = ctx->env();

  if (!args[0]->IsString()) {
    return THROW_ERR_INVALID_ARG_TYPE(env, "key must be a string (PEM)");
  }

  Utf8Value key_pem(env->isolate(), args[0]);

  auto bio = crypto::NodeBIO::NewFixed(*key_pem, key_pem.length());
  if (!bio) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to create BIO");
  }

  switch (crypto::UsePrivateKey(ctx->ctx_.get(), bio)) {
    case crypto::PrivateKeyResult::kSuccess:
      break;
    case crypto::PrivateKeyResult::kParseError:
      return THROW_ERR_CRYPTO_OPERATION_FAILED(
          env, "PEM_read_bio_PrivateKey failed");
    case crypto::PrivateKeyResult::kApplyError:
      return THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                               "SSL_CTX_use_PrivateKey failed");
  }

  // Verify that the private key matches the certificate.
  if (SSL_CTX_check_private_key(ctx->ctx_.get()) != 1) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Private key does not match certificate");
  }
}

void DTLSContext::AddCACert(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());
  Environment* env = ctx->env();

  if (!args[0]->IsString()) {
    return THROW_ERR_INVALID_ARG_TYPE(env, "ca must be a string (PEM)");
  }

  Utf8Value ca_pem(env->isolate(), args[0]);

  auto bio = crypto::NodeBIO::NewFixed(*ca_pem, ca_pem.length());
  if (!bio) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to create BIO");
  }

  ncrypto::ClearErrorOnReturn clear_error_on_return;
  if (crypto::AddCACertificates(env, ctx->ctx_.get(), bio) == 0) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "No CA certificates found in PEM data");
  }
}

void DTLSContext::SetCiphers(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());
  Environment* env = ctx->env();

  if (!args[0]->IsString()) {
    return THROW_ERR_INVALID_ARG_TYPE(env, "ciphers must be a string");
  }

  Utf8Value ciphers(env->isolate(), args[0]);
  if (SSL_CTX_set_cipher_list(ctx->ctx_.get(), *ciphers) != 1) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                             "SSL_CTX_set_cipher_list failed");
  }
}

void DTLSContext::SetALPN(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());
  Environment* env = ctx->env();

  if (!Buffer::HasInstance(args[0])) {
    return THROW_ERR_INVALID_ARG_TYPE(env, "alpnProtocols must be a Buffer");
  }

  const uint8_t* data = reinterpret_cast<const uint8_t*>(Buffer::Data(args[0]));
  size_t len = Buffer::Length(args[0]);

  if (ctx->is_server_) {
    // Server: store protocols for the selection callback.
    ctx->alpn_protos_.assign(data, data + len);
    SSL_CTX_set_alpn_select_cb(ctx->ctx_.get(), ALPNSelectCallback, ctx);
  } else {
    // Client: advertise protocols to the server.
    SSL_CTX_set_alpn_protos(ctx->ctx_.get(), data, len);
  }
}

void DTLSContext::SetSRTP(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());
  Environment* env = ctx->env();

  if (!args[0]->IsString()) {
    return THROW_ERR_INVALID_ARG_TYPE(env, "srtpProfiles must be a string");
  }

  Utf8Value profiles(env->isolate(), args[0]);
  if (SSL_CTX_set_tlsext_use_srtp(ctx->ctx_.get(), *profiles) != 0) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "SSL_CTX_set_tlsext_use_srtp failed");
  }
}

void DTLSContext::SetVerifyMode(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());

  int mode = args[0]->Int32Value(ctx->env()->context()).FromJust();
  SSL_CTX_set_verify(ctx->ctx_.get(), mode, nullptr);
}

void DTLSContext::LoadDefaultCAs(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());
  crypto::UseDefaultRootCertStore(ctx->env(), ctx->ctx_.get());
}

void DTLSContext::SetECDHCurve(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());
  Environment* env = ctx->env();

  CHECK(args[0]->IsString());
  Utf8Value curve(env->isolate(), args[0]);

  // "auto" means use OpenSSL's default curve selection.
  if (strcmp(*curve, "auto") != 0) {
    if (!SSL_CTX_set1_curves_list(ctx->ctx_.get(), *curve)) {
      return THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to set ECDH curve");
    }
  }
}

// HMAC-SHA256 based cookie generation using the peer's address.
// During DTLSv1_listen(), the peer address is taken from
// DTLSContext::current_cookie_peer_ (set synchronously before the call).
// During session handshake, the peer address is taken from the
// DTLSSession stored in SSL app_data.
int DTLSContext::CookieGenerateCallback(SSL* ssl,
                                        unsigned char* cookie,
                                        unsigned int* cookie_len) {
  SSL_CTX* ctx = SSL_get_SSL_CTX(ssl);
  DTLSContext* dtls_ctx = static_cast<DTLSContext*>(SSL_CTX_get_app_data(ctx));
  CHECK_NOT_NULL(dtls_ctx);

  unsigned char addr_buf[sizeof(struct sockaddr_storage)];
  size_t addr_len = 0;

  void* app_data = SSL_get_app_data(ssl);
  if (app_data != nullptr) {
    // Session handshake path.
    auto* session = static_cast<DTLSSession*>(app_data);
    const sockaddr* sa = session->remote_address().data();
    addr_len = SocketAddress::GetLength(sa);
    memcpy(addr_buf, sa, addr_len);
  } else {
    // DTLSv1_listen path — use the peer address stored on the context.
    const sockaddr* sa = dtls_ctx->current_cookie_peer_.data();
    addr_len = SocketAddress::GetLength(sa);
    memcpy(addr_buf, sa, addr_len);
  }

  unsigned int hmac_len = 0;
  unsigned char* result = HMAC(EVP_sha256(),
                               dtls_ctx->cookie_secret_.data(),
                               dtls_ctx->cookie_secret_.size(),
                               addr_buf,
                               addr_len,
                               cookie,
                               &hmac_len);

  if (result == nullptr) return 0;

  *cookie_len = hmac_len;
  return 1;
}

int DTLSContext::CookieVerifyCallback(SSL* ssl,
                                      const unsigned char* cookie,
                                      unsigned int cookie_len) {
  // Generate the expected cookie and compare.
  unsigned char expected[EVP_MAX_MD_SIZE];
  unsigned int expected_len = 0;

  if (CookieGenerateCallback(ssl, expected, &expected_len) != 1) {
    return 0;
  }

  if (cookie_len != expected_len) return 0;

  return CRYPTO_memcmp(cookie, expected, expected_len) == 0 ? 1 : 0;
}

int DTLSContext::ALPNSelectCallback(SSL* ssl,
                                    const unsigned char** out,
                                    unsigned char* outlen,
                                    const unsigned char* in,
                                    unsigned int inlen,
                                    void* arg) {
  DTLSContext* ctx = static_cast<DTLSContext*>(arg);

  if (ctx->alpn_protos_.empty()) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  int ret = SSL_select_next_proto(const_cast<unsigned char**>(out),
                                  outlen,
                                  ctx->alpn_protos_.data(),
                                  ctx->alpn_protos_.size(),
                                  in,
                                  inlen);

  if (ret != OPENSSL_NPN_NEGOTIATED) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  return SSL_TLSEXT_ERR_OK;
}

}  // namespace dtls
}  // namespace node

#endif  // HAVE_OPENSSL && HAVE_DTLS
