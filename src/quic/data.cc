#include "data.h"
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <node_sockaddr-inl.h>
#include <ngtcp2/ngtcp2.h>
#include <v8.h>

namespace node {

using v8::Local;
using v8::Value;

namespace quic {

Path::Path(const SocketAddress& local, const SocketAddress& remote) {
  ngtcp2_addr_init(&this->local, local.data(), local.length());
  ngtcp2_addr_init(&this->remote, remote.data(), remote.length());
}

PathStorage::PathStorage() { ngtcp2_path_storage_zero(this); }
PathStorage::operator ngtcp2_path() { return path; }

// ============================================================================

Store::Store(std::shared_ptr<v8::BackingStore> store,
    size_t length,
    size_t offset)
    : store_(std::move(store)),
      length_(length),
      offset_(offset) {
  CHECK_LE(offset_, store->ByteLength());
  CHECK_LE(length_, store->ByteLength() - offset_);
}

Store::Store(std::unique_ptr<v8::BackingStore> store,
    size_t length,
    size_t offset)
    : store_(std::move(store)),
      length_(length),
      offset_(offset) {
  CHECK_LE(offset_, store->ByteLength());
  CHECK_LE(length_, store->ByteLength() - offset_);
}

Store::Store(v8::Local<v8::ArrayBuffer> buffer, Option option)
    : Store(buffer->GetBackingStore(), buffer->ByteLength()) {
  if (option == Option::DETACH) {
    USE(buffer->Detach(Local<Value>()));
  }
}

Store::Store(v8::Local<v8::ArrayBufferView> view, Option option)
    : Store(view->Buffer()->GetBackingStore(),
            view->ByteLength(),
            view->ByteOffset()) {
  if (option == Option::DETACH) {
    USE(view->Buffer()->Detach(Local<Value>()));
  }
}

Store::operator bool() const { return store_ != nullptr; }
size_t Store::length() const { return length_; }

template <typename T, typename t>
T Store::convert() const {
  T buf;
  buf.base = store_ != nullptr ?
      static_cast<t*>(store_->Data()) + offset_ :
      nullptr;
  buf.len = length_;
  return buf;
}

Store::operator uv_buf_t() const {
  return convert<uv_buf_t, char>();
}

Store::operator ngtcp2_vec() const {
  return convert<ngtcp2_vec, uint8_t>();
}

Store::operator nghttp3_vec() const {
  return convert<nghttp3_vec, uint8_t>();
}

void Store::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("store", store_);
}

}  // namespace quic
}  // namespace node
