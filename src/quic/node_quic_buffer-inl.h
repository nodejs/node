#ifndef SRC_QUIC_NODE_QUIC_BUFFER_INL_H_
#define SRC_QUIC_NODE_QUIC_BUFFER_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_quic_buffer.h"
#include "util-inl.h"
#include "uv.h"

namespace node {

namespace quic {

QuicBufferChunk::QuicBufferChunk(size_t len)
    : data_buf_(len, 0),
      buf_(uv_buf_init(reinterpret_cast<char*>(data_buf_.data()), len)),
      done_called_(true) {}

QuicBufferChunk::QuicBufferChunk(uv_buf_t buf, done_cb done)
    : buf_(buf) {
  if (done != nullptr)
    done_ = std::move(done);
}

QuicBufferChunk::~QuicBufferChunk() {
  CHECK(done_called_);
}

void QuicBufferChunk::Done(int status) {
  if (done_called_) return;
  done_called_ = true;
  if (done_ != nullptr)
    std::move(done_)(status);
}

QuicBuffer& QuicBuffer::operator=(QuicBuffer&& src) noexcept {
  if (this == &src) return *this;
  this->~QuicBuffer();
  return *new(this) QuicBuffer(std::move(src));
}

void QuicBuffer::Consume(ssize_t amount) { Consume(0, amount); }

size_t QuicBuffer::Cancel(int status) {
  size_t remaining = length();
  Consume(status, -1);
  return remaining;
}

uv_buf_t QuicBuffer::head() {
  return head_ == nullptr ?
      uv_buf_init(nullptr, 0) :
      uv_buf_init(
          head_->buf_.base + head_->roffset_,
          head_->buf_.len - head_->roffset_);
}

void QuicBuffer::Push(uv_buf_t buf, done_cb done) {
  std::unique_ptr<QuicBufferChunk> chunk =
      std::make_unique<QuicBufferChunk>(buf, done);
  Push(std::move(chunk));
}

void QuicBuffer::Reset(QuicBuffer* buffer) {
  CHECK(!buffer->root_);
  buffer->head_ = nullptr;
  buffer->tail_ = nullptr;
  buffer->length_ = 0;
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
    size_t datalen = pos->buf_.len - pos->roffset_;
    if (length != nullptr) *length += datalen;
    add_to_list(uv_buf_init(pos->buf_.base + pos->roffset_, datalen));
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
