#pragma once

#include "aliased_struct.h"
#include "memory_tracker.h"
#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <node_bob.h>
#include <node_external_reference.h>
#include <node_http_common.h>
#include <stream_base.h>
#include "quic.h"
#include "session.h"

namespace node {
namespace quic {

#define V(_, name, __) uint64_t name;
struct StreamStats final {
  STREAM_STATS(V)
};
#undef V

struct StreamStatsTraits final {
  using Stats = StreamStats;
  using Base = StatsBase<StreamStatsTraits>;

  template <typename Fn>
  static void ToString(const Stats& stats, Fn&& add_field) {
#define V(_, id, desc) add_field(desc, stats.id);
    STREAM_STATS(V)
#undef V
  }
};

using StreamStatsBase = StatsBase<StreamStatsTraits>;
using Ngtcp2Source = bob::SourceImpl<ngtcp2_vec>;

class ArrayBufferViewSource;
class BlobSource;
class StreamSource;
class StreamBaseSource;

// =============================================================================
// ngtcp2 is responsible for determining when and how to encode stream data into
// a packet.
//
// An individual QUIC packet may contain multiple QUIC frames. Whenever we
// create a QUIC packet, we really have no idea what frames are going to be
// encoded or how much buffered handshake or stream data is going to be included
// within that Packet (if any). If there is buffered data available for a
// stream, we provide an array of pointers to that data and an indication about
// how much data is available, then we leave it entirely up to ngtcp2 and
// nghttp3 to determine how much of the data to encode into the QUIC packet (if
// any). It is only *after* the QUIC packet is encoded that we can know how much
// was actually written.
//
// Once stream data is written to a Packet, we have to keep the data in memory
// until an acknowledgement is received. In QUIC, acknowledgements are received
// per range of packets, but (fortunately) ngtcp2 gives us that information as
// byte offsets instead.
//
// Buffer is complicated because it needs to be able to accomplish three things:
// (a) buffering v8::BackingStore instances passed down from JavaScript without
// memcpy, (b) tracking what data has already been encoded in a QUIC packet and
// what data is remaining
//     to be read, and
// (c) tracking which data has been acknowledged and which hasn't.
//
// Buffer contains a deque of Buffer::Chunk instances. A single Buffer::Chunk
// wraps a v8::BackingStore with length and offset. When the Buffer::Chunk is
// created, we capture the total length of the buffer (minus the offset) and the
// total number of bytes remaining to be sent. Initially, these numbers are
// identical.
//
// When data is encoded into a Packet, we advance the Buffer::Chunk's
// remaining-to-be-read by the number of bytes actually encoded. If there are no
// more bytes remaining to be encoded, we move to the next chunk in the deque
// (but we do not yet pop it off the deque).
//
// When an acknowledgement is received, we decrement the Buffer::Chunk's length
// by the number of acknowledged bytes. Once the unacknowledged length reaches 0
// we pop the chunk off the deque.
class Buffer : public Ngtcp2Source, public MemoryRetainer {
 public:
  // Stores chunks of both inbound and outbound data. Each chunk stores a shared
  // pointer to a v8::BackingStore with appropriate length and offset details.
  // Each Buffer::Chunk is stored in a deque in Buffer which manages the
  // aggregate collection of all chunks.
  class Chunk final : public MemoryRetainer {
   public:
    // Copies len bytes from data into a new Chunk.
    static Chunk Create(Environment* env, const uint8_t* data, size_t len);

    // Stores the given BackingStore directly without copying. One important
    // thing here is the fact the data is not immutable. If user code passes a
    // TypedArray or ArrayBuffer in, the user code can continue to modify it
    // after. For now that's an acceptable risk as it is definitely an edge
    // case. Later, we might want to consider allowing for a copy with the
    // understanding that doing so will introduce a small performance hit.
    static Chunk Create(Store&& store);

    // Identifies an amount of stored data to be acknowledged. Once the amount
    // of acknowledged data equals length_, the chunk can be freed from memory.
    // Returns the actual amount of data acknowledged.
    size_t Acknowledge(size_t amount);

    // Releases the chunk to a v8 Uint8Array. data_ is reset and offset_,
    // length_, and consumed_ are all set to 0 and the strong_ptr_, if any, is
    // reset. This is used only for inbound data and only when queued data is
    // being flushed out to the JavaScript side.
    v8::MaybeLocal<v8::Value> Release(Environment* env);

    // Increments consumed_ by amount bytes. If amount is greater than
    // remaining(), remaining() bytes are advanced. Returns the actual number of
    // bytes advanced.
    size_t Seek(size_t amount);

    // Returns a pointer to the remaining data. This is used only for outbound
    // data.
    const uint8_t* data() const;

    // Returns the total remaining number of unacknowledged bytes.
    inline size_t length() const { return unacknowledged_; }

    // Returns the total remaining number of non-transmitted bytes.
    inline size_t remaining() const {
      if (!data_) return 0;
      return data_.length() - read_;
    }

    operator ngtcp2_vec() const;
    operator uv_buf_t() const;

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(Buffer::Chunk)
    SET_SELF_SIZE(Chunk)

    using Queue = std::deque<Buffer::Chunk>;

   private:
    Chunk(Store&& store);

    Store data_;
    size_t read_ = 0;
    size_t unacknowledged_ = 0;
  };

  // Provides outbound data for a stream
  struct Source : public Ngtcp2Source, public MemoryRetainer {
    Stream* stream = nullptr;

    void Attach(Stream* stream);
    void Resume();

    virtual BaseObjectPtr<BaseObject> GetStrongPtr() = 0;
    virtual size_t Acknowledge(uint64_t offset, size_t amount) = 0;
    virtual size_t Seek(size_t amount) = 0;
    virtual bool is_finished() const = 0;
    virtual bool is_closed() const = 0;
    virtual void set_finished() = 0;
    virtual void set_closed() = 0;
  };

  class SourceObject : public Source {
   public:
    explicit SourceObject(Environment* env);

    bool is_finished() const override;
    bool is_closed() const override;
    void set_finished() override;
    void set_closed() override;

   private:
    struct State final {
#define V(_, name, type) type name;
      SOURCE_STATE(V)
#undef V
    };

    AliasedStruct<State> state_;

    friend class ArrayBufferViewSource;
    friend class BlobSource;
    friend class StreamSource;
    friend class StreamBaseSource;
  };

  Buffer() = default;
  explicit Buffer(const Blob& blob);
  explicit Buffer(Store&& store);
  QUIC_NO_COPY_OR_MOVE(Buffer)

  // Marks the Buffer as having ended, preventing new Buffer::Chunk instances
  // from being added and allowing the Pull operation to know when to signal
  // that the flow of data is completed.
  inline void End() {
    ended_ = true;
  }
  inline bool is_ended() const {
    return ended_;
  }
  inline bool is_finished() const {
    return ended_ && remaining_ == 0 && finished_;
  }

  // Push inbound data onto the buffer.
  void Push(Environment* env, const uint8_t* data, size_t len);

  // Push outbound data onto the buffer.
  void Push(Store&& store);

  // Increment the given number of bytes within the buffer. If amount is greater
  // than length(), length() bytes are advanced. Returns the actual number of
  // bytes advanced. Will not cause bytes to be freed.
  size_t Seek(size_t amount);

  // Acknowledge the given number of bytes in the buffer. May cause bytes to be
  // freed.
  size_t Acknowledge(size_t amount);

  // Clears any bytes remaining in the buffer.
  inline void Clear() {
    queue_.clear();
    head_ = 0;
    length_ = 0;
    remaining_ = 0;
  }

  // The total number of unacknowledged bytes remaining. The length is
  // incremented by Push and decremented by Acknowledge.
  inline size_t length() const {
    return length_;
  }

  // The total number of unread bytes remaining. The remaining length is
  // incremental by Push and decremented by Seek.
  inline size_t remaining() const {
    return remaining_;
  }

  // Flushes the entire inbound queue into a v8::Local<v8::Array> of Uint8Array
  // instances, returning the total number of bytes released to the consumer.
  v8::Maybe<size_t> Release(Stream* consumer);

  int DoPull(bob::Next<ngtcp2_vec> next,
             int options,
             ngtcp2_vec* data,
             size_t count,
             size_t max_count_hint) override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Buffer);
  SET_SELF_SIZE(Buffer);

 private:
  Chunk::Queue queue_;
  bool ended_ = false;
  bool finished_ = false;

  // The queue_ index of the current read head. This is incremented by Seek() as
  // necessary and decremented by Acknowledge() as data is consumed.
  size_t head_ = 0;
  size_t length_ = 0;
  size_t remaining_ = 0;
};

// QUIC Stream's are simple data flows that may be:
//
// * Bidirectional or Unidirectional
// * Server or Client Initiated
//
// The flow direction and origin of the stream are important in determining the
// write and read state (Open or Closed). Specifically:
//
// A Unidirectional stream originating with the Server is:
//
// * Server Writable (Open) but not Client Writable (Closed)
// * Client Readable (Open) but not Server Readable (Closed)
//
// Likewise, a Unidirectional stream originating with the Client is:
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
// All data sent via the Stream is buffered internally until either receipt is
// acknowledged from the peer or attempts to send are abandoned. The fact that
// data is buffered in memory makes it essential that the flow control for the
// session and the stream are properly handled. For now, we are largely relying
// on ngtcp2's default flow control mechanisms which generally should be doing
// the right thing but we may need to switch to a more manual management process
// if too much data ends up being buffered for too long.
//
// A Stream may be in a fully closed state (No longer readable nor writable)
// state but still have unacknowledged data in it's outbound queue.
//
// A Stream is gracefully closed when (a) both Read and Write states are Closed,
// (b) all queued data has been acknowledged.
//
// The Stream may be forcefully closed immediately using destroy(err). This
// causes all queued data and pending JavaScript writes to be abandoned, and
// causes the Stream to be immediately closed at the ngtcp2 level without
// waiting for any outstanding acknowledgements. Keep in mind, however, that the
// peer is not notified that the stream is destroyed and may attempt to continue
// sending data and acknowledgements.
class Stream : public AsyncWrap, public Ngtcp2Source, public StreamStatsBase {
 public:
  using Header = NgHeaderBase<BindingState>;

  enum class Origin {
    SERVER,
    CLIENT,
  };

  static Stream* From(ngtcp2_conn*, void* stream_user_data);

  HAS_INSTANCE()
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> object);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static BaseObjectPtr<Stream> Create(Environment* env,
                                      Session* session,
                                      stream_id id);

  Stream(BaseObjectPtr<Session> session,
         v8::Local<v8::Object> object,
         stream_id id,
         Buffer::Source* source = nullptr);

  inline bool is_destroyed() const { return state_->destroyed; }
  inline bool might_send_trailers() const { return state_->trailers; }

  inline stream_id id() const { return state_->id; }

  inline Direction direction() const {
    return id() & 0b10 ? Direction::UNIDIRECTIONAL : Direction::BIDIRECTIONAL;
  }

  inline Origin origin() const {
    return id() & 0b01 ? Origin::SERVER : Origin::CLIENT;
  }

  inline Session* session() const { return session_.get(); }

  void SetPriority(Session::Application::StreamPriority priority,
                   Session::Application::StreamPriorityFlags flags);
  Session::Application::StreamPriority GetPriority();

  // The final size is the maximum amount of data that has been acknowleged to
  // have been received for a Stream.
  inline uint64_t final_size() const {
    return GetStat(&StreamStats::final_size);
  }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Stream)
  SET_SELF_SIZE(Stream)

 private:
  struct State final {
#define V(_, name, type) type name;
    STREAM_STATE(V)
#undef V
  };

  // JavaScript API
  static void AttachSource(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoDestroy(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoSendHeaders(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoStopSending(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoResetStream(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoSetPriority(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoGetPriority(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FlushInbound(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Internal API

  void UpdateStats(size_t datalen);

  void ProcessInbound();
  void ReadyForTrailers();

  // Signals the beginning of a new block of headers.
  void BeginHeaders(Session::Application::HeadersKind kind);
  // Returns false if the header cannot be added. This will typically only
  // happen if a maximimum number of headers, or the maximum total header length
  // is received.
  bool AddHeader(const Header& header);
  // Signals the ending of the current block of headers.
  void EndHeaders();

  void Commit(size_t amount);
  void Acknowledge(uint64_t offset, size_t datalen);
  void AttachOutboundSource(Buffer::Source* source);

  // In the typical case there is no reason to explicitly close a Stream as it
  // will be closed automatically when both the readable and writable sides are
  // both closed. However, in some cases it is necessary to close the Stream
  // immediately, such as when the owning Session is being closed immediately.
  // Once a stream is destroyed, there is nothing else it can do and the stream
  // should not be used for anything else. The only state remaining will be the
  // collected statistics.
  void Destroy(v8::Maybe<QuicError> error = v8::Nothing<QuicError>());

  void ReceiveData(Session::Application::ReceiveStreamDataFlags flags,
                   const uint8_t* data,
                   size_t datalen,
                   uint64_t offset);

  // When we have received a RESET_STREAM frame from the peer, it is an
  // indication that they have abruptly terminated their side and will not be
  // sending any more data. The final size is an indicator of the amount of data
  // *they* recognize as having been sent to us. The QUIC spec leaves the
  // specific handling of this frame up to the application. We can choose to
  // drop buffered inbound data on the floor or to deliver it to the
  // application. We choose to deliver it then end the readable side of the
  // stream. Importantly, receiving a RESET_STREAM does *not* destroy the
  // stream. It only ends the readable side. If there is a reset-stream event
  // registered on the JavaScript wrapper, we will emit the event.
  void ReceiveResetStream(size_t final_size, QuicError error);

  void ReceiveStopSending(QuicError error);

  // ResetStream will cause ngtcp2 to queue a RESET_STREAM for this stream,
  // signaling abrupt termination of the outbound flow of data.
  void ResetStream(QuicError error = QuicError());

  // StopSending will cause ngtcp2 to queue a STOP_SENDING frame for this stream
  // if appropriate. For unidirectional streams for which we are the origin,
  // this ends up being a non-op. For Bidirectional streams, a STOP_SENDING
  // frame is essentially a polite request to the other side to stop sending
  // data on this stream. The other stream is expected to respond with a
  // RESET_STREAM frame that indicates abrupt termination of the inbound flow of
  // data into this stream.
  //
  // Calling this will have the effect of shutting down the readable side of
  // this stream. Any data currently in the buffer can still be read but no new
  // data will be accepted and ngtcp2 should not attempt to push any more in.
  void StopSending(QuicError error = QuicError());

  // Notifies that the stream writable side has been closed.
  void EndWritable();

  void Resume();
  void Blocked();

  // Sends headers to the QUIC Application. If headers are not supported, false
  // will be returned. Otherwise, returns true
  bool SendHeaders(Session::Application::HeadersKind kind,
                   const v8::Local<v8::Array>& headers,
                   Session::Application::HeadersFlags flags =
                       Session::Application::HeadersFlags::NONE);

  int DoPull(bob::Next<ngtcp2_vec> next,
             int options,
             ngtcp2_vec* data,
             size_t count,
             size_t max_count_hint) override;

  // Set the final size for the Stream. This only works the first time it is
  // called. Subsequent calls will be ignored unless the subsequent size is
  // greater than the prior set size, in which case we have a bug and we'll
  // assert.
  void set_final_size(uint64_t final_size);
  inline void set_headers_kind(Session::Application::HeadersKind headers_kind) {
    headers_kind_ = headers_kind;
  }

  // ============================================================================================
  // JavaScript Outcalls
  using CallbackScope = CallbackScopeBase<Stream>;

  void EmitBlocked();
  void EmitClose();
  void EmitError(QuicError error);
  void EmitHeaders();
  void EmitReset(QuicError error);
  void EmitTrailers();
  v8::Maybe<size_t> EmitData(Buffer::Chunk::Queue queue, bool ended);

  // ============================================================================================
  // Internal fields
  BaseObjectPtr<Session> session_;
  AliasedStruct<State> state_;

  // The outbound_source_ provides the data that is to be sent by this Stream.
  // After the source is read the writable side of the Stream will be closed by
  // sending a fin data frame.
  Buffer::Source* outbound_source_ = nullptr;
  BaseObjectPtr<BaseObject> outbound_source_strong_ptr_;

  // The inbound_ buffer contains the data that has been received by this Stream
  // but not yet delivered to the JavaScript wrapper.
  Buffer inbound_;

  std::vector<v8::Local<v8::Value>> headers_;
  Session::Application::HeadersKind headers_kind_ =
      Session::Application::HeadersKind::INITIAL;

  // The current total byte length of the headers
  size_t current_headers_length_ = 0;

  ListNode<Stream> stream_queue_;

  friend class Session::Application;
  friend class Http3Application;
  friend class DefaultApplication;
  friend class Session;
  friend class Buffer;
  friend class ArrayBufferViewSource;
  friend class BlobSource;
  friend class StreamSource;
  friend class StreamBaseSource;

 public:
  // The Queue/Schedule/Unschedule here are part of the mechanism used to
  // determine which streams have data to send on the session. When a stream
  // potentially has data available, it will be scheduled in the Queue. Then,
  // when the Session::Application starts sending pending data, it will check
  // the queue to see if there are streams waiting. If there are, it will grab
  // one and check to see if there is data to send. When a stream does not have
  // data to send (such as when it is initially created or is using an async
  // source that is still waiting for data to be pushed) it will not appear in
  // the queue.
  using Queue = ListHead<Stream, &Stream::stream_queue_>;

  void Schedule(Queue* queue);

  inline void Unschedule() {
    stream_queue_.Remove();
  }
};

// Receives a single ArrayBufferView and uses it's contents as the complete
// source of outbound data for the Stream.
class ArrayBufferViewSource final : public Buffer::SourceObject,
                                    public BaseObject {
 public:
  HAS_INSTANCE()
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  int DoPull(bob::Next<ngtcp2_vec> next,
             int options,
             ngtcp2_vec* data,
             size_t count,
             size_t max_count_hint) override;

  BaseObjectPtr<BaseObject> GetStrongPtr() override;
  size_t Acknowledge(uint64_t offset, size_t datalen) override;
  size_t Seek(size_t amount) override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(ArrayBufferViewSource);
  SET_SELF_SIZE(ArrayBufferViewSource);

 private:
  ArrayBufferViewSource(Environment* env,
                        v8::Local<v8::Object> wrap,
                        Store&& store);

  Buffer buffer_;
};

// Wraps a Blob instance that provides the outbound data.
class BlobSource final : public AsyncWrap, public Buffer::SourceObject {
 public:
  HAS_INSTANCE()
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  int DoPull(bob::Next<ngtcp2_vec> next,
             int options,
             ngtcp2_vec* data,
             size_t count,
             size_t max_count_hint) override;

  BaseObjectPtr<BaseObject> GetStrongPtr() override;
  size_t Acknowledge(uint64_t offset, size_t datalen) override;
  size_t Seek(size_t amount) override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(BlobSource);
  SET_SELF_SIZE(BlobSource);

 private:
  BlobSource(Environment* env,
             v8::Local<v8::Object> wrap,
             BaseObjectPtr<Blob> blob);

  Buffer buffer_;
};

// Implements StreamBase to asynchronously accept outbound data from the
// JavaScript side.
class StreamSource final : public AsyncWrap, public Buffer::SourceObject {
 public:
  HAS_INSTANCE()
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  int DoPull(bob::Next<ngtcp2_vec> next,
             int options,
             ngtcp2_vec* data,
             size_t count,
             size_t max_count_hint) override;

  size_t Acknowledge(uint64_t offset, size_t datalen) override;
  size_t Seek(size_t amount) override;
  BaseObjectPtr<BaseObject> GetStrongPtr() override;
  void set_closed() override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(StreamSource);
  SET_SELF_SIZE(StreamSource);

 private:
  static void End(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Write(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WriteV(const v8::FunctionCallbackInfo<v8::Value>& args);

  StreamSource(Environment* env, v8::Local<v8::Object> wrap);

  Buffer queue_;
};

// Implements StreamListener to receive data from any native level StreamBase
// implementation.
class StreamBaseSource final : public AsyncWrap,
                               public Buffer::SourceObject,
                               public StreamListener {
 public:
  HAS_INSTANCE()
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  int DoPull(bob::Next<ngtcp2_vec> next,
             int options,
             ngtcp2_vec* data,
             size_t count,
             size_t max_count_hint) override;

  size_t Acknowledge(uint64_t offset, size_t datalen) override;
  size_t Seek(size_t amount) override;
  uv_buf_t OnStreamAlloc(size_t suggested_size) override;
  void OnStreamRead(ssize_t nread, const uv_buf_t& buf) override;
  BaseObjectPtr<BaseObject> GetStrongPtr() override;
  void set_closed() override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(StreamBaseSource)
  SET_SELF_SIZE(StreamBaseSource)

 private:
  StreamBaseSource(
      Environment* env,
      v8::Local<v8::Object> wrap,
      StreamBase* resource,
      BaseObjectPtr<AsyncWrap> strong_ptr = BaseObjectPtr<AsyncWrap>());

  StreamBase* resource_;
  BaseObjectPtr<AsyncWrap> strong_ptr_;
  Buffer buffer_;
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
