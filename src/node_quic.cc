#include "debug_utils.h"
#include "node.h"
#include "env-inl.h"
#include "histogram-inl.h"
#include "node_crypto.h"  // SecureContext
#include "node_crypto_common.h"
#include "node_process.h"
#include "node_quic_crypto.h"
#include "node_quic_session-inl.h"
#include "node_quic_socket.h"
#include "node_quic_stream.h"
#include "node_quic_state.h"
#include "node_quic_util-inl.h"
#include "node_sockaddr-inl.h"

#include <memory>
#include <utility>

namespace node {

using crypto::SecureContext;
using v8::Context;
using v8::DontDelete;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::ReadOnly;
using v8::Value;

namespace quic {

namespace {
// Register the JavaScript callbacks the internal binding will use to report
// status and updates. This is called only once when the quic module is loaded.
void QuicSetCallbacks(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsObject());
  Local<Object> obj = args[0].As<Object>();

#define SETFUNCTION(name, callback)                                           \
  do {                                                                        \
    Local<Value> fn;                                                          \
    CHECK(obj->Get(env->context(),                                            \
                   FIXED_ONE_BYTE_STRING(env->isolate(), name)).ToLocal(&fn));\
    CHECK(fn->IsFunction());                                                  \
    env->set_quic_on_##callback##_function(fn.As<Function>());                \
  } while (0)

  SETFUNCTION("onSocketClose", socket_close);
  SETFUNCTION("onSocketError", socket_error);
  SETFUNCTION("onSessionReady", session_ready);
  SETFUNCTION("onSessionCert", session_cert);
  SETFUNCTION("onSessionClientHello", session_client_hello);
  SETFUNCTION("onSessionClose", session_close);
  SETFUNCTION("onSessionDestroyed", session_destroyed);
  SETFUNCTION("onSessionError", session_error);
  SETFUNCTION("onSessionHandshake", session_handshake);
  SETFUNCTION("onSessionKeylog", session_keylog);
  SETFUNCTION("onSessionPathValidation", session_path_validation);
  SETFUNCTION("onSessionQlog", session_qlog);
  SETFUNCTION("onSessionSilentClose", session_silent_close);
  SETFUNCTION("onSessionStatus", session_status);
  SETFUNCTION("onSessionTicket", session_ticket);
  SETFUNCTION("onSessionVersionNegotiation", session_version_negotiation);
  SETFUNCTION("onStreamReady", stream_ready);
  SETFUNCTION("onStreamClose", stream_close);
  SETFUNCTION("onStreamError", stream_error);
  SETFUNCTION("onStreamReset", stream_reset);
  SETFUNCTION("onSocketServerBusy", socket_server_busy);
  SETFUNCTION("onStreamHeaders", stream_headers);

#undef SETFUNCTION
}


// Sets QUIC specific configuration options for the SecureContext.
// It's entirely likely that there's a better way to do this, but
// for now this works.
template <ngtcp2_crypto_side side>
void QuicInitSecureContext(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsObject());  // Secure Context
  CHECK(args[1]->IsString());  // groups
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args[0].As<Object>(),
                          args.GetReturnValue().Set(UV_EBADF));
  const node::Utf8Value groups(env->isolate(), args[1]);

  InitializeSecureContext(sc, side);

  // TODO(@jasnell): Throw a proper node.js error with code
  if (!crypto::SetGroups(sc, *groups))
    return env->ThrowError("Failed to set groups");
}
}  // namespace


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);

  HistogramBase::Initialize(env);

  std::unique_ptr<QuicState> state(new QuicState(isolate));
#define SET_STATE_TYPEDARRAY(name, field)             \
  target->Set(context,                                \
              FIXED_ONE_BYTE_STRING(isolate, (name)), \
              (field)).FromJust()
  SET_STATE_TYPEDARRAY(
    "sessionConfig", state->quicsessionconfig_buffer.GetJSArray());
  SET_STATE_TYPEDARRAY(
    "http3Config", state->http3config_buffer.GetJSArray());
#undef SET_STATE_TYPEDARRAY

  env->set_quic_state(std::move(state));

  QuicSocket::Initialize(env, target, context);
  QuicEndpoint::Initialize(env, target, context);
  QuicSession::Initialize(env, target, context);
  QuicStream::Initialize(env, target, context);

  env->SetMethod(target,
                 "setCallbacks",
                 QuicSetCallbacks);
  env->SetMethod(target,
                 "initSecureContext",
                 QuicInitSecureContext<NGTCP2_CRYPTO_SIDE_SERVER>);
  env->SetMethod(target,
                 "initSecureContextClient",
                 QuicInitSecureContext<NGTCP2_CRYPTO_SIDE_CLIENT>);

  Local<Object> constants = Object::New(env->isolate());
  NODE_DEFINE_CONSTANT(constants, AF_INET);
  NODE_DEFINE_CONSTANT(constants, AF_INET6);
  NODE_DEFINE_CONSTANT(constants, DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL);
  NODE_DEFINE_CONSTANT(constants, DEFAULT_RETRYTOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(constants, DEFAULT_MAX_CONNECTIONS_PER_HOST);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_CERT_ENABLED);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_CLIENT_HELLO_ENABLED);
  NODE_DEFINE_CONSTANT(constants,
                       IDX_QUIC_SESSION_STATE_PATH_VALIDATED_ENABLED);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_KEYLOG_ENABLED);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_MAX_STREAMS_BIDI);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_MAX_STREAMS_UNI);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_MAX_DATA_LEFT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATE_BYTES_IN_FLIGHT);
  NODE_DEFINE_CONSTANT(constants, MAX_RETRYTOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(constants, MIN_RETRYTOKEN_EXPIRATION);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_MAX_CIDLEN);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_MIN_CIDLEN);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_NO_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_PROTO_VER);
  NODE_DEFINE_CONSTANT(constants, QUIC_ERROR_APPLICATION);
  NODE_DEFINE_CONSTANT(constants, QUIC_ERROR_CRYPTO);
  NODE_DEFINE_CONSTANT(constants, QUIC_ERROR_SESSION);
  NODE_DEFINE_CONSTANT(constants, QUIC_PREFERRED_ADDRESS_ACCEPT);
  NODE_DEFINE_CONSTANT(constants, QUIC_PREFERRED_ADDRESS_IGNORE);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_DEFAULT_MAX_ACK_DELAY);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_PATH_VALIDATION_RESULT_FAILURE);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_PATH_VALIDATION_RESULT_SUCCESS);
  NODE_DEFINE_CONSTANT(constants, SSL_OP_ALL);
  NODE_DEFINE_CONSTANT(constants, SSL_OP_CIPHER_SERVER_PREFERENCE);
  NODE_DEFINE_CONSTANT(constants, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
  NODE_DEFINE_CONSTANT(constants, SSL_OP_NO_ANTI_REPLAY);
  NODE_DEFINE_CONSTANT(constants, SSL_OP_SINGLE_ECDH_USE);
  NODE_DEFINE_CONSTANT(constants, TLS1_3_VERSION);
  NODE_DEFINE_CONSTANT(constants, UV_EBADF);

  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_ACTIVE_CONNECTION_ID_LIMIT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_LOCAL);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_REMOTE);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_STREAM_DATA_UNI);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_DATA);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_STREAMS_BIDI);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_STREAMS_UNI);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_IDLE_TIMEOUT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_PACKET_SIZE);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_ACK_DELAY_EXPONENT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_DISABLE_MIGRATION);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_ACK_DELAY);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_MAX_CRYPTO_BUFFER);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_CONFIG_COUNT);

  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_CREATED_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_HANDSHAKE_START_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_HANDSHAKE_SEND_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_HANDSHAKE_CONTINUE_AT);
  NODE_DEFINE_CONSTANT(constants,
                       IDX_QUIC_SESSION_STATS_HANDSHAKE_COMPLETED_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_HANDSHAKE_ACKED_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_SENT_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_RECEIVED_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_CLOSING_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_BYTES_RECEIVED);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_BYTES_SENT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_BIDI_STREAM_COUNT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_UNI_STREAM_COUNT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_STREAMS_IN_COUNT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_STREAMS_OUT_COUNT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_KEYUPDATE_COUNT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_RETRY_COUNT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_LOSS_RETRANSMIT_COUNT);
  NODE_DEFINE_CONSTANT(constants,
                       IDX_QUIC_SESSION_STATS_ACK_DELAY_RETRANSMIT_COUNT);
  NODE_DEFINE_CONSTANT(constants,
                       IDX_QUIC_SESSION_STATS_PATH_VALIDATION_SUCCESS_COUNT);
  NODE_DEFINE_CONSTANT(constants,
                       IDX_QUIC_SESSION_STATS_PATH_VALIDATION_FAILURE_COUNT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_MAX_BYTES_IN_FLIGHT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_BLOCK_COUNT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_STREAM_STATS_CREATED_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_STREAM_STATS_SENT_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_STREAM_STATS_RECEIVED_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_STREAM_STATS_ACKED_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_STREAM_STATS_CLOSING_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_STREAM_STATS_BYTES_RECEIVED);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_STREAM_STATS_BYTES_SENT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_STREAM_STATS_MAX_OFFSET);

  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_CREATED_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_BOUND_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_LISTEN_AT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_BYTES_RECEIVED);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_BYTES_SENT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_PACKETS_RECEIVED);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_PACKETS_IGNORED);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_PACKETS_SENT);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_SERVER_SESSIONS);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_CLIENT_SESSIONS);
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_STATELESS_RESET_COUNT);

  NODE_DEFINE_CONSTANT(constants, IDX_HTTP3_QPACK_MAX_TABLE_CAPACITY);
  NODE_DEFINE_CONSTANT(constants, IDX_HTTP3_QPACK_BLOCKED_STREAMS);
  NODE_DEFINE_CONSTANT(constants, IDX_HTTP3_MAX_HEADER_LIST_SIZE);
  NODE_DEFINE_CONSTANT(constants, IDX_HTTP3_MAX_PUSHES);
  NODE_DEFINE_CONSTANT(constants, IDX_HTTP3_CONFIG_COUNT);

  NODE_DEFINE_CONSTANT(constants, QUICSTREAM_HEADER_FLAGS_NONE);
  NODE_DEFINE_CONSTANT(constants, QUICSTREAM_HEADER_FLAGS_TERMINAL);

  NODE_DEFINE_CONSTANT(constants, QUICSTREAM_HEADERS_KIND_NONE);
  NODE_DEFINE_CONSTANT(constants, QUICSTREAM_HEADERS_KIND_INFORMATIONAL);
  NODE_DEFINE_CONSTANT(constants, QUICSTREAM_HEADERS_KIND_INITIAL);
  NODE_DEFINE_CONSTANT(constants, QUICSTREAM_HEADERS_KIND_TRAILING);

  NODE_DEFINE_CONSTANT(constants, MIN_MAX_CRYPTO_BUFFER);

  NODE_DEFINE_CONSTANT(
      constants,
      QUICSERVERSESSION_OPTION_REJECT_UNAUTHORIZED);
  NODE_DEFINE_CONSTANT(
      constants,
      QUICSERVERSESSION_OPTION_REQUEST_CERT);
  NODE_DEFINE_CONSTANT(
      constants,
      QUICCLIENTSESSION_OPTION_REQUEST_OCSP);
  NODE_DEFINE_CONSTANT(
      constants,
      QUICCLIENTSESSION_OPTION_VERIFY_HOSTNAME_IDENTITY);
  NODE_DEFINE_CONSTANT(
      constants,
      QUICSOCKET_OPTIONS_VALIDATE_ADDRESS);
  NODE_DEFINE_CONSTANT(
      constants,
      QUICSOCKET_OPTIONS_VALIDATE_ADDRESS_LRU);

  target->Set(context,
              env->constants_string(),
              constants).FromJust();

  constants->DefineOwnProperty(context,
      FIXED_ONE_BYTE_STRING(isolate, NODE_STRINGIFY_HELPER(NGTCP2_ALPN_H3)),
      FIXED_ONE_BYTE_STRING(isolate, NGTCP2_ALPN_H3),
      static_cast<PropertyAttribute>(ReadOnly | DontDelete)).Check();
}

}  // namespace quic
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(quic, node::quic::Initialize)
