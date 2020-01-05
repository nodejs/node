#include "node_quic_buffer-inl.h"
#include "util.h"
#include "uv.h"

#include <array>
#include <algorithm>
#include <functional>
#include <vector>

namespace node {
namespace quic {

namespace {
inline bool IsEmptyBuffer(const uv_buf_t& buf) {
  return buf.len == 0 || buf.base == nullptr;
}
}  // namespace

void QuicBufferChunk::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("data_buf", data_buf_.size());
  tracker->TrackField("next", next_);
}

QuicBuffer& QuicBuffer::operator+=(QuicBuffer&& src) noexcept {
  if (tail_ == nullptr) {
    // If this thing is empty, just do a move...
    return *this = std::move(src);
  }

  tail_->next_ = std::move(src.root_);
  // If head_ is null, then it had been read to the
  // end, set the new head_ equal to the appended
  // root.
  if (head_ == nullptr)
    head_ = tail_->next_.get();
  tail_ = src.tail_;
  length_ += src.length_;
  rlength_ += src.length_;
  size_ += src.size_;
  count_ += src.size_;
  Reset(&src);
  return *this;
}

size_t QuicBuffer::Push(uv_buf_t* bufs, size_t nbufs, done_cb done) {
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

void QuicBuffer::Push(std::unique_ptr<QuicBufferChunk> chunk) {
  size_++;
  count_++;
  if (!tail_) {
    root_ = std::move(chunk);
    head_ = tail_ = root_.get();
  } else {
    tail_->next_ = std::move(chunk);
    tail_ = tail_->next_.get();
    if (!head_)
      head_ = tail_;
  }
}

size_t QuicBuffer::SeekHead(size_t amount) {
  size_t n = 0;
  size_t amt = amount;
  while (head_ != nullptr && amt > 0) {
    head_ = head_->next_.get();
    n++;
    amt--;
    count_--;
    rlength_ -= head_ == nullptr ? 0 : head_->buf_.len;
  }
  return n;
}

void QuicBuffer::SeekHeadOffset(ssize_t amount) {
  // If amount is negative, then we use the full length
  size_t amt = UNLIKELY(amount < 0) ?
      length_ :
      std::min(length_, static_cast<size_t>(amount));
  while (head_ && amt > 0) {
    size_t len = head_->buf_.len - head_->roffset_;
    // If the remaining length in the head is greater than the
    // amount we're seeking, just adjust the roffset
    if (len > amt) {
      head_->roffset_ += amt;
      rlength_ -= amt;
      break;
    }
    // Otherwise, decrement the amt and advance the read head
    // one space and iterate from there.
    amt -= len;
    rlength_ -= len;
    head_ = head_->next_.get();
  }
}

bool QuicBuffer::Pop(int status) {
  if (!root_)
    return false;
  std::unique_ptr<QuicBufferChunk> root(std::move(root_));
  root_ = std::move(root.get()->next_);
  size_--;

  if (head_ == root.get())
    head_ = root_.get();
  if (tail_ == root.get())
    tail_ = root_.get();

  root->Done(status);
  return true;
}

void QuicBuffer::Consume(int status, ssize_t amount) {
  size_t amt = std::min(amount < 0 ? length_ : amount, length_);
  while (root_ && amt > 0) {
    auto root = root_.get();
    size_t len = root->buf_.len - root->offset_;
    if (len > amt) {
      length_ -= amt;
      root->offset_ += amt;
      break;
    }
    length_ -= len;
    amt -= len;
    Pop(status);
  }
}

}  // namespace quic
}  // namespace node
