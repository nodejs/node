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

AllocatedBuffer AllocatedBuffer::AllocateManaged(
    Environment* env,
    size_t size,
    int flags) {
  char* data = flags & ALLOCATE_MANAGED_UNCHECKED ?
      env->AllocateUnchecked(size) :
      env->Allocate(size);
  if (data == nullptr) size = 0;
  return AllocatedBuffer(env, uv_buf_init(data, size));
}

inline AllocatedBuffer::AllocatedBuffer(Environment* env, uv_buf_t buf)
    : env_(env), buffer_(buf) {}

inline void AllocatedBuffer::Resize(size_t len) {
  // The `len` check is to make sure we don't end up with `nullptr` as our base.
  char* new_data = env_->Reallocate(buffer_.base, buffer_.len,
                                    len > 0 ? len : 1);
  CHECK_NOT_NULL(new_data);
  buffer_ = uv_buf_init(new_data, len);
}

inline uv_buf_t AllocatedBuffer::release() {
  uv_buf_t ret = buffer_;
  buffer_ = uv_buf_init(nullptr, 0);
  return ret;
}

inline char* AllocatedBuffer::data() {
  return buffer_.base;
}

inline const char* AllocatedBuffer::data() const {
  return buffer_.base;
}

inline size_t AllocatedBuffer::size() const {
  return buffer_.len;
}

inline AllocatedBuffer::AllocatedBuffer(Environment* env)
    : env_(env), buffer_(uv_buf_init(nullptr, 0)) {}

inline AllocatedBuffer::AllocatedBuffer(AllocatedBuffer&& other)
    : AllocatedBuffer() {
  *this = std::move(other);
}

inline AllocatedBuffer& AllocatedBuffer::operator=(AllocatedBuffer&& other) {
  clear();
  env_ = other.env_;
  buffer_ = other.release();
  return *this;
}

inline AllocatedBuffer::~AllocatedBuffer() {
  clear();
}

inline void AllocatedBuffer::clear() {
  uv_buf_t buf = release();
  if (buf.base != nullptr) {
    CHECK_NOT_NULL(env_);
    env_->Free(buf.base, buf.len);
  }
}

inline v8::MaybeLocal<v8::Object> AllocatedBuffer::ToBuffer() {
  CHECK_NOT_NULL(env_);
  v8::MaybeLocal<v8::Object> obj = Buffer::New(env_, data(), size(), false);
  if (!obj.IsEmpty()) release();
  return obj;
}

inline v8::Local<v8::ArrayBuffer> AllocatedBuffer::ToArrayBuffer() {
  CHECK_NOT_NULL(env_);
  uv_buf_t buf = release();
  auto callback = [](void* data, size_t length, void* deleter_data){
    CHECK_NOT_NULL(deleter_data);

    static_cast<v8::ArrayBuffer::Allocator*>(deleter_data)
        ->Free(data, length);
  };
  std::unique_ptr<v8::BackingStore> backing =
      v8::ArrayBuffer::NewBackingStore(buf.base,
                                       buf.len,
                                       callback,
                                       env_->isolate()
                                          ->GetArrayBufferAllocator());
  return v8::ArrayBuffer::New(env_->isolate(),
                              std::move(backing));
}

}  // namespace node

#endif  // SRC_ALLOCATED_BUFFER_INL_H_
