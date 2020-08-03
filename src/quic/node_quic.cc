#include "debug_utils-inl.h"
#include "node.h"
#include "env-inl.h"
#include "node_crypto.h"  // SecureContext
#include "node_crypto_common.h"
#include "node_errors.h"
#include "node_process.h"
#include "node_quic_crypto.h"
#include "node_quic_session-inl.h"
#include "node_quic_socket-inl.h"
#include "node_quic_stream-inl.h"
#include "node_quic_state.h"
#include "node_quic_util-inl.h"
#include "node_sockaddr-inl.h"
#include "nghttp3/nghttp3.h"

#include <memory>
#include <utility>

namespace node {

using crypto::SecureContext;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

namespace quic {

constexpr FastStringKey QuicState::binding_data_name;

void QuicState::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("root_buffer", root_buffer);
}

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
  SETFUNCTION("onSessionReady", session_ready);
  SETFUNCTION("onSessionCert", session_cert);
  SETFUNCTION("onSessionClientHello", session_client_hello);
  SETFUNCTION("onSessionClose", session_close);
  SETFUNCTION("onSessionHandshake", session_handshake);
  SETFUNCTION("onSessionKeylog", session_keylog);
  SETFUNCTION("onSessionUsePreferredAddress", session_use_preferred_address);
  SETFUNCTION("onSessionPathValidation", session_path_validation);
  SETFUNCTION("onSessionQlog", session_qlog);
  SETFUNCTION("onSessionStatus", session_status);
  SETFUNCTION("onSessionTicket", session_ticket);
  SETFUNCTION("onSessionVersionNegotiation", session_version_negotiation);
  SETFUNCTION("onStreamReady", stream_ready);
  SETFUNCTION("onStreamClose", stream_close);
  SETFUNCTION("onStreamError", stream_error);
  SETFUNCTION("onStreamReset", stream_reset);
  SETFUNCTION("onSocketServerBusy", socket_server_busy);
  SETFUNCTION("onStreamHeaders", stream_headers);
  SETFUNCTION("onStreamBlocked", stream_blocked);

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
  CHECK(args[2]->IsBoolean());  // early data

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args[0].As<Object>(),
                          args.GetReturnValue().Set(UV_EBADF));
  const node::Utf8Value groups(env->isolate(), args[1]);

  bool early_data = args[2]->BooleanValue(env->isolate());

  InitializeSecureContext(
      BaseObjectPtr<SecureContext>(sc),
      early_data,
      side);

  if (!crypto::SetGroups(sc, *groups))
    THROW_ERR_QUIC_CANNOT_SET_GROUPS(env);
}
}  // namespace

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);

  HistogramBase::Initialize(env);

  QuicState* const state =
      env->AddBindingData<QuicState>(context, target);
  if (state == nullptr) return;

#define SET_STATE_TYPEDARRAY(name, field)             \
  target->Set(context,                                \
              FIXED_ONE_BYTE_STRING(isolate, (name)), \
              (field.GetJSArray())).Check()
  SET_STATE_TYPEDARRAY("sessionConfig", state->quicsessionconfig_buffer);
  SET_STATE_TYPEDARRAY("http3Config", state->http3config_buffer);
#undef SET_STATE_TYPEDARRAY

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

// TODO(@jasnell): Audit which constants are actually being used in JS
#define QUIC_CONSTANTS(V)                                                      \
  V(DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL)                                        \
  V(DEFAULT_RETRYTOKEN_EXPIRATION)                                             \
  V(DEFAULT_MAX_CONNECTIONS)                                                   \
  V(DEFAULT_MAX_CONNECTIONS_PER_HOST)                                          \
  V(DEFAULT_MAX_STATELESS_RESETS_PER_HOST)                                     \
  V(IDX_HTTP3_QPACK_MAX_TABLE_CAPACITY)                                        \
  V(IDX_HTTP3_QPACK_BLOCKED_STREAMS)                                           \
  V(IDX_HTTP3_MAX_HEADER_LIST_SIZE)                                            \
  V(IDX_HTTP3_MAX_PUSHES)                                                      \
  V(IDX_HTTP3_MAX_HEADER_PAIRS)                                                \
  V(IDX_HTTP3_MAX_HEADER_LENGTH)                                               \
  V(IDX_HTTP3_CONFIG_COUNT)                                                    \
  V(IDX_QUIC_SESSION_ACTIVE_CONNECTION_ID_LIMIT)                               \
  V(IDX_QUIC_SESSION_MAX_IDLE_TIMEOUT)                                         \
  V(IDX_QUIC_SESSION_MAX_DATA)                                                 \
  V(IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_LOCAL)                               \
  V(IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_REMOTE)                              \
  V(IDX_QUIC_SESSION_MAX_STREAM_DATA_UNI)                                      \
  V(IDX_QUIC_SESSION_MAX_STREAMS_BIDI)                                         \
  V(IDX_QUIC_SESSION_MAX_STREAMS_UNI)                                          \
  V(IDX_QUIC_SESSION_MAX_UDP_PAYLOAD_SIZE)                                     \
  V(IDX_QUIC_SESSION_ACK_DELAY_EXPONENT)                                       \
  V(IDX_QUIC_SESSION_DISABLE_MIGRATION)                                        \
  V(IDX_QUIC_SESSION_MAX_ACK_DELAY)                                            \
  V(IDX_QUIC_SESSION_CC_ALGO)                                                  \
  V(IDX_QUIC_SESSION_CONFIG_COUNT)                                             \
  V(MAX_RETRYTOKEN_EXPIRATION)                                                 \
  V(MIN_RETRYTOKEN_EXPIRATION)                                                 \
  V(NGTCP2_APP_NOERROR)                                                        \
  V(NGTCP2_PATH_VALIDATION_RESULT_FAILURE)                                     \
  V(NGTCP2_PATH_VALIDATION_RESULT_SUCCESS)                                     \
  V(NGTCP2_CC_ALGO_CUBIC)                                                      \
  V(NGTCP2_CC_ALGO_RENO)                                                       \
  V(QUIC_ERROR_APPLICATION)                                                    \
  V(QUIC_ERROR_CRYPTO)                                                         \
  V(QUIC_ERROR_SESSION)                                                        \
  V(QUIC_PREFERRED_ADDRESS_USE)                                                \
  V(QUIC_PREFERRED_ADDRESS_IGNORE)                                             \
  V(QUICCLIENTSESSION_OPTION_REQUEST_OCSP)                                     \
  V(QUICCLIENTSESSION_OPTION_VERIFY_HOSTNAME_IDENTITY)                         \
  V(QUICSERVERSESSION_OPTION_REJECT_UNAUTHORIZED)                              \
  V(QUICSERVERSESSION_OPTION_REQUEST_CERT)                                     \
  V(QUICSOCKET_OPTIONS_VALIDATE_ADDRESS)                                       \
  V(QUICSTREAM_HEADER_FLAGS_NONE)                                              \
  V(QUICSTREAM_HEADER_FLAGS_TERMINAL)                                          \
  V(QUICSTREAM_HEADERS_KIND_NONE)                                              \
  V(QUICSTREAM_HEADERS_KIND_INFORMATIONAL)                                     \
  V(QUICSTREAM_HEADERS_KIND_PUSH)                                              \
  V(QUICSTREAM_HEADERS_KIND_INITIAL)                                           \
  V(QUICSTREAM_HEADERS_KIND_TRAILING)                                          \
  V(ERR_FAILED_TO_CREATE_SESSION)                                              \
  V(UV_EBADF)

#define V(id, _, __)                                                           \
  NODE_DEFINE_CONSTANT(constants, IDX_QUICSESSION_STATE_##id);
  QUICSESSION_SHARED_STATE(V)
#undef V

#define V(id, _, __)                                                           \
  NODE_DEFINE_CONSTANT(constants, IDX_QUICSOCKET_STATE_##id);
  QUICSOCKET_SHARED_STATE(V)
#undef V

#define V(id, _, __)                                                           \
  NODE_DEFINE_CONSTANT(constants, IDX_QUICSTREAM_STATE_##id);
  QUICSTREAM_SHARED_STATE(V)
#undef V

#define V(name, _, __)                                                         \
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SESSION_STATS_##name);
  SESSION_STATS(V)
#undef V

#define V(name, _, __)                                                         \
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_SOCKET_STATS_##name);
  SOCKET_STATS(V)
#undef V

#define V(name, _, __)                                                         \
  NODE_DEFINE_CONSTANT(constants, IDX_QUIC_STREAM_STATS_##name);
  STREAM_STATS(V)
#undef V

#define V(name) NODE_DEFINE_CONSTANT(constants, name);
  QUIC_CONSTANTS(V)
#undef V

  NODE_DEFINE_CONSTANT(constants, NGTCP2_DEFAULT_MAX_PKTLEN);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_PROTO_VER);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_DEFAULT_MAX_ACK_DELAY);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_MAX_CIDLEN);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_MIN_CIDLEN);

  NODE_DEFINE_CONSTANT(constants, NGTCP2_NO_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_INTERNAL_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_CONNECTION_REFUSED);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_FLOW_CONTROL_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_STREAM_LIMIT_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_STREAM_STATE_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_FINAL_SIZE_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_FRAME_ENCODING_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_TRANSPORT_PARAMETER_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_CONNECTION_ID_LIMIT_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_PROTOCOL_VIOLATION);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_INVALID_TOKEN);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_APPLICATION_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_CRYPTO_BUFFER_EXCEEDED);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_KEY_UPDATE_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGTCP2_CRYPTO_ERROR);

  NODE_DEFINE_CONSTANT(constants, AF_INET);
  NODE_DEFINE_CONSTANT(constants, AF_INET6);
  NODE_DEFINE_STRING_CONSTANT(constants,
                              NODE_STRINGIFY_HELPER(NGHTTP3_ALPN_H3),
                              NGHTTP3_ALPN_H3);

  target->Set(context, env->constants_string(), constants).FromJust();
}

}  // namespace quic
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(quic, node::quic::Initialize)
