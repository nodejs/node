#ifndef SRC_NODE_QUIC_STATE_H_
#define SRC_NODE_QUIC_STATE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_buffer.h"

namespace node {

enum QuicSessionConfigIndex : int {
  IDX_QUIC_SESSION_ACTIVE_CONNECTION_ID_LIMIT,
  IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_LOCAL,
  IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_REMOTE,
  IDX_QUIC_SESSION_MAX_STREAM_DATA_UNI,
  IDX_QUIC_SESSION_MAX_DATA,
  IDX_QUIC_SESSION_MAX_STREAMS_BIDI,
  IDX_QUIC_SESSION_MAX_STREAMS_UNI,
  IDX_QUIC_SESSION_IDLE_TIMEOUT,
  IDX_QUIC_SESSION_MAX_PACKET_SIZE,
  IDX_QUIC_SESSION_ACK_DELAY_EXPONENT,
  IDX_QUIC_SESSION_DISABLE_MIGRATION,
  IDX_QUIC_SESSION_MAX_ACK_DELAY,
  IDX_QUIC_SESSION_MAX_CRYPTO_BUFFER,
  IDX_QUIC_SESSION_CONFIG_COUNT
};

enum Http3ConfigIndex : int {
  IDX_HTTP3_QPACK_MAX_TABLE_CAPACITY,
  IDX_HTTP3_QPACK_BLOCKED_STREAMS,
  IDX_HTTP3_MAX_HEADER_LIST_SIZE,
  IDX_HTTP3_MAX_PUSHES,
  IDX_HTTP3_CONFIG_COUNT
};

class QuicState {
 public:
  explicit QuicState(v8::Isolate* isolate) :
    root_buffer(
      isolate,
      sizeof(quic_state_internal)),
    quicsessionconfig_buffer(
      isolate,
      offsetof(quic_state_internal, quicsessionconfig_buffer),
      IDX_QUIC_SESSION_CONFIG_COUNT + 1,
      root_buffer),
    http3config_buffer(
      isolate,
      offsetof(quic_state_internal, http3config_buffer),
      IDX_HTTP3_CONFIG_COUNT +1,
      root_buffer) {
  }

  AliasedUint8Array root_buffer;
  AliasedFloat64Array quicsessionconfig_buffer;
  AliasedFloat64Array http3config_buffer;

 private:
  struct quic_state_internal {
    // doubles first so that they are always sizeof(double)-aligned
    double quicsessionconfig_buffer[IDX_QUIC_SESSION_CONFIG_COUNT + 1];
    double http3config_buffer[IDX_HTTP3_CONFIG_COUNT + 1];
  };
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_QUIC_STATE_H_
