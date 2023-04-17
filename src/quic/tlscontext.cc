#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC

#include "tlscontext.h"
#include "bindingdata.h"
#include "defs.h"
#include "transportparams.h"
#include <base_object-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_openssl.h>
#include <openssl/ssl.h>
#include <v8.h>

namespace node {

using v8::ArrayBuffer;
using v8::BackingStore;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Value;

namespace quic {

// TODO(@jasnell): This session class is just a placeholder.
// The real session impl will be added in a separate commit.
class Session {
 public:
  operator ngtcp2_conn*() { return nullptr; }
  void EmitKeylog(const char* line) const {}
  void EmitSessionTicket(Store&& store) {}
  void SetStreamOpenAllowed() {}
  bool is_destroyed() const { return false; }
  bool wants_session_ticket() const { return false; }
};

namespace {
constexpr size_t kMaxAlpnLen = 255;

int AllowEarlyDataCallback(SSL* ssl, void* arg) {
  // Currently, we always allow early data. Later we might make
  // it configurable.
  return 1;
}

int NewSessionCallback(SSL* ssl, SSL_SESSION* session) {
  // We use this event to trigger generation of the SessionTicket
  // if the user has requested to receive it.
  return TLSContext::From(ssl).OnNewSession(session);
}

void KeylogCallback(const SSL* ssl, const char* line) {
  TLSContext::From(ssl).Keylog(line);
}

int AlpnSelectionCallback(SSL* ssl,
                          const unsigned char** out,
                          unsigned char* outlen,
                          const unsigned char* in,
                          unsigned int inlen,
                          void* arg) {
  auto& context = TLSContext::From(ssl);

  auto requested = context.options().alpn;
  if (requested.length() > kMaxAlpnLen) return SSL_TLSEXT_ERR_NOACK;

  // The Session supports exactly one ALPN identifier. If that does not match
  // any of the ALPN identifiers provided in the client request, then we fail
  // here. Note that this will not fail the TLS handshake, so we have to check
  // later if the ALPN matches the expected identifier or not.
  //
  // We might eventually want to support the ability to negotiate multiple
  // possible ALPN's on a single endpoint/session but for now, we only support
  // one.
  if (SSL_select_next_proto(
          const_cast<unsigned char**>(out),
          outlen,
          reinterpret_cast<const unsigned char*>(requested.c_str()),
          requested.length(),
          in,
          inlen) == OPENSSL_NPN_NO_OVERLAP) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  return SSL_TLSEXT_ERR_OK;
}

BaseObjectPtr<crypto::SecureContext> InitializeSecureContext(
    Side side, Environment* env, const TLSContext::Options& options) {
  auto context = crypto::SecureContext::Create(env);

  auto& ctx = context->ctx();

  switch (side) {
    case Side::SERVER: {
      ctx.reset(SSL_CTX_new(TLS_server_method()));
      SSL_CTX_set_app_data(ctx.get(), context);

      if (ngtcp2_crypto_openssl_configure_server_context(ctx.get()) != 0) {
        return BaseObjectPtr<crypto::SecureContext>();
      }

      SSL_CTX_set_max_early_data(ctx.get(), UINT32_MAX);
      SSL_CTX_set_allow_early_data_cb(
          ctx.get(), AllowEarlyDataCallback, nullptr);
      SSL_CTX_set_options(ctx.get(),
                          (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
                              SSL_OP_SINGLE_ECDH_USE |
                              SSL_OP_CIPHER_SERVER_PREFERENCE |
                              SSL_OP_NO_ANTI_REPLAY);
      SSL_CTX_set_mode(ctx.get(), SSL_MODE_RELEASE_BUFFERS);
      SSL_CTX_set_alpn_select_cb(ctx.get(), AlpnSelectionCallback, nullptr);
      SSL_CTX_set_session_ticket_cb(ctx.get(),
                                    SessionTicket::GenerateCallback,
                                    SessionTicket::DecryptedCallback,
                                    nullptr);

      const unsigned char* sid_ctx = reinterpret_cast<const unsigned char*>(
          options.session_id_ctx.c_str());
      SSL_CTX_set_session_id_context(
          ctx.get(), sid_ctx, options.session_id_ctx.length());

      break;
    }
    case Side::CLIENT: {
      ctx.reset(SSL_CTX_new(TLS_client_method()));
      SSL_CTX_set_app_data(ctx.get(), context);

      if (ngtcp2_crypto_openssl_configure_client_context(ctx.get()) != 0) {
        return BaseObjectPtr<crypto::SecureContext>();
      }

      SSL_CTX_set_session_cache_mode(
          ctx.get(), SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL_STORE);
      SSL_CTX_sess_set_new_cb(ctx.get(), NewSessionCallback);
      break;
    }
    default:
      UNREACHABLE();
  }

  SSL_CTX_set_default_verify_paths(ctx.get());

  if (options.keylog) SSL_CTX_set_keylog_callback(ctx.get(), KeylogCallback);

  if (SSL_CTX_set_ciphersuites(ctx.get(), options.ciphers.c_str()) != 1) {
    return BaseObjectPtr<crypto::SecureContext>();
  }

  if (SSL_CTX_set1_groups_list(ctx.get(), options.groups.c_str()) != 1) {
    return BaseObjectPtr<crypto::SecureContext>();
  }

  // Handle CA certificates...

  const auto addCACert = [&](uv_buf_t ca) {
    crypto::ClearErrorOnReturn clear_error_on_return;
    crypto::BIOPointer bio = crypto::NodeBIO::NewFixed(ca.base, ca.len);
    if (!bio) return false;
    context->SetCACert(bio);
    return true;
  };

  const auto addRootCerts = [&] {
    crypto::ClearErrorOnReturn clear_error_on_return;
    context->SetRootCerts();
  };

  if (!options.ca.empty()) {
    for (auto& ca : options.ca) {
      if (!addCACert(ca)) {
        return BaseObjectPtr<crypto::SecureContext>();
      }
    }
  } else {
    addRootCerts();
  }

  // Handle Certs

  const auto addCert = [&](uv_buf_t cert) {
    crypto::ClearErrorOnReturn clear_error_on_return;
    crypto::BIOPointer bio = crypto::NodeBIO::NewFixed(cert.base, cert.len);
    if (!bio) return Just(false);
    auto ret = context->AddCert(env, std::move(bio));
    return ret;
  };

  for (auto& cert : options.certs) {
    if (!addCert(cert).IsJust()) {
      return BaseObjectPtr<crypto::SecureContext>();
    }
  }

  // Handle keys

  const auto addKey = [&](auto& key) {
    crypto::ClearErrorOnReturn clear_error_on_return;
    return context->UseKey(env, key);
    // TODO(@jasnell): Maybe SSL_CTX_check_private_key also?
  };

  for (auto& key : options.keys) {
    if (!addKey(key).IsJust()) {
      return BaseObjectPtr<crypto::SecureContext>();
    }
  }

  // Handle CRL

  const auto addCRL = [&](uv_buf_t crl) {
    crypto::ClearErrorOnReturn clear_error_on_return;
    crypto::BIOPointer bio = crypto::NodeBIO::NewFixed(crl.base, crl.len);
    if (!bio) return Just(false);
    return context->SetCRL(env, bio);
  };

  for (auto& crl : options.crl) {
    if (!addCRL(crl).IsJust()) {
      return BaseObjectPtr<crypto::SecureContext>();
    }
  }

  // TODO(@jasnell): Possibly handle other bits. Such a pfx, client cert engine,
  // and session timeout.
  return BaseObjectPtr<crypto::SecureContext>(context);
}

void EnableTrace(Environment* env, crypto::BIOPointer* bio, SSL* ssl) {
#if HAVE_SSL_TRACE
  static bool warn_trace_tls = true;
  if (warn_trace_tls) {
    warn_trace_tls = false;
    ProcessEmitWarning(env,
                       "Enabling --trace-tls can expose sensitive data in "
                       "the resulting log");
  }
  if (!*bio) {
    bio->reset(BIO_new_fp(stderr, BIO_NOCLOSE | BIO_FP_TEXT));
    SSL_set_msg_callback(
        ssl,
        [](int write_p,
           int version,
           int content_type,
           const void* buf,
           size_t len,
           SSL* ssl,
           void* arg) -> void {
          crypto::MarkPopErrorOnReturn mark_pop_error_on_return;
          SSL_trace(write_p, version, content_type, buf, len, ssl, arg);
        });
    SSL_set_msg_callback_arg(ssl, bio->get());
  }
#endif
}

template <typename T, typename Opt, std::vector<T> Opt::*member>
bool SetOption(Environment* env,
               Opt* options,
               const v8::Local<v8::Object>& object,
               const v8::Local<v8::String>& name) {
  v8::Local<v8::Value> value;
  if (!object->Get(env->context(), name).ToLocal(&value)) return false;

  // The value can be either a single item or an array of items.

  if (value->IsArray()) {
    auto context = env->context();
    auto values = value.As<v8::Array>();
    uint32_t count = values->Length();
    for (uint32_t n = 0; n < count; n++) {
      v8::Local<v8::Value> item;
      if (!values->Get(context, n).ToLocal(&item)) {
        return false;
      }
      if constexpr (std::is_same<T, std::shared_ptr<crypto::KeyObjectData>>::
                        value) {
        if (crypto::KeyObjectHandle::HasInstance(env, item)) {
          crypto::KeyObjectHandle* handle;
          ASSIGN_OR_RETURN_UNWRAP(&handle, item, false);
          (options->*member).push_back(handle->Data());
        } else {
          return false;
        }
      } else if constexpr (std::is_same<T, Store>::value) {
        if (item->IsArrayBufferView()) {
          (options->*member).emplace_back(item.As<v8::ArrayBufferView>());
        } else if (item->IsArrayBuffer()) {
          (options->*member).emplace_back(item.As<v8::ArrayBuffer>());
        } else {
          return false;
        }
      }
    }
  } else {
    if constexpr (std::is_same<T,
                               std::shared_ptr<crypto::KeyObjectData>>::value) {
      if (crypto::KeyObjectHandle::HasInstance(env, value)) {
        crypto::KeyObjectHandle* handle;
        ASSIGN_OR_RETURN_UNWRAP(&handle, value, false);
        (options->*member).push_back(handle->Data());
      } else {
        return false;
      }
    } else if constexpr (std::is_same<T, Store>::value) {
      if (value->IsArrayBufferView()) {
        (options->*member).emplace_back(value.As<v8::ArrayBufferView>());
      } else if (value->IsArrayBuffer()) {
        (options->*member).emplace_back(value.As<v8::ArrayBuffer>());
      } else {
        return false;
      }
    }
  }
  return true;
}
}  // namespace

Side TLSContext::side() const {
  return side_;
}

const TLSContext::Options& TLSContext::options() const {
  return options_;
}

inline const TLSContext& TLSContext::From(const SSL* ssl) {
  auto ref = static_cast<ngtcp2_crypto_conn_ref*>(SSL_get_app_data(ssl));
  TLSContext* context = ContainerOf(&TLSContext::conn_ref_, ref);
  return *context;
}

inline TLSContext& TLSContext::From(SSL* ssl) {
  auto ref = static_cast<ngtcp2_crypto_conn_ref*>(SSL_get_app_data(ssl));
  TLSContext* context = ContainerOf(&TLSContext::conn_ref_, ref);
  return *context;
}

TLSContext::TLSContext(Environment* env,
                       Side side,
                       Session* session,
                       const Options& options)
    : conn_ref_({getConnection, this}),
      side_(side),
      env_(env),
      session_(session),
      options_(options),
      secure_context_(InitializeSecureContext(side, env, options)) {
  CHECK(secure_context_);
  ssl_.reset(SSL_new(secure_context_->ctx().get()));
  CHECK(ssl_ && SSL_is_quic(ssl_.get()));

  SSL_set_app_data(ssl_.get(), &conn_ref_);
  SSL_set_verify(ssl_.get(), SSL_VERIFY_NONE, crypto::VerifyCallback);

  // Enable tracing if the `--trace-tls` command line flag is used.
  if (UNLIKELY(env->options()->trace_tls || options.enable_tls_trace))
    EnableTrace(env, &bio_trace_, ssl_.get());

  switch (side) {
    case Side::CLIENT: {
      SSL_set_connect_state(ssl_.get());
      CHECK_EQ(0,
               SSL_set_alpn_protos(ssl_.get(),
                                   reinterpret_cast<const unsigned char*>(
                                       options_.alpn.c_str()),
                                   options_.alpn.length()));
      CHECK_EQ(0,
               SSL_set_tlsext_host_name(ssl_.get(), options_.hostname.c_str()));
      break;
    }
    case Side::SERVER: {
      SSL_set_accept_state(ssl_.get());
      if (options.request_peer_certificate) {
        int verify_mode = SSL_VERIFY_PEER;
        if (options.reject_unauthorized)
          verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        SSL_set_verify(ssl_.get(), verify_mode, crypto::VerifyCallback);
      }
      break;
    }
    default:
      UNREACHABLE();
  }
}

void TLSContext::Start() {
  ngtcp2_conn_set_tls_native_handle(*session_, ssl_.get());

  TransportParams tp(TransportParams::Type::ENCRYPTED_EXTENSIONS,
                     ngtcp2_conn_get_local_transport_params(*session_));
  Store store = tp.Encode(env_);
  if (store && store.length() > 0) {
    ngtcp2_vec vec = store;
    SSL_set_quic_transport_params(ssl_.get(), vec.base, vec.len);
  }
}

void TLSContext::Keylog(const char* line) const {
  session_->EmitKeylog(line);
}

int TLSContext::Receive(ngtcp2_crypto_level crypto_level,
                        uint64_t offset,
                        const ngtcp2_vec& vec) {
  // ngtcp2 provides an implementation of this in
  // ngtcp2_crypto_recv_crypto_data_cb but given that we are using the
  // implementation specific error codes below, we can't use it.

  if (UNLIKELY(session_->is_destroyed())) return NGTCP2_ERR_CALLBACK_FAILURE;

  // Internally, this passes the handshake data off to openssl for processing.
  // The handshake may or may not complete.
  int ret = ngtcp2_crypto_read_write_crypto_data(
      *session_, crypto_level, vec.base, vec.len);

  switch (ret) {
    case 0:
    // Fall-through

    // In either of following cases, the handshake is being paused waiting for
    // user code to take action (for instance OCSP requests or client hello
    // modification)
    case NGTCP2_CRYPTO_OPENSSL_ERR_TLS_WANT_X509_LOOKUP:
      [[fallthrough]];
    case NGTCP2_CRYPTO_OPENSSL_ERR_TLS_WANT_CLIENT_HELLO_CB:
      return 0;
  }
  return ret;
}

int TLSContext::OnNewSession(SSL_SESSION* session) {
  // Used to generate and emit a SessionTicket for TLS session resumption.

  // If there is nothing listening for the session ticket, don't both emitting.
  if (!session_->wants_session_ticket()) return 0;

  // Pre-fight to see how much space we need to allocate for the session ticket.
  size_t size = i2d_SSL_SESSION(session, nullptr);

  if (size > 0 && size < crypto::SecureContext::kMaxSessionSize) {
    // Generate the actual ticket. If this fails, we'll simply carry on without
    // emitting the ticket.
    std::shared_ptr<BackingStore> ticket =
        ArrayBuffer::NewBackingStore(env_->isolate(), size);
    unsigned char* data = reinterpret_cast<unsigned char*>(ticket->Data());
    if (i2d_SSL_SESSION(session, &data) <= 0) return 0;
    session_->EmitSessionTicket(Store(std::move(ticket), size));
  }
  // If size == 0, there's no session ticket data to emit. Let's ignore it
  // and continue without emitting the sessionticket event.

  return 0;
}

bool TLSContext::InitiateKeyUpdate() {
  if (session_->is_destroyed() || in_key_update_) return false;
  auto leave = OnScopeLeave([this] { in_key_update_ = false; });
  in_key_update_ = true;

  return ngtcp2_conn_initiate_key_update(*session_, uv_hrtime()) == 0;
}

int TLSContext::VerifyPeerIdentity() {
  return crypto::VerifyPeerCertificate(ssl_);
}

void TLSContext::MaybeSetEarlySession(const SessionTicket& sessionTicket) {
  TransportParams rtp(TransportParams::Type::ENCRYPTED_EXTENSIONS,
                      sessionTicket.transport_params());

  // Ignore invalid remote transport parameters.
  if (!rtp) return;

  uv_buf_t buf = sessionTicket.ticket();
  crypto::SSLSessionPointer ticket = crypto::GetTLSSession(
      reinterpret_cast<unsigned char*>(buf.base), buf.len);

  // Silently ignore invalid TLS session
  if (!ticket || !SSL_SESSION_get_max_early_data(ticket.get())) return;

  // The early data will just be ignored if it's invalid.
  if (crypto::SetTLSSession(ssl_, ticket)) {
    ngtcp2_conn_set_early_remote_transport_params(*session_, rtp);
    session_->SetStreamOpenAllowed();
  }
}

void TLSContext::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("options", options_);
  tracker->TrackField("secure_context", secure_context_);
}

MaybeLocal<Object> TLSContext::cert(Environment* env) const {
  return crypto::X509Certificate::GetCert(env, ssl_);
}

MaybeLocal<Object> TLSContext::peer_cert(Environment* env) const {
  crypto::X509Certificate::GetPeerCertificateFlag flag =
      side_ == Side::SERVER
          ? crypto::X509Certificate::GetPeerCertificateFlag::SERVER
          : crypto::X509Certificate::GetPeerCertificateFlag::NONE;
  return crypto::X509Certificate::GetPeerCert(env, ssl_, flag);
}

MaybeLocal<Value> TLSContext::cipher_name(Environment* env) const {
  return crypto::GetCurrentCipherName(env, ssl_);
}

MaybeLocal<Value> TLSContext::cipher_version(Environment* env) const {
  return crypto::GetCurrentCipherVersion(env, ssl_);
}

MaybeLocal<Object> TLSContext::ephemeral_key(Environment* env) const {
  return crypto::GetEphemeralKey(env, ssl_);
}

const std::string_view TLSContext::servername() const {
  const char* servername = crypto::GetServerName(ssl_.get());
  return servername != nullptr ? std::string_view(servername)
                               : std::string_view();
}

const std::string_view TLSContext::alpn() const {
  const unsigned char* alpn_buf = nullptr;
  unsigned int alpnlen;
  SSL_get0_alpn_selected(ssl_.get(), &alpn_buf, &alpnlen);
  return alpnlen ? std::string_view(reinterpret_cast<const char*>(alpn_buf),
                                    alpnlen)
                 : std::string_view();
}

bool TLSContext::early_data_was_accepted() const {
  return (early_data_ &&
          SSL_get_early_data_status(ssl_.get()) == SSL_EARLY_DATA_ACCEPTED);
}

void TLSContext::Options::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("keys", keys);
  tracker->TrackField("certs", certs);
  tracker->TrackField("ca", ca);
  tracker->TrackField("crl", crl);
}

ngtcp2_conn* TLSContext::getConnection(ngtcp2_crypto_conn_ref* ref) {
  TLSContext* context = ContainerOf(&TLSContext::conn_ref_, ref);
  return *context->session_;
}

Maybe<const TLSContext::Options> TLSContext::Options::From(Environment* env,
                                                           Local<Value> value) {
  if (value.IsEmpty() || !value->IsObject()) {
    return Nothing<const Options>();
  }

  auto& state = BindingData::Get(env);
  auto params = value.As<Object>();
  Options options;

#define SET_VECTOR(Type, name)                                                 \
  SetOption<Type, TLSContext::Options, &TLSContext::Options::name>(            \
      env, &options, params, state.name##_string())

#define SET(name)                                                              \
  SetOption<TLSContext::Options, &TLSContext::Options::name>(                  \
      env, &options, params, state.name##_string())

  if (!SET(keylog) || !SET(reject_unauthorized) || !SET(enable_tls_trace) ||
      !SET(request_peer_certificate) || !SET(verify_hostname_identity) ||
      !SET(alpn) || !SET(hostname) || !SET(session_id_ctx) || !SET(ciphers) ||
      !SET(groups) ||
      !SET_VECTOR(std::shared_ptr<crypto::KeyObjectData>, keys) ||
      !SET_VECTOR(Store, certs) || !SET_VECTOR(Store, ca) ||
      !SET_VECTOR(Store, crl)) {
    return Nothing<const Options>();
  }

  return Just<const Options>(options);
}

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
