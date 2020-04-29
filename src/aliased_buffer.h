#ifndef SRC_ALIASED_BUFFER_H_
#define SRC_ALIASED_BUFFER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cinttypes>
#include "util-inl.h"
#include "v8.h"

namespace node {

template <class NativeT,
          class V8T,
          // SFINAE NativeT to be scalar
          typename Trait = std::enable_if_t<std::is_scalar<NativeT>::value>>
class OwningAliasedBufferBase;

template <class NativeT,
          class V8T,
          // SFINAE NativeT to be scalar
          typename Trait = std::enable_if_t<std::is_scalar<NativeT>::value>>
class AliasedBufferViewBase;

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
template <class NativeT, class V8T>
class AliasedBufferBase {
 public:
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
    DCHECK_LT(index, count_);
    buffer_[index] = value;
  }

  /**
   *  Get value at position index
   */
  inline const NativeT GetValue(const size_t index) const {
    DCHECK_LT(index, count_);
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

  friend class OwningAliasedBufferBase<NativeT, V8T>;
  friend class AliasedBufferViewBase<NativeT, V8T>;

 private:
  AliasedBufferBase(v8::Isolate* isolate, const size_t count)
      : isolate_(isolate), count_(count) {}

  v8::Isolate* isolate_;
  size_t count_;
  NativeT* buffer_;
  v8::Global<V8T> js_array_;
};

template <class NativeT, class V8T, typename Trait>
class OwningAliasedBufferBase : public AliasedBufferBase<NativeT, V8T> {
  typedef AliasedBufferBase<NativeT, V8T> BaseType;

 public:
  OwningAliasedBufferBase(v8::Isolate* isolate, const size_t count)
      : BaseType(isolate, count) {
    CHECK_GT(count, 0);
    const v8::HandleScope handle_scope(isolate);
    const size_t size_in_bytes =
        MultiplyWithOverflowCheck(sizeof(NativeT), count);

    // allocate v8 ArrayBuffer
    v8::Local<v8::ArrayBuffer> ab =
        v8::ArrayBuffer::New(isolate, size_in_bytes);
    BaseType::buffer_ = static_cast<NativeT*>(ab->GetBackingStore()->Data());

    // allocate v8 TypedArray
    v8::Local<V8T> js_array = V8T::New(ab, 0, count);
    BaseType::js_array_ = v8::Global<V8T>(isolate, js_array);
  }

  // Should only be used to extend the array.
  void reserve(size_t new_capacity) {
    size_t count = BaseType::count_;
    v8::Isolate* isolate = BaseType::isolate_;
    DCHECK_GE(new_capacity, count);
    const v8::HandleScope handle_scope(isolate);

    const size_t old_size_in_bytes = sizeof(NativeT) * count;
    const size_t new_size_in_bytes = MultiplyWithOverflowCheck(sizeof(NativeT),
                                                              new_capacity);

    // allocate v8 new ArrayBuffer
    v8::Local<v8::ArrayBuffer> ab =
        v8::ArrayBuffer::New(isolate, new_size_in_bytes);

    // allocate new native buffer
    NativeT* new_buffer = static_cast<NativeT*>(ab->GetBackingStore()->Data());
    // copy old content
    memcpy(new_buffer, BaseType::buffer_, old_size_in_bytes);

    // allocate v8 TypedArray
    v8::Local<V8T> js_array = V8T::New(ab, 0, new_capacity);

    // move over old v8 TypedArray
    BaseType::js_array_ = std::move(v8::Global<V8T>(isolate, js_array));

    BaseType::buffer_ = new_buffer;
    BaseType::count_ = new_capacity;
  }
  OwningAliasedBufferBase(const OwningAliasedBufferBase&) = delete;
  OwningAliasedBufferBase& operator=(const OwningAliasedBufferBase&) = delete;
  OwningAliasedBufferBase(OwningAliasedBufferBase&&) = delete;
  OwningAliasedBufferBase& operator=(OwningAliasedBufferBase&&) = delete;
};

/**
 * Create an AliasedBufferViewBase over a sub-region of another
 * AliasedBufferBase. The two will share a v8::ArrayBuffer instance &
 * a native buffer, but will each read/write to different sections of the
 * native buffer.
 *
 *  Note that byte_offset must by aligned by sizeof(NativeT).
 */
template <class NativeT, class V8T, typename Trait>
class AliasedBufferViewBase : public AliasedBufferBase<NativeT, V8T> {
  typedef AliasedBufferBase<NativeT, V8T> BaseType;

 public:
  AliasedBufferViewBase(
      v8::Isolate* isolate,
      const size_t byte_offset,
      const size_t count,
      const AliasedBufferBase<uint8_t, v8::Uint8Array>& backing_buffer)
      : BaseType(isolate, count), byte_offset_(byte_offset) {
    const v8::HandleScope handle_scope(isolate);

    v8::Local<v8::ArrayBuffer> ab = backing_buffer.GetArrayBuffer();

    // validate that the byte_offset is aligned with sizeof(NativeT)
    CHECK_EQ(byte_offset & (sizeof(NativeT) - 1), 0);
    // validate this fits inside the backing buffer
    CHECK_LE(MultiplyWithOverflowCheck(sizeof(NativeT), count),
             ab->ByteLength() - byte_offset);

    BaseType::buffer_ = reinterpret_cast<NativeT*>(
        const_cast<uint8_t*>(backing_buffer.GetNativeBuffer() + byte_offset));

    v8::Local<V8T> js_array = V8T::New(ab, byte_offset, count);
    BaseType::js_array_ = v8::Global<V8T>(isolate, js_array);
  }

  AliasedBufferViewBase(const AliasedBufferViewBase&) = delete;
  AliasedBufferViewBase& operator=(const AliasedBufferViewBase&) = delete;
  AliasedBufferViewBase(AliasedBufferViewBase&&) = delete;
  AliasedBufferViewBase& operator=(AliasedBufferViewBase&&) = delete;

 private:
  size_t byte_offset_;
};

#define ALIASED_BUFFER_TYPES(V)                                                \
  V(uint8_t, Uint8Array)                                                       \
  V(int32_t, Int32Array)                                                       \
  V(uint32_t, Uint32Array)                                                     \
  V(double, Float64Array)                                                      \
  V(uint64_t, BigUint64Array)

#define V(NativeT, V8T)                                                        \
  typedef OwningAliasedBufferBase<NativeT, v8::V8T> OwningAliased##V8T;        \
  typedef AliasedBufferViewBase<NativeT, v8::V8T> Aliased##V8T##View;
ALIASED_BUFFER_TYPES(V)
#undef V
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ALIASED_BUFFER_H_
