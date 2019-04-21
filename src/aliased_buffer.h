#ifndef SRC_ALIASED_BUFFER_H_
#define SRC_ALIASED_BUFFER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cinttypes>
#include <iostream>
#include "util-inl.h"
#include "v8.h"

namespace node {

typedef size_t AliasedBufferInfo;

/**
 * Do not use this class directly when creating instances of it - use the
 * Aliased*Array defined at the end of this file instead.
 *
 * This class encapsulates the technique of having a native buffer mapped to
 * a JS object. Writes to the native buffer can happen efficiently without
 * going through JS, and the data is then available to user's via the exposed
 * JS object.
 *
 * While this technique is computationally efficient, it is effectively a
 * write to JS program state w/out going through the standard
 * (monitored) API. Thus any VM capabilities to detect the modification are
 * circumvented.
 *
 * The encapsulation herein provides a placeholder where such writes can be
 * observed. Any notification APIs will be left as a future exercise.
 */
template <class NativeT,
          class V8T,
          // SFINAE NativeT to be scalar
          typename = std::enable_if_t<std::is_scalar<NativeT>::value>>
class AliasedBufferBase {
 public:
  AliasedBufferBase(v8::Isolate* isolate,
                    const size_t count,
                    const AliasedBufferInfo* info = nullptr)
      : isolate_(isolate), count_(count), byte_offset_(0), info_(info) {
    CHECK_GT(count, 0);
    if (info != nullptr) {
      // Will be deserialized later.
      return;
    }
    const v8::HandleScope handle_scope(isolate_);
    const size_t size_in_bytes =
        MultiplyWithOverflowCheck(sizeof(NativeT), count);

    // allocate v8 ArrayBuffer
    v8::Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(
        isolate_, size_in_bytes);
    buffer_ = static_cast<NativeT*>(ab->GetBackingStore()->Data());

    // allocate v8 TypedArray
    v8::Local<V8T> js_array = V8T::New(ab, byte_offset_, count);
    js_array_ = v8::Global<V8T>(isolate, js_array);
  }

  /**
   * Create an AliasedBufferBase over a sub-region of another aliased buffer.
   * The two will share a v8::ArrayBuffer instance &
   * a native buffer, but will each read/write to different sections of the
   * native buffer.
   *
   *  Note that byte_offset must by aligned by sizeof(NativeT).
   */
  // TODO(refack): refactor into a non-owning `AliasedBufferBaseView`
  AliasedBufferBase(
      v8::Isolate* isolate,
      const size_t byte_offset,
      const size_t count,
      const AliasedBufferBase<uint8_t, v8::Uint8Array>& backing_buffer,
      const AliasedBufferInfo* info = nullptr)
      : isolate_(isolate),
        count_(count),
        byte_offset_(byte_offset),
        info_(info) {
    if (info != nullptr) {
      // Will be deserialized later.
      return;
    }
    const v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::ArrayBuffer> ab = backing_buffer.GetArrayBuffer();

    // validate that the byte_offset is aligned with sizeof(NativeT)
    CHECK_EQ(byte_offset & (sizeof(NativeT) - 1), 0);
    // validate this fits inside the backing buffer
    CHECK_LE(MultiplyWithOverflowCheck(sizeof(NativeT), count),
             ab->ByteLength() - byte_offset);

    buffer_ = reinterpret_cast<NativeT*>(
        const_cast<uint8_t*>(backing_buffer.GetNativeBuffer() + byte_offset));

    v8::Local<V8T> js_array = V8T::New(ab, byte_offset, count);
    js_array_ = v8::Global<V8T>(isolate, js_array);
  }

  AliasedBufferBase(const AliasedBufferBase& that)
      : isolate_(that.isolate_),
        count_(that.count_),
        byte_offset_(that.byte_offset_),
        buffer_(that.buffer_) {
    DCHECK_NULL(info_);
    js_array_ = v8::Global<V8T>(that.isolate_, that.GetJSArray());
  }

  AliasedBufferInfo Serialize(v8::Local<v8::Context> context,
                              v8::SnapshotCreator* creator) {
    DCHECK_NULL(info_);
    return creator->AddData(context, GetJSArray());
  }

  inline void Deserialize(v8::Local<v8::Context> context) {
    DCHECK_NOT_NULL(info_);
    v8::Local<V8T> arr =
        context->GetDataFromSnapshotOnce<V8T>(*info_).ToLocalChecked();
    // These may not hold true for AliasedBuffers that have grown, so should
    // be removed when we expand the snapshot support.
    DCHECK_EQ(count_, arr->Length());
    DCHECK_EQ(byte_offset_, arr->ByteOffset());
    uint8_t* raw =
        static_cast<uint8_t*>(arr->Buffer()->GetBackingStore()->Data());
    buffer_ = reinterpret_cast<NativeT*>(raw + byte_offset_);
    js_array_.Reset(isolate_, arr);
    info_ = nullptr;
  }

  AliasedBufferBase& operator=(AliasedBufferBase&& that) noexcept {
    DCHECK_NULL(info_);
    this->~AliasedBufferBase();
    isolate_ = that.isolate_;
    count_ = that.count_;
    byte_offset_ = that.byte_offset_;
    buffer_ = that.buffer_;

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
    Reference(AliasedBufferBase<NativeT, V8T>* aliased_buffer, size_t index)
        : aliased_buffer_(aliased_buffer), index_(index) {}

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
    AliasedBufferBase<NativeT, V8T>* aliased_buffer_;
    size_t index_;
  };

  /**
   *  Get the underlying v8 TypedArray overlayed on top of the native buffer
   */
  v8::Local<V8T> GetJSArray() const {
    DCHECK_NULL(info_);
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
    DCHECK_NULL(info_);
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
    DCHECK_LT(index, count_);
    DCHECK_NULL(info_);
    buffer_[index] = value;
  }

  /**
   *  Get value at position index
   */
  inline const NativeT GetValue(const size_t index) const {
    DCHECK_NULL(info_);
    DCHECK_LT(index, count_);
    return buffer_[index];
  }

  /**
   *  Effectively, a synonym for GetValue/SetValue
   */
  Reference operator[](size_t index) {
    DCHECK_NULL(info_);
    return Reference(this, index);
  }

  NativeT operator[](size_t index) const {
    return GetValue(index);
  }

  size_t Length() const {
    return count_;
  }

  // Should only be used to extend the array.
  // Should only be used on an owning array, not one created as a sub array of
  // an owning `AliasedBufferBase`.
  void reserve(size_t new_capacity) {
    DCHECK_NULL(info_);
    DCHECK_GE(new_capacity, count_);
    DCHECK_EQ(byte_offset_, 0);
    const v8::HandleScope handle_scope(isolate_);

    const size_t old_size_in_bytes = sizeof(NativeT) * count_;
    const size_t new_size_in_bytes = MultiplyWithOverflowCheck(sizeof(NativeT),
                                                              new_capacity);

    // allocate v8 new ArrayBuffer
    v8::Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(
        isolate_, new_size_in_bytes);

    // allocate new native buffer
    NativeT* new_buffer = static_cast<NativeT*>(ab->GetBackingStore()->Data());
    // copy old content
    memcpy(new_buffer, buffer_, old_size_in_bytes);

    // allocate v8 TypedArray
    v8::Local<V8T> js_array = V8T::New(ab, byte_offset_, new_capacity);

    // move over old v8 TypedArray
    js_array_ = std::move(v8::Global<V8T>(isolate_, js_array));

    buffer_ = new_buffer;
    count_ = new_capacity;
  }

 private:
  v8::Isolate* isolate_ = nullptr;
  size_t count_ = 0;
  size_t byte_offset_ = 0;
  NativeT* buffer_ = nullptr;
  v8::Global<V8T> js_array_;

  // Deserialize data
  const AliasedBufferInfo* info_ = nullptr;
};

typedef AliasedBufferBase<int32_t, v8::Int32Array> AliasedInt32Array;
typedef AliasedBufferBase<uint8_t, v8::Uint8Array> AliasedUint8Array;
typedef AliasedBufferBase<uint32_t, v8::Uint32Array> AliasedUint32Array;
typedef AliasedBufferBase<double, v8::Float64Array> AliasedFloat64Array;
typedef AliasedBufferBase<uint64_t, v8::BigUint64Array> AliasedBigUint64Array;
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ALIASED_BUFFER_H_
