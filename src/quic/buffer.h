#ifndef SRC_QUIC_BUFFER_H_
#define SRC_QUIC_BUFFER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async_wrap.h"
#include "base_object.h"
#include "memory_tracker.h"
#include "node.h"
#include "node_bob.h"
#include "node_blob.h"
#include "node_file.h"
#include "node_internals.h"
#include "stream_base.h"
#include "util-inl.h"
#include "uv.h"

#include "ngtcp2/ngtcp2.h"

#include <deque>

namespace node {
namespace quic {

class Buffer;
class Stream;

constexpr size_t kMaxVectorCount = 16;

// When data is sent over QUIC, we are required to retain it in memory
// until we receive an acknowledgement that it has been successfully
// received by the peer. The Buffer object is what we use to handle
// that and track until it is acknowledged. To understand the Buffer
// object itself, it is important to understand how ngtcp2 and nghttp3
// handle data that is given to it to serialize into QUIC packets.
//
// An individual QUIC packet may contain multiple QUIC frames. Whenever
// we create a QUIC packet, we really have no idea what frames are going
// to be encoded or how much buffered handshake or stream data is going
// to be included within that Packet (if any). If there is buffered data
// available for a stream, we provide an array of pointers to that data
// and an indication about how much data is available, then we leave it
// entirely up to ngtcp2 and nghttp3 to determine how much of the data
// to encode into the QUIC packet. It is only *after* the QUIC packet
// is encoded that we can know how much was actually written.
//
// Once written to a QUIC Packet, we have to keep the data in memory
// until an acknowledgement is received. In QUIC, acknowledgements are
// received per range of packets, but (fortunately) ngtcp2 gives us that
// information as byte offsets instead.
//
// Buffer is complicated because it needs to be able to accomplish
// three things: (a) buffering v8::BackingStore instances passed down
// from JavaScript without memcpy, (b) tracking what data has already been
// encoded in a QUIC packet and what data is remaining to be read, and
// (c) tracking which data has been acknowledged and which hasn't.
//
// Buffer contains a deque of Buffer::Chunk instances.
// A single Buffer::Chunk wraps a v8::BackingStore with length and
// offset. When the Buffer::Chunk is created, we capture the total
// length of the buffer and the total number of bytes remaining to be sent.
// Initially, these numbers are identical.
//
// When data is encoded into a Packet, we advance the Buffer::Chunk's
// remaining-to-be-read by the number of bytes actually encoded. If there
// are no more bytes remaining to be encoded, we move to the next chunk
// in the deque (but we do not yet pop it off the deque).
//
// When an acknowledgement is received, we decrement the Buffer::Chunk's
// length by the number of acknowledged bytes. Once the unacknowledged
// length reaches 0 we pop the chunk off the deque.

class Buffer final : public bob::SourceImpl<ngtcp2_vec>,
                     public MemoryRetainer {
 public:
  // Stores chunks of both inbound and outbound data. Each chunk
  // stores a shared pointer to a v8::BackingStore with appropriate
  // length and offset details. Each Buffer::Chunk is stored in a
  // deque in Buffer which manages the aggregate collection of all chunks.
  class Chunk final : public MemoryRetainer {
   public:
    // Copies len bytes from data into a new Chunk.
    static std::unique_ptr<Chunk> Create(
        Environment* env,
        const uint8_t* data,
        size_t len);

    // Stores the given BackingStore directly without copying.
    // One important thing here is the fact the data is not
    // immutable. If user code passes a TypedArray or ArrayBuffer
    // in, the user code can continue to modify it after. For now
    // that's an acceptable risk as it is definitely an edge case.
    // Later, we might want to consider allowing for a copy with
    // the understanding that doing so will introduce a small
    // performance hit.
    static std::unique_ptr<Chunk> Create(
        const std::shared_ptr<v8::BackingStore>& data,
        size_t offset,
        size_t length);

    // Identifies an amount of stored data to be acknowledged.
    // Once the amount of acknowledged data equals length_, the
    // chunk can be freed from memory. Returns the actual amount
    // of data acknowledged.
    size_t Acknowledge(size_t amount);

    // Releases the chunk to a v8 Uint8Array. data_ is reset
    // and offset_, length_, and consumed_ are all set to 0
    // and the strong_ptr_, if any, is reset. This is used
    // only for inbound data and only when queued data is
    // being flushed out to the JavaScript side.
    v8::MaybeLocal<v8::Value> Release(Environment* env);

    // Increments consumed_ by amount bytes. If amount is greater
    // than remaining(), remaining() bytes are advanced. Returns
    // the actual number of bytes advanced.
    size_t Seek(size_t amount);

    // Returns a pointer to the remaining data. This is used only
    // for outbound data.
    const uint8_t* data() const;

    // Returns the total remaining number of unacknowledged bytes.
    inline size_t length() const { return unacknowledged_; }

    // Returns the total remaining number of non-transmitted bytes.
    inline size_t remaining() const { return length_ - read_; }

    // Returns a pointer to the remaining data in an ngtcp2_vec struct.
    ngtcp2_vec vec() const;

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(Buffer::Chunk)
    SET_SELF_SIZE(Chunk)

    using Queue = std::deque<std::unique_ptr<Buffer::Chunk>>;

   private:
    Chunk(
        const std::shared_ptr<v8::BackingStore>& data,
        size_t length,
        size_t offset = 0);

    std::shared_ptr<v8::BackingStore> data_;
    size_t offset_ = 0;
    size_t length_ = 0;
    size_t read_ = 0;
    size_t unacknowledged_ = 0;
  };

  // Receives the inbound data for a Stream
  struct Consumer {
    virtual v8::Maybe<size_t> Process(
        Chunk::Queue queue,
        bool ended = false) = 0;
  };

  // Provides outbound data for a stream
  class Source : public bob::SourceImpl<ngtcp2_vec>,
                 public MemoryRetainer {
   public:
    enum InternalFields {
      kSlot = BaseObject::kSlot,
      kDonePromise = BaseObject::kInternalFieldCount,
      kInternalFieldCount
    };

    inline explicit Source(Environment* env) : env_(env) {}

    // If the Source is a BaseObject, then GetStrongPtr will return
    // a BaseObjectPtr that can be used to maintain a strong pointer.
    virtual BaseObjectPtr<BaseObject> GetStrongPtr() {
      return BaseObjectPtr<BaseObject>();
    }

    virtual size_t Acknowledge(uint64_t offset, size_t amount) = 0;
    virtual size_t Seek(size_t amount) = 0;
    virtual bool is_finished() const = 0;
    void set_owner(Stream* owner);

    // If the BufferSource is explicitly marked closed, then it
    // should not accept any more pending data than what's already
    // in it's queue, if any, and it should send EOS as soon as possible.
    // The set_closed state will not be relevant to all sources
    // (e.g. ArrayBufferViewSource and NullSource) so the default
    // implementation is to do nothing.
    virtual void set_closed() { }

    void ResolveDone();
    void RejectDone(v8::Local<v8::Value> reason);

   protected:
    void SetDonePromise();

    inline Stream* owner() { return owner_; }

   private:
    Environment* env_;
    Stream* owner_ = nullptr;
  };

  Buffer() = default;
  Buffer(const Buffer& other) = delete;
  Buffer(const Buffer&& src) = delete;
  Buffer& operator=(const Buffer& other) = delete;
  Buffer& operator=(const Buffer&& src) = delete;

  Buffer(const BaseObjectPtr<Blob>& blob);
  Buffer(const std::shared_ptr<v8::BackingStore>& store,
         size_t length,
         size_t offset);

  // Marks the Buffer as having ended, preventing new Buffer::Chunk
  // instances from being added and allowing the Pull operation to know when
  // to signal that the flow of data is completed.
  inline void End() { ended_ = true; }
  inline bool is_ended() const { return ended_; }
  inline bool is_finished() const {
    return ended_ &&
    remaining_ == 0 &&
    finished_;
  }

  // Push inbound data onto the buffer.
  void Push(Environment* env, const uint8_t* data, size_t len);

  // Push outbound data onto the buffer.
  void Push(
      std::shared_ptr<v8::BackingStore> data,
      size_t length,
      size_t offset = 0);

  // Increment the given number of bytes within the buffer. If amount
  // is greater than length(), length() bytes are advanced. Returns
  // the actual number of bytes advanced. Will not cause bytes to be
  // freed.
  size_t Seek(size_t amount);

  // Acknowledge the given number of bytes in the buffer. May cause
  // bytes to be freed.
  size_t Acknowledge(size_t amount);

  // Clears any bytes remaining in the buffer.
  inline void Clear() {
    queue_.clear();
    head_ = 0;
    length_ = 0;
    remaining_ = 0;
  }

  // The total number of unacknowledged bytes remaining. The length
  // is incremented by Push and decremented by Acknowledge.
  inline size_t length() const { return length_; }

  // The total number of unread bytes remaining. The remaining
  // length is incremental by Push and decremented by Seek.
  inline size_t remaining() const { return remaining_; }

  // Flushes the entire inbound queue into a v8::Local<v8::Array>
  // of Uint8Array instances, returning the total number of bytes
  // released to the consumer.
  v8::Maybe<size_t> Release(Consumer* consumer);

  int DoPull(
      bob::Next<ngtcp2_vec> next,
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

  // The queue_ index of the current read head.
  // This is incremented by Seek() as necessary and
  // decremented by Acknowledge() as data is consumed.
  size_t head_ = 0;
  size_t length_ = 0;
  size_t remaining_ = 0;
};

// The JSQuicBufferConsumer receives inbound data for a Stream
// and forwards that up as Uint8Array instances to the JavaScript
// API.
//
// Someone reviewing this code might notice that this is definitely
// not a StreamBase although it serves a similar purpose -- pushing
// chunks of data out to the JavaScript side. In this case, StreamBase
// would be way too complicated for what is strictly needed here.
// We don't need all of the mechanism that StreamBase brings along
// with it so we don't use it.
class JSQuicBufferConsumer final : public Buffer::Consumer,
                                   public AsyncWrap {
 public:
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Pushes the entire queue out to JavaScript as an array of
  // ArrayBuffer instances. The Process() takes ownership of
  // the queue here and ensures that once the contents have been
  // passed on the JS, the data is freed.
  v8::Maybe<size_t> Process(
      Buffer::Chunk::Queue queue,
      bool ended = false) override;

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(JSQuicBufferConsumer)
  SET_SELF_SIZE(JSQuicBufferConsumer)

 private:
  JSQuicBufferConsumer(
      Environment* env,
      v8::Local<v8::Object> wrap);
};

// The NullSource is used when no payload source is provided
// for a Stream. Whenever DoPull is called, it simply
// immediately responds with no data and EOS set.
class NullSource final : public Buffer::Source,
                         public BaseObject {
 public:
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static BaseObjectPtr<NullSource> Create(Environment* env);

  NullSource(Environment* env, v8::Local<v8::Object> object);

  int DoPull(
      bob::Next<ngtcp2_vec> next,
      int options,
      ngtcp2_vec* data,
      size_t count,
      size_t max_count_hint) override;

  BaseObjectPtr<BaseObject> GetStrongPtr() override {
    return BaseObjectPtr<BaseObject>(this);
  }

  size_t Acknowledge(uint64_t offset, size_t datalen) override { return 0; }

  size_t Seek(size_t amount) override { return 0; }

  bool is_finished() const override { return finished_; }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(NullSource)
  SET_SELF_SIZE(NullSource)

 private:
  bool finished_ = false;
};

// Receives a single ArrayBufferView and uses it's contents as the
// complete source of outbound data for the Stream.
class ArrayBufferViewSource final : public Buffer::Source,
                                    public BaseObject {
 public:
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  int DoPull(
      bob::Next<ngtcp2_vec> next,
      int options,
      ngtcp2_vec* data,
      size_t count,
      size_t max_count_hint) override;

  BaseObjectPtr<BaseObject> GetStrongPtr() override {
    return BaseObjectPtr<BaseObject>(this);
  }
  size_t Acknowledge(uint64_t offset, size_t datalen) override;
  size_t Seek(size_t amount) override;

  bool is_finished() const override { return buffer_.is_finished(); }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(ArrayBufferViewSource);
  SET_SELF_SIZE(ArrayBufferViewSource);

 private:
  ArrayBufferViewSource(
      Environment* env,
      v8::Local<v8::Object> wrap,
      const std::shared_ptr<v8::BackingStore>& store,
      size_t length,
      size_t offset);

  Buffer buffer_;
};

// Wraps a Blob instance that provides the outbound data.
class BlobSource final : public AsyncWrap,
                         public Buffer::Source {
 public:
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  int DoPull(
      bob::Next<ngtcp2_vec> next,
      int options,
      ngtcp2_vec* data,
      size_t count,
      size_t max_count_hint) override;

  BaseObjectPtr<BaseObject> GetStrongPtr() override {
    return BaseObjectPtr<BaseObject>(this);
  }
  size_t Acknowledge(uint64_t offset, size_t datalen) override;
  size_t Seek(size_t amount) override;

  bool is_finished() const override { return buffer_.is_finished(); }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(BlobSource);
  SET_SELF_SIZE(BlobSource);

 private:
  BlobSource(
      Environment* env,
      v8::Local<v8::Object> wrap,
      BaseObjectPtr<Blob> blob);

  Buffer buffer_;
};

// Implements StreamBase to asynchronously accept outbound data from the
// JavaScript side.
class StreamSource final : public AsyncWrap,
                           public Buffer::Source {
 public:
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  size_t Acknowledge(uint64_t offset, size_t datalen) override;
  size_t Seek(size_t amount) override;

  void set_closed() override;

  static void End(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Write(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WriteV(const v8::FunctionCallbackInfo<v8::Value>& args);

  int DoPull(
      bob::Next<ngtcp2_vec> next,
      int options,
      ngtcp2_vec* data,
      size_t count,
      size_t max_count_hint) override;

  BaseObjectPtr<BaseObject> GetStrongPtr() override {
    return BaseObjectPtr<BaseObject>(this);
  }

  bool is_finished() const override { return queue_.is_finished(); }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(StreamSource);
  SET_SELF_SIZE(StreamSource);

 private:
  StreamSource(Environment* env, v8::Local<v8::Object> wrap);

  Buffer queue_;
};

// Implements StreamListener to receive data from any native level
// StreamBase implementation.
class StreamBaseSource final : public AsyncWrap,
                               public Buffer::Source,
                               public StreamListener {
 public:
  static bool HasInstance(Environment* env, v8::Local<v8::Value> value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  ~StreamBaseSource() override;

  int DoPull(
      bob::Next<ngtcp2_vec> next,
      int options,
      ngtcp2_vec* data,
      size_t count,
      size_t max_count_hint) override;

  size_t Acknowledge(uint64_t offset, size_t datalen) override;
  size_t Seek(size_t amount) override;

  uv_buf_t OnStreamAlloc(size_t suggested_size) override;
  void OnStreamRead(ssize_t nread, const uv_buf_t& buf) override;

  BaseObjectPtr<BaseObject> GetStrongPtr() override {
    return BaseObjectPtr<BaseObject>(this);
  }

  void set_closed() override;

  bool is_finished() const override { return buffer_.is_finished(); }

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

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_QUIC_BUFFER_H_
