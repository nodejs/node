#include "crypto.h"
#include <async_wrap-inl.h>
#include <crypto/crypto_bio.h>
#include <crypto/crypto_common.h>
#include <crypto/crypto_context.h>
#include <crypto/crypto_random.h>
#include <crypto/crypto_util.h>
#include <debug_utils-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/crypto/shared.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_openssl.h>
#include <node_process-inl.h>
#include <node_sockaddr-inl.h>
#include <util.h>
#include <v8.h>
#include "openssl/ssl.h"
#include "quic/defs.h"
#include "session.h"

namespace node {

using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::FunctionTemplate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Uint8Array;
using v8::Value;

namespace quic {

// ======================================================================================
namespace {
class AeadContextPointer final {
 public:
  enum class Mode { ENCRYPT, DECRYPT };

  QUIC_MOVE_NO_COPY(AeadContextPointer)

  ~AeadContextPointer() { ngtcp2_crypto_aead_ctx_free(&ctx_); }

  inline ngtcp2_crypto_aead_ctx* operator->() { return &ctx_; }
  inline const ngtcp2_crypto_aead_ctx* operator->() const { return &ctx_; }
  inline ngtcp2_crypto_aead_ctx& operator*() { return ctx_; }
  inline const ngtcp2_crypto_aead_ctx& operator*() const { return ctx_; }

  inline const ngtcp2_crypto_aead_ctx* get() const { return &ctx_; }

  static AeadContextPointer forEncrypt(const uint8_t* key,
                                       const ngtcp2_crypto_aead& aead) {
    AeadContextPointer ptr;
    CHECK(NGTCP2_OK(ngtcp2_crypto_aead_ctx_encrypt_init(
        &ptr.ctx_, &aead, key, kCryptoTokenIvlen)));
    return ptr;
  }

  static AeadContextPointer forDecrypt(const uint8_t* key,
                                       const ngtcp2_crypto_aead& aead) {
    AeadContextPointer ptr;
    CHECK(NGTCP2_OK(ngtcp2_crypto_aead_ctx_decrypt_init(
        &ptr.ctx_, &aead, key, kCryptoTokenIvlen)));
    return ptr;
  }

 private:
  AeadContextPointer() = default;
  ngtcp2_crypto_aead_ctx ctx_;
};
}  // namespace

// ======================================================================================
// SessionTicketAppData

SessionTicketAppData::SessionTicketAppData(SSL_SESSION* session)
    : session_(session) {}

bool SessionTicketAppData::Set(const uint8_t* data, size_t len) {
  if (set_) return false;
  set_ = true;
  SSL_SESSION_set1_ticket_appdata(session_, data, len);
  return set_;
}

bool SessionTicketAppData::Get(uint8_t** data, size_t* len) const {
  if (!set_) return false;
  return SSL_SESSION_get0_ticket_appdata(
             session_, reinterpret_cast<void**>(data), len) == 1;
}

// ======================================================================================
// A stateless reset token is used when a QUIC endpoint receives a QUIC packet
// with a short header but the associated connection ID cannot be matched to any
// known Session. In such cases, the receiver may choose to send a subtle opaque
// indication to the sending peer that state for the Session has apparently been
// lost. For any on- or off- path attacker, a stateless reset packet resembles
// any other QUIC packet with a short header. In order to be successfully
// handled as a stateless reset, the peer must have already seen a reset token
// issued to it associated with the given CID. The token itself is opaque to the
// peer that receives is but must be possible to statelessly recreate by the
// peer that originally created it. The actual implementation is Node.js
// specific but we currently defer to a utility function provided by ngtcp2.
bool StatelessResetToken::GenerateResetToken(const uint8_t* secret,
                                             const CID& cid) {
  return NGTCP2_OK(ngtcp2_crypto_generate_stateless_reset_token(
      buf_, secret, NGTCP2_STATELESS_RESET_TOKENLEN, cid));
}

// ======================================================================================
// A RETRY packet communicates a retry token to the client. Retry tokens are
// generated only by QUIC servers for the purpose of validating the network path
// between a client and server. The content payload of the RETRY packet is
// opaque to the clientand must not be guessable by on- or off-path attackers.
//
// A QUIC server sends a RETRY token as a way of initiating explicit path
// validation in response to an initial QUIC packet. The client, upon receiving
// a RETRY, must abandon the initial connection attempt and try again with the
// received retry token included with the new initial packet sent to the server.
// If the server is performing explicit validation, it will look for the
// presence of the retry token and attempt to validate it if found. The internal
// structure of the retry token must be meaningful to the server, and the server
// must be able to validate that the token is correct without relying on any
// state left over from the previous connection attempt. We use an
// implementation that is provided by ngtcp2.
//
// The token secret must be kept secret on the QUIC server that generated the
// retry. When multiple QUIC servers are used in a cluster, it cannot be
// guaranteed that the same QUIC server instance will receive the subsequent new
// Initial packet. Therefore, all QUIC servers in the cluster should either
// share or be aware of the same token secret or a mechanism needs to be
// implemented to ensure that subsequent packets are routed to the same QUIC
// server instance.
//
// A malicious peer could attempt to guess the token secret by sending a large
// number specially crafted RETRY-eliciting packets to a server then analyzing
// the resulting retry tokens. To reduce the possibility of such attacks, the
// current implementation of QuicSocket generates the token secret randomly for
// each instance, and the number of RETRY responses sent to a given remote
// address should be limited. Such attacks should be of little actual value in
// most cases. In cases where the secret is shared across multiple servers, be
// sure to implement a mechanism to rotate those.

// Validates a retry token, returning the original CID extracted from the token
// if it is valid.
Maybe<CID> ValidateRetryToken(quic_version version,
                              const ngtcp2_vec& token,
                              const SocketAddress& addr,
                              const CID& dcid,
                              const uint8_t* token_secret,
                              uint64_t verification_expiration) {
  CID ocid;
  int ret = ngtcp2_crypto_verify_retry_token(ocid,
                                             token.base,
                                             token.len,
                                             token_secret,
                                             kTokenSecretLen,
                                             version,
                                             addr.data(),
                                             addr.length(),
                                             dcid,
                                             verification_expiration,
                                             uv_hrtime());
  return ret == 0 ? Just(ocid) : Nothing<CID>();
}

// Generates a RETRY packet.
bool GenerateRetryPacket(const BaseObjectPtr<Packet>& packet,
                         quic_version version,
                         const uint8_t* token_secret,
                         const CID& dcid,
                         const CID& scid,
                         const SocketAddress& remote_addr) {
  uint8_t token[256];
  auto cid = CIDFactory::random().Generate(NGTCP2_MAX_CIDLEN);

  auto ret = ngtcp2_crypto_generate_retry_token(token,
                                                token_secret,
                                                kTokenSecretLen,
                                                version,
                                                remote_addr.data(),
                                                remote_addr.length(),
                                                cid,
                                                dcid,
                                                uv_hrtime());

  if (ret == -1) return false;
  size_t tokenlen = ret;
  size_t pktlen = tokenlen + (2 * NGTCP2_MAX_CIDLEN) + scid.length() + 8;

  ngtcp2_vec vec = *packet;

  ssize_t nwrite = ngtcp2_crypto_write_retry(
      vec.base, pktlen, version, scid, cid, dcid, token, tokenlen);

  if (nwrite <= 0) return false;
  packet->Truncate(nwrite);

  return true;
}

// ======================================================================================
Maybe<size_t> GenerateToken(quic_version version,
                            uint8_t* token,
                            const SocketAddress& addr,
                            const uint8_t* token_secret) {
  auto ret = ngtcp2_crypto_generate_regular_token(token,
                                                  token_secret,
                                                  kTokenSecretLen,
                                                  addr.data(),
                                                  addr.length(),
                                                  uv_hrtime());

  if (ret == -1) {
    return Nothing<size_t>();
  }

  return Just(static_cast<size_t>(ret));
}

bool ValidateToken(quic_version version,
                   const ngtcp2_vec& token,
                   const SocketAddress& addr,
                   const uint8_t* token_secret,
                   uint64_t verification_expiration) {
  return NGTCP2_OK(ngtcp2_crypto_verify_regular_token(token.base,
                                                      token.len,
                                                      token_secret,
                                                      kTokenSecretLen,
                                                      addr.data(),
                                                      addr.length(),
                                                      verification_expiration,
                                                      uv_hrtime()));
}
// ======================================================================================
// OpenSSL Helpers

// Get the ALPN protocol identifier that was negotiated for the session
Local<Value> GetALPNProtocol(Session* session) {
  auto env = session->env();
  auto alpn = session->crypto_context().selected_alpn();
  return alpn == &NGHTTP3_ALPN_H3[1]
             ? BindingState::Get(env).http3_alpn_string().As<v8::Value>()
             : ToV8Value(env->context(), alpn, env->isolate())
                   .FromMaybe(Local<Value>());
}

namespace {
Session* GetSession(const SSL* ssl) {
  ngtcp2_crypto_conn_ref* ref =
      static_cast<ngtcp2_crypto_conn_ref*>(SSL_get_app_data(ssl));
  return static_cast<Session*>(ref->user_data);
}

int TlsStatusCallback(SSL* ssl, void* arg) {
  Session* session = GetSession(ssl);
  int ret;
  if (SSL_get_tlsext_status_type(ssl) == TLSEXT_STATUSTYPE_ocsp) {
    ret = session->crypto_context().OnOCSP();
    // We need to check if the session is destroyed after the OnOCSP check.
    return UNLIKELY(session->is_destroyed()) ? 0 : ret;
  }
  return 1;
}

int TlsExtStatusCallback(SSL* ssl, void* arg) {
  return GetSession(ssl)->crypto_context().OnTLSStatus();
}

void KeylogCallback(const SSL* ssl, const char* line) {
  GetSession(ssl)->crypto_context().Keylog(line);
}

int ClientHelloCallback(SSL* ssl, int* tls_alert, void* arg) {
  Session* session = GetSession(ssl);

  int ret = session->crypto_context().OnClientHello();
  if (UNLIKELY(session->is_destroyed())) {
    *tls_alert = SSL_R_SSL_HANDSHAKE_FAILURE;
    return 0;
  }
  switch (ret) {
    case 0:
      return 1;
    case -1:
      return -1;
    default:
      *tls_alert = ret;
      return 0;
  }
}

int AlpnSelectionCallback(SSL* ssl,
                          const unsigned char** out,
                          unsigned char* outlen,
                          const unsigned char* in,
                          unsigned int inlen,
                          void* arg) {
  Session* session = GetSession(ssl);

  size_t alpn_len = session->alpn().length();
  if (alpn_len > 255) return SSL_TLSEXT_ERR_NOACK;

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
          reinterpret_cast<const unsigned char*>(session->alpn().c_str()),
          alpn_len,
          in,
          inlen) == OPENSSL_NPN_NO_OVERLAP) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  return SSL_TLSEXT_ERR_OK;
}

int AllowEarlyDataCallback(SSL* ssl, void* arg) {
  return GetSession(ssl)->allow_early_data() ? 1 : 0;
}

int NewSessionCallback(SSL* ssl, SSL_SESSION* session) {
  auto ret = GetSession(ssl)->crypto_context().OnNewSession(session);
  return ret;
}

int GenerateSessionTicket(SSL* ssl, void* arg) {
  SessionTicketAppData app_data(SSL_get_session(ssl));
  GetSession(ssl)->SetSessionTicketAppData(app_data);
  return 1;
}

SSL_TICKET_RETURN DecryptSessionTicket(SSL* ssl,
                                       SSL_SESSION* session,
                                       const unsigned char* keyname,
                                       size_t keyname_len,
                                       SSL_TICKET_STATUS status,
                                       void* arg) {
  switch (status) {
    default:
      return SSL_TICKET_RETURN_IGNORE;
    case SSL_TICKET_EMPTY:
      // Fall through
    case SSL_TICKET_NO_DECRYPT:
      return SSL_TICKET_RETURN_IGNORE_RENEW;
    case SSL_TICKET_SUCCESS_RENEW:
      // Fall through
    case SSL_TICKET_SUCCESS:
      SessionTicketAppData app_data(session);
      switch (GetSession(ssl)->GetSessionTicketAppData(
          app_data, SessionTicketAppData::Flag::STATUS_NONE)) {
        default:
          return SSL_TICKET_RETURN_IGNORE;
        case SessionTicketAppData::Status::TICKET_IGNORE:
          return SSL_TICKET_RETURN_IGNORE;
        case SessionTicketAppData::Status::TICKET_IGNORE_RENEW:
          return SSL_TICKET_RETURN_IGNORE_RENEW;
        case SessionTicketAppData::Status::TICKET_USE:
          return SSL_TICKET_RETURN_USE;
        case SessionTicketAppData::Status::TICKET_USE_RENEW:
          return SSL_TICKET_RETURN_USE_RENEW;
      }
      UNREACHABLE();
  }
}

bool SetTransportParams(Session* session, const crypto::SSLPointer& ssl) {
  const ngtcp2_transport_params* params =
      ngtcp2_conn_get_local_transport_params(session->connection());
  uint8_t buf[512];
  ssize_t nwrite = ngtcp2_encode_transport_params(
      buf,
      arraysize(buf),
      NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS,
      params);
  return nwrite >= 0 &&
         SSL_set_quic_transport_params(ssl.get(), buf, nwrite) == 1;
}

void SetHostname(const crypto::SSLPointer& ssl, const std::string& hostname) {
  // If the hostname is an IP address, use an empty string as the hostname
  // instead.
  X509_VERIFY_PARAM* param = SSL_get0_param(ssl.get());
  X509_VERIFY_PARAM_set_hostflags(param, 0);

  if (UNLIKELY(SocketAddress::is_numeric_host(hostname.c_str()))) {
    SSL_set_tlsext_host_name(ssl.get(), "");
    CHECK_EQ(X509_VERIFY_PARAM_set1_host(param, "", 0), 1);
    return;
  }

  SSL_set_tlsext_host_name(ssl.get(), hostname.c_str());
  CHECK_EQ(
      X509_VERIFY_PARAM_set1_host(param, hostname.c_str(), hostname.length()),
      1);
}

}  // namespace

ngtcp2_crypto_level from_ossl_level(OSSL_ENCRYPTION_LEVEL ossl_level) {
  switch (ossl_level) {
    case ssl_encryption_initial:
      return NGTCP2_CRYPTO_LEVEL_INITIAL;
    case ssl_encryption_early_data:
      return NGTCP2_CRYPTO_LEVEL_EARLY;
    case ssl_encryption_handshake:
      return NGTCP2_CRYPTO_LEVEL_HANDSHAKE;
    case ssl_encryption_application:
      return NGTCP2_CRYPTO_LEVEL_APPLICATION;
  }
  UNREACHABLE();
}

const char* crypto_level_name(ngtcp2_crypto_level level) {
  switch (level) {
    case NGTCP2_CRYPTO_LEVEL_INITIAL:
      return "initial";
    case NGTCP2_CRYPTO_LEVEL_EARLY:
      return "early";
    case NGTCP2_CRYPTO_LEVEL_HANDSHAKE:
      return "handshake";
    case NGTCP2_CRYPTO_LEVEL_APPLICATION:
      return "app";
  }
  UNREACHABLE();
}

MaybeLocal<Value> GetCertificateData(Environment* env,
                                     crypto::SecureContext* sc,
                                     GetCertificateType type) {
  X509* cert;
  switch (type) {
    case GetCertificateType::SELF:
      cert = sc->cert().get();
      break;
    case GetCertificateType::ISSUER:
      cert = sc->issuer().get();
      break;
    default:
      UNREACHABLE();
  }

  Local<Value> ret = v8::Undefined(env->isolate());
  int size = i2d_X509(cert, nullptr);
  if (size > 0) {
    std::shared_ptr<BackingStore> store =
        ArrayBuffer::NewBackingStore(env->isolate(), size);
    unsigned char* buf = reinterpret_cast<unsigned char*>(store->Data());
    i2d_X509(cert, &buf);
    ret = ArrayBuffer::New(env->isolate(), store);
  }

  return ret;
}

// ======================================================================================
// CryptoContext

struct CryptoContext::CallbackScope final {
  CryptoContext* context;

  inline explicit CallbackScope(CryptoContext* context_) : context(context_) {
    context_->in_tls_callback_ = true;
  }

  inline ~CallbackScope() { context->in_tls_callback_ = false; }

  inline static bool is_in_callback(const CryptoContext& context) {
    return context.in_tls_callback_;
  }
};

struct CryptoContext::ResumeHandshakeScope final {
  using DoneCB = std::function<void()>;
  CryptoContext* context;
  DoneCB done;

  template <typename Fn>
  inline ResumeHandshakeScope(CryptoContext* context_, Fn done_)
      : context(context_), done(std::forward<Fn>(done_)) {}

  inline ~ResumeHandshakeScope() {
    if (!is_handshake_suspended()) return;

    done();

    if (!CallbackScope::is_in_callback(*context)) context->ResumeHandshake();
  }

  inline bool is_handshake_suspended() const {
    return context->in_ocsp_request_ || context->in_client_hello_;
  }
};

BaseObjectPtr<crypto::SecureContext> CryptoContext::InitializeSecureContext(
    const Session& session,
    const CryptoContext::Options& options,
    ngtcp2_crypto_side side) {
  auto context = crypto::SecureContext::Create(session.env());
  bool failed = false;

  context->Initialize([&](crypto::SSLCtxPointer& ctx) {
    switch (side) {
      case NGTCP2_CRYPTO_SIDE_SERVER: {
        ctx.reset(SSL_CTX_new(TLS_server_method()));
        SSL_CTX_set_app_data(ctx.get(), context);

        if (NGTCP2_ERR(
                ngtcp2_crypto_openssl_configure_server_context(ctx.get()))) {
          failed = true;
          return;
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
        if (options.client_hello) {
          SSL_CTX_set_client_hello_cb(ctx.get(), ClientHelloCallback, nullptr);
        }
        SSL_CTX_set_session_ticket_cb(
            ctx.get(), GenerateSessionTicket, DecryptSessionTicket, nullptr);

        const unsigned char* sid_ctx = reinterpret_cast<const unsigned char*>(
            options.session_id_ctx.c_str());
        SSL_CTX_set_session_id_context(
            ctx.get(), sid_ctx, options.session_id_ctx.length());

        break;
      }
      case NGTCP2_CRYPTO_SIDE_CLIENT: {
        ctx.reset(SSL_CTX_new(TLS_client_method()));
        SSL_CTX_set_app_data(ctx.get(), context);

        if (NGTCP2_ERR(
                ngtcp2_crypto_openssl_configure_client_context(ctx.get()))) {
          failed = true;
          return;
        }

        SSL_CTX_set_session_cache_mode(
            ctx.get(),
            SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL_STORE);
        SSL_CTX_sess_set_new_cb(ctx.get(), NewSessionCallback);
        break;
      }
      default:
        UNREACHABLE();
    }

    SSL_CTX_set_default_verify_paths(ctx.get());
    SSL_CTX_set_tlsext_status_cb(ctx.get(), TlsExtStatusCallback);
    SSL_CTX_set_tlsext_status_arg(ctx.get(), nullptr);

    if (options.keylog) SSL_CTX_set_keylog_callback(ctx.get(), KeylogCallback);

    if (SSL_CTX_set_ciphersuites(ctx.get(), options.ciphers.c_str()) != 1) {
      failed = true;
      return;
    }

    if (SSL_CTX_set1_groups_list(ctx.get(), options.groups.c_str()) != 1) {
      failed = true;
      return;
    }
  });

  if (failed) {
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
    if (!bio) return false;
    auto ret = context->AddCert(std::move(bio));
    return ret;
  };

  for (auto& cert : options.certs) {
    if (!addCert(cert)) {
      return BaseObjectPtr<crypto::SecureContext>();
    }
  }

  // Handle keys

  const auto addKey = [&](auto& key) {
    crypto::ClearErrorOnReturn clear_error_on_return;
    return context->UseKey(key);
    // TODO(@jasnell): Maybe SSL_CTX_check_private_key also?
  };

  for (auto& key : options.keys) {
    if (!addKey(key)) {
      return BaseObjectPtr<crypto::SecureContext>();
    }
  }

  // Handle CRL

  const auto addCRL = [&](uv_buf_t crl) {
    crypto::ClearErrorOnReturn clear_error_on_return;
    crypto::BIOPointer bio = crypto::NodeBIO::NewFixed(crl.base, crl.len);
    if (!bio) return false;
    return context->SetCRL(bio);
  };

  for (auto& crl : options.crl) {
    if (!addCRL(crl)) return BaseObjectPtr<crypto::SecureContext>();
  }

  // TODO(@jasnell): Possibly handle other bits. Such a pfx, client cert engine,
  // and session timeout.
  return BaseObjectPtr<crypto::SecureContext>(context);
}

CryptoContext::CryptoContext(Session* session,
                             const Options& options,
                             ngtcp2_crypto_side side)
    : conn_ref_({getConnection, session}),
      session_(session),
      options_(options),
      secure_context_(InitializeSecureContext(*session, options, side)),
      side_(side) {
  DEBUG(session, "Crypto context created");

  CHECK(secure_context_);

  ssl_.reset(SSL_new(secure_context_->ctx().get()));
  CHECK(ssl_);
  CHECK(SSL_is_quic(ssl_.get()));

  SSL_set_app_data(ssl_.get(), &conn_ref_);

  SSL_set_cert_cb(ssl_.get(), TlsStatusCallback, session);
  SSL_set_verify(ssl_.get(), SSL_VERIFY_NONE, crypto::VerifyCallback);

  // Enable tracing if the `--trace-tls` command line flag is used.
  if (UNLIKELY(session->env()->options()->trace_tls ||
               options.enable_tls_trace)) {
    auto& state = BindingState::Get(session->env());
    EnableTrace();
    if (state.warn_trace_tls) {
      state.warn_trace_tls = false;
      ProcessEmitWarning(session->env(),
                         "Enabling --trace-tls can expose sensitive data in "
                         "the resulting log");
    }
  }

  switch (side) {
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      DEBUG(session, "Initializing crypto client");
      SSL_set_connect_state(ssl_.get());
      crypto::SetALPN(ssl_, session->options_.alpn);
      SetHostname(ssl_, session->options_.hostname);
      if (options.ocsp)
        SSL_set_tlsext_status_type(ssl_.get(), TLSEXT_STATUSTYPE_ocsp);
      break;
    }
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      DEBUG(session, "Initializing crypto server");
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

void CryptoContext::Start() {
  ngtcp2_conn_set_tls_native_handle(session_->connection(), ssl_.get());
  SetTransportParams(session_, ssl_);
  DEBUG(session_, "Crypto context started");
}

Store CryptoContext::ocsp_response(bool release) {
  return LIKELY(release) ? std::move(ocsp_response_) : ocsp_response_;
}

ngtcp2_crypto_level CryptoContext::read_crypto_level() const {
  return from_ossl_level(SSL_quic_read_level(ssl_.get()));
}

ngtcp2_crypto_level CryptoContext::write_crypto_level() const {
  return from_ossl_level(SSL_quic_write_level(ssl_.get()));
}

void CryptoContext::Keylog(const char* line) const {
  session_->EmitKeylog(line);
}

int CryptoContext::OnClientHello() {
  DEBUG(session_, "Crypto context OnClientHello");

  if (LIKELY(!session_->options_.crypto_options.client_hello) ||
      session_->state_->client_hello_done == 1)
    return 0;

  CallbackScope cb_scope(this);

  if (in_client_hello_) return -1;
  in_client_hello_ = true;

  // Returning 1 signals an error condition.
  if (!session_->EmitClientHello()) return 1;

  // Returning -1 here will keep the TLS handshake paused until the client hello
  // callback is invoked. Returning 0 means that the handshake is ready to
  // proceed. When the OnClientHello callback is called, it may be resolved
  // synchronously or asynchronously. In case it is resolved synchronously, we
  // need the check below.
  return in_client_hello_ ? -1 : 0;
}

void CryptoContext::OnClientHelloDone() {
  DEBUG(session_, "Crypto context OnClientHelloDone");
  ResumeHandshakeScope handshake_scope(this,
                                       [this] { in_client_hello_ = false; });
  session_->state_->client_hello_done = 1;
}

int CryptoContext::OnOCSP() {
  DEBUG(session_, "Crypto context OnOCSP");
  if (LIKELY(!session_->options_.crypto_options.ocsp) ||
      session_->state_->ocsp_done == 1)
    return 1;

  if (!session_->is_server()) return 1;

  CallbackScope cb_scope(this);

  if (in_ocsp_request_) return -1;
  in_ocsp_request_ = true;

  // Returning 1 signals an error condition.
  if (!session_->EmitOCSP()) return 1;

  // Returning -1 here means that we are still waiting for the OCSP
  // request to be completed. When the OnCert handler is invoked
  // above, it can be resolve synchronously or asynchonously. If
  // resolved synchronously, we need the check below.
  return in_ocsp_request_ ? -1 : 1;
}

void CryptoContext::OnOCSPDone(Store&& ocsp_response) {
  DEBUG(session_, "Crypto context OnOCSPDone");
  ResumeHandshakeScope handshake_scope(this,
                                       [this] { in_ocsp_request_ = false; });
  session_->state_->ocsp_done = 1;
  ocsp_response_ = std::move(ocsp_response);
}

int CryptoContext::Receive(ngtcp2_crypto_level crypto_level,
                           uint64_t offset,
                           const uint8_t* data,
                           size_t datalen) {
  // ngtcp2 provides an implementation of this in
  // ngtcp2_crypto_recv_crypto_data_cb but given that we are using the
  // implementation specific error codes below, we can't use it.

  DEBUG(session_, "Receiving crypto data");

  if (UNLIKELY(session_->is_destroyed())) return NGTCP2_ERR_CALLBACK_FAILURE;

  // Internally, this passes the handshake data off to openssl for processing.
  // The handshake may or may not complete.
  int ret = ngtcp2_crypto_read_write_crypto_data(
      session_->connection(), crypto_level, data, datalen);

  switch (ret) {
    case 0:
    // Fall-through

    // In either of following cases, the handshake is being paused waiting for
    // user code to take action (for instance OCSP requests or client hello
    // modification)
    case NGTCP2_CRYPTO_OPENSSL_ERR_TLS_WANT_X509_LOOKUP:
      // Fall-through
    case NGTCP2_CRYPTO_OPENSSL_ERR_TLS_WANT_CLIENT_HELLO_CB:
      return 0;
  }
  return ret;
}

int CryptoContext::OnTLSStatus() {
  if (LIKELY(!session_->options_.crypto_options.ocsp)) return 1;

  switch (side_) {
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      DEBUG(session_, "Crypto context OnTLSStatus (server)");
      if (!ocsp_response_) return SSL_TLSEXT_ERR_NOACK;

      DEBUG(session_, "Handling ocsp response (server)");
      uv_buf_t buf = ocsp_response_;

      unsigned char* data = crypto::MallocOpenSSL<unsigned char>(buf.len);
      memcpy(data, buf.base, buf.len);

      if (!SSL_set_tlsext_status_ocsp_resp(ssl_.get(), data, buf.len))
        OPENSSL_free(data);

      USE(std::move(ocsp_response_));
      return SSL_TLSEXT_ERR_OK;
    }
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      DEBUG(session_, "Crypto context OnTLSStatus (server)");
      session_->EmitOCSPResponse();
      return 1;
    }
  }
  UNREACHABLE();
}

int CryptoContext::OnNewSession(SSL_SESSION* session) {
  // If there is nothing listening for the session ticket, don't both emitting
  // it.
  if (LIKELY(session_->state_->session_ticket == 0)) return 0;

  size_t size = i2d_SSL_SESSION(session, nullptr);
  if (size > crypto::SecureContext::kMaxSessionSize) return 0;

  std::shared_ptr<BackingStore> ticket;

  auto isolate = session_->env()->isolate();

  if (size > 0) {
    ticket = ArrayBuffer::NewBackingStore(isolate, size);
    unsigned char* data = reinterpret_cast<unsigned char*>(ticket->Data());
    if (i2d_SSL_SESSION(session, &data) <= 0) return 0;
  } else {
    ticket = ArrayBuffer::NewBackingStore(isolate, 0);
  }

  session_->EmitSessionTicket(Store(std::move(ticket), size));

  return 0;
}

void CryptoContext::ResumeHandshake() {
  DEBUG(session_, "Crypto context resuming handshake");
  Session::SendPendingDataScope scope(session_);
  Receive(read_crypto_level(), 0, nullptr, 0);
}

bool CryptoContext::InitiateKeyUpdate() {
  if (UNLIKELY(session_->is_destroyed()) || in_key_update_) return false;

  DEBUG(session_, "Crypto context initiating key update");

  // There's no user code that should be able to run while UpdateKey
  // is running, but we need to gate on it just to be safe.
  auto leave = OnScopeLeave([this]() { in_key_update_ = false; });
  in_key_update_ = true;

  session_->IncrementStat(&SessionStats::keyupdate_count);

  return ngtcp2_conn_initiate_key_update(session_->connection(), uv_hrtime()) ==
         0;
}

int CryptoContext::VerifyPeerIdentity() {
  return crypto::VerifyPeerCertificate(ssl_);
}

void CryptoContext::EnableTrace() {
#if HAVE_SSL_TRACE
  DEBUG(session_, "Enabling TLS trace");
  if (!bio_trace_) {
    bio_trace_.reset(BIO_new_fp(stderr, BIO_NOCLOSE | BIO_FP_TEXT));
    SSL_set_msg_callback(
        ssl_.get(),
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
    SSL_set_msg_callback_arg(ssl_.get(), bio_trace_.get());
  }
#endif
}

void CryptoContext::MaybeSetEarlySession(
    const BaseObjectPtr<SessionTicket>& sessionTicket) {
  // Nothing to do if there is no ticket
  if (!sessionTicket) return;

  DEBUG(session_, "Crypto context setting early session ticket");

  Session::TransportParams rtp(
      Session::TransportParams::Type::ENCRYPTED_EXTENSIONS,
      sessionTicket->transport_params());

  // Ignore invalid remote transport parameters.
  if (!rtp) return;

  uv_buf_t buf = sessionTicket->ticket();
  crypto::SSLSessionPointer ticket = crypto::GetTLSSession(
      reinterpret_cast<unsigned char*>(buf.base), buf.len);

  // Silently ignore invalid TLS session
  if (!ticket || !SSL_SESSION_get_max_early_data(ticket.get())) return;

  // We don't care about the return value here. The early data will just be
  // ignored if it's invalid.
  USE(crypto::SetTLSSession(ssl_, ticket));

  ngtcp2_conn_set_early_remote_transport_params(session_->connection(), rtp);

  DEBUG(session_, "Crypto context early session enabled");
  session_->state_->stream_open_allowed = 1;
}

void CryptoContext::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("secure_context", secure_context_);
  tracker->TrackFieldWithSize("ocsp_response", ocsp_response_);
}

v8::MaybeLocal<v8::Object> CryptoContext::cert(Environment* env) const {
  return crypto::X509Certificate::GetCert(env, ssl_);
}

v8::MaybeLocal<v8::Object> CryptoContext::peer_cert(Environment* env) const {
  crypto::X509Certificate::GetPeerCertificateFlag flag =
      session_->is_server()
          ? crypto::X509Certificate::GetPeerCertificateFlag::SERVER
          : crypto::X509Certificate::GetPeerCertificateFlag::NONE;
  return crypto::X509Certificate::GetPeerCert(env, ssl_, flag);
}

v8::MaybeLocal<v8::Value> CryptoContext::cipher_name(Environment* env) const {
  return crypto::GetCurrentCipherName(env, ssl_);
}

v8::MaybeLocal<v8::Value> CryptoContext::cipher_version(
    Environment* env) const {
  return crypto::GetCurrentCipherVersion(env, ssl_);
}

v8::MaybeLocal<v8::Object> CryptoContext::ephemeral_key(
    Environment* env) const {
  return crypto::GetEphemeralKey(env, ssl_);
}

v8::MaybeLocal<v8::Array> CryptoContext::hello_ciphers(Environment* env) const {
  return crypto::GetClientHelloCiphers(env, ssl_);
}

v8::MaybeLocal<v8::Value> CryptoContext::hello_servername(
    Environment* env) const {
  const char* servername = crypto::GetClientHelloServerName(ssl_);
  if (servername == nullptr) return Undefined(env->isolate());
  return OneByteString(env->isolate(), crypto::GetClientHelloServerName(ssl_));
}

v8::MaybeLocal<v8::Value> CryptoContext::hello_alpn(Environment* env) const {
  const char* alpn = crypto::GetClientHelloALPN(ssl_);
  if (alpn == nullptr) return Undefined(env->isolate());
  return OneByteString(env->isolate(), alpn);
}

std::string CryptoContext::servername() const {
  const char* servername = crypto::GetServerName(ssl_.get());
  return servername != nullptr ? std::string(servername) : std::string();
}

std::string CryptoContext::selected_alpn() const {
  const unsigned char* alpn_buf = nullptr;
  unsigned int alpnlen;
  SSL_get0_alpn_selected(ssl_.get(), &alpn_buf, &alpnlen);
  return alpnlen ? std::string(reinterpret_cast<const char*>(alpn_buf), alpnlen)
                 : std::string();
}

bool CryptoContext::was_early_data_accepted() const {
  return (early_data_ &&
          SSL_get_early_data_status(ssl_.get()) == SSL_EARLY_DATA_ACCEPTED);
}

void CryptoContext::Options::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("keys", keys);
  tracker->TrackField("certs", certs);
  tracker->TrackField("ca", ca);
  tracker->TrackField("crl", crl);
}

ngtcp2_conn* CryptoContext::getConnection(ngtcp2_crypto_conn_ref* ref) {
  CryptoContext* context = ContainerOf(&CryptoContext::conn_ref_, ref);
  return context->session_->connection();
}

// ======================================================================================
// SessionTicket

Local<FunctionTemplate> SessionTicket::GetConstructorTemplate(
    Environment* env) {
  auto& state = BindingState::Get(env);
  auto tmpl = state.sessionticket_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(env->isolate(), New);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        SessionTicket::kInternalFieldCount);
    tmpl->SetClassName(state.sessionticket_string());
    SetProtoMethod(env->isolate(), tmpl, "encoded", GetEncoded);
    state.set_sessionticket_constructor_template(tmpl);
  }
  return tmpl;
}

void SessionTicket::Initialize(Environment* env, Local<Object> target) {
  SetConstructorFunction(env->context(),
                         target,
                         "SessionTicket",
                         GetConstructorTemplate(env),
                         SetConstructorFunctionFlag::NONE);
}

BaseObjectPtr<SessionTicket> SessionTicket::Create(Environment* env,
                                                   Store&& ticket,
                                                   Store&& transport_params) {
  Local<Object> obj;
  if (UNLIKELY(!GetConstructorTemplate(env)
                    ->InstanceTemplate()
                    ->NewInstance(env->context())
                    .ToLocal(&obj))) {
    return BaseObjectPtr<SessionTicket>();
  }

  return MakeBaseObject<SessionTicket>(env,
                                       obj,
                                       std::forward<Store>(ticket),
                                       std::forward<Store>(transport_params));
}

SessionTicket::SessionTicket(Environment* env,
                             Local<Object> object,
                             Store&& ticket,
                             Store&& transport_params)
    : BaseObject(env, object),
      ticket_(std::move(ticket)),
      transport_params_(std::move(transport_params)) {
  MakeWeak();
}

void SessionTicket::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("ticket", ticket_);
  tracker->TrackField("transport_params", transport_params_);
}

void SessionTicket::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(GetEncoded);
}

MaybeLocal<Object> SessionTicket::Encoded() {
  auto context = env()->context();
  v8::ValueSerializer ser(env()->isolate());
  ser.WriteHeader();

  if (ser.WriteValue(context, ticket_.ToArrayBufferView<Uint8Array>(env()))
          .IsNothing() ||
      ser.WriteValue(context,
                     transport_params_.ToArrayBufferView<Uint8Array>(env()))
          .IsNothing()) {
    return MaybeLocal<Object>();
  }

  auto result = ser.Release();
  return node::Buffer::Copy(
      env(), reinterpret_cast<char*>(result.first), result.second);
}

void SessionTicket::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
  auto env = Environment::GetCurrent(args);
  if (!args[0]->IsArrayBufferView()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "The ticket must be an ArrayBufferView.");
    return;
  }

  auto context = env->context();
  crypto::ArrayBufferOrViewContents<uint8_t> view(args[0]);
  v8::ValueDeserializer des(env->isolate(), view.data(), view.size());

  if (des.ReadHeader(context).IsNothing()) {
    THROW_ERR_INVALID_ARG_VALUE(env, "The ticket format is invalid.");
    return;
  }

  Local<Value> ticket;
  Local<Value> transport_params;

  if (!des.ReadValue(context).ToLocal(&ticket) ||
      !ticket->IsArrayBufferView()) {
    THROW_ERR_INVALID_ARG_VALUE(env, "The ticket format is invalid.");
    return;
  }

  if (!des.ReadValue(context).ToLocal(&transport_params) ||
      !transport_params->IsArrayBufferView()) {
    THROW_ERR_INVALID_ARG_VALUE(env, "The ticket format is invalid.");
    return;
  }

  new SessionTicket(env,
                    args.This(),
                    Store(ticket.As<ArrayBufferView>()),
                    Store(transport_params.As<ArrayBufferView>()));
}

void SessionTicket::GetEncoded(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  auto env = Environment::GetCurrent(args);
  SessionTicket* sessionTicket;
  CHECK(SessionTicket::HasInstance(env, args.Holder()));
  ASSIGN_OR_RETURN_UNWRAP(&sessionTicket, args.Holder());
  Local<Object> encoded;
  if (sessionTicket->Encoded().ToLocal(&encoded))
    args.GetReturnValue().Set(encoded);
}

// ======================================================================================
// StatelessResetToken

StatelessResetToken::StatelessResetToken(const uint8_t* token) {
  memcpy(buf_, token, sizeof(buf_));
}

StatelessResetToken::StatelessResetToken(const uint8_t* secret,
                                         const CID& cid) {
  GenerateResetToken(secret, cid);
}

StatelessResetToken::StatelessResetToken(uint8_t* token,
                                         const uint8_t* secret,
                                         const CID& cid)
    : StatelessResetToken(secret, cid) {
  memcpy(buf_, token, sizeof(buf_));
}

std::string StatelessResetToken::ToString() const {
  std::vector<char> dest(NGTCP2_STATELESS_RESET_TOKENLEN * 2 + 1);
  dest[dest.size() - 1] = '\0';
  size_t written = StringBytes::hex_encode(reinterpret_cast<const char*>(buf_),
                                           NGTCP2_STATELESS_RESET_TOKENLEN,
                                           dest.data(),
                                           dest.size());
  return std::string(dest.data(), written);
}

size_t StatelessResetToken::Hash::operator()(
    const StatelessResetToken& token) const {
  size_t hash = 0;
  for (size_t n = 0; n < NGTCP2_STATELESS_RESET_TOKENLEN; n++)
    hash ^= std::hash<uint8_t>{}(token.buf_[n]) + 0x9e3779b9 + (hash << 6) +
            (hash >> 2);
  return hash;
}

}  // namespace quic
}  // namespace node
