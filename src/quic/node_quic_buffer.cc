#include "node_quic_buffer-inl.h"  // NOLINT(build/include)
#include "util.h"
#include "uv.h"

#include <array>
#include <algorithm>
#include <functional>
#include <vector>

namespace node {
namespace quic {

void QuicBufferChunk::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("data_buf", data_buf_.size());
  tracker->TrackField("next", next_);
}

size_t QuicBuffer::push(uv_buf_t* bufs, size_t nbufs, DoneCB done) {
  size_t len = 0;
  if (nbufs == 0 || bufs == nullptr || is_empty(bufs[0])) {
    done(0);
    return 0;
  }
  size_t n = 0;
  while (nbufs > 1) {
    if (!is_empty(bufs[n])) {
      push(bufs[n]);
      len += bufs[n].len;
    }
    n++;
    nbufs--;
  }
  len += bufs[n].len;
  push(bufs[n], done);
  return len;
}

void QuicBuffer::push(std::unique_ptr<QuicBufferChunk> chunk) {
  CHECK(!ended_);
  length_ += chunk->remaining();
  remaining_ += chunk->remaining();
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

size_t QuicBuffer::seek(size_t amount) {
  size_t len = 0;
  while (head_ && amount > 0) {
    size_t amt = head_->seek(amount);
    amount -= amt;
    len += amt;
    remaining_ -= amt;
    if (head_->remaining())
      break;
    head_ = head_->next_.get();
  }
  return len;
}

bool QuicBuffer::pop(int status) {
  if (!root_)
    return false;
  std::unique_ptr<QuicBufferChunk> root(std::move(root_));
  root_ = std::move(root.get()->next_);

  if (head_ == root.get())
    head_ = root_.get();
  if (tail_ == root.get())
    tail_ = root_.get();

  root->done(status);
  return true;
}

size_t QuicBuffer::consume(int status, size_t amount) {
  size_t amt = std::min(amount, length_);
  size_t len = 0;
  while (root_ && amt > 0) {
    auto root = root_.get();
    size_t consumed = root->consume(amt);
    len += consumed;
    length_ -= consumed;
    amt -= consumed;
    if (root->length() > 0)
      break;
    pop(status);
  }
  return len;
}

void QuicBuffer::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("root", root_);
}

}  // namespace quic
}  // namespace node
