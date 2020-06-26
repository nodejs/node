#ifndef SRC_QUIC_NODE_QUIC_BUFFER_INL_H_
#define SRC_QUIC_NODE_QUIC_BUFFER_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_quic_buffer.h"
#include "node_bob-inl.h"
#include "util-inl.h"
#include "uv.h"

#include <algorithm>

namespace node {

namespace quic {

QuicBufferChunk::QuicBufferChunk(size_t len)
    : data_buf_(len, 0),
      buf_(uv_buf_init(reinterpret_cast<char*>(data_buf_.data()), len)),
      length_(len),
      done_called_(true) {}

QuicBufferChunk::QuicBufferChunk(uv_buf_t buf, DoneCB done)
    : buf_(buf),
      length_(buf.len) {
  if (done != nullptr)
    done_ = std::move(done);
}

QuicBufferChunk::~QuicBufferChunk() {
  CHECK(done_called_);
}

size_t QuicBufferChunk::Seek(size_t amount) {
  amount = std::min(amount, remaining());
  buf_.base += amount;
  buf_.len -= amount;
  return amount;
}

size_t QuicBufferChunk::Consume(size_t amount) {
  amount = std::min(amount, length_);
  length_ -= amount;
  return amount;
}

void QuicBufferChunk::Done(int status) {
  if (done_called_) return;
  done_called_ = true;
  if (done_ != nullptr)
    std::move(done_)(status);
}

QuicBuffer::QuicBuffer(QuicBuffer&& src) noexcept
  : head_(src.head_),
    tail_(src.tail_),
    ended_(src.ended_),
    length_(src.length_) {
  root_ = std::move(src.root_);
  src.head_ = nullptr;
  src.tail_ = nullptr;
  src.length_ = 0;
  src.remaining_ = 0;
  src.ended_ = false;
}

QuicBuffer& QuicBuffer::operator=(QuicBuffer&& src) noexcept {
  if (this == &src) return *this;
  this->~QuicBuffer();
  return *new(this) QuicBuffer(std::move(src));
}

bool QuicBuffer::is_empty(uv_buf_t buf) {
  DCHECK_IMPLIES(buf.base == nullptr, buf.len == 0);
  return buf.len == 0;
}

size_t QuicBuffer::Consume(size_t amount) {
  return Consume(0, amount);
}

size_t QuicBuffer::Cancel(int status) {
  if (canceled_) return 0;
  canceled_ = true;
  size_t t = Consume(status, length());
  return t;
}

void QuicBuffer::Push(uv_buf_t buf, DoneCB done) {
  std::unique_ptr<QuicBufferChunk> chunk =
      std::make_unique<QuicBufferChunk>(buf, done);
  Push(std::move(chunk));
}
}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_QUIC_NODE_QUIC_BUFFER_INL_H_
