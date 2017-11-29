#ifndef SRC_NODE_HTTP2_STATE_H_
#define SRC_NODE_HTTP2_STATE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_buffer.h"

namespace node {
namespace http2 {

  enum Http2SettingsIndex {
    IDX_SETTINGS_HEADER_TABLE_SIZE,
    IDX_SETTINGS_ENABLE_PUSH,
    IDX_SETTINGS_INITIAL_WINDOW_SIZE,
    IDX_SETTINGS_MAX_FRAME_SIZE,
    IDX_SETTINGS_MAX_CONCURRENT_STREAMS,
    IDX_SETTINGS_MAX_HEADER_LIST_SIZE,
    IDX_SETTINGS_COUNT
  };

  enum Http2SessionStateIndex {
    IDX_SESSION_STATE_EFFECTIVE_LOCAL_WINDOW_SIZE,
    IDX_SESSION_STATE_EFFECTIVE_RECV_DATA_LENGTH,
    IDX_SESSION_STATE_NEXT_STREAM_ID,
    IDX_SESSION_STATE_LOCAL_WINDOW_SIZE,
    IDX_SESSION_STATE_LAST_PROC_STREAM_ID,
    IDX_SESSION_STATE_REMOTE_WINDOW_SIZE,
    IDX_SESSION_STATE_OUTBOUND_QUEUE_SIZE,
    IDX_SESSION_STATE_HD_DEFLATE_DYNAMIC_TABLE_SIZE,
    IDX_SESSION_STATE_HD_INFLATE_DYNAMIC_TABLE_SIZE,
    IDX_SESSION_STATE_COUNT
  };

  enum Http2StreamStateIndex {
    IDX_STREAM_STATE,
    IDX_STREAM_STATE_WEIGHT,
    IDX_STREAM_STATE_SUM_DEPENDENCY_WEIGHT,
    IDX_STREAM_STATE_LOCAL_CLOSE,
    IDX_STREAM_STATE_REMOTE_CLOSE,
    IDX_STREAM_STATE_LOCAL_WINDOW_SIZE,
    IDX_STREAM_STATE_COUNT
  };

  enum Http2OptionsIndex {
    IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE,
    IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS,
    IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH,
    IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS,
    IDX_OPTIONS_PADDING_STRATEGY,
    IDX_OPTIONS_MAX_HEADER_LIST_PAIRS,
    IDX_OPTIONS_MAX_OUTSTANDING_PINGS,
    IDX_OPTIONS_FLAGS
  };

  enum Http2PaddingBufferFields {
    PADDING_BUF_FRAME_LENGTH,
    PADDING_BUF_MAX_PAYLOAD_LENGTH,
    PADDING_BUF_RETURN_VALUE,
    PADDING_BUF_FIELD_COUNT
  };

class http2_state {
 public:
  explicit http2_state(v8::Isolate* isolate) :
    root_buffer(
      isolate,
      sizeof(http2_state_internal)),
    session_state_buffer(
      isolate,
      offsetof(http2_state_internal, session_state_buffer),
      IDX_SESSION_STATE_COUNT,
      root_buffer),
    stream_state_buffer(
      isolate,
      offsetof(http2_state_internal, stream_state_buffer),
      IDX_STREAM_STATE_COUNT,
      root_buffer),
    padding_buffer(
      isolate,
      offsetof(http2_state_internal, padding_buffer),
      PADDING_BUF_FIELD_COUNT,
      root_buffer),
    options_buffer(
      isolate,
      offsetof(http2_state_internal, options_buffer),
      IDX_OPTIONS_FLAGS + 1,
      root_buffer),
    settings_buffer(
      isolate,
      offsetof(http2_state_internal, settings_buffer),
      IDX_SETTINGS_COUNT + 1,
      root_buffer) {
  }

  AliasedBuffer<uint8_t, v8::Uint8Array> root_buffer;
  AliasedBuffer<double, v8::Float64Array> session_state_buffer;
  AliasedBuffer<double, v8::Float64Array> stream_state_buffer;
  AliasedBuffer<uint32_t, v8::Uint32Array> padding_buffer;
  AliasedBuffer<uint32_t, v8::Uint32Array> options_buffer;
  AliasedBuffer<uint32_t, v8::Uint32Array> settings_buffer;

 private:
  struct http2_state_internal {
    // doubles first so that they are always sizeof(double)-aligned
    double session_state_buffer[IDX_SESSION_STATE_COUNT];
    double stream_state_buffer[IDX_STREAM_STATE_COUNT];
    uint32_t padding_buffer[PADDING_BUF_FIELD_COUNT];
    uint32_t options_buffer[IDX_OPTIONS_FLAGS + 1];
    uint32_t settings_buffer[IDX_SETTINGS_COUNT + 1];
  };
};

}  // namespace http2
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_HTTP2_STATE_H_
