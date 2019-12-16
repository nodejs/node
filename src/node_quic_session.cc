#include "aliased_buffer.h"
#include "debug_utils.h"
#include "env-inl.h"
#include "node_crypto_common.h"
#include "ngtcp2/ngtcp2.h"
#include "ngtcp2/ngtcp2_crypto.h"
#include "ngtcp2/ngtcp2_crypto_openssl.h"
#include "node.h"
#include "node_buffer.h"
#include "node_crypto.h"
#include "node_internals.h"
#include "node_mem-inl.h"
#include "node_quic_buffer.h"
#include "node_quic_crypto.h"
#include "node_quic_session.h"  // NOLINT(build/include_inline)
#include "node_quic_session-inl.h"
#include "node_quic_socket.h"
#include "node_quic_stream.h"
#include "node_quic_state.h"
#include "node_quic_util-inl.h"
#include "node_quic_default_application.h"
#include "node_quic_http3_application.h"
#include "node_sockaddr-inl.h"
#include "v8.h"
#include "uv.h"

#include <array>
#include <string>
#include <type_traits>
#include <utility>

namespace node {

using crypto::EntropySource;
using crypto::SecureContext;

using v8::Array;
using v8::ArrayBufferView;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyAttribute;
using v8::String;
using v8::Undefined;
using v8::Value;

namespace quic {

// Forwards detailed(verbose) debugging information from ngtcp2. Enabled using
// the NODE_DEBUG_NATIVE=NGTCP2_DEBUG category.
static void Ngtcp2DebugLog(void* user_data, const char* fmt, ...) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  va_list ap;
  va_start(ap, fmt);
  std::string format(fmt, strlen(fmt) + 1);
  format[strlen(fmt)] = '\n';
  Debug(session->env(), DebugCategory::NGTCP2_DEBUG, format, ap);
  va_end(ap);
}

void QuicSessionConfig::ResetToDefaults() {
  ngtcp2_settings_default(&settings_);
  settings_.initial_ts = uv_hrtime();
  settings_.log_printf = Ngtcp2DebugLog;
  settings_.transport_params.active_connection_id_limit =
    DEFAULT_ACTIVE_CONNECTION_ID_LIMIT;
  settings_.transport_params.initial_max_stream_data_bidi_local =
    DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL;
  settings_.transport_params.initial_max_stream_data_bidi_remote =
    DEFAULT_MAX_STREAM_DATA_BIDI_REMOTE;
  settings_.transport_params.initial_max_stream_data_uni =
    DEFAULT_MAX_STREAM_DATA_UNI;
  settings_.transport_params.initial_max_streams_bidi =
    DEFAULT_MAX_STREAMS_BIDI;
  settings_.transport_params.initial_max_streams_uni =
    DEFAULT_MAX_STREAMS_UNI;
  settings_.transport_params.initial_max_data = DEFAULT_MAX_DATA;
  settings_.transport_params.idle_timeout = DEFAULT_IDLE_TIMEOUT;
  settings_.transport_params.max_packet_size = NGTCP2_MAX_PKT_SIZE;
  settings_.transport_params.max_ack_delay = NGTCP2_DEFAULT_MAX_ACK_DELAY;
  settings_.transport_params.disable_active_migration = 0;
  settings_.transport_params.preferred_address_present = 0;
  settings_.transport_params.stateless_reset_token_present = 0;
  max_crypto_buffer_ = DEFAULT_MAX_CRYPTO_BUFFER;
}

inline void SetConfig(Environment* env, int idx, uint64_t* val) {
  AliasedFloat64Array& buffer = env->quic_state()->quicsessionconfig_buffer;
  uint64_t flags = static_cast<uint64_t>(buffer[IDX_QUIC_SESSION_CONFIG_COUNT]);
  if (flags & (1ULL << idx))
    *val = static_cast<uint64_t>(buffer[idx]);
}

// Sets the QuicSessionConfig using an AliasedBuffer for efficiency.
void QuicSessionConfig::Set(Environment* env,
                            const sockaddr* preferred_addr) {
  ResetToDefaults();

  SetConfig(env, IDX_QUIC_SESSION_ACTIVE_CONNECTION_ID_LIMIT,
            &settings_.transport_params.active_connection_id_limit);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_LOCAL,
            &settings_.transport_params.initial_max_stream_data_bidi_local);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_REMOTE,
            &settings_.transport_params.initial_max_stream_data_bidi_remote);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAM_DATA_UNI,
            &settings_.transport_params.initial_max_stream_data_uni);
  SetConfig(env, IDX_QUIC_SESSION_MAX_DATA,
            &settings_.transport_params.initial_max_data);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAMS_BIDI,
            &settings_.transport_params.initial_max_streams_bidi);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAMS_UNI,
            &settings_.transport_params.initial_max_streams_uni);
  SetConfig(env, IDX_QUIC_SESSION_IDLE_TIMEOUT,
            &settings_.transport_params.idle_timeout);
  SetConfig(env, IDX_QUIC_SESSION_MAX_PACKET_SIZE,
            &settings_.transport_params.max_packet_size);
  SetConfig(env, IDX_QUIC_SESSION_MAX_ACK_DELAY,
            &settings_.transport_params.max_ack_delay);

  SetConfig(env, IDX_QUIC_SESSION_MAX_CRYPTO_BUFFER,
            &max_crypto_buffer_);

  settings_.transport_params.idle_timeout =
      settings_.transport_params.idle_timeout * 1000000;

  max_crypto_buffer_ = std::max(max_crypto_buffer_, MIN_MAX_CRYPTO_BUFFER);

  if (preferred_addr != nullptr) {
    settings_.transport_params.preferred_address_present = 1;
    switch (preferred_addr->sa_family) {
      case AF_INET: {
        auto& dest = settings_.transport_params.preferred_address.ipv4_addr;
        memcpy(
            &dest,
            &(reinterpret_cast<const sockaddr_in*>(preferred_addr)->sin_addr),
            sizeof(dest));
        settings_.transport_params.preferred_address.ipv4_port =
            SocketAddress::GetPort(preferred_addr);
        break;
      }
      case AF_INET6: {
        auto& dest = settings_.transport_params.preferred_address.ipv6_addr;
        memcpy(
            &dest,
            &(reinterpret_cast<const sockaddr_in6*>(preferred_addr)->sin6_addr),
            sizeof(dest));
        settings_.transport_params.preferred_address.ipv6_port =
            SocketAddress::GetPort(preferred_addr);
        break;
      }
      default:
        UNREACHABLE();
    }
  }
}

void QuicSessionConfig::SetOriginalConnectionID(const ngtcp2_cid* ocid) {
  if (ocid) {
    settings_.transport_params.original_connection_id = *ocid;
    settings_.transport_params.original_connection_id_present = 1;
  }
}

void QuicSessionConfig::GenerateStatelessResetToken() {
  settings_.transport_params.stateless_reset_token_present = 1;
  EntropySource(
      settings_.transport_params.stateless_reset_token,
      arraysize(settings_.transport_params.stateless_reset_token));
}

void QuicSessionConfig::GeneratePreferredAddressToken(ngtcp2_cid* pscid) {
  if (!settings_.transport_params.preferred_address_present)
    return;
  size_t len =
      arraysize(
          settings_.transport_params.preferred_address.stateless_reset_token);
  EntropySource(
      settings_.transport_params.preferred_address.stateless_reset_token, len);

  pscid->datalen = NGTCP2_SV_SCIDLEN;
  EntropySource(pscid->data, pscid->datalen);
  settings_.transport_params.preferred_address.cid = *pscid;
}

void QuicSessionConfig::SetQlog(const ngtcp2_qlog_settings& qlog) {
  settings_.qlog = qlog;
}


QuicSessionListener::~QuicSessionListener() {
  if (session_ != nullptr)
    session_->RemoveListener(this);
}

void QuicSessionListener::OnKeylog(const char* line, size_t len) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnKeylog(line, len);
}

void QuicSessionListener::OnClientHello(
    const char* alpn,
    const char* server_name) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnClientHello(alpn, server_name);
}

void QuicSessionListener::OnCert(const char* server_name) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnCert(server_name);
}

void QuicSessionListener::OnOCSP(const std::string& ocsp) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnOCSP(ocsp);
}

void QuicSessionListener::OnStreamHeaders(
    int64_t stream_id,
    int kind,
    const std::vector<std::unique_ptr<QuicHeader>>& headers) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnStreamHeaders(stream_id, kind, headers);
}

void QuicSessionListener::OnStreamClose(
    int64_t stream_id,
    uint64_t app_error_code) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnStreamClose(stream_id, app_error_code);
}

void QuicSessionListener::OnStreamReset(
    int64_t stream_id,
    uint64_t final_size,
    uint64_t app_error_code) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnStreamReset(stream_id, final_size, app_error_code);
}

void QuicSessionListener::OnSessionDestroyed() {
  if (previous_listener_ != nullptr)
    previous_listener_->OnSessionDestroyed();
}

void QuicSessionListener::OnSessionClose(QuicError error) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnSessionClose(error);
}

void QuicSessionListener::OnStreamReady(BaseObjectPtr<QuicStream> stream) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnStreamReady(stream);
}

void QuicSessionListener::OnHandshakeCompleted() {
  if (previous_listener_ != nullptr)
    previous_listener_->OnHandshakeCompleted();
}

void QuicSessionListener::OnPathValidation(
    ngtcp2_path_validation_result res,
    const sockaddr* local,
    const sockaddr* remote) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnPathValidation(res, local, remote);
}

void QuicSessionListener::OnSessionTicket(int size, SSL_SESSION* session) {
  if (previous_listener_ != nullptr) {
    previous_listener_->OnSessionTicket(size, session);
  }
}

void QuicSessionListener::OnSessionSilentClose(
    bool stateless_reset,
    QuicError error) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnSessionSilentClose(stateless_reset, error);
}

void QuicSessionListener::OnVersionNegotiation(
    uint32_t supported_version,
    const uint32_t* versions,
    size_t vcnt) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnVersionNegotiation(supported_version, versions, vcnt);
}

void QuicSessionListener::OnQLog(const uint8_t* data, size_t len) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnQLog(data, len);
}

void JSQuicSessionListener::OnKeylog(const char* line, size_t len) {
  Environment* env = Session()->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Value> line_bf = Buffer::Copy(env, line, 1 + len).ToLocalChecked();
  char* data = Buffer::Data(line_bf);
  data[len] = '\n';

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(env->quic_on_session_keylog_function(), 1, &line_bf);
}

void JSQuicSessionListener::OnClientHello(
    const char* alpn,
    const char* server_name) {

  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Undefined(env->isolate()),
    Undefined(env->isolate()),
    crypto::GetClientHelloCiphers(env, Session()->CryptoContext()->ssl())
  };

  if (alpn != nullptr) {
    argv[0] = String::NewFromUtf8(
        env->isolate(),
        alpn,
        v8::NewStringType::kNormal).ToLocalChecked();
  }
  if (server_name != nullptr) {
    argv[1] = String::NewFromUtf8(
        env->isolate(),
        server_name,
        v8::NewStringType::kNormal).ToLocalChecked();
  }

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_session_client_hello_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnCert(const char* server_name) {
  Environment* env = Session()->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> servername_str;

  Local<Value> argv[] = {
    server_name == nullptr ?
        String::Empty(env->isolate()) :
        OneByteString(
            env->isolate(),
            server_name,
            strlen(server_name))
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_session_cert_function(),
      arraysize(argv),
      argv);
}

void JSQuicSessionListener::OnStreamHeaders(
    int64_t stream_id,
    int kind,
    const std::vector<std::unique_ptr<QuicHeader>>& headers) {
  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  MaybeStackBuffer<Local<Value>, 16> head(headers.size());
  size_t n = 0;
  for (const auto& header : headers) {
    // name and value should never be empty here, and if
    // they are, there's an actual bug so go ahead and crash
    Local<Value> pair[] = {
      header->GetName(Session()->Application()).ToLocalChecked(),
      header->GetValue(Session()->Application()).ToLocalChecked()
    };
    head[n++] = Array::New(env->isolate(), pair, arraysize(pair));
  }
  Local<Value> argv[] = {
      Number::New(env->isolate(), static_cast<double>(stream_id)),
      Array::New(env->isolate(), head.out(), n),
      Integer::New(env->isolate(), kind)
  };
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_stream_headers_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnOCSP(const std::string& ocsp) {
  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Value> arg = Undefined(env->isolate());
  if (ocsp.length() > 0)
    arg = Buffer::Copy(env, ocsp.c_str(), ocsp.length()).ToLocalChecked();
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(env->quic_on_session_status_function(), 1, & arg);
}

void JSQuicSessionListener::OnStreamClose(
    int64_t stream_id,
    uint64_t app_error_code) {
  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Number::New(env->isolate(), static_cast<double>(stream_id)),
    Number::New(env->isolate(), static_cast<double>(app_error_code))
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_stream_close_function(),
      arraysize(argv),
      argv);
}

void JSQuicSessionListener::OnStreamReset(
    int64_t stream_id,
    uint64_t final_size,
    uint64_t app_error_code) {
  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Number::New(env->isolate(), static_cast<double>(stream_id)),
    Number::New(env->isolate(), static_cast<double>(app_error_code)),
    Number::New(env->isolate(), static_cast<double>(final_size))
  };
  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_stream_reset_function(),
      arraysize(argv),
      argv);
}

void JSQuicSessionListener::OnSessionDestroyed() {
  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  // Emit the 'close' event in JS. This needs to happen after destroying the
  // connection, because doing so also releases the last qlog data.
  Session()->MakeCallback(
      env->quic_on_session_destroyed_function(), 0, nullptr);
}

void JSQuicSessionListener::OnSessionClose(QuicError error) {
  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Number::New(env->isolate(), static_cast<double>(error.code)),
    Integer::New(env->isolate(), error.family)
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_session_close_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnStreamReady(BaseObjectPtr<QuicStream> stream) {
  Environment* env = Session()->env();
  Local<Value> argv[] = {
    stream->object(),
    Number::New(env->isolate(), static_cast<double>(stream->GetID()))
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_stream_ready_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnHandshakeCompleted() {
  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> servername = Undefined(env->isolate());
  const char* hostname =
      crypto::GetServerName(Session()->CryptoContext()->ssl());
  if (hostname != nullptr) {
    servername =
        String::NewFromUtf8(
            env->isolate(),
            hostname,
            v8::NewStringType::kNormal).ToLocalChecked();
  }

  int err = Session()->CryptoContext()->VerifyPeerIdentity(
      hostname != nullptr ?
          hostname :
          Session()->GetHostname().c_str());

  Local<Value> argv[] = {
    servername,
    GetALPNProtocol(Session()),
    crypto::GetCipherName(env, Session()->CryptoContext()->ssl()),
    crypto::GetCipherVersion(env, Session()->CryptoContext()->ssl()),
    Integer::New(env->isolate(), Session()->max_pktlen_),
    err != 0 ?
        crypto::GetValidationErrorReason(env, err) :
        v8::Undefined(env->isolate()).As<Value>(),
    err != 0 ?
        crypto::GetValidationErrorCode(env, err) :
        v8::Undefined(env->isolate()).As<Value>()
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_session_handshake_function(),
      arraysize(argv),
      argv);
}

void JSQuicSessionListener::OnPathValidation(
    ngtcp2_path_validation_result res,
    const sockaddr* local,
    const sockaddr* remote) {
  // This is a fairly expensive operation because both the local and
  // remote addresses have to converted into JavaScript objects. We
  // only do this if a pathValidation handler is registered.
  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Local<Context> context = env->context();
  Context::Scope context_scope(context);
  Local<Value> argv[] = {
    Integer::New(env->isolate(), res),
    AddressToJS(env, local),
    AddressToJS(env, remote)
  };
  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_session_path_validation_function(),
      arraysize(argv),
      argv);
}

void JSQuicSessionListener::OnSessionTicket(int size, SSL_SESSION* session) {
  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  unsigned int session_id_length;
  const unsigned char* session_id_data =
      SSL_SESSION_get_id(session, &session_id_length);

  Local<Value> argv[] = {
    Buffer::Copy(
        env,
        reinterpret_cast<const char*>(session_id_data),
        session_id_length).ToLocalChecked(),
    v8::Undefined(env->isolate()),
    v8::Undefined(env->isolate())
  };

  AllocatedBuffer session_ticket = env->AllocateManaged(size);
  unsigned char* session_data =
      reinterpret_cast<unsigned char*>(session_ticket.data());
  memset(session_data, 0, size);
  i2d_SSL_SESSION(session, &session_data);
  if (!session_ticket.empty())
    argv[1] = session_ticket.ToBuffer().ToLocalChecked();

  if (Session()->IsFlagSet(
          QuicSession::QUICSESSION_FLAG_HAS_TRANSPORT_PARAMS)) {
    argv[2] = Buffer::Copy(
        env,
        reinterpret_cast<const char*>(&Session()->transport_params_),
        sizeof(Session()->transport_params_)).ToLocalChecked();
  }
  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_session_ticket_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnSessionSilentClose(
    bool stateless_reset,
    QuicError error) {
  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    stateless_reset ? v8::True(env->isolate()) : v8::False(env->isolate()),
    Number::New(env->isolate(), static_cast<double>(error.code)),
    Integer::New(env->isolate(), error.family)
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_session_silent_close_function(), arraysize(argv), argv);
}

void JSQuicSessionListener::OnVersionNegotiation(
    uint32_t supported_version,
    const uint32_t* vers,
    size_t vcnt) {
  Environment* env = Session()->env();
  HandleScope scope(env->isolate());
  Local<Context> context = env->context();
  Context::Scope context_scope(context);

  MaybeStackBuffer<Local<Value>, 4> versions(vcnt);
  for (size_t n = 0; n < vcnt; n++)
    versions[n] = Integer::New(env->isolate(), vers[n]);


  Local<Value> supported =
      Integer::New(env->isolate(), supported_version);

  Local<Value> argv[] = {
    Integer::New(env->isolate(), NGTCP2_PROTO_VER),
    Array::New(env->isolate(), versions.out(), vcnt),
    Array::New(env->isolate(), &supported, 1)
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(Session());
  Session()->MakeCallback(
      env->quic_on_session_version_negotiation_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnQLog(const uint8_t* data, size_t len) {
  Environment* env = Session()->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> str =
      String::NewFromOneByte(env->isolate(),
                             data,
                             v8::NewStringType::kNormal,
                             len).ToLocalChecked();

  Session()->MakeCallback(
      env->quic_on_session_qlog_function(),
      1,
      &str);
}

// Generates a new connection ID for this QuicSession.
// ngtcp2 will call this multiple times at the start of a new connection
// in order to build a pool of available CIDs.
void RandomConnectionIDStrategy::GetNewConnectionID(
    QuicSession* session,
    ngtcp2_cid* cid,
    size_t cidlen) {
  cid->datalen = cidlen;
  // cidlen shouldn't ever be zero here but just in case that
  // behavior changes in ngtcp2 in the future...
  if (cidlen > 0)
    EntropySource(cid->data, cidlen);
}

void CryptoStatelessResetTokenStrategy::GetNewStatelessToken(
    QuicSession* session,
    ngtcp2_cid* cid,
    uint8_t* token,
    size_t tokenlen) {
  ResetTokenSecret* secret = session->Socket()->GetSessionResetSecret();
  CHECK(GenerateResetToken(token, secret->data(), secret->size(), cid));
}

// Check required capabilities were not excluded from the OpenSSL build:
// - OPENSSL_NO_SSL_TRACE excludes SSL_trace()
// - OPENSSL_NO_STDIO excludes BIO_new_fp()
// HAVE_SSL_TRACE is available on the internal tcp_wrap binding for the tests.
#if defined(OPENSSL_NO_SSL_TRACE) || defined(OPENSSL_NO_STDIO)
# define HAVE_SSL_TRACE 0
#else
# define HAVE_SSL_TRACE 1
#endif

QuicCryptoContext::QuicCryptoContext(
    QuicSession* session,
    SecureContext* ctx,
    ngtcp2_crypto_side side,
    uint32_t options) :
    session_(session),
    side_(side),
    options_(options) {
  ssl_.reset(SSL_new(ctx->ctx_.get()));
  CHECK(ssl_);
}

uint64_t QuicCryptoContext::Cancel() {
  uint64_t len = handshake_[0].Cancel();
  len += handshake_[1].Cancel();
  len += handshake_[2].Cancel();
  return len;
}

void QuicCryptoContext::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("rx_secret", rx_secret_);
  tracker->TrackField("tx_secret", tx_secret_);
  tracker->TrackField("initial_crypto", handshake_[0]);
  tracker->TrackField("handshake_crypto", handshake_[1]);
  tracker->TrackField("app_crypto", handshake_[2]);
  tracker->TrackField("ocsp_response", ocsp_response_);
}

void QuicCryptoContext::AcknowledgeCryptoData(
    ngtcp2_crypto_level level,
    size_t datalen) {
  // It is possible for the QuicSession to have been destroyed but not yet
  // deconstructed. In such cases, we want to ignore the callback as there
  // is nothing to do but wait for further cleanup to happen.
  if (UNLIKELY(session_->IsDestroyed()))
    return;
  Debug(session_,
        "Acknowledging %d crypto bytes for %s level",
        datalen,
        crypto_level_name(level));

  // Consumes (frees) the given number of bytes in the handshake buffer.
  handshake_[level].Consume(datalen);

  // Update the statistics for the handshake, allowing us to track
  // how long the handshake is taking to be acknowledged. A malicious
  // peer could potentially force the QuicSession to hold on to
  // crypto data for a long time by not sending an acknowledgement.
  // The histogram will allow us to track the time periods between
  // acknowlegements.
  uint64_t now = uv_hrtime();
  if (session_->session_stats_.handshake_acked_at > 0)
    session_->crypto_rx_ack_->Record(
        now - session_->session_stats_.handshake_acked_at);
  session_->session_stats_.handshake_acked_at = now;
}

void QuicCryptoContext::EnableTrace() {
#if HAVE_SSL_TRACE
  if (!bio_trace_) {
    bio_trace_.reset(BIO_new_fp(stderr,  BIO_NOCLOSE | BIO_FP_TEXT));
    SSL_set_msg_callback(
        ssl(),
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
    SSL_set_msg_callback_arg(ssl(), bio_trace_.get());
  }
#endif
}

std::string QuicCryptoContext::GetOCSPResponse() {
  return crypto::GetSSLOCSPResponse(ssl());
}

ngtcp2_crypto_level QuicCryptoContext::GetReadCryptoLevel() {
  return from_ossl_level(SSL_quic_read_level(ssl()));
}

ngtcp2_crypto_level QuicCryptoContext::GetWriteCryptoLevel() {
  return from_ossl_level(SSL_quic_write_level(ssl()));
}

// TLS Keylogging is enabled per-QuicSession by attaching an handler to the
// "keylog" event. Each keylog line is emitted to JavaScript where it can
// be routed to whatever destination makes sense. Typically, this will be
// to a keylog file that can be consumed by tools like Wireshark to intercept
// and decrypt QUIC network traffic.
void QuicCryptoContext::Keylog(const char* line) {
  if (LIKELY(session_->state_[IDX_QUIC_SESSION_STATE_KEYLOG_ENABLED] == 0))
    return;
  session_->Listener()->OnKeylog(line, strlen(line));
}

// If a 'clientHello' event listener is registered on the JavaScript
// QuicServerSession object, the STATE_CLIENT_HELLO_ENABLED state
// will be set and the OnClientHello will cause the 'clientHello'
// event to be emitted.
//
// The 'clientHello' callback will be given it's own callback function
// that must be called when the client has completed handling the event.
// The handshake will not continue until it is called.
//
// The intent here is to allow user code the ability to modify or
// replace the SecurityContext based on the server name, ALPN, or
// other handshake characteristics.
//
// The user can also set a 'cert' event handler that will be called
// when the peer certificate is received, allowing additional tweaks
// and verifications to be performed.
int QuicCryptoContext::OnClientHello() {
  if (LIKELY(session_->state_[
          IDX_QUIC_SESSION_STATE_CLIENT_HELLO_ENABLED] == 0)) {
    return 0;
  }

  TLSCallbackScope callback_scope(this);

  // Not an error but does suspend the handshake until we're ready to go.
  // A callback function is passed to the JavaScript function below that
  // must be called in order to turn QUICSESSION_FLAG_CLIENT_HELLO_CB_RUNNING
  // off. Once that callback is invoked, the TLS Handshake will resume.
  // It is recommended that the user not take a long time to invoke the
  // callback in order to avoid stalling out the QUIC connection.
  if (in_client_hello_)
    return -1;
  in_client_hello_ = true;

  const char* alpn =
      crypto::GetClientHelloALPN(session_->CryptoContext()->ssl());
  const char* server_name =
      crypto::GetClientHelloServerName(session_->CryptoContext()->ssl());
  session_->Listener()->OnClientHello(alpn, server_name);

  return in_client_hello_ ? -1 : 0;
}

void QuicCryptoContext::OnClientHelloDone() {
  // Continue the TLS handshake when this function exits
  // otherwise it will stall and fail.
  TLSHandshakeScope handshake(this, &in_client_hello_);
  // Disable the callback at this point so we don't loop continuously
  session_->state_[IDX_QUIC_SESSION_STATE_CLIENT_HELLO_ENABLED] = 0;
}


// The OnCert callback provides an opportunity to prompt the server to
// perform on OCSP request on behalf of the client (when the client
// requests it). If there is a listener for the 'OCSPRequest' event
// on the JavaScript side, the IDX_QUIC_SESSION_STATE_CERT_ENABLED
// session state slot will equal 1, which will cause the callback to
// be invoked. The callback will be given a reference to a JavaScript
// function that must be called in order for the TLS handshake to
// continue.
int QuicCryptoContext::OnOCSP() {
  if (LIKELY(session_->state_[IDX_QUIC_SESSION_STATE_CERT_ENABLED] == 0)) {
    Debug(session_, "No OCSPRequest handler registered");
    return 1;
  }

  Debug(session_, "Client is requesting an OCSP Response");
  TLSCallbackScope callback_scope(this);

  // As in node_crypto.cc, this is not an error, but does suspend the
  // handshake to continue when OnOCSP is complete.
  if (in_ocsp_request_)
    return -1;
  in_ocsp_request_ = true;

  session_->Listener()->OnCert(
      crypto::GetServerName(session_->CryptoContext()->ssl()));

  return in_ocsp_request_ ? -1 : 1;
}

// The OnCertDone function is called by the QuicSessionOnCertDone
// function when usercode is done handling the OCSPRequest event.
void QuicCryptoContext::OnOCSPDone(
    crypto::SecureContext* context,
    Local<Value> ocsp_response) {
  Debug(session_,
        "OCSPRequest completed. Context Provided? %s, OCSP Provided? %s",
        context != nullptr ? "Yes" : "No",
        ocsp_response->IsArrayBufferView() ? "Yes" : "No");
  // Continue the TLS handshake when this function exits
  // otherwise it will stall and fail.
  TLSHandshakeScope handshake_scope(this, &in_ocsp_request_);

  // Disable the callback at this point so we don't loop continuously
  session_->state_[IDX_QUIC_SESSION_STATE_CERT_ENABLED] = 0;

  if (context != nullptr) {
    int err = crypto::UseSNIContext(ssl(), context);
    if (!err) {
      unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
      if (!err) {
        // TODO(@jasnell): revisit to throw a proper error here
        return session_->env()->ThrowError("CertCbDone");
      }
      return crypto::ThrowCryptoError(session_->env(), err);
    }
  }

  if (ocsp_response->IsArrayBufferView()) {
    ocsp_response_.Reset(
        session_->env()->isolate(),
        ocsp_response.As<ArrayBufferView>());
  }
}

bool QuicCryptoContext::OnSecrets(
    ngtcp2_crypto_level level,
    const uint8_t* rx_secret,
    const uint8_t* tx_secret,
    size_t secretlen) {

  auto maybe_init_app = OnScopeLeave([&]() {
    if (level == NGTCP2_CRYPTO_LEVEL_APP) {
      // TODO(@jasnell): Fail if init returns false
      session_->InitApplication();
    }
  });

  if (level == NGTCP2_CRYPTO_LEVEL_APP) {
    rx_secret_.assign(rx_secret, rx_secret + secretlen);
    tx_secret_.assign(tx_secret, tx_secret + secretlen);
  }

  Debug(session_,
        "Received secrets for %s crypto level",
        crypto_level_name(level));

  return SetCryptoSecrets(session_, level, rx_secret, tx_secret, secretlen);
}

// When the client has requested OSCP, this function will be called to provide
// the OSCP response. The OnCert() callback should have already been called
// by this point if any data is to be provided. If it hasn't, and ocsp_response_
// is empty, no OCSP response will be sent.
int QuicCryptoContext::OnTLSStatus() {
  Environment* env = session_->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  switch (side_) {
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      if (ocsp_response_.IsEmpty()) {
        Debug(session_, "There is no OCSP response");
        return SSL_TLSEXT_ERR_NOACK;
      }

      Local<ArrayBufferView> obj =
          PersistentToLocal::Default(
            env->isolate(),
            ocsp_response_);
      size_t len = obj->ByteLength();

      unsigned char* data = crypto::MallocOpenSSL<unsigned char>(len);
      obj->CopyContents(data, len);

      Debug(session_, "There is an OCSP response of %d bytes", len);

      if (!SSL_set_tlsext_status_ocsp_resp(ssl(), data, len))
        OPENSSL_free(data);

      ocsp_response_.Reset();

      return SSL_TLSEXT_ERR_OK;
    }
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      std::string resp = GetOCSPResponse();
      if (resp.length() > 0)
        Debug(session_, "An OCSP Response has been received");
      session_->Listener()->OnOCSP(resp);
      return 1;
    }
    default:
      UNREACHABLE();
  }
}

// Called by ngtcp2 when a chunk of peer TLS handshake data is received.
// For every chunk, we move the TLS handshake further along until it
// is complete.
int QuicCryptoContext::Receive(
    ngtcp2_crypto_level crypto_level,
    uint64_t offset,
    const uint8_t* data,
    size_t datalen) {
  if (UNLIKELY(session_->IsDestroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  uint64_t now = uv_hrtime();
  if (session_->session_stats_.handshake_start_at == 0)
    session_->session_stats_.handshake_start_at = now;
  session_->session_stats_.handshake_continue_at = now;

  Debug(session_, "Receiving %d bytes of crypto data.", datalen);

  int ret = ngtcp2_crypto_read_write_crypto_data(
      session_->Connection(),
      ssl(),
      crypto_level,
      data,
      datalen);
  switch (ret) {
    case 0:
      return 0;
    case NGTCP2_CRYPTO_ERR_TLS_WANT_X509_LOOKUP:
      Debug(session_, "TLS handshake wants X509 Lookup");
      return 0;
    case NGTCP2_CRYPTO_ERR_TLS_WANT_CLIENT_HELLO_CB:
      Debug(session_, "TLS handshake wants client hello callback");
      return 0;
    default:
      return ret;
  }
}

void QuicCryptoContext::ResumeHandshake() {
  Receive(GetReadCryptoLevel(), 0, nullptr, 0);
  session_->SendPendingData();
}

bool QuicCryptoContext::SetSession(const unsigned char* data, size_t length) {
  return crypto::SetTLSSession(ssl(), data, length);
}

void QuicCryptoContext::SetTLSAlert(int err) {
  Debug(session_, "TLS Alert [%d]: %s", err, SSL_alert_type_string_long(err));
  session_->SetLastError(QuicError(QUIC_ERROR_CRYPTO, err));
}

bool QuicCryptoContext::SetupInitialKey(const ngtcp2_cid* dcid) {
  Debug(session_, "Deriving and installing initial keys");
  return DeriveAndInstallInitialKey(session_, dcid);
}

bool QuicCryptoContext::InitiateKeyUpdate() {
  if (UNLIKELY(session_->IsDestroyed()))
    return false;

  // There's no user code that should be able to run while UpdateKey
  // is running, but we need to gate on it just to be safe.
  auto leave = OnScopeLeave([&]() { in_key_update_ = false; });
  CHECK(!in_key_update_);
  in_key_update_ = true;
  Debug(session_, "Initiating Key Update");

  IncrementStat(
      1, &session_->session_stats_,
      &QuicSession::session_stats::keyupdate_count);

  int err = ngtcp2_conn_initiate_key_update(
             session_->Connection(),
             uv_hrtime());
  return err == 0;
}

bool QuicCryptoContext::KeyUpdate(
    uint8_t* rx_key,
    uint8_t* rx_iv,
    uint8_t* tx_key,
    uint8_t* tx_iv) {
  if (UNLIKELY(session_->IsDestroyed()))
    return false;
  return UpdateKey(
      session_,
      rx_key,
      rx_iv,
      tx_key,
      tx_iv,
      &rx_secret_,
      &tx_secret_);
}

int QuicCryptoContext::VerifyPeerIdentity(const char* hostname) {
  int err = crypto::VerifyPeerCertificate(ssl());
  if (err)
    return err;

  switch (side_) {
    case NGTCP2_CRYPTO_SIDE_CLIENT:
      if (LIKELY(
              IsOptionSet(QUICCLIENTSESSION_OPTION_VERIFY_HOSTNAME_IDENTITY))) {
        return VerifyHostnameIdentity(ssl(), hostname);
      }
      break;
    case NGTCP2_CRYPTO_SIDE_SERVER:
      break;
  }

  return 0;
}

void QuicCryptoContext::WriteHandshake(
    ngtcp2_crypto_level level,
    const uint8_t* data,
    size_t datalen) {
  Debug(session_,
        "Writing %d bytes of %s handshake data.",
        datalen,
        crypto_level_name(level));
  MallocedBuffer<uint8_t> buffer(datalen);
  memcpy(buffer.data, data, datalen);
  session_->session_stats_.handshake_send_at = uv_hrtime();
  CHECK_EQ(
      ngtcp2_conn_submit_crypto_data(
          session_->Connection(),
          level,
          buffer.data,
          datalen), 0);
  handshake_[level].Push(std::move(buffer));
}

std::unique_ptr<QuicPacket> QuicApplication::CreateStreamDataPacket() {
  return QuicPacket::Create(
      "stream data",
      Session()->GetMaxPacketLength());
}

void QuicApplication::StreamHeaders(
    int64_t stream_id,
    int kind,
    const std::vector<std::unique_ptr<QuicHeader>>& headers) {
  Session()->Listener()->OnStreamHeaders(stream_id, kind, headers);
}

void QuicApplication::StreamClose(
    int64_t stream_id,
    uint64_t app_error_code) {
  Session()->Listener()->OnStreamClose(stream_id, app_error_code);
}

void QuicApplication::StreamReset(
    int64_t stream_id,
    uint64_t final_size,
    uint64_t app_error_code) {
  Session()->Listener()->OnStreamReset(stream_id, final_size, app_error_code);
}

QuicApplication::QuicApplication(QuicSession* session) : session_(session) {}

QuicApplication* QuicSession::SelectApplication(QuicSession* session) {
  std::string alpn = session->GetALPN();
  if (alpn == NGTCP2_ALPN_H3) {
    Debug(this, "Selecting HTTP/3 Application");
    return new Http3Application(session);
  }
  // In the future, we may end up supporting additional
  // QUIC protocols. As they are added, extend the cases
  // here to create and return them.

  Debug(this, "Selecting Default Application");
  return new DefaultApplication(session);
}

// Server QuicSession Constructor
QuicSession::QuicSession(
    QuicSocket* socket,
    QuicSessionConfig* config,
    Local<Object> wrap,
    const ngtcp2_cid* rcid,
    const SocketAddress& local_addr,
    const struct sockaddr* remote_addr,
    const ngtcp2_cid* dcid,
    const ngtcp2_cid* ocid,
    uint32_t version,
    const std::string& alpn,
    uint32_t options,
    uint64_t initial_connection_close,
    QlogMode qlog)
  : QuicSession(
        NGTCP2_CRYPTO_SIDE_SERVER,
        socket,
        wrap,
        socket->GetServerSecureContext(),
        AsyncWrap::PROVIDER_QUICSERVERSESSION,
        alpn,
        std::string(""),  // empty hostname. not used on server side
        rcid,
        options,
        QUIC_PREFERRED_ADDRESS_ACCEPT,  // Not used on server sessions
        initial_connection_close) {
  InitServer(
      config,
      local_addr,
      remote_addr,
      dcid,
      ocid,
      version,
      qlog);
}

// Client QuicSession Constructor
QuicSession::QuicSession(
    QuicSocket* socket,
    v8::Local<v8::Object> wrap,
    const SocketAddress& local_addr,
    const struct sockaddr* remote_addr,
    SecureContext* context,
    Local<Value> early_transport_params,
    Local<Value> session_ticket,
    Local<Value> dcid,
    SelectPreferredAddressPolicy select_preferred_address_policy,
    const std::string& alpn,
    const std::string& hostname,
    uint32_t options,
    QlogMode qlog)
  : QuicSession(
        NGTCP2_CRYPTO_SIDE_CLIENT,
        socket,
        wrap,
        context,
        AsyncWrap::PROVIDER_QUICCLIENTSESSION,
        alpn,
        hostname,
        nullptr,  // rcid only used on the server
        options,
        select_preferred_address_policy) {
  CHECK(InitClient(
      local_addr,
      remote_addr,
      early_transport_params,
      session_ticket,
      dcid,
      qlog));
}

// QuicSession is an abstract base class that defines the code used by both
// server and client sessions.
QuicSession::QuicSession(
    ngtcp2_crypto_side side,
    QuicSocket* socket,
    Local<Object> wrap,
    SecureContext* ctx,
    AsyncWrap::ProviderType provider_type,
    const std::string& alpn,
    const std::string& hostname,
    const ngtcp2_cid* rcid,
    uint32_t options,
    SelectPreferredAddressPolicy select_preferred_address_policy,
    uint64_t initial_connection_close)
  : AsyncWrap(socket->env(), wrap, provider_type),
    alloc_info_(MakeAllocator()),
    socket_(socket),
    alpn_(alpn),
    hostname_(hostname),
    initial_connection_close_(initial_connection_close),
    idle_(new Timer(socket->env(), [this]() { OnIdleTimeout(); })),
    retransmit_(new Timer(socket->env(), [this]() { MaybeTimeout(); })),
    select_preferred_address_policy_(select_preferred_address_policy),
    state_(env()->isolate(), IDX_QUIC_SESSION_STATE_COUNT),
    crypto_rx_ack_(
        HistogramBase::New(
            socket->env(),
            1, std::numeric_limits<int64_t>::max())),
    crypto_handshake_rate_(
        HistogramBase::New(
            socket->env(),
            1, std::numeric_limits<int64_t>::max())),
    stats_buffer_(
        socket->env()->isolate(),
        sizeof(session_stats_) / sizeof(uint64_t),
        reinterpret_cast<uint64_t*>(&session_stats_)),
    recovery_stats_buffer_(
        socket->env()->isolate(),
        sizeof(recovery_stats_) / sizeof(double),
        reinterpret_cast<double*>(&recovery_stats_)) {
  PushListener(&default_listener_);
  SetConnectionIDStrategory(&default_connection_id_strategy_);
  SetStatelessResetTokenStrategy(&default_stateless_reset_strategy_);
  crypto_context_.reset(new QuicCryptoContext(this, ctx, side, options));
  application_.reset(SelectApplication(this));
  if (rcid != nullptr)
    rcid_ = *rcid;

  session_stats_.created_at = uv_hrtime();

  if (wrap->DefineOwnProperty(
          env()->context(),
          env()->state_string(),
          state_.GetJSArray(),
          PropertyAttribute::ReadOnly).IsNothing()) return;

  if (wrap->DefineOwnProperty(
          env()->context(),
          env()->stats_string(),
          stats_buffer_.GetJSArray(),
          PropertyAttribute::ReadOnly).IsNothing()) return;

  if (wrap->DefineOwnProperty(
          env()->context(),
          env()->recovery_stats_string(),
          recovery_stats_buffer_.GetJSArray(),
          PropertyAttribute::ReadOnly).IsNothing()) return;

  if (wrap->DefineOwnProperty(
          env()->context(),
          FIXED_ONE_BYTE_STRING(env()->isolate(), "crypto_rx_ack"),
          crypto_rx_ack_->object(),
          PropertyAttribute::ReadOnly).IsNothing()) return;

  if (wrap->DefineOwnProperty(
          env()->context(),
          FIXED_ONE_BYTE_STRING(env()->isolate(), "crypto_handshake_rate"),
          crypto_handshake_rate_->object(),
          PropertyAttribute::ReadOnly).IsNothing()) return;

  // TODO(@jasnell): memory accounting
  // env_->isolate()->AdjustAmountOfExternalAllocatedMemory(kExternalSize);
}

QuicSession::~QuicSession() {
  CHECK(!Ngtcp2CallbackScope::InNgtcp2CallbackScope(this));

  uint64_t handshake_length = crypto_context_->Cancel();

  Debug(this,
        "Destroyed.\n"
        "  Duration: %" PRIu64 "\n"
        "  Handshake Started: %" PRIu64 "\n"
        "  Handshake Completed: %" PRIu64 "\n"
        "  Bytes Received: %" PRIu64 "\n"
        "  Bytes Sent: %" PRIu64 "\n"
        "  Bidi Stream Count: %" PRIu64 "\n"
        "  Uni Stream Count: %" PRIu64 "\n"
        "  Streams In Count: %" PRIu64 "\n"
        "  Streams Out Count: %" PRIu64 "\n"
        "  Remaining handshake_: %" PRIu64 "\n"
        "  Max In Flight Bytes: %" PRIu64 "\n",
        uv_hrtime() - session_stats_.created_at,
        session_stats_.handshake_start_at,
        session_stats_.handshake_completed_at,
        session_stats_.bytes_received,
        session_stats_.bytes_sent,
        session_stats_.bidi_stream_count,
        session_stats_.uni_stream_count,
        session_stats_.streams_in_count,
        session_stats_.streams_out_count,
        handshake_length,
        session_stats_.max_bytes_in_flight);

  connection_.reset();

  QuicSessionListener* listener = Listener();
  listener->OnSessionDestroyed();
  if (listener == Listener())
    RemoveListener(listener);
}

void QuicSession::PushListener(QuicSessionListener* listener) {
  CHECK_NOT_NULL(listener);
  CHECK_NULL(listener->session_);

  listener->previous_listener_ = listener_;
  listener->session_ = this;

  listener_ = listener;
}

void QuicSession::RemoveListener(QuicSessionListener* listener) {
  CHECK_NOT_NULL(listener);

  QuicSessionListener* previous;
  QuicSessionListener* current;

  for (current = listener_, previous = nullptr;
       /* No loop condition because we want a crash if listener is not found */
       ; previous = current, current = current->previous_listener_) {
    CHECK_NOT_NULL(current);
    if (current == listener) {
      if (previous != nullptr)
        previous->previous_listener_ = current->previous_listener_;
      else
        listener_ = listener->previous_listener_;
      break;
    }
  }

  listener->session_ = nullptr;
  listener->previous_listener_ = nullptr;
}

void QuicSession::StreamDataBlocked(int64_t stream_id) {
  // Increments the block count
  IncrementStat(1, &session_stats_, &session_stats::block_count);
}

std::string QuicSession::diagnostic_name() const {
  return std::string("QuicSession ") +
      (IsServer() ? "Server" : "Client") +
      " (" + GetALPN().substr(1) + ", " +
      std::to_string(static_cast<int64_t>(get_async_id())) + ")";
}

// Locate the QuicStream with the given id or return nullptr
QuicStream* QuicSession::FindStream(int64_t id) {
  auto it = streams_.find(id);
  if (it == std::end(streams_))
    return nullptr;
  return it->second.get();
}

void QuicSession::AckedStreamDataOffset(
    int64_t stream_id,
    uint64_t offset,
    size_t datalen) {
  // It is possible for the QuicSession to have been destroyed but not yet
  // deconstructed. In such cases, we want to ignore the callback as there
  // is nothing to do but wait for further cleanup to happen.
  if (UNLIKELY(IsFlagSet(QUICSESSION_FLAG_DESTROYED)))
    return;
  Debug(this,
        "Received acknowledgement for %" PRIu64
        " bytes of stream %" PRId64 " data",
        datalen, stream_id);

  application_->AcknowledgeStreamData(stream_id, offset, datalen);
}

void QuicSession::AddToSocket(QuicSocket* socket) {
  QuicCID scid(scid_);
  socket->AddSession(scid, BaseObjectPtr<QuicSession>(this));

  switch (crypto_context_->Side()) {
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      socket->AssociateCID(QuicCID(rcid_), scid);

      if (pscid_.datalen)
        socket->AssociateCID(QuicCID(pscid_), scid);

      break;
    }
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      std::vector<ngtcp2_cid> cids(ngtcp2_conn_get_num_scid(Connection()));
      ngtcp2_conn_get_scid(Connection(), cids.data());
      for (const ngtcp2_cid& cid : cids)
        socket->AssociateCID(QuicCID(&cid), scid);
      break;
    }
    default:
      UNREACHABLE();
  }
}

// Add the given QuicStream to this QuicSession's collection of streams. All
// streams added must be removed before the QuicSession instance is freed.
void QuicSession::AddStream(BaseObjectPtr<QuicStream> stream) {
  DCHECK(!IsFlagSet(QUICSESSION_FLAG_GRACEFUL_CLOSING));
  Debug(this, "Adding stream %" PRId64 " to session.", stream->GetID());
  streams_.emplace(stream->GetID(), stream);

  // Update tracking statistics for the number of streams associated with
  // this session.
  switch (stream->GetOrigin()) {
    case QuicStream::QuicStreamOrigin::QUIC_STREAM_CLIENT:
      if (IsServer())
        IncrementStat(1, &session_stats_, &session_stats::streams_in_count);
      else
        IncrementStat(1, &session_stats_, &session_stats::streams_out_count);
      break;
    case QuicStream::QuicStreamOrigin::QUIC_STREAM_SERVER:
      if (IsServer())
        IncrementStat(1, &session_stats_, &session_stats::streams_out_count);
      else
        IncrementStat(1, &session_stats_, &session_stats::streams_in_count);
  }
  IncrementStat(1, &session_stats_, &session_stats::streams_out_count);
  switch (stream->GetDirection()) {
    case QuicStream::QuicStreamDirection::QUIC_STREAM_BIRECTIONAL:
      IncrementStat(1, &session_stats_, &session_stats::bidi_stream_count);
      break;
    case QuicStream::QuicStreamDirection::QUIC_STREAM_UNIDIRECTIONAL:
      IncrementStat(1, &session_stats_, &session_stats::uni_stream_count);
      break;
  }
}

// Every QUIC session will have multiple CIDs associated with it.
void QuicSession::AssociateCID(ngtcp2_cid* cid) {
  Socket()->AssociateCID(QuicCID(cid), QuicCID(scid_));
}

// Like the silent close, the immediate close must start with
// the JavaScript side, first shutting down any existing
// streams before entering the closing period. Unlike silent
// close, however, all streams are closed using proper
// STOP_SENDING and RESET_STREAM frames and a CONNECTION_CLOSE
// frame is ultimately sent to the peer. This makes the
// naming a bit of a misnomer in that the connection is
// not immediately torn down, but is allowed to drain
// properly per the QUIC spec description of "immediate close".
void QuicSession::ImmediateClose() {
  // Calling either ImmediateClose or SilentClose will cause
  // the QUICSESSION_FLAG_CLOSING to be set. In either case,
  // we should never re-enter ImmediateClose or SilentClose.
  CHECK(!IsFlagSet(QUICSESSION_FLAG_CLOSING));
  SetFlag(QUICSESSION_FLAG_CLOSING);

  QuicError last_error = GetLastError();
  Debug(this, "Immediate close with code %" PRIu64 " (%s)",
        last_error.code,
        last_error.GetFamilyName());

  Listener()->OnSessionClose(last_error);
}

void QuicSession::InitApplication() {
  Debug(this, "Initializing application handler for ALPN %s",
        GetALPN().c_str() + 1);
  application_->Initialize();
}

// Creates a new stream object and passes it off to the javascript side.
// This has to be called from within a handlescope/contextscope.
QuicStream* QuicSession::CreateStream(int64_t stream_id) {
  CHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));
  CHECK(!IsFlagSet(QUICSESSION_FLAG_GRACEFUL_CLOSING));
  CHECK(!IsFlagSet(QUICSESSION_FLAG_CLOSING));

  BaseObjectPtr<QuicStream> stream = QuicStream::New(this, stream_id);
  CHECK(stream);
  Listener()->OnStreamReady(stream);
  return stream.get();
}

void QuicSession::DisassociateCID(const ngtcp2_cid* cid) {
  if (IsServer())
    Socket()->DisassociateCID(QuicCID(cid));
}

// Mark the QuicSession instance destroyed. After this is called,
// the QuicSession instance will be generally unusable but most
// likely will not be immediately freed.
void QuicSession::Destroy() {
  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED))
    return;
  Debug(this, "Destroying");

  // If we're not in the closing or draining periods,
  // then we should at least attempt to send a connection
  // close to the peer.
  // TODO(@jasnell): If the connection close happens to occur
  // while we're still at the start of the TLS handshake, a
  // CONNECTION_CLOSE is not going to be sent because ngtcp2
  // currently does not yet support it. That will need to be
  // addressed.
  if (!Ngtcp2CallbackScope::InNgtcp2CallbackScope(this) &&
      !IsInClosingPeriod() &&
      !IsInDrainingPeriod()) {
    Debug(this, "Making attempt to send a connection close");
    SetLastError();
    SendConnectionClose();
  }

  // Streams should have already been destroyed by this point.
  CHECK(streams_.empty());

  // Mark the session destroyed.
  SetFlag(QUICSESSION_FLAG_DESTROYED);
  SetFlag(QUICSESSION_FLAG_CLOSING, false);
  SetFlag(QUICSESSION_FLAG_GRACEFUL_CLOSING, false);

  // Stop and free the idle and retransmission timers if they are active.
  StopIdleTimer();
  StopRetransmitTimer();

  // The QuicSession instances are kept alive using
  // BaseObjectPtr. The only persistent BaseObjectPtr
  // is the map in the associated QuicSocket. Removing
  // the QuicSession from the QuicSocket will free
  // that pointer, allowing the QuicSession to be
  // deconstructed once the stack unwinds and any
  // remaining shared_ptr instances fall out of scope.
  RemoveFromSocket();
}

void QuicSession::ExtendMaxStreamData(int64_t stream_id, uint64_t max_data) {
  Debug(this,
        "Extending max stream %" PRId64 " data to %" PRIu64,
        stream_id, max_data);
  application_->ExtendMaxStreamData(stream_id, max_data);
}

void QuicSession::ExtendMaxStreamsRemoteUni(uint64_t max_streams) {
  Debug(this, "Extend remote max unidirectional streams: %" PRIu64,
        max_streams);
  application_->ExtendMaxStreamsRemoteUni(max_streams);
}

void QuicSession::ExtendMaxStreamsRemoteBidi(uint64_t max_streams) {
  Debug(this, "Extend remote max bidirectional streams: %" PRIu64,
        max_streams);
  application_->ExtendMaxStreamsRemoteBidi(max_streams);
}

void QuicSession::ExtendMaxStreamsUni(uint64_t max_streams) {
  Debug(this, "Setting max unidirectional streams to %" PRIu64, max_streams);
  state_[IDX_QUIC_SESSION_STATE_MAX_STREAMS_UNI] =
      static_cast<double>(max_streams);
}

void QuicSession::ExtendMaxStreamsBidi(uint64_t max_streams) {
  Debug(this, "Setting max bidirectional streams to %" PRIu64, max_streams);
  state_[IDX_QUIC_SESSION_STATE_MAX_STREAMS_BIDI] =
      static_cast<double>(max_streams);
}

void QuicSession::ExtendStreamOffset(QuicStream* stream, size_t amount) {
  Debug(this, "Extending max stream %" PRId64 " offset by %" PRId64 " bytes",
        stream->GetID(), amount);
  ngtcp2_conn_extend_max_stream_offset(
      Connection(),
      stream->GetID(),
      amount);
}

void QuicSession::ExtendOffset(size_t amount) {
  Debug(this, "Extending session offset by %" PRId64 " bytes", amount);
  ngtcp2_conn_extend_max_offset(Connection(), amount);
}

// Copies the local transport params into the given struct for serialization.
void QuicSession::GetLocalTransportParams(ngtcp2_transport_params* params) {
  CHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));
  ngtcp2_conn_get_local_transport_params(Connection(), params);
}

// Gets the QUIC version negotiated for this QuicSession
uint32_t QuicSession::GetNegotiatedVersion() {
  CHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));
  return ngtcp2_conn_get_negotiated_version(Connection());
}

void QuicSession::SetConnectionIDStrategory(ConnectionIDStrategy* strategy) {
  CHECK_NOT_NULL(strategy);
  connection_id_strategy_ = strategy;
}

void QuicSession::SetStatelessResetTokenStrategy(
    StatelessResetTokenStrategy* strategy) {
  CHECK_NOT_NULL(strategy);
  stateless_reset_strategy_ = strategy;
}

// Generates and associates a new connection ID for this QuicSession.
// ngtcp2 will call this multiple times at the start of a new connection
// in order to build a pool of available CIDs.
int QuicSession::GetNewConnectionID(
    ngtcp2_cid* cid,
    uint8_t* token,
    size_t cidlen) {
  DCHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));
  CHECK_NOT_NULL(connection_id_strategy_);
  connection_id_strategy_->GetNewConnectionID(
      this,
      cid,
      cidlen);
  stateless_reset_strategy_->GetNewStatelessToken(
      this,
      cid,
      token,
      NGTCP2_STATELESS_RESET_TOKENLEN);

  AssociateCID(cid);
  return 0;
}

void QuicSession::HandleError() {
  if (connection_ && IsInClosingPeriod() && !IsServer())
    return;

  if (!SendConnectionClose()) {
    SetLastError(QUIC_ERROR_SESSION, NGTCP2_ERR_INTERNAL);
    ImmediateClose();
  }
}

// The HandshakeCompleted function is called by ngtcp2 once it
// determines that the TLS Handshake is done. The only thing we
// need to do at this point is let the javascript side know.
void QuicSession::HandshakeCompleted() {
  Debug(this, "Handshake is completed");
  session_stats_.handshake_completed_at = uv_hrtime();
  Listener()->OnHandshakeCompleted();
}

bool QuicSession::IsHandshakeCompleted() {
  DCHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));
  return ngtcp2_conn_get_handshake_completed(Connection());
}

// When a QuicSession hits the idle timeout, it is to be silently and
// immediately closed without attempting to send any additional data to
// the peer. All existing streams are abandoned and closed.
void QuicSession::OnIdleTimeout() {
  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED))
    return;
  Debug(this, "Idle timeout");
  return SilentClose();
}

void QuicSession::MaybeTimeout() {
  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED))
    return;
  uint64_t now = uv_hrtime();
  bool transmit = false;
  if (ngtcp2_conn_loss_detection_expiry(Connection()) <= now) {
    Debug(this, "Retransmitting due to loss detection");
    CHECK_EQ(ngtcp2_conn_on_loss_detection_timer(Connection(), now), 0);
    IncrementStat(
        1, &session_stats_,
        &session_stats::loss_retransmit_count);
    transmit = true;
  } else if (ngtcp2_conn_ack_delay_expiry(Connection()) <= now) {
    Debug(this, "Retransmitting due to ack delay");
    ngtcp2_conn_cancel_expired_ack_delay_timer(Connection(), now);
    IncrementStat(
        1, &session_stats_,
        &session_stats::ack_delay_retransmit_count);
    transmit = true;
  }
  if (transmit)
    SendPendingData();
}

bool QuicSession::OpenBidirectionalStream(int64_t* stream_id) {
  DCHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));
  DCHECK(!IsFlagSet(QUICSESSION_FLAG_CLOSING));
  DCHECK(!IsFlagSet(QUICSESSION_FLAG_GRACEFUL_CLOSING));
  return ngtcp2_conn_open_bidi_stream(Connection(), stream_id, nullptr) == 0;
}

bool QuicSession::OpenUnidirectionalStream(int64_t* stream_id) {
  DCHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));
  DCHECK(!IsFlagSet(QUICSESSION_FLAG_CLOSING));
  DCHECK(!IsFlagSet(QUICSESSION_FLAG_GRACEFUL_CLOSING));
  if (ngtcp2_conn_open_uni_stream(Connection(), stream_id, nullptr))
    return false;
  ngtcp2_conn_shutdown_stream_read(Connection(), *stream_id, 0);
  return true;
}

void QuicSession::PathValidation(
    const ngtcp2_path* path,
    ngtcp2_path_validation_result res) {
  if (res == NGTCP2_PATH_VALIDATION_RESULT_SUCCESS) {
    Debug(this,
          "Path validation succeeded. Updating local and remote addresses");
    SetLocalAddress(&path->local);
    UpdateEndpoint(*path);
    IncrementStat(
        1, &session_stats_,
        &session_stats::path_validation_success_count);
  } else {
    IncrementStat(
        1, &session_stats_,
        &session_stats::path_validation_failure_count);
  }

  // Only emit the callback if there is a handler for the pathValidation
  // event on the JavaScript QuicSession object.
  if (LIKELY(state_[IDX_QUIC_SESSION_STATE_PATH_VALIDATED_ENABLED] == 0))
    return;

  Listener()->OnPathValidation(
      res,
      reinterpret_cast<const sockaddr*>(path->local.addr),
      reinterpret_cast<const sockaddr*>(path->remote.addr));
}

// Calling Ping will trigger the ngtcp2_conn to serialize any
// packets it currently has pending along with a probe frame
// that should keep the connection alive. This is a fire and
// forget and any errors that may occur will be ignored. The
// idle_timeout and retransmit timers will be updated. If Ping
// is called while processing an ngtcp2 callback, or if the
// closing or draining period has started, this is a non-op.
void QuicSession::Ping() {
  if (Ngtcp2CallbackScope::InNgtcp2CallbackScope(this) ||
      IsFlagSet(QUICSESSION_FLAG_DESTROYED) ||
      IsFlagSet(QUICSESSION_FLAG_CLOSING) ||
      IsInClosingPeriod() ||
      IsInDrainingPeriod()) {
    return;
  }
  // TODO(@jasnell): We might want to revisit whether to handle
  // errors right here. For now, we're ignoring them with the
  // intent of capturing them elsewhere.
  WritePackets("ping");
  UpdateIdleTimer();
  ScheduleRetransmit();
}

// A Retry will effectively restart the TLS handshake process
// by generating new initial crypto material. This should only ever
// be called on client sessions
bool QuicSession::ReceiveRetry() {
  CHECK(!IsServer());
  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED))
    return false;
  Debug(this, "A retry packet was received. Restarting the handshake.");
  IncrementStat(1, &session_stats_, &session_stats::retry_count);
  return DeriveAndInstallInitialKey(
    this,
    ngtcp2_conn_get_dcid(Connection()));
}

bool QuicSession::Receive(
    ssize_t nread,
    const uint8_t* data,
    const SocketAddress& local_addr,
    const struct sockaddr* remote_addr,
    unsigned int flags) {
  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED)) {
    Debug(this, "Ignoring packet because session is destroyed");
    return false;
  }

  Debug(this, "Receiving QUIC packet.");
  IncrementStat(nread, &session_stats_, &session_stats::bytes_received);

  // Closing period starts once ngtcp2 has detected that the session
  // is being shutdown locally. Note that this is different that the
  // IsFlagSet(QUICSESSION_FLAG_GRACEFUL_CLOSING) function, which
  // indicates a graceful shutdown that allows the session and streams
  // to finish naturally. When IsInClosingPeriod is true, ngtcp2 is
  // actively in the process of shutting down the connection and a
  // CONNECTION_CLOSE has already been sent. The only thing we can do
  // at this point is either ignore the packet or send another
  // CONNECTION_CLOSE.
  if (IsInClosingPeriod()) {
    Debug(this, "QUIC packet received while in closing period.");
    IncrementConnectionCloseAttempts();
    if (!ShouldAttemptConnectionClose()) {
      Debug(this, "Not sending connection close");
      return false;
    }
    Debug(this, "Sending connection close");
    return SendConnectionClose();
  }

  // When IsInDrainingPeriod is true, ngtcp2 has received a
  // connection close and we are simply discarding received packets.
  // No outbound packets may be sent. Return true here because
  // the packet was correctly processed, even tho it is being
  // ignored.
  if (IsInDrainingPeriod()) {
    Debug(this, "QUIC packet received while in draining period.");
    return true;
  }

  // It's possible for the remote address to change from one
  // packet to the next so we have to look at the addr on
  // every packet.
  remote_address_ = remote_addr;
  QuicPath path(local_addr, remote_address_);

  {
    // These are within a scope to ensure that the InternalCallbackScope
    // and HandleScope are both exited before continuing on with the
    // function. This allows any nextTicks and queued tasks to be processed
    // before we continue.
    auto update_stats = OnScopeLeave([&](){
      UpdateDataStats();
    });
    Debug(this, "Processing received packet");
    HandleScope handle_scope(env()->isolate());
    InternalCallbackScope callback_scope(this);
    if (!ReceivePacket(&path, data, nread)) {
      if (initial_connection_close_ == NGTCP2_NO_ERROR) {
        Debug(this, "Failure processing received packet (code %" PRIu64 ")",
              GetLastError().code);
        HandleError();
        return false;
      } else {
        // When initial_connection_close_ is some value other than
        // NGTCP2_NO_ERROR, then the QuicSession is going to be
        // immediately responded to with a CONNECTION_CLOSE and
        // no additional processing will be performed.
        Debug(this, "Initial connection close with code %" PRIu64,
              initial_connection_close_);
        SetLastError(QUIC_ERROR_SESSION, initial_connection_close_);
        SendConnectionClose();
        return true;
      }
    }
  }

  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED)) {
    Debug(this, "Session was destroyed while processing the received packet");
    // If the QuicSession has been destroyed but it is not
    // in the closing period, a CONNECTION_CLOSE has not yet
    // been sent to the peer. Let's attempt to send one.
    if (!IsInClosingPeriod() && !IsInDrainingPeriod()) {
      Debug(this, "Attempting to send connection close");
      SetLastError();
      SendConnectionClose();
    }
    return true;
  }

  // Only send pending data if we haven't entered draining mode.
  // We enter the draining period when a CONNECTION_CLOSE has been
  // received from the remote peer.
  if (IsInDrainingPeriod()) {
    Debug(this, "In draining period after processing packet");
    // If processing the packet puts us into draining period, there's
    // absolutely nothing left for us to do except silently close
    // and destroy this QuicSession.
    GetConnectionCloseInfo();
    SilentClose();
    return true;
  } else {
    Debug(this, "Sending pending data after processing packet");
    SendPendingData();
  }

  UpdateIdleTimer();
  UpdateRecoveryStats();
  Debug(this, "Successfully processed received packet");
  return true;
}

// The ReceiveClientInitial function is called by ngtcp2 when
// a new connection has been initiated. The very first step to
// establishing a communication channel is to setup the keys
// that will be used to secure the communication.
bool QuicSession::ReceiveClientInitial(const ngtcp2_cid* dcid) {
  if (UNLIKELY(IsFlagSet(QUICSESSION_FLAG_DESTROYED)))
    return false;
  Debug(this, "Receiving client initial parameters.");
  return DeriveAndInstallInitialKey(this, dcid) &&
         initial_connection_close_ == NGTCP2_NO_ERROR;
}

// Captures the error code and family information from a received
// connection close frame.
void QuicSession::GetConnectionCloseInfo() {
  ngtcp2_connection_close_error_code close_code;
  ngtcp2_conn_get_connection_close_error_code(Connection(), &close_code);
  SetLastError(QuicError(close_code));
}

bool QuicSession::ReceivePacket(
    ngtcp2_path* path,
    const uint8_t* data,
    ssize_t nread) {
  DCHECK(!Ngtcp2CallbackScope::InNgtcp2CallbackScope(this));

  // If the QuicSession has been destroyed, we're not going
  // to process any more packets for it.
  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED))
    return true;

  uint64_t now = uv_hrtime();
  session_stats_.session_received_at = now;
  int err = ngtcp2_conn_read_pkt(Connection(), path, data, nread, now);
  if (err < 0) {
    switch (err) {
      case NGTCP2_ERR_DRAINING:
      case NGTCP2_ERR_RECV_VERSION_NEGOTIATION:
        break;
      default:
        // Per ngtcp2: If NGTCP2_ERR_RETRY is returned,
        // QuicSession must be a server and must perform
        // address validation by sending a Retry packet
        // then immediately close the connection.
        if (err == NGTCP2_ERR_RETRY && IsServer()) {
          // TODO(@jasnell): Need to verify proper ordering of
          // scid and rcid here
          Socket()->SendRetry(
              GetNegotiatedVersion(),
              QuicCID(scid()),
              QuicCID(rcid()),
              remote_address_.data());
          ImmediateClose();
          break;
        }
        SetLastError(QUIC_ERROR_SESSION, err);
        return false;
    }
  }
  return true;
}

// Called by ngtcp2 when a chunk of stream data has been received. If
// the stream does not yet exist, it is created, then the data is
// forwarded on.
bool QuicSession::ReceiveStreamData(
    int64_t stream_id,
    int fin,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) {
  auto leave = OnScopeLeave([&]() {
    // This extends the flow control window for the entire session
    // but not for the individual Stream. Stream flow control is
    // only expanded as data is read on the JavaScript side.
    ExtendOffset(datalen);
  });

  // QUIC does not permit zero-length stream packets if
  // fin is not set. ngtcp2 prevents these from coming
  // through but just in case of regression in that impl,
  // let's double check and simply ignore such packets
  // so we do not commit any resources.
  if (UNLIKELY(fin == 0 && datalen == 0))
    return true;

  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED))
    return false;

  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  // From here, we defer to the QuicApplication specific processing logic
  return application_->ReceiveStreamData(stream_id, fin, data, datalen, offset);
}

// Removes the given connection id from the QuicSession.
void QuicSession::RemoveConnectionID(const ngtcp2_cid* cid) {
  if (!IsFlagSet(QUICSESSION_FLAG_DESTROYED))
    DisassociateCID(cid);
}

// Removes the QuicSession from the current socket. This is
// done with when the session is being destroyed or being
// migrated to another QuicSocket. It is important to keep in mind
// that the QuicSocket uses a BaseObjectPtr for the QuicSession.
// If the session is removed and there are no other references held,
// the session object will be destroyed automatically.
void QuicSession::RemoveFromSocket() {
  if (IsServer()) {
    socket_->DisassociateCID(QuicCID(rcid_));

    if (pscid_.datalen > 0)
      socket_->DisassociateCID(QuicCID(pscid_));
  }

  std::vector<ngtcp2_cid> cids(ngtcp2_conn_get_num_scid(Connection()));
  ngtcp2_conn_get_scid(Connection(), cids.data());

  for (auto &cid : cids)
    socket_->DisassociateCID(QuicCID(&cid));

  Debug(this, "Removed from the QuicSocket.");
  socket_->RemoveSession(QuicCID(scid_), remote_address_);
  socket_.reset();
}

// Removes the given stream from the QuicSession. All streams must
// be removed before the QuicSession is destroyed.
void QuicSession::RemoveStream(int64_t stream_id) {
  Debug(this, "Removing stream %" PRId64, stream_id);

  // This will have the side effect of destroying the QuicStream
  // instance.
  streams_.erase(stream_id);
  // Ensure that the stream state is closed and discarded by ngtcp2
  // Be sure to call this after removing the stream from the map
  // above so that when ngtcp2 closes the stream, the callback does
  // not attempt to loop back around and destroy the already removed
  // QuicStream instance. Typically, the stream is already going to
  // be closed by this point.
  ngtcp2_conn_shutdown_stream(Connection(), stream_id, NGTCP2_NO_ERROR);
}

// Schedule the retransmission timer
void QuicSession::ScheduleRetransmit() {
  uint64_t now = uv_hrtime();
  uint64_t expiry = ngtcp2_conn_get_expiry(Connection());
  uint64_t interval = (expiry - now) / 1000000UL;
  if (expiry < now || interval == 0) interval = 1;
  Debug(this, "Scheduling the retransmit timer for %" PRIu64, interval);
  UpdateRetransmitTimer(interval);
}

void QuicSession::UpdateRetransmitTimer(uint64_t timeout) {
  DCHECK_NOT_NULL(retransmit_);
  retransmit_->Update(timeout);
}

// Transmits either a protocol or application connection
// close to the peer. The choice of which is send is
// based on the current value of last_error_.
bool QuicSession::SendConnectionClose() {
  CHECK(!Ngtcp2CallbackScope::InNgtcp2CallbackScope(this));

  // Do not send any frames at all if we're in the draining period
  // or in the middle of a silent close
  if (IsInDrainingPeriod() || IsFlagSet(QUICSESSION_FLAG_SILENT_CLOSE))
    return true;

  switch (crypto_context_->Side()) {
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      // If we're not already in the closing period,
      // first attempt to write any pending packets, then
      // start the closing period. If closing period has
      // already started, skip this.
      if (!IsInClosingPeriod() &&
          (!WritePackets("server connection close - write packets") ||
          !StartClosingPeriod())) {
          return false;
      }

      UpdateIdleTimer();
      CHECK_GT(conn_closebuf_->length(), 0);

      return SendPacket(QuicPacket::Copy(conn_closebuf_));
    }
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      UpdateIdleTimer();
      auto packet = QuicPacket::Create("client connection close", max_pktlen_);
      QuicError error = GetLastError();

      // If we're not already in the closing period,
      // first attempt to write any pending packets, then
      // start the closing period. Note that the behavior
      // here is different than the server
      if (!IsInClosingPeriod() &&
        !WritePackets("client connection close - write packets"))
        return false;

      ssize_t nwrite =
          SelectCloseFn(error.family)(
            Connection(),
            nullptr,
            packet->data(),
            max_pktlen_,
            error.code,
            uv_hrtime());
      if (nwrite < 0) {
        Debug(this, "Error writing connection close: %d", nwrite);
        SetLastError(QUIC_ERROR_SESSION, static_cast<int>(nwrite));
        return false;
      }
      packet->SetLength(nwrite);
      return SendPacket(std::move(packet));
    }
    default:
      UNREACHABLE();
  }
}

bool QuicSession::SelectPreferredAddress(
    ngtcp2_addr* dest,
    const ngtcp2_preferred_addr* paddr) {
  if (IsServer())  // This should never happen, but handle anyway
    return true;

  switch (select_preferred_address_policy_) {
    case QUIC_PREFERRED_ADDRESS_ACCEPT: {
      SocketAddress* local_address = Socket()->GetLocalAddress();
      uv_getaddrinfo_t req;

      if (!ResolvePreferredAddress(
              env(), local_address->GetFamily(),
              paddr, &req)) {
        return false;
      }

      if (req.addrinfo == nullptr)
        return false;

      dest->addrlen = req.addrinfo->ai_addrlen;
      memcpy(dest->addr, req.addrinfo->ai_addr, req.addrinfo->ai_addrlen);
      uv_freeaddrinfo(req.addrinfo);
      break;
    }
    case QUIC_PREFERRED_ADDRESS_IGNORE:
      // Explicitly do nothing in this case.
      break;
  }
  return true;
}

bool QuicSession::SendPacket(
  std::unique_ptr<QuicPacket> packet,
  const ngtcp2_path_storage& path) {
  UpdateEndpoint(path.path);
  return SendPacket(std::move(packet));
}

// Sends buffered stream data.
bool QuicSession::SendStreamData(QuicStream* stream) {
  // Because SendStreamData calls ngtcp2_conn_writev_streams,
  // it is not permitted to be called while we are running within
  // an ngtcp2 callback function.
  CHECK(!Ngtcp2CallbackScope::InNgtcp2CallbackScope(this));

  // No stream data may be serialized and sent if:
  //   - the QuicSession is destroyed
  //   - the QuicStream was never writable,
  //   - a final stream frame has already been sent,
  //   - the QuicSession is in the draining period,
  //   - the QuicSession is in the closing period, or
  //   - we are blocked from sending any data because of flow control
  if (IsDestroyed() ||
      !stream->WasEverWritable() ||
      stream->HasSentFin() ||
      IsInDrainingPeriod() ||
      IsInClosingPeriod() ||
      GetMaxDataLeft() == 0) {
    return true;
  }

  return application_->SendStreamData(stream);
}

bool QuicSession::SendPacket(std::unique_ptr<QuicPacket> packet) {
  CHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));
  CHECK(!IsInDrainingPeriod());

  // There's nothing to send.
  if (packet->length() == 0)
    return true;

  IncrementStat(
      packet->length(),
      &session_stats_,
      &session_stats::bytes_sent);
  session_stats_.session_sent_at = uv_hrtime();
  ScheduleRetransmit();

  Debug(this, "Sending %" PRIu64 " bytes to %s:%d from %s:%d",
        packet->length(),
        remote_address_.GetAddress().c_str(),
        remote_address_.GetPort(),
        local_address_.GetAddress().c_str(),
        local_address_.GetPort());

  int err = Socket()->SendPacket(
      local_address_,
      remote_address_,
      std::move(packet),
      BaseObjectPtr<QuicSession>(this));

  if (err != 0) {
    SetLastError(QUIC_ERROR_SESSION, err);
    return false;
  }

  return true;
}

// Sends any pending handshake or session packet data.
void QuicSession::SendPendingData() {
  // Do not proceed if:
  //  * We are in the ngtcp2 callback scope
  //  * The QuicSession has been destroyed
  //  * The QuicSession is in the draining period
  //  * The QuicSession is a server in the closing period
  if (Ngtcp2CallbackScope::InNgtcp2CallbackScope(this) ||
      IsFlagSet(QUICSESSION_FLAG_DESTROYED) ||
      IsInDrainingPeriod() ||
      (IsServer() && IsInClosingPeriod())) {
    return;
  }

  if (!application_->SendPendingData()) {
    Debug(this, "Error sending QUIC application data");
    HandleError();
  }

  // Otherwise, serialize and send any packets waiting in the queue.
  if (!WritePackets("pending session data - write packets")) {
    Debug(this, "Error writing pending packets");
    HandleError();
  }
}

// When resuming a client session, the serialized transport parameters from
// the prior session must be provided. This is set during construction
// of the client QuicSession object.
bool QuicSession::SetEarlyTransportParams(Local<Value> buffer) {
  CHECK(!IsServer());
  ArrayBufferViewContents<uint8_t> sbuf(buffer.As<ArrayBufferView>());
  ngtcp2_transport_params params;
  if (sbuf.length() != sizeof(ngtcp2_transport_params))
    return false;
  memcpy(&params, sbuf.data(), sizeof(ngtcp2_transport_params));
  ngtcp2_conn_set_early_remote_transport_params(Connection(), &params);
  return true;
}

void QuicSession::SetLocalAddress(const ngtcp2_addr* addr) {
  DCHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));
  ngtcp2_conn_set_local_addr(Connection(), addr);
}

// Set the transport parameters received from the remote peer
int QuicSession::SetRemoteTransportParams(ngtcp2_transport_params* params) {
  DCHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));
  transport_params_ = *params;
  SetFlag(QUICSESSION_FLAG_HAS_TRANSPORT_PARAMS);
  return ngtcp2_conn_set_remote_transport_params(Connection(), params);
}


int QuicSession::SetSession(SSL_SESSION* session) {
  CHECK(!IsServer());
  CHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));

  int size = i2d_SSL_SESSION(session, nullptr);
  if (size > SecureContext::kMaxSessionSize)
    return 0;
  Listener()->OnSessionTicket(size, session);
  return 1;
}

// When resuming a client session, the serialized session ticket from
// the prior session must be provided. This is set during construction
// of the client QuicSession object.
bool QuicSession::SetSession(Local<Value> buffer) {
  CHECK(!IsServer());
  ArrayBufferViewContents<unsigned char> sbuf(buffer.As<ArrayBufferView>());
  return crypto_context_->SetSession(sbuf.data(), sbuf.length());
}

bool QuicSession::SetSocket(QuicSocket* socket, bool nat_rebinding) {
  CHECK(!IsServer());
  CHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));
  CHECK(!IsFlagSet(QUICSESSION_FLAG_GRACEFUL_CLOSING));
  if (socket == nullptr || socket == socket_.get())
    return true;

  // Step 1: Add this Session to the given Socket
  AddToSocket(socket);

  // Step 2: Remove this Session from the current Socket
  RemoveFromSocket();

  // Step 3: Update the internal references
  socket_.reset(socket);
  socket->ReceiveStart();

  // Step 4: Update ngtcp2
  SocketAddress* local_address = socket->GetLocalAddress();
  if (nat_rebinding) {
    ngtcp2_addr addr;
    ToNgtcp2Addr(local_address, &addr);
    ngtcp2_conn_set_local_addr(Connection(), &addr);
  } else {
    QuicPath path(local_address, &remote_address_);
    if (ngtcp2_conn_initiate_migration(
            Connection(),
            &path,
            uv_hrtime()) != 0) {
      return false;
    }
  }

  SendPendingData();
  return true;
}

void QuicSession::ResetStream(int64_t stream_id, uint64_t code) {
  // First, update the internal ngtcp2 state of the given stream
  // and schedule the STOP_SENDING and RESET_STREAM frames as
  // appropriate.
  CHECK_EQ(
      ngtcp2_conn_shutdown_stream(
          Connection(),
          stream_id,
          code), 0);

  // If ShutdownStream is called outside of an ngtcp2 callback,
  // we need to trigger SendPendingData manually to cause the
  // RESET_STREAM and STOP_SENDING frames to be transmitted.
  if (!Ngtcp2CallbackScope::InNgtcp2CallbackScope(this))
    SendPendingData();
}

// Silent Close must start with the JavaScript side, which must
// clean up state, abort any still existing QuicSessions, then
// destroy the handle when done. The most important characteristic
// of the SilentClose is that no frames are sent to the peer.
//
// When a valid stateless reset is received, the connection is
// immediately and unrecoverably closed at the ngtcp2 level.
// Specifically, it will be put into the draining_period so
// absolutely no frames can be sent. What we need to do is
// notify the JavaScript side and destroy the connection with
// a flag set that indicates stateless reset.
void QuicSession::SilentClose(bool stateless_reset) {
  // Calling either ImmediateClose or SilentClose will cause
  // the QUICSESSION_FLAG_CLOSING to be set. In either case,
  // we should never re-enter ImmediateClose or SilentClose.
  CHECK(!IsFlagSet(QUICSESSION_FLAG_CLOSING));
  SetFlag(QUICSESSION_FLAG_SILENT_CLOSE);
  SetFlag(QUICSESSION_FLAG_CLOSING);

  QuicError last_error = GetLastError();
  Debug(this,
        "Silent close with %s code %" PRIu64 " (stateless reset? %s)",
        last_error.GetFamilyName(),
        last_error.code,
        stateless_reset ? "yes" : "no");

  Listener()->OnSessionSilentClose(stateless_reset, last_error);
}

bool QuicSession::StartClosingPeriod() {
  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED))
    return false;
  if (IsInClosingPeriod())
    return true;

  StopRetransmitTimer();
  UpdateIdleTimer();

  QuicError error = GetLastError();
  Debug(this, "Closing period has started. Error %d", error.code);

  // Once the CONNECTION_CLOSE packet is written,
  // IsInClosingPeriod will return true.
  conn_closebuf_ =
      std::move(QuicPacket::Create("close connection", max_pktlen_));
  ssize_t nwrite =
      SelectCloseFn(error.family)(
          Connection(),
          nullptr,
          conn_closebuf_->data(),
          max_pktlen_,
          error.code,
          uv_hrtime());
  if (nwrite < 0) {
    if (nwrite == NGTCP2_ERR_PKT_NUM_EXHAUSTED) {
      SetLastError(QUIC_ERROR_SESSION, NGTCP2_ERR_PKT_NUM_EXHAUSTED);
      SilentClose();
    } else {
      SetLastError(QUIC_ERROR_SESSION, static_cast<int>(nwrite));
    }
    return false;
  }
  conn_closebuf_->SetLength(nwrite);
  return true;
}

// Called by ngtcp2 when a stream has been closed. If the stream does
// not exist, the close is ignored.
void QuicSession::StreamClose(int64_t stream_id, uint64_t app_error_code) {
  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED))
    return;

  Debug(this, "Closing stream %" PRId64 " with code %" PRIu64,
        stream_id,
        app_error_code);

  // If the stream does not actually exist, just ignore
  if (HasStream(stream_id))
    application_->StreamClose(stream_id, app_error_code);
}

void QuicSession::StopIdleTimer() {
  CHECK_NOT_NULL(idle_);
  idle_->Stop();
}

void QuicSession::StopRetransmitTimer() {
  CHECK_NOT_NULL(retransmit_);
  retransmit_->Stop();
}

// Called by ngtcp2 when a stream has been opened. All we do is log
// the activity here. We do not want to actually commit any resources
// until data is received for the stream. This allows us to prevent
// a stream commitment attack. The only exception is shutting the
// stream down explicitly if we are in a graceful close period.
void QuicSession::StreamOpen(int64_t stream_id) {
  if (IsFlagSet(QUICSESSION_FLAG_GRACEFUL_CLOSING)) {
    ngtcp2_conn_shutdown_stream(
        Connection(),
        stream_id,
        NGTCP2_ERR_CLOSING);
  }
  Debug(this, "Stream %" PRId64 " opened", stream_id);
  return application_->StreamOpen(stream_id);
}

// Called when the QuicSession has received a RESET_STREAM frame from the
// peer, indicating that it will no longer send additional frames for the
// stream. If the stream is not yet known, reset is ignored. If the stream
// has already received a STREAM frame with fin set, the stream reset is
// ignored (the QUIC spec permits implementations to handle this situation
// however they want.) If the stream has not yet received a STREAM frame
// with the fin set, then the RESET_STREAM causes the readable side of the
// stream to be abruptly closed and any additional stream frames that may
// be received will be discarded if their offset is greater than final_size.
// On the JavaScript side, receiving a C is undistinguishable from
// a normal end-of-stream. No additional data events will be emitted, the
// end event will be emitted, and the readable side of the duplex will be
// closed.
//
// If the stream is still writable, no additional action is taken. If,
// however, the writable side of the stream has been closed (or was never
// open in the first place as in the case of peer-initiated unidirectional
// streams), the reset will cause the stream to be immediately destroyed.
void QuicSession::StreamReset(
    int64_t stream_id,
    uint64_t final_size,
    uint64_t app_error_code) {
  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED))
    return;

  Debug(this,
        "Reset stream %" PRId64 " with code %" PRIu64
        " and final size %" PRIu64,
        stream_id,
        app_error_code,
        final_size);

  if (HasStream(stream_id))
    application_->StreamReset(stream_id, final_size, app_error_code);
}

void QuicSession::UpdateIdleTimer() {
  CHECK_NOT_NULL(idle_);
  uint64_t now = uv_hrtime();
  uint64_t expiry = ngtcp2_conn_get_idle_expiry(Connection());
  uint64_t timeout = expiry > now ? (expiry - now) / 1000 : 1;
  if (timeout == 0) timeout = 1;
  Debug(this, "Updating idle timeout to %" PRIu64, timeout);
  idle_->Update(timeout);
}

void QuicSession::VersionNegotiation(
      const ngtcp2_pkt_hd* hd,
      const uint32_t* sv,
      size_t nsv) {
  CHECK(!IsServer());
  if (IsFlagSet(QUICSESSION_FLAG_DESTROYED))
    return;
  Listener()->OnVersionNegotiation(NGTCP2_PROTO_VER, sv, nsv);
}

// Write any packets current pending for the ngtcp2 connection based on
// the current state of the QuicSession. If the QuicSession is in the
// closing period, only CONNECTION_CLOSE packets may be written. If the
// QuicSession is in the draining period, no packets may be written.
//
// Packets are flushed to the underlying QuicSocket uv_udp_t as soon
// as they are written. The WritePackets method may cause zero or more
// packets to be serialized.
//
// If there are any acks or retransmissions pending, those will be
// serialized at this point as well. However, WritePackets does not
// serialize stream data that is being sent initially.
bool QuicSession::WritePackets(const char* diagnostic_label) {
  CHECK(!Ngtcp2CallbackScope::InNgtcp2CallbackScope(this));
  CHECK(!IsFlagSet(QUICSESSION_FLAG_DESTROYED));

  // During the draining period, we must not send any frames at all.
  if (IsInDrainingPeriod())
    return true;

  // During the closing period, we are only permitted to send
  // CONNECTION_CLOSE frames.
  if (IsInClosingPeriod()) {
    return SendConnectionClose();
  }

  // Otherwise, serialize and send pending frames
  QuicPathStorage path;
  for (;;) {
    auto packet = QuicPacket::Create(diagnostic_label, max_pktlen_);
    ssize_t nwrite =
        ngtcp2_conn_write_pkt(
            Connection(),
            &path.path,
            packet->data(),
            max_pktlen_,
            uv_hrtime());
    if (nwrite <= 0) {
      switch (nwrite) {
        case 0:
          return true;
        case NGTCP2_ERR_PKT_NUM_EXHAUSTED:
          // There is a finite number of packets that can be sent
          // per connection. Once those are exhausted, there's
          // absolutely nothing we can do except immediately
          // and silently tear down the QuicSession. This has
          // to be silent because we can't even send a
          // CONNECTION_CLOSE since even those require a
          // packet number.
          SilentClose();
          return false;
        default:
          SetLastError(QUIC_ERROR_SESSION, static_cast<int>(nwrite));
          return false;
      }
    }

    packet->SetLength(nwrite);
    UpdateEndpoint(path.path);
    UpdateDataStats();

    if (!SendPacket(std::move(packet)))
      return false;
  }
}

void QuicSession::UpdateEndpoint(const ngtcp2_path& path) {
  remote_address_.Update(path.remote.addr, path.remote.addrlen);
  local_address_.Update(path.local.addr, path.local.addrlen);
}

bool QuicSession::SubmitInformation(
    int64_t stream_id,
    v8::Local<v8::Array> headers) {
  return application_->SubmitInformation(stream_id, headers);
}

bool QuicSession::SubmitHeaders(
    int64_t stream_id,
    v8::Local<v8::Array> headers,
    uint32_t flags) {
  return application_->SubmitHeaders(stream_id, headers, flags);
}

bool QuicSession::SubmitTrailers(
    int64_t stream_id,
    v8::Local<v8::Array> headers) {
  return application_->SubmitTrailers(stream_id, headers);
}

void QuicSession::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("crypto_context", crypto_context_.get());
  tracker->TrackField("alpn", alpn_);
  tracker->TrackField("hostname", hostname_);
  tracker->TrackField("idle", idle_);
  tracker->TrackField("retransmit", retransmit_);
  tracker->TrackField("streams", streams_);
  tracker->TrackField("state", state_);
  tracker->TrackField("crypto_rx_ack", crypto_rx_ack_);
  tracker->TrackField("crypto_handshake_rate", crypto_handshake_rate_);
  tracker->TrackField("stats_buffer", stats_buffer_);
  tracker->TrackField("recovery_stats_buffer", recovery_stats_buffer_);
  tracker->TrackFieldWithSize("current_ngtcp2_memory", current_ngtcp2_memory_);
  tracker->TrackField("conn_closebuf", conn_closebuf_);
  tracker->TrackField("application", application_);
}

QuicSession::InitialPacketResult QuicSession::Accept(
    ngtcp2_pkt_hd* hd,
    uint32_t version,
    const uint8_t* data,
    ssize_t nread) {
  // The initial packet is too short and not a valid QUIC packet.
  if (static_cast<size_t>(nread) < MIN_INITIAL_QUIC_PKT_SIZE)
    return PACKET_IGNORE;

  switch (ngtcp2_accept(hd, data, nread)) {
    case -1:
      return PACKET_IGNORE;
    case 1:
      return PACKET_VERSION;
  }

  if (hd->type == NGTCP2_PKT_0RTT)
    return PACKET_RETRY;

  // Currently, we only understand one version of the QUIC
  // protocol, but that could change in the future. If it
  // does change, the following check needs to be updated
  // to check against a range of possible versions.
  // See NGTCP2_PROTO_VER and NGTCP2_PROTO_VER_MAX in the
  // ngtcp2.h header file for more details.
  if (version != NGTCP2_PROTO_VER)
    return PACKET_VERSION;

  return PACKET_OK;
}

BaseObjectPtr<QuicSession> QuicSession::CreateServer(
    QuicSocket* socket,
    QuicSessionConfig* config,
    const ngtcp2_cid* rcid,
    const SocketAddress& local_addr,
    const struct sockaddr* remote_addr,
    const ngtcp2_cid* dcid,
    const ngtcp2_cid* ocid,
    uint32_t version,
    const std::string& alpn,
    uint32_t options,
    uint64_t initial_connection_close,
    QlogMode qlog) {
  Local<Object> obj;
  if (!socket->env()
             ->quicserversession_constructor_template()
             ->NewInstance(socket->env()->context()).ToLocal(&obj)) {
    return {};
  }
  BaseObjectPtr<QuicSession> session =
      MakeDetachedBaseObject<QuicSession>(
          socket,
          config,
          obj,
          rcid,
          local_addr,
          remote_addr,
          dcid,
          ocid,
          version,
          alpn,
          options,
          initial_connection_close,
          qlog);

  session->AddToSocket(socket);
  return session;
}

void QuicSession::InitServer(
    QuicSessionConfig* config,
    const SocketAddress& local_addr,
    const struct sockaddr* remote_addr,
    const ngtcp2_cid* dcid,
    const ngtcp2_cid* ocid,
    uint32_t version,
    QlogMode qlog) {

  CHECK_NULL(connection_);

  ExtendMaxStreamsBidi(DEFAULT_MAX_STREAMS_BIDI);
  ExtendMaxStreamsUni(DEFAULT_MAX_STREAMS_UNI);

  local_address_ = local_addr;
  remote_address_ = remote_addr;
  max_pktlen_ = GetMaxPktLen(remote_addr);

  config->SetOriginalConnectionID(ocid);
  config->GenerateStatelessResetToken();
  config->GeneratePreferredAddressToken(pscid());
  max_crypto_buffer_ = config->GetMaxCryptoBuffer();

  EntropySource(scid_.data, NGTCP2_SV_SCIDLEN);
  scid_.datalen = NGTCP2_SV_SCIDLEN;

  QuicPath path(local_addr, remote_address_);

  // NOLINTNEXTLINE(readability/pointer_notation)
  if (qlog == QlogMode::kEnabled) config->SetQlog({ *ocid, OnQlogWrite });

  ngtcp2_conn* conn;
  CHECK_EQ(
      ngtcp2_conn_server_new(
          &conn,
          dcid,
          &scid_,
          &path,
          version,
          &callbacks[crypto_context_->Side()],
          config->data(),
          &alloc_info_,
          static_cast<QuicSession*>(this)), 0);

  connection_.reset(conn);

  InitializeTLS(this);
  UpdateDataStats();
  UpdateIdleTimer();
}

void QuicSessionOnClientHelloDone(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->CryptoContext()->OnClientHelloDone();
}

// This callback is invoked by user code after completing handling
// of the 'OCSPRequest' event. The callback is invoked with two
// possible arguments, both of which are optional
//   1. A replacement SecureContext
//   2. An OCSP response
void QuicSessionOnCertDone(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());

  Local<FunctionTemplate> cons = env->secure_context_constructor_template();
  crypto::SecureContext* context = nullptr;
  if (args[0]->IsObject() && cons->HasInstance(args[0]))
    context = Unwrap<crypto::SecureContext>(args[0].As<Object>());
  session->CryptoContext()->OnOCSPDone(context, args[1]);
}

void QuicSession::UpdateRecoveryStats() {
  const ngtcp2_rcvry_stat* stat =
      ngtcp2_conn_get_rcvry_stat(Connection());
  recovery_stats_.min_rtt = static_cast<double>(stat->min_rtt);
  recovery_stats_.latest_rtt = static_cast<double>(stat->latest_rtt);
  recovery_stats_.smoothed_rtt = static_cast<double>(stat->smoothed_rtt);
}

void QuicSession::UpdateDataStats() {
  state_[IDX_QUIC_SESSION_STATE_MAX_DATA_LEFT] =
    static_cast<double>(ngtcp2_conn_get_max_data_left(Connection()));
  size_t bytes_in_flight = ngtcp2_conn_get_bytes_in_flight(Connection());
  state_[IDX_QUIC_SESSION_STATE_BYTES_IN_FLIGHT] =
    static_cast<double>(bytes_in_flight);
  if (bytes_in_flight > session_stats_.max_bytes_in_flight)
    session_stats_.max_bytes_in_flight = bytes_in_flight;
}

BaseObjectPtr<QuicSession> QuicSession::CreateClient(
    QuicSocket* socket,
    const struct sockaddr* addr,
    SecureContext* context,
    Local<Value> early_transport_params,
    Local<Value> session_ticket,
    Local<Value> dcid,
    SelectPreferredAddressPolicy select_preferred_address_policy,
    const std::string& alpn,
    const std::string& hostname,
    uint32_t options,
    QlogMode qlog) {
  Local<Object> obj;
  if (!socket->env()
             ->quicclientsession_constructor_template()
             ->NewInstance(socket->env()->context()).ToLocal(&obj)) {
    return {};
  }

  BaseObjectPtr<QuicSession> session =
      MakeDetachedBaseObject<QuicSession>(
          socket,
          obj,
          *socket->GetLocalAddress(),
          addr,
          context,
          early_transport_params,
          session_ticket,
          dcid,
          select_preferred_address_policy,
          alpn,
          hostname,
          options,
          qlog);

  session->AddToSocket(socket);
  return session;
}

bool QuicSession::InitClient(
    const SocketAddress& local_addr,
    const struct sockaddr* remote_addr,
    Local<Value> early_transport_params,
    Local<Value> session_ticket,
    Local<Value> dcid_value,
    QlogMode qlog) {
  CHECK_NULL(connection_);

  local_address_ = local_addr;
  remote_address_ = remote_addr;
  Debug(this, "Initializing connection from %s:%d to %s:%d",
        local_address_.GetAddress().c_str(),
        local_address_.GetPort(),
        remote_address_.GetAddress().c_str(),
        remote_address_.GetPort());
  max_pktlen_ = GetMaxPktLen(remote_addr);

  QuicSessionConfig config(env());
  max_crypto_buffer_ = config.GetMaxCryptoBuffer();
  ExtendMaxStreamsBidi(DEFAULT_MAX_STREAMS_BIDI);
  ExtendMaxStreamsUni(DEFAULT_MAX_STREAMS_UNI);

  scid_.datalen = NGTCP2_MAX_CIDLEN;
  EntropySource(scid_.data, scid_.datalen);

  ngtcp2_cid dcid;
  if (dcid_value->IsArrayBufferView()) {
    ArrayBufferViewContents<uint8_t> sbuf(
        dcid_value.As<ArrayBufferView>());
    CHECK_LE(sbuf.length(), NGTCP2_MAX_CIDLEN);
    CHECK_GE(sbuf.length(), NGTCP2_MIN_CIDLEN);
    memcpy(dcid.data, sbuf.data(), sbuf.length());
    dcid.datalen = sbuf.length();
  } else {
    dcid.datalen = NGTCP2_MAX_CIDLEN;
    EntropySource(dcid.data, dcid.datalen);
  }

  QuicPath path(local_address_, remote_address_);

  if (qlog == QlogMode::kEnabled) config.SetQlog({ dcid, OnQlogWrite });

  ngtcp2_conn* conn;
  CHECK_EQ(
      ngtcp2_conn_client_new(
          &conn,
          &dcid,
          &scid_,
          &path,
          NGTCP2_PROTO_VER,
          &callbacks[crypto_context_->Side()],
          config.data(),
          &alloc_info_,
          static_cast<QuicSession*>(this)), 0);

  auto n = ngtcp2_conn_get_remote_addr(conn);

  connection_.reset(conn);

  InitializeTLS(this);

  CHECK(DeriveAndInstallInitialKey(
      this, ngtcp2_conn_get_dcid(Connection())));

  // Remote Transport Params
  if (early_transport_params->IsArrayBufferView()) {
    if (SetEarlyTransportParams(early_transport_params)) {
      Debug(this, "Using provided early transport params.");
      crypto_context_->SetOption(QUICCLIENTSESSION_OPTION_RESUME);
    } else {
      Debug(this, "Ignoring invalid early transport params.");
    }
  }

  // Session Ticket
  if (session_ticket->IsArrayBufferView()) {
    if (SetSession(session_ticket)) {
      Debug(this, "Using provided session ticket.");
      crypto_context_->SetOption(QUICCLIENTSESSION_OPTION_RESUME);
    } else {
      Debug(this, "Ignoring provided session ticket.");
    }
  }

  UpdateIdleTimer();
  UpdateDataStats();
  return true;
}

// Static ngtcp2 callbacks are registered when ngtcp2 when a new ngtcp2_conn is
// created. These are static functions that, for the most part, simply defer to
// a QuicSession instance that is passed through as user_data.

// Called by ngtcp2 upon creation of a new client connection
// to initiate the TLS handshake.
int QuicSession::OnClientInitial(
    ngtcp2_conn* conn,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return NGTCP2_OK(session->CryptoContext()->Receive(
      NGTCP2_CRYPTO_LEVEL_INITIAL,
      0, nullptr, 0)) ? 0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

// Called by ngtcp2 for a new server connection when the initial
// crypto handshake from the client has been received.
int QuicSession::OnReceiveClientInitial(
    ngtcp2_conn* conn,
    const ngtcp2_cid* dcid,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  if (!session->ReceiveClientInitial(dcid)) {
    Debug(session, "Receiving initial client handshake failed");
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}

// Called by ngtcp2 for both client and server connections when
// TLS handshake data has been received.
int QuicSession::OnReceiveCryptoData(
    ngtcp2_conn* conn,
    ngtcp2_crypto_level crypto_level,
    uint64_t offset,
    const uint8_t* data,
    size_t datalen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return static_cast<int>(
    session->CryptoContext()->Receive(crypto_level, offset, data, datalen));
}

// Called by ngtcp2 for a client connection when the server has
// sent a retry packet.
int QuicSession::OnReceiveRetry(
    ngtcp2_conn* conn,
    const ngtcp2_pkt_hd* hd,
    const ngtcp2_pkt_retry* retry,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  if (!session->ReceiveRetry()) {
    Debug(session, "Receiving retry token failed");
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}

// Called by ngtcp2 for both client and server connections
// when a request to extend the maximum number of bidirectional
// streams has been received.
int QuicSession::OnExtendMaxStreamsBidi(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamsBidi(max_streams);
  return 0;
}

// Called by ngtcp2 for both client and server connections
// when a request to extend the maximum number of unidirectional
// streams has been received
int QuicSession::OnExtendMaxStreamsUni(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamsUni(max_streams);
  return 0;
}

int QuicSession::OnExtendMaxStreamsRemoteUni(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamsRemoteUni(max_streams);
  return 0;
}

int QuicSession::OnExtendMaxStreamsRemoteBidi(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamsRemoteUni(max_streams);
  return 0;
}

int QuicSession::OnExtendMaxStreamData(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t max_data,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamData(stream_id, max_data);
  return 0;
}

// Called by ngtcp2 for both client and server connections
// when ngtcp2 has determined that the TLS handshake has
// been completed.
int QuicSession::OnHandshakeCompleted(
    ngtcp2_conn* conn,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->HandshakeCompleted();
  return 0;
}

int QuicSession::OnHPMask(
    ngtcp2_conn* conn,
    uint8_t* dest,
    const ngtcp2_crypto_cipher* hp,
    const uint8_t* key,
    const uint8_t* sample,
    void* user_data) {
  return ngtcp2_crypto_hp_mask(dest, hp, key, sample) == 0 ?
      0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

// Called by ngtcp2 when a chunk of stream data has been received.
int QuicSession::OnReceiveStreamData(
    ngtcp2_conn* conn,
    int64_t stream_id,
    int fin,
    uint64_t offset,
    const uint8_t* data,
    size_t datalen,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return session->ReceiveStreamData(stream_id, fin, data, datalen, offset) ?
      0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

// Called by ngtcp2 when a new stream has been opened
int QuicSession::OnStreamOpen(
    ngtcp2_conn* conn,
    int64_t stream_id,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  session->StreamOpen(stream_id);
  return 0;
}

// Called by ngtcp2 when an acknowledgement for a chunk of
// TLS handshake data has been received.
int QuicSession::OnAckedCryptoOffset(
    ngtcp2_conn* conn,
    ngtcp2_crypto_level crypto_level,
    uint64_t offset,
    size_t datalen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->CryptoContext()->AcknowledgeCryptoData(crypto_level, datalen);
  return 0;
}

// Called by ngtcp2 when an acknowledgement for a chunk of
// stream data has been received.
int QuicSession::OnAckedStreamDataOffset(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t offset,
    size_t datalen,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->AckedStreamDataOffset(stream_id, offset, datalen);
  return 0;
}

// Called by ngtcp2 for a client connection when the server
// has indicated a preferred address in the transport
// params.
// For now, there are two modes: we can accept the preferred address
// or we can reject it. Later, we may want to implement a callback
// to ask the user if they want to accept the preferred address or
// not.
int QuicSession::OnSelectPreferredAddress(
    ngtcp2_conn* conn,
    ngtcp2_addr* dest,
    const ngtcp2_preferred_addr* paddr,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  if (!session->SelectPreferredAddress(dest, paddr))
    Debug(session, "Selecting preferred address failed");
  return 0;
}

// Called by ngtcp2 when a stream has been closed for any reason.
int QuicSession::OnStreamClose(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t app_error_code,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->StreamClose(stream_id, app_error_code);
  return 0;
}

int QuicSession::OnStreamReset(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t final_size,
    uint64_t app_error_code,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->StreamReset(stream_id, final_size, app_error_code);
  return 0;
}

// Called by ngtcp2 when it needs to generate some random data
int QuicSession::OnRand(
    ngtcp2_conn* conn,
    uint8_t* dest,
    size_t destlen,
    ngtcp2_rand_ctx ctx,
    void* user_data) {
  EntropySource(dest, destlen);
  return 0;
}

// When a new client connection is established, ngtcp2 will call
// this multiple times to generate a pool of connection IDs to use.
int QuicSession::OnGetNewConnectionID(
    ngtcp2_conn* conn,
    ngtcp2_cid* cid,
    uint8_t* token,
    size_t cidlen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->GetNewConnectionID(cid, token, cidlen);
  return 0;
}

// Called by ngtcp2 to trigger a key update for the connection.
int QuicSession::OnUpdateKey(
    ngtcp2_conn* conn,
    uint8_t* rx_key,
    uint8_t* rx_iv,
    uint8_t* tx_key,
    uint8_t* tx_iv,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  if (!session->CryptoContext()->KeyUpdate(rx_key, rx_iv, tx_key, tx_iv)) {
    Debug(session, "Updating the key failed");
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}

// When a connection is closed, ngtcp2 will call this multiple
// times to remove connection IDs.
int QuicSession::OnRemoveConnectionID(
    ngtcp2_conn* conn,
    const ngtcp2_cid* cid,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->RemoveConnectionID(cid);
  return 0;
}

// Called by ngtcp2 to perform path validation. Path validation
// is necessary to ensure that a packet is originating from the
// expected source.
int QuicSession::OnPathValidation(
    ngtcp2_conn* conn,
    const ngtcp2_path* path,
    ngtcp2_path_validation_result res,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->PathValidation(path, res);
  return 0;
}

int QuicSession::OnVersionNegotiation(
    ngtcp2_conn* conn,
    const ngtcp2_pkt_hd* hd,
    const uint32_t* sv,
    size_t nsv,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->VersionNegotiation(hd, sv, nsv);
  return 0;
}

int QuicSession::OnStatelessReset(
    ngtcp2_conn* conn,
    const ngtcp2_pkt_stateless_reset* sr,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  session->SilentClose(true);
  return 0;
}

void QuicSession::OnQlogWrite(void* user_data, const void* data, size_t len) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  session->Listener()->OnQLog(reinterpret_cast<const uint8_t*>(data), len);
}

const ngtcp2_conn_callbacks QuicSession::callbacks[2] = {
  // NGTCP2_CRYPTO_SIDE_CLIENT
  {
    OnClientInitial,
    nullptr,
    OnReceiveCryptoData,
    OnHandshakeCompleted,
    OnVersionNegotiation,
    ngtcp2_crypto_encrypt_cb,
    ngtcp2_crypto_decrypt_cb,
    OnHPMask,
    OnReceiveStreamData,
    OnAckedCryptoOffset,
    OnAckedStreamDataOffset,
    OnStreamOpen,
    OnStreamClose,
    OnStatelessReset,
    OnReceiveRetry,
    OnExtendMaxStreamsBidi,
    OnExtendMaxStreamsUni,
    OnRand,
    OnGetNewConnectionID,
    OnRemoveConnectionID,
    OnUpdateKey,
    OnPathValidation,
    OnSelectPreferredAddress,
    OnStreamReset,
    OnExtendMaxStreamsRemoteBidi,
    OnExtendMaxStreamsRemoteUni,
    OnExtendMaxStreamData
  },
  // NGTCP2_CRYPTO_SIDE_SERVER
  {
    nullptr,
    OnReceiveClientInitial,
    OnReceiveCryptoData,
    OnHandshakeCompleted,
    nullptr,  // recv_version_negotiation
    ngtcp2_crypto_encrypt_cb,
    ngtcp2_crypto_decrypt_cb,
    OnHPMask,
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
    OnUpdateKey,
    OnPathValidation,
    nullptr,  // select_preferred_addr
    OnStreamReset,
    OnExtendMaxStreamsRemoteBidi,
    OnExtendMaxStreamsRemoteUni,
    OnExtendMaxStreamData
  }
};

// JavaScript API

namespace {
void QuicSessionSetSocket(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  CHECK(args[0]->IsObject());
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args[0].As<Object>());
  args.GetReturnValue().Set(session->SetSocket(socket));
}

// Perform an immediate close on the QuicSession, causing a
// CONNECTION_CLOSE frame to be scheduled and sent and starting
// the closing period for this session. The name "ImmediateClose"
// is a bit of an unfortunate misnomer as the session will not
// be immediately shutdown. The naming is pulled from the QUIC
// spec to indicate a state where the session immediately enters
// the closing period, but the session will not be destroyed
// until either the idle timeout fires or destroy is explicitly
// called.
void QuicSessionClose(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->SetLastError(QuicError(env, args[0], args[1]));
  session->SendConnectionClose();
}

// GracefulClose flips a flag that prevents new local streams
// from being opened and new remote streams from being received. It is
// important to note that this does *NOT* send a CONNECTION_CLOSE packet
// to the peer. Existing streams are permitted to close gracefully.
void QuicSessionGracefulClose(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->StartGracefulClose();
}

// Destroying the QuicSession will trigger sending of a CONNECTION_CLOSE
// packet, after which the QuicSession will be immediately torn down.
void QuicSessionDestroy(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->SetLastError(QuicError(env, args[0], args[1]));
  session->Destroy();
}

void QuicSessionGetEphemeralKeyInfo(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  return args.GetReturnValue().Set(
      crypto::GetEphemeralKey(
          session->env(),
          session->CryptoContext()->ssl()));
}

void QuicSessionGetPeerCertificate(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  args.GetReturnValue().Set(
      crypto::GetPeerCert(
          session->env(),
          session->CryptoContext()->ssl(),
          !args[0]->IsTrue(),
          session->IsServer()));
}

void QuicSessionGetRemoteAddress(
    const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Environment* env = session->env();
  CHECK(args[0]->IsObject());
  args.GetReturnValue().Set(
      session->GetRemoteAddress().ToJS(env, args[0].As<Object>()));
}

void QuicSessionGetCertificate(
    const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  args.GetReturnValue().Set(
      crypto::GetCert(
          session->env(),
          session->CryptoContext()->ssl()));
}

void QuicSessionPing(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->Ping();
}

// TODO(addaleax): This is a temporary solution for testing and should be
// removed later.
void QuicSessionRemoveFromSocket(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->RemoveFromSocket();
}

void QuicSessionUpdateKey(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  args.GetReturnValue().Set(session->CryptoContext()->InitiateKeyUpdate());
}

void NewQuicClientSession(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsObject());
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args[0].As<Object>());

  node::Utf8Value address(args.GetIsolate(), args[2]);
  int32_t family;
  uint32_t port;
  uint32_t flags;
  if (!args[1]->Int32Value(env->context()).To(&family) ||
      !args[3]->Uint32Value(env->context()).To(&port) ||
      !args[4]->Uint32Value(env->context()).To(&flags))
    return;

  // Secure Context
  CHECK(args[5]->IsObject());
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args[5].As<Object>());

  // SNI Servername
  node::Utf8Value servername(args.GetIsolate(), args[6]);
  std::string hostname(*servername);

  sockaddr_storage addr;
  if (SocketAddress::ToSockAddr(family, *address, port, &addr) == nullptr)
    return args.GetReturnValue().Set(-1);

  int select_preferred_address_policy = QUIC_PREFERRED_ADDRESS_IGNORE;
  if (!args[10]->Int32Value(env->context())
      .To(&select_preferred_address_policy)) return;

  std::string alpn(NGTCP2_ALPN_H3);
  if (args[11]->IsString()) {
    Utf8Value val(env->isolate(), args[11]);
    alpn = val.length();
    alpn += *val;
  }

  uint32_t options = QUICCLIENTSESSION_OPTION_VERIFY_HOSTNAME_IDENTITY;
  if (!args[12]->Uint32Value(env->context()).To(&options)) return;

  socket->ReceiveStart();

  BaseObjectPtr<QuicSession> session =
      QuicSession::CreateClient(
          socket,
          const_cast<const sockaddr*>(reinterpret_cast<sockaddr*>(&addr)),
          sc,
          args[7],
          args[8],
          args[9],
          static_cast<SelectPreferredAddressPolicy>
              (select_preferred_address_policy),
          alpn,
          hostname,
          options,
          args[13]->IsTrue() ? QlogMode::kEnabled : QlogMode::kDisabled);

  session->SendPendingData();
  args.GetReturnValue().Set(session->object());
}

// Add methods that are shared by both client and server QuicSessions
void AddMethods(Environment* env, Local<FunctionTemplate> session) {
  env->SetProtoMethod(session, "close", QuicSessionClose);
  env->SetProtoMethod(session, "destroy", QuicSessionDestroy);
  env->SetProtoMethod(session, "getRemoteAddress", QuicSessionGetRemoteAddress);
  env->SetProtoMethod(session, "getCertificate", QuicSessionGetCertificate);
  env->SetProtoMethod(session, "getPeerCertificate",
                      QuicSessionGetPeerCertificate);
  env->SetProtoMethod(session, "gracefulClose", QuicSessionGracefulClose);
  env->SetProtoMethod(session, "updateKey", QuicSessionUpdateKey);
  env->SetProtoMethod(session, "ping", QuicSessionPing);
  env->SetProtoMethod(session, "removeFromSocket", QuicSessionRemoveFromSocket);
  env->SetProtoMethod(session, "onClientHelloDone",
                      QuicSessionOnClientHelloDone);
  env->SetProtoMethod(session, "onCertDone", QuicSessionOnCertDone);
}
}  // namespace

void QuicSession::Initialize(
    Environment* env,
    Local<Object> target,
    Local<Context> context) {
  {
    Local<String> class_name =
        FIXED_ONE_BYTE_STRING(env->isolate(), "QuicServerSession");
    Local<FunctionTemplate> session = FunctionTemplate::New(env->isolate());
    session->SetClassName(class_name);
    session->Inherit(AsyncWrap::GetConstructorTemplate(env));
    Local<ObjectTemplate> sessiont = session->InstanceTemplate();
    sessiont->SetInternalFieldCount(1);
    sessiont->Set(env->owner_symbol(), Null(env->isolate()));
    AddMethods(env, session);
    env->set_quicserversession_constructor_template(sessiont);
  }

  {
    Local<String> class_name =
        FIXED_ONE_BYTE_STRING(env->isolate(), "QuicClientSession");
    Local<FunctionTemplate> session = FunctionTemplate::New(env->isolate());
    session->SetClassName(class_name);
    session->Inherit(AsyncWrap::GetConstructorTemplate(env));
    Local<ObjectTemplate> sessiont = session->InstanceTemplate();
    sessiont->SetInternalFieldCount(1);
    sessiont->Set(env->owner_symbol(), Null(env->isolate()));
    AddMethods(env, session);
    env->SetProtoMethod(session,
                        "getEphemeralKeyInfo",
                        QuicSessionGetEphemeralKeyInfo);
    env->SetProtoMethod(session,
                        "setSocket",
                        QuicSessionSetSocket);
    env->set_quicclientsession_constructor_template(sessiont);

    env->SetMethod(target, "createClientSession", NewQuicClientSession);
  }
}

}  // namespace quic
}  // namespace node
