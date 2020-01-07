#ifndef SRC_QUIC_NODE_QUIC_STREAM_H_
#define SRC_QUIC_NODE_QUIC_STREAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "memory_tracker-inl.h"
#include "async_wrap.h"
#include "env.h"
#include "histogram-inl.h"
#include "node_quic_util.h"
#include "stream_base-inl.h"
#include "util-inl.h"
#include "v8.h"

#include <string>
#include <vector>

namespace node {
namespace quic {

class QuicSession;
class QuicApplication;

enum QuicStreamHeaderFlags : uint32_t {
  // No flags
  QUICSTREAM_HEADER_FLAGS_NONE = 0,

  // Set if the initial headers are considered
  // terminal (that is, the stream should be closed
  // after transmitting the headers). If headers are
  // not supported by the QUIC Application, flag is
  // ignored.
  QUICSTREAM_HEADER_FLAGS_TERMINAL = 1
};

enum QuicStreamHeadersKind : int {
  QUICSTREAM_HEADERS_KIND_NONE = 0,
  QUICSTREAM_HEADERS_KIND_INFORMATIONAL,
  QUICSTREAM_HEADERS_KIND_INITIAL,
  QUICSTREAM_HEADERS_KIND_TRAILING
};

enum QuicStreamStatsIdx : int {
  IDX_QUIC_STREAM_STATS_CREATED_AT,
  IDX_QUIC_STREAM_STATS_SENT_AT,
  IDX_QUIC_STREAM_STATS_RECEIVED_AT,
  IDX_QUIC_STREAM_STATS_ACKED_AT,
  IDX_QUIC_STREAM_STATS_CLOSING_AT,
  IDX_QUIC_STREAM_STATS_BYTES_RECEIVED,
  IDX_QUIC_STREAM_STATS_BYTES_SENT,
  IDX_QUIC_STREAM_STATS_MAX_OFFSET
};

// QuicHeader is a base class for implementing QUIC application
// specific headers. Each type of QUIC application may have
// different internal representations for a header name+value
// pair. QuicApplication implementations that support headers
// per stream must create a specialization of the Header class.
class QuicHeader : public MemoryRetainer {
 public:
  QuicHeader() {}

  virtual ~QuicHeader() {}
  virtual v8::MaybeLocal<v8::String> GetName(QuicApplication* app) const = 0;
  virtual v8::MaybeLocal<v8::String> GetValue(QuicApplication* app) const = 0;
  virtual std::string name() const = 0;
  virtual std::string value() const = 0;

  // Returns the total length of the header in bytes
  // (including the name and value)
  virtual size_t length() const = 0;
};

enum QuicStreamStates : uint32_t {
  // QuicStream is fully open. Readable and Writable
  QUICSTREAM_FLAG_INITIAL = 0x0,

  // QuicStream Read State is closed because a final stream frame
  // has been received from the peer or the QuicStream is unidirectional
  // outbound only (i.e. it was never readable)
  QUICSTREAM_FLAG_READ_CLOSED = 0x1,

  // QuicStream Write State is closed. There may still be data
  // in the outbound queue waiting to be serialized or acknowledged.
  // No additional data may be added to the queue, however, and a
  // final stream packet will be sent once all of the data in the
  // queue has been serialized.
  QUICSTREAM_FLAG_WRITE_CLOSED = 0x2,

  // JavaScript side has switched into flowing mode (Readable side)
  QUICSTREAM_FLAG_READ_STARTED = 0x4,

  // JavaScript side has paused the flow of data (Readable side)
  QUICSTREAM_FLAG_READ_PAUSED = 0x8,

  // QuicStream has received a final stream frame (Readable side)
  QUICSTREAM_FLAG_FIN = 0x10,

  // QuicStream has sent a final stream frame (Writable side)
  QUICSTREAM_FLAG_FIN_SENT = 0x20,

  // QuicStream has been destroyed
  QUICSTREAM_FLAG_DESTROYED = 0x40
};

enum QuicStreamDirection {
  // The QuicStream is readable and writable in both directions
  QUIC_STREAM_BIRECTIONAL,

  // The QuicStream is writable and readable in only one direction.
  // The direction depends on the QuicStreamOrigin.
  QUIC_STREAM_UNIDIRECTIONAL
};

enum QuicStreamOrigin {
  // The QuicStream was created by the server.
  QUIC_STREAM_SERVER,

  // The QuicStream was created by the client.
  QUIC_STREAM_CLIENT
};

// QuicStream's are simple data flows that, fortunately, do not
// require much. They may be:
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
// All data sent via the QuicStream is buffered internally until either
// receipt is acknowledged from the peer or attempts to send are abandoned.
//
// A QuicStream may be in a fully Closed (Read and Write) state but still
// have unacknowledged data in it's outbound queue.
//
// A QuicStream is gracefully closed when (a) both Read and Write states
// are Closed, (b) all queued data has been acknowledged.
//
// The JavaScript Writable side of the QuicStream may be shutdown before
// all pending queued data has been serialized to frames. During this state,
// no additional data may be queued to send.
//
// The Write state of a QuicStream will not be closed while there is still
// pending writes on the JavaScript side.
//
// The QuicStream may be forcefully closed immediately using destroy(err).
// This causes all queued data and pending JavaScript writes to be
// abandoned, and causes the QuicStream to be immediately closed at the
// ngtcp2 level.
class QuicStream : public AsyncWrap, public StreamBase {
 public:
  static void Initialize(
      Environment* env,
      v8::Local<v8::Object> target,
      v8::Local<v8::Context> context);

  static BaseObjectPtr<QuicStream> New(
      QuicSession* session,
      int64_t stream_id);

  QuicStream(
      QuicSession* session,
      v8::Local<v8::Object> target,
      int64_t stream_id);

  std::string diagnostic_name() const override;

  int64_t id() const { return stream_id_; }
  QuicSession* session() const { return session_.get(); }

  // A QuicStream can be either uni- or bi-directional.
  inline QuicStreamDirection direction() const;

  // A QuicStream can be initiated by either the client
  // or the server.
  inline QuicStreamOrigin origin() const;

  // The QuicStream has been destroyed and is no longer usable.
  inline bool is_destroyed() const;

  // The QUICSTREAM_FLAG_FIN flag will be set only when a final stream
  // frame has been received from the peer.
  inline bool has_received_fin() const;

  // The QUICSTREAM_FLAG_FIN_SENT flag will be set only when a final
  // stream frame has been transmitted to the peer. Once sent, no
  // additional data may be transmitted to the peer. If has_sent_fin()
  // is set, is_writable() can be assumed to be false.
  inline bool has_sent_fin() const;

  // WasEverWritable returns true if it is a bidirectional stream,
  // or a Unidirectional stream originating from the local peer.
  // If was_ever_writable() is false, then no stream frames should
  // ever be sent from the local peer, including final stream frames.
  inline bool was_ever_writable() const;

  // A QuicStream will not be writable if:
  //  - The QUICSTREAM_FLAG_WRITE_CLOSED flag is set or
  //  - It is a Unidirectional stream originating from the peer
  inline bool is_writable() const;

  // WasEverReadable returns true if it is a bidirectional stream,
  // or a Unidirectional stream originating from the remote
  // peer.
  inline bool was_ever_readable() const;

  // A QuicStream will not be readable if:
  //  - The QUICSTREAM_FLAG_READ_CLOSED flag is set or
  //  - It is a Unidirectional stream originating from the local peer.
  inline bool is_readable() const;

  // True if reading from this QuicStream has ever initiated.
  inline bool is_read_started() const;

  // True if reading from this QuicStream is currently paused.
  inline bool is_read_paused() const;

  // Records the fact that a final stream frame has been
  // serialized and sent to the peer. There still may be
  // unacknowledged data in the outbound queue, but no
  // additional frames may be sent for the stream other
  // than reset stream.
  inline void set_fin_sent();

  // IsWriteFinished will return true if a final stream frame
  // has been sent and all data has been acknowledged (the
  // send buffer is empty).
  inline bool is_write_finished() const;

  // Specifies the kind of headers currently being processed.
  inline void set_headers_kind(QuicStreamHeadersKind kind);

  // Marks the given data range as having been acknowledged.
  // This means that the data range may be released from
  // memory.
  void Acknowledge(uint64_t offset, size_t datalen);

  // Destroy the QuicStream and render it no longer usable.
  void Destroy();

  // Buffers chunks of data to be written to the QUIC connection.
  int DoWrite(
      WriteWrap* req_wrap,
      uv_buf_t* bufs,
      size_t nbufs,
      uv_stream_t* send_handle) override;

  inline void IncrementAvailableOutboundLength(size_t amount);
  inline void DecrementAvailableOutboundLength(size_t amount);

  // Returns false if the header cannot be added. This will
  // typically only happen if a maximimum number of headers
  // has been reached.
  bool AddHeader(std::unique_ptr<QuicHeader> header);

  // Some QUIC applications support headers, others do not.
  // The following methods allow consistent handling of
  // headers at the QuicStream level regardless of the
  // protocol. For applications that do not support headers,
  // these are simply not used.
  inline void BeginHeaders(
      QuicStreamHeadersKind kind = QUICSTREAM_HEADERS_KIND_NONE);

  // Indicates an amount of unacknowledged data that has been
  // submitted to the QUIC connection.
  inline void Commit(ssize_t amount);

  template <typename T>
  inline size_t DrainInto(
      std::vector<T>* vec,
      size_t max_count = MAX_VECTOR_COUNT);

  template <typename T>
  inline size_t DrainInto(
      T* vec,
      size_t* count,
      size_t max_count = MAX_VECTOR_COUNT);

  inline void EndHeaders();

  // Passes a chunk of data on to the QuicStream listener.
  void ReceiveData(
      int fin,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset);

  // Resets the QUIC stream, sending a signal to the peer that
  // no additional data will be transmitted for this stream.
  inline void ResetStream(uint64_t app_error_code = 0);

  // Submits informational headers. Returns false if headers are not
  // supported on the underlying QuicApplication.
  inline bool SubmitInformation(v8::Local<v8::Array> headers);

  // Submits initial headers. Returns false if headers are not
  // supported on the underlying QuicApplication.
  inline bool SubmitHeaders(v8::Local<v8::Array> headers, uint32_t flags);

  // Submits trailing headers. Returns false if headers are not
  // supported on the underlying QuicApplication.
  inline bool SubmitTrailers(v8::Local<v8::Array> headers);

  // Required for StreamBase
  bool IsAlive() override;
  bool IsClosing() override;
  int ReadStart() override;
  int ReadStop() override;
  int DoShutdown(ShutdownWrap* req_wrap) override;

  AsyncWrap* GetAsyncWrap() override { return this; }

  // Required for MemoryRetainer
  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicStream)
  SET_SELF_SIZE(QuicStream)

 private:
  inline void set_fin_received();
  inline void set_write_close();
  inline void set_read_close();
  inline void set_read_start();
  inline void set_read_pause();
  inline void set_read_resume();
  inline void set_destroyed();

  void IncrementStats(size_t datalen);

  BaseObjectWeakPtr<QuicSession> session_;
  int64_t stream_id_;
  uint64_t max_offset_ = 0;
  uint64_t max_offset_ack_ = 0;
  uint32_t flags_ = QUICSTREAM_FLAG_INITIAL;

  QuicBuffer streambuf_;
  size_t available_outbound_length_ = 0;
  size_t inbound_consumed_data_while_paused_ = 0;

  std::vector<std::unique_ptr<QuicHeader>> headers_;
  QuicStreamHeadersKind headers_kind_;
  size_t current_headers_length_ = 0;

  struct stream_stats {
    // The timestamp at which the stream was created
    uint64_t created_at;
    // The timestamp at which the stream most recently sent data
    uint64_t stream_sent_at;
    // The timestamp at which the stream most recently received data
    uint64_t stream_received_at;
    // The timestamp at which the stream most recently received an
    // acknowledgement for data
    uint64_t stream_acked_at;
    // The timestamp at which a graceful close started
    uint64_t closing_at;
    // The total number of bytes received
    uint64_t bytes_received;
    // The total number of bytes sent
    uint64_t bytes_sent;
    // The maximum extended stream offset
    uint64_t max_offset;
  };
  stream_stats stream_stats_{};

  // data_rx_rate_ measures the elapsed time between data packets
  // for this stream. When used in combination with the data_rx_size,
  // this can be used to track the overall data throughput over time
  // for the stream. Specifically, this can be used to detect
  // potentially bad acting peers that are sending many small chunks
  // of data too slowly in an attempt to DOS the peer.
  BaseObjectPtr<HistogramBase> data_rx_rate_;

  // data_rx_size_ measures the size of data packets for this stream
  // over time. When used in combination with the data_rx_rate_,
  // this can be used to track the overall data throughout over time
  // for the stream. Specifically, this can be used to detect
  // potentially bad acting peers that are sending many small chunks
  // of data too slowly in an attempt to DOS the peer.
  BaseObjectPtr<HistogramBase> data_rx_size_;

  // data_rx_ack_ measures the elapsed time between data acks
  // for this stream. This data can be used to detect peers that are
  // generally taking too long to acknowledge sent stream data.
  BaseObjectPtr<HistogramBase> data_rx_ack_;

  AliasedBigUint64Array stats_buffer_;

  ListNode<QuicStream> stream_queue_;

 public:
  // Linked List of QuicStream objects
  using Queue = ListHead<QuicStream, &QuicStream::stream_queue_>;
  inline void Schedule(Queue* queue);
  inline void Unschedule();
};

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS

#endif  // SRC_QUIC_NODE_QUIC_STREAM_H_
