#ifndef SRC_ALIASED_STRUCT_INL_H_
#define SRC_ALIASED_STRUCT_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_struct.h"
#include "v8.h"
#include <memory>

namespace node {

template <typename T>
template <typename... Args>
AliasedStruct<T>::AliasedStruct(v8::Isolate* isolate, Args&&... args)
    : isolate_(isolate) {
  const v8::HandleScope handle_scope(isolate);

  store_ = v8::ArrayBuffer::NewBackingStore(isolate, sizeof(T));
  ptr_ = new (store_->Data()) T(std::forward<Args>(args)...);
  DCHECK_NOT_NULL(ptr_);

  v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, store_);
  buffer_ = v8::Global<v8::ArrayBuffer>(isolate, buffer);
}

template <typename T>
AliasedStruct<T>::AliasedStruct(const AliasedStruct& that)
    : AliasedStruct(that.isolate_, *that) {}

template <typename T>
AliasedStruct<T>& AliasedStruct<T>::operator=(
    AliasedStruct<T>&& that) noexcept {
  this->~AliasedStruct();
  isolate_ = that.isolate_;
  store_ = that.store_;
  ptr_ = that.ptr_;

  buffer_ = std::move(that.buffer_);

  that.ptr_ = nullptr;
  that.store_.reset();
  return *this;
}

template <typename T>
AliasedStruct<T>::~AliasedStruct() {
  if (ptr_ != nullptr) ptr_->~T();
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ALIASED_STRUCT_INL_H_
