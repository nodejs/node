#ifndef SRC_QUIC_STREAM_H_
#define SRC_QUIC_STREAM_H_
#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_struct.h"
#include "async_wrap.h"
#include "base_object.h"
#include "env.h"
#include "node_http_common.h"
#include "quic/quic.h"
#include "quic/buffer.h"
#include "quic/stats.h"

#include <ngtcp2/ngtcp2.h>
#include <vector>

namespace node {
namespace quic {

#define STREAM_STATS(V)                                                        \
  V(CREATED_AT, created_at, "Created At")                                      \
  V(RECEIVED_AT, received_at, "Last Received At")                              \
  V(ACKED_AT, acked_at, "Last Acknowledged At")                                \
  V(CLOSING_AT, closing_at, "Closing At")                                      \
  V(DESTROYED_AT, destroyed_at, "Destroyed At")                                \
  V(BYTES_RECEIVED, bytes_received, "Bytes Received")                          \
  V(BYTES_SENT, bytes_sent, "Bytes Sent")                                      \
  V(MAX_OFFSET, max_offset, "Max Offset")                                      \
  V(MAX_OFFSET_ACK, max_offset_ack, "Max Acknowledged Offset")                 \
  V(MAX_OFFSET_RECV, max_offset_received, "Max Received Offset")               \
  V(FINAL_SIZE, final_size, "Final Size")

#define STREAM_STATE(V)                                                        \
  V(FIN_SENT, fin_sent, uint8_t)                                               \
  V(FIN_RECEIVED, fin_received, uint8_t)                                       \
  V(READ_ENDED, read_ended, uint8_t)                                           \
  V(TRAILERS, trailers, uint8_t)

class Stream;

#define V(name, _, __) IDX_STATS_STREAM_##name,
enum StreamStatsIdx {
  STREAM_STATS(V)
  IDX_STATS_STREAM_COUNT
};
#undef V

#define V(name, _, __) IDX_STATE_STREAM_##name,
enum StreamStateIdx {
  STREAM_STATE(V)
  IDX_STATE_STREAM_COUNT
};
#undef V

#define V(_, name, __) uint64_t name;
struct StreamStats final {
  STREAM_STATS(V)
};
#undef V

using StreamStatsBase = StatsBase<StatsTraitsImpl<StreamStats, Stream>>;

// QUIC Stream's are simple data flows that may be:
//
// * Bidirectional or Unidirectional
// * Server or Client Initiated
//
// The flow direction and origin of the stream are important in
// determining the write and read state (Open or Closed). Specifically:
//
// A Unidirectional stream originating with the Server is:
//
// * Server Writable (Open) but not Client Writable (Closed)
// * Client Readable (Open) but not Server Readable (Closed)
//
// Likewise, a Unidirectional stream originating with the
// Client is:
//
// * Client Writable (Open) but not Server Writable (Closed)
// * Server Readable (Open) but not Client Readable (Closed)
//
// Bidirectional Stream States
// +------------+--------------+--------------------+---------------------+
// |            | Initiated By | Initial Read State | Initial Write State |
// +------------+--------------+--------------------+---------------------+
// | On Server  |   Server     |        Open        |         Open        |
// +------------+--------------+--------------------+---------------------+
// | On Server  |   Client     |        Open        |         Open        |
// +------------+--------------+--------------------+---------------------+
// | On Client  |   Server     |        Open        |         Open        |
// +------------+--------------+--------------------+---------------------+
// | On Client  |   Client     |        Open        |         Open        |
// +------------+--------------+--------------------+---------------------+
//
// Unidirectional Stream States
// +------------+--------------+--------------------+---------------------+
// |            | Initiated By | Initial Read State | Initial Write State |
// +------------+--------------+--------------------+---------------------+
// | On Server  |   Server     |       Closed       |         Open        |
// +------------+--------------+--------------------+---------------------+
// | On Server  |   Client     |        Open        |        Closed       |
// +------------+--------------+--------------------+---------------------+
// | On Client  |   Server     |        Open        |        Closed       |
// +------------+--------------+--------------------+---------------------+
// | On Client  |   Client     |       Closed       |         Open        |
// +------------+--------------+--------------------+---------------------+
//
// All data sent via the Stream is buffered internally until either
// receipt is acknowledged from the peer or attempts to send are abandoned.
//
// A Stream may be in a fully closed state (No longer readable nor
// writable) state but still have unacknowledged data in it's outbound queue.
//
// A Stream is gracefully closed when (a) both Read and Write states
// are Closed, (b) all queued data has been acknowledged.
//
// The Stream may be forcefully closed immediately using destroy(err).
// This causes all queued data and pending JavaScript writes to be
// abandoned, and causes the Stream to be immediately closed at the
// ngtcp2 level.

class Stream final : public AsyncWrap,
                     public bob::SourceImpl<ngtcp2_vec>,
                     public StreamStatsBase {
 public:
  using Header = NgHeaderBase<BindingState>;

  enum class Origin {
    SERVER,
    CLIENT,
  };

  enum class Direction {
    UNIDIRECTIONAL,
    BIDIRECTIONAL,
  };

  enum class HeadersKind {
    INFO,
    INITIAL,
    TRAILING,
  };

  struct State final {
#define V(_, name, type) type name;
    STREAM_STATE(V)
#undef V
  };

  static bool HasInstance(Environment* env, const v8::Local<v8::Object>& obj);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> object);

  static void DoDestroy(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AttachSource(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AttachConsumer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoSendHeaders(const v8::FunctionCallbackInfo<v8::Value>& args);

  static BaseObjectPtr<Stream> Create(
      Environment* env,
      Session* session,
      stream_id id);

  Stream(
      Session* session,
      v8::Local<v8::Object> object,
      stream_id id,
      Buffer::Source* source = nullptr);

  ~Stream() override;

  // Acknowledge is called when ngtcp2 has received an acknowledgement
  // for one or more stream frames for this Stream.
  void Acknowledge(uint64_t offset, size_t datalen);

  // Returns false if the header cannot be added. This will
  // typically only happen if a maximimum number of headers,
  // or the maximum total header length is received.
  bool AddHeader(std::unique_ptr<Header> header);

  // Attaches the given Buffer::Consumer to this Stream to consume inbound data.
  void AttachInboundConsumer(
      Buffer::Consumer* consumer,
      BaseObjectPtr<AsyncWrap> strong_ptr = BaseObjectPtr<AsyncWrap>());

  // Attaches an outbound Buffer::Source
  void AttachOutboundSource(Buffer::Source* source);

  // Signals the beginning of a new block of headers.
  void BeginHeaders(HeadersKind kind);

  void OnBlocked();
  void OnReset(error_code app_error_code);

  void Commit(size_t ammount);

  int DoPull(
      bob::Next<ngtcp2_vec> next,
      int options,
      ngtcp2_vec* data,
      size_t count,
      size_t max_count_hint) override;

  // Signals the ending of the current block of headers.
  void EndHeaders();

//  void Destroy(const QuicError& error = kQuicNoError);

  void OnClose(error_code app_error_code);

  void Destroy();

  void ReadyForTrailers();

  void ReceiveData(
      uint32_t flags,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset);

  // ResetStream will cause ngtcp2 to queue a RESET_STREAM and STOP_SENDING
  // frame, as appropriate, for the given stream_id. For a locally-initiated
  // unidirectional stream, only a RESET_STREAM frame will be scheduled and
  // the stream will be immediately closed. For a bidirectional stream, a
  // STOP_SENDING frame will be sent.
  void ResetStream(const QuicError& error = kQuicAppNoError);

  void Resume();

  void StopSending(const QuicError& error = kQuicAppNoError);

  enum class SendHeadersFlags {
    NONE,
    TERMINAL,
  };

  // Sends headers to the QUIC Application If headers are not supported,
  // false will be returned. Otherwise, returns true
  bool SendHeaders(
      HeadersKind kind,
      const v8::Local<v8::Array>& headers,
      SendHeadersFlags flags = SendHeadersFlags::NONE);

  inline bool is_destroyed() const { return destroyed_; }

  inline Direction direction() const {
    return id_ & 0b10 ?
        Direction::UNIDIRECTIONAL :
        Direction::BIDIRECTIONAL;
  }

  inline stream_id id() const { return id_; }

  inline Origin origin() const {
    return id_ & 0b01 ? Origin::SERVER : Origin::CLIENT;
  }

  inline Session* session() const { return session_.get(); }

  inline void set_destroyed() { destroyed_ = true; }

  // Set the final size for the Stream. This only works
  // the first time it is called. Subsequent calls will be
  // ignored unless the subsequent size is greater than the
  // prior set size, in which case we have a bug and we'll
  // assert.
  void set_final_size(uint64_t final_size);

  // The final size is the maximum amount of data that has been
  // acknowleged to have been received for a Stream.
  inline uint64_t final_size() const {
    return GetStat(&StreamStats::final_size);
  }

  inline void set_headers_kind(HeadersKind kind) { headers_kind_ = kind; }

  inline bool trailers() const { return state_->trailers; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Stream)
  SET_SELF_SIZE(Stream)

 private:
  void UpdateStats(size_t datalen);
  void ProcessInbound();

  BaseObjectPtr<Session> session_;
  AliasedStruct<State> state_;
  stream_id id_;
  bool destroyed_ = false;

  // The outbound_source_ provides the data that is to be
  // sent by this Stream. After the source is read
  // the writable side of the Stream will be closed
  // by sending a fin data frame.
  Buffer::Source* outbound_source_ = nullptr;
  BaseObjectPtr<BaseObject> outbound_source_strong_ptr_;

  // The inbound_ buffer contains the data that has been
  // received by this Stream. The received data will
  // be buffered in inbound_ until an inbound_consumer_
  // is attached. Only a single inbound_consumer_ may be
  // attached at a time.
  Buffer inbound_;
  Buffer::Consumer* inbound_consumer_ = nullptr;
  BaseObjectPtr<AsyncWrap> inbound_consumer_strong_ptr_;

  std::vector<v8::Local<v8::Value>> headers_;
  HeadersKind headers_kind_;

  // The current total byte length of the headers
  size_t current_headers_length_ = 0;

  ListNode<Stream> stream_queue_;

 public:
  // The Queue/Schedule/Unschedule here are part of the mechanism used
  // to determine which streams have data to send on the session. When
  // a stream potentially has data available, it will be scheduled in
  // the Queue. Then, when the Session::Application starts sending
  // pending data, it will check the queue to see if there are streams
  // wait. If there are, it will grab one and check to see if there is
  // data to send. When a stream does not have data to send (such as
  // when it is initially created or is using an async source that is
  // still waiting for data to be pushed) it will not appear in the
  // queue.
  using Queue = ListHead<Stream, &Stream::stream_queue_>;

  void Schedule(Queue* queue);

  inline void Unschedule() { stream_queue_.Remove(); }
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_QUIC_STREAM_H_
