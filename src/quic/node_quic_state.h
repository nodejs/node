#ifndef SRC_QUIC_NODE_QUIC_STATE_H_
#define SRC_QUIC_NODE_QUIC_STATE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_buffer.h"

namespace node {
namespace quic {

enum QuicSessionConfigIndex : int {
  IDX_QUIC_SESSION_ACTIVE_CONNECTION_ID_LIMIT,
  IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_LOCAL,
  IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_REMOTE,
  IDX_QUIC_SESSION_MAX_STREAM_DATA_UNI,
  IDX_QUIC_SESSION_MAX_DATA,
  IDX_QUIC_SESSION_MAX_STREAMS_BIDI,
  IDX_QUIC_SESSION_MAX_STREAMS_UNI,
  IDX_QUIC_SESSION_MAX_IDLE_TIMEOUT,
  IDX_QUIC_SESSION_MAX_UDP_PAYLOAD_SIZE,
  IDX_QUIC_SESSION_ACK_DELAY_EXPONENT,
  IDX_QUIC_SESSION_DISABLE_MIGRATION,
  IDX_QUIC_SESSION_MAX_ACK_DELAY,
  IDX_QUIC_SESSION_CC_ALGO,
  IDX_QUIC_SESSION_CONFIG_COUNT
};

enum Http3ConfigIndex : int {
  IDX_HTTP3_QPACK_MAX_TABLE_CAPACITY,
  IDX_HTTP3_QPACK_BLOCKED_STREAMS,
  IDX_HTTP3_MAX_HEADER_LIST_SIZE,
  IDX_HTTP3_MAX_PUSHES,
  IDX_HTTP3_MAX_HEADER_PAIRS,
  IDX_HTTP3_MAX_HEADER_LENGTH,
  IDX_HTTP3_CONFIG_COUNT
};

class QuicState : public BaseObject {
 public:
  explicit QuicState(Environment* env, v8::Local<v8::Object> obj)
    : BaseObject(env, obj),
      root_buffer(
        env->isolate(),
        sizeof(quic_state_internal)),
      quicsessionconfig_buffer(
        env->isolate(),
        offsetof(quic_state_internal, quicsessionconfig_buffer),
        IDX_QUIC_SESSION_CONFIG_COUNT + 1,
        root_buffer),
      http3config_buffer(
        env->isolate(),
        offsetof(quic_state_internal, http3config_buffer),
        IDX_HTTP3_CONFIG_COUNT + 1,
        root_buffer) {
  }

  AliasedUint8Array root_buffer;
  AliasedFloat64Array quicsessionconfig_buffer;
  AliasedFloat64Array http3config_buffer;

  bool warn_trace_tls = true;

  static constexpr FastStringKey binding_data_name { "quic" };

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_SELF_SIZE(QuicState)
  SET_MEMORY_INFO_NAME(QuicState)

 private:
  struct quic_state_internal {
    // doubles first so that they are always sizeof(double)-aligned
    double quicsessionconfig_buffer[IDX_QUIC_SESSION_CONFIG_COUNT + 1];
    double http3config_buffer[IDX_HTTP3_CONFIG_COUNT + 1];
  };
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_QUIC_NODE_QUIC_STATE_H_
