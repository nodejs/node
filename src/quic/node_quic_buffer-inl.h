#ifndef SRC_QUIC_NODE_QUIC_BUFFER_INL_H_
#define SRC_QUIC_NODE_QUIC_BUFFER_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_quic_buffer.h"
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

size_t QuicBufferChunk::seek(size_t amount) {
  amount = std::min(amount, remaining());
  buf_.base += amount;
  buf_.len -= amount;
  return amount;
}

size_t QuicBufferChunk::consume(size_t amount) {
  amount = std::min(amount, length_);
  length_ -= amount;
  return amount;
}

void QuicBufferChunk::done(int status) {
  if (done_called_) return;
  done_called_ = true;
  if (done_ != nullptr)
    std::move(done_)(status);
}

QuicBuffer::QuicBuffer(QuicBuffer&& src) noexcept
  : head_(src.head_),
    tail_(src.tail_),
    length_(src.length_) {
  root_ = std::move(src.root_);
  src.head_ = nullptr;
  src.tail_ = nullptr;
  src.length_ = 0;
  src.remaining_ = 0;
}


QuicBuffer& QuicBuffer::operator=(QuicBuffer&& src) noexcept {
  if (this == &src) return *this;
  this->~QuicBuffer();
  return *new(this) QuicBuffer(std::move(src));
}

bool QuicBuffer::is_empty(uv_buf_t buf) {
  return buf.len == 0 || buf.base == nullptr;
}

size_t QuicBuffer::consume(size_t amount) {
  return consume(0, amount);
}

size_t QuicBuffer::cancel(int status) {
  return consume(status, length());
}

void QuicBuffer::push(uv_buf_t buf, DoneCB done) {
  std::unique_ptr<QuicBufferChunk> chunk =
      std::make_unique<QuicBufferChunk>(buf, done);
  push(std::move(chunk));
}

size_t QuicBuffer::DrainInto(
    std::vector<uv_buf_t>* list,
    size_t* length,
    size_t max_count) {
  return DrainInto(
      [&](uv_buf_t buf) { list->push_back(buf); },
      length,
      max_count);
}

template <typename T>
size_t QuicBuffer::DrainInto(
    std::vector<T>* list,
    size_t* length,
    size_t max_count) {
  return DrainInto([&](uv_buf_t buf) {
    list->push_back(T {
      reinterpret_cast<uint8_t*>(buf.base), buf.len });
  }, length, max_count);
}

template <typename T>
size_t QuicBuffer::DrainInto(
    T* list,
    size_t* count,
    size_t* length,
    size_t max_count) {
  *count = 0;
  return DrainInto([&](uv_buf_t buf) {
    list[*count]->base = reinterpret_cast<uint8_t*>(buf.base);
    list[*count]->len = buf.len;
    *count += 1;
  }, length, max_count);
  CHECK_LE(*count, max_count);
}

template <typename Fn>
size_t QuicBuffer::DrainInto(
    Fn&& add_to_list,
    size_t* length,
    size_t max_count) {
  size_t len = 0;
  size_t count = 0;
  bool seen_head = false;
  QuicBufferChunk* pos = head_;
  if (pos == nullptr)
    return 0;
  if (length != nullptr) *length = 0;
  while (pos != nullptr && count < max_count) {
    count++;
    if (length != nullptr) *length += pos->remaining();
    add_to_list(pos->buf());
    if (pos == head_) seen_head = true;
    if (seen_head) len++;
    pos = pos->next_.get();
  }
  return len;
}

}  // namespace quic
}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_QUIC_NODE_QUIC_BUFFER_INL_H_
