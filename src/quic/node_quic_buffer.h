#ifndef SRC_QUIC_NODE_QUIC_BUFFER_H_
#define SRC_QUIC_NODE_QUIC_BUFFER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "memory_tracker-inl.h"
#include "ngtcp2/ngtcp2.h"
#include "node.h"
#include "node_bob.h"
#include "node_internals.h"
#include "util.h"
#include "uv.h"

#include <vector>

namespace node {
namespace quic {

class QuicBuffer;

constexpr size_t kMaxVectorCount = 16;

using DoneCB = std::function<void(int)>;

// When data is sent over QUIC, we are required to retain it in memory
// until we receive an acknowledgement that it has been successfully
// received. The QuicBuffer object is what we use to handle that
// and track until it is acknowledged. To understand the QuicBuffer
// object itself, it is important to understand how ngtcp2 and nghttp3
// handle data that is given to it to serialize into QUIC packets.
//
// An individual QUIC packet may contain multiple QUIC frames. Whenever
// we create a QUIC packet, we really have no idea what frames are going
// to be encoded or how much buffered handshake or stream data is going
// to be included within that QuicPacket. If there is buffered data
// available for a stream, we provide an array of pointers to that data
// and an indication about how much data is available, then we leave it
// entirely up to ngtcp2 and nghttp3 to determine how much of the data
// to encode into the QUIC packet. It is only *after* the QUIC packet
// is encoded that we can know how much was actually written.
//
// Once written to a QUIC Packet, we have to keep the data in memory
// until an acknowledgement is received. In QUIC, acknowledgements are
// received per range of packets.
//
// QuicBuffer is complicated because it needs to be able to accomplish
// three things: (a) buffering uv_buf_t instances passed down from
// JavaScript without memcpy and keeping track of the Write callback
// associated with each, (b) tracking what data has already been
// encoded in a QUIC packet and what data is remaining to be read, and
// (c) tracking which data has been acknowledged and which hasn't.
// QuicBuffer is further complicated by design quirks and limitations
// of the StreamBase API and how it interacts with the JavaScript side.
//
// QuicBuffer is a linked list of QuicBufferChunk instances.
// A single QuicBufferChunk wraps a single non-zero-length uv_buf_t.
// When the QuicBufferChunk is created, we capture the total length
// of the buffer and the total number of bytes remaining to be sent.
// Initially, these numbers are identical.
//
// When data is encoded into a QuicPacket, we advance the QuicBufferChunk's
// remaining-to-be-read by the number of bytes actually encoded. If there
// are no more bytes remaining to be encoded, we move to the next chunk
// in the linked list.
//
// When an acknowledgement is received, we decrement the QuicBufferChunk's
// length by the number of acknowledged bytes. Once the unacknowledged
// length reaches 0, we invoke the callback function associated with the
// QuicBufferChunk (if any).
//
// QuicStream is a StreamBase implementation. For every DoWrite call,
// it receives one or more uv_buf_t instances in a single batch associated
// with a single write callback. For each uv_buf_t DoWrite gets, a
// corresponding QuicBufferChunk is added to the QuicBuffer, with the
// callback associated with the final chunk added to the list.


// A QuicBufferChunk contains the actual buffered data
// along with a callback to be called when the data has
// been consumed.
//
// Any given chunk has a remaining-to-be-acknowledged length (length()) and a
// remaining-to-be-read-length (remaining()). The former tracks the number
// of bytes that have yet to be acknowledged by the QUIC peer. Once the
// remaining-to-be-acknowledged length reaches zero, the done callback
// associated with the QuicBufferChunk can be called and the QuicBufferChunk
// can be discarded. The remaining-to-be-read length is adjusted as data is
// serialized into QUIC packets and sent.
// The remaining-to-be-acknowledged length is adjusted using consume(),
// while the remaining-to-be-ead length is adjusted using seek().
class QuicBufferChunk final : public MemoryRetainer {
 public:
  // Default non-op done handler.
  static void default_done(int status) {}

  // In this variant, the QuicBufferChunk owns the underlying
  // data storage within a vector. The data will be
  // freed when the QuicBufferChunk is destroyed.
  inline explicit QuicBufferChunk(size_t len);

  // In this variant, the QuicBufferChunk only maintains a
  // pointer to the underlying data buffer. The QuicBufferChunk
  // does not take ownership of the buffer. The done callback
  // is invoked to let the caller know when the chunk is no
  // longer being used.
  inline QuicBufferChunk(uv_buf_t buf_, DoneCB done_);

  inline ~QuicBufferChunk() override;

  // Invokes the done callback associated with the QuicBufferChunk.
  inline void Done(int status);

  // length() provides the remaining-to-be-acknowledged length.
  // The QuicBufferChunk must be retained in memory while this
  // count is greater than zero. The length is adjusted by
  // calling Consume();
  inline size_t length() const { return length_; }

  // remaining() provides the remaining-to-be-read length number of bytes.
  // The length is adjusted by calling Seek()
  inline size_t remaining() const { return buf_.len; }

  // Consumes (acknowledges) the given number of bytes. If amount
  // is greater than length(), only length() bytes are consumed.
  // Returns the actual number of bytes consumed.
  inline size_t Consume(size_t amount);

  // Seeks (reads) the given number of bytes. If amount is greater
  // than remaining(), only remaining() bytes are read. Returns
  // the actual number of bytes read.
  inline size_t Seek(size_t amount);

  uint8_t* out() { return reinterpret_cast<uint8_t*>(buf_.base); }
  uv_buf_t buf() { return buf_; }
  const uv_buf_t buf() const { return buf_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicBufferChunk)
  SET_SELF_SIZE(QuicBufferChunk)

 private:
  std::vector<uint8_t> data_buf_;
  uv_buf_t buf_;
  DoneCB done_ = default_done;
  size_t length_ = 0;
  bool done_called_ = false;
  std::unique_ptr<QuicBufferChunk> next_;

  friend class QuicBuffer;
};

class QuicBuffer final : public bob::SourceImpl<ngtcp2_vec>,
                         public MemoryRetainer {
 public:
  QuicBuffer() = default;

  inline QuicBuffer(QuicBuffer&& src) noexcept;
  inline QuicBuffer& operator=(QuicBuffer&& src) noexcept;

  ~QuicBuffer() override {
    Cancel();  // Cancel the remaining data
    CHECK_EQ(length_, 0);
  }

  // Marks the QuicBuffer as having ended, preventing new QuicBufferChunk
  // instances from being appended to the linked list and allowing the
  // Pull operation to know when to signal that the flow of data is
  // completed.
  void End() { ended_ = true; }
  bool is_ended() const { return ended_; }

  // Push one or more uv_buf_t instances into the buffer.
  // the DoneCB callback will be invoked when the last
  // uv_buf_t in the bufs array is consumed and popped out
  // of the internal linked list. Ownership of the uv_buf_t
  // remains with the caller.
  size_t Push(
      uv_buf_t* bufs,
      size_t nbufs,
      DoneCB done = QuicBufferChunk::default_done);

  // Pushes a single QuicBufferChunk into the linked list
  void Push(std::unique_ptr<QuicBufferChunk> chunk);

  // Consume the given number of bytes within the buffer. If amount
  // is greater than length(), length() bytes are consumed. Returns
  // the actual number of bytes consumed.
  inline size_t Consume(size_t amount);

  // Cancels the remaining bytes within the buffer.
  inline size_t Cancel(int status = UV_ECANCELED);

  // Seeks (reads) the given number of bytes. If amount is greater
  // than remaining(), seeks remaining() bytes. Returns the actual
  // number of bytes read.
  size_t Seek(size_t amount);

  // The total number of unacknowledged bytes remaining.
  size_t length() const { return length_; }

  // The total number of unread bytes remaining.
  size_t remaining() const { return remaining_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicBuffer);
  SET_SELF_SIZE(QuicBuffer);

 protected:
  int DoPull(
      bob::Next<ngtcp2_vec> next,
      int options,
      ngtcp2_vec* data,
      size_t count,
      size_t max_count_hint) override;

 private:
  inline static bool is_empty(uv_buf_t buf);
  size_t Consume(int status, size_t amount);
  bool Pop(int status = 0);
  inline void Push(uv_buf_t buf, DoneCB done = nullptr);

  std::unique_ptr<QuicBufferChunk> root_;
  QuicBufferChunk* head_ = nullptr;  // Current Read Position
  QuicBufferChunk* tail_ = nullptr;  // Current Write Position

  bool canceled_ = false;
  bool ended_ = false;
  size_t length_ = 0;
  size_t remaining_ = 0;

  friend class QuicBufferChunk;
};

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS

#endif  // SRC_QUIC_NODE_QUIC_BUFFER_H_
