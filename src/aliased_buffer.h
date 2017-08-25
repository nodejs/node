
#ifndef SRC_ALIASED_BUFFER_H_
#define SRC_ALIASED_BUFFER_H_

#include "v8.h"
#include "util.h"
#include "util-inl.h"

namespace node {

/**
 * This class encapsulates the technique of having a native buffer mapped to
 * a JS object. Writes to the native buffer can happen efficiently without
 * going through JS, and the data is then available to user's via the exposed
 * JS object.
 *
 * While this technique is computationaly efficient, it is effectively a
 * write to JS program state w/out going through the standard
 * (monitored) API. Thus any VM capabilities to detect the modification are
 * circumvented.
 *
 * The encapsulation herein provides a placeholder where such writes can be
 * observed. Any notification APIs will be left as a future exercise.
 */
template <class NativeT, class V8T>
class AliasedBuffer {
 public:
  AliasedBuffer(v8::Isolate* isolate, const size_t count) :
    isolate_(isolate),
    count_(count),
    byte_offset_(0),
    freeBuffer_(true) {
    const v8::HandleScope handle_scope(this->isolate_);

    const size_t sizeInBytes = sizeof(NativeT) * count;

    // allocate native buffer
    this->buffer_ = UncheckedCalloc<NativeT>(count);
    if (this->buffer_ == nullptr) {
      ABORT();
    }

    // allocate v8 ArrayBuffer
    v8::Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(
      this->isolate_, this->buffer_, sizeInBytes);

    // allocate v8 TypedArray
    v8::Local<V8T> jsArray = V8T::New(ab, this->byte_offset_, count);
    this->jsArray_ = v8::Global<V8T>(isolate, jsArray);
  }

  /**
   * Create an AliasedBuffer over a sub-region of another aliased buffer.
   * The two will share a v8::ArrayBuffer instance &
   * a native buffer, but will each read/write to different sections of the
   * native buffer.
   *
   *  Note that byte_offset must by aligned by sizeof(NativeT).
   */
  AliasedBuffer(
    v8::Isolate* isolate,
    const size_t byte_offset,
    const size_t count,
    const AliasedBuffer<uint8_t,
    v8::Uint8Array>& backingBuffer) :
    isolate_(isolate),
    count_(count),
    byte_offset_(byte_offset),
    freeBuffer_(false) {
    const v8::HandleScope handle_scope(this->isolate_);
    // validate that the byte_offset is aligned with sizeof(NativeT)
    CHECK_EQ(byte_offset & (sizeof(NativeT) - 1), 0);
    this->buffer_ = reinterpret_cast<NativeT*>(
      const_cast<uint8_t*>(backingBuffer.GetNativeBuffer() + byte_offset));

    v8::Local<v8::ArrayBuffer> ab = backingBuffer.GetArrayBuffer();
    v8::Local<V8T> jsArray = V8T::New(ab, byte_offset, count);
    this->jsArray_ = v8::Global<V8T>(isolate, jsArray);
  }

  AliasedBuffer(const AliasedBuffer& that) :
    isolate_(that.isolate_),
    count_(that.count_),
    byte_offset_(that.byte_offset_),
    buffer_(that.buffer_),
    freeBuffer_(false) {
    this->jsArray_ = v8::Global<V8T>(that.isolate_, that.GetJSArray());
  }

  ~AliasedBuffer() {
    if (this->freeBuffer_ && this->buffer_ != NULL) {
      free(this->buffer_);
    }
    this->jsArray_.Reset();
  }

  /**
   * Helper class that is returned from operator[] to support assignment into
   * a specified location.
   */
  class Reference {
   public:
    Reference(AliasedBuffer<NativeT, V8T>* aliasedBuffer, size_t index) :
      aliasedBuffer_(aliasedBuffer),
      index_(index) {
    }

    Reference(const Reference& that) :
      aliasedBuffer_(that.aliasedBuffer_),
      index_(that.index_) {
    }

    inline Reference& operator=(const NativeT &val) {
      this->aliasedBuffer_->SetValue(this->index_, val);
      return *this;
    }

    operator NativeT() {
      return this->aliasedBuffer_->GetValue(this->index_);
    }

   private:
    AliasedBuffer<NativeT, V8T>* aliasedBuffer_;
    size_t index_;
  };

  /**
   *  Get the underlying v8 TypedArray overlayed on top of the native buffer
   */
  v8::Local<V8T> GetJSArray() const {
    return this->jsArray_.Get(this->isolate_);
  }

  /**
  *  Get the underlying v8::ArrayBuffer underlying the TypedArray and
  *  overlaying the native buffer
  */
  v8::Local<v8::ArrayBuffer> GetArrayBuffer() const {
    return this->GetJSArray()->Buffer();
  }

  /**
   *  Get the underlying native buffer. Note that all reads/writes should occur
   *  through the GetValue/SetValue/operator[] methods
   */
  inline const NativeT* GetNativeBuffer() const {
    return this->buffer_;
  }

  /**
   *  Synonym for GetBuffer()
   */
  inline const NativeT* operator * () {
    return this->GetNativeBuffer();
  }

  /**
   *  Set position index to given value.
   */
  inline void SetValue(const size_t index, NativeT value) {
    CHECK_LT(index, this->count_);
    this->buffer_[index] = value;
  }

  /**
   *  Get value at position index
   */
  inline const NativeT GetValue(const size_t index) const {
    CHECK_LT(index, this->count_);
    return this->buffer_[index];
  }

  /**
   *  Effectively, a synonym for GetValue/SetValue
   */
  Reference operator[](size_t index) {
    return Reference(this, index);
  }

  NativeT operator[](size_t index) const {
    return this->GetValue(index);
  }

 private:
  v8::Isolate* const isolate_;
  size_t count_;
  size_t byte_offset_;
  NativeT* buffer_;
  v8::Global<V8T> jsArray_;
  bool freeBuffer_;
};
}  // namespace node

#endif  // SRC_ALIASED_BUFFER_H_
