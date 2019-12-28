#ifndef SRC_NODE_QUIC_BUFFER_H_
#define SRC_NODE_QUIC_BUFFER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "memory_tracker-inl.h"
#include "ngtcp2/ngtcp2.h"
#include "node.h"
#include "node_internals.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

#include <array>
#include <algorithm>
#include <functional>
#include <vector>

namespace node {
namespace quic {

constexpr size_t MAX_VECTOR_COUNT = 16;

// QuicBuffer an internal linked list of uv_buf_t instances
// representing data that is to be sent. All data in the
// Buffer has to be retained until it is Consumed or Canceled.
// For QUIC, the data is not consumed until an explicit ack
// is received or we know that we do not need the data.

typedef std::function<void(int status)> done_cb;

// Default non-op done handler.
inline void default_quic_buffer_chunk_done(int status) {}

inline bool IsEmptyBuffer(const uv_buf_t& buf) {
  return buf.len == 0 || buf.base == nullptr;
}

// A quic_buffer_chunk contains the actual buffered data
// along with a callback, and optional V8 object that
// should be kept alive as long as the chunk is alive.
struct quic_buffer_chunk : public MemoryRetainer {
  MallocedBuffer<uint8_t> data_buf;
  uv_buf_t buf;
  done_cb done = default_quic_buffer_chunk_done;
  size_t offset = 0;
  size_t roffset = 0;
  bool done_called = false;
  std::unique_ptr<quic_buffer_chunk> next;

  quic_buffer_chunk(
      MallocedBuffer<uint8_t>&& buf_,
      done_cb done_)
    : quic_buffer_chunk(uv_buf_init(reinterpret_cast<char*>(buf_.data),
                                    buf_.size),
                        done_) {
    data_buf = std::move(buf_);
  }

  explicit quic_buffer_chunk(uv_buf_t buf_) : buf(buf_) {}

  quic_buffer_chunk(
      uv_buf_t buf_,
      done_cb done_)
    : quic_buffer_chunk(buf_) {
    done = std::move(done_);
  }

  ~quic_buffer_chunk() override {
    CHECK(done_called);
  }

  void Done(int status) {
    done_called = true;
    std::move(done)(status);
  }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackFieldWithSize("buf", buf.len);
  }

  SET_MEMORY_INFO_NAME(quic_buffer_chunk)
  SET_SELF_SIZE(quic_buffer_chunk)
};

// A QuicBuffer is a linked-list of quic_buffer_chunk instances.
// There are three significant pointers: root_, head_, and tail_.
//   * root_ is the base of the linked list
//   * head_ is a pointer to the current read position of the linked list
//   * tail_ is a pointer to the current write position of the linked list
// Items are dropped from the linked list only when either Consume() or
// Cancel() is called. Consume() will consume a given number of bytes up
// to, but not including the read head_. Cancel() will consume all remaining
// bytes in the linked list. As whole quic_buffer_chunk instances are
// consumed, the corresponding Done callback will be invoked, allowing
// any memory to be freed up.
//
// Use SeekHead(n) to advance the read head_ forward n positions.
//
// DrainInto() will drain the remaining quic_buffer_chunk instances
// into a vector and will advance the read head_ to the end of the
// QuicBuffer. The function will return the number of positions drained
// which would then be passed to SeekHead(n) to advance the read head.
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
  QuicBuffer()
    : head_(nullptr),
      tail_(nullptr),
      size_(0),
      count_(0),
      length_(0),
      rlength_(0) {}

  QuicBuffer(QuicBuffer&& src) noexcept
    : head_(src.head_),
      tail_(src.tail_),
      size_(src.size_),
      count_(src.count_),
      length_(src.length_),
      rlength_(src.rlength_) {
    root_ = std::move(src.root_);
    src.head_ = nullptr;
    src.tail_ = nullptr;
    src.size_ = 0;
    src.length_ = 0;
    src.rlength_ = 0;
  }

  QuicBuffer& operator=(QuicBuffer&& src) noexcept {
    if (this == &src) return *this;
    this->~QuicBuffer();
    return *new(this) QuicBuffer(std::move(src));
  }

  QuicBuffer& operator+=(QuicBuffer&& src) noexcept {
    if (tail_ == nullptr) {
      // If this thing is empty, just do a move...
      return *this = std::move(src);
    }

    tail_->next = std::move(src.root_);
    // If head_ is null, then it had been read to the
    // end, set the new head_ equal to the appended
    // root.
    if (head_ == nullptr)
      head_ = tail_->next.get();
    tail_ = src.tail_;
    length_ += src.length_;
    rlength_ += src.length_;
    size_ += src.size_;
    count_ += src.size_;
    src.head_ = nullptr;
    src.tail_ = nullptr;
    src.size_ = 0;
    src.length_ = 0;
    src.rlength_ = 0;
    return *this;
  }

  ~QuicBuffer() override {
    Cancel();  // Cancel the remaining data
    CHECK_EQ(length_, 0);
  }

  size_t Copy(uv_buf_t* bufs, size_t nbufs) {
    size_t total = 0;
    for (size_t n = 0; n < nbufs; n++) {
      MallocedBuffer<uint8_t> data(bufs[n].len);
      memcpy(data.data, bufs[n].base, bufs[n].len);
      total += bufs[n].len;
      Push(std::move(data));
    }
    return total;
  }

  // Push one or more uv_buf_t instances into the buffer.
  // the done_cb callback will be invoked when the last
  // uv_buf_t in the bufs array is consumed and popped out
  // of the internal linked list.
  size_t Push(
      uv_buf_t* bufs,
      size_t nbufs,
      done_cb done = default_quic_buffer_chunk_done) {
    size_t len = 0;
    if (nbufs == 0 || bufs == nullptr || IsEmptyBuffer(bufs[0])) {
      done(0);
      return 0;
    }
    size_t n = 0;
    while (nbufs > 1) {
      if (!IsEmptyBuffer(bufs[n])) {
        Push(bufs[n]);
        length_ += bufs[n].len;
        rlength_ += bufs[n].len;
        len += bufs[n].len;
      }
      n++;
      nbufs--;
    }
    length_ += bufs[n].len;
    rlength_ += bufs[n].len;
    len += bufs[n].len;
    Push(bufs[n], done);
    return len;
  }

  // Push a single malloc buf into the buffer.
  // The done_cb will be invoked when the buf is consumed
  // and popped out of the internal linked list.
  size_t Push(
      MallocedBuffer<uint8_t>&& buffer,
      done_cb done = default_quic_buffer_chunk_done) {
    if (buffer.size == 0) {
      done(0);
      return 0;
    }
    length_ += buffer.size;
    rlength_ += buffer.size;
    Push(new quic_buffer_chunk(std::move(buffer), done));
    return buffer.size;
  }

  // Consume the given number of bytes within the buffer. If amount is
  // negative, all buffered bytes that are available to be consumed are
  // consumed.
  void Consume(ssize_t amount = -1) { Consume(0, amount); }

  // Cancels the remaining bytes within the buffer
  size_t Cancel(int status = UV_ECANCELED) {
    size_t remaining = Length();
    Consume(status, -1);
    return remaining;
  }

  // The total buffered bytes
  size_t Length() {
    return length_;
  }

  size_t RemainingLength() {
    return rlength_;
  }

  // The total number of buffers
  size_t Size() {
    return size_;
  }

  // The number of buffers remaining to be read
  size_t ReadRemaining() {
    return count_;
  }

  // Drain the remaining buffers into the given vector.
  // The function will return the number of positions the
  // read head_ can be advanced.
  size_t DrainInto(
      std::vector<uv_buf_t>* list,
      size_t* length = nullptr,
      size_t max_count = MAX_VECTOR_COUNT) {
    return DrainInto(
        [&](uv_buf_t buf) { list->push_back(buf); },
        length,
        max_count);
  }

  template <typename T>
  size_t DrainInto(
      std::vector<T>* list,
      size_t* length = nullptr,
      size_t max_count = MAX_VECTOR_COUNT) {
    return DrainInto([&](uv_buf_t buf) {
      list->push_back(T {
        reinterpret_cast<uint8_t*>(buf.base), buf.len });
    }, length, max_count);
  }

  template <typename T>
  size_t DrainInto(
      T* list,
      size_t* count,
      size_t* length = nullptr,
      size_t max_count = MAX_VECTOR_COUNT) {
    *count = 0;
    return DrainInto([&](uv_buf_t buf) {
      list[*count]->base = reinterpret_cast<uint8_t*>(buf.base);
      list[*count]->len = buf.len;
      *count += 1;
    }, length, max_count);
    CHECK_LE(*count, max_count);
  }

  // Returns the current read head or an empty buffer if
  // we're empty
  uv_buf_t Head() {
    if (head_ == nullptr)
      return uv_buf_init(nullptr, 0);
    return uv_buf_init(
        head_->buf.base + head_->roffset,
        head_->buf.len - head_->roffset);
  }

  // Moves the current read head forward the given
  // number of buffers. If amount is greater than
  // the number of buffers remaining, move to the
  // end, and return the actual number advanced.
  size_t SeekHead(size_t amount = 1) {
    size_t n = 0;
    size_t amt = amount;
    while (head_ != nullptr && amt > 0) {
      head_ = head_->next.get();
      n++;
      amt--;
      count_--;
      rlength_ -= head_ == nullptr ? 0 : head_->buf.len;
    }
    return n;
  }

  void SeekHeadOffset(ssize_t amount) {
    if (amount < 0)
      return;
    size_t amt = std::min(amount < 0 ? length_ : amount, length_);
    while (head_ && amt > 0) {
      size_t len = head_->buf.len - head_->roffset;
      // If the remaining length in the head is greater than the
      // amount we're seeking, just adjust the roffset
      if (len > amt) {
        head_->roffset += amt;
        rlength_ -= amt;
        break;
      }
      // Otherwise, decrement the amt and advance the read head
      // one space and iterate from there.
      amt -= len;
      rlength_ -= len;
      head_ = head_->next.get();
    }
  }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackFieldWithSize("length", length_);
  }
  SET_MEMORY_INFO_NAME(QuicBuffer);
  SET_SELF_SIZE(QuicBuffer);

 private:
  template <typename Fn>
  size_t DrainInto(Fn&& add_to_list, size_t* length, size_t max_count) {
    size_t len = 0;
    size_t count = 0;
    bool seen_head = false;
    quic_buffer_chunk* pos = head_;
    if (pos == nullptr)
      return 0;
    if (length != nullptr) *length = 0;
    while (pos != nullptr && count < max_count) {
      count++;
      size_t datalen = pos->buf.len - pos->roffset;
      if (length != nullptr) *length += datalen;
      add_to_list(uv_buf_init(pos->buf.base + pos->roffset, datalen));
      if (pos == head_) seen_head = true;
      if (seen_head) len++;
      pos = pos->next.get();
    }
    return len;
  }


  void Push(quic_buffer_chunk* chunk) {
    size_++;
    count_++;
    if (!tail_) {
      root_.reset(chunk);
      head_ = tail_ = root_.get();
    } else {
      tail_->next.reset(chunk);
      tail_ = tail_->next.get();
      if (!head_)
        head_ = tail_;
    }
  }

  void Push(uv_buf_t buf) {
    Push(new quic_buffer_chunk(buf));
  }

  void Push(uv_buf_t buf, done_cb done) {
    Push(new quic_buffer_chunk(buf, done));
  }

  bool Pop(int status = 0) {
    if (!root_)
      return false;
    std::unique_ptr<quic_buffer_chunk> root(std::move(root_));
    root_ = std::move(root.get()->next);
    size_--;

    if (head_ == root.get())
      head_ = root_.get();
    if (tail_ == root.get())
      tail_ = root_.get();

    root->Done(status);
    return true;
  }

  void Consume(int status, ssize_t amount) {
    size_t amt = std::min(amount < 0 ? length_ : amount, length_);
    while (root_ && amt > 0) {
      auto root = root_.get();
      // Never allow for partial consumption of head when using a
      // non-cancel status
      // if (status == 0 && head_ == root)
      //   break;
      size_t len = root->buf.len - root->offset;
      if (len > amt) {
        length_ -= amt;
        root->offset += amt;
        break;
      }
      length_ -= len;
      amt -= len;
      Pop(status);
    }
  }

  std::unique_ptr<quic_buffer_chunk> root_;
  quic_buffer_chunk* head_;  // Current Read Position
  quic_buffer_chunk* tail_;  // Current Write Position
  size_t size_;
  size_t count_;
  size_t length_;
  size_t rlength_;
};

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS

#endif  // SRC_NODE_QUIC_BUFFER_H_
