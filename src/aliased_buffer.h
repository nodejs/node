
#ifndef SRC_ALIASED_BUFFER_H_
#define SRC_ALIASED_BUFFER_H_

#include "v8.h"
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
  AliasedBuffer(v8::Isolate* isolate, const size_t count)
      : isolate_(isolate),
        count_(count),
        byte_offset_(0),
        free_buffer_(true) {
    CHECK_GT(count, 0);
    const v8::HandleScope handle_scope(isolate_);

    const size_t sizeInBytes = sizeof(NativeT) * count;

    // allocate native buffer
    buffer_ = Calloc<NativeT>(count);

    // allocate v8 ArrayBuffer
    v8::Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(
        isolate_, buffer_, sizeInBytes);

    // allocate v8 TypedArray
    v8::Local<V8T> js_array = V8T::New(ab, byte_offset_, count);
    js_array_ = v8::Global<V8T>(isolate, js_array);
  }

  /**
   * Create an AliasedBuffer over a sub-region of another aliased buffer.
   * The two will share a v8::ArrayBuffer instance &
   * a native buffer, but will each read/write to different sections of the
   * native buffer.
   *
   *  Note that byte_offset must by aligned by sizeof(NativeT).
   */
  AliasedBuffer(v8::Isolate* isolate,
                const size_t byte_offset,
                const size_t count,
                const AliasedBuffer<uint8_t,
                v8::Uint8Array>& backing_buffer)
      : isolate_(isolate),
        count_(count),
        byte_offset_(byte_offset),
        free_buffer_(false) {
    const v8::HandleScope handle_scope(isolate_);

    v8::Local<v8::ArrayBuffer> ab = backing_buffer.GetArrayBuffer();

    // validate that the byte_offset is aligned with sizeof(NativeT)
    CHECK_EQ(byte_offset & (sizeof(NativeT) - 1), 0);
    // validate this fits inside the backing buffer
    CHECK_LE(sizeof(NativeT) * count,  ab->ByteLength() - byte_offset);

    buffer_ = reinterpret_cast<NativeT*>(
        const_cast<uint8_t*>(backing_buffer.GetNativeBuffer() + byte_offset));

    v8::Local<V8T> js_array = V8T::New(ab, byte_offset, count);
    js_array_ = v8::Global<V8T>(isolate, js_array);
  }

  AliasedBuffer(const AliasedBuffer& that)
      : isolate_(that.isolate_),
        count_(that.count_),
        byte_offset_(that.byte_offset_),
        buffer_(that.buffer_),
        free_buffer_(false) {
    js_array_ = v8::Global<V8T>(that.isolate_, that.GetJSArray());
  }

  ~AliasedBuffer() {
    if (free_buffer_ && buffer_ != nullptr) {
      free(buffer_);
    }
    js_array_.Reset();
  }

  AliasedBuffer& operator=(AliasedBuffer&& that) {
    this->~AliasedBuffer();
    isolate_ = that.isolate_;
    count_ = that.count_;
    byte_offset_ = that.byte_offset_;
    buffer_ = that.buffer_;
    free_buffer_ = that.free_buffer_;

    js_array_.Reset(isolate_, that.js_array_.Get(isolate_));

    that.buffer_ = nullptr;
    that.js_array_.Reset();
    return *this;
  }

  /**
   * Helper class that is returned from operator[] to support assignment into
   * a specified location.
   */
  class Reference {
   public:
    Reference(AliasedBuffer<NativeT, V8T>* aliased_buffer, size_t index)
        : aliased_buffer_(aliased_buffer),
          index_(index) {
    }

    Reference(const Reference& that)
        : aliased_buffer_(that.aliased_buffer_),
          index_(that.index_) {
    }

    inline Reference& operator=(const NativeT& val) {
      aliased_buffer_->SetValue(index_, val);
      return *this;
    }

    inline Reference& operator=(const Reference& val) {
      return *this = static_cast<NativeT>(val);
    }

    operator NativeT() const {
      return aliased_buffer_->GetValue(index_);
    }

    inline Reference& operator+=(const NativeT& val) {
      const NativeT current = aliased_buffer_->GetValue(index_);
      aliased_buffer_->SetValue(index_, current + val);
      return *this;
    }

    inline Reference& operator+=(const Reference& val) {
      return this->operator+=(static_cast<NativeT>(val));
    }

    inline Reference& operator-=(const NativeT& val) {
      const NativeT current = aliased_buffer_->GetValue(index_);
      aliased_buffer_->SetValue(index_, current - val);
      return *this;
    }

   private:
    AliasedBuffer<NativeT, V8T>* aliased_buffer_;
    size_t index_;
  };

  /**
   *  Get the underlying v8 TypedArray overlayed on top of the native buffer
   */
  v8::Local<V8T> GetJSArray() const {
    return js_array_.Get(isolate_);
  }

  /**
  *  Get the underlying v8::ArrayBuffer underlying the TypedArray and
  *  overlaying the native buffer
  */
  v8::Local<v8::ArrayBuffer> GetArrayBuffer() const {
    return GetJSArray()->Buffer();
  }

  /**
   *  Get the underlying native buffer. Note that all reads/writes should occur
   *  through the GetValue/SetValue/operator[] methods
   */
  inline const NativeT* GetNativeBuffer() const {
    return buffer_;
  }

  /**
   *  Synonym for GetBuffer()
   */
  inline const NativeT* operator * () const {
    return GetNativeBuffer();
  }

  /**
   *  Set position index to given value.
   */
  inline void SetValue(const size_t index, NativeT value) {
#if defined(DEBUG) && DEBUG
    CHECK_LT(index, count_);
#endif
    buffer_[index] = value;
  }

  /**
   *  Get value at position index
   */
  inline const NativeT GetValue(const size_t index) const {
#if defined(DEBUG) && DEBUG
    CHECK_LT(index, count_);
#endif
    return buffer_[index];
  }

  /**
   *  Effectively, a synonym for GetValue/SetValue
   */
  Reference operator[](size_t index) {
    return Reference(this, index);
  }

  NativeT operator[](size_t index) const {
    return GetValue(index);
  }

  size_t Length() const {
    return count_;
  }

 private:
  v8::Isolate* isolate_;
  size_t count_;
  size_t byte_offset_;
  NativeT* buffer_;
  v8::Global<V8T> js_array_;
  bool free_buffer_;
};
}  // namespace node

#endif  // SRC_ALIASED_BUFFER_H_
