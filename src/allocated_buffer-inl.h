#ifndef SRC_ALLOCATED_BUFFER_INL_H_
#define SRC_ALLOCATED_BUFFER_INL_H_

#include "allocated_buffer.h"
#include "base_object-inl.h"
#include "node_buffer.h"
#include "env-inl.h"
#include "uv.h"
#include "v8.h"
#include "util-inl.h"
#include "node_internals.h"

namespace node {

// It's a bit awkward to define this Buffer::New() overload here, but it
// avoids a circular dependency with node_internals.h.
namespace Buffer {
v8::MaybeLocal<v8::Uint8Array> New(Environment* env,
                                   v8::Local<v8::ArrayBuffer> ab,
                                   size_t byte_offset,
                                   size_t length);
}

NoArrayBufferZeroFillScope::NoArrayBufferZeroFillScope(
    IsolateData* isolate_data)
  : node_allocator_(isolate_data->node_allocator()) {
  if (node_allocator_ != nullptr) node_allocator_->zero_fill_field()[0] = 0;
}

NoArrayBufferZeroFillScope::~NoArrayBufferZeroFillScope() {
  if (node_allocator_ != nullptr) node_allocator_->zero_fill_field()[0] = 1;
}

AllocatedBuffer AllocatedBuffer::AllocateManaged(
    Environment* env,
    size_t size) {
  NoArrayBufferZeroFillScope no_zero_fill_scope(env->isolate_data());
  std::unique_ptr<v8::BackingStore> bs =
      v8::ArrayBuffer::NewBackingStore(env->isolate(), size);
  return AllocatedBuffer(env, std::move(bs));
}

AllocatedBuffer::AllocatedBuffer(
    Environment* env, std::unique_ptr<v8::BackingStore> bs)
    : env_(env), backing_store_(std::move(bs)) {}

AllocatedBuffer::AllocatedBuffer(
    Environment* env, uv_buf_t buffer)
    : env_(env) {
  if (buffer.base == nullptr) return;
  auto map = env->released_allocated_buffers();
  auto it = map->find(buffer.base);
  CHECK_NE(it, map->end());
  backing_store_ = std::move(it->second);
  map->erase(it);
}

void AllocatedBuffer::Resize(size_t len) {
  if (len == 0) {
    backing_store_ = v8::ArrayBuffer::NewBackingStore(env_->isolate(), 0);
    return;
  }
  NoArrayBufferZeroFillScope no_zero_fill_scope(env_->isolate_data());
  backing_store_ = v8::BackingStore::Reallocate(
      env_->isolate(), std::move(backing_store_), len);
}

uv_buf_t AllocatedBuffer::release() {
  if (data() == nullptr) return uv_buf_init(nullptr, 0);

  CHECK_NOT_NULL(env_);
  uv_buf_t ret = uv_buf_init(data(), size());
  env_->released_allocated_buffers()->emplace(
      ret.base, std::move(backing_store_));
  return ret;
}

char* AllocatedBuffer::data() {
  if (!backing_store_) return nullptr;
  return static_cast<char*>(backing_store_->Data());
}

const char* AllocatedBuffer::data() const {
  if (!backing_store_) return nullptr;
  return static_cast<char*>(backing_store_->Data());
}


size_t AllocatedBuffer::size() const {
  if (!backing_store_) return 0;
  return backing_store_->ByteLength();
}

void AllocatedBuffer::clear() {
  backing_store_.reset();
}

v8::MaybeLocal<v8::Object> AllocatedBuffer::ToBuffer() {
  v8::Local<v8::ArrayBuffer> ab = ToArrayBuffer();
  return Buffer::New(env_, ab, 0, ab->ByteLength())
      .FromMaybe(v8::Local<v8::Uint8Array>());
}

v8::Local<v8::ArrayBuffer> AllocatedBuffer::ToArrayBuffer() {
  return v8::ArrayBuffer::New(env_->isolate(), std::move(backing_store_));
}

}  // namespace node

#endif  // SRC_ALLOCATED_BUFFER_INL_H_
