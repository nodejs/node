#include "node_quic_session-inl.h"  // NOLINT(build/include)
#include "aliased_buffer.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_crypto_common.h"
#include "ngtcp2/ngtcp2.h"
#include "ngtcp2/ngtcp2_crypto.h"
#include "ngtcp2/ngtcp2_crypto_openssl.h"
#include "node.h"
#include "node_buffer.h"
#include "node_crypto.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_http_common-inl.h"
#include "node_mem-inl.h"
#include "node_process.h"
#include "node_quic_buffer-inl.h"
#include "node_quic_crypto.h"
#include "node_quic_socket-inl.h"
#include "node_quic_stream-inl.h"
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

namespace {
void SetConfig(Environment* env, int idx, uint64_t* val) {
  AliasedFloat64Array& buffer = env->quic_state()->quicsessionconfig_buffer;
  uint64_t flags = static_cast<uint64_t>(buffer[IDX_QUIC_SESSION_CONFIG_COUNT]);
  if (flags & (1ULL << idx))
    *val = static_cast<uint64_t>(buffer[idx]);
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

void CopyPreferredAddress(
    uint8_t* dest,
    size_t destlen,
    uint16_t* port,
    const sockaddr* addr) {
  const sockaddr_in* src = reinterpret_cast<const sockaddr_in*>(addr);
  memcpy(dest, &src->sin_addr, destlen);
  *port = SocketAddress::GetPort(addr);
}

}  // namespace

std::string QuicSession::RemoteTransportParamsDebug::ToString() const {
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
         std::to_string(params.max_packet_size) + "\n";

  if (!session->is_server()) {
    if (params.original_connection_id_present) {
      QuicCID cid(params.original_connection_id);
      out += "  Original Connection ID: " + cid.ToString() + "\n";
    } else {
      out += "  Original Connection ID: N/A \n";
    }

    if (params.preferred_address_present) {
      out += "  Preferred Address Present: Yes\n";
      // TODO(@jasnell): Serialize the IPv4 and IPv6 address options
    } else {
      out += "  Preferred Address Present: No\n";
    }

    if (params.stateless_reset_token_present) {
      StatelessResetToken token(params.stateless_reset_token);
      out += "  Stateless Reset Token: " + token.ToString() + "\n";
    } else {
      out += " Stateless Reset Token: N/A";
    }
  }
  return out;
}

void QuicSessionConfig::ResetToDefaults(Environment* env) {
  ngtcp2_settings_default(this);
  initial_ts = uv_hrtime();
  // Detailed(verbose) logging provided by ngtcp2 is only enabled
  // when the NODE_DEBUG_NATIVE=NGTCP2_DEBUG category is used.
  if (UNLIKELY(env->enabled_debug_list()->enabled(
          DebugCategory::NGTCP2_DEBUG))) {
    log_printf = Ngtcp2DebugLog;
  }
  transport_params.active_connection_id_limit =
      DEFAULT_ACTIVE_CONNECTION_ID_LIMIT;
  transport_params.initial_max_stream_data_bidi_local =
      DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL;
  transport_params.initial_max_stream_data_bidi_remote =
      DEFAULT_MAX_STREAM_DATA_BIDI_REMOTE;
  transport_params.initial_max_stream_data_uni =
      DEFAULT_MAX_STREAM_DATA_UNI;
  transport_params.initial_max_streams_bidi =
      DEFAULT_MAX_STREAMS_BIDI;
  transport_params.initial_max_streams_uni =
      DEFAULT_MAX_STREAMS_UNI;
  transport_params.initial_max_data = DEFAULT_MAX_DATA;
  transport_params.max_idle_timeout = DEFAULT_MAX_IDLE_TIMEOUT;
  transport_params.max_packet_size =
      NGTCP2_MAX_PKT_SIZE;
  transport_params.max_ack_delay =
      NGTCP2_DEFAULT_MAX_ACK_DELAY;
  transport_params.disable_active_migration = 0;
  transport_params.preferred_address_present = 0;
  transport_params.stateless_reset_token_present = 0;
}

// Sets the QuicSessionConfig using an AliasedBuffer for efficiency.
void QuicSessionConfig::Set(
    Environment* env,
    const sockaddr* preferred_addr) {
  ResetToDefaults(env);
  SetConfig(env, IDX_QUIC_SESSION_ACTIVE_CONNECTION_ID_LIMIT,
            &transport_params.active_connection_id_limit);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_LOCAL,
            &transport_params.initial_max_stream_data_bidi_local);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_REMOTE,
            &transport_params.initial_max_stream_data_bidi_remote);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAM_DATA_UNI,
            &transport_params.initial_max_stream_data_uni);
  SetConfig(env, IDX_QUIC_SESSION_MAX_DATA,
            &transport_params.initial_max_data);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAMS_BIDI,
            &transport_params.initial_max_streams_bidi);
  SetConfig(env, IDX_QUIC_SESSION_MAX_STREAMS_UNI,
            &transport_params.initial_max_streams_uni);
  SetConfig(env, IDX_QUIC_SESSION_MAX_IDLE_TIMEOUT,
            &transport_params.max_idle_timeout);
  SetConfig(env, IDX_QUIC_SESSION_MAX_PACKET_SIZE,
            &transport_params.max_packet_size);
  SetConfig(env, IDX_QUIC_SESSION_MAX_ACK_DELAY,
            &transport_params.max_ack_delay);

  transport_params.max_idle_timeout =
      transport_params.max_idle_timeout * 1000000000;

  // TODO(@jasnell): QUIC allows both IPv4 and IPv6 addresses to be
  // specified. Here we're specifying one or the other. Need to
  // determine if that's what we want or should we support both.
  if (preferred_addr != nullptr) {
    transport_params.preferred_address_present = 1;
    switch (preferred_addr->sa_family) {
      case AF_INET: {
        CopyPreferredAddress(
            transport_params.preferred_address.ipv4_addr,
            sizeof(transport_params.preferred_address.ipv4_addr),
            &transport_params.preferred_address.ipv4_port,
            preferred_addr);
        break;
      }
      case AF_INET6: {
        CopyPreferredAddress(
            transport_params.preferred_address.ipv6_addr,
            sizeof(transport_params.preferred_address.ipv6_addr),
            &transport_params.preferred_address.ipv6_port,
            preferred_addr);
        break;
      }
      default:
        UNREACHABLE();
    }
  }
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

QuicSessionListener::~QuicSessionListener() {
  if (session_)
    session_->RemoveListener(this);
}

void QuicSessionListener::OnCert(const char* server_name) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnCert(server_name);
}

void QuicSessionListener::OnOCSP(Local<Value> ocsp) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnOCSP(ocsp);
}

void QuicSessionListener::OnStreamHeaders(
    int64_t stream_id,
    int kind,
    const std::vector<std::unique_ptr<QuicHeader>>& headers,
    int64_t push_id) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnStreamHeaders(stream_id, kind, headers, push_id);
}

void QuicSessionListener::OnStreamClose(
    int64_t stream_id,
    uint64_t app_error_code) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnStreamClose(stream_id, app_error_code);
}

void QuicSessionListener::OnStreamReset(
    int64_t stream_id,
    uint64_t app_error_code) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnStreamReset(stream_id, app_error_code);
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

void QuicSessionListener::OnStreamBlocked(int64_t stream_id) {
  if (previous_listener_ != nullptr) {
    previous_listener_->OnStreamBlocked(stream_id);
  }
}

void QuicSessionListener::OnSessionSilentClose(
    bool stateless_reset,
    QuicError error) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnSessionSilentClose(stateless_reset, error);
}

void QuicSessionListener::OnUsePreferredAddress(
    int family,
    const QuicPreferredAddress& preferred_address) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnUsePreferredAddress(family, preferred_address);
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
  Environment* env = session()->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Value> line_bf = Buffer::Copy(env, line, 1 + len).ToLocalChecked();
  char* data = Buffer::Data(line_bf);
  data[len] = '\n';

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(env->quic_on_session_keylog_function(), 1, &line_bf);
}

void JSQuicSessionListener::OnStreamBlocked(int64_t stream_id) {
  Environment* env = session()->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  stream->MakeCallback(env->quic_on_stream_blocked_function(), 0, nullptr);
}

void JSQuicSessionListener::OnClientHello(
    const char* alpn,
    const char* server_name) {

  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Undefined(env->isolate()),
    Undefined(env->isolate()),
    session()->crypto_context()->hello_ciphers().ToLocalChecked()
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
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
      env->quic_on_session_client_hello_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnCert(const char* server_name) {
  Environment* env = session()->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> servername = Undefined(env->isolate());
  if (server_name != nullptr) {
    servername = OneByteString(
        env->isolate(),
        server_name,
        strlen(server_name));
  }

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(env->quic_on_session_cert_function(), 1, &servername);
}

void JSQuicSessionListener::OnStreamHeaders(
    int64_t stream_id,
    int kind,
    const std::vector<std::unique_ptr<QuicHeader>>& headers,
    int64_t push_id) {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  MaybeStackBuffer<Local<Value>, 16> head(headers.size());
  size_t n = 0;
  for (const auto& header : headers) {
    // name and value should never be empty here, and if
    // they are, there's an actual bug so go ahead and crash
    Local<Value> pair[] = {
      header->GetName(session()->application()).ToLocalChecked(),
      header->GetValue(session()->application()).ToLocalChecked()
    };
    head[n++] = Array::New(env->isolate(), pair, arraysize(pair));
  }
  Local<Value> argv[] = {
      Number::New(env->isolate(), static_cast<double>(stream_id)),
      Array::New(env->isolate(), head.out(), n),
      Integer::New(env->isolate(), kind),
      Undefined(env->isolate())
  };
  if (kind == QUICSTREAM_HEADERS_KIND_PUSH)
    argv[3] = Number::New(env->isolate(), static_cast<double>(push_id));
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
      env->quic_on_stream_headers_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnOCSP(Local<Value> ocsp) {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(env->quic_on_session_status_function(), 1, &ocsp);
}

void JSQuicSessionListener::OnStreamClose(
    int64_t stream_id,
    uint64_t app_error_code) {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Number::New(env->isolate(), static_cast<double>(stream_id)),
    Number::New(env->isolate(), static_cast<double>(app_error_code))
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
      env->quic_on_stream_close_function(),
      arraysize(argv),
      argv);
}

void JSQuicSessionListener::OnStreamReset(
    int64_t stream_id,
    uint64_t app_error_code) {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Number::New(env->isolate(), static_cast<double>(stream_id)),
    Number::New(env->isolate(), static_cast<double>(app_error_code))
  };
  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
      env->quic_on_stream_reset_function(),
      arraysize(argv),
      argv);
}

void JSQuicSessionListener::OnSessionDestroyed() {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  // Emit the 'close' event in JS. This needs to happen after destroying the
  // connection, because doing so also releases the last qlog data.
  session()->MakeCallback(
      env->quic_on_session_destroyed_function(), 0, nullptr);
}

void JSQuicSessionListener::OnSessionClose(QuicError error) {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Number::New(env->isolate(), static_cast<double>(error.code)),
    Integer::New(env->isolate(), error.family)
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
      env->quic_on_session_close_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnStreamReady(BaseObjectPtr<QuicStream> stream) {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Value> argv[] = {
    stream->object(),
    Number::New(env->isolate(), static_cast<double>(stream->id())),
    Number::New(env->isolate(), static_cast<double>(stream->push_id()))
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
      env->quic_on_stream_ready_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnHandshakeCompleted() {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  QuicCryptoContext* ctx = session()->crypto_context();
  Local<Value> servername = Undefined(env->isolate());
  const char* hostname = ctx->servername();
  if (hostname != nullptr) {
    servername =
        String::NewFromUtf8(
            env->isolate(),
            hostname,
            v8::NewStringType::kNormal).ToLocalChecked();
  }

  int err = ctx->VerifyPeerIdentity(
      hostname != nullptr ?
          hostname :
          session()->hostname().c_str());

  Local<Value> argv[] = {
    servername,
    GetALPNProtocol(*session()),
    ctx->cipher_name().ToLocalChecked(),
    ctx->cipher_version().ToLocalChecked(),
    Integer::New(env->isolate(), session()->max_pktlen_),
    crypto::GetValidationErrorReason(env, err).ToLocalChecked(),
    crypto::GetValidationErrorCode(env, err).ToLocalChecked(),
    session()->crypto_context()->early_data() ?
        v8::True(env->isolate()) :
        v8::False(env->isolate())
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
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
  Environment* env = session()->env();
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
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
      env->quic_on_session_path_validation_function(),
      arraysize(argv),
      argv);
}

void JSQuicSessionListener::OnSessionTicket(int size, SSL_SESSION* sess) {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    v8::Undefined(env->isolate()),
    v8::Undefined(env->isolate())
  };

  AllocatedBuffer session_ticket = env->AllocateManaged(size);
  unsigned char* session_data =
      reinterpret_cast<unsigned char*>(session_ticket.data());
  memset(session_data, 0, size);
  i2d_SSL_SESSION(sess, &session_data);
  if (!session_ticket.empty())
    argv[0] = session_ticket.ToBuffer().ToLocalChecked();

  if (session()->is_flag_set(
          QuicSession::QUICSESSION_FLAG_HAS_TRANSPORT_PARAMS)) {
    argv[1] = Buffer::Copy(
        env,
        reinterpret_cast<const char*>(&session()->transport_params_),
        sizeof(session()->transport_params_)).ToLocalChecked();
  }
  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
      env->quic_on_session_ticket_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnSessionSilentClose(
    bool stateless_reset,
    QuicError error) {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    stateless_reset ? v8::True(env->isolate()) : v8::False(env->isolate()),
    Number::New(env->isolate(), static_cast<double>(error.code)),
    Integer::New(env->isolate(), error.family)
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
      env->quic_on_session_silent_close_function(), arraysize(argv), argv);
}

void JSQuicSessionListener::OnUsePreferredAddress(
    int family,
    const QuicPreferredAddress& preferred_address) {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Local<Context> context = env->context();
  Context::Scope context_scope(context);

  std::string hostname = family == AF_INET ?
      preferred_address.preferred_ipv4_address():
      preferred_address.preferred_ipv6_address();
  int16_t port =
      family == AF_INET ?
          preferred_address.preferred_ipv4_port() :
          preferred_address.preferred_ipv6_port();

  Local<Value> argv[] = {
      String::NewFromOneByte(
          env->isolate(),
          reinterpret_cast<const uint8_t*>(hostname.c_str()),
          v8::NewStringType::kNormal).ToLocalChecked(),
      Integer::New(env->isolate(), port),
      Integer::New(env->isolate(), family)
  };

  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
      env->quic_on_session_use_preferred_address_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnVersionNegotiation(
    uint32_t supported_version,
    const uint32_t* vers,
    size_t vcnt) {
  Environment* env = session()->env();
  HandleScope scope(env->isolate());
  Local<Context> context = env->context();
  Context::Scope context_scope(context);

  MaybeStackBuffer<Local<Value>, 4> versions(vcnt);
  for (size_t n = 0; n < vcnt; n++)
    versions[n] = Integer::New(env->isolate(), vers[n]);

  // Currently, we only support one version of QUIC but in
  // the future that may change. The callback below passes
  // an array back to the JavaScript side to future-proof.
  Local<Value> supported =
      Integer::New(env->isolate(), supported_version);

  Local<Value> argv[] = {
    Integer::New(env->isolate(), NGTCP2_PROTO_VER),
    Array::New(env->isolate(), versions.out(), vcnt),
    Array::New(env->isolate(), &supported, 1)
  };

  // Grab a shared pointer to this to prevent the QuicSession
  // from being freed while the MakeCallback is running.
  BaseObjectPtr<QuicSession> ptr(session());
  session()->MakeCallback(
      env->quic_on_session_version_negotiation_function(),
      arraysize(argv), argv);
}

void JSQuicSessionListener::OnQLog(const uint8_t* data, size_t len) {
  Environment* env = session()->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> str =
      String::NewFromOneByte(env->isolate(),
                             data,
                             v8::NewStringType::kNormal,
                             len).ToLocalChecked();

  session()->MakeCallback(env->quic_on_session_qlog_function(), 1, &str);
}

// Generates a new random connection ID.
void QuicSession::RandomConnectionIDStrategy(
    QuicSession* session,
    ngtcp2_cid* cid,
    size_t cidlen) {
  // CID min and max length is determined by the QUIC specification.
  CHECK_LE(cidlen, NGTCP2_MAX_CIDLEN);
  CHECK_GE(cidlen, NGTCP2_MIN_CIDLEN);
  cid->datalen = cidlen;
  // cidlen shouldn't ever be zero here but just in case that
  // behavior changes in ngtcp2 in the future...
  if (LIKELY(cidlen > 0))
    EntropySource(cid->data, cidlen);
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


void QuicCryptoContext::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("initial_crypto", handshake_[0]);
  tracker->TrackField("handshake_crypto", handshake_[1]);
  tracker->TrackField("app_crypto", handshake_[2]);
  tracker->TrackField("ocsp_response", ocsp_response_);
}

bool QuicCryptoContext::SetSecrets(
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

  if (NGTCP2_ERR(ngtcp2_crypto_derive_and_install_key(
          session()->connection(),
          ssl_.get(),
          rx_key,
          rx_iv,
          rx_hp,
          tx_key,
          tx_iv,
          tx_hp,
          level,
          rx_secret,
          tx_secret,
          secretlen,
          side_))) {
    return false;
  }

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
  case NGTCP2_CRYPTO_LEVEL_APP:
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

void QuicCryptoContext::AcknowledgeCryptoData(
    ngtcp2_crypto_level level,
    size_t datalen) {
  // It is possible for the QuicSession to have been destroyed but not yet
  // deconstructed. In such cases, we want to ignore the callback as there
  // is nothing to do but wait for further cleanup to happen.
  if (UNLIKELY(session_->is_destroyed()))
    return;
  Debug(session(),
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
  session()->RecordAck(&QuicSessionStats::handshake_acked_at);
}

void QuicCryptoContext::EnableTrace() {
#if HAVE_SSL_TRACE
  if (!bio_trace_) {
    bio_trace_.reset(BIO_new_fp(stderr,  BIO_NOCLOSE | BIO_FP_TEXT));
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

  QuicCryptoContext* ctx = session_->crypto_context();
  session_->listener()->OnClientHello(
      ctx->hello_alpn(),
      ctx->hello_servername());

  // Returning -1 here will keep the TLS handshake paused until the
  // client hello callback is invoked. Returning 0 means that the
  // handshake is ready to proceed. When the OnClientHello callback
  // is called above, it may be resolved synchronously or asynchronously.
  // In case it is resolved synchronously, we need the check below.
  return in_client_hello_ ? -1 : 0;
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
    Debug(session(), "No OCSPRequest handler registered");
    return 1;
  }

  Debug(session(), "Client is requesting an OCSP Response");
  TLSCallbackScope callback_scope(this);

  // As in node_crypto.cc, this is not an error, but does suspend the
  // handshake to continue when OnOCSP is complete.
  if (in_ocsp_request_)
    return -1;
  in_ocsp_request_ = true;

  session_->listener()->OnCert(session_->crypto_context()->servername());

  // Returning -1 here means that we are still waiting for the OCSP
  // request to be completed. When the OnCert handler is invoked
  // above, it can be resolve synchronously or asynchonously. If
  // resolved synchronously, we need the check below.
  return in_ocsp_request_ ? -1 : 1;
}

// The OnCertDone function is called by the QuicSessionOnCertDone
// function when usercode is done handling the OCSPRequest event.
void QuicCryptoContext::OnOCSPDone(
    BaseObjectPtr<crypto::SecureContext> context,
    Local<Value> ocsp_response) {
  Debug(session(),
        "OCSPRequest completed. Context Provided? %s, OCSP Provided? %s",
        context ? "Yes" : "No",
        ocsp_response->IsArrayBufferView() ? "Yes" : "No");
  // Continue the TLS handshake when this function exits
  // otherwise it will stall and fail.
  TLSHandshakeScope handshake_scope(this, &in_ocsp_request_);

  // Disable the callback at this point so we don't loop continuously
  session_->state_[IDX_QUIC_SESSION_STATE_CERT_ENABLED] = 0;

  if (context) {
    int err = crypto::UseSNIContext(ssl_, context);
    if (!err) {
      unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
      return !err ?
          THROW_ERR_QUIC_FAILURE_SETTING_SNI_CONTEXT(session_->env()) :
          crypto::ThrowCryptoError(session_->env(), err);
    }
  }

  if (ocsp_response->IsArrayBufferView()) {
    ocsp_response_.Reset(
        session_->env()->isolate(),
        ocsp_response.As<ArrayBufferView>());
  }
}

// At this point in time, the TLS handshake secrets have been
// generated by openssl for this end of the connection and are
// ready to be used. Within this function, we need to install
// the secrets into the ngtcp2 connection object, store the
// remote transport parameters, and begin initialization of
// the QuicApplication that was selected.
bool QuicCryptoContext::OnSecrets(
    ngtcp2_crypto_level level,
    const uint8_t* rx_secret,
    const uint8_t* tx_secret,
    size_t secretlen) {

  auto maybe_init_app = OnScopeLeave([&]() {
    if (level == NGTCP2_CRYPTO_LEVEL_APP)
      session()->InitApplication();
  });

  Debug(session(),
        "Received secrets for %s crypto level",
        crypto_level_name(level));

  if (!SetSecrets(level, rx_secret, tx_secret, secretlen))
    return false;

  if (level == NGTCP2_CRYPTO_LEVEL_APP)
    session_->set_remote_transport_params();

  return true;
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
        Debug(session(), "There is no OCSP response");
        return SSL_TLSEXT_ERR_NOACK;
      }

      Local<ArrayBufferView> obj =
          PersistentToLocal::Default(
            env->isolate(),
            ocsp_response_);
      size_t len = obj->ByteLength();

      unsigned char* data = crypto::MallocOpenSSL<unsigned char>(len);
      obj->CopyContents(data, len);

      Debug(session(), "There is an OCSP response of %d bytes", len);

      if (!SSL_set_tlsext_status_ocsp_resp(ssl_.get(), data, len))
        OPENSSL_free(data);

      ocsp_response_.Reset();

      return SSL_TLSEXT_ERR_OK;
    }
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      Local<Value> res;
      if (ocsp_response().ToLocal(&res))
        session_->listener()->OnOCSP(res);
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
  if (UNLIKELY(session_->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  // Statistics are collected so we can monitor how long the
  // handshake is taking to operate and complete.
  if (session_->GetStat(&QuicSessionStats::handshake_start_at) == 0)
    session_->RecordTimestamp(&QuicSessionStats::handshake_start_at);
  session_->RecordTimestamp(&QuicSessionStats::handshake_continue_at);

  Debug(session(), "Receiving %d bytes of crypto data", datalen);

  // Internally, this passes the handshake data off to openssl
  // for processing. The handshake may or may not complete.
  int ret = ngtcp2_crypto_read_write_crypto_data(
      session_->connection(),
      ssl_.get(),
      crypto_level,
      data,
      datalen);
  switch (ret) {
    case 0:
      return 0;
    // In either of following cases, the handshake is being
    // paused waiting for user code to take action (for instance
    // OCSP requests or client hello modification)
    case NGTCP2_CRYPTO_ERR_TLS_WANT_X509_LOOKUP:
      Debug(session(), "TLS handshake wants X509 Lookup");
      return 0;
    case NGTCP2_CRYPTO_ERR_TLS_WANT_CLIENT_HELLO_CB:
      Debug(session(), "TLS handshake wants client hello callback");
      return 0;
    default:
      return ret;
  }
}

// Triggers key update to begin. This will fail and return false
// if either a previous key update is in progress and has not been
// confirmed or if the initial handshake has not yet been confirmed.
bool QuicCryptoContext::InitiateKeyUpdate() {
  if (UNLIKELY(session_->is_destroyed()))
    return false;

  // There's no user code that should be able to run while UpdateKey
  // is running, but we need to gate on it just to be safe.
  auto leave = OnScopeLeave([&]() { in_key_update_ = false; });
  CHECK(!in_key_update_);
  in_key_update_ = true;
  Debug(session(), "Initiating Key Update");

  session_->IncrementStat(&QuicSessionStats::keyupdate_count);

  return ngtcp2_conn_initiate_key_update(
      session_->connection(),
      uv_hrtime()) == 0;
}

int QuicCryptoContext::VerifyPeerIdentity(const char* hostname) {
  int err = crypto::VerifyPeerCertificate(ssl_);
  if (err)
    return err;

  // QUIC clients are required to verify the peer identity, servers are not.
  switch (side_) {
    case NGTCP2_CRYPTO_SIDE_CLIENT:
      if (LIKELY(is_option_set(
              QUICCLIENTSESSION_OPTION_VERIFY_HOSTNAME_IDENTITY))) {
        return VerifyHostnameIdentity(ssl_, hostname);
      }
      break;
    case NGTCP2_CRYPTO_SIDE_SERVER:
      // TODO(@jasnell): In the future, we may want to implement this but
      // for now we keep things simple and skip peer identity verification.
      break;
  }

  return 0;
}

// Write outbound TLS handshake data into the ngtcp2 connection
// to prepare it to be serialized. The outbound data must be
// stored in the handshake_ until it is acknowledged by the
// remote peer. It's important to keep in mind that there is
// a potential security risk here -- that is, a malicious peer
// can cause the local session to keep sent handshake data in
// memory by failing to acknowledge it or slowly acknowledging
// it. We currently do not track how much data is being buffered
// here but we do record statistics on how long the handshake
// data is foreced to be kept in memory.
void QuicCryptoContext::WriteHandshake(
    ngtcp2_crypto_level level,
    const uint8_t* data,
    size_t datalen) {
  Debug(session(),
        "Writing %d bytes of %s handshake data.",
        datalen,
        crypto_level_name(level));
  std::unique_ptr<QuicBufferChunk> buffer =
      std::make_unique<QuicBufferChunk>(datalen);
  memcpy(buffer->out(), data, datalen);
  session_->RecordTimestamp(&QuicSessionStats::handshake_send_at);
  CHECK_EQ(
      ngtcp2_conn_submit_crypto_data(
          session_->connection(),
          level,
          buffer->out(),
          datalen), 0);
  handshake_[level].Push(std::move(buffer));
}

void QuicApplication::Acknowledge(
    int64_t stream_id,
    uint64_t offset,
    size_t datalen) {
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  if (LIKELY(stream)) {
    stream->Acknowledge(offset, datalen);
    ResumeStream(stream_id);
  }
}

bool QuicApplication::SendPendingData() {
  // The maximum number of packets to send per call
  static constexpr size_t kMaxPackets = 16;
  QuicPathStorage path;
  std::unique_ptr<QuicPacket> packet;
  uint8_t* pos = nullptr;
  size_t packets_sent = 0;
  int err;

  for (;;) {
    ssize_t ndatalen;
    StreamData stream_data;
    err = GetStreamData(&stream_data);
    if (err < 0) {
      session()->set_last_error(QUIC_ERROR_APPLICATION, err);
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

    if (nwrite <= 0) {
      switch (nwrite) {
        case 0:
          goto congestion_limited;
        case NGTCP2_ERR_PKT_NUM_EXHAUSTED:
          // There is a finite number of packets that can be sent
          // per connection. Once those are exhausted, there's
          // absolutely nothing we can do except immediately
          // and silently tear down the QuicSession. This has
          // to be silent because we can't even send a
          // CONNECTION_CLOSE since even those require a
          // packet number.
          session()->SilentClose();
          return false;
        case NGTCP2_ERR_STREAM_DATA_BLOCKED:
          session()->StreamDataBlocked(stream_data.id);
          if (session()->max_data_left() == 0)
            goto congestion_limited;
          // Fall through
        case NGTCP2_ERR_STREAM_SHUT_WR:
          if (UNLIKELY(!BlockStream(stream_data.id)))
            return false;
          continue;
        case NGTCP2_ERR_STREAM_NOT_FOUND:
          continue;
        case NGTCP2_ERR_WRITE_STREAM_MORE:
          CHECK_GT(ndatalen, 0);
          CHECK(StreamCommit(&stream_data, ndatalen));
          pos += ndatalen;
          continue;
      }
      session()->set_last_error(QUIC_ERROR_SESSION, static_cast<int>(nwrite));
      return false;
    }

    pos += nwrite;

    if (ndatalen >= 0)
      CHECK(StreamCommit(&stream_data, ndatalen));

    Debug(session(), "Sending %" PRIu64 " bytes in serialized packet", nwrite);
    packet->set_length(nwrite);
    if (!session()->SendPacket(std::move(packet), path))
      return false;
    packet.reset();
    pos = nullptr;
    MaybeSetFin(stream_data);
    if (++packets_sent == kMaxPackets)
      break;
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

void QuicApplication::MaybeSetFin(const StreamData& stream_data) {
  if (ShouldSetFin(stream_data))
    set_stream_fin(stream_data.id);
}

void QuicApplication::StreamOpen(int64_t stream_id) {
  Debug(session(), "QUIC Stream %" PRId64 " is open", stream_id);
}

void QuicApplication::StreamHeaders(
    int64_t stream_id,
    int kind,
    const std::vector<std::unique_ptr<QuicHeader>>& headers,
    int64_t push_id) {
  session()->listener()->OnStreamHeaders(stream_id, kind, headers, push_id);
}

void QuicApplication::StreamClose(
    int64_t stream_id,
    uint64_t app_error_code) {
  session()->listener()->OnStreamClose(stream_id, app_error_code);
}

void QuicApplication::StreamReset(
    int64_t stream_id,
    uint64_t app_error_code) {
  session()->listener()->OnStreamReset(stream_id, app_error_code);
}

// Determines which QuicApplication variant the QuicSession will be using
// based on the alpn configured for the application. For now, this is
// determined through configuration when tghe QuicSession is created
// and is not negotiable. In the future, we may allow it to be negotiated.
QuicApplication* QuicSession::SelectApplication(QuicSession* session) {
  std::string alpn = session->alpn();
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
    const QuicSessionConfig& config,
    Local<Object> wrap,
    const QuicCID& rcid,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    const QuicCID& dcid,
    const QuicCID& ocid,
    uint32_t version,
    const std::string& alpn,
    uint32_t options,
    QlogMode qlog)
  : QuicSession(
        NGTCP2_CRYPTO_SIDE_SERVER,
        socket,
        wrap,
        socket->server_secure_context(),
        AsyncWrap::PROVIDER_QUICSERVERSESSION,
        alpn,
        std::string(""),  // empty hostname. not used on server side
        rcid,
        options,
        nullptr) {
  // The config is copied by assignment in the call below.
  InitServer(config, local_addr, remote_addr, dcid, ocid, version, qlog);
}

// Client QuicSession Constructor
QuicSession::QuicSession(
    QuicSocket* socket,
    v8::Local<v8::Object> wrap,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    BaseObjectPtr<SecureContext> secure_context,
    ngtcp2_transport_params* early_transport_params,
    crypto::SSLSessionPointer early_session_ticket,
    Local<Value> dcid,
    PreferredAddressStrategy preferred_address_strategy,
    const std::string& alpn,
    const std::string& hostname,
    uint32_t options,
    QlogMode qlog)
  : QuicSession(
        NGTCP2_CRYPTO_SIDE_CLIENT,
        socket,
        wrap,
        secure_context,
        AsyncWrap::PROVIDER_QUICCLIENTSESSION,
        alpn,
        hostname,
        QuicCID(),
        options,
        preferred_address_strategy) {
  InitClient(
      local_addr,
      remote_addr,
      early_transport_params,
      std::move(early_session_ticket),
      dcid,
      qlog);
}

// QuicSession is an abstract base class that defines the code used by both
// server and client sessions.
QuicSession::QuicSession(
    ngtcp2_crypto_side side,
    QuicSocket* socket,
    Local<Object> wrap,
    BaseObjectPtr<SecureContext> secure_context,
    AsyncWrap::ProviderType provider_type,
    const std::string& alpn,
    const std::string& hostname,
    const QuicCID& rcid,
    uint32_t options,
    PreferredAddressStrategy preferred_address_strategy)
  : AsyncWrap(socket->env(), wrap, provider_type),
    StatsBase(socket->env(), wrap,
              HistogramOptions::ACK |
              HistogramOptions::RATE),
    alloc_info_(MakeAllocator()),
    socket_(socket),
    alpn_(alpn),
    hostname_(hostname),
    idle_(new Timer(socket->env(), [this]() { OnIdleTimeout(); })),
    retransmit_(new Timer(socket->env(), [this]() { MaybeTimeout(); })),
    rcid_(rcid),
    state_(env()->isolate(), IDX_QUIC_SESSION_STATE_COUNT) {
  PushListener(&default_listener_);
  set_connection_id_strategy(RandomConnectionIDStrategy);
  set_preferred_address_strategy(preferred_address_strategy);
  crypto_context_.reset(
      new QuicCryptoContext(
          this,
          secure_context,
          side,
          options));
  application_.reset(SelectApplication(this));

  // TODO(@jasnell): For now, the following is a check rather than properly
  // handled. Before this code moves out of experimental, this should be
  // properly handled.
  wrap->DefineOwnProperty(
      env()->context(),
      env()->state_string(),
      state_.GetJSArray(),
      PropertyAttribute::ReadOnly).Check();

  // TODO(@jasnell): memory accounting
  // env_->isolate()->AdjustAmountOfExternalAllocatedMemory(kExternalSize);
}

QuicSession::~QuicSession() {
  CHECK(!Ngtcp2CallbackScope::InNgtcp2CallbackScope(this));
  crypto_context_->Cancel();
  connection_.reset();

  QuicSessionListener* listener_ = listener();
  listener_->OnSessionDestroyed();
  if (listener_ == listener())
    RemoveListener(listener_);

  DebugStats();
}

template <typename Fn>
void QuicSessionStatsTraits::ToString(const QuicSession& ptr, Fn&& add_field) {
#define V(_n, name, label)                                                     \
  add_field(label, ptr.GetStat(&QuicSessionStats::name));
  SESSION_STATS(V)
#undef V
}

void QuicSession::PushListener(QuicSessionListener* listener) {
  CHECK_NOT_NULL(listener);
  CHECK(!listener->session_);

  listener->previous_listener_ = listener_;
  listener->session_.reset(this);

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

  listener->session_.reset();
  listener->previous_listener_ = nullptr;
}

// The diagnostic_name is used in Debug output.
std::string QuicSession::diagnostic_name() const {
  return std::string("QuicSession ") +
      (is_server() ? "Server" : "Client") +
      " (" + alpn().substr(1) + ", " +
      std::to_string(static_cast<int64_t>(get_async_id())) + ")";
}

// Locate the QuicStream with the given id or return nullptr
BaseObjectPtr<QuicStream> QuicSession::FindStream(int64_t id) const {
  auto it = streams_.find(id);
  return it == std::end(streams_) ? BaseObjectPtr<QuicStream>() : it->second;
}

// Invoked when ngtcp2 receives an acknowledgement for stream data.
void QuicSession::AckedStreamDataOffset(
    int64_t stream_id,
    uint64_t offset,
    size_t datalen) {
  // It is possible for the QuicSession to have been destroyed but not yet
  // deconstructed. In such cases, we want to ignore the callback as there
  // is nothing to do but wait for further cleanup to happen.
  if (LIKELY(!is_flag_set(QUICSESSION_FLAG_DESTROYED))) {
    Debug(this,
          "Received acknowledgement for %" PRIu64
          " bytes of stream %" PRId64 " data",
          datalen, stream_id);

    application_->AcknowledgeStreamData(stream_id, offset, datalen);
  }
}

// Attaches the session to the given QuicSocket. The complexity
// here is that any CID's associated with the session have to
// be associated with the new QuicSocket.
void QuicSession::AddToSocket(QuicSocket* socket) {
  CHECK_NOT_NULL(socket);
  Debug(this, "Adding QuicSession to %s", socket->diagnostic_name());
  socket->AddSession(scid_, BaseObjectPtr<QuicSession>(this));
  switch (crypto_context_->side()) {
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      socket->AssociateCID(rcid_, scid_);
      socket->AssociateCID(pscid_, scid_);
      break;
    }
    case NGTCP2_CRYPTO_SIDE_CLIENT: {
      std::vector<ngtcp2_cid> cids(ngtcp2_conn_get_num_scid(connection()));
      ngtcp2_conn_get_scid(connection(), cids.data());
      for (const ngtcp2_cid& cid : cids) {
        socket->AssociateCID(QuicCID(&cid), scid_);
      }
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
      socket->AssociateStatelessResetToken(
          StatelessResetToken(token.token),
          BaseObjectPtr<QuicSession>(this));
    }
  }
}

// Add the given QuicStream to this QuicSession's collection of streams. All
// streams added must be removed before the QuicSession instance is freed.
void QuicSession::AddStream(BaseObjectPtr<QuicStream> stream) {
  DCHECK(!is_flag_set(QUICSESSION_FLAG_GRACEFUL_CLOSING));
  Debug(this, "Adding stream %" PRId64 " to session", stream->id());
  streams_.emplace(stream->id(), stream);

  // Update tracking statistics for the number of streams associated with
  // this session.
  switch (stream->origin()) {
    case QuicStreamOrigin::QUIC_STREAM_CLIENT:
      if (is_server())
        IncrementStat(&QuicSessionStats::streams_in_count);
      else
        IncrementStat(&QuicSessionStats::streams_out_count);
      break;
    case QuicStreamOrigin::QUIC_STREAM_SERVER:
      if (is_server())
        IncrementStat(&QuicSessionStats::streams_out_count);
      else
        IncrementStat(&QuicSessionStats::streams_in_count);
  }
  IncrementStat(&QuicSessionStats::streams_out_count);
  switch (stream->direction()) {
    case QuicStreamDirection::QUIC_STREAM_BIRECTIONAL:
      IncrementStat(&QuicSessionStats::bidi_stream_count);
      break;
    case QuicStreamDirection::QUIC_STREAM_UNIDIRECTIONAL:
      IncrementStat(&QuicSessionStats::uni_stream_count);
      break;
  }
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
  CHECK(!is_flag_set(QUICSESSION_FLAG_CLOSING));
  set_flag(QUICSESSION_FLAG_CLOSING);

  QuicError err = last_error();
  Debug(this, "Immediate close with code %" PRIu64 " (%s)",
        err.code,
        err.family_name());

  listener()->OnSessionClose(err);
}

// Creates a new stream object and passes it off to the javascript side.
// This has to be called from within a handlescope/contextscope.
BaseObjectPtr<QuicStream> QuicSession::CreateStream(int64_t stream_id) {
  CHECK(!is_flag_set(QUICSESSION_FLAG_DESTROYED));
  CHECK(!is_flag_set(QUICSESSION_FLAG_GRACEFUL_CLOSING));
  CHECK(!is_flag_set(QUICSESSION_FLAG_CLOSING));

  BaseObjectPtr<QuicStream> stream = QuicStream::New(this, stream_id);
  CHECK(stream);
  listener()->OnStreamReady(stream);
  return stream;
}

// Mark the QuicSession instance destroyed. After this is called,
// the QuicSession instance will be generally unusable but most
// likely will not be immediately freed.
void QuicSession::Destroy() {
  if (is_flag_set(QUICSESSION_FLAG_DESTROYED))
    return;

  // If we're not in the closing or draining periods,
  // then we should at least attempt to send a connection
  // close to the peer.
  if (!Ngtcp2CallbackScope::InNgtcp2CallbackScope(this) &&
      !is_in_closing_period() &&
      !is_in_draining_period()) {
    Debug(this, "Making attempt to send a connection close");
    set_last_error();
    SendConnectionClose();
  }

  // Streams should have already been destroyed by this point.
  CHECK(streams_.empty());

  // Mark the session destroyed.
  set_flag(QUICSESSION_FLAG_DESTROYED);
  set_flag(QUICSESSION_FLAG_CLOSING, false);
  set_flag(QUICSESSION_FLAG_GRACEFUL_CLOSING, false);

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

// Generates and associates a new connection ID for this QuicSession.
// ngtcp2 will call this multiple times at the start of a new connection
// in order to build a pool of available CIDs.
bool QuicSession::GetNewConnectionID(
    ngtcp2_cid* cid,
    uint8_t* token,
    size_t cidlen) {
  if (is_flag_set(QUICSESSION_FLAG_DESTROYED))
    return false;
  CHECK(!is_flag_set(QUICSESSION_FLAG_DESTROYED));
  CHECK_NOT_NULL(connection_id_strategy_);
  connection_id_strategy_(this, cid, cidlen);
  QuicCID cid_(cid);
  StatelessResetToken(token, socket()->session_reset_secret(), cid_);
  AssociateCID(cid_);
  return true;
}

void QuicSession::HandleError() {
  if (is_destroyed() || (is_in_closing_period() && !is_server()))
    return;

  if (!SendConnectionClose()) {
    set_last_error(QUIC_ERROR_SESSION, NGTCP2_ERR_INTERNAL);
    ImmediateClose();
  }
}

// The the retransmit libuv timer fires, it will call MaybeTimeout,
// which determines whether or not we need to retransmit data to
// to packet loss or ack delay.
void QuicSession::MaybeTimeout() {
  if (is_flag_set(QUICSESSION_FLAG_DESTROYED))
    return;
  uint64_t now = uv_hrtime();
  bool transmit = false;
  if (ngtcp2_conn_loss_detection_expiry(connection()) <= now) {
    Debug(this, "Retransmitting due to loss detection");
    CHECK_EQ(ngtcp2_conn_on_loss_detection_timer(connection(), now), 0);
    IncrementStat(&QuicSessionStats::loss_retransmit_count);
    transmit = true;
  } else if (ngtcp2_conn_ack_delay_expiry(connection()) <= now) {
    Debug(this, "Retransmitting due to ack delay");
    ngtcp2_conn_cancel_expired_ack_delay_timer(connection(), now);
    IncrementStat(&QuicSessionStats::ack_delay_retransmit_count);
    transmit = true;
  }
  if (transmit)
    SendPendingData();
}

bool QuicSession::OpenBidirectionalStream(int64_t* stream_id) {
  DCHECK(!is_flag_set(QUICSESSION_FLAG_DESTROYED));
  DCHECK(!is_flag_set(QUICSESSION_FLAG_CLOSING));
  DCHECK(!is_flag_set(QUICSESSION_FLAG_GRACEFUL_CLOSING));
  return ngtcp2_conn_open_bidi_stream(connection(), stream_id, nullptr) == 0;
}

bool QuicSession::OpenUnidirectionalStream(int64_t* stream_id) {
  DCHECK(!is_flag_set(QUICSESSION_FLAG_DESTROYED));
  DCHECK(!is_flag_set(QUICSESSION_FLAG_CLOSING));
  DCHECK(!is_flag_set(QUICSESSION_FLAG_GRACEFUL_CLOSING));
  if (ngtcp2_conn_open_uni_stream(connection(), stream_id, nullptr))
    return false;
  ngtcp2_conn_shutdown_stream_read(connection(), *stream_id, 0);
  return true;
}

// When ngtcp2 receives a successfull response to a PATH_CHALLENGE,
// it will trigger the OnPathValidation callback which will, in turn
// invoke this. There's really nothing to do here but update stats and
// and optionally notify the javascript side if there is a handler registered.
// Notifying the JavaScript side is purely informational.
void QuicSession::PathValidation(
    const ngtcp2_path* path,
    ngtcp2_path_validation_result res) {
  if (res == NGTCP2_PATH_VALIDATION_RESULT_SUCCESS) {
    IncrementStat(&QuicSessionStats::path_validation_success_count);
  } else {
    IncrementStat(&QuicSessionStats::path_validation_failure_count);
  }

  // Only emit the callback if there is a handler for the pathValidation
  // event on the JavaScript QuicSession object.
  if (UNLIKELY(state_[IDX_QUIC_SESSION_STATE_PATH_VALIDATED_ENABLED] == 1)) {
    listener_->OnPathValidation(
        res,
        reinterpret_cast<const sockaddr*>(path->local.addr),
        reinterpret_cast<const sockaddr*>(path->remote.addr));
  }
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
      is_flag_set(QUICSESSION_FLAG_DESTROYED) ||
      is_flag_set(QUICSESSION_FLAG_CLOSING) ||
      is_in_closing_period() ||
      is_in_draining_period()) {
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
  CHECK(!is_server());
  if (is_flag_set(QUICSESSION_FLAG_DESTROYED))
    return false;
  Debug(this, "A retry packet was received. Restarting the handshake");
  IncrementStat(&QuicSessionStats::retry_count);
  return DeriveAndInstallInitialKey(*this, dcid());
}

// When the QuicSocket receives a QUIC packet, it is forwarded on to here
// for processing.
bool QuicSession::Receive(
    ssize_t nread,
    const uint8_t* data,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    unsigned int flags) {
  if (is_flag_set(QUICSESSION_FLAG_DESTROYED)) {
    Debug(this, "Ignoring packet because session is destroyed");
    return false;
  }

  Debug(this, "Receiving QUIC packet");
  IncrementStat(&QuicSessionStats::bytes_received, nread);

  // Closing period starts once ngtcp2 has detected that the session
  // is being shutdown locally. Note that this is different that the
  // is_flag_set(QUICSESSION_FLAG_GRACEFUL_CLOSING) function, which
  // indicates a graceful shutdown that allows the session and streams
  // to finish naturally. When is_in_closing_period is true, ngtcp2 is
  // actively in the process of shutting down the connection and a
  // CONNECTION_CLOSE has already been sent. The only thing we can do
  // at this point is either ignore the packet or send another
  // CONNECTION_CLOSE.
  if (is_in_closing_period()) {
    Debug(this, "QUIC packet received while in closing period");
    IncrementConnectionCloseAttempts();
    // For server QuicSession instances, we serialize the connection close
    // packet once but may sent it multiple times. If the client keeps
    // transmitting, then the connection close may have gotten lost.
    // We don't want to send the connection close in response to
    // every received packet, however, so we use an exponential
    // backoff, increasing the ratio of packets received to connection
    // close frame sent with every one we send.
    if (!ShouldAttemptConnectionClose()) {
      Debug(this, "Not sending connection close");
      return false;
    }
    Debug(this, "Sending connection close");
    return SendConnectionClose();
  }

  // When is_in_draining_period is true, ngtcp2 has received a
  // connection close and we are simply discarding received packets.
  // No outbound packets may be sent. Return true here because
  // the packet was correctly processed, even tho it is being
  // ignored.
  if (is_in_draining_period()) {
    Debug(this, "QUIC packet received while in draining period");
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
      Debug(this, "Failure processing received packet (code %" PRIu64 ")",
            last_error().code);
      HandleError();
      return false;
    }
  }

  if (is_destroyed()) {
    Debug(this, "Session was destroyed while processing the received packet");
    // If the QuicSession has been destroyed but it is not
    // in the closing period, a CONNECTION_CLOSE has not yet
    // been sent to the peer. Let's attempt to send one.
    if (!is_in_closing_period() && !is_in_draining_period()) {
      Debug(this, "Attempting to send connection close");
      set_last_error();
      SendConnectionClose();
    }
    return true;
  }

  // Only send pending data if we haven't entered draining mode.
  // We enter the draining period when a CONNECTION_CLOSE has been
  // received from the remote peer.
  if (is_in_draining_period()) {
    Debug(this, "In draining period after processing packet");
    // If processing the packet puts us into draining period, there's
    // absolutely nothing left for us to do except silently close
    // and destroy this QuicSession.
    GetConnectionCloseInfo();
    SilentClose();
    return true;
  }
  Debug(this, "Sending pending data after processing packet");
  SendPendingData();
  UpdateIdleTimer();
  UpdateRecoveryStats();
  Debug(this, "Successfully processed received packet");
  return true;
}

// The ReceiveClientInitial function is called by ngtcp2 when
// a new connection has been initiated. The very first step to
// establishing a communication channel is to setup the keys
// that will be used to secure the communication.
bool QuicSession::ReceiveClientInitial(const QuicCID& dcid) {
  if (UNLIKELY(is_flag_set(QUICSESSION_FLAG_DESTROYED)))
    return false;
  Debug(this, "Receiving client initial parameters");
  crypto_context_->handshake_started();
  return DeriveAndInstallInitialKey(*this, dcid);
}

// Performs intake processing on a received QUIC packet. The received
// data is passed on to ngtcp2 for parsing and processing. ngtcp2 will,
// in turn, invoke a series of callbacks to handle the received packet.
bool QuicSession::ReceivePacket(
    ngtcp2_path* path,
    const uint8_t* data,
    ssize_t nread) {
  DCHECK(!Ngtcp2CallbackScope::InNgtcp2CallbackScope(this));

  // If the QuicSession has been destroyed, we're not going
  // to process any more packets for it.
  if (is_flag_set(QUICSESSION_FLAG_DESTROYED))
    return true;

  uint64_t now = uv_hrtime();
  SetStat(&QuicSessionStats::received_at, now);
  int err = ngtcp2_conn_read_pkt(connection(), path, data, nread, now);
  if (err < 0) {
    switch (err) {
      // In either of the following two cases, the caller will
      // handle the next steps...
      case NGTCP2_ERR_DRAINING:
      case NGTCP2_ERR_RECV_VERSION_NEGOTIATION:
        break;
      default:
        // Per ngtcp2: If NGTCP2_ERR_RETRY is returned,
        // QuicSession must be a server and must perform
        // address validation by sending a Retry packet
        // then immediately close the connection.
        if (err == NGTCP2_ERR_RETRY && is_server()) {
          socket()->SendRetry(scid_, rcid_, local_address_, remote_address_);
          ImmediateClose();
          break;
        }
        set_last_error(QUIC_ERROR_SESSION, err);
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
    // TODO(jasnell): The strategy for extending the flow control
    // window is going to be revisited soon. We don't need to
    // extend on every chunk of data.
    ExtendOffset(datalen);
  });

  // QUIC does not permit zero-length stream packets if
  // fin is not set. ngtcp2 prevents these from coming
  // through but just in case of regression in that impl,
  // let's double check and simply ignore such packets
  // so we do not commit any resources.
  if (UNLIKELY(fin == 0 && datalen == 0))
    return true;

  if (is_flag_set(QUICSESSION_FLAG_DESTROYED))
    return false;

  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  // From here, we defer to the QuicApplication specific processing logic
  return application_->ReceiveStreamData(stream_id, fin, data, datalen, offset);
}

// Removes the QuicSession from the current socket. This is
// done with when the session is being destroyed or being
// migrated to another QuicSocket. It is important to keep in mind
// that the QuicSocket uses a BaseObjectPtr for the QuicSession.
// If the session is removed and there are no other references held,
// the session object will be destroyed automatically.
void QuicSession::RemoveFromSocket() {
  CHECK(socket_);
  Debug(this, "Removing QuicSession from %s", socket_->diagnostic_name());
  if (is_server()) {
    socket_->DisassociateCID(rcid_);
    socket_->DisassociateCID(pscid_);
  }

  std::vector<ngtcp2_cid> cids(ngtcp2_conn_get_num_scid(connection()));
  std::vector<ngtcp2_cid_token> tokens(
      ngtcp2_conn_get_num_active_dcid(connection()));
  ngtcp2_conn_get_scid(connection(), cids.data());
  ngtcp2_conn_get_active_dcid(connection(), tokens.data());

  for (const ngtcp2_cid& cid : cids)
    socket_->DisassociateCID(QuicCID(&cid));

  for (const ngtcp2_cid_token& token : tokens) {
    if (token.token_present) {
      socket_->DisassociateStatelessResetToken(
          StatelessResetToken(token.token));
    }
  }

  Debug(this, "Removed from the QuicSocket");
  BaseObjectPtr<QuicSocket> socket = std::move(socket_);
  socket->RemoveSession(scid_, remote_address_);
}

// Removes the given stream from the QuicSession. All streams must
// be removed before the QuicSession is destroyed.
void QuicSession::RemoveStream(int64_t stream_id) {
  Debug(this, "Removing stream %" PRId64, stream_id);

  // ngtcp2 does no extend the max streams count automatically
  // except in very specific conditions, none of which apply
  // once we've gotten this far. We need to manually extend when
  // a remote peer initiated stream is removed.
  if (!ngtcp2_conn_is_local_stream(connection_.get(), stream_id)) {
    if (ngtcp2_is_bidi_stream(stream_id))
      ngtcp2_conn_extend_max_streams_bidi(connection_.get(), 1);
    else
      ngtcp2_conn_extend_max_streams_uni(connection_.get(), 1);
  }

  // This will have the side effect of destroying the QuicStream
  // instance.
  streams_.erase(stream_id);
  // Ensure that the stream state is closed and discarded by ngtcp2
  // Be sure to call this after removing the stream from the map
  // above so that when ngtcp2 closes the stream, the callback does
  // not attempt to loop back around and destroy the already removed
  // QuicStream instance. Typically, the stream is already going to
  // be closed by this point.
  ngtcp2_conn_shutdown_stream(connection(), stream_id, NGTCP2_NO_ERROR);
}

// The retransmit timer allows us to trigger retransmission
// of packets in case they are considered lost. The exact amount
// of time is determined internally by ngtcp2 according to the
// guidelines established by the QUIC spec but we use a libuv
// timer to actually monitor.
void QuicSession::ScheduleRetransmit() {
  uint64_t now = uv_hrtime();
  uint64_t expiry = ngtcp2_conn_get_expiry(connection());
  uint64_t interval = (expiry - now) / 1000000UL;
  if (expiry < now || interval == 0) interval = 1;
  Debug(this, "Scheduling the retransmit timer for %" PRIu64, interval);
  UpdateRetransmitTimer(interval);
}

// Transmits either a protocol or application connection
// close to the peer. The choice of which is send is
// based on the current value of last_error_.
bool QuicSession::SendConnectionClose() {
  CHECK(!Ngtcp2CallbackScope::InNgtcp2CallbackScope(this));

  // Do not send any frames at all if we're in the draining period
  // or in the middle of a silent close
  if (is_in_draining_period() || is_flag_set(QUICSESSION_FLAG_SILENT_CLOSE))
    return true;

  // The specific handling of connection close varies for client
  // and server QuicSession instances. For servers, we will
  // serialize the connection close once but may end up transmitting
  // it multiple times; whereas for clients, we will serialize it
  // once and send once only.
  QuicError error = last_error();

  // If initial keys have not yet been installed, use the alternative
  // ImmediateConnectionClose to send a stateless connection close to
  // the peer.
  if (crypto_context()->write_crypto_level() ==
        NGTCP2_CRYPTO_LEVEL_INITIAL) {
    socket()->ImmediateConnectionClose(
        dcid(),
        scid_,
        local_address_,
        remote_address_,
        error.code);
    return true;
  }

  switch (crypto_context_->side()) {
    case NGTCP2_CRYPTO_SIDE_SERVER: {
      // If we're not already in the closing period,
      // first attempt to write any pending packets, then
      // start the closing period. If closing period has
      // already started, skip this.
      if (!is_in_closing_period() &&
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
      auto packet = QuicPacket::Create("client connection close");

      // If we're not already in the closing period,
      // first attempt to write any pending packets, then
      // start the closing period. Note that the behavior
      // here is different than the server
      if (!is_in_closing_period() &&
          !WritePackets("client connection close - write packets")) {
        return false;
      }
      ssize_t nwrite =
          SelectCloseFn(error.family)(
            connection(),
            nullptr,
            packet->data(),
            max_pktlen_,
            error.code,
            uv_hrtime());
      if (nwrite < 0) {
        Debug(this, "Error writing connection close: %d", nwrite);
        set_last_error(QUIC_ERROR_SESSION, static_cast<int>(nwrite));
        return false;
      }
      packet->set_length(nwrite);
      return SendPacket(std::move(packet));
    }
    default:
      UNREACHABLE();
  }
}

void QuicSession::IgnorePreferredAddressStrategy(
    QuicSession* session,
    const QuicPreferredAddress& preferred_address) {
  Debug(session, "Ignoring server preferred address");
}

void QuicSession::UsePreferredAddressStrategy(
    QuicSession* session,
    const QuicPreferredAddress& preferred_address) {
  static constexpr int idx =
      IDX_QUIC_SESSION_STATE_USE_PREFERRED_ADDRESS_ENABLED;
  int family = session->socket()->local_address().family();
  if (preferred_address.Use(family)) {
    Debug(session, "Using server preferred address");
    // Emit only if the QuicSession has a usePreferredAddress handler
    // on the JavaScript side.
    if (UNLIKELY(session->state_[idx] == 1)) {
      session->listener()->OnUsePreferredAddress(family, preferred_address);
    }
  } else {
    // If Use returns false, the advertised preferred address does not
    // match the current local preferred endpoint IP version.
    Debug(session,
          "Not using server preferred address due to IP version mismatch");
  }
}

// Passes a serialized packet to the associated QuicSocket.
bool QuicSession::SendPacket(std::unique_ptr<QuicPacket> packet) {
  CHECK(!is_flag_set(QUICSESSION_FLAG_DESTROYED));
  CHECK(!is_in_draining_period());

  // There's nothing to send.
  if (packet->length() == 0)
    return true;

  IncrementStat(&QuicSessionStats::bytes_sent, packet->length());
  RecordTimestamp(&QuicSessionStats::sent_at);
  ScheduleRetransmit();

  Debug(this, "Sending %" PRIu64 " bytes to %s from %s",
        packet->length(),
        remote_address_,
        local_address_);

  int err = socket()->SendPacket(
      local_address_,
      remote_address_,
      std::move(packet),
      BaseObjectPtr<QuicSession>(this));

  if (err != 0) {
    set_last_error(QUIC_ERROR_SESSION, err);
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
  //  * The QuicSession is not currently associated with a QuicSocket
  if (Ngtcp2CallbackScope::InNgtcp2CallbackScope(this) ||
      is_destroyed() ||
      is_in_draining_period() ||
      (is_server() && is_in_closing_period()) ||
      socket() == nullptr) {
    return;
  }

  if (!application_->SendPendingData()) {
    Debug(this, "Error sending QUIC application data");
    HandleError();
  }
}

// When completing the TLS handshake, the TLS session information
// is provided to the QuicSession so that the session ticket and
// the remote transport parameters can be captured to support 0RTT
// session resumption.
int QuicSession::set_session(SSL_SESSION* session) {
  CHECK(!is_server());
  CHECK(!is_flag_set(QUICSESSION_FLAG_DESTROYED));
  int size = i2d_SSL_SESSION(session, nullptr);
  if (size > SecureContext::kMaxSessionSize)
    return 0;
  listener_->OnSessionTicket(size, session);
  return 1;
}

// A client QuicSession can be migrated to a different QuicSocket instance.
// TODO(@jasnell): This will be revisited.
bool QuicSession::set_socket(QuicSocket* socket, bool nat_rebinding) {
  CHECK(!is_server());
  CHECK(!is_flag_set(QUICSESSION_FLAG_DESTROYED));

  if (is_flag_set(QUICSESSION_FLAG_GRACEFUL_CLOSING))
    return false;

  if (socket == nullptr || socket == socket_.get())
    return true;

  Debug(this, "Migrating to %s", socket->diagnostic_name());

  SendSessionScope send(this);

  // Ensure that we maintain a reference to keep this from being
  // destroyed while we are starting the migration.
  BaseObjectPtr<QuicSession> ptr(this);

  // Step 1: Remove the session from the current socket
  RemoveFromSocket();

  // Step 2: Add this Session to the given Socket
  AddToSocket(socket);

  // Step 3: Update the internal references and make sure
  // we are listening.
  socket_.reset(socket);
  socket->ReceiveStart();

  // Step 4: Update ngtcp2
  auto& local_address = socket->local_address();
  if (nat_rebinding) {
    ngtcp2_addr addr;
    ngtcp2_addr_init(
        &addr,
        local_address.data(),
        local_address.length(),
        nullptr);
    ngtcp2_conn_set_local_addr(connection(), &addr);
  } else {
    QuicPath path(local_address, remote_address_);
    if (ngtcp2_conn_initiate_migration(
            connection(),
            &path,
            uv_hrtime()) != 0) {
      return false;
    }
  }

  return true;
}

void QuicSession::ResumeStream(int64_t stream_id) {
  application()->ResumeStream(stream_id);
}

void QuicSession::ResetStream(int64_t stream_id, uint64_t code) {
  SendSessionScope scope(this);
  CHECK_EQ(ngtcp2_conn_shutdown_stream(connection(), stream_id, code), 0);
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
void QuicSession::SilentClose() {
  // Calling either ImmediateClose or SilentClose will cause
  // the QUICSESSION_FLAG_CLOSING to be set. In either case,
  // we should never re-enter ImmediateClose or SilentClose.
  CHECK(!is_flag_set(QUICSESSION_FLAG_CLOSING));
  set_flag(QUICSESSION_FLAG_SILENT_CLOSE);
  set_flag(QUICSESSION_FLAG_CLOSING);

  QuicError err = last_error();
  Debug(this,
        "Silent close with %s code %" PRIu64 " (stateless reset? %s)",
        err.family_name(),
        err.code,
        is_stateless_reset() ? "yes" : "no");

  listener()->OnSessionSilentClose(is_stateless_reset(), err);
}
// Begin connection close by serializing the CONNECTION_CLOSE packet.
// There are two variants: one to serialize an application close, the
// other to serialize a protocol close.  The frames are generally
// identical with the exception of a bit in the header. On server
// QuicSessions, we serialize the frame once and may retransmit it
// multiple times. On client QuicSession instances, we only ever
// serialize the connection close once.
bool QuicSession::StartClosingPeriod() {
  if (is_flag_set(QUICSESSION_FLAG_DESTROYED))
    return false;
  if (is_in_closing_period())
    return true;

  StopRetransmitTimer();
  UpdateIdleTimer();

  QuicError error = last_error();
  Debug(this, "Closing period has started. Error %d", error.code);

  // Once the CONNECTION_CLOSE packet is written,
  // is_in_closing_period will return true.
  conn_closebuf_ = QuicPacket::Create(
      "server connection close");
  ssize_t nwrite =
      SelectCloseFn(error.family)(
          connection(),
          nullptr,
          conn_closebuf_->data(),
          max_pktlen_,
          error.code,
          uv_hrtime());
  if (nwrite < 0) {
    if (nwrite == NGTCP2_ERR_PKT_NUM_EXHAUSTED) {
      set_last_error(QUIC_ERROR_SESSION, NGTCP2_ERR_PKT_NUM_EXHAUSTED);
      SilentClose();
    } else {
      set_last_error(QUIC_ERROR_SESSION, static_cast<int>(nwrite));
    }
    return false;
  }
  conn_closebuf_->set_length(nwrite);
  return true;
}

// Called by ngtcp2 when a stream has been closed. If the stream does
// not exist, the close is ignored.
void QuicSession::StreamClose(int64_t stream_id, uint64_t app_error_code) {
  if (is_flag_set(QUICSESSION_FLAG_DESTROYED))
    return;

  Debug(this, "Closing stream %" PRId64 " with code %" PRIu64,
        stream_id,
        app_error_code);

  application_->StreamClose(stream_id, app_error_code);
}

// Called by ngtcp2 when a stream has been opened. All we do is log
// the activity here. We do not want to actually commit any resources
// until data is received for the stream. This allows us to prevent
// a stream commitment attack. The only exception is shutting the
// stream down explicitly if we are in a graceful close period.
void QuicSession::StreamOpen(int64_t stream_id) {
  if (is_flag_set(QUICSESSION_FLAG_GRACEFUL_CLOSING)) {
    ngtcp2_conn_shutdown_stream(
        connection(),
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
  if (is_flag_set(QUICSESSION_FLAG_DESTROYED))
    return;

  Debug(this,
        "Reset stream %" PRId64 " with code %" PRIu64
        " and final size %" PRIu64,
        stream_id,
        app_error_code,
        final_size);

  BaseObjectPtr<QuicStream> stream = FindStream(stream_id);

  if (stream) {
    stream->set_final_size(final_size);
    application_->StreamReset(stream_id, app_error_code);
  }
}

void QuicSession::UpdateConnectionID(
    int type,
    const QuicCID& cid,
    const StatelessResetToken& token) {
  switch (type) {
    case NGTCP2_CONNECTION_ID_STATUS_TYPE_ACTIVATE:
      socket_->AssociateStatelessResetToken(
          token,
          BaseObjectPtr<QuicSession>(this));
      break;
    case NGTCP2_CONNECTION_ID_STATUS_TYPE_DEACTIVATE:
      socket_->DisassociateStatelessResetToken(token);
      break;
  }
}

// Updates the idle timer timeout. If the idle timer fires, the connection
// will be silently closed. It is important to update this as activity
// occurs to keep the idle timer from firing.
void QuicSession::UpdateIdleTimer() {
  CHECK_NOT_NULL(idle_);
  uint64_t now = uv_hrtime();
  uint64_t expiry = ngtcp2_conn_get_idle_expiry(connection());
  // nano to millis
  uint64_t timeout = expiry > now ? (expiry - now) / 1000000ULL : 1;
  if (timeout == 0) timeout = 1;
  Debug(this, "Updating idle timeout to %" PRIu64, timeout);
  idle_->Update(timeout);
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
  CHECK(!is_flag_set(QUICSESSION_FLAG_DESTROYED));

  // During the draining period, we must not send any frames at all.
  if (is_in_draining_period())
    return true;

  // During the closing period, we are only permitted to send
  // CONNECTION_CLOSE frames.
  if (is_in_closing_period()) {
    return SendConnectionClose();
  }

  // Otherwise, serialize and send pending frames
  QuicPathStorage path;
  for (;;) {
    auto packet = QuicPacket::Create(diagnostic_label, max_pktlen_);
    // ngtcp2_conn_write_pkt will fill the created QuicPacket up
    // as much as possible, and then should be called repeatedly
    // until it returns 0 or fatally errors. On each call, it
    // will return the number of bytes encoded into the packet.
    ssize_t nwrite =
        ngtcp2_conn_write_pkt(
            connection(),
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
          set_last_error(QUIC_ERROR_SESSION, static_cast<int>(nwrite));
          return false;
      }
    }

    packet->set_length(nwrite);
    UpdateEndpoint(path.path);
    UpdateDataStats();

    if (!SendPacket(std::move(packet)))
      return false;
  }
}

void QuicSession::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("crypto_context", crypto_context_.get());
  tracker->TrackField("alpn", alpn_);
  tracker->TrackField("hostname", hostname_);
  tracker->TrackField("idle", idle_);
  tracker->TrackField("retransmit", retransmit_);
  tracker->TrackField("streams", streams_);
  tracker->TrackField("state", state_);
  tracker->TrackFieldWithSize("current_ngtcp2_memory", current_ngtcp2_memory_);
  tracker->TrackField("conn_closebuf", conn_closebuf_);
  tracker->TrackField("application", application_);
  StatsBase::StatsMemoryInfo(tracker);
}

// Static function to create a new server QuicSession instance
BaseObjectPtr<QuicSession> QuicSession::CreateServer(
    QuicSocket* socket,
    const QuicSessionConfig& config,
    const QuicCID& rcid,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    const QuicCID& dcid,
    const QuicCID& ocid,
    uint32_t version,
    const std::string& alpn,
    uint32_t options,
    QlogMode qlog) {
  Local<Object> obj;
  if (!socket->env()
             ->quicserversession_instance_template()
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
          qlog);

  session->AddToSocket(socket);
  return session;
}

// Initializes a newly created server QuicSession.
void QuicSession::InitServer(
    QuicSessionConfig config,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    const QuicCID& dcid,
    const QuicCID& ocid,
    uint32_t version,
    QlogMode qlog) {

  CHECK_NULL(connection_);

  ExtendMaxStreamsBidi(DEFAULT_MAX_STREAMS_BIDI);
  ExtendMaxStreamsUni(DEFAULT_MAX_STREAMS_UNI);

  local_address_ = local_addr;
  remote_address_ = remote_addr;
  max_pktlen_ = GetMaxPktLen(remote_addr);

  config.set_original_connection_id(ocid);

  connection_id_strategy_(this, scid_.cid(), kScidLen);

  config.GenerateStatelessResetToken(this, scid_);
  config.GeneratePreferredAddressToken(connection_id_strategy_, this, &pscid_);

  QuicPath path(local_addr, remote_address_);

  // NOLINTNEXTLINE(readability/pointer_notation)
  if (qlog == QlogMode::kEnabled) config.set_qlog({ *ocid, OnQlogWrite });

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
          &alloc_info_,
          static_cast<QuicSession*>(this)), 0);

  connection_.reset(conn);

  crypto_context_->Initialize();
  UpdateDataStats();
  UpdateIdleTimer();
}

namespace {
// A pointer to this function is passed to the JavaScript side during
// the client hello and is called by user code when the TLS handshake
// should resume.
void QuicSessionOnClientHelloDone(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->crypto_context()->OnClientHelloDone();
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
  session->crypto_context()->OnOCSPDone(
      BaseObjectPtr<crypto::SecureContext>(context),
      args[1]);
}
}  // namespace

// Recovery stats are used to allow user code to keep track of
// important round-trip timing statistics that are updated through
// the lifetime of a connection. Effectively, these communicate how
// much time (from the perspective of the local peer) is being taken
// to exchange data reliably with the remote peer.
void QuicSession::UpdateRecoveryStats() {
  const ngtcp2_rcvry_stat* stat =
      ngtcp2_conn_get_rcvry_stat(connection());
  SetStat(&QuicSessionStats::min_rtt, stat->min_rtt);
  SetStat(&QuicSessionStats::latest_rtt, stat->latest_rtt);
  SetStat(&QuicSessionStats::smoothed_rtt, stat->smoothed_rtt);
}

// Data stats are used to allow user code to keep track of important
// statistics such as amount of data in flight through the lifetime
// of a connection.
void QuicSession::UpdateDataStats() {
  if (is_destroyed())
    return;
  state_[IDX_QUIC_SESSION_STATE_MAX_DATA_LEFT] =
    static_cast<double>(ngtcp2_conn_get_max_data_left(connection()));
  size_t bytes_in_flight = ngtcp2_conn_get_bytes_in_flight(connection());
  state_[IDX_QUIC_SESSION_STATE_BYTES_IN_FLIGHT] =
    static_cast<double>(bytes_in_flight);
  // The max_bytes_in_flight is a highwater mark that can be used
  // in performance analysis operations.
  if (bytes_in_flight > GetStat(&QuicSessionStats::max_bytes_in_flight))
    SetStat(&QuicSessionStats::max_bytes_in_flight, bytes_in_flight);
}

// Static method for creating a new client QuicSession instance.
BaseObjectPtr<QuicSession> QuicSession::CreateClient(
    QuicSocket* socket,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    BaseObjectPtr<SecureContext> secure_context,
    ngtcp2_transport_params* early_transport_params,
    crypto::SSLSessionPointer early_session_ticket,
    Local<Value> dcid,
    PreferredAddressStrategy preferred_address_strategy,
    const std::string& alpn,
    const std::string& hostname,
    uint32_t options,
    QlogMode qlog) {
  Local<Object> obj;
  if (!socket->env()
             ->quicclientsession_instance_template()
             ->NewInstance(socket->env()->context()).ToLocal(&obj)) {
    return {};
  }

  BaseObjectPtr<QuicSession> session =
      MakeDetachedBaseObject<QuicSession>(
          socket,
          obj,
          local_addr,
          remote_addr,
          secure_context,
          early_transport_params,
          std::move(early_session_ticket),
          dcid,
          preferred_address_strategy,
          alpn,
          hostname,
          options,
          qlog);

  session->AddToSocket(socket);

  return session;
}

// Initialize a newly created client QuicSession.
// The early_transport_params and session_ticket are optional to
// perform a 0RTT resumption of a prior session.
// The dcid_value parameter is optional to allow user code the
// ability to provide an explicit dcid (this should be rare)
void QuicSession::InitClient(
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    ngtcp2_transport_params* early_transport_params,
    crypto::SSLSessionPointer early_session_ticket,
    Local<Value> dcid_value,
    QlogMode qlog) {
  CHECK_NULL(connection_);

  local_address_ = local_addr;
  remote_address_ = remote_addr;
  Debug(this, "Initializing connection from %s to %s",
        local_address_,
        remote_address_);

  // The maximum packet length is determined largely
  // by the IP version (IPv4 vs IPv6). Packet sizes
  // should be limited to the maximum MTU necessary to
  // prevent IP fragmentation.
  max_pktlen_ = GetMaxPktLen(remote_address_);

  QuicSessionConfig config(env());
  ExtendMaxStreamsBidi(DEFAULT_MAX_STREAMS_BIDI);
  ExtendMaxStreamsUni(DEFAULT_MAX_STREAMS_UNI);

  connection_id_strategy_(this, scid_.cid(), NGTCP2_MAX_CIDLEN);

  ngtcp2_cid dcid;
  if (dcid_value->IsArrayBufferView()) {
    ArrayBufferViewContents<uint8_t> sbuf(
        dcid_value.As<ArrayBufferView>());
    CHECK_LE(sbuf.length(), NGTCP2_MAX_CIDLEN);
    CHECK_GE(sbuf.length(), NGTCP2_MIN_CIDLEN);
    memcpy(dcid.data, sbuf.data(), sbuf.length());
    dcid.datalen = sbuf.length();
  } else {
    connection_id_strategy_(this, &dcid, NGTCP2_MAX_CIDLEN);
  }

  QuicPath path(local_address_, remote_address_);

  if (qlog == QlogMode::kEnabled) config.set_qlog({ dcid, OnQlogWrite });

  ngtcp2_conn* conn;
  CHECK_EQ(
      ngtcp2_conn_client_new(
          &conn,
          &dcid,
          scid_.cid(),
          &path,
          NGTCP2_PROTO_VER,
          &callbacks[crypto_context_->side()],
          &config,
          &alloc_info_,
          static_cast<QuicSession*>(this)), 0);


  connection_.reset(conn);

  crypto_context_->Initialize();

  CHECK(DeriveAndInstallInitialKey(*this, this->dcid()));

  if (early_transport_params != nullptr)
    ngtcp2_conn_set_early_remote_transport_params(conn, early_transport_params);
  crypto_context_->set_session(std::move(early_session_ticket));

  UpdateIdleTimer();
  UpdateDataStats();
}

// Static ngtcp2 callbacks are registered when ngtcp2 when a new ngtcp2_conn is
// created. These are static functions that, for the most part, simply defer to
// a QuicSession instance that is passed through as user_data.

// Called by ngtcp2 upon creation of a new client connection
// to initiate the TLS handshake. This is only emitted on the client side.
int QuicSession::OnClientInitial(
    ngtcp2_conn* conn,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return NGTCP2_OK(session->crypto_context()->Receive(
      NGTCP2_CRYPTO_LEVEL_INITIAL,
      0, nullptr, 0)) ? 0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

// Triggered by ngtcp2 when an initial handshake packet has been
// received. This is only invoked on server sessions and it is
// the absolute beginning of the communication between a client
// and a server.
int QuicSession::OnReceiveClientInitial(
    ngtcp2_conn* conn,
    const ngtcp2_cid* dcid,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  if (!session->ReceiveClientInitial(QuicCID(dcid))) {
    Debug(session, "Receiving initial client handshake failed");
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}

// Called by ngtcp2 for both client and server connections when
// TLS handshake data has been received and needs to be processed.
// This will be called multiple times during the TLS handshake
// process and may be called during key updates.
int QuicSession::OnReceiveCryptoData(
    ngtcp2_conn* conn,
    ngtcp2_crypto_level crypto_level,
    uint64_t offset,
    const uint8_t* data,
    size_t datalen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  return static_cast<int>(
    session->crypto_context()->Receive(crypto_level, offset, data, datalen));
}

// Triggered by ngtcp2 when a RETRY packet has been received. This is
// only emitted on the client side (only a server can send a RETRY).
//
// Per the QUIC specification, a RETRY is essentially a mechanism for
// the server to force path validation at the very start of a connection
// at the cost of a single round trip. The RETRY includes a token that
// the client must use in subsequent requests. When received, the client
// MUST restart the TLS handshake and must include the RETRY token in
// all initial packets. If the initial packets contain a valid RETRY
// token, then the server assumes the path to be validated. Fortunately
// ngtcp2 handles the retry token for us, so all we have to do is
// regenerate the initial keying material and restart the handshake
// and we can ignore the retry parameter.
int QuicSession::OnReceiveRetry(
    ngtcp2_conn* conn,
    const ngtcp2_pkt_hd* hd,
    const ngtcp2_pkt_retry* retry,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
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
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
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
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamsUni(max_streams);
  return 0;
}

// Triggered by ngtcp2 when the local peer has received an
// indication from the remote peer indicating that additional
// unidirectional streams may be sent. The max_streams parameter
// identifies the highest unidirectional stream ID that may be
// opened.
int QuicSession::OnExtendMaxStreamsRemoteUni(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamsRemoteUni(max_streams);
  return 0;
}

// Triggered by ngtcp2 when the local peer has received an
// indication from the remote peer indicating that additional
// bidirectional streams may be sent. The max_streams parameter
// identifies the highest bidirectional stream ID that may be
// opened.
int QuicSession::OnExtendMaxStreamsRemoteBidi(
    ngtcp2_conn* conn,
    uint64_t max_streams,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamsRemoteUni(max_streams);
  return 0;
}

// Triggered by ngtcp2 when the local peer has received a flow
// control signal from the remote peer indicating that additional
// data can be sent. The max_data parameter identifies the maximum
// data offset that may be sent. That is, a value of 99 means that
// out of a stream of 1000 bytes, only the first 100 may be sent.
// (offsets 0 through 99).
int QuicSession::OnExtendMaxStreamData(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t max_data,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->ExtendMaxStreamData(stream_id, max_data);
  return 0;
}

int QuicSession::OnConnectionIDStatus(
    ngtcp2_conn* conn,
    int type,
    uint64_t seq,
    const ngtcp2_cid* cid,
    const uint8_t* token,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;

  QuicCID qcid(cid);
  Debug(session, "Updating connection ID %s (has reset token? %s)",
        qcid,
        token == nullptr ? "No" : "Yes");
  if (token != nullptr)
    session->UpdateConnectionID(type, qcid, StatelessResetToken(token));
  return 0;
}

// Called by ngtcp2 for both client and server connections
// when ngtcp2 has determined that the TLS handshake has
// been completed. It is important to understand that this
// is only an indication of the local peer's handshake state.
// The remote peer might not yet have completed its part
// of the handshake.
int QuicSession::OnHandshakeCompleted(
    ngtcp2_conn* conn,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->HandshakeCompleted();
  return 0;
}

// Called by ngtcp2 for clients when the handshake has been
// confirmed. Confirmation occurs *after* handshake completion.
int QuicSession::OnHandshakeConfirmed(
    ngtcp2_conn* conn,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->HandshakeConfirmed();
  return 0;
}

// Called by ngtcp2 when a chunk of stream data has been received.
// Currently, ngtcp2 ensures that this callback is always called
// with an offset parameter strictly larger than the previous call's
// offset + datalen (that is, data will never be delivered out of
// order). That behavior may change in the future but only via a
// configuration option.
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
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
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
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  session->StreamOpen(stream_id);
  return 0;
}

// Called by ngtcp2 when an acknowledgement for a chunk of
// TLS handshake data has been received by the remote peer.
// This is only an indication that data was received, not that
// it was successfully processed. Acknowledgements are a key
// part of the QUIC reliability mechanism.
int QuicSession::OnAckedCryptoOffset(
    ngtcp2_conn* conn,
    ngtcp2_crypto_level crypto_level,
    uint64_t offset,
    size_t datalen,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->crypto_context()->AcknowledgeCryptoData(crypto_level, datalen);
  return 0;
}

// Called by ngtcp2 when an acknowledgement for a chunk of
// stream data has been received successfully by the remote peer.
// This is only an indication that data was received, not that
// it was successfully processed. Acknowledgements are a key
// part of the QUIC reliability mechanism.
int QuicSession::OnAckedStreamDataOffset(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t offset,
    size_t datalen,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
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
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);

  // The paddr parameter contains the server advertised preferred
  // address. The dest parameter contains the address that is
  // actually being used. If the preferred address is selected,
  // then the contents of paddr are copied over to dest. It is
  // important to remember that SelectPreferredAddress should
  // return true regardless of whether the preferred address was
  // selected or not. It should only return false if there was
  // an actual failure processing things. Note, however, that
  // even in such a failure, we debug log and ignore it.
  // If the preferred address is not selected, dest remains
  // unchanged.
  QuicPreferredAddress preferred_address(session->env(), dest, paddr);
  session->SelectPreferredAddress(preferred_address);
  return 0;
}

// Called by ngtcp2 when a stream has been closed.
int QuicSession::OnStreamClose(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t app_error_code,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->StreamClose(stream_id, app_error_code);
  return 0;
}

// Stream reset means the remote peer will no longer send data
// on the identified stream. It is essentially a premature close.
// The final_size parameter is important here in that it identifies
// exactly how much data the *remote peer* is aware that it sent.
// If there are lost packets, then the local peer's idea of the final
// size might not match.
int QuicSession::OnStreamReset(
    ngtcp2_conn* conn,
    int64_t stream_id,
    uint64_t final_size,
    uint64_t app_error_code,
    void* user_data,
    void* stream_user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->StreamReset(stream_id, final_size, app_error_code);
  return 0;
}

// Called by ngtcp2 when it needs to generate some random data.
// We currently do not use it, but the ngtcp2_rand_ctx identifies
// why the random data is necessary. When ctx is equal to
// NGTCP2_RAND_CTX_NONE, it typically means the random data
// is being used during the TLS handshake. When ctx is equal to
// NGTCP2_RAND_CTX_PATH_CHALLENGE, the random data is being
// used to construct a PATH_CHALLENGE. These *might* need more
// secure and robust random number generation given the
// sensitivity of PATH_CHALLENGE operations (an attacker
// could use a compromised PATH_CHALLENGE to trick an endpoint
// into redirecting traffic).
// TODO(@jasnell): In the future, we'll want to explore whether
// we want to handle the different cases of ngtcp2_rand_ctx
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
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  CHECK(!Ngtcp2CallbackScope::InNgtcp2CallbackScope(session));
  return session->GetNewConnectionID(cid, token, cidlen) ?
      0 : NGTCP2_ERR_CALLBACK_FAILURE;
}

// When a connection is closed, ngtcp2 will call this multiple
// times to retire connection IDs. It's also possible for this
// to be called at times throughout the lifecycle of the connection
// to remove a CID from the availability pool.
int QuicSession::OnRemoveConnectionID(
    ngtcp2_conn* conn,
    const ngtcp2_cid* cid,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  session->RemoveConnectionID(QuicCID(cid));
  return 0;
}

// Called by ngtcp2 to perform path validation. Path validation
// is necessary to ensure that a packet is originating from the
// expected source. If the res parameter indicates success, it
// means that the path specified has been verified as being
// valid.
//
// Validity here means only that there has been a successful
// exchange of PATH_CHALLENGE information between the peers.
// It's critical to understand that the validity of a path
// can change at any timee so this is only an indication of
// validity at a specific point in time.
int QuicSession::OnPathValidation(
    ngtcp2_conn* conn,
    const ngtcp2_path* path,
    ngtcp2_path_validation_result res,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->PathValidation(path, res);
  return 0;
}

// Triggered by ngtcp2 when a version negotiation is received.
// What this means is that the remote peer does not support the
// QUIC version requested. The only thing we can do here (per
// the QUIC specification) is silently discard the connection
// and notify the JavaScript side that a different version of
// QUIC should be used. The sv parameter does list the QUIC
// versions advertised as supported by the remote peer.
int QuicSession::OnVersionNegotiation(
    ngtcp2_conn* conn,
    const ngtcp2_pkt_hd* hd,
    const uint32_t* sv,
    size_t nsv,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  QuicSession::Ngtcp2CallbackScope callback_scope(session);
  session->VersionNegotiation(sv, nsv);
  return 0;
}

// Triggered by ngtcp2 when a stateless reset is received. What this
// means is that the remote peer might recognize the CID but has lost
// all state necessary to successfully process it. The only thing we
// can do is silently close the connection. For server sessions, this
// means all session state is shut down and discarded, even on the
// JavaScript side. For client sessions, we discard session state at
// the C++ layer but -- at least in the future -- we can retain some
// state at the JavaScript level to allow for automatic session
// resumption.
int QuicSession::OnStatelessReset(
    ngtcp2_conn* conn,
    const ngtcp2_pkt_stateless_reset* sr,
    void* user_data) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  if (UNLIKELY(session->is_destroyed()))
    return NGTCP2_ERR_CALLBACK_FAILURE;
  session->set_flag(QUICSESSION_FLAG_STATELESS_RESET);
  return 0;
}

void QuicSession::OnQlogWrite(void* user_data, const void* data, size_t len) {
  QuicSession* session = static_cast<QuicSession*>(user_data);
  session->listener()->OnQLog(reinterpret_cast<const uint8_t*>(data), len);
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
    ngtcp2_crypto_hp_mask_cb,
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
    ngtcp2_crypto_update_key_cb,
    OnPathValidation,
    OnSelectPreferredAddress,
    OnStreamReset,
    OnExtendMaxStreamsRemoteBidi,
    OnExtendMaxStreamsRemoteUni,
    OnExtendMaxStreamData,
    OnConnectionIDStatus,
    OnHandshakeConfirmed
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
  }
};

// JavaScript API

namespace {
void QuicSessionSetSocket(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  CHECK(args[0]->IsObject());
  ASSIGN_OR_RETURN_UNWRAP(&socket, args[0].As<Object>());
  args.GetReturnValue().Set(session->set_socket(socket));
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
  session->set_last_error(QuicError(env, args[0], args[1]));
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
  session->set_last_error(QuicError(env, args[0], args[1]));
  session->Destroy();
}

void QuicSessionGetEphemeralKeyInfo(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Local<Object> ret;
  if (session->crypto_context()->ephemeral_key().ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void QuicSessionGetPeerCertificate(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Local<Value> ret;
  if (session->crypto_context()->peer_cert(!args[0]->IsTrue()).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void QuicSessionGetRemoteAddress(
    const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Environment* env = session->env();
  CHECK(args[0]->IsObject());
  args.GetReturnValue().Set(
      session->remote_address().ToJS(env, args[0].As<Object>()));
}

void QuicSessionGetCertificate(
    const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Local<Value> ret;
  if (session->crypto_context()->cert().ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void QuicSessionPing(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->Ping();
}

// Triggers a silent close of a QuicSession. This is currently only used
// (and should ever only be used) for testing purposes...
void QuicSessionSilentClose(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args[0].As<Object>());
  ProcessEmitWarning(
      session->env(),
      "Forcing silent close of QuicSession for testing purposes only");
  session->SilentClose();
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
  // Initiating a key update may fail if it is done too early (either
  // before the TLS handshake has been confirmed or while a previous
  // key update is being processed). When it fails, InitiateKeyUpdate()
  // will return false.
  args.GetReturnValue().Set(session->crypto_context()->InitiateKeyUpdate());
}

// When a client wishes to resume a prior TLS session, it must specify both
// the remember transport parameters and remembered TLS session ticket. Those
// will each be provided as a TypedArray. The DecodeTransportParams and
// DecodeSessionTicket functions handle those. If the argument is undefined,
// then resumption is not used.

bool DecodeTransportParams(
    Local<Value> value,
    ngtcp2_transport_params* params) {
  if (value->IsUndefined())
    return false;
  CHECK(value->IsArrayBufferView());
  ArrayBufferViewContents<uint8_t> sbuf(value.As<ArrayBufferView>());
  if (sbuf.length() != sizeof(ngtcp2_transport_params))
    return false;
  memcpy(params, sbuf.data(), sizeof(ngtcp2_transport_params));
  return true;
}

crypto::SSLSessionPointer DecodeSessionTicket(Local<Value> value) {
  if (value->IsUndefined())
    return {};
  CHECK(value->IsArrayBufferView());
  ArrayBufferViewContents<unsigned char> sbuf(value.As<ArrayBufferView>());
  return crypto::GetTLSSession(sbuf.data(), sbuf.length());
}

void QuicSessionStartHandshake(const FunctionCallbackInfo<Value>& args) {
  QuicSession* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->StartHandshake();
}

void NewQuicClientSession(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  QuicSocket* socket;
  int32_t family;
  uint32_t port;
  SecureContext* sc;
  SocketAddress remote_addr;
  int32_t preferred_address_policy;
  PreferredAddressStrategy preferred_address_strategy;
  uint32_t options = QUICCLIENTSESSION_OPTION_VERIFY_HOSTNAME_IDENTITY;
  std::string alpn(NGTCP2_ALPN_H3);

  enum ARG_IDX : int {
    SOCKET,
    TYPE,
    IP,
    PORT,
    SECURE_CONTEXT,
    SNI,
    REMOTE_TRANSPORT_PARAMS,
    SESSION_TICKET,
    DCID,
    PREFERRED_ADDRESS_POLICY,
    ALPN,
    OPTIONS,
    QLOG,
    AUTO_START
  };

  CHECK(args[ARG_IDX::SOCKET]->IsObject());
  CHECK(args[ARG_IDX::SECURE_CONTEXT]->IsObject());
  CHECK(args[ARG_IDX::IP]->IsString());
  CHECK(args[ARG_IDX::ALPN]->IsString());
  CHECK(args[ARG_IDX::TYPE]->Int32Value(env->context()).To(&family));
  CHECK(args[ARG_IDX::PORT]->Uint32Value(env->context()).To(&port));
  CHECK(args[ARG_IDX::OPTIONS]->Uint32Value(env->context()).To(&options));
  CHECK(args[ARG_IDX::AUTO_START]->IsBoolean());
  if (!args[ARG_IDX::SNI]->IsUndefined())
    CHECK(args[ARG_IDX::SNI]->IsString());

  ASSIGN_OR_RETURN_UNWRAP(&socket, args[ARG_IDX::SOCKET].As<Object>());
  ASSIGN_OR_RETURN_UNWRAP(&sc, args[ARG_IDX::SECURE_CONTEXT].As<Object>());

  CHECK(args[ARG_IDX::PREFERRED_ADDRESS_POLICY]->Int32Value(
      env->context()).To(&preferred_address_policy));
  switch (preferred_address_policy) {
    case QUIC_PREFERRED_ADDRESS_ACCEPT:
      preferred_address_strategy = QuicSession::UsePreferredAddressStrategy;
      break;
    default:
      preferred_address_strategy = QuicSession::IgnorePreferredAddressStrategy;
  }

  node::Utf8Value address(env->isolate(), args[ARG_IDX::IP]);
  node::Utf8Value servername(env->isolate(), args[ARG_IDX::SNI]);

  if (!SocketAddress::New(family, *address, port, &remote_addr))
    return args.GetReturnValue().Set(ERR_FAILED_TO_CREATE_SESSION);

  // ALPN is a string prefixed by the length, followed by values
  Utf8Value val(env->isolate(), args[ARG_IDX::ALPN]);
  alpn = val.length();
  alpn += *val;

  crypto::SSLSessionPointer early_session_ticket =
      DecodeSessionTicket(args[ARG_IDX::SESSION_TICKET]);
  ngtcp2_transport_params early_transport_params;
  bool has_early_transport_params =
      DecodeTransportParams(
          args[ARG_IDX::REMOTE_TRANSPORT_PARAMS],
          &early_transport_params);

  socket->ReceiveStart();

  BaseObjectPtr<QuicSession> session =
      QuicSession::CreateClient(
          socket,
          socket->local_address(),
          remote_addr,
          BaseObjectPtr<crypto::SecureContext>(sc),
          has_early_transport_params ? &early_transport_params : nullptr,
          std::move(early_session_ticket),
          args[ARG_IDX::DCID],
          preferred_address_strategy,
          alpn,
          std::string(*servername),
          options,
          args[ARG_IDX::QLOG]->IsTrue() ?
              QlogMode::kEnabled :
              QlogMode::kDisabled);

  // Start the TLS handshake if the autoStart option is true
  // (which it is by default).
  if (args[ARG_IDX::AUTO_START]->BooleanValue(env->isolate())) {
    session->StartHandshake();
    // Session was created but was unable to bootstrap properly during
    // the start of the TLS handshake.
    if (session->is_destroyed())
      return args.GetReturnValue().Set(ERR_FAILED_TO_CREATE_SESSION);
  }

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
    env->set_quicserversession_instance_template(sessiont);
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
    env->SetProtoMethod(session, "startHandshake", QuicSessionStartHandshake);
    env->set_quicclientsession_instance_template(sessiont);

    env->SetMethod(target, "createClientSession", NewQuicClientSession);
    env->SetMethod(target, "silentCloseSession", QuicSessionSilentClose);
  }
}

}  // namespace quic
}  // namespace node
