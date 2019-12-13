#ifndef SRC_NODE_QUIC_STREAM_H_
#define SRC_NODE_QUIC_STREAM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "memory_tracker-inl.h"
#include "async_wrap.h"
#include "env.h"
#include "histogram-inl.h"
#include "node_quic_util.h"
#include "stream_base-inl.h"
#include "v8.h"

#include <string>
#include <vector>

namespace node {
namespace quic {

class QuicSession;

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
class QuicHeader {
 public:
  QuicHeader() {}

  virtual ~QuicHeader() {}
  virtual v8::MaybeLocal<v8::String> GetName(QuicApplication* app) const = 0;
  virtual v8::MaybeLocal<v8::String> GetValue(QuicApplication* app) const = 0;
  virtual std::string GetName() const = 0;
  virtual std::string GetValue() const = 0;
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


  static void Initialize(
      Environment* env,
      v8::Local<v8::Object> target,
      v8::Local<v8::Context> context);

  static BaseObjectPtr<QuicStream> New(
      QuicSession* session, int64_t stream_id);

  std::string diagnostic_name() const override;

  inline QuicStreamDirection GetDirection() const {
    return stream_id_ & 0b10 ?
        QUIC_STREAM_UNIDIRECTIONAL :
        QUIC_STREAM_BIRECTIONAL;
  }

  inline QuicStreamOrigin GetOrigin() const {
    return stream_id_ & 0b01 ?
        QUIC_STREAM_SERVER :
        QUIC_STREAM_CLIENT;
  }

  int64_t GetID() const { return stream_id_; }

  inline bool IsDestroyed() const {
    return flags_ & QUICSTREAM_FLAG_DESTROYED;
  }

  // The QUICSTREAM_FLAG_FIN flag will be set only when a final stream
  // frame has been received from the peer.
  inline bool HasReceivedFin() const {
    return flags_ & QUICSTREAM_FLAG_FIN;
  }

  // The QUICSTREAM_FLAG_FIN_SENT flag will be set only when a final
  // stream frame has been transmitted to the peer. Once sent, no
  // additional data may be transmitted to the peer. If HasSentFin
  // is set, IsWritable() can be assumed to be false.
  inline bool HasSentFin() const {
    return flags_ & QUICSTREAM_FLAG_FIN_SENT;
  }

  // WasEverWritable returns true if it is a bidirectional stream,
  // or a Unidirectional stream originating from the local peer.
  // If WasEverWritable() is false, then no stream frames should
  // ever be sent from the local peer, including final stream frames.
  inline bool WasEverWritable() const {
    if (GetDirection() == QUIC_STREAM_UNIDIRECTIONAL) {
      return session_->IsServer() ?
          GetOrigin() == QUIC_STREAM_SERVER :
          GetOrigin() == QUIC_STREAM_CLIENT;
    }
    return true;
  }

  // A QuicStream will not be writable if:
  //  - The QUICSTREAM_FLAG_WRITE_CLOSED flag is set or
  //  - It is a Unidirectional stream originating from the peer
  inline bool IsWritable() const {
    if (flags_ & QUICSTREAM_FLAG_WRITE_CLOSED)
      return false;

    return WasEverWritable();
  }

  // WasEverReadable returns true if it is a bidirectional stream,
  // or a Unidirectional stream originating from the remote
  // peer.
  inline bool WasEverReadable() const {
    if (GetDirection() == QUIC_STREAM_UNIDIRECTIONAL) {
      return session_->IsServer() ?
          GetOrigin() == QUIC_STREAM_CLIENT :
          GetOrigin() == QUIC_STREAM_SERVER;
    }

    return true;
  }

  // A QuicStream will not be readable if:
  //  - The QUICSTREAM_FLAG_READ_CLOSED flag is set or
  //  - It is a Unidirectional stream originating from the local peer.
  inline bool IsReadable() const {
    if (flags_ & QUICSTREAM_FLAG_READ_CLOSED)
      return false;

    return WasEverReadable();
  }

  inline bool IsReadStarted() const {
    return flags_ & QUICSTREAM_FLAG_READ_STARTED;
  }

  inline bool IsReadPaused() const {
    return flags_ & QUICSTREAM_FLAG_READ_PAUSED;
  }

  bool IsAlive() override {
    return !IsDestroyed() && !IsClosing();
  }

  bool IsClosing() override {
    return !IsWritable() && !IsReadable();
  }

  // Records the fact that a final stream frame has been
  // serialized and sent to the peer. There still may be
  // unacknowledged data in the outbound queue, but no
  // additional frames may be sent for the stream other
  // than reset stream.
  inline void SetFinSent() {
    CHECK(!IsWritable());
    flags_ |= QUICSTREAM_FLAG_FIN_SENT;
  }

  // IsWriteFinished will return true if a final stream frame
  // has been sent and all data has been acknowledged (the
  // send buffer is empty).
  inline bool IsWriteFinished() {
    return HasSentFin() && streambuf_.Length() == 0;
  }

  QuicSession* Session() const { return session_.get(); }

  virtual void AckedDataOffset(uint64_t offset, size_t datalen);

  virtual void Destroy();

  int DoWrite(
      WriteWrap* req_wrap,
      uv_buf_t* bufs,
      size_t nbufs,
      uv_stream_t* send_handle) override;

  inline void IncrementAvailableOutboundLength(size_t amount);
  inline void DecrementAvailableOutboundLength(size_t amount);

  virtual void ReceiveData(
      int fin,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset);

  bool SubmitInformation(v8::Local<v8::Array> headers);
  bool SubmitHeaders(v8::Local<v8::Array> headers, uint32_t flags);
  bool SubmitTrailers(v8::Local<v8::Array> headers);

  // Required for StreamBase
  int ReadStart() override;

  // Required for StreamBase
  int ReadStop() override;

  // Required for StreamBase
  int DoShutdown(ShutdownWrap* req_wrap) override;

  void ResetStream(uint64_t app_error_code);

  void Commit(ssize_t amount);

  template <typename T>
  inline size_t DrainInto(
      std::vector<T>* vec,
      size_t max_count = MAX_VECTOR_COUNT) {
    CHECK(!IsDestroyed());
    size_t length = 0;
    streambuf_.DrainInto(vec, &length, max_count);
    return length;
  }

  template <typename T>
  inline size_t DrainInto(
      T* vec,
      size_t* count,
      size_t max_count = MAX_VECTOR_COUNT) {
    CHECK(!IsDestroyed());
    size_t length = 0;
    streambuf_.DrainInto(vec, count, &length, max_count);
    return length;
  }

  AsyncWrap* GetAsyncWrap() override { return this; }

  // Some QUIC applications support headers, others do not.
  // The following methods allow consistent handling of
  // headers at the QuicStream level regardless of the
  // protocol. For applications that do not support headers,
  // these are simply not used.
  void BeginHeaders(QuicStreamHeadersKind kind = QUICSTREAM_HEADERS_KIND_NONE);

  // Returns false if the header cannot be added. This will
  // typically only happen if a maximimum number of headers
  // has been reached.
  bool AddHeader(std::unique_ptr<QuicHeader> header);
  void EndHeaders();

  // Sets the kind of headers currently being processed.
  void SetHeadersKind(QuicStreamHeadersKind kind);

  void MemoryInfo(MemoryTracker* tracker) const override;

  SET_MEMORY_INFO_NAME(QuicStream)
  SET_SELF_SIZE(QuicStream)

  QuicStream(
      QuicSession* session,
      v8::Local<v8::Object> target,
      int64_t stream_id);

 private:
  // Called only when a final stream frame has been received from
  // the peer. This has the side effect of marking the readable
  // side of the stream closed. No additional data will be received
  // on this QuicStream.
  inline void SetFinReceived() {
    flags_ |= QUICSTREAM_FLAG_FIN;
    SetReadClose();
  }

  // SetWriteClose is called either when the QuicStream is created
  // and is unidirectional from the peer, or when DoShutdown is called.
  // This will indicate that the writable side of the QuicStream is
  // closed and that no data will be pushed to the outbound queue.
  inline void SetWriteClose() {
    flags_ |= QUICSTREAM_FLAG_WRITE_CLOSED;
  }

  // Called when no additional data can be received on the QuicStream.
  inline void SetReadClose() {
    flags_ |= QUICSTREAM_FLAG_READ_CLOSED;
  }

  inline void SetReadStart() {
    flags_ |= QUICSTREAM_FLAG_READ_STARTED;
  }

  inline void SetReadPause() {
    flags_ |= QUICSTREAM_FLAG_READ_PAUSED;
  }

  inline void SetReadResume() {
    flags_ &= QUICSTREAM_FLAG_READ_PAUSED;
  }

  inline void SetDestroyed() {
    flags_ |= QUICSTREAM_FLAG_DESTROYED;
  }

  inline void IncrementStats(size_t datalen);

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
};

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS

#endif  // SRC_NODE_QUIC_STREAM_H_
