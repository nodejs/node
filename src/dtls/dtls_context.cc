#include "dtls_context.h"
#include "dtls_session.h"

#if HAVE_OPENSSL && HAVE_DTLS

#include <base_object-inl.h>
#include <crypto/crypto_bio.h>
#include <crypto/crypto_client_hello.h>
#include <crypto/crypto_tls_certificates.h>
#include <crypto/crypto_util.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <node_errors.h>
#include <node_sockaddr-inl.h>
#include <util-inl.h>
#include <uv.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/srtp.h>
#include <openssl/ssl.h>

#include <cstring>

namespace node {

using v8::Array;
using v8::ArrayBufferView;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;

namespace dtls {

namespace {
// The cookie secret is 32 bytes (256 bits), generated once per context and
// not rotated. Rotation would need the previous secret kept alive to validate
// cookies already in flight, which is the job the time window below already
// does: a cookie stops verifying once its window passes, whatever the secret.
// A fresh secret is generated on every context, so a restart invalidates
// outstanding cookies too.
constexpr size_t kCookieSecretLen = 32;
// Cookies are bound to a coarse time window so they expire. A cookie is
// accepted for the window it was minted in and the immediately preceding one,
// giving ~30-60s of validity -- ample for the cookie exchange while bounding
// how long a captured cookie can be replayed.
constexpr uint64_t kCookieWindowNs = 30ull * 1000 * 1000 * 1000;
}  // namespace

DTLSContext::DTLSContext(Environment* env,
                         Local<Object> wrap,
                         ncrypto::SSLCtxPointer ctx,
                         bool is_server)
    : BaseObject(env, wrap),
      ctx_(std::move(ctx)),
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

  // Keylog is a per-SSL_CTX setting, so register it once here rather than
  // re-registering it from every session constructor. The callback is inert
  // unless the session it resolves to has a keylog listener, so installing it
  // unconditionally costs nothing and no secret reaches the JS heap uninvited.
  SSL_CTX_set_keylog_callback(ctx_.get(), DTLSSession::SSLKeylogCallback);

  // Store pointer to this context in the SSL_CTX app data for callbacks.
  SSL_CTX_set_app_data(ctx_.get(), this);
}

void DTLSContext::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("cookie_secret", cookie_secret_.size());
  tracker->TrackFieldWithSize("alpn_protos", alpn_protos_.size());
  size_t psk_size = 0;
  for (const auto& [identity, key] : psk_identities_) {
    psk_size += identity.size() + key.size();
  }
  tracker->TrackFieldWithSize("psk_identities", psk_size);
  tracker->TrackFieldWithSize("psk_client_key", psk_client_key_.size());
  // Strong references to JavaScript functions held by a weak object. A
  // callback that closes over the server, which is the usual way to write
  // one, is a cycle through C++ that a heap snapshot cannot otherwise show.
  tracker->TrackField("psk_callback", psk_callback_);
  tracker->TrackField("sni_callback", sni_callback_);
  tracker->TrackField("sni_contexts", sni_contexts_);
}

// SSL ex_data slot holding the DTLSContext an SSL was created from, for the
// PSK callbacks, which unlike the ALPN and SNI ones have no argument of their
// own.
//
// On the SSL and not the SSL_CTX. SSL_set_SSL_CTX(), which is how SNI selects
// an identity, reassigns ssl->ctx but does not re-copy the PSK callbacks --
// those were installed on the SSL by SSL_new() and stay. Looking the context
// up through SSL_get_SSL_CTX() therefore found whichever context SNI had
// switched to, which is not the one whose PSK configuration is in force and
// is usually not configured for PSK at all.
static int PSKContextIndex() {
  static const int index =
      SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
  return index;
}

void DTLSContext::BindToSSL(SSL* ssl) {
  SSL_set_ex_data(ssl, PSKContextIndex(), this);
}

DTLSContext* DTLSContext::FromSSL(SSL* ssl) {
  return static_cast<DTLSContext*>(SSL_get_ex_data(ssl, PSKContextIndex()));
}

bool DTLSContext::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
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
    SetProtoMethod(
        isolate, tmpl, "setSessionIdContext", SetSessionIdContext);
    SetProtoMethod(isolate, tmpl, "setSNIContexts", SetSNIContexts);
    SetProtoMethod(isolate, tmpl, "setTicketKeys", SetTicketKeys);
    SetProtoMethod(isolate, tmpl, "setPSK", SetPSK);

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
  registry->Register(SetSessionIdContext);
  registry->Register(SetSNIContexts);
  registry->Register(SetTicketKeys);
  registry->Register(SetPSK);
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

  ncrypto::SSLCtxPointer ctx(SSL_CTX_new(method));
  if (!ctx) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                             "Failed to create DTLS SSL_CTX");
  }

  // Default to DTLS 1.2 only. DTLS 1.0 (based on TLS 1.1) is deprecated
  // by RFC 8996 and lacks AEAD cipher suites.
  //
  // Checked because the whole point is to refuse DTLS 1.0. An OpenSSL built
  // without DTLS 1.2 would fail these and, unchecked, leave a context whose
  // floor is whatever the build allows -- the deprecated version this is
  // meant to exclude.
  if (SSL_CTX_set_min_proto_version(ctx.get(), DTLS1_2_VERSION) != 1 ||
      SSL_CTX_set_max_proto_version(ctx.get(), DTLS1_2_VERSION) != 1) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Failed to restrict the DTLS version to 1.2");
  }

  // Disable OpenSSL's MTU querying (we manage MTU manually).
  SSL_CTX_set_options(ctx.get(), SSL_OP_NO_QUERY_MTU);

  // Name the workarounds rather than taking SSL_OP_ALL. Its membership is not
  // stable -- it has held SSL_OP_LEGACY_SERVER_CONNECT in the past and does
  // not now -- so using the macro means silently inheriting whatever a future
  // OpenSSL decides belongs in it.
  //
  // All four of its current members are TLS-specific and inert under DTLS 1.2,
  // which is the only version this supports. They are kept so that behaviour
  // is unchanged from taking SSL_OP_ALL, not because any is known to be
  // needed:
  //
  //   DONT_INSERT_EMPTY_FRAGMENTS  the 1/n-1 split for CBC in TLS 1.0; DTLS
  //                                1.2 has explicit IVs and does not do it
  //   TLSEXT_PADDING               pads ClientHello for an F5 bug
  //   CRYPTOPRO_TLSEXT_BUG         GOST client workaround
  //   SAFARI_ECDHE_ECDSA_BUG       Safari on OS X 10.8
  SSL_CTX_set_options(ctx.get(),
                      SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS |
                          SSL_OP_TLSEXT_PADDING | SSL_OP_CRYPTOPRO_TLSEXT_BUG |
                          SSL_OP_SAFARI_ECDHE_ECDSA_BUG);

  if (is_server) {
    // NOTE: SSL_OP_COOKIE_EXCHANGE must not be set here, on the context. It
    // would then be inherited by every SSL the context creates, including the
    // session SSLs built by CreateFromSSL() for peers that have *already*
    // completed the cookie exchange, which would leave them waiting for a
    // second one. DTLSv1_listen() sets the option on the individual SSL it is
    // given (d1_lib.c:804), which is the correct scope.

    // Enable session caching for session resumption.
    //
    // SSL_SESS_CACHE_NO_INTERNAL, matching node:tls and the client branch
    // below. The previous mode enabled OpenSSL's internal cache and then set
    // NO_AUTO_CLEAR, which is only coherent as node:tls uses it -- alongside
    // NO_INTERNAL, where there is no internal cache for the auto-clear to
    // walk. Enabled-plus-NO_AUTO_CLEAR instead meant every accepted session
    // was retained, with its master secret, for the 7200 second default
    // timeout and past it.
    SSL_CTX_set_session_cache_mode(
        ctx.get(), SSL_SESS_CACHE_SERVER | SSL_SESS_CACHE_NO_INTERNAL);
  } else {
    // Client session caching for resumption.
    SSL_CTX_set_session_cache_mode(
        ctx.get(), SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL);
  }

  // NOTE: We do NOT call SSL_CTX_set_default_verify_paths() here.
  // CA loading is handled in JS: if the user provides custom CAs, only
  // those are loaded (via addCACert). Otherwise, system default CAs are
  // loaded via loadDefaultCAs(). This matches Node.js TLS behavior.

  new DTLSContext(env, args.This(), std::move(ctx), is_server);
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
    return crypto::ThrowCryptoError(
        env, ERR_get_error(), "PEM_read_bio_X509");
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

  // Optional passphrase for an encrypted key. Absent is not the same as
  // empty: UsePrivateKey() substitutes an empty ByteSource when given
  // nullptr, and OpenSSL reports a decryption failure either way.
  crypto::ByteSource passphrase;
  if (args[1]->IsString()) {
    passphrase = crypto::ByteSource::FromString(env, args[1].As<String>());
  }

  switch (crypto::UsePrivateKey(ctx->ctx_.get(), bio, &passphrase)) {
    case crypto::PrivateKeyResult::kSuccess:
      break;
    case crypto::PrivateKeyResult::kParseError:
      return crypto::ThrowCryptoError(
          env, ERR_get_error(), "PEM_read_bio_PrivateKey");
    case crypto::PrivateKeyResult::kApplyError:
      return crypto::ThrowCryptoError(
          env, ERR_get_error(), "SSL_CTX_use_PrivateKey");
  }

  // Verify that the private key matches the certificate.
  if (SSL_CTX_check_private_key(ctx->ctx_.get()) != 1) {
    return crypto::ThrowCryptoError(
        env, ERR_get_error(), "Private key does not match certificate");
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
    // Client: advertise protocols to the server. Returns 0 on success.
    if (SSL_CTX_set_alpn_protos(ctx->ctx_.get(), data, len) != 0) {
      return THROW_ERR_CRYPTO_OPERATION_FAILED(
          env, "SSL_CTX_set_alpn_protos failed");
    }
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

namespace {
// Installed only where the application has taken responsibility for the
// authorization decision itself: a server that asked for a client certificate
// but disabled rejection. Returning 1 unconditionally keeps the handshake
// going; the verification result is still recorded and remains reachable
// through SSL_get_verify_result(), which is what session.authorized reports.
//
// Everywhere else the callback is left null so OpenSSL enforces, and a peer
// that fails verification receives a proper alert.
int AllowUnauthorizedCallback(int preverify_ok, X509_STORE_CTX* ctx) {
  return 1;
}
}  // namespace

void DTLSContext::SetVerifyMode(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());

  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsBoolean());

  int mode = args[0].As<Int32>()->Value();
  bool defer_to_application = args[1]->IsTrue();

  SSL_CTX_set_verify(
      ctx->ctx_.get(),
      mode,
      defer_to_application ? AllowUnauthorizedCallback : nullptr);
}

void DTLSContext::LoadDefaultCAs(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());

  // This populates the verification store only, not the client-CA list sent
  // in a CertificateRequest. That is deliberate. The bundled root store holds
  // on the order of 150 certificates, and advertising all of their
  // distinguished names would produce a CertificateRequest of tens of
  // kilobytes -- which, over a datagram transport, then has to be fragmented
  // across many packets, any of which can be lost. node:tls does the same:
  // SSL_CTX_add_client_CA() is called for explicitly supplied chains and PFX
  // bundles, never from UseDefaultRootCertStore().
  crypto::UseDefaultRootCertStore(ctx->env(), ctx->ctx_.get());
}

// Scopes cached sessions to this server. OpenSSL refuses to resume a session
// whose id context differs from the one on the accepting SSL, which is what
// stops a session established under one configuration being resumed under
// another. It matters most when client certificates are in use.
void DTLSContext::SetSessionIdContext(
    const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());
  Environment* env = ctx->env();

  CHECK(args[0]->IsString());
  Utf8Value sid_ctx(env->isolate(), args[0]);

  if (!SSL_CTX_set_session_id_context(
          ctx->ctx_.get(),
          reinterpret_cast<const unsigned char*>(*sid_ctx),
          sid_ctx.length())) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Failed to set session id context");
  }
}

// The wildcard key: the identity used when no host name matches.
constexpr const char* kSNIWildcard = "*";

// Ask JavaScript which identity to serve. Synchronous, like the PSK
// callbacks and for the same reason: suspending a DTLS handshake to await an
// answer would mean driving SSL_ERROR_WANT_X509_LOOKUP back through Cycle(),
// and a datagram peer is retransmitting while it waits.
DTLSContext* DTLSContext::SelectSNIContextFromCallback(
    SSL* ssl, const char* servername) {
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  // Called rather than MakeCallback'd: this runs inside SSL_do_handshake(),
  // and MakeCallback drains the tick queue as its scope exits, which would
  // run user code in the middle of OpenSSL's state machine. See
  // PSKServerCallback.
  TryCatch try_catch(env()->isolate());

  // A client that sent no SNI extension still reaches here, and the callback
  // is told so rather than being given an empty string it cannot distinguish
  // from one.
  Local<Value> name = Undefined(env()->isolate());
  if (servername != nullptr) {
    Local<String> name_str;
    if (!String::NewFromUtf8(env()->isolate(), servername).ToLocal(&name_str)) {
      ReportCallbackError(ssl, &try_catch);
      return nullptr;
    }
    // Reject a name that did not survive the round trip through UTF-8, so a
    // peer cannot offer bytes that reach JavaScript as replacement characters
    // and match a host it was never given.
    Utf8Value round_trip(env()->isolate(), name_str);
    if (round_trip != servername) return nullptr;
    name = name_str;
  }

  Local<Value> argv[] = {name};
  Local<Value> ret;
  if (!sni_callback_.Get(env()->isolate())
           ->Call(env()->context(), object(), 1, argv)
           .ToLocal(&ret)) {
    ReportCallbackError(ssl, &try_catch);
    return nullptr;
  }

  // Declining is not an error: it means this name is not served.
  if (!HasInstance(env(), ret)) return nullptr;

  DTLSContext* chosen;
  ASSIGN_OR_RETURN_UNWRAP(&chosen, ret.As<Object>(), nullptr);

  // Hold it. A context the callback built has no other owner once it
  // returns, and the handshake is not finished with it: SSL_set_SSL_CTX()
  // reassigns ssl->ctx, so a later callback resolving its configuration
  // through SSL_get_SSL_CTX() lands on this context and not on the one the
  // endpoint holds. Verified by pointer -- the PSK callback on a session
  // whose name was chosen here reports the chosen context, not the base.
  DTLSSession* session = static_cast<DTLSSession*>(SSL_get_app_data(ssl));
  if (session != nullptr) session->SetSNIContext(chosen);

  return chosen;
}

int DTLSContext::SNISelectCallback(SSL* ssl, int* ad, void* arg) {
  auto* ctx = static_cast<DTLSContext*>(arg);

  const char* servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);

  auto it = servername != nullptr ? ctx->sni_contexts_.find(servername)
                                  : ctx->sni_contexts_.end();
  if (it == ctx->sni_contexts_.end()) {
    it = ctx->sni_contexts_.find(kSNIWildcard);
  }

  DTLSContext* chosen =
      it != ctx->sni_contexts_.end() ? it->second.get() : nullptr;

  // The map answers first, so a configuration using only the map still runs
  // no JavaScript inside the handshake. The callback is reached only when it
  // has nothing to say.
  if (chosen == nullptr && !ctx->sni_callback_.IsEmpty()) {
    chosen = ctx->SelectSNIContextFromCallback(ssl, servername);
  }

  if (chosen == nullptr) {
    // Configuring sni without a "*" entry, or a callback that declines, is a
    // statement that only the named hosts are served, so refuse anything
    // else rather than falling back to the endpoint's own certificate.
    *ad = SSL_AD_UNRECOGNIZED_NAME;
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  SSL_CTX* selected = chosen->ssl_ctx();
  if (SSL_set_SSL_CTX(ssl, selected) != selected) {
    *ad = SSL_AD_INTERNAL_ERROR;
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  // Nothing further is needed. node:tls follows SSL_set_SSL_CTX() with
  // SSL_set1_verify_cert_store() and a duplicated client-CA list, which
  // suggests the switch leaves verification behind; it does not.
  // SSL_set_SSL_CTX() reassigns ssl->ctx (ssl_lib.c), and both the
  // verification store and the client-CA list are read through it, so they
  // move with the certificate. Adding the calls anyway changed no observed
  // behaviour: an SNI identity trusting only ca2 rejects a ca1 client
  // either way.
  //
  // Verify mode is deliberately not touched. requestCert and
  // rejectUnauthorized belong to the endpoint rather than the identity and
  // are configured once on its own context.
  return SSL_TLSEXT_ERR_OK;
}

void DTLSContext::SetSNIContexts(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());
  Environment* env = ctx->env();

  CHECK(args[0]->IsArray());
  Local<Array> entries = args[0].As<Array>();

  // Built to one side and swapped in, so a failure part way through cannot
  // leave the context serving a half-populated map.
  std::unordered_map<std::string, BaseObjectWeakPtr<DTLSContext>> next;

  uint32_t length = entries->Length();
  for (uint32_t i = 0; i < length; i += 2) {
    Local<Value> host;
    Local<Value> value;
    if (!entries->Get(env->context(), i).ToLocal(&host) ||
        !entries->Get(env->context(), i + 1).ToLocal(&value)) {
      return;
    }

    CHECK(host->IsString());
    CHECK(DTLSContext::HasInstance(env, value));

    DTLSContext* entry;
    ASSIGN_OR_RETURN_UNWRAP(&entry, value.As<Object>());

    Utf8Value hostname(env->isolate(), host);
    next[*hostname] = BaseObjectWeakPtr<DTLSContext>(entry);
  }

  ctx->sni_contexts_ = std::move(next);

  // Assigned either way. Only ever setting it meant a context reconfigured
  // with a map and no callback kept the callback it was given before, which
  // for SNI is a fail-open: a map with no '*' entry is meant to refuse an
  // unmatched name, and a leftover callback answers it instead.
  if (args[1]->IsFunction()) {
    ctx->sni_callback_.Reset(env->isolate(), args[1].As<Function>());
  } else {
    ctx->sni_callback_.Reset();
  }

  if (ctx->sni_contexts_.empty() && ctx->sni_callback_.IsEmpty()) {
    SSL_CTX_set_tlsext_servername_callback(ctx->ctx_.get(), nullptr);
    SSL_CTX_set_tlsext_servername_arg(ctx->ctx_.get(), nullptr);
    return;
  }

  SSL_CTX_set_tlsext_servername_callback(ctx->ctx_.get(), SNISelectCallback);
  SSL_CTX_set_tlsext_servername_arg(ctx->ctx_.get(), ctx);
}

// Hand an exception thrown by one of the handshake callbacks to the session
// it belongs to, so it fails that handshake instead of reaching the process
// as an uncaughtException. The exception must not still be pending when
// control returns to OpenSSL, which goes on to build an alert and unwind
// through Cycle()'s error path.
void DTLSContext::ReportCallbackError(SSL* ssl, TryCatch* try_catch) {
  if (!try_catch->HasCaught() || try_catch->HasTerminated()) return;

  DTLSSession* session = static_cast<DTLSSession*>(SSL_get_app_data(ssl));
  Local<Value> exception = try_catch->Exception();

  // Clear before reporting: EmitCallback runs JavaScript, which cannot be
  // entered with an exception still pending.
  try_catch->Reset();

  if (session != nullptr && !exception.IsEmpty()) {
    session->SetPendingError(exception);
  }
}

unsigned int DTLSContext::PSKServerCallback(SSL* ssl,
                                            const char* identity,
                                            unsigned char* psk,
                                            unsigned int max_psk_len) {
  DTLSContext* ctx = FromSSL(ssl);
  if (ctx == nullptr || identity == nullptr) return 0;

  // The map first, so a configuration that uses only the map never runs
  // JavaScript inside the handshake.
  auto it = ctx->psk_identities_.find(identity);
  if (it != ctx->psk_identities_.end()) {
    if (it->second.size() > max_psk_len) return 0;
    memcpy(psk, it->second.data(), it->second.size());
    return static_cast<unsigned int>(it->second.size());
  }

  if (ctx->psk_callback_.IsEmpty()) return 0;

  Environment* env = ctx->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  // Called rather than MakeCallback'd, deliberately. MakeCallback drains the
  // tick queue as its scope exits, and this runs inside SSL_do_handshake():
  // measured, a nextTick scheduled by the callback runs before OpenSSL's
  // state machine has finished the transition it is in the middle of. The
  // module's other MakeCallback uses all fire at safe points in Cycle(),
  // after SSL_do_handshake() has returned. node:tls accepts that risk here;
  // this does not, having a reentrancy guard that exists because of it.
  //
  // The cost is that exceptions are ours to handle: a pending one would
  // otherwise be carried back into OpenSSL and surface as an
  // uncaughtException, killing the process over one bad callback.
  TryCatch try_catch(env->isolate());

  Local<String> identity_str;
  if (!String::NewFromUtf8(env->isolate(), identity).ToLocal(&identity_str)) {
    ctx->ReportCallbackError(ssl, &try_catch);
    return 0;
  }

  // Reject an identity that did not survive the round trip through UTF-8:
  // otherwise a peer could offer bytes that reach JavaScript as replacement
  // characters and match an identity it was never given. node:tls does the
  // same check.
  Utf8Value round_trip(env->isolate(), identity_str);
  if (round_trip != identity) return 0;

  Local<Value> argv[] = {identity_str};
  Local<Value> ret;
  if (!ctx->psk_callback_.Get(env->isolate())
           ->Call(env->context(), ctx->object(), 1, argv)
           .ToLocal(&ret)) {
    ctx->ReportCallbackError(ssl, &try_catch);
    return 0;
  }

  if (!ret->IsArrayBufferView()) return 0;
  ArrayBufferViewContents<unsigned char> key(ret.As<ArrayBufferView>());
  if (key.length() > max_psk_len) return 0;  // measurement: allow empty

  memcpy(psk, key.data(), key.length());
  return static_cast<unsigned int>(key.length());
}

unsigned int DTLSContext::PSKClientCallback(SSL* ssl,
                                            const char* hint,
                                            char* identity,
                                            unsigned int max_identity_len,
                                            unsigned char* psk,
                                            unsigned int max_psk_len) {
  DTLSContext* ctx = FromSSL(ssl);
  if (ctx == nullptr) return 0;

  std::string use_identity = ctx->psk_client_identity_;
  std::vector<unsigned char> use_key = ctx->psk_client_key_;

  if (!ctx->psk_callback_.IsEmpty()) {
    Environment* env = ctx->env();
    HandleScope scope(env->isolate());
    Context::Scope context_scope(env->context());
    // See PSKServerCallback for why this is Call() and not MakeCallback().
    TryCatch try_catch(env->isolate());

    // The hint is optional and frequently absent (RFC 4279 section 5.2).
    Local<Value> hint_value = Undefined(env->isolate());
    if (hint != nullptr) {
      Local<String> hint_str;
      if (!String::NewFromUtf8(env->isolate(), hint).ToLocal(&hint_str)) {
        ctx->ReportCallbackError(ssl, &try_catch);
        return 0;
      }
      hint_value = hint_str;
    }

    Local<Value> argv[] = {hint_value};
    Local<Value> ret;
    if (!ctx->psk_callback_.Get(env->isolate())
             ->Call(env->context(), ctx->object(), 1, argv)
             .ToLocal(&ret)) {
      ctx->ReportCallbackError(ssl, &try_catch);
      return 0;
    }
    if (!ret->IsObject()) return 0;

    // Reading these can throw: the object is whatever the callback returned,
    // so a getter or a Proxy trap runs here. Report it rather than returning
    // with it caught but unreported, which loses the error the caller threw
    // and leaves it to unwind into OpenSSL when the TryCatch is destroyed.
    Local<Object> obj = ret.As<Object>();
    Local<Value> id_val;
    Local<Value> key_val;
    if (!obj->Get(env->context(), FIXED_ONE_BYTE_STRING(env->isolate(),
                                                        "identity"))
             .ToLocal(&id_val) ||
        !obj->Get(env->context(), FIXED_ONE_BYTE_STRING(env->isolate(), "key"))
             .ToLocal(&key_val)) {
      ctx->ReportCallbackError(ssl, &try_catch);
      return 0;
    }
    if (!id_val->IsString() || !key_val->IsArrayBufferView()) return 0;

    Utf8Value id(env->isolate(), id_val);
    ArrayBufferViewContents<unsigned char> key(key_val.As<ArrayBufferView>());
    use_identity.assign(*id, id.length());
    use_key.assign(key.data(), key.data() + key.length());
  }

  if (use_identity.empty() || use_key.empty()) return 0;
  // The identity is written as a C string, so it needs room for the
  // terminator OpenSSL expects.
  if (use_identity.size() + 1 > max_identity_len) return 0;
  if (use_key.size() > max_psk_len) return 0;

  memcpy(identity, use_identity.c_str(), use_identity.size() + 1);
  memcpy(psk, use_key.data(), use_key.size());
  return static_cast<unsigned int>(use_key.size());
}

void DTLSContext::SetTicketKeys(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());

  CHECK(args[0]->IsArrayBufferView());
  ArrayBufferViewContents<unsigned char> buf(args[0].As<ArrayBufferView>());

  // Key name, HMAC key and AES key concatenated. node:tls installs its own
  // callback and defines a 48-byte layout of its own; this uses OpenSSL's
  // native keys instead, which are longer -- a 32-byte HMAC key and AES-256
  // rather than 16 and AES-128 -- and need no callback. The length is asked
  // for rather than hardcoded, since it comes from a private OpenSSL header.
  long expected =  // NOLINT(runtime/int) -- SSL_CTX_ctrl returns long
      SSL_CTX_set_tlsext_ticket_keys(ctx->ctx_.get(), nullptr, 0);

  if (static_cast<long>(buf.length()) != expected) {  // NOLINT(runtime/int)
    return THROW_ERR_INVALID_ARG_VALUE(
        ctx->env(),
        "options.ticketKeys must be exactly %ld bytes",
        expected);
  }

  if (SSL_CTX_set_tlsext_ticket_keys(
          ctx->ctx_.get(),
          const_cast<unsigned char*>(buf.data()),
          buf.length()) != 1) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(ctx->env(),
                                      "Failed to set session ticket keys");
  }
}

// setPSK(entries, hint, clientIdentity, clientKey, callback)
//
// |entries| is a flattened [identity, key, ...] array for a server; the
// client fields carry the single identity a client presents. Any of them may
// be absent: a server may rely entirely on the callback, and a client may
// rely on it too.
void DTLSContext::SetPSK(const FunctionCallbackInfo<Value>& args) {
  DTLSContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.This());
  Environment* env = ctx->env();

  if (args[0]->IsArray()) {
    Local<Array> entries = args[0].As<Array>();
    std::unordered_map<std::string, std::vector<unsigned char>> next;
    uint32_t length = entries->Length();
    for (uint32_t i = 0; i < length; i += 2) {
      Local<Value> identity;
      Local<Value> key;
      if (!entries->Get(env->context(), i).ToLocal(&identity) ||
          !entries->Get(env->context(), i + 1).ToLocal(&key)) {
        return;
      }
      CHECK(identity->IsString());
      CHECK(key->IsArrayBufferView());
      Utf8Value id(env->isolate(), identity);
      ArrayBufferViewContents<unsigned char> buf(key.As<ArrayBufferView>());
      next[std::string(*id, id.length())] =
          std::vector<unsigned char>(buf.data(), buf.data() + buf.length());
    }
    ctx->psk_identities_ = std::move(next);
  }

  if (args[1]->IsString()) {
    Utf8Value hint(env->isolate(), args[1]);
    ctx->psk_identity_hint_.assign(*hint, hint.length());
  }

  if (args[2]->IsString()) {
    Utf8Value identity(env->isolate(), args[2]);
    ctx->psk_client_identity_.assign(*identity, identity.length());
  }

  if (args[3]->IsArrayBufferView()) {
    ArrayBufferViewContents<unsigned char> key(args[3].As<ArrayBufferView>());
    ctx->psk_client_key_.assign(key.data(), key.data() + key.length());
  }

  if (args[4]->IsFunction()) {
    ctx->psk_callback_.Reset(env->isolate(), args[4].As<Function>());
  } else {
    ctx->psk_callback_.Reset();
  }

  if (ctx->is_server_) {
    SSL_CTX_set_psk_server_callback(ctx->ctx_.get(), PSKServerCallback);
    if (!ctx->psk_identity_hint_.empty() &&
        SSL_CTX_use_psk_identity_hint(
            ctx->ctx_.get(), ctx->psk_identity_hint_.c_str()) != 1) {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                        "Failed to set PSK identity hint");
    }
  } else {
    SSL_CTX_set_psk_client_callback(ctx->ctx_.get(), PSKClientCallback);
  }
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

// 1 byte family tag + 2 port + 16 address + 4 scope id.
constexpr size_t kMaxCanonicalAddrLen = 23;

// Writes the parts of an address that identify a peer -- family, port,
// address, and for IPv6 the scope id -- into `out`, returning the number of
// bytes written, or 0 for an address we do not understand.
//
// Deliberately not a memcpy of the sockaddr. That would fold in sin_zero and
// sin6_flowinfo: padding the kernel need not zero, and a QoS label that may
// differ between two datagrams from the same host. Either would change the
// cookie for an unchanged peer and fail the handshake. scope id is included
// because it does identify a link-local peer.
size_t CanonicalizeAddress(const sockaddr* sa, unsigned char* out) {
  switch (sa->sa_family) {
    case AF_INET: {
      const sockaddr_in* v4 = reinterpret_cast<const sockaddr_in*>(sa);
      out[0] = 4;
      memcpy(out + 1, &v4->sin_port, 2);
      memcpy(out + 3, &v4->sin_addr, 4);
      return 7;
    }
    case AF_INET6: {
      const sockaddr_in6* v6 = reinterpret_cast<const sockaddr_in6*>(sa);
      out[0] = 6;
      memcpy(out + 1, &v6->sin6_port, 2);
      memcpy(out + 3, &v6->sin6_addr, 16);
      memcpy(out + 19, &v6->sin6_scope_id, 4);
      return 23;
    }
    default:
      return 0;
  }
}

// HMAC-SHA256 cookie derived from the peer's address and a coarse time window
// so cookies expire (see kCookieWindowNs).
//
// The cookie is not bound to the ClientHello, which RFC 6347 section 4.2.1
// recommends, so within one window a cookie issued to an address is accepted
// for any handshake attempt from that address. The cookie's purpose --
// proving the peer can receive at the address it claims, so the handshake
// cannot be used to amplify traffic at a third party -- is unaffected by
// that. Binding it via SSL_get_client_random() was tried and does not work:
// the random is not populated consistently across the generate and verify
// callbacks during DTLSv1_listen(), and every handshake fails with a
// handshake_failure alert. Doing it would mean lifting the 32-byte random out
// of the raw ClientHello, which CouldBeClientHello() already walks, and
// stashing it next to current_cookie_peer_. During DTLSv1_listen() the peer
// address comes from DTLSContext::current_cookie_peer_ (set synchronously
// before the call); during the session handshake it comes from the
// DTLSSession stored in SSL app_data.
bool DTLSContext::ComputeCookie(SSL* ssl,
                                uint64_t window,
                                unsigned char* out,
                                unsigned int* out_len) {
  SSL_CTX* ctx = SSL_get_SSL_CTX(ssl);
  DTLSContext* dtls_ctx = static_cast<DTLSContext*>(SSL_CTX_get_app_data(ctx));
  CHECK_NOT_NULL(dtls_ctx);

  // Message = canonical peer address followed by the 8-byte window counter.
  unsigned char msg[kMaxCanonicalAddrLen + sizeof(uint64_t)];
  size_t addr_len = 0;

  void* app_data = SSL_get_app_data(ssl);
  if (app_data != nullptr) {
    // Session handshake path.
    const sockaddr* sa =
        static_cast<DTLSSession*>(app_data)->remote_address().data();
    addr_len = CanonicalizeAddress(sa, msg);
  } else {
    // DTLSv1_listen path -- use the peer address stored on the context.
    const sockaddr* sa = dtls_ctx->current_cookie_peer_.data();
    addr_len = CanonicalizeAddress(sa, msg);
  }

  // Fail closed rather than deriving a cookie from an address we could not
  // make sense of.
  if (addr_len == 0) return false;

  // Append the window counter in a fixed byte order.
  for (size_t i = 0; i < sizeof(uint64_t); i++) {
    msg[addr_len + i] = static_cast<unsigned char>((window >> (8 * i)) & 0xff);
  }

  unsigned char* result = HMAC(EVP_sha256(),
                               dtls_ctx->cookie_secret_.data(),
                               dtls_ctx->cookie_secret_.size(),
                               msg,
                               addr_len + sizeof(uint64_t),
                               out,
                               out_len);
  return result != nullptr;
}

int DTLSContext::CookieGenerateCallback(SSL* ssl,
                                        unsigned char* cookie,
                                        unsigned int* cookie_len) {
  const uint64_t window = uv_hrtime() / kCookieWindowNs;
  return ComputeCookie(ssl, window, cookie, cookie_len) ? 1 : 0;
}

int DTLSContext::CookieVerifyCallback(SSL* ssl,
                                      const unsigned char* cookie,
                                      unsigned int cookie_len) {
  const uint64_t window = uv_hrtime() / kCookieWindowNs;

  // Accept a cookie minted in the current window or the immediately preceding
  // one, so a handshake that straddles a window boundary still succeeds.
  unsigned char expected[EVP_MAX_MD_SIZE];
  unsigned int expected_len = 0;
  for (int i = 0; i < 2; i++) {
    if (i == 1 && window == 0) break;
    if (ComputeCookie(ssl, window - i, expected, &expected_len) &&
        cookie_len == expected_len &&
        CRYPTO_memcmp(cookie, expected, expected_len) == 0) {
      return 1;
    }
  }
  return 0;
}

int DTLSContext::ALPNSelectCallback(SSL* ssl,
                                    const unsigned char** out,
                                    unsigned char* outlen,
                                    const unsigned char* in,
                                    unsigned int inlen,
                                    void* arg) {
  DTLSContext* ctx = static_cast<DTLSContext*>(arg);

  // This server does not do ALPN. Decline the extension and let the handshake
  // continue: the client offering protocols to a server that has none
  // configured is not an error. OpenSSL only calls this at all when the client
  // sent the extension, so a client that offered nothing never reaches here.
  if (ctx->alpn_protos_.empty()) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  auto selected = crypto::SelectNextProtocol(
      {ctx->alpn_protos_.data(), ctx->alpn_protos_.size()}, {in, inlen});

  if (!selected.has_value()) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  crypto::SetSelectedProtocol(out, outlen, *selected);
  return SSL_TLSEXT_ERR_OK;
}

}  // namespace dtls
}  // namespace node

#endif  // HAVE_OPENSSL && HAVE_DTLS
