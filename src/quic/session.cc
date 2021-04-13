#include "quic/buffer.h"
#include "quic/crypto.h"
#include "quic/endpoint.h"
#include "quic/http3.h"
#include "quic/qlog.h"
#include "quic/quic.h"
#include "quic/session.h"
#include "quic/stream.h"

#include "crypto/crypto_common.h"
#include "crypto/crypto_x509.h"
#include "aliased_struct-inl.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_bob-inl.h"
#include "node_http_common-inl.h"
#include "node_process-inl.h"
#include "node_sockaddr-inl.h"
#include "v8.h"

#include <ngtcp2/ngtcp2_crypto_openssl.h>
#include <vector>

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::BigInt;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::PropertyAttribute;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;

namespace quic {

namespace {
inline size_t get_max_pkt_len(const std::shared_ptr<SocketAddress>& addr) {
  return addr->family() == AF_INET6 ?
      NGTCP2_MAX_PKTLEN_IPV6 :
      NGTCP2_MAX_PKTLEN_IPV4;
}

inline bool is_ngtcp2_debug_enabled(Environment* env) {
  return env->enabled_debug_list()->enabled(DebugCategory::NGTCP2_DEBUG);
}

// Forwards detailed(verbose) debugging information from ngtcp2. Enabled using
// the NODE_DEBUG_NATIVE=NGTCP2_DEBUG category.
void Ngtcp2DebugLog(void* user_data, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::string format(fmt, strlen(fmt) + 1);
  format[strlen(fmt)] = '\n';
  // Debug() does not work with the va_list here. So we use vfprintf
  // directly instead. Ngtcp2DebugLog is only enabled when the debug
  // category is enabled.
  vfprintf(stderr, format.c_str(), ap);
  va_end(ap);
}

// Qlog is a JSON-based logging format that is being standardized for
// low-level debug logging of QUIC connections and dataflows. The qlog
// output is generated optionally by ngtcp2 for us. The OnQlogWrite
// callback is registered with ngtcp2 to emit the qlog information.
// Every Session will have it's own qlog stream.
void OnQlogWrite(
    void* user_data,
    uint32_t flags,
    const void* data,
    size_t len) {
  Session* session = static_cast<Session*>(user_data);
  Environment* env = session->env();

  // Fun fact... ngtcp2 does not emit the final qlog statement until the
  // ngtcp2_conn object is destroyed. Ideally, destroying is explicit,
  // but sometimes the Session object can be garbage collected without
  // being explicitly destroyed. During those times, we cannot call out
  // to JavaScript. Because we don't know for sure if we're in in a GC
  // when this is called, it is safer to just defer writes to immediate,
  // and to keep it consistent, let's just always defer (this is not
  // performance sensitive so the deferral is fine).
  BaseObjectPtr<LogStream> ptr = session->qlogstream();
  if (ptr) {
    std::vector<uint8_t> buffer(len);
    memcpy(buffer.data(), data, len);
    env->SetImmediate([ptr = std::move(ptr),
                       buffer = std::move(buffer),
                       flags](Environment*) {
      ptr->Emit(buffer.data(), buffer.size(), flags);
    });
  }
}

inline ConnectionCloseFn SelectCloseFn(QuicError error) {
  switch (error.type) {
    case QuicError::Type::TRANSPORT:
      return ngtcp2_conn_write_connection_close;
    case QuicError::Type::APPLICATION:
      return ngtcp2_conn_write_application_close;
    default:
      UNREACHABLE();
  }
}

inline void Consume(ngtcp2_vec** pvec, size_t* pcnt, size_t len) {
  ngtcp2_vec* v = *pvec;
  size_t cnt = *pcnt;

  for (; cnt > 0; --cnt, ++v) {
    if (v->len > len) {
      v->len -= len;
      v->base += len;
      break;
    }
    len -= v->len;
  }

  *pvec = v;
  *pcnt = cnt;
}

inline int IsEmpty(const ngtcp2_vec* vec, size_t cnt) {
  size_t i;
  for (i = 0; i < cnt && vec[i].len == 0; ++i) {}
  return i == cnt;
}

template <typename T>
size_t get_length(const T* vec, size_t count) {
  CHECK_NOT_NULL(vec);
  size_t len = 0;
  for (size_t n = 0; n < count; n++)
    len += vec[n].len;
  return len;
}
}  // namespace

Session::Config::Config(
    Endpoint* endpoint,
    const CID& dcid_,
    const CID& scid_,
    quic_version version_)
    : version(version_),
      dcid(dcid_),
      scid(scid_) {
  ngtcp2_settings_default(this);
  initial_ts = uv_hrtime();
  if (UNLIKELY(is_ngtcp2_debug_enabled(endpoint->env())))
    log_printf = Ngtcp2DebugLog;

  Endpoint::Config config = endpoint->config();

  cc_algo = config.cc_algorithm;
  max_udp_payload_size = config.max_payload_size;

  if (config.max_window_override > 0)
    max_window = config.max_window_override;

  if (config.max_stream_window_override > 0)
    max_stream_window = config.max_stream_window_override;

  if (config.unacknowledged_packet_threshold > 0)
    ack_thresh = config.unacknowledged_packet_threshold;
}

Session::Config::Config(Endpoint* endpoint, quic_version version)
    : Config(endpoint, CID(), CID(), version) {}

void Session::Config::EnableQLog(const CID& ocid) {
  if (ocid) {
    qlog.odcid = *ocid;
    this->ocid = ocid;
  }
  qlog.write = OnQlogWrite;
}

Session::Options::Options(const Session::Options& other) noexcept
    : alpn(other.alpn),
      hostname(other.hostname),
      cid_strategy(other.cid_strategy),
      cid_strategy_strong_ref(other.cid_strategy_strong_ref),
      dcid(other.dcid),
      scid(other.scid),
      preferred_address_strategy(other.preferred_address_strategy),
      qlog(other.qlog),
      preferred_address_ipv4(other.preferred_address_ipv4),
      preferred_address_ipv6(other.preferred_address_ipv6),
      initial_max_stream_data_bidi_local(
          other.initial_max_stream_data_bidi_local),
      initial_max_stream_data_bidi_remote(
          other.initial_max_stream_data_bidi_remote),
      initial_max_stream_data_uni(other.initial_max_stream_data_uni),
      initial_max_data(other.initial_max_data),
      initial_max_streams_bidi(other.initial_max_streams_bidi),
      initial_max_streams_uni(other.initial_max_streams_uni),
      max_idle_timeout(other.max_idle_timeout),
      active_connection_id_limit(other.active_connection_id_limit),
      ack_delay_exponent(other.ack_delay_exponent),
      max_ack_delay(other.max_ack_delay),
      max_datagram_frame_size(other.max_datagram_frame_size),
      keylog(other.keylog),
      disable_active_migration(other.disable_active_migration),
      reject_unauthorized(other.reject_unauthorized),
      client_hello(other.client_hello),
      enable_tls_trace(other.enable_tls_trace),
      request_peer_certificate(other.request_peer_certificate),
      ocsp(other.ocsp),
      verify_hostname_identity(other.verify_hostname_identity),
      psk_callback_present(other.psk_callback_present),
      session_id_ctx(other.session_id_ctx) {}

Session::TransportParams::TransportParams(
    const std::shared_ptr<Options>& options,
    const CID& scid,
    const CID& ocid) {
  ngtcp2_transport_params_default(this);
  active_connection_id_limit = options->active_connection_id_limit;
  initial_max_stream_data_bidi_local =
      options->initial_max_stream_data_bidi_local;
  initial_max_stream_data_bidi_remote =
      options->initial_max_stream_data_bidi_remote;
  initial_max_stream_data_uni = options->initial_max_stream_data_uni;
  initial_max_streams_bidi = options->initial_max_streams_bidi;
  initial_max_streams_uni = options->initial_max_streams_uni;
  initial_max_data = options->initial_max_data;
  max_idle_timeout = options->max_idle_timeout * NGTCP2_SECONDS;
  max_ack_delay = options->max_ack_delay;
  ack_delay_exponent = options->ack_delay_exponent;
  max_datagram_frame_size = options->max_datagram_frame_size;
  disable_active_migration = options->disable_active_migration ? 1 : 0;
  preferred_address_present = 0;
  stateless_reset_token_present = 0;
  retry_scid_present = 0;

  if (ocid) {
    original_dcid = *ocid;
    retry_scid = *scid;
    retry_scid_present = 1;
  } else {
    original_dcid = *scid;
  }

  if (options->preferred_address_ipv4)
    SetPreferredAddress(options->preferred_address_ipv4);

  if (options->preferred_address_ipv6)
    SetPreferredAddress(options->preferred_address_ipv6);
}

void Session::TransportParams::SetPreferredAddress(
    const std::shared_ptr<SocketAddress>& address) {
  preferred_address_present = 1;
  switch (address->family()) {
    case AF_INET: {
      const sockaddr_in* src =
          reinterpret_cast<const sockaddr_in*>(address->data());
      memcpy(preferred_address.ipv4_addr,
             &src->sin_addr,
             sizeof(preferred_address.ipv4_addr));
      preferred_address.ipv4_port = address->port();
      break;
    }
    case AF_INET6: {
      const sockaddr_in6* src =
          reinterpret_cast<const sockaddr_in6*>(address->data());
      memcpy(preferred_address.ipv6_addr,
             &src->sin6_addr,
             sizeof(preferred_address.ipv6_addr));
      preferred_address.ipv6_port = address->port();
      break;
    }
    default:
      UNREACHABLE();
  }
}

void Session::TransportParams::GenerateStatelessResetToken(
    EndpointWrap* endpoint,
    const CID& cid) {
  CHECK(cid);
  stateless_reset_token_present = 1;
  StatelessResetToken token(
    stateless_reset_token,
    endpoint->config().reset_token_secret,
    cid);
}

void Session::TransportParams::GeneratePreferredAddressToken(
    RoutableConnectionIDStrategy* connection_id_strategy,
    Session* session,
    CID* pscid) {
  CHECK(pscid);
  connection_id_strategy->NewConnectionID(pscid);
  preferred_address.cid = **pscid;
  StatelessResetToken(
    preferred_address.stateless_reset_token,
    session->endpoint()->config().reset_token_secret,
    *pscid);
}

Session::CryptoContext::CryptoContext(
    Session* session,
    const std::shared_ptr<Options>& options,
    const BaseObjectPtr<crypto::SecureContext>& context,
    ngtcp2_crypto_side side) :
    session_(session),
    secure_context_(context),
    side_(side),
    options_(options) {
  CHECK(secure_context_);
  ssl_.reset(SSL_new(secure_context_->ctx_.get()));
  CHECK(ssl_);
}

Session::CryptoContext::~CryptoContext() {
  USE(Cancel());
}

void Session::CryptoContext::MaybeSetEarlySession(
    const crypto::ArrayBufferOrViewContents<uint8_t>& session_ticket,
    const crypto::ArrayBufferOrViewContents<uint8_t>& remote_transport_params) {
  if (session_ticket.size() > 0 && remote_transport_params.size() > 0) {
    // Silently ignore invalid remote_transport_params
    if (remote_transport_params.size() != sizeof(ngtcp2_transport_params))
      return;

    crypto::SSLSessionPointer ticket =
        crypto::GetTLSSession(
            reinterpret_cast<const unsigned char*>(session_ticket.data()),
            session_ticket.size());

    // Silently ignore invalid TLS session
    if (!ticket || !SSL_SESSION_get_max_early_data(ticket.get()))
      return;

    // We don't care about the return value here. The early
    // data will just be ignored if it's invalid.
    USE(crypto::SetTLSSession(ssl_, ticket));

    ngtcp2_transport_params params;
    memcpy(&params,
           remote_transport_params.data(),
           sizeof(ngtcp2_transport_params));

    ngtcp2_conn_set_early_remote_transport_params(
        session()->connection(),
        &params);
    Debug(session(), "Early session ticket and remote transport params set");
    session()->state_->stream_open_allowed = 1;
  }
}

void Session::CryptoContext::AcknowledgeCryptoData(
    ngtcp2_crypto_level level,
    size_t datalen) {
  // It is possible for the Session to have been destroyed but not yet
  // deconstructed. In such cases, we want to ignore the callback as there
  // is nothing to do but wait for further cleanup to happen.
  if (UNLIKELY(session_->is_destroyed()))
    return;
  Debug(session(),
        "Acknowledging %" PRIu64 " crypto bytes for %s level",
        datalen,
        crypto_level_name(level));

  // Consumes (frees) the given number of bytes in the handshake buffer.
  handshake_[level].Acknowledge(static_cast<size_t>(datalen));
}

size_t Session::CryptoContext::Cancel() {
  size_t len =
      handshake_[0].remaining() +
      handshake_[1].remaining() +
      handshake_[2].remaining();
  handshake_[0].Clear();
  handshake_[1].Clear();
  handshake_[2].Clear();
  return len;
}

void Session::CryptoContext::Initialize() {
  InitializeTLS(session(), ssl_);
}

void Session::CryptoContext::EnableTrace() {
#if HAVE_SSL_TRACE
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
        SSL_trace(write_p,  version, content_type, buf, len, ssl, arg);
    });
    SSL_set_msg_callback_arg(ssl_.get(), bio_trace_.get());
  }
#endif
}

std::shared_ptr<BackingStore> Session::CryptoContext::ocsp_response(
    bool release) {
  return LIKELY(release) ? std::move(ocsp_response_) : ocsp_response_;
}

std::string Session::CryptoContext::selected_alpn() const {
  const unsigned char* alpn_buf = nullptr;
  unsigned int alpnlen;
  SSL_get0_alpn_selected(ssl_.get(), &alpn_buf, &alpnlen);
  return alpnlen ?
      std::string(reinterpret_cast<const char*>(alpn_buf), alpnlen) :
      std::string();
}

ngtcp2_crypto_level Session::CryptoContext::read_crypto_level() const {
  return from_ossl_level(SSL_quic_read_level(ssl_.get()));
}

ngtcp2_crypto_level Session::CryptoContext::write_crypto_level() const {
  return from_ossl_level(SSL_quic_write_level(ssl_.get()));
}

void Session::CryptoContext::Keylog(const char* line) {
  Environment* env = session_->env();

  BaseObjectPtr<LogStream> ptr = session_->keylogstream();
  if (UNLIKELY(ptr)) {
    std::string data = line;
    data += "\n";
    env->SetImmediate([ptr = std::move(ptr),
                       data = std::move(data)](Environment* env) {
      ptr->Emit(data);
    });
  }
}

int Session::CryptoContext::OnClientHello() {
  if (LIKELY(!session_->options_->client_hello) ||
      session_->state_->client_hello_done == 1) {
    return 0;
  }

  CallbackScope cb_scope(this);

  if (in_client_hello_)
    return -1;
  in_client_hello_ = true;

  Environment* env = session_->env();

  Debug(session(), "Invoking client hello callback");

  BindingState* state = env->GetBindingData<BindingState>(env->context());
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  // Why this instead of using MakeCallback? We need to catch any
  // errors that happen both when preparing the arguments and
  // invoking the callback so that we can properly signal a failure
  // to the peer.
  Session::CallbackScope scb_scope(session());

  Local<Value> argv[3] = {
    v8::Undefined(env->isolate()),
    v8::Undefined(env->isolate()),
    v8::Undefined(env->isolate())
  };

  Session::CryptoContext* crypto_context = session()->crypto_context();

  if (!crypto_context->hello_alpn(env).ToLocal(&argv[0]) ||
      !crypto_context->hello_servername(env).ToLocal(&argv[1]) ||
      !crypto_context->hello_ciphers(env).ToLocal(&argv[2])) {
    return 0;
  }

  // Grab a shared pointer to this to prevent the Session
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<Session> ptr(session());
  USE(state->session_client_hello_callback()->Call(
      env->context(),
      session()->object(),
      arraysize(argv),
      argv));

  // Returning -1 here will keep the TLS handshake paused until the
  // client hello callback is invoked. Returning 0 means that the
  // handshake is ready to proceed. When the OnClientHello callback
  // is called above, it may be resolved synchronously or asynchronously.
  // In case it is resolved synchronously, we need the check below.
  return in_client_hello_ ? -1 : 0;
}

void Session::CryptoContext::OnClientHelloDone(
    BaseObjectPtr<crypto::SecureContext> context) {
  Debug(session(),
        "ClientHello completed. Context Provided? %s\n",
        context ? "Yes" : "No");

  // Continue the TLS handshake when this function exits
  // otherwise it will stall and fail.
  HandshakeScope handshake_scope(
      this,
      [this]() { in_client_hello_ = false; });

  // Disable the callback at this point so we don't loop continuously
  session_->state_->client_hello_done = 1;

  if (context) {
    int err = crypto::UseSNIContext(ssl_, context);
    if (!err) {
      unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
      return !err ?
          THROW_ERR_QUIC_FAILURE_SETTING_SNI_CONTEXT(session_->env()) :
          crypto::ThrowCryptoError(session_->env(), err);
    }
    secure_context_ = context;
  }
}

int Session::CryptoContext::OnOCSP() {
  if (LIKELY(!session_->options_->ocsp) ||
      session_->state_->ocsp_done == 1) {
    return 1;
  }

  if (!session_->is_server())
    return 1;

  Debug(session(), "Client is requesting an OCSP Response");
  CallbackScope callback_scope(this);

  // As in node_crypto.cc, this is not an error, but does suspend the
  // handshake to continue when OnOCSP is complete.
  if (in_ocsp_request_)
    return -1;
  in_ocsp_request_ = true;

  Environment* env = session()->env();
  BindingState* state = env->GetBindingData<BindingState>(env->context());
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  Session::CallbackScope cb_scope(session());

  Local<Value> argv[2] = {
    v8::Undefined(session()->env()->isolate()),
    v8::Undefined(session()->env()->isolate())
  };

  if (!GetCertificateData(
          session()->env(),
          secure_context_.get(),
          GetCertificateType::SELF).ToLocal(&argv[0]) ||
      !GetCertificateData(
          session()->env(),
          secure_context_.get(),
          GetCertificateType::ISSUER).ToLocal(&argv[1])) {
    Debug(session(), "Failed to get certificate or issuer for OCSP request");
    return 1;
  }

  BaseObjectPtr<Session> ptr(session());
  USE(state->session_ocsp_request_callback()->Call(
      env->context(),
      session()->object(),
      arraysize(argv), argv));

  // Returning -1 here means that we are still waiting for the OCSP
  // request to be completed. When the OnCert handler is invoked
  // above, it can be resolve synchronously or asynchonously. If
  // resolved synchronously, we need the check below.
  return in_ocsp_request_ ? -1 : 1;
}

void Session::CryptoContext::OnOCSPDone(
    std::shared_ptr<BackingStore> ocsp_response) {
  Debug(session(), "OCSP Request completed. Response Provided");
  HandshakeScope handshake_scope(
      this,
      [this]() { in_ocsp_request_ = false; });

  session_->state_->ocsp_done = 1;
  ocsp_response_ = std::move(ocsp_response);
}

bool Session::CryptoContext::OnSecrets(
    ngtcp2_crypto_level level,
    const uint8_t* rx_secret,
    const uint8_t* tx_secret,
    size_t secretlen) {

  Debug(session(),
        "Received secrets for %s crypto level",
        crypto_level_name(level));

  if (!SetSecrets(level, rx_secret, tx_secret, secretlen)) {
    Debug(session(), "Failure setting the secrets");
    return false;
  }

  if (level == NGTCP2_CRYPTO_LEVEL_APPLICATION) {
    session_->set_remote_transport_params();
    if (!session()->InitApplication()) {
      Debug(session(), "Failure initializing the application");
      return false;
    }
  }

  return true;
}

int Session::CryptoContext::OnTLSStatus() {
  if (LIKELY(!session_->options_->ocsp))
    return 1;
  Environment* env = session_->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  switch (side_) {
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      if (!ocsp_response_) {
        Debug(session(), "There is no OCSP response");
        return SSL_TLSEXT_ERR_NOACK;
      }

      size_t len = ocsp_response_->ByteLength();
      Debug(session(), "There is an OCSP response of %d bytes", len);

      unsigned char* data = crypto::MallocOpenSSL<unsigned char>(len);
      memcpy(data, ocsp_response_->Data(), len);

      if (!SSL_set_tlsext_status_ocsp_resp(ssl_.get(), data, len))
        OPENSSL_free(data);

      ocsp_response_.reset();
      return SSL_TLSEXT_ERR_OK;
    }
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      HandleScope scope(env->isolate());
      Context::Scope context_scope(env->context());
      Session::CallbackScope cb_scope(session());

      Local<Value> res = v8::Undefined(env->isolate());

      const unsigned char* resp;
      int len = SSL_get_tlsext_status_ocsp_resp(ssl_.get(), &resp);
      if (resp != nullptr && len > 0) {
        std::shared_ptr<BackingStore> store =
            ArrayBuffer::NewBackingStore(env->isolate(), len);
        memcpy(store->Data(), resp, len);
        res = ArrayBuffer::New(env->isolate(), store);
      }

      BindingState* state = BindingState::Get(env);
      BaseObjectPtr<Session> ptr(session());
      USE(state->session_ocsp_response_callback()->Call(
          env->context(),
          session()->object(),
          1, &res));
      return 1;
    }
    default:
      UNREACHABLE();
  }
}

int Session::CryptoContext::Receive(
    ngtcp2_crypto_level crypto_level,
    uint64_t offset,
    const uint8_t* data,
    size_t datalen) {
  if (UNLIKELY(session_->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  Debug(session(), "Receiving %d bytes of crypto data", datalen);

  // Internally, this passes the handshake data off to openssl
  // for processing. The handshake may or may not complete.
  int ret = ngtcp2_crypto_read_write_crypto_data(
      session_->connection(),
      crypto_level,
      data,
      datalen);
  switch (ret) {
    case 0:
      return 0;
    // In either of following cases, the handshake is being
    // paused waiting for user code to take action (for instance
    // OCSP requests or client hello modification)
    case NGTCP2_CRYPTO_OPENSSL_ERR_TLS_WANT_X509_LOOKUP:
      Debug(session(), "TLS handshake wants X509 Lookup");
      return 0;
    case NGTCP2_CRYPTO_OPENSSL_ERR_TLS_WANT_CLIENT_HELLO_CB:
      Debug(session(), "TLS handshake wants client hello callback");
      return 0;
    default:
      return ret;
  }
}

void Session::CryptoContext::ResumeHandshake() {
  Receive(read_crypto_level(), 0, nullptr, 0);
  session_->SendPendingData();
}

MaybeLocal<Object> Session::CryptoContext::cert(Environment* env) const {
  return crypto::X509Certificate::GetCert(env, ssl_);
}

MaybeLocal<Object> Session::CryptoContext::peer_cert(Environment* env) const {
  crypto::X509Certificate::GetPeerCertificateFlag flag = session_->is_server()
      ? crypto::X509Certificate::GetPeerCertificateFlag::SERVER
      : crypto::X509Certificate::GetPeerCertificateFlag::NONE;
  return crypto::X509Certificate::GetPeerCert(env, ssl_, flag);
}

MaybeLocal<Value> Session::CryptoContext::cipher_name(Environment* env) const {
  return crypto::GetCipherName(env, ssl_);
}

MaybeLocal<Value> Session::CryptoContext::cipher_version(
    Environment* env) const {
  return crypto::GetCipherVersion(env, ssl_);
}

MaybeLocal<Object> Session::CryptoContext::ephemeral_key(
    Environment* env) const {
  return crypto::GetEphemeralKey(env, ssl_);
}

MaybeLocal<Array> Session::CryptoContext::hello_ciphers(
    Environment* env) const {
  return crypto::GetClientHelloCiphers(env, ssl_);
}

MaybeLocal<Value> Session::CryptoContext::hello_servername(
    Environment* env) const {
  const char* servername = crypto::GetClientHelloServerName(ssl_);
  if (servername == nullptr)
    return v8::Undefined(env->isolate());
  return OneByteString(env->isolate(), crypto::GetClientHelloServerName(ssl_));
}

MaybeLocal<Value> Session::CryptoContext::hello_alpn(
    Environment* env) const {
  const char* alpn = crypto::GetClientHelloALPN(ssl_);
  if (alpn == nullptr)
    return v8::Undefined(env->isolate());
  return OneByteString(env->isolate(), alpn);
}

std::string Session::CryptoContext::servername() const {
  const char* servername = crypto::GetServerName(ssl_.get());
  return servername != nullptr ? std::string(servername) : std::string();
}

void Session::CryptoContext::set_tls_alert(int err) {
  Debug(session(), "TLS Alert [%d]: %s", err, SSL_alert_type_string_long(err));
  session_->set_last_error({
      QuicError::Type::TRANSPORT,
      static_cast<uint64_t>(NGTCP2_CRYPTO_ERROR | err)
    });
}

void Session::CryptoContext::WriteHandshake(
    ngtcp2_crypto_level level,
    const uint8_t* data,
    size_t datalen) {
  Debug(session(),
        "Writing %d bytes of %s handshake data.",
        datalen,
        crypto_level_name(level));

  std::unique_ptr<BackingStore> store =
      ArrayBuffer::NewBackingStore(
          session()->env()->isolate(),
          datalen);
  memcpy(store->Data(), data, datalen);

  CHECK_EQ(
      ngtcp2_conn_submit_crypto_data(
          session_->connection(),
          level,
          static_cast<uint8_t*>(store->Data()),
          datalen), 0);

  handshake_[level].Push(std::move(store), datalen);
}

bool Session::CryptoContext::InitiateKeyUpdate() {
  if (UNLIKELY(session_->is_destroyed()) || in_key_update_)
    return false;

  // There's no user code that should be able to run while UpdateKey
  // is running, but we need to gate on it just to be safe.
  auto leave = OnScopeLeave([this]() { in_key_update_ = false; });
  in_key_update_ = true;
  Debug(session(), "Initiating key update");

  session_->IncrementStat(&SessionStats::keyupdate_count);

  return ngtcp2_conn_initiate_key_update(
      session_->connection(),
      uv_hrtime()) == 0;
}

int Session::CryptoContext::VerifyPeerIdentity() {
  return crypto::VerifyPeerCertificate(ssl_);
}

bool Session::CryptoContext::early_data() const {
  return (early_data_ &&
      SSL_get_early_data_status(ssl_.get()) == SSL_EARLY_DATA_ACCEPTED) ||
      SSL_get_max_early_data(ssl_.get()) == 0xffffffffUL;
}

void Session::CryptoContext::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("initial_crypto", handshake_[0]);
  tracker->TrackField("handshake_crypto", handshake_[1]);
  tracker->TrackField("app_crypto", handshake_[2]);
  tracker->TrackFieldWithSize(
      "ocsp_response",
      ocsp_response_ ? ocsp_response_->ByteLength() : 0);
}

bool Session::CryptoContext::SetSecrets(
    ngtcp2_crypto_level level,
    const uint8_t* rx_secret,
    const uint8_t* tx_secret,
    size_t secretlen) {

  static constexpr int kCryptoKeylen = 64;
  static constexpr int kCryptoIvlen = 64;
  static constexpr char kQuicClientEarlyTrafficSecret[] =
      "QUIC_CLIENT_EARLY_TRAFFIC_SECRET";
  static constexpr char kQuicClientHandshakeTrafficSecret[] =
      "QUIC_CLIENT_HANDSHAKE_TRAFFIC_SECRET";
  static constexpr char kQuicClientTrafficSecret0[] =
      "QUIC_CLIENT_TRAFFIC_SECRET_0";
  static constexpr char kQuicServerHandshakeTrafficSecret[] =
      "QUIC_SERVER_HANDSHAKE_TRAFFIC_SECRET";
  static constexpr char kQuicServerTrafficSecret[] =
      "QUIC_SERVER_TRAFFIC_SECRET_0";

  uint8_t rx_key[kCryptoKeylen];
  uint8_t rx_hp[kCryptoKeylen];
  uint8_t tx_key[kCryptoKeylen];
  uint8_t tx_hp[kCryptoKeylen];
  uint8_t rx_iv[kCryptoIvlen];
  uint8_t tx_iv[kCryptoIvlen];

  if (NGTCP2_ERR(ngtcp2_crypto_derive_and_install_rx_key(
          session()->connection(),
          rx_key,
          rx_iv,
          rx_hp,
          level,
          rx_secret,
          secretlen))) {
    return false;
  }

  if (NGTCP2_ERR(ngtcp2_crypto_derive_and_install_tx_key(
          session()->connection(),
          tx_key,
          tx_iv,
          tx_hp,
          level,
          tx_secret,
          secretlen))) {
    return false;
  }

  session()->state_->stream_open_allowed = 1;

  switch (level) {
  case NGTCP2_CRYPTO_LEVEL_EARLY:
    crypto::LogSecret(
        ssl_,
        kQuicClientEarlyTrafficSecret,
        rx_secret,
        secretlen);
    break;
  case NGTCP2_CRYPTO_LEVEL_HANDSHAKE:
    crypto::LogSecret(
        ssl_,
        kQuicClientHandshakeTrafficSecret,
        rx_secret,
        secretlen);
    crypto::LogSecret(
        ssl_,
        kQuicServerHandshakeTrafficSecret,
        tx_secret,
        secretlen);
    break;
  case NGTCP2_CRYPTO_LEVEL_APPLICATION:
    crypto::LogSecret(
        ssl_,
        kQuicClientTrafficSecret0,
        rx_secret,
        secretlen);
    crypto::LogSecret(
        ssl_,
        kQuicServerTrafficSecret,
        tx_secret,
        secretlen);
    break;
  default:
    UNREACHABLE();
  }

  return true;
}

void Session::IgnorePreferredAddressStrategy(
    Session* session,
    const PreferredAddress& preferred_address) {
  Debug(session, "Ignoring server preferred address");
}

void Session::UsePreferredAddressStrategy(
    Session* session,
    const PreferredAddress& preferred_address) {
  int family = session->endpoint()->local_address()->family();
  PreferredAddress::Address address = family == AF_INET
      ? preferred_address.ipv4()
      : preferred_address.ipv6();

  if (!preferred_address.Use(address)) {
    Debug(session, "Not using server preferred address");
    return;
  }

  Debug(session, "Using server preferred address");
  session->state_->using_preferred_address = 1;
}

bool Session::HasInstance(Environment* env, const Local<Value>& value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> Session::GetConstructorTemplate(Environment* env) {
  BindingState* state = env->GetBindingData<BindingState>(env->context());
  CHECK_NOT_NULL(state);
  Local<FunctionTemplate> tmpl = state->session_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = FunctionTemplate::New(env->isolate());
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Session"));
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Session::kInternalFieldCount);
    env->SetProtoMethodNoSideEffect(
        tmpl,
        "getRemoteAddress",
        GetRemoteAddress);
    env->SetProtoMethodNoSideEffect(
        tmpl,
        "getCertificate",
        GetCertificate);
    env->SetProtoMethodNoSideEffect(
        tmpl,
        "getPeerCertificate",
        GetPeerCertificate);
    env->SetProtoMethodNoSideEffect(
        tmpl,
        "getEphemeralKeyInfo",
        GetEphemeralKeyInfo);
    env->SetProtoMethod(tmpl, "destroy", DoDestroy);
    env->SetProtoMethod(tmpl, "gracefulClose", GracefulClose);
    env->SetProtoMethod(tmpl, "silentClose", SilentClose);
    env->SetProtoMethod(tmpl, "updateKey", UpdateKey);
    env->SetProtoMethod(tmpl, "attachToEndpoint", DoAttachToEndpoint);
    env->SetProtoMethod(tmpl, "detachFromEndpoint", DoDetachFromEndpoint);
    env->SetProtoMethod(tmpl, "onClientHelloDone", OnClientHelloDone);
    env->SetProtoMethod(tmpl, "onOCSPDone", OnOCSPDone);
    env->SetProtoMethod(tmpl, "openStream", DoOpenStream);
    env->SetProtoMethod(tmpl, "sendDatagram", DoSendDatagram);
    state->set_session_constructor_template(tmpl);
  }
  return tmpl;
}

void Session::Initialize(Environment* env, Local<Object> target) {
  USE(GetConstructorTemplate(env));

  OptionsObject::Initialize(env, target);
  Http3OptionsObject::Initialize(env, target);

#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATS_SESSION_##name);
  SESSION_STATS(V)
  NODE_DEFINE_CONSTANT(target, IDX_STATS_SESSION_COUNT);
#undef V
#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATE_SESSION_##name);
  SESSION_STATE(V)
  NODE_DEFINE_CONSTANT(target, IDX_STATE_SESSION_COUNT);
#undef V

  constexpr uint32_t STREAM_DIRECTION_BIDIRECTIONAL =
      static_cast<uint32_t>(Stream::Direction::BIDIRECTIONAL);
  constexpr uint32_t STREAM_DIRECTION_UNIDIRECTIONAL =
      static_cast<uint32_t>(Stream::Direction::UNIDIRECTIONAL);

  NODE_DEFINE_CONSTANT(target, STREAM_DIRECTION_BIDIRECTIONAL);
  NODE_DEFINE_CONSTANT(target, STREAM_DIRECTION_UNIDIRECTIONAL);
}

BaseObjectPtr<Session> Session::CreateClient(
    EndpointWrap* endpoint,
    const std::shared_ptr<SocketAddress>& local_addr,
    const std::shared_ptr<SocketAddress>& remote_addr,
    const Config& config,
    const std::shared_ptr<Options>& options,
    const BaseObjectPtr<crypto::SecureContext>& context,
    const crypto::ArrayBufferOrViewContents<uint8_t>& session_ticket,
    const crypto::ArrayBufferOrViewContents<uint8_t>& remote_transport_params) {
  Local<Object> obj;
  Local<FunctionTemplate> tmpl = GetConstructorTemplate(endpoint->env());
  CHECK(!tmpl.IsEmpty());
  if (!tmpl->InstanceTemplate()->NewInstance(endpoint->env()->context())
          .ToLocal(&obj)) {
    return BaseObjectPtr<Session>();
  }
  BaseObjectPtr<Session> session =
      MakeBaseObject<Session>(
          endpoint,
          obj,
          local_addr,
          remote_addr,
          config,
          options,
          context,
          config.version,
          session_ticket,
          remote_transport_params);
  if (session) session->SendPendingData();
  return session;
}

// Static function to create a new server Session instance
BaseObjectPtr<Session> Session::CreateServer(
    EndpointWrap* endpoint,
    const std::shared_ptr<SocketAddress>& local_addr,
    const std::shared_ptr<SocketAddress>& remote_addr,
    const CID& dcid,
    const CID& scid,
    const CID& ocid,
    const Config& config,
    const std::shared_ptr<Options>& options,
    const BaseObjectPtr<crypto::SecureContext>& context) {
  HandleScope scope(endpoint->env()->isolate());
  Local<Object> obj;
  Local<FunctionTemplate> tmpl = GetConstructorTemplate(endpoint->env());
  CHECK(!tmpl.IsEmpty());
  if (!tmpl->InstanceTemplate()->NewInstance(endpoint->env()->context())
          .ToLocal(&obj)) {
    return BaseObjectPtr<Session>();
  }

  return MakeDetachedBaseObject<Session>(
      endpoint,
      obj,
      local_addr,
      remote_addr,
      config,
      options,
      context,
      dcid,
      scid,
      ocid,
      config.version);
}

Session::Session(
    EndpointWrap* endpoint,
    v8::Local<v8::Object> object,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    const std::shared_ptr<Options>& options,
    const BaseObjectPtr<crypto::SecureContext>& context,
    const CID& dcid,
    ngtcp2_crypto_side side)
    : AsyncWrap(endpoint->env(), object, AsyncWrap::PROVIDER_QUICSESSION),
      SessionStatsBase(endpoint->env()),
      allocator_(BindingState::GetAllocator(endpoint->env())),
      options_(options),
      endpoint_(endpoint),
      state_(endpoint->env()),
      local_address_(local_address),
      remote_address_(remote_address),
      application_(SelectApplication(options->alpn, options->application)),
      crypto_context_(std::make_unique<CryptoContext>(
          this,
          options,
          std::move(context),
          side)),
      idle_(endpoint->env(), [this]() { OnIdleTimeout(); }),
      retransmit_(endpoint->env(), [this]() { OnRetransmitTimeout(); }),
      dcid_(dcid),
      max_pkt_len_(get_max_pkt_len(remote_address)),
      cid_strategy_(options->cid_strategy->NewInstance(this)) {
  MakeWeak();
  if (options->scid)
    scid_ = options->scid;
  else
    cid_strategy_->NewConnectionID(&scid_);
  ExtendMaxStreamsBidi(DEFAULT_MAX_STREAMS_BIDI);
  ExtendMaxStreamsUni(DEFAULT_MAX_STREAMS_UNI);

  state_->client_hello = options_->client_hello ? 1 : 0;
  state_->ocsp = options_->ocsp ? 1 : 0;

  Debug(this, "Initializing session from %s to %s",
        local_address_->ToString(),
        remote_address_->ToString());

  object->DefineOwnProperty(
      env()->context(),
      env()->state_string(),
      state_.GetArrayBuffer(),
      PropertyAttribute::ReadOnly).Check();

  object->DefineOwnProperty(
      env()->context(),
      env()->stats_string(),
      ToBigUint64Array(env()),
      PropertyAttribute::ReadOnly).Check();

  if (UNLIKELY(options->qlog)) {
    qlogstream_ = LogStream::Create(env());
    if (LIKELY(qlogstream_)) {
      BindingState* state = BindingState::Get(env());
      object->DefineOwnProperty(
          env()->context(),
          state->qlog_string(),
          qlogstream_->object(),
          PropertyAttribute::ReadOnly).Check();
    }
  }

  if (UNLIKELY(options->keylog)) {
    keylogstream_ = LogStream::Create(env());
    if (LIKELY(keylogstream_)) {
      BindingState* state = BindingState::Get(env());
      object->DefineOwnProperty(
          env()->context(),
          state->keylog_string(),
          keylogstream_->object(),
          PropertyAttribute::ReadOnly).Check();
    }
  }

  idle_.Unref();
  retransmit_.Unref();
}

Session::Session(
    EndpointWrap* endpoint,
    Local<Object> object,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    const Config& config,
    const std::shared_ptr<Options>& options,
    const BaseObjectPtr<crypto::SecureContext>& context,
    const CID& dcid,
    const CID& scid,
    const CID& ocid,
    quic_version version)
    : Session(
          endpoint,
          object,
          local_address,
          remote_address,
          options,
          context,
          dcid,
          NGTCP2_CRYPTO_SIDE_SERVER) {
  TransportParams transport_params(options, scid, ocid);
  transport_params.GenerateStatelessResetToken(endpoint, scid_);
  if (transport_params.preferred_address_present) {
    transport_params.GeneratePreferredAddressToken(
        cid_strategy_.get(),
        this,
        &pscid_);
  }

  Path path(local_address, remote_address);

  ngtcp2_conn* conn;
  CHECK_EQ(
      ngtcp2_conn_server_new(
        &conn,
        dcid.cid(),
        scid_.cid(),
        &path,
        version,
        &callbacks[crypto_context_->side()],
        &config,
        &transport_params,
        &allocator_,
        this), 0);
  connection_.reset(conn);
  crypto_context_->Initialize();

  AttachToEndpoint();

  UpdateDataStats();
  UpdateIdleTimer();
}

Session::Session(
    EndpointWrap* endpoint,
    Local<Object> object,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    const Config& config,
    const std::shared_ptr<Options>& options,
    const BaseObjectPtr<crypto::SecureContext>& context,
    quic_version version,
    const crypto::ArrayBufferOrViewContents<uint8_t>& session_ticket,
    const crypto::ArrayBufferOrViewContents<uint8_t>& remote_transport_params)
    : Session(
          endpoint,
          object,
          local_address,
          remote_address,
          options,
          std::move(context)) {
  CID dcid;
  if (options->dcid) {
    dcid = options->dcid;
  } else {
    cid_strategy_->NewConnectionID(&dcid);
  }
  CHECK(dcid);

  TransportParams transport_params(options);
  Path path(local_address_, remote_address_);

  ngtcp2_conn* conn;
  CHECK_EQ(
      ngtcp2_conn_client_new(
          &conn,
          dcid.cid(),
          scid_.cid(),
          &path,
          version,
          &callbacks[crypto_context_->side()],
          &config,
          &transport_params,
          &allocator_,
          this), 0);
  connection_.reset(conn);

  crypto_context_->Initialize();

  crypto_context_->MaybeSetEarlySession(
      session_ticket,
      remote_transport_params);

  AttachToEndpoint();

  UpdateIdleTimer();
  UpdateDataStats();
}

Session::~Session() {
  if (qlogstream_) {
    env()->SetImmediate([ptr = std::move(qlogstream_)](Environment*) {
      ptr->End();
    });
  }
  if (keylogstream_) {
    env()->SetImmediate([ptr = std::move(keylogstream_)](Environment*) {
      ptr->End();
    });
  }
  idle_.Stop();
  retransmit_.Stop();
  DebugStats(this);
}

void Session::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("options", options_);
  tracker->TrackField("endpoint", endpoint_);
  tracker->TrackField("streams", streams_);
  tracker->TrackField("local_address", local_address_);
  tracker->TrackField("remote_address", remote_address_);
  tracker->TrackField("application", application_);
  tracker->TrackField("crypto_context", crypto_context_);
  tracker->TrackField("idle_timer", idle_);
  tracker->TrackField("retransmit_timer", retransmit_);
  tracker->TrackField("conn_closebuf", conn_closebuf_);
  tracker->TrackField("qlogstream", qlogstream_);
  tracker->TrackField("keylogstream", keylogstream_);
}

void Session::AckedStreamDataOffset(
    stream_id id,
    uint64_t offset,
    uint64_t datalen) {
  Debug(this,
        "Received acknowledgement for %" PRIu64
        " bytes of stream %" PRId64 " data",
        datalen, id);

  application_->AcknowledgeStreamData(
      id,
      offset,
      static_cast<size_t>(datalen));
}

void Session::AddStream(const BaseObjectPtr<Stream>& stream) {
  Debug(this, "Adding stream %" PRId64 " to session", stream->id());
  streams_.emplace(stream->id(), stream);
  stream->Resume();

  // Update tracking statistics for the number of streams associated with
  // this session.
  switch (stream->origin()) {
    case Stream::Origin::CLIENT:
      if (is_server())
        IncrementStat(&SessionStats::streams_in_count);
      else
        IncrementStat(&SessionStats::streams_out_count);
      break;
    case Stream::Origin::SERVER:
      if (is_server())
        IncrementStat(&SessionStats::streams_out_count);
      else
        IncrementStat(&SessionStats::streams_in_count);
  }
  IncrementStat(&SessionStats::streams_out_count);
  switch (stream->direction()) {
    case Stream::Direction::BIDIRECTIONAL:
      IncrementStat(&SessionStats::bidi_stream_count);
      break;
    case Stream::Direction::UNIDIRECTIONAL:
      IncrementStat(&SessionStats::uni_stream_count);
      break;
  }
}

void Session::AttachToEndpoint() {
  CHECK(endpoint_);
  Debug(this, "Adding session to %s", endpoint_->diagnostic_name());
  endpoint_->AddSession(scid_, BaseObjectPtr<Session>(this));
  switch (crypto_context_->side()) {
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      endpoint_->AssociateCID(dcid_, scid_);
      endpoint_->AssociateCID(pscid_, scid_);
      break;
    }
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      std::vector<ngtcp2_cid> cids(ngtcp2_conn_get_num_scid(connection()));
      ngtcp2_conn_get_scid(connection(), cids.data());
      for (const ngtcp2_cid& cid : cids)
        endpoint_->AssociateCID(CID(&cid), scid_);
      break;
    }
    default:
      UNREACHABLE();
  }

  std::vector<ngtcp2_cid_token> tokens(
      ngtcp2_conn_get_num_active_dcid(connection()));
  ngtcp2_conn_get_active_dcid(connection(), tokens.data());
  for (const ngtcp2_cid_token& token : tokens) {
    if (token.token_present) {
      endpoint_->AssociateStatelessResetToken(
          StatelessResetToken(token.token),
          BaseObjectPtr<Session>(this));
    }
  }
}

// A client Session can be migrated to a different Endpoint instance.
bool Session::AttachToNewEndpoint(EndpointWrap* endpoint, bool nat_rebinding) {
  CHECK(!is_server());
  CHECK(!is_destroyed());

  // If we're in the process of gracefully closing, attaching the session
  // to a new endpoint is not allowed.
  if (state_->graceful_closing)
    return false;

  if (endpoint == nullptr || endpoint == endpoint_.get())
    return true;

  Debug(this, "Migrating to %s", endpoint_->diagnostic_name());

  // Ensure that we maintain a reference to keep this from being
  // destroyed while we are starting the migration.
  BaseObjectPtr<Session> ptr(this);

  // Step 1: Remove the session from the current socket
  DetachFromEndpoint();

  endpoint_.reset(endpoint);
  // Step 2: Add this Session to the given Socket
  AttachToEndpoint();

  auto local_address = endpoint->local_address();

  // The nat_rebinding option here should rarely, if ever
  // be used in a real application. It is intended to serve
  // as a way of simulating a silent local address change,
  // such as when the NAT binding changes. Currently, Node.js
  // does not really have an effective way of detecting that.
  // Manual user code intervention to handle the migration
  // to the new Endpoint is required, which should always
  // trigger path validation using the ngtcp2_conn_initiate_migration.
  if (LIKELY(!nat_rebinding)) {
    SendSessionScope send(this);
    Path path(local_address, remote_address_);
    return ngtcp2_conn_initiate_migration(
        connection(),
        &path,
        uv_hrtime()) == 0;
  } else {
    ngtcp2_addr addr;
    ngtcp2_conn_set_local_addr(
        connection(),
        ngtcp2_addr_init(
            &addr,
            local_address->data(),
            local_address->length(),
            nullptr));
  }

  return true;
}

void Session::Close(SessionCloseFlags close_flags) {
  if (is_destroyed())
    return;
  bool silent = close_flags == SessionCloseFlags::SILENT;
  bool stateless_reset = silent && state_->stateless_reset;

  // If we're not running within a ngtcp2 callback scope, schedule
  // a CONNECTION_CLOSE to be sent when Close exits. If we are
  // within a ngtcp2 callback scope, sending the CONNECTION_CLOSE
  // will be deferred.
  ConnectionCloseScope close_scope(this, silent);

  // Once Close has been called, we cannot re-enter
  if (UNLIKELY(state_->closing))
    return;

  state_->closing = 1;
  state_->silent_close = silent ? 1 : 0;

  QuicError error = last_error();
  Debug(this,
        "Closing with error: %s (silent: %s, stateless reset: %s)",
        error,
        silent ? "Y" : "N",
        stateless_reset ? "Y" : "N");

  if (!state_->wrapped)
    return Destroy();

  // If the Session has been wrapped by a JS object, we have to
  // notify the JavaScript side that the session is being closed.
  // If it hasn't yet been wrapped, we can skip the call and and
  // go straight to destroy.
  BaseObjectPtr<Session> ptr(this);

  BindingState* state = BindingState::Get(env());
  HandleScope scope(env() ->isolate());
  Context::Scope context_scope(env()->context());

  CallbackScope cb_scope(this);

  Local<Value> argv[] = {
    Number::New(env()->isolate(), static_cast<double>(error.code)),
    Integer::New(env()->isolate(), static_cast<int32_t>(error.type)),
    silent
        ? v8::True(env()->isolate())
        : v8::False(env()->isolate()),
    stateless_reset
        ? v8::True(env()->isolate())
        : v8::False(env()->isolate())
  };

  USE(state->session_close_callback()->Call(
      env()->context(),
      object(),
      arraysize(argv),
      argv));
}

BaseObjectPtr<Stream> Session::CreateStream(stream_id id) {
  CHECK(!is_destroyed());
  CHECK_EQ(state_->graceful_closing, 0);
  CHECK_EQ(state_->closing, 0);

  BaseObjectPtr<Stream> stream = Stream::Create(env(), this, id);
  CHECK(stream);
  AddStream(stream);

  BindingState* state = BindingState::Get(env());
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  CallbackScope cb_scope(this);

  Local<Value> arg = stream->object();

  // Grab a shared pointer to this to prevent the Session
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<Session> ptr(this);

  USE(state->stream_created_callback()->Call(
      env()->context(),
      object(),
      1, &arg));

  return stream;
}

bool Session::SendDatagram(
    const std::shared_ptr<v8::BackingStore>& store,
    size_t offset,
    size_t length) {

  // Step 0: If the remote transport params aren't known, we can't
  // know what size datagram to send, so don't.
  if (!transport_params_set_)
    return false;

  uint64_t max_datagram_size = std::min(
    static_cast<uint64_t>(kDefaultMaxPacketLength),
    transport_params_.max_datagram_frame_size);

  // The datagram will be ignored if it's too large
  if (length > max_datagram_size)
    return false;

  ngtcp2_vec vec;
  vec.base = reinterpret_cast<uint8_t*>(store->Data()) + offset;
  vec.len = length;

  std::unique_ptr<Packet> packet = std::make_unique<Packet>("datagram");
  PathStorage path;
  int accepted = 0;
  ssize_t res = ngtcp2_conn_writev_datagram(
      connection(),
      &path.path,
      nullptr,
      packet->data(),
      max_packet_length(),
      &accepted,
      NGTCP2_DATAGRAM_FLAG_NONE,
      &vec, 1,
      uv_hrtime());

  // The packet could not be written. There are several reasons
  // this could be. Either we're currently at the congestion
  // control limit, the data does not fit into the packet,
  // or we've hit the amplificiation limit, etc. We check
  // accepted just in case but oherwise just return false here.
  if (res == 0 || accepted == 0) {
    CHECK_EQ(accepted, 0);
    return false;
  }

  // If we got here, the data should have been written. Verify.
  CHECK_NE(accepted, 0);
  packet->set_length(res);

  return SendPacket(std::move(packet), path);
}

void Session::Datagram(uint32_t flags, const uint8_t* data, size_t datalen) {
  if (LIKELY(options_->max_datagram_frame_size == 0) || UNLIKELY(datalen == 0))
    return;

  BindingState* state = BindingState::Get(env());
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  CallbackScope cb_scope(this);

  std::shared_ptr<BackingStore> store =
      ArrayBuffer::NewBackingStore(env()->isolate(), datalen);
  if (!store)
    return;
  memcpy(store->Data(), data, datalen);

  Local<Value> argv[] = {
    ArrayBuffer::New(env()->isolate(), store),
    flags & NGTCP2_DATAGRAM_FLAG_EARLY
       ? v8::True(env()->isolate())
       : v8::False(env()->isolate())
  };

  // Grab a shared pointer to this to prevent the Session
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<Session> ptr(this);

  USE(state->session_datagram_callback()->Call(
      env()->context(),
      object(),
      arraysize(argv),
      argv));
}

void Session::Destroy() {
  if (is_destroyed())
    return;

  Debug(this, "Destroying the Session");

  // Mark the session destroyed.
  state_->destroyed = 1;
  state_->closing = 0;
  state_->graceful_closing = 0;

  // TODO(@jasnell): Allow overriding the close code

  // If we're not already in a ConnectionCloseScope, schedule
  // sending a CONNECTION_CLOSE when destroy exits. If we are
  // running within an ngtcp2 callback scope, sending the
  // CONNECTION_CLOSE will be deferred.
  {
    ConnectionCloseScope close_scope(this, state_->silent_close);

    // All existing streams should have already been destroyed
    CHECK(streams_.empty());

    // Stop and free the idle and retransmission timers if they are active.
    idle_.Stop();
    retransmit_.Stop();
  }

  // The Session instances are kept alive usingBaseObjectPtr. The
  // only persistent BaseObjectPtr is the map in the associated
  // Endpoint. Removing the Session from the Endpoint will free
  // that pointer, allowing the Session to be deconstructed once
  // the stack unwinds and any remaining BaseObjectPtr<Session>
  // instances fall out of scope.
  DetachFromEndpoint();
}

void Session::DetachFromEndpoint() {
  CHECK(endpoint_);
  Debug(this, "Removing Session from %s", endpoint_->diagnostic_name());
  if (is_server()) {
    endpoint_->DisassociateCID(dcid_);
    endpoint_->DisassociateCID(pscid_);
  }

  std::vector<ngtcp2_cid> cids(ngtcp2_conn_get_num_scid(connection()));
  std::vector<ngtcp2_cid_token> tokens(
      ngtcp2_conn_get_num_active_dcid(connection()));
  ngtcp2_conn_get_scid(connection(), cids.data());
  ngtcp2_conn_get_active_dcid(connection(), tokens.data());

  for (const ngtcp2_cid& cid : cids)
    endpoint_->DisassociateCID(CID(&cid));

  for (const ngtcp2_cid_token& token : tokens) {
    if (token.token_present) {
      endpoint_->DisassociateStatelessResetToken(
          StatelessResetToken(token.token));
    }
  }

  Debug(this, "Removed from the endpoint");
  BaseObjectPtr<EndpointWrap> endpoint = std::move(endpoint_);
  endpoint->RemoveSession(scid_, remote_address_);
}

void Session::ExtendMaxStreamData(stream_id id, uint64_t max_data) {
  Debug(this,
        "Extending max stream %" PRId64 " data to %" PRIu64, id, max_data);
  application_->ExtendMaxStreamData(id, max_data);
}

void Session::ExtendMaxStreamsBidi(uint64_t max_streams) {
  // Nothing to do here currently
}

void Session::ExtendMaxStreamsRemoteUni(uint64_t max_streams) {
  Debug(this, "Extend remote max unidirectional streams: %" PRIu64,
        max_streams);
  application_->ExtendMaxStreamsRemoteUni(max_streams);
}

void Session::ExtendMaxStreamsRemoteBidi(uint64_t max_streams) {
  Debug(this, "Extend remote max bidirectional streams: %" PRIu64, max_streams);
  application_->ExtendMaxStreamsRemoteBidi(max_streams);
}

void Session::ExtendMaxStreamsUni(uint64_t max_streams) {
  // Nothing to do here currently
}

void Session::ExtendOffset(size_t amount) {
  Debug(this, "Extending session offset by %" PRId64 " bytes", amount);
  ngtcp2_conn_extend_max_offset(connection(), amount);
}

void Session::ExtendStreamOffset(stream_id id, size_t amount) {
  Debug(this, "Extending max stream %" PRId64 " offset by %" PRId64 " bytes",
        id, amount);
  ngtcp2_conn_extend_max_stream_offset(connection(), id, amount);
}

BaseObjectPtr<Stream> Session::FindStream(stream_id id) const {
  auto it = streams_.find(id);
  return it == std::end(streams_) ? BaseObjectPtr<Stream>() : it->second;
}

void Session::GetConnectionCloseInfo() {
  ngtcp2_connection_close_error_code close_code;
  ngtcp2_conn_get_connection_close_error_code(connection(), &close_code);
  set_last_error(QuicError::FromNgtcp2(close_code));
}

void Session::GetLocalTransportParams(ngtcp2_transport_params* params) {
  CHECK(!is_destroyed());
  ngtcp2_conn_get_local_transport_params(connection(), params);
}

void Session::GetNewConnectionID(
    ngtcp2_cid* cid,
    uint8_t* token,
    size_t cidlen) {
  CHECK(cid_strategy_);
  cid_strategy_->NewConnectionID(cid, cidlen);
  CID cid_(cid);
  StatelessResetToken(
      token,
      endpoint_->config().reset_token_secret,
      cid_);
  endpoint_->AssociateCID(cid_, scid_);
}

SessionTicketAppData::Status Session::GetSessionTicketAppData(
    const SessionTicketAppData& app_data,
    SessionTicketAppData::Flag flag) {
  return application_->GetSessionTicketAppData(app_data, flag);
}

void Session::HandleError() {
  if (is_destroyed())
    return;

  // If the Session is a server, send a CONNECTION_CLOSE. In either
  // case, the closing timer will be set and the Session will be
  // destroyed.
  if (is_server())
    SendConnectionClose();
  else
    UpdateClosingTimer();
}

bool Session::HandshakeCompleted(
    const std::shared_ptr<SocketAddress>& remote_address) {
  RemoteTransportParamsDebug transport_params(this);
  Debug(this, "Handshake completed with %s. %s",
        remote_address->ToString(),
        transport_params);
  RecordTimestamp(&SessionStats::handshake_completed_at);

  if (is_server()) {
    uint8_t token[kMaxTokenLen];
    size_t tokenlen = 0;
    if (!endpoint()->GenerateNewToken(token, remote_address).To(&tokenlen)) {
      Debug(this, "Failed to generate new token on handshake complete");
      return false;
    }

    if (NGTCP2_ERR(ngtcp2_conn_submit_new_token(
            connection_.get(),
            token,
            tokenlen))) {
      Debug(this, "Failed to submit new token on handshake complete");
      return false;
    }

    HandshakeConfirmed();
  }

  BindingState* state = BindingState::Get(env());
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  CallbackScope cb_scope(this);

  Local<Value> argv[] = {
    Undefined(env()->isolate()),                   // Server name
    GetALPNProtocol(*this),                        // ALPN
    Undefined(env()->isolate()),                   // Cipher name
    Undefined(env()->isolate()),                   // Cipher version
    Integer::New(env()->isolate(), max_pkt_len_),  // Max packet length
    Undefined(env()->isolate()),                   // Validation error reason
    Undefined(env()->isolate()),                   // Validation error code
    crypto_context_->early_data() ?
        v8::True(env()->isolate()) :
        v8::False(env()->isolate())
  };

  std::string hostname = crypto_context_->servername();
  if (!ToV8Value(env()->context(), hostname).ToLocal(&argv[0]))
    return false;

  if (!crypto_context_->cipher_name(env()).ToLocal(&argv[2]) ||
      !crypto_context_->cipher_version(env()).ToLocal(&argv[3])) {
    return false;
  }

  int err = crypto_context_->VerifyPeerIdentity();
  if (err != X509_V_OK &&
      (!crypto::GetValidationErrorReason(env(), err).ToLocal(&argv[5]) ||
       !crypto::GetValidationErrorCode(env(), err).ToLocal(&argv[6]))) {
      return false;
  }

  BaseObjectPtr<Session> ptr(this);

  USE(state->session_handshake_callback()->Call(
      env()->context(),
      object(),
      arraysize(argv),
      argv));

  return true;
}

void Session::HandshakeConfirmed() {
  Debug(this, "Handshake is confirmed");
  RecordTimestamp(&SessionStats::handshake_confirmed_at);
  state_->handshake_confirmed = 1;
}

bool Session::HasStream(stream_id id) const {
  return streams_.find(id) != std::end(streams_);
}

bool Session::InitApplication() {
  Debug(this, "Initializing application handler for ALPN %s",
      options_->alpn.c_str() + 1);
  return application_->Initialize();
}

void Session::OnIdleTimeout() {
  if (!is_destroyed()) {
    if (state_->idle_timeout == 1) {
      Debug(this, "Idle timeout");
      Close(SessionCloseFlags::SILENT);
      return;
    }
    state_->idle_timeout = 1;
    UpdateClosingTimer();
  }
}

void Session::OnRetransmitTimeout() {
  if (is_destroyed()) return;
  uint64_t now = uv_hrtime();

  if (ngtcp2_conn_get_expiry(connection()) <= now) {
    Debug(this, "Retransmitting due to loss detection");
    IncrementStat(&SessionStats::loss_retransmit_count);
  }

  if (ngtcp2_conn_handle_expiry(connection(), now) != 0) {
    Debug(this, "Handling retransmission failed");
    HandleError();
  }

  SendPendingData();
}

Maybe<stream_id> Session::OpenStream(Stream::Direction direction) {
  DCHECK(!is_destroyed());
  DCHECK(!is_closing());
  DCHECK(!is_graceful_closing());
  stream_id id;
  switch (direction) {
    case Stream::Direction::BIDIRECTIONAL:
      if (ngtcp2_conn_open_bidi_stream(connection(), &id, nullptr) == 0)
        return Just(id);
      break;
    case Stream::Direction::UNIDIRECTIONAL:
      if (ngtcp2_conn_open_uni_stream(connection(), &id, nullptr) == 0)
        return Just(id);
      break;
    default:
      UNREACHABLE();
  }
  return Nothing<stream_id>();
}

void Session::PathValidation(
    const ngtcp2_path* path,
    ngtcp2_path_validation_result res) {
  SocketAddress local(path->local.addr);
  SocketAddress remote(path->remote.addr);
  switch (res) {
    case NGTCP2_PATH_VALIDATION_RESULT_FAILURE:
      Debug(this, "Path not validated: %s <=> %s", local, remote);
      break;
    case NGTCP2_PATH_VALIDATION_RESULT_SUCCESS:
      Debug(this, "Path validated: %s <=> %s", local, remote);
      break;
  }
}

bool Session::Receive(
    size_t nread,
    std::shared_ptr<v8::BackingStore> store,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address) {

  CHECK(!is_destroyed());

  Debug(this, "Receiving QUIC packet");
  IncrementStat(&SessionStats::bytes_received, nread);

  if (is_in_closing_period() && is_server()) {
    Debug(this, "Packet received while in closing period");
    IncrementConnectionCloseAttempts();
    // For server Session instances, we serialize the connection close
    // packet once but may sent it multiple times. If the client keeps
    // transmitting, then the connection close may have gotten lost.
    // We don't want to send the connection close in response to
    // every received packet, however, so we use an exponential
    // backoff, increasing the ratio of packets received to connection
    // close frame sent with every one we send.
    if (UNLIKELY(ShouldAttemptConnectionClose() &&
                 !SendConnectionClose())) {
      Debug(this, "Failure sending another connection close");
      return false;
    }
  }

  {
    // These are within a scope to ensure that the InternalCallbackScope
    // and HandleScope are both exited before continuing on with the
    // function. This allows any nextTicks and queued tasks to be processed
    // before we continue.
    auto update_stats = OnScopeLeave([&](){
      UpdateDataStats();
    });
    HandleScope handle_scope(env()->isolate());
    InternalCallbackScope callback_scope(this);
    remote_address_ = remote_address;
    Path path(local_address, remote_address_);
    uint8_t* data = static_cast<uint8_t*>(store->Data());
    if (!ReceivePacket(&path, data, nread)) {
      HandleError();
      return false;
    }
  }

  // Only send pending data if we haven't entered draining mode.
  // We enter the draining period when a CONNECTION_CLOSE has been
  // received from the remote peer.
  if (is_in_draining_period()) {
    Debug(this, "In draining period after processing packet");
    // If processing the packet puts us into draining period, there's
    // absolutely nothing left for us to do except silently close
    // and destroy this Session, which we do by updating the
    // closing timer.
    GetConnectionCloseInfo();
    UpdateClosingTimer();
    return true;
  }

  if (!is_destroyed())
    UpdateIdleTimer();
  SendPendingData();
  Debug(this, "Successfully processed received packet");
  return true;
}

bool Session::ReceivePacket(
    ngtcp2_path* path,
    const uint8_t* data,
    ssize_t nread) {
  CHECK(!is_destroyed());

  uint64_t now = uv_hrtime();
  SetStat(&SessionStats::received_at, now);
  ngtcp2_pkt_info pi;  // Not used but required.
  int err = ngtcp2_conn_read_pkt(connection(), path, &pi, data, nread, now);
  if (err < 0) {
    switch (err) {
      case NGTCP2_ERR_CALLBACK_FAILURE:
      case NGTCP2_ERR_DRAINING:
      case NGTCP2_ERR_RECV_VERSION_NEGOTIATION:
        break;
      case NGTCP2_ERR_RETRY:
        // This should only ever happen on the server
        CHECK(is_server());
        endpoint_->SendRetry(
            version(),
            scid_,
            dcid_,
            local_address_,
            remote_address_);
        // Fall through
      case NGTCP2_ERR_DROP_CONN:
        Close(SessionCloseFlags::SILENT);
        break;
      default:
        set_last_error({
          QuicError::Type::APPLICATION,
          ngtcp2_err_infer_quic_transport_error_code(err)
        });
        return false;
    }
  }

  // If the Session has been destroyed but it is not
  // in the closing period, a CONNECTION_CLOSE has not yet
  // been sent to the peer. Let's attempt to send one. This
  // will have the effect of setting the idle timer to the
  // closing/draining period, after which the Session
  // will be destroyed.
  if (is_destroyed() && !is_in_closing_period()) {
    Debug(this, "Session was destroyed while processing the packet");
    return SendConnectionClose();
  }

  return true;
}

bool Session::ReceiveStreamData(
    uint32_t flags,
    stream_id id,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) {
  auto leave = OnScopeLeave([&]() {
    // Unconditionally extend the flow control window for the entire
    // session but not for the individual Stream.
    ExtendOffset(datalen);
  });

  return application_->ReceiveStreamData(
      flags,
      id,
      data,
      datalen,
      offset);
}

void Session::ResumeStream(stream_id id) {
  application()->ResumeStream(id);
}

void Session::SelectPreferredAddress(
    const PreferredAddress& preferred_address) {
  CHECK(!is_server());
  options_->preferred_address_strategy(this, preferred_address);
}

bool Session::SendConnectionClose() {
  CHECK(!NgCallbackScope::InNgCallbackScope(this));

  // Do not send any frames at all if we're in the draining period
  // or in the middle of a silent close
  if (is_in_draining_period() || state_->silent_close)
    return true;

  // The specific handling of connection close varies for client
  // and server Session instances. For servers, we will
  // serialize the connection close once but may end up transmitting
  // it multiple times; whereas for clients, we will serialize it
  // once and send once only.
  QuicError error = last_error();
  Debug(this, "Sending connection close with error: %s", error);

  UpdateClosingTimer();

  // If initial keys have not yet been installed, use the alternative
  // ImmediateConnectionClose to send a stateless connection close to
  // the peer.
  if (crypto_context()->write_crypto_level() ==
        NGTCP2_CRYPTO_LEVEL_INITIAL) {
    endpoint_->ImmediateConnectionClose(
        version(),
        dcid(),
        scid_,
        local_address_,
        remote_address_,
        error.code);
    return true;
  }

  switch (crypto_context_->side()) {
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      if (!is_in_closing_period() && !StartClosingPeriod()) {
        Close(SessionCloseFlags::SILENT);
        return false;
      }
      CHECK_GT(conn_closebuf_->length(), 0);
      return SendPacket(Packet::Copy(conn_closebuf_));
    }
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      std::unique_ptr<Packet> packet =
          std::make_unique<Packet>("client connection close");
      ssize_t nwrite =
          SelectCloseFn(error)(
            connection(),
            nullptr,
            nullptr,
            packet->data(),
            max_pkt_len_,
            error.code,
            uv_hrtime());
      if (UNLIKELY(nwrite < 0)) {
        Debug(this, "Error writing connection close: %d", nwrite);
        set_last_error(kQuicInternalError);
        Close(SessionCloseFlags::SILENT);
        return false;
      }
      packet->set_length(nwrite);
      return SendPacket(std::move(packet));
    }
    default:
      UNREACHABLE();
  }
}

bool Session::SendPacket(std::unique_ptr<Packet> packet) {
  CHECK(!is_in_draining_period());

  // There's nothing to send.
  if (!packet || packet->length() == 0)
    return true;

  IncrementStat(&SessionStats::bytes_sent, packet->length());
  RecordTimestamp(&SessionStats::sent_at);
  ScheduleRetransmit();

  Debug(this, "Sending %" PRIu64 " bytes to %s from %s",
        packet->length(),
        remote_address_->ToString(),
        local_address_->ToString());

  endpoint_->SendPacket(
      local_address_,
      remote_address_,
      std::move(packet),
      BaseObjectPtr<Session>(this));

  return true;
}

bool Session::SendPacket(
    std::unique_ptr<Packet> packet,
    const ngtcp2_path_storage& path) {
  UpdateEndpoint(path.path);
  return SendPacket(std::move(packet));
}

void Session::SendPendingData() {
  if (is_unable_to_send_packets())
    return;

  Debug(this, "Sending pending data");
  if (!application_->SendPendingData()) {
    Debug(this, "Error sending pending application data");
    HandleError();
  }
  ScheduleRetransmit();
}

void Session::SetSessionTicketAppData(const SessionTicketAppData& app_data) {
  application_->SetSessionTicketAppData(app_data);
}

void Session::StreamDataBlocked(stream_id id) {
  IncrementStat(&SessionStats::block_count);

  BaseObjectPtr<Stream> stream = FindStream(id);
  if (stream)
      stream->OnBlocked();
}

void Session::IncrementConnectionCloseAttempts() {
  if (connection_close_attempts_ < kMaxSizeT)
    connection_close_attempts_++;
}

void Session::RemoveStream(stream_id id) {
  Debug(this, "Removing stream %" PRId64, id);

  // ngtcp2 does not extend the max streams count automatically
  // except in very specific conditions, none of which apply
  // once we've gotten this far. We need to manually extend when
  // a remote peer initiated stream is removed.
  if (!is_in_draining_period() &&
      !is_in_closing_period() &&
      !state_->silent_close &&
      !ngtcp2_conn_is_local_stream(connection_.get(), id)) {
    if (ngtcp2_is_bidi_stream(id))
      ngtcp2_conn_extend_max_streams_bidi(connection_.get(), 1);
    else
      ngtcp2_conn_extend_max_streams_uni(connection_.get(), 1);
  }

  // Frees the persistent reference to the Stream object,
  // allowing it to be gc'd any time after the JS side releases
  // it's own reference.
  streams_.erase(id);
}

void Session::ScheduleRetransmit() {
  uint64_t now = uv_hrtime();
  uint64_t expiry = ngtcp2_conn_get_expiry(connection());
  // now and expiry are in nanoseconds, interval is milliseconds
  uint64_t interval = (expiry < now) ? 1 : (expiry - now) / 1000000UL;
  // If interval ends up being 0, the repeating timer won't be
  // scheduled, so set it to 1 instead.
  if (interval == 0) interval = 1;
  Debug(this, "Scheduling the retransmit timer for %" PRIu64, interval);
  UpdateRetransmitTimer(interval);
}

bool Session::ShouldAttemptConnectionClose() {
  if (connection_close_attempts_ == connection_close_limit_) {
    if (connection_close_limit_ * 2 <= kMaxSizeT)
      connection_close_limit_ *= 2;
    else
      connection_close_limit_ = kMaxSizeT;
    return true;
  }
  return false;
}

void Session::ShutdownStreamWrite(stream_id id, error_code code) {
  if (is_in_closing_period() ||
      is_in_draining_period() ||
      state_->silent_close == 1) {
    return;  // Nothing to do because we can't send any frames.
  }
  SendSessionScope send_scope(this);
  ngtcp2_conn_shutdown_stream_write(connection(), id, 0);
}

void Session::ShutdownStream(stream_id id, error_code code) {
  if (is_in_closing_period() ||
      is_in_draining_period() ||
      state_->silent_close == 1) {
    return;  // Nothing to do because we can't send any frames.
  }
  SendSessionScope send_scope(this);
  ngtcp2_conn_shutdown_stream(connection(), id, 0);
}

bool Session::StartClosingPeriod() {
  if (is_destroyed())
    return false;
  if (is_in_closing_period())
    return true;

  QuicError error = last_error();
  Debug(this, "Closing period has started. Error %s", error);

  conn_closebuf_ = std::make_unique<Packet>("server connection close");

  ssize_t nwrite =
      SelectCloseFn(error)(
          connection(),
          nullptr,
          nullptr,
          conn_closebuf_->data(),
          max_pkt_len_,
          error.code,
          uv_hrtime());
  if (nwrite < 0) {
    set_last_error(kQuicInternalError);
    return false;
  }
  conn_closebuf_->set_length(nwrite);
  return true;
}

void Session::StartGracefulClose() {
  state_->graceful_closing = 1;
  RecordTimestamp(&SessionStats::closing_at);
}

void Session::StreamClose(stream_id id, error_code app_error_code) {
  Debug(this, "Closing stream %" PRId64 " with code %" PRIu64,
        id,
        app_error_code);

  application_->StreamClose(id, app_error_code);
}

void Session::StreamReset(
    stream_id id,
    uint64_t final_size,
    error_code app_error_code) {
  Debug(this,
        "Reset stream %" PRId64 " with code %" PRIu64
        " and final size %" PRIu64,
        id,
        app_error_code,
        final_size);

  BaseObjectPtr<Stream> stream = FindStream(id);

  if (stream) {
    stream->set_final_size(final_size);
    application_->StreamReset(id, app_error_code);
  }
}

void Session::UpdateClosingTimer() {
  if (state_->closing_timer_enabled)
    return;
  state_->closing_timer_enabled = 1;
  uint64_t timeout =
      is_server() ? (ngtcp2_conn_get_pto(connection()) / 1000000ULL) * 3 : 0;
  Debug(this, "Setting closing timeout to %" PRIu64, timeout);
  retransmit_.Stop();
  idle_.Update(timeout, 0);
  idle_.Ref();
}

void Session::UpdateConnectionID(
    int type,
    const CID& cid,
    const StatelessResetToken& token) {
  switch (type) {
    case NGTCP2_CONNECTION_ID_STATUS_TYPE_ACTIVATE:
      endpoint_->AssociateStatelessResetToken(
          token,
          BaseObjectPtr<Session>(this));
      break;
    case NGTCP2_CONNECTION_ID_STATUS_TYPE_DEACTIVATE:
      endpoint_->DisassociateStatelessResetToken(token);
      break;
  }
}

void Session::UpdateDataStats() {
  if (state_->destroyed)
    return;

  ngtcp2_conn_stat stat;
  ngtcp2_conn_get_conn_stat(connection(), &stat);

  SetStat(
      &SessionStats::bytes_in_flight,
      stat.bytes_in_flight);
  SetStat(
      &SessionStats::congestion_recovery_start_ts,
      stat.congestion_recovery_start_ts);
  SetStat(&SessionStats::cwnd, stat.cwnd);
  SetStat(&SessionStats::delivery_rate_sec, stat.delivery_rate_sec);
  SetStat(&SessionStats::first_rtt_sample_ts, stat.first_rtt_sample_ts);
  SetStat(&SessionStats::initial_rtt, stat.initial_rtt);
  SetStat(&SessionStats::last_tx_pkt_ts,
          reinterpret_cast<uint64_t>(stat.last_tx_pkt_ts));
  SetStat(&SessionStats::latest_rtt, stat.latest_rtt);
  SetStat(&SessionStats::loss_detection_timer, stat.loss_detection_timer);
  SetStat(&SessionStats::loss_time,
          reinterpret_cast<uint64_t>(stat.loss_time));
  SetStat(&SessionStats::max_udp_payload_size, stat.max_udp_payload_size);
  SetStat(&SessionStats::min_rtt, stat.min_rtt);
  SetStat(&SessionStats::pto_count, stat.pto_count);
  SetStat(&SessionStats::rttvar, stat.rttvar);
  SetStat(&SessionStats::smoothed_rtt, stat.smoothed_rtt);
  SetStat(&SessionStats::ssthresh, stat.ssthresh);

  // The max_bytes_in_flight is a highwater mark that can be used
  // in performance analysis operations.
  if (stat.bytes_in_flight > GetStat(&SessionStats::max_bytes_in_flight))
    SetStat(&SessionStats::max_bytes_in_flight, stat.bytes_in_flight);
}

void Session::UpdateEndpoint(const ngtcp2_path& path) {
  remote_address_->Update(path.remote.addr, path.remote.addrlen);
  local_address_->Update(path.local.addr, path.local.addrlen);
  if (remote_address_->family() == AF_INET6) {
    remote_address_->set_flow_label(
        endpoint_->GetFlowLabel(
            local_address_,
            remote_address_,
            scid_));
  }
}

void Session::UpdateIdleTimer() {
  if (state_->closing_timer_enabled)
    return;
  uint64_t now = uv_hrtime();
  uint64_t expiry = ngtcp2_conn_get_idle_expiry(connection());
  // nano to millis
  uint64_t timeout = expiry > now ? (expiry - now) / 1000000ULL : 1;
  if (timeout == 0) timeout = 1;
  Debug(this, "Updating idle timeout to %" PRIu64, timeout);
  idle_.Update(timeout, timeout);
}

void Session::UpdateRetransmitTimer(uint64_t timeout) {
  retransmit_.Update(timeout, timeout);
}

void Session::VersionNegotiation(const quic_version* sv, size_t nsv) {
  CHECK(!is_server());

  BindingState* state = BindingState::Get(env());
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  CallbackScope cb_scope(this);

  std::vector<Local<Value>> versions(nsv);

  for (size_t n = 0; n < nsv; n++)
    versions[n] = Integer::New(env()->isolate(), sv[n]);

  // Currently, we only support one version of QUIC but in
  // the future that may change. The callback below passes
  // an array back to the JavaScript side to future-proof.
  Local<Value> supported = Integer::New(env()->isolate(), NGTCP2_PROTO_VER_MAX);

  Local<Value> argv[] = {
    Integer::New(env()->isolate(), NGTCP2_PROTO_VER_MAX),
    Array::New(env()->isolate(), versions.data(), nsv),
    Array::New(env()->isolate(), &supported, 1)
  };

  // Grab a shared pointer to this to prevent the Session
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<Session> ptr(this);
  USE(state->session_version_negotiation_callback()->Call(
      env()->context(),
      object(),
      arraysize(argv),
      argv));
}

EndpointWrap* Session::endpoint() const { return endpoint_.get(); }

bool Session::is_handshake_completed() const {
  DCHECK(!is_destroyed());
  return ngtcp2_conn_get_handshake_completed(connection());
}

bool Session::is_in_closing_period() const {
  return ngtcp2_conn_is_in_closing_period(connection());
}

bool Session::is_in_draining_period() const {
  return ngtcp2_conn_is_in_draining_period(connection());
}

bool Session::is_unable_to_send_packets() {
  return NgCallbackScope::InNgCallbackScope(this) ||
      is_destroyed() ||
      is_in_draining_period() ||
      (is_server() && is_in_closing_period()) ||
      !endpoint_;
}

uint64_t Session::max_data_left() const {
  return ngtcp2_conn_get_max_data_left(connection());
}

uint64_t Session::max_local_streams_uni() const {
  return ngtcp2_conn_get_max_local_streams_uni(connection());
}

void Session::set_remote_transport_params() {
  DCHECK(!is_destroyed());
  ngtcp2_conn_get_remote_transport_params(connection(), &transport_params_);
  transport_params_set_ = true;
}

int Session::set_session(SSL_SESSION* session) {
  CHECK(!is_server());
  CHECK(!is_destroyed());

  size_t transport_params_size = sizeof(ngtcp2_transport_params);
  size_t size = i2d_SSL_SESSION(session, nullptr);
  if (size > crypto::SecureContext::kMaxSessionSize)
    return 0;

  BindingState* state = BindingState::Get(env());
  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  CallbackScope cb_scope(this);

  std::shared_ptr<BackingStore> ticket;
  std::shared_ptr<BackingStore> transport_params;

  if (size > 0) {
    ticket = ArrayBuffer::NewBackingStore(env()->isolate(), size);
    unsigned char* data = reinterpret_cast<unsigned char*>(ticket->Data());
    if (i2d_SSL_SESSION(session, &data) <= 0)
      return 0;
  } else {
    ticket = ArrayBuffer::NewBackingStore(env()->isolate(), 0);
  }

  if (transport_params_set_) {
    transport_params =
        ArrayBuffer::NewBackingStore(
            env()->isolate(),
            transport_params_size);
    memcpy(
      transport_params->Data(),
      &transport_params_,
      transport_params_size);
  } else {
    transport_params = ArrayBuffer::NewBackingStore(env()->isolate(), 0);
  }

  Local<Value> argv[] = {
    ArrayBuffer::New(env()->isolate(), ticket),
    ArrayBuffer::New(env()->isolate(), transport_params)
  };

  BaseObjectPtr<Session> ptr(this);

  USE(state->session_ticket_callback()->Call(
      env()->context(),
      object(),
      arraysize(argv),
      argv));

  return 0;
}

BaseObjectPtr<LogStream> Session::qlogstream() {
  return qlogstream_;
}

BaseObjectPtr<LogStream> Session::keylogstream() {
  return keylogstream_;
}

// Gets the QUIC version negotiated for this Session
quic_version Session::version() const {
  CHECK(!is_destroyed());
  return ngtcp2_conn_get_negotiated_version(connection());
}

const ngtcp2_callbacks Session::callbacks[2] = {
  // NGTCP2_CRYPTO_SIDE_CLIENT
  {
    ngtcp2_crypto_client_initial_cb,
    nullptr,
    OnReceiveCryptoData,
    OnHandshakeCompleted,
    OnVersionNegotiation,
    ngtcp2_crypto_encrypt_cb,
    ngtcp2_crypto_decrypt_cb,
    ngtcp2_crypto_hp_mask_cb,
    OnReceiveStreamData,
    OnAckedCryptoOffset,
    OnAckedStreamDataOffset,
    OnStreamOpen,
    OnStreamClose,
    OnStatelessReset,
    ngtcp2_crypto_recv_retry_cb,
    OnExtendMaxStreamsBidi,
    OnExtendMaxStreamsUni,
    OnRand,
    OnGetNewConnectionID,
    OnRemoveConnectionID,
    ngtcp2_crypto_update_key_cb,
    OnPathValidation,
    OnSelectPreferredAddress,
    OnStreamReset,
    OnExtendMaxStreamsRemoteBidi,
    OnExtendMaxStreamsRemoteUni,
    OnExtendMaxStreamData,
    OnConnectionIDStatus,
    OnHandshakeConfirmed,
    nullptr,  // recv_new_token
    ngtcp2_crypto_delete_crypto_aead_ctx_cb,
    ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
    OnDatagram
  },
  // NGTCP2_CRYPTO_SIDE_SERVER
  {
    nullptr,
    ngtcp2_crypto_recv_client_initial_cb,
    OnReceiveCryptoData,
    OnHandshakeCompleted,
    nullptr,  // recv_version_negotiation
    ngtcp2_crypto_encrypt_cb,
    ngtcp2_crypto_decrypt_cb,
    ngtcp2_crypto_hp_mask_cb,
    OnReceiveStreamData,
    OnAckedCryptoOffset,
    OnAckedStreamDataOffset,
    OnStreamOpen,
    OnStreamClose,
    OnStatelessReset,
    nullptr,  // recv_retry
    nullptr,  // extend_max_streams_bidi
    nullptr,  // extend_max_streams_uni
    OnRand,
    OnGetNewConnectionID,
    OnRemoveConnectionID,
    ngtcp2_crypto_update_key_cb,
    OnPathValidation,
    nullptr,  // select_preferred_addr
    OnStreamReset,
    OnExtendMaxStreamsRemoteBidi,
    OnExtendMaxStreamsRemoteUni,
    OnExtendMaxStreamData,
    OnConnectionIDStatus,
    nullptr,  // handshake_confirmed
    nullptr,  // recv_new_token
    ngtcp2_crypto_delete_crypto_aead_ctx_cb,
    ngtcp2_crypto_delete_crypto_cipher_ctx_cb,
    OnDatagram
  }
};

int Session::OnReceiveCryptoData(
    ngtcp2_conn* conn,
    ngtcp2_crypto_level crypto_level,
    uint64_t offset,
    const uint8_t* data,
    size_t datalen,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  return session->crypto_context()->Receive(
      crypto_level,
      offset,
      data,
      datalen) == 0 ? 0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

int Session::OnExtendMaxStreamsBidi(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->ExtendMaxStreamsBidi(max_streams);
  return 0;
}

int Session::OnExtendMaxStreamsUni(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->ExtendMaxStreamsUni(max_streams);
  return 0;
}

int Session::OnExtendMaxStreamsRemoteUni(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->ExtendMaxStreamsRemoteUni(max_streams);
  return 0;
}

int Session::OnExtendMaxStreamsRemoteBidi(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->ExtendMaxStreamsRemoteUni(max_streams);
  return 0;
}

int Session::OnExtendMaxStreamData(
    ngtcp2_conn* conn,
    stream_id id,
    uint64_t max_data,
    void* user_data,
    void* stream_user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->ExtendMaxStreamData(id, max_data);
  return 0;
}

int Session::OnConnectionIDStatus(
    ngtcp2_conn* conn,
    int type,
    uint64_t seq,
    const ngtcp2_cid* cid,
    const uint8_t* token,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  if (token != nullptr) {
    NgCallbackScope scope(session);
    CID qcid(cid);
    Debug(session, "Updating connection ID %s with reset token", qcid);
    session->UpdateConnectionID(type, qcid, StatelessResetToken(token));
  }
  return 0;
}

int Session::OnHandshakeCompleted(ngtcp2_conn* conn, void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);

  const ngtcp2_path* path = ngtcp2_conn_get_path(conn);
  return session->HandshakeCompleted(
      std::make_shared<SocketAddress>(path->remote.addr))
          ? 0
          : NGTCP2_ERR_CALLBACK_FAILURE;
}

int Session::OnHandshakeConfirmed(ngtcp2_conn* conn, void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->HandshakeConfirmed();
  return 0;
}

int Session::OnReceiveStreamData(
    ngtcp2_conn* conn,
    uint32_t flags,
    stream_id id,
    uint64_t offset,
    const uint8_t* data,
    size_t datalen,
    void* user_data,
    void* stream_user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  return session->ReceiveStreamData(
      flags,
      id,
      data,
      datalen,
      offset) ? 0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

int Session::OnStreamOpen(ngtcp2_conn* conn, stream_id id, void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  // We currently do not do anything with this callback.
  // Stream instances are created implicitly only once the
  // first chunk of stream data is received.

  return 0;
}

int Session::OnAckedCryptoOffset(
    ngtcp2_conn* conn,
    ngtcp2_crypto_level crypto_level,
    uint64_t offset,
    uint64_t datalen,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->crypto_context()->AcknowledgeCryptoData(crypto_level, datalen);
  return 0;
}

int Session::OnAckedStreamDataOffset(
    ngtcp2_conn* conn,
    stream_id id,
    uint64_t offset,
    uint64_t datalen,
    void* user_data,
    void* stream_user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->AckedStreamDataOffset(id, offset, datalen);
  return 0;
}

int Session::OnSelectPreferredAddress(
    ngtcp2_conn* conn,
    ngtcp2_addr* dest,
    const ngtcp2_preferred_addr* paddr,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);

  // The paddr parameter contains the server advertised preferred
  // address. The dest parameter contains the address that is
  // actually being used. If the preferred address is selected,
  // then the contents of paddr are copied over to dest.
  session->SelectPreferredAddress(
      PreferredAddress(session->env(), dest, paddr));
  return 0;
}

int Session::OnStreamClose(
    ngtcp2_conn* conn,
    stream_id id,
    uint64_t app_error_code,
    void* user_data,
    void* stream_user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->StreamClose(id, app_error_code);
  return 0;
}

int Session::OnStreamReset(
    ngtcp2_conn* conn,
    stream_id id,
    uint64_t final_size,
    uint64_t app_error_code,
    void* user_data,
    void* stream_user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->StreamReset(id, final_size, app_error_code);
  return 0;
}

int Session::OnRand(
    uint8_t* dest,
    size_t destlen,
    const ngtcp2_rand_ctx* rand_ctx,
    ngtcp2_rand_usage usage) {
  // For now, we ignore both rand_ctx and usage. The rand_ctx allows
  // a custom entropy source to be passed in to the ngtcp2 configuration.
  // We don't make use of that mechanism. The usage differentiates what
  // the random data is for, in case an implementation wishes to apply
  // a different mechanism based on purpose. We don't, at least for now.
  crypto::EntropySource(dest, destlen);
  return 0;
}

int Session::OnGetNewConnectionID(
    ngtcp2_conn* conn,
    ngtcp2_cid* cid,
    uint8_t* token,
    size_t cidlen,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope scope(session);
  session->GetNewConnectionID(cid, token, cidlen);
  return 0;
}

int Session::OnRemoveConnectionID(
    ngtcp2_conn* conn,
    const ngtcp2_cid* cid,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  if (session->is_server()) {
    NgCallbackScope callback_scope(session);
    session->endpoint()->DisassociateCID(CID(cid));
  }
  return 0;
}

int Session::OnPathValidation(
    ngtcp2_conn* conn,
    const ngtcp2_path* path,
    ngtcp2_path_validation_result res,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->PathValidation(path, res);
  return 0;
}

int Session::OnVersionNegotiation(
    ngtcp2_conn* conn,
    const ngtcp2_pkt_hd* hd,
    const uint32_t* sv,
    size_t nsv,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  NgCallbackScope callback_scope(session);
  session->VersionNegotiation(sv, nsv);
  return 0;
}

int Session::OnStatelessReset(
    ngtcp2_conn* conn,
    const ngtcp2_pkt_stateless_reset* sr,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  session->stateless_reset_ = true;
  return 0;
}

int Session::OnDatagram(
    ngtcp2_conn* conn,
    uint32_t flags,
    const uint8_t* data,
    size_t datalen,
    void* user_data) {
  Session* session = static_cast<Session*>(user_data);

  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  session->Datagram(flags, data, datalen);
  return 0;
}

void Session::DoDestroy(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->Destroy();
}

void Session::GetRemoteAddress(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  BaseObjectPtr<SocketAddressBase> addr;
  std::shared_ptr<SocketAddress> address = session->remote_address();
  if (address)
    addr = SocketAddressBase::Create(env, address);
  if (addr)
    args.GetReturnValue().Set(addr->object());
}

void Session::GetCertificate(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Local<Value> ret;
  if (session->crypto_context()->cert(env).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void Session::GetPeerCertificate(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Local<Value> ret;
  if (session->crypto_context()->peer_cert(env).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void Session::SilentClose(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  ProcessEmitWarning(
      session->env(),
      "Forcing silent close of Session for testing purposes only");
  session->Close(Session::SessionCloseFlags::SILENT);
}

void Session::GracefulClose(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->StartGracefulClose();
}

void Session::UpdateKey(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  // Initiating a key update may fail if it is done too early (either
  // before the TLS handshake has been confirmed or while a previous
  // key update is being processed). When it fails, InitiateKeyUpdate()
  // will return false.
  args.GetReturnValue().Set(session->crypto_context()->InitiateKeyUpdate());
}

void Session::DoDetachFromEndpoint(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->DetachFromEndpoint();
}

void Session::OnClientHelloDone(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  crypto::SecureContext* context = nullptr;
  if (crypto::SecureContext::HasInstance(env, args[0])) {
    ASSIGN_OR_RETURN_UNWRAP(&context, args[0]);
  }
  session->crypto_context()->OnClientHelloDone(
      BaseObjectPtr<crypto::SecureContext>(context));
}

void Session::OnOCSPDone(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());

  std::shared_ptr<BackingStore> ret;

  if (!args[0]->IsUndefined()) {
    crypto::ArrayBufferOrViewContents<uint8_t> view(args[0]);
    // TODO(@jasnell): Not accounting for ArrayBuffer offset or length
    ret = view.store();
  }

  session->crypto_context()->OnOCSPDone(ret);
}

void Session::GetEphemeralKeyInfo(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Local<Object> ret;
  if (!session->is_server() &&
      session->crypto_context()->ephemeral_key(env).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void Session::DoAttachToEndpoint(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  CHECK(EndpointWrap::HasInstance(env, args[0]));
  EndpointWrap* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args[0]);
  args.GetReturnValue().Set(
      session->AttachToNewEndpoint(endpoint, args[1]->IsTrue()));
}

void Session::DoOpenStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  CHECK(args[0]->IsUint32());

  Stream::Direction direction =
      static_cast<Stream::Direction>(args[0].As<Uint32>()->Value());

  stream_id id;
  if (!session->OpenStream(direction).To(&id))
    return;  // Returning nothing indicates failure

  BaseObjectPtr<Stream> stream = Stream::Create(env, session, id);
  if (stream) {
    session->AddStream(stream);
    args.GetReturnValue().Set(stream->object());
  }
}

void Session::DoSendDatagram(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  crypto::ArrayBufferOrViewContents<uint8_t> datagram(args[0]);
  args.GetReturnValue().Set(
      session->SendDatagram(
          datagram.store(),
          datagram.offset(),
          datagram.size()));
}

template <>
void StatsTraitsImpl<SessionStats, Session>::ToString(
    const Session& ptr,
    AddStatsField add_field) {
#define V(n, name, label) add_field(label, ptr.GetStat(&SessionStats::name));
    SESSION_STATS(V)
#undef V
  }

Session::Application::Options::Options(const Application::Options& other)
    noexcept
        : max_header_pairs(other.max_header_pairs),
          max_header_length(other.max_header_length) {}

// Determines which Application variant the Session will be using
// based on the alpn configured for the application. For now, this is
// determined through configuration when tghe Session is created
// and is not negotiable. In the future, we may allow it to be negotiated.
Session::Application* Session::SelectApplication(
    const std::string& alpn,
    const std::shared_ptr<Application::Options>& options) {

  if (alpn == NGHTTP3_ALPN_H3) {
    Debug(this, "Selecting HTTP/3 Application");
    return new Http3Application(this, options);
  }

  // In the future, we may end up supporting additional
  // QUIC protocols. As they are added, extend the cases
  // here to create and return them.

  Debug(this, "Using default application for %s", alpn);

  return new DefaultApplication(this, options);
}

Session::Application::Application(
    Session* session,
    const std::shared_ptr<Application::Options>& options)
    : session_(session),
      options_(std::move(options)) {}

void Session::Application::Acknowledge(
    stream_id id,
    uint64_t offset,
    size_t datalen) {
  BaseObjectPtr<Stream> stream = session()->FindStream(id);
  if (LIKELY(stream))
    stream->Acknowledge(offset, datalen);
}

std::unique_ptr<Packet> Session::Application::CreateStreamDataPacket() {
  return std::make_unique<Packet>(
      session()->max_packet_length(),
      "stream data");
}

bool Session::Application::Initialize() {
  if (needs_init_) needs_init_ = false;
  return !needs_init_;
}

bool Session::Application::SendPendingData() {
  // The maximum number of packets to send per call
  static constexpr size_t kMaxPackets = 16;
  PathStorage path;
  std::unique_ptr<Packet> packet;
  uint8_t* pos = nullptr;
  size_t packets_sent = 0;
  int err;

  Debug(session(), "Start sending pending data");
  for (;;) {
    ssize_t ndatalen;
    StreamData stream_data;
    err = GetStreamData(&stream_data);
    if (err < 0) {
      session()->set_last_error(kQuicInternalError);
      return false;
    }

    // If stream_data.id is -1, then we're not serializing any data for any
    // specific stream. We still need to process QUIC session packets tho.
    if (stream_data.id > -1)
      Debug(session(), "Serializing packets for stream id %" PRId64,
            stream_data.id);
    else
      Debug(session(), "Serializing session packets");

    // If the packet was sent previously, then packet will have been reset.
    if (!packet) {
      packet = CreateStreamDataPacket();
      pos = packet->data();
    }

    ssize_t nwrite = WriteVStream(&path, pos, &ndatalen, stream_data);
    if (stream_data.id >= 0) {
      Debug(session(),
            "Stream %llu data:\n"
            "\tnwrite = %lld\n"
            "\tndatalen = %lld\n"
            "\tremaining = %lld\n"
            "\tfinal = %d\n"
            "\tside = %s\n",
            stream_data.id,
            nwrite,
            ndatalen,
            stream_data.remaining,
            stream_data.fin,
            stream_data.stream && stream_data.stream->session()->is_server() ?
                "server" : "client");
    }
    if (nwrite <= 0) {
      switch (nwrite) {
        case 0:
          if (stream_data.id >= 0)
            ResumeStream(stream_data.id);
          goto congestion_limited;
        case NGTCP2_ERR_PKT_NUM_EXHAUSTED:
          // There is a finite number of packets that can be sent
          // per connection. Once those are exhausted, there's
          // absolutely nothing we can do except immediately
          // and silently tear down the Session. This has
          // to be silent because we can't even send a
          // CONNECTION_CLOSE since even those require a
          // packet number.
          session()->Close(Session::SessionCloseFlags::SILENT);
          return false;
        case NGTCP2_ERR_STREAM_DATA_BLOCKED:
          Debug(session(), "Stream %lld blocked", stream_data.id);
          session()->StreamDataBlocked(stream_data.id);
          if (session()->max_data_left() == 0) {
            if (stream_data.id >= 0) {
              Debug(session(), "Resuming %llu after block", stream_data.id);
              ResumeStream(stream_data.id);
            }
            goto congestion_limited;
          }
          CHECK_LE(ndatalen, 0);
          continue;
          // Fall through
        case NGTCP2_ERR_STREAM_SHUT_WR:
          Debug(session(), "Stream %lld shut", stream_data.id);
          // TODO(@jasnell): Need to handle correctly.
          CHECK_LE(ndatalen, 0);
          continue;
        case NGTCP2_ERR_STREAM_NOT_FOUND:
          continue;
        case NGTCP2_ERR_WRITE_MORE:
          CHECK_GT(ndatalen, 0);
          CHECK(StreamCommit(&stream_data, ndatalen));
          pos += ndatalen;
          continue;
      }
      session()->set_last_error(kQuicInternalError);
      return false;
    }

    pos += nwrite;
    if (ndatalen > 0)
      CHECK(StreamCommit(&stream_data, ndatalen));

    if (stream_data.id >= 0 && ndatalen < 0)
      ResumeStream(stream_data.id);

    Debug(session(), "Sending %" PRIu64 " bytes in serialized packet", nwrite);
    packet->set_length(nwrite);
    if (!session()->SendPacket(std::move(packet), path)) {
      Debug(session(), "-- Failed to send packet");
      return false;
    }
    packet.reset();
    pos = nullptr;
    if (++packets_sent == kMaxPackets) {
      Debug(session(), "-- Max packets sent");
      break;
    }
    Debug(session(), "-- Looping");
  }
  return true;

congestion_limited:
  // We are either congestion limited or done.
  if (pos - packet->data()) {
    // Some data was serialized into the packet. We need to send it.
    packet->set_length(pos - packet->data());
    Debug(session(), "Congestion limited, but %" PRIu64 " bytes pending",
          packet->length());
    if (!session()->SendPacket(std::move(packet), path))
      return false;
  }
  return true;
}

void Session::Application::StreamClose(
    stream_id id,
    error_code app_error_code) {
  BaseObjectPtr<Stream> stream = session()->FindStream(id);
  if (stream)
    stream->OnClose(app_error_code);
}

void Session::Application::StreamReset(
    stream_id id,
    error_code app_error_code) {
  BaseObjectPtr<Stream> stream = session_->FindStream(id);
  if (stream) stream->OnReset(app_error_code);
}

ssize_t Session::Application::WriteVStream(
    PathStorage* path,
    uint8_t* buf,
    ssize_t* ndatalen,
    const StreamData& stream_data) {
  CHECK_LE(stream_data.count, kMaxVectorCount);
  uint32_t flags = NGTCP2_WRITE_STREAM_FLAG_NONE;
  if (stream_data.remaining > 0)
    flags |= NGTCP2_WRITE_STREAM_FLAG_MORE;
  if (stream_data.fin)
    flags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
  ssize_t ret = ngtcp2_conn_writev_stream(
    session()->connection(),
    &path->path,
    nullptr,
    buf,
    session()->max_packet_length(),
    ndatalen,
    flags,
    stream_data.id,
    stream_data.buf,
    stream_data.count,
    uv_hrtime());
  return ret;
}

std::string Session::RemoteTransportParamsDebug::ToString() const {
  ngtcp2_transport_params params;
  ngtcp2_conn_get_remote_transport_params(session->connection(), &params);
  std::string out = "Remote Transport Params:\n";
  out += "  Ack Delay Exponent: " +
         std::to_string(params.ack_delay_exponent) + "\n";
  out += "  Active Connection ID Limit: " +
         std::to_string(params.active_connection_id_limit) + "\n";
  out += "  Disable Active Migration: " +
         std::string(params.disable_active_migration ? "Yes" : "No") + "\n";
  out += "  Initial Max Data: " +
         std::to_string(params.initial_max_data) + "\n";
  out += "  Initial Max Stream Data Bidi Local: " +
         std::to_string(params.initial_max_stream_data_bidi_local) + "\n";
  out += "  Initial Max Stream Data Bidi Remote: " +
         std::to_string(params.initial_max_stream_data_bidi_remote) + "\n";
  out += "  Initial Max Stream Data Uni: " +
         std::to_string(params.initial_max_stream_data_uni) + "\n";
  out += "  Initial Max Streams Bidi: " +
         std::to_string(params.initial_max_streams_bidi) + "\n";
  out += "  Initial Max Streams Uni: " +
         std::to_string(params.initial_max_streams_uni) + "\n";
  out += "  Max Ack Delay: " +
         std::to_string(params.max_ack_delay) + "\n";
  out += "  Max Idle Timeout: " +
         std::to_string(params.max_idle_timeout) + "\n";
  out += "  Max Packet Size: " +
         std::to_string(params.max_udp_payload_size) + "\n";

  if (!session->is_server()) {
    if (params.retry_scid_present) {
      CID cid(params.original_dcid);
      CID retry(params.retry_scid);
      out += "  Original Connection ID: " + cid.ToString() + "\n";
      out += "  Retry SCID: " + retry.ToString() + "\n";
    } else {
      out += "  Original Connection ID: N/A \n";
    }

    if (params.preferred_address_present)
      out += "  Preferred Address Present: Yes\n";
    else
      out += "  Preferred Address Present: No\n";

    if (params.stateless_reset_token_present) {
      StatelessResetToken token(params.stateless_reset_token);
      out += "  Stateless Reset Token: " + token.ToString() + "\n";
    } else {
      out += " Stateless Reset Token: N/A";
    }
  }
  return out;
}

DefaultApplication::DefaultApplication(
    Session* session,
    const std::shared_ptr<Application::Options>& options)
    : Session::Application(session, options) {
  Debug(session, "Using default application");
}

void DefaultApplication::ScheduleStream(stream_id id) {
  BaseObjectPtr<Stream> stream = session()->FindStream(id);
  if (LIKELY(stream && !stream->is_destroyed())) {
    Debug(session(), "Scheduling stream %" PRIu64, id);
    stream->Schedule(&stream_queue_);
  }
}

void DefaultApplication::UnscheduleStream(stream_id id) {
  BaseObjectPtr<Stream> stream = session()->FindStream(id);
  if (LIKELY(stream)) {
    Debug(session(), "Unscheduling stream %" PRIu64, id);
    stream->Unschedule();
  }
}

void DefaultApplication::ResumeStream(stream_id id) {
  ScheduleStream(id);
}

bool DefaultApplication::ReceiveStreamData(
    uint32_t flags,
    stream_id id,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) {
  // One potential DOS attack vector is to send a bunch of
  // empty stream frames to commit resources. Check that
  // here. Essentially, we only want to create a new stream
  // if the datalen is greater than 0, otherwise, we ignore
  // the packet. ngtcp2 should be handling this for us,
  // but we handle it just to be safe.
  if (UNLIKELY(datalen == 0))
    return true;

  // Ensure that the Stream exists.
  Debug(session(), "Receiving stream data for %" PRIu64, id);
  BaseObjectPtr<Stream> stream = session()->FindStream(id);
  if (!stream) {
    // Because we are closing gracefully, we are not allowing
    // new streams to be created. Shut it down immediately
    // and commit no further resources.
    if (session()->is_graceful_closing()) {
      session()->ShutdownStream(id, NGTCP2_ERR_CLOSING);
      return true;
    }

    stream = session()->CreateStream(id);
  }
  CHECK(stream);

  // If the stream ended up being destroyed immediately after
  // creation, just skip the data processing and return.
  if (UNLIKELY(stream->is_destroyed()))
    return true;

  stream->ReceiveData(flags, data, datalen, offset);
  return true;
}

int DefaultApplication::GetStreamData(StreamData* stream_data) {
  if (stream_queue_.IsEmpty())
    return 0;

  Stream* stream = stream_queue_.PopFront();
  CHECK_NOT_NULL(stream);
  stream_data->stream.reset(stream);
  stream_data->id = stream->id();
  auto next = [&](
      int status,
      const ngtcp2_vec* data,
      size_t count,
      bob::Done done) {
    switch (status) {
      case bob::Status::STATUS_BLOCK:
        // Fall through
      case bob::Status::STATUS_WAIT:
        return;
      case bob::Status::STATUS_EOS:
      case bob::Status::STATUS_END:
        stream_data->fin = 1;
    }

    stream_data->count = count;
    if (count > 0) {
      stream->Schedule(&stream_queue_);
      stream_data->remaining = get_length(data, count);
    } else {
      stream_data->remaining = 0;
    }

    // Not calling done here because we defer committing
    // the data until after we're sure it's written.
  };

  if (LIKELY(!stream->is_eos())) {
    int ret = stream->Pull(
        std::move(next),
        bob::Options::OPTIONS_SYNC,
        stream_data->data,
        arraysize(stream_data->data),
        kMaxVectorCount);
    switch (ret) {
      case bob::Status::STATUS_EOS:
      case bob::Status::STATUS_END:
        stream_data->fin = 1;
        break;
    }
  } else {
    stream_data->fin = 1;
  }

  return 0;
}

bool DefaultApplication::StreamCommit(StreamData* stream_data, size_t datalen) {
  CHECK(stream_data->stream);
  stream_data->remaining -= datalen;
  Consume(&stream_data->buf, &stream_data->count, datalen);
  stream_data->stream->Commit(datalen);
  return true;
}

bool DefaultApplication::ShouldSetFin(const StreamData& stream_data) {
  if (!stream_data.stream || !IsEmpty(stream_data.buf, stream_data.count))
    return false;
  return true;
}

bool OptionsObject::HasInstance(Environment* env, const Local<Value>& value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> OptionsObject::GetConstructorTemplate(
    Environment* env) {
  BindingState* state = env->GetBindingData<BindingState>(env->context());
  Local<FunctionTemplate> tmpl =
      state->session_options_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        OptionsObject::kInternalFieldCount);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "OptionsObject"));
    state->set_session_options_constructor_template(tmpl);
  }
  return tmpl;
}

void OptionsObject::Initialize(Environment* env, Local<Object> target) {
  env->SetConstructorFunction(
      target,
      "OptionsObject",
      GetConstructorTemplate(env),
      Environment::SetConstructorFunctionFlag::NONE);
}

void OptionsObject::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  BindingState* state = BindingState::Get(env);

  CHECK(args[0]->IsString());  // ALPN
  CHECK_IMPLIES(
      !args[1]->IsUndefined(),
      args[1]->IsString());  // Hostname
  CHECK_IMPLIES(  // DCID
      !args[2]->IsUndefined(),
      args[2]->IsArrayBuffer() || args[2]->IsArrayBufferView());
  CHECK_IMPLIES(  // SCID
      !args[3]->IsUndefined(),
      args[3]->IsArrayBuffer() || args[3]->IsArrayBufferView());
  CHECK_IMPLIES(  // Preferred address strategy
      !args[4]->IsUndefined(),
      args[4]->IsInt32());
  CHECK_IMPLIES(  // Connection ID Strategy
      !args[5]->IsUndefined(),
      args[5]->IsObject());
  CHECK_IMPLIES(
      !args[6]->IsUndefined(),
      args[6]->IsBoolean());
  CHECK_IMPLIES(  // TLS options
      !args[7]->IsUndefined(),
      args[7]->IsObject());
  CHECK_IMPLIES(  // Application Options
      !args[8]->IsUndefined(),
      args[8]->IsObject());
  CHECK_IMPLIES(  // TransportParams
      !args[9]->IsUndefined(),
      args[9]->IsObject());

  CHECK_IMPLIES(  // IPv4 Preferred Address
      !args[10]->IsUndefined(),
      SocketAddressBase::HasInstance(env, args[10]));
  CHECK_IMPLIES(  // IPv6 Preferred Address
      !args[11]->IsUndefined(),
      SocketAddressBase::HasInstance(env, args[11]));

  Utf8Value alpn(env->isolate(), args[0]);

  OptionsObject* options = new OptionsObject(env, args.This());
  options->options()->alpn = std::string(1, alpn.length()) + (*alpn);

  if (!args[1]->IsUndefined()) {
    Utf8Value hostname(env->isolate(), args[1]);
    options->options()->hostname = *hostname;
  }

  if (!args[2]->IsUndefined()) {
    crypto::ArrayBufferOrViewContents<uint8_t> cid(args[2]);
    if (cid.size() > 0) {
      memcpy(
          options->options()->dcid.data(),
          cid.data(),
          cid.size());
      options->options()->dcid.set_length(cid.size());
    }
  }

  if (!args[3]->IsUndefined()) {
    crypto::ArrayBufferOrViewContents<uint8_t> cid(args[3]);
    if (cid.size() > 0) {
      memcpy(
          options->options()->scid.data(),
          cid.data(),
          cid.size());
      options->options()->scid.set_length(cid.size());
    }
  }

  if (!args[4]->IsUndefined()) {
    PreferredAddress::Policy policy =
        static_cast<PreferredAddress::Policy>(args[4].As<Int32>()->Value());
    switch (policy) {
      case PreferredAddress::Policy::USE:
        options->options()->preferred_address_strategy =
            Session::UsePreferredAddressStrategy;
        break;
      case PreferredAddress::Policy::IGNORE_PREFERED:
        options->options()->preferred_address_strategy =
            Session::IgnorePreferredAddressStrategy;
        break;
      default:
        UNREACHABLE();
    }
  }

  // Add support for the other strategies once implemented
  if (RandomConnectionIDBase::HasInstance(env, args[5])) {
    RandomConnectionIDBase* cid_strategy;
    ASSIGN_OR_RETURN_UNWRAP(&cid_strategy, args[5]);
    options->options()->cid_strategy = cid_strategy->strategy();
    options->options()->cid_strategy_strong_ref.reset(cid_strategy);
  } else {
    UNREACHABLE();
  }

  options->options()->qlog = args[6]->IsTrue();

  if (!args[7]->IsUndefined()) {
    Local<Object> secure = args[7].As<Object>();

    if (UNLIKELY(options->SetOption(
            secure,
            state->reject_unauthorized_string(),
            &Session::Options::reject_unauthorized).IsNothing()) ||
        UNLIKELY(options->SetOption(
            secure,
            state->client_hello_string(),
            &Session::Options::client_hello).IsNothing()) ||
        UNLIKELY(options->SetOption(
            secure,
            state->enable_tls_trace_string(),
            &Session::Options::enable_tls_trace).IsNothing()) ||
        UNLIKELY(options->SetOption(
            secure,
            state->request_peer_certificate_string(),
            &Session::Options::request_peer_certificate).IsNothing()) ||
        UNLIKELY(options->SetOption(
            secure,
            state->ocsp_string(),
            &Session::Options::ocsp).IsNothing()) ||
        UNLIKELY(options->SetOption(
            secure,
            state->verify_hostname_identity_string(),
            &Session::Options::verify_hostname_identity).IsNothing()) ||
        UNLIKELY(options->SetOption(
            secure,
            state->keylog_string(),
            &Session::Options::keylog).IsNothing())) {
      return;
    }

    Local<Value> val;
    options->options()->psk_callback_present =
        secure->Get(env->context(), state->pskcallback_string()).ToLocal(&val)
            && val->IsFunction();
  }

  if (Http3OptionsObject::HasInstance(env, args[8])) {
    Http3OptionsObject* http3Options;
    ASSIGN_OR_RETURN_UNWRAP(&http3Options, args[8]);
    options->options()->application = http3Options->options();
  }

  if (!args[9]->IsUndefined()) {
    Local<Object> params = args[9].As<Object>();

    if (UNLIKELY(options->SetOption(
            params,
            state->initial_max_stream_data_bidi_local_string(),
            &Session::Options::initial_max_stream_data_bidi_local)
                .IsNothing()) ||
        UNLIKELY(options->SetOption(
            params,
            state->initial_max_stream_data_bidi_remote_string(),
            &Session::Options::initial_max_stream_data_bidi_remote)
                .IsNothing()) ||
        UNLIKELY(options->SetOption(
            params,
            state->initial_max_stream_data_uni_string(),
            &Session::Options::initial_max_stream_data_uni).IsNothing()) ||
        UNLIKELY(options->SetOption(
            params,
            state->initial_max_data_string(),
            &Session::Options::initial_max_data).IsNothing()) ||
        UNLIKELY(options->SetOption(
            params,
            state->initial_max_streams_bidi_string(),
            &Session::Options::initial_max_streams_bidi).IsNothing()) ||
        UNLIKELY(options->SetOption(
            params,
            state->initial_max_streams_uni_string(),
            &Session::Options::initial_max_streams_uni).IsNothing()) ||
        UNLIKELY(options->SetOption(
            params,
            state->max_idle_timeout_string(),
            &Session::Options::max_idle_timeout).IsNothing()) ||
        UNLIKELY(options->SetOption(
            params,
            state->active_connection_id_limit_string(),
            &Session::Options::active_connection_id_limit).IsNothing()) ||
        UNLIKELY(options->SetOption(
            params,
            state->ack_delay_exponent_string(),
            &Session::Options::ack_delay_exponent).IsNothing()) ||
        UNLIKELY(options->SetOption(
            params,
            state->max_ack_delay_string(),
            &Session::Options::max_ack_delay).IsNothing()) ||
        UNLIKELY(options->SetOption(
            params,
            state->max_datagram_frame_size_string(),
            &Session::Options::max_datagram_frame_size).IsNothing()) ||
        UNLIKELY(options->SetOption(
            params,
            state->disable_active_migration_string(),
            &Session::Options::disable_active_migration).IsNothing())) {
      // The if block intentionally does nothing. The code is structured
      // like this to shortcircuit if any of the SetOptions() returns Nothing.
    }
  }

  if (!args[10]->IsUndefined()) {
    SocketAddressBase* preferred_addr;
    ASSIGN_OR_RETURN_UNWRAP(&preferred_addr, args[10]);
    CHECK_EQ(preferred_addr->address()->family(), AF_INET);
    options->options()->preferred_address_ipv4 = preferred_addr->address();
  }

  if (!args[11]->IsUndefined()) {
    SocketAddressBase* preferred_addr;
    ASSIGN_OR_RETURN_UNWRAP(&preferred_addr, args[11]);
    CHECK_EQ(preferred_addr->address()->family(), AF_INET6);
    options->options()->preferred_address_ipv6 = preferred_addr->address();
  }
}

Maybe<bool> OptionsObject::SetOption(
    const Local<Object>& object,
    const Local<String>& name,
    uint64_t Session::Options::*member) {
  Local<Value> value;
  if (!object->Get(env()->context(), name).ToLocal(&value))
    return Nothing<bool>();

  if (value->IsUndefined())
    return Just(false);

  CHECK_IMPLIES(!value->IsBigInt(), value->IsNumber());

  uint64_t val = 0;
  if (value->IsBigInt()) {
    bool lossless = true;
    val = value.As<BigInt>()->Uint64Value(&lossless);
    if (!lossless) {
      Utf8Value label(env()->isolate(), name);
      THROW_ERR_OUT_OF_RANGE(
          env(),
          (std::string("options.") + (*label) + " is out of range").c_str());
      return Nothing<bool>();
    }
  } else {
    val = static_cast<int64_t>(value.As<Number>()->Value());
  }
  options_.get()->*member = val;
  return Just(true);
}

Maybe<bool> OptionsObject::SetOption(
    const Local<Object>& object,
    const Local<String>& name,
    uint32_t Session::Options::*member) {
  Local<Value> value;
  if (!object->Get(env()->context(), name).ToLocal(&value))
    return Nothing<bool>();

  if (value->IsUndefined())
    return Just(false);

  CHECK(value->IsUint32());
  uint32_t val = value.As<Uint32>()->Value();
  options_.get()->*member = val;
  return Just(true);
}

Maybe<bool> OptionsObject::SetOption(
    const Local<Object>& object,
    const Local<String>& name,
    bool Session::Options::*member) {
  Local<Value> value;
  if (!object->Get(env()->context(), name).ToLocal(&value))
    return Nothing<bool>();
  if (value->IsUndefined())
    return Just(false);
  CHECK(value->IsBoolean());
  options_.get()->*member = value->IsTrue();
  return Just(true);
}

OptionsObject::OptionsObject(
    Environment* env,
    Local<Object> object,
    std::shared_ptr<Session::Options> options)
    : BaseObject(env, object),
      options_(std::move(options)) {
  MakeWeak();
}

void Session::Options::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("alpn", alpn.length());
  tracker->TrackFieldWithSize("hostname", hostname.length());
}

void OptionsObject::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("options", options_);
}

}  // namespace quic
}  // namespace node
