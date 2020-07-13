#include "node_quic_buffer-inl.h"  // NOLINT(build/include)
#include "node_bob-inl.h"
#include "util.h"
#include "uv.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace node {
namespace quic {

void QuicBufferChunk::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("buf", data_buf_);
  tracker->TrackField("next", next_);
}

size_t QuicBuffer::Push(uv_buf_t* bufs, size_t nbufs, DoneCB done) {
  size_t len = 0;
  if (UNLIKELY(nbufs == 0)) {
    done(0);
    return 0;
  }
  DCHECK_NOT_NULL(bufs);
  size_t n = 0;
  while (nbufs > 1) {
    if (!is_empty(bufs[n])) {
      Push(bufs[n]);
      len += bufs[n].len;
    }
    n++;
    nbufs--;
  }
  if (!is_empty(bufs[n])) {
    Push(bufs[n], done);
    len += bufs[n].len;
  }
  // Special case if all the bufs were empty.
  if (UNLIKELY(len == 0))
    done(0);

  return len;
}

void QuicBuffer::Push(std::unique_ptr<QuicBufferChunk> chunk) {
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

size_t QuicBuffer::Seek(size_t amount) {
  size_t len = 0;
  while (head_ && amount > 0) {
    size_t amt = head_->Seek(amount);
    amount -= amt;
    len += amt;
    remaining_ -= amt;
    if (head_->remaining())
      break;
    head_ = head_->next_.get();
  }
  return len;
}

bool QuicBuffer::Pop(int status) {
  if (!root_)
    return false;
  std::unique_ptr<QuicBufferChunk> root(std::move(root_));
  root_ = std::move(root.get()->next_);

  if (head_ == root.get())
    head_ = root_.get();
  if (tail_ == root.get())
    tail_ = root_.get();

  root->Done(status);
  return true;
}

size_t QuicBuffer::Consume(int status, size_t amount) {
  size_t amt = std::min(amount, length_);
  size_t len = 0;
  while (root_ && amt > 0) {
    auto root = root_.get();
    size_t consumed = root->Consume(amt);
    len += consumed;
    length_ -= consumed;
    amt -= consumed;
    if (root->length() > 0)
      break;
    Pop(status);
  }
  return len;
}

void QuicBuffer::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("root", root_);
}

int QuicBuffer::DoPull(
    bob::Next<ngtcp2_vec> next,
    int options,
    ngtcp2_vec* data,
    size_t count,
    size_t max_count_hint) {
  size_t len = 0;
  size_t numbytes = 0;
  int status = bob::Status::STATUS_CONTINUE;

  // There's no data to read.
  if (!remaining() || head_ == nullptr) {
    status = is_ended() ?
        bob::Status::STATUS_END :
        bob::Status::STATUS_BLOCK;
    std::move(next)(status, nullptr, 0, [](size_t len) {});
    return status;
  }

  // Ensure that there's storage space.
  MaybeStackBuffer<ngtcp2_vec, kMaxVectorCount> vec;
  if (data == nullptr || count == 0) {
    vec.AllocateSufficientStorage(max_count_hint);
    data = vec.out();
  } else {
    max_count_hint = std::min(count, max_count_hint);
  }

  // Build the list of buffers.
  QuicBufferChunk* pos = head_;
  while (pos != nullptr && len < max_count_hint) {
    data[len].base = reinterpret_cast<uint8_t*>(pos->buf().base);
    data[len].len = pos->buf().len;
    numbytes += data[len].len;
    len++;
    pos = pos->next_.get();
  }

  // If the buffer is ended, and the number of bytes
  // matches the total remaining and OPTIONS_END is
  // used, set the status to STATUS_END.
  if (is_ended() &&
      numbytes == remaining() &&
      options & bob::OPTIONS_END)
    status = bob::Status::STATUS_END;

  // Pass the data back out to the caller.
  std::move(next)(
      status,
      data,
      len,
      [this](size_t len) { Seek(len); });

  return status;
}

}  // namespace quic
}  // namespace node
