#ifndef SRC_ALIASED_STRUCT_H_
#define SRC_ALIASED_STRUCT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_internals.h"
#include "v8.h"
#include <memory>

namespace node {

template <typename T,
          typename = std::enable_if_t<std::is_object<T>::value>>
class AliasedStruct {
 public:
  explicit AliasedStruct(v8::Isolate* isolate) : isolate_(isolate) {
    const v8::HandleScope handle_scope(isolate);

    store_ = v8::ArrayBuffer::NewBackingStore(isolate, sizeof(T));
    ptr_ = new (store_->Data()) T;
    DCHECK_NOT_NULL(ptr_);

    v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, store_);
    buffer_ = v8::Global<v8::ArrayBuffer>(isolate, buffer);
  }

  AliasedStruct(const AliasedStruct& that)
     : isolate_(that.isolate_),
       store_(that.store_),
       ptr_(that.ptr_) {
    buffer_ = v8::Global<v8::ArrayBuffer>(that.isolate_, that.GetArrayBuffer());
  }

  ~AliasedStruct() {
    if (ptr_ != nullptr) ptr_->~T();
  }

  AliasedStruct& operator=(AliasedStruct&& that) noexcept {
    this->~AliasedStruct();
    isolate_ = that.isolate_;
    store_ = that.store_;
    ptr_ = that.ptr_;

    buffer_.Reset(isolate_, that.buffer_.Get(isolate_));

    that.ptr_ = nullptr;
    that.store_.reset();
    that.buffer_.Reset();
    return *this;
  }

  v8::Local<v8::ArrayBuffer> GetArrayBuffer() const {
    return buffer_.Get(isolate_);
  }

  T* Data() const { return ptr_; }

  T* operator*() const { return ptr_; }

  T* operator->() const { return ptr_; }

 private:
  v8::Isolate* isolate_;
  std::shared_ptr<v8::BackingStore> store_;
  T* ptr_;
  v8::Global<v8::ArrayBuffer> buffer_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ALIASED_STRUCT_H_
