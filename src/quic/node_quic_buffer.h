#ifndef SRC_QUIC_NODE_QUIC_BUFFER_H_
#define SRC_QUIC_NODE_QUIC_BUFFER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "memory_tracker-inl.h"
#include "ngtcp2/ngtcp2.h"
#include "node.h"
#include "node_internals.h"
#include "util.h"
#include "uv.h"

#include <vector>

namespace node {
namespace quic {

class QuicBuffer;

constexpr size_t kMaxVectorCount = 16;

using DoneCB = std::function<void(int)>;

// A QuicBufferChunk contains the actual buffered data
// along with a callback to be called when the data has
// been consumed.
class QuicBufferChunk : public MemoryRetainer {
 public:
  // Default non-op done handler.
  static void default_done(int status) {}

  // In this variant, the QuicBufferChunk owns the underlying
  // data storage within a MaybeStackBuffer. The data will be
  // freed when the QuicBufferChunk is destroyed.
  inline explicit QuicBufferChunk(size_t len);

  // In this variant, the QuicBufferChunk only maintains a
  // pointer to the underlying data buffer. The QuicBufferChunk
  // does not take ownership of the buffer. The done callback
  // is invoked to let the caller know when the chunk is no
  // longer being used.
  inline QuicBufferChunk(uv_buf_t buf_, DoneCB done_);

  inline ~QuicBufferChunk() override;

  inline void done(int status);

  // length() provides the remaining unacknowledged number of bytes.
  inline size_t length() const { return length_; }

  // remaining() provides the remaining unread number of bytes.
  inline size_t remaining() const { return buf_.len; }

  // Consumes (acknowledges) the given number of bytes. If amount
  // is greater than length(), only length() bytes are consumed.
  // Returns the actual number of bytes consumed.
  inline size_t consume(size_t amount);

  // Seeks (reads) the given number of bytes. If amount is greater
  // than remaining(), only remaining() bytes are read. Returns
  // the actual number of bytes read.
  inline size_t seek(size_t amount);

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

// A QuicBuffer is a linked-list of QuicBufferChunk instances.
// There are three significant pointers: root_, head_, and tail_.
//   * root_ is the base of the linked list
//   * head_ is a pointer to the current read position of the linked list
//   * tail_ is a pointer to the current write position of the linked list
// Items are dropped from the linked list only when either consume() or
// cancel() is called. consume() will consume a given number of bytes up
// to, but not including the read head_. cancel() will consume all remaining
// bytes in the linked list. As whole QuicBufferChunk instances are
// consumed, the corresponding Done callback will be invoked, allowing
// any memory to be freed up.
//
// Use Seek(n) to advance the read head_ forward n positions.
//
// DrainInto() will drain the remaining QuicBufferChunk instances
// into a vector and will advance the read head_ to the end of the
// QuicBuffer. The function will return the number of positions drained
// which would then be passed to Seek(n) to advance the read head.
//
// QuicBuffer supports move assignment that will completely reset the source.
// That is,
//  QuicBuffer buf1;
//  QuicBuffer buf2;
//  buf2 = std::move(buf1);
//
// Will reset the state of buf2 to that of buf1, then reset buf1
//
// There is also an overloaded += operator that will append the source
// content to the destination and reset the source.
// That is,
//  QuicBuffer buf1;
//  QuicBuffer buf2;
//  buf2 += std::move(buf1);
//
// Will append the contents of buf1 to buf2, then reset buf1
class QuicBuffer : public MemoryRetainer {
 public:
  QuicBuffer() = default;

  inline QuicBuffer(QuicBuffer&& src) noexcept;
  inline QuicBuffer& operator=(QuicBuffer&& src) noexcept;

  ~QuicBuffer() override {
    cancel();  // Cancel the remaining data
    CHECK_EQ(length_, 0);
  }

  void end() { ended_ = true; }
  bool is_ended() const { return ended_; }

  // Push one or more uv_buf_t instances into the buffer.
  // the DoneCB callback will be invoked when the last
  // uv_buf_t in the bufs array is consumed and popped out
  // of the internal linked list.
  size_t push(
      uv_buf_t* bufs,
      size_t nbufs,
      DoneCB done = QuicBufferChunk::default_done);

  // Pushes a single QuicBufferChunk into the linked list
  void push(std::unique_ptr<QuicBufferChunk> chunk);

  // Consume the given number of bytes within the buffer. If amount
  // is greater than length(), length() bytes are consumed. Returns
  // the actual number of bytes consumed.
  inline size_t consume(size_t amount);

  // Cancels the remaining bytes within the buffer.
  inline size_t cancel(int status = UV_ECANCELED);

  // Seeks (reads) the given number of bytes. If amount is greater
  // than remaining(), seeks remaining() bytes. Returns the actual
  // number of bytes read.
  size_t seek(size_t amount);

  // The total number of unacknowledged bytes remaining.
  size_t length() const { return length_; }

  // The total number of unread bytes remaining.
  size_t remaining() const { return remaining_; }

  // Drain the remaining buffers into the given vector.
  // The function will return the number of positions the
  // read head_ can be advanced.
  inline size_t DrainInto(
      std::vector<uv_buf_t>* list,
      size_t* length = nullptr,
      size_t max_count = kMaxVectorCount);

  template <typename T>
  inline size_t DrainInto(
      std::vector<T>* list,
      size_t* length,
      size_t max_count);

  template <typename T>
  inline size_t DrainInto(
      T* list,
      size_t* count,
      size_t* length,
      size_t max_count);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicBuffer);
  SET_SELF_SIZE(QuicBuffer);

 private:
  inline static bool is_empty(uv_buf_t buf);
  size_t consume(int status, size_t amount);
  bool pop(int status = 0);
  inline void push(uv_buf_t buf, DoneCB done = nullptr);

  template <typename Fn>
  inline size_t DrainInto(Fn&& add_to_list, size_t* length, size_t max_count);

  std::unique_ptr<QuicBufferChunk> root_;
  QuicBufferChunk* head_ = nullptr;  // Current Read Position
  QuicBufferChunk* tail_ = nullptr;  // Current Write Position

  bool ended_ = false;
  size_t length_ = 0;
  size_t remaining_ = 0;

  friend class QuicBufferChunk;
};

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS

#endif  // SRC_QUIC_NODE_QUIC_BUFFER_H_
